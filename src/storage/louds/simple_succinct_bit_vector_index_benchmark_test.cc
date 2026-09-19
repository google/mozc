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

  // Returns a value in [0, range) using the high bits, without a division.
  uint32_t NextBelow(uint32_t range) {
    return static_cast<uint32_t>((static_cast<uint64_t>(Next()) * range) >> 32);
  }

 private:
  uint32_t state_;
};

// Shape of the generated bit vector. |density| is the overall ratio of 1-bits.
// Each 32-byte chunk draws its own density uniformly from
//    [density - spread, density + spread],
// which models the clustering of real LOUDS data: in the OSS system dictionary
// the number of 1-bits per chunk has a standard deviation of 32-60, whereas
// independent random bits give 8.
struct DataShape {
  double density;
  double spread;
};

// Independent random bits. The friendliest case for the lower bound caches.
constexpr DataShape kUniform = {0.5, 0.0};
// Emulate LOUDS bit vector of a trie: exactly half of the bits are 1.
constexpr DataShape kLouds = {0.5, 0.25};
// Emulate terminal bit vector of a trie, queried by Select1.
constexpr DataShape kTerminal = {0.3, 0.25};
// Emulate index of BitVectorBasedArray (token array), queried by Select0.
constexpr DataShape kTokenIndex = {0.8, 0.15};

std::vector<uint8_t> GenerateData(size_t size, DataShape shape) {
  constexpr size_t kChunkSize = 32;
  std::vector<uint8_t> data(size);
  Lcg lcg(20100510);
  uint32_t threshold = 0;
  for (size_t i = 0; i < size; ++i) {
    if (i % kChunkSize == 0) {
      const double u = lcg.Next() / 4294967296.0;  // [0, 1)
      const double p = shape.density + shape.spread * (2.0 * u - 1.0);
      threshold = static_cast<uint32_t>(p * 65536.0);
    }
    uint8_t byte = 0;
    for (int bit = 0; bit < 8; ++bit) {
      byte |= static_cast<uint8_t>((lcg.Next() >> 16) < threshold) << bit;
    }
    data[i] = byte;
  }
  return data;
}

// The number of queries per benchmark iteration.
constexpr size_t kNumQueries = 1024;

// The same cache size as the ones used by the system dictionary.
constexpr size_t kLbCacheSize = 1024;

void BM_Rank1(benchmark::State& state, DataShape shape) {
  const int data_size = static_cast<int>(state.range(0));
  const std::vector<uint8_t> data = GenerateData(data_size, shape);
  SimpleSuccinctBitVectorIndex index;
  index.Init(data.data(), data_size, kLbCacheSize, kLbCacheSize);
  const uint32_t range = static_cast<uint32_t>(data_size) * 8 + 1;
  Lcg lcg(42);
  for (auto _ : state) {
    int result = 0;
    for (size_t i = 0; i < kNumQueries; ++i) {
      result += index.Rank1(static_cast<int>(lcg.NextBelow(range)));
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kNumQueries);
}

void BM_Select0(benchmark::State& state, DataShape shape) {
  const int data_size = static_cast<int>(state.range(0));
  const std::vector<uint8_t> data = GenerateData(data_size, shape);
  SimpleSuccinctBitVectorIndex index;
  index.Init(data.data(), data_size, kLbCacheSize, kLbCacheSize);
  const uint32_t range = static_cast<uint32_t>(index.GetNum0Bits());
  Lcg lcg(42);
  for (auto _ : state) {
    int result = 0;
    for (size_t i = 0; i < kNumQueries; ++i) {
      result += index.Select0(1 + static_cast<int>(lcg.NextBelow(range)));
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kNumQueries);
}

void BM_Select1(benchmark::State& state, DataShape shape) {
  const int data_size = static_cast<int>(state.range(0));
  const std::vector<uint8_t> data = GenerateData(data_size, shape);
  SimpleSuccinctBitVectorIndex index;
  index.Init(data.data(), data_size, kLbCacheSize, kLbCacheSize);
  const uint32_t range = static_cast<uint32_t>(index.GetNum1Bits());
  Lcg lcg(42);
  for (auto _ : state) {
    int result = 0;
    for (size_t i = 0; i < kNumQueries; ++i) {
      result += index.Select1(1 + static_cast<int>(lcg.NextBelow(range)));
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kNumQueries);
}

// Data sizes: 64 KiB fits in the L1 data cache together with its index, 512
// KiB is the scale of the bit vectors in the OSS system dictionary (250-700
// KiB), 2 MiB leaves headroom for larger dictionaries, and 4 MiB does not fit
// in the L2 cache of many CPUs.
#define MOZC_LOUDS_BENCHMARK_SIZES \
  ->Arg(64 << 10)->Arg(512 << 10)->Arg(2 << 20)->Arg(4 << 20)

BENCHMARK_CAPTURE(BM_Rank1, Uniform, kUniform) MOZC_LOUDS_BENCHMARK_SIZES;
BENCHMARK_CAPTURE(BM_Rank1, Louds, kLouds) MOZC_LOUDS_BENCHMARK_SIZES;

BENCHMARK_CAPTURE(BM_Select0, Uniform, kUniform) MOZC_LOUDS_BENCHMARK_SIZES;
BENCHMARK_CAPTURE(BM_Select0, Louds, kLouds) MOZC_LOUDS_BENCHMARK_SIZES;
BENCHMARK_CAPTURE(BM_Select0, TokenIndex, kTokenIndex)
MOZC_LOUDS_BENCHMARK_SIZES;

BENCHMARK_CAPTURE(BM_Select1, Uniform, kUniform) MOZC_LOUDS_BENCHMARK_SIZES;
BENCHMARK_CAPTURE(BM_Select1, Louds, kLouds) MOZC_LOUDS_BENCHMARK_SIZES;
BENCHMARK_CAPTURE(BM_Select1, Terminal, kTerminal) MOZC_LOUDS_BENCHMARK_SIZES;

#undef MOZC_LOUDS_BENCHMARK_SIZES

}  // namespace
}  // namespace louds
}  // namespace storage
}  // namespace mozc
