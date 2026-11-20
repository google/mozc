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

#include "storage/louds/simple_succinct_bit_vector_index.h"

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <tuple>

#include "absl/strings/str_cat.h"
#include "testing/gunit.h"

namespace {

using ::mozc::storage::louds::SimpleSuccinctBitVectorIndex;

// The sizes of the lower bound caches for Select0 and Select1.
struct CacheSizes {
  friend void PrintTo(const CacheSizes &cache_sizes, std::ostream *os) {
    *os << "{lb0: " << cache_sizes.lb0 << ", lb1: " << cache_sizes.lb1 << "}";
  }

  size_t lb0;
  size_t lb1;
};

// The chunk size in bytes and the sizes of the lower bound caches.
using TestParam = std::tuple<int, CacheSizes>;

class SimpleSuccinctBitVectorIndexTest
    : public ::testing::TestWithParam<TestParam> {};

INSTANTIATE_TEST_SUITE_P(
    ChunkAndCacheSizes, SimpleSuccinctBitVectorIndexTest,
    ::testing::Combine(::testing::Values(8, 16, 32, 64),
                       ::testing::Values(CacheSizes{0, 0}, CacheSizes{0, 1},
                                         CacheSizes{1, 0}, CacheSizes{1, 1},
                                         CacheSizes{2, 2}, CacheSizes{8, 8},
                                         CacheSizes{1024, 1024})),
    [](const ::testing::TestParamInfo<TestParam> &info) {
      const CacheSizes &cache_sizes = std::get<1>(info.param);
      return absl::StrCat("Chunk", std::get<0>(info.param), "_Lb0Cache",
                          cache_sizes.lb0, "_Lb1Cache", cache_sizes.lb1);
    });

TEST_P(SimpleSuccinctBitVectorIndexTest, Rank) {
  const auto &[chunk_size, cache_sizes] = GetParam();

  static constexpr char kData[] = "\x00\x00\xFF\xFF\x00\x00\xFF\xFF";
  SimpleSuccinctBitVectorIndex bit_vector(chunk_size);

  bit_vector.Init(reinterpret_cast<const uint8_t *>(kData), 8,
                  cache_sizes.lb0, cache_sizes.lb1);
  EXPECT_EQ(bit_vector.GetNum0Bits(), 32);
  EXPECT_EQ(bit_vector.GetNum1Bits(), 32);
  EXPECT_EQ(bit_vector.Rank0(0), 0);
  EXPECT_EQ(bit_vector.Rank1(0), 0);

  for (int i = 1; i <= 16; ++i) {
    EXPECT_EQ(bit_vector.Rank0(i), i) << i;
    EXPECT_EQ(bit_vector.Rank1(i), 0) << i;
  }

  for (int i = 17; i <= 32; ++i) {
    EXPECT_EQ(bit_vector.Rank0(i), 16) << i;
    EXPECT_EQ(bit_vector.Rank1(i), i - 16) << i;
  }

  for (int i = 33; i <= 48; ++i) {
    EXPECT_EQ(bit_vector.Rank0(i), i - 16) << i;
    EXPECT_EQ(bit_vector.Rank1(i), 16) << i;
  }

  for (int i = 49; i <= 64; ++i) {
    EXPECT_EQ(bit_vector.Rank0(i), 32) << i;
    EXPECT_EQ(bit_vector.Rank1(i), i - 32) << i;
  }
}

TEST_P(SimpleSuccinctBitVectorIndexTest, Select) {
  const auto &[chunk_size, cache_sizes] = GetParam();

  static constexpr char kData[] = "\x00\x00\xFF\xFF\x00\x00\xFF\xFF";
  SimpleSuccinctBitVectorIndex bit_vector(chunk_size);

  bit_vector.Init(reinterpret_cast<const uint8_t *>(kData), 8,
                  cache_sizes.lb0, cache_sizes.lb1);
  EXPECT_EQ(bit_vector.GetNum0Bits(), 32);
  EXPECT_EQ(bit_vector.GetNum1Bits(), 32);

  for (int i = 1; i <= 16; ++i) {
    EXPECT_EQ(bit_vector.Select0(i), i - 1) << i;
  }
  for (int i = 17; i <= 32; ++i) {
    EXPECT_EQ(bit_vector.Select0(i), i + 15) << i;
  }
  for (int i = 1; i <= 16; ++i) {
    EXPECT_EQ(bit_vector.Select1(i), i + 15) << i;
  }
  for (int i = 17; i <= 32; ++i) {
    EXPECT_EQ(bit_vector.Select1(i), i + 31) << i;
  }
}

TEST_P(SimpleSuccinctBitVectorIndexTest, Pattern1) {
  const auto &[chunk_size, cache_sizes] = GetParam();

  // Repeat the bit pattern '0b10101010'.
  const std::string data(1024, '\xAA');

  SimpleSuccinctBitVectorIndex bit_vector(chunk_size);
  bit_vector.Init(reinterpret_cast<const uint8_t *>(data.data()), data.length(),
                  cache_sizes.lb0, cache_sizes.lb1);
  EXPECT_EQ(bit_vector.GetNum0Bits(), 4 * 1024);
  EXPECT_EQ(bit_vector.GetNum1Bits(), 4 * 1024);

  for (int i = 0; i < 1024; ++i) {
    EXPECT_EQ(bit_vector.Rank0(i * 8), i * 4) << i;
    EXPECT_EQ(bit_vector.Rank0(i * 8 + 1), i * 4 + 1) << i;
    EXPECT_EQ(bit_vector.Rank0(i * 8 + 2), i * 4 + 1) << i;
    EXPECT_EQ(bit_vector.Rank0(i * 8 + 3), i * 4 + 2) << i;
    EXPECT_EQ(bit_vector.Rank0(i * 8 + 4), i * 4 + 2) << i;
    EXPECT_EQ(bit_vector.Rank0(i * 8 + 5), i * 4 + 3) << i;
    EXPECT_EQ(bit_vector.Rank0(i * 8 + 6), i * 4 + 3) << i;
    EXPECT_EQ(bit_vector.Rank0(i * 8 + 7), i * 4 + 4) << i;

    EXPECT_EQ(bit_vector.Rank1(i * 8), i * 4) << i;
    EXPECT_EQ(bit_vector.Rank1(i * 8 + 1), i * 4) << i;
    EXPECT_EQ(bit_vector.Rank1(i * 8 + 2), i * 4 + 1) << i;
    EXPECT_EQ(bit_vector.Rank1(i * 8 + 3), i * 4 + 1) << i;
    EXPECT_EQ(bit_vector.Rank1(i * 8 + 4), i * 4 + 2) << i;
    EXPECT_EQ(bit_vector.Rank1(i * 8 + 5), i * 4 + 2) << i;
    EXPECT_EQ(bit_vector.Rank1(i * 8 + 6), i * 4 + 3) << i;
    EXPECT_EQ(bit_vector.Rank1(i * 8 + 7), i * 4 + 3) << i;
  }

  for (int i = 0; i < 1024 * 4; ++i) {
    EXPECT_EQ(bit_vector.Select0(i + 1), i * 2) << i;
    EXPECT_EQ(bit_vector.Select1(i + 1), i * 2 + 1) << i;
  }
}

TEST_P(SimpleSuccinctBitVectorIndexTest, Pattern2) {
  const auto &[chunk_size, cache_sizes] = GetParam();

  // Repeat the bit pattern '0b11001100'.
  const std::string data(1024, '\xCC');

  SimpleSuccinctBitVectorIndex bit_vector(chunk_size);
  bit_vector.Init(reinterpret_cast<const uint8_t *>(data.data()), data.length(),
                  cache_sizes.lb0, cache_sizes.lb1);
  EXPECT_EQ(bit_vector.GetNum0Bits(), 4 * 1024);
  EXPECT_EQ(bit_vector.GetNum1Bits(), 4 * 1024);

  for (int i = 0; i < 1024; ++i) {
    EXPECT_EQ(bit_vector.Rank0(i * 8), i * 4) << i;
    EXPECT_EQ(bit_vector.Rank0(i * 8 + 1), i * 4 + 1) << i;
    EXPECT_EQ(bit_vector.Rank0(i * 8 + 2), i * 4 + 2) << i;
    EXPECT_EQ(bit_vector.Rank0(i * 8 + 3), i * 4 + 2) << i;
    EXPECT_EQ(bit_vector.Rank0(i * 8 + 4), i * 4 + 2) << i;
    EXPECT_EQ(bit_vector.Rank0(i * 8 + 5), i * 4 + 3) << i;
    EXPECT_EQ(bit_vector.Rank0(i * 8 + 6), i * 4 + 4) << i;
    EXPECT_EQ(bit_vector.Rank0(i * 8 + 7), i * 4 + 4) << i;

    EXPECT_EQ(bit_vector.Rank1(i * 8), i * 4) << i;
    EXPECT_EQ(bit_vector.Rank1(i * 8 + 1), i * 4) << i;
    EXPECT_EQ(bit_vector.Rank1(i * 8 + 2), i * 4) << i;
    EXPECT_EQ(bit_vector.Rank1(i * 8 + 3), i * 4 + 1) << i;
    EXPECT_EQ(bit_vector.Rank1(i * 8 + 4), i * 4 + 2) << i;
    EXPECT_EQ(bit_vector.Rank1(i * 8 + 5), i * 4 + 2) << i;
    EXPECT_EQ(bit_vector.Rank1(i * 8 + 6), i * 4 + 2) << i;
    EXPECT_EQ(bit_vector.Rank1(i * 8 + 7), i * 4 + 3) << i;
  }

  for (int i = 0; i < 1024 * 4; ++i) {
    EXPECT_EQ(bit_vector.Select0(i + 1), (i * 2) - (i % 2)) << i;
    EXPECT_EQ(bit_vector.Select1(i + 1), (i * 2 + 1) + ((i + 1) % 2)) << i;
  }
}

// Compares Rank1, Select0 and Select1 against a naive scan for every position
// and rank. The sizes cover data shorter than a chunk, a partial last chunk,
// and exact multiples of the chunk size; the densities cover empty, sparse,
// dense and full words.
TEST_P(SimpleSuccinctBitVectorIndexTest, NaiveScan) {
  const auto &[chunk_size, cache_sizes] = GetParam();

  constexpr int kSizes[] = {4, 28, 32, 36, 64, 100, 1024, 4100};
  constexpr uint32_t kDensityPercents[] = {0, 3, 50, 97, 100};
  for (const int size : kSizes) {
    for (const uint32_t density : kDensityPercents) {
      SCOPED_TRACE(::testing::Message()
                   << "size=" << size << " density=" << density);
      // A simple LCG for deterministic pseudo random bits.
      uint32_t state = size * 7 + density;
      std::string data(size, '\0');
      for (char &c : data) {
        uint8_t byte = 0;
        for (int bit = 0; bit < 8; ++bit) {
          state = state * 1664525u + 1013904223u;
          byte |= static_cast<uint8_t>((state >> 16) % 100 < density) << bit;
        }
        c = static_cast<char>(byte);
      }

      SimpleSuccinctBitVectorIndex bit_vector(chunk_size);
      bit_vector.Init(reinterpret_cast<const uint8_t *>(data.data()), size,
                      cache_sizes.lb0, cache_sizes.lb1);
      int num_0bits = 0;
      int num_1bits = 0;
      for (int pos = 0; pos < size * 8; ++pos) {
        ASSERT_EQ(bit_vector.Rank1(pos), num_1bits);
        if ((static_cast<uint8_t>(data[pos / 8]) >> (pos % 8)) & 1) {
          ASSERT_EQ(bit_vector.Select1(++num_1bits), pos);
        } else {
          ASSERT_EQ(bit_vector.Select0(++num_0bits), pos);
        }
      }
      EXPECT_EQ(bit_vector.GetNum0Bits(), num_0bits);
      EXPECT_EQ(bit_vector.GetNum1Bits(), num_1bits);
    }
  }
}

}  // namespace
