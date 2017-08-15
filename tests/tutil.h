#ifndef WORAM_TUTIL_H
#define WORAM_TUTIL_H

#include <cstring>
#include <random>
#include <limits>
#include <type_traits>
#include <algorithm>

#define TEST(cond) do { \
  if (! (cond)) { \
    cout << "FAIL line " << __LINE__ << " in function " << __func__ << ":" << endl; \
    cout << '\t' << #cond << endl; \
    return 1; \
  } \
} while (0)

namespace detworam {

using RGT = std::mt19937_64;
inline RGT& rgen() {
  static RGT rgobj;
  return rgobj;
}

template <typename T>
T get_rand(T min=std::numeric_limits<T>::min(),
           T max=std::numeric_limits<T>::max())
{
  std::uniform_int_distribution<T> dis(min,max);
  return dis(rgen());
}

template <typename T>
T rand_pos(T max=std::numeric_limits<T>::max()) {
  std::uniform_int_distribution<T> dis(0,max);
  return dis(rgen());
}

inline bool rand_bit() { return get_rand(0,1); }

template <typename T>
void set_rand(T& item) {
  static_assert(std::is_trivial<T>::value, "Can only call set_rand on trivial types");
  unsigned char* raw = reinterpret_cast<unsigned char*>(&item);
  for (size_t i=0; i < sizeof item; ++i) {
    raw[i] = get_rand<unsigned char>();
  }
}

template <typename T>
void shuffle(T& arr) {
  size_t n = arr.size();
  for (size_t i=0; i<n-1; ++i) {
    std::swap(arr.at(i), arr.at(get_rand(i, n-1)));
  }
}

// POD ("plain old data") class to use for testing
struct TestA {
  constexpr static size_t ylen = 9;
  int x;
  char y[ylen];
  double z;

  bool operator== (const TestA& other) const {
    return x == other.x && strncmp(y, other.y, ylen) == 0 && z == other.z;
  }

  void rand() {
    set_rand(*this);
    x = get_rand<int>();
    size_t yn = rand_pos(8);
    size_t i;
    for (i=0; i<yn; ++i) y[i] = get_rand('0','~');
    y[i++] = '\0';
    for (; i<ylen; ++i) y[i] = get_rand<char>();
    z = static_cast<double>(get_rand<long>()) / get_rand<long>();
  }
};

} // namespace detworam

#endif // WORAM_TUTIL_H
