#include <fcntl.h>
#include <libaio.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <cassert>
#include <stdexcept>
#include <string>

#define CHECK_EQ(a, b) (a == b) ? true : false

static constexpr int FLAGS_min_nr = 1;
constexpr int FLAGS_max_nr = 1;
constexpr int FLAGS_concurrent_requests = 100; // Number of concurrent requests
constexpr int FLAGS_file_size = 1000; //Length of file in 4k blocks
const std::string FLAGS_path = "./testfile"; // Path to the file

// The size of operation that will occur on the device
static const int kPageSize = 4096;

class AIORequest {
    public:
        int* buffer_;

        virtual void Complete(int res) = 0;

        AIORequest() {
            // TODO: man posix_memalign
            int ret = posix_memalign(reinterpret_cast<void**>(&buffer_),
                    kPageSize, kPageSize);
            if (!CHECK_EQ(ret, 0))
                std::cout << "\nAIORequest CHECK_EQ failed\n";
        }

        virtual ~AIORequest() {
            free(buffer_);
        }
};

class Adder {
    public:
        virtual void Add(int amount) = 0;

        virtual ~Adder() { };
};

class AIOReadRequest : public AIORequest {
    private:
        Adder* adder_;

    public:
        AIOReadRequest(Adder* adder) : AIORequest(), adder_(adder) { }

        virtual void Complete(int res) {
            if (!CHECK_EQ(res, kPageSize))
                std::cout << "\nRead incomplete or error: " << res << std::endl;

            int value = buffer_[0];
            std::cout << "Read of " << value << " completed\n";
            adder_->Add(value);
        }
};

class AIOWriteRequest : public AIORequest {
    private:
        int value_;

    public:
        AIOWriteRequest(int value) : AIORequest(), value_(value) {
            buffer_[0] = value;
        }

        virtual void Complete(int res) {
            if (!CHECK_EQ(res, kPageSize))
                std::cout << "Write incomplete or error: " << res;

            std::cout << "Write of " << value_ << " completed\n";
        }
};

class AIOAdder : public Adder {
    public:
        int fd_;
        io_context_t ioctx_;
        int counter_;
        int reap_counter_;
        int sum_;
        int length_;

        AIOAdder(int length)
            : ioctx_(0), counter_(0), reap_counter_(0), sum_(0), length_(length) { }

        void Init() {
            std::cout << "Opening file";

            fd_ = open(FLAGS_path.c_str(), O_RDWR | O_DIRECT | O_CREAT, 0644);
            if (!fd_ >= 0)
                std::cout << "Error opening file";

            std::cout << "Allocating enough space for the sum\n";

            assert(fallocate(fd_, 0, 0, kPageSize * length_) >= 0); // 4096x1000

            std::cout << "Setting up the io context";
            assert(io_setup(100, &ioctx_) >= 0);  }

        virtual void Add(int amount) {
            sum_ += amount;
            std::cout << "Adding " << amount << " for a total of " << sum_ << std::endl;
        }

        void SubmitWrite() {
            std::cout << "Submitting a write to " << counter_ << std::endl;
            struct iocb iocb;
            struct iocb* iocbs = &iocb;
            AIORequest *req = new AIOWriteRequest(counter_);
            io_prep_pwrite(&iocb, fd_, req->buffer_, kPageSize, counter_ * kPageSize);
            iocb.data = req;
            int res = io_submit(ioctx_, 1, &iocbs);
            if (!CHECK_EQ(res, 1))
                std::cout << "\nSubmitWrite CHECK_EQ failed\n";
        }

        void WriteFile() {
            reap_counter_ = 0;
            // Every (4096/sizeof(int)) = 512 items write 0 ... 999)
            for (counter_ = 0; counter_ < length_; counter_++) {
                SubmitWrite();
                Reap();
            }
            ReapRemaining();
        }

        void SubmitRead() {
            std::cout << "Submitting a read from " << counter_ << std::endl;
            struct iocb iocb;
            struct iocb* iocbs = &iocb;
            AIORequest *req = new AIOReadRequest(this);
            io_prep_pread(&iocb, fd_, req->buffer_, kPageSize, counter_ * kPageSize);
            iocb.data = req;
            int res = io_submit(ioctx_, 1, &iocbs);
            if (!CHECK_EQ(res, 1))
                std::cout << "\nSubmitRead CHECK_EQ failed\n";
        }

        void ReadFile() {
            reap_counter_ = 0;
            for (counter_ = 0; counter_ < length_; counter_++) {
                SubmitRead();
                Reap();
            }
            ReapRemaining();
        }

        int DoReap(int min_nr) {
            std::cout << "Reaping between " << min_nr << " and "
                << FLAGS_max_nr << " io_events\n";
            struct io_event* events = new io_event[FLAGS_max_nr];
            struct timespec timeout;
            timeout.tv_sec = 0;
            timeout.tv_nsec = 100000000;
            int num_events;
            std::cout << "Calling io_getevents\n";
            num_events = io_getevents(ioctx_, min_nr, FLAGS_max_nr, events,
                    &timeout);

            std::cout << "Calling completion function on results\n";
            for (int i = 0; i < num_events; i++) {
                struct io_event event = events[i];
                AIORequest* req = static_cast<AIORequest*>(event.data);
                req->Complete(event.res);
                delete req;
            }
            delete events;

            std::cout << "Reaped " << num_events << " io_events\n";
            reap_counter_ += num_events;
            return num_events;
        }

        void Reap() {
            if (counter_ >= FLAGS_min_nr) {
                DoReap(FLAGS_min_nr);
            }
        }

        void ReapRemaining() {
            while (reap_counter_ < length_) {
                DoReap(1);
            }
        }

        ~AIOAdder() {
            std::cout << "Closing AIO context and file\n";
            io_destroy(ioctx_);
            close(fd_);
        }

        int Sum() {
            std::cout << "Writing consecutive integers to file\n";
            WriteFile();
            std::cout << "Reading consecutive integers from file\n";
            ReadFile();
            return sum_;
        }
};

int main(int argc, char* argv[]) {
    AIOAdder adder(FLAGS_file_size);
    adder.Init();
    int sum = adder.Sum();
    int expected = (FLAGS_file_size * (FLAGS_file_size - 1)) / 2;
    std::cout << "AIO is complete\n";
    assert (sum == expected);

    printf("Successfully calculated that the sum of integers from 0"
            " to %d is %d\n", FLAGS_file_size - 1, sum);
    return 0;
}
