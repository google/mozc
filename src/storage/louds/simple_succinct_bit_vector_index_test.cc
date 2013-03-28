// Copyright 2010-2013, Google Inc.
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

#include "testing/base/public/gunit.h"

namespace {

using ::mozc::storage::louds::SimpleSuccinctBitVectorIndex;

class SimpleSuccinctBitVectorIndexTest : public ::testing::Test {
};

TEST_F(SimpleSuccinctBitVectorIndexTest, Rank) {
  static const char kData[] = "\x00\x00\xFF\xFF\x00\x00\xFF\xFF";
  SimpleSuccinctBitVectorIndex bit_vector;

  bit_vector.Init(reinterpret_cast<const uint8 *>(kData), 8);
  EXPECT_EQ(0, bit_vector.Rank0(0));
  EXPECT_EQ(0, bit_vector.Rank1(0));

  for (int i = 1; i <= 16; ++i) {
    EXPECT_EQ(i, bit_vector.Rank0(i)) << i;
    EXPECT_EQ(0, bit_vector.Rank1(i)) << i;
  }

  for (int i = 17; i <= 32; ++i) {
    EXPECT_EQ(16, bit_vector.Rank0(i)) << i;
    EXPECT_EQ(i - 16, bit_vector.Rank1(i)) << i;
  }

  for (int i = 33; i <= 48; ++i) {
    EXPECT_EQ(i - 16, bit_vector.Rank0(i)) << i;
    EXPECT_EQ(16, bit_vector.Rank1(i)) << i;
  }

  for (int i = 49; i <= 64; ++i) {
    EXPECT_EQ(32, bit_vector.Rank0(i)) << i;
    EXPECT_EQ(i - 32, bit_vector.Rank1(i)) << i;
  }
}

TEST_F(SimpleSuccinctBitVectorIndexTest, Select) {
  static const char kData[] = "\x00\x00\xFF\xFF\x00\x00\xFF\xFF";
  SimpleSuccinctBitVectorIndex bit_vector;

  bit_vector.Init(reinterpret_cast<const uint8 *>(kData), 8);
  for (int i = 1; i <= 16; ++i) {
    EXPECT_EQ(i - 1, bit_vector.Select0(i)) << i;
  }
  for (int i = 17; i <= 32; ++i) {
    EXPECT_EQ(i + 15, bit_vector.Select0(i)) << i;
  }
  for (int i = 1; i <= 16; ++i) {
    EXPECT_EQ(i + 15, bit_vector.Select1(i)) << i;
  }
  for (int i = 17; i <= 32; ++i) {
    EXPECT_EQ(i + 31, bit_vector.Select1(i)) << i;
  }
}

TEST_F(SimpleSuccinctBitVectorIndexTest, Pattern1) {
  // Repeat the bit pattern '0b10101010'.
  const string data(1024, '\xAA');

  SimpleSuccinctBitVectorIndex bit_vector;
  bit_vector.Init(reinterpret_cast<const uint8 *>(data.data()), data.length());
  for (int i = 0; i < 1024; ++i) {
    EXPECT_EQ(i * 4, bit_vector.Rank0(i * 8)) << i;
    EXPECT_EQ(i * 4 + 1, bit_vector.Rank0(i * 8 + 1)) << i;
    EXPECT_EQ(i * 4 + 1, bit_vector.Rank0(i * 8 + 2)) << i;
    EXPECT_EQ(i * 4 + 2, bit_vector.Rank0(i * 8 + 3)) << i;
    EXPECT_EQ(i * 4 + 2, bit_vector.Rank0(i * 8 + 4)) << i;
    EXPECT_EQ(i * 4 + 3, bit_vector.Rank0(i * 8 + 5)) << i;
    EXPECT_EQ(i * 4 + 3, bit_vector.Rank0(i * 8 + 6)) << i;
    EXPECT_EQ(i * 4 + 4, bit_vector.Rank0(i * 8 + 7)) << i;

    EXPECT_EQ(i * 4, bit_vector.Rank1(i * 8)) << i;
    EXPECT_EQ(i * 4, bit_vector.Rank1(i * 8 + 1)) << i;
    EXPECT_EQ(i * 4 + 1, bit_vector.Rank1(i * 8 + 2)) << i;
    EXPECT_EQ(i * 4 + 1, bit_vector.Rank1(i * 8 + 3)) << i;
    EXPECT_EQ(i * 4 + 2, bit_vector.Rank1(i * 8 + 4)) << i;
    EXPECT_EQ(i * 4 + 2, bit_vector.Rank1(i * 8 + 5)) << i;
    EXPECT_EQ(i * 4 + 3, bit_vector.Rank1(i * 8 + 6)) << i;
    EXPECT_EQ(i * 4 + 3, bit_vector.Rank1(i * 8 + 7)) << i;
  }

  for (int i = 0; i < 1024 * 4; ++i) {
    EXPECT_EQ(i * 2, bit_vector.Select0(i + 1)) << i;
    EXPECT_EQ(i * 2 + 1, bit_vector.Select1(i + 1)) << i;
  }
}

TEST_F(SimpleSuccinctBitVectorIndexTest, Pattern2) {
  // Repeat the bit pattern '0b11001100'.
  const string data(1024, '\xCC');

  SimpleSuccinctBitVectorIndex bit_vector;
  bit_vector.Init(reinterpret_cast<const uint8 *>(data.data()), data.length());
  for (int i = 0; i < 1024; ++i) {
    EXPECT_EQ(i * 4, bit_vector.Rank0(i * 8)) << i;
    EXPECT_EQ(i * 4 + 1, bit_vector.Rank0(i * 8 + 1)) << i;
    EXPECT_EQ(i * 4 + 2, bit_vector.Rank0(i * 8 + 2)) << i;
    EXPECT_EQ(i * 4 + 2, bit_vector.Rank0(i * 8 + 3)) << i;
    EXPECT_EQ(i * 4 + 2, bit_vector.Rank0(i * 8 + 4)) << i;
    EXPECT_EQ(i * 4 + 3, bit_vector.Rank0(i * 8 + 5)) << i;
    EXPECT_EQ(i * 4 + 4, bit_vector.Rank0(i * 8 + 6)) << i;
    EXPECT_EQ(i * 4 + 4, bit_vector.Rank0(i * 8 + 7)) << i;

    EXPECT_EQ(i * 4, bit_vector.Rank1(i * 8)) << i;
    EXPECT_EQ(i * 4, bit_vector.Rank1(i * 8 + 1)) << i;
    EXPECT_EQ(i * 4 , bit_vector.Rank1(i * 8 + 2)) << i;
    EXPECT_EQ(i * 4 + 1, bit_vector.Rank1(i * 8 + 3)) << i;
    EXPECT_EQ(i * 4 + 2, bit_vector.Rank1(i * 8 + 4)) << i;
    EXPECT_EQ(i * 4 + 2, bit_vector.Rank1(i * 8 + 5)) << i;
    EXPECT_EQ(i * 4 + 2, bit_vector.Rank1(i * 8 + 6)) << i;
    EXPECT_EQ(i * 4 + 3, bit_vector.Rank1(i * 8 + 7)) << i;
  }

  for (int i = 0; i < 1024 * 4; ++i) {
    EXPECT_EQ((i * 2) - (i % 2), bit_vector.Select0(i + 1)) << i;
    EXPECT_EQ((i * 2 + 1) + ((i + 1) % 2) , bit_vector.Select1(i + 1)) << i;
  }
}

}  // namespace
