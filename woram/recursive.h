#ifndef WORAM_RECURSIVE_H
#define WORAM_RECURSIVE_H

#include <woram/posmap.h>
#include <woram/pack.h>
#include <woram/split.h>

namespace woram {

// some traits for a recursive Woram built on the given type and backend mem
template <typename BMT, typename PWTrait,
          template <typename,size_t,size_t> class ST = ChunkSplit>
struct RecWoTrait {
  template <typename M, size_t X, size_t Y = (M::size() - X)>
  using Splitter = ST<M,X,Y>;

  template <size_t N>
  static constexpr size_t pmblocks()
  { return ppm_size(BMT::blocksize(), N, PWTrait::template Factory<BMT,N>::T::pmax()); }

  template <size_t N>
  static constexpr size_t prefsize()
  { return (N <= 1) ? N : (pmsize<N>() + PWTrait::prefsize(N)); }

  template <size_t N>
  static constexpr size_t pmsize()
  { return prefsize<pmblocks<N>()>(); }
};


// RecWoramFactory is used to define types for and create Recursive woram instances.
// this is the forward declaration with default template values
template <typename BMT,
          size_t N,
          typename PWTrait,
          typename RecTrait = RecWoTrait<BMT,PWTrait>,
          bool basecase = (ppm_size(BMT::blocksize(), N, PWTrait::template Factory<BMT,N>::T::pmax()) <= BMT::blocksize())
         >
struct RecWoramFactory;

// this is the general case (basecase == false)
template <typename BMT,
          size_t N,
          typename PWTrait,
          typename RecTrait,
          bool basecase>
struct RecWoramFactory {
  static_assert(is_memory<BMT>::value,
      "BMT template to RecWoramFactory must be a memory");

  // first part is for the posmap
  using Split = typename RecTrait::template Splitter<BMT, RecTrait::template pmsize<N>()>;

  using WoMem = typename Split::Mem1;
  using WoFac = typename PWTrait::template Factory<WoMem, N>;
  using WT = typename WoFac::T;

  using PosMem = typename Split::Mem0;
  using RWF = RecWoramFactory<
    PosMem, // BMT
    ppm_size(BMT::blocksize(), N, WT::pmax()), // N
    PWTrait,
    RecTrait>;
  using PMT = PackPosMap<typename RWF::T, N, WT::pmax()>;

  using T = PMWoram<WT, PMT>;

  static auto create(std::unique_ptr<BMT> mp) {
    auto halves = Split::create(std::move(mp));
    return std::make_unique<T>(
        WoFac::create(std::move(std::get<1>(halves))),
        std::make_unique<PMT>(RWF::create(std::move(std::get<0>(halves)))));
  }
};

// basecase that uses TrivialWoram for the position map
template <typename BMT,
          size_t N,
          typename PWTrait,
          typename RecTrait>
struct RecWoramFactory<BMT,N,PWTrait,RecTrait,true> {
  static_assert(is_memory<BMT>::value,
      "BMT template to RecWoramFactory must be a memory");

  // first part is for the posmap
  using Split = typename RecTrait::template Splitter<BMT, RecTrait::template pmblocks<N>()>;

  using WoMem = typename Split::Mem1;
  using WoFac = typename PWTrait::template Factory<WoMem, N>;
  using WT = typename WoFac::T;

  using PosMem = typename Split::Mem0;
  using PMT = PackPosMap<TrivialWoram<PosMem>, N, WT::pmax()>;

  using T = PMWoram<WT, PMT>;

  static auto create(std::unique_ptr<BMT> mp) {
    auto halves = Split::create(std::move(mp));
    return std::make_unique<T>(
        WoFac::create(std::move(std::get<1>(halves))),
        std::make_unique<PMT>(
          std::make_unique<TrivialWoram<PosMem>>(
            std::move(std::get<0>(halves)))));
  }
};


} // namespace woram

#endif // WORAM_RECURSIVE_H
