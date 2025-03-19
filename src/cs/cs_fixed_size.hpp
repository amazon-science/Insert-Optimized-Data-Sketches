#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "hash.hpp"

namespace fixed_size {

template <typename T, size_t t = 2048, size_t d = 5>
class CountSketch {
 public:
  CountSketch() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    for (size_t i = 0; i < d; ++i) {
      seeds_[i] = gen();
    }
  }

  void Insert(const T& value) noexcept {
    for (size_t i = 0; i < d; ++i) {
      __uint128_t hash = detail::Hash(value, seeds_[i]);
      size_t h = static_cast<size_t>(hash) % t;
      int64_t sign = ((hash >> 127) & 1) ? 1 : -1;
      C[i * t + h] += sign;
    }
  }

 private:
  std::array<int64_t, t * d> C{};
  std::array<__uint128_t, d> seeds_{};
};

}  // namespace fixed_size
