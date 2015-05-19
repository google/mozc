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

#include "dictionary/system/value_dictionary.h"

#include <algorithm>
#include <climits>
#include <string>

#include "base/base.h"
#include "base/flags.h"
#include "base/trie.h"
#include "base/util.h"
#include "converter/node.h"
#include "dictionary/file/dictionary_file.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/rx/rx_trie.h"
#include "dictionary/system/codec_interface.h"

namespace mozc {

ValueDictionary::ValueDictionary()
    : value_trie_(new rx::RxTrie),
      dictionary_file_(new DictionaryFile),
      codec_(dictionary::SystemDictionaryCodecFactory::GetCodec()),
      empty_limit_(Limit()) {
}

ValueDictionary::~ValueDictionary() {}

// static
ValueDictionary *ValueDictionary::CreateValueDictionaryFromFile(
    const string &filename) {
  ValueDictionary *instance = new ValueDictionary();
  DCHECK(instance);
  if (!instance->dictionary_file_->OpenFromFile(filename)) {
    LOG(ERROR) << "Failed to open system dictionary file";
    return NULL;
  }
  if (!instance->OpenDictionaryFile()) {
    LOG(ERROR) << "Failed to create value dictionary";
    delete instance;
    return NULL;
  }
  return instance;
}

// static
ValueDictionary *ValueDictionary::CreateValueDictionaryFromImage(
    const char *ptr, int len) {
  // Make the dictionary not to be paged out.
  // We don't check the return value because the process doesn't necessarily
  // has the priviledge to mlock.
  // Note that we don't munlock the space because it's always better to keep
  // the singleton system dictionary paged in as long as the process runs.
  Util::MaybeMLock(ptr, len);
  ValueDictionary *instance = new ValueDictionary();
  DCHECK(instance);
  if (!instance->dictionary_file_->OpenFromImage(ptr, len)) {
    LOG(ERROR) << "Failed to open system dictionary file";
    return NULL;
  }
  if (!instance->OpenDictionaryFile()) {
    LOG(ERROR) << "Failed to create value dictionary";
    delete instance;
    return NULL;
  }
  return instance;
}

bool ValueDictionary::OpenDictionaryFile() {
  int image_len = 0;
  const unsigned char *value_image =
      reinterpret_cast<const unsigned char *>(dictionary_file_->GetSection(
          codec_->GetSectionNameForValue(), &image_len));
  CHECK(value_image) << "can not find value section";
  if (!(value_trie_->OpenImage(value_image))) {
    DLOG(ERROR) << "Cannot open value trie";
    return false;
  }
  return true;
}

Node *ValueDictionary::LookupPredictiveWithLimit(
    const char *str, int size,
    const Limit &limit,
    NodeAllocatorInterface *allocator) const {
  string lookup_key_str;
  codec_->EncodeValue(string(str, size), &lookup_key_str);

  DCHECK(value_trie_.get() != NULL);

  vector<rx::RxEntry> results;
  // TODO(toshiyuki): node_size_limit can be defined in the Limit
  int node_size_limit = -1;  // no limit
  if (allocator != NULL) {
    node_size_limit = allocator->max_nodes_size();
    value_trie_->PredictiveSearchWithLimit(
        lookup_key_str, node_size_limit, &results);
  } else {
    value_trie_->PredictiveSearch(lookup_key_str, &results);
  }

  Node *res = NULL;
  for (size_t i = 0; i < results.size(); ++i) {
    if (node_size_limit == 0) {
      break;
    }
    if (!Util::StartsWith(results[i].key, lookup_key_str)) {
      break;
    }
    string value;
    codec_->DecodeValue(results[i].key, &value);
    // filter by begin with list
    if (limit.begin_with_trie != NULL) {
      string trie_value;
      size_t key_length = 0;
      bool has_subtrie = false;
      if (!limit.begin_with_trie->LookUpPrefix(value.data() + size, &trie_value,
                                               &key_length, &has_subtrie)) {
        continue;
      }
    }

    Node *new_node = NULL;
    if (allocator != NULL) {
      new_node = allocator->NewNode();
    } else {
      // for test
      new_node = new Node();
    }
    // Set fake token information.
    // Since value dictionary is intended to use for suggestion,
    // we use SuggestOnlyWordId here.
    // Cost is also set without lookup.
    // TODO(toshiyuki): If necessary, implement simple cost lookup.
    // Bloom filter may be one option.
    new_node->lid = POSMatcher::GetSuggestOnlyWordId();
    new_node->rid = POSMatcher::GetSuggestOnlyWordId();
    new_node->wcost = 10000;
    new_node->key = value;
    new_node->value = value;
    new_node->node_type = Node::NOR_NODE;

    new_node->bnext = res;
    res = new_node;
    if (node_size_limit > 0) {
      --node_size_limit;
    }
  }
  return res;
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

Node *ValueDictionary::LookupReverse(const char *str, int size,
                                     NodeAllocatorInterface *allocator) const {
  return NULL;
}

}  // namespace mozc
