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

#include "base/util.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ios>
#include <iterator>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

template <typename T>
class TypedUtilTest : public ::testing::Test {};

using StrTypes = ::testing::Types<std::string, absl::string_view>;
TYPED_TEST_SUITE(TypedUtilTest, StrTypes);

TYPED_TEST(TypedUtilTest, AppendUtf8Chars) {
  using StrType = TypeParam;
  {
    std::vector<StrType> output;
    Util::AppendUtf8Chars("", output);
    EXPECT_EQ(output.size(), 0);
  }
  {
    constexpr absl::string_view kInputs[] = {
        "a", "あ", "亜", "\n", "a",
    };
    const std::string joined_string = absl::StrJoin(kInputs, "");

    std::vector<StrType> output = {"x", "y", "z"};
    Util::AppendUtf8Chars(joined_string, output);
    EXPECT_THAT(output, ElementsAre("x", "y", "z", "a", "あ", "亜", "\n", "a"));
  }
}

TEST(UtilTest, SplitStringToUtf8Chars) {
  {
    const std::vector<std::string> output = Util::SplitStringToUtf8Chars("");
    EXPECT_EQ(output.size(), 0);
  }
  {
    const std::string kInputs[] = {
        "a", "あ", "亜", "\n", "a",
    };
    const std::string joined_string = absl::StrJoin(kInputs, "");

    const std::vector<std::string> output =
        Util::SplitStringToUtf8Chars(joined_string);
    EXPECT_THAT(output, ElementsAreArray(kInputs));
  }
}

TEST(UtilTest, SplitStringToUtf8Graphemes) {
  {  // Single codepoint characters.
    const std::string kInputs[] = {
        "a", "あ", "亜", "\n", "a",
    };
    const std::string joined_string = absl::StrJoin(kInputs, "");

    std::vector<std::string> graphemes;
    Util::SplitStringToUtf8Graphemes(joined_string, &graphemes);
    EXPECT_THAT(graphemes, ElementsAreArray(kInputs));
  }

  {  // Multiple codepoint characters
    const std::string kInputs[] = {
        "神",  // U+795E
        "神︀",  // U+795E,U+FE00  - 2 codepoints [SVS]
        "神󠄀",  // U+795E,U+E0100 - 2 codepoints [IVS]
        "あ゙",  // U+3042,U+3099  - 2 codepoints [Dakuten]
        "か゚",  // U+304B,U+309A  - 2 codepoints [Handakuten]
    };
    const std::string joined_string = absl::StrJoin(kInputs, "");

    std::vector<std::string> graphemes;
    Util::SplitStringToUtf8Graphemes(joined_string, &graphemes);
    EXPECT_THAT(graphemes, ElementsAreArray(kInputs));
  }

  {  // Multiple codepoint emojis
    const std::string kInputs[] = {
        "🛳\uFE0E",  // U+1F6F3,U+FE0E - text presentation sequence
        "🛳\uFE0F",  // U+1F6F3,U+FE0F - emoji presentation sequence
        "✌🏿",      // U+261D,U+1F3FF - emoji modifier sequence
        "🇯🇵",       // U+1F1EF,U+1F1F5 - emoji flag sequence
        "🏴󠁧󠁢󠁥󠁮󠁧󠁿",  // U+1F3F4,U+E0067,U+E0062,U+E0065,U+E006E,U+E0067
                                         // - emoji tag sequence
        "#️⃣",  // U+0023,U+FE0F,U+20E3 - emoji keycap sequence
        "👨‍👩‍👧‍👦",  // U+1F468,U+200D,U+1F469,U+200D,U+1F467,U+200D,U+1F466
                                      // - emoji zwj sequence
    };
    const std::string joined_string = absl::StrJoin(kInputs, "");

    std::vector<std::string> graphemes;
    Util::SplitStringToUtf8Graphemes(joined_string, &graphemes);
    EXPECT_THAT(graphemes, ElementsAreArray(kInputs));
  }

  // Multiple emoji flag sequences
  {
    const std::string kInputs[] = {
        "🇧🇸",  // U+1F1E7,U+1F1F8 - Bahamas (Country code: BS)
        "🇸🇧",  // U+1F1F8,U+1F1E7 - Solomon Islands (Country code: SB)
    };
    const std::string joined_string = absl::StrJoin(kInputs, "");

    std::vector<std::string> graphemes;
    Util::SplitStringToUtf8Graphemes(joined_string, &graphemes);
    EXPECT_THAT(graphemes, ElementsAreArray(kInputs));
  }

  {  // Invalid codepoint characters
    const std::string kInputs[] = {
        "\uFE00",              // Extend only [SVS]
        "神\uFE00\U000E0100",  // Multiple extends [SVS, IVS]
        "🛳\uFE0E\uFE0E",       // Multiple extends [text_presentation_selector,
                               // text_presentation_selector]
        "🛳\uFE0F\uFE0F",       // Multiple extends [emoji_presentation_selector,
                               // emoji_presentation_selector]
        "✌🏿\U0001F3FF",       // Multiple extends [emoji_modifier,
                               // emoji_modifier]
        "🏴󠁧󠁢󠁥󠁮󠁧󠁿\U000E0067",  // Multiple extends
                                                   // [tag_end,
                                                   // tag_end]
        "\U0001F468\u200D\U0001F469\u200D\u200D\U0001F467\u200D"
        "\U0001F466"  // Successive ZWJ between "👨‍👩‍👧‍👦"
    };
    const std::string joined_string = absl::StrJoin(kInputs, "");

    std::vector<std::string> graphemes;
    Util::SplitStringToUtf8Graphemes(joined_string, &graphemes);
    EXPECT_THAT(graphemes, ElementsAreArray(kInputs));
  }
}

TEST(UtilTest, SplitCSV) {
  std::vector<std::string> answer_vector;

  Util::SplitCSV("Google,x,\"Buchheit, Paul\",\"string with \"\" quote in it\"",
                 &answer_vector);
  EXPECT_EQ(answer_vector.size(), 4);
  EXPECT_EQ(answer_vector[0], "Google");
  EXPECT_EQ(answer_vector[1], "x");
  EXPECT_EQ(answer_vector[2], "Buchheit, Paul");
  EXPECT_EQ(answer_vector[3], "string with \" quote in it");

  Util::SplitCSV("Google,hello,", &answer_vector);
  EXPECT_EQ(answer_vector.size(), 3);
  EXPECT_EQ(answer_vector[0], "Google");
  EXPECT_EQ(answer_vector[1], "hello");
  EXPECT_EQ(answer_vector[2], "");

  Util::SplitCSV("Google rocks,hello", &answer_vector);
  EXPECT_EQ(answer_vector.size(), 2);
  EXPECT_EQ(answer_vector[0], "Google rocks");
  EXPECT_EQ(answer_vector[1], "hello");

  Util::SplitCSV(",,\"\",,", &answer_vector);
  EXPECT_EQ(answer_vector.size(), 5);
  EXPECT_EQ(answer_vector[0], "");
  EXPECT_EQ(answer_vector[1], "");
  EXPECT_EQ(answer_vector[2], "");
  EXPECT_EQ(answer_vector[3], "");
  EXPECT_EQ(answer_vector[4], "");

  // Test a string containing a comma.
  Util::SplitCSV("\",\",hello", &answer_vector);
  EXPECT_EQ(answer_vector.size(), 2);
  EXPECT_EQ(answer_vector[0], ",");
  EXPECT_EQ(answer_vector[1], "hello");

  // Invalid CSV
  Util::SplitCSV("\"no,last,quote", &answer_vector);
  EXPECT_EQ(answer_vector.size(), 1);
  EXPECT_EQ(answer_vector[0], "no,last,quote");

  Util::SplitCSV("backslash\\,is,no,an,\"escape\"", &answer_vector);
  EXPECT_EQ(answer_vector.size(), 5);
  EXPECT_EQ(answer_vector[0], "backslash\\");
  EXPECT_EQ(answer_vector[1], "is");
  EXPECT_EQ(answer_vector[2], "no");
  EXPECT_EQ(answer_vector[3], "an");
  EXPECT_EQ(answer_vector[4], "escape");

  Util::SplitCSV("", &answer_vector);
  EXPECT_EQ(answer_vector.size(), 0);
}

TEST(UtilTest, LowerString) {
  std::string s = "TeSTtest";
  Util::LowerString(&s);
  EXPECT_EQ(s, "testtest");

  std::string s2 = "ＴｅＳＴ＠ＡＢＣＸＹＺ［｀ａｂｃｘｙｚ｛";
  Util::LowerString(&s2);
  EXPECT_EQ(s2, "ｔｅｓｔ＠ａｂｃｘｙｚ［｀ａｂｃｘｙｚ｛");
}

TEST(UtilTest, UpperString) {
  std::string s = "TeSTtest";
  Util::UpperString(&s);
  EXPECT_EQ(s, "TESTTEST");

  std::string s2 = "ＴｅＳＴ＠ＡＢＣＸＹＺ［｀ａｂｃｘｙｚ｛";
  Util::UpperString(&s2);
  EXPECT_EQ(s2, "ＴＥＳＴ＠ＡＢＣＸＹＺ［｀ＡＢＣＸＹＺ｛");
}

TEST(UtilTest, CapitalizeString) {
  std::string s = "TeSTtest";
  Util::CapitalizeString(&s);
  EXPECT_EQ(s, "Testtest");

  std::string s2 = "ＴｅＳＴ＠ＡＢＣＸＹＺ［｀ａｂｃｘｙｚ｛";
  Util::CapitalizeString(&s2);
  EXPECT_EQ(s2, "Ｔｅｓｔ＠ａｂｃｘｙｚ［｀ａｂｃｘｙｚ｛");
}

TEST(UtilTest, IsLowerAscii) {
  EXPECT_TRUE(Util::IsLowerAscii(""));
  EXPECT_TRUE(Util::IsLowerAscii("hello"));
  EXPECT_FALSE(Util::IsLowerAscii("HELLO"));
  EXPECT_FALSE(Util::IsLowerAscii("Hello"));
  EXPECT_FALSE(Util::IsLowerAscii("HeLlO"));
  EXPECT_FALSE(Util::IsLowerAscii("symbol!"));
  EXPECT_FALSE(Util::IsLowerAscii("Ｈｅｌｌｏ"));
}

TEST(UtilTest, IsUpperAscii) {
  EXPECT_TRUE(Util::IsUpperAscii(""));
  EXPECT_FALSE(Util::IsUpperAscii("hello"));
  EXPECT_TRUE(Util::IsUpperAscii("HELLO"));
  EXPECT_FALSE(Util::IsUpperAscii("Hello"));
  EXPECT_FALSE(Util::IsUpperAscii("HeLlO"));
  EXPECT_FALSE(Util::IsUpperAscii("symbol!"));
  EXPECT_FALSE(Util::IsUpperAscii("Ｈｅｌｌｏ"));
}

TEST(UtilTest, IsCapitalizedAscii) {
  EXPECT_TRUE(Util::IsCapitalizedAscii(""));
  EXPECT_FALSE(Util::IsCapitalizedAscii("hello"));
  EXPECT_FALSE(Util::IsCapitalizedAscii("HELLO"));
  EXPECT_TRUE(Util::IsCapitalizedAscii("Hello"));
  EXPECT_FALSE(Util::IsCapitalizedAscii("HeLlO"));
  EXPECT_FALSE(Util::IsCapitalizedAscii("symbol!"));
  EXPECT_FALSE(Util::IsCapitalizedAscii("Ｈｅｌｌｏ"));
}

TEST(UtilTest, Utf8ToUtf32) {
  EXPECT_EQ(Util::Utf8ToUtf32(""), U"");
  // Single codepoint characters.
  EXPECT_EQ(Util::Utf8ToUtf32("aあ亜\na"), U"aあ亜\na");
  // Multiple codepoint characters
  constexpr absl::string_view kStr =
      ("神"  // U+795E
       "神󠄀"  // U+795E,U+E0100 - 2 codepoints [IVS]
       "あ゙"  // U+3042,U+3099  - 2 codepoints [Dakuten]
      );
  EXPECT_EQ(Util::Utf8ToUtf32(kStr), U"\u795E\u795E\U000E0100\u3042\u3099");
}

TEST(UtilTest, Utf32ToUtf8) {
  EXPECT_EQ(Util::Utf32ToUtf8(U""), "");
  // Single codepoint characters.
  EXPECT_EQ(Util::Utf32ToUtf8(U"aあ亜\na"), "aあ亜\na");
  // Multiple codepoint characters
  constexpr absl::string_view kExpected =
      ("神"  // U+795E
       "神󠄀"  // U+795E,U+E0100 - 2 codepoints [IVS]
       "あ゙"  // U+3042,U+3099  - 2 codepoints [Dakuten]
      );
  constexpr std::u32string_view kU32Str = U"\u795E\u795E\U000E0100\u3042\u3099";
  EXPECT_EQ(Util::Utf32ToUtf8(kU32Str), kExpected);
}

void VerifyUtf8ToCodepoint(absl::string_view text, char32_t expected_codepoint,
                           size_t expected_len) {
  const char* begin = text.data();
  const char* end = begin + text.size();
  size_t mblen = 0;
  char32_t result = Util::Utf8ToCodepoint(begin, end, &mblen);
  EXPECT_EQ(result, expected_codepoint)
      << text << " " << std::hex << static_cast<uint64_t>(expected_codepoint);
  EXPECT_EQ(mblen, expected_len) << text << " " << expected_len;
}

TEST(UtilTest, Utf8ToCodepoint) {
  VerifyUtf8ToCodepoint("", 0, 0);
  VerifyUtf8ToCodepoint("\x01", 1, 1);
  VerifyUtf8ToCodepoint("\x7F", 0x7F, 1);
  VerifyUtf8ToCodepoint("\xC2\x80", 0x80, 2);
  VerifyUtf8ToCodepoint("\xDF\xBF", 0x7FF, 2);
  VerifyUtf8ToCodepoint("\xE0\xA0\x80", 0x800, 3);
  VerifyUtf8ToCodepoint("\xEF\xBF\xBF", 0xFFFF, 3);
  VerifyUtf8ToCodepoint("\xF0\x90\x80\x80", 0x10000, 4);
  VerifyUtf8ToCodepoint("\xF7\xBF\xBF\xBF", 0x1FFFFF, 4);
  // do not test 5-6 bytes because it's out of spec of UTF8.
}

TEST(UtilTest, CodepointToUtf8) {
  // Do nothing if |c| is NUL. Previous implementation of CodepointToUtf8 worked
  // like this even though the reason is unclear.
  std::string output = Util::CodepointToUtf8(0);
  EXPECT_TRUE(output.empty());

  output = Util::CodepointToUtf8(0x7F);
  EXPECT_EQ(output, "\x7F");
  output = Util::CodepointToUtf8(0x80);
  EXPECT_EQ(output, "\xC2\x80");
  output = Util::CodepointToUtf8(0x7FF);
  EXPECT_EQ(output, "\xDF\xBF");
  output = Util::CodepointToUtf8(0x800);
  EXPECT_EQ(output, "\xE0\xA0\x80");
  output = Util::CodepointToUtf8(0xFFFF);
  EXPECT_EQ(output, "\xEF\xBF\xBF");
  output = Util::CodepointToUtf8(0x10000);
  EXPECT_EQ(output, "\xF0\x90\x80\x80");
  output = Util::CodepointToUtf8(0x1FFFFF);
  EXPECT_EQ(output, "\xF7\xBF\xBF\xBF");

  // Buffer version.
  char buf[7];

  EXPECT_EQ(Util::CodepointToUtf8(0, buf), 0);
  EXPECT_EQ(strcmp(buf, ""), 0);

  EXPECT_EQ(Util::CodepointToUtf8(0x7F, buf), 1);
  EXPECT_EQ(strcmp("\x7F", buf), 0);

  EXPECT_EQ(Util::CodepointToUtf8(0x80, buf), 2);
  EXPECT_EQ(strcmp("\xC2\x80", buf), 0);

  EXPECT_EQ(Util::CodepointToUtf8(0x7FF, buf), 2);
  EXPECT_EQ(strcmp("\xDF\xBF", buf), 0);

  EXPECT_EQ(Util::CodepointToUtf8(0x800, buf), 3);
  EXPECT_EQ(strcmp("\xE0\xA0\x80", buf), 0);

  EXPECT_EQ(Util::CodepointToUtf8(0xFFFF, buf), 3);
  EXPECT_EQ(strcmp("\xEF\xBF\xBF", buf), 0);

  EXPECT_EQ(Util::CodepointToUtf8(0x10000, buf), 4);
  EXPECT_EQ(strcmp("\xF0\x90\x80\x80", buf), 0);

  EXPECT_EQ(Util::CodepointToUtf8(0x1FFFFF, buf), 4);
  EXPECT_EQ(strcmp("\xF7\xBF\xBF\xBF", buf), 0);
}

TEST(UtilTest, CharsLen) {
  const std::string src = "私の名前は中野です";
  EXPECT_EQ(Util::CharsLen(src), 9);
}

TEST(UtilTest, CharsLenInvalid) {
  const absl::string_view kText = "あいうえお";
  EXPECT_EQ(Util::CharsLen(kText.substr(0, 1)), 1);
  EXPECT_EQ(Util::CharsLen(kText.substr(0, 4)), 2);
}

TEST(UtilTest, PrefixCharsLen) {
  const std::string src = "東京駅";  // 9 bytes: 3 + 3 + 3
  EXPECT_THAT(Util::PrefixCharsLen(src),
              ElementsAre(0, 0, 0, 1, 1, 1, 2, 2, 2, 3));

  EXPECT_THAT(Util::PrefixCharsLen("a亜b"),
              ElementsAre(0, 1, 1, 1, 2, 3));
  EXPECT_THAT(Util::PrefixCharsLen(""), ElementsAre(0));
}

TEST(UtilTest, Utf8SubString) {
  const absl::string_view src = "私の名前は中野です";
  absl::string_view result;

  result = Util::Utf8SubString(src, 0, 2);
  EXPECT_EQ(result, "私の");
  // |result|'s data should point to the same memory block as src.
  EXPECT_LE(src.data(), result.data());

  result = Util::Utf8SubString(src, 4, 1);
  EXPECT_EQ(result, "は");
  EXPECT_LE(src.data(), result.data());

  result = Util::Utf8SubString(src, 5, 3);
  EXPECT_EQ(result, "中野で");
  EXPECT_LE(src.data(), result.data());

  result = Util::Utf8SubString(src, 6, 10);
  EXPECT_EQ(result, "野です");
  EXPECT_LE(src.data(), result.data());

  result = Util::Utf8SubString(src, 4, 2);
  EXPECT_EQ(result, "は中");
  EXPECT_LE(src.data(), result.data());

  result = Util::Utf8SubString(src, 2, std::string::npos);
  EXPECT_EQ(result, "名前は中野です");
  EXPECT_LE(src.data(), result.data());

  result = Util::Utf8SubString(src, 5, std::string::npos);
  EXPECT_EQ(result, "中野です");
  EXPECT_LE(src.data(), result.data());
}

TEST(UtilTest, Utf8SubString2) {
  const absl::string_view src = "私はGoogleです";

  absl::string_view result;

  result = Util::Utf8SubString(src, 0);
  EXPECT_EQ(result, src);

  result = Util::Utf8SubString(src, 5);
  EXPECT_EQ(result, "gleです");

  result = Util::Utf8SubString(src, 10);
  EXPECT_TRUE(result.empty());

  result = Util::Utf8SubString(src, 13);
  EXPECT_TRUE(result.empty());
}

TEST(UtilTest, Utf8SubString3) {
  const absl::string_view src = "私の名前は中野です";
  std::string result;

  result.clear();
  Util::Utf8SubString(src, 0, 2, &result);
  EXPECT_EQ(result, "私の");

  result.clear();
  Util::Utf8SubString(src, 4, 1, &result);
  EXPECT_EQ(result, "は");

  result.clear();
  Util::Utf8SubString(src, 5, 3, &result);
  EXPECT_EQ(result, "中野で");

  result.clear();
  Util::Utf8SubString(src, 6, 10, &result);
  EXPECT_EQ(result, "野です");

  result.clear();
  Util::Utf8SubString(src, 4, 2, &result);
  EXPECT_EQ(result, "は中");

  result.clear();
  Util::Utf8SubString(src, 2, std::string::npos, &result);
  EXPECT_EQ(result, "名前は中野です");

  result.clear();
  Util::Utf8SubString(src, 5, std::string::npos, &result);
  EXPECT_EQ(result, "中野です");

  // Doesn't clear result and call Util::Utf8SubString
  Util::Utf8SubString(src, 5, std::string::npos, &result);
  EXPECT_EQ(result, "中野です");
}

TEST(UtilTest, StripUtf8Bom) {
  // Should be stripped.
  EXPECT_EQ(Util::StripUtf8Bom("\xef\xbb\xbf"
                               "abc"),
            "abc");

  // Should be stripped.
  EXPECT_EQ(Util::StripUtf8Bom("\xef\xbb\xbf"), "");

  // BOM in the middle of text. Shouldn't be stripped.
  EXPECT_EQ(Util::StripUtf8Bom("a"
                               "\xef\xbb\xbf"
                               "bc"),
            "a"
            "\xef\xbb\xbf"
            "bc");

  // Incomplete BOM. Shouldn't be stripped.
  EXPECT_EQ(Util::StripUtf8Bom("\xef\xbb"
                               "abc"),
            "\xef\xbb"
            "abc");

  // String shorter than the BOM. Do nothing.
  EXPECT_EQ(Util::StripUtf8Bom("a"), "a");

  // Empty string. Do nothing.
  EXPECT_EQ(Util::StripUtf8Bom(""), "");
}

TEST(UtilTest, IsUtf16Bom) {
  EXPECT_FALSE(Util::IsUtf16Bom(""));
  EXPECT_FALSE(Util::IsUtf16Bom("abc"));
  EXPECT_TRUE(Util::IsUtf16Bom("\xfe\xff"));
  EXPECT_TRUE(Util::IsUtf16Bom("\xff\xfe"));
  EXPECT_TRUE(Util::IsUtf16Bom("\xfe\xff "));
  EXPECT_TRUE(Util::IsUtf16Bom("\xff\xfe "));
  EXPECT_FALSE(Util::IsUtf16Bom(" \xfe\xff"));
  EXPECT_FALSE(Util::IsUtf16Bom(" \xff\xfe"));
  EXPECT_FALSE(Util::IsUtf16Bom("\xff\xff"));
}

TEST(UtilTest, BracketTest) {
  using BracketPair = std::pair<absl::string_view, absl::string_view>;
  constexpr std::array<BracketPair, 16> kBracketType = {{
      // {{, }} is necessary to initialize std::array.
      {"（", "）"},
      {"〔", "〕"},
      {"［", "］"},
      {"｛", "｝"},
      {"〈", "〉"},
      {"《", "》"},
      {"「", "」"},
      {"『", "』"},
      {"【", "】"},
      {"〘", "〙"},
      {"〚", "〛"},
      {"«", "»"},
      {"‘", "’"},
      {"“", "”"},
      {"‹", "›"},
      {"〝", "〟"},
  }};

  absl::string_view pair;
  for (const BracketPair& bracket : kBracketType) {
    EXPECT_TRUE(Util::IsOpenBracket(bracket.first, &pair));
    EXPECT_EQ(pair, bracket.second);
    EXPECT_TRUE(Util::IsCloseBracket(bracket.second, &pair));
    EXPECT_EQ(pair, bracket.first);
    EXPECT_FALSE(Util::IsOpenBracket(bracket.second, &pair));
    EXPECT_FALSE(Util::IsCloseBracket(bracket.first, &pair));
  }
}

TEST(UtilTest, IsBracketPairText) {
  EXPECT_TRUE(Util::IsBracketPairText("\"\""));
  EXPECT_TRUE(Util::IsBracketPairText("''"));
  EXPECT_TRUE(Util::IsBracketPairText("<>"));
  EXPECT_TRUE(Util::IsBracketPairText("``"));
  EXPECT_TRUE(Util::IsBracketPairText("«»"));
  EXPECT_TRUE(Util::IsBracketPairText("()"));
  EXPECT_TRUE(Util::IsBracketPairText("[]"));
  EXPECT_TRUE(Util::IsBracketPairText("{}"));
  EXPECT_TRUE(Util::IsBracketPairText("‘’"));
  EXPECT_TRUE(Util::IsBracketPairText("“”"));
  EXPECT_TRUE(Util::IsBracketPairText("‹›"));
  EXPECT_TRUE(Util::IsBracketPairText("〈〉"));
  EXPECT_TRUE(Util::IsBracketPairText("《》"));
  EXPECT_TRUE(Util::IsBracketPairText("「」"));
  EXPECT_TRUE(Util::IsBracketPairText("『』"));
  EXPECT_TRUE(Util::IsBracketPairText("【】"));
  EXPECT_TRUE(Util::IsBracketPairText("〔〕"));
  EXPECT_TRUE(Util::IsBracketPairText("〘〙"));
  EXPECT_TRUE(Util::IsBracketPairText("〚〛"));
  EXPECT_TRUE(Util::IsBracketPairText("（）"));
  EXPECT_TRUE(Util::IsBracketPairText("［］"));
  EXPECT_TRUE(Util::IsBracketPairText("｛｝"));
  EXPECT_TRUE(Util::IsBracketPairText("｢｣"));
  EXPECT_TRUE(Util::IsBracketPairText("〝〟"));

  // Open bracket and close brakcet don't match.
  EXPECT_FALSE(Util::IsBracketPairText("(]"));

  // More than two characters.
  EXPECT_FALSE(Util::IsBracketPairText("{{}}"));

  // Non bracket character exists.
  EXPECT_FALSE(Util::IsBracketPairText("1)"));
}

TEST(UtilTest, IsEnglishTransliteration) {
  EXPECT_TRUE(Util::IsEnglishTransliteration("ABC"));
  EXPECT_TRUE(Util::IsEnglishTransliteration("Google"));
  EXPECT_TRUE(Util::IsEnglishTransliteration("Google Map"));
  EXPECT_TRUE(Util::IsEnglishTransliteration("ABC-DEF"));
  EXPECT_TRUE(Util::IsEnglishTransliteration("Foo-bar"));
  EXPECT_TRUE(Util::IsEnglishTransliteration("Foo!"));
  EXPECT_TRUE(Util::IsEnglishTransliteration("Who's"));
  EXPECT_TRUE(Util::IsEnglishTransliteration("!"));
  EXPECT_TRUE(Util::IsEnglishTransliteration("  "));
  EXPECT_FALSE(Util::IsEnglishTransliteration("てすと"));
  EXPECT_FALSE(Util::IsEnglishTransliteration("テスト"));
  EXPECT_FALSE(Util::IsEnglishTransliteration("東京"));
}

TEST(UtilTest, ChopReturns) {
  std::string line = "line\n";
  EXPECT_TRUE(Util::ChopReturns(&line));
  EXPECT_EQ(line, "line");

  line = "line\r";
  EXPECT_TRUE(Util::ChopReturns(&line));
  EXPECT_EQ(line, "line");

  line = "line\r\n";
  EXPECT_TRUE(Util::ChopReturns(&line));
  EXPECT_EQ(line, "line");

  line = "line";
  EXPECT_FALSE(Util::ChopReturns(&line));
  EXPECT_EQ(line, "line");

  line = "line1\nline2\n";
  EXPECT_TRUE(Util::ChopReturns(&line));
  EXPECT_EQ(line, "line1\nline2");

  line = "line\n\n\n";
  EXPECT_TRUE(Util::ChopReturns(&line));
  EXPECT_EQ(line, "line");
}

TEST(UtilTest, ScriptType) {
  EXPECT_TRUE(Util::IsScriptType("くどう", Util::HIRAGANA));
  EXPECT_TRUE(Util::IsScriptType("京都", Util::KANJI));
  // (b/4201140)
  EXPECT_TRUE(Util::IsScriptType("人々", Util::KANJI));
  EXPECT_TRUE(Util::IsScriptType("モズク", Util::KATAKANA));
  EXPECT_TRUE(Util::IsScriptType("モズクﾓｽﾞｸ", Util::KATAKANA));
  EXPECT_TRUE(Util::IsScriptType("ぐーぐる", Util::HIRAGANA));
  EXPECT_TRUE(Util::IsScriptType("グーグル", Util::KATAKANA));
  // U+309F: HIRAGANA DIGRAPH YORI
  EXPECT_TRUE(Util::IsScriptType("ゟ", Util::HIRAGANA));
  // U+30FF: KATAKANA DIGRAPH KOTO
  EXPECT_TRUE(Util::IsScriptType("ヿ", Util::KATAKANA));
  EXPECT_TRUE(Util::IsScriptType("ヷヸヹヺㇰㇱㇲㇳㇴㇵㇶㇷㇸㇹㇺㇻㇼㇽㇾㇿ",
                                 Util::KATAKANA));
  // "𛀀" U+1B000: KATAKANA LETTER ARCHAIC E
  EXPECT_TRUE(Util::IsScriptType("\xF0\x9B\x80\x80", Util::KATAKANA));
  // "𛀁" U+1B001: HIRAGANA LETTER ARCHAIC YE
  EXPECT_TRUE(Util::IsScriptType("\xF0\x9B\x80\x81", Util::HIRAGANA));

  EXPECT_TRUE(Util::IsScriptType("012", Util::NUMBER));
  EXPECT_TRUE(Util::IsScriptType("０１２012", Util::NUMBER));
  EXPECT_TRUE(Util::IsScriptType("abcABC", Util::ALPHABET));
  EXPECT_TRUE(Util::IsScriptType("ＡＢＣＤ", Util::ALPHABET));
  EXPECT_TRUE(Util::IsScriptType("@!#", Util::UNKNOWN_SCRIPT));

  EXPECT_FALSE(Util::IsScriptType("くどカう", Util::HIRAGANA));
  EXPECT_FALSE(Util::IsScriptType("京あ都", Util::KANJI));
  EXPECT_FALSE(Util::IsScriptType("モズあク", Util::KATAKANA));
  EXPECT_FALSE(Util::IsScriptType("モあズクﾓｽﾞｸ", Util::KATAKANA));
  EXPECT_FALSE(Util::IsScriptType("012あ", Util::NUMBER));
  EXPECT_FALSE(Util::IsScriptType("０１２あ012", Util::NUMBER));
  EXPECT_FALSE(Util::IsScriptType("abcABあC", Util::ALPHABET));
  EXPECT_FALSE(Util::IsScriptType("ＡＢあＣＤ", Util::ALPHABET));
  EXPECT_FALSE(Util::IsScriptType("ぐーぐるグ", Util::HIRAGANA));
  EXPECT_FALSE(Util::IsScriptType("グーグルぐ", Util::KATAKANA));

  EXPECT_TRUE(Util::ContainsScriptType("グーグルsuggest", Util::ALPHABET));
  EXPECT_FALSE(Util::ContainsScriptType("グーグルサジェスト", Util::ALPHABET));

  EXPECT_EQ(Util::GetScriptType("くどう"), Util::HIRAGANA);
  EXPECT_EQ(Util::GetScriptType("京都"), Util::KANJI);
  // b/4201140
  EXPECT_EQ(Util::GetScriptType("人々"), Util::KANJI);
  EXPECT_EQ(Util::GetScriptType("モズク"), Util::KATAKANA);
  EXPECT_EQ(Util::GetScriptType("モズクﾓｽﾞｸ"), Util::KATAKANA);
  EXPECT_EQ(Util::GetScriptType("ぐーぐる"), Util::HIRAGANA);
  EXPECT_EQ(Util::GetFirstScriptType("ぐーぐる"), Util::HIRAGANA);

  EXPECT_EQ(Util::GetScriptType("グーグル"), Util::KATAKANA);
  EXPECT_EQ(Util::GetFirstScriptType("グーグル"), Util::KATAKANA);
  // U+309F HIRAGANA DIGRAPH YORI
  EXPECT_EQ(Util::GetScriptType("ゟ"), Util::HIRAGANA);
  EXPECT_EQ(Util::GetFirstScriptType("ゟ"), Util::HIRAGANA);

  // U+30FF KATAKANA DIGRAPH KOTO
  EXPECT_EQ(Util::GetScriptType("ヿ"), Util::KATAKANA);
  EXPECT_EQ(Util::GetScriptType("ヷヸヹヺㇰㇱㇲㇳㇴㇵㇶㇷㇸㇹㇺㇻㇼㇽㇾㇿ"),
            Util::KATAKANA);
  // "𛀀" U+1B000 KATAKANA LETTER ARCHAIC E
  EXPECT_EQ(Util::GetScriptType("\xF0\x9B\x80\x80"), Util::KATAKANA);
  // "𛀁" U+1B001 HIRAGANA LETTER ARCHAIC YE
  EXPECT_EQ(Util::GetScriptType("\xF0\x9B\x80\x81"), Util::HIRAGANA);

  EXPECT_EQ(Util::GetScriptType("!グーグル"), Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetScriptType("ー"), Util::UNKNOWN_SCRIPT);    // U+30FC
  EXPECT_EQ(Util::GetFirstScriptType("ー"), Util::KATAKANA);     // U+30FC
  EXPECT_EQ(Util::GetScriptType("ーー"), Util::UNKNOWN_SCRIPT);  // U+30FC * 2
  EXPECT_EQ(Util::GetFirstScriptType("ーー"), Util::KATAKANA);   // U+30FC * 2
  EXPECT_EQ(Util::GetScriptType("゛"), Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetScriptType("゜"), Util::UNKNOWN_SCRIPT);

  EXPECT_EQ(Util::GetScriptType("012"), Util::NUMBER);
  EXPECT_EQ(Util::GetScriptType("０１２012"), Util::NUMBER);
  EXPECT_EQ(Util::GetScriptType("abcABC"), Util::ALPHABET);
  EXPECT_EQ(Util::GetScriptType("ＡＢＣＤ"), Util::ALPHABET);
  EXPECT_EQ(Util::GetScriptType("@!#"), Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetScriptType("＠！＃"), Util::UNKNOWN_SCRIPT);

  EXPECT_EQ(Util::GetScriptType("ーひらがな"), Util::HIRAGANA);
  EXPECT_EQ(Util::GetFirstScriptType("ーひらがな"), Util::KATAKANA);
  EXPECT_EQ(Util::GetScriptType("ーカタカナ"), Util::KATAKANA);
  EXPECT_EQ(Util::GetScriptType("ｰｶﾀｶﾅ"), Util::KATAKANA);
  EXPECT_EQ(Util::GetScriptType("ひらがなー"), Util::HIRAGANA);
  EXPECT_EQ(Util::GetScriptType("カタカナー"), Util::KATAKANA);
  EXPECT_EQ(Util::GetScriptType("ｶﾀｶﾅｰ"), Util::KATAKANA);

  EXPECT_EQ(Util::GetScriptType("あ゛っ"), Util::HIRAGANA);
  EXPECT_EQ(Util::GetScriptType("あ゜っ"), Util::HIRAGANA);
  EXPECT_EQ(Util::GetScriptType("ア゛ッ"), Util::KATAKANA);
  EXPECT_EQ(Util::GetScriptType("ア゜ッ"), Util::KATAKANA);

  EXPECT_EQ(Util::GetScriptType("くどカう"), Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetScriptType("京あ都"), Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetFirstScriptType("京あ都"), Util::KANJI);

  EXPECT_EQ(Util::GetScriptType("モズあク"), Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetFirstScriptType("モズあク"), Util::KATAKANA);

  // Numbers with period
  EXPECT_EQ(Util::GetScriptType("25.4"), Util::NUMBER);
  EXPECT_EQ(Util::GetScriptType("."), Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetScriptType(".123"), Util::UNKNOWN_SCRIPT);
  // The following are limitations of the current implementation.
  // It's nice to be treated as UNKNOWN_SCRIPT.
  EXPECT_EQ(Util::GetScriptType("1..9"), Util::NUMBER);
  EXPECT_EQ(Util::GetScriptType("42."), Util::NUMBER);

  EXPECT_EQ(Util::GetScriptType("モあズクﾓｽﾞｸ"), Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetScriptType("012あ"), Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetFirstScriptType("012あ"), Util::NUMBER);
  EXPECT_EQ(Util::GetScriptType("０１２あ012"), Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetFirstScriptType("０１２あ012"), Util::NUMBER);
  EXPECT_EQ(Util::GetScriptType("abcABあC"), Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetScriptType("ＡＢあＣＤ"), Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetScriptType("ぐーぐるグ"), Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetScriptType("グーグルぐ"), Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetScriptType("@012"), Util::UNKNOWN_SCRIPT);
  // ・ (U+30FB, KATAKANA MIDDLE DOT) + numbers
  EXPECT_EQ(Util::GetScriptType("・012"), Util::UNKNOWN_SCRIPT);
  // ー (U+30FC, KATAKANA-HIRAGANA PROLONGED SOUND MARK) + numbers
  EXPECT_EQ(Util::GetScriptType("ー012"), Util::UNKNOWN_SCRIPT);

  // "龦" U+9FA6
  EXPECT_EQ(Util::GetScriptType("\xE9\xBE\xA6"), Util::KANJI);
  // "龻" U+9FBB
  EXPECT_EQ(Util::GetScriptType("\xE9\xBE\xBB"), Util::KANJI);
  // U+9FFF is not assigned yet but reserved for CJK Unified Ideographs.
  EXPECT_EQ(Util::GetScriptType("\xE9\xBF\xBF"), Util::KANJI);
  // "𠮟咤" U+20B9F U+54A4
  EXPECT_EQ(Util::GetScriptType("\xF0\xA0\xAE\x9F\xE5\x92\xA4"), Util::KANJI);
  // "𠮷野" U+20BB7 U+91CE
  EXPECT_EQ(Util::GetScriptType("\xF0\xA0\xAE\xB7\xE9\x87\x8E"), Util::KANJI);
  // "巽" U+2F884
  EXPECT_EQ(Util::GetScriptType("\xF0\xAF\xA2\x84"), Util::KANJI);

  // U+1F466, BOY/smile emoji
  EXPECT_EQ(Util::GetScriptType("\xF0\x9F\x91\xA6"), Util::EMOJI);
  // U+FE003, Snow-man Android PUA emoji
  // Historically, Android PUA Emoji was treated as EMOJI. However, because Mozc
  // does not support Android PUA Emoji, GetScriptType for Android PUA Emoji
  // just returns UNKNOWN_SCRIPT now.
  EXPECT_EQ(Util::GetScriptType("\xf3\xbe\x80\x83"), Util::UNKNOWN_SCRIPT);
}

TEST(UtilTest, ScriptTypeWithoutSymbols) {
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("くど う"), Util::HIRAGANA);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("京 都"), Util::KANJI);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("モズク"), Util::KATAKANA);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("モズ クﾓｽﾞｸ"), Util::KATAKANA);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("Google Earth"), Util::ALPHABET);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("Google "), Util::ALPHABET);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols(" Google"), Util::ALPHABET);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols(" Google "), Util::ALPHABET);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("     g"), Util::ALPHABET);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols(""), Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols(" "), Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("   "), Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("Hello!"), Util::ALPHABET);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("Hello!あ"),
            Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("CD-ROM"), Util::ALPHABET);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("CD-ROMア"),
            Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("-"), Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("-A"), Util::ALPHABET);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("--A"), Util::ALPHABET);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("--A---"), Util::ALPHABET);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("--A-ｱ-"), Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("!"), Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("・あ"), Util::HIRAGANA);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("・・あ"), Util::HIRAGANA);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("コギト・エルゴ・スム"),
            Util::KATAKANA);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("コギト・エルゴ・住む"),
            Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("人☆名"), Util::KANJI);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("ひとの☆なまえ"), Util::HIRAGANA);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("超☆最高です"),
            Util::UNKNOWN_SCRIPT);
  EXPECT_EQ(Util::GetScriptTypeWithoutSymbols("・--☆"), Util::UNKNOWN_SCRIPT);
}

TEST(UtilTest, FormType) {
  EXPECT_EQ(Util::GetFormType("くどう"), Util::FULL_WIDTH);
  EXPECT_EQ(Util::GetFormType("京都"), Util::FULL_WIDTH);
  EXPECT_EQ(Util::GetFormType("モズク"), Util::FULL_WIDTH);
  EXPECT_EQ(Util::GetFormType("ﾓｽﾞｸ"), Util::HALF_WIDTH);
  EXPECT_EQ(Util::GetFormType("ぐーぐる"), Util::FULL_WIDTH);
  EXPECT_EQ(Util::GetFormType("グーグル"), Util::FULL_WIDTH);
  EXPECT_EQ(Util::GetFormType("ｸﾞｰｸﾞﾙ"), Util::HALF_WIDTH);
  EXPECT_EQ(Util::GetFormType("ｰ"), Util::HALF_WIDTH);
  EXPECT_EQ(Util::GetFormType("ー"), Util::FULL_WIDTH);
  EXPECT_EQ(Util::GetFormType("¢£¥¦¬¯"), Util::HALF_WIDTH);
  // "￨￩￪￫￬￭￮"
  EXPECT_EQ(Util::GetFormType("\xEF\xBF\xA8\xEF\xBF\xA9\xEF\xBF\xAA\xEF\xBF\xAB"
                              "\xEF\xBF\xAC\xEF\xBF\xAD\xEF\xBF\xAE"),
            Util::HALF_WIDTH);

  // Half-width mathematical symbols
  // [U+27E6, U+27ED], U+2985, and U+2986
  EXPECT_EQ(Util::GetFormType("⟦⟧⟨⟩⟪⟫⟬⟭⦅⦆"), Util::HALF_WIDTH);

  // Half-width hangul "ﾠﾡﾢ"
  EXPECT_EQ(Util::GetFormType("\xEF\xBE\xA0\xEF\xBE\xA1\xEF\xBE\xA2"),
            Util::HALF_WIDTH);

  // Half-width won "₩"
  EXPECT_EQ(Util::GetFormType("₩"), Util::HALF_WIDTH);

  EXPECT_EQ(Util::GetFormType("012"), Util::HALF_WIDTH);
  EXPECT_EQ(Util::GetFormType("０１２012"), Util::UNKNOWN_FORM);
  EXPECT_EQ(Util::GetFormType("abcABC"), Util::HALF_WIDTH);
  EXPECT_EQ(Util::GetFormType("ＡＢＣＤ"), Util::FULL_WIDTH);
  EXPECT_EQ(Util::GetFormType("@!#"), Util::HALF_WIDTH);
}

TEST(UtilTest, IsAscii) {
  EXPECT_FALSE(Util::IsAscii("あいうえお"));
  EXPECT_TRUE(Util::IsAscii("abc"));
  EXPECT_FALSE(Util::IsAscii("abcあいう"));
  EXPECT_TRUE(Util::IsAscii(""));
  EXPECT_TRUE(Util::IsAscii("\x7F"));
  EXPECT_FALSE(Util::IsAscii("\x80"));
}

TEST(UtilTest, IsKanaSymbolContained) {
  const std::string kFullstop("。");
  const std::string kSpace(" ");
  EXPECT_TRUE(Util::IsKanaSymbolContained(kFullstop));
  EXPECT_TRUE(Util::IsKanaSymbolContained(kSpace + kFullstop));
  EXPECT_TRUE(Util::IsKanaSymbolContained(kFullstop + kSpace));
  EXPECT_FALSE(Util::IsKanaSymbolContained(kSpace));
  EXPECT_FALSE(Util::IsKanaSymbolContained(""));
}

TEST(UtilTest, SplitFirstChar32) {
  absl::string_view rest;
  char32_t c = 0;

  rest = absl::string_view();
  c = 0;
  EXPECT_FALSE(Util::SplitFirstChar32("", &c, &rest));
  EXPECT_EQ(c, 0);
  EXPECT_TRUE(rest.empty());

  // Allow nullptr to ignore the matched value.
  rest = absl::string_view();
  EXPECT_TRUE(Util::SplitFirstChar32("01", nullptr, &rest));
  EXPECT_EQ(rest, "1");

  // Allow nullptr to ignore the matched value.
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("01", &c, nullptr));
  EXPECT_EQ(c, '0');

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\x01 ", &c, &rest));
  EXPECT_EQ(c, 1);
  EXPECT_EQ(rest, " ");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\x7F ", &c, &rest));
  EXPECT_EQ(c, 0x7F);
  EXPECT_EQ(rest, " ");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\xC2\x80 ", &c, &rest));
  EXPECT_EQ(c, 0x80);
  EXPECT_EQ(rest, " ");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\xDF\xBF ", &c, &rest));
  EXPECT_EQ(c, 0x7FF);
  EXPECT_EQ(rest, " ");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\xE0\xA0\x80 ", &c, &rest));
  EXPECT_EQ(c, 0x800);
  EXPECT_EQ(rest, " ");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\xEF\xBF\xBF ", &c, &rest));
  EXPECT_EQ(c, 0xFFFF);
  EXPECT_EQ(rest, " ");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\xF0\x90\x80\x80 ", &c, &rest));
  EXPECT_EQ(c, 0x10000);
  EXPECT_EQ(rest, " ");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\xF7\xBF\xBF\xBF ", &c, &rest));
  EXPECT_EQ(c, 0x1FFFFF);
  EXPECT_EQ(rest, " ");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\xF8\x88\x80\x80\x80 ", &c, &rest));
  EXPECT_EQ(c, 0x200000);
  EXPECT_EQ(rest, " ");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\xFB\xBF\xBF\xBF\xBF ", &c, &rest));
  EXPECT_EQ(c, 0x3FFFFFF);
  EXPECT_EQ(rest, " ");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\xFC\x84\x80\x80\x80\x80 ", &c, &rest));
  EXPECT_EQ(c, 0x4000000);
  EXPECT_EQ(rest, " ");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\xFD\xBF\xBF\xBF\xBF\xBF ", &c, &rest));
  EXPECT_EQ(c, 0x7FFFFFFF);
  EXPECT_EQ(rest, " ");

  // If there is any invalid sequence, the entire text should be treated as
  // am empty string.
  {
    c = 0;
    EXPECT_FALSE(Util::SplitFirstChar32("\xC2 ", &c, &rest));
    EXPECT_EQ(c, 0);

    c = 0;
    EXPECT_FALSE(Util::SplitFirstChar32("\xC2\xC2 ", &c, &rest));
    EXPECT_EQ(c, 0);

    c = 0;
    EXPECT_FALSE(Util::SplitFirstChar32("\xE0 ", &c, &rest));
    EXPECT_EQ(c, 0);

    c = 0;
    EXPECT_FALSE(Util::SplitFirstChar32("\xE0\xE0\xE0 ", &c, &rest));
    EXPECT_EQ(c, 0);

    c = 0;
    EXPECT_FALSE(Util::SplitFirstChar32("\xF0 ", &c, &rest));
    EXPECT_EQ(c, 0);

    c = 0;
    EXPECT_FALSE(Util::SplitFirstChar32("\xF0\xF0\xF0\xF0 ", &c, &rest));
    EXPECT_EQ(c, 0);
  }

  // BOM should be treated as invalid byte.
  {
    c = 0;
    EXPECT_FALSE(Util::SplitFirstChar32("\xFF ", &c, &rest));
    EXPECT_EQ(c, 0);

    c = 0;
    EXPECT_FALSE(Util::SplitFirstChar32("\xFE ", &c, &rest));
    EXPECT_EQ(c, 0);
  }

  // Invalid sequence for U+002F (redundant encoding)
  {
    c = 0;
    EXPECT_FALSE(Util::SplitFirstChar32("\xC0\xAF", &c, &rest));
    EXPECT_EQ(c, 0);

    c = 0;
    EXPECT_FALSE(Util::SplitFirstChar32("\xE0\x80\xAF", &c, &rest));
    EXPECT_EQ(c, 0);

    c = 0;
    EXPECT_FALSE(Util::SplitFirstChar32("\xF0\x80\x80\xAF", &c, &rest));
    EXPECT_EQ(c, 0);
  }
}

TEST(UtilTest, SplitLastChar32) {
  absl::string_view rest;
  char32_t c = 0;

  rest = absl::string_view();
  c = 0;
  EXPECT_FALSE(Util::SplitLastChar32("", &rest, &c));
  EXPECT_EQ(c, 0);
  EXPECT_TRUE(rest.empty());

  // Allow nullptr to ignore the matched value.
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32("01", nullptr, &c));
  EXPECT_EQ(c, '1');

  // Allow nullptr to ignore the matched value.
  rest = absl::string_view();
  EXPECT_TRUE(Util::SplitLastChar32("01", &rest, nullptr));
  EXPECT_EQ(rest, "0");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \x01", &rest, &c));
  EXPECT_EQ(c, 1);
  EXPECT_EQ(rest, " ");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \x7F", &rest, &c));
  EXPECT_EQ(c, 0x7F);
  EXPECT_EQ(rest, " ");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \xC2\x80", &rest, &c));
  EXPECT_EQ(c, 0x80);
  EXPECT_EQ(rest, " ");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \xDF\xBF", &rest, &c));
  EXPECT_EQ(c, 0x7FF);
  EXPECT_EQ(rest, " ");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \xE0\xA0\x80", &rest, &c));
  EXPECT_EQ(c, 0x800);
  EXPECT_EQ(rest, " ");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \xEF\xBF\xBF", &rest, &c));
  EXPECT_EQ(c, 0xFFFF);
  EXPECT_EQ(rest, " ");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \xF0\x90\x80\x80", &rest, &c));
  EXPECT_EQ(c, 0x10000);
  EXPECT_EQ(rest, " ");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \xF7\xBF\xBF\xBF", &rest, &c));
  EXPECT_EQ(c, 0x1FFFFF);
  EXPECT_EQ(rest, " ");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \xF8\x88\x80\x80\x80", &rest, &c));
  EXPECT_EQ(c, 0x200000);
  EXPECT_EQ(rest, " ");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \xFB\xBF\xBF\xBF\xBF", &rest, &c));
  EXPECT_EQ(c, 0x3FFFFFF);
  EXPECT_EQ(rest, " ");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \xFC\x84\x80\x80\x80\x80", &rest, &c));
  EXPECT_EQ(c, 0x4000000);
  EXPECT_EQ(rest, " ");

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \xFD\xBF\xBF\xBF\xBF\xBF", &rest, &c));
  EXPECT_EQ(c, 0x7FFFFFFF);
  EXPECT_EQ(rest, " ");

  // If there is any invalid sequence, the entire text should be treated as
  // am empty string.
  {
    c = 0;
    EXPECT_FALSE(Util::SplitLastChar32(" \xC2", &rest, &c));
    EXPECT_EQ(c, 0);

    c = 0;
    EXPECT_FALSE(Util::SplitLastChar32(" \xC2\xC2", &rest, &c));
    EXPECT_EQ(c, 0);

    c = 0;
    EXPECT_FALSE(Util::SplitLastChar32(" \xE0", &rest, &c));
    EXPECT_EQ(c, 0);

    c = 0;
    EXPECT_FALSE(Util::SplitLastChar32(" \xE0\xE0\xE0", &rest, &c));
    EXPECT_EQ(c, 0);

    c = 0;
    EXPECT_FALSE(Util::SplitLastChar32(" \xF0", &rest, &c));
    EXPECT_EQ(c, 0);

    c = 0;
    EXPECT_FALSE(Util::SplitLastChar32(" \xF0\xF0\xF0\xF0", &rest, &c));
    EXPECT_EQ(c, 0);
  }

  // BOM should be treated as invalid byte.
  {
    c = 0;
    EXPECT_FALSE(Util::SplitLastChar32(" \xFF", &rest, &c));
    EXPECT_EQ(c, 0);

    c = 0;
    EXPECT_FALSE(Util::SplitLastChar32(" \xFE", &rest, &c));
    EXPECT_EQ(c, 0);
  }

  // Invalid sequence for U+002F (redundant encoding)
  {
    c = 0;
    EXPECT_FALSE(Util::SplitLastChar32("\xC0\xAF", &rest, &c));
    EXPECT_EQ(c, 0);

    c = 0;
    EXPECT_FALSE(Util::SplitLastChar32("\xE0\x80\xAF", &rest, &c));
    EXPECT_EQ(c, 0);

    c = 0;
    EXPECT_FALSE(Util::SplitLastChar32("\xF0\x80\x80\xAF", &rest, &c));
    EXPECT_EQ(c, 0);
  }
}

TEST(UtilTest, IsValidUtf8) {
  EXPECT_TRUE(Util::IsValidUtf8(""));
  EXPECT_TRUE(Util::IsValidUtf8("abc"));
  EXPECT_TRUE(Util::IsValidUtf8("あいう"));
  EXPECT_TRUE(Util::IsValidUtf8("aあbいcう"));

  EXPECT_FALSE(Util::IsValidUtf8("\xC2 "));
  EXPECT_FALSE(Util::IsValidUtf8("\xC2\xC2 "));
  EXPECT_FALSE(Util::IsValidUtf8("\xE0 "));
  EXPECT_FALSE(Util::IsValidUtf8("\xE0\xE0\xE0 "));
  EXPECT_FALSE(Util::IsValidUtf8("\xF0 "));
  EXPECT_FALSE(Util::IsValidUtf8("\xF0\xF0\xF0\xF0 "));

  // BOM should be treated as invalid byte.
  EXPECT_FALSE(Util::IsValidUtf8("\xFF "));
  EXPECT_FALSE(Util::IsValidUtf8("\xFE "));

  // Redundant encoding with U+002F is invalid.
  EXPECT_FALSE(Util::IsValidUtf8("\xC0\xAF"));
  EXPECT_FALSE(Util::IsValidUtf8("\xE0\x80\xAF"));
  EXPECT_FALSE(Util::IsValidUtf8("\xF0\x80\x80\xAF"));
}

TEST(UtilTest, IsAcceptableCharacterAsCandidate) {
  // Control Characters
  EXPECT_FALSE(Util::IsAcceptableCharacterAsCandidate('\n'));
  EXPECT_FALSE(Util::IsAcceptableCharacterAsCandidate('\t'));
  EXPECT_FALSE(Util::IsAcceptableCharacterAsCandidate('\r'));

  // Bidirectional text controls
  // See: https://en.wikipedia.org/wiki/Bidirectional_text
  EXPECT_FALSE(Util::IsAcceptableCharacterAsCandidate(0x200E));  // LRM
  EXPECT_FALSE(Util::IsAcceptableCharacterAsCandidate(0x200F));  // RLM
  EXPECT_FALSE(Util::IsAcceptableCharacterAsCandidate(0x2069));  // PDI
  EXPECT_FALSE(Util::IsAcceptableCharacterAsCandidate(0x061C));  // ALM

  // For normal letters, it should be false
  EXPECT_TRUE(Util::IsAcceptableCharacterAsCandidate(u'あ'));
  EXPECT_TRUE(Util::IsAcceptableCharacterAsCandidate(u'↘'));
  EXPECT_TRUE(Util::IsAcceptableCharacterAsCandidate(u'永'));
  EXPECT_TRUE(Util::IsAcceptableCharacterAsCandidate(u'⊿'));
  EXPECT_TRUE(Util::IsAcceptableCharacterAsCandidate(u'a'));
  EXPECT_TRUE(
      Util::IsAcceptableCharacterAsCandidate(0x1B001));  // hentaigana ye
  EXPECT_TRUE(Util::IsAcceptableCharacterAsCandidate(0x1F600));  // emoji
}

TEST(UtilTest, SerializeAndDeserializeUint64) {
  struct {
    const char* str;
    uint64_t value;
  } kCorrectPairs[] = {
      {"\x00\x00\x00\x00\x00\x00\x00\x00", 0},
      {"\x00\x00\x00\x00\x00\x00\x00\xFF", std::numeric_limits<uint8_t>::max()},
      {"\x00\x00\x00\x00\x00\x00\xFF\xFF",
       std::numeric_limits<uint16_t>::max()},
      {"\x00\x00\x00\x00\xFF\xFF\xFF\xFF",
       std::numeric_limits<uint32_t>::max()},
      {"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF",
       std::numeric_limits<uint64_t>::max()},
      {"\x01\x23\x45\x67\x89\xAB\xCD\xEF", 0x0123456789ABCDEF},
      {"\xFE\xDC\xBA\x98\x76\x54\x32\x10", 0xFEDCBA9876543210},
  };

  for (size_t i = 0; i < std::size(kCorrectPairs); ++i) {
    const std::string serialized(kCorrectPairs[i].str, 8);
    EXPECT_EQ(Util::SerializeUint64(kCorrectPairs[i].value), serialized);

    uint64_t v;
    EXPECT_TRUE(Util::DeserializeUint64(serialized, &v));
    EXPECT_EQ(v, kCorrectPairs[i].value);
  }

  // Invalid patterns for DeserializeUint64.
  const char* kFalseCases[] = {
      "",
      "abc",
      "helloworld",
  };
  for (size_t i = 0; i < std::size(kFalseCases); ++i) {
    uint64_t v;
    EXPECT_FALSE(Util::DeserializeUint64(kFalseCases[i], &v));
  }
}

}  // namespace
}  // namespace mozc
