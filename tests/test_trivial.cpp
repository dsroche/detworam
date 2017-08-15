#include <iostream>
#include <detworam/woram.h>
#include "memtest.h"

using namespace std;
using namespace detworam;

template <size_t B, size_t N>
struct Fact {
  using Mem = LocalMem<B, N>;
  using T = TrivialWoram<Mem>;
  static auto create() { return std::make_unique<T>(std::make_unique<Mem>()); }
};

int main() {
  // generic tests with different sizes
  if (test_mem<Fact>()()) return 1;

  cout << "pass" << endl;
  return 0;
}
