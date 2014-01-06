// Copyright 2010-2014, Google Inc.
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

#ifdef OS_WIN
#include <Windows.h>
#include <WinCrypt.h>
#include <time.h>
#include <stdio.h>  // MSVC requires this for _vsnprintf
#else  // OS_WIN

#ifdef OS_MACOSX
#include <mach/mach.h>
#include <mach/mach_time.h>

#elif defined(__native_client__)  // OS_MACOSX
#include <irt.h>
#endif  // OS_MACOSX or __native_client__
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#endif  // OS_WIN

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/text_converter.h"


namespace {

#if MOZC_MSVC_VERSION_LT(18, 0)
void va_copy(va_list &a, va_list &b) {
  a = b;
}
#endif  // Visual C++ 2012 and prior

// Lower-level routine that takes a va_list and appends to a specified
// string.  All other routines of sprintf family are just convenience
// wrappers around it.
void StringAppendV(string *dst, const char *format, va_list ap) {
  // First try with a small fixed size buffer
  char space[1024];

  // It's possible for methods that use a va_list to invalidate
  // the data in it upon use.  The fix is to make a copy
  // of the structure before using it and use that copy instead.
  va_list backup_ap;
  va_copy(backup_ap, ap);
  int result = vsnprintf(space, sizeof(space), format, backup_ap);
  va_end(backup_ap);

  if ((result >= 0) && (result < sizeof(space))) {
    // It fit
    dst->append(space, result);
    return;
  }

  // Repeatedly increase buffer size until it fits
  int length = sizeof(space);
  while (true) {
    if (result < 0) {
      // Older behavior: just try doubling the buffer size
      length *= 2;
    } else {
      // We need exactly "result+1" characters
      length = result+1;
    }
    char *buf = new char[length];

    // Restore the va_list before we use it again
    va_copy(backup_ap, ap);
    result = vsnprintf(buf, length, format, backup_ap);
    va_end(backup_ap);

    if ((result >= 0) && (result < length)) {
      // It fit
      dst->append(buf, result);
      delete[] buf;
      return;
    }
    delete[] buf;
  }
}
}   // namespace

namespace mozc {

ConstChar32Iterator::ConstChar32Iterator(StringPiece utf8_string)
    : utf8_string_(utf8_string),
      current_(0),
      done_(false) {
  Next();
}

char32 ConstChar32Iterator::Get() const {
  DCHECK(!done_);
  return current_;
}

void ConstChar32Iterator::Next() {
  if (!done_) {
    done_ = !Util::SplitFirstChar32(utf8_string_, &current_, &utf8_string_);
  }
}

bool ConstChar32Iterator::Done() const {
  return done_;
}

ConstChar32ReverseIterator::ConstChar32ReverseIterator(StringPiece utf8_string)
    : utf8_string_(utf8_string),
      current_(0),
      done_(false) {
  Next();
}

char32 ConstChar32ReverseIterator::Get() const {
  DCHECK(!done_);
  return current_;
}

void ConstChar32ReverseIterator::Next() {
  if (!done_) {
    done_ = !Util::SplitLastChar32(utf8_string_, &utf8_string_, &current_);
  }
}

bool ConstChar32ReverseIterator::Done() const {
  return done_;
}

MultiDelimiter::MultiDelimiter(const char* delim) {
  fill(lookup_table_, lookup_table_ + kTableSize, 0);
  for (const char* p = delim; *p != '\0'; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    lookup_table_[c >> 3] |= 1 << (c & 0x07);
  }
}

template <typename Delimiter>
SplitIterator<Delimiter, SkipEmpty>::SplitIterator(StringPiece s,
                                                   const char *delim)
    : end_(s.data() + s.size()),
      delim_(delim),
      sp_begin_(s.data()),
      sp_len_(0) {
  while (sp_begin_ != end_ && delim_.Contains(*sp_begin_)) ++sp_begin_;
  const char *p = sp_begin_;
  for (; p != end_ && !delim_.Contains(*p); ++p) {}
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
  for (; p != end_ && !delim_.Contains(*p); ++p) {}
  sp_len_ = p - sp_begin_;
}

template <typename Delimiter>
SplitIterator<Delimiter, AllowEmpty>::SplitIterator(StringPiece s,
                                                    const char *delim)
    : end_(s.data() + s.size()),
      delim_(delim),
      sp_begin_(s.data()),
      sp_len_(0),
      done_(sp_begin_ == end_) {
  const char *p = sp_begin_;
  for (; p != end_ && !delim_.Contains(*p); ++p) {}
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
  for (; p != end_ && !delim_.Contains(*p); ++p) {}
  sp_len_ = p - sp_begin_;
}

// Explicitly instantiate the implementations of 4 patterns.
template class SplitIterator<SingleDelimiter, SkipEmpty>;
template class SplitIterator<MultiDelimiter, SkipEmpty>;
template class SplitIterator<SingleDelimiter, AllowEmpty>;
template class SplitIterator<MultiDelimiter, AllowEmpty>;

void Util::SplitStringUsing(StringPiece str,
                            const char *delim,
                            vector<string> *output) {
  if (delim[0] != '\0' && delim[1] == '\0') {
    for (SplitIterator<SingleDelimiter> iter(str, delim);
         !iter.Done(); iter.Next()) {
      PushBackStringPiece(iter.Get(), output);
    }
  } else {
    for (SplitIterator<MultiDelimiter> iter(str, delim);
         !iter.Done(); iter.Next()) {
      PushBackStringPiece(iter.Get(), output);
    }
  }
}

void Util::SplitStringUsing(StringPiece str,
                            const char *delim,
                            vector<StringPiece> *output) {
  if (delim[0] != '\0' && delim[1] == '\0') {
    for (SplitIterator<SingleDelimiter> iter(str, delim);
         !iter.Done(); iter.Next()) {
      output->push_back(iter.Get());
    }
  } else {
    for (SplitIterator<MultiDelimiter> iter(str, delim);
         !iter.Done(); iter.Next()) {
      output->push_back(iter.Get());
    }
  }
}

void Util::SplitStringAllowEmpty(StringPiece str,
                                 const char *delim,
                                 vector<string> *output) {
  if (delim[0] != '\0' && delim[1] == '\0') {
    for (SplitIterator<SingleDelimiter, AllowEmpty> iter(str, delim);
         !iter.Done(); iter.Next()) {
      PushBackStringPiece(iter.Get(), output);
    }
  } else {
    for (SplitIterator<MultiDelimiter, AllowEmpty> iter(str, delim);
         !iter.Done(); iter.Next()) {
      PushBackStringPiece(iter.Get(), output);
    }
  }
}

void Util::SplitStringToUtf8Chars(const string &str, vector<string> *output) {
  size_t begin = 0;
  const size_t end = str.size();

  while (begin < end) {
    const size_t mblen = OneCharLen(str.c_str() + begin);
    output->push_back(str.substr(begin, mblen));
    begin += mblen;
  }
  DCHECK_EQ(begin, end);
}

void Util::SplitCSV(const string &input, vector<string> *output) {
  scoped_ptr<char[]> tmp(new char[input.size() + 1]);
  char *str = tmp.get();
  memcpy(str, input.data(), input.size());
  str[input.size()] = '\0';

  char *eos = str + input.size();
  char *start = NULL;
  char *end = NULL;
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
          if (*str != '"')
            break;
        }
        *end++ = *str;
      }
      str = find(str, eos, ',');
    } else {
      start = str;
      str = find(str, eos, ',');
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

void Util::JoinStrings(const vector<string> &input,
                       const char *delim,
                       string *output) {
  output->clear();
  for (size_t i = 0; i < input.size(); ++i) {
    if (i > 0) {
      *output += delim;
    }
    *output += input[i];
  }
}

void Util::JoinStringPieces(const vector<StringPiece> &pieces,
                            const char *delim,
                            string *output) {
  if (pieces.empty()) {
    output->clear();
    return;
  }

  const size_t delim_len = strlen(delim);
  size_t len = delim_len * (pieces.size() - 1);
  for (size_t i = 0; i < pieces.size(); ++i) {
    len += pieces[i].size();
  }
  output->reserve(len);
  pieces[0].CopyToString(output);
  for (size_t i = 1; i < pieces.size(); ++i) {
    output->append(delim, delim_len);
    output->append(pieces[i].data(), pieces[i].size());
  }
}

void Util::AppendStringWithDelimiter(StringPiece delimiter,
                                     StringPiece append_string,
                                     string *output) {
  CHECK(output);
  if (!output->empty()) {
    delimiter.AppendToString(output);
  }
  append_string.AppendToString(output);
}


void Util::StringReplace(StringPiece s, StringPiece oldsub,
                         StringPiece newsub, bool replace_all,
                         string *res) {
  if (oldsub.empty()) {
    s.AppendToString(res);  // if empty, append the given string.
    return;
  }

  string::size_type start_pos = 0;
  string::size_type pos;
  do {
    pos = s.find(oldsub, start_pos);
    if (pos == string::npos) {
      break;
    }
    res->append(s.data() + start_pos, pos - start_pos);
    newsub.AppendToString(res);
    start_pos = pos + oldsub.size();  // start searching again after the "old"
  } while (replace_all);
  res->append(s.data() + start_pos, s.length() - start_pos);
}

// The offset value to transform the upper case character to the lower
// case.  The value comes from both of (0x0061 "a" - 0x0041 "A") and
// (0xFF41 "ａ" - 0xFF21 "Ａ").
namespace {
const size_t kOffsetFromUpperToLower = 0x0020;
}

void Util::LowerString(string *str) {
  const char *begin = str->data();
  size_t mblen = 0;

  string utf8;
  size_t pos = 0;
  while (pos < str->size()) {
    char32 ucs4 = UTF8ToUCS4(begin + pos, begin + str->size(), &mblen);
    if (mblen == 0) {
      break;
    }
    // ('A' <= ucs4 && ucs4 <= 'Z') || ('Ａ' <= ucs4 && ucs4 <= 'Ｚ')
    if ((0x0041 <= ucs4 && ucs4 <= 0x005A) ||
        (0xFF21 <= ucs4 && ucs4 <= 0xFF3A)) {
      ucs4 += kOffsetFromUpperToLower;
      UCS4ToUTF8(ucs4, &utf8);
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

void Util::UpperString(string *str) {
  const char *begin = str->data();
  size_t mblen = 0;

  string utf8;
  size_t pos = 0;
  while (pos < str->size()) {
    char32 ucs4 = UTF8ToUCS4(begin + pos, begin + str->size(), &mblen);
    // ('a' <= ucs4 && ucs4 <= 'z') || ('ａ' <= ucs4 && ucs4 <= 'ｚ')
    if ((0x0061 <= ucs4 && ucs4 <= 0x007A) ||
        (0xFF41 <= ucs4 && ucs4 <= 0xFF5A)) {
      ucs4 -= kOffsetFromUpperToLower;
      UCS4ToUTF8(ucs4, &utf8);
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

void Util::CapitalizeString(string *str) {
  string first_str;
  SubString(*str, 0, 1, &first_str);
  UpperString(&first_str);

  string tailing_str;
  SubString(*str, 1, string::npos, &tailing_str);
  LowerString(&tailing_str);

  str->assign(first_str + tailing_str);
}

bool Util::IsLowerAscii(StringPiece s) {
  for (StringPiece::const_iterator iter = s.begin(); iter != s.end(); ++iter) {
    if (!islower(*iter)) {
      return false;
    }
  }
  return true;
}

bool Util::IsUpperAscii(StringPiece s) {
  for (StringPiece::const_iterator iter = s.begin(); iter != s.end(); ++iter) {
    if (!isupper(*iter)) {
      return false;
    }
  }
  return true;
}

bool Util::IsCapitalizedAscii(StringPiece s) {
  if (s.empty()) {
    return true;
  }
  if (isupper(*s.begin())) {
    return IsLowerAscii(s.substr(1));
  }
  return false;
}

bool Util::IsLowerOrUpperAscii(StringPiece s) {
  if (s.empty()) {
    return true;
  }
  if (islower(*s.begin())) {
    return IsLowerAscii(s.substr(1));
  }
  if (isupper(*s.begin())) {
    return IsUpperAscii(s.substr(1));
  }
  return false;
}

bool Util::IsUpperOrCapitalizedAscii(StringPiece s) {
  if (s.empty()) {
    return true;
  }
  if (isupper(*s.begin())) {
    return IsLowerOrUpperAscii(s.substr(1));
  }
  return false;
}

void Util::StripWhiteSpaces(const string &input, string *output) {
  DCHECK(output);
  output->clear();

  if (input.empty()) {
    return;
  }

  size_t start = 0;
  size_t end = input.size() - 1;
  for (; start < input.size() && isspace(input[start]); ++start) {}
  for (; end > start && isspace(input[end]); --end) {}

  if (end > start) {
    output->assign(input.data() + start, end - start + 1);
  }
}

namespace {

// Table of UTF-8 character lengths, based on first byte
const unsigned char kUTF8LenTbl[256] = {
  1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
  2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,
  3,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3, 4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4
};

bool IsUTF8TrailingByte(uint8 c) {
  return (c & 0xc0) == 0x80;
}

}  // namespace

// Return length of a single UTF-8 source character
size_t Util::OneCharLen(const char *src) {
  return kUTF8LenTbl[*reinterpret_cast<const uint8*>(src)];
}

size_t Util::CharsLen(const char *src, size_t length) {
  const char *begin = src;
  const char *end = src + length;
  int result = 0;
  while (begin < end) {
    ++result;
    begin += OneCharLen(begin);
  }
  return result;
}

char32 Util::UTF8ToUCS4(const char *begin,
                        const char *end,
                        size_t *mblen) {
  StringPiece s(begin, end - begin);
  StringPiece rest;
  char32 c = 0;
  if (!Util::SplitFirstChar32(s, &c, &rest)) {
    *mblen = 0;
    return 0;
  }
  *mblen = rest.begin() - s.begin();
  return c;
}

bool Util::SplitFirstChar32(StringPiece s,
                            char32 *first_char32,
                            StringPiece *rest) {
  char32 dummy_char32 = 0;
  if (first_char32 == NULL) {
    first_char32 = &dummy_char32;
  }
  StringPiece dummy_rest;
  if (rest == NULL) {
    rest = &dummy_rest;
  }

  *first_char32 = 0;
  rest->clear();

  while (true) {
    if (s.empty()) {
      return false;
    }

    char32 result = 0;
    size_t len = 0;
    char32 min_value = 0;
    char32 max_value = 0xffffffff;
    {
      const uint8 leading_byte = static_cast<uint8>(s[0]);
      if (leading_byte < 0x80) {
        *first_char32 = leading_byte;
        *rest = s.substr(1);
        return true;
      }

      if (IsUTF8TrailingByte(leading_byte)) {
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
      const uint8 c = static_cast<uint8>(s[i]);
      if (!IsUTF8TrailingByte(c)) {
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
    *rest = s.substr(len);
    return true;
  }
}

bool Util::SplitLastChar32(StringPiece s,
                           StringPiece *rest,
                           char32 *last_char32) {
  StringPiece dummy_rest;
  if (rest == NULL) {
    rest = &dummy_rest;
  }
  char32 dummy_char32 = 0;
  if (last_char32 == NULL) {
    last_char32 = &dummy_char32;
  }

  *last_char32 = 0;
  rest->clear();

  if (s.empty()) {
    return false;
  }
  StringPiece::const_reverse_iterator it = s.rbegin();
  for (; (it != s.rend()) && IsUTF8TrailingByte(*it); ++it) {}
  if (it == s.rend()) {
    return false;
  }
  const StringPiece::difference_type len = distance(s.rbegin(), it) + 1;
  StringPiece last_piece = s.substr(s.size() - len);
  StringPiece result_piece;
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

void Util::UCS4ToUTF8(char32 c, string *output) {
  output->clear();
  UCS4ToUTF8Append(c, output);
}

void Util::UCS4ToUTF8Append(char32 c, string *output) {
  if (c == 0) {
    // Do nothing if |c| is NUL. Previous implementation of UCS4ToUTF8Append
    // worked like this.
    return;
  }
  if (c < 0x00080) {
    output->push_back(static_cast<char>(c & 0xFF));
    return;
  }
  if (c < 0x00800) {
    const char buf[] = {
      static_cast<char>(0xC0 + ((c >> 6) & 0x1F)),
      static_cast<char>(0x80 + (c & 0x3F)),
    };
    output->append(buf, arraysize(buf));
    return;
  }
  if (c < 0x10000) {
    const char buf[] = {
      static_cast<char>(0xE0 + ((c >> 12) & 0x0F)),
      static_cast<char>(0x80 + ((c >> 6) & 0x3F)),
      static_cast<char>(0x80 + (c & 0x3F)),
    };
    output->append(buf, arraysize(buf));
    return;
  }
  if (c < 0x200000) {
    const char buf[] = {
      static_cast<char>(0xF0 + ((c >> 18) & 0x07)),
      static_cast<char>(0x80 + ((c >> 12) & 0x3F)),
      static_cast<char>(0x80 + ((c >> 6)  & 0x3F)),
      static_cast<char>(0x80 + (c & 0x3F)),
    };
    output->append(buf, arraysize(buf));
    return;
  }
  // below is not in UCS4 but in 32bit int.
  if (c < 0x8000000) {
    const char buf[] = {
      static_cast<char>(0xF8 + ((c >> 24) & 0x03)),
      static_cast<char>(0x80 + ((c >> 18) & 0x3F)),
      static_cast<char>(0x80 + ((c >> 12) & 0x3F)),
      static_cast<char>(0x80 + ((c >> 6)  & 0x3F)),
      static_cast<char>(0x80 + (c & 0x3F)),
    };
    output->append(buf, arraysize(buf));
    return;
  }
  const char buf[] = {
    static_cast<char>(0xFC + ((c >> 30) & 0x01)),
    static_cast<char>(0x80 + ((c >> 24) & 0x3F)),
    static_cast<char>(0x80 + ((c >> 18) & 0x3F)),
    static_cast<char>(0x80 + ((c >> 12) & 0x3F)),
    static_cast<char>(0x80 + ((c >> 6)  & 0x3F)),
    static_cast<char>(0x80 + (c & 0x3F)),
  };
  output->append(buf, arraysize(buf));
}

#ifdef OS_WIN
size_t Util::WideCharsLen(StringPiece src) {
  const int num_chars =
      ::MultiByteToWideChar(CP_UTF8, 0, src.begin(), src.size(), NULL, 0);
  if (num_chars <= 0) {
    return 0;
  }
  return num_chars;
}

int Util::UTF8ToWide(StringPiece input, wstring *output) {
  const size_t output_length = WideCharsLen(input);
  if (output_length == 0) {
    return 0;
  }

  const size_t buffer_len = output_length + 1;
  scoped_ptr<wchar_t[]> input_wide(new wchar_t[buffer_len]);
  const int copied_num_chars = ::MultiByteToWideChar(
      CP_UTF8, 0, input.begin(), input.size(), input_wide.get(),
      buffer_len);
  if (0 <= copied_num_chars && copied_num_chars < buffer_len) {
    output->assign(input_wide.get(), copied_num_chars);
  }
  return copied_num_chars;
}

int Util::WideToUTF8(const wchar_t *input, string *output) {
  const int output_length = WideCharToMultiByte(CP_UTF8, 0, input, -1, NULL, 0,
                                                NULL, NULL);
  if (output_length == 0) {
    return 0;
  }

  scoped_ptr<char[]> input_encoded(new char[output_length + 1]);
  const int result = WideCharToMultiByte(CP_UTF8, 0, input, -1,
                                         input_encoded.get(),
                                         output_length + 1, NULL, NULL);
  if (result > 0) {
    output->assign(input_encoded.get());
  }
  return result;
}

int Util::WideToUTF8(const wstring &input, string *output) {
  return WideToUTF8(input.c_str(), output);
}
#endif  // OS_WIN

StringPiece Util::SubStringPiece(
    const StringPiece src, const size_t start, const size_t length) {
  size_t l = start;
  const char *substr_begin = src.data();
  while (l > 0) {
    substr_begin += OneCharLen(substr_begin);
    --l;
  }

  l = length;
  const char *substr_end = substr_begin;
  const char *const end = src.data() + src.size();
  while (l > 0 && substr_end < end) {
    substr_end += OneCharLen(substr_end);
    --l;
  }

  return StringPiece(substr_begin, substr_end - substr_begin);
}

void Util::SubString(const StringPiece src,
                     const size_t start,
                     const size_t length,
                     string *result) {
  DCHECK(result);
  const StringPiece substr = SubStringPiece(src, start, length);
  substr.CopyToString(result);
}

bool Util::StartsWith(const StringPiece str, const StringPiece prefix) {
  if (str.size() < prefix.size()) {
    return false;
  }
  return (0 == memcmp(str.data(), prefix.data(), prefix.size()));
}

bool Util::EndsWith(const StringPiece str, const StringPiece suffix) {
  if (str.size() < suffix.size()) {
    return false;
  }
  return (0 == memcmp(str.data() + str.size() - suffix.size(),
                      suffix.data(), suffix.size()));
}

void Util::StripUTF8BOM(string *line) {
  static const char kUTF8BOM[] = "\xef\xbb\xbf";
  if (line->substr(0, 3) == kUTF8BOM) {
    line->erase(0, 3);
  }
}

bool Util::IsUTF16BOM(const string &line) {
  static const char kUTF16LEBOM[] = "\xff\xfe";
  static const char kUTF16BEBOM[] = "\xfe\xff";
  if (line.size() >= 2 &&
      (line.substr(0, 2) == kUTF16LEBOM ||
       line.substr(0, 2) == kUTF16BEBOM)) {
    return true;
  }
  return false;
}

bool Util::IsAndroidPuaEmoji(const StringPiece s) {
  static const char kUtf8MinAndroidPuaEmoji[] = "\xf3\xbe\x80\x80";
  static const char kUtf8MaxAndroidPuaEmoji[] = "\xf3\xbe\xba\xa0";
  return (s.size() == 4 &&
          kUtf8MinAndroidPuaEmoji <= s && s <= kUtf8MaxAndroidPuaEmoji);
}

string Util::StringPrintf(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  string result;
  StringAppendV(&result, format, ap);
  va_end(ap);
  return result;
}

bool Util::ChopReturns(string *line) {
  const string::size_type line_end = line->find_last_not_of("\r\n");
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
  if (!::CryptAcquireContext(&hprov,
                             NULL,
                             NULL,
                             PROV_RSA_FULL,
                             CRYPT_VERIFYCONTEXT)) {
    return false;
  }
  if (!::CryptGenRandom(hprov,
                        static_cast<DWORD>(buf_size),
                        reinterpret_cast<BYTE *>(buf))) {
    ::CryptReleaseContext(hprov, 0);
    return false;
  }
  ::CryptReleaseContext(hprov, 0);
  return true;
#elif defined(__native_client__)
  struct nacl_irt_random interface;

  if (nacl_interface_query(NACL_IRT_RANDOM_v0_1, &interface,
                           sizeof(interface)) != sizeof(interface)) {
    DLOG(ERROR) << "Cannot get NACL_IRT_RANDOM_v0_1 interface";
    return false;
  }

  size_t nread;
  const int error = interface.get_random_bytes(buf, buf_size, &nread);
  if (error != 0) {
    LOG(ERROR) << "interface.get_random_bytes error: " << error;
    return false;
  } else if (nread != buf_size) {
    LOG(ERROR) << "interface.get_random_bytes error. nread: " << nread
               << " buf_size: " << buf_size;
    return false;
  }
  return true;
#else  // !OS_WIN && !__native_client__
  // Use non blocking interface on Linux.
  // Mac also have /dev/urandom (although it's identical with /dev/random)
  ifstream ifs("/dev/urandom", ios::binary);
  if (!ifs) {
    return false;
  }
  ifs.read(buf, buf_size);
  return true;
#endif  // OS_WIN or __native_client__
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
  const char kCharMap[] =
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
  return static_cast<int> (1.0 * size * rand() / (RAND_MAX + 1.0));
}

void Util::SetRandomSeed(uint32 seed) {
  ::srand(seed);
}

namespace {
class ClockImpl : public Util::ClockInterface {
 public:
#ifndef __native_client__
  ClockImpl() {}
#else  // __native_client__
  ClockImpl() : timezone_offset_sec_(0) {}
#endif  // __native_client__
  virtual ~ClockImpl() {}

  virtual void GetTimeOfDay(uint64 *sec, uint32 *usec) {
#ifdef OS_WIN
    FILETIME file_time;
    GetSystemTimeAsFileTime(&file_time);
    ULARGE_INTEGER time_value;
    time_value.HighPart = file_time.dwHighDateTime;
    time_value.LowPart = file_time.dwLowDateTime;
    // Convert into microseconds
    time_value.QuadPart /= 10;
    // kDeltaEpochInMicroSecs is difference between January 1, 1970 and
    // January 1, 1601 in microsecond.
    // This number is calculated as follows.
    // ((1970 - 1601) * 365 + 89) * 24 * 60 * 60 * 1000000
    // 89 is the number of leap years between 1970 and 1601.
    const uint64 kDeltaEpochInMicroSecs = 11644473600000000ULL;
    // Convert file time to unix epoch
    time_value.QuadPart -= kDeltaEpochInMicroSecs;
    *sec = static_cast<uint64>(time_value.QuadPart / 1000000UL);
    *usec = static_cast<uint32>(time_value.QuadPart % 1000000UL);
#else  // OS_WIN
    struct timeval tv;
    gettimeofday(&tv, NULL);
    *sec = tv.tv_sec;
    *usec = tv.tv_usec;
#endif  // OS_WIN
  }

  virtual uint64 GetTime() {
#ifdef OS_WIN
    return static_cast<uint64>(_time64(NULL));
#else
    return static_cast<uint64>(time(NULL));
#endif  // OS_WIN
  }

  virtual bool GetTmWithOffsetSecond(time_t offset_sec, tm *output) {
    const time_t current_sec = static_cast<time_t>(this->GetTime());
    const time_t modified_sec = current_sec + offset_sec;

#ifdef OS_WIN
    if (_localtime64_s(output, &modified_sec) != 0) {
      return false;
    }
#elif defined(__native_client__)
    const time_t localtime_sec = modified_sec + timezone_offset_sec_;
    if (gmtime_r(&localtime_sec, output) == NULL) {
      return false;
    }
#else  // !OS_WIN && !__native_client__
    if (localtime_r(&modified_sec, output) == NULL) {
      return false;
    }
#endif  // OS_WIN
    return true;
  }

  virtual uint64 GetFrequency() {
#if defined(OS_WIN)
    LARGE_INTEGER timestamp;
    // TODO(yukawa): Consider the case where QueryPerformanceCounter is not
    // available.
    const BOOL result = ::QueryPerformanceFrequency(&timestamp);
    return static_cast<uint64>(timestamp.QuadPart);
#elif defined(OS_MACOSX)
    static mach_timebase_info_data_t timebase_info;
    mach_timebase_info(&timebase_info);
    return static_cast<uint64>(
        1.0e9 * timebase_info.denom / timebase_info.numer);
#elif defined(OS_LINUX)
#if defined(HAVE_LIBRT)
    return 1000000000uLL;
#else  // HAVE_LIBRT
    return 1000000uLL;
#endif  // HAVE_LIBRT
#else  // platforms (OS_WIN, OS_MACOSX, OS_LINUX, ...)
#error "Not supported platform"
#endif  // platforms (OS_WIN, OS_MACOSX, OS_LINUX, ...)
  }

  virtual uint64 GetTicks() {
#if defined(OS_WIN)
    LARGE_INTEGER timestamp;
    // TODO(yukawa): Consider the case where QueryPerformanceCounter is not
    // available.
    const BOOL result = ::QueryPerformanceCounter(&timestamp);
    return static_cast<uint64>(timestamp.QuadPart);
#elif defined(OS_MACOSX)
    return static_cast<uint64>(mach_absolute_time());
#elif defined(OS_LINUX)
#if defined(HAVE_LIBRT)
    struct timespec timestamp;
    if (-1 == clock_gettime(CLOCK_REALTIME, &timestamp)) {
      return 0;
    }
    return timestamp.tv_sec * 1000000000uLL + timestamp.tv_nsec;
#else  // HAVE_LIBRT
    // librt is not linked on Android, so we uses GetTimeOfDay instead.
    // GetFrequency() always returns 1MHz when librt is not available,
    // so we uses microseconds as ticks.
    uint64 sec;
    uint32 usec;
    GetTimeOfDay(&sec, &usec);
    return sec * 1000000 + usec;
#endif  // HAVE_LIBRT
#else  // platforms (OS_WIN, OS_MACOSX, OS_LINUX, ...)
#error "Not supported platform"
#endif  // platforms (OS_WIN, OS_MACOSX, OS_LINUX, ...)
  }

#ifdef __native_client__
  virtual void SetTimezoneOffset(int32 timezone_offset_sec) {
    timezone_offset_sec_ = timezone_offset_sec;
  }

 private:
  int32 timezone_offset_sec_;
#endif  // __native_client__
};

Util::ClockInterface *g_clock_handler = NULL;

Util::ClockInterface *GetClockHandler() {
  if (g_clock_handler != NULL) {
    return g_clock_handler;
  } else {
    return Singleton<ClockImpl>::get();
  }
}

}  // namespace

void Util::SetClockHandler(Util::ClockInterface *handler) {
  g_clock_handler = handler;
}

void Util::GetTimeOfDay(uint64 *sec, uint32 *usec) {
  GetClockHandler()->GetTimeOfDay(sec, usec);
}

uint64 Util::GetTime() {
  return GetClockHandler()->GetTime();
}

bool Util::GetCurrentTm(tm *current_time) {
  return GetTmWithOffsetSecond(current_time, 0);
}

bool Util::GetTmWithOffsetSecond(tm *time_with_offset, int offset_sec) {
  return GetClockHandler()->GetTmWithOffsetSecond(offset_sec, time_with_offset);
}

uint64 Util::GetFrequency() {
  return GetClockHandler()->GetFrequency();
}

uint64 Util::GetTicks() {
  return GetClockHandler()->GetTicks();
}

void Util::Sleep(uint32 msec) {
#ifdef OS_WIN
  ::Sleep(msec);
#else  // OS_WIN
  usleep(msec * 1000);
#endif  // OS_WIN
}

#ifdef __native_client__
void Util::SetTimezoneOffset(int32 timezone_offset_sec) {
  return GetClockHandler()->SetTimezoneOffset(timezone_offset_sec);
}
#endif  // __native_client__

namespace {

void EscapeInternal(char input, const string &prefix, string *output) {
  const int hi = ((static_cast<int>(input) & 0xF0) >> 4);
  const int lo = (static_cast<int>(input) & 0x0F);
  *output += prefix;
  *output += static_cast<char>(hi >= 10 ? hi - 10 + 'A' : hi + '0');
  *output += static_cast<char>(lo >= 10 ? lo - 10 + 'A' : lo + '0');
}

}  // namespace

// Load  Rules
#include "base/japanese_util_rule.h"

void Util::HiraganaToKatakana(const StringPiece input, string *output) {
  TextConverter::Convert(hiragana_to_katakana_da,
                         hiragana_to_katakana_table,
                         input,
                         output);
}

void Util::HiraganaToHalfwidthKatakana(const StringPiece input,
                                       string *output) {
  // combine two rules
  string tmp;
  TextConverter::Convert(hiragana_to_katakana_da,
                         hiragana_to_katakana_table,
                         input, &tmp);
  TextConverter::Convert(fullwidthkatakana_to_halfwidthkatakana_da,
                         fullwidthkatakana_to_halfwidthkatakana_table,
                         tmp, output);
}

void Util::HiraganaToRomanji(const StringPiece input, string *output) {
  TextConverter::Convert(hiragana_to_romanji_da,
                         hiragana_to_romanji_table,
                         input,
                         output);
}

void Util::HalfWidthAsciiToFullWidthAscii(const StringPiece input,
                                          string *output) {
  TextConverter::Convert(halfwidthascii_to_fullwidthascii_da,
                         halfwidthascii_to_fullwidthascii_table,
                         input,
                         output);
}

void Util::FullWidthAsciiToHalfWidthAscii(const StringPiece input,
                                          string *output) {
  TextConverter::Convert(fullwidthascii_to_halfwidthascii_da,
                         fullwidthascii_to_halfwidthascii_table,
                         input,
                         output);
}

void Util::HiraganaToFullwidthRomanji(const StringPiece input, string *output) {
  string tmp;
  TextConverter::Convert(hiragana_to_romanji_da,
                         hiragana_to_romanji_table,
                         input,
                         &tmp);
  TextConverter::Convert(halfwidthascii_to_fullwidthascii_da,
                         halfwidthascii_to_fullwidthascii_table,
                         tmp,
                         output);
}

void Util::RomanjiToHiragana(const StringPiece input, string *output) {
  TextConverter::Convert(romanji_to_hiragana_da,
                         romanji_to_hiragana_table,
                         input,
                         output);
}

void Util::KatakanaToHiragana(const StringPiece input, string *output) {
  TextConverter::Convert(katakana_to_hiragana_da,
                         katakana_to_hiragana_table,
                         input,
                         output);
}

void Util::HalfWidthKatakanaToFullWidthKatakana(const StringPiece input,
                                                string *output) {
  TextConverter::Convert(halfwidthkatakana_to_fullwidthkatakana_da,
                         halfwidthkatakana_to_fullwidthkatakana_table,
                         input,
                         output);
}

void Util::FullWidthKatakanaToHalfWidthKatakana(const StringPiece input,
                                                string *output) {
  TextConverter::Convert(fullwidthkatakana_to_halfwidthkatakana_da,
                         fullwidthkatakana_to_halfwidthkatakana_table,
                         input,
                         output);
}

void Util::FullWidthToHalfWidth(const StringPiece input, string *output) {
  string tmp;
  FullWidthAsciiToHalfWidthAscii(input, &tmp);
  output->clear();
  FullWidthKatakanaToHalfWidthKatakana(tmp, output);
}

void Util::HalfWidthToFullWidth(const StringPiece input, string *output) {
  string tmp;
  HalfWidthAsciiToFullWidthAscii(input, &tmp);
  output->clear();
  HalfWidthKatakanaToFullWidthKatakana(tmp, output);
}

// TODO(tabata): Add another function to split voice mark
// of some UNICODE only characters (required to display
// and commit for old clients)
void Util::NormalizeVoicedSoundMark(StringPiece input, string *output) {
  TextConverter::Convert(normalize_voiced_sound_da,
                         normalize_voiced_sound_table,
                         input,
                         output);
}

namespace {
class BracketHandler {
 public:
  BracketHandler() {
    VLOG(1) << "Init bracket mapping";

    const struct BracketType {
      const char *open_bracket;
      const char *close_bracket;
    } kBracketType[] = {
      //  { "（", "）" },
      //  { "〔", "〕" },
      //  { "［", "］" },
      //  { "｛", "｝" },
      //  { "〈", "〉" },
      //  { "《", "》" },
      //  { "「", "」" },
      //  { "『", "』" },
      //  { "【", "】" },
      //  { "〘", "〙" },
      //  { "〚", "〛" },
      { "\xEF\xBC\x88", "\xEF\xBC\x89" },
      { "\xE3\x80\x94", "\xE3\x80\x95" },
      { "\xEF\xBC\xBB", "\xEF\xBC\xBD" },
      { "\xEF\xBD\x9B", "\xEF\xBD\x9D" },
      { "\xE3\x80\x88", "\xE3\x80\x89" },
      { "\xE3\x80\x8A", "\xE3\x80\x8B" },
      { "\xE3\x80\x8C", "\xE3\x80\x8D" },
      { "\xE3\x80\x8E", "\xE3\x80\x8F" },
      { "\xE3\x80\x90", "\xE3\x80\x91" },
      { "\xe3\x80\x98", "\xe3\x80\x99" },
      { "\xe3\x80\x9a", "\xe3\x80\x9b" },
      { NULL, NULL },  // sentinel
    };
    string open_full_width, open_half_width;
    string close_full_width, close_half_width;

    for (size_t i = 0;
         (kBracketType[i].open_bracket != NULL ||
          kBracketType[i].close_bracket != NULL);
         ++i) {
      Util::FullWidthToHalfWidth(kBracketType[i].open_bracket,
                                 &open_full_width);
      Util::HalfWidthToFullWidth(kBracketType[i].open_bracket,
                                 &open_half_width);
      Util::FullWidthToHalfWidth(kBracketType[i].close_bracket,
                                 &close_full_width);
      Util::HalfWidthToFullWidth(kBracketType[i].close_bracket,
                                 &close_half_width);
      open_bracket_[open_half_width]   = close_half_width;
      open_bracket_[open_full_width]   = close_full_width;
      close_bracket_[close_half_width] = open_half_width;
      close_bracket_[close_full_width] = open_full_width;
    }
  }
  ~BracketHandler() {}

  bool IsOpenBracket(const string &key, string *close_bracket) const {
    map<string, string>::const_iterator it =
        open_bracket_.find(key);
    if (it == open_bracket_.end()) {
      return false;
    }
    *close_bracket = it->second;
    return true;
  }

  bool IsCloseBracket(const string &key, string *open_bracket) const {
    map<string, string>::const_iterator it =
        close_bracket_.find(key);
    if (it == close_bracket_.end()) {
      return false;
    }
    *open_bracket = it->second;
    return true;
  }

 private:
  map<string, string> open_bracket_;
  map<string, string> close_bracket_;
};
}  // namespace

bool Util::IsOpenBracket(const string &key, string *close_bracket) {
  return Singleton<BracketHandler>::get()->IsOpenBracket(key, close_bracket);
}

bool Util::IsCloseBracket(const string &key, string *open_bracket) {
  return Singleton<BracketHandler>::get()->IsCloseBracket(key, open_bracket);
}

bool Util::IsFullWidthSymbolInHalfWidthKatakana(const string &input) {
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

bool Util::IsHalfWidthKatakanaSymbol(const string &input) {
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

bool Util::IsKanaSymbolContained(const string &input) {
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

bool Util::IsEnglishTransliteration(const string &value) {
  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] == 0x20 || value[i] == 0x21 ||
        value[i] == 0x27 || value[i] == 0x2D ||
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
void Util::EncodeURI(const string &input, string *output) {
  const char kDigits[] = "0123456789ABCDEF";
  const char *begin = input.data();
  const char *end = input.data() + input.size();
  output->clear();
  while (begin < end) {
    if (isascii(*begin) &&
        (isdigit(*begin) || isalpha(*begin))) {
      *output += *begin;
    } else {
      *output += '%';
      *output += kDigits[(*begin >> 4) & 0x0f];
      *output += kDigits[*begin & 0x0f];
    }
    ++begin;
  }
}

void Util::DecodeURI(const string &src, string *output) {
  output->clear();
  const char *p = src.data();
  const char *end = src.data() + src.size();
  while (p < end) {
    if (*p == '%' && p + 2 < end) {
      const char h = toupper(p[1]);
      const char l = toupper(p[2]);
      const int vh = isalpha(h) ? (10 + (h -'A')) : (h - '0');
      const int vl = isalpha(l) ? (10 + (l -'A')) : (l - '0');
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

void Util::AppendCGIParams(const vector<pair<string, string> > &params,
                           string *base) {
  if (params.size() == 0 || base == NULL) {
    return;
  }

  string encoded;
  for (vector<pair<string, string> >::const_iterator it = params.begin();
       it != params.end();
       ++it) {
    // Append "<first>=<encoded second>&"
    base->append(it->first);
    base->append("=");
    EncodeURI(it->second, &encoded);
    base->append(encoded);
    base->append("&");
  }

  // Delete the last "&".
  if (!base->empty()) {
    base->erase(base->size() - 1);
  }
}

void Util::Escape(const string &input, string *output) {
  output->clear();
  for (size_t i = 0; i < input.size(); ++i) {
    EscapeInternal(input[i], "\\x", output);
  }
}

void Util::EscapeUrl(const string &input, string *output) {
  output->clear();
  for (size_t i = 0; i < input.size(); ++i) {
    EscapeInternal(input[i], "%", output);
  }
}

string Util::EscapeUrl(const string &input) {
  string escaped_input;
  EscapeUrl(input, &escaped_input);
  return escaped_input;
}

void Util::EscapeHtml(const string &plain, string *escaped) {
  string tmp1, tmp2, tmp3, tmp4;
  StringReplace(plain, "&", "&amp;", true, &tmp1);
  StringReplace(tmp1, "<", "&lt;", true, &tmp2);
  StringReplace(tmp2, ">", "&gt;", true, &tmp3);
  StringReplace(tmp3, "\"", "&quot;", true, &tmp4);
  StringReplace(tmp4, "'", "&#39;", true, escaped);
}

void Util::UnescapeHtml(const string &escaped, string *plain) {
  string tmp1, tmp2, tmp3, tmp4;
  StringReplace(escaped, "&amp;", "&", true, &tmp1);
  StringReplace(tmp1, "&lt;", "<", true, &tmp2);
  StringReplace(tmp2, "&gt;", ">", true, &tmp3);
  StringReplace(tmp3, "&quot;", "\"", true, &tmp4);
  StringReplace(tmp4, "&#39;", "'", true, plain);
}

void Util::EscapeCss(const string &plain, string *escaped) {
  // ">" and "&" are not escaped because they are used for operands of
  // CSS.
  StringReplace(plain, "<", "&lt;", true, escaped);
}

#define INRANGE(w, a, b) ((w) >= (a) && (w) <= (b))

// script type
// TODO(yukawa, team): Make a mechanism to keep this classifier up-to-date
//   based on the original data from Unicode.org.
Util::ScriptType Util::GetScriptType(char32 w) {
  if (INRANGE(w, 0x0030, 0x0039) ||    // ascii number
      INRANGE(w, 0xFF10, 0xFF19)) {    // full width number
    return NUMBER;
  } else if (
      INRANGE(w, 0x0041, 0x005A) ||    // ascii upper
      INRANGE(w, 0x0061, 0x007A) ||    // ascii lower
      INRANGE(w, 0xFF21, 0xFF3A) ||    // fullwidth ascii upper
      INRANGE(w, 0xFF41, 0xFF5A)) {    // fullwidth ascii lower
    return ALPHABET;
  } else if (
      w == 0x3005 ||                   // IDEOGRAPHIC ITERATION MARK "々"
      INRANGE(w, 0x3400, 0x4DBF) ||    // CJK Unified Ideographs Extension A
      INRANGE(w, 0x4E00, 0x9FFF) ||    // CJK Unified Ideographs
      INRANGE(w, 0xF900, 0xFAFF) ||    // CJK Compatibility Ideographs
      INRANGE(w, 0x20000, 0x2A6DF) ||  // CJK Unified Ideographs Extension B
      INRANGE(w, 0x2A700, 0x2B73F) ||  // CJK Unified Ideographs Extension C
      INRANGE(w, 0x2B740, 0x2B81F) ||  // CJK Unified Ideographs Extension D
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
  } else if (
      INRANGE(w, 0x3041, 0x309F) ||    // hiragana
      w == 0x1B001) {                  // HIRAGANA LETTER ARCHAIC YE
    return HIRAGANA;
  } else if (
      INRANGE(w, 0x30A1, 0x30FF) ||    // full width katakana
      INRANGE(w, 0x31F0, 0x31FF) ||    // Katakana Phonetic Extensions for Ainu
      INRANGE(w, 0xFF65, 0xFF9F) ||    // half width katakana
      w == 0x1B000) {                  // KATAKANA LETTER ARCHAIC E
    return KATAKANA;
  } else if (
      INRANGE(w, 0x02300, 0x023F3) ||  // Miscellaneous Technical
      INRANGE(w, 0x02700, 0x027BF) ||  // Dingbats
      INRANGE(w, 0x1F000, 0x1F02F) ||  // Mahjong tiles
      INRANGE(w, 0x1F030, 0x1F09F) ||  // Domino tiles
      INRANGE(w, 0x1F0A0, 0x1F0FF) ||  // Playing cards
      INRANGE(w, 0x1F100, 0x1F2FF) ||  // Enclosed Alphanumeric Supplement
      INRANGE(w, 0x1F200, 0x1F2FF) ||  // Enclosed Ideographic Supplement
      INRANGE(w, 0x1F300, 0x1F5FF) ||  // Miscellaneous Symbols And Pictographs
      INRANGE(w, 0x1F600, 0x1F64F) ||  // Emoticons
      INRANGE(w, 0x1F680, 0x1F6FF) ||  // Transport And Map Symbols
      INRANGE(w, 0x1F700, 0x1F77F) ||  // Alchemical Symbols
      w == 0x26CE) {                   // Ophiuchus
    return EMOJI;
  }

  return UNKNOWN_SCRIPT;
}

Util::FormType Util::GetFormType(char32 w) {
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
Util::ScriptType Util::GetScriptType(const char *begin,
                                     const char *end, size_t *mblen) {
  const char32 w = UTF8ToUCS4(begin, end, mblen);
  return GetScriptType(w);
}

namespace {

Util::ScriptType GetScriptTypeInternal(const string &str,
                                       bool ignore_symbols) {
  Util::ScriptType result = Util::SCRIPT_TYPE_SIZE;

  for (ConstChar32Iterator iter(str); !iter.Done(); iter.Next()) {
    const char32 w = iter.Get();
    Util::ScriptType type = Util::GetScriptType(w);
    if ((w == 0x30FC || w == 0x30FB || (w >= 0x3099 && w <= 0x309C)) &&
        // PROLONGEDSOUND MARK|MIDLE_DOT|VOICED_SOUND_MARKS
        // are HIRAGANA as well
        (result == Util::SCRIPT_TYPE_SIZE ||
         result == Util::HIRAGANA || result == Util::KATAKANA)) {
      type = result;  // restore the previous state
    }

    // Ignore symbols
    // Regard UNKNOWN_SCRIPT as symbols here
    if (ignore_symbols &&
        result != Util::UNKNOWN_SCRIPT &&
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

Util::ScriptType Util::GetScriptType(const string &str) {
  return GetScriptTypeInternal(str, false);
}

Util::ScriptType Util::GetFirstScriptType(const string &str) {
  size_t mblen = 0;
  return GetScriptType(str.c_str(),
                       str.c_str() + str.size(),
                       &mblen);
}

Util::ScriptType Util::GetScriptTypeWithoutSymbols(const string &str) {
  return GetScriptTypeInternal(str, true);
}

// return true if all script_type in str is "type"
bool Util::IsScriptType(const StringPiece str, Util::ScriptType type) {
  for (ConstChar32Iterator iter(str); !iter.Done(); iter.Next()) {
    const char32 w = iter.Get();
    // Exception: 30FC (PROLONGEDSOUND MARK is categorized as HIRAGANA as well)
    if (type != GetScriptType(w) && (w != 0x30FC || type != HIRAGANA)) {
      return false;
    }
  }
  return true;
}

// return true if the string contains script_type char
bool Util::ContainsScriptType(const StringPiece str, ScriptType type) {
  for (ConstChar32Iterator iter(str); !iter.Done(); iter.Next()) {
    if (type == GetScriptType(iter.Get())) {
      return true;
    }
  }
  return false;
}

// return the Form Type of string
Util::FormType Util::GetFormType(const string &str) {
  // TODO(hidehiko): get rid of using FORM_TYPE_SIZE.
  FormType result = FORM_TYPE_SIZE;

  for (ConstChar32Iterator iter(str); !iter.Done(); iter.Next()) {
    const FormType type = GetFormType(iter.Get());
    if (type == UNKNOWN_FORM ||
        (result != FORM_TYPE_SIZE && type != result)) {
      return UNKNOWN_FORM;
    }
    result = type;
  }

  return result;
}

// Util::CharcterSet Util::GetCharacterSet(char32 ucs4);
#include "base/character_set.h"

Util::CharacterSet Util::GetCharacterSet(const string &str) {
  CharacterSet result = ASCII;
  for (ConstChar32Iterator iter(str); !iter.Done(); iter.Next()) {
    result = max(result, GetCharacterSet(iter.Get()));
  }
  return result;
}

}  // namespace mozc
