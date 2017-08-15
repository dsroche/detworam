#include <iostream>
#include <memory>
#include <detworam/split.h>
#include <detworam/memory.h>
#include "memtest.h"

using namespace std;
using namespace detworam;

template <template <typename,size_t,size_t> class SplitT, size_t Tot, size_t X>
struct Fact {
  template <size_t B, size_t N>
  struct F0 {
    using M = LocalMem<B, Tot>;
    using CS = SplitT<M, N, X>;
    static_assert(is_split<CS>::value, "SplitT must be a splitter");
    using T = typename CS::Mem0;
    static auto create()
    { return std::get<0>(CS::create(std::make_unique<M>())); }
  };
  template <size_t B, size_t N>
  struct F1 {
    using M = LocalMem<B, Tot>;
    using CS = SplitT<M, X, N>;
    static_assert(is_split<CS>::value, "SplitT must be a splitter");
    using T = typename CS::Mem1;
    static auto create()
    { return std::get<1>(CS::create(std::make_unique<M>())); }
  };
};

int main() {
  // generic tests with different sizes
  if (test_mem<Fact<ChunkSplit,150,43>::template F0>()()) return 1;
  if (test_mem<Fact<ChunkSplit,150,43>::template F1>()()) return 1;

  cout << "pass" << endl;
  return 0;
}
