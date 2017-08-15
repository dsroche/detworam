#include <iostream>
#include <detworam/woram.h>
#include <detworam/detworam.h>
#include "memtest.h"

using namespace std;
using namespace detworam;

template <typename PWTrait>
struct PWF {
  template <size_t B, size_t N>
  struct Fact {
    using Mem = LocalMem<B, PWTrait::prefsize(N)>;
    using WF = typename PWTrait::template Factory<Mem,N>;
    using PM = LocalPosMap<N, WF::T::pmax()>;
    using T = PMWoram<typename WF::T, PM>;
    static auto create() {
      return std::make_unique<T>(
          PWTrait::template Factory<Mem,N>::create(std::make_unique<Mem>()),
          std::make_unique<PM>());
    }
  };
};

int main() {
  // generic tests with different sizes
  if (test_mem<PWF<OneWriteWoramTrait>::Fact>()()) return 1;
  if (test_mem<PWF<DetWoramTrait<>>::Fact>()()) return 2;

  cout << "pass" << endl;
  return 0;
}
