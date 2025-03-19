#pragma once

#include <immintrin.h>

#include "compiler.hpp"

namespace detail {

#define _mm256_loadu_si256(x) \
  _mm256_loadu_si256(reinterpret_cast<const __m256i*>(x))

/// Broadcast 16-bit integer to all lanes of an AVX2 vector.
ALWAYS_INLINE static __m256i broadcast_epi16(uint16_t x) {
  return _mm256_broadcastw_epi16(_mm_cvtsi64_si128(x));
}

/// Broadcast 32-bit integer to all lanes of an AVX2 vector.
ALWAYS_INLINE static __m256i broadcast_epi32(uint32_t x) {
  return _mm256_broadcastd_epi32(_mm_cvtsi64_si128(x));
}

/// Broadcast 64-bit integer to all lanes of an AVX2 vector.
ALWAYS_INLINE static __m256i broadcast_epi64(uint64_t x) {
  return _mm256_broadcastq_epi64(_mm_cvtsi64_si128(x));
}

/// A utility function to return a mask from the msb from each 8 bit lane.
ALWAYS_INLINE static uint64_t movemask_epi8(__m256i x) {
  return _mm256_movemask_epi8(x);
}

/// A utility function to return a mask from the msb from each 32 bit lane.
ALWAYS_INLINE static uint64_t movemask_epi32(__m256i x) {
  return _mm256_movemask_ps(_mm256_castsi256_ps(x));
}

/// A utility function to return a mask from the msb from each 64 bit lane.
ALWAYS_INLINE static uint64_t movemask_epi64(__m256i x) {
  return _mm256_movemask_pd(_mm256_castsi256_pd(x));
}

/// Compares a 16 bit item with 64 keys of the sketch using AVX2 instructions.
/// @return a bitmask of the matching keys.
OPT_INLINE static uint64_t Compare64Keys16Bit(const __m256i& v,
                                              const uint16_t* keys) {
  __m256i x1 = _mm256_loadu_si256(keys);
  __m256i x2 = _mm256_loadu_si256(keys + 16);
  __m256i x3 = _mm256_loadu_si256(keys + 32);
  __m256i x4 = _mm256_loadu_si256(keys + 48);
  x1 = _mm256_cmpeq_epi16(x1, v);
  x2 = _mm256_cmpeq_epi16(x2, v);
  x3 = _mm256_cmpeq_epi16(x3, v);
  x4 = _mm256_cmpeq_epi16(x4, v);
  __m256i x12 = _mm256_packs_epi16(x1, x2);
  __m256i x34 = _mm256_packs_epi16(x3, x4);
  x12 = _mm256_permute4x64_epi64(x12, _MM_SHUFFLE(3, 1, 2, 0));
  x34 = _mm256_permute4x64_epi64(x34, _MM_SHUFFLE(3, 1, 2, 0));
  uint64_t m = movemask_epi8(x12);
  m |= movemask_epi8(x34) << 32;
  return m;
}

/// Compares a 16 bit item with 32 keys of the sketch using AVX2 instructions.
/// @return a bitmask of the matching keys.
OPT_INLINE static uint64_t Compare32Keys16Bit(const __m256i& v,
                                              const uint16_t* keys) {
  __m256i x1 = _mm256_loadu_si256(keys);
  __m256i x2 = _mm256_loadu_si256(keys + 16);
  x1 = _mm256_cmpeq_epi16(x1, v);
  x2 = _mm256_cmpeq_epi16(x2, v);
  __m256i x12 = _mm256_packs_epi16(x1, x2);
  x12 = _mm256_permute4x64_epi64(x12, _MM_SHUFFLE(3, 1, 2, 0));
  uint64_t m = movemask_epi8(x12);
  return m;
}

/// Compares a 32 bit item with 64 keys of the sketch using AVX2 instructions.
/// @return a bitmask of the matching keys.
OPT_INLINE static uint64_t Compare64Keys32Bit(const __m256i& v,
                                              const uint32_t* keys) {
  __m256i x1 = _mm256_loadu_si256(keys);
  __m256i x2 = _mm256_loadu_si256(keys + 8);
  __m256i x3 = _mm256_loadu_si256(keys + 16);
  __m256i x4 = _mm256_loadu_si256(keys + 24);
  __m256i x5 = _mm256_loadu_si256(keys + 32);
  __m256i x6 = _mm256_loadu_si256(keys + 40);
  __m256i x7 = _mm256_loadu_si256(keys + 48);
  __m256i x8 = _mm256_loadu_si256(keys + 56);
  x1 = _mm256_cmpeq_epi32(x1, v);
  x2 = _mm256_cmpeq_epi32(x2, v);
  x3 = _mm256_cmpeq_epi32(x3, v);
  x4 = _mm256_cmpeq_epi32(x4, v);
  x5 = _mm256_cmpeq_epi32(x5, v);
  x6 = _mm256_cmpeq_epi32(x6, v);
  x7 = _mm256_cmpeq_epi32(x7, v);
  x8 = _mm256_cmpeq_epi32(x8, v);
  uint64_t m = movemask_epi32(x1);
  m |= movemask_epi32(x2) << 8;
  m |= movemask_epi32(x3) << 16;
  m |= movemask_epi32(x4) << 24;
  m |= movemask_epi32(x5) << 32;
  m |= movemask_epi32(x6) << 40;
  m |= movemask_epi32(x7) << 48;
  m |= movemask_epi32(x8) << 56;
  return m;
}

/// Compares a 32 bit item with 32 keys of the sketch using AVX2 instructions.
/// @return a bitmask of the matching keys.
OPT_INLINE static uint64_t Compare32Keys32Bit(const __m256i& v,
                                              const uint32_t* keys) {
  __m256i x1 = _mm256_loadu_si256(keys);
  __m256i x2 = _mm256_loadu_si256(keys + 8);
  __m256i x3 = _mm256_loadu_si256(keys + 16);
  __m256i x4 = _mm256_loadu_si256(keys + 24);
  x1 = _mm256_cmpeq_epi32(x1, v);
  x2 = _mm256_cmpeq_epi32(x2, v);
  x3 = _mm256_cmpeq_epi32(x3, v);
  x4 = _mm256_cmpeq_epi32(x4, v);
  uint64_t m = movemask_epi32(x1);
  m |= movemask_epi32(x2) << 8;
  m |= movemask_epi32(x3) << 16;
  m |= movemask_epi32(x4) << 24;
  return m;
}

/// Compares a 64 bit item with 32 keys of the sketch using AVX2 instructions.
/// @return a bitmask of the matching keys.
OPT_INLINE static uint64_t Compare32Keys64Bit(const __m256i& v,
                                              const uint64_t* keys) {
  __m256i x1 = _mm256_loadu_si256(keys);
  __m256i x2 = _mm256_loadu_si256(keys + 4);
  __m256i x3 = _mm256_loadu_si256(keys + 8);
  __m256i x4 = _mm256_loadu_si256(keys + 12);
  __m256i x5 = _mm256_loadu_si256(keys + 16);
  __m256i x6 = _mm256_loadu_si256(keys + 20);
  __m256i x7 = _mm256_loadu_si256(keys + 24);
  __m256i x8 = _mm256_loadu_si256(keys + 28);
  x1 = _mm256_cmpeq_epi64(x1, v);
  x2 = _mm256_cmpeq_epi64(x2, v);
  x3 = _mm256_cmpeq_epi64(x3, v);
  x4 = _mm256_cmpeq_epi64(x4, v);
  x5 = _mm256_cmpeq_epi64(x5, v);
  x6 = _mm256_cmpeq_epi64(x6, v);
  x7 = _mm256_cmpeq_epi64(x7, v);
  x8 = _mm256_cmpeq_epi64(x8, v);
  uint64_t m = movemask_epi64(x1);
  m |= movemask_epi64(x2) << 4;
  m |= movemask_epi64(x3) << 8;
  m |= movemask_epi64(x4) << 12;
  m |= movemask_epi64(x5) << 16;
  m |= movemask_epi64(x6) << 20;
  m |= movemask_epi64(x7) << 24;
  m |= movemask_epi64(x8) << 28;
  return m;
}

}  // namespace detail
