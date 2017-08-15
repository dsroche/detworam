#include <iostream>
#include <memory>
#include <detworam/woram.h>
#include <detworam/detworam.h>
#include <detworam/recursive.h>
#include "memtest.h"

using namespace std;
using namespace detworam;

template <typename PWTrait>
struct PWF {
  template <size_t B, size_t N>
  struct Fact {
    using Mem = LocalMem<B, 10*N>;
    using RWF = RecWoramFactory<Mem, N, PWTrait>;
    using T = typename RWF::T;
    static auto create() { return RWF::create(std::make_unique<Mem>()); }
  };
};

int main() {
  // generic tests with different sizes
  if (test_mem<PWF<OneWriteWoramTrait>::Fact>()()) return 1;
  if (test_mem<PWF<DetWoramTrait<>>::Fact>()()) return 2;

  cout << "pass" << endl;
  return 0;
}
