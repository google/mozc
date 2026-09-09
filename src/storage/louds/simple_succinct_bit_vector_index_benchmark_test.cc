// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/algorithm/container.h"
#include "storage/louds/simple_succinct_bit_vector_index.h"
#include "testing/benchmark.h"

namespace mozc {
namespace storage {
namespace louds {
namespace {

// A simple Linear Congruential Generator (LCG) for deterministic pseudo random
// number generation for benchmarking. Not intended for cryptographic use.
class Lcg {
 public:
  explicit Lcg(uint32_t seed) : state_(seed) {}

  uint32_t Next() {
    state_ = state_ * 1664525u + 1013904223u;
    return state_;
  }

 private:
  uint32_t state_;
};

std::vector<uint8_t> GenerateRandomData(size_t size) {
  std::vector<uint8_t> data(size);
  Lcg lcg(20100510);
  absl::c_generate(data,
                   [&] { return static_cast<uint8_t>(lcg.Next() >> 24); });
  return data;
}

// Generates pseudo random queries in [min, max].
std::vector<int> GenerateQueries(int min, int max, size_t count) {
  std::vector<int> queries(count);
  Lcg lcg(42);
  absl::c_generate(queries, [&] { return min + lcg.Next() % (max - min + 1); });
  return queries;
}

constexpr size_t kNumQueries = 1024;

// The same cache size as the one used by BitVectorBasedArray.
constexpr size_t kLbCacheSize = 1024;

void BM_Rank1(benchmark::State &state) {
  const int data_size = static_cast<int>(state.range(0));
  const std::vector<uint8_t> data = GenerateRandomData(data_size);
  SimpleSuccinctBitVectorIndex index;
  index.Init(data.data(), data_size, kLbCacheSize, kLbCacheSize);
  const std::vector<int> queries =
      GenerateQueries(0, data_size * 8, kNumQueries);
  for (auto _ : state) {
    int result = 0;
    for (const int n : queries) {
      result += index.Rank1(n);
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kNumQueries);
}
BENCHMARK(BM_Rank1)->Arg(64 << 10)->Arg(4 << 20);

void BM_Select0(benchmark::State &state) {
  const int data_size = static_cast<int>(state.range(0));
  const std::vector<uint8_t> data = GenerateRandomData(data_size);
  SimpleSuccinctBitVectorIndex index;
  index.Init(data.data(), data_size, kLbCacheSize, kLbCacheSize);
  const std::vector<int> queries =
      GenerateQueries(1, index.GetNum0Bits(), kNumQueries);
  for (auto _ : state) {
    int result = 0;
    for (const int n : queries) {
      result += index.Select0(n);
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kNumQueries);
}
BENCHMARK(BM_Select0)->Arg(64 << 10)->Arg(4 << 20);

void BM_Select1(benchmark::State &state) {
  const int data_size = static_cast<int>(state.range(0));
  const std::vector<uint8_t> data = GenerateRandomData(data_size);
  SimpleSuccinctBitVectorIndex index;
  index.Init(data.data(), data_size, kLbCacheSize, kLbCacheSize);
  const std::vector<int> queries =
      GenerateQueries(1, index.GetNum1Bits(), kNumQueries);
  for (auto _ : state) {
    int result = 0;
    for (const int n : queries) {
      result += index.Select1(n);
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kNumQueries);
}
BENCHMARK(BM_Select1)->Arg(64 << 10)->Arg(4 << 20);

}  // namespace
}  // namespace louds
}  // namespace storage
}  // namespace mozc
