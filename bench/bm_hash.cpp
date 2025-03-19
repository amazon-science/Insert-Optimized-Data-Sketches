#include "benchmark.hpp"
#include "benchmark/benchmark.h"
#include "data.hpp"
#include "hash.hpp"
#include "types.hpp"

template <typename HashFn, typename T>
void BM_Hash(benchmark::State& state) {
  const auto& data = GetData<T>();
  HashFn hash_fn;
  for (auto _ : state) {
    for (const auto& value : data) {
      ::benchmark::DoNotOptimize(hash_fn(value));
    }
    ::benchmark::ClobberMemory();
  }

  int64_t num_items = state.iterations() * data.size();
  state.SetItemsProcessed(num_items);
  int64_t item_size = sizeof(T);
  if constexpr (detail::is_string_v<T>) {
    item_size = data[0].size() * sizeof(char);
  }
  state.SetBytesProcessed(num_items * item_size);
  state.counters["item_size"] = item_size;
}

#define BENCHMARK_HASH_ALL_TYPES(hashfn)           \
  BENCHMARK_TEMPLATE(BM_Hash, hashfn, int16_t);    \
  BENCHMARK_TEMPLATE(BM_Hash, hashfn, int32_t);    \
  BENCHMARK_TEMPLATE(BM_Hash, hashfn, int64_t);    \
  BENCHMARK_TEMPLATE(BM_Hash, hashfn, __int128_t); \
  BENCHMARK_TEMPLATE(BM_Hash, hashfn, float);      \
  BENCHMARK_TEMPLATE(BM_Hash, hashfn, double)

struct HashFn {
  template <typename T>
  constexpr auto operator()(const T& v) const {
    return detail::Hash(v);
  }
};
BENCHMARK_HASH_ALL_TYPES(HashFn);

struct HashNoUnrollFn {
  template <typename T>
  constexpr auto operator()(const T& v) const {
    return detail::HashNoUnroll(v);
  }
};
BENCHMARK_HASH_ALL_TYPES(HashNoUnrollFn);

CUSTOM_BENCHMARK_MAIN(true, false);
