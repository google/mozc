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
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "base/strings/internal/double_array.h"
#include "base/strings/internal/japanese_rules.h"

namespace mozc::japanese {

using ::mozc::japanese::internal::ConvertUsingDoubleArray;

void HiraganaToKatakana(absl::string_view input, std::string *output) {
  *output = HiraganaToKatakana(input);
}

std::string HiraganaToKatakana(const absl::string_view input) {
  return ConvertUsingDoubleArray(internal::hiragana_to_katakana_da,
                                 internal::hiragana_to_katakana_table, input);
}

void HiraganaToHalfwidthKatakana(absl::string_view input, std::string *output) {
  *output = HiraganaToHalfwidthKatakana(input);
}

std::string HiraganaToHalfwidthKatakana(const absl::string_view input) {
  // combine two rules
  return FullWidthKatakanaToHalfWidthKatakana(HiraganaToKatakana(input));
}

void HiraganaToRomanji(absl::string_view input, std::string *output) {
  *output = HiraganaToRomanji(input);
}

std::string HiraganaToRomanji(const absl::string_view input) {
  return ConvertUsingDoubleArray(internal::hiragana_to_romanji_da,
                                 internal::hiragana_to_romanji_table, input);
}

void HalfWidthAsciiToFullWidthAscii(absl::string_view input,
                                    std::string *output) {
  *output = HalfWidthAsciiToFullWidthAscii(input);
}

std::string HalfWidthAsciiToFullWidthAscii(const absl::string_view input) {
  return ConvertUsingDoubleArray(
      internal::halfwidthascii_to_fullwidthascii_da,
      internal::halfwidthascii_to_fullwidthascii_table, input);
}

void FullWidthAsciiToHalfWidthAscii(absl::string_view input,
                                    std::string *output) {
  *output = FullWidthAsciiToHalfWidthAscii(input);
}

std::string FullWidthAsciiToHalfWidthAscii(const absl::string_view input) {
  return ConvertUsingDoubleArray(
      internal::fullwidthascii_to_halfwidthascii_da,
      internal::fullwidthascii_to_halfwidthascii_table, input);
}

void HiraganaToFullwidthRomanji(absl::string_view input, std::string *output) {
  *output = HiraganaToFullwidthRomanji(input);
}

std::string HiraganaToFullwidthRomanji(const absl::string_view input) {
  return HalfWidthAsciiToFullWidthAscii(HiraganaToRomanji(input));
}

void RomanjiToHiragana(absl::string_view input, std::string *output) {
  *output = RomanjiToHiragana(input);
}

std::string RomanjiToHiragana(const absl::string_view input) {
  return ConvertUsingDoubleArray(internal::romanji_to_hiragana_da,
                                 internal::romanji_to_hiragana_table, input);
}

void KatakanaToHiragana(absl::string_view input, std::string *output) {
  *output = KatakanaToHiragana(input);
}

std::string KatakanaToHiragana(absl::string_view input) {
  return ConvertUsingDoubleArray(internal::katakana_to_hiragana_da,
                                 internal::katakana_to_hiragana_table, input);
}

void HalfWidthKatakanaToFullWidthKatakana(absl::string_view input,
                                          std::string *output) {
  *output = HalfWidthKatakanaToFullWidthKatakana(input);
}

std::string HalfWidthKatakanaToFullWidthKatakana(absl::string_view input) {
  return ConvertUsingDoubleArray(
      internal::halfwidthkatakana_to_fullwidthkatakana_da,
      internal::halfwidthkatakana_to_fullwidthkatakana_table, input);
}

void FullWidthKatakanaToHalfWidthKatakana(absl::string_view input,
                                          std::string *output) {
  *output = FullWidthKatakanaToHalfWidthKatakana(input);
}

std::string FullWidthKatakanaToHalfWidthKatakana(absl::string_view input) {
  return ConvertUsingDoubleArray(
      internal::fullwidthkatakana_to_halfwidthkatakana_da,
      internal::fullwidthkatakana_to_halfwidthkatakana_table, input);
}

void FullWidthToHalfWidth(absl::string_view input, std::string *output) {
  *output = FullWidthToHalfWidth(input);
}

std::string FullWidthToHalfWidth(const absl::string_view input) {
  return FullWidthKatakanaToHalfWidthKatakana(
      FullWidthAsciiToHalfWidthAscii(input));
}

void HalfWidthToFullWidth(absl::string_view input, std::string *output) {
  *output = HalfWidthToFullWidth(input);
}

std::string HalfWidthToFullWidth(const absl::string_view input) {
  return HalfWidthKatakanaToFullWidthKatakana(
      HalfWidthAsciiToFullWidthAscii(input));
}

// TODO(tabata): Add another function to split voice mark
// of some UNICODE only characters (required to display
// and commit for old clients)
void NormalizeVoicedSoundMark(absl::string_view input, std::string *output) {
  *output = NormalizeVoicedSoundMark(input);
}

std::string NormalizeVoicedSoundMark(const absl::string_view input) {
  return ConvertUsingDoubleArray(internal::normalize_voiced_sound_da,
                                 internal::normalize_voiced_sound_table, input);
}

std::vector<std::pair<absl::string_view, absl::string_view>>
AlignRomanjiToHiragana(absl::string_view input) {
  return AlignUsingDoubleArray(internal::romanji_to_hiragana_da,
                               internal::romanji_to_hiragana_table, input);
}

std::vector<std::pair<absl::string_view, absl::string_view>>
AlignHiraganaToRomanji(absl::string_view input) {
  return AlignUsingDoubleArray(internal::hiragana_to_romanji_da,
                               internal::hiragana_to_romanji_table, input);
}

}  // namespace mozc::japanese
