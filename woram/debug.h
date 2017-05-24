#ifndef WORAM_DEBUG_H
#define WORAM_DEBUG_H

#ifdef WORAM_DEBUG
#include <iostream>
#include <iomanip>
#endif

#include <tuple>

#include <woram/common.h>

namespace woram {

#ifdef WORAM_DEBUG

#define werr std::cerr

inline std::ostream& operator << (std::ostream& out, const byte* b) {
  auto flags = out.flags();
  auto fill = out.fill();
  for (int i=0; i<4; ++i) {
    out << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b[i]);
  }
  out.flags(flags);
  out.fill(fill);
  return out;
}

#else // not WORAM_DEBUG

using werr_t = std::tuple<>;

template <typename T>
inline werr_t operator << (werr_t out, const T&) { return out; }

#define werr (werr_t{})

#endif // WORAM_DEBUG

}

#endif // WORAM_DEBUG_H
