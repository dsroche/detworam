#include "nbdserv.h"

#include <vector>
#include <array>
#include <algorithm>
#include <sstream>

// modeled on DevExample from nbdserv.h
// stores the blocks as a vector of block-sized arrays
template <unsigned BS=4096>
class RamDisk {
  private:
    std::vector<std::array<nbdcpp::byte, BS>> _data;

  public:
    RamDisk(size_t nblocks) :_data(nblocks) { }

    // should return false if some unrecoverable error has occurred
    bool good() const { return true; }

    // number of bytes per block
    static constexpr size_t blocksize() { return BS; }

    // number of blocks in device
    size_t numblocks() const { return _data.size(); }

    // read a single block from the device
    // index is the index of the block
    // data is a pointer to an array of size at last blocksize()
    void read(size_t index, nbdcpp::byte* data) const {
      std::copy(_data.at(index).begin(), _data.at(index).end(), data);
    }

    // write a single block to the device
    // index is the index of the block
    // data is a pointer to an array of size at last blocksize()
    void write(size_t index, const nbdcpp::byte* data) {
      std::copy(data, data + BS, _data.at(index).begin());
    }

    // read multiple blocks at once
    void multiread(size_t index, size_t count, nbdcpp::byte* data) const {
      nbdcpp::multiread_default(*this, index, count, data);
    }

    // write multiple blocks at once
    void multiwrite(size_t index, size_t count, const nbdcpp::byte* data) {
      nbdcpp::multiwrite_default(*this, index, count, data);
    }

    // returns true iff the flush operation is supported
    constexpr bool flushes() const { return false; }

    // Syncs all pending read/write ops to any underlying device
    void flush() const { }

    // returns true iff the trim operation is supported
    constexpr bool trims() const { return false; }

    // Performs a DISCARD/TRIM operation (optional)
    void trim(size_t index, size_t count) { }
};

using namespace std;
using namespace nbdcpp;

int main(int argc, char** argv) {
  auto usage = [argv]() {
    errout() << "usage: " << argv[0] << " size" << nbd_usage_line() << "\n";
    errout() << "  Provides a ramdisk over an NBD server.\n";
    errout() << "  size is the size of the ramdisk in KB\n";
    nbd_usage_doc(errout());
  };

  // size must be the first command line argument
  size_t size;
  if (argc < 2 || !(istringstream(argv[1]) >> size) || size <= 0) {
    usage();
    return 1;
  }

  size_t numblocks = 1 + (size*1024 - 1) / RamDisk<>::blocksize();

  // everything else is taken care of by nbdcpp
  return nbdcpp_main<RamDisk<>>(argc, argv, 2, usage, numblocks);
}
