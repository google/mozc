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

#include "base/win32/wide_char.h"

#include <string>
#include <string_view>

#include "base/util.h"
#include "testing/gunit.h"

namespace mozc::win32 {
namespace {

constexpr char kTwoSurrogatePairs[] = "𝄞𝄁";  // U+1D11E, U+1D101
constexpr wchar_t kTwoSurrogatePairsW[] = L"𝄞𝄁";

TEST(WideCharTest, WideCharsLen) {
  EXPECT_EQ(WideCharsLen(""), 0);
  EXPECT_EQ(WideCharsLen("mozc"), 4);
  EXPECT_EQ(WideCharsLen("私の名前は中野です。"), 10);
  EXPECT_EQ(WideCharsLen("𡈽"), 2);  // 𡈽 = U+2123D
  EXPECT_EQ(WideCharsLen(kTwoSurrogatePairs), 4);
  EXPECT_EQ(WideCharsLen("\xFF\xFF"), 2);  // invalid Utf-8 still counts
}

TEST(WideCharTest, WideCharsLen2) {
  const std::string input_utf8 = "a\360\240\256\237b";
  EXPECT_EQ(WideCharsLen(input_utf8), 4);
  EXPECT_EQ(WideCharsLen(Util::Utf8SubString(input_utf8, 0, 0)), 0);
  EXPECT_EQ(WideCharsLen(Util::Utf8SubString(input_utf8, 0, 1)), 1);
  EXPECT_EQ(WideCharsLen(Util::Utf8SubString(input_utf8, 0, 2)), 3);
  EXPECT_EQ(WideCharsLen(Util::Utf8SubString(input_utf8, 0, 3)), 4);
}

TEST(WideCharTest, Utf8ToWide) {
  EXPECT_EQ(Utf8ToWide(""), L"");
  EXPECT_EQ(Utf8ToWide("mozc"), L"mozc");
  EXPECT_EQ(Utf8ToWide("私の名前は中野です。"), L"私の名前は中野です。");
  EXPECT_EQ(Utf8ToWide("𡈽"), L"𡈽");
  EXPECT_EQ(Utf8ToWide(kTwoSurrogatePairs), kTwoSurrogatePairsW);
  EXPECT_EQ(Utf8ToWide("\xFF\xFF"), L"\uFFFD\uFFFD");  // invalid Utf-8
}

TEST(WideCharTest, Utf8ToWide_RoundTrip) {
  const std::string input_utf8 = "abc";
  const std::wstring output_wide = Utf8ToWide(input_utf8);
  const std::string output_utf8 = WideToUtf8(output_wide);
  EXPECT_EQ(output_utf8, "abc");
}

TEST(WideCharTest, WideToUtf8) {
  EXPECT_EQ(WideToUtf8(L""), "");
  EXPECT_EQ(WideToUtf8(L"mozc"), "mozc");
  EXPECT_EQ(WideToUtf8(L"私の名前は中野です。"), "私の名前は中野です。");
  EXPECT_EQ(WideToUtf8(L"𡈽"), "𡈽");
  EXPECT_EQ(WideToUtf8(kTwoSurrogatePairsW), kTwoSurrogatePairs);
  constexpr wchar_t kInvalid[] = {0xD800, 0};
  EXPECT_EQ(WideToUtf8(kInvalid), "\uFFFD");
}

TEST(WideCharTest, WideToUtf8_SurrogatePairSupport) {
  // Visual C++ 2008 does not support embedding surrogate pair in string
  // literals like L"\uD842\uDF9F". This is why we use wchar_t array instead.
  const wchar_t input_wide[] = {0xD842, 0xDF9F, 0};
  const std::string output_utf8 = WideToUtf8(input_wide);
  const std::wstring output_wide = Utf8ToWide(output_utf8);
  EXPECT_EQ(output_utf8, "\360\240\256\237");
  EXPECT_EQ(output_wide, input_wide);
}

TEST(WideCharTest, StrAppendW) {
  {
    std::wstring result;
    StrAppendW(&result);
    EXPECT_EQ(result, L"");
  }
  {
    std::wstring result = L"Mozc, ";
    StrAppendW(&result, L"こんにちは");
    EXPECT_EQ(result, L"Mozc, こんにちは");
  }
  {
    constexpr std::wstring_view s0 = L"Hello";
    const std::wstring s1 = L"World";
    std::wstring result;
    StrAppendW(&result, s0, L", ", s1);
    EXPECT_EQ(result, L"Hello, World");
  }
  {
    std::wstring result = L"123";
    StrAppendW(&result, L"4", L"5", L"6", L"7", L"8", L"9");
    EXPECT_EQ(result, L"123456789");
  }
}

TEST(WideCharTest, StrCatW) {
  EXPECT_EQ(StrCatW(), L"");
  EXPECT_EQ(StrCatW(L"こんにちは"), L"こんにちは");

  constexpr std::wstring_view s0 = L"Hello";
  const std::wstring s1 = L"World";
  EXPECT_EQ(StrCatW(s0, L", ", s1, L"!"), L"Hello, World!");
  EXPECT_EQ(StrCatW(s0, L"1", L"2", L"3", L"4", L"5", L"6", L"7"),
            L"Hello1234567");
}

}  // namespace
}  // namespace mozc::win32
