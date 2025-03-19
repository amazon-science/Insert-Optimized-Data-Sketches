#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "benchmark/benchmark.h"
#include "data.hpp"

inline auto AmendArgs(int argc, char* argv[]) {
  std::vector<char*> new_argv(argv, argv + argc);
  new_argv.push_back(const_cast<char*>("--benchmark_counters_tabular=true"));
  new_argv.push_back(const_cast<char*>("--benchmark_out_format=json"));
  return new_argv;
}

#define CUSTOM_BENCHMARK_MAIN(cache_data, cache_hashes)                 \
  int main(int argc, char** argv) {                                     \
    if (cache_data) {                                                   \
      GetData<int16_t>();                                               \
      GetData<int32_t>();                                               \
      GetData<int64_t>();                                               \
      GetData<__int128_t>();                                            \
      GetData<float>();                                                 \
      GetData<double>();                                                \
      GetData<std::string>();                                           \
    }                                                                   \
                                                                        \
    if (cache_hashes) {                                                 \
      GetHashes<int16_t>();                                             \
      GetHashes<int32_t>();                                             \
      GetHashes<int64_t>();                                             \
      GetHashes<__int128_t>();                                          \
      GetHashes<float>();                                               \
      GetHashes<double>();                                              \
      GetHashes<std::string>();                                         \
    }                                                                   \
                                                                        \
    auto new_argv = AmendArgs(argc, argv);                              \
    argc = static_cast<int>(new_argv.size());                           \
    ::benchmark::Initialize(&argc, new_argv.data());                    \
    if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1; \
    ::benchmark::RunSpecifiedBenchmarks();                              \
    ::benchmark::Shutdown();                                            \
    return 0;                                                           \
  }
