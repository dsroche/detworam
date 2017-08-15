#include <iostream>
#include <memory>
#include <detworam/memory.h>
#include "memtest.h"

using namespace std;
using namespace detworam;

template <size_t B, size_t N>
struct Fact {
  using T = LocalMem<B,N>;
  static auto create() { return std::make_unique<T>(); }
};

int main() {
  // generic tests with different sizes
  if (test_mem<Fact>()()) return 1;

  cout << "pass" << endl;
  return 0;
}
