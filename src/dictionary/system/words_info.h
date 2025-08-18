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

// Container for words info to build system dictionary

#ifndef MOZC_DICTIONARY_SYSTEM_WORDS_INFO_H_
#define MOZC_DICTIONARY_SYSTEM_WORDS_INFO_H_

namespace mozc {
namespace dictionary {

struct Token;

// TODO(toshiyuki): These could be implemented by protobuf
// Represents each token info
struct TokenInfo {
  enum PosType {
    DEFAULT_POS = 0,
    FREQUENT_POS = 1,
    SAME_AS_PREV_POS = 2,
    POS_TYPE_SIZE = 3,
  };
  enum ValueType {
    DEFAULT_VALUE = 0,
    // value is same as prev token's value
    SAME_AS_PREV_VALUE = 1,
    // value is same as key
    AS_IS_HIRAGANA = 2,
    // we can get the value by converting key to katakana form.
    AS_IS_KATAKANA = 3,
    VALUE_TYPE_SIZE = 4,
  };
  enum CostType {
    DEFAULT_COST = 0,
    CAN_USE_SMALL_ENCODING = 1,
    COST_TYPE_SIZE = 2,
  };
  enum AccentEncodingType {
    ENCODED_IN_VALUE = 0,
    EMBEDDED_IN_TOKEN = 1,
    ACCENT_ENCODING_TYPE_SIZE = 2,
  };
  explicit TokenInfo(Token* t) {
    Clear();
    token = t;
  }
  void Clear() {
    token = nullptr;
    id_in_value_trie = -1;
    id_in_frequent_pos_map = -1;
    pos_type = DEFAULT_POS;
    value_type = DEFAULT_VALUE;
    cost_type = DEFAULT_COST;
    accent_encoding_type = ENCODED_IN_VALUE;
    accent_type = -1;
  }
  // Do not delete |token| in destructor, because |token| can be
  // on-memory object owned by other module.
  // TODO(toshiyuki): Make it more simple structure by;
  // 1) Making subclass which deletes token, or
  // 2) Modifying text_dictionary_loader not to handle the ownership for token

  // original token
  Token* token;
  // id of the value(=word) string in value trie
  int id_in_value_trie;
  // id of the pos map
  int id_in_frequent_pos_map;
  // POS type for encoding
  PosType pos_type;
  ValueType value_type;
  CostType cost_type;
  AccentEncodingType accent_encoding_type;
  int accent_type;
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_SYSTEM_WORDS_INFO_H_
