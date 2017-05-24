#include <iostream>
#include <memory>
#include <woram/pack.h>
#include <woram/memory.h>
#include "memtest.h"
#include "pmtest.h"

using namespace std;
using namespace woram;

template <size_t B, size_t N>
struct MemFact {
  using M = LocalMem<100,100>;
  using T = PackMem<M,B,N>;
  static auto create() { return std::make_unique<T>(std::make_unique<M>()); }
};

template <size_t N, size_t Max>
struct PMFact {
  using M = LocalMem<100,100>;
  using T = PackPosMap<M,N,Max>;
  static auto create() { return std::make_unique<T>(std::make_unique<M>()); }
};

int main() {
  // generic tests with different sizes

  if (test_mem<MemFact>()()) return 1;
  if (test_pm<PMFact>()()) return 2;

  cout << "pass" << endl;
  return 0;
}
