#pragma once

#include <cmath>
#include <cstddef>

#include "compiler.hpp"
#include "hash.hpp"
#include "helpers.hpp"
#include "simd.hpp"
#include "types.hpp"

namespace naive {

template <typename T>
class SpaceSaving {
 public:
  SpaceSaving(size_t k = 96)
      : k_(k), values(detail::sequence<T>(k_)), weights(k_, 0) {}

  void Insert(const T& v) noexcept {
    const auto& value = Normalized(v);
    if (auto it = std::find(values.begin(), values.end(), value);
        it != values.end()) {
      ++weights[it - values.begin()];
    } else {
      auto min_it = std::min_element(weights.begin(), weights.end());
      ++(*min_it);
      values[min_it - weights.begin()] = value;
    }
  }

 private:
  size_t k_;
  std::vector<T> values;
  std::vector<uint64_t> weights;

  OPT_INLINE const T& Normalized(const auto& value) const {
    if constexpr (std::is_floating_point_v<T>) {
      static constexpr T kZero = 0.0;
      return (value == kZero) ? kZero : value;
    }
    return value;
  }
};

}  // namespace naive
