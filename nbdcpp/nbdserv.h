#ifndef NBDPP_NBDSERV_H
#define NBDPP_NBDSERV_H

extern "C" {
#include <endian.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <linux/nbd.h>
}

#include <cstdint>
#include <string>
#include <iostream>
#include <utility>

extern "C" {
/* copied from clisrv.h in NBD distribution */
#define INIT_PASSWD "NBDMAGIC"
#define NBD_OPT_EXPORT_NAME	(1)
#define NBD_OPT_ABORT		(2)
#define NBD_OPT_LIST		(3)
#define NBD_REP_ACK		(1)
#define NBD_REP_SERVER		(2)
#define NBD_REP_FLAG_ERROR	(1 << 31)
#define NBD_REP_ERR_UNSUP	(1 | NBD_REP_FLAG_ERROR)
#define NBD_FLAG_FIXED_NEWSTYLE (1 << 0)
#define NBD_FLAG_NO_ZEROES	(1 << 1)
#define NBD_FLAG_C_NO_ZEROES	NBD_FLAG_NO_ZEROES
/* copied from clisrv.c in NBD distribution */
static constexpr uint64_t cliserv_magic = 0x00420281861253LL;
static constexpr uint64_t opts_magic = 0x49484156454F5054LL;
static constexpr uint64_t rep_magic = 0x3e889045565a9LL;
static inline u_int64_t ntohll(u_int64_t a) {
#if __BYTE_ORDER == __BIG_ENDIAN
  return a;
#elif __BYTE_ORDER == __LITTLE_ENDIAN
  u_int32_t lo = a & 0xffffffff;
  u_int32_t hi = a >> 32U;
  lo = ntohl(lo);
  hi = ntohl(hi);
  return ((u_int64_t) lo) << 32U | hi;
#else
#error "Invalid endianness"
#endif /* __BYTE_ORDER */
}
#define htonll ntohll
} // end copied C code

namespace nbdcpp {

static inline std::ostream& errout() { return std::cerr; }
static inline std::ostream& logout() { return std::cout; }
static inline std::ostream& infoout() { return std::cout; }

typedef uint64_t size_t;
typedef unsigned char byte;

// this is an ARCHETYPE
// you should make a class with these same methods in order
// to implement a custom NBD server backend
class DevExample {
  public:
    // Of course you probably want a constructor and destructor for your
    // actual class...
    // There are no open() or close() methods so it all has to be done
    // in your object creation and destruction.
    // Your constructor may have any arguments, and they will be forwarded
    // from the NbdServer constructor (see below).

    // should return false if some unrecoverable error has occurred
    bool good() const { return true; }

    // number of bytes per block
    static constexpr size_t blocksize() { return 1024; }

    // number of blocks in device
    size_t numblocks() const;

    // read a single block from the device
    // index is the index of the block
    // data is a pointer to an array of size at last blocksize()
    void read(size_t index, byte* data) const;

    // write a single block to the device
    // index is the index of the block
    // data is a pointer to an array of size at last blocksize()
    void write(size_t index, const byte* data);

    // Read a number of blocks from device and write them
    // to the given file descriptor.
    // you can use this method as-is or optimize further.
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
    // You can use this method as-is or optimize futher.
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


// archetype socket address classes
class SockAddr {
  public:
    // see socket(2) man page
    constexpr int domain() const;

    // see socket(2) man page
    int type() const { return SOCK_STREAM; }

    // see socket(2) man page
    constexpr int protocol() const { return 0; }

    // see connect(2) man page
    struct sockaddr* addr();
    const struct sockaddr* addr() const;

    // see connect(2) man page
    socklen_t addrlen() const;

    // show the nbd-client options needed to connect to this address
    void show_client(std::ostream& out) const;
};

// local unix domain socket address
class UnixSock :public SockAddr {
  public:
    struct sockaddr_un _theaddr = {AF_UNIX, {}};

  public:
    UnixSock(const std::string& path = "") {
      auto pathlen = sizeof _theaddr.sun_path;
      strncpy(_theaddr.sun_path, path.c_str(), pathlen-1);
    }

    static constexpr int domain() { return AF_UNIX; }
    static constexpr int type() { return SOCK_STREAM; }
    static constexpr int protocol() { return 0; }
    struct sockaddr* addr()
    { return reinterpret_cast<struct sockaddr*>(&_theaddr); }
    const struct sockaddr* addr() const
    { return reinterpret_cast<const struct sockaddr*>(&_theaddr); }
    static constexpr socklen_t addrlen() { return sizeof _theaddr; }

    // show the nbd-client options needed to connect to this address
    void show_client(std::ostream& out) const {
      out << "-unix " << _theaddr.sun_path;
    }
};

static inline std::ostream& operator<< (std::ostream& out, const UnixSock& us) {
  return out << "unix:" << us._theaddr.sun_path;
}

// IPv4 TCP socket address
class IP4Sock :public SockAddr {
  public:
    static constexpr int DEFPORT = 10809;
    struct sockaddr_in _theaddr = {};

  public:
    // host must be a dotted quad, "localhost", or "any"
    // note 10809 is the default NBD server port
    IP4Sock(int port = DEFPORT, const std::string& host = "localhost") {
      _theaddr.sin_family = AF_INET;
      _theaddr.sin_port = htons(port);
      if (host == "localhost") {
        _theaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      }
      else if (host == "any") {
        _theaddr.sin_addr.s_addr = htonl(INADDR_ANY);
      } else {
        if (inet_pton(AF_INET, host.c_str(), &_theaddr.sin_addr) != 1) {
          errout() << "ERROR: could not convert \"" << host << "\" to IPv4 address\n";
          exit(1);
        }
      }
    }

    static constexpr int domain() { return AF_INET; }
    static constexpr int type() { return SOCK_STREAM; }
    static constexpr int protocol() { return 0; }
    struct sockaddr* addr()
    { return reinterpret_cast<struct sockaddr*>(&_theaddr); }
    const struct sockaddr* addr() const
    { return reinterpret_cast<const struct sockaddr*>(&_theaddr); }
    static constexpr socklen_t addrlen() { return sizeof _theaddr; }

    // show the nbd-client options needed to connect to this address
    void show_client(std::ostream& out) const {
      constexpr unsigned tlen = 100;
      char temp[tlen] = {};
      if (inet_ntop(AF_INET, &_theaddr.sin_addr, temp, tlen-1) == temp) {
        out << temp;
      } else {
        out << "localhost";
      }
      auto port = ntohs(_theaddr.sin_port);
      if (port != DEFPORT) {
        out << " " << port;
      }
    }
};

static inline std::ostream& operator<< (std::ostream& out, const IP4Sock& is) {
  constexpr unsigned tlen = 100;
  char temp[tlen] = {};
  if (inet_ntop(AF_INET, &is._theaddr.sin_addr, temp, tlen-1) == temp) {
    out << temp;
  }
  return out << ":" << ntohs(is._theaddr.sin_port);
}

// global variable and signal handler for SIGINT

volatile sig_atomic_t nbd_sig = 0;
void sighandle(int sig) { nbd_sig = 1; }

// This class provides the main server functionality.
// DevT should match the archetype DevExample above.
template <typename DevT, typename SockT>
class NbdServer {
  private:
    DevT _thedev;
    int _ssock = -1;
    struct sigaction myhandler = {};
    struct sigaction oldhandler = {};

    bool read_all(int csock, void* dest, size_t len) const {
      auto got = recv(csock, dest, len, MSG_WAITALL);
      if (got != (ssize_t)len) {
        errout() << "Error trying to read " << len << " bytes\n";
        return false;
      }
      else return true;
    }

    bool write_all(int csock, const void* src, size_t len) const {
      auto sent = send(csock, src, len, 0);
      if (sent != (ssize_t)len) {
        errout() << "Error trying to send " << len << " bytes\n";
        return false;
      }
      else return true;
    }

    // reads in and ignores that many bytes
    bool read_ignore(int fd, size_t numbytes) {
      constexpr size_t bufsiz = 1024;
      if (numbytes == 0) return true;
      char buf[bufsiz];

      while (numbytes > bufsiz) {
        if (!read_all(fd, buf, bufsiz)) return false;
        numbytes -= bufsiz;
      }

      return read_all(fd, buf, numbytes);
    }

    // send a reply during the negotiation phase
    // copied from nbd-server.c
    bool send_reply(int fd, uint32_t opt, uint32_t reply_type, size_t datasize) {
      uint64_t magic = htonll(rep_magic);
      opt = htonl(opt);
      reply_type = htonl(reply_type);
      datasize = htonl(datasize);
      return write_all(fd, &magic, sizeof magic)
        &&   write_all(fd, &opt, sizeof opt)
        &&   write_all(fd, &reply_type, sizeof reply_type)
        &&   write_all(fd, &datasize, sizeof datasize);
    }

    // return true if SIGINT has been set
    static bool check_sig() {
      if (nbd_sig) {
        logout() << "Caught SIGINT; shutting down" << std::endl;
        return true;
      }
      else return false;
    }

  public:
    template <typename... DevArgs>
    NbdServer(const SockT& sockaddr, DevArgs&&... devargs)
      :_thedev(std::forward<DevArgs>(devargs)...)
    {
      // catch SIGINT
      myhandler.sa_handler = sighandle;
      sigaction(SIGINT, &myhandler, &oldhandler);
      _ssock = socket(sockaddr.domain(), sockaddr.type(), sockaddr.protocol());
      bind(_ssock, sockaddr.addr(), sockaddr.addrlen());
      logout() << "Server bound to address " << sockaddr << std::endl;
      listen(_ssock, 5);

      infoout() << "To connect, run the following command as root:\n"
        << "  nbd-client ";
      sockaddr.show_client(infoout());
      infoout() << " /dev/nbd0 -block-size " << _thedev.blocksize() << "\n"
        << "(optionally changing /dev/nbd0 to another nbd device or adding other options)"
        << std::endl;
      infoout() << "Remember to run (as root) modprobe nbd in order to load the nbd module first." << std::endl;
      infoout() << "Send SIGINT to gracefully kill the server, as in:\n"
        << "  kill -SIGINT " << getpid() << std::endl;
    }

    virtual ~NbdServer() {
      if (_ssock >= 0) close(_ssock);
      sigaction(SIGINT, &oldhandler, nullptr);
    }

    // returns true on one successful connection
    bool connect_once(bool stophere=true) {
      SockT client_addr;
      if (check_sig()) return false;
      logout() << "Listening for a new connection..." << std::endl;
      socklen_t addrlen = 0;
      int csock = accept(_ssock, client_addr.addr(), &addrlen);
      if (stophere) {
        close(_ssock);
        _ssock = -1;
      }
      if (check_sig()) return false;
      if (csock < 0 || addrlen <= 0) {
        errout() << "Error trying to accept connection; aborting\n";
        return false;
      }
      logout() << "Accepted connection to client " << client_addr << std::endl;
      if (!negotiate(csock)) {
        close(csock);
        return true;
      }
      return run_connection(csock);
    }

    // keeps listening for more connections until catching a SIGINT
    void connect_many() {
      while (connect_once(false));
    }

    // processes NBD requests over a running connection
    // returns whether it's POSSIBLE to restart with a new connection
    bool run_connection(int csock) {
      size_t blocksize = _thedev.blocksize();
      size_t numblocks = _thedev.numblocks();
      struct nbd_request request;
      struct nbd_reply reply = {};
      reply.magic = htonl(NBD_REPLY_MAGIC);
      reply.error = htonl(0);
      bool go = true;
      bool good = true;
      while (go && read_all(csock, &request, sizeof request)) {
        if (check_sig()) {
          go = false;
          break;
        }
        if (request.magic != htonl(NBD_REQUEST_MAGIC)) {
          errout() << "Error: incorrect magic on request packet; terminating connection\n";
          go = good = false;
          break;
        }
        memcpy(reply.handle, request.handle, sizeof reply.handle);
        if (!_thedev.good()) {
          // error in underlying device; send EIO and shut it down
          reply.error = htonl(EIO);
          go = false;
          if (!write_all(csock, &reply, sizeof reply)) good = false;
          break;
        }
        size_t start, len;
        switch (ntohl(request.type)) {
          case NBD_CMD_READ:
            start = ntohll(request.from) / blocksize;
            len = ntohl(request.len) / blocksize;
            if (start + len > numblocks) {
              reply.error = htonl(EFAULT);
              if (!write_all(csock, &reply, sizeof reply)) go = good = false;
              reply.error = htonl(0);
            } else {
              if (!write_all(csock, &reply, sizeof reply)) go = good = false;
              if (!_thedev.read_to_fd(start, len, csock)) go = good = false;
            }
            break;
          case NBD_CMD_WRITE:
            start = ntohll(request.from) / blocksize;
            len = ntohl(request.len) / blocksize;
            if (start + len > numblocks) {
              reply.error = htonl(EFAULT);
              if (!write_all(csock, &reply, sizeof reply)) go = good = false;
              reply.error = htonl(0);
            } else {
              if (!write_all(csock, &reply, sizeof reply)) go = good = false;
              if (!_thedev.write_from_fd(start, len, csock)) go = good = false;
            }
            break;
          case NBD_CMD_DISC:
            _thedev.flush();
            go = false;
            if (!write_all(csock, &reply, sizeof reply)) good = false;
            // note fall-through
          case NBD_CMD_FLUSH:
            _thedev.flush();
            break;
          case NBD_CMD_TRIM:
            start = ntohll(request.from) / blocksize;
            len = ntohl(request.len) / blocksize;
            if (start + len > numblocks) {
              reply.error = htonl(EFAULT);
              if (!write_all(csock, &reply, sizeof reply)) go = good = false;
              reply.error = htonl(0);
            } else {
              if (!write_all(csock, &reply, sizeof reply)) go = good = false;
              _thedev.trim(start, len);
            }
            break;
          default:
            errout() << "Error: received invalid request "
              << ntohl(request.type) << "\n";
            reply.error = htonl(EINVAL);
            if (!write_all(csock, &reply, sizeof reply)) go = good = false;
            break;
        }
      }
      if (csock) {
        if (good) {
          logout() << "terminating connection normally" << std::endl;
        } else {
          errout() << "terminating connection due to error" << std::endl;
        }
        close(csock);
      }
      return _thedev.good();
    }

    // does the NBD negotiation step to start the connection.
    // Returns true iff the connection should continue on to actually run the device.
    // mostly copied from nbd-server.c in current NBD release
    bool negotiate(int csock) {
      uint16_t smallflags = NBD_FLAG_FIXED_NEWSTYLE | NBD_FLAG_NO_ZEROES;
      uint64_t magic;
      uint32_t cflags = 0;
      uint32_t opt;

      if (!write_all(csock, INIT_PASSWD, 8)) return false;
      magic = htonll(opts_magic);
      if (!write_all(csock, &magic, sizeof magic)) return false;

      smallflags = htons(smallflags);
      if (!write_all(csock, &smallflags, sizeof smallflags)) return false;
      if (!read_all(csock, &cflags, sizeof cflags)) return false;
      cflags = htonl(cflags);
      while (true) {
        if (check_sig()) return false;
        if (!read_all(csock, &magic, sizeof magic)) return false;
        magic = ntohll(magic);
        if(magic != opts_magic) {
          errout() << "Error: magic mismatch in negotiation\n";
          return false;
        }
        if (!read_all(csock, &opt, sizeof opt)) return false;
        opt = ntohl(opt);
        // every message has a length
        uint32_t msglen;
        if (!read_all(csock, &msglen, sizeof msglen)) return false;
        msglen = ntohl(msglen);
        switch(opt) {
          case NBD_OPT_EXPORT_NAME:
            {
              // pretend to read the specified name
              read_ignore(csock, msglen);
              // send the info about this export now
              uint16_t flags = NBD_FLAG_HAS_FLAGS;
              uint64_t size_host = _thedev.blocksize() * _thedev.numblocks();
              size_host = htonll(size_host);
              if (!write_all(csock, &size_host, 8)) return false;
              if (_thedev.flushes()) flags |= NBD_FLAG_SEND_FLUSH;
              if (_thedev.trims()) flags |= NBD_FLAG_SEND_TRIM;
              flags = htons(flags);
              if (!write_all(csock, &flags, sizeof flags)) return false;
              if (!(cflags & NBD_FLAG_C_NO_ZEROES)) {
                char zeros[128] = {};
                // TODO why is this 124?
                if (!write_all(csock, zeros, 124)) return false;
              }
              return true;
            }
            break; // unreachable
          case NBD_OPT_LIST:
            {
              // msglen should be zero, but just in case...
              read_ignore(csock, msglen);
              std::string srvname = "NBDCPP_SERVER";
              msglen = srvname.length();
              auto nwlen = htonl(msglen);
              if( !send_reply(csock, opt, NBD_REP_SERVER, msglen + sizeof msglen) ||
                  !write_all(csock, &nwlen, sizeof msglen) ||
                  !write_all(csock, srvname.c_str(), msglen) ||
                  !send_reply(csock, opt, NBD_REP_ACK, 0))
                return false;
            }
            break;
          case NBD_OPT_ABORT:
            logout() << "Session terminated by client during negotiation phase" << std::endl;
            return false;
            break; // unreachable
          default:
            if (!send_reply(csock, opt, NBD_REP_ERR_UNSUP, 0)) return false;
            break;
        }
      }
      return false; // unreachable
    }
};

// convenience factory method to create NbdServer objects
template <typename DevT, typename SockT, typename... DevArgs>
NbdServer<DevT,SockT>
  create_nbd_server(const SockT& sockaddr, DevArgs&&... devargs)
{
  return NbdServer<DevT,SockT>(sockaddr, std::forward<DevArgs>(devargs)...);
}

} // namespace nbdcpp

#endif // NBDPP_NBDSERV_H
