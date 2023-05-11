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

#include "base/strings/japanese.h"

#include <string>

#include "base/strings/internal/double_array.h"
#include "base/strings/internal/japanese_rules.h"
#include "absl/strings/string_view.h"

namespace mozc::japanese {

using ::mozc::japanese::internal::ConvertUsingDoubleArray;

void HiraganaToKatakana(absl::string_view input, std::string *output) {
  ConvertUsingDoubleArray(internal::hiragana_to_katakana_da,
                          internal::hiragana_to_katakana_table, input, output);
}

void HiraganaToHalfwidthKatakana(absl::string_view input, std::string *output) {
  // combine two rules
  std::string tmp;
  ConvertUsingDoubleArray(internal::hiragana_to_katakana_da,
                          internal::hiragana_to_katakana_table, input, &tmp);
  ConvertUsingDoubleArray(
      internal::fullwidthkatakana_to_halfwidthkatakana_da,
      internal::fullwidthkatakana_to_halfwidthkatakana_table, tmp, output);
}

void HiraganaToRomanji(absl::string_view input, std::string *output) {
  ConvertUsingDoubleArray(internal::hiragana_to_romanji_da,
                          internal::hiragana_to_romanji_table, input, output);
}

void HalfWidthAsciiToFullWidthAscii(absl::string_view input,
                                    std::string *output) {
  ConvertUsingDoubleArray(internal::halfwidthascii_to_fullwidthascii_da,
                          internal::halfwidthascii_to_fullwidthascii_table,
                          input, output);
}

void FullWidthAsciiToHalfWidthAscii(absl::string_view input,
                                    std::string *output) {
  ConvertUsingDoubleArray(internal::fullwidthascii_to_halfwidthascii_da,
                          internal::fullwidthascii_to_halfwidthascii_table,
                          input, output);
}

void HiraganaToFullwidthRomanji(absl::string_view input, std::string *output) {
  std::string tmp;
  ConvertUsingDoubleArray(internal::hiragana_to_romanji_da,
                          internal::hiragana_to_romanji_table, input, &tmp);
  ConvertUsingDoubleArray(internal::halfwidthascii_to_fullwidthascii_da,
                          internal::halfwidthascii_to_fullwidthascii_table, tmp,
                          output);
}

void RomanjiToHiragana(absl::string_view input, std::string *output) {
  ConvertUsingDoubleArray(internal::romanji_to_hiragana_da,
                          internal::romanji_to_hiragana_table, input, output);
}

void KatakanaToHiragana(absl::string_view input, std::string *output) {
  ConvertUsingDoubleArray(internal::katakana_to_hiragana_da,
                          internal::katakana_to_hiragana_table, input, output);
}

void HalfWidthKatakanaToFullWidthKatakana(absl::string_view input,
                                          std::string *output) {
  ConvertUsingDoubleArray(
      internal::halfwidthkatakana_to_fullwidthkatakana_da,
      internal::halfwidthkatakana_to_fullwidthkatakana_table, input, output);
}

void FullWidthKatakanaToHalfWidthKatakana(absl::string_view input,
                                          std::string *output) {
  ConvertUsingDoubleArray(
      internal::fullwidthkatakana_to_halfwidthkatakana_da,
      internal::fullwidthkatakana_to_halfwidthkatakana_table, input, output);
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
  ConvertUsingDoubleArray(internal::normalize_voiced_sound_da,
                          internal::normalize_voiced_sound_table, input,
                          output);
}

}  // namespace mozc::japanese
