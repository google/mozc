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

#include "gui/base/encoding_util.h"

#include <string>

#include "absl/strings/string_view.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

#ifdef _WIN32
#include <windows.h>

#include "base/win32/wide_char.h"
#endif  // _WIN32

namespace mozc {
namespace {

using ::testing::IsEmpty;

#ifdef _WIN32

bool Convert(absl::string_view input, std::string* output) {
  constexpr int CP_932 = 932;

  output->clear();
  if (input.empty()) {
    return true;
  }

  const int wide_length = MultiByteToWideChar(
      CP_932, MB_ERR_INVALID_CHARS, input.data(), input.size(), nullptr, 0);
  if (wide_length == 0) {
    return false;
  }

  std::wstring wide(wide_length, 0);
  if (MultiByteToWideChar(CP_932, MB_ERR_INVALID_CHARS, input.data(),
                          input.size(), wide.data(),
                          wide_length + 1) != wide_length) {
    return false;
  }

  *output = win32::WideToUtf8(wide);
  return true;
}

TEST(EncodingUtilTest, CompareToWinAPI) {
  constexpr absl::string_view kTestCases[] = {
      // "私の名前はGoogleです。"
      "\x8E\x84\x82\xCC\x96\xBC\x91\x4F\x82\xCD\x47\x6F\x6F\x67\x6C\x65"
      "\x82\xC5\x82\xB7\x81\x42",
      // "今日はとても良い天気です。"
      "\x8D\xA1\x93\xFA\x82\xCD\x82\xC6\x82\xC4\x82\xE0\x97\xC7\x82\xA2"
      "\x93\x56\x8B\x43\x82\xC5\x82\xB7\x81\x42",
      "This is a test for SJIS.",
      // "あいうえおアイウエオｱｲｳｴｵ"
      "\x82\xA0\x82\xA2\x82\xA4\x82\xA6\x82\xA8\x83\x41\x83\x43\x83\x45"
      "\x83\x47\x83\x49\xB1\xB2\xB3\xB4\xB5",
  };
  for (const absl::string_view sjis : kTestCases) {
    std::string actual = EncodingUtil::SjisToUtf8(sjis);
    std::string expected;
    ASSERT_TRUE(Convert(sjis, &expected));
    EXPECT_EQ(actual, expected);
  }
}

#endif  // _WIN32

TEST(EncodingUtilTest, Issue2190350) {
  std::string result = EncodingUtil::SjisToUtf8("\x82\xA0");
  EXPECT_EQ(result.length(), 3);
  EXPECT_EQ(result, "あ");
}

TEST(EncodingUtilTest, ValidSJIS) {
  constexpr struct {
    absl::string_view sjis;
    absl::string_view utf8;
  } kTestCases[] = {
      // "私の名前はGoogleです。"
      {"\x8E\x84\x82\xCC\x96\xBC\x91\x4F\x82\xCD\x47\x6F\x6F\x67\x6C\x65"
       "\x82\xC5\x82\xB7\x81\x42",
       "私の名前はGoogleです。"},
      // "今日はとても良い天気です。"
      {"\x8D\xA1\x93\xFA\x82\xCD\x82\xC6\x82\xC4\x82\xE0\x97\xC7\x82\xA2"
       "\x93\x56\x8B\x43\x82\xC5\x82\xB7\x81\x42",
       "今日はとても良い天気です。"},
      {"This is a test for SJIS.", "This is a test for SJIS."},
      // "あいうえおアイウエオｱｲｳｴｵ"
      {"\x82\xA0\x82\xA2\x82\xA4\x82\xA6\x82\xA8\x83\x41\x83\x43\x83\x45"
       "\x83\x47\x83\x49\xB1\xB2\xB3\xB4\xB5",
       "あいうえおアイウエオｱｲｳｴｵ"},
  };
  for (const auto& tc : kTestCases) {
    EXPECT_EQ(EncodingUtil::SjisToUtf8(tc.sjis), tc.utf8);
  }
}

TEST(EncodingUtilTest, InvalidSJIS) {
  constexpr absl::string_view kInvalidInputs[] = {
      // Invalid first byte (0xA0) at 1st byte
      "\xA0\x61\x62\x63",
      // Invalid first byte (0xA0) at 4-th byte
      "\x61\x62\x63\xA0\x64\x65\x66",
      // Invalid first byte (0xA0) at the last byte
      "\x61\x62\x63\xA0",
      // Valid first byte (0xE0) but there's no second byte
      "\x61\x62\x63\xE0",
      // Valid first byte (0x90) in range 2 + invalid second byte (0x15)
      "\x61\x62\x63\x90\x15\x64\x65\x66",
      // Valid first byte (0xEE) in range 4 + invalid second byte (0x01)
      "\x61\x62\x63\xEE\x01\x64\x65\x66",
  };
  for (const absl::string_view input : kInvalidInputs) {
    EXPECT_THAT(EncodingUtil::SjisToUtf8(input), IsEmpty());
  }
}

}  // namespace
}  // namespace mozc
