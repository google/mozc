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

#include "dictionary/system/value_dictionary.h"

#include <limits>
#include <string>

#include "base/logging.h"
#include "base/port.h"
#include "base/string_piece.h"
#include "base/system_util.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/file/dictionary_file.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/system/codec_interface.h"
#include "storage/louds/louds_trie.h"

namespace mozc {
namespace dictionary {

using mozc::storage::louds::LoudsTrie;

ValueDictionary::ValueDictionary(const POSMatcher& pos_matcher)
    : value_trie_(new LoudsTrie),
      dictionary_file_(new DictionaryFile),
      codec_(SystemDictionaryCodecFactory::GetCodec()),
      suggestion_only_word_id_(pos_matcher.GetSuggestOnlyWordId()) {
}

ValueDictionary::~ValueDictionary() {}

// static
ValueDictionary *ValueDictionary::CreateValueDictionaryFromFile(
    const POSMatcher& pos_matcher, const string &filename) {
  scoped_ptr<ValueDictionary> instance(new ValueDictionary(pos_matcher));
  DCHECK(instance.get());
  if (!instance->dictionary_file_->OpenFromFile(filename)) {
    LOG(ERROR) << "Failed to open system dictionary file";
    return NULL;
  }
  if (!instance->OpenDictionaryFile()) {
    LOG(ERROR) << "Failed to create value dictionary";
    return NULL;
  }
  return instance.release();
}

// static
ValueDictionary *ValueDictionary::CreateValueDictionaryFromImage(
    const POSMatcher& pos_matcher, const char *ptr, int len) {
  // Make the dictionary not to be paged out.
  // We don't check the return value because the process doesn't necessarily
  // has the priviledge to mlock.
  // Note that we don't munlock the space because it's always better to keep
  // the singleton system dictionary paged in as long as the process runs.
  SystemUtil::MaybeMLock(ptr, len);
  scoped_ptr<ValueDictionary> instance(new ValueDictionary(pos_matcher));
  DCHECK(instance.get());
  if (!instance->dictionary_file_->OpenFromImage(ptr, len)) {
    LOG(ERROR) << "Failed to open system dictionary file";
    return NULL;
  }
  if (!instance->OpenDictionaryFile()) {
    LOG(ERROR) << "Failed to create value dictionary";
    return NULL;
  }
  return instance.release();
}

bool ValueDictionary::OpenDictionaryFile() {
  int image_len = 0;
  const unsigned char *value_image =
      reinterpret_cast<const uint8 *>(dictionary_file_->GetSection(
          codec_->GetSectionNameForValue(), &image_len));
  CHECK(value_image) << "can not find value section";
  if (!(value_trie_->Open(value_image))) {
    DLOG(ERROR) << "Cannot open value trie";
    return false;
  }
  return true;
}

// ValueDictionary is supposed to use the same data with SystemDictionary
// and SystemDictionary::HasValue should return the same result with
// ValueDictionary::HasValue.  So we can skip the actual logic of HasValue
// and return just false.
bool ValueDictionary::HasValue(StringPiece value) const {
  return false;
}

namespace {

// A version of the above function for Token.
inline void FillToken(const uint16 suggestion_only_word_id,
                      StringPiece key, Token *token) {
  key.CopyToString(&token->key);
  token->value = token->key;
  token->cost = 10000;
  token->lid = token->rid = suggestion_only_word_id;
  token->attributes = Token::NONE;
}

// Converts a value of SystemDictionary::Callback::ResultType to the
// corresponding value of LoudsTrie::Callback::ResultType.
inline LoudsTrie::Callback::ResultType ConvertResultType(
    const DictionaryInterface::Callback::ResultType result) {
  switch (result) {
    case DictionaryInterface::Callback::TRAVERSE_DONE:
      return LoudsTrie::Callback::SEARCH_DONE;
    case DictionaryInterface::Callback::TRAVERSE_NEXT_KEY:
    case DictionaryInterface::Callback::TRAVERSE_CONTINUE:
      return LoudsTrie::Callback::SEARCH_CONTINUE;
    case DictionaryInterface::Callback::TRAVERSE_CULL:
      return LoudsTrie::Callback::SEARCH_CULL;
    default:
      LOG(DFATAL) << "Enum value " << result << " cannot be converted";
      return LoudsTrie::Callback::SEARCH_DONE;  // dummy
  }
}

class PredictiveTraverser : public LoudsTrie::Callback {
 public:
  PredictiveTraverser(const SystemDictionaryCodecInterface *codec,
                      const uint16 suggestion_only_word_id,
                      DictionaryInterface::Callback *callback)
      : codec_(codec),
        suggestion_only_word_id_(suggestion_only_word_id),
        callback_(callback) {}
  virtual ~PredictiveTraverser() {}

  virtual ResultType Run(const char *key_begin, size_t len, int key_id) {
    // The decoded key of value trie corresponds to value (surface form).
    string value;
    codec_->DecodeValue(StringPiece(key_begin, len), &value);
    DictionaryInterface::Callback::ResultType result = callback_->OnKey(value);
    if (result != DictionaryInterface::Callback::TRAVERSE_CONTINUE) {
      return ConvertResultType(result);
    }
    FillToken(suggestion_only_word_id_, value, &token_);
    result = callback_->OnToken(value, value, token_);
    return ConvertResultType(result);
  }

 private:
  const SystemDictionaryCodecInterface *codec_;
  const uint16 suggestion_only_word_id_;
  DictionaryInterface::Callback *callback_;
  Token token_;

  DISALLOW_COPY_AND_ASSIGN(PredictiveTraverser);
};

}  // namespace

void ValueDictionary::LookupPredictive(
    StringPiece key,
    bool,  // use_kana_modifier_insensitive_lookup
    Callback *callback) const {
  // Do nothing for empty key, although looking up all the entries with empty
  // string seems natural.
  if (key.empty()) {
    return;
  }
  string lookup_key_str;
  codec_->EncodeValue(key, &lookup_key_str);
  DCHECK(value_trie_.get() != NULL);
  PredictiveTraverser traverser(codec_, suggestion_only_word_id_, callback);
  value_trie_->PredictiveSearch(lookup_key_str.c_str(), &traverser);
}

void ValueDictionary::LookupPrefix(
    StringPiece key, bool use_kana_modifier_insensitive_lookup,
    Callback *callback) const {}

void ValueDictionary::LookupExact(StringPiece key, Callback *callback) const {
  if (key.empty()) {
    // For empty string, return NULL for compatibility reason; see the comment
    // above.
    return;
  }
  DCHECK(value_trie_.get() != NULL);

  string lookup_key_str;
  codec_->EncodeValue(key, &lookup_key_str);
  if (value_trie_->ExactSearch(lookup_key_str) == -1) {
    return;
  }
  if (callback->OnKey(key) != Callback::TRAVERSE_CONTINUE) {
    return;
  }
  Token token;
  FillToken(suggestion_only_word_id_, key, &token);
  callback->OnToken(key, key, token);
}

void ValueDictionary::LookupReverse(StringPiece str,
                                    NodeAllocatorInterface *allocator,
                                    Callback *callback) const {
}

}  // namespace dictionary
}  // namespace mozc
