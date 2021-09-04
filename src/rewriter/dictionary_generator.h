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
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/freelist.h"
#include "base/port.h"
#include "data_manager/data_manager_interface.h"
#include "dictionary/user_pos.h"

namespace mozc {
namespace rewriter {

class Token final {
 public:
  Token() = default;
  ~Token() = default;

  Token(const Token &) = delete;
  Token &operator=(const Token &) = delete;

  // Merge the values of the token.
  void MergeFrom(const Token &token);

  // Return the fingerprint of the keys (key, value and pos).
  uint64_t GetID() const;

  // Accessors
  int sorting_key() const { return sorting_key_; }
  void set_sorting_key(int sorting_key) { sorting_key_ = sorting_key; }

  const std::string &key() const { return key_; }
  void set_key(const std::string &key) { key_ = key; }

  const std::string &value() const { return value_; }
  void set_value(const std::string &value) { value_ = value; }

  const std::string &pos() const { return pos_; }
  void set_pos(const std::string &pos) { pos_ = pos; }

  const std::string &description() const { return description_; }
  const std::string &additional_description() const {
    return additional_description_;
  }
  void set_description(const std::string &description) {
    description_ = description;
  }
  void set_additional_description(const std::string &additional_description) {
    additional_description_ = additional_description;
  }

 private:
  int sorting_key_ = 0;
  std::string key_;
  std::string value_;
  std::string pos_;
  std::string description_;
  std::string additional_description_;
  // NOTE(komatsu): When new arguments are added, MergeFrom function
  // should be updated too.
};

class DictionaryGenerator {
 public:
  explicit DictionaryGenerator(const DataManagerInterface &data_manager);
  virtual ~DictionaryGenerator();

  DictionaryGenerator(const DictionaryGenerator &) = delete;
  DictionaryGenerator &operator=(const DictionaryGenerator &) = delete;

  // Add the token into the pool.
  void AddToken(const Token &token);

  // Output the tokens into the filename.
  bool Output(const std::string &filename) const;

 private:
  ObjectPool<Token> token_pool_;
  std::map<uint64_t, Token *> token_map_;
  std::unique_ptr<const UserPOSInterface> user_pos_;
  uint16_t open_bracket_id_;
  uint16_t close_bracket_id_;
};

}  // namespace rewriter
}  // namespace mozc

#endif  // MOZC_REWRITER_DICTIONARY_GENERATOR_H_
