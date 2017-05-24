#ifndef WORAM_TRIEPM_H
#define WORAM_TRIEPM_H

#include <algorithm>
#include <woram/common.h>
#include <woram/bytenum.h>
#include <woram/pack.h>
#include <woram/detworam.h>
#include <woram/errors.h>
#include <woram/crypto.h>

namespace woram {

// helper class to store a single node in a trie
// K is the branching factor and S is the pointer size
template <unsigned K, unsigned S>
struct TrieNode {
  static constexpr unsigned branching() { return K; }
  static constexpr unsigned ptrsize() { return S; }
  static constexpr unsigned length() { return branching() * ptrsize(); }

  byte data[length()];
  size_t index;

  // get the pointer value from the i'th index
  size_t get(unsigned i) const {
    check_range(i, branching()-1, "index out of range in TrieNode get");
    return getnum<ptrsize()>(data + i*ptrsize());
  }

  // set the value of the i'th pointer
  void set(unsigned i, size_t ptr) {
    check_range(i, branching()-1, "index out of range in TrieNode set");
    storenum<ptrsize()>(data + i*ptrsize(), ptr);
  }
};


// the height of the trie containing N nodes with branching factor K
// Note, height is defined as longest path length from root to leaf,
// and in particular the root node has height 0.
template <size_t N, unsigned K>
constexpr unsigned heighthelp(unsigned ht=0, size_t sofar=0, size_t lastrow=1)
{ return (sofar >= N) ? ht : heighthelp<N,K>(ht+1, sofar + lastrow*K, lastrow*K); }


// helper class for the position map of the trie nodes themselves
// (not the position map of actual positions in the main woram)
// This class is modeled on PositionMap.
// MemT is the backend memory type
// N is the number of trie nodes to store - NOT including the root node
// M is the maximum pointer value that will be stored in a leaf
// K is the branching factor
// PWTrait is a trait class (with a factory) for a PlainWoram
template <typename MemT, size_t N, size_t M, unsigned K, typename PWTrait>
class TrieNodePM {
  static_assert(is_memory<MemT>::value,
      "MemT parameter to TrieNodePM must be a memory");

  public:
    static constexpr size_t numnodes() { return N; }
    static constexpr unsigned branching() { return K; }
    static constexpr unsigned height()
    { return heighthelp<numnodes(), branching()>(); }

    static constexpr size_t maxleaf() { return M; }

    static constexpr unsigned ptrsize() {
      constexpr auto minsize = bytelen(maxleaf());
      constexpr auto nodesize = TrieNode<branching(), minsize>::length();
      constexpr auto numpbl = MemT::size() * (MemT::blocksize() / nodesize);
      constexpr auto maxintern =
        PWTrait::template pmax<nodesize, numnodes(), numpbl>();
      return bytelen(std::max(maxleaf(), maxintern+1));
    }

    static constexpr size_t pmax()
    { return bitmask(ptrsize() * bits_per_byte()); }

    using Backend = MemT;
    using Node = TrieNode<branching(), ptrsize()>;
    using Pack = PackMem<Backend, Node::length()>;
    using WoFact = typename PWTrait::template Factory<Pack, numnodes()>;
    using Woram = typename WoFact::T;
    static_assert(is_plain_woram<Woram>::value,
        "PWTrait must provide a factory for plain worams");

    static constexpr size_t maxintern() { return Woram::pmax(); }
    static constexpr size_t nptr() { return bytemask(ptrsize()); }
    static_assert(maxintern() < nptr(), "nptr has to be an invalid internal pointer");

    static constexpr size_t nullind() { return 0; }
    static constexpr size_t minind() { return 1; }

  private:
    std::shared_ptr<Woram> nodestore;
    woram_array<Node, height()+1> writecache;
    mutable woram_array<Node, height()+1> readcache;
    mutable unsigned rcsplit; // the first index where readcache diverges from writecache

    static constexpr size_t childof(size_t index, unsigned which)
    { return branching() * index + which + 1; }

    static constexpr size_t parentof(size_t index)
    { return (index - 1) / branching(); }

    static constexpr size_t which_child(size_t index)
    { return (index - 1) % branching(); }

    // converts my node indexes to storage indexes
    static constexpr size_t nodeind(size_t index) { return index - minind(); }

    static constexpr size_t maxinternal() { return numnodes() + minind() - 1; }
    static constexpr size_t maxindex()
    { return childof(maxinternal(), branching()-1); }

    // gets the path of nodes to the given index
    // returns the size of the path in the given array
    using PathT = woram_array<size_t, height()+1>;

    static unsigned pathto(size_t index, PathT& path) {
      check_range(index, minind(), maxindex(), "index out of range for pathto");

      // compute the path from that index back up the trie
      unsigned pbegin = path.size() - 1;
      path.at(pbegin) = parentof(index);
      while (path.at(pbegin) >= minind()) {
        path.at(pbegin-1) = parentof(path.at(pbegin));
        --pbegin;
      }

      return pbegin;
    }

    // returns the length of the matching paths between the given path
    // and the given cache.
    // pbegin is the offset to add to path indices,
    // and sofar is the index (in cache) of the first possible mismatch
    // The index of the first actual mismatch (in cache) is returned.
    template <typename NodeArr>
    static unsigned matchlen (const PathT& path, unsigned pbegin, unsigned sofar, const NodeArr& cache) {
      check_length(path.size() <= cache.size() + pbegin,
          "path is too short in matchlen helper function");
      while (pbegin + sofar < path.size()
             && cache.at(sofar).index == path.at(pbegin + sofar))
      { ++sofar; }
      return sofar;
    }

    // fetches the path to the given index and returns the height
    // of that index
    unsigned fetch_write(size_t index) {
      // compute the path from that index back up the trie
      PathT path;
      auto pbegin = pathto(index, path);
      auto fetched = matchlen(path, pbegin, 1, writecache);

      // fetch necessary blocks
      if (pbegin + fetched < path.size()) {
        do {
          size_t find = path.at(pbegin + fetched);
          auto pos = writecache.at(fetched-1).get(which_child(find));
          if (pos == nptr()) {
            // this node doesn't exist yet
            for (unsigned i=0; i < branching(); ++i) writecache.at(fetched).set(i, nptr());
          } else {
            nodestore->load(nodeind(find), pos, writecache.at(fetched).data);
          }
          writecache.at(fetched).index = find;
          ++fetched;
        } while (pbegin + fetched < path.size());
        // clear write cache below this spot
        if (fetched < writecache.size())
          writecache.at(fetched).index = nullind();
      }

      return fetched - 1;
    }

    // returns the actual index
    size_t fetch_read(size_t index) const {
      // compute the path from that index back up the trie
      PathT path;
      auto pbegin = pathto(index, path);
      auto fetched = matchlen(path, pbegin, 1, writecache);

      if (pbegin + fetched == path.size()) {
        // node was found in the write cache
        return writecache.at(fetched - 1).get(which_child(index));
      }

      size_t find, pos;

      if (rcsplit == fetched) {
        // try to jump ahead in the read cache
        fetched = matchlen(path, pbegin, rcsplit, readcache);
      } else {
        // reset rcsplit to this point
        rcsplit = fetched;
      }

      if (fetched > rcsplit) {
        if (pbegin + fetched == path.size()) {
          // node was found in the read cache
          return readcache.at(fetched - 1).get(which_child(index));
        } else {
          find = path.at(pbegin + fetched);
          pos = readcache.at(fetched-1).get(which_child(find));
        }
      } else {
        find = path.at(pbegin + fetched);
        pos = writecache.at(fetched-1).get(which_child(find));
      }

      // fetch new nodes down the path
      while (pos != nptr()) {
        nodestore->load(nodeind(find), pos, readcache.at(fetched).data);
        readcache.at(fetched).index = find;
        ++fetched;
        if (pbegin + fetched == path.size()) {
          // found it - clear read cache below this spot and return
          if (fetched < readcache.size()) readcache.at(fetched).index = nullind();
          return readcache.at(fetched-1).get(which_child(index));
        }
        find = path.at(pbegin + fetched);
        pos = readcache.at(fetched-1).get(which_child(find));
      }

      // clear read cache below this spot
      readcache.at(fetched).index = nullind();
      return pos;
    }

  public:
    static constexpr size_t size() { return maxindex() + 1; }

    TrieNodePM(std::unique_ptr<Backend> bp)
      :nodestore(WoFact::create(std::make_unique<Pack>(std::move(bp))))
    {
      // set up root node with null child pointers
      for (unsigned i=0; i < branching(); ++i) {
        writecache.at(0).set(i, nptr());
      }
      // all nodes uninitialized except for root
      for (size_t i=0; i < writecache.size(); ++i) {
        writecache.at(i).index = nullind();
        readcache.at(i).index = nullind();
      }
      rcsplit = 1;

      if (showinfo()) {
        std::cout << "TriePM created with branching factor " << branching()
          << ", " << numnodes() << " size-" << Node::length()
          << " nodes, and height " << height() << std::endl;
      }
    };

    size_t load(size_t index) const {
      index += minind();
      auto pos = fetch_read(index);
      return pos;
    }

    void store(size_t index, size_t pos) {
      index += minind();
      auto ht = fetch_write(index);
      writecache.at(ht).set(which_child(index), pos);

      // write back the saved path
      for (size_t i = writecache.size()-1; i > 0; --i) {
        size_t curind = writecache.at(i).index;
        if (curind == nullind()) {
          nodestore->dummy_write(*this);
        } else {
          auto pos = nodestore->store(nodeind(curind), writecache.at(i).data, *this);
          writecache.at(i-1).set(which_child(curind), pos);
        }
      }

      // clear the read cache
      rcsplit = 1;
      readcache.at(rcsplit).index = nullind();
    }

    void flush() { nodestore->flush(); }
};
template <typename MemT, size_t N, size_t M, unsigned K, typename PWTrait>
struct is_posmap<TrieNodePM<MemT,N,M,K,PWTrait>> :public std::true_type { };

// trivial case (no externally stored nodes) specialization
template <typename MemT, size_t M, unsigned K, typename PWTrait>
class TrieNodePM<MemT,0UL,M,K,PWTrait> {
  public:
    static constexpr size_t branching() { return K; }
    static constexpr size_t size() { return branching(); }
    static constexpr size_t pmax() { return M; }
    static constexpr size_t nptr() { return pmax()+1; }
    static constexpr size_t ptrsize() { return bytelen(nptr()); }
    static constexpr size_t minind() { return 0; }

    using Node = TrieNode<branching(), ptrsize()>;

  private:
    Node root;

  public:
    TrieNodePM(std::unique_ptr<MemT>) {
      // set up root node with null child pointers
      for (unsigned i=0; i < branching(); ++i) {
        root.set(i, nptr());
      }
    };

    void showpath() const { }

    size_t load(size_t index) const {
      check_range(index, size()-1, "index out of range in trivial trienodepm");
      return root.get(static_cast<unsigned>(index));
    }

    void store(size_t index, size_t pos) {
      check_range(index, size()-1, "index out of range in trivial trienodepm");
      root.set(static_cast<unsigned>(index), pos);
    }

    void flush() { }
};

// Modeled on PositionMap
// MemT is the backend memory type
// N is the number of pointers to store (indexed 0 through N-1)
// M is the maximum position value that will be stored
// PWTrait is a trait class (with a factory) for a PlainWoram
// K is the branching factor to use
template <typename MemT, size_t N, size_t M, typename PWTrait, unsigned K=2>
class TriePM {
  static_assert(is_memory<MemT>::value,
      "MemT template to TriePM must be a memory");

  public:
    static constexpr size_t size() { return N; }
    static constexpr size_t pmax() { return M; }
    static constexpr unsigned branching() { return K; }
    static constexpr size_t numnodes() { return (std::max(N,2ul) - 2) / (branching() - 1); }

    using Mem = MemT;
    using TNPM = TrieNodePM<Mem, numnodes(), pmax(), branching(), PWTrait>;
    static constexpr size_t nptr() { return TNPM::nptr(); }

  private:
    TNPM tnpm;

    static size_t trie_ind(size_t index) {
      check_range(index, size() - 1, "index too large in TriePM");
      return numnodes() + index;
    }

  public:
    TriePM(std::unique_ptr<Mem> mp) :tnpm(std::move(mp)) { }

    void showpath() const { tnpm.showpath(); }

    size_t load(size_t index) const { return tnpm.load(trie_ind(index)); }

    void store(size_t index, size_t pos) {
      check_range(pos, pmax(), "position too large in TriePM store");
      tnpm.store(trie_ind(index), pos);
    }

    void flush() { tnpm.flush(); }
};
template <typename MemT, size_t N, size_t M, typename PWTrait, unsigned K>
struct is_posmap<TriePM<MemT,N,M,PWTrait,K>> :public std::true_type { };


template <typename PWTrait, unsigned K=2>
struct TriePMTrait {
  template <typename MemT, size_t N, size_t M>
  struct Factory {
    using T = TriePM<MemT,N,M,PWTrait,K>;
    static auto create(std::unique_ptr<MemT> mp)
    { return std::make_unique<T>(std::move(mp)); }
  };

  template <size_t B, size_t N, size_t M>
  static constexpr size_t prefsize() {
    constexpr auto numnodes = (std::max(N,2ul) - 2) / (K - 1);
    constexpr auto memsize = PWTrait::prefsize(numnodes);
    constexpr auto ptrsize = bytelen(M);
    constexpr auto nodesize = TrieNode<K, ptrsize>::length();
    constexpr auto nodeperblock = B / nodesize;
    return ceiling(memsize, nodeperblock);
  }
};

// This is used to create PMWoram instances using a TriePM as the position map
template <unsigned K=2, template <typename,size_t,size_t> class ST = ChunkSplit>
struct TPMWTrait {
  template <typename BMT, size_t N, typename PWTrait, typename TPMTrait=TriePMTrait<PWTrait,K>>
  struct Factory {
    static constexpr size_t pmax()
    { return PWTrait::template pmax<BMT::blocksize(), N, BMT::size()>(); }

    static constexpr size_t pmblocks()
    { return TPMTrait::template prefsize<BMT::blocksize(), N, pmax()>(); }

    using Split = ST<BMT, pmblocks(), BMT::size() - pmblocks()>;

    using PosMem = typename Split::Mem0;
    using PMFact = typename TPMTrait::template Factory<PosMem, N, pmax()>;
    using PMT = typename PMFact::T;

    using WoMem = typename Split::Mem1;
    using WFact = typename PWTrait::template Factory<WoMem, N>;
    using WT = typename WFact::T;

    using T = PMWoram<WT, PMT>;

    static auto create(std::unique_ptr<BMT> mp) {
      auto halves = Split::create(std::move(mp));
      return std::make_unique<T>(
          WFact::create(std::move(std::get<1>(halves))),
          PMFact::create(std::move(std::get<0>(halves))));
    }
  };
};

// This is the whole deal, DetWoram with crypto and Trie position map
// Note, the block size must be divisible by the crypto block size,
// and also must be sufficiently large to store at least one node
template <unsigned Branching=2, unsigned KeyBytes=16>
struct DetWoCryptTrie {
  template <typename BMT, size_t N>
  using Factory = typename TPMWTrait<Branching, CryptSplitType<KeyBytes>::template RandT>
    ::template Factory<
      BMT,
      N,
      DetWoramTrait<CryptSplitType<KeyBytes>::template CtrT>,
      TriePMTrait<DetWoramTrait<ChunkSplit, heighthelp<N,Branching>()+2>,Branching>
    >;
};

} // namespace woram

#endif // WORAM_TRIEPM_H
