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
#include "base/flags.h"
#include "base/logging.h"
#include "base/mmap.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/trie.h"
#include "base/util.h"
#include "converter/node.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/file/dictionary_file.h"
#include "dictionary/system/codec_interface.h"
#include "dictionary/system/words_info.h"
#include "dictionary/text_dictionary_loader.h"
#include "session/commands.pb.h"
#include "session/request_handler.h"


namespace mozc {
namespace dictionary {
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


// Note that this class is just introduced due to performance reason.
// Conceptually, it should be in somewhere close to the codec implementation
// (see comments in Next method for details).
// However, it is necessary to refactor a bit larger area, especially around
// codec implementations, to make it happen.
// Considering the merit to introduce this class, we temporarily put it here.
// TODO(hidehiko): Move this class into a Codec related file.
class TokenDecodeIterator {
 public:
  typedef SystemDictionary::TrieType TrieType;

  TokenDecodeIterator(
      const SystemDictionaryCodecInterface *codec,
      const TrieType *value_trie,
      const uint32 *frequent_pos,
      const string &key,
      const uint8 *ptr)
      : codec_(codec),
        value_trie_(value_trie),
        frequent_pos_(frequent_pos),
        key_(key),
        state_(HAS_NEXT),
        ptr_(ptr),
        token_info_(NULL) {
    token_.key = key;
    NextInternal();
  }

  ~TokenDecodeIterator() {
  }

  const TokenInfo& Get() const { return token_info_; }
  bool Done() const { return state_ == DONE; }

  void Next() {
    DCHECK_NE(state_, DONE);
    if (state_ == LAST_TOKEN) {
      state_ = DONE;
      return;
    }
    NextInternal();
  }

 private:
  enum State {
    HAS_NEXT,
    LAST_TOKEN,
    DONE,
  };

  void NextInternal() {
    // Reset token_info with preserving some needed info in previous token.
    int prev_id_in_value_trie = token_info_.id_in_value_trie;
    token_info_.Clear();
    token_info_.token = &token_;

    // Do not clear key in token.
    token_info_.token->attributes = Token::NONE;

    // This implementation is depending on the internal behavior of DecodeToken
    // especially which fields are updated or not. Important fields are:
    // Token::key, Token::value : key and value are never updated.
    // Token::cost : always updated.
    // Token::lid, Token::rid : updated iff the pos_type is neither
    //   FREQUENT_POS nor SAME_AS_PREV_POS.
    // Token::attributes : updated iff the value is SPELLING_COLLECTION.
    // TokenInfo::id_in_value_trie : updated iff the value_type is
    //   DEFAULT_VALUE.
    // Thus, by not-reseting Token instance intentionally, we can skip most
    //   SAME_AS_PREV operations.
    // The exception is Token::attributes. It is not-always set, so we need
    // reset it everytime.
    // This kind of structure should be packed in the codec or some
    // related but new class.
    int read_bytes;
    if (!codec_->DecodeToken(ptr_, &token_info_, &read_bytes)) {
      state_ = LAST_TOKEN;
    }
    ptr_ += read_bytes;

    // Fill remaining values.
    switch (token_info_.value_type) {
      case TokenInfo::DEFAULT_VALUE: {
        token_.value.clear();
        LookupValue(token_info_.id_in_value_trie, &token_.value);
        break;
      }
      case TokenInfo::SAME_AS_PREV_VALUE: {
        DCHECK_NE(prev_id_in_value_trie, -1);
        token_info_.id_in_value_trie = prev_id_in_value_trie;
        // We can keep the current value here.
        break;
      }
      case TokenInfo::AS_IS_HIRAGANA: {
        token_.value = token_.key;
        break;
      }
      case TokenInfo::AS_IS_KATAKANA: {
        if (!key_.empty() && key_katakana_.empty()) {
          Util::HiraganaToKatakana(key_, &key_katakana_);
        }
        token_.value = key_katakana_;
        break;
      }
      default: {
        LOG(DFATAL) << "unknown value_type: " << token_info_.value_type;
        break;
      }
    }

    if (token_info_.pos_type == TokenInfo::FREQUENT_POS) {
      const uint32 pos = frequent_pos_[token_info_.id_in_frequent_pos_map];
      token_.lid = pos >> 16;
      token_.rid = pos & 0xffff;
    }
  }

  void LookupValue(int id, string *value) const {
    string encoded_value;
    value_trie_->ReverseLookup(id, &encoded_value);
    codec_->DecodeValue(encoded_value, value);
  }

  const SystemDictionaryCodecInterface *codec_;
  const TrieType *value_trie_;
  const uint32 *frequent_pos_;

  const string &key_;
  // Katakana key will be lazily initialized.
  string key_katakana_;

  State state_;
  const uint8 *ptr_;

  TokenInfo token_info_;
  Token token_;

  DISALLOW_COPY_AND_ASSIGN(TokenDecodeIterator);
};


Node *CreateNodeFromToken(
    NodeAllocatorInterface *allocator, const Token &token, int penalty) {
  Node *new_node = NULL;
  // TODO(hidehiko): DCHECK nullable of allocator, and use allocator
  // always even for unit tests.
  if (allocator != NULL) {
    new_node = allocator->NewNode();
  } else {
    // for test
    new_node = new Node();
  }
  new_node->lid = token.lid;
  new_node->rid = token.rid;
  new_node->wcost = token.cost + penalty;
  new_node->key = token.key;
  new_node->value = token.value;
  new_node->node_type = Node::NOR_NODE;
  if (token.attributes & Token::SPELLING_CORRECTION) {
    new_node->attributes |= Node::SPELLING_CORRECTION;
  }
  return new_node;
}

}  // namespace

SystemDictionary::SystemDictionary()
    : key_trie_(new TrieType),
      value_trie_(new TrieType),
      token_array_(new ArrayType),
      dictionary_file_(new DictionaryFile),
      frequent_pos_(NULL),
      codec_(dictionary::SystemDictionaryCodecFactory::GetCodec()),
      empty_limit_(Limit()) {
}

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
  Util::MaybeMLock(ptr, len);
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
#ifdef MOZC_USE_MOZC_LOUDS
  token_array_->Open(token_image);
#else
  if (!(token_array_->OpenImage(token_image))) {
    LOG(ERROR) << "can not open tokens array";
    return false;
  }
#endif  // MOZC_USE_MOZC_LOUDS

  frequent_pos_ =
      reinterpret_cast<const uint32*>(dictionary_file_->GetSection(
          codec_->GetSectionNameForPos(), &len));
  if (frequent_pos_ == NULL) {
    LOG(ERROR) << "can not find frequent pos section";
    return false;
  }

  return true;
}


Node *SystemDictionary::LookupPredictiveWithLimit(
    const char *str, int size,
    const Limit &lookup_limit,
    NodeAllocatorInterface *allocator) const {
  if (size == 0) {
    // To fill the gap of the behavior between rx/mozc's louds, return
    // NULL immediately if the key is empty.
    // Background:
    // - For predictive search, RxTrie returns no entries because rx doesn't
    //   invoke callbacks if the key is empty.
    // - On the other hand, Mozc's louds trie returns all values in the trie.
    // From the trie's point of view, returning all values looks a bit more
    // natrual. So, we fill the gap at the dictionary layer as a short term
    // fix.
    // TODO(hidehiko): Returning all entries in dictionary for predictive
    //   searching with an empty key may look natural as well. So we should
    //   find an appropriate handling point.
    return NULL;
  }

  string lookup_key_str;
  codec_->EncodeKey(string(str, size), &lookup_key_str);

  vector<EntryType> results;
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

  filter.key_len_lower_limit = lookup_limit.key_len_lower_limit;
  if (lookup_limit.begin_with_trie != NULL) {
    filter.key_begin_with_pos = size;
    filter.key_begin_with_trie = lookup_limit.begin_with_trie;
  }
  return GetNodesFromLookupResults(filter, results, allocator, &limit);
}

Node *SystemDictionary::LookupPredictive(
    const char *str, int size, NodeAllocatorInterface *allocator) const {
  return LookupPredictiveWithLimit(str, size, empty_limit_, allocator);
}

Node *SystemDictionary::LookupPrefixWithLimit(
    const char *str, int size,
    const Limit &lookup_limit,
    NodeAllocatorInterface *allocator) const {
  string lookup_key_str;
  codec_->EncodeKey(string(str, size), &lookup_key_str);

  vector<EntryType> results;
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
  return GetNodesFromLookupResults(filter, results, allocator, &limit);
}

Node *SystemDictionary::LookupPrefix(
    const char *str, int size,
    NodeAllocatorInterface *allocator) const {
  // default limit
  return LookupPrefixWithLimit(str, size, empty_limit_, allocator);
}

Node *SystemDictionary::LookupExact(const char *str, int size,
                                    NodeAllocatorInterface *allocator) const {
  string lookup_key_str;
  codec_->EncodeKey(string(str, size), &lookup_key_str);
  vector<EntryType> results;
  if (allocator != NULL) {
    key_trie_->PrefixSearchWithLimit(
        lookup_key_str, allocator->max_nodes_size(), &results);
  } else {
    key_trie_->PrefixSearch(lookup_key_str, &results);
  }
  FilterInfo filter;
  filter.key_len_lower_limit = size;
  filter.key_len_upper_limit = size;
  int limit = -1;  // No limit
  return GetNodesFromLookupResults(filter, results, allocator, &limit);
}

Node *SystemDictionary::GetNodesFromLookupResults(
    const FilterInfo &filter,
    const vector<EntryType> &results,
    NodeAllocatorInterface *allocator,
    int *limit) const {
  DCHECK(limit);
  Node *res = NULL;
  for (size_t i = 0; i < results.size(); ++i) {
    if (*limit == 0) {
      break;
    }
    // decode key
    string tokens_key;
    string actual_key;
    codec_->DecodeKey(results[i].key, &tokens_key);
    codec_->DecodeKey(results[i].actual_key, &actual_key);

    // filter by key length
    if (tokens_key.size() < filter.key_len_lower_limit) {
      continue;
    }
    if (tokens_key.size() > filter.key_len_upper_limit) {
      continue;
    }

    // filter by begin with list
    if (filter.key_begin_with_pos >= 0 &&
        filter.key_begin_with_trie != NULL) {
      string value;
      size_t key_length = 0;
      bool has_subtrie = false;
      if (!filter.key_begin_with_trie->LookUpPrefix(
              tokens_key.data() + filter.key_begin_with_pos,
              &value, &key_length, &has_subtrie)) {
        continue;
      }
    }

    // gets tokens block of this key.
#ifdef MOZC_USE_MOZC_LOUDS
    size_t dummy_length;
    const uint8 *encoded_tokens_ptr = reinterpret_cast<const uint8*>(
        token_array_->Get(results[i].id, &dummy_length));
#else
    const uint8 *encoded_tokens_ptr = token_array_->Get(results[i].id);
#endif  // MOZC_USE_MOZC_LOUDS
    res = AppendNodesFromTokens(filter,
                                tokens_key,
                                actual_key,
                                encoded_tokens_ptr,
                                res,
                                allocator,
                                limit);
  }
  return res;
}

Node *SystemDictionary::AppendNodesFromTokens(
    const FilterInfo &filter,
    const string &tokens_key,
    const string &actual_key,
    const uint8 *encoded_tokens_ptr,
    Node *node,
    NodeAllocatorInterface *allocator,
    int *limit) const {
  DCHECK(limit);
  if (*limit == 0) {
    return node;
  }

  int penalty = 0;

  Node *res = node;
  for (TokenDecodeIterator iter(
           codec_, value_trie_.get(),
           frequent_pos_, actual_key, encoded_tokens_ptr);
       !iter.Done(); iter.Next()) {
    const TokenInfo &token_info = iter.Get();
    if (IsBadToken(filter, token_info)) {
      continue;
    }

    Node *new_node = CreateNodeFromToken(
        allocator, *token_info.token, penalty);
    new_node->bnext = res;
    res = new_node;

    // *limit may be negative value, which means no-limit.
    if (*limit > 0) {
      --(*limit);
      if (*limit == 0) {
        break;
      }
    }
  }
  return res;
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
    vector<EntryType> results;
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

  vector<EntryType> results;
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

  vector<EntryType> value_results;

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
#ifdef MOZC_USE_MOZC_LOUDS
  size_t dummy_length;
  const uint8 *encoded_tokens_ptr =
      reinterpret_cast<const uint8*>(token_array_->Get(0, &dummy_length));
#else
  const uint8 *encoded_tokens_ptr = token_array_->Get(0);
#endif  // MOZC_USE_MOZC_LOUDS
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
#ifdef MOZC_USE_MOZC_LOUDS
  size_t dummy_length;
  const uint8 *encoded_tokens_ptr =
      reinterpret_cast<const uint8*>(token_array_->Get(0, &dummy_length));
#else
  const uint8 *encoded_tokens_ptr = token_array_->Get(0);
#endif  // MOZC_USE_MOZC_LOUDS
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

      const ReverseLookupResult &reverse_result = result_itr->second;

      string encoded_key;
      key_trie_->ReverseLookup(reverse_result.id_in_key_trie, &encoded_key);
      string tokens_key;
      codec_->DecodeKey(encoded_key, &tokens_key);

      // actual_key is always the same as tokens_key for reverse conversions.
      res = AppendNodesFromTokens(
          filter,
          tokens_key,
          tokens_key,
          encoded_tokens_ptr + reverse_result.tokens_offset,
          res,
          allocator,
          limit);
    }
  }
  return res;
}

}  // namespace dictionary
}  // namespace mozc
