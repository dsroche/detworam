#include <iostream>
#include <memory>

#include <woram/memory.h>
#include <woram/triepm.h>
#include <woram/woram.h>
#include <woram/detworam.h>

#include "pmtest.h"

using namespace std;
using namespace woram;

template <typename PWTrait, size_t B, size_t MemN>
struct Fact {
  template <size_t N, size_t M>
  struct F {
    using Mem = LocalMem<B,MemN>;
    using Fac = typename TriePMTrait<PWTrait>::template Factory<Mem, N, M>;
    using T = typename Fac::T;
    static auto create() { return Fac::create(std::make_unique<Mem>()); }
  };
};

int main() {
  // generic tests with different sizes
  if (test_pm<Fact<OneWriteWoramTrait,100,100>::template F>()()) return 1;
  if (test_pm<Fact<DetWoramTrait<>,100,100>::template F>()()) return 2;
  if (test_pm<Fact<DetWoramTrait<>,20,50>::template F>()()) return 3;

  cout << "pass" << endl;
  return 0;
}
