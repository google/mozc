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

#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/number_util.h"
#include "base/port.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/mozctest.h"
#include "absl/strings/match.h"
#include "absl/strings/str_join.h"

namespace mozc {

using ::testing::ElementsAreArray;

TEST(UtilTest, AppendStringWithDelimiter) {
  std::string result;
  std::string input;
  constexpr char kDelemiter[] = ":";

  {
    result.clear();
    Util::AppendStringWithDelimiter(kDelemiter, "test", &result);
    EXPECT_EQ("test", result);
  }

  {
    result = "foo";
    Util::AppendStringWithDelimiter(kDelemiter, "test", &result);
    EXPECT_EQ("foo:test", result);
  }

  {
    result = "foo";
    Util::AppendStringWithDelimiter(kDelemiter, "", &result);
    EXPECT_EQ("foo:", result);
  }
}

TEST(UtilTest, SplitIterator_SingleDelimiter_SkipEmpty) {
  typedef SplitIterator<SingleDelimiter, SkipEmpty> SplitIterator;
  {
    SplitIterator iter("", " ");
    EXPECT_TRUE(iter.Done());
  }
  {
    SplitIterator iter(absl::string_view(), " ");
    EXPECT_TRUE(iter.Done());
  }
  {
    const char *s = "a b cde";
    SplitIterator iter(s, " ");
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("a", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("b", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("cde", iter.Get());
    iter.Next();
    EXPECT_TRUE(iter.Done());
  }
  {
    const char *s = " a b  cde ";
    SplitIterator iter(s, " ");
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("a", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("b", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("cde", iter.Get());
    iter.Next();
    EXPECT_TRUE(iter.Done());
  }
  {
    absl::string_view s("a b  cde ", 5);
    SplitIterator iter(s, " ");
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("a", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("b", iter.Get());
    iter.Next();
    EXPECT_TRUE(iter.Done());
  }
}

TEST(UtilTest, SplitIterator_MultiDelimiter_SkipEmpty) {
  typedef SplitIterator<MultiDelimiter, SkipEmpty> SplitIterator;
  {
    SplitIterator iter("", " \t,");
    EXPECT_TRUE(iter.Done());
  }
  {
    SplitIterator iter(absl::string_view(), ",.");
    EXPECT_TRUE(iter.Done());
  }
  {
    const char *s = "a b\tcde:fg";
    SplitIterator iter(s, " \t:");
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("a", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("b", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("cde", iter.Get());
    EXPECT_FALSE(iter.Done());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("fg", iter.Get());
    iter.Next();
    EXPECT_TRUE(iter.Done());
  }
  {
    const char *s = "  \t:a b\t\tcde:fg:";
    SplitIterator iter(s, " \t:");
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("a", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("b", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("cde", iter.Get());
    EXPECT_FALSE(iter.Done());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("fg", iter.Get());
    iter.Next();
    EXPECT_TRUE(iter.Done());
  }
}

TEST(UtilTest, SplitIterator_SingleDelimiter_AllowEmpty) {
  typedef SplitIterator<SingleDelimiter, AllowEmpty> SplitIterator;
  {
    SplitIterator iter("", " ");
    EXPECT_TRUE(iter.Done());
  }
  {
    SplitIterator iter(absl::string_view(), " ");
    EXPECT_TRUE(iter.Done());
  }
  {
    const char *s = "a b cde";
    SplitIterator iter(s, " ");
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("a", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("b", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("cde", iter.Get());
    iter.Next();
    EXPECT_TRUE(iter.Done());
  }
  {
    const char *s = " a b  cde ";
    SplitIterator iter(s, " ");
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("a", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("b", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("cde", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("", iter.Get());
    iter.Next();
    EXPECT_TRUE(iter.Done());
  }
  {
    absl::string_view s("a b  cde ", 5);
    SplitIterator iter(s, " ");
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("a", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("b", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("", iter.Get());
    iter.Next();
    EXPECT_TRUE(iter.Done());
  }
}

TEST(UtilTest, SplitIterator_MultiDelimiter_AllowEmpty) {
  typedef SplitIterator<MultiDelimiter, AllowEmpty> SplitIterator;
  {
    SplitIterator iter("", " \t,");
    EXPECT_TRUE(iter.Done());
  }
  {
    SplitIterator iter(absl::string_view(), ",.");
    EXPECT_TRUE(iter.Done());
  }
  {
    const char *s = "a b\tcde:fg";
    SplitIterator iter(s, " \t:");
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("a", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("b", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("cde", iter.Get());
    EXPECT_FALSE(iter.Done());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("fg", iter.Get());
    iter.Next();
    EXPECT_TRUE(iter.Done());
  }
  {
    const char *s = "a b\t\tcde:fg:";
    SplitIterator iter(s, " \t:");
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("a", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("b", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("cde", iter.Get());
    EXPECT_FALSE(iter.Done());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("fg", iter.Get());
    iter.Next();
    EXPECT_FALSE(iter.Done());
    EXPECT_EQ("", iter.Get());
    iter.Next();
    EXPECT_TRUE(iter.Done());
  }
}

TEST(UtilTest, StripWhiteSpaces) {
  // basic scenario.
  {
    const std::string input = "  foo   ";
    std::string output;
    Util::StripWhiteSpaces(input, &output);
    EXPECT_EQ("foo", output);
  }

  // no space means just copy.
  {
    const std::string input = "foo";
    std::string output;
    Util::StripWhiteSpaces(input, &output);
    EXPECT_EQ("foo", output);
  }

  // tabs and linebreaks are also spaces.
  {
    const std::string input = " \tfoo\n";
    std::string output;
    Util::StripWhiteSpaces(input, &output);
    EXPECT_EQ("foo", output);
  }

  // spaces in the middle remains.
  {
    const std::string input = " foo bar baz ";
    std::string output;
    Util::StripWhiteSpaces(input, &output);
    EXPECT_EQ("foo bar baz", output);
  }

  // all spaces means clear out output.
  {
    const std::string input = " \v \r ";
    std::string output;
    Util::StripWhiteSpaces(input, &output);
    EXPECT_TRUE(output.empty());
  }

  // empty input.
  {
    const std::string input = "";
    std::string output;
    Util::StripWhiteSpaces(input, &output);
    EXPECT_TRUE(output.empty());
  }

  // one character.
  {
    const std::string input = "a";
    std::string output;
    Util::StripWhiteSpaces(input, &output);
    EXPECT_EQ("a", output);
  }
}

TEST(UtilTest, SplitStringToUtf8Chars) {
  {
    std::vector<std::string> output;
    Util::SplitStringToUtf8Chars("", &output);
    EXPECT_EQ(0, output.size());
  }

  {
    const std::string kInputs[] = {
        "a", "あ", "亜", "\n", "a",
    };
    std::string joined_string;
    for (int i = 0; i < std::size(kInputs); ++i) {
      joined_string += kInputs[i];
    }

    std::vector<std::string> output;
    Util::SplitStringToUtf8Chars(joined_string, &output);
    EXPECT_EQ(std::size(kInputs), output.size());

    for (size_t i = 0; i < output.size(); ++i) {
      EXPECT_EQ(kInputs[i], output[i]);
    }
  }
}

TEST(UtilTest, SplitStringToUtf8Graphemes) {
  {
    std::vector<std::string> output;
    Util::SplitStringToUtf8Chars("", &output);
    EXPECT_EQ(0, output.size());
  }

  {  // Single codepoint characters.
    const std::string kInputs[] = {
        "a", "あ", "亜", "\n", "a",
    };
    std::string joined_string = absl::StrJoin(kInputs, "");

    std::vector<std::string> graphemes;
    Util::SplitStringToUtf8Graphemes(joined_string, &graphemes);
    EXPECT_EQ(std::size(kInputs), graphemes.size());

    for (size_t i = 0; i < graphemes.size(); ++i) {
      EXPECT_EQ(kInputs[i], graphemes[i]);
    }
  }

  {  // Multiple codepoint characters
    const std::string kInputs[] = {
        "神",  // U+795E
        "神︀",  // U+795E,U+FE00  - 2 codepoints [SVS]
        "神󠄀",  // U+795E,U+E0100 - 2 codepoints [IVS]
        "あ゙",  // U+3042,U+3099  - 2 codepoints [Dakuten]
        "か゚",  // U+304B,U+309A  - 2 codepoints [Handakuten]
    };
    std::string joined_string = absl::StrJoin(kInputs, "");

    std::vector<std::string> graphemes;
    Util::SplitStringToUtf8Graphemes(joined_string, &graphemes);
    EXPECT_EQ(std::size(kInputs), graphemes.size());

    for (size_t i = 0; i < graphemes.size(); ++i) {
      EXPECT_EQ(kInputs[i], graphemes[i]);
    }
  }

  {  // Invalid codepoint characters
    const std::string kInputs[] = {
        u8"\uFE00",  // Extend only [SVS]
        u8"神\uFE00\U000E0100",  // Multiple extends [SVS, IVS]
    };
    std::string joined_string;
    for (int i = 0; i < std::size(kInputs); ++i) {
      joined_string += kInputs[i];
    }

    std::vector<std::string> graphemes;
    Util::SplitStringToUtf8Graphemes(joined_string, &graphemes);
    EXPECT_EQ(std::size(kInputs), graphemes.size());

    for (size_t i = 0; i < graphemes.size(); ++i) {
      EXPECT_EQ(kInputs[i], graphemes[i]);
    }
  }
}

TEST(UtilTest, SplitCSV) {
  std::vector<std::string> answer_vector;

  Util::SplitCSV("Google,x,\"Buchheit, Paul\",\"string with \"\" quote in it\"",
                 &answer_vector);
  CHECK_EQ(answer_vector.size(), 4);
  CHECK_EQ(answer_vector[0], "Google");
  CHECK_EQ(answer_vector[1], "x");
  CHECK_EQ(answer_vector[2], "Buchheit, Paul");
  CHECK_EQ(answer_vector[3], "string with \" quote in it");

  Util::SplitCSV("Google,hello,", &answer_vector);
  CHECK_EQ(answer_vector.size(), 3);
  CHECK_EQ(answer_vector[0], "Google");
  CHECK_EQ(answer_vector[1], "hello");
  CHECK_EQ(answer_vector[2], "");

  Util::SplitCSV("Google rocks,hello", &answer_vector);
  CHECK_EQ(answer_vector.size(), 2);
  CHECK_EQ(answer_vector[0], "Google rocks");
  CHECK_EQ(answer_vector[1], "hello");

  Util::SplitCSV(",,\"\",,", &answer_vector);
  CHECK_EQ(answer_vector.size(), 5);
  CHECK_EQ(answer_vector[0], "");
  CHECK_EQ(answer_vector[1], "");
  CHECK_EQ(answer_vector[2], "");
  CHECK_EQ(answer_vector[3], "");
  CHECK_EQ(answer_vector[4], "");

  // Test a string containing a comma.
  Util::SplitCSV("\",\",hello", &answer_vector);
  CHECK_EQ(answer_vector.size(), 2);
  CHECK_EQ(answer_vector[0], ",");
  CHECK_EQ(answer_vector[1], "hello");

  // Invalid CSV
  Util::SplitCSV("\"no,last,quote", &answer_vector);
  CHECK_EQ(answer_vector.size(), 1);
  CHECK_EQ(answer_vector[0], "no,last,quote");

  Util::SplitCSV("backslash\\,is,no,an,\"escape\"", &answer_vector);
  CHECK_EQ(answer_vector.size(), 5);
  CHECK_EQ(answer_vector[0], "backslash\\");
  CHECK_EQ(answer_vector[1], "is");
  CHECK_EQ(answer_vector[2], "no");
  CHECK_EQ(answer_vector[3], "an");
  CHECK_EQ(answer_vector[4], "escape");

  Util::SplitCSV("", &answer_vector);
  CHECK_EQ(answer_vector.size(), 0);
}

TEST(UtilTest, ReplaceString) {
  const std::string input = "foobarfoobar";
  std::string output;
  Util::StringReplace(input, "bar", "buz", true, &output);
  EXPECT_EQ("foobuzfoobuz", output);

  output.clear();
  Util::StringReplace(input, "bar", "buz", false, &output);
  EXPECT_EQ("foobuzfoobar", output);
}

TEST(UtilTest, LowerString) {
  std::string s = "TeSTtest";
  Util::LowerString(&s);
  EXPECT_EQ("testtest", s);

  std::string s2 = "ＴｅＳＴ＠ＡＢＣＸＹＺ［｀ａｂｃｘｙｚ｛";
  Util::LowerString(&s2);
  EXPECT_EQ("ｔｅｓｔ＠ａｂｃｘｙｚ［｀ａｂｃｘｙｚ｛", s2);
}

TEST(UtilTest, UpperString) {
  std::string s = "TeSTtest";
  Util::UpperString(&s);
  EXPECT_EQ("TESTTEST", s);

  std::string s2 = "ＴｅＳＴ＠ＡＢＣＸＹＺ［｀ａｂｃｘｙｚ｛";
  Util::UpperString(&s2);
  EXPECT_EQ("ＴＥＳＴ＠ＡＢＣＸＹＺ［｀ＡＢＣＸＹＺ｛", s2);
}

TEST(UtilTest, CapitalizeString) {
  std::string s = "TeSTtest";
  Util::CapitalizeString(&s);
  EXPECT_EQ("Testtest", s);

  std::string s2 = "ＴｅＳＴ＠ＡＢＣＸＹＺ［｀ａｂｃｘｙｚ｛";
  Util::CapitalizeString(&s2);
  EXPECT_EQ("Ｔｅｓｔ＠ａｂｃｘｙｚ［｀ａｂｃｘｙｚ｛", s2);
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

TEST(UtilTest, IsLowerOrUpperAscii) {
  EXPECT_TRUE(Util::IsLowerOrUpperAscii(""));
  EXPECT_TRUE(Util::IsLowerOrUpperAscii("hello"));
  EXPECT_TRUE(Util::IsLowerOrUpperAscii("HELLO"));
  EXPECT_FALSE(Util::IsLowerOrUpperAscii("Hello"));
  EXPECT_FALSE(Util::IsLowerOrUpperAscii("HeLlO"));
  EXPECT_FALSE(Util::IsLowerOrUpperAscii("symbol!"));
  EXPECT_FALSE(Util::IsLowerOrUpperAscii("Ｈｅｌｌｏ"));
}

TEST(UtilTest, IsUpperOrCapitalizedAscii) {
  EXPECT_TRUE(Util::IsUpperOrCapitalizedAscii(""));
  EXPECT_FALSE(Util::IsUpperOrCapitalizedAscii("hello"));
  EXPECT_TRUE(Util::IsUpperOrCapitalizedAscii("HELLO"));
  EXPECT_TRUE(Util::IsUpperOrCapitalizedAscii("Hello"));
  EXPECT_FALSE(Util::IsUpperOrCapitalizedAscii("HeLlO"));
  EXPECT_FALSE(Util::IsUpperOrCapitalizedAscii("symbol!"));
  EXPECT_FALSE(Util::IsUpperOrCapitalizedAscii("Ｈｅｌｌｏ"));
}

TEST(UtilTest, Utf8ToCodepoints) {
  {
    const std::vector<char32_t> codepoints = Util::Utf8ToCodepoints("");
    EXPECT_TRUE(codepoints.empty());
  }

  {  // Single codepoint characters.
    std::string str =  "aあ亜\na";
    std::vector<int> expected = {0x0061, 0x3042, 0x4E9C, 0x000A, 0x0061};

    std::vector<char32_t> codepoints = Util::Utf8ToCodepoints(str);
    EXPECT_THAT(codepoints, ElementsAreArray(expected));
  }

  {  // Multiple codepoint characters
    std::string str = (
        "神"  // U+795E
        "神󠄀"  // U+795E,U+E0100 - 2 codepoints [IVS]
        "あ゙"  // U+3042,U+3099  - 2 codepoints [Dakuten]
    );
    std::vector<int> expected = {0x795E, 0x795E, 0xE0100, 0x3042, 0x3099 };

    std::vector<char32_t> codepoints = Util::Utf8ToCodepoints(str);
    EXPECT_THAT(codepoints, ElementsAreArray(expected));
  }
}

TEST(UtilTest, CodepointsToUtf8) {
  {
    const std::string output = Util::CodepointsToUtf8({});
    EXPECT_TRUE(output.empty());
  }

  {  // Single codepoint characters.
    std::string expected =  "aあ亜\na";
    std::vector<char32_t> cps = {0x0061, 0x3042, 0x4E9C, 0x000A, 0x0061};
    EXPECT_EQ(Util::CodepointsToUtf8(cps), expected);
  }

  {  // Multiple codepoint characters
    std::string expected = (
        "神"  // U+795E
        "神󠄀"  // U+795E,U+E0100 - 2 codepoints [IVS]
        "あ゙"  // U+3042,U+3099  - 2 codepoints [Dakuten]
    );
    std::vector<char32_t> cps = {0x795E, 0x795E, 0xE0100, 0x3042, 0x3099};
    EXPECT_EQ(Util::CodepointsToUtf8(cps), expected);
  }
}

void VerifyUtf8ToUcs4(const std::string &text, char32_t expected_ucs4,
                      size_t expected_len) {
  const char *begin = text.data();
  const char *end = begin + text.size();
  size_t mblen = 0;
  char32_t result = Util::Utf8ToUcs4(begin, end, &mblen);
  EXPECT_EQ(expected_ucs4, result) << text << " " << expected_ucs4;
  EXPECT_EQ(expected_len, mblen) << text << " " << expected_len;
}

TEST(UtilTest, Utf8ToUcs4) {
  VerifyUtf8ToUcs4("", 0, 0);
  VerifyUtf8ToUcs4("\x01", 1, 1);
  VerifyUtf8ToUcs4("\x7F", 0x7F, 1);
  VerifyUtf8ToUcs4("\xC2\x80", 0x80, 2);
  VerifyUtf8ToUcs4("\xDF\xBF", 0x7FF, 2);
  VerifyUtf8ToUcs4("\xE0\xA0\x80", 0x800, 3);
  VerifyUtf8ToUcs4("\xEF\xBF\xBF", 0xFFFF, 3);
  VerifyUtf8ToUcs4("\xF0\x90\x80\x80", 0x10000, 4);
  VerifyUtf8ToUcs4("\xF7\xBF\xBF\xBF", 0x1FFFFF, 4);
  // do not test 5-6 bytes because it's out of spec of UTF8.
}

TEST(UtilTest, Ucs4ToUtf8) {
  std::string output;

  // Do nothing if |c| is NUL. Previous implementation of Ucs4ToUtf8 worked like
  // this even though the reason is unclear.
  Util::Ucs4ToUtf8(0, &output);
  EXPECT_TRUE(output.empty());

  Util::Ucs4ToUtf8(0x7F, &output);
  EXPECT_EQ("\x7F", output);
  Util::Ucs4ToUtf8(0x80, &output);
  EXPECT_EQ("\xC2\x80", output);
  Util::Ucs4ToUtf8(0x7FF, &output);
  EXPECT_EQ("\xDF\xBF", output);
  Util::Ucs4ToUtf8(0x800, &output);
  EXPECT_EQ("\xE0\xA0\x80", output);
  Util::Ucs4ToUtf8(0xFFFF, &output);
  EXPECT_EQ("\xEF\xBF\xBF", output);
  Util::Ucs4ToUtf8(0x10000, &output);
  EXPECT_EQ("\xF0\x90\x80\x80", output);
  Util::Ucs4ToUtf8(0x1FFFFF, &output);
  EXPECT_EQ("\xF7\xBF\xBF\xBF", output);

  // Buffer version.
  char buf[7];

  EXPECT_EQ(0, Util::Ucs4ToUtf8(0, buf));
  EXPECT_EQ(0, strcmp(buf, ""));

  EXPECT_EQ(1, Util::Ucs4ToUtf8(0x7F, buf));
  EXPECT_EQ(0, strcmp("\x7F", buf));

  EXPECT_EQ(2, Util::Ucs4ToUtf8(0x80, buf));
  EXPECT_EQ(0, strcmp("\xC2\x80", buf));

  EXPECT_EQ(2, Util::Ucs4ToUtf8(0x7FF, buf));
  EXPECT_EQ(0, strcmp("\xDF\xBF", buf));

  EXPECT_EQ(3, Util::Ucs4ToUtf8(0x800, buf));
  EXPECT_EQ(0, strcmp("\xE0\xA0\x80", buf));

  EXPECT_EQ(3, Util::Ucs4ToUtf8(0xFFFF, buf));
  EXPECT_EQ(0, strcmp("\xEF\xBF\xBF", buf));

  EXPECT_EQ(4, Util::Ucs4ToUtf8(0x10000, buf));
  EXPECT_EQ(0, strcmp("\xF0\x90\x80\x80", buf));

  EXPECT_EQ(4, Util::Ucs4ToUtf8(0x1FFFFF, buf));
  EXPECT_EQ(0, strcmp("\xF7\xBF\xBF\xBF", buf));
}

TEST(UtilTest, CharsLen) {
  const std::string src = "私の名前は中野です";
  EXPECT_EQ(Util::CharsLen(src.c_str(), src.size()), 9);
}

TEST(UtilTest, Utf8SubString) {
  const absl::string_view src = "私の名前は中野です";
  absl::string_view result;

  result = Util::Utf8SubString(src, 0, 2);
  EXPECT_EQ("私の", result);
  // |result|'s data should point to the same memory block as src.
  EXPECT_LE(src.data(), result.data());

  result = Util::Utf8SubString(src, 4, 1);
  EXPECT_EQ("は", result);
  EXPECT_LE(src.data(), result.data());

  result = Util::Utf8SubString(src, 5, 3);
  EXPECT_EQ("中野で", result);
  EXPECT_LE(src.data(), result.data());

  result = Util::Utf8SubString(src, 6, 10);
  EXPECT_EQ("野です", result);
  EXPECT_LE(src.data(), result.data());

  result = Util::Utf8SubString(src, 4, 2);
  EXPECT_EQ("は中", result);
  EXPECT_LE(src.data(), result.data());

  result = Util::Utf8SubString(src, 2, std::string::npos);
  EXPECT_EQ("名前は中野です", result);
  EXPECT_LE(src.data(), result.data());

  result = Util::Utf8SubString(src, 5, std::string::npos);
  EXPECT_EQ("中野です", result);
  EXPECT_LE(src.data(), result.data());
}

TEST(UtilTest, Utf8SubString2) {
  const absl::string_view src = "私はGoogleです";

  absl::string_view result;

  result = Util::Utf8SubString(src, 0);
  EXPECT_EQ(src, result);

  result = Util::Utf8SubString(src, 5);
  EXPECT_EQ("gleです", result);

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
  std::string line;

  // Should be stripped.
  line =
      "\xef\xbb\xbf"
      "abc";
  Util::StripUtf8Bom(&line);
  EXPECT_EQ("abc", line);

  // Should be stripped.
  line = "\xef\xbb\xbf";
  Util::StripUtf8Bom(&line);
  EXPECT_EQ("", line);

  // BOM in the middle of text. Shouldn't be stripped.
  line =
      "a"
      "\xef\xbb\xbf"
      "bc";
  Util::StripUtf8Bom(&line);
  EXPECT_EQ(
      "a"
      "\xef\xbb\xbf"
      "bc",
      line);

  // Incomplete BOM. Shouldn't be stripped.
  line =
      "\xef\xbb"
      "abc";
  Util::StripUtf8Bom(&line);
  EXPECT_EQ(
      "\xef\xbb"
      "abc",
      line);

  // String shorter than the BOM. Do nothing.
  line = "a";
  Util::StripUtf8Bom(&line);
  EXPECT_EQ("a", line);

  // Empty string. Do nothing.
  line = "";
  Util::StripUtf8Bom(&line);
  EXPECT_EQ("", line);
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

TEST(UtilTest, IsAndroidPuaEmoji) {
  EXPECT_FALSE(Util::IsAndroidPuaEmoji(""));
  EXPECT_FALSE(Util::IsAndroidPuaEmoji("A"));
  EXPECT_FALSE(Util::IsAndroidPuaEmoji("a"));

  std::string str;
  Util::Ucs4ToUtf8(0xFDFFF, &str);
  EXPECT_FALSE(Util::IsAndroidPuaEmoji(str));
  Util::Ucs4ToUtf8(0xFE000, &str);
  EXPECT_TRUE(Util::IsAndroidPuaEmoji(str));
  Util::Ucs4ToUtf8(0xFE800, &str);
  EXPECT_TRUE(Util::IsAndroidPuaEmoji(str));
  Util::Ucs4ToUtf8(0xFEEA0, &str);
  EXPECT_TRUE(Util::IsAndroidPuaEmoji(str));
  Util::Ucs4ToUtf8(0xFEEA1, &str);
  EXPECT_FALSE(Util::IsAndroidPuaEmoji(str));

  // If it has two ucs4 chars (or more), just expect false.
  Util::Ucs4ToUtf8(0xFE000, &str);
  Util::Ucs4ToUtf8Append(0xFE000, &str);
  EXPECT_FALSE(Util::IsAndroidPuaEmoji(str));
}

TEST(UtilTest, BracketTest) {
  static const struct BracketType {
    const char *open_bracket;
    const char *close_bracket;
  } kBracketType[] = {
      {"（", "）"}, {"〔", "〕"}, {"［", "］"}, {"｛", "｝"},
      {"〈", "〉"}, {"《", "》"}, {"「", "」"}, {"『", "』"},
      {"【", "】"}, {"〘", "〙"}, {"〚", "〛"}, {nullptr, nullptr},  // sentinel
  };

  std::string pair;
  for (size_t i = 0; (kBracketType[i].open_bracket != nullptr ||
                      kBracketType[i].close_bracket != nullptr);
       ++i) {
    EXPECT_TRUE(Util::IsOpenBracket(kBracketType[i].open_bracket, &pair));
    EXPECT_EQ(kBracketType[i].close_bracket, pair);
    EXPECT_TRUE(Util::IsCloseBracket(kBracketType[i].close_bracket, &pair));
    EXPECT_EQ(kBracketType[i].open_bracket, pair);
    EXPECT_FALSE(Util::IsOpenBracket(kBracketType[i].close_bracket, &pair));
    EXPECT_FALSE(Util::IsCloseBracket(kBracketType[i].open_bracket, &pair));
  }
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
  EXPECT_EQ("line", line);

  line = "line\r";
  EXPECT_TRUE(Util::ChopReturns(&line));
  EXPECT_EQ("line", line);

  line = "line\r\n";
  EXPECT_TRUE(Util::ChopReturns(&line));
  EXPECT_EQ("line", line);

  line = "line";
  EXPECT_FALSE(Util::ChopReturns(&line));
  EXPECT_EQ("line", line);

  line = "line1\nline2\n";
  EXPECT_TRUE(Util::ChopReturns(&line));
  EXPECT_EQ("line1\nline2", line);

  line = "line\n\n\n";
  EXPECT_TRUE(Util::ChopReturns(&line));
  EXPECT_EQ("line", line);
}

TEST(UtilTest, EncodeUri) {
  std::string encoded;
  Util::EncodeUri("もずく", &encoded);
  EXPECT_EQ("%E3%82%82%E3%81%9A%E3%81%8F", encoded);

  encoded.clear();
  Util::EncodeUri("mozc", &encoded);
  EXPECT_EQ("mozc", encoded);

  encoded.clear();
  Util::EncodeUri("http://mozc/?q=Hello World", &encoded);
  EXPECT_EQ("http%3A%2F%2Fmozc%2F%3Fq%3DHello%20World", encoded);
}

TEST(UtilTest, DecodeUri) {
  std::string decoded;
  Util::DecodeUri("%E3%82%82%E3%81%9A%E3%81%8F", &decoded);
  EXPECT_EQ("もずく", decoded);

  decoded.clear();
  Util::DecodeUri("mozc", &decoded);
  EXPECT_EQ("mozc", decoded);

  decoded.clear();
  Util::DecodeUri("http%3A%2F%2Fmozc%2F%3Fq%3DHello+World", &decoded);
  EXPECT_EQ("http://mozc/?q=Hello World", decoded);
}

TEST(UtilTest, AppendCgiParams) {
  std::vector<std::pair<std::string, std::string> > params;
  std::string url;
  Util::AppendCgiParams(params, &url);
  EXPECT_TRUE(url.empty());

  params.push_back(std::make_pair("foo", "b a+r"));
  url = "http://mozc.com?";
  Util::AppendCgiParams(params, &url);
  EXPECT_EQ("http://mozc.com?foo=b%20a%2Br", url);

  params.push_back(std::make_pair("buzz", "mozc"));
  url.clear();
  Util::AppendCgiParams(params, &url);
  EXPECT_EQ("foo=b%20a%2Br&buzz=mozc", url);
}

TEST(UtilTest, Escape) {
  std::string escaped;
  Util::Escape("らむだ", &escaped);
  EXPECT_EQ("\\xE3\\x82\\x89\\xE3\\x82\\x80\\xE3\\x81\\xA0", escaped);
}

TEST(UtilTest, Unescape) {
  std::string unescaped;
  EXPECT_TRUE(Util::Unescape("\\xE3\\x82\\x89\\xE3\\x82\\x80\\xE3\\x81\\xA0",
                             &unescaped));
  EXPECT_EQ("らむだ", unescaped);

  EXPECT_TRUE(Util::Unescape("\\x4D\\x6F\\x7A\\x63", &unescaped));
  EXPECT_EQ("Mozc", unescaped);

  // A binary sequence (upper case)
  EXPECT_TRUE(Util::Unescape("\\x00\\x01\\xEF\\xFF", &unescaped));
  EXPECT_EQ(std::string("\x00\x01\xEF\xFF", 4), unescaped);

  // A binary sequence (lower case)
  EXPECT_TRUE(Util::Unescape("\\x00\\x01\\xef\\xff", &unescaped));
  EXPECT_EQ(std::string("\x00\x01\xEF\xFF", 4), unescaped);

  EXPECT_TRUE(Util::Unescape("", &unescaped));
  EXPECT_TRUE(unescaped.empty());

  EXPECT_FALSE(Util::Unescape("\\AB\\CD\\EFG", &unescaped));
  EXPECT_FALSE(Util::Unescape("\\01\\XY", &unescaped));
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

  EXPECT_EQ(Util::HIRAGANA, Util::GetScriptType("くどう"));
  EXPECT_EQ(Util::KANJI, Util::GetScriptType("京都"));
  // b/4201140
  EXPECT_EQ(Util::KANJI, Util::GetScriptType("人々"));
  EXPECT_EQ(Util::KATAKANA, Util::GetScriptType("モズク"));
  EXPECT_EQ(Util::KATAKANA, Util::GetScriptType("モズクﾓｽﾞｸ"));
  EXPECT_EQ(Util::HIRAGANA, Util::GetScriptType("ぐーぐる"));
  EXPECT_EQ(Util::HIRAGANA, Util::GetFirstScriptType("ぐーぐる"));

  EXPECT_EQ(Util::KATAKANA, Util::GetScriptType("グーグル"));
  EXPECT_EQ(Util::KATAKANA, Util::GetFirstScriptType("グーグル"));
  // U+309F HIRAGANA DIGRAPH YORI
  EXPECT_EQ(Util::HIRAGANA, Util::GetScriptType("ゟ"));
  EXPECT_EQ(Util::HIRAGANA, Util::GetFirstScriptType("ゟ"));

  // U+30FF KATAKANA DIGRAPH KOTO
  EXPECT_EQ(Util::KATAKANA, Util::GetScriptType("ヿ"));
  EXPECT_EQ(Util::KATAKANA,
            Util::GetScriptType("ヷヸヹヺㇰㇱㇲㇳㇴㇵㇶㇷㇸㇹㇺㇻㇼㇽㇾㇿ"));
  // "𛀀" U+1B000 KATAKANA LETTER ARCHAIC E
  EXPECT_EQ(Util::KATAKANA, Util::GetScriptType("\xF0\x9B\x80\x80"));
  // "𛀁" U+1B001 HIRAGANA LETTER ARCHAIC YE
  EXPECT_EQ(Util::HIRAGANA, Util::GetScriptType("\xF0\x9B\x80\x81"));

  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptType("!グーグル"));
  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptType("ー"));    // U+30FC
  EXPECT_EQ(Util::KATAKANA, Util::GetFirstScriptType("ー"));     // U+30FC
  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptType("ーー"));  // U+30FC * 2
  EXPECT_EQ(Util::KATAKANA, Util::GetFirstScriptType("ーー"));   // U+30FC * 2
  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptType("゛"));
  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptType("゜"));

  EXPECT_EQ(Util::NUMBER, Util::GetScriptType("012"));
  EXPECT_EQ(Util::NUMBER, Util::GetScriptType("０１２012"));
  EXPECT_EQ(Util::ALPHABET, Util::GetScriptType("abcABC"));
  EXPECT_EQ(Util::ALPHABET, Util::GetScriptType("ＡＢＣＤ"));
  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptType("@!#"));
  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptType("＠！＃"));

  EXPECT_EQ(Util::HIRAGANA, Util::GetScriptType("ーひらがな"));
  EXPECT_EQ(Util::KATAKANA, Util::GetFirstScriptType("ーひらがな"));
  EXPECT_EQ(Util::KATAKANA, Util::GetScriptType("ーカタカナ"));
  EXPECT_EQ(Util::KATAKANA, Util::GetScriptType("ｰｶﾀｶﾅ"));
  EXPECT_EQ(Util::HIRAGANA, Util::GetScriptType("ひらがなー"));
  EXPECT_EQ(Util::KATAKANA, Util::GetScriptType("カタカナー"));
  EXPECT_EQ(Util::KATAKANA, Util::GetScriptType("ｶﾀｶﾅｰ"));

  EXPECT_EQ(Util::HIRAGANA, Util::GetScriptType("あ゛っ"));
  EXPECT_EQ(Util::HIRAGANA, Util::GetScriptType("あ゜っ"));
  EXPECT_EQ(Util::KATAKANA, Util::GetScriptType("ア゛ッ"));
  EXPECT_EQ(Util::KATAKANA, Util::GetScriptType("ア゜ッ"));

  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptType("くどカう"));
  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptType("京あ都"));
  EXPECT_EQ(Util::KANJI, Util::GetFirstScriptType("京あ都"));

  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptType("モズあク"));
  EXPECT_EQ(Util::KATAKANA, Util::GetFirstScriptType("モズあク"));

  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptType("モあズクﾓｽﾞｸ"));
  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptType("012あ"));
  EXPECT_EQ(Util::NUMBER, Util::GetFirstScriptType("012あ"));
  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptType("０１２あ012"));
  EXPECT_EQ(Util::NUMBER, Util::GetFirstScriptType("０１２あ012"));
  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptType("abcABあC"));
  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptType("ＡＢあＣＤ"));
  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptType("ぐーぐるグ"));
  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptType("グーグルぐ"));

  // "龦" U+9FA6
  EXPECT_EQ(Util::KANJI, Util::GetScriptType("\xE9\xBE\xA6"));
  // "龻" U+9FBB
  EXPECT_EQ(Util::KANJI, Util::GetScriptType("\xE9\xBE\xBB"));
  // U+9FFF is not assigned yet but reserved for CJK Unified Ideographs.
  EXPECT_EQ(Util::KANJI, Util::GetScriptType("\xE9\xBF\xBF"));
  // "𠮟咤" U+20B9F U+54A4
  EXPECT_EQ(Util::KANJI, Util::GetScriptType("\xF0\xA0\xAE\x9F\xE5\x92\xA4"));
  // "𠮷野" U+20BB7 U+91CE
  EXPECT_EQ(Util::KANJI, Util::GetScriptType("\xF0\xA0\xAE\xB7\xE9\x87\x8E"));
  // "巽" U+2F884
  EXPECT_EQ(Util::KANJI, Util::GetScriptType("\xF0\xAF\xA2\x84"));

  // U+1F466, BOY/smile emoji
  EXPECT_EQ(Util::EMOJI, Util::GetScriptType("\xF0\x9F\x91\xA6"));
  // U+FE003, Snow-man Android PUA emoji
  EXPECT_TRUE(Util::IsAndroidPuaEmoji("\xf3\xbe\x80\x83"));
  EXPECT_EQ(Util::EMOJI, Util::GetScriptType("\xf3\xbe\x80\x83"));
}

TEST(UtilTest, ScriptTypeWithoutSymbols) {
  EXPECT_EQ(Util::HIRAGANA, Util::GetScriptTypeWithoutSymbols("くど う"));
  EXPECT_EQ(Util::KANJI, Util::GetScriptTypeWithoutSymbols("京 都"));
  EXPECT_EQ(Util::KATAKANA, Util::GetScriptTypeWithoutSymbols("モズク"));
  EXPECT_EQ(Util::KATAKANA, Util::GetScriptTypeWithoutSymbols("モズ クﾓｽﾞｸ"));
  EXPECT_EQ(Util::ALPHABET, Util::GetScriptTypeWithoutSymbols("Google Earth"));
  EXPECT_EQ(Util::ALPHABET, Util::GetScriptTypeWithoutSymbols("Google "));
  EXPECT_EQ(Util::ALPHABET, Util::GetScriptTypeWithoutSymbols(" Google"));
  EXPECT_EQ(Util::ALPHABET, Util::GetScriptTypeWithoutSymbols(" Google "));
  EXPECT_EQ(Util::ALPHABET, Util::GetScriptTypeWithoutSymbols("     g"));
  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptTypeWithoutSymbols(""));
  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptTypeWithoutSymbols(" "));
  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptTypeWithoutSymbols("   "));
  EXPECT_EQ(Util::ALPHABET, Util::GetScriptTypeWithoutSymbols("Hello!"));
  EXPECT_EQ(Util::UNKNOWN_SCRIPT,
            Util::GetScriptTypeWithoutSymbols("Hello!あ"));
  EXPECT_EQ(Util::ALPHABET, Util::GetScriptTypeWithoutSymbols("CD-ROM"));
  EXPECT_EQ(Util::UNKNOWN_SCRIPT,
            Util::GetScriptTypeWithoutSymbols("CD-ROMア"));
  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptTypeWithoutSymbols("-"));
  EXPECT_EQ(Util::ALPHABET, Util::GetScriptTypeWithoutSymbols("-A"));
  EXPECT_EQ(Util::ALPHABET, Util::GetScriptTypeWithoutSymbols("--A"));
  EXPECT_EQ(Util::ALPHABET, Util::GetScriptTypeWithoutSymbols("--A---"));
  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptTypeWithoutSymbols("--A-ｱ-"));
  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptTypeWithoutSymbols("!"));
  EXPECT_EQ(Util::HIRAGANA, Util::GetScriptTypeWithoutSymbols("・あ"));
  EXPECT_EQ(Util::HIRAGANA, Util::GetScriptTypeWithoutSymbols("・・あ"));
  EXPECT_EQ(Util::KATAKANA,
            Util::GetScriptTypeWithoutSymbols("コギト・エルゴ・スム"));
  EXPECT_EQ(Util::UNKNOWN_SCRIPT,
            Util::GetScriptTypeWithoutSymbols("コギト・エルゴ・住む"));
  EXPECT_EQ(Util::KANJI, Util::GetScriptTypeWithoutSymbols("人☆名"));
  EXPECT_EQ(Util::HIRAGANA, Util::GetScriptTypeWithoutSymbols("ひとの☆なまえ"));
  EXPECT_EQ(Util::UNKNOWN_SCRIPT,
            Util::GetScriptTypeWithoutSymbols("超☆最高です"));
  EXPECT_EQ(Util::UNKNOWN_SCRIPT, Util::GetScriptTypeWithoutSymbols("・--☆"));
}

TEST(UtilTest, FormType) {
  EXPECT_EQ(Util::FULL_WIDTH, Util::GetFormType("くどう"));
  EXPECT_EQ(Util::FULL_WIDTH, Util::GetFormType("京都"));
  EXPECT_EQ(Util::FULL_WIDTH, Util::GetFormType("モズク"));
  EXPECT_EQ(Util::HALF_WIDTH, Util::GetFormType("ﾓｽﾞｸ"));
  EXPECT_EQ(Util::FULL_WIDTH, Util::GetFormType("ぐーぐる"));
  EXPECT_EQ(Util::FULL_WIDTH, Util::GetFormType("グーグル"));
  EXPECT_EQ(Util::HALF_WIDTH, Util::GetFormType("ｸﾞｰｸﾞﾙ"));
  EXPECT_EQ(Util::HALF_WIDTH, Util::GetFormType("ｰ"));
  EXPECT_EQ(Util::FULL_WIDTH, Util::GetFormType("ー"));
  EXPECT_EQ(Util::HALF_WIDTH, Util::GetFormType("¢£¥¦¬¯"));
  // "￨￩￪￫￬￭￮"
  EXPECT_EQ(Util::HALF_WIDTH,
            Util::GetFormType("\xEF\xBF\xA8\xEF\xBF\xA9\xEF\xBF\xAA\xEF\xBF\xAB"
                              "\xEF\xBF\xAC\xEF\xBF\xAD\xEF\xBF\xAE"));

  // Half-width mathematical symbols
  // [U+27E6, U+27ED], U+2985, and U+2986
  EXPECT_EQ(Util::HALF_WIDTH, Util::GetFormType("⟦⟧⟨⟩⟪⟫⟬⟭⦅⦆"));

  // Half-width hangul "ﾠﾡﾢ"
  EXPECT_EQ(Util::HALF_WIDTH,
            Util::GetFormType("\xEF\xBE\xA0\xEF\xBE\xA1\xEF\xBE\xA2"));

  // Half-width won "₩"
  EXPECT_EQ(Util::HALF_WIDTH, Util::GetFormType("₩"));

  EXPECT_EQ(Util::HALF_WIDTH, Util::GetFormType("012"));
  EXPECT_EQ(Util::UNKNOWN_FORM, Util::GetFormType("０１２012"));
  EXPECT_EQ(Util::HALF_WIDTH, Util::GetFormType("abcABC"));
  EXPECT_EQ(Util::FULL_WIDTH, Util::GetFormType("ＡＢＣＤ"));
  EXPECT_EQ(Util::HALF_WIDTH, Util::GetFormType("@!#"));
}

TEST(UtilTest, IsAscii) {
  EXPECT_FALSE(Util::IsAscii("あいうえお"));
  EXPECT_TRUE(Util::IsAscii("abc"));
  EXPECT_FALSE(Util::IsAscii("abcあいう"));
  EXPECT_TRUE(Util::IsAscii(""));
  EXPECT_TRUE(Util::IsAscii("\x7F"));
  EXPECT_FALSE(Util::IsAscii("\x80"));
}

TEST(UtilTest, IsJisX0208) {
  EXPECT_TRUE(Util::IsJisX0208(u8"\u007F"));
  EXPECT_FALSE(Util::IsJisX0208(u8"\u0080"));

  EXPECT_TRUE(Util::IsJisX0208("あいうえお"));
  EXPECT_TRUE(Util::IsJisX0208("abc"));
  EXPECT_TRUE(Util::IsJisX0208("abcあいう"));

  // half width katakana
  EXPECT_TRUE(Util::IsJisX0208("ｶﾀｶﾅ"));
  EXPECT_TRUE(Util::IsJisX0208("ｶﾀｶﾅカタカナ"));

  // boundary edges
  EXPECT_TRUE(Util::IsJisX0208("ﾟ"));  // U+FF9F, the last char of JIS X 0208
  EXPECT_TRUE(Util::IsJisX0208(u8"\uFF9F"));   // U+FF9F
  EXPECT_FALSE(Util::IsJisX0208(u8"\uFFA0"));  // U+FF9F + 1
  EXPECT_FALSE(Util::IsJisX0208(u8"\uFFFF"));
  EXPECT_FALSE(Util::IsJisX0208(u8"\U00010000"));

  // JIS X 0213
  EXPECT_FALSE(Util::IsJisX0208("Ⅰ"));
  EXPECT_FALSE(Util::IsJisX0208("①"));
  EXPECT_FALSE(Util::IsJisX0208("㊤"));

  // only in CP932
  EXPECT_FALSE(Util::IsJisX0208("凬"));

  // only in Unicode
  EXPECT_FALSE(Util::IsJisX0208("￦"));

  // SIP range (U+20000 - U+2FFFF)
  EXPECT_FALSE(Util::IsJisX0208("𠮟"));  // U+20B9F
  EXPECT_FALSE(Util::IsJisX0208("𪚲"));  // U+2A6B2
  EXPECT_FALSE(Util::IsJisX0208("𠮷"));  // U+20BB7
}

#ifdef OS_WIN
TEST(UtilTest, WideCharsLen) {
  // "að ®b"
  const std::string input_utf8 = "a\360\240\256\237b";
  EXPECT_EQ(4, Util::WideCharsLen(input_utf8));
  EXPECT_EQ(0, Util::WideCharsLen(Util::Utf8SubString(input_utf8, 0, 0)));
  EXPECT_EQ(1, Util::WideCharsLen(Util::Utf8SubString(input_utf8, 0, 1)));
  EXPECT_EQ(3, Util::WideCharsLen(Util::Utf8SubString(input_utf8, 0, 2)));
  EXPECT_EQ(4, Util::WideCharsLen(Util::Utf8SubString(input_utf8, 0, 3)));
}

TEST(UtilTest, Utf8ToWide) {
  const std::string input_utf8 = "abc";
  std::wstring output_wide;
  Util::Utf8ToWide(input_utf8, &output_wide);

  std::string output_utf8;
  Util::WideToUtf8(output_wide, &output_utf8);
  EXPECT_EQ("abc", output_utf8);
}

TEST(UtilTest, WideToUtf8_SurrogatePairSupport) {
  // Visual C++ 2008 does not support embedding surrogate pair in string
  // literals like L"\uD842\uDF9F". This is why we use wchar_t array instead.
  // "ð ®"
  const wchar_t input_wide[] = {0xD842, 0xDF9F, 0};
  std::string output_utf8;
  Util::WideToUtf8(input_wide, &output_utf8);

  std::wstring output_wide;
  Util::Utf8ToWide(output_utf8, &output_wide);

  EXPECT_EQ("\360\240\256\237", output_utf8);
  EXPECT_EQ(input_wide, output_wide);
}
#endif  // OS_WIN

TEST(UtilTest, IsKanaSymbolContained) {
  const std::string kFullstop("。");
  const std::string kSpace(" ");
  EXPECT_TRUE(Util::IsKanaSymbolContained(kFullstop));
  EXPECT_TRUE(Util::IsKanaSymbolContained(kSpace + kFullstop));
  EXPECT_TRUE(Util::IsKanaSymbolContained(kFullstop + kSpace));
  EXPECT_FALSE(Util::IsKanaSymbolContained(kSpace));
  EXPECT_FALSE(Util::IsKanaSymbolContained(""));
}

TEST(UtilTest, RandomSeedTest) {
  Util::SetRandomSeed(0);
  const int first_try = Util::Random(INT_MAX);
  const int second_try = Util::Random(INT_MAX);
  EXPECT_NE(first_try, second_try);

  // Reset the seed.
  Util::SetRandomSeed(0);
  EXPECT_EQ(first_try, Util::Random(INT_MAX));
}

TEST(UtilTest, SplitFirstChar32) {
  absl::string_view rest;
  char32_t c = 0;

  rest = absl::string_view();
  c = 0;
  EXPECT_FALSE(Util::SplitFirstChar32("", &c, &rest));
  EXPECT_EQ(0, c);
  EXPECT_TRUE(rest.empty());

  // Allow nullptr to ignore the matched value.
  rest = absl::string_view();
  EXPECT_TRUE(Util::SplitFirstChar32("01", nullptr, &rest));
  EXPECT_EQ("1", rest);

  // Allow nullptr to ignore the matched value.
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("01", &c, nullptr));
  EXPECT_EQ('0', c);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\x01 ", &c, &rest));
  EXPECT_EQ(1, c);
  EXPECT_EQ(" ", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\x7F ", &c, &rest));
  EXPECT_EQ(0x7F, c);
  EXPECT_EQ(" ", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\xC2\x80 ", &c, &rest));
  EXPECT_EQ(0x80, c);
  EXPECT_EQ(" ", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\xDF\xBF ", &c, &rest));
  EXPECT_EQ(0x7FF, c);
  EXPECT_EQ(" ", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\xE0\xA0\x80 ", &c, &rest));
  EXPECT_EQ(0x800, c);
  EXPECT_EQ(" ", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\xEF\xBF\xBF ", &c, &rest));
  EXPECT_EQ(0xFFFF, c);
  EXPECT_EQ(" ", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\xF0\x90\x80\x80 ", &c, &rest));
  EXPECT_EQ(0x10000, c);
  EXPECT_EQ(" ", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\xF7\xBF\xBF\xBF ", &c, &rest));
  EXPECT_EQ(0x1FFFFF, c);
  EXPECT_EQ(" ", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\xF8\x88\x80\x80\x80 ", &c, &rest));
  EXPECT_EQ(0x200000, c);
  EXPECT_EQ(" ", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\xFB\xBF\xBF\xBF\xBF ", &c, &rest));
  EXPECT_EQ(0x3FFFFFF, c);
  EXPECT_EQ(" ", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\xFC\x84\x80\x80\x80\x80 ", &c, &rest));
  EXPECT_EQ(0x4000000, c);
  EXPECT_EQ(" ", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitFirstChar32("\xFD\xBF\xBF\xBF\xBF\xBF ", &c, &rest));
  EXPECT_EQ(0x7FFFFFFF, c);
  EXPECT_EQ(" ", rest);

  // If there is any invalid sequence, the entire text should be treated as
  // am empty string.
  {
    c = 0;
    EXPECT_FALSE(Util::SplitFirstChar32("\xC2 ", &c, &rest));
    EXPECT_EQ(0, c);

    c = 0;
    EXPECT_FALSE(Util::SplitFirstChar32("\xC2\xC2 ", &c, &rest));
    EXPECT_EQ(0, c);

    c = 0;
    EXPECT_FALSE(Util::SplitFirstChar32("\xE0 ", &c, &rest));
    EXPECT_EQ(0, c);

    c = 0;
    EXPECT_FALSE(Util::SplitFirstChar32("\xE0\xE0\xE0 ", &c, &rest));
    EXPECT_EQ(0, c);

    c = 0;
    EXPECT_FALSE(Util::SplitFirstChar32("\xF0 ", &c, &rest));
    EXPECT_EQ(0, c);

    c = 0;
    EXPECT_FALSE(Util::SplitFirstChar32("\xF0\xF0\xF0\xF0 ", &c, &rest));
    EXPECT_EQ(0, c);
  }

  // BOM should be treated as invalid byte.
  {
    c = 0;
    EXPECT_FALSE(Util::SplitFirstChar32("\xFF ", &c, &rest));
    EXPECT_EQ(0, c);

    c = 0;
    EXPECT_FALSE(Util::SplitFirstChar32("\xFE ", &c, &rest));
    EXPECT_EQ(0, c);
  }

  // Invalid sequence for U+002F (redundant encoding)
  {
    c = 0;
    EXPECT_FALSE(Util::SplitFirstChar32("\xC0\xAF", &c, &rest));
    EXPECT_EQ(0, c);

    c = 0;
    EXPECT_FALSE(Util::SplitFirstChar32("\xE0\x80\xAF", &c, &rest));
    EXPECT_EQ(0, c);

    c = 0;
    EXPECT_FALSE(Util::SplitFirstChar32("\xF0\x80\x80\xAF", &c, &rest));
    EXPECT_EQ(0, c);
  }
}

TEST(UtilTest, SplitLastChar32) {
  absl::string_view rest;
  char32_t c = 0;

  rest = absl::string_view();
  c = 0;
  EXPECT_FALSE(Util::SplitLastChar32("", &rest, &c));
  EXPECT_EQ(0, c);
  EXPECT_TRUE(rest.empty());

  // Allow nullptr to ignore the matched value.
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32("01", nullptr, &c));
  EXPECT_EQ('1', c);

  // Allow nullptr to ignore the matched value.
  rest = absl::string_view();
  EXPECT_TRUE(Util::SplitLastChar32("01", &rest, nullptr));
  EXPECT_EQ("0", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \x01", &rest, &c));
  EXPECT_EQ(1, c);
  EXPECT_EQ(" ", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \x7F", &rest, &c));
  EXPECT_EQ(0x7F, c);
  EXPECT_EQ(" ", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \xC2\x80", &rest, &c));
  EXPECT_EQ(0x80, c);
  EXPECT_EQ(" ", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \xDF\xBF", &rest, &c));
  EXPECT_EQ(0x7FF, c);
  EXPECT_EQ(" ", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \xE0\xA0\x80", &rest, &c));
  EXPECT_EQ(0x800, c);
  EXPECT_EQ(" ", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \xEF\xBF\xBF", &rest, &c));
  EXPECT_EQ(0xFFFF, c);
  EXPECT_EQ(" ", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \xF0\x90\x80\x80", &rest, &c));
  EXPECT_EQ(0x10000, c);
  EXPECT_EQ(" ", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \xF7\xBF\xBF\xBF", &rest, &c));
  EXPECT_EQ(0x1FFFFF, c);
  EXPECT_EQ(" ", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \xF8\x88\x80\x80\x80", &rest, &c));
  EXPECT_EQ(0x200000, c);
  EXPECT_EQ(" ", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \xFB\xBF\xBF\xBF\xBF", &rest, &c));
  EXPECT_EQ(0x3FFFFFF, c);
  EXPECT_EQ(" ", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \xFC\x84\x80\x80\x80\x80", &rest, &c));
  EXPECT_EQ(0x4000000, c);
  EXPECT_EQ(" ", rest);

  rest = absl::string_view();
  c = 0;
  EXPECT_TRUE(Util::SplitLastChar32(" \xFD\xBF\xBF\xBF\xBF\xBF", &rest, &c));
  EXPECT_EQ(0x7FFFFFFF, c);
  EXPECT_EQ(" ", rest);

  // If there is any invalid sequence, the entire text should be treated as
  // am empty string.
  {
    c = 0;
    EXPECT_FALSE(Util::SplitLastChar32(" \xC2", &rest, &c));
    EXPECT_EQ(0, c);

    c = 0;
    EXPECT_FALSE(Util::SplitLastChar32(" \xC2\xC2", &rest, &c));
    EXPECT_EQ(0, c);

    c = 0;
    EXPECT_FALSE(Util::SplitLastChar32(" \xE0", &rest, &c));
    EXPECT_EQ(0, c);

    c = 0;
    EXPECT_FALSE(Util::SplitLastChar32(" \xE0\xE0\xE0", &rest, &c));
    EXPECT_EQ(0, c);

    c = 0;
    EXPECT_FALSE(Util::SplitLastChar32(" \xF0", &rest, &c));
    EXPECT_EQ(0, c);

    c = 0;
    EXPECT_FALSE(Util::SplitLastChar32(" \xF0\xF0\xF0\xF0", &rest, &c));
    EXPECT_EQ(0, c);
  }

  // BOM should be treated as invalid byte.
  {
    c = 0;
    EXPECT_FALSE(Util::SplitLastChar32(" \xFF", &rest, &c));
    EXPECT_EQ(0, c);

    c = 0;
    EXPECT_FALSE(Util::SplitLastChar32(" \xFE", &rest, &c));
    EXPECT_EQ(0, c);
  }

  // Invalid sequence for U+002F (redundant encoding)
  {
    c = 0;
    EXPECT_FALSE(Util::SplitLastChar32("\xC0\xAF", &rest, &c));
    EXPECT_EQ(0, c);

    c = 0;
    EXPECT_FALSE(Util::SplitLastChar32("\xE0\x80\xAF", &rest, &c));
    EXPECT_EQ(0, c);

    c = 0;
    EXPECT_FALSE(Util::SplitLastChar32("\xF0\x80\x80\xAF", &rest, &c));
    EXPECT_EQ(0, c);
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
    const char *str;
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
    EXPECT_EQ(serialized, Util::SerializeUint64(kCorrectPairs[i].value));

    uint64_t v;
    EXPECT_TRUE(Util::DeserializeUint64(serialized, &v));
    EXPECT_EQ(kCorrectPairs[i].value, v);
  }

  // Invalid patterns for DeserializeUint64.
  const char *kFalseCases[] = {
      "",
      "abc",
      "helloworld",
  };
  for (size_t i = 0; i < std::size(kFalseCases); ++i) {
    uint64_t v;
    EXPECT_FALSE(Util::DeserializeUint64(kFalseCases[i], &v));
  }
}

}  // namespace mozc
