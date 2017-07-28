#ifndef NBDPP_NBDSERV_H
#define NBDPP_NBDSERV_H

extern "C" {
#include <endian.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
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
#include <fstream>
#include <utility>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>

extern "C" {
/* copied from clisrv.h in NBD distribution */
#define INIT_PASSWD "NBDMAGIC"
#define NBD_OPT_EXPORT_NAME	(1)
#define NBD_FLAG_FIXED_NEWSTYLE (1 << 0)
#define NBD_FLAG_NO_ZEROES	(1 << 1)
#define NBD_FLAG_C_NO_ZEROES	NBD_FLAG_NO_ZEROES
/* copied from clisrv.c in NBD distribution */
static constexpr uint64_t opts_magic = 0x49484156454F5054LL;
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

static inline std::ostream*& errout_ptr() {
  static std::ostream* ptr = &std::cerr;
  return ptr;
}

static inline std::ostream*& logout_ptr() {
  static std::ostream* ptr = &std::cout;
  return ptr;
}

static inline std::ostream*& infoout_ptr() {
  static std::ostream* ptr = &std::cout;
  return ptr;
}

static inline std::ostream& errout() { return *errout_ptr(); }
static inline std::ostream& logout() { return *logout_ptr(); }
static inline std::ostream& infoout() { return *infoout_ptr(); }

typedef uint64_t size_t;
typedef unsigned char byte;

// default method to read in multiple bytes at once
// reads count blocks from dev and stores the result in data
template <class Dev>
void multiread_default(const Dev& dev, size_t index, size_t count, byte* data) {
  byte* curblock = data;
  for (size_t curind = index; dev.good() && curind < index + count; ++curind) {
    dev.read(curind, curblock);
    curblock += dev.blocksize();
  }
}

// default method to write multiple bytes at once
// writes count blocks from the raw array data to dev
template <class Dev>
void multiwrite_default(Dev& dev, size_t index, size_t count, const byte* data) {
  const byte* curblock = data;
  for (size_t curind = index; dev.good() && curind < index + count; ++curind) {
    dev.write(curind, curblock);
    curblock += dev.blocksize();
  }
}

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
    bool good() const;

    // number of bytes per block
    static constexpr size_t blocksize() { return 1024; }

    // number of blocks in device
    size_t numblocks() const;

    // read a single block from the device
    // index is the index of the block
    // data is a pointer to an array of size at least blocksize()
    void read(size_t index, byte* data) const;

    // write a single block to the device
    // index is the index of the block
    // data is a pointer to an array of size at least blocksize()
    void write(size_t index, const byte* data);

    // read multiple blocks at once
    // you may use the default implementation here, or optimize further
    void multiread(size_t index, size_t count, byte* data) const {
      multiread_default(*this, index, count, data);
    }

    // write multiple blocks at once
    // you may use the default implementation here, or optimize further
    void multiwrite(size_t index, size_t count, const byte* data) {
      multiwrite_default(*this, index, count, data);
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
      byte* curdest = reinterpret_cast<byte*>(dest);
      size_t remain = len;
      while (remain) {
        auto got = recv(csock, curdest, remain, 0);
        if (got <= 0) {
          errout() << "Error trying to read " << len << " bytes:\n";
          if (got == 0) errout() << "client closed connection\n";
          else perror("error code");
          return false;
        }
        remain -= got;
        curdest += got;
      }
      return true;
    }

    bool write_all(int csock, const void* src, size_t len) const {
      auto sent = send(csock, src, len, 0);
      if (sent != (ssize_t)len) {
        errout() << "Error trying to send " << len << " bytes\n";
        if (sent < (ssize_t)len) errout() << "client closed connection\n";
        else perror("error code");
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

    // return true if SIGINT has been set
    static bool check_sig() {
      if (nbd_sig) {
        logout() << "Caught SIGINT; shutting down" << std::endl;
        return true;
      }
      else return false;
    }

  public:
    static constexpr unsigned blocksize() { return DevT::blocksize(); }

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
      infoout() << " -block-size " << _thedev.blocksize() << " -Nx /dev/nbd0\n"
        << "(optionally changing /dev/nbd0 to another nbd device or adding other options)"
        << std::endl;
    }

    virtual ~NbdServer() {
      if (_ssock >= 0) close(_ssock);
      sigaction(SIGINT, &oldhandler, nullptr);
    }

    // don't allow copying
    NbdServer(const NbdServer&) = delete;
    NbdServer& operator= (const NbdServer&) = delete;

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
      std::vector<byte> buffer;
      constexpr auto blocksize = DevT::blocksize();
      size_t numblocks = _thedev.numblocks();
      struct nbd_request request;
      struct nbd_reply reply = {};
      reply.magic = htonl(NBD_REPLY_MAGIC);
      reply.error = htonl(0);
      bool go = true;
      bool good = true;

      // lambda to send an error reply message
      auto reply_err =
        [this, &csock, &reply, &go, &good]
        (int ercode, const char* msg)
      {
        logout() << "Replying with error: " << msg << "\n";
        reply.error = htonl(ercode);
        if (!write_all(csock, &reply, sizeof reply)) go = good = false;
        reply.error = 0;
      };

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
          reply_err(EIO, "Unrecoverable I/O error in device");
          go = false;
          break;
        }
        auto from = ntohll(request.from);
        auto len = ntohl(request.len);
        size_t startb = from / blocksize;
        unsigned startoff = from % blocksize;
        size_t endb = (from + len - 1) / blocksize;
        unsigned endoff = (from + len) % blocksize;
        size_t lenb = endb + 1 - startb;
        switch (ntohl(request.type)) {
          case NBD_CMD_READ:
#ifdef NBDCPP_DEBUG
            logout() << "Requested read of " << lenb << " blocks starting at " << startb << std::endl;
#endif
            if (endb >= numblocks) {
              reply_err(EFAULT, "read requested past end of device");
              break;
            }
            buffer.reserve(lenb * blocksize);
            _thedev.multiread(startb, lenb, buffer.data());
            if (!_thedev.good()) {
              reply_err(EIO, "some device I/O error occurred");
              // try to clear the error with a trivial read
              _thedev.read(0, buffer.data());
              break;
            }
            if (!write_all(csock, &reply, sizeof reply)) go = good = false;
            else if (!write_all(csock, buffer.data() + startoff, len))
              go = good = false;
            break;
          case NBD_CMD_WRITE:
#ifdef NBDCPP_DEBUG
            logout() << "Requested write of " << lenb << " blocks starting at " << startb << std::endl;
#endif
            if (endb >= numblocks) {
              reply_err(EFAULT, "write requested past end of device");
              break;
            }
            buffer.reserve(lenb * blocksize);
            if (startoff) {
              logout() << "Warning: start of write not block-aligned" << std::endl;
              _thedev.read(startb, buffer.data());
            }
            if (endoff) {
              logout() << "Warning: end of write not block-aligned" << std::endl;
              _thedev.read(endb, buffer.data() + ((lenb - 1) * blocksize));
            }
            if (!read_all(csock, buffer.data() + startoff, len)) {
              go = good = false;
              break;
            }
            _thedev.multiwrite(startb, lenb, buffer.data());
            if (!_thedev.good()) {
              reply_err(EIO, "some device I/O error occurred");
              // try to clear the error with a trivial read
              _thedev.read(0, buffer.data());
              break;
            }
            if (!write_all(csock, &reply, sizeof reply)) go = good = false;
            break;
          case NBD_CMD_DISC:
            logout() << "Requested disconnect request" << std::endl;
            go = false;
            // note fall-through
          case NBD_CMD_FLUSH:
#ifdef NBDCPP_DEBUG
            logout() << "Performing flush" << std::endl;
#endif
            if (_thedev.flushes()) _thedev.flush();
            if (!write_all(csock, &reply, sizeof reply)) good = false;
            break;
          case NBD_CMD_TRIM:
#ifdef NBDCPP_DEBUG
            logout() << "Requested trim of " << lenb << " blocks starting at " << startb << std::endl;
#endif
            if (_thedev.trims()) {
              if (lenb == 0) break;
              if (startoff) {
                ++startb;
                if (--lenb == 0) break;
              }
              if (endoff) {
                --endb;
                if (--lenb == 0) break;
              }
              if (endb >= numblocks) {
                endb = numblocks - 1;
                lenb = endb + 1 - startb;
              }
              _thedev.trim(startb, lenb);
            }
            if (!write_all(csock, &reply, sizeof reply)) go = good = false;
            break;
          default:
            reply_err(EINVAL, "received invalid request");
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
      if (check_sig()) return false;
      if (!read_all(csock, &magic, sizeof magic)) return false;
      magic = ntohll(magic);
      if(magic != opts_magic) {
        errout() << "Error: magic mismatch in negotiation\n";
        return false;
      }
      if (!read_all(csock, &opt, sizeof opt)) return false;
      opt = ntohl(opt);
      if (opt == NBD_OPT_EXPORT_NAME) {
        uint32_t msglen;
        if (!read_all(csock, &msglen, sizeof msglen)) return false;
        msglen = ntohl(msglen);
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
          // I don't know why this is 124, but copied from nbd-server.c and it works...
          if (!write_all(csock, zeros, 124)) return false;
        }
        return true;
      } else {
        errout() << "Unsupported operation from client; terminating connection\n";
        return false;
      }
    }
};

std::string nbd_usage_line() { return " ([host:][port] | -u socketfile) [-l logfile] [-d] [-q]"; }

void nbd_usage_doc(std::ostream& out) {
  out << "  host can be \"localhost\" or an IPv4 dotted pair; default is localhost\n"
      << "  port defaults to " << IP4Sock::DEFPORT << "\n"
      << "  -u means to use a unix socket associated to the given file\n"
      << "  -l specifies the file to append log and error messages to instead of stdout/stderr\n"
      << "  -d means to go into the background (daemonize).\n"
      << "  -q means \"quiet\": suppress all output except (possibly) the daemon PID"
      << std::endl;
}

template <typename ServT>
int nbdserv_run(ServT&& serv, bool daemonize) {
  if (daemonize) {
    auto pid = fork();
    if (pid < 0) {
      perror("Error: Could not fork for daemon");
      return 1;
    }
    if (pid) {
      // parent
      infoout() << "Send SIGINT to gracefully kill the server, as in:\n"
        << "  kill -INT ";
      // print pid and blocksize if in quiet mode
      std::cout << pid << ' ' << serv.blocksize() << std::endl;
      _exit(0);
    }
    if (setsid() < 0) {
      perror("Error: Could not daemonize");
      return 1;
    }
    logout() << "Daemon running on PID " << getpid() << std::endl;
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
  } else {
    infoout() << "Type Ctrl-C to gracefully kill the server." << std::endl;
  }
  serv.connect_many();
  return 0;
}

template <typename DevT, typename UsageFun, typename... DevArgs>
int nbdcpp_main(int argc, char** argv, int argind, UsageFun usage, DevArgs&&... devargs) {
  // process command line arguments
  bool unix = false;
  bool gothost = false;
  bool daemonize = false;
  bool quiet = false;
  std::ofstream nullfile;
  std::ofstream logfile;
  std::string host = "localhost";
  int port = IP4Sock::DEFPORT;
  for (; argind < argc; ++argind) {
    std::string curarg = argv[argind];
    if (curarg == "-u") {
      unix = true;
      // the host will be considered the filename
    } else if (curarg == "-l") {
      if (++argind >= argc) {
        usage();
        return 1;
      }
      logfile.open(argv[argind], std::ofstream::out|std::ofstream::app);
      if (!logfile.is_open()) {
        errout() << "Error: could not open log file " << argv[argind] << "\n";
        return 1;
      }
    } else if (curarg == "-d") {
      daemonize = true;
      logout_ptr() = &logfile;
      errout_ptr() = &logfile;
    } else if (curarg == "-q") {
      quiet = true;
      infoout_ptr() = &nullfile;
      logout_ptr() = &logfile;
      errout_ptr() = &logfile;
    } else if (curarg.find('-') == 0) {
      // some other option, not supported
      usage();
      return 1;
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
          if (!(std::istringstream(curarg) >> port)) {
            usage();
            return 1;
          }
        }
      }
      gothost = true;
    } else {
      usage();
      return 1;
    }
  }

  // check socket file for unix sockets
  if (unix) {
    if (!gothost) {
      usage();
      return 1;
    }
    // the socket file must not exist yet
    if (access(host.c_str(), F_OK) == 0) {
      if (quiet) {
        logout() << "Warning: clobbering existing socket file " << host << std::endl;
      } else {
        infoout() << "Error: socket file " << host
            << " already exists. Delete it now? (Y/n) " << std::flush;
        std::string resp;
        std::getline(std::cin, resp);
        auto letter = std::find_if(resp.begin(), resp.end(),
            [](int ch){return !std::isspace(ch);});
        if (letter != resp.end() && std::tolower(*letter) != 'y') return 1;
      }
    } else {
      // try to create the file
      int fd = creat(host.c_str(), 0777);
      if (fd < 0) {
        errout() << "Error: cannot create socket file " << host << std::endl;
        return 1;
      }
      close(fd);
    }
    unlink(host.c_str());
  }

  // redirect to log file now
  if (logfile.is_open()) {
    logfile << "Log started\n";
    errout_ptr() = &logfile;
    logout_ptr() = &logfile;
  }

  if (unix) {
    return nbdserv_run(
        NbdServer<DevT,UnixSock>
          (UnixSock(host), std::forward<DevArgs>(devargs)...),
        daemonize);
  } else {
    return nbdserv_run(
        NbdServer<DevT,IP4Sock>
          (IP4Sock(port, host), std::forward<DevArgs>(devargs)...),
        daemonize);
  }
}

} // namespace nbdcpp

#endif // NBDPP_NBDSERV_H
