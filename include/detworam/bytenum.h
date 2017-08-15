#ifndef WORAM_BYTENUM_H
#define WORAM_BYTENUM_H

#include <cstring>
#include <detworam/common.h>

namespace detworam {

// try to determine endianness
// source: http://stackoverflow.com/a/27054190/1008966
#if defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN || \
    defined(__BIG_ENDIAN__) || \
    defined(__ARMEB__) || \
    defined(__THUMBEB__) || \
    defined(__AARCH64EB__) || \
    defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__)
// It's a big-endian target architecture
constexpr bool is_bigend() { return true; }
#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN || \
    defined(__LITTLE_ENDIAN__) || \
    defined(__ARMEL__) || \
    defined(__THUMBEL__) || \
    defined(__AARCH64EL__) || \
    defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__)
// It's a little-endian target architecture
constexpr bool is_bigend() { return false; }
#else
#warning "Could not determine endianness at compile time; resorting to run-time check"
bool is_bigend() {
  size_t tnum = 1;
  char tch;
  std::memcpy(&tch, &tnum, 1);
  return tch == 0;
}
#endif

// returns the number of bytes needed to represent n
constexpr unsigned bytelen(size_t n)
{ return n == 0 ? 0 : (bytelen(n >> bits_per_byte()) + 1); }

// stores n into the given byte array
template <unsigned BUFLEN>
void storenum(unsigned char* buf, size_t n) {
  static_assert(BUFLEN <= sizeof(size_t), "numbers are not that big!");
  constexpr size_t BYTEMASK = (1UL << bits_per_byte()) - 1;
  if (is_bigend()) {
    for (unsigned i=0; i < BUFLEN; ++i) {
      buf[i] = n & BYTEMASK;
      n >>= bits_per_byte();
    }
  } else {
    std::memcpy(buf, &n, BUFLEN);
  }
}

// reads n from the given byte array
template <unsigned BUFLEN>
size_t getnum(const unsigned char* buf) {
  static_assert(BUFLEN <= sizeof(size_t), "numbers are not that big!");
  size_t res = 0;
  if (is_bigend()) {
    for (unsigned i=0; i < BUFLEN; ++i) {
      res |= (buf[i] << (bits_per_byte() * i));
    }
  } else {
    std::memcpy(&res, buf, BUFLEN);
  }
  return res;
}

} // namespace detworam

#endif // WORAM_BYTENUM_H
