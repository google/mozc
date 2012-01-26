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

#include "dictionary/rx/rx_trie.h"

#include <string>
#include <vector>

#include "base/base.h"
#include "third_party/rx/v1_0rc2/rx.h"

namespace mozc {
namespace rx {
namespace {
const int kMaxTokensPerLookup = 10000;

struct RxResults {
  int limit;
  vector<RxEntry> *result;
};

static int RxCallback(void *cookie, const char *s, int len, int id) {
  RxResults *res = reinterpret_cast<RxResults *>(cookie);
  DCHECK(res);
  if (res->limit <= 0) {
    // stops traversal.
    return -1;
  }
  --(res->limit);
  // truncates to len byte.
  RxEntry entry;
  entry.key = string(s, len);
  entry.id = id;
  res->result->push_back(entry);
  return 0;
}
}  // namespace

RxTrie::RxTrie() : rx_trie_(NULL) {}

RxTrie::~RxTrie() {
  rx_close(rx_trie_);
}

bool RxTrie::OpenImage(const unsigned char *image) {
  rx_trie_ = rx_open(image);
  return (rx_trie_ != NULL);
}

void RxTrie::PredictiveSearch(const string &key,
                              vector<RxEntry> *result) const {
  DCHECK(result);
  SearchInternal(key, PREDICTIVE, kMaxTokensPerLookup, result);
}

void RxTrie::PrefixSearch(const string &key,
                          vector<RxEntry> *result) const {
  DCHECK(result);
  SearchInternal(key, PREFIX, kMaxTokensPerLookup, result);
}

void RxTrie::PredictiveSearchWithLimit(const string &key,
                                       int limit,
                                       vector<RxEntry> *result) const {
  DCHECK(result);
  SearchInternal(key, PREDICTIVE, limit, result);
}

void RxTrie::PrefixSearchWithLimit(const string &key,
                                   int limit,
                                   vector<RxEntry> *result) const {
  DCHECK(result);
  SearchInternal(key, PREFIX, limit, result);
}

void RxTrie::ReverseLookup(int id, string *key) const {
  DCHECK(key);
  char buf[256];
  rx_reverse(rx_trie_, id, buf, sizeof(buf));
  key->assign(buf);
}

void RxTrie::SearchInternal(const string &key, SearchType type,
                            int limit,
                            vector<RxEntry> *result) const {
  DCHECK(result);
  RxResults rx_results;
  rx_results.limit = limit;
  rx_results.result = result;
  if (type == PREDICTIVE) {
    rx_search(rx_trie_, 1, key.c_str(), RxCallback, &rx_results);
  } else if (type == PREFIX) {
    rx_search(rx_trie_, 0, key.c_str(), RxCallback, &rx_results);
  } else {
    DLOG(FATAL) << "should not come here.";
  }
}
}  // namespace rx
}  // namespace mozc
