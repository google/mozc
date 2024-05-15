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

#include "base/strings/internal/utf8_internal.h"

#include <utility>

#include "absl/strings/string_view.h"
#include "testing/gunit.h"

namespace mozc::utf8_internal {
namespace {

TEST(Utf8InternalTest, OneCharLen) {
  EXPECT_EQ(OneCharLen(0x00), 1);
  EXPECT_EQ(OneCharLen(0x7F), 1);
  EXPECT_EQ(OneCharLen(0xC2), 2);
  EXPECT_EQ(OneCharLen(0xDF), 2);
  EXPECT_EQ(OneCharLen(0xE0), 3);
  EXPECT_EQ(OneCharLen(0xEF), 3);
  EXPECT_EQ(OneCharLen(0xF0), 4);
  EXPECT_EQ(OneCharLen(0xF4), 4);
}

class EncodeDecodeTest
    : public ::testing::TestWithParam<std::pair<absl::string_view, char32_t>> {
};

TEST_P(EncodeDecodeTest, Decode) {
  const DecodeResult actual =
      Decode(GetParam().first.data(),
             GetParam().first.data() + GetParam().first.size());
  EXPECT_TRUE(actual.ok());
  EXPECT_EQ(actual.code_point(), GetParam().second);
  EXPECT_EQ(actual.bytes_seen(), GetParam().first.size());
}

TEST_P(EncodeDecodeTest, Encode) {
  const EncodeResult actual = Encode(GetParam().second);
  EXPECT_EQ(absl::string_view(actual.data(), actual.size()), GetParam().first);
}

constexpr std::pair<absl::string_view, char32_t> kEncodeDecodeParams[] = {
    {absl::string_view("\0", 1), 0},
    {"a", U'a'},
    {"€", U'€'},
    {"あ", U'あ'},
    {"🙂", U'🙂'},
    {"\xEF\xBF\xBD", U'\uFFFD'},
};

INSTANTIATE_TEST_SUITE_P(Convert, EncodeDecodeTest,
                         ::testing::ValuesIn(kEncodeDecodeParams));

TEST(EncodeTest, Invalid) {
  EncodeResult actual = Encode(0xfffd);
  EXPECT_EQ(absl::string_view(actual.data(), actual.size()), "\ufffd");
  actual = Encode(0x110000);
  EXPECT_EQ(absl::string_view(actual.data(), actual.size()), "\ufffd");
}

TEST(DecodeTest, Empty) {
  const absl::string_view kEmptyString{""};
  const DecodeResult actual = Decode(kEmptyString.data(), kEmptyString.data());
  EXPECT_TRUE(actual.IsSentinel());
  // Check if the `Sentinel()` has the following characteristics. Not too
  // critical but they are chosen to minimize the risk when the sentinel was
  // accidentally read.
  EXPECT_FALSE(actual.ok());
  EXPECT_EQ(actual.code_point(), 0);
  EXPECT_EQ(actual.bytes_seen(), 0);
}

class DecodeInvalidTest
    : public ::testing::TestWithParam<std::pair<absl::string_view, int>> {};

constexpr std::pair<absl::string_view, int> kDecodeInvalidTestParams[] = {
    {"\xC0\xA0", 1},  // C0 is not allowed
    {"\xC2", 1},
    {"\xC2 ", 1},
    {"\xC2\xC2 ", 1},
    {"\xE0 ", 1},
    {"\xE0\xE0\xE0 ", 1},
    {"\xF0 ", 1},
    {"\xF0\xF0\xF0\xF0 ", 1},
    {"\xF5\x80\x80\x80", 1},  // F5 is not allowed
    // BOM
    {"\xFF ", 1},
    {"\xFE ", 1},
    // Non-shortest form sequences
    {"\xC0\xAF", 1},
    {"\xE0\x80\xBF", 1},
    {"\xF0\x81\x82\x42", 1},
    {"\xF0\x80\x80\xAF\x41", 1},
    // I-llformed sequences for surrogates
    {"\xed\xa0\x80", 1},
    {"\xed\xbf\xbf", 1},
    {"\xed\xaf\xaf\x41", 1},
    {"\xe0\x80\xe2", 1},
    // Beyond valid unicode range
    {"\xf4\x91\x92\x93", 1},
    {"\xff\x41", 1},
    {"\x80\xbf\x42", 1},
    // Truncated sequences
    {"\xe1\x80\xe2", 2},
    {"\xe2\xf0", 1},
    {"\xf0\x91\x92\xf1", 3},
    {"\xf1\xbf\x41", 2},
    // Buffer is not long enough.
    {"\xc2", 1},
    {"\xe2", 1},
    {"\xec\x80", 2},
    {"\xf1", 1},
    {"\xf1\xbf", 2},
    {"\xf1\xbf\x80", 3},
    {"\xf4\xbf ", 1},
};

TEST_P(DecodeInvalidTest, Decode) {
  const DecodeResult actual =
      Decode(GetParam().first.data(),
             GetParam().first.data() + GetParam().first.size());
  EXPECT_FALSE(actual.ok());
  EXPECT_EQ(actual.code_point(), kReplacementCharacter);
  EXPECT_EQ(actual.bytes_seen(), GetParam().second);
}

INSTANTIATE_TEST_SUITE_P(Invalid, DecodeInvalidTest,
                         ::testing::ValuesIn(kDecodeInvalidTestParams));

}  // namespace
}  // namespace mozc::utf8_internal
