#include <iostream>
#include <memory>
#include <algorithm>
#include <detworam/woram.h>
#include <detworam/detworam.h>
#include <detworam/triepm.h>
#include <detworam/crypto.h>
#include <detworam/split.h>
#include "memtest.h"

using namespace std;
using namespace detworam;

constexpr unsigned K = 32;
constexpr unsigned cblock = 16;

template <typename PWTrait, unsigned BR=2, template <typename,size_t,size_t> class Splitter=ChunkSplit, typename TPMTrait=TriePMTrait<PWTrait,BR>>
struct PWF {
  template <size_t B, size_t N>
  struct Fact {
    static constexpr size_t memblock() {
      constexpr auto s0 = std::max(B, 100ul);
      constexpr auto mod = s0 % cblock;
      return (mod == 0) ? s0 : (s0 + cblock - mod);
    }
    using Mem = LocalMem<memblock(), 10*N + 10>;
    using F = typename TPMWTrait<BR,Splitter>::template Factory<Mem, std::max(N,10ul), PWTrait, TPMTrait>;
    using T = PackMem<typename F::T, B, N>;
    static auto create() { return std::make_unique<T>(F::create(std::make_unique<Mem>())); }
  };
};

int main() {
  // initialize the key
  crypto_key<K>()[0] = 171;
  for (unsigned i=1; i < K; ++i) {
    crypto_key<K>()[i] = (crypto_key<K>()[i-1] * 83) & 0xff;
  }

  // generic tests with different sizes
  //if (test_mem<PWF<OneWriteWoramTrait>::Fact>()()) return 1;
  if (test_mem<PWF<DetWoramTrait<>>::Fact>()()) return 2;
  if (test_mem<PWF<DetWoramTrait<CryptSplitType<K>::template CtrT>, 2, CryptSplitType<K>::template RandT, TriePMTrait<DetWoramTrait<>,2>>::Fact>()()) return 3;

  cout << "pass" << endl;
  return 0;
}
