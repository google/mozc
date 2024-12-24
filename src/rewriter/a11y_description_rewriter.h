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

#ifndef MOZC_REWRITER_A11Y_DESCRIPTION_REWRITER_H_
#define MOZC_REWRITER_A11Y_DESCRIPTION_REWRITER_H_

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "converter/segments.h"
#include "data_manager/data_manager.h"
#include "data_manager/serialized_dictionary.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

class A11yDescriptionRewriter : public RewriterInterface {
 public:
  explicit A11yDescriptionRewriter(const DataManager *data_manager);

  A11yDescriptionRewriter(const A11yDescriptionRewriter &) = delete;
  A11yDescriptionRewriter &operator=(const A11yDescriptionRewriter &) = delete;

  ~A11yDescriptionRewriter() override = default;

  int capability(const ConversionRequest &request) const override;
  bool Rewrite(const ConversionRequest &request,
               Segments *segments) const override;

 private:
  enum CharacterType {
    INITIAL_STATE,
    HIRAGANA,                          // 'あ'
    HIRAGANA_SMALL_LETTER,             // 'ぁ'
    KATAKANA,                          // 'ア'
    KATAKANA_SMALL_LETTER,             // 'ァ'
    HALF_WIDTH_KATAKANA,               // 'ｱ'
    HALF_WIDTH_KATAKANA_SMALL_LETTER,  // 'ｧ'
    PROLONGED_SOUND_MARK,              // 'ー'
    HALF_ALPHABET_LOWER,               // 'a' - 'z'
    HALF_ALPHABET_UPPER,               // 'A' - 'Z'
    FULL_ALPHABET_LOWER,               // 'ａ' - 'ｚ'
    FULL_ALPHABET_UPPER,               // 'Ａ' - 'Ｚ'
    OTHERS,                            // '亜', number, symbol, alphabet...
  };

  CharacterType GetCharacterType(char32_t codepoint) const;
  std::string GetKanaCharacterLabel(char32_t codepoint,
                                    CharacterType current_type,
                                    CharacterType previous_type) const;
  void AddA11yDescription(Segment::Candidate *candidate) const;

  const absl::flat_hash_set<char32_t> small_letter_set_;
  const absl::flat_hash_map<char32_t, char32_t>
      half_width_small_katakana_to_large_katakana_;
  std::unique_ptr<SerializedDictionary> description_map_;
};

}  // namespace mozc

#endif  // MOZC_REWRITER_A11Y_DESCRIPTION_REWRITER_H_
