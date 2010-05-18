// Copyright 2010, Google Inc.
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
#include <windows.h>
#include <Lmcons.h>
#include <shlobj.h>
#include <time.h>
#include <sddl.h>
// #include <KnownFolders.h>
#else
#include <pwd.h>
#include <string.h>
#include <sys/stat.h>
#ifdef OS_MACOSX
#include <sys/sysctl.h>
#endif  // OS_MACOSX
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#endif  // OS_WINDOWS
#include <algorithm>
#include <cctype>
#include <cerrno>
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
#include "base/mmap.h"
#include "base/mutex.h"
#include "base/port.h"
#include "base/singleton.h"
#include "base/text_converter.h"
#ifdef OS_MACOSX
#include "base/mac_util.h"
#endif

namespace {

const char kFileDelimiterForUnix = '/';
const char kFileDelimiterForWindows = '\\';

#ifdef OS_WINDOWS
const char kFileDelimiter = kFileDelimiterForWindows;
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
        while (++p != end && *p != c);
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
    bool inquote = false;
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
      inquote = true;
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
    uint16 ucs2 = Util::UTF8ToUCS2(begin + pos, begin + str->size(), &mblen);
    // ('A' <= ucs2 && ucs2 <= 'Z') || ('Ａ' <= ucs2 && ucs2 <= 'Ｚ')
    if ((0x0041 <= ucs2 && ucs2 <= 0x005A) ||
        (0xFF21 <= ucs2 && ucs2 <= 0xFF3A)) {
      ucs2 += kOffsetFromUpperToLower;
      UCS2ToUTF8(ucs2, &utf8);
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
    uint16 ucs2 = Util::UTF8ToUCS2(begin + pos, begin + str->size(), &mblen);
    // ('a' <= ucs2 && ucs2 <= 'z') || ('ａ' <= ucs2 && ucs2 <= 'ｚ')
    if ((0x0061 <= ucs2 && ucs2 <= 0x007A) ||
        (0xFF41 <= ucs2 && ucs2 <= 0xFF5A)) {
      ucs2 -= kOffsetFromUpperToLower;
      UCS2ToUTF8(ucs2, &utf8);
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

uint16 Util::UTF8ToUCS2(const char *begin,
                        const char *end,
                        size_t *mblen) {
  const uint32 len = static_cast<uint32>(end - begin);
  if (static_cast<unsigned char>(begin[0]) < 0x80) {
    *mblen = 1;
    return static_cast<unsigned char>(begin[0]);
  } else if (len >= 2 && (begin[0] & 0xe0) == 0xc0) {
    *mblen = 2;
    return ((begin[0] & 0x1f) << 6) |(begin[1] & 0x3f);
  } else if (len >= 3 && (begin[0] & 0xf0) == 0xe0) {
    *mblen = 3;
    return ((begin[0] & 0x0f) << 12) |
        ((begin[1] & 0x3f) << 6) |(begin[2] & 0x3f);
    /* belows are out of UCS2 */
  } else if (len >= 4 && (begin[0] & 0xf8) == 0xf0) {
    *mblen = 4;
    return 0;
  } else if (len >= 5 && (begin[0] & 0xfc) == 0xf8) {
    *mblen = 5;
    return 0;
  } else if (len >= 6 && (begin[0] & 0xfe) == 0xfc) {
    *mblen = 6;
    return 0;
  } else {
    *mblen = 1;
    return 0;
  }
}

namespace {
void UCS2ToUTF8Internal(uint16 c, char *output) {
  if (c < 0x00080) {
    output[0] = static_cast<char>(c & 0xFF);
    output[1] = '\0';
  } else if (c < 0x00800) {
    output[0] = static_cast<char>(0xC0 + ((c >> 6) & 0x1F));
    output[1] = static_cast<char>(0x80 + (c & 0x3F));
    output[2] = '\0';
  } else {
    output[0] = static_cast<char>(0xE0 + ((c >> 12) & 0x0F));
    output[1] = static_cast<char>(0x80 + ((c >> 6) & 0x3F));
    output[2] = static_cast<char>(0x80 + (c & 0x3F));
    output[3] = '\0';
  }

  // UCS4 range
  // output[0] = static_cast<char>(0xF0 + ((c >> 18) & 0x07));
  // output[1] = static_cast<char>(0x80 + ((c >> 12) & 0x3F));
  // output[2] = static_cast<char>(0x80 + ((c >> 6) & 0x3F));
  // output[3] = static_cast<char>(0x80 + (c & 0x3F));
  // output[4] = '\0';
}
}   // namespace

void Util::UCS2ToUTF8(uint16 c, string *output) {
  char buf[4];
  UCS2ToUTF8Internal(c, buf);
  output->clear();
  *output = buf;
}

void Util::UCS2ToUTF8Append(uint16 c, string *output) {
  char buf[4];
  UCS2ToUTF8Internal(c, buf);
  *output += buf;
}

#ifdef OS_WINDOWS
int Util::UTF8ToWide(const char *input, wstring *output) {
  const int output_length = MultiByteToWideChar(CP_UTF8, 0, input, -1, NULL, 0);
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

namespace {
const int kInt32BufferSize = 12;  // -2147483648\0
}
string Util::SimpleItoa(int32 number) {
  char buffer[kInt32BufferSize];
  int length = snprintf(buffer, kInt32BufferSize, "%d", number);
  return string(buffer, length);
}

int Util::SimpleAtoi(const string &str) {
  stringstream ss;
  ss << str;
  int i = 0;
  ss >> i;
  return i;
}

bool Util::SafeStrToUInt32(const string &str, uint32 *value) {
  DCHECK(value);

  const char *s = str.c_str();

  // strtoul does not give any errors on negative numbers, so we have to
  // search the string for '-' manually.
  while (isspace(*s)) {
    ++s;
  }
  if (*s == '-') {
    return false;
  }

  char *endptr;
  errno = 0;  // errno only gets set on errors
  unsigned long ul = strtoul(s, &endptr, 10);
  if (endptr != s) {
    while (isspace(*endptr)) {
      ++endptr;
    }
  }

  *value = static_cast<uint32>(ul);
  return *s != 0 && *endptr == 0 && errno == 0 &&
      static_cast<unsigned long>(*value) == ul;  // no overflow
}

bool Util::SafeStrToUInt64(const string &str, uint64 *value) {
  DCHECK(value);

  const char *s = str.c_str();

  // strtoull does not give any errors on negative numbers, so we have to
  // search the string for '-' manually.
  while (isspace(*s)) {
    ++s;
  }
  if (*s == '-') {
    return false;
  }

  char *endptr;
  errno = 0;  // errno only gets set on errors
#ifdef OS_WINDOWS
  *value = _strtoui64(s, &endptr, 10);
#else
  unsigned long long ull = strtoull(s, &endptr, 10);
#endif
  if (endptr != s) {
    while (isspace(*endptr)) {
      ++endptr;
    }
  }

#ifdef OS_WINDOWS
  return *s != 0 && *endptr == 0 && errno == 0;
#else
  *value = static_cast<uint64>(ull);
  return *s != 0 && *endptr == 0 && errno == 0 &&
      static_cast<unsigned long long>(*value) == ull;  // no overflow
#endif
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
#else
  // Use non blocking interface on Linux.
  // Mac also have /dev/urandom (although it's identical with /dev/random)
  ifstream ifs("/dev/urandom", ios::binary);
  if (!ifs) {
    return false;
  }
  ifs.read(buf, buf_size);
#endif

  return true;
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

void Util::GetTimeOfDay(uint64 *sec, uint32 *usec) {
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

uint64 Util::GetTime() {
#ifdef OS_WINDOWS
  return static_cast<uint64>(_time64(NULL));
#else
  return static_cast<uint64>(time(NULL));
#endif
}

bool Util::GetCurrentTm(tm *current_time) {
  return GetTmWithOffsetSecond(current_time, 0);
}

bool Util::GetTmWithOffsetSecond(tm *time_with_offset, int offset_sec) {
  time_t now;
  time(&now);
  const time_t unixtime_with_offset = now + offset_sec;
#ifdef OS_WINDOWS
  if (_localtime64_s(time_with_offset, &unixtime_with_offset) != 0) {
    return false;
  }
#else
  if (localtime_r(&unixtime_with_offset, time_with_offset) == NULL) {
    return false;
  }
#endif
  return true;
}

void Util::Sleep(uint32 msec) {
#ifdef OS_WINDOWS
  ::Sleep(msec);
#else
  usleep(msec * 1000);
#endif
}

namespace {
// TODO(yukawa): this should be moved into ports.h
const uint64 kUInt64Max = 0xFFFFFFFFFFFFFFFFull;

// There is a informative discuttion about the overflow detection in
// "Hacker's Delight" (http://www.hackersdelight.org/basics.pdf)
//   2-12 'Overflow Detection'

// *output = arg1 + arg2
// return false when an integer overflow happens.
bool AddAndCheckOverflow(uint64 arg1, uint64 arg2, uint64 *output) {
  *output = arg1 + arg2;
  if (arg2 > (kUInt64Max - arg1)) {
    // overflow happens
    return false;
  }
  return true;
}

// *output = arg1 * arg2
// return false when an integer overflow happens.
bool MultiplyAndCheckOverflow(uint64 arg1, uint64 arg2, uint64 *output) {
  *output = arg1 * arg2;
  if (arg1 !=0 && arg2 > (kUInt64Max / arg1)) {
    // overflow happens
    return false;
  }
  return true;
}

// Decode decimal number sequence into one number,
// considering scalind number.
//  [5,4,3] => 543
//  [5,100,4,10,3] => 543
// Return
//  ture: conversion is successful.
bool NormalizeNumbersHelper(const vector<uint64>::const_iterator &begin,
                            const vector<uint64>::const_iterator &end,
                            uint64 *number_output) {
  *number_output = 0;

  if (begin >= end) {
    return true;
  }

  vector<uint64>::const_iterator it = max_element(begin, end);

  // 20, 30 and 40 are treated as a special case
  const bool is_special_number = (*it > 10 && *it < 100);
  const bool is_regular_number = (*it < 10);

  if (is_regular_number) {
    // when no scaling number is found and digit size is >= 2,
    // convert number directly. i.e, [5,4,3] => 543
    uint64 n = 0;
    for (vector<uint64>::const_iterator it = begin;
         it < end; ++it) {
      // n = n * 10 + *it
      if (!MultiplyAndCheckOverflow(n, 10, &n)
          || !AddAndCheckOverflow(n, *it, &n)) {
        return false;
      }
    }
    *number_output = n;
    return true;
  }

  if (is_special_number) {
    *number_output = *it;
    return true;
  }

  if (begin == it) {
    uint64 rest = 0;
    return NormalizeNumbersHelper(it + 1, end, &rest)
        && AddAndCheckOverflow(*it, rest, number_output);
  }

  {
    uint64 scaled_number = 0;
    uint64 rest = 0;
    return NormalizeNumbersHelper(begin, it, &scaled_number)
        && NormalizeNumbersHelper(it + 1, end, &rest)
        && MultiplyAndCheckOverflow(scaled_number, *it, number_output)
        && AddAndCheckOverflow(*number_output, rest, number_output);
  }
}
}  // end of anonymous namespace

// Convert Kanji numbers into Arabic numbers:
// e.g. "百二十万" -> 1200000
bool Util::NormalizeNumbers(const string &input,
                            bool trim_leading_zeros,
                            string *kanji_output,
                            string *arabic_output) {
  DCHECK(kanji_output);
  DCHECK(arabic_output);
  const char *begin = input.data();
  const char *end = input.data() + input.size();
  vector<uint64> numbers;
  numbers.reserve(input.size());

  const char *const kNumKanjiDigits[] = {
    "\xe3\x80\x87", "\xe4\xb8\x80", "\xe4\xba\x8c", "\xe4\xb8\x89",
    "\xe5\x9b\x9b", "\xe4\xba\x94", "\xe5\x85\xad", "\xe4\xb8\x83",
    "\xe5\x85\xab", "\xe4\xb9\x9d", NULL
  };
  //   "〇", "一", "二", "三", "四", "五", "六", "七", "八", "九", NULL

  kanji_output->clear();
  while (begin < end) {
    size_t mblen = 0;
    const uint16 wchar = UTF8ToUCS2(begin, end, &mblen);
    string w(begin, mblen);
    if (wchar >= 0x0030 && wchar <= 0x0039) {
      *kanji_output += kNumKanjiDigits[wchar - 0x0030];
    } else if (wchar >= 0xFF10 && wchar <= 0xFF19) {
      *kanji_output += kNumKanjiDigits[wchar - 0xFF10];
    } else {
      *kanji_output += w;
    }

    string tmp;
    KanjiNumberToArabicNumber(w, &tmp);

    uint64 n = 0;
    for (size_t i = 0; i < tmp.size(); ++i) {
      if (!isdigit(tmp[i])) {   // non-number is included
        return false;
      }
      if (!MultiplyAndCheckOverflow(n, 10, &n)
          || !AddAndCheckOverflow(n, static_cast<uint64>(tmp[i] - '0'),
                                  &n)) {
        return false;
      }
    }
    numbers.push_back(n);
    begin += mblen;
  }

  if (numbers.empty()) {
    return false;
  }

  uint64 n = 0;
  if (!NormalizeNumbersHelper(numbers.begin(), numbers.end(), &n)) {
    return false;
  }

  if (!trim_leading_zeros) {
    // if numbers is [0, 0, 0], we add output "00".
    for (size_t i = 0; i < numbers.size() - 1; ++i) {
      if (numbers[i] == 0) {
        *arabic_output += "0";
      } else {
        break;
      }
    }
  }

  char buf[1024];
  snprintf(buf, sizeof(buf), "%llu", n);
  *arabic_output += buf;
  return true;
}

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

void Util::KanjiNumberToArabicNumber(const string &input,
                                     string *output) {
  TextConverter::Convert(kanjinumber_to_arabicnumber_da,
                         kanjinumber_to_arabicnumber_table,
                         input,
                         output);
}

namespace {
class BracketHandler {
 public:
  BracketHandler() {
    VLOG(1) << "Init bracket mapping";

    static const struct BracketType {
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
      { "\xEF\xBC\x88", "\xEF\xBC\x89" },
      { "\xE3\x80\x94", "\xE3\x80\x95" },
      { "\xEF\xBC\xBB", "\xEF\xBC\xBD" },
      { "\xEF\xBD\x9B", "\xEF\xBD\x9D" },
      { "\xE3\x80\x88", "\xE3\x80\x89" },
      { "\xE3\x80\x8A", "\xE3\x80\x8B" },
      { "\xE3\x80\x8C", "\xE3\x80\x8D" },
      { "\xE3\x80\x8E", "\xE3\x80\x8F" },
      { "\xE3\x80\x90", "\xE3\x80\x91" },
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
  const char *begin = input.data();
  const char *end = begin + input.size();
  while (begin < end) {
    size_t mblen = 0;
    uint16 w = UTF8ToUCS2(begin, end, &mblen);
    switch (w) {
      case 0x3002:  // FULLSTOP
      case 0x300C:  // LEFT CORNER BRACKET
      case 0x300D:  // RIGHT CORNER BRACKET
      case 0x3001:  // COMMA
      case 0x30FB:  // MIDDLE DOT
      case 0x30FC:  // SOUND_MARK
      case 0x3099:  // VOICE SOUND MARK
      case 0x309A:  // SEMI VOICE SOUND MARK
        begin += mblen;
        break;
      default:
        return false;
    }
    begin += mblen;
  }
  return true;
}

bool Util::IsHalfWidthKatakanaSymbol(const string &input) {
  const char *begin = input.data();
  const char *end = begin + input.size();
  while (begin < end) {
    size_t mblen = 0;
    uint16 w = UTF8ToUCS2(begin, end, &mblen);
    switch (w) {
      case 0xFF61:  // FULLSTOP
      case 0xFF62:  // LEFT CORNER BRACKET
      case 0xFF63:  // RIGHT CORNER BRACKET
      case 0xFF64:  // COMMA
      case 0xFF65:  // MIDDLE DOT
      case 0xFF70:  // SOUND_MARK
      case 0xFF9E:  // VOICE SOUND MARK
      case 0xFF9F:  // SEMI VOICE SOUND MARK
        begin += mblen;
        break;
      default:
        return false;
    }
  }
  return true;
}

bool Util::Unlink(const string &filename) {
#ifdef OS_WINDOWS
  wstring wide;
  return Util::UTF8ToWide(filename.c_str(), &wide) > 0 &&
      ::DeleteFileW(wide.c_str()) != 0;
#else
  return ::unlink(filename.c_str()) == 0;
#endif
}

bool Util::RemoveDirectory(const string &dirname) {
#ifdef OS_WINDOWS
  wstring wide;
  return (Util::UTF8ToWide(dirname.c_str(), &wide) > 0 &&
          ::RemoveDirectoryW(wide.c_str()) != 0);
#else
  return ::rmdir(dirname.c_str()) == 0;
#endif
}

bool Util::CreateDirectory(const string &path) {
#ifdef OS_WINDOWS
  wstring wide;
  if (Util::UTF8ToWide(path.c_str(), &wide) <= 0)
    return false;
  ::CreateDirectoryW(wide.c_str(), NULL);
#else
  ::mkdir(path.c_str(), 0700);
#endif
  return true;
}

bool Util::FileExists(const string &filename) {
#ifdef OS_WINDOWS
  wstring wide;
  return (Util::UTF8ToWide(filename.c_str(), &wide) > 0 &&
          ::GetFileAttributesW(wide.c_str()) != -1);
#else
  struct stat s;
  return ::stat(filename.c_str(), &s) == 0;
#endif
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
#else
  struct stat s;
  return (::stat(dirname.c_str(), &s) == 0 && S_ISDIR(s.st_mode));
#endif
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
#else
  return ::rename(from.c_str(), to.c_str()) == 0;
#endif
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

  const HMODULE lib_ktmw = mozc::Util::LoadSystemLibrary(L"ktmw32.dll");
  if (lib_ktmw == NULL) {
    LOG(ERROR) << "LoadSystemLibrary for ktmw32.dll failed.";
    return;
  }

  const HMODULE lib_kernel = Util::GetSystemModuleHandle(L"kernel32.dll");
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
}  // namespace
#endif  // OS_WINDOWS

bool Util::AtomicRename(const string &from, const string &to) {
#ifdef OS_WINDOWS
  CallOnce(&g_init_tx_move_file_once, &InitTxMoveFile);

  wstring fromw, tow;
  Util::UTF8ToWide(from.c_str(), &fromw);
  Util::UTF8ToWide(to.c_str(), &tow);

  bool transaction_failed = false;

  if (g_commit_transaction != NULL &&
      g_move_file_transactedw != NULL &&
      g_create_transaction != NULL) {
    HANDLE handle = (*g_create_transaction)(NULL, 0, 0, 0, 0, 0, NULL);
    if (INVALID_HANDLE_VALUE == handle) {
      LOG(ERROR) << "CreateTransaction failed: " << ::GetLastError();
      transaction_failed = true;
    }

    if (!transaction_failed &&
        !(*g_move_file_transactedw)(fromw.c_str(), tow.c_str(),
                                    NULL, NULL,
                                    MOVEFILE_WRITE_THROUGH |
                                    MOVEFILE_REPLACE_EXISTING,
                                    handle)) {
      LOG(ERROR) << "MoveFileTransactedW failed: " << ::GetLastError();
      transaction_failed = true;
    }

    if (!transaction_failed &&
        !(*g_commit_transaction)(handle)) {
      LOG(ERROR) << "CommitTransaction failed: " << ::GetLastError();
      transaction_failed = true;
    }

    LOG_IF(ERROR, transaction_failed)
        << "Transactional MoveFile failed. Execute fallback plan";

    ::CloseHandle(handle);
  } else {
    transaction_failed = true;
  }

  if (transaction_failed &&
      !::MoveFileEx(fromw.c_str(), tow.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    LOG(ERROR) << "MoveFileEx failed: " << ::GetLastError();
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

// TODO(taku):  This value is defined in KnownFolders.h
// If Win SDK for Vista is installed. Check the availability of SDK later.
#ifdef OS_WINDOWS
EXTERN_C const GUID DECLSPEC_SELECTANY FOLDERID_LocalAppDataLow = {
  0xA520A1A4, 0x1780, 0x4FF6, { 0xBD, 0x18, 0x16, 0x73, 0x43, 0xC5, 0xAF, 0x16 }
};

#endif

namespace {

class UserProfileDirectoryImpl {
 public:
  UserProfileDirectoryImpl();
  const string &get() { return dir_; }
  void set(const string &dir) { dir_ = dir; }
 private:
  string dir_;
};

UserProfileDirectoryImpl::UserProfileDirectoryImpl() {
  string dir;
#ifdef OS_WINDOWS
  // Windows Vista: use LocalLow
  // Call SHGetKnownFolderPath dynamically.
  // http://msdn.microsoft.com/en-us/library/bb762188(VS.85).aspx
  // http://msdn.microsoft.com/en-us/library/bb762584(VS.85).aspx
  // GUID: {A520A1A4-1780-4FF6-BD18-167343C5AF16}
  if (Util::IsVistaOrLater()) {
    const HMODULE hLib = mozc::Util::LoadSystemLibrary(L"shell32.dll");
    if (hLib != NULL) {
      typedef HRESULT (WINAPI *FPSHGetKnownFolderPath)(
          const GUID &, DWORD, HANDLE, PWSTR *);
      FPSHGetKnownFolderPath func = reinterpret_cast<FPSHGetKnownFolderPath>
          (::GetProcAddress(hLib, "SHGetKnownFolderPath"));
      if (func != NULL) {
        PWSTR pstr = NULL;
        const HRESULT result =
            (*func)(FOLDERID_LocalAppDataLow, 0, NULL, &pstr);
        if (SUCCEEDED(result) && pstr != NULL && ::lstrlen(pstr) > 0) {
          Util::WideToUTF8(pstr, &dir);
          ::CoTaskMemFree(pstr);
        }
      }
      ::FreeLibrary(hLib);
    }
  }

  // Windows XP: use "%USERPROFILE%\Local Settings\Application Data"
  if (dir.empty() || !Util::IsVistaOrLater()) {
    // Retrieve the directory "%USERPROFILE%\Local Settings\Application Data",
    // which is a user directory which serves a data repository for local
    // applications, to avoid user profiles from being roamed by indexers.
    wchar_t config[MAX_PATH];
    if (::SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT,
                           &config[0]) == S_OK) {
      Util::WideToUTF8(&config[0], &dir);
    }
  }

  CHECK(!dir.empty());

  dir = Util::JoinPath(dir, kCompanyNameInEnglish);
  Util::CreateDirectory(dir);
  dir = Util::JoinPath(dir, kProductNameInEnglish);

#elif defined(OS_MACOSX)
  dir = MacUtil::GetApplicationSupportDirectory();
  dir = Util::JoinPath(dir, "Google");
  // The permission of ~/Library/Application Support/Google seems to be 0755.
  // TODO(komatsu): nice to make a wrapper function.
  ::mkdir(dir.c_str(), 0755);
  dir = Util::JoinPath(dir, "JapaneseInput");
#else  // OS_LINUX
  char buf[1024];
  struct passwd pw, *ppw;
  const uid_t uid = geteuid();
  CHECK_EQ(0, getpwuid_r(uid, &pw, buf, sizeof(buf), &ppw))
      << "Can't get passwd entry for uid " << uid << ".";
  CHECK_LT(0, strlen(pw.pw_dir))
      << "Home directory for uid " << uid << " is not set.";
  dir =  Util::JoinPath(pw.pw_dir, ".mozc");
#endif

  Util::CreateDirectory(dir);
  if (!Util::DirectoryExists(dir)) {
    LOG(ERROR) << "Failed to create directory: " << dir;
  }

  // set User profile directory
  dir_ = dir;
}

#ifdef OS_WINDOWS
class ServerDirectoryCache {
 public:
  ServerDirectoryCache() {
    wchar_t program_files_path_buffer[MAX_PATH];
#if defined(_M_X64)
    // For x86-64 binaries (such as Text Input Prosessor DLL for 64-bit apps),
    // CSIDL_PROGRAM_FILES points 64-bit Program Files directory so that
    // CSIDL_PROGRAM_FILESX86 is appropriate to find server, renderer,
    // and other binaries' path.
    const HRESULT hr = ::SHGetFolderPathW(
        NULL, CSIDL_PROGRAM_FILESX86, NULL,
        SHGFP_TYPE_CURRENT, program_files_path_buffer);
#elif defined(_M_IX86)
    // For x86 binaries (such as server, renderer, and other binaries),
    // CSIDL_PROGRAM_FILES always points 32-bit Program Files directory
    // even if they are running in 64-bit Windows.
    const HRESULT hr = ::SHGetFolderPathW(
        NULL, CSIDL_PROGRAM_FILES, NULL,
                           SHGFP_TYPE_CURRENT, program_files_path_buffer);
#else
#error "Unsupported CPU architecture"
#endif  // _M_X64, _M_IX86, and others
    CHECK(S_OK == hr) << "Failed to get server directory. HRESULT = "
                      << hr;
    string program_files_path;
    Util::WideToUTF8(program_files_path_buffer, &program_files_path);
    server_path_ = program_files_path;
    server_path_ = Util::JoinPath(server_path_, kCompanyNameInEnglish);
    server_path_ = Util::JoinPath(server_path_, kProductNameInEnglish);
  }
  const string server_path() const {
    return server_path_;
  }
 private:
  string server_path_;
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
  return Singleton<ServerDirectoryCache>::get()->server_path();
#endif  // OS_WINDOWS

// TODO(mazda): Not to use hardcoded path.
#ifdef OS_MACOSX
  return MacUtil::GetServerDirectory();
#endif  // OS_MACOSX

#ifdef OS_LINUX
  return "/usr/lib/mozc";
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

string Util::GetUserNameAsString() {
  string username;
#ifdef OS_WINDOWS
  wchar_t wusername[UNLEN + 1];
  DWORD name_size = UNLEN + 1;
  // Call the same name Windows API.  (include Advapi32.lib).
  ::GetUserName(wusername, &name_size);
  Util::WideToUTF8(&wusername[0], &username);
#else  // OS_WINDOWS
  char buf[1024];
  struct passwd pw, *ppw;
  CHECK_EQ(0, getpwuid_r(geteuid(), &pw, buf, sizeof(buf), &ppw));
  username.append(pw.pw_name);
#endif  // OS_WINDOWS
  return username;
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
}
#endif

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
  if (!::ProcessIdToSessionId(::GetCurrentProcessId(),
                              &session_id)) {
    LOG(ERROR) << "cannot get session id: " << ::GetLastError();
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
    const int hi = ((static_cast<int>(input[i]) & 0xF0) >> 4);
    const int lo = (static_cast<int>(input[i]) & 0x0F);
    *output += "\\x";
    *output += static_cast<char>(hi >= 10 ? hi - 10 + 'A' : hi + '0');
    *output += static_cast<char>(lo >= 10 ? lo - 10 + 'A' : lo + '0');
  }
}

void Util::EscapeHtml(const string &plain, string *escaped) {
  string tmp1, tmp2, tmp3, tmp4;
  Util::StringReplace(plain, "&", "&amp;", true, &tmp1);
  Util::StringReplace(tmp1, "<", "&lt;", true, &tmp2);
  Util::StringReplace(tmp2, ">", "&gt;", true, &tmp3);
  Util::StringReplace(tmp3, "\"", "&quot;", true, &tmp4);
  Util::StringReplace(tmp4, "'", "&#39;", true, escaped);
}

void Util::EscapeCss(const string &plain, string *escaped) {
  // ">" and "&" are not escaped because they are used for operands of
  // CSS.
  Util::StringReplace(plain, "<", "&lt;", true, escaped);
}

#define INRANGE(w, a, b) ((w) >= (a) && (w) <= (b))

// script type
Util::ScriptType Util::GetScriptType(uint16 w) {
  if (INRANGE(w, 0x0030, 0x0039) ||   // ascii number
      INRANGE(w, 0xFF10, 0xFF19)) {   // full width number
    return NUMBER;
  } else if (INRANGE(w, 0x0041, 0x005A) ||  // ascii upper
             INRANGE(w, 0x0061, 0x007A) ||  // ascii lower
             INRANGE(w, 0xFF21, 0xFF3A) ||  // fullwidth ascii upper
             INRANGE(w, 0xFF41, 0xFF5A)) {  // fullwidth ascii lower
    return ALPHABET;
  } else if (INRANGE(w, 0x4E00, 0x9FA5) ||  // CJK Unified Ideographs
             INRANGE(w, 0x3400, 0x4DBF) ||  // CJK Unified Ideographs ExtentionA
             INRANGE(w, 0xF900, 0xFA2D) ||  // CJK Compatibility Ideographs
             w == 0x305) {  // 々 (repetition)
    return KANJI;
  } else if (INRANGE(w, 0x3041, 0x309F)) {  // hiragana
    return HIRAGANA;
  } else if (INRANGE(w, 0x30A1, 0x30FE) ||  // full width katakana
             INRANGE(w, 0xFF65, 0xFF9F)) {  // half width katakana
    return KATAKANA;
  }

  return UNKNOWN_SCRIPT;
}

Util::FormType Util::GetFormType(uint16 w) {
  if (INRANGE(w, 0x0020, 0x007F) ||  // ascii
      INRANGE(w, 0xFF61, 0xFF9F)) {  // half-width katakana
    return HALF_WIDTH;
  } else {
    return FULL_WIDTH;
  }

  return UNKNOWN_FORM;
}

#undef INRANGE

// return script type of first character in str
Util::ScriptType Util::GetScriptType(const char *begin,
                                     const char *end, size_t *mblen) {
  const uint16 w = Util::UTF8ToUCS2(begin, end, mblen);
  return Util::GetScriptType(w);
}

Util::ScriptType Util::GetScriptType(const string &str) {
  const char *begin = str.data();
  const char *end = str.data() + str.size();
  size_t mblen = 0;
  ScriptType result = SCRIPT_TYPE_SIZE;

  while (begin < end) {
    const uint16 w = Util::UTF8ToUCS2(begin, end, &mblen);
    ScriptType type = Util::GetScriptType(w);
    if ((w == 0x30FC || w == 0x30FB ||
         (w >= 0x3099 && w <= 0x309C)) &&
        // PROLONGEDSOUND MARK|MIDLE_DOT|VOICDE_SOUND_MARKS
        // are HIRAGANA as well
        (result == SCRIPT_TYPE_SIZE ||
         result == HIRAGANA || result == KATAKANA)) {
      type = result;  // restore the previous state
    }
    if (type == UNKNOWN_SCRIPT) {
      return UNKNOWN_SCRIPT;
    }
    // not first character
    if (str.data() != begin && result != SCRIPT_TYPE_SIZE && type != result) {
      return UNKNOWN_SCRIPT;
    }
    result = type;
    begin += mblen;
  }

  if (result == SCRIPT_TYPE_SIZE) {  // everything is "ー"
    return UNKNOWN_SCRIPT;
  }

  return result;
}

// return true if all script_type in str is "type"
bool Util::IsScriptType(const string &str, Util::ScriptType type) {
  const char *begin = str.data();
  const char *end = str.data() + str.size();
  size_t mblen = 0;
  while (begin < end) {
    const uint16 w = Util::UTF8ToUCS2(begin, end, &mblen);
    // Exception: 30FC (PROLONGEDSOUND MARK is categorized as HIRAGANA as well)
    if ((w == 0x30FC && type == HIRAGANA) || type == GetScriptType(w)) {
      begin += mblen;
    } else {
      return false;
    }
  }
  return true;
}

// return true if the string contains script_type char
bool Util::ContainsScriptType(const string &str, ScriptType type) {
  const char *begin = str.data();
  const char *end = str.data() + str.size();
  while (begin < end) {
    size_t mblen;
    const uint16 w = Util::UTF8ToUCS2(begin, end, &mblen);
    if (type == Util::GetScriptType(w)) {
      return true;
    }
    begin += mblen;
  }
  return false;
}

// return the Form Type of string
Util::FormType Util::GetFormType(const string &str) {
  const char *begin = str.data();
  const char *end = str.data() + str.size();
  size_t mblen = 0;
  Util::FormType result = Util::UNKNOWN_FORM;

  while (begin < end) {
    const uint16 w = Util::UTF8ToUCS2(begin, end, &mblen);
    const Util::FormType type = GetFormType(w);
    if (type == UNKNOWN_FORM || (str.data() != begin && type != result)) {
      return UNKNOWN_FORM;
    }
    result = type;
    begin += mblen;
  }

  return result;
}

// Util::CharcterSet Util::GetCharacterSet(uint16 ucs2);
#include "base/character_set.h"

Util::CharacterSet Util::GetCharacterSet(const string &str) {
  const char *begin = str.data();
  const char *end = str.data() + str.size();
  size_t mblen = 0;
  Util::CharacterSet result = Util::ASCII;

  while (begin < end) {
    const uint16 w = Util::UTF8ToUCS2(begin, end, &mblen);
    result = max(result, GetCharacterSet(w));
    begin += mblen;
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

  if (IsWindowsX64() && !Util::IsVistaOrLater()) {
    // NT 5.x OSes are excluded from Mozc 64-bit support.
    return false;  // not supported
  }
  // You can find a table of version number for each version of Windows in
  // the following page.
  // http://msdn.microsoft.com/en-us/library/ms724833.aspx
  {
    // Windows 7 <= |OSVERSION|: supported
    OSVERSIONINFOEX osvi = { 0 };
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
    OSVERSIONINFOEX osvi = { 0 };
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
    // Windows Server 2003 <= |OSVERSION| < Windows Vista SP1: not supported
    OSVERSIONINFOEX osvi = { 0 };
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    osvi.dwMajorVersion = 5;
    osvi.dwMinorVersion = 2;
    DWORDLONG conditional = 0;
    VER_SET_CONDITION(conditional, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditional, VER_MINORVERSION, VER_GREATER_EQUAL);
    const DWORD typemask = VER_MAJORVERSION | VER_MINORVERSION;
    if (::VerifyVersionInfo(&osvi, typemask, conditional) != 0) {
      return false;  // not supported
    }
  }
  {
    // Windows XP SP2 <= |OSVERSION| < Windows Server 2003: supported
    OSVERSIONINFOEX osvi = { 0 };
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
bool Util::IsVistaOrLater() {
  OSVERSIONINFOEX osvi = { 0 };
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  osvi.dwMajorVersion = 6;
  DWORDLONG conditional = 0;
  VER_SET_CONDITION(conditional, VER_MAJORVERSION, VER_GREATER_EQUAL);
  return 0 != VerifyVersionInfo(&osvi, VER_MAJORVERSION, conditional);
}

class IsWindowsX64Cache {
 public:
  IsWindowsX64Cache() : is_x64_(IsX64()) {}
  bool is_x64() const {
    return is_x64_;
  }

 private:
  static bool IsX64() {
    typedef void (WINAPI *GetNativeSystemInfoProc)(
        __out  LPSYSTEM_INFO lpSystemInfo);

    const HMODULE kernel32 = Util::GetSystemModuleHandle(L"kernel32.dll");
    DCHECK_NE(kernel32, NULL);

    GetNativeSystemInfoProc proc = reinterpret_cast<GetNativeSystemInfoProc>(
        ::GetProcAddress(kernel32, "GetNativeSystemInfo"));

    if (proc == NULL) {
      DLOG(INFO) << "GetNativeSystemInfo not found";
      // If GetNativeSystemInfo API is not exported,
      // the system is never X64 edition.
      return false;
    }

    SYSTEM_INFO system_info;
    ::ZeroMemory(&system_info, sizeof(system_info));
    proc(&system_info);

    return system_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64;
  }

  bool is_x64_;
  DISALLOW_COPY_AND_ASSIGN(IsWindowsX64Cache);
};

bool Util::IsWindowsX64() {
  return Singleton<IsWindowsX64Cache>::get()->is_x64();
}

namespace {
class SystemDirectoryCache {
 public:
  SystemDirectoryCache() : system_dir_(NULL) {
    const HRESULT hr = ::SHGetFolderPathW(NULL, CSIDL_SYSTEM, NULL,
                                          SHGFP_TYPE_CURRENT, path_buffer_);
    DCHECK(hr == S_OK);
    if (hr != S_OK) {
       LOG(ERROR) << "Failed to get system directory. HRESULT = " << hr;
      return;
    }
    system_dir_ = path_buffer_;
  }
  const wchar_t *system_dir() const {
    return system_dir_;
  }
 private:
  wchar_t path_buffer_[MAX_PATH];
  wchar_t *system_dir_;
};
}  // namespace

const wchar_t *Util::GetSystemDir() {
  return Singleton<SystemDirectoryCache>::get()->system_dir();
}

HMODULE Util::LoadSystemLibrary(const wstring &base_filename) {
  wstring fullpath = Util::GetSystemDir();
  fullpath += L"\\";
  fullpath += base_filename;

  const HMODULE module = ::LoadLibraryExW(fullpath.c_str(),
                                          NULL,
                                          LOAD_WITH_ALTERED_SEARCH_PATH);
  if (NULL == module) {
    const int last_error = ::GetLastError();
    DLOG(WARNING) << "LoadLibraryEx failed."
                  << " fullpath = " << fullpath.c_str()
                  << " error = " << last_error;
  }
  return module;
}

HMODULE Util::LoadMozcLibrary(const wstring &base_filename) {
  wstring fullpath;
  Util::UTF8ToWide(Util::GetServerDirectory().c_str(), &fullpath);
  fullpath += L"\\";
  fullpath += base_filename;

  const HMODULE module = ::LoadLibraryExW(fullpath.c_str(),
                                          NULL,
                                          LOAD_WITH_ALTERED_SEARCH_PATH);
  if (NULL == module) {
    const int last_error = ::GetLastError();
    DLOG(WARNING) << "LoadLibraryEx failed."
                  << " fullpath = " << fullpath.c_str()
                  << " error = " << last_error;
  }
  return module;
}

HMODULE Util::GetSystemModuleHandle(const wstring &base_filename) {
  wstring fullpath = Util::GetSystemDir();
  fullpath += L"\\";
  fullpath += base_filename;

  const HMODULE module = ::GetModuleHandleW(fullpath.c_str());
  if (NULL == module) {
    const int last_error = ::GetLastError();
    DLOG(WARNING) << "GetModuleHandle failed."
                  << " fullpath = " << fullpath.c_str()
                  << " error = " << last_error;
  }
  return module;
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
#endif  // OS_WINDOWS

// TODO(toshiyuki): move this to the initialization module and calculate
// version only when initializing.
string Util::GetOSVersionString() {
#ifdef OS_WINDOWS
  string ret = "Windows";
  OSVERSIONINFOEX osvi = { 0 };
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  if (GetVersionEx(reinterpret_cast<OSVERSIONINFO *>(&osvi))) {
    ret += "." + SimpleItoa(osvi.dwMajorVersion);
    ret += "." + SimpleItoa(osvi.dwMinorVersion);
    ret += "." + SimpleItoa(osvi.wServicePackMajor);
    ret += "." + SimpleItoa(osvi.wServicePackMinor);
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

void Util::PreloadMappedRegion(const void *begin,
                               size_t region_size_in_byte,
                               volatile bool *query_quit) {
#if defined(OS_WINDOWS)
  SYSTEM_INFO system_info;
  ::ZeroMemory(&system_info, sizeof(system_info));
  ::GetSystemInfo(&system_info);
  const size_t page_size = system_info.dwPageSize;
#elif defined(OS_MACOSX) || defined(OS_LINUX)
#if defined(_SC_PAGESIZE)
  const size_t page_size = sysconf(_SC_PAGESIZE);
#else
  const size_t page_size = 4096;
#endif
#else  // !(defined(OS_WINDOWS) || defined(OS_MACOSX) || defined(OS_LINUX))
#error "unknown platform"
#endif
  const char *begin_ptr = reinterpret_cast<const char *>(begin);
  const char *end_ptr = begin_ptr + region_size_in_byte;
  // make sure the read operation will not be removed by the optimization
  static volatile char dummy = 0;
  for (const char *p = begin_ptr; p < end_ptr; p += page_size) {
    if (query_quit != NULL && (*query_quit)) {
      break;
    }
    dummy += *p;
  }
}

void Util::MakeByteArrayFile(const string &name,
                          const string &input,
                          const string &output) {
  OutputFileStream ofs(output.c_str());
  CHECK(ofs);
  Util::MakeByteArrayStream(name, input, &ofs);
}

void Util::MakeByteArrayStream(const string &name,
                               const string &input,
                               ostream *os) {
  Mmap<char> mmap;
  CHECK(mmap.Open(input.c_str()));
  Util::WriteByteArray(name, mmap.begin(), mmap.GetFileSize(), os);
}

void Util::WriteByteArray(const string &name, const char *image,
                          size_t image_size, ostream *ofs) {
  const char *begin = image;
  const char *end = image + image_size;
  *ofs << "const size_t k" << name << "_size = "
       << image_size << ";" << endl;

#ifdef OS_WINDOWS
  *ofs << "const uint64 k" << name << "_data_uint64[] = {" << endl;
  ofs->setf(ios::hex, ios::basefield);  // in hex
  ofs->setf(ios::showbase);             // add 0x
  int num = 0;
  while (begin < end) {
    uint64 n = 0;
    uint8 *buf = reinterpret_cast<uint8 *>(&n);
    const size_t size = min(static_cast<size_t>(end - begin),
                            static_cast<size_t>(8));
    for (size_t i = 0; i < size; ++i) {
      buf[i] = static_cast<uint8>(begin[i]);
    }
    begin += 8;
    *ofs << n << ", ";
    if (++num % 8 == 0) {
      *ofs << endl;
    }
  }
  *ofs << "};" << endl;
  *ofs << "const char *k" << name << "_data"
       << " = reinterpret_cast<const char *>(k"
       << name << "_data_uint64);" << endl;
#else
  *ofs << "const char k" << name << "_data[] =" << endl;
  static const size_t kBucketSize = 20;
  while (begin < end) {
     const size_t size = min(static_cast<size_t>(end - begin), kBucketSize);
     string buf;
     Util::Escape(string(begin, size), &buf);
     *ofs << "\"" << buf << "\"";
     *ofs << endl;
     begin += kBucketSize;
   }
   *ofs << ";" << endl;
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

}  // namespace mozc
