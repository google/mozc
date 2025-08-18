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

#ifndef MOZC_DICTIONARY_SYSTEM_TOKEN_DECODE_ITERATOR_H_
#define MOZC_DICTIONARY_SYSTEM_TOKEN_DECODE_ITERATOR_H_

#include <cstdint>
#include <string>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/japanese_util.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/system/codec_interface.h"
#include "dictionary/system/words_info.h"
#include "storage/louds/louds_trie.h"

namespace mozc {
namespace dictionary {

class TokenDecodeIterator {
 public:
  TokenDecodeIterator(const TokenDecodeIterator&) = delete;
  TokenDecodeIterator& operator=(const TokenDecodeIterator&) = delete;
  TokenDecodeIterator(const SystemDictionaryCodecInterface* codec,
                      const storage::louds::LoudsTrie& value_trie,
                      const uint32_t* frequent_pos, absl::string_view key,
                      const uint8_t* ptr);
  ~TokenDecodeIterator() = default;

  const TokenInfo& Get() const { return token_info_; }
  bool Done() const { return state_ == DONE; }
  void Next();

 private:
  enum State {
    HAS_NEXT,
    LAST_TOKEN,
    DONE,
  };

  void NextInternal();

  void LookupValue(int id, std::string* value) const {
    char buffer[storage::louds::LoudsTrie::kMaxDepth + 1];
    const absl::string_view encoded_value =
        value_trie_->RestoreKeyString(id, buffer);
    codec_->DecodeValue(encoded_value, value);
  }

  const SystemDictionaryCodecInterface* codec_;
  const storage::louds::LoudsTrie* value_trie_;
  const uint32_t* frequent_pos_;

  const absl::string_view key_;
  // Katakana key will be lazily initialized.
  std::string key_katakana_;

  State state_;
  const uint8_t* ptr_;

  TokenInfo token_info_;
  Token token_;
};

// Implementation is inlined for performance.

inline TokenDecodeIterator::TokenDecodeIterator(
    const SystemDictionaryCodecInterface* codec,
    const storage::louds::LoudsTrie& value_trie, const uint32_t* frequent_pos,
    absl::string_view key, const uint8_t* ptr)
    : codec_(codec),
      value_trie_(&value_trie),
      frequent_pos_(frequent_pos),
      key_(key),
      state_(HAS_NEXT),
      ptr_(ptr),
      token_info_(nullptr) {
  token_.key.assign(key.data(), key.size());
  NextInternal();
}

inline void TokenDecodeIterator::Next() {
  DCHECK_NE(state_, DONE);
  if (state_ == LAST_TOKEN) {
    state_ = DONE;
    return;
  }
  NextInternal();
}

inline void TokenDecodeIterator::NextInternal() {
  // Reset token_info with preserving some needed info in previous token.
  int prev_id_in_value_trie = token_info_.id_in_value_trie;
  token_info_.Clear();
  token_info_.token = &token_;

  // Do not clear key in token.
  token_info_.token->attributes = Token::NONE;

  // This implementation is depending on the internal behavior of DecodeToken
  // especially which fields are updated or not. Important fields are:
  // Token::key, Token::value : key and value are never updated.
  // Token::cost : always updated.
  // Token::lid, Token::rid : updated iff the pos_type is neither
  //   FREQUENT_POS nor SAME_AS_PREV_POS.
  // Token::attributes : updated iff the value is SPELLING_COLLECTION.
  // TokenInfo::id_in_value_trie : updated iff the value_type is
  //   DEFAULT_VALUE.
  // Thus, by not-reseting Token instance intentionally, we can skip most
  //   SAME_AS_PREV operations.
  // The exception is Token::attributes. It is not-always set, so we need
  // reset it everytime.
  // This kind of structure should be packed in the codec or some
  // related but new class.
  int read_bytes;
  if (!codec_->DecodeToken(ptr_, &token_info_, &read_bytes)) {
    state_ = LAST_TOKEN;
  }
  ptr_ += read_bytes;

  // Fill remaining values.
  switch (token_info_.value_type) {
    case TokenInfo::DEFAULT_VALUE: {
      token_.value.clear();
      LookupValue(token_info_.id_in_value_trie, &token_.value);
      break;
    }
    case TokenInfo::SAME_AS_PREV_VALUE: {
      DCHECK_NE(prev_id_in_value_trie, -1);
      token_info_.id_in_value_trie = prev_id_in_value_trie;
      // We can keep the current value here.
      break;
    }
    case TokenInfo::AS_IS_HIRAGANA: {
      token_.value = token_.key;
      break;
    }
    case TokenInfo::AS_IS_KATAKANA: {
      if (!key_.empty() && key_katakana_.empty()) {
        key_katakana_ = japanese_util::HiraganaToKatakana(key_);
      }
      token_.value = key_katakana_;
      break;
    }
    default: {
      LOG(DFATAL) << "unknown value_type: " << token_info_.value_type;
      break;
    }
  }

  if (token_info_.accent_encoding_type == TokenInfo::EMBEDDED_IN_TOKEN) {
    absl::StrAppend(&token_.value, "_", token_info_.accent_type);
  }

  if (token_info_.pos_type == TokenInfo::FREQUENT_POS) {
    const uint32_t pos = frequent_pos_[token_info_.id_in_frequent_pos_map];
    token_.lid = pos >> 16;
    token_.rid = pos & 0xffff;
  }
}

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_SYSTEM_TOKEN_DECODE_ITERATOR_H_
