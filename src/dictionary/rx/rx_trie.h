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

// This class wraps Rx library code for mozc

#ifndef MOZC_DICTIONARY_RX_RX_TRIE_H_
#define MOZC_DICTIONARY_RX_RX_TRIE_H_

#include <string>
#include <vector>

#include "base/base.h"

struct rx;

namespace mozc {
namespace rx {

// Container for search result
struct RxEntry {
  string key;
  int id;
};

class RxTrie {
 public:
  RxTrie();
  virtual ~RxTrie();

  // Open from image.
  bool OpenImage(const unsigned char *image);

  void PredictiveSearch(const string &key, vector<RxEntry> *result) const;

  void PrefixSearch(const string &key, vector<RxEntry> *result) const;

  // Returns limited numbers of results
  void PredictiveSearchWithLimit(const string &key,
                                 int limit,
                                 vector<RxEntry> *result) const;

  void PrefixSearchWithLimit(const string &key,
                             int limit,
                             vector<RxEntry> *result) const;

  // Lookup key string from id.
  // * Do not call ReverseLookup for the missing id. *
  // It will cause infinite loop.
  void ReverseLookup(int id, string *key) const;

 private:
  enum SearchType {
    PREDICTIVE = 0,
    PREFIX = 1,
  };

  void SearchInternal(const string &key, SearchType type,
                      int limit,
                      vector<RxEntry> *result) const;

  struct rx *rx_trie_;

  DISALLOW_COPY_AND_ASSIGN(RxTrie);
};
}  // namespace rx
}  // namespace mozc

#endif  // MOZC_DICTIONARY_RX_RX_TRIE_H_
