// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

// This implementation is based on the Apache DataSketches v4.1.0 implementation
// of the sketch. We made the following significant changes:
// - Adapted the code to the common interface used in this repo by copying over
//    the relevant functions.
// - Removed quantiles_sorted_view as we are only looking at inserts here.
// - Removed min and max element tracking, which we don't need.
// - Removed self move protection for fundamental types, saving a few branches.
// - Removed some debug assertions from the hot path.
// - Caching the level capacities in an array. The level capacity function is
//    called very often, so caching the results improves performance.
// - Changed the randomness source from std::mt19937 to pcg32_fast with is much
//    faster without sacrificing randomness quality.
// - Reducing the number of allocations pre-allocating the maximum
//    sketch size. It can at maximum contain 985 elements for k=200.

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "const.hpp"
#include "pcg_random.hpp"
#include "span.hpp"

namespace final {

/// KLL sketch constants
namespace kll_constants {
/// default value of parameter K
const uint16_t DEFAULT_K = 200;
const uint8_t DEFAULT_M = 8;
/// min value of parameter K
const uint16_t MIN_K = DEFAULT_M;
/// max value of parameter K
const uint16_t MAX_K = (1 << 16) - 1;
}  // namespace kll_constants

class kll_helper {
 public:
  // this version moves objects within the same buffer
  // assumes that destination has initialized objects
  // does not destroy the originals after the move
  template <typename T, typename C>
  static void merge_sorted_arrays(T* buf, uint32_t start_a, uint32_t len_a,
                                  uint32_t start_b, uint32_t len_b,
                                  uint32_t start_c) {
    const uint32_t len_c = len_a + len_b;
    const uint32_t lim_a = start_a + len_a;
    const uint32_t lim_b = start_b + len_b;
    const uint32_t lim_c = start_c + len_c;

    uint32_t a = start_a;
    uint32_t b = start_b;

    for (size_t c = start_c; c < lim_c; ++c) {
      if (a == lim_a) {
        if (std::is_fundamental_v<T> || b != c) buf[c] = std::move(buf[b]);
        ++b;
      } else if (b == lim_b) {
        if (std::is_fundamental_v<T> || a != c) buf[c] = std::move(buf[a]);
        ++a;
      } else if (C()(buf[a], buf[b])) {
        if (std::is_fundamental_v<T> || a != c) buf[c] = std::move(buf[a]);
        ++a;
      } else {
        if (std::is_fundamental_v<T> || b != c) buf[c] = std::move(buf[b]);
        ++b;
      }
    }
  }

  template <typename T>
  static void move_construct(T* src, size_t src_first, size_t src_last, T* dst,
                             size_t dst_first, bool destroy) {
    while (src_first != src_last) {
      new (&dst[dst_first++]) T(std::move(src[src_first]));
      if (destroy) src[src_first].~T();
      src_first++;
    }
  }
};

template <typename T, typename C = std::less<T>, typename A = std::allocator<T>>
class KarninLangLiberty {
 public:
  using value_type = T;
  using comparator = C;
  using allocator_type = A;
  using vector_u32 = std::vector<
      uint32_t,
      typename std::allocator_traits<A>::template rebind_alloc<uint32_t>>;

  explicit KarninLangLiberty(uint16_t k = 200, const C& comparator = C(),
                             const A& allocator = A())

      : comparator_(comparator),
        allocator_(allocator),
        k_(k),
        m_(8),
        min_k_(k),
        num_levels_(1),
        is_level_zero_sorted_(false),
        n_(0),
        levels_(kMaxNumLevels, 0, allocator),
        max_capacity_(compute_total_capacity(kMaxNumLevels)),
        level_capacities(compute_level_capacities(k_, m_)) {
    if (k < kll_constants::MIN_K || k > kll_constants::MAX_K) {
      throw std::invalid_argument(
          "K must be >= " + std::to_string(kll_constants::MIN_K) + " and <= " +
          std::to_string(kll_constants::MAX_K) + ": " + std::to_string(k));
    }
    levels_[0] = levels_[1] = k;
    items_storage_ = allocator_.allocate(max_capacity_);
    items_ = std::span<T>(items_storage_ + (max_capacity_ - k), k);
  }

  ~KarninLangLiberty() {
    if (items_storage_ != nullptr) {
      const uint32_t begin = levels_[0];
      const uint32_t end = levels_[num_levels_];
      for (uint32_t i = begin; i < end; i++) items_[i].~T();
      allocator_.deallocate(items_storage_, max_capacity_);
    }
    // reset_sorted_view();
  }

  KarninLangLiberty(const KarninLangLiberty& other) = delete;
  KarninLangLiberty& operator=(const KarninLangLiberty& other) = delete;
  KarninLangLiberty(KarninLangLiberty&& other) = delete;
  KarninLangLiberty& operator=(KarninLangLiberty&& other) = delete;

  /// Insert a value into the sketch.
  void Insert(const T& x) noexcept { update(x); }

  /// Insert a value into the sketch.
  void Insert(T&& x) noexcept { update(std::move(x)); }

 private:
  /// Randomness source.
  std::independent_bits_engine<pcg32_fast, 1, uint32_t> random_bit;

  C comparator_;
  A allocator_;
  uint16_t k_;
  uint8_t m_;       // minimum buffer "width"
  uint16_t min_k_;  // for error estimation after merging with different k
  uint8_t num_levels_;
  bool is_level_zero_sorted_;
  uint64_t n_;
  size_t max_capacity_;
  vector_u32 levels_;
  T* items_storage_;
  std::span<T> items_;

  /// 60 levels are enough to fit std::numeric_limits<size_t>::max() elements.
  static constexpr size_t kMaxNumLevels = 60;
  std::array<uint16_t, kMaxNumLevels> level_capacities;

  void randomly_halve_down(T* buf, uint32_t start, uint32_t length) {
    const uint32_t half_length = length / 2;
    const uint32_t offset = random_bit();
    uint32_t j = start + offset;
    for (uint32_t i = start; i < (start + half_length); i++) {
      if (std::is_fundamental_v<T> || i != j) buf[i] = std::move(buf[j]);
      j += 2;
    }
  }

  void randomly_halve_up(T* buf, uint32_t start, uint32_t length) {
    const uint32_t half_length = length / 2;
    const uint32_t offset = random_bit();
    uint32_t j = (start + length) - 1 - offset;
    for (uint32_t i = (start + length) - 1; i >= (start + half_length); i--) {
      if (std::is_fundamental_v<T> || i != j) buf[i] = std::move(buf[j]);
      j -= 2;
    }
  }

  inline bool is_even(uint32_t value) const { return (value & 1) == 0; }

  inline bool is_odd(uint32_t value) const { return (value & 1) > 0; }

  inline uint16_t level_capacity(uint16_t, uint8_t numLevels, uint8_t height,
                                 uint8_t) const {
    const uint8_t depth = numLevels - height - 1;
    return level_capacities[depth];
  }

  static inline uint16_t int_cap_aux(uint16_t k, uint8_t depth) {
    if (depth <= 30) return int_cap_aux_aux(k, depth);
    const uint8_t half = depth / 2;
    const uint8_t rest = depth - half;
    const uint16_t tmp = int_cap_aux_aux(k, half);
    return int_cap_aux_aux(tmp, rest);
  }

  static inline uint16_t int_cap_aux_aux(uint16_t k, uint8_t depth) {
    const uint64_t twok = k << 1;  // for rounding, we pre-multiply by 2
    const uint64_t tmp =
        (uint64_t)(((uint64_t)twok << depth) / detail::powers_of_three[depth]);
    const uint64_t result =
        (tmp + 1) >> 1;  // then here we add 1 and divide by 2
    return static_cast<uint16_t>(result);
  }

  /// The level_capacity function is called very frequently, so we precompute a
  /// lookup table at compile time.
  static auto compute_level_capacities(uint16_t k, uint8_t min_wid) {
    std::array<uint16_t, kMaxNumLevels> level_capacities{};
    // std::fill is not constexpr before C++20
    for (auto& level_capacity : level_capacities) {
      level_capacity = min_wid;
    }
    for (size_t depth = 0; depth < level_capacities.size(); ++depth) {
      level_capacities[depth] =
          std::max<uint16_t>(min_wid, int_cap_aux(k, depth));
      if (level_capacities[depth] == min_wid) break;
    }
    return level_capacities;
  }

  /// Computes the total sketch capacity given the sketch parameters.
  size_t compute_total_capacity(uint8_t num_levels) {
    size_t total = 0;
    for (uint8_t h = 0; h < num_levels; ++h) {
      total += level_capacity(k_, num_levels, h, m_);
    }
    return total;
  }

  template <
      typename TT = T,
      typename std::enable_if<std::is_floating_point<TT>::value, int>::type = 0>
  static inline bool check_update_item(TT item) {
    return !std::isnan(item);
  }

  template <typename TT = T,
            typename std::enable_if<!std::is_floating_point<TT>::value,
                                    int>::type = 0>
  static inline bool check_update_item(TT) {
    return true;
  }

  bool is_empty() const { return n_ == 0; }

  uint8_t find_level_to_compact() const {
    uint8_t level = 0;
    while (true) {
      const uint32_t pop = levels_[level + 1] - levels_[level];
      const uint32_t cap = level_capacity(k_, num_levels_, level, m_);
      if (pop >= cap) {
        return level;
      }
      level++;
    }
  }

  void add_empty_top_level_to_completely_full_sketch() {
    const uint32_t cur_total_cap = levels_[num_levels_];

    // note that merging MIGHT over-grow levels_, in which case we might not
    // have to grow it here
    const uint8_t new_levels_size = num_levels_ + 2;
    if (levels_.size() < new_levels_size) {
      levels_.resize(new_levels_size);
    }

    const uint32_t delta_cap = level_capacity(k_, num_levels_ + 1, 0, m_);
    const uint32_t new_total_cap = cur_total_cap + delta_cap;

    items_ = std::span<T>(items_storage_ + max_capacity_ - new_total_cap,
                          new_total_cap);

    // this loop includes the old "extra" index at the top
    for (uint8_t i = 0; i <= num_levels_; i++) {
      levels_[i] += delta_cap;
    }

    num_levels_++;
    levels_[num_levels_] =
        new_total_cap;  // initialize the new "extra" index at the top
  }

  void compress_while_updating(void) {
    const uint8_t level = find_level_to_compact();

    // It is important to add the new top level right here. Be aware that this
    // operation grows the buffer and shifts the data and also the boundaries of
    // the data and grows the levels array and increments num_levels_
    if (level == (num_levels_ - 1)) {
      add_empty_top_level_to_completely_full_sketch();
    }

    const uint32_t raw_beg = levels_[level];
    const uint32_t raw_lim = levels_[level + 1];
    // +2 is OK because we already added a new top level if necessary
    const uint32_t pop_above = levels_[level + 2] - raw_lim;
    const uint32_t raw_pop = raw_lim - raw_beg;
    const bool odd_pop = is_odd(raw_pop);
    const uint32_t adj_beg = odd_pop ? raw_beg + 1 : raw_beg;
    const uint32_t adj_pop = odd_pop ? raw_pop - 1 : raw_pop;
    const uint32_t half_adj_pop = adj_pop / 2;
    const uint32_t destroy_beg = levels_[0];

    // level zero might not be sorted, so we must sort it if we wish to compact
    // it sort_level_zero() is not used here because of the adjustment for odd
    // number of items
    if ((level == 0) && !is_level_zero_sorted_) {
      std::sort(items_.begin() + adj_beg, items_.begin() + adj_beg + adj_pop, comparator_);
    }
    if (pop_above == 0) {
      randomly_halve_up(items_.data(), adj_beg, adj_pop);
    } else {
      randomly_halve_down(items_.data(), adj_beg, adj_pop);
      kll_helper::merge_sorted_arrays<T, C>(items_.data(), adj_beg, half_adj_pop,
                                            raw_lim, pop_above,
                                            adj_beg + half_adj_pop);
    }
    levels_[level + 1] -= half_adj_pop;  // adjust boundaries of the level above
    if (odd_pop) {
      levels_[level] =
          levels_[level + 1] - 1;  // the current level now contains one item
      if (std::is_fundamental_v<T> || levels_[level] != raw_beg) {
        // namely this leftover guy
        items_[levels_[level]] = std::move(items_[raw_beg]);
      }
    } else {
      levels_[level] = levels_[level + 1];  // the current level is now empty
    }

    // finally, we need to shift up the data in the levels below
    // so that the freed-up space can be used by level zero
    if (level > 0) {
      const uint32_t amount = raw_beg - levels_[0];
      std::move_backward(items_.begin() + levels_[0],
                         items_.begin() + levels_[0] + amount,
                         items_.begin() + levels_[0] + half_adj_pop + amount);
      for (uint8_t lvl = 0; lvl < level; lvl++) levels_[lvl] += half_adj_pop;
    }
    for (uint32_t i = 0; i < half_adj_pop; i++) items_[i + destroy_beg].~T();
  }

  uint32_t internal_update() {
    if (levels_[0] == 0) compress_while_updating();
    n_++;
    is_level_zero_sorted_ = false;
    return --levels_[0];
  }

  template <typename FwdT>
  void update(FwdT&& item) {
    if (!check_update_item(item)) {
      return;
    }
    // min and max are always copies
    const uint32_t index = internal_update();
    new (&items_[index]) T(std::forward<FwdT>(item));
    // reset_sorted_view();
  }
};

}  // namespace final
