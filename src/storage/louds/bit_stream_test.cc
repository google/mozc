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

#include "storage/louds/bit_stream.h"

#include <cstdint>
#include <iterator>
#include <string>

#include "testing/gunit.h"
#include "absl/strings/str_cat.h"

namespace mozc::storage::louds::internal {
namespace {

class BitStreamTest : public ::testing::Test {};

TEST_F(BitStreamTest, Pattern1) {
  BitStream bit_stream;
  for (int i = 0; i < 128; ++i) {
    bit_stream.PushBit(0);
    bit_stream.PushBit(1);
    EXPECT_EQ(bit_stream.num_bits(), 2 * (i + 1));
    EXPECT_EQ(bit_stream.ByteSize(), i / 4 + 1);
  }

  EXPECT_EQ(bit_stream.image(), std::string(32, '\xAA'));
}

TEST_F(BitStreamTest, Pattern2) {
  BitStream bit_stream;
  for (int i = 0; i < 128; ++i) {
    bit_stream.PushBit(0);
    bit_stream.PushBit(0);
    bit_stream.PushBit(1);
    bit_stream.PushBit(1);
    EXPECT_EQ(bit_stream.num_bits(), 4 * (i + 1));
    EXPECT_EQ(bit_stream.ByteSize(), i / 2 + 1);
  }

  EXPECT_EQ(bit_stream.image(), std::string(64, '\xCC'));
}

TEST_F(BitStreamTest, FillPadding32) {
  BitStream bit_stream;

  bit_stream.FillPadding32();
  EXPECT_EQ("", bit_stream.image());

  bit_stream.PushBit(1);
  bit_stream.FillPadding32();
  EXPECT_EQ(bit_stream.image(), std::string("\x01\x00\x00\x00", 4));
}

TEST_F(BitStreamTest, PushInt32) {
  std::string image;
  std::string expected = std::string(4, '\x00');
  PushInt32(0, image);
  EXPECT_EQ(image, expected);

  absl::StrAppend(&expected, std::string(4, '\xff'));

  PushInt32(0xffffffff, image);
  EXPECT_EQ(image, expected);

  absl::StrAppend(&expected, "\x78\x56\x34\x12");
  PushInt32(0x12345678, image);
  EXPECT_EQ(image, expected);
}

TEST_F(BitStreamTest, ReadInt32) {
  constexpr uint8_t kTestData[][sizeof(uint32_t)] = {
      {0, 0, 0, 0},
      {0xff, 0xff, 0xff, 0xff},
      {0x78, 0x56, 0x34, 0x12},
  };
  constexpr uint32_t kExpected[] = {0, 0xffffffff, 0x12345678};
  for (int i = 0; i < std::size(kTestData); ++i) {
    EXPECT_EQ(ReadInt32(kTestData[i]), kExpected[i]);
  }
}

}  // namespace
}  // namespace mozc::storage::louds::internal
