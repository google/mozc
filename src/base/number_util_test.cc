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

#include "base/number_util.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

TEST(NumberUtilTest, SimpleAtoi) {
  EXPECT_EQ(NumberUtil::SimpleAtoi("0"), 0);
  EXPECT_EQ(NumberUtil::SimpleAtoi("123"), 123);
  EXPECT_EQ(NumberUtil::SimpleAtoi("-1"), -1);

  // Invalid cases return 0.
  EXPECT_EQ(NumberUtil::SimpleAtoi("abc"), 0);
  EXPECT_EQ(NumberUtil::SimpleAtoi("a1"), 0);
  EXPECT_EQ(NumberUtil::SimpleAtoi("1 a"), 0);
}

TEST(NumberUtilTest, SafeStrToInt16) {
  int16_t value = 0x4321;

  EXPECT_TRUE(NumberUtil::SafeStrToInt16("0", &value));
  EXPECT_EQ(value, 0);
  value = 0x4321;
  EXPECT_TRUE(NumberUtil::SafeStrToInt16("+0", &value));
  EXPECT_EQ(value, 0);
  value = 0x4321;
  EXPECT_TRUE(NumberUtil::SafeStrToInt16("-0", &value));
  EXPECT_EQ(value, 0);
  value = 0x4321;
  EXPECT_TRUE(NumberUtil::SafeStrToInt16(" \t\r\n\v\f0 \t\r\n\v\f", &value));
  EXPECT_EQ(value, 0);
  value = 0x4321;
  EXPECT_TRUE(NumberUtil::SafeStrToInt16(" \t\r\n\v\f-0 \t\r\n\v\f", &value));
  EXPECT_EQ(value, 0);
  value = 0x4321;
  EXPECT_TRUE(NumberUtil::SafeStrToInt16("012345", &value));
  EXPECT_EQ(value, 12345);
  value = 0x4321;
  EXPECT_TRUE(NumberUtil::SafeStrToInt16("-012345", &value));
  EXPECT_EQ(value, -12345);
  value = 0x4321;
  EXPECT_TRUE(NumberUtil::SafeStrToInt16("-32768", &value));
  EXPECT_EQ(value, std::numeric_limits<int16_t>::min());
  value = 0x4321;
  EXPECT_TRUE(NumberUtil::SafeStrToInt16("32767", &value));
  EXPECT_EQ(value, std::numeric_limits<int16_t>::max());
  value = 0x4321;
  EXPECT_TRUE(NumberUtil::SafeStrToInt16(" 1", &value));
  EXPECT_EQ(value, 1);
  value = 0x4321;
  EXPECT_TRUE(NumberUtil::SafeStrToInt16("2 ", &value));
  EXPECT_EQ(value, 2);

  EXPECT_FALSE(NumberUtil::SafeStrToInt16("0x1234", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToInt16("-32769", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToInt16("32768", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToInt16("18446744073709551616", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToInt16("3e", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToInt16("0.", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToInt16(".0", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToInt16("", &value));

  // Test for absl::string_view input.
  constexpr absl::string_view kString = "123 abc 789";
  EXPECT_TRUE(NumberUtil::SafeStrToInt16(kString.substr(0, 3), &value));
  EXPECT_EQ(value, 123);
  EXPECT_FALSE(NumberUtil::SafeStrToInt16(kString.substr(4, 3), &value));
  EXPECT_TRUE(NumberUtil::SafeStrToInt16(kString.substr(8, 3), &value));
  EXPECT_EQ(value, 789);
  EXPECT_TRUE(NumberUtil::SafeStrToInt16(kString.substr(7, 4), &value));
  EXPECT_EQ(value, 789);
}

TEST(NumberUtilTest, SafeStrToUInt16) {
  uint16_t value = 0xBEEF;

  EXPECT_TRUE(NumberUtil::SafeStrToUInt16("0", &value));
  EXPECT_EQ(value, 0);

  value = 0xBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToUInt16(" \t\r\n\v\f0 \t\r\n\v\f", &value));
  EXPECT_EQ(value, 0);

  value = 0xBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToUInt16("012345", &value));
  EXPECT_EQ(value, 12345);

  value = 0xBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToUInt16("65535", &value));
  EXPECT_EQ(value, 65535u);  // max of 16-bit unsigned integer

  value = 0xBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToUInt16(" 1", &value));
  EXPECT_EQ(value, 1);

  value = 0xBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToUInt16("2 ", &value));
  EXPECT_EQ(value, 2);

  EXPECT_FALSE(NumberUtil::SafeStrToUInt16("-0", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToUInt16("0x1234", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToUInt16("65536", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToUInt16("18446744073709551616", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToUInt16("3e", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToUInt16("0.", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToUInt16(".0", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToUInt16("", &value));

  // Test for absl::string_view input.
  constexpr absl::string_view kString = "123 abc 789";
  EXPECT_TRUE(NumberUtil::SafeStrToUInt16(kString.substr(0, 3), &value));
  EXPECT_EQ(value, 123);
  EXPECT_FALSE(NumberUtil::SafeStrToUInt16(kString.substr(4, 3), &value));
  EXPECT_TRUE(NumberUtil::SafeStrToUInt16(kString.substr(8, 3), &value));
  EXPECT_EQ(value, 789);
  EXPECT_TRUE(NumberUtil::SafeStrToUInt16(kString.substr(7, 4), &value));
  EXPECT_EQ(value, 789);
}

TEST(NumberUtilTest, SafeStrToDouble) {
  double value = 1.0;

  EXPECT_TRUE(NumberUtil::SafeStrToDouble("0", &value));
  EXPECT_EQ(value, 0.0);
  EXPECT_TRUE(NumberUtil::SafeStrToDouble(" \t\r\n\v\f0 \t\r\n\v\f", &value));
  EXPECT_EQ(value, 0.0);
  EXPECT_TRUE(NumberUtil::SafeStrToDouble("-0", &value));
  EXPECT_EQ(value, 0.0);
  EXPECT_TRUE(NumberUtil::SafeStrToDouble("1.0e1", &value));
  EXPECT_EQ(value, 10.0);
  EXPECT_TRUE(NumberUtil::SafeStrToDouble("-5.0e-1", &value));
  EXPECT_EQ(value, -0.5);
  EXPECT_TRUE(NumberUtil::SafeStrToDouble(".0", &value));
  EXPECT_EQ(value, 0.0);
  EXPECT_TRUE(NumberUtil::SafeStrToDouble("0.", &value));
  EXPECT_EQ(value, 0.0);
  EXPECT_TRUE(NumberUtil::SafeStrToDouble("0.0", &value));
  EXPECT_EQ(value, 0.0);
  // Approximate representation of max of double.  The value checking is done by
  // EXPECT_DOUBLE_EQ as the result might be very slightly different on some
  // platforms.
  EXPECT_TRUE(NumberUtil::SafeStrToDouble("1.7976931348623158e308", &value));
  EXPECT_DOUBLE_EQ(1.7976931348623158e308, value);
  EXPECT_TRUE(NumberUtil::SafeStrToDouble("-1.7976931348623158e308", &value));
  EXPECT_DOUBLE_EQ(-1.7976931348623158e308, value);

  // It seems that the Android libc doesn't accept hex format, so disable it.
#ifndef __ANDROID__
  EXPECT_TRUE(NumberUtil::SafeStrToDouble("0x1234", &value));
  EXPECT_EQ(value, static_cast<double>(0x1234));
#endif  // __ANDROID__

  EXPECT_FALSE(NumberUtil::SafeStrToDouble("1.0e309", &value));   // overflow
  EXPECT_FALSE(NumberUtil::SafeStrToDouble("-1.0e309", &value));  // underflow
  EXPECT_FALSE(NumberUtil::SafeStrToDouble("NaN", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToDouble("3e", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToDouble(".", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToDouble("", &value));

  // Test for absl::string_view input.
  constexpr absl::string_view kString = "0.01 3.1415 double";
  EXPECT_TRUE(NumberUtil::SafeStrToDouble(kString.substr(0, 4), &value));
  EXPECT_EQ(value, 0.01);
  EXPECT_TRUE(NumberUtil::SafeStrToDouble(kString.substr(5, 6), &value));
  EXPECT_EQ(value, 3.1415);
  EXPECT_FALSE(NumberUtil::SafeStrToDouble(kString.substr(12, 6), &value));
}

TEST(NumberUtilTest, IsArabicNumber) {
  EXPECT_FALSE(NumberUtil::IsArabicNumber(""));

  EXPECT_TRUE(NumberUtil::IsArabicNumber("0"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("1"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("2"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("3"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("4"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("5"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("6"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("7"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("8"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("9"));

  EXPECT_TRUE(NumberUtil::IsArabicNumber("０"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("１"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("２"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("３"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("４"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("５"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("６"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("７"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("８"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("９"));

  EXPECT_TRUE(NumberUtil::IsArabicNumber("0123456789"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("01234567890123456789"));

  EXPECT_TRUE(NumberUtil::IsArabicNumber("１０"));

  EXPECT_FALSE(NumberUtil::IsArabicNumber("abc"));
  EXPECT_FALSE(NumberUtil::IsArabicNumber("十"));
  EXPECT_FALSE(NumberUtil::IsArabicNumber("億"));

  EXPECT_FALSE(NumberUtil::IsArabicNumber("グーグル"));
}

TEST(NumberUtilTest, IsDecimalInteger) {
  EXPECT_FALSE(NumberUtil::IsDecimalInteger(""));

  EXPECT_TRUE(NumberUtil::IsDecimalInteger("0"));
  EXPECT_TRUE(NumberUtil::IsDecimalInteger("1"));
  EXPECT_TRUE(NumberUtil::IsDecimalInteger("2"));
  EXPECT_TRUE(NumberUtil::IsDecimalInteger("3"));
  EXPECT_TRUE(NumberUtil::IsDecimalInteger("4"));
  EXPECT_TRUE(NumberUtil::IsDecimalInteger("5"));
  EXPECT_TRUE(NumberUtil::IsDecimalInteger("6"));
  EXPECT_TRUE(NumberUtil::IsDecimalInteger("7"));
  EXPECT_TRUE(NumberUtil::IsDecimalInteger("8"));
  EXPECT_TRUE(NumberUtil::IsDecimalInteger("9"));
  EXPECT_TRUE(NumberUtil::IsDecimalInteger("0123456789"));
  EXPECT_TRUE(NumberUtil::IsDecimalInteger("01234567890123456789"));

  EXPECT_FALSE(NumberUtil::IsDecimalInteger("０"));
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("１"));
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("２"));
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("３"));
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("４"));
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("５"));
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("６"));
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("７"));
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("８"));
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("９"));

  EXPECT_FALSE(NumberUtil::IsDecimalInteger("１０"));
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("十"));
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("億"));
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("abc"));
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("グーグル"));
}

TEST(NumberUtilTest, KanjiNumberToArabicNumber) {
  constexpr absl::string_view inputs[] = {"十", "百", "千", "万",
                                          "億", "兆", "京"};
  constexpr absl::string_view expects[] = {"10",
                                           "100",
                                           "1000",
                                           "10000",
                                           "100000000",
                                           "1000000000000",
                                           "10000000000000000"};

  for (size_t i = 0; i < std::size(inputs); ++i) {
    std::string arabic;
    NumberUtil::KanjiNumberToArabicNumber(inputs[i], &arabic);
    EXPECT_EQ(arabic, expects[i]);
  }
}

TEST(NumberUtilTest, NormalizeNumbers) {
  // An element has input, expected Kanji output, and expected Arabic output.
  constexpr absl::string_view success_data[][3] = {
      {"一", "一", "1"},
      {"九", "九", "9"},
      {"十", "十", "10"},
      {"十五", "十五", "15"},
      {"二十", "二十", "20"},
      {"三十五", "三十五", "35"},
      {"百", "百", "100"},
      {"二百", "二百", "200"},
      {"二百十", "二百十", "210"},
      {"二百五十", "二百五十", "250"},
      {"七百七十七", "七百七十七", "777"},
      {"千", "千", "1000"},
      {"一千", "一千", "1000"},
      {"八千", "八千", "8000"},
      {"八千七百三十九", "八千七百三十九", "8739"},
      {"一万二十五", "一万二十五", "10025"},
      // 2^64 - 1
      {"千八百四十四京六千七百四十四兆七百三十七億九百五十五万千六百十五",
       "千八百四十四京六千七百四十四兆七百三十七億九百五十五万千六百十五",
       "18446744073709551615"},
      {"百億百", "百億百", "10000000100"},
      {"一千京", "一千京", "10000000000000000000"},

      // Old Kanji numbers
      {"零", "零", "0"},
      {"拾", "拾", "10"},
      {"拾四", "拾四", "14"},
      {"廿", "廿", "20"},
      {"廿万廿", "廿万廿", "200020"},
      {"弐拾参", "弐拾参", "23"},
      {"零弐拾参", "零弐拾参", "23"},

      // Array of Kanji number digits
      {"0", "〇", "0"},
      {"00", "〇〇", "0"},
      {"二三五", "二三五", "235"},
      {"０１２", "〇一二", "12"},
      {"二零一一", "二零一一", "2011"},

      // Combinations of several types
      {"二三五万四三", "二三五万四三", "2350043"},
      {"二百三五万一", "二百三五万一", "2350001"},
      {"2十5", "二十五", "25"},
      {"2千四十３", "二千四十三", "2043"},
      {"九０", "九〇", "90"},
  };

  for (size_t i = 0; i < std::size(success_data); ++i) {
    std::string arabic_output = "dummy_text_arabic";
    std::string kanji_output = "dummy_text_kanji";
    EXPECT_TRUE(NumberUtil::NormalizeNumbers(success_data[i][0], true,
                                             &kanji_output, &arabic_output));
    EXPECT_EQ(kanji_output, success_data[i][1]);
    EXPECT_EQ(arabic_output, success_data[i][2]);
  }

  // An element has input, expected Kanji output, and expected Arabic output.
  constexpr absl::string_view success_notrim_data[][3] = {
      {"０１２", "〇一二", "012"},
      {"０00", "〇〇〇", "000"},
      {"００１２", "〇〇一二", "0012"},
      {"０零０１２", "〇零〇一二", "00012"},
      {"0", "〇", "0"},
      {"00", "〇〇", "00"},
  };

  for (size_t i = 0; i < std::size(success_notrim_data); ++i) {
    std::string arabic_output = "dummy_text_arabic";
    std::string kanji_output = "dummy_text_kanji";
    EXPECT_TRUE(NumberUtil::NormalizeNumbers(success_notrim_data[i][0], false,
                                             &kanji_output, &arabic_output));
    EXPECT_EQ(kanji_output, success_notrim_data[i][1]);
    EXPECT_EQ(arabic_output, success_notrim_data[i][2]);
  }

  // Test data expected to fail
  constexpr absl::string_view fail_data[] = {
      // 2^64
      "千八百四十四京六千七百四十四兆七百三十七億九百五十五万千六百十六",
      "てすと",
      "てすと２",
      "一十",
      "一百",
      "万二千三百四十五",  // lack of number before "万"
      "三億一京",          // large base, "京", after small one, "億"
      "三百四百",          // same base appears twice
      "五億六億",          // same base appears twice
      "二十三十",          // same base appears twice
      "二十百",            // relatively large base "百" after "十"
      "一二三四五六七八九十",
      "九九八十一",
  };

  for (size_t i = 0; i < std::size(fail_data); ++i) {
    std::string arabic_output, kanji_output;
    EXPECT_FALSE(NumberUtil::NormalizeNumbers(fail_data[i], true, &kanji_output,
                                              &arabic_output));
  }
}

TEST(NumberUtilTest, NormalizeNumbersWithSuffix) {
  {
    // Checks that kanji_output and arabic_output is cleared.
    constexpr absl::string_view kInput = "一個";
    std::string arabic_output = "dummy_text_arabic";
    std::string kanji_output = "dummy_text_kanji";
    std::string suffix = "dummy_text_suffix";
    EXPECT_TRUE(NumberUtil::NormalizeNumbersWithSuffix(
        kInput, true, &kanji_output, &arabic_output, &suffix));
    EXPECT_EQ(kanji_output, "一");
    EXPECT_EQ(arabic_output, "1");
    EXPECT_EQ(suffix, "個");
  }

  {
    constexpr absl::string_view kInput = "一万二十五個";
    std::string arabic_output, kanji_output, suffix;
    EXPECT_TRUE(NumberUtil::NormalizeNumbersWithSuffix(
        kInput, true, &kanji_output, &arabic_output, &suffix));
    EXPECT_EQ(kanji_output, "一万二十五");
    EXPECT_EQ(arabic_output, "10025");
    EXPECT_EQ(suffix, "個");
  }

  {
    constexpr absl::string_view kInput = "二百三五万一番目";
    std::string arabic_output, kanji_output, suffix;
    EXPECT_TRUE(NumberUtil::NormalizeNumbersWithSuffix(
        kInput, true, &kanji_output, &arabic_output, &suffix));
    EXPECT_EQ(kanji_output, "二百三五万一");
    EXPECT_EQ(arabic_output, "2350001");
    EXPECT_EQ(suffix, "番目");
  }

  {
    constexpr absl::string_view kInput = "てすと";
    std::string arabic_output, kanji_output, suffix;
    EXPECT_FALSE(NumberUtil::NormalizeNumbersWithSuffix(
        kInput, true, &kanji_output, &arabic_output, &suffix));
  }

  {
    constexpr absl::string_view kInput = "てすと２";
    std::string arabic_output, kanji_output, suffix;
    EXPECT_FALSE(NumberUtil::NormalizeNumbersWithSuffix(
        kInput, true, &kanji_output, &arabic_output, &suffix));
  }

  // Tests for numbers less than 10.
  {
    constexpr absl::string_view kInput = "零セット";
    std::string arabic_output, kanji_output, suffix;
    EXPECT_TRUE(NumberUtil::NormalizeNumbersWithSuffix(
        kInput, true, &kanji_output, &arabic_output, &suffix));
    EXPECT_EQ(kanji_output, "零");
    EXPECT_EQ(arabic_output, "0");
    EXPECT_EQ(suffix, "セット");
  }

  {
    constexpr absl::string_view kInput = "九０ぷよ";
    std::string arabic_output, kanji_output, suffix;
    EXPECT_TRUE(NumberUtil::NormalizeNumbersWithSuffix(
        kInput, true, &kanji_output, &arabic_output, &suffix));
    EXPECT_EQ(kanji_output, "九〇");
    EXPECT_EQ(arabic_output, "90");
    EXPECT_EQ(suffix, "ぷよ");
  }

  {
    constexpr absl::string_view kInput = "三五$";
    std::string arabic_output, kanji_output, suffix;
    EXPECT_TRUE(NumberUtil::NormalizeNumbersWithSuffix(
        kInput, true, &kanji_output, &arabic_output, &suffix));
    EXPECT_EQ(kanji_output, "三五");
    EXPECT_EQ(arabic_output, "35");
    EXPECT_EQ(suffix, "$");
  }

  {
    constexpr absl::string_view kInput =
        "二十三十に";  // same base appears twice
    std::string arabic_output, kanji_output, suffix;
    EXPECT_FALSE(NumberUtil::NormalizeNumbersWithSuffix(
        kInput, true, &kanji_output, &arabic_output, &suffix));
  }

  {
    constexpr absl::string_view kInput =
        "２，０００";  // separated number style
    std::string arabic_output, kanji_output, suffix;
    EXPECT_FALSE(NumberUtil::NormalizeNumbersWithSuffix(
        kInput, true, &kanji_output, &arabic_output, &suffix));
  }

  {
    constexpr absl::string_view kInput = "２時３０分";
    std::string arabic_output, kanji_output, suffix;
    EXPECT_FALSE(NumberUtil::NormalizeNumbersWithSuffix(
        kInput, true, &kanji_output, &arabic_output, &suffix));
  }
}

TEST(NumberUtilTest, ArabicToWideArabicTest) {
  absl::string_view arabic;
  std::vector<NumberUtil::NumberString> output;

  arabic = "12345";
  output.clear();
  EXPECT_TRUE(NumberUtil::ArabicToWideArabic(arabic, &output));
  ASSERT_EQ(output.size(), 2);
  EXPECT_EQ(output[0].value, "一二三四五");
  EXPECT_EQ(output[0].style, NumberUtil::NumberString::NUMBER_KANJI_ARABIC);
  EXPECT_EQ(output[1].value, "１２３４５");
  EXPECT_EQ(output[1].style, NumberUtil::NumberString::DEFAULT_STYLE);

  arabic = "00123";
  output.clear();
  EXPECT_TRUE(NumberUtil::ArabicToWideArabic(arabic, &output));
  ASSERT_EQ(output.size(), 2);
  EXPECT_EQ(output[0].value, "〇〇一二三");
  EXPECT_EQ(output[0].style, NumberUtil::NumberString::NUMBER_KANJI_ARABIC);
  EXPECT_EQ(output[1].value, "００１２３");
  EXPECT_EQ(output[1].style, NumberUtil::NumberString::DEFAULT_STYLE);

  arabic = "abcde";
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToWideArabic(arabic, &output));
  EXPECT_EQ(output.size(), 0);

  arabic = "012abc345";
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToWideArabic(arabic, &output));
  EXPECT_EQ(output.size(), 0);

  arabic = "0.001";
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToWideArabic(arabic, &output));
  EXPECT_EQ(output.size(), 0);

  arabic = "-100";
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToWideArabic(arabic, &output));
  EXPECT_EQ(output.size(), 0);

  arabic = "18446744073709551616";  // UINT_MAX + 1
  EXPECT_TRUE(NumberUtil::ArabicToWideArabic(arabic, &output));
  ASSERT_EQ(2, output.size());
  EXPECT_EQ(output[0].value, "一八四四六七四四〇七三七〇九五五一六一六");
  EXPECT_EQ(output[1].value, "１８４４６７４４０７３７０９５５１６１６");
  EXPECT_EQ(output[0].style, NumberUtil::NumberString::NUMBER_KANJI_ARABIC);
  EXPECT_EQ(output[1].style, NumberUtil::NumberString::DEFAULT_STYLE);
}

namespace {
constexpr int kMaxCandsInArabicToKanjiTest = 4;
struct ArabicToKanjiTestData {
  absl::string_view input;
  int expect_num;
  absl::string_view expect_value[kMaxCandsInArabicToKanjiTest];
  NumberUtil::NumberString::Style expect_style[kMaxCandsInArabicToKanjiTest];
};
}  // namespace

// ArabicToKanji TEST
TEST(NumberUtilTest, ArabicToKanjiTest) {
  constexpr NumberUtil::NumberString::Style kOldKanji =
      NumberUtil::NumberString::NUMBER_OLD_KANJI;
  constexpr NumberUtil::NumberString::Style kKanji =
      NumberUtil::NumberString::NUMBER_KANJI;
  constexpr NumberUtil::NumberString::Style kHalfArabicKanji =
      NumberUtil::NumberString::NUMBER_ARABIC_AND_KANJI_HALFWIDTH;
  constexpr NumberUtil::NumberString::Style kFullArabicKanji =
      NumberUtil::NumberString::NUMBER_ARABIC_AND_KANJI_FULLWIDTH;

  constexpr ArabicToKanjiTestData kData[] = {
      {"0", 1, {"零"}, {kOldKanji}},
      {"00000", 1, {"零"}, {kOldKanji}},
      {"2", 2, {"二", "弐"}, {kKanji, kOldKanji}},
      // "壱拾" is needed to avoid mistakes. Please refer http://b/6422355
      // for details.
      {"10", 3, {"十", "壱拾", "拾"}, {kKanji, kOldKanji, kOldKanji}},
      {"100", 2, {"百", "壱百"}, {kKanji, kOldKanji}},
      {"1000", 3, {"千", "壱阡", "阡"}, {kKanji, kOldKanji, kOldKanji}},
      {"20", 3, {"二十", "弐拾", "廿"}, {kKanji, kOldKanji, kOldKanji}},
      {"11111",
       4,
       {"1万1111", "１万１１１１", "一万千百十一", "壱萬壱阡壱百壱拾壱"},
       {kHalfArabicKanji, kFullArabicKanji, kKanji, kOldKanji}},
      {"12345",
       4,
       {"1万2345", "１万２３４５", "一万二千三百四十五", "壱萬弐阡参百四拾五"},
       {kHalfArabicKanji, kFullArabicKanji, kKanji, kOldKanji}},
      {"100002345",
       4,
       {"1億2345", "１億２３４５", "一億二千三百四十五", "壱億弐阡参百四拾五"},
       {kHalfArabicKanji, kFullArabicKanji, kKanji, kOldKanji}},
      {"18446744073709551615",
       4,
       {"1844京6744兆737億955万1615",
        "１８４４京６７４４兆７３７億９５５万１６１５",
        "千八百四十四京六千七百四十四兆七百三十七億九百五十五万千六百十五",
        "壱阡八百四拾四京六阡七百四拾四兆七百参拾七億九百五拾五萬壱阡六百壱拾"
        "五"},
       {kHalfArabicKanji, kFullArabicKanji, kKanji, kOldKanji}},
  };

  for (size_t i = 0; i < std::size(kData); ++i) {
    std::vector<NumberUtil::NumberString> output;
    ASSERT_LE(kData[i].expect_num, kMaxCandsInArabicToKanjiTest);
    EXPECT_TRUE(NumberUtil::ArabicToKanji(kData[i].input, &output));
    ASSERT_EQ(output.size(), kData[i].expect_num)
        << "on conversion of '" << kData[i].input << "'";
    for (int j = 0; j < kData[i].expect_num; ++j) {
      EXPECT_EQ(kData[i].expect_value[j], output[j].value)
          << "input : " << kData[i].input << "\nj : " << j;
      EXPECT_EQ(kData[i].expect_style[j], output[j].style)
          << "input : " << kData[i].input << "\nj : " << j;
    }
  }

  constexpr absl::string_view kFailInputs[] = {"asf56789", "0.001", "-100",
                                               "123456789012345678901"};
  for (size_t i = 0; i < std::size(kFailInputs); ++i) {
    std::vector<NumberUtil::NumberString> output;
    EXPECT_FALSE(NumberUtil::ArabicToKanji(kFailInputs[i], &output));
    ASSERT_EQ(output.size(), 0) << "input : " << kFailInputs[i];
  }
}

// ArabicToSeparatedArabic TEST
TEST(NumberUtilTest, ArabicToSeparatedArabicTest) {
  std::vector<NumberUtil::NumberString> output;

  // Test data expected to succeed
  constexpr absl::string_view kSuccess[][3] = {
      {"4", "4", "４"},
      {"123456789", "123,456,789", "１２３，４５６，７８９"},
      {"1234567.89", "1,234,567.89", "１，２３４，５６７．８９"},
      // UINT64_MAX + 1
      {"18446744073709551616", "18,446,744,073,709,551,616", ""},
  };

  for (size_t i = 0; i < std::size(kSuccess); ++i) {
    const absl::string_view arabic = kSuccess[i][0];
    output.clear();
    EXPECT_TRUE(NumberUtil::ArabicToSeparatedArabic(arabic, &output));
    ASSERT_EQ(output.size(), 2);
    EXPECT_EQ(output[0].value, kSuccess[i][1]);
    EXPECT_EQ(NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_HALFWIDTH,
              output[0].style);
    if (!kSuccess[i][2].empty()) {
      EXPECT_EQ(output[1].value, kSuccess[i][2]);
      EXPECT_EQ(NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH,
                output[1].style);
    }
  }

  // Test data expected to fail
  constexpr absl::string_view kFail[] = {
      "0123456789",
      "asdf0123456789",
      "0.001",
      "-100",
  };

  for (const absl::string_view arabic : kFail) {
    output.clear();
    EXPECT_FALSE(NumberUtil::ArabicToSeparatedArabic(arabic, &output));
    ASSERT_EQ(output.size(), 0);
  }
}

// ArabicToOtherForms
TEST(NumberUtilTest, ArabicToOtherFormsTest) {
  absl::string_view arabic;
  std::vector<NumberUtil::NumberString> output;

  arabic = "5";
  output.clear();
  EXPECT_TRUE(NumberUtil::ArabicToOtherForms(arabic, &output));
  ASSERT_EQ(output.size(), 5);

  EXPECT_EQ(output[0].value, "Ⅴ");
  EXPECT_EQ(output[0].style, NumberUtil::NumberString::NUMBER_ROMAN_CAPITAL);

  EXPECT_EQ(output[1].value, "ⅴ");
  EXPECT_EQ(output[1].style, NumberUtil::NumberString::NUMBER_ROMAN_SMALL);

  EXPECT_EQ(output[2].value, "⑤");
  EXPECT_EQ(output[2].style, NumberUtil::NumberString::NUMBER_CIRCLED);

  EXPECT_EQ(output[3].value, "⁵");
  EXPECT_EQ(output[3].style, NumberUtil::NumberString::NUMBER_SUPERSCRIPT);

  EXPECT_EQ(output[4].value, "₅");
  EXPECT_EQ(output[4].style, NumberUtil::NumberString::NUMBER_SUBSCRIPT);

  arabic = "0123456789";
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToOtherForms(arabic, &output));
  ASSERT_EQ(output.size(), 0);

  arabic = "asdf0123456789";
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToOtherForms(arabic, &output));
  ASSERT_EQ(output.size(), 0);

  arabic = "0.001";
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToOtherForms(arabic, &output));
  ASSERT_EQ(output.size(), 0);

  arabic = "-100";
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToOtherForms(arabic, &output));
  ASSERT_EQ(output.size(), 0);

  arabic = "18446744073709551616";  // UINT64_MAX + 1
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToOtherForms(arabic, &output));
}

// ArabicToOtherRadixes
TEST(NumberUtilTest, ArabicToOtherRadixesTest) {
  absl::string_view arabic;
  std::vector<NumberUtil::NumberString> output;

  arabic = "1";
  output.clear();
  // "1" is "1" in any radixes.
  EXPECT_FALSE(NumberUtil::ArabicToOtherRadixes(arabic, &output));

  arabic = "2";
  output.clear();
  EXPECT_TRUE(NumberUtil::ArabicToOtherRadixes(arabic, &output));
  ASSERT_EQ(output.size(), 1);

  arabic = "8";
  output.clear();
  EXPECT_TRUE(NumberUtil::ArabicToOtherRadixes(arabic, &output));
  ASSERT_EQ(output.size(), 2);
  EXPECT_EQ(output[0].value, "010");
  EXPECT_EQ(output[0].style, NumberUtil::NumberString::NUMBER_OCT);
  EXPECT_EQ(output[1].value, "0b1000");
  EXPECT_EQ(output[1].style, NumberUtil::NumberString::NUMBER_BIN);

  arabic = "16";
  output.clear();
  EXPECT_TRUE(NumberUtil::ArabicToOtherRadixes(arabic, &output));
  ASSERT_EQ(output.size(), 3);
  EXPECT_EQ(output[0].value, "0x10");
  EXPECT_EQ(output[0].style, NumberUtil::NumberString::NUMBER_HEX);
  EXPECT_EQ(output[1].value, "020");
  EXPECT_EQ(output[1].style, NumberUtil::NumberString::NUMBER_OCT);
  EXPECT_EQ(output[2].value, "0b10000");
  EXPECT_EQ(output[2].style, NumberUtil::NumberString::NUMBER_BIN);

  arabic = "asdf0123456789";
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToOtherRadixes(arabic, &output));
  ASSERT_EQ(output.size(), 0);

  arabic = "0.001";
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToOtherRadixes(arabic, &output));
  ASSERT_EQ(output.size(), 0);

  arabic = "-100";
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToOtherRadixes(arabic, &output));
  ASSERT_EQ(output.size(), 0);

  arabic = "18446744073709551616";  // UINT64_MAX + 1
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToOtherRadixes(arabic, &output));
}

}  // namespace
}  // namespace mozc
