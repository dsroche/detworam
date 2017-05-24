#ifndef WORAM_MEMTEST_H
#define WORAM_MEMTEST_H

#include <utility>
#include <unordered_set>

#include <woram/common.h>
#include <woram/memory.h>
#include <woram/debug.h>

#include "tutil.h"

using namespace std;
using namespace woram;

// sizes for testing must be specified at compile-time
// since the size is a template parameter for the Bits class.
constexpr size_t NS = 20;
constexpr array<pair<size_t,size_t>,NS> sizes =
  {{{50,1}, {47,2}, {43,3}, {88,4}, {84,8}, {81,16}, {16,32}, {26,35}, {91,40}, {85,41},
    {53,43}, {6,53}, {78,58}, {25,64}, {95,66}, {66,69}, {39,89}, {65,95}, {33,97}, {29,99}}};

// Function to perform random tests of the given Memory class.
// It uses the next bitlength and size from the compile-time sizes
// array and performs a number of checks on that size.
// Note this has to be a functor class to allow partial template
// specialization in the base case.
template <template <size_t,size_t> class MemFac, size_t I=0>
struct test_mem {
  static constexpr auto B = get<I>(sizes).first;
  static constexpr auto N = get<I>(sizes).second;

  MemFac<B,N> mf;

  int operator() () {
    using Mem = typename decltype(mf.create())::element_type;
    using Block = array<byte, B>;

    TEST(is_memory<Mem>::value);
    TEST(Mem::blocksize() == B);
    TEST(Mem::size() == N);

    bool debug = (N == 3192);
    debug = false; // TODO
    if (debug) cerr << "B=" << B << " N=" << N << endl;

    for (size_t count=0; count<100; ++count) {
      // declare actual and check memories
      if (debug) cerr << "count=" << count << endl;
      auto m = mf.create();
      array<Block, Mem::size()> check;
      Block temp;

      // randomly assign half of the mem
      unordered_set<size_t> assigned;
      while (assigned.size() < Mem::size()/2) {
        size_t index = rand_pos(m->size()-1);
        assigned.emplace(index);
        set_rand(temp);
        m->store(index, temp.data());
        if (debug) cerr << "wrote " << (byte*)temp.data() << " to index " << index << "\n";
        check[index] = temp;
      }

      // check those assignments
      for (auto index : assigned) {
        m->load(index, temp.data());
        if (debug) cerr << "loaded " << (byte*)temp.data() << " from index " << index << "\n";
        TEST(temp == check[index]);
      }

      // re-assign everything
      array<size_t, Mem::size()> inds;
      for (size_t i=0; i<inds.size(); ++i) inds.at(i) = i;
      shuffle(inds);
      for (size_t index : inds) {
        set_rand(temp);
        m->store(index, temp.data());
        if (debug) cerr << "wrote " << (byte*)temp.data() << " to index " << index << "\n";
        check[index] = temp;
      }

      // check everything
      shuffle(inds);
      for (size_t index : inds) {
        m->load(index, temp.data());
        if (debug) cerr << "loaded " << (byte*)temp.data() << " from index " << index << "\n";
        TEST(temp == check[index]);
      }
    }

    return test_mem<MemFac, I+1>()();
  }
};

// recursion base case
template <template <size_t,size_t> class MemFac>
struct test_mem<MemFac,NS> {
  int operator() () const { return 0; }
};

#endif // WORAM_MEMTEST_H
