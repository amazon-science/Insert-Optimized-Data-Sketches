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

#pragma once

#include <chrono>
#include <random>
#include <thread>

namespace random_utils {

static std::random_device rd;  // possibly unsafe in MinGW with GCC < 9.2
static thread_local std::mt19937_64 rand(rd());

// thread-safe random bit
static thread_local std::independent_bits_engine<std::mt19937, 1, uint32_t>
    random_bit(static_cast<uint32_t>(
        std::chrono::system_clock::now().time_since_epoch().count() +
        std::hash<std::thread::id>{}(std::this_thread::get_id())));

}  // namespace random_utils
