#ifndef WORAM_ERRORS
#define WORAM_ERRORS

#ifndef WORAM_NOEXCEPT
#include <stdexcept>
#include <array>
#endif

namespace woram {

#ifndef WORAM_NOEXCEPT

template <typename Int>
void check_range(Int& val, const Int& min, const Int& max, const char* msg) {
  if (val < min || val > max) throw std::out_of_range(msg);
}

inline void check_length(bool ok, const char* msg) {
  if (!ok) throw std::length_error(msg);
}

template <typename T>
void check_equal(T& val, const T& other, const char* msg) {
  if (val != other) throw std::runtime_error(msg);
}

template <class T, std::size_t N>
using woram_array = std::array<T,N>;

#else // WORAM_NOEXCEPT

template <typename Int>
void check_range(Int&, const Int&, const Int&, const char*) { }

inline void check_length(bool, const char*) { }

template <typename T>
inline void check_equal(T&, const T&, const char*) { }

template <class T, std::size_t N>
struct woram_array {
  using value_type = T;
  using size_type = std::size_t;
  using reference = value_type&;
  using const_reference = const value_type&;

  T _data[N];

  constexpr bool empty() const { return N == 0; }
  constexpr size_type size() const { return N; }

  reference front() { return _data[0]; }
  const_reference front() const { return _data[0]; }

  reference back() { return _data[N-1]; }
  const_reference back() const { return _data[N-1]; }

  inline reference at(size_type pos) { return _data[pos]; }
  inline const_reference at(size_type pos) const { return _data[pos]; }

  const T* begin() const { return &_data[0]; }
  const T* end() const { return &_data[0] + N; }

  T* begin() { return &_data[0]; }
  T* end() { return &_data[0] + N; }

  reference operator[](size_type pos) { return _data[pos]; }
  const_reference operator[](size_type pos) const { return _data[pos]; }

  T* data() { return &front(); }
  const T* data() const { return &front(); }

  void fill(const T& value)
  { for (size_t i=0; i < N; ++i) _data[i] = value; }
};

#endif // WORAM_NOEXCEPT

// overloaded for convenience
template <typename Int>
inline void check_range(Int& val, const Int& max, const char* msg)
{ check_range<Int>(val, 0, max, msg); }

} // namespace woram

#endif // WORAM_ERRORS
