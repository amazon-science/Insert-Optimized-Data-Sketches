#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "compiler.hpp"
#include "hash.hpp"
#include "helpers.hpp"
#include "simd.hpp"
#include "types.hpp"

namespace heap {

template <typename T>
class SpaceSaving {
 public:
  SpaceSaving(size_t k = 96)
      : k_(k), values(detail::sequence<T>(k_)), weights(k_, 0) {}

  void Insert(const T& v) noexcept {
    const auto& value = Normalized(v);
    size_t i = 0;
    if (auto it = std::find(values.begin(), values.end(), value);
        it != values.end()) {
      i = it - values.begin();
    }
    UpdateHeap(value, i);
  }

 private:
  size_t k_;
  std::vector<T> values;
  std::vector<uint64_t> weights;

  inline void SiftDown(size_t i) {
    const uint64_t weight = weights[i];
    const T value = values[i];
    size_t parent = i;
    size_t child = 2 * parent + 1;
    while (child < k_) {
      // Switch to right child if it is smaller.
      const size_t right_child = child + 1;
      if (right_child < k_ && weights[child] > weights[right_child]) {
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

  void UpdateHeap(const T& value, size_t i) {
    weights[i] += 1;
    values[i] = value;
    SiftDown(i);
  }

  OPT_INLINE const T& Normalized(const auto& value) const {
    if constexpr (std::is_floating_point_v<T>) {
      static constexpr T kZero = 0.0;
      return (value == kZero) ? kZero : value;
    }
    return value;
  }
};

}  // namespace heap
