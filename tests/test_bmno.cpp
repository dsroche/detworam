#include <iostream>
#include <memory>
#include <woram/woram.h>
#include <woram/detworam.h>
#include <woram/crypto.h>
#include <woram/pack.h>
#include <woram/posmap.h>
#include <woram/bmno.h>
#include "memtest.h"
#include "pmtest.h"

using namespace std;
using namespace woram;

constexpr unsigned K = 16;
constexpr unsigned cblock = 16;

template <size_t B, size_t N>
struct Fact1 {
  static constexpr size_t growb()
  { return B + ((cblock - (B % cblock)) % cblock); }

  using Mem = LocalMem<std::max(growb(), 128ul), 2*N + 200>;
  using PM = LocalPosMap<N, Mem::size()-1>;
  using W = BMNOWoram<Mem,N,PM>;
  using T = PackMem<W,B,N>;
  static auto create() {
    return std::make_unique<T>(std::make_unique<W>(
        std::make_unique<Mem>(),
        std::make_unique<PM>()
        ));
  }
};

template <size_t N, size_t M>
struct Fact3 {
  using Mem = LocalMem<32, 200ul>;
  using F = bmno_pm_fact<Mem,N,M>;
  using T = typename F::T;
  static auto create() { return F::create(std::make_unique<Mem>()); }
};

template <size_t B, size_t N>
struct Fact4 {
  static constexpr size_t growb()
  { return B + ((cblock - (B % cblock)) % cblock); }

  using Mem = LocalMem<std::max(growb(), 128ul), 5*N + 200>;
  using F = bmno_fact<Mem,N>;
  using T = PackMem<typename F::T,B,N>;
  static auto create() {
    return std::make_unique<T>(F::create(std::make_unique<Mem>()));
  }
};

int main() {
  // initialize the key
  crypto_key<K>()[0] = 61;
  for (unsigned i=1; i < K; ++i) {
    crypto_key<K>()[i] = (crypto_key<K>()[i-1] * 173) & 0xff;
  }

  // generic tests with different sizes
  if (test_mem<Fact1>()()) return 1;
  //if (test_mem<Fact2>()()) return 2;
  if (test_pm<Fact3>()()) return 3;
  if (test_mem<Fact4>()()) return 4;

  cout << "pass" << endl;
  return 0;
}
