#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "fastrange.hpp"
#include "hash.hpp"

namespace fastrange {

template <typename T>
class CountSketch {
 public:
  CountSketch(size_t t = 2048, size_t d = 5) : t_(t), d_(d), C(t * d) {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    seeds_.reserve(d_);
    for (size_t i = 0; i < d_; ++i) {
      seeds_.push_back(gen());
    }
  }

  void Insert(const T& value) noexcept {
    for (size_t i = 0; i < d_; ++i) {
      __uint128_t hash = detail::Hash(value, seeds_[i]);
      size_t h = fastrange64(static_cast<size_t>(hash), t_);
      int64_t sign = ((hash >> 127) & 1) ? 1 : -1;
      C[i * t_ + h] += sign;
    }
  }

 private:
  size_t t_;
  size_t d_;
  std::vector<int64_t> C;
  std::vector<size_t> hash_seeds;
  std::vector<__uint128_t> seeds_;
};

}  // namespace fastrange
