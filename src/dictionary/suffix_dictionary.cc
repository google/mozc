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

#include "dictionary/suffix_dictionary.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "base/base.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/trie.h"
#include "base/util.h"
#include "converter/node.h"
#include "dictionary/suffix_dictionary_token.h"

namespace mozc {
SuffixDictionary::SuffixDictionary(const SuffixToken *suffix_tokens,
                                   size_t suffix_tokens_size)
    : suffix_tokens_(suffix_tokens),
      suffix_tokens_size_(suffix_tokens_size),
      empty_limit_(Limit()) {}

SuffixDictionary::~SuffixDictionary() {}

Node *SuffixDictionary::LookupPredictive(
    const char *str, int size,
    NodeAllocatorInterface *allocator) const {
  return LookupPredictiveWithLimit(str, size, empty_limit_, allocator);
}

namespace {
class SuffixTokenKeyPrefixComparator {
 public:
  explicit SuffixTokenKeyPrefixComparator(size_t size) : size_(size) {
  }

  bool operator()(const SuffixToken &t1, const SuffixToken &t2) const {
    return std::strncmp(t1.key, t2.key, size_) < 0;
  }

 private:
  size_t size_;
};
}  // namespace

bool SuffixDictionary::HasValue(const StringPiece value) const {
  // SuffixDictionary::HasValue() is never called and unnecessary to
  // implement. To avoid accidental calls of this method, the method simply dies
  // so that we can immediately notice this unimplemented method during
  // development.
  LOG(FATAL) << "bool SuffixDictionary::HasValue() is not implemented";
  return false;
}

Node *SuffixDictionary::LookupPredictiveWithLimit(
    const char *str, int size, const Limit &limit,
    NodeAllocatorInterface *allocator) const {
  size_t begin_index, end_index;
  if (str == NULL) {
    DCHECK(size == 0);
    begin_index = 0;
    end_index = suffix_tokens_size_;
  } else {
    SuffixToken key;
    key.key = str;
    pair<const SuffixToken *, const SuffixToken *> range = equal_range(
        suffix_tokens_, suffix_tokens_ + suffix_tokens_size_,
        key, SuffixTokenKeyPrefixComparator(size));
    begin_index = range.first - suffix_tokens_;
    end_index = range.second - suffix_tokens_;
  }

  DCHECK(allocator);

  Node *result = NULL;
  for (size_t i = begin_index; i < end_index; ++i) {
    const SuffixToken &token = suffix_tokens_[i];
    DCHECK(token.key);
    // check begin with
    if (limit.begin_with_trie != NULL) {
      string value;
      size_t key_length = 0;
      bool has_subtrie = false;
      if (!limit.begin_with_trie->LookUpPrefix(token.key + size, &value,
                                               &key_length, &has_subtrie)) {
        continue;
      }
    }

    Node *node = allocator->NewNode();
    DCHECK(node);
    node->Init();
    node->wcost = token.wcost;
    node->key = token.key;
    // To save binary image, value is NULL if key == value.
    node->value = (token.value == NULL) ? token.key : token.value;
    node->lid = token.lid;
    node->rid = token.rid;
    node->bnext = result;
    result = node;
  }

  return result;
}

// SuffixDictionary doesn't support Prefix/Revese Lookup.
Node *SuffixDictionary::LookupPrefixWithLimit(
    const char *str, int size,
    const Limit &limit,
    NodeAllocatorInterface *allocator) const {
  return NULL;
}

Node *SuffixDictionary::LookupPrefix(
    const char *str, int size,
    NodeAllocatorInterface *allocator) const {
  return NULL;
}

Node *SuffixDictionary::LookupReverse(
    const char *str, int size,
    NodeAllocatorInterface *allocator) const {
  return NULL;
}

}  // namespace mozc
