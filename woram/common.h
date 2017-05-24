#ifndef WORAM_COMMON_H
#define WORAM_COMMON_H

#include <cstddef>
#include <limits>
#include <type_traits>
#include <memory>
#include <woram/errors.h>

namespace woram {

using std::size_t;

using byte = unsigned char;

inline bool& showinfo() {
  static bool showit = false;
  return showit;
}

template <typename Int1, typename Int2>
constexpr decltype(((Int1)0) + ((Int2)0)) ceiling(const Int1& num, const Int2& denom)
{ return 1 + (num - 1) / denom; }

inline constexpr unsigned bits_per_byte()
{ return std::numeric_limits<unsigned char>::digits; }

// returns the number of bits needed to represent n
inline constexpr unsigned bitlen(size_t n)
{ return n == 0 ? 0 : (bitlen(n/2) + 1); }

// returns a bitmask of the given length
inline constexpr size_t bitmask(unsigned len) {
  return (len >= std::numeric_limits<size_t>::digits)
    ? std::numeric_limits<size_t>::max()
    : ((1UL << len) - 1);
}

inline constexpr size_t bytemask(unsigned len)
{ return bitmask(bits_per_byte() * len); }

// returns the i'th bit of the buffer
inline unsigned get_bit(const byte* buf, unsigned i)
{ return (buf[i / bits_per_byte()] >> (i % bits_per_byte())) & 1; }

// returns the index of first mismatching bit
template <size_t B>
size_t mismatch_bit(const byte* b1, const byte* b2) {
  size_t res = 0;
  for (size_t i=0; i < B; ++i) {
    if (b1[i] != b2[i]) {
      byte x = b1[i] ^ b2[i];
      while ((x & 1) == 0) {
        x >>= 1;
        ++res;
      }
      break;
    }
    res += bits_per_byte();
  }
  return res;
}

// TYPE TRAITS

// Full (position map included) Write-Only ORAM trait
template <typename T> struct is_woram :public std::false_type { };

// Plaim (position map free) Write-Only ORAM trait
template <typename T> struct is_plain_woram :public std::false_type { };

// Memory trait (every full Woram is a Memory)
template <class T> struct is_memory :public is_woram<T> { };

// Position Map trait
template <typename T> struct is_posmap :public std::false_type { };

// Split memory trait
template <typename T> struct is_split :public std::false_type { };

} // namespace woram

#endif // WORAM_COMMON_H
