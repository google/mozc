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

#ifndef MOZC_BASE_NUMBER_UTIL_H_
#define MOZC_BASE_NUMBER_UTIL_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"

namespace mozc {

// This class sets up utilities to manage strings including numbers like
// Arabic numbers, Roman numbers, Kanji numbers, and so on.
class NumberUtil {
 public:
  NumberUtil() = delete;
  NumberUtil(const NumberUtil&) = delete;
  NumberUtil& operator=(const NumberUtil&) = delete;
  // Converts the string to a number and return it.
  static int SimpleAtoi(absl::string_view str);

  // Returns true if the given input_string contains only number characters
  // (regardless of halfwidth or fullwidth).
  // False for empty string.
  static bool IsArabicNumber(absl::string_view input_string);

  // Returns true if the given str consists of only ASCII digits.
  // False for empty string.
  static bool IsDecimalInteger(absl::string_view str);

  struct NumberString {
   public:
    enum Style {
      DEFAULT_STYLE = 0,
      // 123,456,789
      NUMBER_SEPARATED_ARABIC_HALFWIDTH,
      // "１２３，４５６，７８９"
      NUMBER_SEPARATED_ARABIC_FULLWIDTH,
      // "123億456万7890"
      NUMBER_ARABIC_AND_KANJI_HALFWIDTH,
      // "１２３億４５６万７８９０"
      NUMBER_ARABIC_AND_KANJI_FULLWIDTH,
      // "一億二千三百四十五万六千七百八十九"
      NUMBER_KANJI,
      // "壱億弐千参百四拾五万六千七百八拾九"
      NUMBER_OLD_KANJI,
      // "ⅠⅡⅢ"
      NUMBER_ROMAN_CAPITAL,
      // "ⅰⅱⅲ"
      NUMBER_ROMAN_SMALL,
      // "①②③"
      NUMBER_CIRCLED,
      // "ニ〇〇"
      NUMBER_KANJI_ARABIC,
      // "0x4d2" (1234 in decimal)
      NUMBER_HEX,
      // "02322" (1234 in decimal)
      NUMBER_OCT,
      // "0b10011010010" (1234 in decimal)
      NUMBER_BIN,
      // "¹²³⁴⁵⁶⁷⁸⁹"
      NUMBER_SUPERSCRIPT,
      // "₁₂₃₄₅₆₇₈₉"
      NUMBER_SUBSCRIPT,
    };

    // description is string_view because it's mostly a static string.
    NumberString(std::string value, absl::string_view description, Style style)
        : value(std::move(value)), description(description), style(style) {}

    // Converted string
    std::string value;

    // Description of Converted String
    std::string description;

    // Converted Number Style
    Style style;
  };

  // Following five functions are main functions to convert number strings.
  // They receive two arguments:
  //   - input_num: a string consisting of Arabic numeric characters.
  //   - output: a vector consists of conveted results.
  // If |input_num| is invalid or cannot represent as the form, these
  // functions do nothing.  If a method finds more than one representations,
  // it pushes all candidates into the output.

  // Converts half-width Arabic number string to Kan-su-ji string.
  //   - input_num: a string which *must* be half-width number string.
  //   - output: function appends new representation into output vector.
  // value, desc and style are stored same size and same order.
  // if invalid string is set, this function do nothing.
  static bool ArabicToKanji(absl::string_view input_num,
                            std::vector<NumberString>* output);

  // Converts half-width Arabic number string to Separated Arabic string.
  // (e.g. 1234567890 are converted to 1,234,567,890)
  // Arguments are same as ArabicToKanji (above).
  static bool ArabicToSeparatedArabic(absl::string_view input_num,
                                      std::vector<NumberString>* output);

  // Converts half-width Arabic number string to full-width Arabic number
  // string.
  // Arguments are same as ArabicToKanji (above).
  static bool ArabicToWideArabic(absl::string_view input_num,
                                 std::vector<NumberString>* output);

  // Converts half-width Arabic number to various styles.
  // Arguments are same as ArabicToKanji (above).
  //   - Roman style (i) (ii) ...
  static bool ArabicToOtherForms(absl::string_view input_num,
                                 std::vector<NumberString>* output);

  // Converts half-width Arabic number to various radices (2,8,16).
  // Arguments are same as ArabicToKanji (above).
  // Excepted number of input digits is smaller than 20, but it can be
  // converted only if it can be stored in an unsigned 64-bit integer.
  static bool ArabicToOtherRadixes(absl::string_view input_num,
                                   std::vector<NumberString>* output);

  // Converts the string to a 32-/64-bit signed/unsigned int.  Returns true if
  // success or false if the string is in the wrong format.
  static bool SafeStrToInt16(absl::string_view str, int16_t* value);
  static bool SafeStrToUInt16(absl::string_view str, uint16_t* value);

  // Converts the string to a double.  Returns true if success or false if the
  // string is in the wrong format.
  // If |str| is a hexadecimal number like "0x1234", the result depends on
  // compiler.  It returns false when compiled by VisualC++.  On the other hand
  // it returns true and sets correct value when compiled by gcc.
  static bool SafeStrToDouble(absl::string_view str, double* value);

  // Convert Kanji numeric into Arabic numeric.
  // When the trim_leading_zeros is true, leading zeros for arabic_output
  // are trimmed off.
  // TODO(toshiyuki): This parameter is only applied for arabic_output now.
  //
  // Input: "2千五百"
  // kanji_output: "二千五百"
  // arabic output: 2500
  //
  // NormalizeNumbers() returns false if it finds non-number characters.
  // NormalizeNumbersWithSuffix() skips trailing non-number characters and
  // return them in "suffix".
  static bool NormalizeNumbers(absl::string_view input, bool trim_leading_zeros,
                               std::string* kanji_output,
                               std::string* arabic_output);

  static bool NormalizeNumbersWithSuffix(absl::string_view input,
                                         bool trim_leading_zeros,
                                         std::string* kanji_output,
                                         std::string* arabic_output,
                                         std::string* suffix);

  // Note: this function just does charcter-by-character conversion
  // "百二十" -> 10020
  static std::string KanjiNumberToArabicNumber(absl::string_view input);
};

}  // namespace mozc

#endif  // MOZC_BASE_NUMBER_UTIL_H_
