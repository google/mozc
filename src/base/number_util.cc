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

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "base/japanese_util_rule.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/util.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"

namespace mozc {
namespace {

// Table of number character of Kansuji
const char *const kNumKanjiDigits[] = {"〇", "一", "二", "三", "四",   "五",
                                       "六", "七", "八", "九", nullptr};
const char *const kNumKanjiOldDigits[] = {nullptr, "壱", "弐", "参", "四",
                                          "五",    "六", "七", "八", "九"};
const char *const kNumFullWidthDigits[] = {"０", "１", "２", "３", "４",   "５",
                                           "６", "７", "８", "９", nullptr};
const char *const kNumHalfWidthDigits[] = {"0", "1", "2", "3", "4",    "5",
                                           "6", "7", "8", "9", nullptr};

// Table of Kanji number ranks
const char *const kNumKanjiRanks[] = {nullptr, "", "十", "百", "千"};
const char *const kNumKanjiBiggerRanks[] = {"", "万", "億", "兆", "京"};
const char *const kNumKanjiOldRanks[] = {nullptr, "", "拾", "百", "阡"};
const char *const kNumKanjiBiggerOldRanks[] = {"", "萬", "億", "兆", "京"};

const char *const kRomanNumbersCapital[] = {nullptr, "Ⅰ", "Ⅱ", "Ⅲ",    "Ⅳ",
                                            "Ⅴ",     "Ⅵ", "Ⅶ", "Ⅷ",    "Ⅸ",
                                            "Ⅹ",     "Ⅺ", "Ⅻ", nullptr};

const char *const kRomanNumbersSmall[] = {nullptr, "ⅰ", "ⅱ", "ⅲ",    "ⅳ",
                                          "ⅴ",     "ⅵ", "ⅶ", "ⅷ",    "ⅸ",
                                          "ⅹ",     "ⅺ", "ⅻ", nullptr};

const char *const kCircledNumbers[] = {
    nullptr, "①",  "②",  "③",  "④",  "⑤",  "⑥",  "⑦",    "⑧",  "⑨",  "⑩",
    "⑪",     "⑫",  "⑬",  "⑭",  "⑮",  "⑯",  "⑰",  "⑱",    "⑲",  "⑳",  "㉑",
    "㉒",    "㉓", "㉔", "㉕", "㉖", "㉗", "㉘", "㉙",   "㉚", "㉛", "㉜",
    "㉝",    "㉞", "㉟", "㊱", "㊲", "㊳", "㊴", "㊵",   "㊶", "㊷", "㊸",
    "㊹",    "㊺", "㊻", "㊼", "㊽", "㊾", "㊿", nullptr};

// Structure to store character set variations.
struct NumberStringVariation {
  const char *const *const digits;
  const int numbers_size;
  const char *description;
  const char *separator;
  const char *point;
  const NumberUtil::NumberString::Style style;
};

// Judges given string is a decimal number (including integer) or not.
// It accepts strings whose last point is a decimal point like "123456."
bool IsDecimalNumber(absl::string_view str) {
  int num_point = 0;
  for (size_t i = 0; i < str.size(); ++i) {
    if (str[i] == '.') {
      ++num_point;
      // A valid decimal number has at most one decimal point.
      if (num_point >= 2) {
        return false;
      }
    } else if (!isdigit(str[i])) {
      return false;
    }
  }

  return true;
}

const char kAsciiZero = '0';
const char kAsciiOne = '1';
const char kAsciiNine = '9';

}  // namespace

int NumberUtil::SimpleAtoi(absl::string_view str) {
  int integer;
  if (absl::SimpleAtoi(str, &integer)) {
    return integer;
  }
  return 0;
}

namespace {

// TODO(hidehiko): Refactoring with GetScriptType in Util class.
inline bool IsArabicDecimalChar32(char32 ucs4) {
  // Halfwidth digit.
  if (kAsciiZero <= ucs4 && ucs4 <= kAsciiNine) {
    return true;
  }

  // Fullwidth digit.
  if (0xFF10 <= ucs4 && ucs4 <= 0xFF19) {
    return true;
  }

  return false;
}

}  // namespace

bool NumberUtil::IsArabicNumber(absl::string_view input_string) {
  if (input_string.empty()) {
    return false;
  }
  for (ConstChar32Iterator iter(input_string); !iter.Done(); iter.Next()) {
    if (!IsArabicDecimalChar32(iter.Get())) {
      // Found non-Arabic decimal character.
      return false;
    }
  }

  // All characters are numbers.
  return true;
}

bool NumberUtil::IsDecimalInteger(absl::string_view str) {
  if (str.empty()) {
    return false;
  }
  for (size_t i = 0; i < str.size(); ++i) {
    if (!isdigit(str[i])) {
      return false;
    }
  }
  return true;
}

namespace {

// To know what "大字" means, please refer
// http://ja.wikipedia.org/wiki/%E5%A4%A7%E5%AD%97_(%E6%95%B0%E5%AD%97)
const NumberStringVariation kKanjiVariations[] = {
    {kNumHalfWidthDigits, 10, "数字", nullptr, nullptr,
     NumberUtil::NumberString::NUMBER_ARABIC_AND_KANJI_HALFWIDTH},
    {kNumFullWidthDigits, 10, "数字", nullptr, nullptr,
     NumberUtil::NumberString::NUMBER_ARABIC_AND_KANJI_FULLWIDTH},
    {kNumKanjiDigits, 10, "漢数字", nullptr, nullptr,
     NumberUtil::NumberString::NUMBER_KANJI},
    {kNumKanjiOldDigits, 10, "大字", nullptr, nullptr,
     NumberUtil::NumberString::NUMBER_OLD_KANJI},
};

const char kOldTwoTen[] = "弐拾";
const size_t kOldTwoTenLength = arraysize(kOldTwoTen) - 1;
const char kOldTwenty[] = "廿";

}  // namespace

bool NumberUtil::ArabicToKanji(absl::string_view input_num,
                               std::vector<NumberString> *output) {
  DCHECK(output);
  const char *const kNumZero = "零";
  const int kDigitsInBigRank = 4;

  if (!IsDecimalInteger(input_num)) {
    return false;
  }

  {
    // We don't convert a number starting with '0', other than 0 itself.
    absl::string_view::size_type i;
    for (i = 0; i < input_num.size() && input_num[i] == kAsciiZero; ++i) {
    }
    if (i == input_num.size()) {
      output->push_back(
          NumberString(kNumZero, "大字", NumberString::NUMBER_OLD_KANJI));
      return true;
    }
  }

  // If given number needs higher ranks than our expectations,
  // we don't convert it.
  if (arraysize(kNumKanjiBiggerRanks) * kDigitsInBigRank < input_num.size()) {
    return false;
  }

  // Fill '0' in the beginning of input_num to make its length
  // (N * kDigitsInBigRank).
  const int filled_zero_num =
      (kDigitsInBigRank - (input_num.size() % kDigitsInBigRank)) %
      kDigitsInBigRank;
  std::string input(filled_zero_num, kAsciiZero);
  input.append(input_num.data(), input_num.size());

  // Segment into kDigitsInBigRank-digits pieces
  std::vector<std::string> ranked_numbers;
  for (int i = static_cast<int>(input.size()) - kDigitsInBigRank; i >= 0;
       i -= kDigitsInBigRank) {
    ranked_numbers.push_back(input.substr(i, kDigitsInBigRank));
  }
  const size_t rank_size = ranked_numbers.size();

  for (size_t variation_index = 0;
       variation_index < arraysize(kKanjiVariations); ++variation_index) {
    const NumberStringVariation &variation = kKanjiVariations[variation_index];
    const char *const *const digits = variation.digits;
    const NumberString::Style style = variation.style;

    if (rank_size == 1 &&
        (style == NumberString::NUMBER_ARABIC_AND_KANJI_HALFWIDTH ||
         style == NumberString::NUMBER_ARABIC_AND_KANJI_FULLWIDTH)) {
      continue;
    }

    const char *const *ranks;
    const char *const *bigger_ranks;
    if (style == NumberString::NUMBER_OLD_KANJI) {
      ranks = kNumKanjiOldRanks;
      bigger_ranks = kNumKanjiBiggerOldRanks;
    } else {
      ranks = kNumKanjiRanks;
      bigger_ranks = kNumKanjiBiggerRanks;
    }

    // TODO(peria): Bring |result| out if it improves the performance.
    std::string result;

    // Converts each segment, and merges them with rank Kanjis.
    for (int rank = rank_size - 1; rank >= 0; --rank) {
      const std::string &segment = ranked_numbers[rank];
      std::string segment_result;
      bool leading = true;
      for (size_t i = 0; i < segment.size(); ++i) {
        if (leading && segment[i] == kAsciiZero) {
          continue;
        }

        leading = false;
        if (style == NumberString::NUMBER_ARABIC_AND_KANJI_HALFWIDTH ||
            style == NumberString::NUMBER_ARABIC_AND_KANJI_FULLWIDTH) {
          segment_result += digits[segment[i] - kAsciiZero];
        } else {
          if (segment[i] == kAsciiZero) {
            continue;
          }
          // In "大字" style, "壱" is also required on every rank.
          if (style == NumberString::NUMBER_OLD_KANJI ||
              i == kDigitsInBigRank - 1 || segment[i] != kAsciiOne) {
            segment_result += digits[segment[i] - kAsciiZero];
          }
          segment_result += ranks[kDigitsInBigRank - i];
        }
      }
      if (!segment_result.empty()) {
        result += segment_result + bigger_ranks[rank];
      }
    }

    const char *description = variation.description;
    // Add simply converted numbers.
    output->push_back(NumberString(result, description, style));

    // Add specialized style numbers.
    if (style == NumberString::NUMBER_OLD_KANJI) {
      size_t index = result.find(kOldTwoTen);
      if (index != std::string::npos) {
        std::string result2(result);
        do {
          result2.replace(index, kOldTwoTenLength, kOldTwenty);
          index = result2.find(kOldTwoTen, index);
        } while (index != std::string::npos);
        output->push_back(NumberString(result2, description, style));
      }

      // for single kanji
      if (input == "0010") {
        output->push_back(NumberString("拾", description, style));
      }
      if (input == "1000") {
        output->push_back(NumberString("阡", description, style));
      }
    }
  }

  return true;
}

namespace {

const NumberStringVariation kNumDigitsVariations[] = {
    {kNumHalfWidthDigits, 10, "数字", ",", ".",
     NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_HALFWIDTH},
    {kNumFullWidthDigits, 10, "数字", "，", "．",
     NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH},
};

}  // namespace

bool NumberUtil::ArabicToSeparatedArabic(absl::string_view input_num,
                                         std::vector<NumberString> *output) {
  DCHECK(output);

  if (!IsDecimalNumber(input_num)) {
    return false;
  }

  // Separate a number into an integral part and a fractional part.
  absl::string_view::size_type point_pos = input_num.find('.');
  if (point_pos == absl::string_view::npos) {
    point_pos = input_num.size();
  }
  const absl::string_view integer = input_num.substr(0, point_pos);
  // |fraction| has the decimal point with digits in fractional part.
  const absl::string_view fraction =
      input_num.substr(point_pos, input_num.size() - point_pos);

  // We don't add separator to number whose integral part starts with '0'
  if (integer[0] == kAsciiZero) {
    return false;
  }

  for (size_t i = 0; i < arraysize(kNumDigitsVariations); ++i) {
    const NumberStringVariation &variation = kNumDigitsVariations[i];
    const char *const *const digits = variation.digits;
    // TODO(peria): Bring |result| out if it improves the performance.
    std::string result;

    // integral part
    for (absl::string_view::size_type j = 0; j < integer.size(); ++j) {
      // We don't add separater first
      if (j != 0 && (integer.size() - j) % 3 == 0) {
        result.append(variation.separator);
      }
      const uint32_t d = static_cast<uint32_t>(integer[j] - kAsciiZero);
      if (d <= 9 && digits[d]) {
        result.append(digits[d]);
      }
    }

    // fractional part
    if (!fraction.empty()) {
      DCHECK_EQ(fraction[0], '.');
      result.append(variation.point);
      for (absl::string_view::size_type j = 1; j < fraction.size(); ++j) {
        result.append(digits[static_cast<int>(fraction[j] - kAsciiZero)]);
      }
    }

    output->push_back(
        NumberString(result, variation.description, variation.style));
  }
  return true;
}

namespace {

// use default for wide Arabic, because half/full width for
// normal number is learned by character form manager.
const NumberStringVariation kSingleDigitsVariations[] = {
    {kNumKanjiDigits, 10, "漢数字", nullptr, nullptr,
     NumberUtil::NumberString::NUMBER_KANJI_ARABIC},
    {kNumFullWidthDigits, 10, "数字", nullptr, nullptr,
     NumberUtil::NumberString::DEFAULT_STYLE},
};

}  // namespace

bool NumberUtil::ArabicToWideArabic(absl::string_view input_num,
                                    std::vector<NumberString> *output) {
  DCHECK(output);

  if (!IsDecimalInteger(input_num)) {
    return false;
  }

  for (size_t i = 0; i < arraysize(kSingleDigitsVariations); ++i) {
    const NumberStringVariation &variation = kSingleDigitsVariations[i];
    // TODO(peria): Bring |result| out if it improves the performance.
    std::string result;
    for (absl::string_view::size_type j = 0; j < input_num.size(); ++j) {
      result.append(
          variation.digits[static_cast<int>(input_num[j] - kAsciiZero)]);
    }
    if (!result.empty()) {
      output->push_back(
          NumberString(result, variation.description, variation.style));
    }
  }
  return true;
}

namespace {

const NumberStringVariation kSpecialNumericVariations[] = {
    {kRomanNumbersCapital, arraysize(kRomanNumbersCapital),
     "ローマ数字(大文字)", nullptr, nullptr,
     NumberUtil::NumberString::NUMBER_ROMAN_CAPITAL},
    {kRomanNumbersSmall, arraysize(kRomanNumbersSmall), "ローマ数字(小文字)",
     nullptr, nullptr, NumberUtil::NumberString::NUMBER_ROMAN_SMALL},
    {kCircledNumbers, arraysize(kCircledNumbers), "丸数字", nullptr, nullptr,
     NumberUtil::NumberString::NUMBER_CIRCLED},
};

}  // namespace

bool NumberUtil::ArabicToOtherForms(absl::string_view input_num,
                                    std::vector<NumberString> *output) {
  DCHECK(output);

  if (!IsDecimalInteger(input_num)) {
    return false;
  }

  bool converted = false;

  // Googol
  {
    // 10^100
    const char *const kNumGoogol =
        "100000000000000000000000000000000000000000000000000"
        "00000000000000000000000000000000000000000000000000";

    if (input_num == kNumGoogol) {
      output->push_back(
          NumberString("Googol", "", NumberString::DEFAULT_STYLE));
      converted = true;
    }
  }

  // Following conversions require uint64 number.
  uint64_t n;
  if (!SafeStrToUInt64(input_num, &n)) {
    return converted;
  }

  // Special forms
  for (size_t i = 0; i < arraysize(kSpecialNumericVariations); ++i) {
    const NumberStringVariation &variation = kSpecialNumericVariations[i];
    if (n < variation.numbers_size && variation.digits[n]) {
      output->push_back(NumberString(variation.digits[n], variation.description,
                                     variation.style));
      converted = true;
    }
  }

  return converted;
}

bool NumberUtil::ArabicToOtherRadixes(absl::string_view input_num,
                                      std::vector<NumberString> *output) {
  DCHECK(output);

  if (!IsDecimalInteger(input_num)) {
    return false;
  }

  uint64_t n;
  if (!SafeStrToUInt64(input_num, &n)) {
    return false;
  }

  // Hexadecimal
  if (n > 9) {
    const std::string hex = absl::StrFormat("0x%x", static_cast<uint64_t>(n));
    output->push_back(NumberString(hex, "16進数", NumberString::NUMBER_HEX));
  }

  // Octal
  if (n > 7) {
    const std::string oct = absl::StrFormat("0%o", static_cast<uint64_t>(n));
    output->push_back(NumberString(oct, "8進数", NumberString::NUMBER_OCT));
  }

  // Binary
  if (n > 1) {
    std::string binary;
    for (uint64_t num = n; num; num >>= 1) {
      binary.push_back(kAsciiZero + static_cast<char>(num & 0x1));
    }
    // "b0" will be "0b" in head of |binary|
    binary.append("b0");
    std::reverse(binary.begin(), binary.end());
    output->push_back(NumberString(binary, "2進数", NumberString::NUMBER_BIN));
  }

  return (n > 1);
}

namespace {

const absl::string_view SkipWhiteSpace(absl::string_view str) {
  absl::string_view::size_type i;
  for (i = 0; i < str.size() && isspace(str[i]); ++i) {
  }
  DCHECK(i == str.size() || !isspace(str[i]));
  return str.substr(i);
}

// There is an informative discussion about the overflow detection in
// "Hacker's Delight" (http://www.hackersdelight.org/basics.pdf)
//   2-12 'Overflow Detection'

// *output = arg1 + arg2
// return false when an integer overflow happens.
bool AddAndCheckOverflow(uint64_t arg1, uint64_t arg2, uint64_t *output) {
  *output = arg1 + arg2;
  if (arg2 > (std::numeric_limits<uint64_t>::max() - arg1)) {
    // overflow happens
    return false;
  }
  return true;
}

// *output = arg1 * arg2
// return false when an integer overflow happens.
bool MultiplyAndCheckOverflow(uint64_t arg1, uint64_t arg2, uint64_t *output) {
  *output = arg1 * arg2;
  if (arg1 != 0 && arg2 > (std::numeric_limits<uint64_t>::max() / arg1)) {
    // overflow happens
    return false;
  }
  return true;
}

// A simple wrapper of strtoull function. |c_str| must be terminated by '\0'.
inline uint64_t StrToUint64(const char *c_str, char **end_ptr, int base) {
#ifdef OS_WIN
  return _strtoui64(c_str, end_ptr, base);
#else   // OS_WIN
  return strtoull(c_str, end_ptr, base);
#endif  // OS_WIN
}

// Converts a string which describes a number into an uint64 value in |base|
// radix.  Does not convert octal or hexadecimal strings with "0" or "0x"
// suffixes.
bool SafeStrToUInt64WithBase(absl::string_view str, int base, uint64_t *value) {
  DCHECK(value);

  // Maximum possible length of number string, including terminating '\0'. Note
  // that the maximum possible length is achieved when str="111...11" (64
  // unities) and base=2.
  const size_t kMaxPossibleLength = 65;

  // Leading white spaces are allowed.
  const absl::string_view stripped_str = SkipWhiteSpace(str);
  if (stripped_str.empty() || stripped_str.size() >= kMaxPossibleLength) {
    return false;
  }
  // StrToUint64() does not check if the input is negative.  However, a leading
  // '+' is OK.
  if (stripped_str[0] == '-') {
    return false;
  }

  // Since absl::string_view doesn't end with '\0', we make a c-string on stack
  // here.
  char buf[kMaxPossibleLength];
  memcpy(buf, str.data(), str.size());
  buf[str.size()] = '\0';

  char *end_ptr = nullptr;
  errno = 0;
  *value = StrToUint64(buf, &end_ptr, base);
  if (errno != 0 || end_ptr == buf) {  // Failed to parse uint64.
    return false;
  }
  // Trailing white spaces are allowed.
  const absl::string_view trailing_str(end_ptr, buf + str.size() - end_ptr);
  return SkipWhiteSpace(trailing_str).empty();
}

template <typename T1, typename T2>
struct GenericFalseTypeArity2 {
  // TODO(yukawa): Use std::false_type once C++11 is enabled everywhere.
  static constexpr bool value = false;
};

template <typename SrcType, typename DestType>
bool SafeCast(SrcType src, DestType *dest) {
  static_assert(GenericFalseTypeArity2<SrcType, DestType>::value,
                "Shouldn't be used with implicit type conversion.");
  return false;
}

template <>
bool SafeCast(int64_t src, int16_t *dest) {
  if (src < static_cast<int64_t>(std::numeric_limits<int16_t>::min()) ||
      static_cast<int64_t>(std::numeric_limits<int16_t>::max()) < src) {
    return false;
  }
  *dest = static_cast<int16_t>(src);
  return true;
}

template <>
bool SafeCast(int64_t src, int32_t *dest) {
  if (src < static_cast<int64_t>(std::numeric_limits<int32_t>::min()) ||
      static_cast<int64_t>(std::numeric_limits<int32_t>::max()) < src) {
    return false;
  }
  *dest = static_cast<int32_t>(src);
  return true;
}

template <>
bool SafeCast(uint64_t src, int64_t *dest) {
  if (src > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
    return false;
  }
  *dest = static_cast<int64_t>(src);
  return true;
}

template <>
bool SafeCast(uint64_t src, uint16_t *dest) {
  if (src > static_cast<uint64_t>(std::numeric_limits<uint16_t>::max())) {
    return false;
  }
  *dest = static_cast<uint16_t>(src);
  return true;
}

template <>
bool SafeCast(uint64_t src, uint32_t *dest) {
  if (src > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
    return false;
  }
  *dest = static_cast<uint32_t>(src);
  return true;
}

template <typename SrcType, typename DestType>
bool SafeUnaryNegation(SrcType src, DestType *dest) {
  static_assert(GenericFalseTypeArity2<SrcType, DestType>::value,
                "Shouldn't be used with implicit type conversion.");
  return false;
}

template <>
bool SafeUnaryNegation(uint64_t src, int64_t *dest) {
  int64_t tmp = 0;
  if (!SafeCast(src, &tmp)) {
    if (src == 0x8000000000000000ul) {
      // This is an exceptional case. |src| isn't in the range of int64,
      // but |-src| is in the range.
      *dest = std::numeric_limits<int64_t>::min();
      return true;
    }
    return false;
  }
  *dest = -tmp;
  return true;
}

}  // namespace

bool NumberUtil::SafeStrToInt16(absl::string_view str, int16_t *value) {
  int64_t tmp;
  if (!SafeStrToInt64(str, &tmp)) {
    return false;
  }
  return SafeCast(tmp, value);
}

bool NumberUtil::SafeStrToInt32(absl::string_view str, int32_t *value) {
  int64_t tmp;
  if (!SafeStrToInt64(str, &tmp)) {
    return false;
  }
  return SafeCast(tmp, value);
}

bool NumberUtil::SafeStrToInt64(absl::string_view str, int64_t *value) {
  const absl::string_view stripped_str = SkipWhiteSpace(str);
  if (stripped_str.empty()) {
    return false;
  }
  uint64_t tmp;
  if (stripped_str[0] == '-') {
    absl::string_view opposite_str =
        stripped_str.substr(1, stripped_str.size() - 1);
    if (!SafeStrToUInt64WithBase(opposite_str, 10, &tmp)) {
      return false;
    }
    return SafeUnaryNegation(tmp, value);
  }
  if (!SafeStrToUInt64WithBase(str, 10, &tmp)) {
    return false;
  }
  return SafeCast(tmp, value);
}

bool NumberUtil::SafeStrToUInt16(absl::string_view str, uint16_t *value) {
  uint64_t tmp;
  if (!SafeStrToUInt64WithBase(str, 10, &tmp)) {
    return false;
  }
  return SafeCast(tmp, value);
}

bool NumberUtil::SafeStrToUInt32(absl::string_view str, uint32_t *value) {
  uint64_t tmp;
  if (!SafeStrToUInt64WithBase(str, 10, &tmp)) {
    return false;
  }
  return SafeCast(tmp, value);
}

bool NumberUtil::SafeHexStrToUInt32(absl::string_view str, uint32_t *value) {
  uint64_t tmp;
  if (!SafeStrToUInt64WithBase(str, 16, &tmp)) {
    return false;
  }
  return SafeCast(tmp, value);
}

bool NumberUtil::SafeOctStrToUInt32(absl::string_view str, uint32_t *value) {
  uint64_t tmp;
  if (!SafeStrToUInt64WithBase(str, 8, &tmp)) {
    return false;
  }
  return SafeCast(tmp, value);
}

bool NumberUtil::SafeStrToUInt64(absl::string_view str, uint64_t *value) {
  return SafeStrToUInt64WithBase(str, 10, value);
}

bool NumberUtil::SafeStrToDouble(absl::string_view str, double *value) {
  DCHECK(value);
  // Note that absl::string_view isn't terminated by '\0'.  However, since
  // strtod requires null-terminated string, we make a string here. If we have a
  // good estimate of the maximum possible length of the input string, we may be
  // able to use char buffer instead.  Note: const reference ensures the life of
  // this temporary string until the end!
  const std::string &s = std::string(str);
  const char *ptr = s.c_str();

  char *end_ptr;
  errno = 0;  // errno only gets set on errors
  // strtod of GCC accepts hexadecimal number like "0x1234", but that of
  // VisualC++ does not.
  // Note that strtod accepts white spaces at the beginning of the parameter.
  *value = std::strtod(ptr, &end_ptr);

  if (errno != 0 || ptr == end_ptr || std::isnan(*value) ||
      *value == std::numeric_limits<double>::infinity() ||
      *value == -std::numeric_limits<double>::infinity()) {
    return false;
  }
  // Trailing white spaces are allowed.
  const absl::string_view trailing_str(end_ptr, ptr + s.size() - end_ptr);
  return SkipWhiteSpace(trailing_str).empty();
}

namespace {

// Reduces leading digits less than 10 as their base10 interpretation, e.g.,
//   [1, 2, 3, 10, 100] => begin points to [10, 100], output = 123
// Returns false when overflow happened.
bool ReduceLeadingNumbersAsBase10System(
    std::vector<uint64_t>::const_iterator *begin,
    const std::vector<uint64_t>::const_iterator &end, uint64_t *output) {
  *output = 0;
  for (; *begin < end; ++*begin) {
    if (**begin >= 10) {
      return true;
    }
    // *output = *output * 10 + *it
    if (!MultiplyAndCheckOverflow(*output, 10, output) ||
        !AddAndCheckOverflow(*output, **begin, output)) {
      return false;
    }
  }
  return true;
}

// Interprets digits as base10 system, e.g.,
//   [1, 2, 3] => 123
//   [1, 2, 3, 10] => false
// Returns false if a number greater than 10 was found or overflow happened.
bool InterpretNumbersAsBase10System(const std::vector<uint64_t> &numbers,
                                    uint64_t *output) {
  auto begin = numbers.begin();
  const bool success =
      ReduceLeadingNumbersAsBase10System(&begin, numbers.end(), output);
  // Check if the whole numbers were reduced.
  return (success && begin == numbers.end());
}

// Reads a leading number in a sequence and advances the iterator. Returns false
// if the range is empty or the leading number is not less than 10.
bool ReduceOnesDigit(std::vector<uint64_t>::const_iterator *begin,
                     const std::vector<uint64_t>::const_iterator &end,
                     uint64_t *num) {
  if (*begin == end || **begin >= 10) {
    return false;
  }
  *num = **begin;
  ++*begin;
  return true;
}

// Given expected_base, 10, 100, or 1000, reads leading one or two numbers and
// calculates the number in the follwoing way:
//   Case: expected_base == 10
//     [10, ...] => 10
//     [2, 10, ...] => 20
//     [1, 10, ...] => error because we don't write "一十" in Japanese.
//     [20, ...] => 20 because "廿" is interpreted as 20.
//     [2, 0, ...] => 20
//   Case: expected_base == 100
//     [100, ...] => 100
//     [2, 100, ...] => 200
//     [1, 100, ...] => error because we don't write "一百" in Japanese.
//     [1, 2, 3, ...] => 123
//   Case: expected_base == 1000
//     [1000, ...] => 1000
//     [2, 1000, ...] => 2000
//     [1, 1000, ...] => 1000
//     [1, 2, 3, 4, ...] => 1234
bool ReduceDigitsHelper(std::vector<uint64_t>::const_iterator *begin,
                        const std::vector<uint64_t>::const_iterator &end,
                        uint64_t *num, const uint64_t expected_base) {
  // Skip leading zero(s).
  while (*begin != end && **begin == 0) {
    ++*begin;
  }
  if (*begin == end) {
    return false;
  }
  const uint64_t leading_number = **begin;

  // If the leading number is less than 10, e.g., patterns like [2, 10], we need
  // to check the next number.
  if (leading_number < 10) {
    if (end - *begin < 2) {
      return false;
    }
    const uint64_t next_number = *(*begin + 1);

    // If the next number is also less than 10, this pattern is like
    // [1, 2, ...] => 12. In this case, the result must be less than
    // 10 * expected_base.
    if (next_number < 10) {
      if (!ReduceLeadingNumbersAsBase10System(begin, end, num) ||
          *num >= expected_base * 10 || (*begin != end && **begin < 10000)) {
        *begin = end;  // Force to ignore the rest of the sequence.
        return false;
      }
      return true;
    }

    // Patterns like [2, 10, ...] and [1, 1000, ...].
    if (next_number != expected_base ||
        (leading_number == 1 && expected_base != 1000)) {
      return false;
    }
    *num = leading_number * expected_base;
    *begin += 2;
    return true;
  }

  // Patterns like [10, ...], [100, ...], [1000, ...], [20, ...]. The leading 20
  // is a special case for Kanji "廿".
  if (leading_number == expected_base ||
      (expected_base == 10 && leading_number == 20)) {
    *num = leading_number;
    ++*begin;
    return true;
  }
  return false;
}

inline bool ReduceTensDigit(std::vector<uint64_t>::const_iterator *begin,
                            const std::vector<uint64_t>::const_iterator &end,
                            uint64_t *num) {
  return ReduceDigitsHelper(begin, end, num, 10);
}

inline bool ReduceHundredsDigit(
    std::vector<uint64_t>::const_iterator *begin,
    const std::vector<uint64_t>::const_iterator &end, uint64_t *num) {
  return ReduceDigitsHelper(begin, end, num, 100);
}

inline bool ReduceThousandsDigit(
    std::vector<uint64_t>::const_iterator *begin,
    const std::vector<uint64_t>::const_iterator &end, uint64_t *num) {
  return ReduceDigitsHelper(begin, end, num, 1000);
}

// Reduces leading digits as a number less than 10000 and advances the
// iterator. For example:
//   [1, 1000, 2, 100, 3, 10, 4, 10000, ...]
//        => begin points to [10000, ...], num = 1234
//   [3, 100, 4, 100]
//        => error because same base number appears twice
bool ReduceNumberLessThan10000(std::vector<uint64_t>::const_iterator *begin,
                               const std::vector<uint64_t>::const_iterator &end,
                               uint64_t *num) {
  *num = 0;
  bool success = false;
  uint64_t n = 0;
  // Note: the following additions never overflow.
  if (ReduceThousandsDigit(begin, end, &n)) {
    *num += n;
    success = true;
  }
  if (ReduceHundredsDigit(begin, end, &n)) {
    *num += n;
    success = true;
  }
  if (ReduceTensDigit(begin, end, &n)) {
    *num += n;
    success = true;
  }
  if (ReduceOnesDigit(begin, end, &n)) {
    *num += n;
    success = true;
  }
  // If at least one reduce was successful, no number remains in the sequence or
  // the next number should be a base number greater than 1000 (e.g., 10000,
  // 100000, etc.). Strictly speaking, better to check **begin % 10 == 0.
  return success && (*begin == end || **begin >= 10000);
}

// Interprets a sequence of numbers in a Japanese reading way. For example:
//   "一万二千三百四十五" = [1, 10000, 2, 1000, 3, 100, 4, 10, 5] => 12345
// Base-10 numbers must be decreasing, i.e.,
//   "一十二百" = [1, 10, 2, 100] => error
bool InterpretNumbersInJapaneseWay(const std::vector<uint64_t> &numbers,
                                   uint64_t *output) {
  uint64_t last_base = std::numeric_limits<uint64_t>::max();
  auto begin = numbers.begin();
  *output = 0;
  do {
    uint64_t coef = 0;
    if (!ReduceNumberLessThan10000(&begin, numbers.end(), &coef)) {
      return false;
    }
    if (begin == numbers.end()) {
      return AddAndCheckOverflow(*output, coef, output);
    }
    if (*begin >= last_base) {
      return false;  // Increasing order of base-10 numbers.
    }
    // Safely performs *output += coef * *begin.
    uint64_t delta = 0;
    if (!MultiplyAndCheckOverflow(coef, *begin, &delta) ||
        !AddAndCheckOverflow(*output, delta, output)) {
      return false;
    }
    last_base = *begin++;
  } while (begin != numbers.end());

  return true;
}

// Interprets a sequence of numbers directly or in a Japanese reading way
// depending on the maximum number in the sequence.
bool NormalizeNumbersHelper(const std::vector<uint64_t> &numbers,
                            uint64_t *number_output) {
  const auto itr_max = std::max_element(numbers.begin(), numbers.end());
  if (itr_max == numbers.end()) {
    return false;  // numbers is empty
  }

  // When no scaling number is found, convert number directly.
  // For example, [5,4,3] => 543
  if (*itr_max < 10) {
    return InterpretNumbersAsBase10System(numbers, number_output);
  }
  return InterpretNumbersInJapaneseWay(numbers, number_output);
}

// TODO(peria): Do refactoring this method.
bool NormalizeNumbersInternal(absl::string_view input, bool trim_leading_zeros,
                              bool allow_suffix, std::string *kanji_output,
                              std::string *arabic_output, std::string *suffix) {
  DCHECK(kanji_output);
  DCHECK(arabic_output);
  const char *begin = input.data();
  const char *end = input.data() + input.size();
  std::vector<uint64_t> numbers;
  numbers.reserve(input.size());

  // Map Kanji number string to digits, e.g., "二百十一" -> [2, 100, 10, 1].
  // Simultaneously, constructs a Kanji number string.
  kanji_output->clear();
  arabic_output->clear();
  std::string kanji_char;

  while (begin < end) {
    size_t mblen = 0;
    const char32 wchar = Util::UTF8ToUCS4(begin, end, &mblen);
    kanji_char.assign(begin, mblen);

    std::string tmp;
    NumberUtil::KanjiNumberToArabicNumber(kanji_char, &tmp);

    uint64_t n = 0;
    if (!NumberUtil::SafeStrToUInt64(tmp, &n)) {
      break;
    }

    if (wchar >= 0x0030 && wchar <= 0x0039) {  // '0' <= wchar <= '9'
      kanji_char.assign(kNumKanjiDigits[wchar - 0x0030], 3);
    } else if (wchar >= 0xFF10 && wchar <= 0xFF19) {  // '０' <= wchar <= '９'
      kanji_char.assign(kNumKanjiDigits[wchar - 0xFF10], 3);
    }
    kanji_output->append(kanji_char);
    numbers.push_back(n);
    begin += mblen;
  }
  if (begin < end) {
    if (!allow_suffix) {
      return false;
    }
    DCHECK(suffix);
    suffix->assign(begin, end);
  }

  if (numbers.empty()) {
    return false;
  }

  // Try interpreting the sequence of digits.
  uint64_t n = 0;
  if (!NormalizeNumbersHelper(numbers, &n)) {
    return false;
  }

  if (!trim_leading_zeros) {
    // If |numbers| contains only k zeros, add (k - 1) zeros to the output.
    // Otherwise, add the same number of leading zeros.
    size_t num_zeros;
    for (num_zeros = 0; num_zeros < numbers.size(); ++num_zeros) {
      if (numbers[num_zeros] != 0) {
        break;
      }
    }
    if (num_zeros == numbers.size()) {
      --num_zeros;
    }
    arabic_output->append(num_zeros, kAsciiZero);
  }

  arabic_output->append(absl::StrFormat("%u", static_cast<uint64_t>(n)));
  return true;
}

}  // namespace

// Convert Kanji numbers into Arabic numbers:
// e.g. "百二十万" -> 1200000
bool NumberUtil::NormalizeNumbers(absl::string_view input,
                                  bool trim_leading_zeros,
                                  std::string *kanji_output,
                                  std::string *arabic_output) {
  return NormalizeNumbersInternal(input, trim_leading_zeros,
                                  false,  // allow_suffix
                                  kanji_output, arabic_output, nullptr);
}

bool NumberUtil::NormalizeNumbersWithSuffix(absl::string_view input,
                                            bool trim_leading_zeros,
                                            std::string *kanji_output,
                                            std::string *arabic_output,
                                            std::string *suffix) {
  return NormalizeNumbersInternal(input, trim_leading_zeros,
                                  true,  // allow_suffix
                                  kanji_output, arabic_output, suffix);
}

void NumberUtil::KanjiNumberToArabicNumber(absl::string_view input,
                                           std::string *output) {
  Util::ConvertUsingDoubleArray(
      japanese_util_rule::kanjinumber_to_arabicnumber_da,
      japanese_util_rule::kanjinumber_to_arabicnumber_table, input, output);
}

}  // namespace mozc
