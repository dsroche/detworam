#include "nbdserv.h"

extern "C" {
#include <fcntl.h>
}

#include <cassert>
#include <vector>
#include <array>
#include <algorithm>
#include <sstream>

using namespace std;
using namespace nbdcpp;

// modeled on DevExample from nbdserv.h
// stores the blocks as a vector of block-sized arrays
template <unsigned BS=1024>
class RamDisk {
  private:
    std::vector<std::array<byte, BS>> _data;

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
    void read(size_t index, byte* data) const {
      std::copy(_data.at(index).begin(), _data.at(index).end(), data);
    }

    // write a single block to the device
    // index is the index of the block
    // data is a pointer to an array of size at last blocksize()
    void write(size_t index, const byte* data) {
      std::copy(data, data + BS, _data.at(index).begin());
    }

    // Read a number of blocks from device and write them
    // to the given file descriptor.
    // (copied version from nbdserv.h)
    bool read_to_fd(size_t index, size_t count, int fd) const {
      byte block[blocksize()];
      for (size_t curind = index; curind < index + count; ++curind) {
        read(curind, block);
        if (::write(fd, block, blocksize()) != blocksize()) {
          errout() << "Error writing block to stream\n";
          return false;
        }
      }
      return true;
    }

    // Write a number of blocks to device from the given file
    // descriptor.
    // (copied version from nbdserv.h)
    bool write_from_fd(size_t index, size_t count, int fd) {
      byte block[blocksize()];
      for (size_t curind = index; curind < index + count; ++curind) {
        if (::read(fd, block, blocksize()) != blocksize()) {
          errout() << "Error reading block from stream\n";
          return false;
        }
        write(curind, block);
      }
      return true;
    }

    // returns true iff the flush operation is supported
    bool flushes() const { return false; }

    // Syncs all pending read/write ops to any underlying device
    void flush() const { }

    // returns true iff the trim operation is supported
    bool trims() const { return false; }

    // Performs a DISCARD/TRIM operation (optional)
    void trim(size_t index, size_t count) { }
};

void usage(const char* arg0) {
  cerr << "usage: " << arg0 << " size ([host:][port] | -u file)\n";
  cerr << "  size is the size of the ramdisk in bytes\n";
  cerr << "  host can be \"localhost\" or an IPv4 dotted pair; default is localhost\n";
  cerr << "  port defaults to " << IP4Sock::DEFPORT << "\n";
  cerr << "  -u means to use a unix socket associated to the given file\n";
  exit(1);
}

template <unsigned BS, typename SockT>
void run_server(const SockT& sockaddr, size_t size) {
  create_nbd_server<RamDisk<BS>>(sockaddr, size).connect_many();
}

int main(int argc, char** argv) {
  // process command line arguments
  constexpr unsigned blocksize = 4096;
  bool unix = false;
  bool gotsize = false;
  bool gothost = false;
  string host = "localhost";
  int port = IP4Sock::DEFPORT;
  size_t size = 0;
  for (int argind=1; argind < argc; ++argind) {
    string curarg = argv[argind];
    if (curarg == "-u") {
      unix = true;
      // the host will be considered the filename
    } else if (curarg.find('-') == 0) {
      // some other option, not supported
      usage(argv[0]);
    } else if (!gotsize) {
      if (!(istringstream(curarg) >> size)) usage(argv[0]);
      if (size == 0) usage(argv[0]);
      size = 1 + (size - 1) / blocksize;
      gotsize = true;
    } else if (!gothost) {
      if (unix) {
        host = curarg;
      } else {
        auto colon = curarg.find(':');
        if (colon != curarg.npos) {
          host = curarg.substr(colon);
          if (colon < curarg.size() - 1) {
            curarg = curarg.substr(colon+1);
            colon = curarg.npos;
          }
        }
        if (colon == curarg.npos) {
          if (!(istringstream(curarg) >> port)) usage(argv[0]);
        }
      }
      gothost = true;
    } else if (size == 0) {
      if (!(istringstream(curarg) >> size)) usage(argv[0]);
    } else {
      usage(argv[0]);
    }
  }

  if (!gotsize) usage(argv[0]);

  // check socket file for unix sockets
  if (unix) {
    if (!gothost) usage(argv[0]);
    // the socket file must not exist yet
    if (access(host.c_str(), F_OK) == 0) {
      cout << "Error: socket file " << host
           << " already exists. Delete it now? (y/N) " << flush;
      string resp;
      getline(cin, resp);
      if (resp.find('y') != 0) exit(1);
    } else {
      // try to create the file
      int fd = creat(host.c_str(), 0777);
      if (fd < 0) {
        cout << "Error: cannot create socket file " << host << endl;
        exit(1);
      }
      close(fd);
    }
    unlink(host.c_str());
  }

  if (unix) run_server<blocksize>(UnixSock(host), size);
  else run_server<blocksize>(IP4Sock(port, host), size);

  return 0;
}
