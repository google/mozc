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

#ifndef MOZC_BASE_JAPANESE_UTIL_H_
#define MOZC_BASE_JAPANESE_UTIL_H_

#include <string>

#include "base/double_array.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace japanese_util {

// Japanese utilities for character form transliteration.
void ConvertUsingDoubleArray(const japanese_util_rule::DoubleArray *da,
                             const char *table, absl::string_view input,
                             std::string *output);
void HiraganaToKatakana(absl::string_view input, std::string *output);
void HiraganaToHalfwidthKatakana(absl::string_view input, std::string *output);
void HiraganaToRomanji(absl::string_view input, std::string *output);
void HalfWidthAsciiToFullWidthAscii(absl::string_view input,
                                    std::string *output);
void FullWidthAsciiToHalfWidthAscii(absl::string_view input,
                                    std::string *output);
void HiraganaToFullwidthRomanji(absl::string_view input, std::string *output);
void RomanjiToHiragana(absl::string_view input, std::string *output);
void KatakanaToHiragana(absl::string_view input, std::string *output);
void HalfWidthKatakanaToFullWidthKatakana(absl::string_view input,
                                          std::string *output);
void FullWidthKatakanaToHalfWidthKatakana(absl::string_view input,
                                          std::string *output);
void FullWidthToHalfWidth(absl::string_view input, std::string *output);
void HalfWidthToFullWidth(absl::string_view input, std::string *output);
void NormalizeVoicedSoundMark(absl::string_view input, std::string *output);

}  // namespace japanese_util
}  // namespace mozc

#endif  // MOZC_BASE_JAPANESE_UTIL_H_
