#ifndef WORAM_CRYPTO_H
#define WORAM_CRYPTO_H

#include <mbedtls/aes.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <woram/common.h>
#include <woram/split.h>

namespace woram {

// the key is global and must be set BEFORE constructing
// any crypto instances
template <unsigned K=16>
inline byte* crypto_key() {
  static byte thekey[K] = {0};
  return thekey;
}

// Wraps the underlying memory in counter-mode encryption.
// NOTE: the memory *must* be written sequentially!
template <typename MemT, unsigned K=16, bool Real=true>
class CtrCrypt {
  static_assert(is_memory<MemT>::value,
      "MemT template to CtrCrypt must be a memory");
  public:
    using Mem = MemT;
    static constexpr size_t blocksize() { return Mem::blocksize(); }
    static constexpr size_t size() { return Mem::size(); }

    // this will be the size of the key argument: in bits
    static constexpr unsigned keylen() { return K; }

    // the size of the blocks in the block cipher: in bytes
    // i.e., 16 means 128 bits.
    static constexpr size_t cryptblock() { return 16; }

    static_assert(blocksize() % cryptblock() == 0,
        "blocksize must be a multiple of the block cipher blocksize");

  private:
    std::unique_ptr<Mem> backend;
    mutable mbedtls_aes_context _aes_ctx;
    size_t round;
    size_t nextpos = 0;

    void enc(size_t ctr, const byte* m, byte* c) const {
      // counter starts with ctr (8 bytes) || 0^8
      static_assert( 2*sizeof(size_t) == cryptblock(), "size_t should be 8 bytes");
      size_t counter[2] = {ctr, 0};
      byte pad[cryptblock()];
      constexpr size_t n = blocksize() / cryptblock();

      for(size_t i=0; i<n; i++)
      {
        // stream from aes
        mbedtls_aes_crypt_ecb(&_aes_ctx, MBEDTLS_AES_ENCRYPT,
            (byte*)counter, pad);

        // xor
        for(size_t j=0; j<cryptblock(); j++)
          *c++ = (*m++) ^ pad[j];

        // increment counter
        counter[1]++;
      }
    }

    void dec(size_t ctr, byte* m, const byte* c) const {
      enc(ctr, c, m);
    }

  public:
    CtrCrypt(std::unique_ptr<Mem> mp, size_t startround=0)
      :backend(std::move(mp)), round(startround)
    {
      mbedtls_aes_setkey_enc(&_aes_ctx, crypto_key<keylen()>(), keylen()*bits_per_byte());
      if (showinfo()) {
        std::cout << "CtrCrypt created with backend of " << size() << " size-"
          << blocksize() << " blocks and starting counter " << startround << std::endl;
      }
    }

    void load(size_t index, byte* buf) const {
      byte ctext[blocksize()];
      backend->load(index, ctext);
      size_t ctr;
      if (Real) {
        ctr = round + index;
        if (index >= nextpos) ctr -= size();
      } else {
        ctr = index;
      }
      dec(ctr, buf, ctext);
    }

    void store(size_t index, const byte* buf) {
      byte ctext[blocksize()];
      size_t ctr;
      if (Real) {
        check_range(index, nextpos, nextpos,
            "non-sequential write in CtrCrypt is not allowed!");
        ctr = round + index;
      }
      else ctr = index;
      enc(ctr, buf, ctext);
      backend->store(index, ctext);
      if (++nextpos == size()) {
        nextpos = 0;
        round += size();
      }
    }

    void flush() { backend->flush(); }
};
template <typename MemT, unsigned K>
struct is_memory<CtrCrypt<MemT,K>> :public std::true_type { };


template <typename MemT, unsigned K=16>
class RandCrypt {
  static_assert(is_memory<MemT>::value,
      "MemT template to RandCrypt must be a memory");
  public:
    using Mem = MemT;

    // the size of the blocks in the block cipher: in bytes
    // i.e., 16 means 128 bits.
    static constexpr size_t cryptblock() { return 16; }
    static_assert(Mem::blocksize() > cryptblock(),
        "need space for IV in RandCrypt blocks");

    // the IV is taken out of the blocksize
    static constexpr size_t blocksize() {
      return Mem::blocksize() - cryptblock();
    }

    static constexpr size_t size() { return Mem::size(); }

    // this will be the size of the key argument: in bits
    static constexpr unsigned keylen() { return K; }

    static_assert(blocksize() % cryptblock() == 0,
        "blocksize must be a multiple of the block cipher blocksize");

  private:
    std::unique_ptr<Mem> backend;
    const char* _pers;
    mutable mbedtls_aes_context _aes_ctx_enc;
    mutable mbedtls_aes_context _aes_ctx_dec;
    mbedtls_ctr_drbg_context _ctr_drbg;
    mbedtls_entropy_context _entropy;

    void enc(const byte* m, byte* c) {
      // choose iv at random: put it in the first 16 bytes of c
      mbedtls_ctr_drbg_random( &_ctr_drbg, c, cryptblock() );

      // copy iv
      byte iv[cryptblock()];
      std::copy(c, c + cryptblock(), iv);

      // the rest of c contains blocksize() bytes of encryption of m
      mbedtls_aes_crypt_cbc(&_aes_ctx_enc, MBEDTLS_AES_ENCRYPT,
          blocksize(), iv, m, c+cryptblock());
    }

    void dec(byte* m, const byte* c) const {
      // the first 16 bytes of c is IV
      // the rest of c contains blocksize() bytes of encryption of m
      byte iv[cryptblock()];
      std::copy(c, c + cryptblock(), iv);
      mbedtls_aes_crypt_cbc(&_aes_ctx_dec, MBEDTLS_AES_DECRYPT,
          blocksize(), iv, c+cryptblock(), m);
    }

  public:
    RandCrypt(std::unique_ptr<Mem> mp)
      :backend(std::move(mp)), _pers{"we need to get iv's for cbc"}
    {
      mbedtls_aes_setkey_enc(&_aes_ctx_enc, crypto_key<keylen()>(), keylen()*bits_per_byte());
      mbedtls_aes_setkey_dec(&_aes_ctx_dec, crypto_key<keylen()>(), keylen()*bits_per_byte());
      mbedtls_entropy_init(&_entropy);
      mbedtls_ctr_drbg_init(&_ctr_drbg);
      mbedtls_ctr_drbg_seed(&_ctr_drbg, mbedtls_entropy_func,
          &_entropy, (byte*)_pers, strlen(_pers));
      if (showinfo()) {
        std::cout << "RandCrypt created with backend of " << size() << " size-"
          << Mem::blocksize() << " blocks" << std::endl;
      }
    }

    void load(size_t index, byte* buf) const {
      byte ctext[cryptblock() + blocksize()];
      backend->load(index, ctext);
      dec(buf, ctext);
    }

    void store(size_t index, const byte* buf) {
      byte ctext[cryptblock() + blocksize()];
      enc(buf, ctext);
      backend->store(index, ctext);
    }

    void flush() { backend->flush(); }
};
template <typename MemT, unsigned K>
struct is_memory<RandCrypt<MemT,K>> :public std::true_type { };


// applies ctrcrypt on top of a ChunkSplit
template <typename MemT, size_t X, size_t Y=(MemT::size()-X), unsigned K=16>
struct CtrCryptSplit {
  using Backend = MemT;

  using Split = ChunkSplit<Backend,X,Y>;
  using SM0 = typename Split::Mem0;
  using SM1 = typename Split::Mem1;
  using Mem0 = CtrCrypt<SM0, K>;
  using Mem1 = CtrCrypt<SM1, K>;

  static auto create(std::shared_ptr<Backend> bp) {
    auto halves = Split::create(bp);
    auto m0 = std::make_unique<Mem0>(std::move(std::get<0>(halves)));
    // make counters unique
    auto m1 = std::make_unique<Mem1>(
        std::move(std::get<1>(halves)),
        std::numeric_limits<size_t>::max()/2);
    return std::tuple<decltype(m0),decltype(m1)>(std::move(m0), std::move(m1));
  }
};
template <typename MemT, size_t X, size_t Y, size_t K>
struct is_split<CtrCryptSplit<MemT,X,Y,K>> :public std::true_type { };

// applies randcrypt on the FIRST part of a ChunkSplit
template <typename MemT, size_t X, size_t Y=(MemT::size()-X), unsigned K=16>
struct RandCryptSplit {
  using Backend = MemT;

  using Split = ChunkSplit<Backend,X,Y>;
  using SM0 = typename Split::Mem0;
  using Mem1 = typename Split::Mem1;
  using Mem0 = RandCrypt<SM0, K>;

  static auto create(std::shared_ptr<Backend> bp) {
    auto halves = Split::create(bp);
    auto m0 = std::make_unique<Mem0>(std::move(std::get<0>(halves)));
    return std::tuple<decltype(m0),std::unique_ptr<Mem1>>
      (std::move(m0), std::move(std::get<1>(halves)));
  }
};
template <typename MemT, size_t X, size_t Y, size_t K>
struct is_split<RandCryptSplit<MemT,X,Y,K>> :public std::true_type { };

// template separation helper
template <size_t K=16>
struct CryptSplitType {
  template <typename MemT, size_t X, size_t Y=(MemT::size()-X)>
  using CtrT = CtrCryptSplit<MemT,X,Y,K>;

  template <typename MemT, size_t X, size_t Y=(MemT::size()-X)>
  using RandT = RandCryptSplit<MemT,X,Y,K>;
};

} // namespace woram

#endif // WORAM_CRYPTO_H
