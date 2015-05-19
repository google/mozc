// Copyright 2010-2012, Google Inc.
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

#ifdef OS_WINDOWS
#include <Windows.h>
#include <LMCons.h>
#include <Sddl.h>
#include <ShlObj.h>
#include <WinCrypt.h>
#include <time.h>
#include <stdio.h>  // MSVC requires this for _vsnprintf
// #include <KnownFolders.h>
#else  // OS_WINDOWS
#ifdef OS_MACOSX
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <sys/sysctl.h>
#elif defined(__native_client__)  // OS_MACOSX
#include <irt.h>
#endif  // OS_MACOSX or __native_client__
#include <pwd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // OS_WINDOWS
#include <algorithm>
#include <cctype>
#ifdef OS_MACOSX
#include <cerrno>
#endif  // OS_MACOSX
#include <cstdarg>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/base.h"
#include "base/const.h"
#include "base/file_stream.h"
#include "base/logging.h"
#include "base/mmap.h"
#include "base/mutex.h"
#include "base/number_util.h"
#include "base/port.h"
#include "base/scoped_handle.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/text_converter.h"

#ifdef OS_MACOSX
#include "base/mac_util.h"
#endif
#include "base/win_util.h"

#ifdef OS_ANDROID
// HACK to avoid a bug in sysconf in android.
#include "android/sysconf.h"
#define sysconf mysysconf
#include "base/android_util.h"
#endif


// Ad-hoc workadound against macro problem on Windows.
// On Windows, following macros, defined when you include <Windows.h>,
// should be removed here because they affects the method name definition of
// Util class.
// TODO(yukawa): Use different method name if applicable.
#ifdef CreateDirectory
#undef CreateDirectory
#endif  // CreateDirectory
#ifdef RemoveDirectory
#undef RemoveDirectory
#endif  // RemoveDirectory
#ifdef CopyFile
#undef CopyFile
#endif  // CopyFile

namespace {

const char kFileDelimiterForUnix = '/';
const char kFileDelimiterForWindows = '\\';

#ifdef OS_WINDOWS
const char kFileDelimiter = kFileDelimiterForWindows;
volatile mozc::Util::IsWindowsX64Mode g_is_windows_x64_mode =
    mozc::Util::IS_WINDOWS_X64_DEFAULT_MODE;
#else
const char kFileDelimiter = kFileDelimiterForUnix;
#endif

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

template <class ITR>
void SplitStringToIteratorUsing(const string &full,
                                const char *delim,
                                ITR &result) {
  // Optimize the common case where delim is a single character.
  if (delim[0] != '\0' && delim[1] == '\0') {
    char c = delim[0];
    const char *p = full.data();
    const char *end = p + full.size();
    while (p != end) {
      if (*p == c) {
        ++p;
      } else {
        const char *start = p;
        while (++p != end && *p != c) {}
        *result++ = string(start, p - start);
      }
    }
    return;
  }

  string::size_type begin_index, end_index;
  begin_index = full.find_first_not_of(delim);
  while (begin_index != string::npos) {
    end_index = full.find_first_of(delim, begin_index);
    if (end_index == string::npos) {
      *result++ = full.substr(begin_index);
      return;
    }
    *result++ = full.substr(begin_index, (end_index - begin_index));
    begin_index = full.find_first_not_of(delim, end_index);
  }
}

template <typename ITR>
void SplitStringToIteratorAllowEmpty(const string &full,
                                     const char *delim,
                                     ITR &result) {
  string::size_type begin_index, end_index;
  begin_index = 0;

  for (size_t i = 0; ; i++) {
    end_index = full.find_first_of(delim, begin_index);
    if (end_index == string::npos) {
      *result++ = full.substr(begin_index);
      return;
    }
    *result++ = full.substr(begin_index, (end_index - begin_index));
    begin_index = end_index + 1;
  }
}

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
void Util::SplitStringUsing(const string &str,
                            const char *delim,
                            vector<string> *output) {
  back_insert_iterator<vector<string> > it(*output);
  SplitStringToIteratorUsing(str, delim, it);
}

void Util::SplitStringAllowEmpty(const string &full, const char *delim,
                                 vector<string> *result) {
  back_insert_iterator<vector<string> > it(*result);
  SplitStringToIteratorAllowEmpty(full, delim, it);
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
  scoped_array<char> tmp(new char[input.size() + 1]);
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

void Util::AppendStringWithDelimiter(const string &delimiter,
                                     const string &append_string,
                                     string *output) {
  CHECK(output);
  if (!output->empty()) {
    output->append(delimiter);
  }
  output->append(append_string);
}


void Util::StringReplace(const string &s, const string &oldsub,
                         const string &newsub, bool replace_all,
                         string *res) {
  if (oldsub.empty()) {
    res->append(s);  // if empty, append the given string.
    return;
  }

  string::size_type start_pos = 0;
  string::size_type pos;
  do {
    pos = s.find(oldsub, start_pos);
    if (pos == string::npos) {
      break;
    }
    res->append(s, start_pos, pos - start_pos);
    res->append(newsub);
    start_pos = pos + oldsub.size();  // start searching again after the "old"
  } while (replace_all);
  res->append(s, start_pos, s.length() - start_pos);
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
    char32 ucs4 = Util::UTF8ToUCS4(begin + pos, begin + str->size(), &mblen);
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
    char32 ucs4 = Util::UTF8ToUCS4(begin + pos, begin + str->size(), &mblen);
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
  Util::SubString(*str, 0, 1, &first_str);
  Util::UpperString(&first_str);

  string tailing_str;
  Util::SubString(*str, 1, string::npos, &tailing_str);
  Util::LowerString(&tailing_str);

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
    begin += Util::OneCharLen(begin);
  }
  return result;
}

char32 Util::UTF8ToUCS4(const char *begin,
                        const char *end,
                        size_t *mblen) {
  DCHECK_LE(begin, end);
  const size_t len = static_cast<size_t>(end - begin);
  if (begin == end) {
    *mblen = 0;
    return 0;
  }

  if (static_cast<unsigned char>(begin[0]) < 0x80) {
    *mblen = 1;
    return static_cast<unsigned char>(begin[0]);
  } else if (len >= 2 && (begin[0] & 0xe0) == 0xc0) {
    *mblen = 2;
    return ((begin[0] & 0x1f) << 6) | (begin[1] & 0x3f);
  } else if (len >= 3 && (begin[0] & 0xf0) == 0xe0) {
    *mblen = 3;
    return ((begin[0] & 0x0f) << 12) |
        ((begin[1] & 0x3f) << 6) | (begin[2] & 0x3f);
  } else if (len >= 4 && (begin[0] & 0xf8) == 0xf0) {
    *mblen = 4;
    return ((begin[0] & 0x07) << 18) |
        ((begin[1] & 0x3f) << 12) | ((begin[2] & 0x3f) << 6) |
        (begin[3] & 0x3f);
  // Below is out of UCS4 but acceptable in 32-bit.
  } else if (len >= 5 && (begin[0] & 0xfc) == 0xf8) {
    *mblen = 5;
    return ((begin[0] & 0x03) << 24) |
        ((begin[1] & 0x3f) << 18) | ((begin[2] & 0x3f) << 12) |
        ((begin[3] & 0x3f) << 6) | (begin[4] & 0x3f);
  } else if (len >= 6 && (begin[0] & 0xfe) == 0xfc) {
    *mblen = 6;
    return ((begin[0] & 0x01) << 30) |
        ((begin[1] & 0x3f) << 24) | ((begin[2] & 0x3f) << 18) |
        ((begin[3] & 0x3f) << 12) | ((begin[4] & 0x3f) << 6) |
        (begin[5] & 0x3f);
  } else {
    *mblen = 1;
    return 0;
  }
}

uint16 Util::UTF8ToUCS2(const char *begin,
                        const char *end,
                        size_t *mblen) {
  return static_cast<uint16>(UTF8ToUCS4(begin, end, mblen));
}

namespace {
void UCS4ToUTF8Internal(char32 c, char *output) {
  if (c < 0x00080) {
    output[0] = static_cast<char>(c & 0xFF);
    output[1] = '\0';
  } else if (c < 0x00800) {
    output[0] = static_cast<char>(0xC0 + ((c >> 6) & 0x1F));
    output[1] = static_cast<char>(0x80 + (c & 0x3F));
    output[2] = '\0';
  } else if (c < 0x10000) {
    output[0] = static_cast<char>(0xE0 + ((c >> 12) & 0x0F));
    output[1] = static_cast<char>(0x80 + ((c >> 6) & 0x3F));
    output[2] = static_cast<char>(0x80 + (c & 0x3F));
    output[3] = '\0';
  } else if (c < 0x200000) {
    output[0] = static_cast<char>(0xF0 + ((c >> 18) & 0x07));
    output[1] = static_cast<char>(0x80 + ((c >> 12) & 0x3F));
    output[2] = static_cast<char>(0x80 + ((c >> 6)  & 0x3F));
    output[3] = static_cast<char>(0x80 + (c & 0x3F));
    output[4] = '\0';
  // below is not in UCS4 but in 32bit int.
  } else if (c < 0x8000000) {
    output[0] = static_cast<char>(0xF8 + ((c >> 24) & 0x03));
    output[1] = static_cast<char>(0x80 + ((c >> 18) & 0x3F));
    output[2] = static_cast<char>(0x80 + ((c >> 12) & 0x3F));
    output[3] = static_cast<char>(0x80 + ((c >> 6)  & 0x3F));
    output[4] = static_cast<char>(0x80 + (c & 0x3F));
    output[5] = '\0';
  } else {
    output[0] = static_cast<char>(0xFC + ((c >> 30) & 0x01));
    output[1] = static_cast<char>(0x80 + ((c >> 24) & 0x3F));
    output[2] = static_cast<char>(0x80 + ((c >> 18) & 0x3F));
    output[3] = static_cast<char>(0x80 + ((c >> 12) & 0x3F));
    output[4] = static_cast<char>(0x80 + ((c >> 6)  & 0x3F));
    output[5] = static_cast<char>(0x80 + (c & 0x3F));
    output[6] = '\0';
  }
}
}   // namespace

void Util::UCS4ToUTF8(char32 c, string *output) {
  output->clear();
  UCS4ToUTF8Append(c, output);
}

void Util::UCS4ToUTF8Append(char32 c, string *output) {
  char buf[7];
  UCS4ToUTF8Internal(c, buf);
  *output += buf;
}
void Util::UCS2ToUTF8(uint16 c, string *output) {
  UCS4ToUTF8(c, output);
}

void Util::UCS2ToUTF8Append(uint16 c, string *output) {
  UCS4ToUTF8Append(c, output);
}

#ifdef OS_WINDOWS
size_t Util::WideCharsLen(const char *src) {
  const int len = ::MultiByteToWideChar(CP_UTF8, 0, src, -1, NULL, 0);
  if (len <= 0) {
    return 0;
  }
  return len - 1;  // -1 represents Null termination.
}

size_t Util::WideCharsLen(const string &src) {
  return WideCharsLen(src.c_str());
}

int Util::UTF8ToWide(const char *input, wstring *output) {
  const int output_length = WideCharsLen(input);
  if (output_length == 0) {
    return 0;
  }

  scoped_array<wchar_t> input_wide(new wchar_t[output_length + 1]);
  const int result = MultiByteToWideChar(CP_UTF8, 0, input, -1,
                                         input_wide.get(), output_length + 1);
  if (result > 0) {
    output->assign(input_wide.get());
  }
  return result;
}

int Util::UTF8ToWide(const string &input, wstring *output) {
  return Util::UTF8ToWide(input.c_str(), output);
}

int Util::WideToUTF8(const wchar_t *input, string *output) {
  const int output_length = WideCharToMultiByte(CP_UTF8, 0, input, -1, NULL, 0,
                                                NULL, NULL);
  if (output_length == 0) {
    return 0;
  }

  scoped_array<char> input_encoded(new char[output_length + 1]);
  const int result = WideCharToMultiByte(CP_UTF8, 0, input, -1,
                                         input_encoded.get(),
                                         output_length + 1, NULL, NULL);
  if (result > 0) {
    output->assign(input_encoded.get());
  }
  return result;
}

int Util::WideToUTF8(const wstring &input, string *output) {
  return Util::WideToUTF8(input.c_str(), output);
}
#endif

void Util::SubString(const string &src,
                     const size_t start,
                     const size_t length,
                     string *result) {
  DCHECK(result);
  result->clear();

  size_t l = start;
  const char *begin = src.c_str();
  const char *end = begin + src.size();
  while (l > 0) {
    begin += Util::OneCharLen(begin);
    --l;
  }

  l = length;
  while (l > 0 && begin < end) {
    const size_t len = Util::OneCharLen(begin);
    result->append(begin, len);
    begin += len;
    --l;
  }

  return;
}

bool Util::StartsWith(const string &str, const string &prefix) {
  if (str.size() < prefix.size()) {
    return false;
  }
  return (0 == memcmp(str.data(), prefix.data(), prefix.size()));
}

bool Util::EndsWith(const string &str, const string &suffix) {
  if (str.size() < suffix.size()) {
    return false;
  }
  return (0 == memcmp(str.data() + str.size() - suffix.size(),
                      suffix.data(), suffix.size()));
}

void Util::StripUTF8BOM(string *line) {
  const char kUTF8BOM[] = "\xef\xbb\xbf";
  if (line->substr(0, 3) == kUTF8BOM) {
    line->erase(0, 3);
  }
}

bool Util::IsUTF16BOM(const string &line) {
  const char kUTF16LEBOM[] = "\xff\xfe";
  const char kUTF16BEBOM[] = "\xfe\xff";
  if (line.size() >= 2 &&
      (line.substr(0, 2) == kUTF16LEBOM ||
       line.substr(0, 2) == kUTF16BEBOM)) {
    return true;
  }
  return false;
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

bool Util::GetSecureRandomSequence(char *buf, size_t buf_size) {
  memset(buf, '\0', buf_size);
#ifdef OS_WINDOWS
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
#else  // !OS_WINDOWS && !__native_client__
  // Use non blocking interface on Linux.
  // Mac also have /dev/urandom (although it's identical with /dev/random)
  ifstream ifs("/dev/urandom", ios::binary);
  if (!ifs) {
    return false;
  }
  ifs.read(buf, buf_size);
  return true;
#endif
}

bool Util::GetSecureRandomAsciiSequence(char *buf, size_t buf_size) {
  // We use this map to convert a random byte value to an ascii character.
  // Its size happens to be 64, which is just one fourth of the number of
  // values that can be represented by a sigle byte value. This accidental
  // coincidence makes implementation of the method quite simple.
  const char kCharMap[] =
      "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_";
  if (!GetSecureRandomSequence(buf, buf_size)) {
    return false;
  }
  for (size_t i = 0; i < buf_size; ++i) {
    // The size of kCharMap is just one fourth of 256. So we don't need to
    // care if probability distribution over the characters is biased.
    buf[i] = kCharMap[static_cast<unsigned char>(buf[i]) % 64];
  }
  return true;
}

int Util::Random(int size) {
  return static_cast<int> (1.0 * size * rand() / (RAND_MAX + 1.0));
}

void Util::SetRandomSeed(uint32 seed) {
  ::srand(seed);
}

namespace {
class ClockImpl : public Util::ClockInterface {
 public:
  ClockImpl() {}
  virtual ~ClockImpl() {}

  virtual void GetTimeOfDay(uint64 *sec, uint32 *usec) {
#ifdef OS_WINDOWS
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
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    *sec = tv.tv_sec;
    *usec = tv.tv_usec;
#endif
  }

  virtual uint64 GetTime() {
#ifdef OS_WINDOWS
    return static_cast<uint64>(_time64(NULL));
# else
    return static_cast<uint64>(time(NULL));
# endif
  }

  virtual bool GetTmWithOffsetSecond(time_t offset_sec, tm *output) {
    const time_t current_sec = static_cast<time_t>(this->GetTime());
    const time_t modified_sec = current_sec + offset_sec;

#ifdef OS_WINDOWS
    if (_localtime64_s(output, &modified_sec) != 0) {
      return false;
    }
#else
    if (localtime_r(&modified_sec, output) == NULL) {
      return false;
    }
#endif
    return true;
  }

  virtual uint64 GetFrequency() {
#if defined(OS_WINDOWS)
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
#else
    return 1000000uLL;
#endif  // HAVE_LIBRT
#else
#error "Not supported platform"
#endif  // platforms (OS_WINDOWS, OS_MACOSX, OS_LINUX, ...)
  }

  virtual uint64 GetTicks() {
#if defined(OS_WINDOWS)
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
#else
    // librt is not linked on Android, so we uses GetTimeOfDay instead.
    // GetFrequency() always returns 1MHz when librt is not available,
    // so we uses microseconds as ticks.
    uint64 sec;
    uint32 usec;
    GetTimeOfDay(&sec, &usec);
    return sec * 1000000 + usec;
#endif  // HAVE_LIBRT
#else
#error "Not supported platform"
#endif  // platforms (OS_WINDOWS, OS_MACOSX, OS_LINUX, ...)
  }
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
#ifdef OS_WINDOWS
  ::Sleep(msec);
#else
  usleep(msec * 1000);
#endif
}

namespace {
void EscapeInternal(char input, const string &prefix, string *output) {
  const int hi = ((static_cast<int>(input) & 0xF0) >> 4);
  const int lo = (static_cast<int>(input) & 0x0F);
  *output += prefix;
  *output += static_cast<char>(hi >= 10 ? hi - 10 + 'A' : hi + '0');
  *output += static_cast<char>(lo >= 10 ? lo - 10 + 'A' : lo + '0');
}
}  // end of anonymous namespace

// Load  Rules
#include "base/japanese_util_rule.h"

void Util::HiraganaToKatakana(const string &input,
                              string *output) {
  TextConverter::Convert(hiragana_to_katakana_da,
                         hiragana_to_katakana_table,
                         input,
                         output);
}

void Util::HiraganaToHalfwidthKatakana(const string &input,
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

void Util::HiraganaToRomanji(const string &input,
                             string *output) {
  TextConverter::Convert(hiragana_to_romanji_da,
                         hiragana_to_romanji_table,
                         input,
                         output);
}

void Util::HalfWidthAsciiToFullWidthAscii(const string &input,
                                          string *output) {
  TextConverter::Convert(halfwidthascii_to_fullwidthascii_da,
                         halfwidthascii_to_fullwidthascii_table,
                         input,
                         output);
}

void Util::FullWidthAsciiToHalfWidthAscii(const string &input,
                                          string *output) {
  TextConverter::Convert(fullwidthascii_to_halfwidthascii_da,
                         fullwidthascii_to_halfwidthascii_table,
                         input,
                         output);
}

void Util::HiraganaToFullwidthRomanji(const string &input,
                                      string *output) {
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

void Util::RomanjiToHiragana(const string &input,
                             string *output) {
  TextConverter::Convert(romanji_to_hiragana_da,
                         romanji_to_hiragana_table,
                         input,
                         output);
}

void Util::KatakanaToHiragana(const string &input,
                              string *output) {
  TextConverter::Convert(katakana_to_hiragana_da,
                         katakana_to_hiragana_table,
                         input,
                         output);
}

void Util::HalfWidthKatakanaToFullWidthKatakana(const string &input,
                                                string *output) {
  TextConverter::Convert(halfwidthkatakana_to_fullwidthkatakana_da,
                         halfwidthkatakana_to_fullwidthkatakana_table,
                         input,
                         output);
}

void Util::FullWidthKatakanaToHalfWidthKatakana(const string &input,
                                                string *output) {
  TextConverter::Convert(fullwidthkatakana_to_halfwidthkatakana_da,
                         fullwidthkatakana_to_halfwidthkatakana_table,
                         input,
                         output);
}

void Util::FullWidthToHalfWidth(const string &input, string *output) {
  string tmp;
  Util::FullWidthAsciiToHalfWidthAscii(input, &tmp);
  output->clear();
  Util::FullWidthKatakanaToHalfWidthKatakana(tmp, output);
}

void Util::HalfWidthToFullWidth(const string &input, string *output) {
  string tmp;
  Util::HalfWidthAsciiToFullWidthAscii(input, &tmp);
  output->clear();
  Util::HalfWidthKatakanaToFullWidthKatakana(tmp, output);
}

// TODO(tabata): Add another function to split voice mark
// of some UNICODE only characters (required to display
// and commit for old clients)
void Util::NormalizeVoicedSoundMark(const string &input,
                                    string *output) {
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
      mozc::Util::FullWidthToHalfWidth(kBracketType[i].open_bracket,
                                       &open_full_width);
      mozc::Util::HalfWidthToFullWidth(kBracketType[i].open_bracket,
                                       &open_half_width);
      mozc::Util::FullWidthToHalfWidth(kBracketType[i].close_bracket,
                                       &close_full_width);
      mozc::Util::HalfWidthToFullWidth(kBracketType[i].close_bracket,
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

#ifndef MOZC_USE_PEPPER_FILE_IO

bool Util::Unlink(const string &filename) {
#ifdef OS_WINDOWS
  wstring wide;
  return Util::UTF8ToWide(filename.c_str(), &wide) > 0 &&
      ::DeleteFileW(wide.c_str()) != 0;
#else  // OS_WINDOWS
  return ::unlink(filename.c_str()) == 0;
#endif  // OS_WINDOWS
}

bool Util::RemoveDirectory(const string &dirname) {
#ifdef OS_WINDOWS
  wstring wide;
  return (Util::UTF8ToWide(dirname.c_str(), &wide) > 0 &&
          ::RemoveDirectoryW(wide.c_str()) != 0);
#else  // OS_WINDOWS
  return ::rmdir(dirname.c_str()) == 0;
#endif  // OS_WINDOWS
}

bool Util::CreateDirectory(const string &path) {
#ifdef OS_WINDOWS
  wstring wide;
  if (Util::UTF8ToWide(path.c_str(), &wide) <= 0)
    return false;
  ::CreateDirectoryW(wide.c_str(), NULL);
#else  // OS_WINDOWS
  ::mkdir(path.c_str(), 0700);
#endif  // OS_WINDOWS
  return true;
}

bool Util::FileExists(const string &filename) {
#ifdef OS_WINDOWS
  wstring wide;
  return (Util::UTF8ToWide(filename.c_str(), &wide) > 0 &&
          ::GetFileAttributesW(wide.c_str()) != -1);
#else  // OS_WINDOWS
  struct stat s;
  return ::stat(filename.c_str(), &s) == 0;
#endif  // OS_WINDOWS
}

bool Util::DirectoryExists(const string &dirname) {
#ifdef OS_WINDOWS
  wstring wide;
  if (Util::UTF8ToWide(dirname.c_str(), &wide) <= 0) {
    return false;
  }

  const DWORD attribute = ::GetFileAttributesW(wide.c_str());
  return ((attribute != -1) &&
          (attribute & FILE_ATTRIBUTE_DIRECTORY));
#else  // OS_WINDOWS
  struct stat s;
  return (::stat(dirname.c_str(), &s) == 0 && S_ISDIR(s.st_mode));
#endif  // OS_WINDOWS
}

bool Util::Rename(const string &from, const string &to) {
  // If a file whose name is 'to' already exists, do nothing and
  // return false.
  if (FileExists(to)) {
    return false;
  }
#ifdef OS_WINDOWS
  wstring wfrom;
  wstring wto;
  return Util::UTF8ToWide(from.c_str(), &wfrom) > 0 &&
      Util::UTF8ToWide(to.c_str(), &wto) > 0 &&
      ::MoveFileW(wfrom.c_str(), wto.c_str());
#else  // OS_WINDOWS
  return ::rename(from.c_str(), to.c_str()) == 0;
#endif  // OS_WINDOWS
}

#ifdef OS_WINDOWS
namespace {
typedef HANDLE (WINAPI *FPCreateTransaction)(LPSECURITY_ATTRIBUTES,
                                             LPGUID,
                                             DWORD,
                                             DWORD,
                                             DWORD,
                                             DWORD,
                                             LPWSTR);
typedef BOOL (WINAPI *FPMoveFileTransactedW)(LPCTSTR,
                                             LPCTSTR,
                                             LPPROGRESS_ROUTINE,
                                             LPVOID,
                                             DWORD,
                                             HANDLE);
typedef BOOL (WINAPI *FPCommitTransaction)(HANDLE);

FPCreateTransaction   g_create_transaction    = NULL;
FPMoveFileTransactedW g_move_file_transactedw = NULL;
FPCommitTransaction   g_commit_transaction    = NULL;

static once_t g_init_tx_move_file_once = MOZC_ONCE_INIT;

void InitTxMoveFile() {
  if (!Util::IsVistaOrLater()) {
    return;
  }

  const HMODULE lib_ktmw = WinUtil::LoadSystemLibrary(L"ktmw32.dll");
  if (lib_ktmw == NULL) {
    LOG(ERROR) << "LoadSystemLibrary for ktmw32.dll failed.";
    return;
  }

  const HMODULE lib_kernel = WinUtil::GetSystemModuleHandle(L"kernel32.dll");
  if (lib_kernel == NULL) {
    LOG(ERROR) << "LoadSystemLibrary for kernel32.dll failed.";
    return;
  }

  g_create_transaction =
      reinterpret_cast<FPCreateTransaction>
      (::GetProcAddress(lib_ktmw, "CreateTransaction"));

  g_move_file_transactedw =
      reinterpret_cast<FPMoveFileTransactedW>
      (::GetProcAddress(lib_kernel, "MoveFileTransactedW"));

  g_commit_transaction =
      reinterpret_cast<FPCommitTransaction>
      (::GetProcAddress(lib_ktmw, "CommitTransaction"));

  LOG_IF(ERROR, g_create_transaction == NULL)
      << "CreateTransaction init failed";
  LOG_IF(ERROR, g_move_file_transactedw == NULL)
      << "MoveFileTransactedW init failed";
  LOG_IF(ERROR, g_commit_transaction == NULL)
      << "CommitTransaction init failed";
}

bool TransactionalMoveFile(const wstring &from, const wstring &to) {
  CallOnce(&g_init_tx_move_file_once, &InitTxMoveFile);

  if (g_commit_transaction == NULL || g_move_file_transactedw == NULL ||
      g_create_transaction == NULL) {
    // Transactional NTFS is not available.
    return false;
  }

  const DWORD kTimeout = 5000;  // 5 sec.
  ScopedHandle handle((*g_create_transaction)(
      NULL, 0, 0, 0, 0, kTimeout, NULL));
  const DWORD create_transaction_error = ::GetLastError();
  if (handle.get() == 0) {
    LOG(ERROR) << "CreateTransaction failed: " << create_transaction_error;
    return false;
  }

  if (!(*g_move_file_transactedw)(from.c_str(), to.c_str(),
                                  NULL, NULL,
                                  MOVEFILE_COPY_ALLOWED |
                                  MOVEFILE_REPLACE_EXISTING,
                                  handle.get())) {
    const DWORD move_file_transacted_error = ::GetLastError();
    LOG(ERROR) << "MoveFileTransactedW failed: "
               << move_file_transacted_error;
    return false;
  }

  if (!(*g_commit_transaction)(handle.get())) {
    const DWORD commit_transaction_error = ::GetLastError();
    LOG(ERROR) << "CommitTransaction failed: " << commit_transaction_error;
    return false;
  }

  return true;
}

}  // namespace
#endif  // OS_WINDOWS


bool Util::AtomicRename(const string &from, const string &to) {
#ifdef OS_WINDOWS
  wstring fromw, tow;
  Util::UTF8ToWide(from.c_str(), &fromw);
  Util::UTF8ToWide(to.c_str(), &tow);

  if (TransactionalMoveFile(fromw, tow)) {
    return true;
  }

  if (!::MoveFileExW(fromw.c_str(), tow.c_str(),
                     MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING)) {
    const DWORD move_file_ex_error = ::GetLastError();
    LOG(ERROR) << "MoveFileEx failed: " << move_file_ex_error;
    return false;
  }

  return true;
#else    // OS_WINDOWS
  return (0 == rename(from.c_str(), to.c_str()));
#endif   // OS_WINDOWS
}

// This function uses LF as a line separator. This is OK because it is
// used only for test code now. If you want to use it for a general
// purpose, I may need to make it recognize CR and CR+LF as well.
bool Util::CopyTextFile(const string &from, const string &to) {
  InputFileStream ifs(from.c_str());
  if (!ifs) {
    LOG(ERROR) << "Can't open input file. " << from;
    return false;
  }
  OutputFileStream ofs(to.c_str());
  if (!ofs) {
    LOG(ERROR) << "Can't open output file. " << to;
    return false;
  }

  string line;
  while (getline(ifs, line)) {
    ofs << line << "\n";
  }

  return true;
}

bool Util::CopyFile(const string &from, const string &to) {
  Mmap input;
  if (!input.Open(from.c_str(), "r")) {
    LOG(ERROR) << "Can't open input file. " << from;
    return false;
  }

  OutputFileStream ofs(to.c_str(), ios::binary);
  if (!ofs) {
    LOG(ERROR) << "Can't open output file. " << to;
    return false;
  }

  // TOOD(taku): opening file with mmap could not be
  // a best solution. Also, we have to check disk quota
  // in advance.
  ofs.write(input.begin(), input.size());

  return true;
}

// Return true if |filename1| and |filename2| are identical.
bool Util::IsEqualFile(const string &filename1,
                       const string &filename2) {
  Mmap mmap1, mmap2;

  if (!mmap1.Open(filename1.c_str(), "r")) {
    LOG(ERROR) << "Cannot open: " << filename1;
    return false;
  }

  if (!mmap2.Open(filename2.c_str(), "r")) {
    LOG(ERROR) << "Cannot open: " << filename2;
    return false;
  }

  if (mmap1.size() != mmap2.size()) {
    return false;
  }

  return 0 == memcmp(mmap1.begin(), mmap2.begin(), mmap1.size());
}

#endif  // !MOZC_USE_PEPPER_FILE_IO

// TODO(taku):  This value is defined in KnownFolders.h
// If Win SDK for Vista is installed. Check the availability of SDK later.
#ifdef OS_WINDOWS
EXTERN_C const GUID DECLSPEC_SELECTANY FOLDERID_LocalAppDataLow = {
  0xA520A1A4, 0x1780, 0x4FF6, { 0xBD, 0x18, 0x16, 0x73, 0x43, 0xC5, 0xAF, 0x16 }
};
#endif  // OS_WINDOWS

namespace {

class UserProfileDirectoryImpl {
 public:
  UserProfileDirectoryImpl();
  const string &get() { return dir_; }
  void set(const string &dir) { dir_ = dir; }
 private:
  string dir_;
};

#ifdef OS_WINDOWS
// TODO(yukawa): Use API wrapper so that unit test can emulate any case.
class LocalAppDataDirectoryCache {
 public:
  LocalAppDataDirectoryCache() : result_(E_FAIL) {
    result_ = SafeTryGetLocalAppData(&path_);
  }
  HRESULT result() const {
    return result_;
  }
  const bool succeeded() const {
    return SUCCEEDED(result_);
  }
  const string &path() const {
    return path_;
  }

 private:
  // b/5707813 implies that TryGetLocalAppData causes an exception and makes
  // Singleton<LocalAppDataDirectoryCache> invalid state which results in an
  // infinite spin loop in CallOnce. To prevent this, the constructor of
  // LocalAppDataDirectoryCache must be exception free.
  // Note that __try and __except does not guarantees that any destruction
  // of internal C++ objects when a non-C++ exception occurs except that
  // /EHa compiler option is specified.
  // Since Mozc uses /EHs option in common.gypi, we must admit potential
  // memory leakes when any non-C++ exception occues in TryGetLocalAppData.
  // See http://msdn.microsoft.com/en-us/library/1deeycx5.aspx
  static HRESULT __declspec(nothrow) SafeTryGetLocalAppData(string *dir) {
    __try {
      return TryGetLocalAppData(dir);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      return E_UNEXPECTED;
    }
  }

  static HRESULT TryGetLocalAppData(string *dir) {
    if (dir == NULL) {
      return E_FAIL;
    }
    dir->clear();

    if (Util::IsVistaOrLater()) {
      return TryGetLocalAppDataLow(dir);
    }

    // Windows XP: use "%USERPROFILE%\Local Settings\Application Data"

    // Retrieve the directory "%USERPROFILE%\Local Settings\Application Data",
    // which is a user directory which serves a data repository for local
    // applications, to avoid user profiles from being roamed by indexers.
    wchar_t config[MAX_PATH] = {};
    const HRESULT result = ::SHGetFolderPathW(
        NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, &config[0]);
    if (FAILED(result)) {
      return result;
    }

    string buffer;
    if (Util::WideToUTF8(&config[0], &buffer) == 0) {
      return E_FAIL;
    }

    *dir = buffer;
    return S_OK;
  }

  static HRESULT TryGetLocalAppDataLow(string *dir) {
    if (dir == NULL) {
      return E_FAIL;
    }
    dir->clear();

    if (!Util::IsVistaOrLater()) {
      return E_NOTIMPL;
    }

    // Windows Vista: use LocalLow
    // Call SHGetKnownFolderPath dynamically.
    // http://msdn.microsoft.com/en-us/library/bb762188(VS.85).aspx
    // http://msdn.microsoft.com/en-us/library/bb762584(VS.85).aspx
    // GUID: {A520A1A4-1780-4FF6-BD18-167343C5AF16}
    const HMODULE hLib = WinUtil::LoadSystemLibrary(L"shell32.dll");
    if (hLib == NULL) {
      return E_NOTIMPL;
    }

    typedef HRESULT (WINAPI *FPSHGetKnownFolderPath)(
        const GUID &, DWORD, HANDLE, PWSTR *);
    FPSHGetKnownFolderPath func = reinterpret_cast<FPSHGetKnownFolderPath>
        (::GetProcAddress(hLib, "SHGetKnownFolderPath"));
    if (func == NULL) {
      ::FreeLibrary(hLib);
      return E_NOTIMPL;
    }

    wchar_t *task_mem_buffer = NULL;
    const HRESULT result =
        (*func)(FOLDERID_LocalAppDataLow, 0, NULL, &task_mem_buffer);
    if (FAILED(result)) {
      if (task_mem_buffer != NULL) {
        ::CoTaskMemFree(task_mem_buffer);
      }
      ::FreeLibrary(hLib);
      return result;
    }

    if (task_mem_buffer == NULL) {
      ::FreeLibrary(hLib);
      return E_UNEXPECTED;
    }

    wstring wpath = task_mem_buffer;
    ::CoTaskMemFree(task_mem_buffer);

    string path;
    if (Util::WideToUTF8(wpath, &path) == 0) {
      ::FreeLibrary(hLib);
      return E_UNEXPECTED;
    }

    *dir = path;

    ::FreeLibrary(hLib);
    return S_OK;
  }

  HRESULT result_;
  string path_;
};
#endif

UserProfileDirectoryImpl::UserProfileDirectoryImpl() {
#ifdef MOZC_USE_PEPPER_FILE_IO
  // In NaCl, we can't call Util::CreateDirectory() nor Util::DirectoryExists().
  // So we just set dir_ here.
  dir_ = "/";
  return;
#else  // MOZC_USE_PEPPER_FILE_IO
  string dir;

#ifdef OS_WINDOWS
  DCHECK(SUCCEEDED(Singleton<LocalAppDataDirectoryCache>::get()->result()));
  dir = Singleton<LocalAppDataDirectoryCache>::get()->path();
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  dir = Util::JoinPath(dir, kCompanyNameInEnglish);
  Util::CreateDirectory(dir);
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
  dir = Util::JoinPath(dir, kProductNameInEnglish);

#elif defined(OS_MACOSX)
  dir = MacUtil::GetApplicationSupportDirectory();
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  dir = Util::JoinPath(dir, "Google");
  // The permission of ~/Library/Application Support/Google seems to be 0755.
  // TODO(komatsu): nice to make a wrapper function.
  ::mkdir(dir.c_str(), 0755);
  dir = Util::JoinPath(dir, "JapaneseInput");
#else  //  GOOGLE_JAPANESE_INPUT_BUILD
  dir = Util::JoinPath(dir, "Mozc");
#endif  //  GOOGLE_JAPANESE_INPUT_BUILD

#elif defined(OS_ANDROID)
  // For android, we just use pre-defined directory, which is under the package
  // directory, asssuming each device has single user.
  dir = Util::JoinPath("/data/data", kMozcAndroidPackage);
  dir = Util::JoinPath(dir, ".mozc");

#else  // !OS_WINDOWS && !OS_MACOSX && !OS_ANDROID
  char buf[1024];
  struct passwd pw, *ppw;
  const uid_t uid = geteuid();
  CHECK_EQ(0, getpwuid_r(uid, &pw, buf, sizeof(buf), &ppw))
      << "Can't get passwd entry for uid " << uid << ".";
  CHECK_LT(0, strlen(pw.pw_dir))
      << "Home directory for uid " << uid << " is not set.";
  dir = Util::JoinPath(pw.pw_dir, ".mozc");
#endif  // !OS_WINDOWS && !OS_MACOSX && !OS_ANDROID

  Util::CreateDirectory(dir);
  if (!Util::DirectoryExists(dir)) {
    LOG(ERROR) << "Failed to create directory: " << dir;
  }

  // set User profile directory
  dir_ = dir;
#endif  // MOZC_USE_PEPPER_FILE_IO
}

#ifdef OS_WINDOWS
// TODO(yukawa): Use API wrapper so that unit test can emulate any case.
class ProgramFilesX86Cache {
 public:
  ProgramFilesX86Cache() : result_(E_FAIL) {
    result_ = SafeTryProgramFilesPath(&path_);
  }
  const bool succeeded() const {
    return SUCCEEDED(result_);
  }
  const HRESULT result() const {
    return result_;
  }
  const string &path() const {
    return path_;
  }

 private:
  // b/5707813 implies that the Shell API causes an exception in some cases.
  // In order to avoid potential infinite loops in CallOnce. the constructor of
  // ProgramFilesX86Cache must be exception free.
  // Note that __try and __except does not guarantees that any destruction
  // of internal C++ objects when a non-C++ exception occurs except that
  // /EHa compiler option is specified.
  // Since Mozc uses /EHs option in common.gypi, we must admit potential
  // memory leakes when any non-C++ exception occues in TryProgramFilesPath.
  // See http://msdn.microsoft.com/en-us/library/1deeycx5.aspx
  static HRESULT __declspec(nothrow) SafeTryProgramFilesPath(string *path) {
    __try {
      return TryProgramFilesPath(path);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      return E_UNEXPECTED;
    }
  }

  static HRESULT TryProgramFilesPath(string *path) {
    if (path == NULL) {
      return E_FAIL;
    }
    path->clear();

    wchar_t program_files_path_buffer[MAX_PATH] = {};
#if defined(_M_X64)
    // In 64-bit processes (such as Text Input Prosessor DLL for 64-bit apps),
    // CSIDL_PROGRAM_FILES points 64-bit Program Files directory. In this case,
    // we should use CSIDL_PROGRAM_FILESX86 to find server, renderer, and other
    // binaries' path.
    const HRESULT result = ::SHGetFolderPathW(
        NULL, CSIDL_PROGRAM_FILESX86, NULL,
        SHGFP_TYPE_CURRENT, program_files_path_buffer);
#elif defined(_M_IX86)
    // In 32-bit processes (such as server, renderer, and other binaries),
    // CSIDL_PROGRAM_FILES always points 32-bit Program Files directory
    // even if they are running in 64-bit Windows.
    const HRESULT result = ::SHGetFolderPathW(
        NULL, CSIDL_PROGRAM_FILES, NULL,
        SHGFP_TYPE_CURRENT, program_files_path_buffer);
#else
#error "Unsupported CPU architecture"
#endif  // _M_X64, _M_IX86, and others
    if (FAILED(result)) {
      return result;
    }

    string program_files;
    if (Util::WideToUTF8(program_files_path_buffer, &program_files) == 0) {
      return E_FAIL;
    }
    *path = program_files;
    return S_OK;
  }
  HRESULT result_;
  string path_;
};
#endif  // OS_WINDOWS
}  // namespace

string Util::GetUserProfileDirectory() {
  return Singleton<UserProfileDirectoryImpl>::get()->get();
}

void Util::SetUserProfileDirectory(const string &path) {
  Singleton<UserProfileDirectoryImpl>::get()->set(path);
}

string Util::GetLoggingDirectory() {
#ifdef OS_MACOSX
  string dir = MacUtil::GetLoggingDirectory();
  Util::CreateDirectory(dir);
  return dir;
#else
  return GetUserProfileDirectory();
#endif
}

string Util::GetServerDirectory() {
#ifdef OS_WINDOWS
  DCHECK(SUCCEEDED(Singleton<ProgramFilesX86Cache>::get()->result()));
#if defined(GOOGLE_JAPANESE_INPUT_BUILD)
  return Util::JoinPath(
      Util::JoinPath(Singleton<ProgramFilesX86Cache>::get()->path(),
                     kCompanyNameInEnglish),
      kProductNameInEnglish);
#else
  return Util::JoinPath(Singleton<ProgramFilesX86Cache>::get()->path(),
                        kProductNameInEnglish);
#endif
#endif  // OS_WINDOWS

// TODO(mazda): Not to use hardcoded path.
#ifdef OS_MACOSX
  return MacUtil::GetServerDirectory();
#endif  // OS_MACOSX

#ifdef OS_LINUX
  return kMozcServerDirectory;
#endif  // OS_LINUX
}

string Util::GetServerPath() {
  const string server_path = mozc::Util::GetServerDirectory();
  // if server path is empty, return empty path
  if (server_path.empty()) {
    return "";
  }
  return mozc::Util::JoinPath(server_path, kMozcServerName);
}

string Util::GetRendererPath() {
  const string server_path = mozc::Util::GetServerDirectory();
  // if server path is empty, return empty path
  if (server_path.empty()) {
    return "";
  }
  return mozc::Util::JoinPath(server_path, kMozcRenderer);
}

string Util::GetToolPath() {
  const string server_path = mozc::Util::GetServerDirectory();
  // if server path is empty, return empty path
  if (server_path.empty()) {
    return "";
  }
  return mozc::Util::JoinPath(server_path, kMozcTool);
}

string Util::GetDocumentDirectory() {
#ifdef OS_MACOSX
  return Util::GetServerDirectory();
#else
  return Util::JoinPath(Util::GetServerDirectory(), "documents");
#endif
}

string Util::GetUserNameAsString() {
#ifdef MOZC_USE_PEPPER_FILE_IO
  LOG(ERROR) << "Util::GetUserNameAsString() is not implemented in NaCl.";
  return "username";

#elif defined(OS_WINDOWS)  // MOZC_USE_PEPPER_FILE_IO
  wchar_t wusername[UNLEN + 1];
  DWORD name_size = UNLEN + 1;
  // Call the same name Windows API.  (include Advapi32.lib).
  // TODO(komatsu, yukawa): Add error handling.
  // TODO(komatsu, yukawa): Consider the case where the current thread is
  //   or will be impersonated.
  const BOOL result = ::GetUserName(wusername, &name_size);
  DCHECK_NE(FALSE, result);
  string username;
  Util::WideToUTF8(&wusername[0], &username);
  return username;

#elif defined(OS_ANDROID)  // OS_WINDOWS
  // Android doesn't seem to support getpwuid_r.
  struct passwd *ppw = getpwuid(geteuid());
  CHECK(ppw != NULL);
  return ppw->pw_name;

#else  // OS_ANDROID
  // OS_MACOSX or OS_LINUX
  struct passwd pw, *ppw;
  char buf[1024];
  CHECK_EQ(0, getpwuid_r(geteuid(), &pw, buf, sizeof(buf), &ppw));
  return pw.pw_name;
#endif  // !MOZC_USE_PEPPER_FILE_IO && !OS_WINDOWS && !OS_ANDROID
}

#ifdef OS_WINDOWS
namespace {

string GetObjectNameAsString(HANDLE handle) {
  if (handle == NULL) {
    LOG(ERROR) << "Unknown handle";
    return "";
  }

  DWORD size = 0;
  if (::GetUserObjectInformationA(handle, UOI_NAME,
                                  NULL, 0, &size) ||
      ERROR_INSUFFICIENT_BUFFER != ::GetLastError()) {
    LOG(ERROR) << "GetUserObjectInformationA() failed: "
               << ::GetLastError();
    return "";
  }

  if (size == 0) {
    LOG(ERROR) << "buffer size is 0";
    return "";
  }

  scoped_array<char> buf(new char[size]);
  DWORD return_size = 0;
  if (!::GetUserObjectInformationA(handle, UOI_NAME, buf.get(),
                                   size, &return_size)) {
    LOG(ERROR) << "::GetUserObjectInformationA() failed: "
               << ::GetLastError();
    return "";
  }

  if (return_size <= 1) {
    LOG(ERROR) << "result buffer size is too small";
    return "";
  }

  char *result = buf.get();
  result[return_size - 1] = '\0';  // just make sure NULL terminated

  return result;
}

bool GetCurrentSessionId(DWORD *session_id) {
  DCHECK(session_id);
  *session_id = 0;
  if (!::ProcessIdToSessionId(::GetCurrentProcessId(),
                              session_id)) {
    LOG(ERROR) << "cannot get session id: " << ::GetLastError();
    return false;
  }
  return true;
}

}  // namespace
#endif  // OS_WINDOWS

string Util::GetDesktopNameAsString() {
#ifdef OS_LINUX
  const char *display = getenv("DISPLAY");
  if (display == NULL) {
    return "";
  }
  return display;
#endif

#ifdef OS_MACOSX
  return "";
#endif

#ifdef OS_WINDOWS
  DWORD session_id = 0;
  if (!GetCurrentSessionId(&session_id)) {
    return "";
  }

  char id[64];
  snprintf(id, sizeof(id), "%u", session_id);

  string result = id;
  result += ".";
  result += GetObjectNameAsString(::GetProcessWindowStation());
  result += ".";
  result += GetObjectNameAsString(::GetThreadDesktop(::GetCurrentThreadId()));

  return result;
#endif
}

#ifdef OS_WINDOWS
namespace {

class UserSidImpl {
 public:
  UserSidImpl();
  const string &get() { return sid_; }
 private:
  string sid_;
};

UserSidImpl::UserSidImpl() {
  HANDLE htoken = NULL;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &htoken)) {
    sid_ = Util::GetUserNameAsString();
    LOG(ERROR) << "OpenProcessToken failed: " << ::GetLastError();
    return;
  }

  DWORD length = 0;
  ::GetTokenInformation(htoken, TokenUser, NULL, 0, &length);
  scoped_array<char> buf(new char[length]);
  PTOKEN_USER p_user = reinterpret_cast<PTOKEN_USER>(buf.get());

  if (length == 0 ||
      !::GetTokenInformation(htoken, TokenUser, p_user, length, &length)) {
    ::CloseHandle(htoken);
    sid_ = Util::GetUserNameAsString();
    LOG(ERROR) << "OpenTokenInformation failed: " << ::GetLastError();
    return;
  }

  LPTSTR p_sid_user_name;
  if (!::ConvertSidToStringSidW(p_user->User.Sid, &p_sid_user_name)) {
    ::CloseHandle(htoken);
    sid_ = Util::GetUserNameAsString();
    LOG(ERROR) << "ConvertSidToStringSidW failed: " << ::GetLastError();
    return;
  }

  Util::WideToUTF8(p_sid_user_name, &sid_);

  ::LocalFree(p_sid_user_name);
  ::CloseHandle(htoken);
}
}  // namespace
#endif  // OS_WINDOWS

string Util::GetUserSidAsString() {
#ifdef OS_WINDOWS
  return Singleton<UserSidImpl>::get()->get();
#else
  return Util::GetUserNameAsString();
#endif  // OS_WINDOWS
}

// Path
string Util::JoinPath(const string &path1, const string &path2) {
  string output;
  JoinPath(path1, path2, &output);
  return output;
}

void Util::JoinPath(const string &path1, const string &path2,
                    string *output) {
  *output = path1;
  if (path1.size() > 0 && path1[path1.size() - 1] != kFileDelimiter) {
    *output += kFileDelimiter;
  }
  *output += path2;
}

// TODO(taku): what happens if filename == '/foo/bar/../bar/..
string Util::Dirname(const string &filename) {
  const string::size_type p = filename.find_last_of(kFileDelimiter);
  if (p == string::npos) {
    return "";
  }
  return filename.substr(0, p);
}

string Util::Basename(const string &filename) {
  const string::size_type p = filename.find_last_of(kFileDelimiter);
  if (p == string::npos) {
    return filename;
  }
  return filename.substr(p + 1, filename.size() - p);
}

string Util::NormalizeDirectorySeparator(const string &path) {
#ifdef OS_WINDOWS
  string normalized;
  Util::StringReplace(path, string(1, kFileDelimiterForUnix),
                      string(1, kFileDelimiterForWindows), true, &normalized);
  return normalized;
#else
  return path;
#endif
}

// Command line flags
bool Util::CommandLineGetFlag(int argc,
                              char **argv,
                              string *key,
                              string *value,
                              int *used_args) {
  key->clear();
  value->clear();
  *used_args = 0;
  if (argc < 1) {
    return false;
  }

  *used_args = 1;
  const char *start = argv[0];
  if (start[0] != '-') {
    return false;
  }

  ++start;
  if (start[0] == '-') ++start;
  const string arg = start;
  const size_t n = arg.find("=");
  if (n != string::npos) {
    *key = arg.substr(0, n);
    *value = arg.substr(n + 1, arg.size() - n);
    return true;
  }

  key->assign(arg);
  value->clear();

  if (argc == 1) {
    return true;
  }
  start = argv[1];
  if (start[0] == '-') {
    return true;
  }

  *used_args = 2;
  value->assign(start);
  return true;
}

void Util::CommandLineRotateArguments(int argc, char ***argv) {
  char *arg = **argv;
  memmove(*argv, *argv + 1, (argc - 1) * sizeof(**argv));
  (*argv)[argc - 1] = arg;
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
    Util::EncodeURI(it->second, &encoded);
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
  Util::EscapeUrl(input, &escaped_input);
  return escaped_input;
}

void Util::EscapeHtml(const string &plain, string *escaped) {
  string tmp1, tmp2, tmp3, tmp4;
  Util::StringReplace(plain, "&", "&amp;", true, &tmp1);
  Util::StringReplace(tmp1, "<", "&lt;", true, &tmp2);
  Util::StringReplace(tmp2, ">", "&gt;", true, &tmp3);
  Util::StringReplace(tmp3, "\"", "&quot;", true, &tmp4);
  Util::StringReplace(tmp4, "'", "&#39;", true, escaped);
}

void Util::UnescapeHtml(const string &escaped, string *plain) {
  string tmp1, tmp2, tmp3, tmp4;
  Util::StringReplace(escaped, "&amp;", "&", true, &tmp1);
  Util::StringReplace(tmp1, "&lt;", "<", true, &tmp2);
  Util::StringReplace(tmp2, "&gt;", ">", true, &tmp3);
  Util::StringReplace(tmp3, "&quot;", "\"", true, &tmp4);
  Util::StringReplace(tmp4, "&#39;", "'", true, plain);
}

void Util::EscapeCss(const string &plain, string *escaped) {
  // ">" and "&" are not escaped because they are used for operands of
  // CSS.
  Util::StringReplace(plain, "<", "&lt;", true, escaped);
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
      INRANGE(w, 0x3400, 0x4DBF) ||    // CJK Unified Ideographs Extention A
      INRANGE(w, 0x4E00, 0x9FFF) ||    // CJK Unified Ideographs
      INRANGE(w, 0xF900, 0xFAFF) ||    // CJK Compatibility Ideographs
      INRANGE(w, 0x20000, 0x2A6DF) ||  // CJK Unified Ideographs Extention B
      INRANGE(w, 0x2A700, 0x2B73F) ||  // CJK Unified Ideographs Extention C
      INRANGE(w, 0x2B740, 0x2B81F) ||  // CJK Unified Ideographs Extention D
      INRANGE(w, 0x2F800, 0x2FA1F)) {  // CJK Compatibility Ideographs
    // As of Unicode 6.0.2, each block has the following characters assigned.
    // [U+3400, U+4DB5]:   CJK Unified Ideographs Extention A
    // [U+4E00, U+9FCB]:   CJK Unified Ideographs
    // [U+4E00, U+FAD9]:   CJK Compatibility Ideographs
    // [U+20000, U+2A6D6]: CJK Unified Ideographs Extention B
    // [U+2A700, U+2B734]: CJK Unified Ideographs Extention C
    // [U+2B740, U+2B81D]: CJK Unified Ideographs Extention D
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
      INRANGE(w, 0x1F000, 0x1F02F) ||  // Mahjong tiles
      INRANGE(w, 0x1F030, 0x1F09F) ||  // Domino tiles
      INRANGE(w, 0x1F0A0, 0x1F0FF) ||  // Playing cards
      INRANGE(w, 0x1F200, 0x1F2FF) ||  // Enclosed Ideographic Supplement
      INRANGE(w, 0x1F300, 0x1F5FF) ||  // Miscellaneous Symbols And Pictographs
      INRANGE(w, 0x1F600, 0x1F64F) ||  // Emoticons
      INRANGE(w, 0x1F680, 0x1F6FF) ||  // Transport And Map Symbols
      INRANGE(w, 0x1F700, 0x1F77F)) {  // Alchemical Symbols
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
  const char32 w = Util::UTF8ToUCS4(begin, end, mblen);
  return Util::GetScriptType(w);
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
}  // anonymous namespace

Util::ScriptType Util::GetScriptType(const string &str) {
  return GetScriptTypeInternal(str, false);
}

Util::ScriptType Util::GetFirstScriptType(const string &str) {
  size_t mblen = 0;
  return Util::GetScriptType(str.c_str(),
                             str.c_str() + str.size(),
                             &mblen);
}

Util::ScriptType Util::GetScriptTypeWithoutSymbols(const string &str) {
  return GetScriptTypeInternal(str, true);
}

// return true if all script_type in str is "type"
bool Util::IsScriptType(const string &str, Util::ScriptType type) {
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
bool Util::ContainsScriptType(const string &str, ScriptType type) {
  for (ConstChar32Iterator iter(str); !iter.Done(); iter.Next()) {
    if (type == Util::GetScriptType(iter.Get())) {
      return true;
    }
  }
  return false;
}

// return the Form Type of string
Util::FormType Util::GetFormType(const string &str) {
  // TODO(hidehiko): get rid of using FORM_TYPE_SIZE.
  Util::FormType result = Util::FORM_TYPE_SIZE;

  for (ConstChar32Iterator iter(str); !iter.Done(); iter.Next()) {
    const Util::FormType type = GetFormType(iter.Get());
    if (type == UNKNOWN_FORM ||
        (result != Util::FORM_TYPE_SIZE && type != result)) {
      return UNKNOWN_FORM;
    }
    result = type;
  }

  return result;
}

// Util::CharcterSet Util::GetCharacterSet(char32 ucs4);
#include "base/character_set.h"

Util::CharacterSet Util::GetCharacterSet(const string &str) {
  Util::CharacterSet result = Util::ASCII;
  for (ConstChar32Iterator iter(str); !iter.Done(); iter.Next()) {
    result = max(result, GetCharacterSet(iter.Get()));
  }
  return result;
}

bool Util::IsPlatformSupported() {
#if defined(OS_MACOSX)
  // TODO(yukawa): support Mac.
  return true;
#elif defined(OS_LINUX)
  // TODO(yukawa): support Linux.
  return true;
#elif defined(OS_WINDOWS)
  // Sometimes we suffer from version lie of GetVersion(Ex) such as
  // http://b/2430094
  // This is why we use VerifyVersionInfo here instead of GetVersion(Ex).

  // You can find a table of version number for each version of Windows in
  // the following page.
  // http://msdn.microsoft.com/en-us/library/ms724833.aspx
  {
    // Windows 7 <= |OSVERSION|: supported
    OSVERSIONINFOEX osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    osvi.dwMajorVersion = 6;
    osvi.dwMinorVersion = 1;
    DWORDLONG conditional = 0;
    VER_SET_CONDITION(conditional, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditional, VER_MINORVERSION, VER_GREATER_EQUAL);
    const DWORD typemask = VER_MAJORVERSION | VER_MINORVERSION;
    if (::VerifyVersionInfo(&osvi, typemask, conditional) != 0) {
      return true;  // supported
    }
  }
  {
    // Windows Vista SP1 <= |OSVERSION| < Windows 7: supported
    OSVERSIONINFOEX osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    osvi.dwMajorVersion = 6;
    osvi.dwMinorVersion = 0;
    osvi.wServicePackMajor = 1;
    DWORDLONG conditional = 0;
    VER_SET_CONDITION(conditional, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditional, VER_MINORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditional, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
    const DWORD typemask = VER_MAJORVERSION | VER_MINORVERSION |
                           VER_SERVICEPACKMAJOR;
    if (::VerifyVersionInfo(&osvi, typemask, conditional) != 0) {
      return true;  // supported
    }
  }
  {
    // Windows Vista RTM <= |OSVERSION| < Windows Vista SP1: not supported
    OSVERSIONINFOEX osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    osvi.dwMajorVersion = 6;
    osvi.dwMinorVersion = 0;
    osvi.wServicePackMajor = 0;
    DWORDLONG conditional = 0;
    VER_SET_CONDITION(conditional, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditional, VER_MINORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditional, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
    const DWORD typemask = VER_MAJORVERSION | VER_MINORVERSION |
                           VER_SERVICEPACKMAJOR;
    if (::VerifyVersionInfo(&osvi, typemask, conditional) != 0) {
      return false;  // not supported
    }
  }
  {
    // Windows XP x64/Server 2003 <= |OSVERSION| < Windows Vista RTM: supported
    // ---
    // Note: We do not oficially support these platforms but allows users to
    //   install Mozc into them.  See b/5182031 for the background information.
    OSVERSIONINFOEX osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    osvi.dwMajorVersion = 5;
    osvi.dwMinorVersion = 2;
    DWORDLONG conditional = 0;
    VER_SET_CONDITION(conditional, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditional, VER_MINORVERSION, VER_GREATER_EQUAL);
    const DWORD typemask = VER_MAJORVERSION | VER_MINORVERSION;
    if (::VerifyVersionInfo(&osvi, typemask, conditional) != 0) {
      return true;  // supported
    }
  }
  {
    // Windows XP SP2 <= |OSVERSION| < Windows XP x64/Server 2003: supported
    OSVERSIONINFOEX osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    osvi.dwMajorVersion = 5;
    osvi.dwMinorVersion = 1;
    osvi.wServicePackMajor = 2;
    DWORDLONG conditional = 0;
    VER_SET_CONDITION(conditional, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditional, VER_MINORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditional, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
    const DWORD typemask = VER_MAJORVERSION | VER_MINORVERSION |
                           VER_SERVICEPACKMAJOR;
    if (::VerifyVersionInfo(&osvi, typemask, conditional) != 0) {
      return true;  // supported
    }
  }
  // |OSVERSION| < Windows XP SP2: not supported
  return false;  // not support
#else
  COMPILE_ASSERT(false, unsupported_platform);
#endif
}
#ifdef OS_WINDOWS
namespace {
// TODO(yukawa): Use API wrapper so that unit test can emulate any case.
template<DWORD MajorVersion, DWORD MinorVersion>
class IsWindowsVerXOrLaterCache {
 public:
  IsWindowsVerXOrLaterCache()
    : succeeded_(false),
      is_ver_x_or_later_(true) {
    // Examine if this system is greater than or equal to WinNT ver. X
    {
      OSVERSIONINFOEX osvi = {};
      osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
      osvi.dwMajorVersion = MajorVersion;
      osvi.dwMinorVersion = MinorVersion;
      DWORDLONG conditional = 0;
      VER_SET_CONDITION(conditional, VER_MAJORVERSION, VER_GREATER_EQUAL);
      VER_SET_CONDITION(conditional, VER_MINORVERSION, VER_GREATER_EQUAL);
      const BOOL result =
        ::VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION,
                            conditional);
      if (result != FALSE) {
        succeeded_ = true;
        is_ver_x_or_later_ = true;
        return;
      }
    }

    // Examine if this system is less than WinNT ver. X
    {
      OSVERSIONINFOEX osvi = {};
      osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
      osvi.dwMajorVersion = MajorVersion;
      osvi.dwMinorVersion = MinorVersion;
      DWORDLONG conditional = 0;
      VER_SET_CONDITION(conditional, VER_MAJORVERSION, VER_LESS);
      VER_SET_CONDITION(conditional, VER_MINORVERSION, VER_LESS);
      const BOOL result =
        ::VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION,
                            conditional);
      if (result != FALSE) {
        succeeded_ = true;
        is_ver_x_or_later_ = false;
        return;
      }
    }

    // Unexpected situation.
    succeeded_ = false;
    is_ver_x_or_later_ = false;
  }
  const bool succeeded() const {
    return succeeded_;
  }
  const bool is_ver_x_or_later() const {
    return is_ver_x_or_later_;
  }

 private:
  bool succeeded_;
  bool is_ver_x_or_later_;
};

typedef IsWindowsVerXOrLaterCache<6, 0> IsWindowsVistaOrLaterCache;
typedef IsWindowsVerXOrLaterCache<6, 1> IsWindows7OrLaterCache;
typedef IsWindowsVerXOrLaterCache<6, 2> IsWindows8OrLaterCache;

// TODO(yukawa): Use API wrapper so that unit test can emulate any case.
class SystemDirectoryCache {
 public:
  SystemDirectoryCache() : system_dir_(NULL) {
    const UINT copied_len_wo_null_if_success =
        ::GetSystemDirectory(path_buffer_, arraysize(path_buffer_));
    if (copied_len_wo_null_if_success >= arraysize(path_buffer_)) {
      // Function failed.
      return;
    }
    DCHECK_EQ(L'\0', path_buffer_[copied_len_wo_null_if_success]);
    system_dir_ = path_buffer_;
  }
  const bool succeeded() const {
    return system_dir_ != NULL;
  }
  const wchar_t *system_dir() const {
    return system_dir_;
  }
 private:
  wchar_t path_buffer_[MAX_PATH];
  wchar_t *system_dir_;
};

}  // namespace

bool Util::IsVistaOrLater() {
  DCHECK(Singleton<IsWindowsVistaOrLaterCache>::get()->succeeded());
  return Singleton<IsWindowsVistaOrLaterCache>::get()->is_ver_x_or_later();
}

bool Util::IsWindows7OrLater() {
  DCHECK(Singleton<IsWindows7OrLaterCache>::get()->succeeded());
  return Singleton<IsWindows7OrLaterCache>::get()->is_ver_x_or_later();
}

bool Util::IsWindows8OrLater() {
  DCHECK(Singleton<IsWindows8OrLaterCache>::get()->succeeded());
  return Singleton<IsWindows8OrLaterCache>::get()->is_ver_x_or_later();
}

bool Util::IsWindowsX64() {
  switch (g_is_windows_x64_mode) {
    case IS_WINDOWS_X64_EMULATE_32BIT_MACHINE:
      return false;
    case IS_WINDOWS_X64_EMULATE_64BIT_MACHINE:
      return true;
    case IS_WINDOWS_X64_DEFAULT_MODE:
      // handled below.
      break;
    default:
      // Should never reach here.
      DLOG(FATAL) << "Unexpected mode specified.  mode = "
                  << g_is_windows_x64_mode;
      // handled below.
      break;
  }
  SYSTEM_INFO system_info = {};
  // This function never fails.
  ::GetNativeSystemInfo(&system_info);
  return (system_info.wProcessorArchitecture ==
          PROCESSOR_ARCHITECTURE_AMD64);
}

void Util::SetIsWindowsX64ModeForTest(IsWindowsX64Mode mode) {
  g_is_windows_x64_mode = mode;
  switch (g_is_windows_x64_mode) {
    case IS_WINDOWS_X64_EMULATE_32BIT_MACHINE:
    case IS_WINDOWS_X64_EMULATE_64BIT_MACHINE:
    case IS_WINDOWS_X64_DEFAULT_MODE:
      // Known mode. OK.
      break;
    default:
      DLOG(FATAL) << "Unexpected mode specified.  mode = "
                  << g_is_windows_x64_mode;
      break;
  }
}

const wchar_t *Util::GetSystemDir() {
  DCHECK(Singleton<SystemDirectoryCache>::get()->succeeded());
  return Singleton<SystemDirectoryCache>::get()->system_dir();
}

bool Util::GetFileVersion(const wstring &file_fullpath,
                          int *major, int *minor, int *build, int *revision) {
  DCHECK(major);
  DCHECK(minor);
  DCHECK(build);
  DCHECK(revision);
  string path;
  Util::WideToUTF8(file_fullpath.c_str(), &path);

  // Accoding to KB826496, we should check file existence.
  // http://support.microsoft.com/kb/826496
  if (!FileExists(path)) {
    LOG(ERROR) << "file not found";
    return false;
  }

  DWORD handle = 0;
  const DWORD version_size =
      ::GetFileVersionInfoSizeW(file_fullpath.c_str(), &handle);

  if (version_size == 0) {
    LOG(ERROR) << "GetFileVersionInfoSizeW failed."
               << " error = " << ::GetLastError();
    return false;
  }

  scoped_array<BYTE> version_buffer(new BYTE[version_size]);

  if (!::GetFileVersionInfoW(file_fullpath.c_str(), 0,
                             version_size, version_buffer.get())) {
    LOG(ERROR) << "GetFileVersionInfo failed."
               << " error = " << ::GetLastError();
    return false;
  }

  VS_FIXEDFILEINFO *fixed_fileinfo = NULL;
  UINT length = 0;
  if (!::VerQueryValueW(version_buffer.get(), L"\\",
                        reinterpret_cast<LPVOID *>(&fixed_fileinfo),
                        &length)) {
    LOG(ERROR) << "VerQueryValue failed."
               << " error = " << ::GetLastError();
    return false;
  }

  *major = HIWORD(fixed_fileinfo->dwFileVersionMS);
  *minor = LOWORD(fixed_fileinfo->dwFileVersionMS);
  *build = HIWORD(fixed_fileinfo->dwFileVersionLS);
  *revision = LOWORD(fixed_fileinfo->dwFileVersionLS);

  return true;
}

string Util::GetFileVersionString(const wstring &file_fullpath) {
  int major, minor, build, revision;
  if (!GetFileVersion(file_fullpath, &major, &minor, &build, &revision)) {
    return "";
  }

  stringstream buf;
  buf << major << "." << minor << "." << build << "." << revision;

  return buf.str();
}

string Util::GetMSCTFAsmCacheReadyEventName() {
  DWORD session_id = 0;
  if (!GetCurrentSessionId(&session_id)) {
    return "";
  }

  const string &desktop_name =
      GetObjectNameAsString(::GetThreadDesktop(::GetCurrentThreadId()));

  if (desktop_name.empty()) {
    DLOG(ERROR) << "Failed to retrieve desktop name";
    return "";
  }

  // Compose "Local\MSCTF.AsmCacheReady.<desktop name><session #>".
  return ("Local\\MSCTF.AsmCacheReady." + desktop_name +
          StringPrintf("%u", session_id));
}

#endif  // OS_WINDOWS

// TODO(toshiyuki): move this to the initialization module and calculate
// version only when initializing.
string Util::GetOSVersionString() {
#ifdef OS_WINDOWS
  string ret = "Windows";
  OSVERSIONINFOEX osvi = { 0 };
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  if (GetVersionEx(reinterpret_cast<OSVERSIONINFO *>(&osvi))) {
    ret += "." + NumberUtil::SimpleItoa(osvi.dwMajorVersion);
    ret += "." + NumberUtil::SimpleItoa(osvi.dwMinorVersion);
    ret += "." + NumberUtil::SimpleItoa(osvi.wServicePackMajor);
    ret += "." + NumberUtil::SimpleItoa(osvi.wServicePackMinor);
  } else {
    LOG(WARNING) << "GetVersionEx failed";
  }
  return ret;
#elif defined(OS_MACOSX)
  const string ret = "MacOSX " + MacUtil::GetOSVersionString();
  // TODO(toshiyuki): get more specific info
  return ret;
#elif defined(OS_LINUX)
  const string ret = "Linux";
  return ret;
#else
  const string ret = "Unknown";
  return ret;
#endif
}

void Util::DisableIME() {
#ifdef OS_WINDOWS
  // turn off IME:
  // AFAIK disabling TSF under Vista and later OS is almost impossible
  // so that all we have to do is to prevent from calling
  // ITfThreadMgr::Activate and ITfThreadMgrEx::ActivateEx in this thread.
  ::ImmDisableTextFrameService(-1);
  ::ImmDisableIME(-1);
#endif
}

uint64 Util::GetTotalPhysicalMemory() {
#if defined(OS_WINDOWS)
  MEMORYSTATUSEX memory_status = { sizeof(MEMORYSTATUSEX) };
  if (!::GlobalMemoryStatusEx(&memory_status)) {
    return 0;
  }
  return memory_status.ullTotalPhys;
#elif defined(OS_MACOSX)
  int mib[] = { CTL_HW, HW_MEMSIZE };
  uint64 total_memory = 0;
  size_t size = sizeof(total_memory);
  const int error =
      sysctl(mib, arraysize(mib), &total_memory, &size, NULL, 0);
  if (error == -1) {
    const int error = errno;
    LOG(ERROR) << "sysctl with hw.memsize failed. "
               << "errno: " << error;
    return 0;
  }
  return total_memory;
#elif defined(OS_LINUX)
#if defined(_SC_PAGESIZE) && defined(_SC_PHYS_PAGES)
  const long page_size = sysconf(_SC_PAGESIZE);
  const long number_of_phyisical_pages = sysconf(_SC_PHYS_PAGES);
  if (number_of_phyisical_pages < 0) {
    // likely to be overflowed.
    LOG(FATAL) << number_of_phyisical_pages << ", " << page_size;
    return 0;
  }
  return static_cast<uint64>(number_of_phyisical_pages) * page_size;
#else
  return 0;
#endif  // defined(_SC_PAGESIZE) && defined(_SC_PHYS_PAGES)
#else  // !(defined(OS_WINDOWS) || defined(OS_MACOSX) || defined(OS_LINUX))
#error "unknown platform"
#endif
}

bool Util::IsLittleEndian() {
#ifndef OS_WINDOWS
  union {
    unsigned char c[4];
    unsigned int i;
  } u;
  COMPILE_ASSERT(sizeof(u.c) == sizeof(u.i), bad_sizeof_int);
  COMPILE_ASSERT(sizeof(u) == sizeof(u.i), misaligned_union);
  u.i = 0x12345678U;
  return u.c[0] == 0x78U;
#else
  return true;
#endif
}

int Util::MaybeMLock(const void *addr, size_t len) {
  // TODO(yukawa): Integrate mozc_cache service.
#if defined(OS_WINDOWS) || defined(OS_ANDROID) || defined(__native_client__)
  return -1;
#else  // defined(OS_WINDOWS) || defined(OS_ANDROID) ||
       // defined(__native_client__)
  return mlock(addr, len);
#endif  // defined(OS_WINDOWS) || defined(OS_ANDROID) ||
        // defined(__native_client__)
}

int Util::MaybeMUnlock(const void *addr, size_t len) {
#if defined(OS_WINDOWS) || defined(OS_ANDROID) || defined(__native_client__)
  return -1;
#else  // defined(OS_WINDOWS) || defined(OS_ANDROID) ||
       // defined(__native_client__)
  return munlock(addr, len);
#endif  // defined(OS_WINDOWS) || defined(OS_ANDROID) ||
        // defined(__native_client__)
}

#ifdef OS_WINDOWS
// TODO(team): Support other platforms.
bool Util::EnsureVitalImmutableDataIsAvailable() {
  if (!Singleton<IsWindowsVistaOrLaterCache>::get()->succeeded()) {
    return false;
  }
  if (!Singleton<IsWindows7OrLaterCache>::get()->succeeded()) {
    return false;
  }
  if (!Singleton<IsWindows8OrLaterCache>::get()->succeeded()) {
    return false;
  }
  if (!Singleton<SystemDirectoryCache>::get()->succeeded()) {
    return false;
  }
  if (!Singleton<ProgramFilesX86Cache>::get()->succeeded()) {
    return false;
  }
  if (!Singleton<LocalAppDataDirectoryCache>::get()->succeeded()) {
    return false;
  }
  return true;
}
#endif  // OS_WINDOWS

}  // namespace mozc
