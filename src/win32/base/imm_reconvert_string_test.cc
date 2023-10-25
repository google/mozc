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

#include "win32/base/imm_reconvert_string.h"

#include <optional>
#include <string_view>
#include <utility>

#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace win32 {
namespace {

using Strings = ReconvertString::Strings;

struct FixedReconvertString : public ReconvertString {
  wchar_t buffer[2048];
  void Initialize(const std::wstring_view entire_string,
                  const size_t padding_bytes) {
    dwSize = sizeof(FixedReconvertString);
    dwVersion = 0;

    dwStrOffset = FIELD_OFFSET(FixedReconvertString, buffer) + padding_bytes;
    wchar_t *dest = buffer + padding_bytes / sizeof(wchar_t);
    for (size_t i = 0; i < entire_string.size(); ++i) {
      dest[i] = entire_string[i];
    }
    dwStrLen = entire_string.size();

    dwCompStrLen = 0;
    dwCompStrOffset = 0;
    dwTargetStrLen = 0;
    dwTargetStrOffset = 0;
  }
  void SetComposition(size_t offset_in_chars, size_t length) {
    dwCompStrLen = length;
    dwCompStrOffset = sizeof(wchar_t) * offset_in_chars;
  }
  void SetTarget(size_t offset_in_chars, size_t length) {
    dwTargetStrLen = length;
    dwTargetStrOffset = sizeof(wchar_t) * offset_in_chars;
  }
};
}  // namespace

TEST(ReconvertStringTest, BasicTest) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"Hello World!", 10);
  EXPECT_TRUE(reconvert_string.Validate());
}

TEST(ReconvertStringTest, dwVersion_IsNonZero) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"Hello World!", 10);
  reconvert_string.dwVersion = 1;
  EXPECT_FALSE(reconvert_string.Validate());
}

TEST(ReconvertStringTest, dwSize_IsSmallerThanSizeofRECONVERTSTRING) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"Hello World!", 10);
  reconvert_string.dwSize = sizeof(RECONVERTSTRING) - 1;
  EXPECT_FALSE(reconvert_string.Validate());
}

#if defined(_M_IX86)
TEST(ReconvertStringTest, dwSize_IsTooLargeIn32bitEnv) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"Hello World!", 10);
  reconvert_string.dwSize = 0xffff0000;
  EXPECT_FALSE(reconvert_string.Validate());
}
#endif  // _M_IX86

TEST(ReconvertStringTest, dwStrOffset_IsOnEnd) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"Hello World!", 10);
  reconvert_string.dwStrOffset = reconvert_string.dwSize;
  reconvert_string.dwStrLen = 0;
  EXPECT_TRUE(reconvert_string.Validate());
}

TEST(ReconvertStringTest, dwStrOffset_IsOutOfRange) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"Hello World!", 10);
  reconvert_string.dwStrOffset = reconvert_string.dwSize + 1;
  EXPECT_FALSE(reconvert_string.Validate());
}

TEST(ReconvertStringTest, dwStrLen_IsOutOfRange) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"Hello World!", 10);
  reconvert_string.dwStrLen = reconvert_string.dwSize / sizeof(wchar_t);
  EXPECT_FALSE(reconvert_string.Validate());
}

TEST(ReconvertStringTest, dwCompStrLen_IsOutOfRange) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"Hello World!", 10);
  reconvert_string.dwCompStrOffset = 0;
  reconvert_string.dwCompStrLen = reconvert_string.dwStrLen + 1;
  EXPECT_FALSE(reconvert_string.Validate());
}

TEST(ReconvertStringTest, dwCompStrOffset_IsOutOfRange) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"Hello World!", 10);
  reconvert_string.dwCompStrOffset =
      reconvert_string.dwStrLen * sizeof(wchar_t);
  reconvert_string.dwCompStrLen = 1;
  EXPECT_FALSE(reconvert_string.Validate());
}

TEST(ReconvertStringTest, dwCompAndTargetStrOffset_IsOddOffset) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"Hello World!", 10);
  reconvert_string.dwCompStrOffset = 1;
  reconvert_string.dwCompStrLen = 2;
  reconvert_string.dwTargetStrOffset = 3;
  reconvert_string.dwTargetStrLen = 0;
  EXPECT_FALSE(reconvert_string.Validate());
}

TEST(ReconvertStringTest, dwCompAndTargetStrOffset_IsOnEnd) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"Hello World!", 10);
  reconvert_string.dwCompStrOffset =
      reconvert_string.dwStrLen * sizeof(wchar_t);
  reconvert_string.dwCompStrLen = 0;
  reconvert_string.dwTargetStrOffset = reconvert_string.dwCompStrOffset;
  reconvert_string.dwTargetStrLen = reconvert_string.dwCompStrLen;
  EXPECT_TRUE(reconvert_string.Validate());
}

struct DecomposeTestParam {
  std::pair<size_t, size_t> composition, target;
  std::optional<Strings> expected;
};

class DecomposeTest : public ::testing::TestWithParam<DecomposeTestParam> {
 public:
  DecomposeTest() { reconvert_string_.Initialize(L"Hello", 10); }

  FixedReconvertString reconvert_string_;
};

MATCHER_P(DecomposedStrEq, strings, "") {
  if (arg.has_value() && strings.has_value()) {
    return arg->preceding_text == strings->preceding_text &&
           arg->preceding_composition == strings->preceding_composition &&
           arg->target == strings->target &&
           arg->following_composition == strings->following_composition &&
           arg->following_text == strings->following_text;
  }
  return arg.has_value() == strings.has_value();
}

TEST_P(DecomposeTest, Test) {
  DecomposeTestParam param = GetParam();
  reconvert_string_.SetComposition(param.composition.first,
                                   param.composition.second);
  reconvert_string_.SetTarget(param.target.first, param.target.second);
  std::optional<ReconvertString::Strings> actual =
      reconvert_string_.Decompose();
  EXPECT_THAT(actual, DecomposedStrEq(param.expected));
}

// CB: CompositionBegin
// CE: CompositionEnd
// TB: TargetBegin
// TE: TargetEnd
INSTANTIATE_TEST_SUITE_P(
    DecomposeTests, DecomposeTest,
    testing::Values(
        // 'H' |CB| 'e' |TB| 'l' |TE| 'l' |CE| 'o'
        DecomposeTestParam{
            {1, 3}, {2, 1}, Strings{L"H", L"e", L"l", L"l", L"o"}},

        // 'H' |CB| 'e' |TB| |TE| 'l' 'l' |CE| 'o'
        DecomposeTestParam{
            {1, 3},
            {2, 0},
            Strings{L"H", L"e", L"", L"ll", L"o"},
        },
        // 'H' |CB| 'e' |TB| 'l' 'l' |TE| |CE| 'o'
        DecomposeTestParam{
            {1, 3},
            {2, 2},
            Strings{L"H", L"e", L"ll", L"", L"o"},
        },
        // 'H' |CB| |TB| 'e' 'l' 'l' |TE| |CE| 'o'
        DecomposeTestParam{
            {1, 3},
            {1, 3},
            Strings{L"H", L"", L"ell", L"", L"o"},
        },
        // 'H' |CB| 'e' |TB| 'l' 'l' |CE| 'o' |TE|
        DecomposeTestParam{
            {1, 3},
            {2, 3},
            std::nullopt,
        },
        // 'H' |TB| 'e' |CB| 'l' 'l' |CE| 'o' |TE|
        DecomposeTestParam{
            {2, 2},
            {1, 4},
            std::nullopt,
        },
        // 'H' |TB| 'e' |CB| 'l' 'l' |TE| 'o' |CE|
        DecomposeTestParam{{2, 3}, {1, 3}, std::nullopt},
        // 'H' |TB| 'e' |TE| 'l' 'l' |CB| 'o' |CE|
        DecomposeTestParam{{4, 1}, {1, 1}, std::nullopt},
        // 'H' |CB| 'e' |CE| 'l' 'l' |TB| 'o' |TE|
        DecomposeTestParam{{1, 1}, {4, 1}, std::nullopt},
        // 'H' |CB| 'e' 'l' |TB||CE| 'l'  'o' |TE|
        DecomposeTestParam{{1, 2}, {1, 3}, std::nullopt}));

class ComposeDecomposeTest : public testing::Test {
 public:
  UniqueReconvertString ComposeDecompose(Strings strings) {
    UniqueReconvertString reconvert_string = ReconvertString::Compose(strings);
    EXPECT_TRUE(reconvert_string);
    std::optional<Strings> actual = reconvert_string->Decompose();
    EXPECT_THAT(actual, DecomposedStrEq(std::make_optional(strings)));
    return reconvert_string;
  }
};

struct ComposeTestParam {
  Strings strings;
  size_t expected_str_len;
};

class ComposeTest : public ComposeDecomposeTest,
                    public testing::WithParamInterface<ComposeTestParam> {};

TEST_P(ComposeTest, Test) {
  UniqueReconvertString actual = ComposeDecompose(GetParam().strings);
  EXPECT_EQ(actual->dwStrLen, GetParam().expected_str_len);
}

INSTANTIATE_TEST_SUITE_P(
    ComposeTests, ComposeTest,
    testing::Values(ComposeTestParam{{L"H", L"e", L"l", L"l", L"o"}, 5},
                    ComposeTestParam{{L"H", L"", L"", L"", L""}, 1},
                    ComposeTestParam{{L"", L"e", L"", L"", L""}, 1},
                    ComposeTestParam{{L"", L"", L"l", L"", L""}, 1},
                    ComposeTestParam{{L"", L"", L"", L"l", L""}, 1},
                    ComposeTestParam{{L"", L"", L"", L"", L"o"}, 1}));

class EnsureCompositionIsNotEmptyTest
    : public ComposeDecomposeTest,
      public testing::WithParamInterface<std::pair<Strings, Strings>> {};

TEST_P(EnsureCompositionIsNotEmptyTest, Test) {
  UniqueReconvertString actual = ComposeDecompose(GetParam().first);
  EXPECT_TRUE(actual->EnsureCompositionIsNotEmpty());
}

INSTANTIATE_TEST_SUITE_P(EnsureCompositionIsNotEmpty,
                         EnsureCompositionIsNotEmptyTest,
                         testing::Values(
                             std::pair<Strings, Strings>{
                                 {L"東京", L"", L"", L"", L"都に住む"},
                                 {L"", L"", L"東京都", L"", L"に住む"},
                             },
                             std::pair<Strings, Strings>{
                                 {L"はい", L"", L"", L"", L"。"},
                                 {L"はい", L"", L"。", L"", L""},
                             },
                             std::pair<Strings, Strings>{
                                 {L"", L"", L"", L"", L"南アメリカ大陸"},
                                 {L"", L"", L"南", L"", L"アメリカ大陸"},
                             },
                             std::pair<Strings, Strings>{
                                 {L"て\nき", L"", L"", L"", L"と\nう"},
                                 {L"て\n", L"", L"きと", L"", L"\nう"},
                             }));

INSTANTIATE_TEST_SUITE_P(CompositionIsNotEmptyAgainstScriptTypeUnknown,
                         EnsureCompositionIsNotEmptyTest,
                         testing::Values(
                             std::pair<Strings, Strings>{
                                 {L"Hello", L"", L"", L"", L", world!"},
                                 {L"Hello", L"", L",", L"", L" world!"},
                             },
                             std::pair<Strings, Strings>{
                                 {
                                     L"@@@",
                                     L"",
                                     L"",
                                     L"",
                                     L"",
                                 },
                                 {L"@@", L"", L"@", L"", L""},
                             },
                             std::pair<Strings, Strings>{
                                 {L"", L"", L"", L"", L"@@@"},
                                 {L"", L"", L"@", L"", L"@@"},
                             }));

INSTANTIATE_TEST_SUITE_P(
    EnsureCompositionIsNotEmptyAgainstCompositeCharacter,
    EnsureCompositionIsNotEmptyTest,
    testing::Values(
        std::pair<Strings, Strings>{
            // L"パ", L"", L"", L"", L"パ"
            {L"\u30CF\u309A", L"", L"", L"", L"\u30D1"},
            // TODO(team, yukawa): Support Composite Character in Unicode.
            // L"", L"", L"パパ", L"", L"" (composite-character-aware)
            // L"パ", L"", L"パ", L"", L"" (non-composite-character-aware)
            {L"\u30CF\u309A", L"", L"\u30D1", L"", L""},
        },
        std::pair<Strings, Strings>{
            // L"パ", L"", L"", L"", L"パ"
            {L"\u30D1", L"", L"", L"", L"\u30CF\u309A"},
            // TODO(team, yukawa): Support Composite Character in Unicode.
            // L"", L"", L"パパ", L"", L"" (composite-character-aware)
            // L"", L"", L"パハ", L"", L"\u309A" (non-composite-character-aware)
            {L"", L"", L"\u30D1\u30CF", L"", L"\u309A"},
        }));

// L"今𠮟る"
constexpr std::wstring_view kSurrogatePairText = L"今𠮟る";

INSTANTIATE_TEST_SUITE_P(
    EnsureCompositionIsNotEmptyAgainstSurrogatePair,
    EnsureCompositionIsNotEmptyTest,
    testing::Values(
        std::pair<Strings, Strings>{
            {L"", L"", L"", L"", L"今𠮟る"},
            {L"", L"", L"今𠮟", L"", L"る"}  // (surrogate-pairs-aware)
        },
        std::pair<Strings, Strings>{
            // L"今𠮟[High]", L"", L"", L"", L"𠮟[Low]る"
            {kSurrogatePairText.substr(0, 2), L"", L"", L"",
             kSurrogatePairText.substr(2)},
            {L"", L"", L"今𠮟", L"", L"る"}  // (surrogate-pairs-aware)
        },
        std::pair<Strings, Strings>{
            {L"今𠮟", L"", L"", L"", L""},
            {L"", L"", L"今𠮟", L"", L""}  // (surrogate-pairs-aware)
        }));

constexpr std::wstring_view kStrWithControlCode(L"\0つづく", 4);

INSTANTIATE_TEST_SUITE_P(
    ExcludeControlCode, EnsureCompositionIsNotEmptyTest,
    testing::Values(std::pair<Strings, Strings>{
        // L"テスト", L"", L"", L"", L"\0つづく"
        {L"\u30C6\u30B9\u30C8", L"", L"", L"", kStrWithControlCode},
        // L"", L"", L"テスト", L"", L"\0つづく"
        {L"", L"", L"\u30C6\u30B9\u30C8", L"", kStrWithControlCode}}));

INSTANTIATE_TEST_SUITE_P(ExcludeCRLF_Issue4196242,
                         EnsureCompositionIsNotEmptyTest,
                         testing::Values(std::pair<Strings, Strings>{
                             {L"テスト", L"", L"", L"", L"@\r\n\r\n\r\nおわり"},
                             {L"テスト", L"", L"@", L"", L"\r\n\r\n\r\nおわり"},
                         }));

}  // namespace win32
}  // namespace mozc
