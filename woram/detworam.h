#ifndef WORAM_DETWORAM_H
#define WORAM_DETWORAM_H

#include <type_traits>
#include <ratio>

#include <woram/common.h>
#include <woram/woram.h>
#include <woram/split.h>
#include <woram/recursive.h>
#include <woram/debug.h>

namespace woram {

// This is a helper class to represent a "pointer" within a
// detworam. which consists of the holding area index,
// a bit index, and a bit value.
// S is the holding area size and B is the blockize in Bytes
template <size_t S, size_t B>
class DWPointer {
  public:
    static constexpr size_t holdsize() { return S; }
    static constexpr size_t blockbits()
    { return bits_per_byte() * B; }

    static constexpr unsigned hold_bits() { return woram::bitlen(holdsize()-1); }
    static constexpr unsigned bind_bits() { return woram::bitlen(blockbits()-1); }

    static constexpr unsigned bitlen() { return hold_bits() + bind_bits() + 1; }
    static constexpr unsigned bytelen()
    { return 1 + (bitlen() - 1) / bits_per_byte(); }

    static constexpr size_t get_hold(size_t ptr)
    { return ptr >> (bind_bits() + 1); }

    static constexpr size_t get_bind(size_t ptr)
    { return (ptr >> 1) & bitmask(bind_bits()); }

    static constexpr unsigned get_bitval(size_t ptr)
    { return ptr & 1; }

    static constexpr size_t build(size_t hold, size_t bind, unsigned bitval)
    { return (((hold << bind_bits()) | bind) << 1) | bitval; }

    static constexpr size_t pmax()
    { return build(holdsize()-1, blockbits()-1, 1); }
};

// modeled on PlainWoram<B, N, psize()>
// LTMemT is the Memory type for the long-term area
// HMemT is the Memory type for the holding area
// The blocksizes of these must match.
template <typename LTMemT, typename HMemT>
class DetWoram {
  static_assert(is_memory<LTMemT>::value, "LTMemT template must be modeled on Memory");
  static_assert(is_memory<HMemT>::value, "HMemT template must be modeled on Memory");
  static_assert(LTMemT::size() >= 1 && HMemT::size() >= 1,
      "must have at least one block in long-term and holding storage");
  static_assert(LTMemT::blocksize() == HMemT::blocksize(),
      "blocksizes of long-term and holding memories must match");

  public:
    using LTMem = LTMemT;
    using HMem = HMemT;

    static constexpr size_t blocksize() { return LTMem::blocksize(); }
    static constexpr size_t size() { return LTMem::size(); }
    static constexpr size_t holdsize() { return HMem::size(); }

    // a pack stores [holding_position, block_index, bit]
    using Ptr = DWPointer<holdsize(), blocksize()>;
    static constexpr size_t pmax() { return Ptr::pmax(); }
    static constexpr size_t nptr() { return pmax()+1; }

  private:
    std::unique_ptr<LTMem> longterm;
    size_t longpos = 0;
    std::unique_ptr<HMem> holding;
    size_t holdpos = 0;

    // performs a single round of long-term writes
    template <typename PM>
    void ltws(const PM& posmap) {
      static_assert(is_posmap<PM>::value,
          "posmap argument to DetWoram store must be a PositionMap");

      // do long-term writes
      double hfrac = static_cast<double>(holdpos+1) / holding->size();
      size_t num_ltw = static_cast<size_t>(hfrac * longterm->size())
                       - longpos;
      byte temp[blocksize()];
      for (size_t i=0; i<num_ltw; ++i) {
        auto pos = posmap.load(longpos);
        load(longpos, (pos == posmap.nptr() ? nptr() : pos), temp);
        longterm->store(longpos, temp);
        if (++longpos == longterm->size()) longpos = 0;
      }
    }

  public:
    DetWoram(decltype(longterm) ltp, decltype(holding) hp)
      :longterm(std::move(ltp)), holding(std::move(hp))
    {
      if (showinfo()) {
        std::cout << "DetWoram created with blocksize " << blocksize()
          << ", size " << size() << ", and holding size " << holdsize() << std::endl;
      }
    }

    void load(size_t index, size_t position, byte* buf) const {
      check_range(index, size()-1, "invalid index in DetWoram load");
      longterm->load(index, buf);
      // check if long-term value matches info in the stored position
      if (position != nptr() &&
          get_bit(buf, Ptr::get_bind(position)) != Ptr::get_bitval(position))
      {
        // long-term is a mismatch, so fetch from holding instead
        holding->load(Ptr::get_hold(position), buf);
      }
    }

    // returns the position
    template <typename PM>
    size_t store(size_t index, const byte* buf, const PM& posmap) {
      // do long-term writes
      ltws(posmap);

      byte temp[blocksize()];

      // determine position info for new block
      longterm->load(index, temp);
      auto diffpos = mismatch_bit<blocksize()>(temp, buf);
      if (diffpos == blocksize() * bits_per_byte()) diffpos = 0;
      auto diffbit = get_bit(buf, diffpos);
      auto newpos = Ptr::build(holdpos, diffpos, diffbit);

      // write block to holding area and position map
      holding->store(holdpos, buf);
      if (++holdpos == holding->size()) holdpos = 0;

      return newpos;
    }

    template <typename PM>
    void dummy_write(const PM& posmap) {
      // do long-term writes
      ltws(posmap);

      // write dummy block to holding area
      byte buf[blocksize()] = {0};
      holding->store(holdpos, buf);
      if (++holdpos == holding->size()) holdpos = 0;
    }

    void flush() {
      longterm->flush();
      holding->flush();
    }
};
// type trait
template <typename LTMemT, typename HMemT>
struct is_plain_woram<DetWoram<LTMemT,HMemT>> :public std::true_type { };

template <template <typename,size_t,size_t> class ST = ChunkSplit, size_t growrat=2>
struct DetWoramTrait {
  template <typename BMT, size_t N>
  struct Factory {
    using Split = ST<BMT, N, BMT::size()-N>;
    using T = DetWoram<typename Split::Mem0, typename Split::Mem1>;
    static auto create(std::unique_ptr<BMT> m) {
      auto halves = Split::create(std::move(m));
      return std::make_unique<T>(
          std::move(std::get<0>(halves)), std::move(std::get<1>(halves)));
    }
  };

  // M is the total backend memory size
  template <size_t B, size_t N, size_t M>
  static constexpr size_t pmax() {
    static_assert(M > N,
        "backend size must be larger than logical size for DetWoram");
    return DWPointer<M-N, B>::pmax();
  }

  static constexpr size_t prefsize(size_t N) { return growrat*N; }
};


} // namespace woram

#endif // WORAM_DETWORAM_H
