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

#include "dictionary/suffix_dictionary.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>

#include "base/iterator_adapter.h"
#include "base/logging.h"
#include "base/util.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/suffix_dictionary_token.h"

namespace mozc {

SuffixDictionary::SuffixDictionary(const SuffixToken *suffix_tokens,
                                   size_t suffix_tokens_size)
    : suffix_tokens_(suffix_tokens),
      suffix_tokens_size_(suffix_tokens_size) {}

SuffixDictionary::~SuffixDictionary() {}

namespace {

struct SuffixTokenKeyAdapter : public AdapterBase<const char *> {
  value_type operator()(const SuffixToken *token) const {
    return token->key;
  }
};

class ComparePrefix {
 public:
  explicit ComparePrefix(size_t max_len) : max_len_(max_len) {}

  // Note: the inputs don't need to be null-terminated.
  bool operator()(const char *x, const char *y) const {
    return std::strncmp(x, y, max_len_) < 0;
  }

 private:
  const size_t max_len_;
};

}  // namespace

bool SuffixDictionary::HasValue(StringPiece value) const {
  // SuffixDictionary::HasValue() is never called and unnecessary to
  // implement. To avoid accidental calls of this method, the method simply dies
  // so that we can immediately notice this unimplemented method during
  // development.
  LOG(FATAL) << "bool SuffixDictionary::HasValue() is not implemented";
  return false;
}

void SuffixDictionary::LookupPredictive(
    StringPiece key,
    bool,  // use_kana_modifier_insensitive_lookup
    Callback *callback) const {
  typedef IteratorAdapter<const SuffixToken *, SuffixTokenKeyAdapter> Iter;
  pair<Iter, Iter> range = equal_range(
      MakeIteratorAdapter(suffix_tokens_, SuffixTokenKeyAdapter()),
      MakeIteratorAdapter(suffix_tokens_ + suffix_tokens_size_,
                          SuffixTokenKeyAdapter()),
      key.data(), ComparePrefix(key.size()));

  Token token;
  token.attributes = Token::NONE;  // Common for all suffix tokens.
  for (; range.first != range.second; ++range.first) {
    const SuffixToken &suffix_token = *range.first.base();
    token.key = suffix_token.key;
    switch (callback->OnKey(token.key)) {
      case Callback::TRAVERSE_DONE:
        return;
      case Callback::TRAVERSE_NEXT_KEY:
        continue;
      case Callback::TRAVERSE_CULL:
        LOG(FATAL) << "Culling is not supported.";
        continue;
      default:
        break;
    }
    token.value = (suffix_token.value == NULL) ? token.key : suffix_token.value;
    token.lid = suffix_token.lid;
    token.rid = suffix_token.rid;
    token.cost = suffix_token.wcost;
    if (callback->OnToken(token.key, token.key, token) !=
        Callback::TRAVERSE_CONTINUE) {
      break;
    }
  }
}

void SuffixDictionary::LookupPrefix(StringPiece key,
                                    bool use_kana_modifier_insensitive_lookup,
                                    Callback *callback) const {
}

void SuffixDictionary::LookupReverse(StringPiece str,
                                     NodeAllocatorInterface *allocator,
                                     Callback *callback) const {
}

void SuffixDictionary::LookupExact(StringPiece key, Callback *callback) const {
}

}  // namespace mozc
