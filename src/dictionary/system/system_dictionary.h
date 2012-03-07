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

// Mozc system dictionary

#ifndef MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_H_
#define MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/base.h"
#include "base/trie.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/rx/rx_trie.h"
#include "dictionary/rx/rbx_array.h"
#include "dictionary/system/codec_interface.h"
#include "dictionary/system/words_info.h"
// for FRIEND_TEST
#include "testing/base/public/gunit_prod.h"

namespace mozc {

namespace dictionary {
class SystemDictionaryCodecInterface;
}  // namespace dictionary

class NodeAllocatorInterface;
class DictionaryFile;
struct Token;

class SystemDictionary : public DictionaryInterface {
 public:
  struct ReverseLookupResult {
    // Offset from the tokens section beginning.
    // (token_array_->Get(id_in_key_trie) ==
    //  token_array_->Get(0) + tokens_offset)
    int tokens_offset;
    // Id in key trie
    int id_in_key_trie;
  };

  virtual ~SystemDictionary();

  static SystemDictionary *CreateSystemDictionaryFromFile(
      const string &filename);

  static SystemDictionary *CreateSystemDictionaryFromImage(
      const char *ptr, int len);

  // Predictive lookup
  virtual Node *LookupPredictiveWithLimit(
      const char *str, int size, const Limit &limit,
      NodeAllocatorInterface *allocator) const;
  virtual Node *LookupPredictive(const char *str, int size,
                                 NodeAllocatorInterface *allocator) const;
  // Prefix lookup
  virtual Node *LookupPrefixWithLimit(
      const char *str, int size,
      const Limit &limit,
      NodeAllocatorInterface *allocator) const;
  // Prefix lookup
  virtual Node *LookupPrefix(
      const char *str, int size,
      NodeAllocatorInterface *allocator) const;
  // Value to key prefix lookup
  virtual Node *LookupReverse(const char *str, int size,
                              NodeAllocatorInterface *allocator) const;
  virtual void PopulateReverseLookupCache(
      const char *str, int size, NodeAllocatorInterface *allocator) const;
  virtual void ClearReverseLookupCache(NodeAllocatorInterface *allocator) const;

 private:
  FRIEND_TEST(SystemDictionaryTest, TokenAfterSpellningToken);

  struct FilterInfo {
    enum Condition {
      NONE = 0,
      VALUE_ID = 1,
      NO_SPELLING_CORRECTION = 2,
      ONLY_T13N = 4,
    };
    int conditions;
    // Return results only for tokens with given |value_id|.
    // If VALUE_ID is specified
    int value_id;
    int key_len_lower_limit;
    int key_len_upper_limit;
    // Starting position for begin with filter.
    // Assume that the target key length >= pos.
    // Do not filter if -1.
    // We will check the key is beginning with the string in the list.
    //
    // Example:
    //  key_begin_with_pos: 3
    //  key_begin_with_list: a, b, cd,
    //
    //  input:
    //   abcd -> NG (input + 3 is 'd')
    //   abca -> OK
    //   abcc -> NG (input + 3 does not start with any of 'a', 'b', and 'cd')
    //   abccd -> OK
    //   abcaaaa -> OK
    int key_begin_with_pos;
    // This does not have the ownership
    const Trie<string> *key_begin_with_trie;
    FilterInfo() : conditions(NONE), value_id(-1),
                   key_len_lower_limit(0),
                   key_len_upper_limit(kint32max),
                   key_begin_with_pos(-1),
                   key_begin_with_trie(NULL) {}
  };

  SystemDictionary();

  bool OpenDictionaryFile();

  Node *AppendNodesFromTokens(
      const FilterInfo &filter,
      const string &tokens_key,
      vector<dictionary::TokenInfo> *tokens,
      Node *node,
      NodeAllocatorInterface *allocator,
      int *limit) const;

  void FillTokenInfo(const string &key,
                     const string &key_katakana,
                     const dictionary::TokenInfo *prev_token_info,
                     dictionary::TokenInfo *token_info) const;

  bool IsBadToken(const FilterInfo &filter,
                  const dictionary::TokenInfo &token_info) const;

  void LookupValue(dictionary::TokenInfo *token_info) const;

  Node *GetNodesFromLookupResults(const FilterInfo &filter,
                                  const vector<rx::RxEntry> &results,
                                  NodeAllocatorInterface *allocator,
                                  int *limit) const;

  Node *GetReverseLookupNodesForT13N(const string &value,
                                     NodeAllocatorInterface *allocator,
                                     int *limit) const;

  Node *GetReverseLookupNodesForValue(const string &value,
                                      NodeAllocatorInterface *allocator,
                                      int *limit) const;

  void ScanTokens(const set<int> &id_set,
                  multimap<int, ReverseLookupResult> *reverse_results) const;

  Node *GetNodesFromReverseLookupResults(
      const set<int> &id_set,
      const multimap<int, ReverseLookupResult> &reverse_results,
      NodeAllocatorInterface *allocator,
      int *limit) const;

  Node *CopyTokenToNode(NodeAllocatorInterface *allocator,
                        const Token &token) const;

  scoped_ptr<rx::RxTrie> key_trie_;
  scoped_ptr<rx::RxTrie> value_trie_;
  scoped_ptr<rx::RbxArray> token_array_;
  scoped_ptr<DictionaryFile> dictionary_file_;
  const uint32 *frequent_pos_;
  const dictionary::SystemDictionaryCodecInterface *codec_;
  const Limit empty_limit_;

  DISALLOW_COPY_AND_ASSIGN(SystemDictionary);
};
}  // namespace mozc

#endif  // MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_H_
