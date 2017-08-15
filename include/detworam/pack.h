#ifndef WORAM_PACK_H
#define WORAM_PACK_H

#include <type_traits>
#include <algorithm>

#include <detworam/common.h>
#include <detworam/bytenum.h>
#include <detworam/memory.h>
#include <detworam/posmap.h>
#include <detworam/errors.h>

namespace detworam {

// Modeled on Memory<B,N>
template <typename MemT,
          size_t B,
          size_t N = MemT::size() * (MemT::blocksize() / B),
          bool Trivial=(MemT::blocksize()==B)>
class PackMem {
  static_assert(MemT::blocksize() > B,
      "Pack blocks must be smaller than the backend blocks (except for trivial packs)");
  static_assert(is_memory<MemT>::value, "template param to Pack must be a memory");

  public:
    using Backend = MemT;
    static constexpr size_t blocksize() { return B; }
    static constexpr size_t size() { return N; }
    static constexpr size_t perblock() { return MemT::blocksize() / blocksize(); }
    // the number of blocks in backend memory
    static constexpr size_t backblocks() { return ceiling(size(), perblock()); }

    static_assert(perblock() >= 1, "blocks are too small for a Pack");
    static_assert(MemT::size() >= backblocks(),
        "not enough blocks to fit N packs");

  public:
    std::unique_ptr<Backend> back;
  private:
    byte wblock[Backend::blocksize()];
    size_t wind = Backend::size();
    bool wchange = false;
    mutable byte rblock[Backend::blocksize()];
    mutable size_t rind = Backend::size();

  public:
    PackMem(decltype(back) bp) :back(std::move(bp))
    { }

    void load(size_t index, byte* buf) const {
      check_range(index, size()-1, "index in PackMem load");
      size_t outer = index / perblock();
      size_t instart = (index % perblock()) * blocksize();
      size_t inend = instart + blocksize();
      if (outer == wind) {
        std::copy(wblock + instart, wblock + inend, buf);
      } else {
        if (outer != rind) {
          back->load(outer, rblock);
          rind = outer;
        }
        std::copy(rblock + instart, rblock + inend, buf);
      }
    }

    void store(size_t index, const byte* buf) {
      check_range(index, size()-1, "index in PackMem store");
      size_t outer = index / perblock();
      size_t instart = (index % perblock()) * blocksize();
      if (outer != wind) {
        if (wchange) {
          back->store(wind, wblock);
          wchange = false;
        }
        if (outer == rind) {
          std::copy(rblock, rblock + Backend::blocksize(), wblock);
          rind = Backend::size();
        } else {
          back->load(outer, wblock);
        }
        wind = outer;
      }
      std::copy(buf, buf + blocksize(), wblock + instart);
      wchange = true;
    }

    void flush() {
      if (wchange) {
        back->store(wind, wblock);
        wchange = false;
      }
      back->flush();
    }
};
// type trait
template <typename MemT, size_t B, size_t N, bool Trivial>
struct is_memory<PackMem<MemT,B,N,Trivial>> :public std::true_type { };

//specialization for trivial packs
template <typename MemT, size_t B, size_t N>
class PackMem<MemT, B, N, true> {
  static_assert(MemT::blocksize() == B,
      "invalid Trivial argument to PackMem template");
  static_assert(is_memory<MemT>::value, "template param to Pack must be a memory");

  public:
    using Backend = MemT;
    static constexpr size_t blocksize() { return B; }
    static constexpr size_t size() { return N; }
    static constexpr size_t perblock() { return 1; }
    static constexpr size_t backblocks() { return size(); }

    static_assert(MemT::size() >= size(),
        "not enough blocks to fit N packs");

  public:
    std::unique_ptr<Backend> back;

  public:
    PackMem(decltype(back) bp) :back(std::move(bp)) { }

    void load(size_t index, byte* buf) const { back->load(index,buf); }

    void store(size_t index, const byte* buf) { back->store(index, buf); }

    void flush() { back->flush(); }
};

// factory structs
template <size_t B, size_t N = std::numeric_limits<size_t>::max()>
struct PackMemFactory {
  template <typename Mem>
  using T = PackMem<Mem, B, N>;

  template <typename Mem>
  static auto create(std::unique_ptr<Mem> mp)
  { return std::make_unique<T>(std::move(mp)); }
};
template <size_t B>
struct PackMemFactory<B, std::numeric_limits<size_t>::max()> {
  template <typename Mem>
  using T = PackMem<Mem, B>;

  template <typename Mem>
  static auto create(std::unique_ptr<Mem> mp)
  { return std::make_unique<T>(std::move(mp)); }
};


// helper function to determine the size needed for a PackPosMap
inline constexpr size_t ppm_size(size_t blocksize, size_t size, size_t pmax)
{ return ceiling(size, blocksize / bytelen(pmax)); }


// Modeled on PositionMap
template <typename MemT, size_t N, size_t M>
class PackPosMap {
  static_assert(is_memory<MemT>::value,
      "MemT type must be a memory in PackPosMap");

  public:
    static constexpr size_t size() { return N; }
    static constexpr size_t pmax() { return M; }
    static constexpr size_t nptr() { return pmax()+1; }

    static constexpr unsigned ptrsize() { return bytelen(nptr()); }
    using Pack = PackMem<MemT, ptrsize(), size()>;

    static_assert(Pack::backblocks() == ppm_size(MemT::blocksize(), size(), pmax()),
        "something's wrong in calculation of ppm_size");

  public:
    Pack backend;

  public:
    PackPosMap(std::unique_ptr<MemT> bp) :backend(std::move(bp)) { }

    size_t load(size_t index) const {
      byte buf[ptrsize()];
      backend.load(index, buf);
      return getnum<ptrsize()>(buf);
    }

    void store(size_t index, size_t pos) {
      check_range(pos, pmax(), "position out of range in PackPosMap store");
      byte buf[ptrsize()];
      storenum<ptrsize()>(buf, pos);
      backend.store(index, buf);
    }

    void flush() { backend.flush(); }
};
// type trait
template <typename MemT, size_t N, size_t M>
struct is_posmap<PackPosMap<MemT,N,M>> :public std::true_type { };

// factory struct
template <size_t N, size_t M>
struct PackPosMapFactory {
  template <typename Mem>
  using T = PackPosMap<Mem,N,M>;

  template <typename Mem>
  static auto create(std::unique_ptr<Mem> mp)
  { return std::make_unique<T>(std::move(mp)); }
};

} // namespace detworam

#endif // WORAM_PACK_H
