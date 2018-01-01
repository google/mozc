// Copyright 2010-2018, Google Inc.
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

#ifndef MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_H_
#define MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/port.h"
#include "base/string_piece.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/file/codec_interface.h"
#include "dictionary/system/codec_interface.h"
#include "dictionary/system/key_expansion_table.h"
#include "dictionary/system/words_info.h"
#include "storage/louds/bit_vector_based_array.h"
#include "storage/louds/louds_trie.h"

namespace mozc {
namespace dictionary {

class DictionaryFile;
class DictionaryFileCodecInterface;
class SystemDictionaryCodecInterface;

class SystemDictionary : public DictionaryInterface {
 public:
  // System dictionary options represented as bitwise enum.
  enum Options {
    NONE = 0,
    // If ENABLE_REVERSE_LOOKUP_INDEX is set, we will have the index in heap
    // from the id in value trie to the id in key trie.
    // That consumes more memory but we can perform reverse lookup more quickly.
    ENABLE_REVERSE_LOOKUP_INDEX = 1,
  };

  // Builder class for system dictionary
  // Usage:
  //   SystemDictionary::Builder builder(filename);
  //   builder.SetOptions(SystemDictionary::NONE);
  //   builder.SetCodec(NULL);
  //   SystemDictionary *dictionary = builder.Build();
  //   ...
  //   delete dictionary;
  class Builder {
   public:
    // Creates Builder from filename
    explicit Builder(const string &filename);
    // Creates Builder from image
    Builder(const char *ptr, int len);
    ~Builder();

    // Sets options (default: NONE)
    Builder &SetOptions(Options options);

    // Sets codec (default: NULL)
    // Uses default codec if this is NULL
    // Doesn't take the ownership of |codec|.
    Builder &SetCodec(const SystemDictionaryCodecInterface *codec);

    // Builds and returns system dictionary.
    SystemDictionary *Build();

   private:
    struct Specification;
    std::unique_ptr<Specification> spec_;
    DISALLOW_COPY_AND_ASSIGN(Builder);
  };

  virtual ~SystemDictionary();

  const storage::louds::LoudsTrie &value_trie() const { return value_trie_; }

  // Implementation of DictionaryInterface.
  virtual bool HasKey(StringPiece key) const;
  virtual bool HasValue(StringPiece value) const;

  virtual void LookupPredictive(StringPiece key,
                                const ConversionRequest &converter_request,
                                Callback *callback) const;

  virtual void LookupPrefix(StringPiece key,
                            const ConversionRequest &converter_request,
                            Callback *callback) const;

  virtual void LookupExact(StringPiece key,
                           const ConversionRequest &converter_request,
                           Callback *callback) const;

  virtual void LookupReverse(StringPiece str,
                             const ConversionRequest &converter_request,
                             Callback *callback) const;

  virtual void PopulateReverseLookupCache(StringPiece str) const;
  virtual void ClearReverseLookupCache() const;

 private:
  class ReverseLookupCache;
  class ReverseLookupIndex;
  struct PredictiveLookupSearchState;

  explicit SystemDictionary(const SystemDictionaryCodecInterface *codec,
                            const DictionaryFileCodecInterface *file_codec);
  bool OpenDictionaryFile(bool enable_reverse_lookup_index);

  void RegisterReverseLookupTokensForT13N(StringPiece value,
                                          Callback *callback) const;
  void RegisterReverseLookupTokensForValue(StringPiece value,
                                           Callback *callback) const;
  void ScanTokens(const std::set<int> &id_set, ReverseLookupCache *cache) const;
  void RegisterReverseLookupResults(const std::set<int> &id_set,
                                    const ReverseLookupCache &cache,
                                    Callback *callback) const;
  void InitReverseLookupIndex();

  Callback::ResultType LookupPrefixWithKeyExpansionImpl(
      const char *key,
      StringPiece encoded_key,
      const KeyExpansionTable &table,
      Callback *callback,
      storage::louds::LoudsTrie::Node node,
      StringPiece::size_type key_pos,
      bool is_expanded,
      char *actual_key_buffer,
      string *actual_prefix) const;

  void CollectPredictiveNodesInBfsOrder(
      StringPiece encoded_key,
      const KeyExpansionTable &table,
      size_t limit,
      std::vector<PredictiveLookupSearchState> *result) const;

  storage::louds::LoudsTrie key_trie_;
  storage::louds::LoudsTrie value_trie_;
  storage::louds::BitVectorBasedArray token_array_;
  const uint32 *frequent_pos_;
  const SystemDictionaryCodecInterface *codec_;
  KeyExpansionTable hiragana_expansion_table_;
  std::unique_ptr<DictionaryFile> dictionary_file_;
  mutable std::unique_ptr<ReverseLookupCache> reverse_lookup_cache_;
  std::unique_ptr<ReverseLookupIndex> reverse_lookup_index_;

  DISALLOW_COPY_AND_ASSIGN(SystemDictionary);
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_H_
