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

// static class for nomalizing session output
// Class functions to be used for output by the Session class.

#ifndef MOZC_BASE_TEXT_NORMALIZER_H_
#define MOZC_BASE_TEXT_NORMALIZER_H_

#include <string>

#include "absl/strings/string_view.h"

namespace mozc {

class TextNormalizer {
 public:
  enum Flag {
    kNone = 0,     // No normalization.
    kDefault = 1,  // Default behavior (different per platform).
    kAll = 2,      // All normalizations.
  };

  TextNormalizer() = delete;
  TextNormalizer(const TextNormalizer&) = delete;
  TextNormalizer& operator=(const TextNormalizer&) = delete;

  // Normalizes `input` with all configurations.
  static std::string NormalizeTextWithFlag(absl::string_view input, Flag flag);

  // Normalizes `input` considering the platform.
  static std::string NormalizeText(absl::string_view input) {
    return NormalizeTextWithFlag(input, kDefault);
  }

  // Normalizes Japanese CJK compatibility ideographs to SVS characters.
  // Returns false and keeps output as is, if no character is normalized.
  static bool NormalizeTextToSvs(absl::string_view input, std::string* output);
  static std::string NormalizeTextToSvs(absl::string_view input);
};

}  // namespace mozc

#endif  // MOZC_BASE_TEXT_NORMALIZER_H_
