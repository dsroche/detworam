#ifndef WORAM_WORAM_H
#define WORAM_WORAM_H

#include <woram/posmap.h>
#include <woram/split.h>

namespace woram {

// this is an ARCHETYPE - don't derive from it
// similar to Memory, but there's the extra position information
// for each load and store.
// BMT is the backend Memory type
// N is the size() of this Woram, which presumably must be less than BMT::size()
template <typename BMT, size_t N>
class PlainWoram final {
  public:
    static constexpr size_t blocksize() { return BMT::blocksize(); }
    static constexpr size_t size() { return N; }
    // this should be the size of pointers, i.e., the largest pointer value plus 1
    static constexpr size_t pmax();
    // this indicates a "null pointer" in the woram
    static constexpr size_t nptr() { return pmax() + 1; }

    void load(size_t index, size_t position, byte* buf) const;

    // returns the position
    template <typename PM>
    size_t store(size_t index, const byte* buf, const PM& posmap);

    // does a dummy write
    template <typename PM>
    void dummy_write(const PM& posmap);

    void flush();
};
// specialize this type trait for every plain Woram type
template <typename BMT, size_t N>
struct is_plain_woram<PlainWoram<BMT,N>> :public std::true_type { };

// define a traits class like this for every plain Woram type
// (this one is just a dummy of course)
struct PlainWoramTrait {
  template <typename MemT, size_t N>
  struct Factory {
    using T = PlainWoram<MemT, N>; // T should be a typedef for the memory type
    // creates an instance of T with size N using backend memory MemT
    static std::unique_ptr<T> create(std::unique_ptr<MemT> mp);
  };

  // returns the pmax value for storing N size_B blocks in a size-M backend
  template <size_t B, size_t N, size_t M>
  static constexpr size_t pmax();

  // gets the "preferred" backend size if the frontend size is N
  static constexpr size_t prefsize(size_t N);
};


// modeled on Memory
// WT should be a PlainWoram
// PMT should be a PositionMap
template <typename WT, typename PMT>
class PMWoram {
  static_assert(is_plain_woram<WT>::value,
      "WT parameter to PMWoram must be a PlainWoram");
  static_assert(is_posmap<PMT>::value,
      "PMT parameter to PMWoram must be a PositionMap");

  public:
    static constexpr size_t blocksize() { return WT::blocksize(); }
    static constexpr size_t size() { return WT::size(); }
    static constexpr size_t pmax() { return WT::pmax(); }

    using Woram = WT;
    using PM = PMT;

    static_assert(PM::pmax() >= WT::pmax(),
        "position map's positions aren't large enough for Woram in PMWoram");

  private:
    std::unique_ptr<Woram> backend;
    std::unique_ptr<PM> posmap;

  public:
    PMWoram(decltype(backend) wp, decltype(posmap) pmp)
      :backend(std::move(wp)), posmap(std::move(pmp)) { }

    void load(size_t index, byte* buf) const {
      auto pos = posmap->load(index);
      if (pos == posmap->nptr()) pos = backend->nptr();
      else {
        check_range(pos, backend->pmax(),
            "invalid value returned from posmap in LocalPMWoram");
      }
      backend->load(index, pos, buf);
    }

    void store(size_t index, const byte* buf) {
      auto pos = backend->store(index, buf, *posmap);
      check_range(pos, backend->pmax(),
          "invalid value returned from backend.store in LocalPMWoram");
      posmap->store(index, pos);
    }

    void flush() {
      backend->flush();
      posmap->flush();
    }
};
// type trait
template <typename WT, typename PMT>
struct is_woram<PMWoram<WT,PMT>> :public std::true_type { };


// modeled on PlainWoram
// BMT is the type of backend memory
// N is the frontend size
template <typename BMT, size_t N>
class OneWriteWoram {
  static_assert(is_memory<BMT>::value, "BMT parameter must be modeled on Memory");

  public:
    static constexpr size_t blocksize() { return BMT::blocksize(); }
    static constexpr size_t size() { return N; }
    static constexpr size_t pmax() { return BMT::size() - 1; }
    static constexpr size_t nptr() { return BMT::size(); }
    static constexpr size_t maxwrites() { return BMT::size(); }

    using Mem = BMT;

  private:
    std::unique_ptr<Mem> backend;
    size_t nextpos = 0;

  public:
    OneWriteWoram(decltype(backend) bp) :backend(std::move(bp)) { }

    size_t remaining_writes() const
    { return maxwrites() - nextpos; }

    // note: index is ignored
    void load(size_t, size_t position, byte* buf) const
    { backend->load(position, buf); }

    // note: index and posmap are ignored
    template <typename PM>
    size_t store(size_t, const byte* buf, const PM&) {
      check_length(nextpos < maxwrites(), "too many writes in OneWriteWoram");
      backend->store(nextpos, buf);
      return nextpos++;
    }

    template <typename PM>
    void dummy_write(const PM&) {
      byte buf[blocksize()] = {0};
      check_length(nextpos < maxwrites(), "too many writes in OneWriteWoram");
      backend->store(nextpos++, buf);
    }

    void flush() { backend->flush(); }
};
// type trait
template <typename BMT, size_t N>
struct is_plain_woram<OneWriteWoram<BMT,N>> :public std::true_type { };

struct OneWriteWoramTrait {
  template <typename MemT, size_t N>
  struct Factory {
    using T = OneWriteWoram<MemT, N>;

    static auto create(std::unique_ptr<MemT> mp)
    { return std::make_unique<T>(std::move(mp)); }
  };

  template <size_t B, size_t N, size_t M>
  static constexpr size_t pmax() { return M-1; }

  // gets the "preferred" backend size if the frontend size is N
  static constexpr size_t prefsize(size_t N)
  { return 10*N; }
};


// modeled on Memory<BMT::blocksize(),N>
// BMT should be a model of Memory
template <typename BMT, size_t N=BMT::size()>
class TrivialWoram {
  static_assert(BMT::size() >= N, "need at least N blocks in the backend");
  public:
    static constexpr size_t blocksize() { return BMT::blocksize(); }
    static constexpr size_t size() { return N; }
    static constexpr size_t used(size_t) { return size(); }

    using Mem = BMT;

  private:
    std::unique_ptr<Mem> backend;

  public:
    TrivialWoram(decltype(backend) bp) :backend(std::move(bp)) { }

    void load(size_t index, byte* buf) const
    { backend->load(index, buf); }

    void store(size_t index, const byte* buf) {
      check_range(index, size(), "invalid index in TrivialWoram store");
      byte temp[blocksize()];
      for (size_t curpos=0; curpos < size(); ++curpos) {
        if (curpos == index) {
          backend->store(curpos, buf);
        } else {
          backend->load(curpos, temp);
          backend->store(curpos, temp);
        }
      }
    }

    void flush() { backend->flush(); }
};
// type trait
template <typename BMT, size_t N>
struct is_memory<TrivialWoram<BMT,N>> :public std::true_type { };


} // namespace woram

#endif // WORAM_WORAM_H
