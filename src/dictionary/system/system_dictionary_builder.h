// Copyright 2010-2012, Google Inc.
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

#include <deque>
#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "base/base.h"
#include "dictionary/system/words_info.h"

namespace mozc {
struct DictionaryFileSection;
struct Token;

#ifdef MOZC_USE_MOZC_LOUDS
namespace storage {
namespace louds {
class BitVectorBasedArrayBuilder;
class LoudsTrieBuilder;
}  // namespace louds
}  // namespace storage
#else
namespace rx {
class RbxArrayBuilder;
class RxTrieBuilder;
}  // namespace rx
#endif  // MOZC_USE_MOZC_LOUDS

namespace dictionary {
class SystemDictionaryCodecInterface;

class SystemDictionaryBuilder {
 public:
  // Represents words info for a certain key(=reading).
  struct KeyInfo {
    KeyInfo() : id_in_key_trie(-1) {}
    // id of the key(=reading) string in key trie
    int id_in_key_trie;
    string key;
    vector<TokenInfo> tokens;
  };

  SystemDictionaryBuilder();
  virtual ~SystemDictionaryBuilder();

  void BuildFromFile(const string &input_file);
  void BuildFromTokens(const vector<Token *> &tokens);

  void WriteToFile(const string &output_file) const;
  void WriteToStream(const string &intermediate_output_file_base_path,
                     ostream *output_stream) const;

 private:
  typedef deque<KeyInfo> KeyInfoList;

#ifdef MOZC_USE_MOZC_LOUDS
  typedef ::mozc::storage::louds::LoudsTrieBuilder TrieBuilder;
  typedef ::mozc::storage::louds::BitVectorBasedArrayBuilder ArrayBuilder;
#else
  typedef ::mozc::rx::RxTrieBuilder TrieBuilder;
  typedef ::mozc::rx::RbxArrayBuilder ArrayBuilder;
#endif

  void ReadTokens(const vector<Token *>& tokens,
                  KeyInfoList *key_info_list) const;

  void BuildFrequentPos(const KeyInfoList &key_info_list);

  void BuildValueTrie(const KeyInfoList &key_info_list);

  void BuildKeyTrie(const KeyInfoList &key_info_list);

  void BuildTokenArray(const KeyInfoList &key_info_list);

  void SetIdForValue(KeyInfoList *key_info_list) const;
  void SetIdForKey(KeyInfoList *key_info_list) const;
  void SortTokenInfo(KeyInfoList *key_info_list) const;

  void SetCostType(KeyInfoList *key_info_list) const;
  void SetPosType(KeyInfoList *keyinfomap) const;
  void SetValueType(KeyInfoList *key_info_list) const;

  scoped_ptr<TrieBuilder> value_trie_builder_;
  scoped_ptr<TrieBuilder> key_trie_builder_;
  scoped_ptr<ArrayBuilder> token_array_builder_;

  // mapping from {left_id, right_id} to POS index (0--255)
  map<uint32, int> frequent_pos_;

  const dictionary::SystemDictionaryCodecInterface *codec_;

  DISALLOW_COPY_AND_ASSIGN(SystemDictionaryBuilder);
};
}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_BUILDER_H_
