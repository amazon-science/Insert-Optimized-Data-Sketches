#pragma once

#include <cmath>
#include <cstddef>
#include <unordered_map>

#include "compiler.hpp"
#include "hash.hpp"
#include "helpers.hpp"
#include "simd.hpp"
#include "types.hpp"

namespace map {

template <typename T>
class SpaceSaving {
 public:
  SpaceSaving(size_t k = 96) : k_(k) {}

  void Insert(const T& v) noexcept {
    const auto& value = Normalized(v);

    if (auto it = values.find(value); it != values.end()) {
      ++(it->second);
    } else {
      if (values.size() < k_) {
        values[value] = 1;
      } else {
        auto min_it = std::min_element(
            values.begin(), values.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });

        uint64_t min_weight = min_it->second + 1;
        values.erase(min_it);
        values[value] = min_weight;
      }
    }
  }

 private:
  size_t k_;
  std::unordered_map<T, uint64_t> values;

  OPT_INLINE const T& Normalized(const auto& value) const {
    if constexpr (std::is_floating_point_v<T>) {
      static constexpr T kZero = 0.0;
      return (value == kZero) ? kZero : value;
    }
    return value;
  }
};

}  // namespace map
