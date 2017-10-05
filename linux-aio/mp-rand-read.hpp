#ifndef __MP_RAND_READ_HPP__
#define __MP_RAND_READ_HPP__

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
#include <cassert>

#include "syncio.hpp"

#define CHECK_EQ(a, b) (a == b) ? true : false

#ifndef BLOCKSIZE
#define BLOCKSIZE 4096
#endif

#ifndef MAX_IOS
#define MAX_IOS 20
#endif

// TODO: Del
static constexpr int FLAGS_min_nr = 1;
constexpr int FLAGS_max_nr = 1;
// TODO: End Del


void sum(PartitionedFile<double>::ptr pf, const size_t tid, double* lsum) {
    srand(1234);
    size_t num_pending_ios = 0;

    // open the file
    int fd = open((pf->get_part_fn(tid)).c_str(), O_RDONLY | O_DIRECT , 0644);

    // Allocate read buffer space
    double* read_buffer;
    assert(posix_memalign(reinterpret_cast<void**>(&read_buffer),
            BLOCKSIZE, BLOCKSIZE*MAX_IOS) != 0);

    // setup io context
    io_context_t ioctx_ = 0;
    assert(io_setup(MAX_IOS, &ioctx_) >= 0);

    // Create control block structures
    struct iocb cb;
    struct iocb* cbs = &cb;

    // FIXME: Ensure file size is multiple of BLOCKSIZE
    size_t epb = BLOCKSIZE/sizeof(double); // elements per block
    for (size_t block_num = 0; block_num < (pf->size(tid)/BLOCKSIZE);
            block_num++) {
        io_prep_pread(&cb, fd, &read_buffer[epb], BLOCKSIZE,
                block_num * BLOCKSIZE);
        // Submit IO request
        assert(io_submit(ioctx_, 1, &cbs) == 1);
        num_pending_ios++;

        if (num_pending_ios >= (MAX_IOS-1) ||
                (block_num == ((pf->size(tid)/BLOCKSIZE)-1))) {
            printf("Should wait for ios\n");
            struct io_event* events = new io_event[num_pending_ios];

            size_t nevents = io_getevents(ioctx_, num_pending_ios,
                    MAX_IOS, events, NULL); // NULL means block

            // Callback
            printf("Doing the callback\n");

            for (int i = 0; i < num_pending_ios; i++) {
                struct io_event event = events[i];
                // AIORequest* req = static_cast<AIORequest*>(event.data);
                //req->Complete(event.res);
                //delete req;
            }
            delete events;
        }

        printf("Thread: %lu Submitted IO w block#: %lu . Pending: %lu\n",
                tid, block_num, num_pending_ios);
    }

    // Dealloc read buffer space
    free(read_buffer);
    io_destroy(ioctx_);
    close(fd);
}

template <typename T>
class AddOverlord {
    private:
        std::vector<std::thread> threads;
        typename PartitionedFile<T>::ptr pf1;
        typename PartitionedFile<T>::ptr pf2;

        AddOverlord(const std::string fn1, const std::string fn2,
                const size_t nparts, const size_t nelems) {

            printf("Creating PartitionedFiles ...\n");
            pf1 = PartitionedFile<T>::create(fn1, nparts);
            pf2 = PartitionedFile<T>::create(fn2, nparts);

            printf("Initing parts ...\n");
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
            std::vector<T> red_vec;
            red_vec.assign(pf1->get_nparts(), 0);

            for (size_t thd_id = 0; thd_id < pf1->get_nparts(); thd_id++) {
                threads.push_back(std::thread(sum, pf1, thd_id,
                            reinterpret_cast<double*>(&red_vec[thd_id])));
            }

            for (std::vector<std::thread>::iterator it = threads.begin();
                    it != threads.end(); ++it) {
                (*it).join();
            }
        }
};

class AIORequest {
    public:
        int* buffer_;

        virtual void Complete(int res) = 0;

        AIORequest() {
            int ret = posix_memalign(reinterpret_cast<void**>(&buffer_),
                    BLOCKSIZE, BLOCKSIZE);
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
            if (!CHECK_EQ(res, BLOCKSIZE))
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
            io_prep_pread(&iocb, fd_, req->buffer_, BLOCKSIZE, counter_ * BLOCKSIZE);
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

#endif
