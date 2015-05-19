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

#include "dictionary/system/system_dictionary_builder.h"

#include <algorithm>
#include <climits>
#include <cstring>
#include <sstream>

#include "base/base.h"
#include "base/file_stream.h"
#include "base/hash_tables.h"
#include "base/util.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/file/codec_interface.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/rx/rbx_array_builder.h"
#include "dictionary/rx/rx_trie_builder.h"
#include "dictionary/system/codec.h"
#include "dictionary/system/codec_interface.h"
#include "dictionary/system/words_info.h"
#include "dictionary/text_dictionary_loader.h"

DEFINE_bool(preserve_intermediate_dictionary, false,
            "preserve inetemediate dictionary file.");
DEFINE_int32(min_key_length_to_use_small_cost_encoding, 6,
             "minimum key length to use 1 byte cost encoding.");

namespace mozc {
namespace {

void WriteSectionToFile(const DictionaryFileSection &section,
                        const string &filename);

}  // namespace

namespace dictionary {

struct TokenGreaterThan {
  inline bool operator()(const TokenInfo& lhs,
                         const TokenInfo& rhs) const {
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

SystemDictionaryBuilder::SystemDictionaryBuilder()
    : value_trie_builder_(new rx::RxTrieBuilder),
      key_trie_builder_(new rx::RxTrieBuilder),
      token_array_builder_(new rx::RbxArrayBuilder),
      codec_(SystemDictionaryCodecFactory::GetCodec()) {}

SystemDictionaryBuilder::~SystemDictionaryBuilder() {}

void SystemDictionaryBuilder::BuildFromFile(const string &input_file) {
  VLOG(1) << "load file: " << input_file;
  const POSMatcher pos_matcher;
  TextDictionaryLoader loader(pos_matcher);
  loader.Open(input_file.c_str());
  // Get all tokens
  vector<Token *> tokens;
  loader.CollectTokens(&tokens);

  VLOG(1) << tokens.size() << " tokens";
  BuildFromTokens(tokens);
}

void SystemDictionaryBuilder::BuildFromTokens(const vector<Token *> &tokens) {
  KeyInfoMap key_info_map;
  ReadTokens(tokens, &key_info_map);

  BuildFrequentPos(key_info_map);
  BuildValueTrie(key_info_map);
  BuildKeyTrie(key_info_map);

  SetIdForValue(&key_info_map);
  SetIdForKey(&key_info_map);
  SortTokenInfo(&key_info_map);
  SetCostType(&key_info_map);
  SetPosType(&key_info_map);
  SetValueType(&key_info_map);

  BuildTokenArray(key_info_map);
}

void SystemDictionaryBuilder::WriteToFile(const string &output_file) const {
  OutputFileStream ofs(output_file.c_str(), ios::binary | ios::out);
  WriteToStream(output_file, &ofs);
}

void SystemDictionaryBuilder::WriteToStream(
    const string &intermediate_output_file_base_path,
    ostream *output_stream) const {
  // Memory images of each section
  vector<DictionaryFileSection> sections;
  DictionaryFileCodecInterface *file_codec =
      DictionaryFileCodecFactory::GetCodec();
  DictionaryFileSection value_trie_section = {
    value_trie_builder_->GetImageBody(),
    value_trie_builder_->GetImageSize(),
    file_codec->GetSectionName(codec_->GetSectionNameForValue()),
  };
  sections.push_back(value_trie_section);
  DictionaryFileSection key_trie_section = {
    key_trie_builder_->GetImageBody(),
    key_trie_builder_->GetImageSize(),
    file_codec->GetSectionName(codec_->GetSectionNameForKey()),
  };
  sections.push_back(key_trie_section);
  DictionaryFileSection token_array_section = {
    token_array_builder_->GetImageBody(),
    token_array_builder_->GetImageSize(),
    file_codec->GetSectionName(codec_->GetSectionNameForTokens()),
  };
  sections.push_back(token_array_section);
  uint32 frequent_pos_array[256] = {0};
  for (map<uint32, int>::const_iterator i = frequent_pos_.begin();
       i != frequent_pos_.end(); ++i) {
    frequent_pos_array[i->second] = i->first;
  }
  DictionaryFileSection frequent_pos_section = {
    reinterpret_cast<const char *>(frequent_pos_array),
    sizeof frequent_pos_array,
    file_codec->GetSectionName(codec_->GetSectionNameForPos()),
  };
  sections.push_back(frequent_pos_section);

  if (FLAGS_preserve_intermediate_dictionary &&
      !intermediate_output_file_base_path.empty()) {
    // Write out intermediate results to files.
    const string &basepath = intermediate_output_file_base_path;
    LOG(INFO) << "Writing intermediate files.";
    WriteSectionToFile(value_trie_section, basepath + ".value");
    WriteSectionToFile(key_trie_section, basepath + ".key");
    WriteSectionToFile(token_array_section, basepath + ".tokens");
    WriteSectionToFile(frequent_pos_section, basepath + ".freq_pos");
  }

  LOG(INFO) << "Start writing dictionary file.";
  DictionaryFileCodecFactory::GetCodec()->WriteSections(sections,
                                                        output_stream);
  LOG(INFO) << "Start writing dictionary file... done.";
}

namespace {
uint32 GetCombinedPos(uint16 lid, uint16 rid) {
  return (lid << 16) | rid;
}

TokenInfo::ValueType GetValueType(const Token *token) {
  if (token->value == token->key) {
    return TokenInfo::AS_IS_HIRAGANA;
  }
  string katakana;
  Util::HiraganaToKatakana(token->key, &katakana);
  if (token->value == katakana) {
    return TokenInfo::AS_IS_KATAKANA;
  }
  return TokenInfo::DEFAULT_VALUE;
}

bool HaveHomonymsInSamePos(const KeyInfo &key_info) {
  hash_set<uint32> seen;
  for (size_t i = 0; i < key_info.tokens.size(); ++i) {
    const Token *token = key_info.tokens[i].token;
    const uint32 pos = GetCombinedPos(token->lid, token->rid);
    if (seen.find(pos) == seen.end()) {
      seen.insert(pos);
    } else {
      return true;
    }
  }
  return false;
}
}  // namespace

void SystemDictionaryBuilder::ReadTokens(const vector<Token *> &tokens,
                                         KeyInfoMap *key_info_map) const {
  for (vector<Token *>::const_iterator iter = tokens.begin();
       iter != tokens.end(); ++iter) {
    Token *token = *iter;
    CHECK(!token->key.empty()) << "empty key string in input";
    CHECK(!token->value.empty()) << "empty value string in input";
    KeyInfoMap::iterator i = key_info_map->find(token->key);
    if (i == key_info_map->end()) {
      pair<KeyInfoMap::iterator, bool> result =
          key_info_map->insert(make_pair(token->key, KeyInfo()));
      CHECK(result.second);
      i = result.first;
    }
    i->second.tokens.push_back(TokenInfo(token));
    i->second.tokens.back().value_type = GetValueType(token);
  }
}

void SystemDictionaryBuilder::BuildFrequentPos(
    const KeyInfoMap &key_info_map) {
  // Calculate frequency of each pos
  // TODO(toshiyuki): It might be better to count frequency
  // with considering same_as_prev_pos.
  map<uint32, int> pos_map;
  for (KeyInfoMap::const_iterator itr = key_info_map.begin();
       itr != key_info_map.end(); ++itr) {
    const KeyInfo &key_info = itr->second;
    for (size_t i = 0; i < key_info.tokens.size(); ++i) {
      const Token *token = key_info.tokens[i].token;
      pos_map[GetCombinedPos(token->lid, token->rid)]++;
    }
  }

  // Get histgram of frequency
  map<int, int> freq_map;
  for (map<uint32, int>::const_iterator jt = pos_map.begin();
       jt != pos_map.end(); ++jt) {
    freq_map[jt->second]++;
  }
  // Compute the lower threshold of frequence
  int num_freq_pos = 0;
  int freq_threshold = INT_MAX;
  for (map<int, int>::reverse_iterator kt = freq_map.rbegin();
       kt != freq_map.rend(); ++kt) {
    if (num_freq_pos + kt->second > 255) {
      break;
    }
    freq_threshold = kt->first;
    num_freq_pos += kt->second;
  }

  // Collect high frequent pos
  VLOG(1) << "num_freq_pos" << num_freq_pos;
  VLOG(1) << "Pos threshold=" << freq_threshold;
  int freq_pos_idx = 0;
  int num_tokens = 0;
  map<uint32, int>::iterator lt;
  for (lt = pos_map.begin(); lt != pos_map.end(); ++lt) {
    if (lt->second >= freq_threshold) {
      frequent_pos_[lt->first] = freq_pos_idx;
      freq_pos_idx++;
      num_tokens += lt->second;
    }
  }
  CHECK(freq_pos_idx == num_freq_pos)
      << "inconsistent result to find frequent pos";
  VLOG(1) << freq_pos_idx << " high frequent Pos has "
          << num_tokens << " tokens";
}


void SystemDictionaryBuilder::BuildValueTrie(const KeyInfoMap &key_info_map) {
  for (KeyInfoMap::const_iterator itr = key_info_map.begin();
       itr != key_info_map.end(); ++itr) {
    const KeyInfo &key_info = itr->second;
    for (size_t i = 0; i < key_info.tokens.size(); ++i) {
      const TokenInfo &token_info = key_info.tokens[i];
      if (token_info.value_type == TokenInfo::AS_IS_HIRAGANA ||
          token_info.value_type == TokenInfo::AS_IS_KATAKANA) {
        // These values will be stored in token array as flags
        continue;
      }
      string value_str;
      codec_->EncodeValue(token_info.token->value, &value_str);
      value_trie_builder_->AddKey(value_str);
    }
  }
  value_trie_builder_->Build();
}

void SystemDictionaryBuilder::SetIdForValue(KeyInfoMap *key_info_map) const {
  for (KeyInfoMap::iterator itr = key_info_map->begin();
       itr != key_info_map->end(); ++itr) {
    for (size_t i = 0; i < itr->second.tokens.size(); ++i) {
      TokenInfo *token_info = &(itr->second.tokens[i]);
      string value_str;
      codec_->EncodeValue(token_info->token->value, &value_str);
      token_info->id_in_value_trie =
          value_trie_builder_->GetIdFromKey(value_str);
    }
  }
}

void SystemDictionaryBuilder::SortTokenInfo(KeyInfoMap *key_info_map) const {
  for (KeyInfoMap::iterator itr = key_info_map->begin();
       itr != key_info_map->end(); ++itr) {
    KeyInfo *key_info = &(itr->second);
    sort(key_info->tokens.begin(), key_info->tokens.end(), TokenGreaterThan());
  }
}

void SystemDictionaryBuilder::SetCostType(KeyInfoMap *key_info_map) const {
  for (KeyInfoMap::iterator itr = key_info_map->begin();
       itr != key_info_map->end(); ++itr) {
    KeyInfo *key_info = &(itr->second);
    if (HaveHomonymsInSamePos(*key_info)) {
      continue;
    }
    for (size_t i = 0; i < key_info->tokens.size(); ++i) {
      TokenInfo *token_info = &key_info->tokens[i];
      const int key_len = Util::CharsLen(token_info->token->key);
      if (key_len >= FLAGS_min_key_length_to_use_small_cost_encoding) {
        token_info->cost_type = TokenInfo::CAN_USE_SMALL_ENCODING;
      }
    }
  }
}

void SystemDictionaryBuilder::SetPosType(KeyInfoMap *key_info_map) const {
  for (KeyInfoMap::iterator itr = key_info_map->begin();
       itr != key_info_map->end(); ++itr) {
    KeyInfo *key_info = &(itr->second);
    for (size_t i = 0; i < key_info->tokens.size(); ++i) {
      TokenInfo *token_info = &(key_info->tokens[i]);
      const uint32 pos = GetCombinedPos(token_info->token->lid,
                                        token_info->token->rid);
      map<uint32, int>::const_iterator itr = frequent_pos_.find(pos);
      if (itr != frequent_pos_.end()) {
        token_info->pos_type = TokenInfo::FREQUENT_POS;
        token_info->id_in_frequent_pos_map = itr->second;
      }
      if (i >= 1) {
        const TokenInfo &prev_token_info = key_info->tokens[i - 1];
        const uint32 prev_pos = GetCombinedPos(prev_token_info.token->lid,
                                               prev_token_info.token->rid);
        if (prev_pos == pos) {
          // we can overwrite FREQUENT_POS
          token_info->pos_type = TokenInfo::SAME_AS_PREV_POS;
        }
      }
    }
  }
}

void SystemDictionaryBuilder::SetValueType(KeyInfoMap *key_info_map) const {
  for (KeyInfoMap::iterator itr = key_info_map->begin();
       itr != key_info_map->end(); ++itr) {
    KeyInfo *key_info = &(itr->second);
    for (size_t i = 1; i < key_info->tokens.size(); ++i) {
      const TokenInfo *prev_token_info = &(key_info->tokens[i - 1]);
      TokenInfo *token_info = &(key_info->tokens[i]);
      if (token_info->value_type != TokenInfo::AS_IS_HIRAGANA &&
          token_info->value_type != TokenInfo::AS_IS_KATAKANA &&
          (token_info->token->value == prev_token_info->token->value)) {
        token_info->value_type = TokenInfo::SAME_AS_PREV_VALUE;
      }
    }
  }
}

void SystemDictionaryBuilder::BuildKeyTrie(const KeyInfoMap &key_info_map) {
  for (KeyInfoMap::const_iterator itr = key_info_map.begin();
       itr != key_info_map.end(); ++itr) {
    string key_str;
    codec_->EncodeKey(itr->first, &key_str);
    key_trie_builder_->AddKey(key_str);
  }
  key_trie_builder_->Build();
}

void SystemDictionaryBuilder::SetIdForKey(KeyInfoMap *key_info_map) const {
  for (KeyInfoMap::iterator itr = key_info_map->begin();
       itr != key_info_map->end(); ++itr) {
    string key_str;
    codec_->EncodeKey(itr->first, &key_str);
    KeyInfo *key_info = &(itr->second);
    key_info->id_in_key_trie =
        key_trie_builder_->GetIdFromKey(key_str);
  }
}

void SystemDictionaryBuilder::BuildTokenArray(const KeyInfoMap &key_info_map) {
  // map from trie id to key string.
  vector<string> id_map;
  id_map.resize(key_info_map.size());
  for (KeyInfoMap::const_iterator itr = key_info_map.begin();
       itr != key_info_map.end(); ++itr) {
    const KeyInfo &key_info = itr->second;
    const int id = key_info.id_in_key_trie;
    id_map[id] = itr->first;
  }

  for (size_t i = 0; i < id_map.size(); ++i) {
    KeyInfoMap::const_iterator itr = key_info_map.find(id_map[i]);
    CHECK(itr != key_info_map.end());
    const KeyInfo &key_info = itr->second;
    string tokens_str;
    codec_->EncodeTokens(key_info.tokens, &tokens_str);
    token_array_builder_->Push(tokens_str);
  }
  token_array_builder_->Push(string(1, codec_->GetTokensTerminationFlag()));
  token_array_builder_->Build();
}

}  // namespace dictionary

namespace {

void WriteSectionToFile(const DictionaryFileSection &section,
                        const string &filename) {
  OutputFileStream ofs(filename.c_str(), ios::binary | ios::out);
  ofs.write(section.ptr, section.len);
}

}  // namespace
}  // namespace mozc
