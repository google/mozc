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

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/strings/string_view.h"
#include "storage/louds/bit_vector_based_array.h"
#include "storage/louds/bit_vector_based_array_builder.h"
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

  // Returns a value in (0, 1).
  double NextUnit() { return (Next() + 0.5) / 4294967296.0; }

 private:
  uint32_t state_;
};

// A synthetic array that follows the token array of the OSS system dictionary,
// the main user of BitVectorBasedArray. The token array stores the encoded
// tokens of each key as one element with base_length 4 and step_length 1, so an
// element of n steps is 4 + n bytes long and is represented by n 1-bits in the
// index. The OSS token array as of Mozc version 3.34.6239 has these properties,
// which this class reproduces within a few percent:
//
//  - 889,346 elements with 4.4 steps on average. 15% of the elements have no
//    step, 40% have one, and 9% have 12 or more. The latter have a long tail
//    with a median of 18 steps, a 99th percentile of 162, and a maximum of
//    1734.
//  - The elements are strongly clustered. The number of elements per 32-byte
//    chunk of the index has a mean of 48 but a standard deviation of 35,
//    whereas independent elements would give 10. Adjacent elements have a
//    correlation of 0.29 in their numbers of steps.
//
// The clustering matters because the lookup first binary-searches the chunks
// and then scans within a chunk, so the spread of the elements over the chunks
// shapes the cost.
class SyntheticTokenArray {
 public:
  SyntheticTokenArray() = delete;

  static constexpr size_t kNumElements = 889346;

  // Returns the image of the array, which is generated on the first call and
  // shared by all the benchmarks.
  static const std::vector<uint8_t>& GetOrCreate() {
    static const absl::NoDestructor<std::vector<uint8_t>> image(Build());
    return *image;
  }

 private:
  static constexpr size_t kBaseLength = 4;
  static constexpr size_t kStepLength = 1;

  // Generates the numbers of steps of the elements with a two-state Markov
  // chain of sparse regions (mostly 0 or 1 step) and rich regions (more steps
  // and more long elements), each with its own distribution. An element repeats
  // the previous one with probability 1/4, which gives the correlation between
  // adjacent elements, and elements with 12 or more steps follow a log-normal
  // distribution.
  class StepGenerator {
   public:
    StepGenerator() : lcg_(20100510) {}

    // Returns the number of steps of the next element.
    size_t Next() {
      if (lcg_.NextBelow(1000) >= region_->stay_permille) {
        region_ = region_ == &kSparse ? &kRich : &kSparse;
      }
      if (has_previous_ && lcg_.NextBelow(1000) < kRepeatPermille) {
        return previous_steps_;
      }
      has_previous_ = true;
      previous_steps_ = lcg_.NextBelow(1000) < region_->tail_permille
                            ? NextTailSteps(*region_)
                            : NextHeadSteps(*region_);
      return previous_steps_;
    }

   private:
    // Parameters of one kind of region.
    struct RegionParams {
      // Probability in permille that the next element stays in this region.
      uint32_t stay_permille;
      // Probability in permille of an element with 12 or more steps.
      uint32_t tail_permille;
      // Cumulative distribution in permille of 0, 1, ..., 11 steps.
      uint32_t head_cumulative_permille[12];
      // Parameters of the log-normal distribution of (steps - 11).
      double tail_log_mean;
      double tail_log_stddev;
    };

    // Sparse regions have 333 elements on average, mostly with 0 or 1 step.
    static constexpr RegionParams kSparse = {
        .stay_permille = 997,
        .tail_permille = 15,
        .head_cumulative_permille = {220, 820, 920, 950, 965, 972, 985, 990,
                                     996, 998, 999, 1000},
        .tail_log_mean = 1.7,
        .tail_log_stddev = 1.0,
    };

    // Rich regions have 200 elements on average, with more steps and more
    // long elements.
    static constexpr RegionParams kRich = {
        .stay_permille = 995,
        .tail_permille = 200,
        .head_cumulative_permille = {80, 280, 470, 560, 640, 680, 760, 800, 880,
                                     900, 910, 1000},
        .tail_log_mean = 2.0,
        .tail_log_stddev = 1.45,
    };

    // Probability in permille that an element repeats the previous one.
    static constexpr uint32_t kRepeatPermille = 250;

    size_t NextHeadSteps(const RegionParams& region) {
      const uint32_t r = lcg_.NextBelow(1000);
      size_t steps = 0;
      while (r >= region.head_cumulative_permille[steps]) {
        ++steps;
      }
      return steps;
    }

    size_t NextTailSteps(const RegionParams& region) {
      // The sum of four uniform values approximates a normal distribution with
      // a standard deviation of sqrt(4 / 12).
      const double sum =
          lcg_.NextUnit() + lcg_.NextUnit() + lcg_.NextUnit() + lcg_.NextUnit();
      const double z = (sum - 2.0) * std::sqrt(3.0);
      const double value =
          std::exp(region.tail_log_mean + region.tail_log_stddev * z);
      return 11 + std::max<size_t>(1, static_cast<size_t>(value));
    }

    Lcg lcg_;
    const RegionParams* region_ = &kSparse;
    bool has_previous_ = false;
    size_t previous_steps_ = 0;
  };

  static std::vector<uint8_t> Build() {
    BitVectorBasedArrayBuilder builder;
    builder.SetSize(kBaseLength, kStepLength);
    StepGenerator generator;
    for (size_t i = 0; i < kNumElements; ++i) {
      builder.Add(
          std::string(kBaseLength + kStepLength * generator.Next(), 'x'));
    }
    builder.Build();
    const absl::string_view image = builder.image();
    return std::vector<uint8_t>(image.begin(), image.end());
  }
};

// The number of queries per benchmark iteration.
constexpr size_t kNumQueries = 1024;

void BM_Open(benchmark::State& state) {
  const std::vector<uint8_t>& image = SyntheticTokenArray::GetOrCreate();
  for (auto _ : state) {
    BitVectorBasedArray array;
    array.Open(image.data());
    benchmark::DoNotOptimize(array);
  }
}

// Looks up uniformly random indices that keep changing across iterations.
// This is the access pattern of the system dictionary lookup, where the key ids
// found in the key trie have no locality, so the branches of the lookup are
// unpredictable and the index is mostly accessed out of the L1 cache.
void BM_Get_Random(benchmark::State& state) {
  const std::vector<uint8_t>& image = SyntheticTokenArray::GetOrCreate();
  BitVectorBasedArray array;
  array.Open(image.data());
  constexpr uint32_t kNumElements = SyntheticTokenArray::kNumElements;
  Lcg lcg(42);
  for (auto _ : state) {
    size_t result = 0;
    for (size_t i = 0; i < kNumQueries; ++i) {
      size_t length = 0;
      const char* element = array.Get(lcg.NextBelow(kNumElements), &length);
      benchmark::DoNotOptimize(element);
      result += length;
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kNumQueries);
}

// Looks up consecutive indices, wrapping around at the end. This is not an
// access pattern of the system dictionary. It keeps the index and the bit
// vector in the CPU cache and the branches of the lookup predictable, so it
// measures the instruction cost of a lookup rather than its memory cost.
void BM_Get_Sequential(benchmark::State& state) {
  const std::vector<uint8_t>& image = SyntheticTokenArray::GetOrCreate();
  BitVectorBasedArray array;
  array.Open(image.data());
  constexpr size_t kNumElements = SyntheticTokenArray::kNumElements;
  size_t index = 0;
  for (auto _ : state) {
    size_t result = 0;
    for (size_t i = 0; i < kNumQueries; ++i) {
      size_t length = 0;
      const char* element = array.Get(index, &length);
      benchmark::DoNotOptimize(element);
      result += length;
      if (++index == kNumElements) {
        index = 0;
      }
    }
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * kNumQueries);
}

BENCHMARK(BM_Open);
BENCHMARK(BM_Get_Random);
BENCHMARK(BM_Get_Sequential);

}  // namespace
}  // namespace louds
}  // namespace storage
}  // namespace mozc
