// Copyright 2010-2011, Google Inc.
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

#ifndef MOZC_COMPOSER_INTERNAL_TRANSLITERATORS_JA_H_
#define MOZC_COMPOSER_INTERNAL_TRANSLITERATORS_JA_H_

#include "composer/internal/transliterators.h"

namespace mozc {
namespace composer {

// Factory class providing TransliteratorInterface instances for Japanese input.
class TransliteratorsJa : public Transliterators {
 public:
  // Return a singleton instance for Hiragana input.
  static const TransliteratorInterface *GetHiraganaTransliterator();

  // Return a singleton instance for full-width Katakana input.
  static const TransliteratorInterface *GetFullKatakanaTransliterator();

  // Return a singleton instance for half-width Katakana input.
  static const TransliteratorInterface *GetHalfKatakanaTransliterator();

  // Return a singleton instance for full-width Ascii input.
  static const TransliteratorInterface *GetFullAsciiTransliterator();

  // Return a singleton instance for half-width Ascii input.
  static const TransliteratorInterface *GetHalfAsciiTransliterator();
};

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_INTERNAL_TRANSLITERATORS_JA_H_
