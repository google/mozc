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

#ifndef MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_BUILDER_H_
#define MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_BUILDER_H_

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/port.h"
#include "dictionary/system/words_info.h"

namespace mozc {
namespace storage {
namespace louds {
class BitVectorBasedArrayBuilder;
class LoudsTrieBuilder;
}  // namespace louds
}  // namespace storage

namespace dictionary {

class SystemDictionaryCodecInterface;
class DictionaryFileCodecInterface;
struct Token;

class SystemDictionaryBuilder {
 public:
  // Represents words info for a certain key(=reading).
  struct KeyInfo {
    KeyInfo() : id_in_key_trie(-1) {}
    // id of the key(=reading) string in key trie
    int id_in_key_trie;
    std::string key;
    std::vector<TokenInfo> tokens;
  };

  SystemDictionaryBuilder();
  SystemDictionaryBuilder(const SystemDictionaryCodecInterface *codec,
                          const DictionaryFileCodecInterface *file_codec);
  virtual ~SystemDictionaryBuilder();
  void BuildFromTokens(const std::vector<Token *> &tokens);

  void WriteToFile(const std::string &output_file) const;
  void WriteToStream(const std::string &intermediate_output_file_base_path,
                     std::ostream *output_stream) const;

 private:
  typedef std::deque<KeyInfo> KeyInfoList;

  void ReadTokens(const std::vector<Token *> &tokens,
                  KeyInfoList *key_info_list) const;

  void BuildFrequentPos(const KeyInfoList &key_info_list);

  void BuildValueTrie(const KeyInfoList &key_info_list);

  void BuildKeyTrie(const KeyInfoList &key_info_list);

  void BuildTokenArray(const KeyInfoList &key_info_list);

  void SetIdForValue(KeyInfoList *key_info_list) const;
  void SetIdForKey(KeyInfoList *key_info_list) const;
  void SortTokenInfo(KeyInfoList *key_info_list) const;

  void SetCostType(KeyInfoList *key_info_list) const;
  void SetPosType(KeyInfoList *key_info_list) const;
  void SetValueType(KeyInfoList *key_info_list) const;

  std::unique_ptr<mozc::storage::louds::LoudsTrieBuilder> value_trie_builder_;
  std::unique_ptr<mozc::storage::louds::LoudsTrieBuilder> key_trie_builder_;
  std::unique_ptr<mozc::storage::louds::BitVectorBasedArrayBuilder>
      token_array_builder_;

  // mapping from {left_id, right_id} to POS index (0--255)
  std::map<uint32_t, int> frequent_pos_;

  const SystemDictionaryCodecInterface *codec_;
  const DictionaryFileCodecInterface *file_codec_;

  DISALLOW_COPY_AND_ASSIGN(SystemDictionaryBuilder);
};
}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_BUILDER_H_
