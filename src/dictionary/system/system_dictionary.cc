// Copyright 2010-2011, Google Inc.
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

// System dictionary maintains following sections
//  (1) Key trie
//  (2) Value trie
//  (3) Token array
//  (4) Table for high frequent POS(left/right ID)

#include "dictionary/system/system_dictionary.h"

#include <algorithm>
#include <climits>
#include <map>
#include <string>

#include "base/base.h"
#include "base/singleton.h"
#include "base/util.h"
#include "base/flags.h"
#include "base/mmap.h"
#include "converter/node.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/file/dictionary_file.h"
#include "dictionary/rx/rx_trie.h"
#include "dictionary/rx/rbx_array.h"
#include "dictionary/system/codec_interface.h"
#include "dictionary/system/words_info.h"
#include "dictionary/text_dictionary_loader.h"

namespace mozc {
namespace {

// rbx_array default setting
const int kMinRbxBlobSize = 4;
const char *kReverseLookupCache = "reverse_lookup_cache";

class ReverseLookupCache : public NodeAllocatorData::Data {
 public:
  multimap<int, SystemDictionary::ReverseLookupResult> results;
};

// Append node list |rhs| to |lhs|
Node *AppendNodes(Node *lhs, Node *rhs) {
  if (lhs == NULL) {
    return rhs;
  }
  Node *node = lhs;
  while (true) {
    if (node->bnext == NULL) {
      break;
    }
    node = node->bnext;
  }
  node->bnext = rhs;
  return lhs;
}

bool IsCacheAvailable(
    const set<int> &id_set,
    const multimap<int, SystemDictionary::ReverseLookupResult> &results) {
  for (set<int>::const_iterator itr = id_set.begin();
       itr != id_set.end();
       ++itr) {
    if (results.find(*itr) == results.end()) {
      return false;
    }
  }
  return true;
}
}  // namespace

SystemDictionary::SystemDictionary()
    : key_trie_(new rx::RxTrie),
      value_trie_(new rx::RxTrie),
      token_array_(new rx::RbxArray),
      dictionary_file_(new DictionaryFile),
      frequent_pos_(NULL),
      codec_(dictionary::SystemDictionaryCodecFactory::GetCodec()) {}

SystemDictionary::~SystemDictionary() {}

// static
SystemDictionary *SystemDictionary::CreateSystemDictionaryFromFile(
    const string &filename) {
  SystemDictionary *instance = new SystemDictionary();
  DCHECK(instance);
  do {
    if (!instance->dictionary_file_->OpenFromFile(filename)) {
      LOG(ERROR) << "Failed to open system dictionary file";
      break;
    }
    if (!instance->OpenDictionaryFile()) {
      LOG(ERROR) << "Failed to create system dictionary";
      break;
    }
    return instance;
  } while (true);

  delete instance;
  return NULL;
}

// static
SystemDictionary *SystemDictionary::CreateSystemDictionaryFromImage(
    const char *ptr, int len) {
  // Make the dictionary not to be paged out.
  // We don't check the return value because the process doesn't necessarily
  // has the priviledge to mlock.
  // Note that we don't munlock the space because it's always better to keep
  // the singleton system dictionary paged in as long as the process runs.
#ifndef OS_WINDOWS
  mlock(ptr, len);
#endif  // OS_WINDOWS
  SystemDictionary *instance = new SystemDictionary();
  DCHECK(instance);
  do {
    if (!instance->dictionary_file_->OpenFromImage(ptr, len)) {
      LOG(ERROR) << "Failed to open system dictionary file";
      break;
    }
    if (!instance->OpenDictionaryFile()) {
      LOG(ERROR) << "Failed to create system dictionary";
      break;
    }
    return instance;
  } while (true);

  delete instance;
  return NULL;
}

bool SystemDictionary::OpenDictionaryFile() {
  int len;

  const unsigned char *key_image =
      reinterpret_cast<const unsigned char *>(dictionary_file_->GetSection(
          codec_->GetSectionNameForKey(), &len));
  if (!(key_trie_->OpenImage(key_image))) {
    LOG(ERROR) << "cannot open key trie";
    return false;
  }

  const unsigned char *value_image =
      reinterpret_cast<const unsigned char *>(dictionary_file_->GetSection(
          codec_->GetSectionNameForValue(), &len));
  if (!(value_trie_->OpenImage(value_image))) {
    LOG(ERROR) << "can not open value trie";
    return false;
  }

  const unsigned char *token_image =
      reinterpret_cast<const unsigned char *>(dictionary_file_->GetSection(
          codec_->GetSectionNameForTokens(), &len));
  if (!(token_array_->OpenImage(token_image))) {
    LOG(ERROR) << "can not open tokens array";
    return false;
  }

  frequent_pos_ =
      reinterpret_cast<const uint32*>(dictionary_file_->GetSection(
          codec_->GetSectionNameForPos(), &len));
  if (frequent_pos_ == NULL) {
    LOG(ERROR) << "can not find frequent pos section";
    return false;
  }

  return true;
}

Node *SystemDictionary::LookupPredictive(
    const char *str, int size,
    NodeAllocatorInterface *allocator) const {
  string lookup_key_str;
  codec_->EncodeKey(string(str, size), &lookup_key_str);

  vector<rx::RxEntry> results;
  int limit = -1;  // no limit
  if (allocator != NULL) {
    limit = allocator->max_nodes_size();
    key_trie_->PredictiveSearchWithLimit(lookup_key_str,
                                         limit,
                                         &results);
  } else {
    key_trie_->PredictiveSearch(lookup_key_str, &results);
  }

  // a predictive look-up with no limit works slowly, so add a filter of
  // key_len_upper_limit so that the number of node is reduced.
  // the value of key_len_upper_limit is determined by the length of the
  // k-th (currently k = 64) shortest key.
  const size_t kFrequencySize = 30;
  vector<size_t> frequency(kFrequencySize + 1, 0);
  for (size_t i = 0; i < results.size(); ++i) {
    string tokens_key;
    codec_->DecodeKey(results[i].key, &tokens_key);
    if (tokens_key.size() <= kFrequencySize) {
      frequency[tokens_key.size()]++;
    }
  }
  FilterInfo filter;
  const size_t kCriteriaRankForLimit = 64;
  for (size_t len = 1, sum = 0; len <= kFrequencySize; ++len) {
    sum += frequency[len];
    if (sum >= kCriteriaRankForLimit) {
      filter.key_len_upper_limit = len;
      break;
    }
  }

  return GetNodesFromLookupResults(
      filter, results, allocator, &limit);
}

Node *SystemDictionary::LookupPrefixWithLimit(
    const char *str, int size,
    const Limit &lookup_limit,
    NodeAllocatorInterface *allocator) const {
  string lookup_key_str;
  codec_->EncodeKey(string(str, size), &lookup_key_str);

  vector<rx::RxEntry> results;
  int limit = -1;  // no limit
  if (allocator != NULL) {
    limit = allocator->max_nodes_size();
    key_trie_->PrefixSearchWithLimit(
        lookup_key_str, limit, &results);
  } else {
    key_trie_->PrefixSearch(lookup_key_str, &results);
  }

  FilterInfo filter;
  filter.key_len_lower_limit = lookup_limit.key_len_lower_limit;
  return GetNodesFromLookupResults(
      filter, results, allocator, &limit);
}

Node *SystemDictionary::GetNodesFromLookupResults(
    const FilterInfo &filter,
    const vector<rx::RxEntry> &results,
    NodeAllocatorInterface *allocator,
    int *limit) const {
  DCHECK(limit);
  Node *res = NULL;
  vector<dictionary::TokenInfo> tokens;
  for (size_t i = 0; i < results.size(); ++i) {
    if (*limit == 0) {
      break;
    }
    // decode key
    string tokens_key;
    codec_->DecodeKey(results[i].key, &tokens_key);

    // filter by key length
    if (tokens_key.size() < filter.key_len_lower_limit) {
      continue;
    }
    if (tokens_key.size() > filter.key_len_upper_limit) {
      continue;
    }

    // gets tokens block of this key.
    const uint8 *encoded_tokens_ptr = token_array_->Get(results[i].id);
    tokens.clear();
    codec_->DecodeTokens(encoded_tokens_ptr, &tokens);

    res = AppendNodesFromTokens(filter,
                                tokens_key,
                                &tokens,
                                res,
                                allocator,
                                limit);
    // delete tokens
    for (size_t j = 0; j < tokens.size(); ++j) {
      delete tokens[j].token;
    }
    tokens.clear();
  }
  return res;
}

Node *SystemDictionary::AppendNodesFromTokens(
    const FilterInfo &filter,
    const string &tokens_key,
    vector<dictionary::TokenInfo> *tokens,
    Node *node,
    NodeAllocatorInterface *allocator,
    int *limit) const {
  DCHECK(limit);

  string key_katakana;
  Util::HiraganaToKatakana(tokens_key, &key_katakana);

  Node *res = node;
  for (size_t i = 0; i < tokens->size(); ++i) {
    if (*limit == 0) {
      break;
    }

    const dictionary::TokenInfo *prev_token_info =
        ((i > 0) ? &(tokens->at(i - 1)) : NULL);
    dictionary::TokenInfo *token_info = &(tokens->at(i));

    FillTokenInfo(tokens_key, key_katakana, prev_token_info, token_info);

    if (IsBadToken(filter, *token_info)) {
      continue;
    }

    if (token_info->value_type == dictionary::TokenInfo::DEFAULT_VALUE) {
      // Actual lookup here
      LookupValue(token_info);
    }

    if (*limit == -1 || *limit > 0) {
      Node *new_node = CopyTokenToNode(allocator, *(token_info->token));
      new_node->bnext = res;
      res = new_node;
      if (*limit > 0) {
        --(*limit);
      }
    }
  }
  return res;
}

void SystemDictionary::FillTokenInfo(
    const string &key,
    const string &key_katakana,
    const dictionary::TokenInfo *prev_token_info,
    dictionary::TokenInfo *token_info) const {
  Token *token = token_info->token;
  token->key = key;

  switch (token_info->value_type) {
    case dictionary::TokenInfo::DEFAULT_VALUE: {
      // Lookup value later to reduce looking up in filtered condition
      break;
    }
    case dictionary::TokenInfo::SAME_AS_PREV_VALUE: {
      DCHECK(prev_token_info != NULL);
      token_info->id_in_value_trie = prev_token_info->id_in_value_trie;
      token->value = prev_token_info->token->value;
      break;
    }
    case dictionary::TokenInfo::AS_IS_HIRAGANA: {
      token->value = key;
      break;
    }
    case dictionary::TokenInfo::AS_IS_KATAKANA: {
      token->value = key_katakana;
      break;
    }
    default: {
      DCHECK(!token->value.empty());
      break;
    }
  }
  switch (token_info->pos_type) {
    case dictionary::TokenInfo::SAME_AS_PREV_POS: {
      DCHECK(prev_token_info != NULL);
      token->lid = prev_token_info->token->lid;
      token->rid = prev_token_info->token->rid;
      break;
    }
    case dictionary::TokenInfo::FREQUENT_POS: {
      const uint32 pos = frequent_pos_[token_info->id_in_frequent_pos_map];
      token->lid = pos >> 16;
      token->rid = pos & 0xffff;
      break;
    }
    default: {
      break;
    }
  }
}

bool SystemDictionary::IsBadToken(
    const FilterInfo &filter,
    const dictionary::TokenInfo &token_info) const {
  if ((filter.conditions & FilterInfo::NO_SPELLING_CORRECTION) &&
      (token_info.token->attributes & Token::SPELLING_CORRECTION)) {
    return true;
  }

  if ((filter.conditions & FilterInfo::VALUE_ID) &&
      token_info.id_in_value_trie != filter.value_id) {
    return true;
  }

  if ((filter.conditions & FilterInfo::ONLY_T13N) &&
      (token_info.value_type != dictionary::TokenInfo::AS_IS_HIRAGANA &&
       token_info.value_type != dictionary::TokenInfo::AS_IS_KATAKANA)) {
    // SAME_AS_PREV_VALUE may be t13n token.
    string hiragana;
    Util::KatakanaToHiragana(token_info.token->value, &hiragana);
    if (token_info.token->key != hiragana) {
      return true;
    }
  }
  return false;
}

void SystemDictionary::LookupValue(dictionary::TokenInfo *token_info) const {
  const int id = token_info->id_in_value_trie;
  string encoded_value;
  value_trie_->ReverseLookup(id, &encoded_value);
  string *value = &(token_info->token->value);
  codec_->DecodeValue(encoded_value, value);
}

Node *SystemDictionary::LookupReverse(
    const char *str, int size,
    NodeAllocatorInterface *allocator) const {
  int limit = -1;  // no limit
  if (allocator != NULL) {
    limit = allocator->max_nodes_size();
  }

  // 1st step: Hiragana/Katakana are not in the value trie
  // 2nd step: Reverse lookup in value trie
  const string value(str, size);
  Node *t13n_node = GetReverseLookupNodesForT13N(value, allocator, &limit);
  Node *reverse_node = GetReverseLookupNodesForValue(value, allocator, &limit);
  Node *ret = AppendNodes(t13n_node, reverse_node);

  // swap key and value
  for (Node *node = ret; node != NULL; node = node->bnext) {
    node->value.swap(node->key);
  }
  return ret;
}

void SystemDictionary::PopulateReverseLookupCache(
    const char *str, int size, NodeAllocatorInterface *allocator) const {
  if (allocator == NULL) {
    return;
  }
  ReverseLookupCache *cache =
      allocator->mutable_data()->get<ReverseLookupCache>(kReverseLookupCache);
  DCHECK(cache) << "can't get cache data.";
  int pos = 0;
  set<int> ids;
  // Iterate each suffix and collect IDs of all substrings.
  while (pos < size) {
    const string suffix = string(&str[pos], size - pos);
    string lookup_key;
    codec_->EncodeValue(suffix, &lookup_key);
    vector<rx::RxEntry> results;
    value_trie_->PrefixSearch(lookup_key, &results);
    for (size_t i = 0; i < results.size(); ++i) {
      ids.insert(results[i].id);
    }
    pos += Util::OneCharLen(&str[pos]);
  }
  // Collect tokens for all IDs.
  ScanTokens(ids, &cache->results);
}

void SystemDictionary::ClearReverseLookupCache(
    NodeAllocatorInterface *allocator) const {
  allocator->mutable_data()->erase(kReverseLookupCache);
}

Node *SystemDictionary::GetReverseLookupNodesForT13N(
    const string &value, NodeAllocatorInterface *allocator, int *limit) const {
  string hiragana;
  Util::KatakanaToHiragana(value, &hiragana);
  string lookup_key;
  codec_->EncodeKey(hiragana, &lookup_key);

  vector<rx::RxEntry> results;
  if (*limit != -1) {
    key_trie_->PrefixSearchWithLimit(lookup_key, *limit, &results);
  } else {
    key_trie_->PrefixSearch(lookup_key, &results);
  }

  FilterInfo filter;
  filter.conditions = (FilterInfo::NO_SPELLING_CORRECTION |
                       FilterInfo::ONLY_T13N);
  return GetNodesFromLookupResults(filter,
                                   results,
                                   allocator,
                                   limit);
}

Node *SystemDictionary::GetReverseLookupNodesForValue(
    const string &value, NodeAllocatorInterface *allocator, int *limit) const {
  string lookup_key;
  codec_->EncodeValue(value, &lookup_key);

  vector<rx::RxEntry> value_results;

  if (*limit != -1) {
    value_trie_->PrefixSearchWithLimit(lookup_key, *limit, &value_results);
  } else {
    value_trie_->PrefixSearch(lookup_key, &value_results);
  }
  set<int> id_set;
  for (size_t i = 0; i < value_results.size(); ++i) {
    id_set.insert(value_results[i].id);
  }

  multimap<int, ReverseLookupResult> *results = NULL;
  multimap<int, ReverseLookupResult> non_cached_results;
  const bool has_cache = (allocator != NULL &&
                          allocator->data().has(kReverseLookupCache));
  ReverseLookupCache *cache =
      (has_cache ? allocator->mutable_data()->get<ReverseLookupCache>(
          kReverseLookupCache) : NULL);
  if (cache != NULL && IsCacheAvailable(id_set, cache->results)) {
    results = &(cache->results);
  } else {
    // Cache is not available. Get token for each ID.
    ScanTokens(id_set, &non_cached_results);
    results = &non_cached_results;
  }
  DCHECK(results != NULL);

  return GetNodesFromReverseLookupResults(id_set, *results, allocator, limit);
}

void SystemDictionary::ScanTokens(
    const set<int> &id_set,
    multimap<int, ReverseLookupResult> *reverse_results) const {
  int offset = 0;
  int tokens_offset = 0;
  int index = 0;
  const uint8 *encoded_tokens_ptr = token_array_->Get(0);
  const uint8 termination_flag = codec_->GetTokensTerminationFlag();
  while (encoded_tokens_ptr[offset] != termination_flag) {
    int read_bytes;
    int value_id = -1;
    const bool is_last_token =
        !(codec_->ReadTokenForReverseLookup(encoded_tokens_ptr + offset,
                                            &value_id, &read_bytes));
    if (value_id != -1 &&
        id_set.find(value_id) != id_set.end()) {
      ReverseLookupResult result;
      result.tokens_offset = tokens_offset;
      result.id_in_key_trie = index;
      reverse_results->insert(make_pair(value_id, result));
    }
    if (is_last_token) {
      int tokens_size = offset + read_bytes - tokens_offset;
      if (tokens_size < kMinRbxBlobSize) {
        tokens_size = kMinRbxBlobSize;
      }
      tokens_offset += tokens_size;
      ++index;
      offset = tokens_offset;
    } else {
      offset += read_bytes;
    }
  }
}

Node *SystemDictionary::GetNodesFromReverseLookupResults(
    const set<int> &id_set,
    const multimap<int, ReverseLookupResult> &reverse_results,
    NodeAllocatorInterface *allocator,
    int *limit) const {
  Node *res = NULL;
  vector<dictionary::TokenInfo> tokens;
  const uint8 *encoded_tokens_ptr = token_array_->Get(0);
  for (set<int>::const_iterator set_itr = id_set.begin();
       set_itr != id_set.end();
       ++set_itr) {
    FilterInfo filter;
    filter.conditions =
        (FilterInfo::VALUE_ID | FilterInfo::NO_SPELLING_CORRECTION);
    filter.value_id = *set_itr;

    typedef multimap<int, ReverseLookupResult>::const_iterator ResultItr;
    pair<ResultItr, ResultItr> range = reverse_results.equal_range(*set_itr);
    for (ResultItr result_itr = range.first;
         result_itr != range.second;
         ++result_itr) {
      if (*limit == 0) {
        break;
      }

      const ReverseLookupResult reverse_result = result_itr->second;

      tokens.clear();
      codec_->DecodeTokens(
          encoded_tokens_ptr + reverse_result.tokens_offset, &tokens);

      string encoded_key;
      key_trie_->ReverseLookup(reverse_result.id_in_key_trie, &encoded_key);
      string tokens_key;
      codec_->DecodeKey(encoded_key, &tokens_key);

      res = AppendNodesFromTokens(filter,
                                  tokens_key,
                                  &tokens,
                                  res,
                                  allocator,
                                  limit);

      // delete tokens
      for (size_t i = 0; i < tokens.size(); ++i) {
        delete tokens[i].token;
      }
      tokens.clear();
    }
  }
  return res;
}

Node *SystemDictionary::CopyTokenToNode(NodeAllocatorInterface *allocator,
                                        const Token &token) const {
  Node *new_node = NULL;
  if (allocator != NULL) {
    new_node = allocator->NewNode();
  } else {
    // for test
    new_node = new Node();
  }
  new_node->lid = token.lid;
  new_node->rid = token.rid;
  new_node->wcost = token.cost;
  new_node->key = token.key;
  new_node->value = token.value;
  new_node->node_type = Node::NOR_NODE;
  if (token.attributes & Token::SPELLING_CORRECTION) {
    new_node->attributes |= Node::SPELLING_CORRECTION;
  }
  return new_node;
}
}  // namespace mozc
