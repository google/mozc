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

#include "dictionary/system/value_dictionary.h"

#include <cstdint>
#include <queue>
#include <string>

#include "absl/strings/string_view.h"
#include "base/util.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/system/codec_interface.h"
#include "request/conversion_request.h"
#include "storage/louds/louds_trie.h"

namespace mozc {
namespace dictionary {

using ::mozc::storage::louds::LoudsTrie;

ValueDictionary::ValueDictionary(const PosMatcher& pos_matcher,
                                 const LoudsTrie* value_trie)
    : value_trie_(value_trie),
      codec_(SystemDictionaryCodecFactory::GetCodec()),
      suggestion_only_word_id_(pos_matcher.GetSuggestOnlyWordId()) {}

// ValueDictionary is supposed to use the same data with SystemDictionary
// and SystemDictionary::HasKey should return the same result with
// ValueDictionary::HasKey.  So we can skip the actual logic of HasKey
// and return just false.
bool ValueDictionary::HasKey(absl::string_view key) const { return false; }

// ValueDictionary is supposed to use the same data with SystemDictionary
// and SystemDictionary::HasValue should return the same result with
// ValueDictionary::HasValue.  So we can skip the actual logic of HasValue
// and return just false.
bool ValueDictionary::HasValue(absl::string_view value) const { return false; }

namespace {

// A version of the above function for Token.
inline void FillToken(const uint16_t suggestion_only_word_id,
                      absl::string_view key, Token* token) {
  token->key.assign(key.data(), key.size());
  token->value = token->key;
  token->cost = 10000;
  token->lid = token->rid = suggestion_only_word_id;
  token->attributes = Token::NONE;
}

inline bool IsValidKey(absl::string_view key) {
  // Do nothing for empty key, although looking up all the entries with empty
  // string seems natural.
  if (key.empty()) {
    return false;
  }
  // Value dictionary is used to suggest English words from literal ASCII
  // query.
  const Util::ScriptType type = Util::GetFirstScriptType(key);
  if (type == Util::HIRAGANA || type == Util::KANJI || type == Util::KATAKANA) {
    return false;
  }
  return true;
}

inline DictionaryInterface::Callback::ResultType HandleTerminalNode(
    const LoudsTrie& value_trie, const SystemDictionaryCodecInterface& codec,
    const uint16_t suggestion_only_word_id, const LoudsTrie::Node& node,
    DictionaryInterface::Callback* callback, char* encoded_value_buffer,
    std::string* value, Token* token) {
  const absl::string_view encoded_value =
      value_trie.RestoreKeyString(node, encoded_value_buffer);

  value->clear();
  codec.DecodeValue(encoded_value, value);
  DictionaryInterface::Callback::ResultType result = callback->OnKey(*value);
  if (result != DictionaryInterface::Callback::TRAVERSE_CONTINUE) {
    return result;
  }
  result = callback->OnActualKey(*value, *value, /* num_expanded= */ 0);
  if (result != DictionaryInterface::Callback::TRAVERSE_CONTINUE) {
    return result;
  }
  FillToken(suggestion_only_word_id, *value, token);
  return callback->OnToken(*value, *value, *token);
}

}  // namespace

void ValueDictionary::LookupPredictive(
    absl::string_view key, const ConversionRequest& conversion_request,
    Callback* callback) const {
  if (!IsValidKey(key)) {
    return;
  }

  std::string encoded_key;
  codec_->EncodeValue(key, &encoded_key);

  LoudsTrie::Node node;
  if (!value_trie_->Traverse(encoded_key, &node)) {
    return;
  }

  char encoded_value_buffer[LoudsTrie::kMaxDepth + 1];
  std::string value;
  value.reserve(key.size() * 2);
  Token token;

  // Traverse subtree rooted at |node|.
  std::queue<LoudsTrie::Node> queue;
  queue.push(node);
  do {
    node = queue.front();
    queue.pop();

    if (value_trie_->IsTerminalNode(node)) {
      switch (HandleTerminalNode(*value_trie_, *codec_,
                                 suggestion_only_word_id_, node, callback,
                                 encoded_value_buffer, &value, &token)) {
        case Callback::TRAVERSE_DONE:
          return;
        case Callback::TRAVERSE_CULL:
          continue;
        default:
          break;
      }
    }

    for (value_trie_->MoveToFirstChild(&node); value_trie_->IsValidNode(node);
         value_trie_->MoveToNextSibling(&node)) {
      queue.push(node);
    }
  } while (!queue.empty());
}

void ValueDictionary::LookupPrefix(absl::string_view key,
                                   const ConversionRequest& conversion_request,
                                   Callback* callback) const {}

void ValueDictionary::LookupExact(absl::string_view key,
                                  const ConversionRequest& conversion_request,
                                  Callback* callback) const {
  if (!IsValidKey(key)) {
    return;
  }

  std::string lookup_key_str;
  codec_->EncodeValue(key, &lookup_key_str);
  if (value_trie_->ExactSearch(lookup_key_str) == -1) {
    return;
  }
  if (callback->OnKey(key) != Callback::TRAVERSE_CONTINUE) {
    return;
  }
  if (callback->OnActualKey(key, key, /* num_expanded= */ 0) !=
      Callback::TRAVERSE_CONTINUE) {
    return;
  }
  Token token;
  FillToken(suggestion_only_word_id_, key, &token);
  callback->OnToken(key, key, token);
}

void ValueDictionary::LookupReverse(absl::string_view str,
                                    const ConversionRequest& conversion_request,
                                    Callback* callback) const {}

}  // namespace dictionary
}  // namespace mozc
