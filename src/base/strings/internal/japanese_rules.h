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

#ifndef MOZC_BASE_STRINGS_INTERNAL_JAPANESE_RULES_H_
#define MOZC_BASE_STRINGS_INTERNAL_JAPANESE_RULES_H_

#include "base/strings/internal/double_array.h"

namespace mozc::japanese::internal {

#define DECLARE_RULE(name)          \
  extern const char name##_table[]; \
  extern const DoubleArray name##_da[]

DECLARE_RULE(hiragana_to_katakana);
DECLARE_RULE(hiragana_to_romanji);
DECLARE_RULE(katakana_to_hiragana);
DECLARE_RULE(romanji_to_hiragana);
DECLARE_RULE(fullwidthkatakana_to_halfwidthkatakana);
DECLARE_RULE(halfwidthkatakana_to_fullwidthkatakana);
DECLARE_RULE(halfwidthascii_to_fullwidthascii);
DECLARE_RULE(fullwidthascii_to_halfwidthascii);
DECLARE_RULE(normalize_voiced_sound);
DECLARE_RULE(kanjinumber_to_arabicnumber);

#undef DECLARE_RULE

}  // namespace mozc::japanese::internal

#endif  // MOZC_BASE_STRINGS_INTERNAL_JAPANESE_RULES_H_
