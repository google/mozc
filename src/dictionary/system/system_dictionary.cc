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

// System dictionary maintains following sections
//  (1) Key trie
//       Trie containing encoded key. Returns ids for lookup.
//       We can get the key using the id by performing reverse lookup
//       against the trie.
//  (2) Value trie
//       Trie containing encoded value. Returns ids for lookup.
//       We can get the value using the id by performing reverse lookup
//       against the trie.
//  (3) Token array
//       Array containing encoded tokens. Array index is the id in key trie.
//       Token contains cost, POS, the id in key trie, etc.
//  (4) Table for high frequent POS(left/right ID)
//       Frequenty appearing POSs are stored as POS ids in token info for
//       reducing binary size. This table is the map from the id to the
//       actual ids.

#include "dictionary/system/system_dictionary.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/japanese_util.h"
#include "base/mmap.h"
#include "base/strings/unicode.h"
#include "base/util.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/file/codec_factory.h"
#include "dictionary/file/codec_interface.h"
#include "dictionary/file/dictionary_file.h"
#include "dictionary/system/codec_interface.h"
#include "dictionary/system/key_expansion_table.h"
#include "dictionary/system/token_decode_iterator.h"
#include "dictionary/system/words_info.h"
#include "request/conversion_request.h"
#include "storage/louds/bit_vector_based_array.h"
#include "storage/louds/louds_trie.h"

namespace mozc {
namespace dictionary {

using ::mozc::storage::louds::BitVectorBasedArray;
using ::mozc::storage::louds::LoudsTrie;

namespace {

constexpr int kMinTokenArrayBlobSize = 4;

// TODO(noriyukit): The following parameters may not be well optimized.  In our
// experiments, Select1 is computational burden, so increasing cache size for
// lb1/select1 may improve performance.
constexpr size_t kKeyTrieLb0CacheSize = 1 * 1024;
constexpr size_t kKeyTrieLb1CacheSize = 1 * 1024;
constexpr size_t kKeyTrieSelect0CacheSize = 4 * 1024;
constexpr size_t kKeyTrieSelect1CacheSize = 4 * 1024;
constexpr size_t kKeyTrieTermvecCacheSize = 1 * 1024;

constexpr size_t kValueTrieLb0CacheSize = 1 * 1024;
constexpr size_t kValueTrieLb1CacheSize = 1 * 1024;
constexpr size_t kValueTrieSelect0CacheSize = 1 * 1024;
constexpr size_t kValueTrieSelect1CacheSize = 16 * 1024;
constexpr size_t kValueTrieTermvecCacheSize = 4 * 1024;

// Expansion table format:
// "<Character to expand>[<Expanded character 1><Expanded character 2>...]"
//
// Only characters that will be encoded into 1-byte ASCII char are allowed in
// the table.
//
// Note that this implementation has potential issue that the key/values may
// be mixed.
// TODO(hidehiko): Clean up this hacky implementation.
const char* kHiraganaExpansionTable[] = {
    "ああぁ",   "いいぃ",   "ううぅゔ", "ええぇ",   "おおぉ",   "かかが",
    "ききぎ",   "くくぐ",   "けけげ",   "ここご",   "ささざ",   "ししじ",
    "すすず",   "せせぜ",   "そそぞ",   "たただ",   "ちちぢ",   "つつっづ",
    "っっづ",   "ててで",   "ととど",   "ははばぱ", "ひひびぴ", "ふふぶぷ",
    "へへべぺ", "ほほぼぽ", "ややゃ",   "ゆゆゅ",   "よよょ",   "わわゎ",
};

void SetKeyExpansion(const char key, const absl::string_view expansion,
                     KeyExpansionTable* key_expansion_table) {
  key_expansion_table->Add(key, expansion);
}

void BuildHiraganaExpansionTable(const SystemDictionaryCodecInterface& codec,
                                 KeyExpansionTable* encoded_table) {
  for (size_t index = 0; index < std::size(kHiraganaExpansionTable); ++index) {
    std::string encoded;
    codec.EncodeKey(kHiraganaExpansionTable[index], &encoded);
    DCHECK(Util::IsAscii(encoded))
        << "Encoded expansion data are supposed to fit within ASCII";

    DCHECK_GT(encoded.size(), 0) << "Expansion data is empty";

    if (encoded.size() == 1) {
      continue;
    } else {
      SetKeyExpansion(encoded[0], encoded.substr(1), encoded_table);
    }
  }
}

inline const uint8_t* GetTokenArrayPtr(const BitVectorBasedArray& token_array,
                                       int key_id) {
  size_t length = 0;
  return reinterpret_cast<const uint8_t*>(token_array.Get(key_id, &length));
}

// Iterator for scanning token array.
// This iterator does not return actual token info but returns
// id data and the position only.
// This will be used only for reverse lookup.
// Forward lookup does not need such iterator because it can access
// a token directly without linear scan.
//
//  Usage:
//    for (TokenScanIterator iter(codec_, token_array_);
//         !iter.Done(); iter.Next()) {
//      const TokenScanIterator::Result &result = iter.Get();
//      // Do something with |result|.
//    }
class TokenScanIterator {
 public:
  struct Result {
    // Value id for the current token
    int value_id;
    // Index (= key id) for the current token
    int index;
    // Offset from the tokens section beginning.
    // (token_array_->Get(id_in_key_trie) ==
    //  token_array_->Get(0) + tokens_offset)
    int tokens_offset;
  };

  TokenScanIterator(const TokenScanIterator&) = delete;
  TokenScanIterator& operator=(const TokenScanIterator&) = delete;
  TokenScanIterator(const SystemDictionaryCodecInterface* codec,
                    const BitVectorBasedArray& token_array)
      : codec_(codec),
        termination_flag_(codec->GetTokensTerminationFlag()),
        state_(HAS_NEXT),
        offset_(0),
        tokens_offset_(0),
        index_(0) {
    encoded_tokens_ptr_ = GetTokenArrayPtr(token_array, 0);
    NextInternal();
  }

  ~TokenScanIterator() = default;

  const Result& Get() const { return result_; }

  bool Done() const { return state_ == DONE; }

  void Next() {
    DCHECK_NE(state_, DONE);
    NextInternal();
  }

 private:
  enum State {
    HAS_NEXT,
    DONE,
  };

  void NextInternal() {
    if (encoded_tokens_ptr_[offset_] == termination_flag_) {
      state_ = DONE;
      return;
    }
    int read_bytes;
    result_.value_id = -1;
    result_.index = index_;
    result_.tokens_offset = tokens_offset_;
    const bool is_last_token = !(codec_->ReadTokenForReverseLookup(
        encoded_tokens_ptr_ + offset_, &result_.value_id, &read_bytes));
    if (is_last_token) {
      int tokens_size = offset_ + read_bytes - tokens_offset_;
      if (tokens_size < kMinTokenArrayBlobSize) {
        tokens_size = kMinTokenArrayBlobSize;
      }
      tokens_offset_ += tokens_size;
      ++index_;
      offset_ = tokens_offset_;
    } else {
      offset_ += read_bytes;
    }
  }

  const SystemDictionaryCodecInterface* codec_;
  const uint8_t* encoded_tokens_ptr_;
  const uint8_t termination_flag_;
  State state_;
  Result result_;
  int offset_;
  int tokens_offset_;
  int index_;
};

struct ReverseLookupResult {
  ReverseLookupResult() : tokens_offset(-1), id_in_key_trie(-1) {}
  // Offset from the tokens section beginning.
  // (token_array_.Get(id_in_key_trie) == token_array_.Get(0) + tokens_offset)
  int tokens_offset;
  // Id in key trie
  int id_in_key_trie;
};

}  // namespace

class SystemDictionary::ReverseLookupCache {
 public:
  ReverseLookupCache() = default;
  ReverseLookupCache(const ReverseLookupCache&) = delete;
  ReverseLookupCache& operator=(const ReverseLookupCache&) = delete;

  bool IsAvailable(const absl::btree_set<int>& id_set) const {
    // std::all_of(x.begin(), x.end(), .. ) returns true if x is empty
    // regardless of the condition.
    return std::all_of(id_set.begin(), id_set.end(),
                       [&](const int id) { return results.contains(id); });
  }

  absl::btree_multimap<int, ReverseLookupResult> results;
};

class SystemDictionary::ReverseLookupIndex {
 public:
  ReverseLookupIndex(const ReverseLookupIndex&) = delete;
  ReverseLookupIndex& operator=(const ReverseLookupIndex&) = delete;
  ReverseLookupIndex(const SystemDictionaryCodecInterface* codec,
                     const BitVectorBasedArray& token_array) {
    // Gets id size.
    int value_id_max = -1;
    for (TokenScanIterator iter(codec, token_array); !iter.Done();
         iter.Next()) {
      const TokenScanIterator::Result& result = iter.Get();
      value_id_max = std::max(value_id_max, result.value_id);
    }

    CHECK_GE(value_id_max, 0);
    index_size_ = value_id_max + 1;
    index_ = std::make_unique<ReverseLookupResultArray[]>(index_size_);

    // Gets result size for each ids.
    for (TokenScanIterator iter(codec, token_array); !iter.Done();
         iter.Next()) {
      const TokenScanIterator::Result& result = iter.Get();
      if (result.value_id != -1) {
        DCHECK_LT(result.value_id, index_size_);
        ++(index_[result.value_id].size);
      }
    }

    for (size_t i = 0; i < index_size_; ++i) {
      index_[i].results =
          std::make_unique<ReverseLookupResult[]>(index_[i].size);
    }

    // Builds index.
    for (TokenScanIterator iter(codec, token_array); !iter.Done();
         iter.Next()) {
      const TokenScanIterator::Result& result = iter.Get();
      if (result.value_id == -1) {
        continue;
      }

      DCHECK_LT(result.value_id, index_size_);
      ReverseLookupResultArray* result_array = &index_[result.value_id];

      // Finds uninitialized result.
      size_t result_index = 0;
      for (result_index = 0; result_index < result_array->size;
           ++result_index) {
        const ReverseLookupResult& lookup_result =
            result_array->results[result_index];
        if (lookup_result.tokens_offset == -1 &&
            lookup_result.id_in_key_trie == -1) {
          result_array->results[result_index].tokens_offset =
              result.tokens_offset;
          result_array->results[result_index].id_in_key_trie = result.index;
          break;
        }
      }
    }

    CHECK(index_ != nullptr);
  }

  ~ReverseLookupIndex() = default;

  void FillResultMap(
      const absl::btree_set<int>& id_set,
      absl::btree_multimap<int, ReverseLookupResult>* result_map) const {
    for (const int id : id_set) {
      const ReverseLookupResultArray& result_array = index_[id];
      for (size_t i = 0; i < result_array.size; ++i) {
        result_map->emplace(id, result_array.results[i]);
      }
    }
  }

 private:
  struct ReverseLookupResultArray {
    ReverseLookupResultArray() : size(0) {}
    // Use std::unique_ptr for reducing memory consumption as possible.
    // Using vector requires 90 MB even when we call resize explicitly.
    // On the other hand, std::unique_ptr requires 57 MB.
    std::unique_ptr<ReverseLookupResult[]> results;
    size_t size;
  };

  // Use scoped array for reducing memory consumption as possible.
  std::unique_ptr<ReverseLookupResultArray[]> index_;
  size_t index_size_;
};

struct SystemDictionary::PredictiveLookupSearchState {
  PredictiveLookupSearchState() : key_pos(0), num_expanded(0) {}
  PredictiveLookupSearchState(const storage::louds::LoudsTrie::Node& n,
                              size_t pos, int expanded)
      : node(n), key_pos(pos), num_expanded(expanded) {}

  storage::louds::LoudsTrie::Node node;
  size_t key_pos;
  int num_expanded;
};

SystemDictionary::Builder::Builder(absl::string_view filename)
    : spec_(new Specification(Specification::FILENAME, filename, nullptr, -1,
                              NONE, nullptr, nullptr)) {}

SystemDictionary::Builder::Builder(const char* ptr, int len)
    : spec_(new Specification(Specification::IMAGE, "", ptr, len, NONE, nullptr,
                              nullptr)) {}

SystemDictionary::Builder& SystemDictionary::Builder::SetOptions(
    Options options) {
  spec_->options = options;
  return *this;
}

SystemDictionary::Builder& SystemDictionary::Builder::SetCodec(
    const SystemDictionaryCodecInterface* codec) {
  spec_->codec = codec;
  return *this;
}

absl::StatusOr<std::unique_ptr<SystemDictionary>>
SystemDictionary::Builder::Build() {
  if (spec_->codec == nullptr) {
    spec_->codec = SystemDictionaryCodecFactory::GetCodec();
  }

  if (spec_->file_codec == nullptr) {
    spec_->file_codec = DictionaryFileCodecFactory::GetCodec();
  }

  auto instance =
      absl::WrapUnique(new SystemDictionary(spec_->codec, spec_->file_codec));

  switch (spec_->type) {
    case Specification::FILENAME: {
      auto status = instance->dictionary_file_->OpenFromFile(spec_->filename);
      if (!status.ok()) {
        return status;
      }
      break;
    }
    case Specification::IMAGE: {
      // Make the dictionary not to be paged out.
      // We don't check the return value because the process doesn't necessarily
      // has the privilege to mlock.
      // Note that we don't munlock the space because it's always better to keep
      // the singleton system dictionary paged in as long as the process runs.
      Mmap::MaybeMLock(spec_->ptr, spec_->len);
      auto status =
          instance->dictionary_file_->OpenFromImage(spec_->ptr, spec_->len);
      if (!status.ok()) {
        return status;
      }
      break;
    }
    default:
      return absl::InvalidArgumentError("Invalid spec type");
  }

  if (!instance->OpenDictionaryFile(
          (spec_->options & ENABLE_REVERSE_LOOKUP_INDEX) != 0)) {
    return absl::UnknownError("Failed to create system dictionary");
  }

  return instance;
}

SystemDictionary::SystemDictionary(
    const SystemDictionaryCodecInterface* codec,
    const DictionaryFileCodecInterface* file_codec)
    : frequent_pos_(nullptr),
      codec_(codec),
      dictionary_file_(new DictionaryFile(file_codec)) {}

SystemDictionary::~SystemDictionary() = default;

bool SystemDictionary::OpenDictionaryFile(bool enable_reverse_lookup_index) {
  int len;

  const uint8_t* key_image = reinterpret_cast<const uint8_t*>(
      dictionary_file_->GetSection(codec_->GetSectionNameForKey(), &len));
  if (!key_trie_.Open(key_image, kKeyTrieLb0CacheSize, kKeyTrieLb1CacheSize,
                      kKeyTrieSelect0CacheSize, kKeyTrieSelect1CacheSize,
                      kKeyTrieTermvecCacheSize)) {
    LOG(ERROR) << "cannot open key trie";
    return false;
  }

  BuildHiraganaExpansionTable(*codec_, &hiragana_expansion_table_);

  const uint8_t* value_image = reinterpret_cast<const uint8_t*>(
      dictionary_file_->GetSection(codec_->GetSectionNameForValue(), &len));
  if (!value_trie_.Open(value_image, kValueTrieLb0CacheSize,
                        kValueTrieLb1CacheSize, kValueTrieSelect0CacheSize,
                        kValueTrieSelect1CacheSize,
                        kValueTrieTermvecCacheSize)) {
    LOG(ERROR) << "can not open value trie";
    return false;
  }

  const unsigned char* token_image = reinterpret_cast<const unsigned char*>(
      dictionary_file_->GetSection(codec_->GetSectionNameForTokens(), &len));
  token_array_.Open(token_image);

  frequent_pos_ = reinterpret_cast<const uint32_t*>(
      dictionary_file_->GetSection(codec_->GetSectionNameForPos(), &len));
  if (frequent_pos_ == nullptr) {
    LOG(ERROR) << "can not find frequent pos section";
    return false;
  }

  if (enable_reverse_lookup_index) {
    InitReverseLookupIndex();
  }

  return true;
}

void SystemDictionary::InitReverseLookupIndex() {
  if (reverse_lookup_index_ != nullptr) {
    return;
  }
  reverse_lookup_index_ =
      std::make_unique<ReverseLookupIndex>(codec_, token_array_);
}

bool SystemDictionary::HasKey(absl::string_view key) const {
  std::string encoded_key;
  codec_->EncodeKey(key, &encoded_key);
  return key_trie_.HasKey(encoded_key);
}

bool SystemDictionary::HasValue(absl::string_view value) const {
  std::string encoded_value;
  codec_->EncodeValue(value, &encoded_value);
  if (value_trie_.HasKey(encoded_value)) {
    return true;
  }

  // Because Hiragana, Katakana and Alphabet words are not stored in the
  // value_trie for the data compression.  They are only stored in the
  // key_trie with flags.  So we also need to check the existence in
  // the key_trie.

  // Normalize the value as the key.  This process depends on the
  // implementation of SystemDictionaryBuilder::BuildValueTrie.
  std::string key = japanese_util::KatakanaToHiragana(value);

  std::string encoded_key;
  codec_->EncodeKey(key, &encoded_key);
  const int key_id = key_trie_.ExactSearch(encoded_key);
  if (key_id == -1) {
    return false;
  }

  // We need to check the contents of value_trie for Katakana values.
  // If (key, value) = (かな, カナ) is in the dictionary, "カナ" is
  // not used as a key for value_trie or key_trie.  Only "かな" is
  // used as a key for key_trie.  If we accept this limitation, we can
  // skip the following code.
  //
  // If we add "if (key == value) return true;" here, we can check
  // almost all cases of Hiragana and Alphabet words without the
  // following iteration.  However, when (mozc, MOZC) is stored but
  // (mozc, mozc) is NOT stored, HasValue("mozc") wrongly returns
  // true.

  // Get the block of tokens for this key.
  const uint8_t* encoded_tokens_ptr = GetTokenArrayPtr(token_array_, key_id);

  // Check tokens.
  for (TokenDecodeIterator iter(codec_, value_trie_, frequent_pos_, key,
                                encoded_tokens_ptr);
       !iter.Done(); iter.Next()) {
    const Token* token = iter.Get().token;
    if (value == token->value) {
      return true;
    }
  }
  return false;
}

void SystemDictionary::CollectPredictiveNodesInBfsOrder(
    absl::string_view encoded_key, const KeyExpansionTable& table, size_t limit,
    std::vector<PredictiveLookupSearchState>* result) const {
  std::queue<PredictiveLookupSearchState> queue;
  queue.push(PredictiveLookupSearchState(LoudsTrie::Node(), 0, false));
  do {
    PredictiveLookupSearchState state = queue.front();
    queue.pop();

    // Update traversal state for |encoded_key| and its expanded keys.
    if (state.key_pos < encoded_key.size()) {
      const char target_char = encoded_key[state.key_pos];
      const ExpandedKey& chars = table.ExpandKey(target_char);

      for (key_trie_.MoveToFirstChild(&state.node);
           key_trie_.IsValidNode(state.node);
           key_trie_.MoveToNextSibling(&state.node)) {
        const char c = key_trie_.GetEdgeLabelToParentNode(state.node);
        if (!chars.IsHit(c)) {
          continue;
        }
        const int num_expanded =
            state.num_expanded + static_cast<int>(c != target_char);
        queue.push(PredictiveLookupSearchState(state.node, state.key_pos + 1,
                                               num_expanded));
      }
      continue;
    }

    // Collect prediction keys (state.key_pos >= encoded_key.size()).
    if (key_trie_.IsTerminalNode(state.node)) {
      result->push_back(state);
    }

    // Collected enough entries.  Collect all the remaining keys that have the
    // same length as the longest key.
    if (result->size() > limit) {
      // The current key is the longest because of BFS property.
      const size_t max_key_len = state.key_pos;
      while (!queue.empty()) {
        state = queue.front();
        queue.pop();
        if (state.key_pos > max_key_len) {
          // Key length in the queue is monotonically increasing because of BFS
          // property.  So we don't need to check all the elements in the queue.
          break;
        }
        DCHECK_EQ(state.key_pos, max_key_len);
        if (key_trie_.IsTerminalNode(state.node)) {
          result->push_back(state);
        }
      }
      break;
    }

    // Update traversal state for children.
    for (key_trie_.MoveToFirstChild(&state.node);
         key_trie_.IsValidNode(state.node);
         key_trie_.MoveToNextSibling(&state.node)) {
      queue.push(PredictiveLookupSearchState(state.node, state.key_pos + 1,
                                             state.num_expanded));
    }
  } while (!queue.empty());
}

void SystemDictionary::LookupPredictive(
    absl::string_view key, const ConversionRequest& conversion_request,
    Callback* callback) const {
  // Do nothing for empty key, although looking up all the entries with empty
  // string seems natural.
  if (key.empty()) {
    return;
  }

  std::string encoded_key;
  codec_->EncodeKey(key, &encoded_key);
  if (encoded_key.size() > LoudsTrie::kMaxDepth) {
    return;
  }

  const KeyExpansionTable& table =
      conversion_request.IsKanaModifierInsensitiveConversion()
          ? hiragana_expansion_table_
          : KeyExpansionTable::GetDefaultInstance();

  // TODO(noriyukit): Lookup limit should be implemented at caller side by using
  // callback mechanism.  This hard-coding limits the capability and generality
  // of dictionary module.  CollectPredictiveNodesInBfsOrder() and the following
  // loop for callback should be integrated for this purpose.
  constexpr size_t kLookupLimit = 64;
  std::vector<PredictiveLookupSearchState> result;
  result.reserve(kLookupLimit);
  CollectPredictiveNodesInBfsOrder(encoded_key, table, kLookupLimit, &result);

  // Reused buffer and instances inside the following loop.
  char encoded_actual_key_buffer[LoudsTrie::kMaxDepth + 1];
  std::string decoded_key, actual_key_str;
  decoded_key.reserve(key.size() * 2);
  actual_key_str.reserve(key.size() * 2);
  for (size_t i = 0; i < result.size(); ++i) {
    const PredictiveLookupSearchState& state = result[i];

    // Computes the actual key.  For example:
    // key = "くー"
    // encoded_actual_key = encode("ぐーぐる")  [expanded]
    // encoded_actual_key_prediction_suffix = encode("ぐる")
    const absl::string_view encoded_actual_key =
        key_trie_.RestoreKeyString(state.node, encoded_actual_key_buffer);
    const absl::string_view encoded_actual_key_prediction_suffix =
        absl::ClippedSubstr(encoded_actual_key, encoded_key.size(),
                            encoded_actual_key.size() - encoded_key.size());

    // decoded_key = "くーぐる" (= key + prediction suffix)
    decoded_key.clear();
    decoded_key.assign(key.data(), key.size());
    codec_->DecodeKey(encoded_actual_key_prediction_suffix, &decoded_key);
    switch (callback->OnKey(decoded_key)) {
      case Callback::TRAVERSE_DONE:
        return;
      case Callback::TRAVERSE_NEXT_KEY:
        continue;
      case DictionaryInterface::Callback::TRAVERSE_CULL:
        LOG(FATAL) << "Culling is not implemented.";
      default:
        break;
    }

    absl::string_view actual_key;
    if (state.num_expanded > 0) {
      actual_key_str.clear();
      codec_->DecodeKey(encoded_actual_key, &actual_key_str);
      actual_key = actual_key_str;
    } else {
      actual_key = decoded_key;
    }
    switch (
        callback->OnActualKey(decoded_key, actual_key, state.num_expanded)) {
      case Callback::TRAVERSE_DONE:
        return;
      case Callback::TRAVERSE_NEXT_KEY:
        continue;
      case Callback::TRAVERSE_CULL:
        LOG(FATAL) << "Culling is not implemented.";
      default:
        break;
    }

    const int key_id = key_trie_.GetKeyIdOfTerminalNode(state.node);
    for (TokenDecodeIterator iter(codec_, value_trie_, frequent_pos_,
                                  actual_key,
                                  GetTokenArrayPtr(token_array_, key_id));
         !iter.Done(); iter.Next()) {
      const TokenInfo& token_info = iter.Get();
      const Callback::ResultType result =
          callback->OnToken(decoded_key, actual_key, *token_info.token);
      if (result == Callback::TRAVERSE_DONE) {
        return;
      }
      if (result == Callback::TRAVERSE_NEXT_KEY) {
        break;
      }
      DCHECK_NE(Callback::TRAVERSE_CULL, result) << "Not implemented";
    }
  }
}

namespace {

// An implementation of prefix search without key expansion.  Runs |callback|
// for prefixes of |encoded_key| in |key_trie|.
// Args:
//   key_trie, value_trie, token_array, codec, frequent_pos:
//     Members in SystemDictionary.
//   key:
//     The head address of the original key before applying codec.
//   encoded_key:
//     The encoded |key|.
//   callback:
//     A callback function to be called.
//   token_filter:
//     A functor of signature bool(const TokenInfo &).  Only tokens for which
//     this functor returns true are passed to callback function.
template <typename Func>
void RunCallbackOnEachPrefix(const LoudsTrie& key_trie,
                             const LoudsTrie& value_trie,
                             const BitVectorBasedArray& token_array,
                             const SystemDictionaryCodecInterface* codec,
                             const uint32_t* frequent_pos, const char* key,
                             absl::string_view encoded_key,
                             DictionaryInterface::Callback* callback,
                             Func token_filter) {
  typedef DictionaryInterface::Callback Callback;
  LoudsTrie::Node node;
  for (absl::string_view::size_type i = 0; i < encoded_key.size();) {
    if (!key_trie.MoveToChildByLabel(encoded_key[i], &node)) {
      return;
    }
    ++i;  // Increment here for next loop and |encoded_prefix| defined below.
    if (!key_trie.IsTerminalNode(node)) {
      continue;
    }
    const absl::string_view encoded_prefix = encoded_key.substr(0, i);
    const absl::string_view prefix(key,
                                   codec->GetDecodedKeyLength(encoded_prefix));

    switch (callback->OnKey(prefix)) {
      case Callback::TRAVERSE_DONE:
      case Callback::TRAVERSE_CULL:
        return;
      case Callback::TRAVERSE_NEXT_KEY:
        continue;
      default:
        break;
    }

    switch (callback->OnActualKey(prefix, prefix, false)) {
      case Callback::TRAVERSE_DONE:
      case Callback::TRAVERSE_CULL:
        return;
      case Callback::TRAVERSE_NEXT_KEY:
        continue;
      default:
        break;
    }

    const int key_id = key_trie.GetKeyIdOfTerminalNode(node);
    for (TokenDecodeIterator iter(codec, value_trie, frequent_pos, prefix,
                                  GetTokenArrayPtr(token_array, key_id));
         !iter.Done(); iter.Next()) {
      const TokenInfo& token_info = iter.Get();
      if (!token_filter(token_info)) {
        continue;
      }
      const Callback::ResultType res =
          callback->OnToken(prefix, prefix, *token_info.token);
      if (res == Callback::TRAVERSE_DONE || res == Callback::TRAVERSE_CULL) {
        return;
      }
      if (res == Callback::TRAVERSE_NEXT_KEY) {
        break;
      }
    }
  }
}

struct SelectAllTokens {
  bool operator()(const TokenInfo& token_info) const { return true; }
};

class ReverseLookupCallbackWrapper : public DictionaryInterface::Callback {
 public:
  explicit ReverseLookupCallbackWrapper(DictionaryInterface::Callback* callback)
      : callback_(callback) {}
  ~ReverseLookupCallbackWrapper() override = default;
  SystemDictionary::Callback::ResultType OnToken(absl::string_view key,
                                                 absl::string_view actual_key,
                                                 const Token& token) override {
    Token modified_token = token;
    modified_token.key.swap(modified_token.value);
    return callback_->OnToken(key, actual_key, modified_token);
  }

  DictionaryInterface::Callback* callback_;
};

}  // namespace

// Recursive implementation of depth-first prefix search with key expansion.
// Input parameters:
//   key:
//     The head address of the original key before applying codec.
//   encoded_key:
//     The encoded |key|.
//   table:
//     Key expansion table.
//   callback:
//     A callback function to be called.
// Parameters for recursion:
//   node:
//     Stores the current location in |key_trie_|.
//   key_pos:
//     Depth of node, i.e., encoded_key.substr(0, key_pos) is the current prefix
//     for search.
//   num_expanded:
//     the number of chharacters expanded. if non zero, characters are expanded.
//   actual_key_buffer:
//     Buffer for storing actually used characters to reach this node, i.e.,
//     absl::string_view(actual_key_buffer, key_pos) is the matched prefix using
//     key expansion.
//   actual_prefix:
//     A reused string for decoded actual key.  This is just for performance
//     purpose.
DictionaryInterface::Callback::ResultType
SystemDictionary::LookupPrefixWithKeyExpansionImpl(
    const char* key, absl::string_view encoded_key,
    const KeyExpansionTable& table, Callback* callback, LoudsTrie::Node node,
    absl::string_view::size_type key_pos, int num_expanded,
    char* actual_key_buffer, std::string* actual_prefix) const {
  // This do-block handles a terminal node and callback.  do-block is used to
  // break the block and continue to the subsequent traversal phase.
  do {
    if (!key_trie_.IsTerminalNode(node)) {
      break;
    }

    const absl::string_view encoded_prefix = encoded_key.substr(0, key_pos);
    const absl::string_view prefix(key,
                                   codec_->GetDecodedKeyLength(encoded_prefix));
    Callback::ResultType result = callback->OnKey(prefix);
    if (result == Callback::TRAVERSE_DONE ||
        result == Callback::TRAVERSE_CULL) {
      return result;
    }
    if (result == Callback::TRAVERSE_NEXT_KEY) {
      break;  // Go to the traversal phase.
    }

    const absl::string_view encoded_actual_prefix(actual_key_buffer, key_pos);
    actual_prefix->clear();
    codec_->DecodeKey(encoded_actual_prefix, actual_prefix);
    result = callback->OnActualKey(prefix, *actual_prefix, num_expanded);
    if (result == Callback::TRAVERSE_DONE ||
        result == Callback::TRAVERSE_CULL) {
      return result;
    }
    if (result == Callback::TRAVERSE_NEXT_KEY) {
      break;  // Go to the traversal phase.
    }

    const int key_id = key_trie_.GetKeyIdOfTerminalNode(node);
    for (TokenDecodeIterator iter(codec_, value_trie_, frequent_pos_,
                                  *actual_prefix,
                                  GetTokenArrayPtr(token_array_, key_id));
         !iter.Done(); iter.Next()) {
      const TokenInfo& token_info = iter.Get();
      result = callback->OnToken(prefix, *actual_prefix, *token_info.token);
      if (result == Callback::TRAVERSE_DONE ||
          result == Callback::TRAVERSE_CULL) {
        return result;
      }
      if (result == Callback::TRAVERSE_NEXT_KEY) {
        break;  // Go to the traversal phase.
      }
    }
  } while (false);

  // Traversal phase.
  if (key_pos == encoded_key.size()) {
    return Callback::TRAVERSE_CONTINUE;
  }
  const char current_char = encoded_key[key_pos];
  const ExpandedKey& chars = table.ExpandKey(current_char);
  for (key_trie_.MoveToFirstChild(&node); key_trie_.IsValidNode(node);
       key_trie_.MoveToNextSibling(&node)) {
    const char c = key_trie_.GetEdgeLabelToParentNode(node);
    if (!chars.IsHit(c)) {
      continue;
    }
    actual_key_buffer[key_pos] = c;
    const Callback::ResultType result = LookupPrefixWithKeyExpansionImpl(
        key, encoded_key, table, callback, node, key_pos + 1,
        num_expanded + static_cast<int>(c != current_char), actual_key_buffer,
        actual_prefix);
    if (result == Callback::TRAVERSE_DONE) {
      return Callback::TRAVERSE_DONE;
    }
  }

  return Callback::TRAVERSE_CONTINUE;
}

void SystemDictionary::LookupPrefix(absl::string_view key,
                                    const ConversionRequest& conversion_request,
                                    Callback* callback) const {
  std::string encoded_key;
  codec_->EncodeKey(key, &encoded_key);

  if (!conversion_request.IsKanaModifierInsensitiveConversion()) {
    RunCallbackOnEachPrefix(key_trie_, value_trie_, token_array_, codec_,
                            frequent_pos_, key.data(), encoded_key, callback,
                            SelectAllTokens());
    return;
  }

  char actual_key_buffer[LoudsTrie::kMaxDepth + 1];
  std::string actual_prefix;
  actual_prefix.reserve(key.size() * 3);
  LookupPrefixWithKeyExpansionImpl(
      key.data(), encoded_key, hiragana_expansion_table_, callback,
      LoudsTrie::Node(), 0, false, actual_key_buffer, &actual_prefix);
}

void SystemDictionary::LookupExact(absl::string_view key,
                                   const ConversionRequest& conversion_request,
                                   Callback* callback) const {
  // Find the key in the key trie.
  std::string encoded_key;
  codec_->EncodeKey(key, &encoded_key);
  const int key_id = key_trie_.ExactSearch(encoded_key);
  if (key_id == -1) {
    return;
  }
  if (callback->OnKey(key) != Callback::TRAVERSE_CONTINUE) {
    return;
  }
  if (callback->OnActualKey(key, key, /* num_expanded= */ 0) !=
      Callback::TRAVERSE_CONTINUE) {
    return;
  }
  // Callback on each token.
  for (TokenDecodeIterator iter(codec_, value_trie_, frequent_pos_, key,
                                GetTokenArrayPtr(token_array_, key_id));
       !iter.Done(); iter.Next()) {
    if (callback->OnToken(key, key, *iter.Get().token) !=
        Callback::TRAVERSE_CONTINUE) {
      break;
    }
  }
}

void SystemDictionary::LookupReverse(
    absl::string_view str, const ConversionRequest& conversion_request,
    Callback* callback) const {
  // 1st step: Hiragana/Katakana are not in the value trie
  // 2nd step: Reverse lookup in value trie
  ReverseLookupCallbackWrapper callback_wrapper(callback);
  RegisterReverseLookupTokensForT13N(str, &callback_wrapper);
  RegisterReverseLookupTokensForValue(str, &callback_wrapper);
}

namespace {

class AddKeyIdsToSet {
 public:
  explicit AddKeyIdsToSet(absl::btree_set<int>* output) : output_(output) {}

  void operator()(absl::string_view key, size_t prefix_len,
                  const LoudsTrie& trie, LoudsTrie::Node node) {
    output_->insert(trie.GetKeyIdOfTerminalNode(node));
  }

 private:
  absl::btree_set<int>* output_;
};

inline void AddKeyIdsOfAllPrefixes(const LoudsTrie& trie, absl::string_view key,
                                   absl::btree_set<int>* key_ids) {
  trie.PrefixSearch(key, AddKeyIdsToSet(key_ids));
}

}  // namespace

void SystemDictionary::PopulateReverseLookupCache(absl::string_view str) const {
  if (reverse_lookup_index_ != nullptr) {
    // We don't need to prepare cache for the current reverse conversion,
    // as we have already built the index for reverse lookup.
    return;
  }

  auto reverse_lookup_cache = std::make_shared<ReverseLookupCache>();

  // Iterate each suffix and collect IDs of all substrings.
  absl::btree_set<int> id_set;
  int pos = 0;
  std::string lookup_key;
  lookup_key.reserve(str.size());
  while (pos < str.size()) {
    const absl::string_view suffix = absl::ClippedSubstr(str, pos);
    lookup_key.clear();
    codec_->EncodeValue(suffix, &lookup_key);
    AddKeyIdsOfAllPrefixes(value_trie_, lookup_key, &id_set);
    pos += strings::OneCharLen(suffix.data());
  }
  // Collect tokens for all IDs.
  ScanTokens(id_set, reverse_lookup_cache.get());

  reverse_lookup_cache_.store(std::move(reverse_lookup_cache));
}

void SystemDictionary::ClearReverseLookupCache() const {
  reverse_lookup_cache_.store(nullptr);
}

namespace {

class FilterTokenForRegisterReverseLookupTokensForT13N {
 public:
  FilterTokenForRegisterReverseLookupTokensForT13N() {
    tmp_str_.reserve(LoudsTrie::kMaxDepth * 3);
  }

  bool operator()(const TokenInfo& token_info) {
    // Skip spelling corrections.
    if (token_info.token->attributes & Token::SPELLING_CORRECTION) {
      return false;
    }
    if (token_info.value_type != TokenInfo::AS_IS_HIRAGANA &&
        token_info.value_type != TokenInfo::AS_IS_KATAKANA) {
      // SAME_AS_PREV_VALUE may be t13n token.
      tmp_str_ = japanese_util::KatakanaToHiragana(token_info.token->value);
      if (token_info.token->key != tmp_str_) {
        return false;
      }
    }
    return true;
  }

 private:
  std::string tmp_str_;
};

}  // namespace

void SystemDictionary::RegisterReverseLookupTokensForT13N(
    absl::string_view value, Callback* callback) const {
  std::string hiragana_value = japanese_util::KatakanaToHiragana(value);
  std::string encoded_key;
  codec_->EncodeKey(hiragana_value, &encoded_key);
  RunCallbackOnEachPrefix(key_trie_, value_trie_, token_array_, codec_,
                          frequent_pos_, hiragana_value.data(), encoded_key,
                          callback,
                          FilterTokenForRegisterReverseLookupTokensForT13N());
}

void SystemDictionary::RegisterReverseLookupTokensForValue(
    absl::string_view value, Callback* callback) const {
  std::string lookup_key;
  codec_->EncodeValue(value, &lookup_key);

  absl::btree_set<int> id_set;
  AddKeyIdsOfAllPrefixes(value_trie_, lookup_key, &id_set);

  const ReverseLookupCache* results = nullptr;
  ReverseLookupCache non_cached_results;
  std::shared_ptr<ReverseLookupCache> cached_results;
  if (reverse_lookup_index_ != nullptr) {
    reverse_lookup_index_->FillResultMap(id_set, &non_cached_results.results);
    results = &non_cached_results;
  } else if (cached_results = reverse_lookup_cache_.load();
             (cached_results && cached_results->IsAvailable(id_set))) {
    results = cached_results.get();
  } else {
    // Cache is not available. Get token for each ID.
    ScanTokens(id_set, &non_cached_results);
    results = &non_cached_results;
  }
  DCHECK(results != nullptr);

  RegisterReverseLookupResults(id_set, *results, callback);
}

void SystemDictionary::ScanTokens(const absl::btree_set<int>& id_set,
                                  ReverseLookupCache* cache) const {
  for (TokenScanIterator iter(codec_, token_array_); !iter.Done();
       iter.Next()) {
    const TokenScanIterator::Result& result = iter.Get();
    if (result.value_id != -1 && id_set.contains(result.value_id)) {
      ReverseLookupResult lookup_result;
      lookup_result.tokens_offset = result.tokens_offset;
      lookup_result.id_in_key_trie = result.index;
      cache->results.emplace(result.value_id, lookup_result);
    }
  }
}

void SystemDictionary::RegisterReverseLookupResults(
    const absl::btree_set<int>& id_set, const ReverseLookupCache& cache,
    Callback* callback) const {
  const uint8_t* encoded_tokens_ptr = GetTokenArrayPtr(token_array_, 0);
  char buffer[LoudsTrie::kMaxDepth + 1];
  for (const int value_id : id_set) {
    const auto range = cache.results.equal_range(value_id);
    for (auto result_itr = range.first; result_itr != range.second;
         ++result_itr) {
      const ReverseLookupResult& reverse_result = result_itr->second;

      const absl::string_view encoded_key =
          key_trie_.RestoreKeyString(reverse_result.id_in_key_trie, buffer);
      std::string tokens_key;
      codec_->DecodeKey(encoded_key, &tokens_key);
      if (callback->OnKey(tokens_key) != Callback::TRAVERSE_CONTINUE) {
        continue;
      }
      for (TokenDecodeIterator iter(
               codec_, value_trie_, frequent_pos_, tokens_key,
               encoded_tokens_ptr + reverse_result.tokens_offset);
           !iter.Done(); iter.Next()) {
        const TokenInfo& token_info = iter.Get();
        if (token_info.token->attributes & Token::SPELLING_CORRECTION ||
            token_info.id_in_value_trie != value_id) {
          continue;
        }
        callback->OnToken(tokens_key, tokens_key, *token_info.token);
      }
    }
  }
}

}  // namespace dictionary
}  // namespace mozc
