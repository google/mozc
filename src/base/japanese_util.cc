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

#include <string>

#include "base/double_array.h"
#include "base/japanese_util_rule.h"
#include "base/strings/unicode.h"
#include "absl/strings/string_view.h"

namespace mozc {

namespace {

using strings::OneCharLen;

int LookupDoubleArray(const japanese_util_rule::DoubleArray *array,
                      const char *key, int len, int *result) {
  int seekto = 0;
  int n = 0;
  int b = array[0].base;
  uint32_t p = 0;
  *result = -1;
  uint32_t num = 0;

  for (int i = 0; i < len; ++i) {
    p = b;
    n = array[p].base;
    if (static_cast<uint32_t>(b) == array[p].check && n < 0) {
      seekto = i;
      *result = -n - 1;
      ++num;
    }
    p = b + static_cast<uint8_t>(key[i]) + 1;
    if (static_cast<uint32_t>(b) == array[p].check) {
      b = array[p].base;
    } else {
      return seekto;
    }
  }
  p = b;
  n = array[p].base;
  if (static_cast<uint32_t>(b) == array[p].check && n < 0) {
    seekto = len;
    *result = -n - 1;
  }

  return seekto;
}

}  // namespace

namespace japanese_util {

void ConvertUsingDoubleArray(const japanese_util_rule::DoubleArray *da,
                             const char *ctable, absl::string_view input,
                             std::string *output) {
  output->clear();
  const char *begin = input.data();
  const char *const end = input.data() + input.size();
  while (begin < end) {
    int result = 0;
    int mblen =
        LookupDoubleArray(da, begin, static_cast<int>(end - begin), &result);
    if (mblen > 0) {
      const char *p = &ctable[result];
      const size_t len = strlen(p);
      output->append(p, len);
      mblen -= static_cast<int32_t>(p[len + 1]);
      begin += mblen;
    } else {
      mblen = OneCharLen(*begin);
      output->append(begin, mblen);
      begin += mblen;
    }
  }
}

void HiraganaToKatakana(absl::string_view input, std::string *output) {
  ConvertUsingDoubleArray(japanese_util_rule::hiragana_to_katakana_da,
                          japanese_util_rule::hiragana_to_katakana_table, input,
                          output);
}

void HiraganaToHalfwidthKatakana(absl::string_view input, std::string *output) {
  // combine two rules
  std::string tmp;
  ConvertUsingDoubleArray(japanese_util_rule::hiragana_to_katakana_da,
                          japanese_util_rule::hiragana_to_katakana_table, input,
                          &tmp);
  ConvertUsingDoubleArray(
      japanese_util_rule::fullwidthkatakana_to_halfwidthkatakana_da,
      japanese_util_rule::fullwidthkatakana_to_halfwidthkatakana_table, tmp,
      output);
}

void HiraganaToRomanji(absl::string_view input, std::string *output) {
  ConvertUsingDoubleArray(japanese_util_rule::hiragana_to_romanji_da,
                          japanese_util_rule::hiragana_to_romanji_table, input,
                          output);
}

void HalfWidthAsciiToFullWidthAscii(absl::string_view input,
                                    std::string *output) {
  ConvertUsingDoubleArray(
      japanese_util_rule::halfwidthascii_to_fullwidthascii_da,
      japanese_util_rule::halfwidthascii_to_fullwidthascii_table, input,
      output);
}

void FullWidthAsciiToHalfWidthAscii(absl::string_view input,
                                    std::string *output) {
  ConvertUsingDoubleArray(
      japanese_util_rule::fullwidthascii_to_halfwidthascii_da,
      japanese_util_rule::fullwidthascii_to_halfwidthascii_table, input,
      output);
}

void HiraganaToFullwidthRomanji(absl::string_view input, std::string *output) {
  std::string tmp;
  ConvertUsingDoubleArray(japanese_util_rule::hiragana_to_romanji_da,
                          japanese_util_rule::hiragana_to_romanji_table, input,
                          &tmp);
  ConvertUsingDoubleArray(
      japanese_util_rule::halfwidthascii_to_fullwidthascii_da,
      japanese_util_rule::halfwidthascii_to_fullwidthascii_table, tmp, output);
}

void RomanjiToHiragana(absl::string_view input, std::string *output) {
  ConvertUsingDoubleArray(japanese_util_rule::romanji_to_hiragana_da,
                          japanese_util_rule::romanji_to_hiragana_table, input,
                          output);
}

void KatakanaToHiragana(absl::string_view input, std::string *output) {
  ConvertUsingDoubleArray(japanese_util_rule::katakana_to_hiragana_da,
                          japanese_util_rule::katakana_to_hiragana_table, input,
                          output);
}

void HalfWidthKatakanaToFullWidthKatakana(absl::string_view input,
                                          std::string *output) {
  ConvertUsingDoubleArray(
      japanese_util_rule::halfwidthkatakana_to_fullwidthkatakana_da,
      japanese_util_rule::halfwidthkatakana_to_fullwidthkatakana_table, input,
      output);
}

void FullWidthKatakanaToHalfWidthKatakana(absl::string_view input,
                                          std::string *output) {
  ConvertUsingDoubleArray(
      japanese_util_rule::fullwidthkatakana_to_halfwidthkatakana_da,
      japanese_util_rule::fullwidthkatakana_to_halfwidthkatakana_table, input,
      output);
}

void FullWidthToHalfWidth(absl::string_view input, std::string *output) {
  std::string tmp;
  FullWidthAsciiToHalfWidthAscii(input, &tmp);
  output->clear();
  FullWidthKatakanaToHalfWidthKatakana(tmp, output);
}

void HalfWidthToFullWidth(absl::string_view input, std::string *output) {
  std::string tmp;
  HalfWidthAsciiToFullWidthAscii(input, &tmp);
  output->clear();
  HalfWidthKatakanaToFullWidthKatakana(tmp, output);
}

// TODO(tabata): Add another function to split voice mark
// of some UNICODE only characters (required to display
// and commit for old clients)
void NormalizeVoicedSoundMark(absl::string_view input, std::string *output) {
  ConvertUsingDoubleArray(japanese_util_rule::normalize_voiced_sound_da,
                          japanese_util_rule::normalize_voiced_sound_table,
                          input, output);
}

}  // namespace japanese_util

}  // namespace mozc
