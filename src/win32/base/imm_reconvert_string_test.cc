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

#include "base/util.h"
#include "testing/googletest.h"
#include "testing/gunit.h"

namespace mozc {
namespace win32 {
namespace {
struct FixedReconvertString : public RECONVERTSTRING {
  BYTE buffer[4096];
  void Initialize(const std::wstring &entire_string, size_t padding_bytes) {
    dwSize = sizeof(FixedReconvertString);
    dwVersion = 0;

    dwStrOffset = FIELD_OFFSET(FixedReconvertString, buffer) + padding_bytes;
    wchar_t *dest = reinterpret_cast<wchar_t *>(&buffer[padding_bytes]);
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
  EXPECT_TRUE(ReconvertString::Validate(&reconvert_string));
}

TEST(ReconvertStringTest, dwVersion_IsNonZero) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"Hello World!", 10);
  reconvert_string.dwVersion = 1;
  EXPECT_FALSE(ReconvertString::Validate(&reconvert_string));
}

TEST(ReconvertStringTest, dwSize_IsSmallerThanSizeofRECONVERTSTRING) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"Hello World!", 10);
  reconvert_string.dwSize = sizeof(RECONVERTSTRING) - 1;
  EXPECT_FALSE(ReconvertString::Validate(&reconvert_string));
}

#if defined(_M_IX86)
TEST(ReconvertStringTest, dwSize_IsTooLargeIn32bitEnv) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"Hello World!", 10);
  reconvert_string.dwSize = 0xffff0000;
  EXPECT_FALSE(ReconvertString::Validate(&reconvert_string));
}
#endif  // _M_IX86

TEST(ReconvertStringTest, dwStrOffset_IsOnEnd) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"Hello World!", 10);
  reconvert_string.dwStrOffset = reconvert_string.dwSize;
  reconvert_string.dwStrLen = 0;
  EXPECT_TRUE(ReconvertString::Validate(&reconvert_string));
}

TEST(ReconvertStringTest, dwStrOffset_IsOutOfRange) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"Hello World!", 10);
  reconvert_string.dwStrOffset = reconvert_string.dwSize + 1;
  EXPECT_FALSE(ReconvertString::Validate(&reconvert_string));
}

TEST(ReconvertStringTest, dwStrLen_IsOutOfRange) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"Hello World!", 10);
  reconvert_string.dwStrLen = reconvert_string.dwSize / sizeof(wchar_t);
  EXPECT_FALSE(ReconvertString::Validate(&reconvert_string));
}

TEST(ReconvertStringTest, dwCompStrLen_IsOutOfRange) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"Hello World!", 10);
  reconvert_string.dwCompStrOffset = 0;
  reconvert_string.dwCompStrLen = reconvert_string.dwStrLen + 1;
  EXPECT_FALSE(ReconvertString::Validate(&reconvert_string));
}

TEST(ReconvertStringTest, dwCompStrOffset_IsOutOfRange) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"Hello World!", 10);
  reconvert_string.dwCompStrOffset =
      reconvert_string.dwStrLen * sizeof(wchar_t);
  reconvert_string.dwCompStrLen = 1;
  EXPECT_FALSE(ReconvertString::Validate(&reconvert_string));
}

TEST(ReconvertStringTest, dwCompAndTargetStrOffset_IsOddOffset) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"Hello World!", 10);
  reconvert_string.dwCompStrOffset = 1;
  reconvert_string.dwCompStrLen = 2;
  reconvert_string.dwTargetStrOffset = 3;
  reconvert_string.dwTargetStrLen = 0;
  EXPECT_FALSE(ReconvertString::Validate(&reconvert_string));
}

TEST(ReconvertStringTest, dwCompAndTargetStrOffset_IsOnEnd) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"Hello World!", 10);
  reconvert_string.dwCompStrOffset =
      reconvert_string.dwStrLen * sizeof(wchar_t);
  reconvert_string.dwCompStrLen = 0;
  reconvert_string.dwTargetStrOffset = reconvert_string.dwCompStrOffset;
  reconvert_string.dwTargetStrLen = reconvert_string.dwCompStrLen;
  EXPECT_TRUE(ReconvertString::Validate(&reconvert_string));
}

#define EXPECT_DECOMPOSE_SUCCESS(                                              \
    expected_precedeing_text, expected_preceding_composition, expected_target, \
    expected_following_composition, expected_following_text,                   \
    actual_reconvert_string)                                                   \
  do {                                                                         \
    std::wstring actual_precedeing_text;                                       \
    std::wstring actual_preceding_composition;                                 \
    std::wstring actual_target;                                                \
    std::wstring actual_following_composition;                                 \
    std::wstring actual_following_text;                                        \
    EXPECT_TRUE(ReconvertString::Decompose(                                    \
        &(actual_reconvert_string), &actual_precedeing_text,                   \
        &actual_preceding_composition, &actual_target,                         \
        &actual_following_composition, &actual_following_text));               \
    EXPECT_EQ((expected_precedeing_text), actual_precedeing_text);             \
    EXPECT_EQ((expected_preceding_composition), actual_preceding_composition); \
    EXPECT_EQ((expected_target), actual_target);                               \
    EXPECT_EQ((expected_following_composition), actual_following_composition); \
    EXPECT_EQ((expected_following_text), actual_following_text);               \
  } while (false)

#define EXPECT_DECOMPOSE_FAIL(actual_reconvert_string)           \
  do {                                                           \
    std::wstring actual_precedeing_text;                         \
    std::wstring actual_preceding_composition;                   \
    std::wstring actual_target;                                  \
    std::wstring actual_following_composition;                   \
    std::wstring actual_following_text;                          \
    EXPECT_FALSE(ReconvertString::Decompose(                     \
        &(actual_reconvert_string), &actual_precedeing_text,     \
        &actual_preceding_composition, &actual_target,           \
        &actual_following_composition, &actual_following_text)); \
  } while (false)

TEST(ReconvertStringTest, Decompose) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"Hello", 10);

  // CB: CompositionBegin
  // CE: CompositionEnd
  // TB: TargetBegin
  // TE: TargetEnd

  // 'H' |CB| 'e' |TB| 'l' |TE| 'l' |CE| 'o'
  reconvert_string.SetComposition(1, 3);
  reconvert_string.SetTarget(2, 1);
  EXPECT_DECOMPOSE_SUCCESS(L"H", L"e", L"l", L"l", L"o", reconvert_string);

  // 'H' |CB| 'e' |TB| |TE| 'l' 'l' |CE| 'o'
  reconvert_string.SetComposition(1, 3);
  reconvert_string.SetTarget(2, 0);
  EXPECT_DECOMPOSE_SUCCESS(L"H", L"e", L"", L"ll", L"o", reconvert_string);

  // 'H' |CB| 'e' |TB| 'l' 'l' |TE| |CE| 'o'
  reconvert_string.SetComposition(1, 3);
  reconvert_string.SetTarget(2, 2);
  EXPECT_DECOMPOSE_SUCCESS(L"H", L"e", L"ll", L"", L"o", reconvert_string);

  // 'H' |CB| |TB| 'e' 'l' 'l' |TE| |CE| 'o'
  reconvert_string.SetComposition(1, 3);
  reconvert_string.SetTarget(1, 3);
  EXPECT_DECOMPOSE_SUCCESS(L"H", L"", L"ell", L"", L"o", reconvert_string);

  // 'H' |CB| 'e' |TB| 'l' 'l' |CE| 'o' |TE|
  reconvert_string.SetComposition(1, 3);
  reconvert_string.SetTarget(2, 3);
  EXPECT_DECOMPOSE_FAIL(reconvert_string);

  // 'H' |TB| 'e' |CB| 'l' 'l' |CE| 'o' |TE|
  reconvert_string.SetComposition(2, 2);
  reconvert_string.SetTarget(1, 4);
  EXPECT_DECOMPOSE_FAIL(reconvert_string);

  // 'H' |TB| 'e' |CB| 'l' 'l' |TE| 'o' |CE|
  reconvert_string.SetComposition(2, 3);
  reconvert_string.SetTarget(1, 3);
  EXPECT_DECOMPOSE_FAIL(reconvert_string);

  // 'H' |TB| 'e' |TE| 'l' 'l' |CB| 'o' |CE|
  reconvert_string.SetComposition(4, 1);
  reconvert_string.SetTarget(1, 1);
  EXPECT_DECOMPOSE_FAIL(reconvert_string);

  // 'H' |CB| 'e' |CE| 'l' 'l' |TB| 'o' |TE|
  reconvert_string.SetComposition(1, 1);
  reconvert_string.SetTarget(4, 1);
  EXPECT_DECOMPOSE_FAIL(reconvert_string);

  // 'H' |CB| 'e' 'l' |TB||CE| 'l'  'o' |TE|
  reconvert_string.SetComposition(1, 2);
  reconvert_string.SetTarget(3, 2);
  EXPECT_DECOMPOSE_FAIL(reconvert_string);
}

TEST(ReconvertStringTest, Compose) {
  {
    FixedReconvertString reconvert_string;
    reconvert_string.Initialize(L"", 10);

    EXPECT_TRUE(ReconvertString::Compose(L"H", L"e", L"l", L"l", L"o",
                                         &reconvert_string));
    EXPECT_EQ(5, reconvert_string.dwStrLen);
    EXPECT_DECOMPOSE_SUCCESS(L"H", L"e", L"l", L"l", L"o", reconvert_string);
  }

  {
    FixedReconvertString reconvert_string;
    reconvert_string.Initialize(L"", 10);
    EXPECT_TRUE(
        ReconvertString::Compose(L"H", L"", L"", L"", L"", &reconvert_string));
    EXPECT_EQ(1, reconvert_string.dwStrLen);
    EXPECT_DECOMPOSE_SUCCESS(L"H", L"", L"", L"", L"", reconvert_string);
  }

  {
    FixedReconvertString reconvert_string;
    reconvert_string.Initialize(L"", 10);
    EXPECT_TRUE(
        ReconvertString::Compose(L"", L"e", L"", L"", L"", &reconvert_string));
    EXPECT_EQ(1, reconvert_string.dwStrLen);
    EXPECT_DECOMPOSE_SUCCESS(L"", L"e", L"", L"", L"", reconvert_string);
  }

  {
    FixedReconvertString reconvert_string;
    reconvert_string.Initialize(L"", 10);
    EXPECT_TRUE(
        ReconvertString::Compose(L"", L"", L"l", L"", L"", &reconvert_string));
    EXPECT_EQ(1, reconvert_string.dwStrLen);
    EXPECT_DECOMPOSE_SUCCESS(L"", L"", L"l", L"", L"", reconvert_string);
  }

  {
    FixedReconvertString reconvert_string;
    reconvert_string.Initialize(L"", 10);
    EXPECT_TRUE(
        ReconvertString::Compose(L"", L"", L"", L"l", L"", &reconvert_string));
    EXPECT_EQ(1, reconvert_string.dwStrLen);
    EXPECT_DECOMPOSE_SUCCESS(L"", L"", L"", L"l", L"", reconvert_string);
  }

  {
    FixedReconvertString reconvert_string;
    reconvert_string.Initialize(L"", 10);
    EXPECT_TRUE(
        ReconvertString::Compose(L"", L"", L"", L"", L"o", &reconvert_string));
    EXPECT_EQ(1, reconvert_string.dwStrLen);
    EXPECT_DECOMPOSE_SUCCESS(L"", L"", L"", L"", L"o", reconvert_string);
  }

  {
    FixedReconvertString reconvert_string;
    reconvert_string.Initialize(L"", 10);
    // Emulate the buffer size is too short.
    reconvert_string.dwSize = sizeof(RECONVERTSTRING);

    EXPECT_FALSE(
        ReconvertString::Compose(L"H", L"", L"", L"", L"o", &reconvert_string));
  }
}

TEST(ReconvertStringTest, EnsureCompositionIsNotEmpty) {
  {
    FixedReconvertString reconvert_string;
    reconvert_string.Initialize(L"", 10);

    // L"東京", L"", L"", L"", L"都に住む"
    EXPECT_TRUE(ReconvertString::Compose(L"\u6771\u4EAC", L"", L"", L"",
                                         L"\u90FD\u306B\u4F4F\u3080",
                                         &reconvert_string));
    EXPECT_TRUE(
        ReconvertString::EnsureCompositionIsNotEmpty(&reconvert_string));
    // L"", L"", L"東京都", L"", L"に住む"
    EXPECT_DECOMPOSE_SUCCESS(L"", L"", L"\u6771\u4EAC\u90FD", L"",
                             L"\u306B\u4F4F\u3080", reconvert_string);
  }

  {
    FixedReconvertString reconvert_string;
    reconvert_string.Initialize(L"", 10);

    // L"はい", L"", L"", L"", L"。"
    EXPECT_TRUE(ReconvertString::Compose(L"\u306F\u3044", L"", L"", L"",
                                         L"\u3002", &reconvert_string));
    EXPECT_TRUE(
        ReconvertString::EnsureCompositionIsNotEmpty(&reconvert_string));
    // L"はい", L"", L"。", L"", L""
    EXPECT_DECOMPOSE_SUCCESS(L"\u306F\u3044", L"", L"\u3002", L"", L"",
                             reconvert_string);
  }

  {
    FixedReconvertString reconvert_string;
    reconvert_string.Initialize(L"", 10);

    // L"", L"", L"", L"", L"南アメリカ大陸"
    EXPECT_TRUE(ReconvertString::Compose(
        L"", L"", L"", L"", L"\u5357\u30A2\u30E1\u30EA\u30AB\u5927\u9678",
        &reconvert_string));
    EXPECT_TRUE(
        ReconvertString::EnsureCompositionIsNotEmpty(&reconvert_string));
    // L"", L"", L"はい", L"", L"。"
    EXPECT_DECOMPOSE_SUCCESS(L"", L"", L"\u5357", L"",
                             L"\u30A2\u30E1\u30EA\u30AB\u5927\u9678",
                             reconvert_string);
  }

  {
    FixedReconvertString reconvert_string;
    reconvert_string.Initialize(L"", 10);

    // L"て\nき", L"", L"", L"", L"と\nう"
    EXPECT_TRUE(ReconvertString::Compose(L"\u3066\n\u304D", L"", L"", L"",
                                         L"\u3068\n\u3046", &reconvert_string));
    EXPECT_TRUE(
        ReconvertString::EnsureCompositionIsNotEmpty(&reconvert_string));
    // L"て\n", L"", L"きと", L"", L"\nう"
    EXPECT_DECOMPOSE_SUCCESS(L"\u3066\n", L"", L"\u304D\u3068", L"",
                             L"\n\u3046", reconvert_string);
  }
}

TEST(ReconvertStringTest, CompositionIsNotEmptyAgainstScriptTypeUnknown) {
  {
    FixedReconvertString reconvert_string;
    reconvert_string.Initialize(L"", 10);

    EXPECT_TRUE(ReconvertString::Compose(L"Hello", L"", L"", L"", L", world!",
                                         &reconvert_string));
    EXPECT_TRUE(
        ReconvertString::EnsureCompositionIsNotEmpty(&reconvert_string));
    EXPECT_DECOMPOSE_SUCCESS(L"Hello", L"", L",", L"", L" world!",
                             reconvert_string);
  }

  {
    FixedReconvertString reconvert_string;
    reconvert_string.Initialize(L"", 10);

    EXPECT_TRUE(ReconvertString::Compose(L"@@@", L"", L"", L"", L"",
                                         &reconvert_string));
    EXPECT_TRUE(
        ReconvertString::EnsureCompositionIsNotEmpty(&reconvert_string));
    EXPECT_DECOMPOSE_SUCCESS(L"@@", L"", L"@", L"", L"", reconvert_string);
  }

  {
    FixedReconvertString reconvert_string;
    reconvert_string.Initialize(L"", 10);

    EXPECT_TRUE(ReconvertString::Compose(L"", L"", L"", L"", L"@@@",
                                         &reconvert_string));
    EXPECT_TRUE(
        ReconvertString::EnsureCompositionIsNotEmpty(&reconvert_string));
    EXPECT_DECOMPOSE_SUCCESS(L"", L"", L"@", L"", L"@@", reconvert_string);
  }
}

TEST(ReconvertStringTest,
     EnsureCompositionIsNotEmptyAgainstCompositeCharacter) {
  {
    FixedReconvertString reconvert_string;
    reconvert_string.Initialize(L"", 10);

    // L"パ", L"", L"", L"", L"パ"
    EXPECT_TRUE(ReconvertString::Compose(L"\u30CF\u309A", L"", L"", L"",
                                         L"\u30D1", &reconvert_string));
    EXPECT_TRUE(
        ReconvertString::EnsureCompositionIsNotEmpty(&reconvert_string));

    // TODO(team, yukawa): Support Composite Character in Unicode.
    // L"", L"", L"パパ", L"", L"" (composite-character-aware)
    // EXPECT_DECOMPOSE_SUCCESS(
    //     L"", L"",
    //     L"\u30CF\u309A\u3046",
    //     L"", L"", reconvert_string);

    // L"パ", L"", L"パ", L"", L"" (non-composite-character-aware)
    EXPECT_DECOMPOSE_SUCCESS(L"\u30CF\u309A", L"", L"\u30D1", L"", L"",
                             reconvert_string);
  }

  {
    FixedReconvertString reconvert_string;
    reconvert_string.Initialize(L"", 10);

    // L"パ", L"", L"", L"", L"パ"
    EXPECT_TRUE(ReconvertString::Compose(L"\u30D1", L"", L"", L"",
                                         L"\u30CF\u309A", &reconvert_string));
    EXPECT_TRUE(
        ReconvertString::EnsureCompositionIsNotEmpty(&reconvert_string));

    // TODO(team, yukawa): Support Composite Character in Unicode.
    // L"", L"", L"パパ", L"", L"" (composite-character-aware)
    // EXPECT_DECOMPOSE_SUCCESS(
    //     L"", L"",
    //     L"\u30D1\u30CF\u309A",
    //     L"", L"", reconvert_string);

    // L"", L"", L"パハ", L"", L"\u309A" (non-composite-character-aware)
    EXPECT_DECOMPOSE_SUCCESS(L"", L"", L"\u30D1\u30CF", L"", L"\u309A",
                             reconvert_string);
  }
}

TEST(ReconvertStringTest, EnsureCompositionIsNotEmptyAgainstSurrogatePair) {
  // Visual C++ 2008 does not support embedding surrogate pair in string
  // literals like L"\uD842\uDF9F".  This is why we use wchar_t array instead.
  // L"今𠮟る"
  const wchar_t kSurrogatePairText0_4[] = {0x4ECA, 0xD842, 0xDF9F, 0x308B,
                                           0x0000};
  // L"今𠮟"
  const wchar_t kSurrogatePairText0_3[] = {0x4ECA, 0xD842, 0xDF9F, 0x0000};
  // Invalid
  const wchar_t kSurrogatePairText0_2[] = {0x4ECA, 0xD842, 0x0000};
  // L"今"
  const wchar_t kSurrogatePairText0_1[] = {0x4ECA, 0x0000};
  // L"𠮟る"
  const wchar_t kSurrogatePairText1_3[] = {0xD842, 0xDF9F, 0x308B, 0x0000};
  // L"𠮟"
  const wchar_t kSurrogatePairText1_2[] = {0xD842, 0xDF9F, 0x0000};
  // Invalid
  const wchar_t kSurrogatePairText1_1[] = {0xD842, 0x0000};
  // Invalid
  const wchar_t kSurrogatePairText2_2[] = {0xDF9F, 0x308B, 0x0000};
  // Invalid
  const wchar_t kSurrogatePairText2_1[] = {0xDF9F, 0x0000};
  // L"る"
  const wchar_t kSurrogatePairText3_1[] = {0x308B, 0x0000};
  {
    FixedReconvertString reconvert_string;
    reconvert_string.Initialize(L"", 10);

    // L"", L"", L"", L"", L"今𠮟る"
    EXPECT_TRUE(ReconvertString::Compose(
        L"", L"", L"", L"", kSurrogatePairText0_4, &reconvert_string));
    EXPECT_TRUE(
        ReconvertString::EnsureCompositionIsNotEmpty(&reconvert_string));

    // L"", L"", L"今𠮟", L"", L"る" (surrogate-pairs-aware)
    EXPECT_DECOMPOSE_SUCCESS(L"", L"", kSurrogatePairText0_3, L"",
                             kSurrogatePairText3_1, reconvert_string);
  }
  {
    FixedReconvertString reconvert_string;
    reconvert_string.Initialize(L"", 10);

    // L"今𠮟[High]", L"", L"", L"", L"𠮟[Low]る"
    EXPECT_TRUE(ReconvertString::Compose(kSurrogatePairText0_2, L"", L"", L"",
                                         kSurrogatePairText2_2,
                                         &reconvert_string));
    EXPECT_TRUE(
        ReconvertString::EnsureCompositionIsNotEmpty(&reconvert_string));

    // L"", L"", L"今𠮟", L"", L"る" (surrogate-pairs-aware)
    EXPECT_DECOMPOSE_SUCCESS(L"", L"", kSurrogatePairText0_3, L"",
                             kSurrogatePairText3_1, reconvert_string);
  }
  {
    FixedReconvertString reconvert_string;
    reconvert_string.Initialize(L"", 10);

    // L"今𠮟", L"", L"", L"", L""
    EXPECT_TRUE(ReconvertString::Compose(kSurrogatePairText0_3, L"", L"", L"",
                                         L"", &reconvert_string));
    EXPECT_TRUE(
        ReconvertString::EnsureCompositionIsNotEmpty(&reconvert_string));

    // L"", L"", L"今𠮟", L"", L"" (surrogate-pairs-aware)
    EXPECT_DECOMPOSE_SUCCESS(L"", L"", kSurrogatePairText0_3, L"", L"",
                             reconvert_string);
  }
}

TEST(ReconvertStringTest, ExcludeControlCode) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"", 10);

  std::wstring following_text;
  following_text += L'\0';
  // "つづく"
  following_text += L"\u3064\u3065\u304F";

  // L"テスト", L"", L"", L"", L"\0つづく"
  EXPECT_TRUE(ReconvertString::Compose(L"\u30C6\u30B9\u30C8", L"", L"", L"",
                                       following_text, &reconvert_string));
  EXPECT_TRUE(ReconvertString::EnsureCompositionIsNotEmpty(&reconvert_string));
  // L"", L"", L"テスト", L"", L"\0つづく"
  EXPECT_DECOMPOSE_SUCCESS(L"", L"", L"\u30C6\u30B9\u30C8", L"", following_text,
                           reconvert_string);
}

TEST(ReconvertStringTest, ExcludeCRLF_Issue4196242) {
  FixedReconvertString reconvert_string;
  reconvert_string.Initialize(L"", 10);

  // L"テスト", L"", L"", L"", L"@\r\n\r\n\r\nおわり"
  EXPECT_TRUE(ReconvertString::Compose(L"\u30C6\u30B9\u30C8", L"", L"", L"",
                                       L"@\r\n\r\n\r\n\u304A\u308F\u308A",
                                       &reconvert_string));
  EXPECT_TRUE(ReconvertString::EnsureCompositionIsNotEmpty(&reconvert_string));
  // L"テスト", L"", L"@", L"", L"\r\n\r\n\r\nおわり"
  EXPECT_DECOMPOSE_SUCCESS(L"\u30C6\u30B9\u30C8", L"", L"@", L"",
                           L"\r\n\r\n\r\n\u304A\u308F\u308A", reconvert_string);
}
}  // namespace win32
}  // namespace mozc
