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

// System dictionary maintains following sections
//  (1) Key trie
//  (2) Value trie
//  (3) Token array
//  (4) Table for high frequent POS(left/right ID)

#include "dictionary/system/system_dictionary.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/port.h"
#include "base/string_piece.h"
#include "base/system_util.h"
#include "base/util.h"
#include "converter/node.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/file/dictionary_file.h"
#include "dictionary/system/codec_interface.h"
#include "dictionary/system/words_info.h"
#include "storage/louds/bit_vector_based_array.h"
#include "storage/louds/louds_trie.h"

namespace mozc {
namespace dictionary {

using mozc::storage::louds::BitVectorBasedArray;
using mozc::storage::louds::KeyExpansionTable;
using mozc::storage::louds::LoudsTrie;

namespace {

// rbx_array default setting
const int kMinRbxBlobSize = 4;
const char *kReverseLookupCache = "reverse_lookup_cache";
// The cost is 500 * log(30): 30 times in freq.
const int32 kKanaModifierInsensitivePenalty = 1700;

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

// Expansion table format:
// "<Character to expand>[<Expanded character 1><Expanded character 2>...]"
//
// Only characters that will be encoded into 1-byte ASCII char are allowed in
// the table.
//
// Note that this implementation has potential issue that the key/values may
// be mixed.
// TODO(hidehiko): Clean up this hacky implementation.
const char *kHiraganaExpansionTable[] = {
  "\xe3\x81\x82\xe3\x81\x82\xe3\x81\x81",  // "ああぁ"
  "\xe3\x81\x84\xe3\x81\x84\xe3\x81\x83",  // "いいぃ"
  "\xe3\x81\x86\xe3\x81\x86\xe3\x81\x85\xe3\x82\x94",  // "ううぅゔ"
  "\xe3\x81\x88\xe3\x81\x88\xe3\x81\x87",  // "ええぇ"
  "\xe3\x81\x8a\xe3\x81\x8a\xe3\x81\x89",  // "おおぉ"
  "\xe3\x81\x8b\xe3\x81\x8b\xe3\x81\x8c",  // "かかが"
  "\xe3\x81\x8d\xe3\x81\x8d\xe3\x81\x8e",  // "ききぎ"
  "\xe3\x81\x8f\xe3\x81\x8f\xe3\x81\x90",  // "くくぐ"
  "\xe3\x81\x91\xe3\x81\x91\xe3\x81\x92",  // "けけげ"
  "\xe3\x81\x93\xe3\x81\x93\xe3\x81\x94",  // "ここご"
  "\xe3\x81\x95\xe3\x81\x95\xe3\x81\x96",  // "ささざ"
  "\xe3\x81\x97\xe3\x81\x97\xe3\x81\x98",  // "ししじ"
  "\xe3\x81\x99\xe3\x81\x99\xe3\x81\x9a",  // "すすず"
  "\xe3\x81\x9b\xe3\x81\x9b\xe3\x81\x9c",  // "せせぜ"
  "\xe3\x81\x9d\xe3\x81\x9d\xe3\x81\x9e",  // "そそぞ"
  "\xe3\x81\x9f\xe3\x81\x9f\xe3\x81\xa0",  // "たただ"
  "\xe3\x81\xa1\xe3\x81\xa1\xe3\x81\xa2",  // "ちちぢ"
  "\xe3\x81\xa4\xe3\x81\xa4\xe3\x81\xa3\xe3\x81\xa5",  // "つつっづ"
  "\xe3\x81\xa6\xe3\x81\xa6\xe3\x81\xa7",  // "ててで"
  "\xe3\x81\xa8\xe3\x81\xa8\xe3\x81\xa9",  // "ととど"
  "\xe3\x81\xaf\xe3\x81\xaf\xe3\x81\xb0\xe3\x81\xb1",  // "ははばぱ"
  "\xe3\x81\xb2\xe3\x81\xb2\xe3\x81\xb3\xe3\x81\xb4",  // "ひひびぴ"
  "\xe3\x81\xb5\xe3\x81\xb5\xe3\x81\xb6\xe3\x81\xb7",  // "ふふぶぷ"
  "\xe3\x81\xb8\xe3\x81\xb8\xe3\x81\xb9\xe3\x81\xba",  // "へへべぺ"
  "\xe3\x81\xbb\xe3\x81\xbb\xe3\x81\xbc\xe3\x81\xbd",  // "ほほぼぽ"
  "\xe3\x82\x84\xe3\x82\x84\xe3\x82\x83",  // "ややゃ"
  "\xe3\x82\x86\xe3\x82\x86\xe3\x82\x85",  // "ゆゆゅ"
  "\xe3\x82\x88\xe3\x82\x88\xe3\x82\x87",  // "よよょ"
  "\xe3\x82\x8f\xe3\x82\x8f\xe3\x82\x8e",  // "わわゎ"
};

const uint32 kAsciiRange = 0x80;

// Confirm that all the characters are within ASCII range.
bool ContainsAsciiCodeOnly(const string &str) {
  for (string::const_iterator it = str.begin(); it != str.end(); ++it) {
    if (static_cast<unsigned char>(*it) >= kAsciiRange) {
      return false;
    }
  }
  return true;
}

void SetKeyExpansion(char key, const string &expansion,
                     KeyExpansionTable *key_expansion_table) {
  key_expansion_table->Add(key, expansion);
}

void BuildHiraganaExpansionTable(
    const SystemDictionaryCodecInterface &codec,
    KeyExpansionTable *encoded_table) {
  for (size_t index = 0; index < arraysize(kHiraganaExpansionTable); ++index) {
    string encoded;
    codec.EncodeKey(kHiraganaExpansionTable[index], &encoded);
    DCHECK(ContainsAsciiCodeOnly(encoded)) <<
        "Encoded expansion data are supposed to fit within ASCII";

    DCHECK_GT(encoded.size(), 0) << "Expansion data is empty";

    if (encoded.size() == 1) {
      continue;
    } else {
      SetKeyExpansion(encoded[0], encoded.substr(1), encoded_table);
    }
  }
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
  TokenDecodeIterator(
      const SystemDictionaryCodecInterface *codec,
      const LoudsTrie *value_trie,
      const uint32 *frequent_pos,
      const StringPiece key,
      const uint8 *ptr)
      : codec_(codec),
        value_trie_(value_trie),
        frequent_pos_(frequent_pos),
        key_(key),
        state_(HAS_NEXT),
        ptr_(ptr),
        token_info_(NULL) {
    key.CopyToString(&token_.key);
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
    char buffer[LoudsTrie::kMaxDepth + 1];
    const char *encoded_value = value_trie_->Reverse(id, buffer);
    const size_t encoded_value_len =
        LoudsTrie::kMaxDepth - (encoded_value - buffer);
    DCHECK_EQ(encoded_value_len, strlen(encoded_value));
    codec_->DecodeValue(StringPiece(encoded_value, encoded_value_len), value);
  }

  const SystemDictionaryCodecInterface *codec_;
  const LoudsTrie *value_trie_;
  const uint32 *frequent_pos_;

  const StringPiece key_;
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
    : key_trie_(new LoudsTrie),
      value_trie_(new LoudsTrie),
      token_array_(new BitVectorBasedArray),
      dictionary_file_(new DictionaryFile),
      frequent_pos_(NULL),
      codec_(SystemDictionaryCodecFactory::GetCodec()),
      empty_limit_(Limit()) {
}

SystemDictionary::~SystemDictionary() {}

// static
SystemDictionary *SystemDictionary::CreateSystemDictionaryFromFile(
    const string &filename) {
  scoped_ptr<SystemDictionary> instance(new SystemDictionary);
  if (!instance->dictionary_file_->OpenFromFile(filename)) {
    LOG(ERROR) << "Failed to open system dictionary file";
    return NULL;
  }
  if (!instance->OpenDictionaryFile()) {
    LOG(ERROR) << "Failed to create system dictionary";
    return NULL;
  }

  return instance.release();
}

// static
SystemDictionary *SystemDictionary::CreateSystemDictionaryFromImage(
    const char *ptr, int len) {
  // Make the dictionary not to be paged out.
  // We don't check the return value because the process doesn't necessarily
  // has the priviledge to mlock.
  // Note that we don't munlock the space because it's always better to keep
  // the singleton system dictionary paged in as long as the process runs.
  SystemUtil::MaybeMLock(ptr, len);
  scoped_ptr<SystemDictionary> instance(new SystemDictionary);
  if (!instance->dictionary_file_->OpenFromImage(ptr, len)) {
    LOG(ERROR) << "Failed to open system dictionary file";
    return NULL;
  }
  if (!instance->OpenDictionaryFile()) {
    LOG(ERROR) << "Failed to create system dictionary";
    return NULL;
  }
  return instance.release();
}

bool SystemDictionary::OpenDictionaryFile() {
  int len;

  const uint8 *key_image = reinterpret_cast<const uint8 *>(
      dictionary_file_->GetSection(codec_->GetSectionNameForKey(), &len));
  if (!key_trie_->Open(key_image)) {
    LOG(ERROR) << "cannot open key trie";
    return false;
  }

  BuildHiraganaExpansionTable(*codec_, &hiragana_expansion_table_);

  const uint8 *value_image = reinterpret_cast<const uint8 *>(
      dictionary_file_->GetSection(codec_->GetSectionNameForValue(), &len));
  if (!value_trie_->Open(value_image)) {
    LOG(ERROR) << "can not open value trie";
    return false;
  }

  const unsigned char *token_image = reinterpret_cast<const unsigned char *>(
      dictionary_file_->GetSection(codec_->GetSectionNameForTokens(), &len));
  token_array_->Open(token_image);

  frequent_pos_ = reinterpret_cast<const uint32*>(
      dictionary_file_->GetSection(codec_->GetSectionNameForPos(), &len));
  if (frequent_pos_ == NULL) {
    LOG(ERROR) << "can not find frequent pos section";
    return false;
  }

  return true;
}

const KeyExpansionTable &SystemDictionary::GetExpansionTableBySetting(
    const Limit &limit) const {
  return limit.kana_modifier_insensitive_lookup_enabled ?
      hiragana_expansion_table_ : KeyExpansionTable::GetDefaultInstance();
}

bool SystemDictionary::HasValue(const StringPiece value) const {
  string encoded;
  codec_->EncodeValue(value, &encoded);
  return value_trie_->ExactSearch(encoded) != -1;
}

namespace {

// Converts a value of SystemDictionary::Callback::ResultType to the
// corresponding value of LoudsTrie::Callback::ResultType.
inline LoudsTrie::Callback::ResultType ConvertResultType(
    const SystemDictionary::Callback::ResultType result) {
  switch (result) {
    case SystemDictionary::Callback::TRAVERSE_DONE: {
      return LoudsTrie::Callback::SEARCH_DONE;
    }
    case SystemDictionary::Callback::TRAVERSE_NEXT_KEY: {
      return LoudsTrie::Callback::SEARCH_CONTINUE;
    }
    case SystemDictionary::Callback::TRAVERSE_CULL: {
      return LoudsTrie::Callback::SEARCH_CULL;
    }
    default: {
      DLOG(FATAL) << "Enum value " << result << " cannot be converted";
      return LoudsTrie::Callback::SEARCH_DONE;  // dummy
    }
  }
}

// Collects short keys preferentially.
class ShortKeyCollector : public LoudsTrie::Callback {
 public:
  // Holds a lookup result from trie.
  struct Entry {
    string encoded_key;  // Encoded lookup key
    string encoded_actual_key;  // Encoded actual key in trie (expanded key)
    size_t actual_key_len;  // Decoded actual key length
    int key_id;  // Key ID in trie
  };

  ShortKeyCollector(const SystemDictionaryCodecInterface *codec,
                    const StringPiece original_encoded_key,
                    size_t min_key_len,
                    size_t limit)
      : codec_(codec),
        original_encoded_key_(original_encoded_key),
        min_key_len_(min_key_len),
        limit_(limit),
        current_max_key_len_(0),
        num_max_key_length_entries_(0) {
    entry_list_.reserve(limit);
  }

  virtual ResultType Run(const char *trie_key,
                         size_t trie_key_len, int key_id) {
    const StringPiece encoded_actual_key(trie_key, trie_key_len);

    // First calculate the length of decoded key.
    // Note: In the current kana modifier insensitive lookup mechanism, the
    // lengths of the original lookup key and its expanded key are equal, so we
    // can omit the construction of lookup key by calculating the length of
    // decoded actual key. Just DCHECK here.
    const size_t key_len = codec_->GetDecodedKeyLength(encoded_actual_key);
    DCHECK_EQ(key_len, codec_->GetDecodedKeyLength(
        original_encoded_key_.as_string() +
        string(trie_key + original_encoded_key_.size(),
               trie_key_len - original_encoded_key_.size())));
    // Uninterested in too short key.
    if (key_len < min_key_len_) {
      return SEARCH_CONTINUE;
    }

    // Check the key length after decoding and update the internal state. As
    // explained above, the length of actual key (expanded key) is equal to that
    // of key.
    const size_t actual_key_len = key_len;
    if (actual_key_len > current_max_key_len_) {
      if (entry_list_.size() > limit_) {
        return SEARCH_CULL;
      }
      current_max_key_len_ = actual_key_len;
      num_max_key_length_entries_ = 1;
    } else if (actual_key_len == current_max_key_len_) {
      ++num_max_key_length_entries_;
    } else {
      if (entry_list_.size() - num_max_key_length_entries_ + 1 >= limit_) {
        RemoveAllMaxKeyLengthEntries();
        UpdateMaxKeyLengthInternal();
      }
    }

    // Keep this entry at the back.
    entry_list_.push_back(Entry());
    entry_list_.back().encoded_key.reserve(trie_key_len);
    original_encoded_key_.CopyToString(&entry_list_.back().encoded_key);
    entry_list_.back().encoded_key.append(
        trie_key + original_encoded_key_.size(),
        trie_key_len - original_encoded_key_.size());
    entry_list_.back().encoded_actual_key.assign(trie_key, trie_key_len);
    entry_list_.back().actual_key_len = actual_key_len;
    entry_list_.back().key_id = key_id;

    return SEARCH_CONTINUE;
  }

  const vector<Entry> &entry_list() const {
    return entry_list_;
  }

 private:
  void RemoveAllMaxKeyLengthEntries() {
    for (size_t i = 0; i < entry_list_.size(); ) {
      Entry *result = &entry_list_[i];
      if (result->actual_key_len < current_max_key_len_) {
        // This result should still be kept.
        ++i;
        continue;
      }

      // Remove the i-th result by swapping it for the last one.
      Entry *last_result = &entry_list_.back();
      swap(result->encoded_key, last_result->encoded_key);
      swap(result->encoded_actual_key, last_result->encoded_actual_key);
      result->actual_key_len = last_result->actual_key_len;
      result->key_id = last_result->key_id;
      entry_list_.pop_back();

      // Do not update the |i| here.
    }
  }

  void UpdateMaxKeyLengthInternal() {
    current_max_key_len_ = 0;
    num_max_key_length_entries_ = 0;
    for (size_t i = 0; i < entry_list_.size(); ++i) {
      if (entry_list_[i].actual_key_len > current_max_key_len_) {
        current_max_key_len_ = entry_list_[i].actual_key_len;
        num_max_key_length_entries_ = 1;
      } else if (entry_list_[i].actual_key_len == current_max_key_len_) {
        ++num_max_key_length_entries_;
      }
    }
  }

  const SystemDictionaryCodecInterface *codec_;
  const StringPiece original_encoded_key_;

  // Filter conditions.
  const size_t min_key_len_;
  const size_t limit_;

  // Internal state for tracking current maximum key length.
  size_t current_max_key_len_;
  size_t num_max_key_length_entries_;

  // Contains lookup results.
  vector<Entry> entry_list_;

  DISALLOW_COPY_AND_ASSIGN(ShortKeyCollector);
};

}  // namespace

Node *SystemDictionary::LookupPredictiveWithLimit(
    const char *str, int size,
    const Limit &lookup_limit,
    NodeAllocatorInterface *allocator) const {
  if (size == 0) {
    // If the key is empty, return NULL (representing an empty result)
    // immediately for backword compatibility.
    // TODO(hidehiko): Returning all entries in dictionary for predictive
    //   searching with an empty key may look natural as well. So we should
    //   find an appropriate handling point.
    return NULL;
  }

  string lookup_key_str;
  codec_->EncodeKey(StringPiece(str, size), &lookup_key_str);
  if (lookup_key_str.size() > LoudsTrie::kMaxDepth) {
    return NULL;
  }

  // First, collect up to 64 keys so that results are as short as possible,
  // which emulates BFS over trie.
  int limit = (allocator == NULL) ?
      numeric_limits<int>::max() : allocator->max_nodes_size();
  ShortKeyCollector collector(
      codec_, lookup_key_str, lookup_limit.key_len_lower_limit, min(64, limit));
  key_trie_->PredictiveSearchWithKeyExpansion(
      lookup_key_str.c_str(),
      GetExpansionTableBySetting(lookup_limit),
      &collector);

  // Build a list of nodes from the lookup results.
  Node *result = NULL;
  string key, actual_key;
  for (size_t i = 0; i < collector.entry_list().size(); ++i) {
    const ShortKeyCollector::Entry &entry = collector.entry_list()[i];

    key.clear();
    codec_->DecodeKey(entry.encoded_key, &key);

    // Filter using |begin_with_trie|.
    // TODO(noriyukit): This filtering feature should be implemented in
    // LoudsTrie in terms of performance. This is why this filtering is not
    // integrated in ShortKeyCollector.
    if (lookup_limit.begin_with_trie != NULL) {
      string value;
      size_t key_length = 0;
      bool has_subtrie = false;
      if (!lookup_limit.begin_with_trie->LookUpPrefix(
              key.data() + size, &value, &key_length, &has_subtrie)) {
        continue;
      }
    }

    actual_key.clear();
    codec_->DecodeKey(entry.encoded_actual_key, &actual_key);

    // Add a penalty if the key differs from the actual key (expanded key).
    // Since codec is bijective, compare in encoded domain for speed.
    const int penalty = (entry.encoded_key == entry.encoded_actual_key) ?
        0 : kKanaModifierInsensitivePenalty;

    // Decode tokens for this key and update the list of nodes.
    size_t dummy_length;
    const uint8 *encoded_tokens_ptr = reinterpret_cast<const uint8*>(
        token_array_->Get(entry.key_id, &dummy_length));
    for (TokenDecodeIterator iter(
             codec_, value_trie_.get(),
             frequent_pos_, actual_key, encoded_tokens_ptr);
         !iter.Done(); iter.Next()) {
      const TokenInfo &token_info = iter.Get();
      Node *new_node = CreateNodeFromToken(
          allocator, *token_info.token, penalty);
      new_node->bnext = result;
      result = new_node;
      --limit;
      if (limit <= 0) {
        return result;
      }
    }
  }

  return result;
}

Node *SystemDictionary::LookupPredictive(
    const char *str, int size, NodeAllocatorInterface *allocator) const {
  return LookupPredictiveWithLimit(str, size, empty_limit_, allocator);
}

namespace {

// A general purpose traverser for prefix search over the system dictionary.
class PrefixTraverser : public LoudsTrie::Callback {
 public:
  PrefixTraverser(const BitVectorBasedArray *token_array,
                  const LoudsTrie *value_trie,
                  const SystemDictionaryCodecInterface *codec,
                  const uint32 *frequent_pos,
                  const StringPiece original_encoded_key,
                  SystemDictionary::Callback *callback)
      : token_array_(token_array),
        value_trie_(value_trie),
        codec_(codec),
        frequent_pos_(frequent_pos),
        original_encoded_key_(original_encoded_key),
        callback_(callback) {
  }

  virtual ResultType Run(const char *trie_key,
                         size_t trie_key_len, int key_id) {
    // Decode key and call back OnKey().
    const StringPiece encoded_key =
        original_encoded_key_.substr(0, trie_key_len);
    string key;
    codec_->DecodeKey(encoded_key, &key);
    SystemDictionary::Callback::ResultType result = callback_->OnKey(key);
    if (result != SystemDictionary::Callback::TRAVERSE_CONTINUE) {
      return ConvertResultType(result);
    }

    // Decode actual key (expanded key) and call back OnActualKey().  To check
    // if the actual key is expanded, compare the keys in encoded domain for
    // performance (this is guaranteed as codec is a bijection).
    const StringPiece encoded_actual_key(trie_key, trie_key_len);
    string actual_key;
    actual_key.reserve(key.size());
    codec_->DecodeKey(encoded_actual_key, &actual_key);
    const bool is_expanded = encoded_actual_key != encoded_key;
    DCHECK_EQ(is_expanded, key != actual_key);
    result = callback_->OnActualKey(key, actual_key, is_expanded);
    if (result != SystemDictionary::Callback::TRAVERSE_CONTINUE) {
      return ConvertResultType(result);
    }

    // Decode tokens and call back OnToken() for each token.
    size_t dummy_length;
    const uint8 *encoded_tokens_ptr = reinterpret_cast<const uint8*>(
        token_array_->Get(key_id, &dummy_length));
    for (TokenDecodeIterator iter(
             codec_, value_trie_, frequent_pos_,
             actual_key, encoded_tokens_ptr);
         !iter.Done(); iter.Next()) {
      const TokenInfo &token_info = iter.Get();
      result = callback_->OnToken(key, actual_key, token_info);
      if (result != SystemDictionary::Callback::TRAVERSE_CONTINUE) {
        return ConvertResultType(result);
      }
    }
    return SEARCH_CONTINUE;
  }

 private:
  const BitVectorBasedArray *token_array_;
  const LoudsTrie *value_trie_;
  const SystemDictionaryCodecInterface *codec_;
  const uint32 *frequent_pos_;
  const StringPiece original_encoded_key_;
  SystemDictionary::Callback *callback_;

  DISALLOW_COPY_AND_ASSIGN(PrefixTraverser);
};

// Provides basic functionality for building a list of nodes.
class BaseNodeListBuilder : public SystemDictionary::Callback {
 public:
  BaseNodeListBuilder(NodeAllocatorInterface *allocator, int limit)
      : allocator_(allocator), limit_(limit), penalty_(0), result_(NULL) {
  }

  // Determines a penalty for tokens of this (key, actual_key) pair.
  virtual ResultType OnActualKey(const string &key,
                                 const string &actual_key,
                                 bool is_expanded) {
    penalty_ = is_expanded ? kKanaModifierInsensitivePenalty : 0;
    return TRAVERSE_CONTINUE;
  }

  // Creates a new node and prepends it to the current list.
  virtual ResultType OnToken(const string &key, const string &actual_key,
                             const TokenInfo &token_info) {
    Node *new_node = CreateNodeFromToken(
        allocator_, *token_info.token, penalty_);
    new_node->bnext = result_;
    result_ = new_node;
    --limit_;
    if (limit_ <= 0) {
      return TRAVERSE_DONE;
    }
    return TRAVERSE_CONTINUE;
  }

  int limit() const {
    return limit_;
  }

  Node *result() const {
    return result_;
  }

 private:
  NodeAllocatorInterface *allocator_;
  int limit_;
  int penalty_;
  Node *result_;

  DISALLOW_COPY_AND_ASSIGN(BaseNodeListBuilder);
};

// Implements key filtering rule for LookupPrefix().
class NodeListBuilderForLookupPrefix : public BaseNodeListBuilder {
 public:
  NodeListBuilderForLookupPrefix(NodeAllocatorInterface *allocator,
                                 int limit, size_t min_key_length)
      : BaseNodeListBuilder(allocator, limit),
        min_key_length_(min_key_length) {
  }

  virtual ResultType OnKey(const string &key) {
    return key.size() < min_key_length_ ? TRAVERSE_NEXT_KEY : TRAVERSE_CONTINUE;
  }

 private:
  const size_t min_key_length_;
  DISALLOW_COPY_AND_ASSIGN(NodeListBuilderForLookupPrefix);
};

}  // namespace

Node *SystemDictionary::LookupPrefixWithLimit(
    const char *str, int size,
    const Limit &lookup_limit,
    NodeAllocatorInterface *allocator) const {
  const int max_nodes_size = (allocator == NULL) ?
      numeric_limits<int>::max() : allocator->max_nodes_size();
  NodeListBuilderForLookupPrefix builder(
      allocator, max_nodes_size, lookup_limit.key_len_lower_limit);
  LookupPrefixWithCallback(
      StringPiece(str, size),
      lookup_limit.kana_modifier_insensitive_lookup_enabled,
      &builder);
  return builder.result();
}

Node *SystemDictionary::LookupPrefix(
    const char *str, int size, NodeAllocatorInterface *allocator) const {
  // default limit
  return LookupPrefixWithLimit(str, size, empty_limit_, allocator);
}

void SystemDictionary::LookupPrefixWithCallback(
    const StringPiece key,
    bool use_kana_modifier_insensitive_lookup,
    Callback *callback) const {
  string original_encoded_key;
  codec_->EncodeKey(key, &original_encoded_key);
  PrefixTraverser traverser(token_array_.get(), value_trie_.get(), codec_,
                            frequent_pos_, original_encoded_key, callback);
  const KeyExpansionTable &table = use_kana_modifier_insensitive_lookup ?
      hiragana_expansion_table_ : KeyExpansionTable::GetDefaultInstance();
  key_trie_->PrefixSearchWithKeyExpansion(
      original_encoded_key.c_str(), table, &traverser);
}

Node *SystemDictionary::LookupExact(
    const char *str, int size, NodeAllocatorInterface *allocator) const {
  const StringPiece key(str, size);

  // Find the key in the key trie.
  string encoded_key;
  codec_->EncodeKey(key, &encoded_key);
  const int key_id = key_trie_->ExactSearch(encoded_key);
  if (key_id == -1) {
    return NULL;
  }

  // Get the block of tokens for this key.
  size_t dummy_length;
  const uint8 *encoded_tokens_ptr = reinterpret_cast<const uint8*>(
      token_array_->Get(key_id, &dummy_length));

  // Build a list of nodes from the tokens.
  Node *result = NULL;
  int limit = (allocator == NULL) ?
      numeric_limits<int>::max() : allocator->max_nodes_size();
  for (TokenDecodeIterator iter(
           codec_, value_trie_.get(), frequent_pos_, key, encoded_tokens_ptr);
       !iter.Done(); iter.Next()) {
    if (limit <= 0) {
      break;
    }
    const TokenInfo &token_info = iter.Get();
    Node *new_node = CreateNodeFromToken(allocator, *token_info.token, 0);
    new_node->bnext = result;
    result = new_node;
    --limit;
  }

  return result;
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
  if (tokens_key != actual_key) {
    penalty = kKanaModifierInsensitivePenalty;
  }

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
    const TokenInfo &token_info) const {
  if ((filter.conditions & FilterInfo::NO_SPELLING_CORRECTION) &&
      (token_info.token->attributes & Token::SPELLING_CORRECTION)) {
    return true;
  }

  if ((filter.conditions & FilterInfo::VALUE_ID) &&
      token_info.id_in_value_trie != filter.value_id) {
    return true;
  }

  if ((filter.conditions & FilterInfo::ONLY_T13N) &&
      (token_info.value_type != TokenInfo::AS_IS_HIRAGANA &&
       token_info.value_type != TokenInfo::AS_IS_KATAKANA)) {
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
    const char *str, int size, NodeAllocatorInterface *allocator) const {
  int limit = (allocator != NULL) ?
      allocator->max_nodes_size() : numeric_limits<int>::max();

  // 1st step: Hiragana/Katakana are not in the value trie
  // 2nd step: Reverse lookup in value trie
  const StringPiece value(str, size);
  Node *t13n_node = GetReverseLookupNodesForT13N(value, allocator, &limit);
  Node *reverse_node = GetReverseLookupNodesForValue(value, allocator, &limit);
  Node *ret = AppendNodes(t13n_node, reverse_node);

  // swap key and value
  for (Node *node = ret; node != NULL; node = node->bnext) {
    node->value.swap(node->key);
  }
  return ret;
}

namespace {

// Collects all the IDs of louds trie whose keys match lookup query. The limit
// parameter can be used to restrict the maximum number of lookup.
class IdCollector : public LoudsTrie::Callback {
 public:
  IdCollector() : limit_(numeric_limits<int>::max()) {
  }

  explicit IdCollector(int limit) : limit_(limit) {
  }

  // Called back on each key found. Inserts the key id to |id_set_| up to
  // |limit_|.
  virtual ResultType Run(const char *key, size_t key_len, int key_id) {
    if (limit_ <= 0) {
      return SEARCH_DONE;
    }
    id_set_.insert(key_id);
    --limit_;
    return SEARCH_CONTINUE;
  }

  const set<int> &id_set() const {
    return id_set_;
  }

 private:
  int limit_;
  set<int> id_set_;

  DISALLOW_COPY_AND_ASSIGN(IdCollector);
};

}  // namespace

void SystemDictionary::PopulateReverseLookupCache(
    const char *str, int size, NodeAllocatorInterface *allocator) const {
  if (allocator == NULL) {
    return;
  }
  ReverseLookupCache *cache =
      allocator->mutable_data()->get<ReverseLookupCache>(kReverseLookupCache);
  DCHECK(cache) << "can't get cache data.";

  IdCollector id_collector;
  int pos = 0;
  // Iterate each suffix and collect IDs of all substrings.
  while (pos < size) {
    const StringPiece suffix(&str[pos], size - pos);
    string lookup_key;
    codec_->EncodeValue(suffix, &lookup_key);
    value_trie_->PrefixSearch(lookup_key.c_str(), &id_collector);
    pos += Util::OneCharLen(&str[pos]);
  }
  // Collect tokens for all IDs.
  ScanTokens(id_collector.id_set(), &cache->results);
}

void SystemDictionary::ClearReverseLookupCache(
    NodeAllocatorInterface *allocator) const {
  allocator->mutable_data()->erase(kReverseLookupCache);
}

namespace {

// Provides token filtering rule for reverse lookup of T13N entries.
class T13nNodeListBuilder : public BaseNodeListBuilder {
 public:
  T13nNodeListBuilder(NodeAllocatorInterface *allocator, int limit)
      : BaseNodeListBuilder(allocator, limit) {
  }

  virtual ResultType OnToken(const string &key, const string &expanded_key,
                             const TokenInfo &token_info) {
    if (token_info.token->attributes & Token::SPELLING_CORRECTION) {
      return TRAVERSE_CONTINUE;
    }
    if (token_info.value_type != TokenInfo::AS_IS_HIRAGANA &&
        token_info.value_type != TokenInfo::AS_IS_KATAKANA) {
      // SAME_AS_PREV_VALUE may be t13n token.
      string hiragana;
      Util::KatakanaToHiragana(token_info.token->value, &hiragana);
      if (token_info.token->key != hiragana) {
        return TRAVERSE_CONTINUE;
      }
    }
    return BaseNodeListBuilder::OnToken(key, expanded_key, token_info);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(T13nNodeListBuilder);
};

}  // namespace

Node *SystemDictionary::GetReverseLookupNodesForT13N(
    const StringPiece value, NodeAllocatorInterface *allocator,
    int *limit) const {
  string hiragana;
  Util::KatakanaToHiragana(value, &hiragana);
  T13nNodeListBuilder builder(allocator, *limit);
  LookupPrefixWithCallback(hiragana, false, &builder);
  *limit = builder.limit();  // Update limit.
  return builder.result();
}

Node *SystemDictionary::GetReverseLookupNodesForValue(
    const StringPiece value, NodeAllocatorInterface *allocator,
    int *limit) const {
  string lookup_key;
  codec_->EncodeValue(value, &lookup_key);

  IdCollector id_collector(*limit);
  value_trie_->PrefixSearch(lookup_key.c_str(), &id_collector);
  const set<int> &id_set = id_collector.id_set();

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
  size_t dummy_length;
  const uint8 *encoded_tokens_ptr =
      reinterpret_cast<const uint8*>(token_array_->Get(0, &dummy_length));
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
  size_t dummy_length;
  const uint8 *encoded_tokens_ptr =
      reinterpret_cast<const uint8*>(token_array_->Get(0, &dummy_length));
  char buffer[LoudsTrie::kMaxDepth + 1];
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
        return res;
      }

      const ReverseLookupResult &reverse_result = result_itr->second;

      const char *encoded_key =
          key_trie_->Reverse(reverse_result.id_in_key_trie, buffer);
      const size_t encoded_key_len =
          LoudsTrie::kMaxDepth - (encoded_key - buffer);
      DCHECK_EQ(encoded_key_len, strlen(encoded_key));
      string tokens_key;
      codec_->DecodeKey(StringPiece(encoded_key, encoded_key_len), &tokens_key);

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
