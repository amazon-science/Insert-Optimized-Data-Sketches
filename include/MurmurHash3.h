// Modified from Austin Applebee's code:
//  * Removed MurmurHash3_x86_32 and MurmurHash3_x86_128
//  * Changed input seed in MurmurHash3_x64_128 to uint64_t
//  * Made entire hash function defined inline
//  * Removed Windows support
//  * Changed return type to __uint128_t
//  * Made constexpr (removed getblock64 for that)
//  * Change casts to C++ style
//  * Add manually unrolled versions for 16, 32, 64 and 128 bit inputs, which
//    are faster
//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

#pragma once

#include <stdint.h>

#include <cstring>

#define MURMUR3_FORCE_INLINE inline __attribute__((always_inline))

MURMUR3_FORCE_INLINE constexpr uint64_t rotl64(uint64_t x, int8_t r) {
  return (x << r) | (x >> (64 - r));
}

#define MURMUR3_ROTL64(x, y) rotl64(x, y)

#define MURMUR3_BIG_CONSTANT(x) (x##LLU)

//-----------------------------------------------------------------------------
// Finalization mix - force all bits of a hash block to avalanche

MURMUR3_FORCE_INLINE constexpr uint64_t fmix64(uint64_t k) {
  k ^= k >> 33;
  k *= MURMUR3_BIG_CONSTANT(0xff51afd7ed558ccd);
  k ^= k >> 33;
  k *= MURMUR3_BIG_CONSTANT(0xc4ceb9fe1a85ec53);
  k ^= k >> 33;

  return k;
}

constexpr uint64_t c1 = MURMUR3_BIG_CONSTANT(0x87c37b91114253d5);
constexpr uint64_t c2 = MURMUR3_BIG_CONSTANT(0x4cf5ad432745937f);

MURMUR3_FORCE_INLINE constexpr __uint128_t MurmurHash3_x64_128(const void* key,
                                                               size_t lenBytes,
                                                               uint64_t seed) {
  const uint8_t* data = static_cast<const uint8_t*>(key);

  uint64_t h1 = seed;
  uint64_t h2 = seed;

  // Number of full 128-bit blocks of 16 bytes.
  // Possible exclusion of a remainder of up to 15 bytes.
  const size_t nblocks = lenBytes >> 4;  // bytes / 16

  // Process the 128-bit blocks (the body) into the hash
  for (size_t i = 0; i < nblocks; ++i) {  // 16 bytes per block
    uint64_t k1 = *reinterpret_cast<const uint64_t*>(
        data + (i * 2 + 0) * sizeof(uint64_t));
    uint64_t k2 = *reinterpret_cast<const uint64_t*>(
        data + (i * 2 + 1) * sizeof(uint64_t));

    k1 *= c1;
    k1 = MURMUR3_ROTL64(k1, 31);
    k1 *= c2;
    h1 ^= k1;
    h1 = MURMUR3_ROTL64(h1, 27);
    h1 += h2;
    h1 = h1 * 5 + 0x52dce729;

    k2 *= c2;
    k2 = MURMUR3_ROTL64(k2, 33);
    k2 *= c1;
    h2 ^= k2;
    h2 = MURMUR3_ROTL64(h2, 31);
    h2 += h1;
    h2 = h2 * 5 + 0x38495ab5;
  }

  // tail
  const uint8_t* tail = (data + (nblocks << 4));

  uint64_t k1 = 0;
  uint64_t k2 = 0;

  switch (lenBytes & 15) {
    case 15:
      k2 ^= static_cast<uint64_t>(tail[14]) << 48;  // falls through
    case 14:
      k2 ^= static_cast<uint64_t>(tail[13]) << 40;  // falls through
    case 13:
      k2 ^= static_cast<uint64_t>(tail[12]) << 32;  // falls through
    case 12:
      k2 ^= static_cast<uint64_t>(tail[11]) << 24;  // falls through
    case 11:
      k2 ^= static_cast<uint64_t>(tail[10]) << 16;  // falls through
    case 10:
      k2 ^= static_cast<uint64_t>(tail[9]) << 8;  // falls through
    case 9:
      k2 ^= static_cast<uint64_t>(tail[8]) << 0;
      k2 *= c2;
      k2 = MURMUR3_ROTL64(k2, 33);
      k2 *= c1;
      h2 ^= k2;
      // falls through
    case 8:
      k1 ^= static_cast<uint64_t>(tail[7]) << 56;  // falls through
    case 7:
      k1 ^= static_cast<uint64_t>(tail[6]) << 48;  // falls through
    case 6:
      k1 ^= static_cast<uint64_t>(tail[5]) << 40;  // falls through
    case 5:
      k1 ^= static_cast<uint64_t>(tail[4]) << 32;  // falls through
    case 4:
      k1 ^= static_cast<uint64_t>(tail[3]) << 24;  // falls through
    case 3:
      k1 ^= static_cast<uint64_t>(tail[2]) << 16;  // falls through
    case 2:
      k1 ^= static_cast<uint64_t>(tail[1]) << 8;  // falls through
    case 1:
      k1 ^= static_cast<uint64_t>(tail[0]) << 0;
      k1 *= c1;
      k1 = MURMUR3_ROTL64(k1, 31);
      k1 *= c2;
      h1 ^= k1;
  };

  //----------
  // finalization

  h1 ^= lenBytes;
  h2 ^= lenBytes;

  h1 += h2;
  h2 += h1;

  h1 = fmix64(h1);
  h2 = fmix64(h2);

  h1 += h2;
  h2 += h1;

  return h1 | (static_cast<__uint128_t>(h2) << 64);
}

MURMUR3_FORCE_INLINE static constexpr __uint128_t MurmurHash3_x64_128(
    const uint8_t& k, uint64_t seed) {
  uint64_t h1 = seed;
  uint64_t h2 = seed;

  uint64_t k1 = 0;

  k1 ^= static_cast<uint64_t>(k);
  k1 *= c1;
  k1 = MURMUR3_ROTL64(k1, 31);
  k1 *= c2;
  h1 ^= k1;

  h1 ^= sizeof(k);
  h2 ^= sizeof(k);

  h1 += h2;
  h2 += h1;

  h1 = fmix64(h1);
  h2 = fmix64(h2);

  h1 += h2;
  h2 += h1;

  return h1 | (static_cast<__uint128_t>(h2) << 64);
}

MURMUR3_FORCE_INLINE static constexpr __uint128_t MurmurHash3_x64_128(
    const uint16_t& k, uint64_t seed) {
  uint64_t h1 = seed;
  uint64_t h2 = seed;

  uint64_t k1 = 0;

  k1 ^= static_cast<uint64_t>(k);
  k1 *= c1;
  k1 = MURMUR3_ROTL64(k1, 31);
  k1 *= c2;
  h1 ^= k1;

  h1 ^= sizeof(k);
  h2 ^= sizeof(k);

  h1 += h2;
  h2 += h1;

  h1 = fmix64(h1);
  h2 = fmix64(h2);

  h1 += h2;
  h2 += h1;

  return h1 | (static_cast<__uint128_t>(h2) << 64);
}

MURMUR3_FORCE_INLINE static constexpr __uint128_t MurmurHash3_x64_128(
    const uint32_t& k, uint64_t seed) {
  uint64_t h1 = seed;
  uint64_t h2 = seed;

  uint64_t k1 = 0;

  k1 ^= static_cast<uint64_t>(k);
  k1 *= c1;
  k1 = MURMUR3_ROTL64(k1, 31);
  k1 *= c2;
  h1 ^= k1;

  h1 ^= sizeof(k);
  h2 ^= sizeof(k);

  h1 += h2;
  h2 += h1;

  h1 = fmix64(h1);
  h2 = fmix64(h2);

  h1 += h2;
  h2 += h1;

  return h1 | (static_cast<__uint128_t>(h2) << 64);
}

MURMUR3_FORCE_INLINE static constexpr __uint128_t MurmurHash3_x64_128(
    const uint64_t& k, uint64_t seed) {
  uint64_t h1 = seed;
  uint64_t h2 = seed;

  uint64_t k1 = 0;

  k1 ^= static_cast<uint64_t>(k);
  k1 *= c1;
  k1 = MURMUR3_ROTL64(k1, 31);
  k1 *= c2;
  h1 ^= k1;

  h1 ^= sizeof(k);
  h2 ^= sizeof(k);

  h1 += h2;
  h2 += h1;

  h1 = fmix64(h1);
  h2 = fmix64(h2);

  h1 += h2;
  h2 += h1;

  return h1 | (static_cast<__uint128_t>(h2) << 64);
}

MURMUR3_FORCE_INLINE static constexpr __uint128_t MurmurHash3_x64_128(
    const __uint128_t& k, uint64_t seed) {
  uint64_t h1 = seed;
  uint64_t h2 = seed;

  uint64_t k1 = static_cast<uint64_t>(k);
  uint64_t k2 = static_cast<uint64_t>(k >> 64);

  k1 *= c1;
  k1 = MURMUR3_ROTL64(k1, 31);
  k1 *= c2;
  h1 ^= k1;
  h1 = MURMUR3_ROTL64(h1, 27);
  h1 += h2;
  h1 = h1 * 5 + 0x52dce729;

  k2 *= c2;
  k2 = MURMUR3_ROTL64(k2, 33);
  k2 *= c1;
  h2 ^= k2;
  h2 = MURMUR3_ROTL64(h2, 31);
  h2 += h1;
  h2 = h2 * 5 + 0x38495ab5;

  h1 ^= sizeof(k);
  h2 ^= sizeof(k);

  h1 += h2;
  h2 += h1;

  h1 = fmix64(h1);
  h2 = fmix64(h2);

  h1 += h2;
  h2 += h1;

  return h1 | (static_cast<__uint128_t>(h2) << 64);
}

#undef MURMUR3_FORCE_INLINE
#undef MURMUR3_ROTL64
#undef MURMUR3_BIG_CONSTANT
