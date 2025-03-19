#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "compiler.hpp"
#include "hash.hpp"
#include "helpers.hpp"
#include "simd.hpp"
#include "types.hpp"

namespace final {

/// SpaceSaving sketch for frequent item estimation.
///
/// The implementation roughly follows the book
///   Cormode, Graham, and Ke Yi. Small summaries for big data. Cambridge
///   University Press, 2020.
/// It stores the weights and values in a min-heap, and leverages SIMD search
/// for fast find operations.
///
/// The sketch was introduced in the paper
///   Metwally, Ahmed, Divyakant Agrawal, and Amr El Abbadi. "Efficient
///   computation of frequent and top-k elements in data streams." International
///   conference on database theory. Berlin, Heidelberg: Springer Berlin
///   Heidelberg, 2005.
///
/// @tparam T the data type the sketch summarizes.
/// @tparam K number of elements the sketch can store.
template <typename T, size_t K = 96, typename = void>
class SpaceSaving {};

/// SpaceSaving sketch for frequent item estimation.
///
/// Specialization arithmetic types which are stored inside the sketch alongside
/// their weights, organized as a min-heap.
///
/// For more details see the doc comment on the SpaceSaving primary template.
template <typename T, size_t K>
class SpaceSaving<T, K,
                  std::enable_if_t<std::is_arithmetic_v<T> &&
                                   !std::is_same_v<T, __int128_t> &&
                                   !std::is_same_v<T, __uint128_t>>> {
 public:
  /// Update the weight of a given value.
  ///
  /// Takes O(K) time to check if the value already exists, and O(log(K)) time
  /// to update the heap.
  void Insert(const T& v) noexcept {
    const auto& value = Normalized(v);
    size_t i = Find</*NotFound=*/0>(value);
    UpdateHeap(value, i);
  }

 private:
  /// Returns a normalized representation of the given value.
  ///
  /// Since we use bit equality to compare the values, we need to normalize
  /// values that are equal but not bit equal to the same value.
  ///
  /// For floating point types, +0.0 == -0.0, but they have different binary
  /// representations. Hence we return one of them.
  OPT_INLINE const T& Normalized(const auto& value) const {
    if constexpr (std::is_floating_point_v<T>) {
      static constexpr T kZero = 0.0;
      return (value == kZero) ? kZero : value;
    }
    return value;
  }

  /// Sifts down the element at index i in the min heap
  ///
  /// This assumes that the weight at index i was increased, and will restore
  /// the min heap condition. It will also swap the elements in `values`
  /// accordingly.
  inline void SiftDown(size_t i) {
    const uint64_t weight = weights[i];
    const T value = values[i];
    size_t parent = i;
    size_t child = 2 * parent + 1;
    while (child < K) {
      // Switch to right child if it is smaller.
      const size_t right_child = child + 1;
      if (right_child < K && weights[child] > weights[right_child]) {
        child = right_child;
      }
      // If weight is not greater than the child's weight we are done.
      if (!(weight > weights[child])) break;
      // Else sift down.
      weights[parent] = weights[child];
      values[parent] = values[child];
      parent = child;
      child = 2 * parent + 1;
    }
    weights[parent] = weight;
    values[parent] = value;
  }

  /// Sets the value at index i in the min heap to value and increments the
  /// weight. Then restores the min heap condition.
  void UpdateHeap(const T& value, size_t i) {
    weights[i] += 1;
    values[i] = value;
    SiftDown(i);
  }

  /// Finds the given value if the values array.
  /// @param value the value to search for.
  /// @param i the index to start off at.
  /// @return the index of the value in the values array, or `NotFound`.
  /// @tparam NotFound the value to return if the value was not found.
  template <size_t NotFound = K>
  size_t Find(const T& value) const {
    size_t i = 0;
    if constexpr (sizeof(T) == 2) {
      const auto* data = reinterpret_cast<const uint16_t*>(values.data());
      const __m256i v =
          detail::broadcast_epi16(*reinterpret_cast<const uint16_t*>(&value));
      for (; i + 64 <= K; i += 64) {
        const uint64_t mask = detail::Compare64Keys16Bit(v, data + i);
        if (mask != 0) return i + __builtin_ctzll(mask);
      }
      for (; i + 32 <= K; i += 32) {
        const uint64_t mask = detail::Compare32Keys16Bit(v, data + i);
        if (mask != 0) return i + __builtin_ctzll(mask);
      }
    } else if constexpr (sizeof(T) == 4) {
      const auto* data = reinterpret_cast<const uint32_t*>(values.data());
      const __m256i v =
          detail::broadcast_epi32(*reinterpret_cast<const uint32_t*>(&value));
      for (; i + 64 <= K; i += 64) {
        const uint64_t mask = detail::Compare64Keys32Bit(v, data + i);
        if (mask != 0) return i + __builtin_ctzll(mask);
      }
      for (; i + 32 <= K; i += 32) {
        const uint64_t mask = detail::Compare32Keys32Bit(v, data + i);
        if (mask != 0) return i + __builtin_ctzll(mask);
      }
    } else if constexpr (sizeof(T) == 8) {
      const auto* data = reinterpret_cast<const uint64_t*>(values.data());
      const __m256i v =
          detail::broadcast_epi64(*reinterpret_cast<const uint64_t*>(&value));
      for (; i + 32 <= K; i += 32) {
        const uint64_t mask = detail::Compare32Keys64Bit(v, data + i);
        if (mask != 0) return i + __builtin_ctzll(mask);
      }
    } else {
      static_assert(!sizeof(T), "Unsupported datatype T");
    }
    return NotFound;
  }

  /// Values array filled with distinct dummy values by default.
  /// Needs to be 32-byte aligned for SIMD operations.
  alignas(32) std::array<T, K> values = detail::sequence<T, K>();
  /// Min heap of size k, with weights initialized to 0.
  std::array<uint64_t, K> weights{};
  /// K needs to be a multiple of 32 for the SIMD algorithm we use.
  static_assert(K % 32 == 0);
  static_assert(sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8 ||
                sizeof(T) == 16);
};

/// SpaceSaving sketch for frequent item estimation.
///
/// Specialization non-arithmetic types which are stored in an auxiliary array.
/// This implementation works just like the implementation for arithmetic types,
/// but instead of operating on the values themselves, we operator on 64 bit
/// hashes. For the Find operation, we have to check all matches from the
/// returned bitmask for equality instead of just taking the first one.
///
/// For more details see the doc comment on the SpaceSaving primary template.
template <typename T, size_t K>
class SpaceSaving<T, K,
                  std::enable_if_t<!std::is_arithmetic_v<T> ||
                                   std::is_same_v<T, __int128_t> ||
                                   std::is_same_v<T, __uint128_t>>> {
 public:
  /// Update the weight of element a given value.
  void Insert(const T& value) noexcept {
    __uint128_t hash = detail::Hash(value);
    Insert(value, hash);
  }

  /// Update the weight of element a given pre-hashed value.
  ///
  /// Takes O(K) time to check if the value already exists, and O(log(K)) time
  /// to update the heap.
  void Insert(const T& value, const __uint128_t& h) noexcept {
    uint64_t hash = detail::roll_down(h);
    size_t i = Find</*NotFound=*/0>(value, hash);
    UpdateHeap(value, hash, i);
  }

 private:
  /// Sifts down the element at index i in the min heap
  ///
  /// This assumes that the weight at index i was increased, and will restore
  /// the min heap condition. It will also swap the elements in `values`
  /// accordingly.
  inline void SiftDown(size_t i) {
    const uint64_t weight = weights[i];
    const uint64_t hash = hashes[i];
    T value = std::move(values[i]);
    size_t parent = i;
    size_t child = 2 * parent + 1;
    while (child < K) {
      // Switch to right child if it is smaller.
      const size_t right_child = child + 1;
      if (right_child < K && weights[child] > weights[right_child]) {
        child = right_child;
      }
      // If weight is not greater than the child's weight we are done.
      if (!(weight > weights[child])) break;
      // Else sift down.
      weights[parent] = weights[child];
      hashes[parent] = hashes[child];
      values[parent] = std::move(values[child]);
      parent = child;
      child = 2 * parent + 1;
    }
    weights[parent] = weight;
    hashes[parent] = hash;
    values[parent] = std::move(value);
  }

  /// Sets the value at index i in the min heap to value and increments the
  /// weight. Then restores the min heap condition.
  void UpdateHeap(const T& value, uint64_t hash, size_t i) {
    hashes[i] = hash;
    weights[i] += 1;
    values[i] = value;
    SiftDown(i);
  }

  /// Finds the given value if the values array.
  /// @return the index of the value in the values array, or `NotFound`.
  /// @tparam NotFound the value to return if the value was not found.
  template <size_t NotFound = K>
  size_t Find(const T& value, uint64_t hash) const {
    const auto* data = hashes.data();
    const __m256i v = _mm256_set1_epi64x(hash);
    for (size_t i = 0; i + 32 <= K; i += 32) {
      uint64_t mask = detail::Compare32Keys64Bit(v, data + i);
      while (mask != 0) {
        uint64_t j = i + __builtin_ctzll(mask);
        if (LIKELY(values[j] == value)) return j;
        mask &= mask - 1;
      }
    }
    return NotFound;
  }

  /// Hashes array filled with distinct dummy values by default.
  /// Needs to be 32-byte aligned for SIMD operations.
  alignas(32) std::array<uint64_t, K> hashes = detail::sequence<uint64_t, K>();
  /// Min heap of size k, with weights initialized to 0.
  std::array<uint64_t, K> weights{};
  /// Values array.
  std::array<T, K> values{};
  /// K needs to be a multiple of 32 for the SIMD algorithm we use.
  static_assert(K % 32 == 0);
};

}  // namespace final
