#ifndef WORAM_MEMORY_H
#define WORAM_MEMORY_H

#include <type_traits>
#include <algorithm>

#include <detworam/common.h>
#include <detworam/errors.h>

namespace detworam {

// this is an ARCHETYPE - don't derive from it
template <size_t B, size_t N>
class Memory final {
  public:
    static constexpr size_t blocksize() { return B; }
    static constexpr size_t size() { return N; }

    void load(size_t index, byte* buf) const;

    void store(size_t index, const byte* buf);

    void flush();
};
// declare this trait for every Memory type
template <size_t B, size_t N>
struct is_memory<Memory<B,N>> :public std::true_type { };


// modeled on Memory<B,N>
// This uses a simple array to store the data in RAM
template <size_t B, size_t N>
class LocalMem {
  public:
    static constexpr size_t blocksize() { return B; }
    static constexpr size_t size() { return N; }

    using BlockT = woram_array<byte, blocksize()>;

  private:
    woram_array<BlockT, size()> backend = {{}};

  public:
    void load(size_t index, byte* buf) const {
      check_range(index, size()-1, "index in LocalMem load");
      std::copy(backend.at(index).begin(), backend.at(index).end(), buf);
    }

    void store(size_t index, const byte* buf) {
      check_range(index, size()-1, "index in LocalMem store");
      std::copy(buf, buf + blocksize(), backend.at(index).begin());
    }

    void flush() { }
};

// type trait
template <size_t B, size_t N>
struct is_memory<LocalMem<B,N>> :public std::true_type { };


} // namespace detworam

#endif // WORAM_MEMORY_H
