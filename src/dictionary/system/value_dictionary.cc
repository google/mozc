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

#include "dictionary/system/value_dictionary.h"

#include <limits>
#include <string>

#include "base/logging.h"
#include "base/port.h"
#include "base/string_piece.h"
#include "base/system_util.h"
#include "base/trie.h"
#include "converter/node.h"
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
      empty_limit_(Limit()),
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

bool ValueDictionary::HasValue(const StringPiece value) const {
  string encoded;
  codec_->EncodeValue(value, &encoded);
  return value_trie_->ExactSearch(encoded) != -1;
}

namespace {

inline void FillNode(const uint16 suggestion_only_word_id,
                     const char *value, size_t value_size,
                     Node *node) {
  // Set fake token information.
  // Since value dictionary is intended to use for suggestion,
  // we use SuggestOnlyWordId here.
  // Cost is also set without lookup.
  // TODO(toshiyuki): If necessary, implement simple cost lookup.
  // Bloom filter may be one option.
  node->lid = suggestion_only_word_id;
  node->rid = suggestion_only_word_id;
  node->wcost = 10000;
  node->key.assign(value, value_size);
  node->value.assign(value, value_size);
  node->node_type = Node::NOR_NODE;
  node->bnext = NULL;
}

class NodeListBuilder : public LoudsTrie::Callback {
 public:
  NodeListBuilder(const int original_key_len,
                  const SystemDictionaryCodecInterface *codec,
                  const uint16 suggestion_only_word_id,
                  const Trie<string> *begin_with_trie,
                  NodeAllocatorInterface *allocator)
      : original_key_len_(original_key_len),
        codec_(codec),
        suggestion_only_word_id_(suggestion_only_word_id),
        begin_with_trie_(begin_with_trie),
        allocator_(allocator),
        limit_(allocator == NULL ?
            numeric_limits<int>::max() : allocator_->max_nodes_size()),
        result_(NULL) {
  }

  virtual ResultType Run(const char *key_begin, size_t len, int key_id) {
    if (limit_ <= 0) {
      return SEARCH_DONE;
    }

    // The decoded key of value trie corresponds to value (surface form).
    string value;
    codec_->DecodeValue(StringPiece(key_begin, len), &value);

    if (begin_with_trie_ != NULL) {
      // If |begin_with_trie_| was provided, check if the value ends with some
      // key in it. For example, if original key is "he" and "hello" was found,
      // the node for "hello" is built only when "llo" is in |begin_with_trie_|.
      string trie_value;
      size_t key_length = 0;
      bool has_subtrie = false;
      if (!begin_with_trie_->LookUpPrefix(value.data() + original_key_len_,
                                          &trie_value,
                                          &key_length, &has_subtrie)) {
        return SEARCH_CONTINUE;
      }
    }

    // TODO(noriyukit): This is a very hacky way of injection for node
    // allocation. We should implement an allocator that just creates new Node
    // for unit tests.
    Node *new_node = NULL;
    if (allocator_ != NULL) {
      new_node = allocator_->NewNode();
    } else {
      // for test
      new_node = new Node();
    }

    FillNode(suggestion_only_word_id_, value.data(), value.size(), new_node);

    // Update the list structure: insert |new_node| to the head.
    new_node->bnext = result_;
    result_ = new_node;

    --limit_;
    return SEARCH_CONTINUE;
  }

  Node *result() const {
    return result_;
  }

 private:
  const int original_key_len_;
  const SystemDictionaryCodecInterface *codec_;
  const uint16 suggestion_only_word_id_;
  const Trie<string> *begin_with_trie_;
  NodeAllocatorInterface *allocator_;
  int limit_;
  Node *result_;

  DISALLOW_COPY_AND_ASSIGN(NodeListBuilder);
};

}  // namespace

Node *ValueDictionary::LookupPredictiveWithLimit(
    const char *str, int size,
    const Limit &limit,
    NodeAllocatorInterface *allocator) const {
  if (size == 0) {
    // For empty key, return NULL (representing an empty result) immediately
    // for backword compatibility.
    // TODO(hidehiko): Returning all entries in dictionary for predictive
    //   searching with an empty key may look natural as well. So we should
    //   find an appropriate handling point.
    return NULL;
  }
  string lookup_key_str;
  codec_->EncodeValue(StringPiece(str, size), &lookup_key_str);

  DCHECK(value_trie_.get() != NULL);
  NodeListBuilder builder(size, codec_, suggestion_only_word_id_,
                          limit.begin_with_trie, allocator);
  value_trie_->PredictiveSearch(lookup_key_str.c_str(), &builder);
  return builder.result();
}

Node *ValueDictionary::LookupPredictive(
    const char *str, int size,
    NodeAllocatorInterface *allocator) const {
  return LookupPredictiveWithLimit(str, size, empty_limit_, allocator);
}

// Value dictioanry is intended to use for prediction,
// so we don't support LookupPrefix
Node *ValueDictionary::LookupPrefixWithLimit(
    const char *str, int size,
    const Limit &limit,
    NodeAllocatorInterface *allocator) const {
  return NULL;
}

Node *ValueDictionary::LookupPrefix(
    const char *str, int size,
    NodeAllocatorInterface *allocator) const {
  return NULL;
}

Node *ValueDictionary::LookupExact(const char *str, int size,
                                   NodeAllocatorInterface *allocator) const {
  if (size == 0) {
    // For empty string, return NULL for compatibility reason; see the comment
    // above.
    return NULL;
  }
  DCHECK(value_trie_.get() != NULL);
  DCHECK(allocator != NULL);

  string lookup_key_str;
  codec_->EncodeValue(StringPiece(str, size), &lookup_key_str);
  if (value_trie_->ExactSearch(lookup_key_str) == -1) {
    return NULL;
  }
  Node *node = allocator->NewNode();
  FillNode(suggestion_only_word_id_, str, size, node);
  return node;
}

Node *ValueDictionary::LookupReverse(const char *str, int size,
                                     NodeAllocatorInterface *allocator) const {
  return NULL;
}

}  // namespace dictionary
}  // namespace mozc
