#pragma once

#include <random>
#include <vector>

namespace naive {

template <typename T>
class KarninLangLiberty {
 public:
  KarninLangLiberty(const size_t k = 200, const float c = 2. / 3.)
      : k_(k), c_(c) {
    B.resize(k);
  }

  /// Insert a value into the sketch.
  void Insert(const T& x) noexcept {
    B[0].push_back(x);
    Compress();
  }

  void Compress() {
    for (size_t l = 0; l < B.size(); ++l) {
      if (B[l].size() >= GetCapacity(l)) {
        std::sort(B[l].begin(), B[l].end());

        if (l == B.size() - 1) {
          B.resize(B.size() + 1);
        }

        for (size_t i = CoinToss(); i < B[l].size(); i += 2) {
          B[l + 1].push_back(B[l][i]);
        }

        // clear with forced reallocation
        std::vector<T>().swap(B[l]);
      }
    }
  }

 private:
  size_t k_;
  float c_;
  std::vector<std::vector<T>> B{1};

  std::mt19937 gen{42};
  size_t CoinToss() { return gen() & 1; }

  size_t GetCapacity(const size_t l) const {
    return std::max(2ul, static_cast<size_t>(
                             std::ceil(std::pow(c_, B.size() - 1 - l) * k_)));
  }
};

}  // namespace naive