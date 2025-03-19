// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

// This implementation is based on the Apache DataSketches v4.1.0 implementation
// of the sketch. We made the following significant changes:
// - Adapted the code to the common interface used in this repo by copying over
// the relevant functions.

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <random>

#include "MurmurHash3.h"

namespace datasketches {

static const uint64_t DEFAULT_SEED = 9001;

template <typename T, typename W = uint64_t,
          typename Allocator = std::allocator<W>>
class CountMinSketch {
 public:
  CountMinSketch(uint8_t num_hashes = 5, uint32_t num_buckets = 2048,
                 uint64_t seed = DEFAULT_SEED)
      : _num_hashes(num_hashes),
        _num_buckets(num_buckets),
        _sketch_array(
            (num_hashes * num_buckets < 1 << 30) ? num_hashes * num_buckets : 0,
            0, _allocator),
        _seed(seed),
        _total_weight(0) {
    if (num_buckets < 3)
      throw std::invalid_argument(
          "Using fewer than 3 buckets incurs relative error greater than 1.");

    // This check is to ensure later compatibility with a Java implementation
    // whose maximum size can only be 2^31-1.  We check only against 2^30 for
    // simplicity.
    if (num_buckets * num_hashes >= 1 << 30) {
      throw std::invalid_argument(
          "These parameters generate a sketch that exceeds 2^30 elements."
          "Try reducing either the number of buckets or the number of hash "
          "functions.");
    }

    std::default_random_engine rng(_seed);
    std::uniform_int_distribution<uint64_t> extra_hash_seeds(
        0, std::numeric_limits<uint64_t>::max());
    hash_seeds.reserve(num_hashes);

    for (uint64_t i = 0; i < num_hashes; ++i) {
      hash_seeds.push_back(
          extra_hash_seeds(rng) +
          _seed);  // Adds the global seed to all hash functions.
    }
  };

  /// Insert a value into the sketch.
  void Insert(const T& value) noexcept { update(&value, sizeof(value), 1); }

 private:
  Allocator _allocator;
  uint8_t _num_hashes;
  uint32_t _num_buckets;
  std::vector<W, Allocator> _sketch_array;  // the array stored by the sketch
  uint64_t _seed;
  W _total_weight;
  std::vector<uint64_t> hash_seeds;

  std::vector<uint64_t> get_hashes(const void* item, size_t size) const {
    /*
     * Returns the hash locations for the input item using the original hashing
     * scheme from [1].
     * Generate _num_hashes separate hashes from calls to murmurmhash.
     * This could be optimized by keeping both of the 64bit parts of the hash
     * function, rather than generating a new one for every level.
     *
     *
     * Postscript.
     * Note that a tradeoff can be achieved over the update time and space
     * complexity of the sketch by using a combinatorial hashing scheme from
     * https://github.com/Claudenw/BloomFilter/wiki/Bloom-Filters----An-overview
     * https://www.eecs.harvard.edu/~michaelm/postscripts/tr-02-05.pdf
     */
    uint64_t bucket_index;
    std::vector<uint64_t> sketch_update_locations;
    sketch_update_locations.reserve(_num_hashes);

    uint64_t hash_seed_index = 0;
    for (const auto& it : hash_seeds) {
      auto hashes = MurmurHash3_x64_128(item, size, it);
      uint64_t hash = hashes;
      bucket_index = hash % _num_buckets;
      sketch_update_locations.push_back((hash_seed_index * _num_buckets) +
                                        bucket_index);
      hash_seed_index += 1;
    }
    return sketch_update_locations;
  }

  void update(const void* item, size_t size, W weight) {
    /*
     * Gets the item's hash locations and then increments the sketch in those
     * locations by the weight.
     */
    _total_weight += weight >= 0 ? weight : -weight;
    std::vector<uint64_t> hash_locations = get_hashes(item, size);
    for (const auto h : hash_locations) {
      _sketch_array[h] += weight;
    }
  }
};

}  // namespace datasketches
