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

#include <cerrno>
#include <cstdint>

#ifdef OS_WIN
// clang-format off
#include <Windows.h>
#include <WinCrypt.h>  // WinCrypt.h must be included after Windows.h
// clang-format on
#include <stdio.h>  // MSVC requires this for _vsnprintf
#include <time.h>
#endif  // OS_WIN

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif  // __APPLE__

#ifndef OS_WIN
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#endif  // !OS_WIN

#include <algorithm>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/double_array.h"
#include "base/logging.h"
#include "base/port.h"
#include "absl/numeric/bits.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"

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

MultiDelimiter::MultiDelimiter(const char *delim) {
  std::fill(lookup_table_, lookup_table_ + kTableSize, 0);
  for (const char *p = delim; *p != '\0'; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    lookup_table_[c >> 3] |= 1 << (c & 0x07);
  }
}

template <typename Delimiter>
SplitIterator<Delimiter, SkipEmpty>::SplitIterator(absl::string_view s,
                                                   const char *delim)
    : end_(s.data() + s.size()),
      delim_(delim),
      sp_begin_(s.data()),
      sp_len_(0) {
  while (sp_begin_ != end_ && delim_.Contains(*sp_begin_)) ++sp_begin_;
  const char *p = sp_begin_;
  for (; p != end_ && !delim_.Contains(*p); ++p) {
  }
  sp_len_ = p - sp_begin_;
}

template <typename Delimiter>
void SplitIterator<Delimiter, SkipEmpty>::Next() {
  sp_begin_ += sp_len_;
  while (sp_begin_ != end_ && delim_.Contains(*sp_begin_)) ++sp_begin_;
  if (sp_begin_ == end_) {
    sp_len_ = 0;
    return;
  }
  const char *p = sp_begin_;
  for (; p != end_ && !delim_.Contains(*p); ++p) {
  }
  sp_len_ = p - sp_begin_;
}

template <typename Delimiter>
SplitIterator<Delimiter, AllowEmpty>::SplitIterator(absl::string_view s,
                                                    const char *delim)
    : end_(s.data() + s.size()),
      sp_begin_(s.data()),
      sp_len_(0),
      delim_(delim),
      done_(sp_begin_ == end_) {
  const char *p = sp_begin_;
  for (; p != end_ && !delim_.Contains(*p); ++p) {
  }
  sp_len_ = p - sp_begin_;
}

template <typename Delimiter>
void SplitIterator<Delimiter, AllowEmpty>::Next() {
  sp_begin_ += sp_len_;
  if (sp_begin_ == end_) {
    sp_len_ = 0;
    done_ = true;
    return;
  }
  const char *p = ++sp_begin_;
  for (; p != end_ && !delim_.Contains(*p); ++p) {
  }
  sp_len_ = p - sp_begin_;
}

// Explicitly instantiate the implementations of 4 patterns.
template class SplitIterator<SingleDelimiter, SkipEmpty>;
template class SplitIterator<MultiDelimiter, SkipEmpty>;
template class SplitIterator<SingleDelimiter, AllowEmpty>;
template class SplitIterator<MultiDelimiter, AllowEmpty>;

void Util::SplitStringToUtf8Chars(absl::string_view str,
                                  std::vector<std::string> *output) {
  const char *begin = str.data();
  const char *const end = str.data() + str.size();
  while (begin < end) {
    const size_t mblen = OneCharLen(begin);
    output->emplace_back(begin, mblen);
    begin += mblen;
  }
  DCHECK_EQ(begin, end);
}

// Grapheme is user-perceived character. It may contain multiple codepoints
// such as modifiers and variation squesnces (e.g. 神︀ = U+795E,U+FE00 [SVS]).
// Note, this function does not support full requirements of the grapheme
// specifications defined by Unicode.
// * https://www.unicode.org/reports/tr29/#Grapheme_Cluster_Boundaries
void Util::SplitStringToUtf8Graphemes(absl::string_view str,
                                      std::vector<std::string> *graphemes) {
  Util::SplitStringToUtf8Chars(str, graphemes);
  if (graphemes->size() <= 1) {
    return;
  }

  // Use U+0000 (NUL) to represent a codepoint which doesn't appear in any
  // grapheme clusters since it's always used standalone.
  constexpr char32_t kStandaloneCodepoint = 0x0000;
  char32_t prev = kStandaloneCodepoint;
  std::vector<std::string> new_graphemes;
  new_graphemes.reserve(graphemes->capacity());

  for (std::string &grapheme : *graphemes) {
    const char32_t codepoint = Util::Utf8ToUcs4(grapheme);
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

void Util::SplitCSV(const std::string &input,
                    std::vector<std::string> *output) {
  std::unique_ptr<char[]> tmp(new char[input.size() + 1]);
  char *str = tmp.get();
  memcpy(str, input.data(), input.size());
  str[input.size()] = '\0';

  char *eos = str + input.size();
  char *start = nullptr;
  char *end = nullptr;
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
    *end = '\0';
    output->push_back(start);
    if (end_is_empty) {
      output->push_back("");
    }

    ++str;
  }
}

void Util::AppendStringWithDelimiter(absl::string_view delimiter,
                                     absl::string_view append_string,
                                     std::string *output) {
  CHECK(output);
  if (!output->empty()) {
    output->append(delimiter.data(), delimiter.size());
  }
  output->append(append_string.data(), append_string.size());
}

void Util::StringReplace(absl::string_view s, absl::string_view oldsub,
                         absl::string_view newsub, bool replace_all,
                         std::string *res) {
  if (oldsub.empty()) {
    res->append(s.data(), s.size());  // if empty, append the given string.
    return;
  }

  std::string::size_type start_pos = 0;
  std::string::size_type pos;
  do {
    pos = s.find(oldsub, start_pos);
    if (pos == std::string::npos) {
      break;
    }
    res->append(s.data() + start_pos, pos - start_pos);
    res->append(newsub.data(), newsub.size());
    start_pos = pos + oldsub.size();  // start searching again after the "old"
  } while (replace_all);
  res->append(s.data() + start_pos, s.length() - start_pos);
}

// The offset value to transform the upper case character to the lower
// case.  The value comes from both of (0x0061 "a" - 0x0041 "A") and
// (0xFF41 "ａ" - 0xFF21 "Ａ").
namespace {
constexpr size_t kOffsetFromUpperToLower = 0x0020;
}

void Util::LowerString(std::string *str) {
  const char *begin = str->data();
  size_t mblen = 0;

  std::string utf8;
  size_t pos = 0;
  while (pos < str->size()) {
    char32_t ucs4 = Utf8ToUcs4(begin + pos, begin + str->size(), &mblen);
    if (mblen == 0) {
      break;
    }
    // ('A' <= ucs4 && ucs4 <= 'Z') || ('Ａ' <= ucs4 && ucs4 <= 'Ｚ')
    if ((0x0041 <= ucs4 && ucs4 <= 0x005A) ||
        (0xFF21 <= ucs4 && ucs4 <= 0xFF3A)) {
      ucs4 += kOffsetFromUpperToLower;
      Ucs4ToUtf8(ucs4, &utf8);
      // The size of upper case character must be equal to the source
      // lower case character.  The following check asserts it.
      if (utf8.size() != mblen) {
        LOG(ERROR) << "The generated size differs from the source.";
        return;
      }
      str->replace(pos, mblen, utf8);
    }
    pos += mblen;
  }
}

void Util::UpperString(std::string *str) {
  const char *begin = str->data();
  size_t mblen = 0;

  std::string utf8;
  size_t pos = 0;
  while (pos < str->size()) {
    char32_t ucs4 = Utf8ToUcs4(begin + pos, begin + str->size(), &mblen);
    // ('a' <= ucs4 && ucs4 <= 'z') || ('ａ' <= ucs4 && ucs4 <= 'ｚ')
    if ((0x0061 <= ucs4 && ucs4 <= 0x007A) ||
        (0xFF41 <= ucs4 && ucs4 <= 0xFF5A)) {
      ucs4 -= kOffsetFromUpperToLower;
      Ucs4ToUtf8(ucs4, &utf8);
      // The size of upper case character must be equal to the source
      // lower case character.  The following check asserts it.
      if (utf8.size() != mblen) {
        LOG(ERROR) << "The generated size differs from the source.";
        return;
      }
      str->replace(pos, mblen, utf8);
    }
    pos += mblen;
  }
}

void Util::CapitalizeString(std::string *str) {
  auto first_str = std::string(Utf8SubString(*str, 0, 1));
  UpperString(&first_str);

  auto tailing_str = std::string(Utf8SubString(*str, 1, std::string::npos));
  LowerString(&tailing_str);

  str->assign(absl::StrCat(first_str, tailing_str));
}

bool Util::IsLowerAscii(absl::string_view s) {
  for (absl::string_view::const_iterator iter = s.begin(); iter != s.end();
       ++iter) {
    if (!islower(*iter)) {
      return false;
    }
  }
  return true;
}

bool Util::IsUpperAscii(absl::string_view s) {
  for (absl::string_view::const_iterator iter = s.begin(); iter != s.end();
       ++iter) {
    if (!isupper(*iter)) {
      return false;
    }
  }
  return true;
}

bool Util::IsCapitalizedAscii(absl::string_view s) {
  if (s.empty()) {
    return true;
  }
  if (isupper(*s.begin())) {
    return IsLowerAscii(absl::ClippedSubstr(s, 1));
  }
  return false;
}

bool Util::IsLowerOrUpperAscii(absl::string_view s) {
  if (s.empty()) {
    return true;
  }
  if (islower(*s.begin())) {
    return IsLowerAscii(absl::ClippedSubstr(s, 1));
  }
  if (isupper(*s.begin())) {
    return IsUpperAscii(absl::ClippedSubstr(s, 1));
  }
  return false;
}

bool Util::IsUpperOrCapitalizedAscii(absl::string_view s) {
  if (s.empty()) {
    return true;
  }
  if (isupper(*s.begin())) {
    return IsLowerOrUpperAscii(absl::ClippedSubstr(s, 1));
  }
  return false;
}

void Util::StripWhiteSpaces(const std::string &input, std::string *output) {
  *output = std::string(absl::StripAsciiWhitespace(input));
}

namespace {

// Table of UTF-8 character lengths, based on first byte
const unsigned char kUtf8LenTbl[256] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};

bool IsUtf8TrailingByte(uint8_t c) { return (c & 0xc0) == 0x80; }

}  // namespace

// Return length of a single UTF-8 source character
size_t Util::OneCharLen(const char *src) {
  return kUtf8LenTbl[*reinterpret_cast<const uint8_t *>(src)];
}

size_t Util::CharsLen(const char *src, size_t size) {
  const char *begin = src;
  const char *end = src + size;
  int length = 0;
  while (begin < end) {
    ++length;
    begin += OneCharLen(begin);
  }
  return length;
}

std::vector<char32_t> Util::Utf8ToCodepoints(absl::string_view str) {
  std::vector<char32_t> codepoints;
  char32_t codepoint;
  while (Util::SplitFirstChar32(str, &codepoint, &str)) {
    codepoints.push_back(codepoint);
  }
  return codepoints;
}

std::string Util::CodepointsToUtf8(const std::vector<char32_t> &codepoints) {
  std::string output;
  for (const char32_t codepoint : codepoints) {
    Ucs4ToUtf8Append(codepoint, &output);
  }
  return output;
}

char32_t Util::Utf8ToUcs4(const char *begin, const char *end, size_t *mblen) {
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

bool Util::SplitFirstChar32(absl::string_view s, char32_t *first_char32,
                            absl::string_view *rest) {
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

bool Util::SplitLastChar32(absl::string_view s, absl::string_view *rest,
                           char32_t *last_char32) {
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

void Util::Ucs4ToUtf8(char32_t c, std::string *output) {
  output->clear();
  Ucs4ToUtf8Append(c, output);
}

void Util::Ucs4ToUtf8Append(char32_t c, std::string *output) {
  char buf[7];
  output->append(buf, Ucs4ToUtf8(c, buf));
}

size_t Util::Ucs4ToUtf8(char32_t c, char *output) {
  if (c == 0) {
    // Do nothing if |c| is NUL. Previous implementation of Ucs4ToUtf8Append
    // worked like this.
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

#ifdef OS_WIN
size_t Util::WideCharsLen(absl::string_view src) {
  const int num_chars =
      ::MultiByteToWideChar(CP_UTF8, 0, src.data(), src.size(), nullptr, 0);
  if (num_chars <= 0) {
    return 0;
  }
  return num_chars;
}

int Util::Utf8ToWide(absl::string_view input, std::wstring *output) {
  const size_t output_length = WideCharsLen(input);
  if (output_length == 0) {
    return 0;
  }

  const size_t buffer_len = output_length + 1;
  std::unique_ptr<wchar_t[]> input_wide(new wchar_t[buffer_len]);
  const int copied_num_chars = ::MultiByteToWideChar(
      CP_UTF8, 0, input.data(), input.size(), input_wide.get(), buffer_len);
  if (0 <= copied_num_chars && copied_num_chars < buffer_len) {
    output->assign(input_wide.get(), copied_num_chars);
  }
  return copied_num_chars;
}

int Util::WideToUtf8(const wchar_t *input, std::string *output) {
  const int output_length =
      WideCharToMultiByte(CP_UTF8, 0, input, -1, nullptr, 0, nullptr, nullptr);
  if (output_length == 0) {
    return 0;
  }

  std::unique_ptr<char[]> input_encoded(new char[output_length + 1]);
  const int result =
      WideCharToMultiByte(CP_UTF8, 0, input, -1, input_encoded.get(),
                          output_length + 1, nullptr, nullptr);
  if (result > 0) {
    output->assign(input_encoded.get());
  }
  return result;
}

int Util::WideToUtf8(const std::wstring &input, std::string *output) {
  return WideToUtf8(input.c_str(), output);
}
#endif  // OS_WIN

absl::string_view Util::Utf8SubString(absl::string_view src, size_t start) {
  const char *begin = src.data();
  const char *end = begin + src.size();
  for (size_t i = 0; i < start && begin < end; ++i) {
    begin += OneCharLen(begin);
  }
  const size_t prefix_len = begin - src.data();
  return absl::string_view(begin, src.size() - prefix_len);
}

absl::string_view Util::Utf8SubString(absl::string_view src, size_t start,
                                      size_t length) {
  src = Utf8SubString(src, start);
  size_t l = length;
  const char *substr_end = src.data();
  const char *const end = src.data() + src.size();
  while (l > 0 && substr_end < end) {
    substr_end += OneCharLen(substr_end);
    --l;
  }
  return absl::string_view(src.data(), substr_end - src.data());
}

void Util::Utf8SubString(absl::string_view src, size_t start, size_t length,
                         std::string *result) {
  DCHECK(result);
  const absl::string_view substr = Utf8SubString(src, start, length);
  result->assign(substr.data(), substr.size());
}

void Util::StripUtf8Bom(std::string *line) {
  static constexpr char kUtf8Bom[] = "\xef\xbb\xbf";
  *line = std::string(absl::StripPrefix(*line, kUtf8Bom));
}

bool Util::IsUtf16Bom(const std::string &line) {
  static constexpr char kUtf16LeBom[] = "\xff\xfe";
  static constexpr char kUtf16BeBom[] = "\xfe\xff";
  if (line.size() >= 2 &&
      (line.substr(0, 2) == kUtf16LeBom || line.substr(0, 2) == kUtf16BeBom)) {
    return true;
  }
  return false;
}

bool Util::ChopReturns(std::string *line) {
  const std::string::size_type line_end = line->find_last_not_of("\r\n");
  if (line_end + 1 != line->size()) {
    line->erase(line_end + 1);
    return true;
  }
  return false;
}

namespace {
bool GetSecureRandomSequence(char *buf, size_t buf_size) {
  memset(buf, '\0', buf_size);
#ifdef OS_WIN
  HCRYPTPROV hprov;
  if (!::CryptAcquireContext(&hprov, nullptr, nullptr, PROV_RSA_FULL,
                             CRYPT_VERIFYCONTEXT)) {
    return false;
  }
  if (!::CryptGenRandom(hprov, static_cast<DWORD>(buf_size),
                        reinterpret_cast<BYTE *>(buf))) {
    ::CryptReleaseContext(hprov, 0);
    return false;
  }
  ::CryptReleaseContext(hprov, 0);
  return true;
#endif  // OS_WIN

#if defined(OS_CHROMEOS)
  // TODO(googleo): b/171939770 Accessing "/dev/urandom" is not allowed in
  // "ime" sandbox. Returns false to use the self-implemented random number
  // instead. When the API is unblocked, remove the hack.
  return false;
#endif  // OS_CHROMEOS

  // Use non blocking interface on Linux.
  // Mac also have /dev/urandom (although it's identical with /dev/random)
  std::ifstream ifs("/dev/urandom", std::ios::binary);
  if (!ifs) {
    return false;
  }
  ifs.read(buf, buf_size);
  return true;
}
}  // namespace

void Util::GetRandomSequence(char *buf, size_t buf_size) {
  if (GetSecureRandomSequence(buf, buf_size)) {
    return;
  }
  LOG(ERROR) << "Failed to generate secure random sequence. "
             << "Make it with Util::Random()";
  for (size_t i = 0; i < buf_size; ++i) {
    buf[i] = static_cast<char>(Util::Random(256));
  }
}

void Util::GetRandomAsciiSequence(char *buf, size_t buf_size) {
  // We use this map to convert a random byte value to an ascii character.
  // Its size happens to be 64, which is just one fourth of the number of
  // values that can be represented by a single byte value. This accidental
  // coincidence makes implementation of the method quite simple.
  constexpr char kCharMap[] =
      "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_";
  GetRandomSequence(buf, buf_size);
  for (size_t i = 0; i < buf_size; ++i) {
    // The size of kCharMap is just one fourth of 256. So we don't need to
    // care if probability distribution over the characters is biased.
    buf[i] = kCharMap[static_cast<unsigned char>(buf[i]) % 64];
  }
}

int Util::Random(int size) {
  DLOG_IF(FATAL, size < 0) << "|size| should be positive or 0. size: " << size;
  // Caveat: RAND_MAX is likely to be too small to achieve fine-grained
  // uniform distribution.
  // TODO(yukawa): Improve the resolution.
  return static_cast<int>(1.0 * size * rand() / (RAND_MAX + 1.0));
}

void Util::SetRandomSeed(uint32_t seed) { ::srand(seed); }

void Util::Sleep(uint32_t msec) {
#ifdef OS_WIN
  ::Sleep(msec);
#else   // OS_WIN
  usleep(msec * 1000);
#endif  // OS_WIN
}

namespace {

void EscapeInternal(char input, absl::string_view prefix, std::string *output) {
  const int hi = ((static_cast<int>(input) & 0xF0) >> 4);
  const int lo = (static_cast<int>(input) & 0x0F);
  output->append(prefix.data(), prefix.size());
  *output += static_cast<char>(hi >= 10 ? hi - 10 + 'A' : hi + '0');
  *output += static_cast<char>(lo >= 10 ? lo - 10 + 'A' : lo + '0');
}

struct BracketPair {
  absl::string_view GetOpenBracket() const {
    return absl::string_view(open, open_len);
  }
  absl::string_view GetCloseBracket() const {
    return absl::string_view(close, close_len);
  }

  const char *open;
  size_t open_len;

  const char *close;
  size_t close_len;
};

// A bidirectional map between opening and closing brackets as a sorted array.
// NOTE: This array is sorted in order of both |open| and |close|.  If you add a
// new bracket pair, you must keep this property.
const BracketPair kSortedBracketPairs[] = {
    {"(", 1, ")", 1},   {"[", 1, "]", 1},   {"{", 1, "}", 1},
    {"〈", 3, "〉", 3}, {"《", 3, "》", 3}, {"「", 3, "」", 3},
    {"『", 3, "』", 3}, {"【", 3, "】", 3}, {"〔", 3, "〕", 3},
    {"〘", 3, "〙", 3}, {"〚", 3, "〛", 3}, {"（", 3, "）", 3},
    {"［", 3, "］", 3}, {"｛", 3, "｝", 3}, {"｢", 3, "｣", 3},
};

}  // namespace

bool Util::IsOpenBracket(absl::string_view key, std::string *close_bracket) {
  struct OrderByOpenBracket {
    bool operator()(const BracketPair &x, absl::string_view key) const {
      return x.GetOpenBracket() < key;
    }
  };
  const auto end = std::end(kSortedBracketPairs);
  const auto iter = std::lower_bound(std::begin(kSortedBracketPairs), end, key,
                                     OrderByOpenBracket());
  if (iter == end || iter->GetOpenBracket() != key) {
    return false;
  }
  *close_bracket = std::string(iter->GetCloseBracket());
  return true;
}

bool Util::IsCloseBracket(absl::string_view key, std::string *open_bracket) {
  struct OrderByCloseBracket {
    bool operator()(const BracketPair &x, absl::string_view key) const {
      return x.GetCloseBracket() < key;
    }
  };
  const auto end = std::end(kSortedBracketPairs);
  const auto iter = std::lower_bound(std::begin(kSortedBracketPairs), end, key,
                                     OrderByCloseBracket());
  if (iter == end || iter->GetCloseBracket() != key) {
    return false;
  }
  *open_bracket = std::string(iter->GetOpenBracket());
  return true;
}

bool Util::IsFullWidthSymbolInHalfWidthKatakana(const std::string &input) {
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

bool Util::IsHalfWidthKatakanaSymbol(const std::string &input) {
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

bool Util::IsKanaSymbolContained(const std::string &input) {
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

bool Util::IsEnglishTransliteration(const std::string &value) {
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

// URL
void Util::EncodeUri(const std::string &input, std::string *output) {
  constexpr char kDigits[] = "0123456789ABCDEF";
  const char *begin = input.data();
  const char *end = input.data() + input.size();
  output->clear();
  while (begin < end) {
    if (absl::ascii_isascii(*begin) && absl::ascii_isalnum(*begin)) {
      *output += *begin;
    } else {
      *output += '%';
      *output += kDigits[(*begin >> 4) & 0x0f];
      *output += kDigits[*begin & 0x0f];
    }
    ++begin;
  }
}

void Util::DecodeUri(const std::string &input, std::string *output) {
  output->clear();
  const char *p = input.data();
  const char *end = input.data() + input.size();
  while (p < end) {
    if (*p == '%' && p + 2 < end) {
      const char h = toupper(p[1]);
      const char l = toupper(p[2]);
      const int vh = isalpha(h) ? (10 + (h - 'A')) : (h - '0');
      const int vl = isalpha(l) ? (10 + (l - 'A')) : (l - '0');
      *output += ((vh << 4) + vl);
      p += 3;
    } else if (*p == '+') {
      *output += ' ';
      p++;
    } else {
      *output += *p++;
    }
  }
}

void Util::AppendCgiParams(
    const std::vector<std::pair<std::string, std::string> > &params,
    std::string *base) {
  if (params.empty() || base == nullptr) {
    return;
  }

  std::string encoded;
  for (std::vector<std::pair<std::string, std::string> >::const_iterator it =
           params.begin();
       it != params.end(); ++it) {
    // Append "<first>=<encoded second>&"
    base->append(it->first);
    base->append("=");
    EncodeUri(it->second, &encoded);
    base->append(encoded);
    base->append("&");
  }

  // Delete the last "&".
  if (!base->empty()) {
    base->erase(base->size() - 1);
  }
}

void Util::Escape(absl::string_view input, std::string *output) {
  output->clear();
  for (size_t i = 0; i < input.size(); ++i) {
    EscapeInternal(input[i], "\\x", output);
  }
}

std::string Util::Escape(absl::string_view input) {
  std::string s;
  Escape(input, &s);
  return s;
}

bool Util::Unescape(absl::string_view input, std::string *output) {
  return absl::CUnescape(input, output);
}

#define INRANGE(w, a, b) ((w) >= (a) && (w) <= (b))

// script type
// TODO(yukawa, team): Make a mechanism to keep this classifier up-to-date
//   based on the original data from Unicode.org.
Util::ScriptType Util::GetScriptType(char32_t w) {
  if (INRANGE(w, 0x0030, 0x0039) ||  // ascii number
      INRANGE(w, 0xFF10, 0xFF19)) {  // full width number
    return NUMBER;
  } else if (INRANGE(w, 0x0041, 0x005A) ||  // ascii upper
             INRANGE(w, 0x0061, 0x007A) ||  // ascii lower
             INRANGE(w, 0xFF21, 0xFF3A) ||  // fullwidth ascii upper
             INRANGE(w, 0xFF41, 0xFF5A)) {  // fullwidth ascii lower
    return ALPHABET;
  } else if (w == 0x3005 ||  // IDEOGRAPHIC ITERATION MARK "々"
             INRANGE(w, 0x3400,
                     0x4DBF) ||  // CJK Unified Ideographs Extension A
             INRANGE(w, 0x4E00, 0x9FFF) ||  // CJK Unified Ideographs
             INRANGE(w, 0xF900, 0xFAFF) ||  // CJK Compatibility Ideographs
             INRANGE(w, 0x20000,
                     0x2A6DF) ||  // CJK Unified Ideographs Extension B
             INRANGE(w, 0x2A700,
                     0x2B73F) ||  // CJK Unified Ideographs Extension C
             INRANGE(w, 0x2B740,
                     0x2B81F) ||  // CJK Unified Ideographs Extension D
             INRANGE(w, 0x2F800, 0x2FA1F)) {  // CJK Compatibility Ideographs
    // As of Unicode 6.0.2, each block has the following characters assigned.
    // [U+3400, U+4DB5]:   CJK Unified Ideographs Extension A
    // [U+4E00, U+9FCB]:   CJK Unified Ideographs
    // [U+4E00, U+FAD9]:   CJK Compatibility Ideographs
    // [U+20000, U+2A6D6]: CJK Unified Ideographs Extension B
    // [U+2A700, U+2B734]: CJK Unified Ideographs Extension C
    // [U+2B740, U+2B81D]: CJK Unified Ideographs Extension D
    // [U+2F800, U+2FA1D]: CJK Compatibility Ideographs
    return KANJI;
  } else if (INRANGE(w, 0x3041, 0x309F) ||  // hiragana
             w == 0x1B001) {                // HIRAGANA LETTER ARCHAIC YE
    return HIRAGANA;
  } else if (INRANGE(w, 0x30A1, 0x30FF) ||  // full width katakana
             INRANGE(w, 0x31F0,
                     0x31FF) ||  // Katakana Phonetic Extensions for Ainu
             INRANGE(w, 0xFF65, 0xFF9F) ||  // half width katakana
             w == 0x1B000) {                // KATAKANA LETTER ARCHAIC E
    return KATAKANA;
  } else if (INRANGE(w, 0x02300, 0x023F3) ||  // Miscellaneous Technical
             INRANGE(w, 0x02700, 0x027BF) ||  // Dingbats
             INRANGE(w, 0x1F000, 0x1F02F) ||  // Mahjong tiles
             INRANGE(w, 0x1F030, 0x1F09F) ||  // Domino tiles
             INRANGE(w, 0x1F0A0, 0x1F0FF) ||  // Playing cards
             INRANGE(w, 0x1F100,
                     0x1F2FF) ||  // Enclosed Alphanumeric Supplement
             INRANGE(w, 0x1F200, 0x1F2FF) ||  // Enclosed Ideographic Supplement
             INRANGE(w, 0x1F300,
                     0x1F5FF) ||  // Miscellaneous Symbols And Pictographs
             INRANGE(w, 0x1F600, 0x1F64F) ||  // Emoticons
             INRANGE(w, 0x1F680, 0x1F6FF) ||  // Transport And Map Symbols
             INRANGE(w, 0x1F700, 0x1F77F) ||  // Alchemical Symbols
             w == 0x26CE) {                   // Ophiuchus
    return EMOJI;
  }

  return UNKNOWN_SCRIPT;
}

Util::FormType Util::GetFormType(char32_t w) {
  // 'Unicode Standard Annex #11: EAST ASIAN WIDTH'
  // http://www.unicode.org/reports/tr11/

  // Characters marked as 'Na' in
  // http://www.unicode.org/Public/UNIDATA/EastAsianWidth.txt
  if (INRANGE(w, 0x0020, 0x007F) ||  // ascii
      INRANGE(w, 0x27E6, 0x27ED) ||  // narrow mathematical symbols
      INRANGE(w, 0x2985, 0x2986)) {  // narrow white parentheses
    return HALF_WIDTH;
  }

  // Other characters marked as 'Na' in
  // http://www.unicode.org/Public/UNIDATA/EastAsianWidth.txt
  if (INRANGE(w, 0x00A2, 0x00AF)) {
    switch (w) {
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
  if (w == 0x20A9 ||                 // WON SIGN
      INRANGE(w, 0xFF61, 0xFF9F) ||  // half-width katakana
      INRANGE(w, 0xFFA0, 0xFFBE) ||  // half-width hangul
      INRANGE(w, 0xFFC2, 0xFFCF) ||  // half-width hangul
      INRANGE(w, 0xFFD2, 0xFFD7) ||  // half-width hangul
      INRANGE(w, 0xFFDA, 0xFFDC) ||  // half-width hangul
      INRANGE(w, 0xFFE8, 0xFFEE)) {  // half-width symbols
    return HALF_WIDTH;
  }

  return FULL_WIDTH;
}

#undef INRANGE

// return script type of first character in str
Util::ScriptType Util::GetScriptType(const char *begin, const char *end,
                                     size_t *mblen) {
  const char32_t w = Utf8ToUcs4(begin, end, mblen);
  return GetScriptType(w);
}

namespace {

Util::ScriptType GetScriptTypeInternal(absl::string_view str,
                                       bool ignore_symbols) {
  Util::ScriptType result = Util::SCRIPT_TYPE_SIZE;

  for (ConstChar32Iterator iter(str); !iter.Done(); iter.Next()) {
    const char32_t w = iter.Get();
    Util::ScriptType type = Util::GetScriptType(w);
    if ((w == 0x30FC || w == 0x30FB || (w >= 0x3099 && w <= 0x309C)) &&
        // PROLONGEDSOUND MARK|MIDLE_DOT|VOICED_SOUND_MARKS
        // are HIRAGANA as well
        (result == Util::SCRIPT_TYPE_SIZE || result == Util::HIRAGANA ||
         result == Util::KATAKANA)) {
      type = result;  // restore the previous state
    }

    // Ignore symbols
    // Regard UNKNOWN_SCRIPT as symbols here
    if (ignore_symbols && result != Util::UNKNOWN_SCRIPT &&
        type == Util::UNKNOWN_SCRIPT) {
      continue;
    }

    // Periods are NUMBER as well, if it is not the first character.
    // 0xFF0E == '．', 0x002E == '.' in UCS4 encoding.
    if (result == Util::NUMBER && (w == 0xFF0E || w == 0x002E)) {
      continue;
    }

    // Not first character.
    // Note: GetScriptType doesn't return SCRIPT_TYPE_SIZE, thus if result
    // is not SCRIPT_TYPE_SIZE, it is not the first character.
    if (result != Util::SCRIPT_TYPE_SIZE && type != result) {
      return Util::UNKNOWN_SCRIPT;
    }
    result = type;
  }

  if (result == Util::SCRIPT_TYPE_SIZE) {  // everything is "ー"
    return Util::UNKNOWN_SCRIPT;
  }

  return result;
}

}  // namespace

Util::ScriptType Util::GetScriptType(absl::string_view str) {
  return GetScriptTypeInternal(str, false);
}

Util::ScriptType Util::GetFirstScriptType(absl::string_view str) {
  size_t mblen = 0;
  return GetScriptType(str.data(), str.data() + str.size(), &mblen);
}

Util::ScriptType Util::GetScriptTypeWithoutSymbols(const std::string &str) {
  return GetScriptTypeInternal(str, true);
}

// return true if all script_type in str is "type"
bool Util::IsScriptType(absl::string_view str, Util::ScriptType type) {
  for (ConstChar32Iterator iter(str); !iter.Done(); iter.Next()) {
    const char32_t w = iter.Get();
    // Exception: 30FC (PROLONGEDSOUND MARK is categorized as HIRAGANA as well)
    if (type != GetScriptType(w) && (w != 0x30FC || type != HIRAGANA)) {
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
Util::FormType Util::GetFormType(const std::string &str) {
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
  for (const char c : str) {
    if (!absl::ascii_isascii(c)) {
      return false;
    }
  }
  return true;
}

namespace {
// const uint32_t* kJisX0208Bitmap[]
// constexpr uint64_t kJisX0208BitmapIndex
#include "base/character_set.inc"

bool IsJisX0208Char(char32_t ucs4) {
  if (ucs4 <= 0x7F) {
    return true;  // ASCII
  }

  if ((65377 <= ucs4 && ucs4 <= 65439)) {
    return true;  // JISX0201
  }

  if (ucs4 < 65536) {
    const int index = ucs4 / 1024;
    if ((kJisX0208BitmapIndex & (static_cast<uint64_t>(1) << index)) == 0) {
      return false;
    }
    // Note, the return value of 1 << 64 is not zero. It's undefined.
    const int bitmap_index =
        absl::popcount(kJisX0208BitmapIndex << (63 - index)) - 1;
    const uint32_t *bitmap = kJisX0208Bitmap[bitmap_index];
    if ((bitmap[(ucs4 % 1024) / 32] >> (ucs4 % 32)) & 0b1) {
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

bool Util::DeserializeUint64(absl::string_view s, uint64_t *x) {
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

bool Util::IsLittleEndian() {
#ifdef OS_WIN
  return true;
#endif  // OS_WIN

  union {
    unsigned char c[4];
    unsigned int i;
  } u;
  static_assert(sizeof(u.c) == sizeof(u.i),
                "Expecting (unsigned) int is 32-bit integer.");
  static_assert(sizeof(u) == sizeof(u.i), "Checking alignment.");
  u.i = 0x12345678U;
  return u.c[0] == 0x78U;
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

absl::StatusCode Util::ErrnoToCanonicalCode(int error_number) {
  switch (error_number) {
    case 0:
      return absl::StatusCode::kOk;
    case EACCES:
      return absl::StatusCode::kPermissionDenied;
    case ENOENT:
      return absl::StatusCode::kNotFound;
    case EEXIST:
      return absl::StatusCode::kAlreadyExists;
    default:
      return absl::StatusCode::kUnknown;
  }
}

absl::Status Util::ErrnoToCanonicalStatus(int error_number,
                                          absl::string_view message) {
  return absl::Status(ErrnoToCanonicalCode(error_number),
                      absl::StrCat(message, ": errno=", error_number));
}
}  // namespace mozc
