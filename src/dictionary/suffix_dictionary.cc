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

#include "dictionary/suffix_dictionary.h"

#include <string>
#include "base/base.h"
#include "base/util.h"
#include "base/singleton.h"
#include "converter/node.h"

namespace mozc {
namespace {

struct SuffixToken {
  const char *key;
  const char *value;
  uint16 lid;
  uint16 rid;
  int16  wcost;
};

#include "dictionary/suffix_data.h"
}   // namespace

SuffixDictionary::SuffixDictionary() {}
SuffixDictionary::~SuffixDictionary() {}

Node *SuffixDictionary::LookupPredictive(
    const char *str, int size,
    NodeAllocatorInterface *allocator) const {
  string input_key;
  if (str != NULL && size > 0) {
    input_key.assign(str, size);
  }

  DCHECK(allocator);
  Node *result = NULL;
  for (size_t i = 0; i < arraysize(kSuffixTokens); ++i) {
    const SuffixToken *token = &kSuffixTokens[i];
    DCHECK(token);
    DCHECK(token->key);
    if (!input_key.empty() && !Util::StartsWith(token->key, input_key)) {
      continue;
    }
    Node *node = allocator->NewNode();
    DCHECK(node);
    node->Init();
    node->wcost = token->wcost;
    node->key = token->key;
    // To save binary image, value is NULL if key == value.
    node->value = token->value == NULL ? token->key : token->value;
    node->lid = token->lid;
    node->rid = token->rid;
    node->bnext = result;
    result = node;
  }

  return result;
}

// SuffixDictionary doesn't support Prefix/Revese Lookup.
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

namespace {
DictionaryInterface *g_suffix_dictionary = NULL;
}   // namespace

DictionaryInterface *SuffixDictionaryFactory::GetSuffixDictionary() {
  if (g_suffix_dictionary == NULL) {
    return Singleton<SuffixDictionary>::get();
  } else {
    return g_suffix_dictionary;
  }
}

void SuffixDictionaryFactory::SetSuffixDictionary(
    DictionaryInterface *dictionary) {
  g_suffix_dictionary = dictionary;
}
}  // namespace mozc
