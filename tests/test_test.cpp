#include <iostream>

#include <woram/common.h>
#include <woram/errors.h>
#include <woram/bytenum.h>
#include <woram/memory.h>
#include <woram/filemem.h>
#include <woram/posmap.h>
#include <woram/woram.h>
#include <woram/detworam.h>
#include <woram/split.h>
#include <woram/pack.h>
#include <woram/recursive.h>
#include <woram/triepm.h>
#include <woram/crypto.h>

#include <fstream>
#include <cstring>
#include <stdexcept>
#include <unordered_map>
#include <string>

using namespace std;
using namespace woram;

#define RUN(STR,IND) do { \
  strncpy(s, STR, ss); \
  ind = IND; \
  cout << "Storing \"" << s << "\" at index " << ind << endl; \
  m.store(ind, s); \
  check[ind] = s; \
  cout << "Contents:" << endl; \
  for (size_t i=0; i<m.size(); ++i) { \
    m.load(s, i); \
    s[ss-1] = '\0'; \
    cout << "  " << i << ": " << s << endl; \
  } \
  cout << "Checking " << check.size() << " items..." << endl; \
  for (const auto& kvp : check) { \
    m.load(s, kvp.first); \
    if (kvp.second != s) { \
      cout << "ERROR index " << kvp.first << " expected " << kvp.second << endl; \
      return 1; \
    } \
  } \
  /*cout << "posmap:"; for (size_t i=0; i<m.actual.posmap.size(); ++i) cout << ' ' << i << ':' << m.actual.posmap.load(i); cout << endl; */ \
  cout << endl; \
} while(false);


int main() {
  /* decls
   */
  {
    LocalMem<10,3> m1;
    ofstream ns("/dev/null");
    FileMem<50,90> m3("tenmeg.dat");
    LocalPosMap<40,1000> m5;
    ChunkSplit<LocalMem<256,80>,40,37>::create(nullptr);
    PackMem<LocalMem<128,50>, 33, 121> m9(nullptr);
    // SplitPMWoram<decltype(m3), 20, DefDetWoram, PackPosMap> m13("tenmeg.dat");
    // typename RecWoType<DefDetWoram>::WT<decltype(m3), 15> m14("tenmeg.dat");
  }

  cout << "pass" << endl; return 0;
  cout << endl;

  /* Memory
  constexpr size_t B = 512;
  constexpr size_t M = 1000;
  constexpr size_t N = 300;
  constexpr size_t Branch = 2;

  showinfo() = true;
  using Mem = LocalMem<B,M>;
  cout << "enter key: " << flush;
  cin >> (char*)crypto_key();
  auto m = DetWoCryptTrie<Branch>::Factory<Mem, N>::create(std::make_unique<Mem>());

  while (true) {
    char data[B] = {0};
    size_t ind;
    cout << "Enter index and data: " << flush;
    if (!(cin >> ind >> data)) break;
    m->store(ind, (byte*)data);
    cout << "Enter index to load: " << flush;
    if (!(cin >> ind)) break;
    m->load(ind, (byte*)data);
    cout << "Loaded: " << data << endl;
  }
  cout << endl;
  */

  /* PositionMap
  using M = LocalMem<64,10>;
  auto pm = TriePMTrait<OneWriteWoramTrait>::Factory<M, 3, 10000>::create(make_unique<M>());
  while (true) {
    size_t ind, pos;
    cout << "Enter index: ";
    if (!(cin >> ind)) break;
    pos = pm->load(ind);
    cout << "Current position: " << pos << endl;
    cout << "new position: ";
    if (!(cin >> pos)) break;
    pm->store(ind, pos);
    cout << endl;
  }
  */

  /* Split (defunct)
  constexpr size_t B = 6;
  constexpr size_t K = 4;

  using Full = Bits<B*K>;
  Full b;
  cout << "enter " << b.num_bytes() << " bytes: ";
  cin >> b;

  cout << "part 0 is " << make_split<B,0>(b) << endl;
  cout << "part 1 is " << make_split<B,1>(b) << endl;
  cout << "part 2 is " << make_split<B,2>(b) << endl;
  cout << "part 3 is " << make_split<B,3>(b) << endl;
  cout << "total is " << b << endl;
  cout << endl;

  auto s = make_split<B,1>(b);
  cout << "enter " << s.num_bytes() << " bytes for part 1: ";
  cin >> s;
  cout << "part 0 is " << make_split<B,0>(b) << endl;
  cout << "part 1 is " << make_split<B,1>(b) << endl;
  cout << "part 2 is " << make_split<B,2>(b) << endl;
  cout << "part 3 is " << make_split<B,3>(b) << endl;
  cout << "total is " << b << endl;
  */

  /* Nums
  NumPack<3,5,10> np;
  cout << np.count() << endl;
  cout << np.totalbits() << endl;
  cout << "3 numbers: ";
  size_t a,b,c;
  cin >> a >> b >> c;
  cout << np.pack(a,b,c) << endl;
  cout << BitNum<np.maxval()>(np.pack(a,b,c)) << endl;

  cout << "middle number was: " << np.extract<1>(np.pack(a,b,c)) << endl;
  cout << "(which is less than " << np.getmax<1>() << ')' << endl;

  auto all = np.unpack(np.pack(a,b,c));
  cout << "all of em:"; for (auto x : all) cout << ' ' << x; cout << endl;

  cout << "-------" << endl;

  BitNum<13> bn;
  size_t x;
  cout << bn.get_num() << endl;
  cin >> x;
  bn = x;
  cout << bn.get_num() << endl;
  cin >> x;
  bn.set_num(x);
  cout << bn.get_num() << endl;
  cout << bn << endl;

  BitNum<31> bn2;
  bn2.fill(true);
  cout << "filled: " << bn2 << endl;
  bn.fill(true); // should cause exception
  */

  return 0;
}
