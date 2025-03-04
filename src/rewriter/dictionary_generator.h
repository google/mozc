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

// Classes managing dictionary entries.

#ifndef MOZC_REWRITER_DICTIONARY_GENERATOR_H_
#define MOZC_REWRITER_DICTIONARY_GENERATOR_H_

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>

#include "absl/container/btree_set.h"
#include "absl/strings/str_format.h"
#include "data_manager/data_manager.h"
#include "dictionary/user_pos.h"

namespace mozc {
namespace rewriter {

struct Token {
  void MergeFrom(Token new_token);

  template <typename Sink>
  friend void AbslStringify(Sink &sink, const Token &token) {
    absl::Format(
        &sink,
        "sorting_key = %d, key = %s, value = %s, pos = %s, description "
        "= %s, additional_description = %s",
        token.sorting_key, token.key, token.value, token.pos, token.description,
        token.additional_description);
  }

  int sorting_key = 0;
  std::string key;
  std::string value;
  std::string pos;
  std::string description;
  std::string additional_description;
  // NOTE(komatsu): When new arguments are added, MergeFrom function
  // should be updated too.
};

bool operator==(const Token &lhs, const Token &rhs);
inline bool operator!=(const Token &lhs, const Token &rhs) {
  return !(lhs == rhs);
}
// Sort by keys first.  Key represents the reading of the token. If the keys are
// equal, sorting keys are used.  Sorting keys are typically character encodings
// like CP932, Unicode, etc. If the both keys are equal, values (encoded in
// UTF8) are used.
bool operator<(const Token &lhs, const Token &rhs);

class DictionaryGenerator {
 public:
  explicit DictionaryGenerator(const DataManager &data_manager);

  DictionaryGenerator(DictionaryGenerator &&) = default;
  DictionaryGenerator &operator=(DictionaryGenerator &&) = default;

  // Add the token into the pool.
  const Token &AddToken(Token token);

  // Output the tokens into the stream.
  bool Output(std::ostream &os) const;

 private:
  absl::btree_set<Token> tokens_;
  std::unique_ptr<const dictionary::UserPos> user_pos_;
  uint16_t open_bracket_id_;
  uint16_t close_bracket_id_;
};

}  // namespace rewriter
}  // namespace mozc

#endif  // MOZC_REWRITER_DICTIONARY_GENERATOR_H_
