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
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/bits.h"
#include "base/mmap.h"
#include "converter/connector.h"
#include "testing/benchmark.h"
#include "testing/mozctest.h"

namespace mozc {
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

// The number of queries per benchmark iteration.
constexpr size_t kNumQueries = 1024;

Mmap MapConnectionData() {
  const std::string path = testing::GetSourceFileOrDie(
      {"data_manager", "testing", "connection.data"});
  absl::StatusOr<Mmap> mmap = Mmap::Map(path);
  CHECK_OK(mmap);
  return *std::move(mmap);
}

Connector CreateConnector(absl::string_view connection_data) {
  absl::StatusOr<Connector> connector = Connector::Create(connection_data);
  CHECK_OK(connector);
  return *std::move(connector);
}

// The sizes of the connection matrix. They are stored in the header of the
// connection data as described in connector.cc, but Connector does not expose
// them.
struct MatrixSize {
  uint16_t rsize;
  uint16_t lsize;
};

MatrixSize GetMatrixSize(absl::string_view connection_data) {
  // The connection data starts with an 8-byte header of four uint16_t values,
  // see Metadata in connector.cc:
  //   offset 0: magic, offset 2: resolution, offset 4: rsize, offset 6: lsize.
  constexpr size_t kHeaderSize = 8;
  constexpr size_t kRsizeOffset = 4;
  constexpr size_t kLsizeOffset = 6;
  CHECK_GE(connection_data.size(), kHeaderSize);
  return {LoadUnaligned<uint16_t>(connection_data.data() + kRsizeOffset),
          LoadUnaligned<uint16_t>(connection_data.data() + kLsizeOffset)};
}

void BM_Create(benchmark::State& state) {
  const Mmap mmap = MapConnectionData();
  for (auto _ : state) {
    absl::StatusOr<Connector> connector = Connector::Create(mmap.string_view());
    benchmark::DoNotOptimize(connector);
  }
}

// Looks up uniformly random (rid, lid) pairs. The pairs keep changing across
// iterations, so almost every query misses the internal cache of Connector and
// measures the lookup in the compressed row data.
void BM_GetTransitionCost_Random(benchmark::State& state) {
  const Mmap mmap = MapConnectionData();
  const Connector connector = CreateConnector(mmap.string_view());
  const MatrixSize size = GetMatrixSize(mmap.string_view());
  Lcg lcg(42);
  for (auto _ : state) {
    int result = 0;
    for (size_t i = 0; i < kNumQueries; ++i) {
      const uint16_t rid = lcg.NextBelow(size.rsize);
      const uint16_t lid = lcg.NextBelow(size.lsize);
      result += connector.GetTransitionCost(rid, lid);
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kNumQueries);
}

// Looks up every rid for one lid per iteration, moving to the next lid in the
// next iteration. This is the access pattern of CachingConnector, which fills
// its cache for a right node from all left nodes in the lattice. Every query
// misses the internal cache of Connector.
void BM_GetTransitionCost_Column(benchmark::State& state) {
  const Mmap mmap = MapConnectionData();
  const Connector connector = CreateConnector(mmap.string_view());
  const MatrixSize size = GetMatrixSize(mmap.string_view());
  uint16_t lid = 0;
  for (auto _ : state) {
    int result = 0;
    for (uint16_t rid = 0; rid < size.rsize; ++rid) {
      result += connector.GetTransitionCost(rid, lid);
    }
    benchmark::DoNotOptimize(result);
    if (++lid == size.lsize) {
      lid = 0;
    }
  }
  state.SetItemsProcessed(state.iterations() * size.rsize);
}

// Repeats the same queries whose cache buckets do not collide, so that every
// query hits the internal cache of Connector after the first iteration.
void BM_GetTransitionCost_CacheHit(benchmark::State& state) {
  const Mmap mmap = MapConnectionData();
  const Connector connector = CreateConnector(mmap.string_view());
  const MatrixSize size = GetMatrixSize(mmap.string_view());
  CHECK_GE(size.lsize, kNumQueries);
  for (auto _ : state) {
    int result = 0;
    for (size_t i = 0; i < kNumQueries; ++i) {
      result += connector.GetTransitionCost(0, static_cast<uint16_t>(i));
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kNumQueries);
}

BENCHMARK(BM_Create);
BENCHMARK(BM_GetTransitionCost_Random);
BENCHMARK(BM_GetTransitionCost_Column);
BENCHMARK(BM_GetTransitionCost_CacheHit);

}  // namespace
}  // namespace mozc
