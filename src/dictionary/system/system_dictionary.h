// Copyright 2010-2013, Google Inc.
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
#include "base/string_piece.h"
#include "base/trie.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/system/codec_interface.h"
#include "dictionary/system/words_info.h"
#include "storage/louds/bit_vector_based_array.h"
#include "storage/louds/louds_trie.h"
// for FRIEND_TEST
#include "testing/base/public/gunit_prod.h"

namespace mozc {

class NodeAllocatorInterface;
class DictionaryFile;
struct Token;

namespace dictionary {

class SystemDictionaryCodecInterface;

class SystemDictionary : public DictionaryInterface {
 public:
  // Callback interface for dictionary traversal (currently implemented only for
  // prefix search). Each method is called in the following manner:
  //
  // for (each key found) {
  //   OnKey(key);
  //   OnActualKey(key, actual_key, key != actual_key);
  //   for (each token in the token array for the key) {
  //     OnToken(key, actual_key, token);
  //   }
  // }
  //
  // Using the return value of each call of the three methods, you can tell the
  // traverser how to proceed. The meanings of the four values are as follows:
  //   1) TRAVERSE_DONE
  //       Quit the traversal, i.e., no more callbacks for keys and/or tokens.
  //   2) TRAVERSE_NEXT_KEY
  //       Finish the traversal for the current key and search for the next key.
  //       If returned from OnToken(), the remaining tokens are discarded.
  //   3) TRAVERSE_CULL
  //       Similar to TRAVERSE_NEXT_KEY, finish the traversal for the current
  //       key but search for the next key by using search culling. Namely,
  //       traversal of the subtree starting with the current key is skipped,
  //       which is the difference from TRAVERSE_NEXT_KEY.
  //   4) TRAVERSE_CONTINUE
  //       Continue the traversal for the current key or tokens, namely:
  //         - If returned from OnKey(), OnActualKey() will be called back.
  //         - If returned from OnActualKey(), a series of OnToken()'s will be
  //           called back.
  //         - If returned from OnToken(), OnToken() will be called back again
  //           with the next token, provided that it exists. Proceed to the next
  //           key if there's no more token.
  class Callback {
   public:
    enum ResultType {
      TRAVERSE_DONE,
      TRAVERSE_NEXT_KEY,
      TRAVERSE_CULL,
      TRAVERSE_CONTINUE,
    };

    virtual ~Callback() {
    }

    // Called back when key is found.
    virtual ResultType OnKey(const string &key) {
      return TRAVERSE_CONTINUE;
    }

    // Called back when actual key is decoded. The third argument is guaranteed
    // to be (key != actual_key) but computed in an efficient way.
    virtual ResultType OnActualKey(
        const string &key, const string &actual_key, bool is_expanded) {
      return TRAVERSE_CONTINUE;
    }

    // Called back when a token is decoded.
    virtual ResultType OnToken(const string &key,
                               const string &expanded_key,
                               const TokenInfo &token_info) {
      return TRAVERSE_CONTINUE;
    }

   protected:
    Callback() {}
  };

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

  // Implementation of DictionaryInterface.
  virtual bool HasValue(const StringPiece value) const;

  // Predictive lookup
  virtual Node *LookupPredictiveWithLimit(
      const char *str, int size, const Limit &limit,
      NodeAllocatorInterface *allocator) const;
  virtual Node *LookupPredictive(const char *str, int size,
                                 NodeAllocatorInterface *allocator) const;

  // Prefix lookup
  virtual Node *LookupPrefixWithLimit(const char *str, int size,
                                      const Limit &limit,
                                      NodeAllocatorInterface *allocator) const;
  virtual Node *LookupPrefix(const char *str, int size,
                             NodeAllocatorInterface *allocator) const;
  // TODO(noriyukit): We may want to define this method as a part of
  // DictionaryInterface for general purpose prefix search.
  void LookupPrefixWithCallback(const StringPiece key,
                                bool use_kana_modifier_insensitive_lookup,
                                Callback *callback) const;

  // Exact lookup
  virtual Node *LookupExact(const char *str, int size,
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
    FilterInfo() : conditions(NONE), value_id(-1) {}
  };

  SystemDictionary();

  bool OpenDictionaryFile();

  // Allocates nodes from |allocator| and append them to |node|.
  // Token info will be filled using |tokens_key|, |actual_key| and |tokens|
  // |tokens_key| is a key used for look up.
  // |actual_key| is a node's key.
  // They may be different when we perform ambiguous search.
  Node *AppendNodesFromTokens(
      const FilterInfo &filter,
      const string &tokens_key,
      const string &actual_key,
      const uint8 *,
      Node *node,
      NodeAllocatorInterface *allocator,
      int *limit) const;

  bool IsBadToken(const FilterInfo &filter, const TokenInfo &token_info) const;

  Node *GetReverseLookupNodesForT13N(const StringPiece value,
                                     NodeAllocatorInterface *allocator,
                                     int *limit) const;

  Node *GetReverseLookupNodesForValue(const StringPiece value,
                                      NodeAllocatorInterface *allocator,
                                      int *limit) const;

  void ScanTokens(const set<int> &id_set,
                  multimap<int, ReverseLookupResult> *reverse_results) const;

  Node *GetNodesFromReverseLookupResults(
      const set<int> &id_set,
      const multimap<int, ReverseLookupResult> &reverse_results,
      NodeAllocatorInterface *allocator,
      int *limit) const;

  const storage::louds::KeyExpansionTable &GetExpansionTableBySetting(
      const Limit &limit) const;

  scoped_ptr<storage::louds::LoudsTrie> key_trie_;
  scoped_ptr<storage::louds::LoudsTrie> value_trie_;
  scoped_ptr<storage::louds::BitVectorBasedArray> token_array_;
  scoped_ptr<DictionaryFile> dictionary_file_;
  const uint32 *frequent_pos_;
  const SystemDictionaryCodecInterface *codec_;
  const Limit empty_limit_;
  storage::louds::KeyExpansionTable hiragana_expansion_table_;

  DISALLOW_COPY_AND_ASSIGN(SystemDictionary);
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_H_
