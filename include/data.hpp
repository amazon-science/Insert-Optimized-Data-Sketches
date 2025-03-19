#pragma once

#include <iomanip>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "hash.hpp"
#include "types.hpp"

template <typename T>
std::string to_string(const T& value) {
  std::stringstream ss;
  ss << std::fixed;
  int width = 0;

  if constexpr (std::is_same_v<T, float>) {
    ss << std::setprecision(std::numeric_limits<float>::max_digits10);
    width = 40;  // Enough to represent any float value
  } else if constexpr (std::is_same_v<T, double>) {
    ss << std::setprecision(std::numeric_limits<double>::max_digits10);
    width = 320;  // Enough to represent any double value
  } else if constexpr (std::is_same_v<T, int16_t>) {
    width = 6;  // -32768 to 32767
  } else if constexpr (std::is_same_v<T, int32_t>) {
    width = 11;  // -2147483648 to 2147483647
  } else if constexpr (std::is_same_v<T, int64_t>) {
    width = 20;  // -9223372036854775808 to 9223372036854775807
  } else if constexpr (std::is_same_v<T, uint16_t>) {
    width = 5;  // 0 to 65535
  } else if constexpr (std::is_same_v<T, uint32_t>) {
    width = 10;  // 0 to 4294967295
  } else if constexpr (std::is_same_v<T, uint64_t>) {
    width = 20;  // 0 to 18446744073709551615
  } else {
    static_assert(!sizeof(T), "Unsupported type");
  }

  // Handle negative numbers for signed types
  if constexpr (std::is_signed_v<T>) {
    if (value < 0) {
      ss << '-' << std::setfill('0') << std::setw(width - 1) << -value;
    } else {
      ss << std::setfill('0') << std::setw(width) << value;
    }
  } else {
    ss << std::setfill('0') << std::setw(width) << value;
  }

  return std::move(ss).str();
}

template <typename T>
const std::vector<T>& GetData() {
  static std::vector<T> data;
  if (data.empty()) {
    using TT = std::conditional_t<
        detail::is_string_v<T>, double,
        std::conditional_t<
            std::is_same_v<T, __int128_t>, int64_t,
            std::conditional_t<std::is_same_v<T, __uint128_t>, uint64_t, T>>>;
    using distribution = std::conditional_t<std::is_integral_v<TT>,
                                            std::uniform_int_distribution<TT>,
                                            std::uniform_real_distribution<TT>>;
    constexpr size_t kSeed = 42;
    std::mt19937 gen(kSeed);
    // pcg32_fast gen(kSeed);
    distribution dist(std::numeric_limits<TT>::min(),
                      std::numeric_limits<TT>::max());
    constexpr size_t kNumValues = 1'000'000;
    data.reserve(kNumValues);
    for (size_t i = 0; i < kNumValues; ++i) {
      if constexpr (detail::is_string_v<T>) {
        data.emplace_back(to_string(dist(gen)));
      } else if constexpr (std::is_same_v<T, __int128_t> ||
                           std::is_same_v<T, __uint128_t>) {
        data.push_back((static_cast<T>(dist(gen)) << 64) |
                       static_cast<T>(dist(gen)));
      } else {
        data.emplace_back(dist(gen));
      }
    }
  }
  return data;
}

template <typename T>
const std::vector<__uint128_t>& GetHashes() {
  static std::vector<__uint128_t> hashes;
  if (hashes.empty()) {
    const auto& data = GetData<T>();
    hashes.reserve(data.size());
    for (const auto& value : data) {
      hashes.push_back(detail::Hash(value));
    }
  }
  return hashes;
}
