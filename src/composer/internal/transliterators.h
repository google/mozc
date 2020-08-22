// Copyright 2010-2020, Google Inc.
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

#ifndef MOZC_COMPOSER_INTERNAL_TRANSLITERATORS_H_
#define MOZC_COMPOSER_INTERNAL_TRANSLITERATORS_H_

#include "composer/internal/transliterator_interface.h"

namespace mozc {
namespace composer {

// Factory class providing basic TransliteratorInterface instances.
class Transliterators {
 public:
  enum Transliterator {
    // Always uses a converted string rather than a raw string.
    // This should be used as default value.
    CONVERSION_STRING,
    // Always uses a raw string rather than a converted string.
    RAW_STRING,
    // Returns hiragana.
    HIRAGANA,
    // Returns full katakana.
    FULL_KATAKANA,
    // Returns half katakana.
    HALF_KATAKANA,
    // Returns full ascii.
    FULL_ASCII,
    // Returns half ascii.
    HALF_ASCII,
    // Special transliterator.
    // Use locally asigned transliterator.
    // Many methods don't accept this value.
    LOCAL,

    NUM_OF_TRANSLITERATOR,
  };

  // Return a singleton instance of a TransliteratorInterface.
  // LOCAL transliterator is not accepted.
  static const TransliteratorInterface *GetTransliterator(
      Transliterator transliterator);

  static bool SplitRaw(const size_t position, const std::string &raw,
                       const std::string &converted, std::string *raw_lhs,
                       std::string *raw_rhs, std::string *converted_lhs,
                       std::string *converted_rhs);

  static bool SplitConverted(const size_t position, const std::string &raw,
                             const std::string &converted, std::string *raw_lhs,
                             std::string *raw_rhs, std::string *converted_lhs,
                             std::string *converted_rhs);
};

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_INTERNAL_TRANSLITERATORS_H_
