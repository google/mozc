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

#include "base/strings/unicode.h"

#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc::strings {
namespace {

using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::Pair;

TEST(UnicodeTest, OneCharLen) {
  // Here, we're testing the additional overload.
  // See internal/utf8_internal_test.cc for other tests.
  constexpr absl::string_view kText = "Mozc";
  EXPECT_EQ(OneCharLen(kText.begin()), 1);
}

TEST(UnicodeTest, CharsLen) {
  EXPECT_EQ(CharsLen(""), 0);

  constexpr absl::string_view kText = "私の名前は中野です";
  EXPECT_EQ(CharsLen(kText), 9);
  EXPECT_EQ(CharsLen(kText.begin(), kText.end()), 9);
  EXPECT_EQ(CharsLen(kText.end(), kText.end()), 0);
}

TEST(UnicodeTest, AtLeastCharsLen) {
  EXPECT_EQ(AtLeastCharsLen("", 0), 0);
  EXPECT_EQ(AtLeastCharsLen("", 1), 0);
  EXPECT_EQ(AtLeastCharsLen("あ", 0), 0);
  EXPECT_EQ(AtLeastCharsLen("あ", 1), 1);
  EXPECT_EQ(AtLeastCharsLen("あ", 2), 1);
  EXPECT_EQ(AtLeastCharsLen("Mozc", 3), 3);
  EXPECT_EQ(AtLeastCharsLen("Mozc", 4), 4);
  EXPECT_EQ(AtLeastCharsLen("Mozc", 5), 4);
}

TEST(UnicodeTest, FrontChar) {
  EXPECT_THAT(FrontChar(""), Pair("", ""));
  EXPECT_THAT(FrontChar("01"), Pair("0", "1"));
  EXPECT_THAT(FrontChar("\x01"), Pair("\x01", ""));
  EXPECT_THAT(FrontChar("あいうえお"), Pair("あ", "いうえお"));
  EXPECT_THAT(FrontChar("𧜎𧝒"), Pair("𧜎", "𧝒"));
  // Invalid length
  EXPECT_THAT(FrontChar("\xC2"), Pair("\xC2", ""));
  // BOM
  EXPECT_THAT(FrontChar("\xEF\xBB\xBF"
                        "abc"),
              Pair("\xEF\xBB\xBF", "abc"));
}

TEST(UnicodeTest, Utf8ToUtf32) {
  EXPECT_EQ(Utf8ToUtf32(""), U"");
  // Single codepoint characters.
  EXPECT_EQ(Utf8ToUtf32("aあ亜\na"), U"aあ亜\na");
  // Multiple codepoint characters
  constexpr absl::string_view kStr =
      ("神"  // U+795E
       "神󠄀"  // U+795E,U+E0100 - 2 codepoints [IVS]
       "あ゙"  // U+3042,U+3099  - 2 codepoints [Dakuten]
      );
  EXPECT_EQ(Utf8ToUtf32(kStr), U"\u795E\u795E\U000E0100\u3042\u3099");
}

TEST(UnicodeTest, Utf32ToUtf8) {
  EXPECT_EQ(Utf32ToUtf8(U""), "");
  // Single codepoint characters.
  EXPECT_EQ(Utf32ToUtf8(U"aあ亜\na"), "aあ亜\na");
  // Multiple codepoint characters
  constexpr absl::string_view kExpected =
      ("神"  // U+795E
       "神󠄀"  // U+795E,U+E0100 - 2 codepoints [IVS]
       "あ゙"  // U+3042,U+3099  - 2 codepoints [Dakuten]
      );
  constexpr std::u32string_view kU32Str = U"\u795E\u795E\U000E0100\u3042\u3099";
  EXPECT_EQ(Utf32ToUtf8(kU32Str), kExpected);
}

TEST(UnicodeTest, StrAppendChar32) {
  std::string result;
  StrAppendChar32(&result, 0);
  EXPECT_THAT(result, IsEmpty());
  StrAppendChar32(&result, 'A');
  EXPECT_EQ(result, "A");
  StrAppendChar32(&result, U'あ');
  EXPECT_EQ(result, "Aあ");
  StrAppendChar32(&result, 0x110000);
  EXPECT_EQ(result, "Aあ\uFFFD");
}

TEST(UnicodeTest, Char32ToUtf8) {
  EXPECT_EQ(Char32ToUtf8(0), absl::string_view("\0", 1));
  EXPECT_EQ(Char32ToUtf8('A'), "A");
  EXPECT_EQ(Char32ToUtf8(U'あ'), "あ");
  EXPECT_EQ(Char32ToUtf8(0x110000), "\uFFFD");
}

TEST(UnicodeTest, IsValidUtf8) {
  EXPECT_TRUE(IsValidUtf8(""));
  EXPECT_TRUE(IsValidUtf8("abc"));
  EXPECT_TRUE(IsValidUtf8("あいう"));
  EXPECT_TRUE(IsValidUtf8("aあbいcう"));

  EXPECT_FALSE(IsValidUtf8("\xC2 "));
  EXPECT_FALSE(IsValidUtf8("\xC2\xC2 "));
  EXPECT_FALSE(IsValidUtf8("\xE0 "));
  EXPECT_FALSE(IsValidUtf8("\xE0\xE0\xE0 "));
  EXPECT_FALSE(IsValidUtf8("\xF0 "));
  EXPECT_FALSE(IsValidUtf8("\xF0\xF0\xF0\xF0 "));

  // String is too short.
  EXPECT_FALSE(IsValidUtf8("\xC2"));
  constexpr char kNotNullTerminated[] = {0xf0, 0x90};
  EXPECT_FALSE(IsValidUtf8(
      absl::string_view(kNotNullTerminated, std::size(kNotNullTerminated))));

  // BOM should be treated as invalid byte.
  EXPECT_FALSE(IsValidUtf8("\xFF "));
  EXPECT_FALSE(IsValidUtf8("\xFE "));

  // Redundant encoding.
  EXPECT_FALSE(IsValidUtf8("\xC0\xAF"));
  EXPECT_FALSE(IsValidUtf8("\xE0\x80\xAF"));
  EXPECT_FALSE(IsValidUtf8("\xF0\x80\x80\xAF"));

  // Ill-formed sequences for Surrogates
  EXPECT_FALSE(IsValidUtf8("\xED\xA0\x80"));
  EXPECT_FALSE(IsValidUtf8("\xED\xBF\xBF"));
  EXPECT_FALSE(IsValidUtf8("\xED\xAF\x41"));
}

TEST(UnicodeTest, Utf8Substring) {
  EXPECT_EQ(Utf8Substring("", 0), "");
  EXPECT_EQ(Utf8Substring("Mozc", 0), "Mozc");
  EXPECT_EQ(Utf8Substring("あいうえお", 3), "えお");
  EXPECT_EQ(Utf8Substring("日本語入力", 5), "");

  EXPECT_EQ(Utf8Substring("", 0, 0), "");
  EXPECT_EQ(Utf8Substring("", 0, 42), "");
  EXPECT_EQ(Utf8Substring("五十音ABC", 2, 2), "音A");
  EXPECT_EQ(Utf8Substring("Mozc は便利", 6, 100), "便利");
  EXPECT_EQ(Utf8Substring("日本語", 2, 0), "");

  // Invalid sequence.
  EXPECT_EQ(Utf8Substring("\xF0\x80\x80\xAF", 1, 2), "\x80\x80");
}

struct Utf8AsCharsTestParam {
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const Utf8AsCharsTestParam& param) {
    sink.Append(::testing::PrintToString(param.input));
  }

  absl::string_view input;
  std::u32string_view chars32;
  std::vector<absl::string_view> u8_strings;
};

class Utf8AsCharsTest : public ::testing::TestWithParam<Utf8AsCharsTestParam> {
};

INSTANTIATE_TEST_SUITE_P(
    Valid, Utf8AsCharsTest,
    ::testing::Values(
        Utf8AsCharsTestParam{"", U"", {}},
        Utf8AsCharsTestParam{"あ", U"あ", {"あ"}},
        Utf8AsCharsTestParam{"Mozc", U"Mozc", {"M", "o", "z", "c"}},
        Utf8AsCharsTestParam{
            "日本語入力", U"日本語入力", {"日", "本", "語", "入", "力"}},
        Utf8AsCharsTestParam{
            "神"   // U+795E
            "神󠄀"   // U+795E,U+E0100 - 2 codepoints [IVS]
            "あ゙",  // U+3042,U+3099  - 2 codepoints [Dakuten]
            U"神神󠄀あ゙",
            {"神", "\u795e", "\U000e0100", "\u3042", "\u3099"}},
        Utf8AsCharsTestParam{"😀 🙆‍♀️",  // 😀, 🙆, ZWJ, ♀, U+FE0F
                             U"😀 🙆‍♀️",
                             {"😀", " ", "🙆", "\u200d", "♀", "\uFE0F"}}));

INSTANTIATE_TEST_SUITE_P(
    Invalid, Utf8AsCharsTest,
    ::testing::Values(
        Utf8AsCharsTestParam{"\xff", U"\ufffd", {"\xff"}},
        Utf8AsCharsTestParam{"\xbf", U"\ufffd", {"\xbf"}},
        Utf8AsCharsTestParam{
            "a\xbf\xbf", U"a\ufffd\ufffd", {"a", "\xbf", "\xbf"}},
        Utf8AsCharsTestParam{"日\xc0", U"日\ufffd", {"日", "\xc0"}},
        Utf8AsCharsTestParam{"\xa0あ", U"\ufffdあ", {"\xa0", "あ"}},
        Utf8AsCharsTestParam{
            "\xff\xc0🙂", U"\ufffd\ufffd🙂", {"\xff", "\xc0", "🙂"}},
        Utf8AsCharsTestParam{"\xed\xbf\xbfあ",
                             U"\ufffd\ufffd\ufffdあ",
                             {"\xed", "\xbf", "\xbf", "あ"}},
        Utf8AsCharsTestParam{"あ\xed\xbf\xbf",
                             U"あ\ufffd\ufffd\ufffd",
                             {"あ", "\xed", "\xbf", "\xbf"}},
        // From Unicode Standard §3.9 Unicode Encoding Forms
        // Non-shortest form sequences (Table 3-8)
        Utf8AsCharsTestParam{
            "\xc0\xaf\xe0\x80\xbf\xf0\x81\x82\x41",
            U"\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\x41",
            {"\xc0", "\xaf", "\xe0", "\x80", "\xbf", "\xf0", "\x81", "\x82",
             "\x41"}},
        // Ill-formed sequences for surrogates (Table 3-9)
        Utf8AsCharsTestParam{
            "\xed\xa0\x80\xed\xbf\xbf\xed\xaf\x41",
            U"\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\ufffd\x41",
            {"\xed", "\xa0", "\x80", "\xed", "\xbf", "\xbf", "\xed", "\xaf",
             "\x41"},
        },
        // Other ill-formed sequences (Table 3-10)
        Utf8AsCharsTestParam{
            "\xf4\x91\x92\x93\xff\x41\x80\xbf\x42",
            U"\ufffd\ufffd\ufffd\ufffd\ufffd\x41\ufffd\ufffd\x42",
            {"\xf4", "\x91", "\x92", "\x93", "\xff", "\x41", "\x80", "\xbf",
             "\x42"},
        },
        // Truncated sequences (Table 3-11)
        Utf8AsCharsTestParam{
            "\xe1\x80\xe2\xf0\x91\x92\xf1\xbf\x41",
            U"\ufffd\ufffd\ufffd\ufffd\x41",
            {"\xe1\x80", "\xe2", "\xf0\x91\x92", "\xf1\xbf", "\x41"},
        }));

INSTANTIATE_TEST_SUITE_P(
    BoundChecks, Utf8AsCharsTest,
    ::testing::Values(
        // 1 byte
        Utf8AsCharsTestParam{absl::string_view("\x00", 1),
                             std::u32string_view(U"\x00", 1),
                             {absl::string_view("\x00", 1)}},
        Utf8AsCharsTestParam{"\x01", U"\x01", {"\x01"}},
        Utf8AsCharsTestParam{"\x7F", U"\x7F", {"\x7F"}},
        // 2 bytes
        Utf8AsCharsTestParam{"\xC2\x7F", U"\ufffd\x7F", {"\xC2", "\x7F"}},
        Utf8AsCharsTestParam{"\xC2\x80", U"\x80", {"\xC2\x80"}},
        Utf8AsCharsTestParam{"\xDF\xBF", U"\u07FF", {"\xDF\xBF"}},
        Utf8AsCharsTestParam{"\xDF\xC0", U"\ufffd\ufffd", {"\xDF", "\xC0"}},
        // 3 bytes
        Utf8AsCharsTestParam{
            "\xE0\x9F\xBF", U"\ufffd\ufffd\ufffd", {"\xE0", "\x9F", "\xBF"}},
        Utf8AsCharsTestParam{"\xE0\xA0\x80", U"\u0800", {"\xE0\xA0\x80"}},
        Utf8AsCharsTestParam{"\xE1\x80\x80", U"\u1000", {"\xE1\x80\x80"}},
        Utf8AsCharsTestParam{"\xED\x9F\xBF", U"\uD7FF", {"\xED\x9F\xBF"}},
        Utf8AsCharsTestParam{
            "\xED\xA0\x80", U"\ufffd\ufffd\ufffd", {"\xED", "\xA0", "\x80"}},
        // 4 bytes
        Utf8AsCharsTestParam{"\xF0\x8F\xBF\xBF",
                             U"\ufffd\ufffd\ufffd\ufffd",
                             {"\xF0", "\x8F", "\xBF", "\xBF"}},
        Utf8AsCharsTestParam{
            "\xF0\x90\x80\x80", U"\U00010000", {"\xF0\x90\x80\x80"}},
        Utf8AsCharsTestParam{
            "\xF1\x80\x80\x80", U"\U00040000", {"\xF1\x80\x80\x80"}},
        Utf8AsCharsTestParam{
            "\xF3\xBF\xBF\xBF", U"\U000FFFFF", {"\xF3\xBF\xBF\xBF"}},
        Utf8AsCharsTestParam{
            "\xF4\x8F\xBF\xBF", U"\U0010FFFF", {"\xF4\x8F\xBF\xBF"}},
        Utf8AsCharsTestParam{"\xF4\x90\x80\x80",
                             U"\ufffd\ufffd\ufffd\ufffd",
                             {"\xF4", "\x90", "\x80", "\x80"}},
        Utf8AsCharsTestParam{"\xF5\x80\x80\x80",
                             U"\ufffd\ufffd\ufffd\ufffd",
                             {"\xF5", "\x80", "\x80", "\x80"}},
        Utf8AsCharsTestParam{"\xFF\xBF\xBF\xBF",
                             U"\ufffd\ufffd\ufffd\ufffd",
                             {"\xFF", "\xBF", "\xBF", "\xBF"}}));

TEST_P(Utf8AsCharsTest, AsChars32) {
  const Utf8AsChars32 s(GetParam().input);
  std::u32string actual;
  for (const char32_t c : s) {
    actual.push_back(c);
  }
  EXPECT_EQ(actual, GetParam().chars32);
  if (!s.empty()) {
    EXPECT_EQ(s.front(), GetParam().chars32.front());
    EXPECT_EQ(s.back(), GetParam().chars32.back());
  }

  actual.clear();
  absl::c_copy(s, std::back_inserter(actual));
  EXPECT_EQ(actual, GetParam().chars32);
}

TEST_P(Utf8AsCharsTest, AsString) {
  const Utf8AsChars s(GetParam().input);
  std::vector<absl::string_view> actual;
  for (const absl::string_view c : s) {
    actual.push_back(c);
  }
  EXPECT_EQ(actual, GetParam().u8_strings);
  if (!s.empty()) {
    EXPECT_EQ(s.front(), GetParam().u8_strings.front());
    EXPECT_EQ(s.back(), GetParam().u8_strings.back());
  }

  actual.clear();
  absl::c_copy(s, std::back_inserter(actual));
  EXPECT_EQ(actual, GetParam().u8_strings);
}

TEST_P(Utf8AsCharsTest, AsUnicodeChar) {
  const Utf8AsUnicodeChar s(GetParam().input);
  std::u32string actual_u32;
  std::vector<absl::string_view> actual_string_views;
  for (const UnicodeChar data : s) {
    actual_u32.push_back(data.char32());
    actual_string_views.push_back(data.utf8());
  }
  EXPECT_EQ(actual_u32, GetParam().chars32);
  EXPECT_EQ(actual_string_views, GetParam().u8_strings);
  if (!s.empty()) {
    EXPECT_EQ(s.front().char32(), GetParam().chars32.front());
    EXPECT_EQ(s.front().utf8(), GetParam().u8_strings.front());
    EXPECT_EQ(s.back().char32(), GetParam().chars32.back());
    EXPECT_EQ(s.back().utf8(), GetParam().u8_strings.back());
  }
}

TEST_P(Utf8AsCharsTest, Properties) {
  const Utf8AsChars32 s(GetParam().input);
  const absl::string_view input = GetParam().input;
  EXPECT_EQ(s.empty(), input.empty());
  EXPECT_EQ(s.begin(), s.cbegin());
  EXPECT_EQ(s.end(), s.cend());
  EXPECT_EQ(s.max_size(), input.max_size());
  EXPECT_EQ(s.view(), input);
}

TEST_P(Utf8AsCharsTest, Constructors) {
  const Utf8AsChars32 sv(GetParam().input);
  const Utf8AsChars32 data_size(GetParam().input.data(),
                                GetParam().input.size());
  EXPECT_EQ(data_size, sv);
  Utf8AsChars range(sv.begin(), sv.end());
  EXPECT_EQ(range, sv);
}

TEST_P(Utf8AsCharsTest, ToUtf32) {
  EXPECT_EQ(Utf8ToUtf32(GetParam().input), GetParam().chars32);
}

TEST_P(Utf8AsCharsTest, Substring) {
  const Utf8AsChars32 sv(GetParam().input);
  EXPECT_EQ(sv.Substring(sv.begin()), GetParam().input);
  EXPECT_EQ(sv.Substring(sv.begin(), sv.end()), GetParam().input);

  auto first = sv.begin();
  auto expected = GetParam().u8_strings.begin();
  for (int i = 0; i < 2 && first != sv.end(); ++i) {
    ++first;
    ++expected;
  }
  auto last = first;
  for (int i = 0; i < 2 && last != sv.end(); ++i) {
    ++last;
  }
  const Utf8AsChars substr(first, last);
  for (const absl::string_view s : substr) {
    EXPECT_EQ(s, *expected++);
  }

  const Utf8AsChars32 substr2(first.SubstringTo(sv.end()));
  EXPECT_THAT(substr2, ElementsAreArray(first, sv.end()));
}

// Tests if the `DCHECK` fo reading the `end` iterator hits.
// `DCHECK` is enabled only if `NDEBUG` is not defined.
#if !defined(NDEBUG)
TEST(Utf8AsCharsDeathTest, Empty) {
  Utf8AsUnicodeChar utf8_as_unicode_char("");
  auto it = utf8_as_unicode_char.begin();
  EXPECT_DEATH(*it, "");
  EXPECT_DEATH(it.view(), "");
  EXPECT_DEATH(it.char32(), "");
  EXPECT_DEATH(++it, "");
}

TEST(Utf8AsCharsDeathTest, End) {
  Utf8AsUnicodeChar utf8_as_unicode_char("a");
  auto it = utf8_as_unicode_char.begin();
  EXPECT_EQ(it.char32(), 'a');
  ++it;
  EXPECT_DEATH(*it, "");
  EXPECT_DEATH(it.view(), "");
  EXPECT_DEATH(it.char32(), "");
  EXPECT_DEATH(++it, "");
}
#endif  // !defined(NDEBUG)

TEST(Utf8AsCharsStandaloneTest, Comparators) {
  const Utf8AsChars a("aA");
  const Utf8AsChars32 b("aあ");
  EXPECT_EQ(a, a);
  EXPECT_EQ(b, b);
  EXPECT_NE(a, b);
  EXPECT_NE(b, a);
  EXPECT_LE(a, a);
  EXPECT_LE(a, b);
  EXPECT_LT(a, b);
  EXPECT_GE(b, b);
  EXPECT_GE(b, a);
  EXPECT_GT(b, a);
}

TEST(Utf8AsCharsStandaloneTest, IteratorMethods) {
  Utf8AsChars chars("a\uFFFD\xDF");
  auto it = chars.begin();
  EXPECT_EQ(it.char32(), 'a');
  EXPECT_EQ(it.view(), "a");
  EXPECT_EQ(it.size(), it.view().size());
  EXPECT_TRUE(it.ok());
  ++it;
  EXPECT_EQ(it.char32(), 0xFFFD);
  EXPECT_EQ(it.view(), "\uFFFD");
  EXPECT_EQ(it.size(), it.view().size());
  EXPECT_TRUE(it.ok());
  ++it;
  EXPECT_EQ(it.char32(), 0xFFFD);
  EXPECT_EQ(it.view(), "\xDF");
  EXPECT_EQ(it.size(), it.view().size());
  EXPECT_FALSE(it.ok());
}

}  // namespace
}  // namespace mozc::strings
