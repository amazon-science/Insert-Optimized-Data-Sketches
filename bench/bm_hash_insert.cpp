#include <cstddef>

#include "benchmark.hpp"
#include "benchmark/benchmark.h"
#include "cs/cs_final.hpp"
#include "ss/ss_final.hpp"
#include "types.hpp"

template <typename Sketch, typename T>
void BM_HashInsert(benchmark::State& state) {
  const auto& data = GetData<T>();
  const auto& hashes = GetHashes<T>();
  for (auto _ : state) {
    Sketch sketch;
    for (size_t i = 0; i < data.size(); ++i) {
      sketch.Insert(data[i], hashes[i]);
    }
    ::benchmark::DoNotOptimize(sketch);
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

#define BENCHMARK_HASH_INSERT_TYPE(sketch, type) \
  BENCHMARK_TEMPLATE(BM_HashInsert, sketch<type>, type)

#define BENCHMARK_HASH_INSERT_ALL_TYPES(sketch)   \
  BENCHMARK_HASH_INSERT_TYPE(sketch, int16_t);    \
  BENCHMARK_HASH_INSERT_TYPE(sketch, int32_t);    \
  BENCHMARK_HASH_INSERT_TYPE(sketch, int64_t);    \
  BENCHMARK_HASH_INSERT_TYPE(sketch, __int128_t); \
  BENCHMARK_HASH_INSERT_TYPE(sketch, float);      \
  BENCHMARK_HASH_INSERT_TYPE(sketch, double);     \
  BENCHMARK_HASH_INSERT_TYPE(sketch, std::string)

#define BENCHMARK_HASH_INSERT_LARGE_TYPES(sketch) \
  BENCHMARK_HASH_INSERT_TYPE(sketch, __int128_t); \
  BENCHMARK_HASH_INSERT_TYPE(sketch, std::string)

BENCHMARK_HASH_INSERT_LARGE_TYPES(final::SpaceSaving);

BENCHMARK_HASH_INSERT_ALL_TYPES(final::CountSketch);

CUSTOM_BENCHMARK_MAIN(true, true);
