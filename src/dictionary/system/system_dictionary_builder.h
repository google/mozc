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

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/file/codec_factory.h"
#include "dictionary/file/codec_interface.h"
#include "dictionary/system/codec_interface.h"
#include "dictionary/system/words_info.h"
#include "storage/louds/bit_vector_based_array_builder.h"
#include "storage/louds/louds_trie_builder.h"

namespace mozc {
namespace dictionary {

struct Token;

class SystemDictionaryBuilder final {
 public:
  // Represents words info for a certain key(=reading).
  struct KeyInfo {
    // id of the key(=reading) string in key trie
    int id_in_key_trie = -1;
    std::string key;
    std::vector<TokenInfo> tokens;
  };

  SystemDictionaryBuilder() = default;
  // This class does not have the ownership of |codec|.
  SystemDictionaryBuilder(const SystemDictionaryCodecInterface* codec,
                          const DictionaryFileCodecInterface* file_codec)
      : codec_(codec), file_codec_(file_codec) {}

  SystemDictionaryBuilder(const SystemDictionaryBuilder&) = delete;
  SystemDictionaryBuilder& operator=(const SystemDictionaryBuilder&) = delete;

  void BuildFromTokens(absl::Span<Token* const> tokens) {
    BuildFromTokensInternal(std::vector<Token*>(tokens.begin(), tokens.end()));
  }
  void BuildFromTokens(absl::Span<const std::unique_ptr<Token>> token);

  void WriteToFile(const std::string& output_file) const;
  void WriteToStream(absl::string_view intermediate_output_file_base_path,
                     std::ostream* output_stream) const;

 private:
  using KeyInfoList = std::deque<KeyInfo>;

  // Build process needs to make a copy of input token pointers. Therefore, make
  // a copy at call site, which is helpful to efficiently support the above two
  // versions of BuildFromTokens() without extra copying.
  void BuildFromTokensInternal(std::vector<Token*> tokens);

  KeyInfoList ReadTokens(std::vector<Token*> tokens) const;

  void BuildFrequentPos(const KeyInfoList& key_info_list);
  void BuildValueTrie(const KeyInfoList& key_info_list);
  void BuildKeyTrie(const KeyInfoList& key_info_list);
  void BuildTokenArray(const KeyInfoList& key_info_list);

  void SetIdForValue(KeyInfoList* key_info_list) const;
  void SetIdForKey(KeyInfoList* key_info_list) const;
  void SortTokenInfo(KeyInfoList* key_info_list) const;

  void SetCostType(KeyInfoList* key_info_list) const;
  void SetPosType(KeyInfoList* key_info_list) const;
  void SetValueType(KeyInfoList* key_info_list) const;

  storage::louds::LoudsTrieBuilder value_trie_builder_;
  storage::louds::LoudsTrieBuilder key_trie_builder_;
  storage::louds::BitVectorBasedArrayBuilder token_array_builder_;

  // mapping from {left_id, right_id} to POS index (0--255)
  std::map<uint32_t, int> frequent_pos_;

  const SystemDictionaryCodecInterface* codec_ =
      SystemDictionaryCodecFactory::GetCodec();
  const DictionaryFileCodecInterface* file_codec_ =
      DictionaryFileCodecFactory::GetCodec();
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_BUILDER_H_
