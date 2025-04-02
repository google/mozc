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

#ifndef MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_H_
#define MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/thread.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/file/codec_interface.h"
#include "dictionary/file/dictionary_file.h"
#include "dictionary/system/codec_interface.h"
#include "dictionary/system/key_expansion_table.h"
#include "request/conversion_request.h"
#include "storage/louds/bit_vector_based_array.h"
#include "storage/louds/louds_trie.h"

namespace mozc {
namespace dictionary {

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
  //   builder.SetCodec(nullptr);
  //   SystemDictionary *dictionary = builder.Build();
  //   ...
  //   delete dictionary;
  class Builder {
   public:
    // Creates Builder from filename
    explicit Builder(absl::string_view filename);
    // Creates Builder from image
    Builder(const char *ptr, int len);
    Builder(const Builder &) = delete;
    Builder &operator=(const Builder &) = delete;
    ~Builder() = default;

    // Sets options (default: NONE)
    Builder &SetOptions(Options options);

    // Sets codec (default: nullptr)
    // Uses default codec if this is nullptr
    // Doesn't take the ownership of |codec|.
    Builder &SetCodec(const SystemDictionaryCodecInterface *codec);

    // Builds and returns system dictionary.
    absl::StatusOr<std::unique_ptr<SystemDictionary>> Build();

   private:
    struct Specification {
      enum InputType {
        FILENAME,
        IMAGE,
      };

      Specification(InputType t, absl::string_view fn, const char *p, int l,
                    Options o, const SystemDictionaryCodecInterface *codec,
                    const DictionaryFileCodecInterface *file_codec)
          : type(t),
            filename(fn),
            ptr(p),
            len(l),
            options(o),
            codec(codec),
            file_codec(file_codec) {}

      InputType type;

      // For InputType::FILENAME
      const std::string filename;

      // For InputType::IMAGE
      const char *ptr;
      const int len;

      Options options;
      const SystemDictionaryCodecInterface *codec;
      const DictionaryFileCodecInterface *file_codec;
    };

    std::unique_ptr<Specification> spec_;
  };

  SystemDictionary(const SystemDictionary &) = delete;
  SystemDictionary &operator=(const SystemDictionary &) = delete;

  ~SystemDictionary() override;

  const storage::louds::LoudsTrie &value_trie() const { return value_trie_; }

  // Implementation of DictionaryInterface.
  bool HasKey(absl::string_view key) const override;
  bool HasValue(absl::string_view value) const override;

  void LookupPredictive(absl::string_view key,
                        const ConversionRequest &conversion_request,
                        Callback *callback) const override;

  void LookupPrefix(absl::string_view key,
                    const ConversionRequest &conversion_request,
                    Callback *callback) const override;

  void LookupExact(absl::string_view key,
                   const ConversionRequest &conversion_request,
                   Callback *callback) const override;

  void LookupReverse(absl::string_view str,
                     const ConversionRequest &conversion_request,
                     Callback *callback) const override;

  void PopulateReverseLookupCache(absl::string_view str) const override;
  void ClearReverseLookupCache() const override;

 private:
  class ReverseLookupCache;
  class ReverseLookupIndex;
  struct PredictiveLookupSearchState;

  SystemDictionary(const SystemDictionaryCodecInterface *codec,
                   const DictionaryFileCodecInterface *file_codec);

  bool OpenDictionaryFile(bool enable_reverse_lookup_index);

  void RegisterReverseLookupTokensForT13N(absl::string_view value,
                                          Callback *callback) const;
  void RegisterReverseLookupTokensForValue(absl::string_view value,
                                           Callback *callback) const;
  void ScanTokens(const absl::btree_set<int> &id_set,
                  ReverseLookupCache *cache) const;
  void RegisterReverseLookupResults(const absl::btree_set<int> &id_set,
                                    const ReverseLookupCache &cache,
                                    Callback *callback) const;
  void InitReverseLookupIndex();

  Callback::ResultType LookupPrefixWithKeyExpansionImpl(
      const char *key, absl::string_view encoded_key,
      const KeyExpansionTable &table, Callback *callback,
      storage::louds::LoudsTrie::Node node,
      absl::string_view::size_type key_pos, int num_expanded,
      char *actual_key_buffer, std::string *actual_prefix) const;

  void CollectPredictiveNodesInBfsOrder(
      absl::string_view encoded_key, const KeyExpansionTable &table,
      size_t limit, std::vector<PredictiveLookupSearchState> *result) const;

  storage::louds::LoudsTrie key_trie_;
  storage::louds::LoudsTrie value_trie_;
  storage::louds::BitVectorBasedArray token_array_;
  const uint32_t *frequent_pos_;
  const SystemDictionaryCodecInterface *codec_;
  KeyExpansionTable hiragana_expansion_table_;
  std::unique_ptr<DictionaryFile> dictionary_file_;
  mutable AtomicSharedPtr<ReverseLookupCache> reverse_lookup_cache_;
  std::unique_ptr<ReverseLookupIndex> reverse_lookup_index_;
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_H_
