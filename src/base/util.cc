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

#include <algorithm>
#include <array>
#include <bit>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "base/strings/unicode.h"

#ifdef _WIN32
#include "base/win32/wide_char.h"
#endif  // _WIN32

namespace mozc {

ConstChar32Iterator::ConstChar32Iterator(absl::string_view utf8_string)
    : utf8_string_(utf8_string), current_(0), done_(false) {
  Next();
}

char32_t ConstChar32Iterator::Get() const {
  DCHECK(!done_);
  return current_;
}

void ConstChar32Iterator::Next() {
  if (!done_) {
    done_ = !Util::SplitFirstChar32(utf8_string_, &current_, &utf8_string_);
  }
}

bool ConstChar32Iterator::Done() const { return done_; }

ConstChar32ReverseIterator::ConstChar32ReverseIterator(
    absl::string_view utf8_string)
    : utf8_string_(utf8_string), current_(0), done_(false) {
  Next();
}

char32_t ConstChar32ReverseIterator::Get() const {
  DCHECK(!done_);
  return current_;
}

void ConstChar32ReverseIterator::Next() {
  if (!done_) {
    done_ = !Util::SplitLastChar32(utf8_string_, &utf8_string_, &current_);
  }
}

bool ConstChar32ReverseIterator::Done() const { return done_; }

namespace {

template <typename T>
void AppendUtf8CharsImpl(absl::string_view str, std::vector<T>& output) {
  const char* begin = str.data();
  const char* const end = str.data() + str.size();
  while (begin < end) {
    const size_t mblen = strings::OneCharLen(begin);
    output.emplace_back(begin, mblen);
    begin += mblen;
  }
  DCHECK_EQ(begin, end);
}

}  // namespace

std::vector<std::string> Util::SplitStringToUtf8Chars(absl::string_view str) {
  std::vector<std::string> output;
  AppendUtf8Chars(str, output);
  return output;
}

void Util::AppendUtf8Chars(absl::string_view str,
                           std::vector<std::string>& output) {
  AppendUtf8CharsImpl(str, output);
}

void Util::AppendUtf8Chars(absl::string_view str,
                           std::vector<absl::string_view>& output) {
  AppendUtf8CharsImpl(str, output);
}

// Grapheme is user-perceived character. It may contain multiple codepoints
// such as modifiers and variation squesnces (e.g. 神︀ = U+795E,U+FE00 [SVS]).
// Note, this function does not support full requirements of the grapheme
// specifications defined by Unicode.
// * https://www.unicode.org/reports/tr29/#Grapheme_Cluster_Boundaries
void Util::SplitStringToUtf8Graphemes(absl::string_view str,
                                      std::vector<std::string>* graphemes) {
  *graphemes = SplitStringToUtf8Chars(str);
  if (graphemes->size() <= 1) {
    return;
  }

  // Use U+0000 (NUL) to represent a codepoint which doesn't appear in any
  // grapheme clusters since it's always used standalone.
  constexpr char32_t kStandaloneCodepoint = 0x0000;
  char32_t prev = kStandaloneCodepoint;
  std::vector<std::string> new_graphemes;
  new_graphemes.reserve(graphemes->capacity());

  for (std::string& grapheme : *graphemes) {
    const char32_t codepoint = Util::Utf8ToCodepoint(grapheme);
    const bool is_dakuten = (codepoint == 0x3099 || codepoint == 0x309A);
    const bool is_svs = (0xFE00 <= codepoint && codepoint <= 0xFE0F);
    const bool is_ivs = (0xE0100 <= codepoint && codepoint <= 0xE01EF);

    // ED-8a. text presentation sequence
    // * https://www.unicode.org/reports/tr51/#def_text_presentation_sequence
    const bool is_text_presentation_sequence =
        (codepoint == 0xFE0E);  // VARIATION SELECTOR-15 (VS15)

    // ED-9a. emoji presentation sequence
    // * https://www.unicode.org/reports/tr51/#def_emoji_presentation_sequence
    const bool is_emoji_presentation_sequence =
        (codepoint == 0xFE0F);  // VARIATION SELECTOR-16 (VS16)

    // ED-13. emoji modifier sequence
    // * https://www.unicode.org/reports/tr51/#def_emoji_modifier_sequence
    const bool is_emoji_modifier_sequence =
        (codepoint >= 0x1F3FB && codepoint <= 0x1F3FF);  // \p{Emoji_Modifie}

    // ED-14. emoji flag sequence
    // * https://www.unicode.org/reports/tr51/#def_emoji_flag_sequence
    const bool is_emoji_flag_sequence =
        ((prev >= 0x1F1E6 && prev <= 0x1F1FF) &&
         (codepoint >= 0x1F1E6 &&
          codepoint <= 0x1F1FF));  // \p{Regional_Indicator}

    // ED-14.a. emoji tag sequence (ETS)
    // * https://www.unicode.org/reports/tr51/#def_emoji_tag_sequence
    const bool is_emoji_tag_sequence =
        ((codepoint >= 0xE0020 && codepoint <= 0xE007E) ||  // tag_spec
         (codepoint == 0xE007F));                           // tag_end

    // ED-14.c. emoji keycap sequence
    // * https://www.unicode.org/reports/tr51/#def_emoji_keycap_sequence
    const bool is_emoji_keycap_sequence =
        (prev == 0xFE0F && codepoint == 0x20E3);

    // ED-16. emoji zwj sequence
    // * https://www.unicode.org/reports/tr51/#def_emoji_zwj_sequence
    const bool is_emoji_zwj_sequence =
        (codepoint == 0x200D || prev == 0x200D);  // ZERO WIDTH JOINER (ZWJ)

    if (is_emoji_flag_sequence && !new_graphemes.empty()) {
      absl::StrAppend(&new_graphemes.back(), grapheme);
      // Reset prev with noop value in case that regional indicator codepoints
      // continues more than 2.
      prev = kStandaloneCodepoint;
      continue;
    }

    if ((is_dakuten || is_svs || is_ivs || is_text_presentation_sequence ||
         is_emoji_presentation_sequence || is_emoji_modifier_sequence ||
         is_emoji_tag_sequence || is_emoji_keycap_sequence ||
         is_emoji_zwj_sequence) &&
        !new_graphemes.empty()) {
      absl::StrAppend(&new_graphemes.back(), grapheme);
    } else {
      new_graphemes.push_back(std::move(grapheme));
    }
    prev = codepoint;
  }

  *graphemes = std::move(new_graphemes);
}

void Util::SplitCSV(absl::string_view input, std::vector<std::string>* output) {
  std::string tmp(input);
  char* str = tmp.data();

  char* eos = str + input.size();
  char* start = nullptr;
  char* end = nullptr;
  output->clear();

  while (str < eos) {
    while (*str == ' ' || *str == '\t') {
      ++str;
    }

    if (*str == '"') {
      start = ++str;
      end = start;
      for (; str < eos; ++str) {
        if (*str == '"') {
          str++;
          if (*str != '"') break;
        }
        *end++ = *str;
      }
      str = std::find(str, eos, ',');
    } else {
      start = str;
      str = std::find(str, eos, ',');
      end = str;
    }
    bool end_is_empty = false;
    if (*end == ',' && end == eos - 1) {
      end_is_empty = true;
    }
    output->emplace_back(start, end);
    if (end_is_empty) {
      output->push_back("");
    }

    ++str;
  }
}

// The offset value to transform the upper case character to the lower
// case.  The value comes from both of (0x0061 "a" - 0x0041 "A") and
// (0xFF41 "ａ" - 0xFF21 "Ａ").
namespace {
constexpr size_t kOffsetFromUpperToLower = 0x0020;
}

void Util::LowerString(std::string* str) {
  for (const UnicodeChar ch : Utf8AsUnicodeChar(*str)) {
    char32_t codepoint = ch.char32();
    // ('A' <= codepoint && codepoint <= 'Z') ||
    // ('Ａ' <= codepoint && codepoint <= 'Ｚ')
    if ((0x0041 <= codepoint && codepoint <= 0x005A) ||
        (0xFF21 <= codepoint && codepoint <= 0xFF3A)) {
      codepoint += kOffsetFromUpperToLower;
      const std::string utf8 = CodepointToUtf8(codepoint);
      // The size of upper case character must be equal to the source
      // lower case character.  The following check asserts it.
      if (utf8.size() != ch.utf8().size()) {
        LOG(ERROR) << "The generated size differs from the source.";
        return;
      }
      const size_t pos = ch.utf8().data() - str->data();
      str->replace(pos, ch.utf8().size(), utf8);
    }
  }
}

void Util::UpperString(std::string* str) {
  for (const UnicodeChar ch : Utf8AsUnicodeChar(*str)) {
    char32_t codepoint = ch.char32();
    // ('a' <= codepoint && codepoint <= 'z') ||
    // ('ａ' <= codepoint && codepoint <= 'ｚ')
    if ((0x0061 <= codepoint && codepoint <= 0x007A) ||
        (0xFF41 <= codepoint && codepoint <= 0xFF5A)) {
      codepoint -= kOffsetFromUpperToLower;
      const std::string utf8 = CodepointToUtf8(codepoint);
      // The size of upper case character must be equal to the source
      // lower case character.  The following check asserts it.
      if (utf8.size() != ch.utf8().size()) {
        LOG(ERROR) << "The generated size differs from the source.";
        return;
      }
      const size_t pos = ch.utf8().data() - str->data();
      str->replace(pos, ch.utf8().size(), utf8);
    }
  }
}

void Util::CapitalizeString(std::string* str) {
  auto first_str = std::string(Utf8SubString(*str, 0, 1));
  UpperString(&first_str);

  auto tailing_str = std::string(Utf8SubString(*str, 1, std::string::npos));
  LowerString(&tailing_str);

  str->assign(absl::StrCat(first_str, tailing_str));
}

bool Util::IsLowerAscii(absl::string_view s) {
  return absl::c_all_of(s, absl::ascii_islower);
}

bool Util::IsUpperAscii(absl::string_view s) {
  return absl::c_all_of(s, absl::ascii_isupper);
}

bool Util::IsCapitalizedAscii(absl::string_view s) {
  if (s.empty()) {
    return true;
  }
  if (absl::ascii_isupper(s.front())) {
    return IsLowerAscii(absl::ClippedSubstr(s, 1));
  }
  return false;
}

namespace {

bool IsUtf8TrailingByte(uint8_t c) { return (c & 0xc0) == 0x80; }

}  // namespace

size_t Util::CharsLen(absl::string_view str) {
  size_t length = 0;
  while (!str.empty()) {
    ++length;
    str = absl::ClippedSubstr(str, strings::OneCharLen(str.begin()));
  }
  return length;
}

std::u32string Util::Utf8ToUtf32(absl::string_view str) {
  std::u32string codepoints;
  char32_t codepoint;
  while (Util::SplitFirstChar32(str, &codepoint, &str)) {
    codepoints.push_back(codepoint);
  }
  return codepoints;
}

std::string Util::Utf32ToUtf8(const std::u32string_view str) {
  std::string output;
  for (const char32_t codepoint : str) {
    CodepointToUtf8Append(codepoint, &output);
  }
  return output;
}

char32_t Util::Utf8ToCodepoint(const char* begin, const char* end,
                               size_t* mblen) {
  absl::string_view s(begin, end - begin);
  absl::string_view rest;
  char32_t c = 0;
  if (!Util::SplitFirstChar32(s, &c, &rest)) {
    *mblen = 0;
    return 0;
  }
  *mblen = rest.data() - s.data();
  return c;
}

bool Util::SplitFirstChar32(absl::string_view s, char32_t* first_char32,
                            absl::string_view* rest) {
  char32_t dummy_char32 = 0;
  if (first_char32 == nullptr) {
    first_char32 = &dummy_char32;
  }
  absl::string_view dummy_rest;
  if (rest == nullptr) {
    rest = &dummy_rest;
  }

  *first_char32 = 0;
  *rest = absl::string_view();

  if (s.empty()) {
    return false;
  }

  char32_t result = 0;
  size_t len = 0;
  char32_t min_value = 0;
  char32_t max_value = 0xffffffff;
  {
    const uint8_t leading_byte = static_cast<uint8_t>(s[0]);
    if (leading_byte < 0x80) {
      *first_char32 = leading_byte;
      *rest = absl::ClippedSubstr(s, 1);
      return true;
    }

    if (IsUtf8TrailingByte(leading_byte)) {
      // UTF-8 sequence should not start trailing bytes.
      return false;
    }

    if ((leading_byte & 0xe0) == 0xc0) {
      len = 2;
      min_value = 0x0080;
      max_value = 0x07ff;
      result = (leading_byte & 0x1f);
    } else if ((leading_byte & 0xf0) == 0xe0) {
      len = 3;
      min_value = 0x0800;
      max_value = 0xffff;
      result = (leading_byte & 0x0f);
    } else if ((leading_byte & 0xf8) == 0xf0) {
      len = 4;
      min_value = 0x010000;
      max_value = 0x1fffff;
      result = (leading_byte & 0x07);
      // Below is out of UCS4 but acceptable in 32-bit.
    } else if ((leading_byte & 0xfc) == 0xf8) {
      len = 5;
      min_value = 0x0200000;
      max_value = 0x3ffffff;
      result = (leading_byte & 0x03);
    } else if ((leading_byte & 0xfe) == 0xfc) {
      len = 6;
      min_value = 0x4000000;
      max_value = 0x7fffffff;
      result = (leading_byte & 0x01);
    } else {
      // Currently 0xFE and 0xFF are treated as invalid.
      return false;
    }
  }

  if (s.size() < len) {
    // Data length is too short.
    return false;
  }

  for (size_t i = 1; i < len; ++i) {
    const uint8_t c = static_cast<uint8_t>(s[i]);
    if (!IsUtf8TrailingByte(c)) {
      // Trailing bytes not found.
      return false;
    }
    result <<= 6;
    result += (c & 0x3f);
  }
  if ((result < min_value) || (max_value < result)) {
    // redundant UTF-8 sequence found.
    return false;
  }
  *first_char32 = result;
  *rest = absl::ClippedSubstr(s, len);
  return true;
}

bool Util::SplitLastChar32(absl::string_view s, absl::string_view* rest,
                           char32_t* last_char32) {
  absl::string_view dummy_rest;
  if (rest == nullptr) {
    rest = &dummy_rest;
  }
  char32_t dummy_char32 = 0;
  if (last_char32 == nullptr) {
    last_char32 = &dummy_char32;
  }

  *last_char32 = 0;
  *rest = absl::string_view();

  if (s.empty()) {
    return false;
  }
  absl::string_view::const_reverse_iterator it = s.rbegin();
  for (; (it != s.rend()) && IsUtf8TrailingByte(*it); ++it) {
  }
  if (it == s.rend()) {
    return false;
  }
  const absl::string_view::difference_type len =
      std::distance(s.rbegin(), it) + 1;
  const absl::string_view last_piece = absl::ClippedSubstr(s, s.size() - len);
  absl::string_view result_piece;
  if (!SplitFirstChar32(last_piece, last_char32, &result_piece)) {
    return false;
  }
  if (!result_piece.empty()) {
    return false;
  }
  *rest = s;
  rest->remove_suffix(len);
  return true;
}

bool Util::IsValidUtf8(absl::string_view s) {
  char32_t first;
  absl::string_view rest;
  while (!s.empty()) {
    if (!SplitFirstChar32(s, &first, &rest)) {
      return false;
    }
    s = rest;
  }
  return true;
}

std::string Util::CodepointToUtf8(char32_t c) {
  std::string output;
  CodepointToUtf8Append(c, &output);
  return output;
}

void Util::CodepointToUtf8Append(char32_t c, std::string* output) {
  output->reserve(output->size() + 6);
  char buf[7];
  const size_t mblen = CodepointToUtf8(c, buf);
  absl::StrAppend(output, absl::string_view(buf, mblen));
}

size_t Util::CodepointToUtf8(char32_t c, char* output) {
  if (c == 0) {
    // Do nothing if |c| is `\0`. Previous implementation of
    // CodepointToUtf8Append worked like this.
    output[0] = '\0';
    return 0;
  }
  if (c < 0x00080) {
    output[0] = static_cast<char>(c & 0xFF);
    output[1] = '\0';
    return 1;
  }
  if (c < 0x00800) {
    output[0] = static_cast<char>(0xC0 + ((c >> 6) & 0x1F));
    output[1] = static_cast<char>(0x80 + (c & 0x3F));
    output[2] = '\0';
    return 2;
  }
  if (c < 0x10000) {
    output[0] = static_cast<char>(0xE0 + ((c >> 12) & 0x0F));
    output[1] = static_cast<char>(0x80 + ((c >> 6) & 0x3F));
    output[2] = static_cast<char>(0x80 + (c & 0x3F));
    output[3] = '\0';
    return 3;
  }
  if (c < 0x200000) {
    output[0] = static_cast<char>(0xF0 + ((c >> 18) & 0x07));
    output[1] = static_cast<char>(0x80 + ((c >> 12) & 0x3F));
    output[2] = static_cast<char>(0x80 + ((c >> 6) & 0x3F));
    output[3] = static_cast<char>(0x80 + (c & 0x3F));
    output[4] = '\0';
    return 4;
  }
  // below is not in UCS4 but in 32bit int.
  if (c < 0x8000000) {
    output[0] = static_cast<char>(0xF8 + ((c >> 24) & 0x03));
    output[1] = static_cast<char>(0x80 + ((c >> 18) & 0x3F));
    output[2] = static_cast<char>(0x80 + ((c >> 12) & 0x3F));
    output[3] = static_cast<char>(0x80 + ((c >> 6) & 0x3F));
    output[4] = static_cast<char>(0x80 + (c & 0x3F));
    output[5] = '\0';
    return 5;
  }
  output[0] = static_cast<char>(0xFC + ((c >> 30) & 0x01));
  output[1] = static_cast<char>(0x80 + ((c >> 24) & 0x3F));
  output[2] = static_cast<char>(0x80 + ((c >> 18) & 0x3F));
  output[3] = static_cast<char>(0x80 + ((c >> 12) & 0x3F));
  output[4] = static_cast<char>(0x80 + ((c >> 6) & 0x3F));
  output[5] = static_cast<char>(0x80 + (c & 0x3F));
  output[6] = '\0';
  return 6;
}

absl::string_view Util::Utf8SubString(absl::string_view src, size_t start) {
  const char* begin = src.data();
  const char* end = begin + src.size();
  for (size_t i = 0; i < start && begin < end; ++i) {
    begin += strings::OneCharLen(begin);
  }
  const size_t prefix_len = begin - src.data();
  return absl::string_view(begin, src.size() - prefix_len);
}

absl::string_view Util::Utf8SubString(absl::string_view src, size_t start,
                                      size_t length) {
  src = Utf8SubString(src, start);
  size_t l = length;
  const char* substr_end = src.data();
  const char* const end = src.data() + src.size();
  while (l > 0 && substr_end < end) {
    substr_end += strings::OneCharLen(substr_end);
    --l;
  }
  return absl::string_view(src.data(), substr_end - src.data());
}

void Util::Utf8SubString(absl::string_view src, size_t start, size_t length,
                         std::string* result) {
  DCHECK(result);
  const absl::string_view substr = Utf8SubString(src, start, length);
  result->assign(substr.data(), substr.size());
}

absl::string_view Util::StripUtf8Bom(absl::string_view line) {
  static constexpr char kUtf8Bom[] = "\xef\xbb\xbf";
  return absl::StripPrefix(line, kUtf8Bom);
}

bool Util::IsUtf16Bom(absl::string_view line) {
  static constexpr char kUtf16LeBom[] = "\xff\xfe";
  static constexpr char kUtf16BeBom[] = "\xfe\xff";
  if (line.size() >= 2 &&
      (line.substr(0, 2) == kUtf16LeBom || line.substr(0, 2) == kUtf16BeBom)) {
    return true;
  }
  return false;
}

bool Util::ChopReturns(std::string* line) {
  const std::string::size_type line_end = line->find_last_not_of("\r\n");
  if (line_end + 1 != line->size()) {
    line->erase(line_end + 1);
    return true;
  }
  return false;
}

namespace {

// A sorted array of opening and closing brackets.
// * Both `open` and `close` bracket must be sorted.
// * Both `open` and `close` bracket must be the same size.
// If you add a new bracket pair, you must keep this property.
constexpr std::array<absl::string_view, 20> kSortedBrackets{
    "()",    // U+0028 U+0029
    "[]",    // U+005B U+005D
    "{}",    // U+007B U+007D
    "«»",    // U+00AB U+00BB
    "‘’",    // U+2018 U+2019
    "“”",    // U+201C U+201D
    "‹›",    // U+2039 U+203A
    "〈〉",  // U+3008 U+3009
    "《》",  // U+300A U+300B
    "「」",  // U+300C U+300D
    "『』",  // U+300E U+300F
    "【】",  // U+3010 U+3011
    "〔〕",  // U+3014 U+3015
    "〘〙",  // U+3018 U+3019
    "〚〛",  // U+301A U+301B
    "〝〟",  // U+301D U+301F
    "（）",  // U+FF08 U+FF09
    "［］",  // U+FF3B U+FF3D
    "｛｝",  // U+FF5B U+FF5D
    "｢｣",    // U+FF62 U+FF63
};

absl::string_view OpenBracket(absl::string_view pair) {
  return pair.substr(0, pair.size() / 2);
}

absl::string_view CloseBracket(absl::string_view pair) {
  return pair.substr(pair.size() / 2);
}

}  // namespace

bool Util::IsOpenBracket(absl::string_view key,
                         absl::string_view* close_bracket) {
  const auto end = kSortedBrackets.end();
  const auto iter =
      std::lower_bound(kSortedBrackets.begin(), end, key,
                       [](absl::string_view pair, absl::string_view key) {
                         return OpenBracket(pair) < key;
                       });
  if (iter == end || OpenBracket(*iter) != key) {
    return false;
  }
  *close_bracket = CloseBracket(*iter);
  return true;
}

bool Util::IsCloseBracket(absl::string_view key,
                          absl::string_view* open_bracket) {
  const auto end = kSortedBrackets.end();
  const auto iter =
      std::lower_bound(kSortedBrackets.begin(), end, key,
                       [](absl::string_view pair, absl::string_view key) {
                         return CloseBracket(pair) < key;
                       });
  if (iter == end || CloseBracket(*iter) != key) {
    return false;
  }
  *open_bracket = OpenBracket(*iter);
  return true;
}

bool Util::IsBracketPairText(absl::string_view input) {
  // The characters like ", ' <, > and ` are not always considered as "brackets"
  // themselves but they also have bracket-like usages by doubling, or coupling
  // with the corresponding characters. (e.g. "", '', <>, ``)
  // This function considers such character sequences as "bracket pairs".
  static const std::array<absl::string_view, 4> kAdditionalBracketPairs{
      "\"\"", "''", "<>", "``"};  // sorted
  const auto iter0 = std::lower_bound(kAdditionalBracketPairs.begin(),
                                      kAdditionalBracketPairs.end(), input);
  if (iter0 != kAdditionalBracketPairs.end() && *iter0 == input) {
    return true;
  }

  const auto iter1 =
      std::lower_bound(kSortedBrackets.begin(), kSortedBrackets.end(), input);
  if (iter1 != kSortedBrackets.end() && *iter1 == input) {
    return true;
  }

  return false;
}

bool Util::IsFullWidthSymbolInHalfWidthKatakana(absl::string_view input) {
  for (ConstChar32Iterator iter(input); !iter.Done(); iter.Next()) {
    switch (iter.Get()) {
      case 0x3002:  // FULLSTOP "。"
      case 0x300C:  // LEFT CORNER BRACKET "「"
      case 0x300D:  // RIGHT CORNER BRACKET "」"
      case 0x3001:  // COMMA "、"
      case 0x30FB:  // MIDDLE DOT "・"
      case 0x30FC:  // SOUND_MARK "ー"
      case 0x3099:  // VOICE SOUND MARK "゙"
      case 0x309A:  // SEMI VOICE SOUND MARK "゚"
        break;
      default:
        return false;
    }
  }
  return true;
}

bool Util::IsHalfWidthKatakanaSymbol(absl::string_view input) {
  for (ConstChar32Iterator iter(input); !iter.Done(); iter.Next()) {
    switch (iter.Get()) {
      case 0xFF61:  // FULLSTOP "｡"
      case 0xFF62:  // LEFT CORNER BRACKET "｢"
      case 0xFF63:  // RIGHT CORNER BRACKET "｣"
      case 0xFF64:  // COMMA "､"
      case 0xFF65:  // MIDDLE DOT "･"
      case 0xFF70:  // SOUND_MARK "ｰ"
      case 0xFF9E:  // VOICE SOUND MARK "ﾞ"
      case 0xFF9F:  // SEMI VOICE SOUND MARK "ﾟ"
        break;
      default:
        return false;
    }
  }
  return true;
}

bool Util::IsKanaSymbolContained(absl::string_view input) {
  for (ConstChar32Iterator iter(input); !iter.Done(); iter.Next()) {
    switch (iter.Get()) {
      case 0x3002:  // FULLSTOP "。"
      case 0x300C:  // LEFT CORNER BRACKET "「"
      case 0x300D:  // RIGHT CORNER BRACKET "」"
      case 0x3001:  // COMMA "、"
      case 0x30FB:  // MIDDLE DOT "・"
      case 0x30FC:  // SOUND_MARK "ー"
      case 0x3099:  // VOICE SOUND MARK "゙"
      case 0x309A:  // SEMI VOICE SOUND MARK "゚"
      case 0xFF61:  // FULLSTOP "｡"
      case 0xFF62:  // LEFT CORNER BRACKET "｢"
      case 0xFF63:  // RIGHT CORNER BRACKET "｣"
      case 0xFF64:  // COMMA "､"
      case 0xFF65:  // MIDDLE DOT "･"
      case 0xFF70:  // SOUND_MARK "ｰ"
      case 0xFF9E:  // VOICE SOUND MARK "ﾞ"
      case 0xFF9F:  // SEMI VOICE SOUND MARK "ﾟ"
        return true;
    }
  }
  return false;
}

bool Util::IsEnglishTransliteration(absl::string_view value) {
  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] == 0x20 || value[i] == 0x21 || value[i] == 0x27 ||
        value[i] == 0x2D ||
        // " ", "!", "'", "-"
        (value[i] >= 0x41 && value[i] <= 0x5A) ||  // A..Z
        (value[i] >= 0x61 && value[i] <= 0x7A)) {  // a..z
      // do nothing
    } else {
      return false;
    }
  }
  return true;
}

#define INRANGE(w, a, b) ((w) >= (a) && (w) <= (b))

// script type
// TODO(yukawa, team): Make a mechanism to keep this classifier up-to-date
//   based on the original data from Unicode.org.
Util::ScriptType Util::GetScriptType(char32_t codepoint) {
  if (INRANGE(codepoint, 0x0030, 0x0039) ||  // ascii number
      INRANGE(codepoint, 0xFF10, 0xFF19)) {  // full width number
    return NUMBER;
  } else if (INRANGE(codepoint, 0x0041, 0x005A) ||  // ascii upper
             INRANGE(codepoint, 0x0061, 0x007A) ||  // ascii lower
             INRANGE(codepoint, 0xFF21, 0xFF3A) ||  // fullwidth ascii upper
             INRANGE(codepoint, 0xFF41, 0xFF5A)) {  // fullwidth ascii lower
    return ALPHABET;
  } else if (codepoint == 0x3005 ||  // IDEOGRAPHIC ITERATION MARK "々"
             INRANGE(codepoint, 0x3400,
                     0x4DBF) ||  // CJK Unified Ideographs Extension A
             INRANGE(codepoint, 0x4E00, 0x9FFF) ||  // CJK Unified Ideographs
             INRANGE(codepoint, 0xF900,
                     0xFAFF) ||  // CJK Compatibility Ideographs
             INRANGE(codepoint, 0x20000,
                     0x2A6DF) ||  // CJK Unified Ideographs Extension B
             INRANGE(codepoint, 0x2A700,
                     0x2B73F) ||  // CJK Unified Ideographs Extension C
             INRANGE(codepoint, 0x2B740,
                     0x2B81F) ||  // CJK Unified Ideographs Extension D
             INRANGE(codepoint, 0x2F800,
                     0x2FA1F)) {  // CJK Compatibility Ideographs
    // As of Unicode 6.0.2, each block has the following characters assigned.
    // [U+3400, U+4DB5]:   CJK Unified Ideographs Extension A
    // [U+4E00, U+9FCB]:   CJK Unified Ideographs
    // [U+4E00, U+FAD9]:   CJK Compatibility Ideographs
    // [U+20000, U+2A6D6]: CJK Unified Ideographs Extension B
    // [U+2A700, U+2B734]: CJK Unified Ideographs Extension C
    // [U+2B740, U+2B81D]: CJK Unified Ideographs Extension D
    // [U+2F800, U+2FA1D]: CJK Compatibility Ideographs
    return KANJI;
  } else if (INRANGE(codepoint, 0x3041, 0x309F) ||  // hiragana
             codepoint == 0x1B001) {  // HIRAGANA LETTER ARCHAIC YE
    return HIRAGANA;
  } else if (INRANGE(codepoint, 0x30A1, 0x30FF) ||  // full width katakana
             INRANGE(codepoint, 0x31F0,
                     0x31FF) ||  // Katakana Phonetic Extensions for Ainu
             INRANGE(codepoint, 0xFF65, 0xFF9F) ||  // half width katakana
             codepoint == 0x1B000) {                // KATAKANA LETTER ARCHAIC E
    return KATAKANA;
  } else if (INRANGE(codepoint, 0x02300, 0x023F3) ||  // Miscellaneous Technical
             INRANGE(codepoint, 0x02700, 0x027BF) ||  // Dingbats
             INRANGE(codepoint, 0x1F000, 0x1F02F) ||  // Mahjong tiles
             INRANGE(codepoint, 0x1F030, 0x1F09F) ||  // Domino tiles
             INRANGE(codepoint, 0x1F0A0, 0x1F0FF) ||  // Playing cards
             INRANGE(codepoint, 0x1F100,
                     0x1F2FF) ||  // Enclosed Alphanumeric Supplement
             INRANGE(codepoint, 0x1F200,
                     0x1F2FF) ||  // Enclosed Ideographic Supplement
             INRANGE(codepoint, 0x1F300,
                     0x1F5FF) ||  // Miscellaneous Symbols And Pictographs
             INRANGE(codepoint, 0x1F600, 0x1F64F) ||  // Emoticons
             INRANGE(codepoint, 0x1F680,
                     0x1F6FF) ||  // Transport And Map Symbols
             INRANGE(codepoint, 0x1F700, 0x1F77F) ||  // Alchemical Symbols
             codepoint == 0x26CE) {                   // Ophiuchus
    return EMOJI;
  }

  return UNKNOWN_SCRIPT;
}

Util::FormType Util::GetFormType(char32_t codepoint) {
  // 'Unicode Standard Annex #11: EAST ASIAN WIDTH'
  // http://www.unicode.org/reports/tr11/

  // Characters marked as 'Na' in
  // http://www.unicode.org/Public/UNIDATA/EastAsianWidth.txt
  if (INRANGE(codepoint, 0x0020, 0x007F) ||  // ascii
      INRANGE(codepoint, 0x27E6, 0x27ED) ||  // narrow mathematical symbols
      INRANGE(codepoint, 0x2985, 0x2986)) {  // narrow white parentheses
    return HALF_WIDTH;
  }

  // Other characters marked as 'Na' in
  // http://www.unicode.org/Public/UNIDATA/EastAsianWidth.txt
  if (INRANGE(codepoint, 0x00A2, 0x00AF)) {
    switch (codepoint) {
      case 0x00A2:  // CENT SIGN
      case 0x00A3:  // POUND SIGN
      case 0x00A5:  // YEN SIGN
      case 0x00A6:  // BROKEN BAR
      case 0x00AC:  // NOT SIGN
      case 0x00AF:  // MACRON
        return HALF_WIDTH;
    }
  }

  // Characters marked as 'H' in
  // http://www.unicode.org/Public/UNIDATA/EastAsianWidth.txt
  if (codepoint == 0x20A9 ||                 // WON SIGN
      INRANGE(codepoint, 0xFF61, 0xFF9F) ||  // half-width katakana
      INRANGE(codepoint, 0xFFA0, 0xFFBE) ||  // half-width hangul
      INRANGE(codepoint, 0xFFC2, 0xFFCF) ||  // half-width hangul
      INRANGE(codepoint, 0xFFD2, 0xFFD7) ||  // half-width hangul
      INRANGE(codepoint, 0xFFDA, 0xFFDC) ||  // half-width hangul
      INRANGE(codepoint, 0xFFE8, 0xFFEE)) {  // half-width symbols
    return HALF_WIDTH;
  }

  return FULL_WIDTH;
}

#undef INRANGE

// Returns the script type of the first character in `str`.
Util::ScriptType Util::GetFirstScriptType(absl::string_view str,
                                          size_t* mblen) {
  if (str.empty()) {
    if (mblen) {
      *mblen = 0;
    }
    return GetScriptType(0);
  }
  const Utf8AsChars32 utf8_as_char32(str);
  if (mblen) {
    *mblen = utf8_as_char32.begin().ok() ? utf8_as_char32.begin().size() : 0;
  }
  return GetScriptType(utf8_as_char32.front());
}

namespace {
using ScriptTypeBitSet = std::bitset<Util::SCRIPT_TYPE_SIZE>;
constexpr ScriptTypeBitSet kNumBs(1 << Util::NUMBER);
constexpr ScriptTypeBitSet kKanaBs((1 << Util::HIRAGANA) |
                                   (1 << Util::KATAKANA));

Util::ScriptType GetScriptTypeInternal(absl::string_view str,
                                       bool ignore_symbols) {
  ScriptTypeBitSet bs(-1);
  DCHECK(bs.all());

  for (const char32_t codepoint : Utf8AsChars32(str)) {
    if (bs.count() == 0) {
      return Util::UNKNOWN_SCRIPT;
    }

    // PROLONGED SOUND MARK|MIDLE_DOT|VOICED_SOUND_MARKS
    // are HIRAGANA or KATAKANA as well.
    if (codepoint == U'ー' || codepoint == U'・' ||
        (codepoint >= 0x3099 && codepoint <= 0x309C)) {
      bs &= kKanaBs;
      continue;
    }

    // Periods ('．' U+FF0E and '.' U+002E) are NUMBER as well, if they are not
    // the first character.
    if ((codepoint == U'．' || codepoint == U'.') && bs == kNumBs) {
      continue;
    }

    Util::ScriptType type = Util::GetScriptType(codepoint);
    // Ignore symbols
    // Regard UNKNOWN_SCRIPT as symbols here
    if (ignore_symbols && type == Util::UNKNOWN_SCRIPT) {
      continue;
    }

    ScriptTypeBitSet type_bs(1 << type);
    bs &= type_bs;
  }

  if (bs.count() != 1) {
    return Util::UNKNOWN_SCRIPT;
  }

  const uint32_t onehot = static_cast<uint32_t>(bs.to_ulong());
  return static_cast<Util::ScriptType>(std::countr_zero(onehot));
}
}  // namespace

Util::ScriptType Util::GetScriptType(absl::string_view str) {
  return GetScriptTypeInternal(str, false);
}

Util::ScriptType Util::GetScriptTypeWithoutSymbols(absl::string_view str) {
  return GetScriptTypeInternal(str, true);
}

// return true if all script_type in str is "type"
bool Util::IsScriptType(absl::string_view str, Util::ScriptType type) {
  for (ConstChar32Iterator iter(str); !iter.Done(); iter.Next()) {
    const char32_t codepoint = iter.Get();
    // Exception: 30FC (PROLONGEDSOUND MARK is categorized as HIRAGANA as well)
    if (type != GetScriptType(codepoint) &&
        (codepoint != 0x30FC || type != HIRAGANA)) {
      return false;
    }
  }
  return true;
}

// return true if the string contains script_type char
bool Util::ContainsScriptType(absl::string_view str, ScriptType type) {
  for (ConstChar32Iterator iter(str); !iter.Done(); iter.Next()) {
    if (type == GetScriptType(iter.Get())) {
      return true;
    }
  }
  return false;
}

// return the Form Type of string
Util::FormType Util::GetFormType(absl::string_view str) {
  // TODO(hidehiko): get rid of using FORM_TYPE_SIZE.
  FormType result = FORM_TYPE_SIZE;

  for (ConstChar32Iterator iter(str); !iter.Done(); iter.Next()) {
    const FormType type = GetFormType(iter.Get());
    if (type == UNKNOWN_FORM || (result != FORM_TYPE_SIZE && type != result)) {
      return UNKNOWN_FORM;
    }
    result = type;
  }

  return result;
}

bool Util::IsAscii(absl::string_view str) {
  return absl::c_all_of(str, absl::ascii_isascii);
}

namespace {
// const uint32_t* kJisX0208Bitmap[]
// constexpr uint64_t kJisX0208BitmapIndex
#include "base/character_set.inc"

bool IsJisX0208Char(char32_t codepoint) {
  if (codepoint <= 0x7F) {
    return true;  // ASCII
  }

  if ((65377 <= codepoint && codepoint <= 65439)) {
    return true;  // JISX0201
  }

  if (codepoint < 65536) {
    const int index = codepoint / 1024;
    if ((kJisX0208BitmapIndex & (static_cast<uint64_t>(1) << index)) == 0) {
      return false;
    }
    // Note, the return value of 1 << 64 is not zero. It's undefined.
    const int bitmap_index =
        std::popcount(kJisX0208BitmapIndex << (63 - index)) - 1;
    const uint32_t* bitmap = kJisX0208Bitmap[bitmap_index];
    if ((bitmap[(codepoint % 1024) / 32] >> (codepoint % 32)) & 0b1) {
      return true;  // JISX0208
    }
    return false;
  }

  return false;
}
}  // namespace

bool Util::IsJisX0208(absl::string_view str) {
  for (ConstChar32Iterator iter(str); !iter.Done(); iter.Next()) {
    if (!IsJisX0208Char(iter.Get())) {
      return false;
    }
  }
  return true;
}

// CAUTION: Be careful to change the implementation of serialization.  Some
// files use this format, so compatibility can be lost.  See, e.g.,
// data_manager/dataset_writer.cc.
std::string Util::SerializeUint64(uint64_t x) {
  const char s[8] = {
      static_cast<char>(x >> 56),          static_cast<char>((x >> 48) & 0xFF),
      static_cast<char>((x >> 40) & 0xFF), static_cast<char>((x >> 32) & 0xFF),
      static_cast<char>((x >> 24) & 0xFF), static_cast<char>((x >> 16) & 0xFF),
      static_cast<char>((x >> 8) & 0xFF),  static_cast<char>(x & 0xFF),
  };
  return std::string(s, 8);
}

bool Util::DeserializeUint64(absl::string_view s, uint64_t* x) {
  if (s.size() != 8) {
    return false;
  }
  // The following operations assume `char` is unsigned (i.e. -funsigned-char).
  static_assert(std::is_unsigned_v<char>,
                "`char` is not unsigned. Use -funsigned-char.");
  *x = static_cast<uint64_t>(s[0]) << 56 | static_cast<uint64_t>(s[1]) << 48 |
       static_cast<uint64_t>(s[2]) << 40 | static_cast<uint64_t>(s[3]) << 32 |
       static_cast<uint64_t>(s[4]) << 24 | static_cast<uint64_t>(s[5]) << 16 |
       static_cast<uint64_t>(s[6]) << 8 | static_cast<uint64_t>(s[7]);
  return true;
}

bool Util::IsAcceptableCharacterAsCandidate(char32_t letter) {
  // Unicode does not have code point larger than 0x10FFFF.
  if (letter > 0x10FFFF) {
    return false;
  }

  // Control characters are not acceptable.  0x7F is DEL.
  if (letter < 0x20 || (0x7F <= letter && letter <= 0x9F)) {
    return false;
  }

  // Bidirectional text control are not acceptable.
  // See: https://en.wikipedia.org/wiki/Bidirectional_text
  // Note: Bidirectional text controls can be allowed, as it can be contained in
  // some emoticons. If they are found not to be harmful for the system, this
  // section of validation can be abolished.
  if (letter == 0x061C || letter == 0x200E || letter == 0x200F ||
      (0x202A <= letter && letter <= 0x202E) ||
      (0x2066 <= letter && letter <= 0x2069)) {
    return false;
  }
  return true;
}

}  // namespace mozc
