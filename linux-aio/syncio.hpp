#ifndef __SYNCIO__
#define  __SYNCIO__

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <fstream>
#include <string>
#include <memory>
#include <vector>
#include <stdexcept>
#include <map>
#include <cassert>

#include <omp.h>

#ifndef ROUNDUP_PAGE
#define ROUNDUP_PAGE(nbytes) ((nbytes/BLOCKSIZE)*BLOCKSIZE)+BLOCKSIZE
#endif

#ifndef BLOCKSIZE
#define BLOCKSIZE 4096
#endif

enum FileOrg {
    STRIPE,
    HASH
};

typedef std::pair<std::string, size_t> FilePart;
typedef std::pair<std::string, std::string> FilenameSplit;

FilenameSplit splitext(std::string fn) {
    size_t last_index = fn.find_last_of(".");
    return FilenameSplit(fn.substr(0, last_index),
            fn.substr(last_index, fn.size()));
}

template <typename T>
class PartitionedFile {
    private:
        size_t nbytes; // total file size
        std::string fn; // name user gives file
        size_t nparts;
        std::vector<FilePart> parts; // (fn, nelems)
        size_t start_part_len; // length of the first partition

        PartitionedFile(const std::string fn, const size_t nparts) :
            fn(fn), nparts(nparts) {
        }

    public:
        typedef std::shared_ptr<PartitionedFile<T> > ptr;

        std::string build_part_fn(FilenameSplit fnsplit,
                const size_t part_id) {
            return fnsplit.first + std::to_string(part_id) + fnsplit.second;
        }

        size_t size(const int part_id=-1) {
            if (part_id == -1)
                return ROUNDUP_PAGE(nbytes);
            else
                return ROUNDUP_PAGE((parts[part_id].second)*sizeof(T));
        }

        std::string get_part_fn(const size_t part_id) {
            return build_part_fn(splitext(fn), part_id);
        }

        void init_parts(const size_t nelems) {
            FilenameSplit fnsplit = splitext(this->fn);
            start_part_len = nelems/nparts;

            for (size_t i = 0; i < nparts; i++) {
                size_t _nelems = i < (nparts-1) ? start_part_len:
                    start_part_len + (nelems % nparts);

                parts.push_back(FilePart(build_part_fn(fnsplit, i), _nelems));
            }
        }

        size_t get_nparts() {
            return nparts;
        }

        static ptr create(const std::string fn, const size_t nparts) {
            return std::shared_ptr<PartitionedFile>(
                    new PartitionedFile(fn, nparts));
        }

        size_t get_partindex(size_t g_index) {
            return std::min(g_index/start_part_len, nparts);
        }

        void write(const std::vector<T>& buffer) {
            write(&buffer[0], buffer.size());
        }

        // len is the number of elements
        void write(const T* buffer, const size_t len) {
            this->init_parts(len);
            this->nbytes = sizeof(T)*len;

            omp_set_num_threads(nparts);

            size_t ret = 0;
#pragma omp parallel for (+: ret)
            for (size_t i = 0; i < nparts; i++) {
                FILE* f = fopen(parts[i].first.c_str(), "wb");
                size_t ret = fwrite(reinterpret_cast<const void*>(
                            &buffer[i*parts[0].second]),
                            sizeof(T)*parts[i].second,
                            1, f);
                fclose(f);
            }
            assert(ret != len*sizeof(T));
        }

        size_t read(T* buffer, const size_t len) {
            omp_set_num_threads(nparts);
            return 0; // TODO
        }
};

template <typename T>
class SynchronousIO {
protected:
    std::string fn;
    mode_t mode;
    T* buffer;
    size_t buffer_size;
    int flags;

    typedef std::shared_ptr<SynchronousIO> ptr;

    SynchronousIO(const std::string fn, const int flags, const mode_t mode):
        fn(fn), flags(flags), mode(mode) {
    }

public:
    static ptr create(const std::string fn, const int flags, const mode_t mode) {
        return std::shared_ptr<SynchronousIO>(new SynchronousIO(fn, flags, mode));
    }

    void set_buffer(const T* buffer, const size_t len) {
        this->buffer = buffer;
    }
};

template <typename T>
class SynchronousWriter: SynchronousIO<T> {

private:

    SynchronousWriter(const std::string fn, const int flags=(O_RDWR | O_DIRECT | O_CREAT),
            const mode_t mode=0644): SynchronousIO<T>(fn, flags, mode){
    }

public:

    int write(std::vector<std::string> disks, FileOrg how) {
        int retcode = 0;
        switch (how) {
            case STRIPE:
                {


                }
                break;
            case HASH:
                {

                }
                break;
            default:
                throw std::runtime_error("Invalid FileOrg seletion");
        }
        return retcode;
    }

};
#endif
