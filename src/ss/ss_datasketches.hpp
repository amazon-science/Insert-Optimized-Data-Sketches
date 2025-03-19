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

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

#include "MurmurHash3.h"

namespace std {
template <>
struct hash<__int128_t> {
  size_t operator()(__int128_t x) const {
    uint64_t high = x >> 64;
    uint64_t low = static_cast<uint64_t>(x);
    return hash<uint64_t>()(high) ^ (hash<uint64_t>()(low) << 1);
  }
};

template <>
struct hash<__uint128_t> {
  size_t operator()(__uint128_t x) const {
    uint64_t high = x >> 64;
    uint64_t low = static_cast<uint64_t>(x);
    return hash<uint64_t>()(high) ^ (hash<uint64_t>()(low) << 1);
  }
};
}  // namespace std

namespace datasketches {

template <typename K, typename V = uint64_t, typename H = std::hash<K>,
          typename E = std::equal_to<K>, typename A = std::allocator<K>>
class reverse_purge_hash_map {
 public:
  using AllocV = typename std::allocator_traits<A>::template rebind_alloc<V>;
  using AllocU16 =
      typename std::allocator_traits<A>::template rebind_alloc<uint16_t>;

  reverse_purge_hash_map(uint8_t lg_cur_size, uint8_t lg_max_size,
                         const E& equal, const A& allocator)
      : equal_(equal),
        allocator_(allocator),
        lg_cur_size_(lg_cur_size),
        lg_max_size_(lg_max_size),
        num_active_(0),
        keys_(allocator_.allocate(1ULL << lg_cur_size)),
        values_(nullptr),
        states_(nullptr) {
    AllocV av(allocator_);
    values_ = av.allocate(1ULL << lg_cur_size);
    AllocU16 au16(allocator_);
    states_ = au16.allocate(1ULL << lg_cur_size);
    std::fill(states_, states_ + (1ULL << lg_cur_size),
              static_cast<uint16_t>(0));
  }

  ~reverse_purge_hash_map() {
    const uint32_t size = 1 << lg_cur_size_;
    if (num_active_ > 0) {
      for (uint32_t i = 0; i < size; i++) {
        if (is_active(i)) {
          keys_[i].~K();
          if (--num_active_ == 0) break;
        }
      }
    }
    if (keys_ != nullptr) {
      allocator_.deallocate(keys_, size);
    }
    if (values_ != nullptr) {
      AllocV av(allocator_);
      av.deallocate(values_, size);
    }
    if (states_ != nullptr) {
      AllocU16 au16(allocator_);
      au16.deallocate(states_, size);
    }
  }

  reverse_purge_hash_map(const reverse_purge_hash_map& other) = delete;
  reverse_purge_hash_map(reverse_purge_hash_map&& other) noexcept = delete;
  reverse_purge_hash_map& operator=(const reverse_purge_hash_map& other) =
      delete;
  reverse_purge_hash_map& operator=(reverse_purge_hash_map&& other) = delete;

  template <typename FwdK>
  V adjust_or_insert(FwdK&& key, V value) {
    const uint32_t num_active_before = num_active_;
    const uint32_t index = internal_adjust_or_insert(key, value);
    if (num_active_ > num_active_before) {
      new (&keys_[index]) K(std::forward<FwdK>(key));
      return resize_or_purge_if_needed();
    }
    return 0;
  }

  uint32_t get_capacity() const {
    return static_cast<uint32_t>((1 << lg_cur_size_) * LOAD_FACTOR);
  }

 private:
  static constexpr double LOAD_FACTOR = 0.75;
  static constexpr uint16_t DRIFT_LIMIT = 1024;  // used only for stress testing
  static constexpr uint32_t MAX_SAMPLE_SIZE =
      1024;  // number of samples to compute approximate median during purge

  E equal_;
  A allocator_;
  uint8_t lg_cur_size_;
  uint8_t lg_max_size_;
  uint32_t num_active_;
  K* keys_;
  V* values_;
  uint16_t* states_;

  inline bool is_active(uint32_t index) const { return states_[index] > 0; }

  void subtract_and_keep_positive_only(V amount) {
    // starting from the back, find the first empty cell,
    // which establishes the high end of a cluster.
    uint32_t first_probe = (1 << lg_cur_size_) - 1;
    while (is_active(first_probe)) first_probe--;
    // when we find the next non-empty cell, we know we are at the high end of a
    // cluster work towards the front, delete any non-positive entries.
    for (uint32_t probe = first_probe; probe-- > 0;) {
      if (is_active(probe)) {
        if (values_[probe] <= amount) {
          hash_delete(probe);  // does the work of deletion and moving higher
                               // items towards the front
          num_active_--;
        } else {
          values_[probe] -= amount;
        }
      }
    }
    // now work on the first cluster that was skipped
    for (uint32_t probe = (1 << lg_cur_size_); probe-- > first_probe;) {
      if (is_active(probe)) {
        if (values_[probe] <= amount) {
          hash_delete(probe);
          num_active_--;
        } else {
          values_[probe] -= amount;
        }
      }
    }
  }

  void hash_delete(uint32_t delete_index) {
    // Looks ahead in the table to search for another
    // item to move to this location
    // if none are found, the status is changed
    states_[delete_index] = 0;  // mark as empty
    keys_[delete_index].~K();
    uint16_t drift = 1;
    const uint32_t mask = (1 << lg_cur_size_) - 1;
    uint32_t probe =
        (delete_index + drift) & mask;  // map length must be a power of 2
    // advance until we find a free location replacing locations as needed
    while (is_active(probe)) {
      if (states_[probe] > drift) {
        // move current element
        new (&keys_[delete_index]) K(std::move(keys_[probe]));
        values_[delete_index] = values_[probe];
        states_[delete_index] = states_[probe] - drift;
        states_[probe] = 0;  // mark as empty
        keys_[probe].~K();
        drift = 0;
        delete_index = probe;
      }
      probe = (probe + 1) & mask;
      drift++;
      // only used for theoretical analysis
      if (drift >= DRIFT_LIMIT)
        throw std::logic_error("drift: " + std::to_string(drift) +
                               " >= DRIFT_LIMIT");
    }
  }

  uint32_t internal_adjust_or_insert(const K& key, V value) {
    const uint32_t mask = (1 << lg_cur_size_) - 1;
    uint32_t index = fmix64(H()(key)) & mask;
    uint16_t drift = 1;
    while (is_active(index)) {
      if (E()(keys_[index], key)) {
        // adjusting the value of an existing key
        values_[index] += value;
        return index;
      }
      index = (index + 1) & mask;
      drift++;
      // only used for theoretical analysis
      if (drift >= DRIFT_LIMIT) throw std::logic_error("drift limit reached");
    }
    // adding the key and value to the table
    if (num_active_ > get_capacity()) {
      throw std::logic_error("num_active " + std::to_string(num_active_) +
                             " > capacity " + std::to_string(get_capacity()));
    }
    values_[index] = value;
    states_[index] = drift;
    num_active_++;
    return index;
  }

  V resize_or_purge_if_needed() {
    if (num_active_ > get_capacity()) {
      if (lg_cur_size_ < lg_max_size_) {  // can grow
        resize(lg_cur_size_ + 1);
      } else {  // at target size, must purge
        const V offset = purge();
        if (num_active_ > get_capacity()) {
          throw std::logic_error("purge did not reduce number of active items");
        }
        return offset;
      }
    }
    return 0;
  }

  void resize(uint8_t lg_new_size) {
    const uint32_t old_size = 1 << lg_cur_size_;
    K* old_keys = keys_;
    V* old_values = values_;
    uint16_t* old_states = states_;
    const uint32_t new_size = 1 << lg_new_size;
    keys_ = allocator_.allocate(new_size);
    AllocV av(allocator_);
    values_ = av.allocate(new_size);
    AllocU16 au16(allocator_);
    states_ = au16.allocate(new_size);
    std::fill(states_, states_ + new_size, static_cast<uint16_t>(0));
    num_active_ = 0;
    lg_cur_size_ = lg_new_size;
    for (uint32_t i = 0; i < old_size; i++) {
      if (old_states[i] > 0) {
        adjust_or_insert(std::move(old_keys[i]), old_values[i]);
        old_keys[i].~K();
      }
    }
    allocator_.deallocate(old_keys, old_size);
    av.deallocate(old_values, old_size);
    au16.deallocate(old_states, old_size);
  }

  V purge() {
    const uint32_t limit = std::min(MAX_SAMPLE_SIZE, num_active_);
    uint32_t num_samples = 0;
    uint32_t i = 0;
    AllocV av(allocator_);
    V* samples = av.allocate(limit);
    while (num_samples < limit) {
      if (is_active(i)) {
        samples[num_samples++] = values_[i];
      }
      i++;
    }
    std::nth_element(samples, samples + (num_samples / 2),
                     samples + num_samples);
    const V median = samples[num_samples / 2];
    av.deallocate(samples, limit);
    subtract_and_keep_positive_only(median);
    return median;
  }
};

template <typename T, typename W = uint64_t, typename H = std::hash<T>,
          typename E = std::equal_to<T>, typename A = std::allocator<T>>
class SpaceSaving {
 public:
  static constexpr uint8_t LG_MIN_MAP_SIZE = 3;

  /// lg_max_map_size of 8 results in epsilon 0.013671875 ≈ 1/73.14
  /// lg_max_map_size of 9 results in epsilon 0.00683594 ≈ 1/146.3
  SpaceSaving(uint8_t lg_max_map_size = 8,
              uint8_t lg_start_map_size = 8, const E& equal = E(),
              const A& allocator = A())
      : total_weight(0),
        offset(0),
        map(std::max(lg_start_map_size, LG_MIN_MAP_SIZE),
            std::max(lg_max_map_size, LG_MIN_MAP_SIZE), equal, allocator) {
    if (lg_start_map_size > lg_max_map_size)
      throw std::invalid_argument(
          "starting size must not be greater than maximum size");
  }

  void Insert(const T& value) noexcept { update(value, 1); }

 private:
  static const uint8_t SERIAL_VERSION = 1;
  static const uint8_t FAMILY_ID = 10;
  static const uint8_t PREAMBLE_LONGS_EMPTY = 1;
  static const uint8_t PREAMBLE_LONGS_NONEMPTY = 4;
  static constexpr double EPSILON_FACTOR = 3.5;

  W total_weight;
  W offset;
  reverse_purge_hash_map<T, W, H, E, A> map;

  void update(const T& item, W weight) {
    check_weight(weight);
    if (weight == 0) return;
    total_weight += weight;
    offset += map.adjust_or_insert(item, weight);
  }

  // version for integral signed type
  template <typename WW = W,
            typename std::enable_if<std::is_integral<WW>::value &&
                                        std::is_signed<WW>::value,
                                    int>::type = 0>
  static inline void check_weight(WW weight) {
    if (weight < 0) {
      throw std::invalid_argument("weight must be non-negative");
    }
  }

  // version for integral unsigned type
  template <typename WW = W,
            typename std::enable_if<std::is_integral<WW>::value &&
                                        std::is_unsigned<WW>::value,
                                    int>::type = 0>
  static inline void check_weight(WW) {}

  // version for floating point type
  template <
      typename WW = W,
      typename std::enable_if<std::is_floating_point<WW>::value, int>::type = 0>
  static inline void check_weight(WW weight) {
    if (weight < 0) {
      throw std::invalid_argument("weight must be non-negative");
    }
    if (std::isnan(weight)) {
      throw std::invalid_argument("weight must be a valid number");
    }
    if (std::isinf(weight)) {
      throw std::invalid_argument("weight must be finite");
    }
  }

  /// Row in the output from #get_frequent_items
  class row {
   public:
    row(const T* item, W weight, W offset)
        : item(item), weight(weight), offset(offset) {}
    /// @return item
    const T& get_item() const { return *item; }
    /// @return frequency (weight) estimate
    W get_estimate() const { return weight + offset; }
    /// @return estimate lower bound
    W get_lower_bound() const { return weight; }
    /// @return estimate upper bound
    W get_upper_bound() const { return weight + offset; }

   private:
    const T* item;
    W weight;
    W offset;
  };
};

}  // namespace datasketches
