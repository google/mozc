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

#ifndef MOZC_COMPOSER_INTERNAL_SPECIAL_KEY_H_
#define MOZC_COMPOSER_INTERNAL_SPECIAL_KEY_H_

#include "absl/strings/string_view.h"

namespace mozc::composer::internal {

// Use [U+F000, U+F8FF] to represent special keys (e.g. {!}, {abc}).
// The range of Unicode PUA is [U+E000, U+F8FF], and we use them from U+F000.
// * The range of [U+E000, U+F000) is used for user defined PUA characters.
// * The users can still use [U+F000, U+F8FF] for their user dictionary.
//   but, they should not use them for composing rules.
constexpr char32_t kSpecialKeyBegin = 0xF000;
constexpr char32_t kSpecialKeyEnd = 0xF8FF;

// U+000F and U+000E are used as fallback for special keys that are not
// registered in the table. "{abc}" is converted to "\u000Fabc\u000E".
constexpr char kSpecialKeyOpen[] = "\u000F";   // Shift-In of ASCII (1 byte)
constexpr char kSpecialKeyClose[] = "\u000E";  // Shift-Out of ASCII (1 byte)

constexpr bool IsSpecialKey(char32_t c) {
  return (kSpecialKeyBegin <= c && c <= kSpecialKeyEnd);
}

// Trims a special key from input and returns the rest.
// If the input doesn't have any special keys at the beginning, it returns the
// entire string.
absl::string_view TrimLeadingSpecialKey(absl::string_view input);

}  // namespace mozc::composer::internal

#endif  // MOZC_COMPOSER_INTERNAL_SPECIAL_KEY_H_
