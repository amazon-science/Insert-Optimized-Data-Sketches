#pragma once

#include <array>
#include <cstddef>

namespace detail {

/// @return compile-time sequence 0..k.
template <typename T, size_t K>
constexpr auto sequence() {
  std::array<T, K> sequence{};
  for (size_t i = 0; i < K; ++i) {
    sequence[i] = i;
  }
  return sequence;
}

/// @return sequence 0..k.
template <typename T>
constexpr auto sequence(size_t k) {
  std::vector<T> sequence(k);
  for (size_t i = 0; i < k; ++i) {
    sequence[i] = i;
  }
  return sequence;
}

}  // namespace detail
