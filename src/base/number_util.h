// Copyright 2010-2013, Google Inc.
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

#include <string>
#include <vector>

#include "base/port.h"
#include "base/string_piece.h"

namespace mozc {

// This class sets up utilities to manage strings including numbers like
// Arabic numbers, Roman numbers, Kanji numbers, and so on.
class NumberUtil {
 public:
  // Convert the number to a string and append it to output.
  static string SimpleItoa(int32 number);

  // Convert the string to a number and return it.
  static int SimpleAtoi(StringPiece str);

  // Returns true if the given input_string contains only number characters
  // (regardless of halfwidth or fullwidth).
  // False for empty string.
  static bool IsArabicNumber(StringPiece input_string);

  // Returns true if the given str consists of only ASCII digits.
  // False for empty string.
  static bool IsDecimalInteger(StringPiece str);

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
    };

    NumberString(StringPiece value, StringPiece description, Style style)
        : value(value.as_string()),
          description(description.as_string()),
          style(style) {}

    // Converted string
    string value;

    // Description of Converted String
    string description;

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
  static bool ArabicToKanji(StringPiece input_num,
                            vector<NumberString> *output);

  // Converts half-width Arabic number string to Separated Arabic string.
  // (e.g. 1234567890 are converted to 1,234,567,890)
  // Arguments are same as ArabicToKanji (above).
  static bool ArabicToSeparatedArabic(StringPiece input_num,
                                      vector<NumberString> *output);

  // Converts half-width Arabic number string to full-width Arabic number
  // string.
  // Arguments are same as ArabicToKanji (above).
  static bool ArabicToWideArabic(StringPiece input_num,
                                 vector<NumberString> *output);

  // Converts half-width Arabic number to various styles.
  // Arguments are same as ArabicToKanji (above).
  //   - Roman style (i) (ii) ...
  static bool ArabicToOtherForms(StringPiece input_num,
                                 vector<NumberString> *output);

  // Converts half-width Arabic number to various radices (2,8,16).
  // Arguments are same as ArabicToKanji (above).
  // Excepted number of input digits is smaller than 20, but it can be
  // converted only if it can be stored in an unsigned 64-bit integer.
  static bool ArabicToOtherRadixes(StringPiece input_num,
                                   vector<NumberString> *output);

  // Converts the string to a 32-/64-bit unsigned int.  Returns true if success
  // or false if the string is in the wrong format.
  static bool SafeStrToUInt32(StringPiece str, uint32 *value);
  static bool SafeStrToUInt64(StringPiece str, uint64 *value);
  static bool SafeHexStrToUInt32(StringPiece str, uint32 *value);
  static bool SafeOctStrToUInt32(StringPiece str, uint32 *value);

  // Converts the string to a double.  Returns true if success or false if the
  // string is in the wrong format.
  // If |str| is a hexadecimal number like "0x1234", the result depends on
  // compiler.  It returns false when compiled by VisualC++.  On the other hand
  // it returns true and sets correct value when compiled by gcc.
  static bool SafeStrToDouble(StringPiece str, double *value);

  // Converts the string to a float. Returns true if success or false if the
  // string is in the wrong format.
  static bool SafeStrToFloat(StringPiece str, float *value);
  // Converts the string to a float.
  static float StrToFloat(StringPiece str) {
    float value;
    SafeStrToFloat(str, &value);
    return value;
  }

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
  static bool NormalizeNumbers(StringPiece input,
                               bool trim_leading_zeros,
                               string *kanji_output,
                               string *arabic_output);

  static bool NormalizeNumbersWithSuffix(StringPiece input,
                                         bool trim_leading_zeros,
                                         string *kanji_output,
                                         string *arabic_output,
                                         string *suffix);

  // Note: this function just does charcter-by-character conversion
  // "百二十" -> 10020
  static void KanjiNumberToArabicNumber(StringPiece input, string *output);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(NumberUtil);
};

}  // namespace mozc

#endif  // MOZC_BASE_NUMBER_UTIL_H_
