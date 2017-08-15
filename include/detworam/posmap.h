#ifndef WORAM_POSMAP_H
#define WORAM_POSMAP_H

#include <detworam/common.h>
#include <detworam/bytenum.h>
#include <detworam/memory.h>
#include <detworam/errors.h>

namespace detworam {

// this is an ARCHETYPE - don't derive from it
// MemT is the underlying backend memory type
// N is the number of positions stored
// M is the largest position value
template <typename MemT, size_t N, size_t M>
class PositionMap final {
  public:
    static constexpr size_t size() { return N; }
    static constexpr size_t pmax() { return M; }

    // some value that indicates an invalid pointer
    static constexpr size_t nptr() { return pmax()+1; }

    size_t load(size_t index) const;

    void store(size_t index, size_t pos);

    void flush();
};
// declare this trait for every position map
template <typename MemT, size_t N, size_t M>
struct is_posmap<PositionMap<MemT,N,M>> :public std::true_type { };


// trivial position map stored in memory
template <size_t N, size_t M>
class LocalPosMap {
  public:
    static constexpr size_t size() { return N; }
    static constexpr size_t pmax() { return M; }
    static constexpr size_t nptr() { return pmax()+1; }

  private:
    woram_array<size_t, size()> backend;

  public:
    LocalPosMap() {
      backend.fill(nptr());
    }

    size_t load(size_t index) const {
      return backend.at(index);
    }

    void store(size_t index, size_t pos) {
      check_range(pos, pmax(), "position value in posmap store");
      backend.at(index) = pos;
    }

    void flush() { }
};
// type trait
template <size_t N, size_t M>
struct is_posmap<LocalPosMap<N,M>> :public std::true_type { };

} // namespace detworam

#endif // WORAM_POSMAP_H
