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
#include "base/logging.h"
#include "base/util.h"
#include "data_manager/user_pos_manager.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/file/codec_interface.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/system/codec.h"
#include "dictionary/system/codec_interface.h"
#include "dictionary/system/words_info.h"
#include "dictionary/text_dictionary_loader.h"

#ifdef MOZC_USE_MOZC_LOUDS
#include "storage/louds/bit_vector_based_array_builder.h"
#include "storage/louds/louds_trie_builder.h"
#else
#include "dictionary/rx/rbx_array_builder.h"
#include "dictionary/rx/rx_trie_builder.h"
#endif  // MOZC_USE_MOZC_LOUDS

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
    : value_trie_builder_(new TrieBuilder),
      key_trie_builder_(new TrieBuilder),
      token_array_builder_(new ArrayBuilder),
      codec_(SystemDictionaryCodecFactory::GetCodec()) {}

SystemDictionaryBuilder::~SystemDictionaryBuilder() {}

void SystemDictionaryBuilder::BuildFromFile(const string &input_file) {
  VLOG(1) << "load file: " << input_file;
  const POSMatcher *pos_matcher =
      UserPosManager::GetUserPosManager()->GetPOSMatcher();
  TextDictionaryLoader loader(*pos_matcher);
  loader.Open(input_file.c_str());
  // Get all tokens
  vector<Token *> tokens;
  loader.CollectTokens(&tokens);

  VLOG(1) << tokens.size() << " tokens";
  BuildFromTokens(tokens);
}

void SystemDictionaryBuilder::BuildFromTokens(const vector<Token *> &tokens) {
  KeyInfoList key_info_list;
  ReadTokens(tokens, &key_info_list);

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
#ifdef MOZC_USE_MOZC_LOUDS
  DictionaryFileSection value_trie_section(
    value_trie_builder_->image().data(),
    value_trie_builder_->image().size(),
    file_codec->GetSectionName(codec_->GetSectionNameForValue()));
#else
  DictionaryFileSection value_trie_section(
    value_trie_builder_->GetImageBody(),
    value_trie_builder_->GetImageSize(),
    file_codec->GetSectionName(codec_->GetSectionNameForValue()));
#endif  // MOZC_USE_MOZC_LOUDS
  sections.push_back(value_trie_section);

#ifdef MOZC_USE_MOZC_LOUDS
  DictionaryFileSection key_trie_section(
    key_trie_builder_->image().data(),
    key_trie_builder_->image().size(),
    file_codec->GetSectionName(codec_->GetSectionNameForKey()));
#else
  DictionaryFileSection key_trie_section(
    key_trie_builder_->GetImageBody(),
    key_trie_builder_->GetImageSize(),
    file_codec->GetSectionName(codec_->GetSectionNameForKey()));
#endif  // MOZC_USE_MOZC_LOUDS
  sections.push_back(key_trie_section);

#ifdef MOZC_USE_MOZC_LOUDS
  DictionaryFileSection token_array_section(
    token_array_builder_->image().data(),
    token_array_builder_->image().size(),
    file_codec->GetSectionName(codec_->GetSectionNameForTokens()));
#else
  DictionaryFileSection token_array_section(
    token_array_builder_->GetImageBody(),
    token_array_builder_->GetImageSize(),
    file_codec->GetSectionName(codec_->GetSectionNameForTokens()));
#endif  // MOZC_USE_MOZC_LOUDS

  sections.push_back(token_array_section);
  uint32 frequent_pos_array[256] = {0};
  for (map<uint32, int>::const_iterator i = frequent_pos_.begin();
       i != frequent_pos_.end(); ++i) {
    frequent_pos_array[i->second] = i->first;
  }
  DictionaryFileSection frequent_pos_section(
    reinterpret_cast<const char *>(frequent_pos_array),
    sizeof frequent_pos_array,
    file_codec->GetSectionName(codec_->GetSectionNameForPos()));
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

bool HasHomonymsInSamePos(
    const SystemDictionaryBuilder::KeyInfo &key_info) {
  // Early exit path mainly for performance.
  if (key_info.tokens.size() == 1) {
    return false;
  }

  hash_set<uint32> seen;
  for (size_t i = 0; i < key_info.tokens.size(); ++i) {
    const Token *token = key_info.tokens[i].token;
    const uint32 pos = GetCombinedPos(token->lid, token->rid);
    if (!seen.insert(pos).second) {
      // Insertion failed, which means we already have |pos|.
      return true;
    }
  }
  return false;
}

struct TokenPtrLessThan {
  inline bool operator()(const Token* lhs, const Token* rhs) const {
    return lhs->key < rhs->key;
  }
};

}  // namespace

void SystemDictionaryBuilder::ReadTokens(const vector<Token *> &tokens,
                                         KeyInfoList *key_info_list) const {
  // Create KeyInfoList in two steps.
  // 1. Create an array of Token with (stably) sorting Token::key.
  //    [Token 1(key:aaa)][Token 2(key:aaa)][Token 3(key:abc)][...]
  // 2. Group Token(s) by Token::Key and convert them into KeyInfo.
  //    [KeyInfo(key:aaa)[Token 1][Token 2]][KeyInfo(key:abc)[Token 3]][...]

  // Step 1.
  typedef vector<Token *> ReduceBuffer;
  ReduceBuffer reduce_buffer;
  reduce_buffer.reserve(tokens.size());
  for (vector<Token *>::const_iterator iter = tokens.begin();
       iter != tokens.end(); ++iter) {
    Token *token = *iter;
    CHECK(!token->key.empty()) << "empty key string in input";
    CHECK(!token->value.empty()) << "empty value string in input";
    reduce_buffer.push_back(token);
  }
  stable_sort(reduce_buffer.begin(), reduce_buffer.end(), TokenPtrLessThan());

  // Step 2.
  key_info_list->clear();
  if (reduce_buffer.size() == 0) {
    return;
  }
  KeyInfo last_key_info;
  last_key_info.key = reduce_buffer.front()->key;
  for (ReduceBuffer::const_iterator iter = reduce_buffer.begin();
       iter != reduce_buffer.end(); ++iter) {
    Token *token = *iter;
    if (last_key_info.key != token->key) {
      key_info_list->push_back(last_key_info);
      last_key_info = KeyInfo();
      last_key_info.key = token->key;
    }
    last_key_info.tokens.push_back(TokenInfo(token));
    last_key_info.tokens.back().value_type = GetValueType(token);
  }
  key_info_list->push_back(last_key_info);
}

void SystemDictionaryBuilder::BuildFrequentPos(
    const KeyInfoList &key_info_list) {
  // Calculate frequency of each pos
  // TODO(toshiyuki): It might be better to count frequency
  // with considering same_as_prev_pos.
  map<uint32, int> pos_map;
  for (KeyInfoList::const_iterator itr = key_info_list.begin();
       itr != key_info_list.end(); ++itr) {
    const KeyInfo &key_info = *itr;
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


void SystemDictionaryBuilder::BuildValueTrie(const KeyInfoList &key_info_list) {
  for (KeyInfoList::const_iterator itr = key_info_list.begin();
       itr != key_info_list.end(); ++itr) {
    const KeyInfo &key_info = *itr;
    for (size_t i = 0; i < key_info.tokens.size(); ++i) {
      const TokenInfo &token_info = key_info.tokens[i];
      if (token_info.value_type == TokenInfo::AS_IS_HIRAGANA ||
          token_info.value_type == TokenInfo::AS_IS_KATAKANA) {
        // These values will be stored in token array as flags
        continue;
      }
      string value_str;
      codec_->EncodeValue(token_info.token->value, &value_str);
#ifdef MOZC_USE_MOZC_LOUDS
      value_trie_builder_->Add(value_str);
#else
      value_trie_builder_->AddKey(value_str);
#endif  // MOZC_USE_MOZC_LOUDS
    }
  }
  value_trie_builder_->Build();
}

void SystemDictionaryBuilder::SetIdForValue(KeyInfoList *key_info_list) const {
  for (KeyInfoList::iterator itr = key_info_list->begin();
       itr != key_info_list->end(); ++itr) {
    for (size_t i = 0; i < itr->tokens.size(); ++i) {
      TokenInfo *token_info = &(itr->tokens[i]);
      string value_str;
      codec_->EncodeValue(token_info->token->value, &value_str);
#ifdef MOZC_USE_MOZC_LOUDS
      token_info->id_in_value_trie =
          value_trie_builder_->GetId(value_str);
#else
      token_info->id_in_value_trie =
          value_trie_builder_->GetIdFromKey(value_str);
#endif  // MOZC_USE_MOZC_LOUDS
    }
  }
}

void SystemDictionaryBuilder::SortTokenInfo(KeyInfoList *key_info_list) const {
  for (KeyInfoList::iterator itr = key_info_list->begin();
       itr != key_info_list->end(); ++itr) {
    KeyInfo *key_info = &(*itr);
    sort(key_info->tokens.begin(), key_info->tokens.end(), TokenGreaterThan());
  }
}

void SystemDictionaryBuilder::SetCostType(KeyInfoList *key_info_list) const {
  for (KeyInfoList::iterator itr = key_info_list->begin();
       itr != key_info_list->end(); ++itr) {
    KeyInfo *key_info = &(*itr);
    if (HasHomonymsInSamePos(*key_info)) {
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

void SystemDictionaryBuilder::SetPosType(KeyInfoList *key_info_list) const {
  for (KeyInfoList::iterator itr = key_info_list->begin();
       itr != key_info_list->end(); ++itr) {
    KeyInfo *key_info = &(*itr);
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

void SystemDictionaryBuilder::SetValueType(KeyInfoList *key_info_list) const {
  for (KeyInfoList::iterator itr = key_info_list->begin();
       itr != key_info_list->end(); ++itr) {
    KeyInfo *key_info = &(*itr);
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

void SystemDictionaryBuilder::BuildKeyTrie(const KeyInfoList &key_info_list) {
  for (KeyInfoList::const_iterator itr = key_info_list.begin();
       itr != key_info_list.end(); ++itr) {
    string key_str;
    codec_->EncodeKey(itr->key, &key_str);
#ifdef MOZC_USE_MOZC_LOUDS
    key_trie_builder_->Add(key_str);
#else
    key_trie_builder_->AddKey(key_str);
#endif  // MOZC_USE_MOZC_LOUDS
  }
  key_trie_builder_->Build();
}

void SystemDictionaryBuilder::SetIdForKey(KeyInfoList *key_info_list) const {
  for (KeyInfoList::iterator itr = key_info_list->begin();
       itr != key_info_list->end(); ++itr) {
    KeyInfo *key_info = &(*itr);
    string key_str;
    codec_->EncodeKey(key_info->key, &key_str);
#ifdef MOZC_USE_MOZC_LOUDS
    key_info->id_in_key_trie =
        key_trie_builder_->GetId(key_str);
#else
    key_info->id_in_key_trie =
        key_trie_builder_->GetIdFromKey(key_str);
#endif  // MOZC_USE_MOZC_LOUDS
  }
}

void SystemDictionaryBuilder::BuildTokenArray(
    const KeyInfoList &key_info_list) {
  // Here we make a reverse lookup table as follows:
  //   |key_info_list[X].id_in_key_trie| -> |key_info_list[X]|
  // assuming |key_info_list[X].id_in_key_trie| is unique and successive.
  {
    vector<const KeyInfo *> id_to_keyinfo_table;
    id_to_keyinfo_table.resize(key_info_list.size());
    for (KeyInfoList::const_iterator itr = key_info_list.begin();
         itr != key_info_list.end(); ++itr) {
      const KeyInfo &key_info = *itr;
      const int id = key_info.id_in_key_trie;
      id_to_keyinfo_table[id] = &key_info;
    }

    for (size_t i = 0; i < id_to_keyinfo_table.size(); ++i) {
      const KeyInfo &key_info = *id_to_keyinfo_table[i];
      string tokens_str;
      codec_->EncodeTokens(key_info.tokens, &tokens_str);
#ifdef MOZC_USE_MOZC_LOUDS
      token_array_builder_->Add(tokens_str);
#else
      token_array_builder_->Push(tokens_str);
#endif  // MOZC_USE_MOZC_LOUDS
    }
  }

#ifdef MOZC_USE_MOZC_LOUDS
  token_array_builder_->Add(string(1, codec_->GetTokensTerminationFlag()));
#else
  token_array_builder_->Push(string(1, codec_->GetTokensTerminationFlag()));
#endif
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
