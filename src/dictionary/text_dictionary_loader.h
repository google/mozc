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

#ifndef MOZC_DICTIONARY_TEXT_DICTIONARY_LOADER_H_
#define MOZC_DICTIONARY_TEXT_DICTIONARY_LOADER_H_

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "dictionary/dictionary_token.h"

namespace mozc {
namespace dictionary {

class PosMatcher;

class TextDictionaryLoader {
 public:
  // TODO(noriyukit): Better to pass the pointer of pos_matcher.
  explicit TextDictionaryLoader(const PosMatcher& pos_matcher);
  TextDictionaryLoader(uint16_t zipcode_id, uint16_t isolated_word_id);

  TextDictionaryLoader(const TextDictionaryLoader&) = delete;
  TextDictionaryLoader& operator=(const TextDictionaryLoader&) = delete;

  virtual ~TextDictionaryLoader() = default;

  // Loads tokens from system dictionary files and reading correction
  // files. Each file name can take multiple file names by separating commas.
  // The reading correction file is optional and can be an empty string.  Note
  // that the tokens loaded so far are all cleared and that this class takes the
  // ownership of the loaded tokens, i.e., they are deleted on destruction of
  // this loader instance.
  void Load(absl::string_view dictionary_filename,
            absl::string_view reading_correction_filename);

  // The same as Load() method above except that the number of tokens to be
  // loaded is limited up to first |limit| entries.
  void LoadWithLineLimit(absl::string_view dictionary_filename,
                         absl::string_view reading_correction_filename,
                         int limit);

  // Clears the loaded tokens.
  void Clear() { tokens_.clear(); }

  void AddToken(std::unique_ptr<Token> token) {
    tokens_.push_back(std::move(token));
  }

  absl::Span<const std::unique_ptr<Token>> tokens() const { return tokens_; }

  // Appends the tokens owned by this instance to |res|.  Note that the appended
  // tokens are still owned by this instance and deleted on destruction of this
  // instance or when Clear() is called.
  void CollectTokens(std::vector<Token*>* res) const;

 private:
  friend class TextDictionaryLoaderTestPeer;

  static std::vector<std::unique_ptr<Token>> LoadReadingCorrectionTokens(
      absl::string_view reading_correction_filename,
      absl::Span<const std::unique_ptr<Token>> ref_sorted_tokens, int* limit);

  // Encodes special information into |token| with the |label|.
  // Currently, label must be:
  //   - empty string,
  //   - "SPELLING_CORRECITON",
  //   - "ZIP_CODE", or
  //   - "ENGLISH".
  // Otherwise, the method returns false.
  bool RewriteSpecialToken(Token* token, absl::string_view label) const;

  std::unique_ptr<Token> ParseTSVLine(absl::string_view line) const;
  std::unique_ptr<Token> ParseTSV(
      absl::Span<const absl::string_view> columns) const;

  const uint16_t zipcode_id_;
  const uint16_t isolated_word_id_;
  std::vector<std::unique_ptr<Token>> tokens_;
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_TEXT_DICTIONARY_LOADER_H_
