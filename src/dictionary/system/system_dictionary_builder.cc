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

#include "dictionary/system/system_dictionary_builder.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstring>
#include <ios>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/japanese_util.h"
#include "base/util.h"
#include "base/vlog.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/file/codec_interface.h"
#include "dictionary/file/section.h"
#include "dictionary/system/codec_interface.h"
#include "dictionary/system/words_info.h"
#include "storage/louds/bit_vector_based_array_builder.h"
#include "storage/louds/louds_trie_builder.h"

ABSL_FLAG(bool, preserve_intermediate_dictionary, false,
          "preserve inetemediate dictionary file.");
ABSL_FLAG(int32_t, min_key_length_to_use_small_cost_encoding, 6,
          "minimum key length to use 1 byte cost encoding.");

namespace mozc {
namespace dictionary {
namespace {

struct TokenGreaterThan {
  bool operator()(const TokenInfo& lhs, const TokenInfo& rhs) const {
    if (lhs.token->lid != rhs.token->lid) {
      return lhs.token->lid > rhs.token->lid;
    }
    if (lhs.token->rid != rhs.token->rid) {
      return lhs.token->rid > rhs.token->rid;
    }
    if (lhs.id_in_value_trie != rhs.id_in_value_trie) {
      return lhs.id_in_value_trie < rhs.id_in_value_trie;
    }
    return lhs.token->attributes < rhs.token->attributes;
  }
};

void WriteSectionToFile(const DictionaryFileSection& section,
                        const std::string& filename) {
  if (absl::Status s = FileUtil::SetContents(
          filename, absl::string_view(section.ptr, section.len));
      !s.ok()) {
    LOG(ERROR) << "Cannot write a section to " << filename;
  }
}

}  // namespace

void SystemDictionaryBuilder::BuildFromTokens(
    absl::Span<const std::unique_ptr<Token>> tokens) {
  std::vector<Token*> ptrs;
  ptrs.reserve(tokens.size());
  for (const std::unique_ptr<Token>& token : tokens) {
    ptrs.push_back(token.get());
  }
  BuildFromTokensInternal(std::move(ptrs));
}

void SystemDictionaryBuilder::BuildFromTokensInternal(
    std::vector<Token*> tokens) {
  KeyInfoList key_info_list = ReadTokens(std::move(tokens));

  BuildFrequentPos(key_info_list);
  BuildValueTrie(key_info_list);
  BuildKeyTrie(key_info_list);

  SetIdForValue(&key_info_list);
  SetIdForKey(&key_info_list);
  SortTokenInfo(&key_info_list);
  SetCostType(&key_info_list);
  SetPosType(&key_info_list);
  SetValueType(&key_info_list);

  BuildTokenArray(key_info_list);
}

void SystemDictionaryBuilder::WriteToFile(
    const std::string& output_file) const {
  OutputFileStream ofs(output_file, std::ios::binary | std::ios::out);
  WriteToStream(output_file, &ofs);
}

void SystemDictionaryBuilder::WriteToStream(
    const absl::string_view intermediate_output_file_base_path,
    std::ostream* output_stream) const {
  // Memory images of each section
  std::vector<DictionaryFileSection> sections;
  DictionaryFileSection value_trie_section(
      value_trie_builder_.image().data(), value_trie_builder_.image().size(),
      file_codec_->GetSectionName(codec_->GetSectionNameForValue()));
  sections.push_back(value_trie_section);

  DictionaryFileSection key_trie_section(
      key_trie_builder_.image().data(), key_trie_builder_.image().size(),
      file_codec_->GetSectionName(codec_->GetSectionNameForKey()));
  sections.push_back(key_trie_section);

  DictionaryFileSection token_array_section(
      token_array_builder_.image().data(), token_array_builder_.image().size(),
      file_codec_->GetSectionName(codec_->GetSectionNameForTokens()));

  sections.push_back(token_array_section);
  uint32_t frequent_pos_array[256] = {0};
  for (std::map<uint32_t, int>::const_iterator i = frequent_pos_.begin();
       i != frequent_pos_.end(); ++i) {
    frequent_pos_array[i->second] = i->first;
  }
  DictionaryFileSection frequent_pos_section(
      reinterpret_cast<const char*>(frequent_pos_array),
      sizeof frequent_pos_array,
      file_codec_->GetSectionName(codec_->GetSectionNameForPos()));
  sections.push_back(frequent_pos_section);

  if (absl::GetFlag(FLAGS_preserve_intermediate_dictionary) &&
      !intermediate_output_file_base_path.empty()) {
    // Write out intermediate results to files.
    const absl::string_view basepath = intermediate_output_file_base_path;
    LOG(INFO) << "Writing intermediate files.";
    WriteSectionToFile(value_trie_section, absl::StrCat(basepath, ".value"));
    WriteSectionToFile(key_trie_section, absl::StrCat(basepath, ".key"));
    WriteSectionToFile(token_array_section, absl::StrCat(basepath, ".tokens"));
    WriteSectionToFile(frequent_pos_section,
                       absl::StrCat(basepath, ".freq_pos"));
  }

  LOG(INFO) << "Start writing dictionary file.";
  file_codec_->WriteSections(sections, output_stream);
  LOG(INFO) << "Start writing dictionary file... done.";
}

namespace {

uint32_t GetCombinedPos(uint16_t lid, uint16_t rid) {
  return (lid << 16) | rid;
}

TokenInfo::ValueType GetValueType(const Token* token) {
  if (token->value == token->key) {
    return TokenInfo::AS_IS_HIRAGANA;
  }
  std::string katakana = japanese_util::HiraganaToKatakana(token->key);
  if (token->value == katakana) {
    return TokenInfo::AS_IS_KATAKANA;
  }
  return TokenInfo::DEFAULT_VALUE;
}

bool HasHomonymsInSamePos(const SystemDictionaryBuilder::KeyInfo& key_info) {
  // Early exit path mainly for performance.
  if (key_info.tokens.size() == 1) {
    return false;
  }

  absl::flat_hash_set<uint32_t> seen;
  for (const TokenInfo& token_info : key_info.tokens) {
    const Token* token = token_info.token;
    const uint32_t pos = GetCombinedPos(token->lid, token->rid);
    if (!seen.insert(pos).second) {
      // Insertion failed, which means we already have |pos|.
      return true;
    }
  }
  return false;
}

bool HasHeterophones(
    const SystemDictionaryBuilder::KeyInfo& key_info,
    const absl::flat_hash_set<absl::string_view>& heteronym_values) {
  for (const TokenInfo& token_info : key_info.tokens) {
    if (heteronym_values.contains(token_info.token->value)) {
      return true;
    }
  }
  return false;
}

}  // namespace

SystemDictionaryBuilder::KeyInfoList SystemDictionaryBuilder::ReadTokens(
    std::vector<Token*> tokens) const {
  // Check if all the key values are nonempty.
  for (const Token* token : tokens) {
    CHECK(!token->key.empty()) << "empty key string in input";
    CHECK(!token->value.empty()) << "empty value string in input";
  }

  // Create KeyInfoList in two steps.
  // 1. Create an array of Token with (stably) sorting Token::key.
  //    [Token 1(key:aaa)][Token 2(key:aaa)][Token 3(key:abc)][...]
  // 2. Group Token(s) by Token::Key and convert them into KeyInfo.
  //    [KeyInfo(key:aaa)[Token 1][Token 2]][KeyInfo(key:abc)[Token 3]][...]

  // Step 1.
  std::stable_sort(
      tokens.begin(), tokens.end(),
      [](const Token* l, const Token* r) { return l->key < r->key; });

  // Step 2.
  KeyInfoList key_info_list;
  if (tokens.empty()) {
    return key_info_list;
  }
  KeyInfo last_key_info;
  last_key_info.key = tokens.front()->key;
  for (Token* token : tokens) {
    if (last_key_info.key != token->key) {
      key_info_list.push_back(std::move(last_key_info));
      last_key_info = KeyInfo();
      last_key_info.key = token->key;
    }
    last_key_info.tokens.emplace_back(token);
    last_key_info.tokens.back().value_type = GetValueType(token);
  }
  key_info_list.push_back(std::move(last_key_info));
  return key_info_list;
}

void SystemDictionaryBuilder::BuildFrequentPos(
    const KeyInfoList& key_info_list) {
  // Calculate the frequency of each POS.
  // TODO(toshiyuki): It might be better to count frequency
  // with considering same_as_prev_pos.
  absl::btree_map<uint32_t, int> pos_map;
  for (const KeyInfo& key_info : key_info_list) {
    for (const TokenInfo& token_info : key_info.tokens) {
      const Token* token = token_info.token;
      pos_map[GetCombinedPos(token->lid, token->rid)]++;
    }
  }

  // Get histgram of frequency.
  absl::btree_map<int, int> freq_map;
  for (auto [unused, freq] : pos_map) {
    freq_map[freq]++;
  }

  // Compute the lower threshold of frequency.
  int num_freq_pos = 0;
  int freq_threshold = INT_MAX;
  for (auto iter = freq_map.crbegin(); iter != freq_map.crend(); ++iter) {
    if (num_freq_pos + iter->second > 255) {
      break;
    }
    freq_threshold = iter->first;
    num_freq_pos += iter->second;
  }

  // Collect high frequent pos.
  MOZC_VLOG(1) << "num_freq_pos" << num_freq_pos;
  MOZC_VLOG(1) << "Pos threshold=" << freq_threshold;
  int freq_pos_idx = 0;
  int num_tokens = 0;
  for (auto [combined_pos, freq] : pos_map) {
    if (freq >= freq_threshold) {
      frequent_pos_[combined_pos] = freq_pos_idx;
      freq_pos_idx++;
      num_tokens += freq;
    }
  }
  CHECK(freq_pos_idx == num_freq_pos)
      << "inconsistent result to find frequent pos";
  MOZC_VLOG(1) << freq_pos_idx << " high frequent Pos has " << num_tokens
               << " tokens";
}

void SystemDictionaryBuilder::BuildValueTrie(const KeyInfoList& key_info_list) {
  for (const KeyInfo& key_info : key_info_list) {
    for (const TokenInfo& token_info : key_info.tokens) {
      if (token_info.value_type == TokenInfo::AS_IS_HIRAGANA ||
          token_info.value_type == TokenInfo::AS_IS_KATAKANA) {
        // These values will be stored in token array as flags
        continue;
      }
      std::string value_str;
      codec_->EncodeValue(token_info.token->value, &value_str);
      value_trie_builder_.Add(value_str);
    }
  }
  value_trie_builder_.Build();
}

void SystemDictionaryBuilder::SetIdForValue(KeyInfoList* key_info_list) const {
  for (KeyInfo& key_info : *key_info_list) {
    for (TokenInfo& token_info : key_info.tokens) {
      std::string value_str;
      codec_->EncodeValue(token_info.token->value, &value_str);
      token_info.id_in_value_trie = value_trie_builder_.GetId(value_str);
    }
  }
}

void SystemDictionaryBuilder::SortTokenInfo(KeyInfoList* key_info_list) const {
  for (KeyInfo& key_info : *key_info_list) {
    std::stable_sort(key_info.tokens.begin(), key_info.tokens.end(),
                     TokenGreaterThan());
  }
}

void SystemDictionaryBuilder::SetCostType(KeyInfoList* key_info_list) const {
  // Values that have multiple keys.
  absl::flat_hash_set<absl::string_view> heterophone_values;
  {
    // value → reading
    absl::flat_hash_map<absl::string_view, absl::string_view> seen_reading_map;
    for (const KeyInfo& key_info : *key_info_list) {
      for (const TokenInfo& token_info : key_info.tokens) {
        const Token* token = token_info.token;
        if (heterophone_values.contains(token->value)) {
          continue;
        }
        auto it = seen_reading_map.find(token->value);
        if (it == seen_reading_map.end()) {
          seen_reading_map[token->value] = token->key;
          continue;
        }
        if (it->second == token->key) {
          continue;
        }
        heterophone_values.insert(token->value);
      }
    }
  }

  const int min_key_len =
      absl::GetFlag(FLAGS_min_key_length_to_use_small_cost_encoding);
  for (KeyInfo& key_info : *key_info_list) {
    if (Util::CharsLen(key_info.key) < min_key_len) {
      // Do not use small cost encoding for short keys.
      continue;
    }
    if (HasHomonymsInSamePos(key_info)) {
      continue;
    }
    if (HasHeterophones(key_info, heterophone_values)) {
      // We want to keep the cost order for LookupReverse().
      continue;
    }

    for (TokenInfo& token_info : key_info.tokens) {
      if (token_info.token->cost < 0x100) {
        // Small cost encoding ignores lower 8 bits.
        continue;
      }
      token_info.cost_type = TokenInfo::CAN_USE_SMALL_ENCODING;
    }
  }
}

void SystemDictionaryBuilder::SetPosType(KeyInfoList* key_info_list) const {
  for (KeyInfo& key_info : *key_info_list) {
    for (size_t i = 0; i < key_info.tokens.size(); ++i) {
      TokenInfo* token_info = &(key_info.tokens[i]);
      const uint32_t pos =
          GetCombinedPos(token_info->token->lid, token_info->token->rid);
      if (auto iter = frequent_pos_.find(pos); iter != frequent_pos_.end()) {
        token_info->pos_type = TokenInfo::FREQUENT_POS;
        token_info->id_in_frequent_pos_map = iter->second;
      }
      if (i >= 1) {
        const TokenInfo& prev_token_info = key_info.tokens[i - 1];
        const uint32_t prev_pos = GetCombinedPos(prev_token_info.token->lid,
                                                 prev_token_info.token->rid);
        if (prev_pos == pos) {
          // we can overwrite FREQUENT_POS
          token_info->pos_type = TokenInfo::SAME_AS_PREV_POS;
        }
      }
    }
  }
}

void SystemDictionaryBuilder::SetValueType(KeyInfoList* key_info_list) const {
  for (KeyInfo& key_info : *key_info_list) {
    for (size_t i = 1; i < key_info.tokens.size(); ++i) {
      const TokenInfo& prev_token_info = key_info.tokens[i - 1];
      TokenInfo* token_info = &(key_info.tokens[i]);
      if (token_info->value_type != TokenInfo::AS_IS_HIRAGANA &&
          token_info->value_type != TokenInfo::AS_IS_KATAKANA &&
          (token_info->token->value == prev_token_info.token->value)) {
        token_info->value_type = TokenInfo::SAME_AS_PREV_VALUE;
      }
    }
  }
}

void SystemDictionaryBuilder::BuildKeyTrie(const KeyInfoList& key_info_list) {
  for (const KeyInfo& key_info : key_info_list) {
    std::string key_str;
    codec_->EncodeKey(key_info.key, &key_str);
    key_trie_builder_.Add(key_str);
  }
  key_trie_builder_.Build();
}

void SystemDictionaryBuilder::SetIdForKey(KeyInfoList* key_info_list) const {
  for (KeyInfo& key_info : *key_info_list) {
    std::string key_str;
    codec_->EncodeKey(key_info.key, &key_str);
    key_info.id_in_key_trie = key_trie_builder_.GetId(key_str);
  }
}

void SystemDictionaryBuilder::BuildTokenArray(
    const KeyInfoList& key_info_list) {
  // Here we make a reverse lookup table as follows:
  //   |key_info_list[X].id_in_key_trie| -> |key_info_list[X]|
  // assuming |key_info_list[X].id_in_key_trie| is unique and successive.
  {
    std::vector<const KeyInfo*> id_to_keyinfo_table(key_info_list.size());
    for (const KeyInfo& key_info : key_info_list) {
      const int id = key_info.id_in_key_trie;
      id_to_keyinfo_table[id] = &key_info;
    }

    for (const KeyInfo* key_info : id_to_keyinfo_table) {
      std::string tokens_str;
      codec_->EncodeTokens(key_info->tokens, &tokens_str);
      token_array_builder_.Add(tokens_str);
    }
  }

  token_array_builder_.Add(std::string(1, codec_->GetTokensTerminationFlag()));
  token_array_builder_.Build();
}

}  // namespace dictionary
}  // namespace mozc
