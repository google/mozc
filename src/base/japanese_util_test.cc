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

#include "base/japanese_util.h"

#include <iterator>
#include <string>

#include "testing/gunit.h"

namespace mozc {

TEST(JapaneseUtilTest, HiraganaToKatakana) {
  {
    const std::string input =
        "あいうえおぁぃぅぇぉかきくけこがぎぐげごさしすせそざじずぜぞたちつてと"
        "だぢづでどっなにぬねのはひふへほばびぶべぼぱぴぷぺぽまみむめもやゆよゃ"
        "ゅょらりるれろわゎをんゔ";
    std::string output;
    japanese_util::HiraganaToKatakana(input, &output);
    EXPECT_EQ(
        "アイウエオァィゥェォカキクケコガギグゲゴサシスセソザジズゼゾタチツテト"
        "ダヂヅデドッナニヌネノハヒフヘホバビブベボパピプペポマミムメモヤユヨャ"
        "ュョラリルレロワヮヲンヴ",
        output);
  }
  {
    const std::string input = "わたしのなまえはなかのですうまーよろしゅう";
    std::string output;
    japanese_util::HiraganaToKatakana(input, &output);
    EXPECT_EQ("ワタシノナマエハナカノデスウマーヨロシュウ", output);
  }
  {
    const std::string input = "グーグル工藤よろしくabc";
    std::string output;
    japanese_util::HiraganaToKatakana(input, &output);
    EXPECT_EQ("グーグル工藤ヨロシクabc", output);
  }
}

TEST(JapaneseUtilTest, KatakanaToHiragana) {
  {
    const std::string input =
        "アイウエオァィゥェォカキクケコガギグゲゴサシスセソザジズゼゾタチツテト"
        "ダヂヅデドッナニヌネノハヒフヘホバビブベボパピプペポマミムメモヤユヨャ"
        "ュョラリルレロワヮヲンヰヱヴ";
    std::string output;
    japanese_util::KatakanaToHiragana(input, &output);
    EXPECT_EQ(
        "あいうえおぁぃぅぇぉかきくけこがぎぐげごさしすせそざじずぜぞたちつてと"
        "だぢづでどっなにぬねのはひふへほばびぶべぼぱぴぷぺぽまみむめもやゆよゃ"
        "ゅょらりるれろわゎをんゐゑゔ",
        output);
  }
  {
    const std::string input = "ワタシノナマエハナカノデスウマーヨロシュウ";
    std::string output;
    japanese_util::KatakanaToHiragana(input, &output);
    EXPECT_EQ("わたしのなまえはなかのですうまーよろしゅう", output);
  }
  {
    const std::string input = "グーグル工藤ヨロシクabc";
    std::string output;
    japanese_util::KatakanaToHiragana(input, &output);
    EXPECT_EQ("ぐーぐる工藤よろしくabc", output);
  }
}

TEST(JapaneseUtilTest, RomanjiToHiragana) {
  struct {
    const char *input;
    const char *expected;
  } kTestCases[] = {
      {"watasinonamaehatakahashinoriyukidesu",
       "わたしのなまえはたかはしのりゆきです"},
      {"majissukamajiyabexe", "まじっすかまじやべぇ"},
      {"kk", "っk"},
      {"xyz", "xyz"},
  };
  for (const auto &test_case : kTestCases) {
    std::string actual;
    japanese_util::RomanjiToHiragana(test_case.input, &actual);
    EXPECT_EQ(test_case.expected, actual);
  }
}

TEST(JapaneseUtilTest, HiraganaToRomaji) {
  struct {
    const char *input;
    const char *expected;
  } kTestCases[] = {
      {"わたしのなまえはたかはしのりゆきです",
       "watasinonamaehatakahasinoriyukidesu"},
      {"まじっすかまじやべぇ", "mazissukamaziyabexe"},
      {"おっっっ", "oxtuxtuxtu"},
      {"おっっっと", "oxtuxtutto"},
      {"xyz", "xyz"},
  };
  for (const auto &test_case : kTestCases) {
    std::string actual;
    japanese_util::HiraganaToRomanji(test_case.input, &actual);
    EXPECT_EQ(test_case.expected, actual);
  }
}

TEST(JapaneseUtilTest, NormalizeVoicedSoundMark) {
  const std::string input = "僕のう゛ぁいおりん";
  std::string output;
  japanese_util::NormalizeVoicedSoundMark(input, &output);
  EXPECT_EQ("僕のゔぁいおりん", output);
}

TEST(JapaneseUtilTest, FullWidthAndHalfWidth) {
  std::string output;

  japanese_util::FullWidthToHalfWidth("", &output);
  EXPECT_EQ("", output);

  japanese_util::HalfWidthToFullWidth("", &output);
  EXPECT_EQ("", output);

  japanese_util::HalfWidthToFullWidth("abc[]?.", &output);
  EXPECT_EQ("ａｂｃ［］？．", output);

  japanese_util::HalfWidthToFullWidth("ｲﾝﾀｰﾈｯﾄ｢」", &output);
  EXPECT_EQ("インターネット「」", output);

  japanese_util::HalfWidthToFullWidth("ｲﾝﾀｰﾈｯﾄグーグル", &output);
  EXPECT_EQ("インターネットグーグル", output);

  japanese_util::FullWidthToHalfWidth("ａｂｃ［］？．", &output);
  EXPECT_EQ("abc[]?.", output);

  japanese_util::FullWidthToHalfWidth("インターネット", &output);
  EXPECT_EQ("ｲﾝﾀｰﾈｯﾄ", output);

  japanese_util::FullWidthToHalfWidth("ｲﾝﾀｰﾈｯﾄグーグル", &output);
  EXPECT_EQ("ｲﾝﾀｰﾈｯﾄｸﾞｰｸﾞﾙ", output);

  // spaces
  japanese_util::FullWidthToHalfWidth(" 　",
                                      &output);  // Half- and full-width spaces
  EXPECT_EQ("  ", output);                       // 2 half-width spaces

  japanese_util::HalfWidthToFullWidth(" 　",
                                      &output);  // Half- and full-width spaces
  EXPECT_EQ("　　", output);                     // 2 full-width spaces

  // Spaces are treated as Ascii here
  // Half- and full-width spaces
  japanese_util::FullWidthAsciiToHalfWidthAscii(" 　", &output);
  EXPECT_EQ("  ", output);  // 2 half-width spaces

  japanese_util::HalfWidthAsciiToFullWidthAscii("  ", &output);
  EXPECT_EQ("　　", output);  // 2 full-width spaces

  // Half- and full-width spaces
  japanese_util::FullWidthKatakanaToHalfWidthKatakana(" 　", &output);
  EXPECT_EQ(" 　", output);  // Not changed

  // Half- and full-width spaces
  japanese_util::HalfWidthKatakanaToFullWidthKatakana(" 　", &output);
  EXPECT_EQ(" 　", output);  // Not changed
}

}  // namespace mozc
