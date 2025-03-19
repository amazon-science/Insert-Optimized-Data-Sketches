#pragma once

#include <cstdint>
#include <type_traits>

#include "MurmurHash3.h"
#include "compiler.hpp"
#include "types.hpp"

static constexpr uint64_t kSeed = 9001;

namespace detail {

ALWAYS_INLINE static uint32_t fp_hash_bits(const float& f) {
  constexpr uint32_t kMask = std::numeric_limits<int32_t>::max();
  auto i = *reinterpret_cast<const uint32_t*>(&f);
  return (i & kMask) ? i : 0;
}

ALWAYS_INLINE static uint64_t fp_hash_bits(const double& f) {
  constexpr uint64_t kMask = std::numeric_limits<int64_t>::max();
  auto i = *reinterpret_cast<const uint64_t*>(&f);
  return (i & kMask) ? i : 0;
}

template <typename T>
OPT_INLINE constexpr __uint128_t Hash(const T& key, uint64_t seed) {
  if constexpr (std::is_same_v<T, int16_t>) {
    return MurmurHash3_x64_128(static_cast<uint16_t>(key), seed);
  } else if constexpr (std::is_same_v<T, int32_t>) {
    return MurmurHash3_x64_128(static_cast<uint32_t>(key), seed);
  } else if constexpr (std::is_same_v<T, int64_t>) {
    return MurmurHash3_x64_128(static_cast<uint64_t>(key), seed);
  } else if constexpr (std::is_same_v<T, __int128_t>) {
    return MurmurHash3_x64_128(static_cast<__uint128_t>(key), seed);
  } else if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
    return MurmurHash3_x64_128(fp_hash_bits(key), seed);
  } else if constexpr (is_string_v<T> || std::is_same_v<T, std::string_view>) {
    return MurmurHash3_x64_128(key.data(), key.size(), seed);
  } else {
    return MurmurHash3_x64_128(key, seed);
  }
}

template <typename T>
OPT_INLINE constexpr __uint128_t Hash(const T& key) {
  return Hash(key, kSeed);
}

template <typename T>
OPT_INLINE constexpr __uint128_t HashNoUnroll(const T& key) {
  if constexpr (is_string_v<T> || std::is_same_v<T, std::string_view>) {
    return MurmurHash3_x64_128(key.data(), key.size(), kSeed);
  } else {
    return MurmurHash3_x64_128(&key, sizeof(key), kSeed);
  }
}

/// Roll down 128 hash to 64 bit hash.
OPT_INLINE constexpr uint64_t roll_down(const __uint128_t& hash) {
  return hash ^ (hash >> 64);
}

}  // namespace detail
