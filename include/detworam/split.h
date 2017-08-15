#ifndef WORAM_SPLIT_H
#define WORAM_SPLIT_H

#include <type_traits>

#include <detworam/common.h>
#include <detworam/memory.h>

namespace detworam {

// this is an ARCHETYPE - don't derive from it
template <typename MemT, size_t X, size_t Y=(MemT::size()-X)>
struct Split final {
  using Backend = MemT;
  static_assert(is_memory<Backend>::value, "template param to Split must be a memory");
  static_assert(X + Y <= Backend::size(), "backend memory is too small to Split");

  // of course you would want to change these...
  using Mem0 = LocalMem<Backend::blocksize(), X>;
  using Mem1 = LocalMem<Backend::blocksize(), Y>;

  static std::tuple<std::unique_ptr<Mem0>, std::unique_ptr<Mem1>>
    create(std::shared_ptr<Backend> bp);
};
// declare this trait for every Split type
template <typename MemT, size_t X, size_t Y>
struct is_split<Split<MemT,X,Y>> :public std::true_type { };


// Modeled on Memory<MemT::blocksize(), N>
// Helper class for ChunkSplit, providing a view of the underlying
// memory at the given offset and length
template <typename MemT, size_t N, size_t O=0>
class OffsetMem {
  static_assert(is_memory<MemT>::value,
      "template param to OffsetMem must be a memory");
  static_assert(O + N <= MemT::size(), "offset or length too great in OffsetMem");

  public:
    using Backend = MemT;
    static constexpr size_t blocksize() { return Backend::blocksize(); }
    static constexpr size_t size() { return N; }
    static constexpr size_t offset() { return O; }

  private:
    std::shared_ptr<Backend> back;

  public:
    OffsetMem(decltype(back) b) :back(b) { }

    void load(size_t index, byte* buf) const
    { back->load(offset() + index, buf); }

    void store(size_t index, const byte* buf)
    { back->store(offset() + index, buf); }

    void flush() { back->flush(); }
};
// type trait
template <typename MemT, size_t N, size_t O>
struct is_memory<OffsetMem<MemT,N,O>> :public std::true_type { };

// Modeled on Split<MemT, X, Y>
// Splits into contiguous "chunks" of blocks
template <typename MemT, size_t X, size_t Y=(MemT::size() - X)>
struct ChunkSplit {
  using Backend = MemT;
  static_assert(is_memory<Backend>::value, "template param to Split must be a memory");
  static_assert(X + Y <= Backend::size(), "backend memory is too small to Split");
  using Mem0 = OffsetMem<Backend, X>;
  using Mem1 = OffsetMem<Backend, Y, X>;

  static auto create(std::shared_ptr<Backend> bp) {
    return std::make_tuple(
        std::make_unique<Mem0>(bp), std::make_unique<Mem1>(bp));
  }
};
// type trait
template <typename MemT, size_t X, size_t Y>
struct is_split<ChunkSplit<MemT,X,Y>> :public std::true_type { };


} // namespace detworam

#endif // WORAM_SPLIT_H
