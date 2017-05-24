#ifndef WORAM_FILEMEM_H
#define WORAM_FILEMEM_H

extern "C" {
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
}

#include <type_traits>

#include <woram/common.h>
#include <woram/errors.h>
#include <woram/memory.h>

namespace woram {

template <size_t B, size_t N>
class FileMem {
  public:
    static constexpr size_t blocksize() { return B; }
    static constexpr size_t size() { return N; }

  private:
    int fd;

  public:
    FileMem(const char* fname) {
      fd = open(fname, O_RDWR);
      if (fd < 0) {
        perror("Could not open file for FileMem");
        exit(1);
      }
      auto sz = lseek(fd, 0, SEEK_END);
      if (sz < 0 || static_cast<size_t>(sz) < size() * blocksize()) {
        fprintf(stderr, "ERROR: file is too small, need at least %lu bytes\n",
            size() * blocksize());
        exit(1);
      }
    }

    ~FileMem() { close(fd); }

    void load(size_t index, byte* buf) const {
      check_range(index, size()-1, "index out of bounds in FileMem load");
      lseek(fd, index * blocksize(), SEEK_SET);
      auto res = read(fd, buf, blocksize());
      if (res < 0) {
        perror("Unsuccessful read in FileMem load");
        exit(1);
      } else if (res != blocksize()) {
        fprintf(stderr, "ERROR: Read did not complete in FileMem load\n");
        exit(1);
      }
    }

    void store(size_t index, const byte* buf) {
      check_range(index, size()-1, "index out of bounds in FileMem store");
      lseek(fd, index * blocksize(), SEEK_SET);
      auto res = write(fd, buf, blocksize());
      if (res < 0) {
        perror("Unsuccessful write in FileMem store");
        exit(1);
      } else if (res != blocksize()) {
        fprintf(stderr, "Write did not complete in FileMem store");
        exit(1);
      }
    }

    void flush() {
      fdatasync(fd);
    }
};
// type trait
template <size_t B, size_t N>
struct is_memory<FileMem<B,N>> :public std::true_type { };

} // namespace woram

#endif // WORAM_FILEMEM_H
