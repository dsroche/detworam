#include <iostream>
#include <memory>
#include <detworam/filemem.h>
#include "memtest.h"

using namespace std;
using namespace detworam;

template <size_t B, size_t N>
struct Fact {
  using T = FileMem<B,N>;
  static auto create() { return std::make_unique<T>("tenmeg.dat"); }
};


int main() {
  // generic tests with different sizes
  if (test_mem<Fact>()()) return 1;

  cout << "pass" << endl;
  return 0;
}
