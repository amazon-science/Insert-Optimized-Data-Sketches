#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "compiler.hpp"
#include "hash.hpp"

namespace final_no_murmur_unroll {

template <typename T, size_t t = 2048, size_t d = 5>
class CountSketch {
  // For efficient hash range reduction and splitting the hash.
  static_assert((t & (t - 1)) == 0, "t must be a power of 2");
  // We need CTZ(2*t) bits for each of the d layers.
  static_assert(__builtin_ctz(size_t{2} * t) * d <= sizeof(__uint128_t) * 8,
                "hash must have enough bits for each layer of the sketch");

 public:
  /// Insert a value into the sketch.
  void Insert(const T& value) noexcept {
    const __uint128_t hash = detail::HashNoUnroll(value);
    Insert(hash);
  }

  /// Insert a hashed value into the sketch.
  void Insert(const __uint128_t& hash) noexcept {
    for (size_t j = 0; j < d; j++) {
      const auto [h, sign] = HashExtract(hash, j);
      GetCounter(j, h) += sign;
    }
  }

 private:
  /// Counters of the sketch
  std::array<int64_t, t * d> C;

  /// Number of bits needed for one hash for one layer of the sketch. The hash
  /// is in the range [0, 2t). t is a power of 2. Hence we can just count the
  /// trailing zeros.
  constexpr static size_t hash_bits = __builtin_ctz(2 * t);

  /// Counter accessor used to be able to reuse code
  int64_t const& GetCounter(size_t j, size_t h) const { return C[j * t + h]; }

  /// From  Effective C++, 3rd ed by Scott Meyers, ISBN-13: 9780321334879
  /// Avoid Duplication in const and Non-const Member Function," p. 23 Item 3
  int64_t& GetCounter(size_t j, size_t h) {
    return const_cast<int64_t&>(std::as_const(*this).GetCounter(j, h));
  }

  /// Extract the two hashes needed for counting (h, g) from one 128 bit hash.
  ///
  /// First, we get a j-th hash in the range [0, 2t). Then, we split it
  /// according to "Small Summaries for Big Data":
  ///  Still, in some high performance cases, where huge amounts of data is
  ///  processed, we wish to make this part as efficient as possible. One way
  ///  to reduce the cost of hashing is to compute a single hash function for
  ///  row j that maps to the range 2t, and use the last bit to determine gj
  ///  (+1 or −1), while the remaining bits determine hj.
  OPT_INLINE std::pair<uint32_t, int64_t> HashExtract(const __uint128_t& hash,
                                                      size_t j) const {
    uint32_t hashes;
    if constexpr (__builtin_ctz(size_t{2} * t) * d <= sizeof(hash) * 8 / 2) {
      // We only ever use the lower half of the 128 bit hash. In the following
      // statement we tell the compiler that explicitly, resulting in the
      // compiler generating only half the instructions.
      const auto lower_hash = static_cast<uint64_t>(hash);

      hashes = (lower_hash >> (j * hash_bits)) % t;
    } else {
      hashes = (hash >> (j * hash_bits)) % t;
    }

    const uint32_t h = hashes >> 1;
    const uint32_t g = hashes & 1;

    // Map g to sign: 0 -> -1, 1 -> +1
    const int64_t sign = (static_cast<int64_t>(g) * 2) - 1;

    return {h, sign};
  }
};

}  // namespace final_no_murmur_unroll
