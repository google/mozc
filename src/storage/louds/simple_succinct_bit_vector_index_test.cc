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
#include <string>
#include <utility>

#include "testing/gunit.h"

namespace {

using ::mozc::storage::louds::SimpleSuccinctBitVectorIndex;

using CacheSizeParam = std::pair<size_t, size_t>;

class SimpleSuccinctBitVectorIndexTest
    : public ::testing::TestWithParam<CacheSizeParam> {};

#define INSTANTIATE_TEST_CASE(Generator)                            \
  INSTANTIATE_TEST_SUITE_P(                                         \
      Generator, SimpleSuccinctBitVectorIndexTest,                  \
      ::testing::Values(CacheSizeParam(0, 0), CacheSizeParam(0, 1), \
                        CacheSizeParam(1, 0), CacheSizeParam(1, 1), \
                        CacheSizeParam(2, 2), CacheSizeParam(8, 8), \
                        CacheSizeParam(1024, 1024)));

TEST_P(SimpleSuccinctBitVectorIndexTest, Rank) {
  const CacheSizeParam &param = GetParam();

  static constexpr char kData[] = "\x00\x00\xFF\xFF\x00\x00\xFF\xFF";
  SimpleSuccinctBitVectorIndex bit_vector;

  bit_vector.Init(reinterpret_cast<const uint8_t *>(kData), 8, param.first,
                  param.second);
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
INSTANTIATE_TEST_CASE(GenRankTest);

TEST_P(SimpleSuccinctBitVectorIndexTest, Select) {
  const CacheSizeParam &param = GetParam();

  static constexpr char kData[] = "\x00\x00\xFF\xFF\x00\x00\xFF\xFF";
  SimpleSuccinctBitVectorIndex bit_vector;

  bit_vector.Init(reinterpret_cast<const uint8_t *>(kData), 8, param.first,
                  param.second);
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
INSTANTIATE_TEST_CASE(GenSelectTest);

TEST_P(SimpleSuccinctBitVectorIndexTest, Pattern1) {
  const CacheSizeParam &param = GetParam();

  // Repeat the bit pattern '0b10101010'.
  const std::string data(1024, '\xAA');

  SimpleSuccinctBitVectorIndex bit_vector;
  bit_vector.Init(reinterpret_cast<const uint8_t *>(data.data()), data.length(),
                  param.first, param.second);
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
INSTANTIATE_TEST_CASE(GenPattern1Test);

TEST_P(SimpleSuccinctBitVectorIndexTest, Pattern2) {
  const CacheSizeParam &param = GetParam();

  // Repeat the bit pattern '0b11001100'.
  const std::string data(1024, '\xCC');

  SimpleSuccinctBitVectorIndex bit_vector;
  bit_vector.Init(reinterpret_cast<const uint8_t *>(data.data()), data.length(),
                  param.first, param.second);
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
INSTANTIATE_TEST_CASE(GenPattern2Test);

}  // namespace
