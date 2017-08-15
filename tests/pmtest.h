#ifndef WORAM_PMTEST_H
#define WORAM_PMTEST_H

#include <utility>
#include <unordered_map>

#include <detworam/posmap.h>

#include "tutil.h"

using namespace std;
using namespace detworam;

// sizes for testing must be specified at compile-time
// since the size is a template parameter for the PositionMap class.
constexpr size_t PMNS = 20;
constexpr array<pair<size_t,size_t>,PMNS> bounds =
  {{{1035890UL,1}, {1138UL,2}, {28155781UL,3}, {135871944UL,4}, {1UL,5},
  {2281UL,8}, {927749UL,15}, {174790195UL,16}, {338UL,17}, {1162UL,29},
  {25477822UL,33}, {391280UL,39}, {860UL,40}, {197925UL,41}, {22891UL,49},
  {1334393UL,54}, {12574UL,60}, {9074UL,61}, {245090660UL,86},
  {224816087UL,99}}};

// Function to perform random tests of the given PositionMap class.
// It uses the next max index and size from the compile-time bounds
// array and performs a number of checks on that size.
// Note this has to be a functor class to allow partial template
// specialization in the base case.
template <template <size_t,size_t> class PMFac, size_t I=0>
struct test_pm {
  static constexpr size_t max = get<I>(bounds).first;
  static constexpr size_t size = get<I>(bounds).second;

  PMFac<size,max> pmf;

  int operator() () {
    using PM = typename decltype(pmf.create())::element_type;
    TEST(PM::size() == size);
    TEST(PM::pmax() == max);

    for (size_t count=0; count<100; ++count) {
      // declare actual and check memories
      auto m = pmf.create();
      unordered_map<size_t, size_t> check;

      // add some stuff
      for (size_t i=0; i<size/2; ++i) {
	size_t ind = rand_pos(size-1);
	size_t pos = rand_pos(max);
	m->store(ind, pos);
	check[ind] = pos;
      }

      // check everything
      for (const auto& kvp : check) {
        auto pos = m->load(kvp.first);
	TEST(pos == kvp.second);
      }

      // add more stuff
      for (size_t i=0; i<size; ++i) {
	size_t ind = rand_pos(size-1);
	size_t pos = rand_pos(max);
	m->store(ind, pos);
	check[ind] = pos;
      }

      // check everything again
      for (const auto& kvp : check) {
        auto pos = m->load(kvp.first);
	TEST(pos == kvp.second);
      }
    }

    return test_pm<PMFac, I+1>()();
  }
};

// recursion base case
template <template <size_t,size_t> class PMFac>
struct test_pm<PMFac,PMNS> {
  int operator() () { return 0; }
};

#endif // WORAM_PMTEST_H
