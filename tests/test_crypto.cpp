#include <iostream>
#include <memory>
#include <woram/woram.h>
#include <woram/detworam.h>
#include <woram/crypto.h>
#include <woram/pack.h>
#include <woram/posmap.h>
#include "memtest.h"

using namespace std;
using namespace woram;

constexpr unsigned K = 32;
constexpr unsigned cblock = 16;

template <typename PWTrait>
struct CtrFact {
  template <size_t B, size_t N>
  struct Fact {
    static constexpr size_t growb()
    { return B + ((cblock - (B % cblock)) % cblock); }
    using Mem = LocalMem<growb(),PWTrait::prefsize(N)>;
    using C = CtrCrypt<Mem, K>;
    using W = typename PWTrait::template Factory<C,N>::T;
    using PM = LocalPosMap<N, W::pmax()>;
    using WW = PMWoram<W,PM>;
    using T = PackMem<WW, B, N>;
    static auto create() {
      auto wp = std::make_unique<WW>(
          PWTrait::template Factory<C,N>::create(std::make_unique<C>(std::make_unique<Mem>())),
          std::make_unique<PM>());
      byte block[WW::blocksize()] = {0};
      for (size_t i=0; i<wp->size(); ++i) {
        wp->store(i, block);
      }
      return std::make_unique<T>(std::move(wp));
    }
  };
};

template <typename PWTrait>
struct RegFact {
  template <size_t B, size_t N>
  struct Fact {
    static constexpr size_t growb()
    { return B + ((cblock - (B % cblock)) % cblock); }
    using Mem = LocalMem<growb(),PWTrait::prefsize(N)>;
    using W = typename PWTrait::template Factory<Mem,N>::T;
    using PM = LocalPosMap<N, W::pmax()>;
    using WW = PMWoram<W,PM>;
    using T = PackMem<WW, B, N>;
    static auto create() {
      auto wp = std::make_unique<WW>(
          PWTrait::template Factory<Mem,N>::create(std::make_unique<Mem>()),
          std::make_unique<PM>());
      byte block[WW::blocksize()] = {0};
      for (size_t i=0; i<wp->size(); ++i) {
        wp->store(i, block);
      }
      return std::make_unique<T>(std::move(wp));
    }
  };
};

template <size_t B, size_t N>
struct RandFact {
  static constexpr size_t growb()
  { return B + ((cblock - (B % cblock)) % cblock); }
  using Mem = LocalMem<growb()+cblock, N>;
  using C = RandCrypt<Mem, K>;
  using T = PackMem<C, B, N>;
  static auto create() {
    return std::make_unique<T>(std::make_unique<C>(std::make_unique<Mem>()));
  }
};

int main() {
  // initialize the key
  crypto_key<K>()[0] = 61;
  for (unsigned i=1; i < K; ++i) {
    crypto_key<K>()[i] = (crypto_key<K>()[i-1] * 173) & 0xff;
  }

  // generic tests with different sizes
  if (test_mem<RandFact>()()) return 1;
  if (test_mem<CtrFact<OneWriteWoramTrait>::template Fact>()()) return 2;
  if (test_mem<RegFact<DetWoramTrait<CryptSplitType<K>::template CtrT>>::template Fact>()()) return 3;

  cout << "pass" << endl;
  return 0;
}
