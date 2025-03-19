/**
 * Fair maps to intervals without division.
 * Reference :
 * http://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/
 *
 * (c) Daniel Lemire
 * Apache License 2.0
 */

#pragma once

#include <cstdint>

static __attribute__((always_inline)) inline uint32_t fastrange32(
    const uint32_t word, const uint32_t p) {
  return (uint32_t)(((uint64_t)word * (uint64_t)p) >> 32);
}

static __attribute__((always_inline)) inline uint64_t fastrange64(
    const uint64_t word, const uint64_t p) {
  return (uint64_t)(((__uint128_t)word * (__uint128_t)p) >> 64);
}
