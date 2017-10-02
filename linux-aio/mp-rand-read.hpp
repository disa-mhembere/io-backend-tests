#include <fcntl.h>
#include <libaio.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <thread>
#include <string>

#include "syncio.hpp"

#define CHECK_EQ(a, b) (a == b) ? true : false

static const int kPageSize = 4096;

// TODO: Del
static constexpr int FLAGS_min_nr = 1;
constexpr int FLAGS_max_nr = 1;
// TODO: End Del

void test() {
    printf("Printing ...\n");
}

template <typename T>
class AddOverlord {
    private:
        std::vector<std::thread> threads;
        typename PartitionedFile<T>::ptr pf1;
        typename PartitionedFile<T>::ptr pf2;

        AddOverlord(const std::string fn1, const std::string fn2,
                const size_t nparts, const size_t nelems) {

            pf1 = PartitionedFile<T>::create(fn1, nparts);
            pf2 = PartitionedFile<T>::create(fn2, nparts);

            pf1->init_parts(nelems);
            pf2->init_parts(nelems);
        }

    public:
        typedef std::shared_ptr<AddOverlord> ptr;
        static ptr create(std::string fn1, std::string fn2,
                const size_t nparts, const size_t nelems) {
            return std::shared_ptr<AddOverlord>(
                    new AddOverlord(fn1, fn2, nparts, nelems));
        }

        void run() {
            threads.push_back(std::thread(test));
        }
};

//class AIOAddtask {
    //public:
        //AIOAddtask(PartitionedFile::ptr f1, size_t offset1,
                //PartitionedFile::ptr f2, size_t offset2) {
            //;
        //}

//};

class AIORequest {
    public:
        int* buffer_;

        virtual void Complete(int res) = 0;

        AIORequest() {
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

class AIOAdder : public Adder {
    public:
        int fd_;
        io_context_t ioctx_;
        int counter_;
        int reap_counter_;
        int sum_;
        int length_;

        void SubmitRead() {
            int counter_ = 0; // FIXME Stub
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
};
