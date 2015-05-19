// Copyright 2010-2014, Google Inc.
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
#include "converter/node_allocator.h"
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

class ReverseLookupCache : public NodeAllocatorData::Data {
 public:
  multimap<int, SystemDictionary::ReverseLookupResult> results;
};

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
      StringPiece key,
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

    if (token_info_.accent_encoding_type == TokenInfo::EMBEDDED_IN_TOKEN) {
      token_.value += "_" + Util::StringPrintf("%d", token_info_.accent_type);
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

// Iterator for scanning token array.
// This iterator does not return actual token info but returns
// id data and the position only.
// This will be used only for reverse lookup.
// Forward lookup does not need such iterator because it can access
// a token directly without linear scan.
//
//  Usage:
//    for (TokenScanIterator iter(codec_, token_array_.get());
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

  TokenScanIterator(
      const SystemDictionaryCodecInterface *codec,
      const storage::louds::BitVectorBasedArray *token_array)
      : codec_(codec),
        termination_flag_(codec->GetTokensTerminationFlag()),
        state_(HAS_NEXT),
        offset_(0),
        tokens_offset_(0),
        index_(0) {
    size_t dummy_length = 0;
    encoded_tokens_ptr_ =
        reinterpret_cast<const uint8*>(token_array->Get(0, &dummy_length));
    NextInternal();
  }

  ~TokenScanIterator() {
  }

  const Result &Get() const { return result_; }

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
    const bool is_last_token =
        !(codec_->ReadTokenForReverseLookup(encoded_tokens_ptr_ + offset_,
                                            &result_.value_id, &read_bytes));
    if (is_last_token) {
      int tokens_size = offset_ + read_bytes - tokens_offset_;
      if (tokens_size < kMinRbxBlobSize) {
        tokens_size = kMinRbxBlobSize;
      }
      tokens_offset_ += tokens_size;
      ++index_;
      offset_ = tokens_offset_;
    } else {
      offset_ += read_bytes;
    }
  }

  const SystemDictionaryCodecInterface *codec_;
  const uint8 *encoded_tokens_ptr_;
  const uint8 termination_flag_;
  State state_;
  Result result_;
  int offset_;
  int tokens_offset_;
  int index_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TokenScanIterator);
};

}  // namespace

class ReverseLookupIndex {
 public:
  ReverseLookupIndex(
      const SystemDictionaryCodecInterface *codec,
      const storage::louds::BitVectorBasedArray *token_array) {
    // Gets id size.
    int value_id_max = -1;
    for (TokenScanIterator iter(codec, token_array);
         !iter.Done(); iter.Next()) {
      const TokenScanIterator::Result &result = iter.Get();
      value_id_max = max(value_id_max, result.value_id);
    }

    CHECK_GE(value_id_max, 0);
    index_size_ = value_id_max + 1;
    index_.reset(new ReverseLookupResultArray[index_size_]);

    // Gets result size for each ids.
    for (TokenScanIterator iter(codec, token_array);
         !iter.Done(); iter.Next()) {
      const TokenScanIterator::Result &result = iter.Get();
      if (result.value_id != -1) {
        DCHECK_LT(result.value_id, index_size_);
        ++(index_[result.value_id].size);
      }
    }

    for (size_t i = 0; i < index_size_; ++i) {
      index_[i].results.reset(
          new SystemDictionary::ReverseLookupResult[index_[i].size]);
    }

    // Builds index.
    for (TokenScanIterator iter(codec, token_array);
         !iter.Done(); iter.Next()) {
      const TokenScanIterator::Result &result = iter.Get();
      if (result.value_id == -1) {
        continue;
      }

      DCHECK_LT(result.value_id, index_size_);
      ReverseLookupResultArray *result_array = &index_[result.value_id];

      // Finds uninitialized result.
      size_t result_index = 0;
      for (result_index = 0;
           result_index < result_array->size;
           ++result_index) {
        const SystemDictionary::ReverseLookupResult &lookup_result =
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

    CHECK(index_.get() != NULL);
  }

  ~ReverseLookupIndex() {}

  void FillResultMap(
      const set<int> &id_set,
      multimap<int, SystemDictionary::ReverseLookupResult> *result_map) {
    for (set<int>::const_iterator id_itr  = id_set.begin();
         id_itr != id_set.end(); ++id_itr) {
      const ReverseLookupResultArray &result_array = index_[*id_itr];
      for (size_t i = 0; i < result_array.size; ++i) {
        result_map->insert(make_pair(*id_itr, result_array.results[i]));
      }
    }
  }

 private:
  struct ReverseLookupResultArray {
    ReverseLookupResultArray() : size(0) {}
    // Use scoped_ptr for reducing memory consumption as possible.
    // Using vector requires 90 MB even when we call resize explicitly.
    // On the other hand, scoped_ptr requires 57 MB.
    scoped_ptr<SystemDictionary::ReverseLookupResult[]> results;
    size_t size;
  };

  // Use scoped array for reducing memory consumption as possible.
  scoped_ptr<ReverseLookupResultArray[]> index_;
  size_t index_size_;

  DISALLOW_COPY_AND_ASSIGN(ReverseLookupIndex);
};

SystemDictionary::Builder::Builder(const string &filename)
    : type_(FILENAME), filename_(filename),
      ptr_(NULL), len_(-1), options_(NONE), codec_(NULL)  {}

SystemDictionary::Builder::Builder(const char *ptr, int len)
    : type_(IMAGE), filename_(""),
      ptr_(ptr), len_(len), options_(NONE), codec_(NULL) {}

SystemDictionary::Builder::~Builder() {}

void SystemDictionary::Builder::SetOptions(Options options) {
  options_ = options;
}

// This does not have the ownership of |codec|
void SystemDictionary::Builder::SetCodec(
    const SystemDictionaryCodecInterface *codec) {
  codec_ = codec;
}

SystemDictionary *SystemDictionary::Builder::Build() {
  if (codec_ == NULL) {
    codec_ = SystemDictionaryCodecFactory::GetCodec();
  }

  scoped_ptr<SystemDictionary> instance(new SystemDictionary(codec_));

  switch (type_) {
    case FILENAME:
      if (!instance->dictionary_file_->OpenFromFile(filename_)) {
        LOG(ERROR) << "Failed to open system dictionary file";
        return NULL;
      }
      break;
    case IMAGE:
      // Make the dictionary not to be paged out.
      // We don't check the return value because the process doesn't necessarily
      // has the priviledge to mlock.
      // Note that we don't munlock the space because it's always better to keep
      // the singleton system dictionary paged in as long as the process runs.
      SystemUtil::MaybeMLock(ptr_, len_);
      if (!instance->dictionary_file_->OpenFromImage(ptr_, len_)) {
        LOG(ERROR) << "Failed to open system dictionary file";
        return NULL;
      }
      break;
    default:
      LOG(ERROR) << "Invalid input type.";
      return NULL;
  }

  if (!instance->OpenDictionaryFile(
          (options_ & ENABLE_REVERSE_LOOKUP_INDEX) != 0)) {
    LOG(ERROR) << "Failed to create system dictionary";
    return NULL;
  }

  return instance.release();
}

SystemDictionary::SystemDictionary(const SystemDictionaryCodecInterface *codec)
    : key_trie_(new LoudsTrie),
      value_trie_(new LoudsTrie),
      token_array_(new BitVectorBasedArray),
      dictionary_file_(new DictionaryFile),
      frequent_pos_(NULL),
      codec_(codec) {}

SystemDictionary::~SystemDictionary() {}

// static
SystemDictionary *SystemDictionary::CreateSystemDictionaryFromFileWithOptions(
    const string &filename, Options options) {
  Builder builder(filename);
  builder.SetOptions(options);
  return builder.Build();
}

// static
SystemDictionary *SystemDictionary::CreateSystemDictionaryFromFile(
    const string &filename) {
  return CreateSystemDictionaryFromFileWithOptions(filename, NONE);
}

// static
SystemDictionary *SystemDictionary::CreateSystemDictionaryFromImageWithOptions(
    const char *ptr, int len, Options options) {
  Builder builder(ptr, len);
  builder.SetOptions(options);
  return builder.Build();
}

// static
SystemDictionary *SystemDictionary::CreateSystemDictionaryFromImage(
    const char *ptr, int len) {
  return CreateSystemDictionaryFromImageWithOptions(ptr, len, NONE);
}

bool SystemDictionary::OpenDictionaryFile(bool enable_reverse_lookup_index) {
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

  if (enable_reverse_lookup_index) {
    InitReverseLookupIndex();
  }

  return true;
}

void SystemDictionary::InitReverseLookupIndex() {
  if (reverse_lookup_index_.get() != NULL) {
    return;
  }

  reverse_lookup_index_.reset(
      new ReverseLookupIndex(codec_, token_array_.get()));
}

bool SystemDictionary::HasValue(StringPiece value) const {
  string encoded_value;
  codec_->EncodeValue(value, &encoded_value);
  if (value_trie_->ExactSearch(encoded_value) != -1) {
    return true;
  }

  // Because Hiragana, Katakana and Alphabet words are not stored in the
  // value_trie for the data compression.  They are only stored in the
  // key_trie with flags.  So we also need to check the existence in
  // the key_trie.

  // Normalize the value as the key.  This process depends on the
  // implementation of SystemDictionaryBuilder::BuildValueTrie.
  string key;
  Util::KatakanaToHiragana(value, &key);

  string encoded_key;
  codec_->EncodeKey(key, &encoded_key);
  const int key_id = key_trie_->ExactSearch(encoded_key);
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
  size_t length = 0;  // unused
  const uint8 *encoded_tokens_ptr = reinterpret_cast<const uint8*>(
      token_array_->Get(key_id, &length));

  // Check tokens.
  for (TokenDecodeIterator iter(
           codec_, value_trie_.get(), frequent_pos_, key, encoded_tokens_ptr);
       !iter.Done(); iter.Next()) {
    const Token *token = iter.Get().token;
    if (value == token->value) {
      return true;
    }
  }
  return false;
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
                    StringPiece original_encoded_key,
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

void SystemDictionary::LookupPredictive(
    StringPiece key, bool use_kana_modifier_insensitive_lookup,
    Callback *callback) const {
  // Do nothing for empty key, although looking up all the entries with empty
  // string seems natural.
  if (key.empty()) {
    return;
  }

  string lookup_key_str;
  codec_->EncodeKey(key, &lookup_key_str);
  if (lookup_key_str.size() > LoudsTrie::kMaxDepth) {
    return;
  }

  // First, collect up to 64 keys so that results are as short as possible,
  // which emulates BFS over trie.
  ShortKeyCollector collector(codec_, lookup_key_str, 0, 64);
  const KeyExpansionTable &table =
      use_kana_modifier_insensitive_lookup ?
      hiragana_expansion_table_ : KeyExpansionTable::GetDefaultInstance();
  key_trie_->PredictiveSearchWithKeyExpansion(lookup_key_str.c_str(), table,
                                              &collector);

  string decoded_key, actual_key;
  for (size_t i = 0; i < collector.entry_list().size(); ++i) {
    const ShortKeyCollector::Entry &entry = collector.entry_list()[i];

    decoded_key.clear();
    codec_->DecodeKey(entry.encoded_key, &decoded_key);
    switch (callback->OnKey(decoded_key)) {
      case DictionaryInterface::Callback::TRAVERSE_DONE:
        return;
      case DictionaryInterface::Callback::TRAVERSE_NEXT_KEY:
        continue;
      case DictionaryInterface::Callback::TRAVERSE_CULL:
        LOG(FATAL) << "Culling is not implemented.";
        continue;
      default:
        break;
    }

    actual_key.clear();
    codec_->DecodeKey(entry.encoded_actual_key, &actual_key);
    const bool is_expanded = entry.encoded_key != entry.encoded_actual_key;
    switch (callback->OnActualKey(decoded_key, actual_key, is_expanded)) {
      case DictionaryInterface::Callback::TRAVERSE_DONE:
        return;
      case DictionaryInterface::Callback::TRAVERSE_NEXT_KEY:
        continue;
      case DictionaryInterface::Callback::TRAVERSE_CULL:
        LOG(FATAL) << "Culling is not implemented.";
        continue;
      default:
        break;
    }

    size_t dummy_length = 0;
    const uint8 *encoded_tokens_ptr = reinterpret_cast<const uint8*>(
        token_array_->Get(entry.key_id, &dummy_length));
    for (TokenDecodeIterator iter(codec_, value_trie_.get(),
                                  frequent_pos_, actual_key,
                                  encoded_tokens_ptr);
         !iter.Done(); iter.Next()) {
      const TokenInfo &token_info = iter.Get();
      const DictionaryInterface::Callback::ResultType result =
          callback->OnToken(decoded_key, actual_key, *token_info.token);
      if (result == DictionaryInterface::Callback::TRAVERSE_DONE) {
        return;
      }
      if (result == DictionaryInterface::Callback::TRAVERSE_NEXT_KEY) {
        break;
      }
      DCHECK_NE(DictionaryInterface::Callback::TRAVERSE_CULL, result)
          << "Culling is not implemented.";
    }
  }
}

namespace {

// A general purpose traverser for prefix search over the system dictionary.
class PrefixTraverser : public LoudsTrie::Callback {
 public:
  PrefixTraverser(const BitVectorBasedArray *token_array,
                  const LoudsTrie *value_trie,
                  const SystemDictionaryCodecInterface *codec,
                  const uint32 *frequent_pos,
                  StringPiece original_encoded_key,
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
    string key, actual_key;
    SystemDictionary::Callback::ResultType result = RunOnKeyAndOnActualKey(
        trie_key, trie_key_len, key_id, &key, &actual_key);
    if (result != SystemDictionary::Callback::TRAVERSE_CONTINUE) {
      return ConvertResultType(result);
    }

    // Decode tokens and call back OnToken() for each token.
    size_t dummy_length = 0;
    const uint8 *encoded_tokens_ptr = reinterpret_cast<const uint8*>(
        token_array_->Get(key_id, &dummy_length));
    for (TokenDecodeIterator iter(
             codec_, value_trie_, frequent_pos_,
             actual_key, encoded_tokens_ptr);
         !iter.Done(); iter.Next()) {
      const TokenInfo &token_info = iter.Get();
      result = callback_->OnToken(key, actual_key, *token_info.token);
      if (result != SystemDictionary::Callback::TRAVERSE_CONTINUE) {
        return ConvertResultType(result);
      }
    }
    return SEARCH_CONTINUE;
  }

 protected:
  SystemDictionary::Callback::ResultType RunOnKeyAndOnActualKey(
      const char *trie_key, size_t trie_key_len, int key_id,
      string *key, string *actual_key) {
    // Decode key and call back OnKey().
    const StringPiece encoded_key =
        original_encoded_key_.substr(0, trie_key_len);
    codec_->DecodeKey(encoded_key, key);
    SystemDictionary::Callback::ResultType result = callback_->OnKey(*key);
    if (result != SystemDictionary::Callback::TRAVERSE_CONTINUE) {
      return result;
    }

    // Decode actual key (expanded key) and call back OnActualKey().  To check
    // if the actual key is expanded, compare the keys in encoded domain for
    // performance (this is guaranteed as codec is a bijection).
    const StringPiece encoded_actual_key(trie_key, trie_key_len);
    actual_key->reserve(key->size());
    codec_->DecodeKey(encoded_actual_key, actual_key);
    const bool is_expanded = encoded_actual_key != encoded_key;
    DCHECK_EQ(is_expanded, *key != *actual_key);
    return callback_->OnActualKey(*key, *actual_key, is_expanded);
  }

  const BitVectorBasedArray *token_array_;
  const LoudsTrie *value_trie_;
  const SystemDictionaryCodecInterface *codec_;
  const uint32 *frequent_pos_;
  const StringPiece original_encoded_key_;
  SystemDictionary::Callback *callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrefixTraverser);
};

class ReverseLookupCallbackWrapper : public DictionaryInterface::Callback {
 public:
  explicit ReverseLookupCallbackWrapper(DictionaryInterface::Callback *callback)
      : callback_(callback) {}
  virtual ~ReverseLookupCallbackWrapper() {}
  virtual SystemDictionary::Callback::ResultType OnToken(
      StringPiece key, StringPiece actual_key, const Token &token) {
    Token modified_token = token;
    modified_token.key.swap(modified_token.value);
    return callback_->OnToken(key, actual_key, modified_token);
  }

  DictionaryInterface::Callback *callback_;
};

}  // namespace

void SystemDictionary::LookupPrefix(
    StringPiece key,
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

void SystemDictionary::LookupExact(StringPiece key, Callback *callback) const {
  // Find the key in the key trie.
  string encoded_key;
  codec_->EncodeKey(key, &encoded_key);
  const int key_id = key_trie_->ExactSearch(encoded_key);
  if (key_id == -1) {
    return;
  }
  if (callback->OnKey(key) != Callback::TRAVERSE_CONTINUE) {
    return;
  }

  // Get the block of tokens for this key.
  size_t dummy_length = 0;
  const uint8 *encoded_tokens_ptr = reinterpret_cast<const uint8*>(
      token_array_->Get(key_id, &dummy_length));

  // Callback on each token.
  for (TokenDecodeIterator iter(
           codec_, value_trie_.get(), frequent_pos_, key, encoded_tokens_ptr);
       !iter.Done(); iter.Next()) {
    if (callback->OnToken(key, key, *iter.Get().token) !=
        Callback::TRAVERSE_CONTINUE) {
      break;
    }
  }
}

void SystemDictionary::LookupReverse(
    StringPiece str, NodeAllocatorInterface *allocator,
    Callback *callback) const {
  // 1st step: Hiragana/Katakana are not in the value trie
  // 2nd step: Reverse lookup in value trie
  ReverseLookupCallbackWrapper callback_wrapper(callback);
  RegisterReverseLookupTokensForT13N(str, &callback_wrapper);
  RegisterReverseLookupTokensForValue(str, allocator, &callback_wrapper);
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
    StringPiece str, NodeAllocatorInterface *allocator) const {
  if (allocator == NULL) {
    return;
  }
  if (reverse_lookup_index_ != NULL) {
    // We don't need to prepare cache for the current reverse conversion,
    // as we have already built the index for reverse lookup.
    return;
  }
  ReverseLookupCache *cache =
      allocator->mutable_data()->get<ReverseLookupCache>(kReverseLookupCache);
  DCHECK(cache) << "can't get cache data.";

  IdCollector id_collector;
  int pos = 0;
  // Iterate each suffix and collect IDs of all substrings.
  while (pos < str.size()) {
    const StringPiece suffix(str, pos);
    string lookup_key;
    codec_->EncodeValue(suffix, &lookup_key);
    value_trie_->PrefixSearch(lookup_key.c_str(), &id_collector);
    pos += Util::OneCharLen(suffix.data());
  }
  // Collect tokens for all IDs.
  ScanTokens(id_collector.id_set(), &cache->results);
}

void SystemDictionary::ClearReverseLookupCache(
    NodeAllocatorInterface *allocator) const {
  allocator->mutable_data()->erase(kReverseLookupCache);
}

namespace {

// A traverser for prefix search over T13N entries.
class T13nPrefixTraverser : public PrefixTraverser {
 public:
  T13nPrefixTraverser(const BitVectorBasedArray *token_array,
                      const LoudsTrie *value_trie,
                      const SystemDictionaryCodecInterface *codec,
                      const uint32 *frequent_pos,
                      StringPiece original_encoded_key,
                      SystemDictionary::Callback *callback)
      : PrefixTraverser(token_array, value_trie, codec, frequent_pos,
                        original_encoded_key, callback) {}

  virtual ResultType Run(const char *trie_key,
                         size_t trie_key_len, int key_id) {
    string key, actual_key;
    SystemDictionary::Callback::ResultType result = RunOnKeyAndOnActualKey(
        trie_key, trie_key_len, key_id, &key, &actual_key);
    if (result != SystemDictionary::Callback::TRAVERSE_CONTINUE) {
      return ConvertResultType(result);
    }

    // Decode tokens and call back OnToken() for each T13N token.
    size_t dummy_length = 0;
    const uint8 *encoded_tokens_ptr = reinterpret_cast<const uint8*>(
        token_array_->Get(key_id, &dummy_length));
    for (TokenDecodeIterator iter(
             codec_, value_trie_, frequent_pos_,
             actual_key, encoded_tokens_ptr);
         !iter.Done(); iter.Next()) {
      const TokenInfo &token_info = iter.Get();
      // Skip spelling corrections.
      if (token_info.token->attributes & Token::SPELLING_CORRECTION) {
        continue;
      }
      if (token_info.value_type != TokenInfo::AS_IS_HIRAGANA &&
          token_info.value_type != TokenInfo::AS_IS_KATAKANA) {
        // SAME_AS_PREV_VALUE may be t13n token.
        string hiragana;
        Util::KatakanaToHiragana(token_info.token->value, &hiragana);
        if (token_info.token->key != hiragana) {
          continue;
        }
      }
      result = callback_->OnToken(key, actual_key, *token_info.token);
      if (result != SystemDictionary::Callback::TRAVERSE_CONTINUE) {
        return ConvertResultType(result);
      }
    }
    return SEARCH_CONTINUE;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(T13nPrefixTraverser);
};

}  // namespace

void SystemDictionary::RegisterReverseLookupTokensForT13N(
    StringPiece value, Callback *callback) const {
  string hiragana, original_encoded_key;
  Util::KatakanaToHiragana(value, &hiragana);
  codec_->EncodeKey(hiragana, &original_encoded_key);
  T13nPrefixTraverser traverser(token_array_.get(), value_trie_.get(), codec_,
                                frequent_pos_, original_encoded_key, callback);
  key_trie_->PrefixSearchWithKeyExpansion(
      original_encoded_key.c_str(), KeyExpansionTable::GetDefaultInstance(),
      &traverser);
}

void SystemDictionary::RegisterReverseLookupTokensForValue(
    StringPiece value, NodeAllocatorInterface *allocator,
    Callback *callback) const {
  string lookup_key;
  codec_->EncodeValue(value, &lookup_key);

  IdCollector id_collector;
  value_trie_->PrefixSearch(lookup_key.c_str(), &id_collector);
  const set<int> &id_set = id_collector.id_set();

  const bool has_cache = (allocator != NULL &&
                          allocator->data().has(kReverseLookupCache));
  ReverseLookupCache *cache =
      (has_cache ? allocator->mutable_data()->get<ReverseLookupCache>(
          kReverseLookupCache) : NULL);

  multimap<int, ReverseLookupResult> *results = NULL;
  multimap<int, ReverseLookupResult> non_cached_results;
  if (reverse_lookup_index_ != NULL) {
    reverse_lookup_index_->FillResultMap(id_set, &non_cached_results);
    results = &non_cached_results;
  } else if (cache != NULL && IsCacheAvailable(id_set, cache->results)) {
    results = &(cache->results);
  } else {
    // Cache is not available. Get token for each ID.
    ScanTokens(id_set, &non_cached_results);
    results = &non_cached_results;
  }
  DCHECK(results != NULL);

  RegisterReverseLookupResults(id_set, *results, callback);
}

void SystemDictionary::ScanTokens(
    const set<int> &id_set,
    multimap<int, ReverseLookupResult> *reverse_results) const {
  for (TokenScanIterator iter(codec_, token_array_.get());
       !iter.Done(); iter.Next()) {
    const TokenScanIterator::Result &result = iter.Get();
    if (result.value_id != -1 &&
        id_set.find(result.value_id) != id_set.end()) {
      ReverseLookupResult lookup_result;
      lookup_result.tokens_offset = result.tokens_offset;
      lookup_result.id_in_key_trie = result.index;
      reverse_results->insert(make_pair(result.value_id, lookup_result));
    }
  }
}

void SystemDictionary::RegisterReverseLookupResults(
    const set<int> &id_set,
    const multimap<int, ReverseLookupResult> &reverse_results,
    Callback *callback) const {
  size_t dummy_length = 0;
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
      const ReverseLookupResult &reverse_result = result_itr->second;

      const char *encoded_key =
          key_trie_->Reverse(reverse_result.id_in_key_trie, buffer);
      const size_t encoded_key_len =
          LoudsTrie::kMaxDepth - (encoded_key - buffer);
      DCHECK_EQ(encoded_key_len, strlen(encoded_key));
      string tokens_key;
      codec_->DecodeKey(StringPiece(encoded_key, encoded_key_len), &tokens_key);
      if (callback->OnKey(tokens_key) !=
              SystemDictionary::Callback::TRAVERSE_CONTINUE) {
        continue;
      }

      // actual_key is always the same as tokens_key for reverse conversions.
      RegisterTokens(
          filter,
          tokens_key,
          tokens_key,
          encoded_tokens_ptr + reverse_result.tokens_offset,
          callback);
    }
  }
}

void SystemDictionary::RegisterTokens(
    const FilterInfo &filter,
    const string &tokens_key,
    const string &actual_key,
    const uint8 *encoded_tokens_ptr,
    Callback *callback) const {
  for (TokenDecodeIterator iter(codec_, value_trie_.get(), frequent_pos_,
                                actual_key, encoded_tokens_ptr);
       !iter.Done(); iter.Next()) {
    const TokenInfo &token_info = iter.Get();
    if (IsBadToken(filter, token_info)) {
      continue;
    }
    callback->OnToken(tokens_key, actual_key, *token_info.token);
  }
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

}  // namespace dictionary
}  // namespace mozc
