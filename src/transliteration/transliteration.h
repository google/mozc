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

// Libraries for transliterations.

#ifndef MOZC_TRANSLITERATION_TRANSLITERATION_H_
#define MOZC_TRANSLITERATION_TRANSLITERATION_H_

#include <string>
#include <vector>

namespace mozc {
namespace transliteration {

enum TransliterationType {
  // "ひらがな"
  HIRAGANA,
  // "カタカナ"
  FULL_KATAKANA,
  // "ascII"
  HALF_ASCII,
  // "ASCII"
  HALF_ASCII_UPPER,
  // "ascii"
  HALF_ASCII_LOWER,
  // "Ascii"
  HALF_ASCII_CAPITALIZED,
  // "ａｓｃＩＩ"
  FULL_ASCII,
  // "ＡＳＣＩＩ"
  FULL_ASCII_UPPER,
  // "ａｓｃｉｉ"
  FULL_ASCII_LOWER,
  // "Ａｓｃｉｉ"
  FULL_ASCII_CAPITALIZED,
  // "ｶﾀｶﾅ"
  HALF_KATAKANA,
  NUM_T13N_TYPES
};
typedef std::vector<std::string> Transliterations;

static const TransliterationType TransliterationTypeArray[NUM_T13N_TYPES] = {
    HIRAGANA,         FULL_KATAKANA,          HALF_ASCII,    HALF_ASCII_UPPER,
    HALF_ASCII_LOWER, HALF_ASCII_CAPITALIZED, FULL_ASCII,    FULL_ASCII_UPPER,
    FULL_ASCII_LOWER, FULL_ASCII_CAPITALIZED, HALF_KATAKANA,
};

class T13n {
 public:
  T13n() = delete;
  T13n(const T13n&) = delete;
  T13n& operator=(const T13n&) = delete;

  // Return true if the prefix of the type starts with FULL_ASCII.
  static bool IsInFullAsciiTypes(TransliterationType type);

  // Return true if the prefix of the type starts with HALF_ASCII.
  static bool IsInHalfAsciiTypes(TransliterationType type);

  // Return true if the prefix of the type starts with HIRAGANA.
  static bool IsInHiraganaTypes(TransliterationType type);

  // Return true if the prefix of the type starts with FULL_KATAKANA.
  static bool IsInFullKatakanaTypes(TransliterationType type);

  // Return true if the prefix of the type starts with HALF_KATAKANA.
  static bool IsInHalfKatakanaTypes(TransliterationType type);

  // Return one of full ascii types toggling the current type.
  static TransliterationType ToggleFullAsciiTypes(
      TransliterationType current_type);

  // Return one of half ascii types toggling the current type.
  static TransliterationType ToggleHalfAsciiTypes(
      TransliterationType current_type);
};

}  // namespace transliteration
}  // namespace mozc

#endif  // MOZC_TRANSLITERATION_TRANSLITERATION_H_
