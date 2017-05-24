#ifndef WORAM_BMNO_H
#define WORAM_BMNO_H

#include <mbedtls/aes.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <woram/common.h>
#include <woram/split.h>
#include <woram/pack.h>
#include <woram/woram.h>
#include <woram/crypto.h>

namespace woram {

// metadata block
template <unsigned indsize, size_t cblock>
struct BMNO_metadata {
  static constexpr size_t length() { return indsize + cblock; }
  byte data[length()];

  size_t get_ind() const {
    return getnum<indsize>(data);
  }

  void  set_ind(size_t x) {
    storenum<indsize>(data, x);
  }

  void set_iv(const byte* iv) {
    std::copy(iv, iv + cblock, data + indsize);
  }

  void get_iv(byte* iv) const {
    std::copy(data + indsize, data + (indsize + cblock), iv);
  }
};


// we need an extra operation commit for the recursive position map here only
template <typename T> struct has_commit :public std::false_type { };

template <typename PMT, bool hc = has_commit<PMT>::value>
struct Committer;

// write-only ORAM of BMNO, CCS'14
template <typename MemT, size_t N, typename PMT, unsigned K=3>
class BMNOWoram {
  static_assert(is_memory<MemT>::value, "MemT must be a memory in KWoram");
  static_assert(is_posmap<PMT>::value, "PMT must be a memory in KWoram");

  public:
    using Mem = MemT;
    using PM = PMT;

    static constexpr size_t cryptblock() { return 16; }
    static constexpr size_t keylen() { return 16; }
    static constexpr size_t blocksize() { return MemT::blocksize(); }
    static constexpr size_t size() { return N; }
    static constexpr size_t stashlen() { return 60 + bitlen(size()); }

    using Meta = BMNO_metadata<bytelen(MemT::size()), cryptblock()>;
    static constexpr size_t metablocks() {
      constexpr auto perblock = (blocksize() - 16) / Meta::length();
      return ceiling(Mem::size(), perblock);
    }

    using Split = ChunkSplit<MemT, metablocks()>;
    using MCrypt = RandCrypt<typename Split::Mem0>;
    using MPack = PackMem<MCrypt, Meta::length(), Mem::size()>;
    using Pri = typename Split::Mem1;

    static_assert(Pri::size() > stashlen() + size(),
        "need room for stash and mem in BMNO");
    static constexpr size_t pmax() { return Pri::size() - stashlen(); }
    static_assert(PM::pmax() >= pmax(),
        "position map posns aren't large enough in BMNO");
    static constexpr size_t nptr() { return PM::nptr(); }

  private:
    std::unique_ptr<MPack> meta;
    std::unique_ptr<Pri> primary;
    std::unique_ptr<PM> posmap;
    LocalMem<blocksize(), stashlen()> stash;
    woram_array<size_t, stashlen()> stashloc;
    size_t stashused = 0;
    const char* _pers;
    mutable mbedtls_aes_context _aes_ctx_enc;
    mutable mbedtls_aes_context _aes_ctx_dec;
    mutable mbedtls_ctr_drbg_context _ctr_drbg;
    mutable mbedtls_entropy_context _entropy;

    size_t randpos() const {
      byte buf[cryptblock()];
      constexpr auto sby = bytelen(pmax());
      constexpr int shift = sby * bits_per_byte() - bitlen(pmax());
      while (true) {
        mbedtls_ctr_drbg_random( &_ctr_drbg, buf, cryptblock() );
        size_t x = getnum<sby>(buf) >> shift;
        if (x <= pmax()) return x;
      }
    }

  public:
    BMNOWoram(std::unique_ptr<MemT> mp, std::unique_ptr<PM> pmp)
      :posmap(std::move(pmp)),
       _pers{"we need to get iv's for cbc"}
    {
      auto halves = Split::create(std::move(mp));
      meta = std::make_unique<MPack>(std::make_unique<MCrypt>(std::move(std::get<0>(halves))));
      primary = std::move(std::get<1>(halves));
      mbedtls_aes_setkey_enc(&_aes_ctx_enc, crypto_key<keylen()>(), keylen()*bits_per_byte());
      mbedtls_aes_setkey_dec(&_aes_ctx_dec, crypto_key<keylen()>(), keylen()*bits_per_byte());
      mbedtls_entropy_init(&_entropy);
      mbedtls_ctr_drbg_init(&_ctr_drbg);
      mbedtls_ctr_drbg_seed(&_ctr_drbg, mbedtls_entropy_func,
          &_entropy, (byte*)_pers, strlen(_pers));
      if (showinfo()) {
        std::cout << "BMNOWoram created with size " << size()
          << ", using " << primary->size() << " size-"
          << blocksize() << " blocks and "
          << meta->size() << " size-" << meta->blocksize()
          << " metadata blocks" << std::endl;
      }
    }

    void load(size_t index, byte* buf) const {
      check_range(index, size()-1, "index in bmno load");

      // look in stash first
      for (size_t si=0; si < stashused; ++si) {
        if (stashloc.at(si) == index) {
          stash.load(si, buf);
          return;
        }
      }

      // get position from posmap
      auto position = posmap->load(index);
      if (position > pmax()) {
        std::fill(buf, buf + blocksize(), 0);
        return;
      }
      check_range(position, pmax(), "position in bmno load");

      Meta m;
      meta->load(position, m.data);
      byte iv[cryptblock()];
      m.get_iv(iv);
      byte temp[blocksize()];
      primary->load(position, temp);
      mbedtls_aes_crypt_cbc(&_aes_ctx_dec, MBEDTLS_AES_DECRYPT,
          blocksize(), iv, temp, buf);
    }

    void store(size_t index, const byte* buf) {
      // put new block into stash
      size_t si = 0;
      while (si < stashused) {
        if (stashloc.at(si) == index) {
          stash.store(si, buf);
          break;
        }
        ++si;
      }
      if (si == stashused) {
        if (stashused >= stashlen()) {
          std::cerr << "ERROR: STASH OVERFLOW!!!!\n";
          abort();
        }
        stash.store(stashused, buf);
        stashloc.at(stashused) = index;
        ++stashused;
      }

      byte ctext[blocksize()];
      byte ptext[blocksize()];
      byte iv[cryptblock()];

      // chose K random indices and rewrite
      woram_array<size_t,K> used;
      for (unsigned i=0; i < K; ++i) {
        bool already;
        do {
          already = false;
          used.at(i) = randpos();
          for (unsigned j=0; j < i; ++j) {
            if (used.at(j) == used.at(i)) {
              already = true;
              break;
            }
          }
        } while (i <= pmax() && already);

        auto curpos = used.at(i);
        check_range(curpos, pmax(), "curpos in bmno store");

        Meta me;
        meta->load(curpos, me.data);

        if (stashused > 0 && (me.get_ind() >= size() || posmap->load(me.get_ind()) != curpos)) {
          // write a block from stash
          stash.load(stashused-1, ptext);
          me.set_ind(stashloc.at(stashused-1));
          --stashused;
          posmap->store(me.get_ind(), curpos);
        } else {
          // rewrite what's there
          primary->load(curpos, ctext);
          me.get_iv(iv);
          mbedtls_aes_crypt_cbc(&_aes_ctx_dec, MBEDTLS_AES_DECRYPT,
              blocksize(), iv, ctext, ptext);
        }

        // re-encrypt the data and store it back
        mbedtls_ctr_drbg_random( &_ctr_drbg, iv, cryptblock() );
        me.set_iv(iv);
        mbedtls_aes_crypt_cbc(&_aes_ctx_enc, MBEDTLS_AES_ENCRYPT,
            blocksize(), iv, ptext, ctext);

        primary->store(curpos, ctext);
        meta->store(curpos, me.data);
      }
      Committer<PM>()(*posmap);
    }

    void flush() {
      byte temp[blocksize()];
      for (size_t i=0; i < stashlen(); ++i) {
        stash.load(i, temp);
        primary->store(primary->size() - i - 1, temp);
      }
      primary->flush();
      meta->flush();
      posmap->flush();
    }
};
template <typename MemT, size_t N, typename PMT, unsigned K>
struct is_woram<BMNOWoram<MemT,N,PMT,K>> :public std::true_type { };


// write-only ORAM of BMNO, CCS'14 (recursive case, no encryption here)
template <typename MemT, size_t N, typename PMT, unsigned K=3>
class BMNOWoramRec {
  static_assert(is_memory<MemT>::value, "MemT must be a memory in KWoram");
  static_assert(is_posmap<PMT>::value, "PMT must be a memory in KWoram");

  public:
    using Mem = MemT;
    using PM = PMT;

    static constexpr size_t blocksize() { return MemT::blocksize(); }
    static constexpr size_t size() { return N; }
    static constexpr size_t stashlen() { return 60 + bitlen(size()); }

    using Meta = BMNO_metadata<bytelen(MemT::size()), 0>;
    static constexpr size_t metablocks() {
      constexpr auto perblock = blocksize() / Meta::length();
      return ceiling(Mem::size(), perblock);
    }

    using Split = ChunkSplit<MemT, metablocks()>;
    using MPack = PackMem<typename Split::Mem0, Meta::length(), Mem::size()>;
    using Pri = typename Split::Mem1;

    static_assert(Pri::size() > stashlen() + size(),
        "need room for stash and mem in BMNORec");
    static constexpr size_t pmax() { return Pri::size() - stashlen(); }
    static_assert(PM::pmax() >= pmax(),
        "position map posns aren't large enough in BMNORec");
    static constexpr size_t nptr() { return PM::nptr(); }

  private:
    std::unique_ptr<MPack> meta;
    std::unique_ptr<Pri> primary;
    std::unique_ptr<PM> posmap;
    LocalMem<blocksize(), stashlen()> stash;
    woram_array<size_t, stashlen()> stashloc;
    size_t stashused = 0;
    const char* _pers;
    mutable mbedtls_ctr_drbg_context _ctr_drbg;
    mutable mbedtls_entropy_context _entropy;

    size_t randpos() const {
      constexpr unsigned cb=16;
      byte buf[cb];
      constexpr auto sby = bytelen(pmax());
      constexpr int shift = sby * bits_per_byte() - bitlen(pmax());
      while (true) {
        mbedtls_ctr_drbg_random( &_ctr_drbg, buf, cb );
        size_t x = getnum<sby>(buf) >> shift;
        if (x <= pmax()) return x;
      }
    }

  public:
    BMNOWoramRec(std::unique_ptr<MemT> mp, std::unique_ptr<PM> pmp)
      :posmap(std::move(pmp)),
       _pers{"we need to get iv's for cbc"}
    {
      auto halves = Split::create(std::move(mp));
      meta = std::make_unique<MPack>(std::move(std::get<0>(halves)));
      primary = std::move(std::get<1>(halves));
      mbedtls_entropy_init(&_entropy);
      mbedtls_ctr_drbg_init(&_ctr_drbg);
      mbedtls_ctr_drbg_seed(&_ctr_drbg, mbedtls_entropy_func,
          &_entropy, (byte*)_pers, strlen(_pers));
      if (showinfo()) {
        std::cout << "BMNOWoramRec created with size " << size()
          << ", using " << primary->size() << " size-"
          << blocksize() << " blocks and "
          << meta->size() << " size-" << meta->blocksize()
          << " metadata blocks" << std::endl;
      }
    }

    void load(size_t index, byte* buf) const {
      check_range(index, size()-1, "index in bmnorec load");

      // look in stash first
      for (size_t si=0; si < stashused; ++si) {
        if (stashloc.at(si) == index) {
          stash.load(si, buf);
          return;
        }
      }

      // get position from posmap
      auto position = posmap->load(index);
      if (position > pmax()) {
        std::fill(buf, buf + blocksize(), 0);
        return;
      }
      check_range(position, pmax(), "position in bmnorec load");

      primary->load(position, buf);
    }

    void store(size_t index, const byte* buf) {
      // put new block into stash
      size_t si = 0;
      while (si < stashused) {
        if (stashloc.at(si) == index) {
          stash.store(si, buf);
          break;
        }
        ++si;
      }
      if (si == stashused) {
        if (stashused >= stashlen()) {
          std::cerr << "ERROR: STASH OVERFLOW!!!!\n";
          abort();
        }
        stash.store(stashused, buf);
        stashloc.at(stashused) = index;
        ++stashused;
      }
    }

    void commit() {
      byte ptext[blocksize()];

      // chose K random indices and rewrite
      woram_array<size_t,K> used;
      for (unsigned i=0; i < K; ++i) {
        bool already;
        do {
          already = false;
          used.at(i) = randpos();
          for (unsigned j=0; j < i; ++j) {
            if (used.at(j) == used.at(i)) {
              already = true;
              break;
            }
          }
        } while (i <= pmax() && already);

        auto curpos = used.at(i);
        check_range(curpos, pmax(), "curpos in bmnorec store");

        Meta me;
        meta->load(curpos, me.data);

        if (stashused > 0 && (me.get_ind() >= size() || posmap->load(me.get_ind()) != curpos)) {
          // write a block from stash
          stash.load(stashused-1, ptext);
          me.set_ind(stashloc.at(stashused-1));
          --stashused;
          posmap->store(me.get_ind(), curpos);
        } else {
          // rewrite what's there
          primary->load(curpos, ptext);
        }

        // re-encrypt the data and store it back

        primary->store(curpos, ptext);
        meta->store(curpos, me.data);
      }
      Committer<PM>()(*posmap);
    }

    void flush() {
      byte temp[blocksize()];
      for (size_t i=0; i < stashlen(); ++i) {
        stash.load(i, temp);
        primary->store(primary->size() - i - 1, temp);
      }
      primary->flush();
      meta->flush();
      posmap->flush();
    }
};
template <typename MemT, size_t N, typename PMT, unsigned K>
struct is_woram<BMNOWoramRec<MemT,N,PMT,K>> :public std::true_type { };

template <typename MemT, size_t N, typename PMT, unsigned K>
struct has_commit<BMNOWoramRec<MemT,N,PMT,K>> :public std::true_type { };

// have to add a commit op here too
template <typename MemT, size_t N, size_t M>
class CommitPosMap :public PackPosMap<MemT,N,M> {
  public:
    using PPM = PackPosMap<MemT,N,M>;
    static constexpr size_t size() { return PPM::size(); }
    static constexpr size_t pmax() { return PPM::pmax(); }
    static constexpr size_t nptr() { return PPM::nptr(); }

    CommitPosMap(std::unique_ptr<MemT> mp) :PPM(std::move(mp)) { }

    void commit() { PPM::backend.back->commit(); }
};
template <typename MemT, size_t N, size_t M>
struct is_posmap<CommitPosMap<MemT,N,M>> :public std::true_type { };
template <typename MemT, size_t N, size_t M>
struct has_commit<CommitPosMap<MemT,N,M>> :public std::true_type { };

// basecase posmap
template <typename MemT, size_t N, size_t M>
class BasePosMap :public PackPosMap<TrivialWoram<PackMem<MemT,MemT::blocksize(),1ul>>, N, M> {
  public:
    using Pack = PackMem<MemT,MemT::blocksize(),1ul>;
    using TW = TrivialWoram<Pack>;
    using PPM = PackPosMap<TW,N,M>;
    static constexpr size_t size() { return PPM::size(); }
    static constexpr size_t pmax() { return PPM::pmax(); }
    static constexpr size_t nptr() { return PPM::nptr(); }

    BasePosMap(std::unique_ptr<MemT> mp)
      :PPM(std::make_unique<TW>(std::make_unique<Pack>(std::move(mp))))
    { }

    void commit() { }
};
template <typename MemT, size_t N, size_t M>
struct is_posmap<BasePosMap<MemT,N,M>> :public std::true_type { };
template <typename MemT, size_t N, size_t M>
struct has_commit<BasePosMap<MemT,N,M>> :public std::true_type { };


// forward declaration
template <typename MemT, size_t N, size_t M, size_t PackSize = ppm_size(MemT::blocksize(), N, M)>
struct bmno_pm_fact;

// general case
template <typename MemT, size_t N, size_t M, size_t PackSize>
struct bmno_pm_fact {
  static constexpr size_t size0() { return 2*PackSize+70; }
  using Split = ChunkSplit<MemT, size0()>;
  using Rec = bmno_pm_fact<typename Split::Mem1,PackSize,size0()>;
  using Wo = BMNOWoramRec<typename Split::Mem0, PackSize, typename Rec::T>;
  using T = CommitPosMap<Wo, N, M>;
  static auto create(std::unique_ptr<MemT> mp) {
    auto halves = Split::create(std::move(mp));
    auto pm = Rec::create(std::move(std::get<1>(halves)));
    auto wop = std::make_unique<Wo>(std::move(std::get<0>(halves)), std::move(pm));
    return std::make_unique<T>(std::move(wop));
  }
};

// base case
template <typename MemT, size_t N, size_t M>
struct bmno_pm_fact<MemT,N,M,1ul> {
  using T = BasePosMap<MemT,N,M>;
  static auto create(std::unique_ptr<MemT> mp) {
    return std::make_unique<T>(std::move(mp));
  }
};

// factory for the whole thing
template <typename MemT, size_t N>
struct bmno_fact {
  static constexpr size_t size0() { return 2*N + ceiling(MemT::size(),MemT::blocksize()/24) + 70; }
  using Split = ChunkSplit<MemT, size0()>;
  using RC = RandCrypt<typename Split::Mem1>;
  using Rec = bmno_pm_fact<RC, N, size0()-1>;
  using T = BMNOWoram<typename Split::Mem0, N, typename Rec::T>;
  static auto create(std::unique_ptr<MemT> mp) {
    auto halves = Split::create(std::move(mp));
    auto pm = Rec::create(std::make_unique<RC>(std::move(std::get<1>(halves))));
    return std::make_unique<T>(std::move(std::get<0>(halves)), std::move(pm));
  }
};

template <typename PMT, bool hc>
struct Committer
{ void operator()(PMT&) { } };

template <typename PMT>
struct Committer<PMT,true> { void operator()(PMT& pm) { pm.commit(); } };


} // namespace woram

#endif // WORAM_BMNO_H
