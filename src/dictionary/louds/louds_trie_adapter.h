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

#ifndef MOZC_DICTIONARY_LOUDS_LOUDS_TRIE_ADAPTER_H_
#define MOZC_DICTIONARY_LOUDS_LOUDS_TRIE_ADAPTER_H_

#include <string>
#include <vector>

#include "base/port.h"
#include "storage/louds/louds_trie.h"

namespace mozc {

namespace storage {
namespace louds {
class KeyExpansionTable;
}  // namespace louds
}  // namespace storage

namespace dictionary {
namespace louds {

// The result entry of search in LoudsTrieAdapter.
struct Entry {
  string key;
  string actual_key;
  int id;
};

// An adapter to fill the small gaps between LoudsTrie implementation and
// Mozc's requirement.
// This class is introduced to make transition from Rx smoother.
class LoudsTrieAdapter {
 public:
  LoudsTrieAdapter();
  ~LoudsTrieAdapter();

  bool OpenImage(const uint8 *image);
  void SetExpansionTable(const storage::louds::KeyExpansionTable *table);

  // TODO(hidehiko): Get rid of these methods, instead, we can pass appropriate
  //   callback directly to the LoudsTrie, so that we can reduce unnecessary
  //   memory allocation, and it should be efficient.
  void PrefixSearch(const string &key, vector<Entry> *result) const;
  void PredictiveSearch(const string &key, vector<Entry> *result) const;

  void PrefixSearchWithLimit(
      const string &key, int limit, vector<Entry> *result) const;
  void PredictiveSearchWithLimit(
      const string &key, int limit, vector<Entry> *result) const;

  // Looks up key string from the id.
  // Do not call this method for the missing id, as it will cause
  // infinit loop.
  void ReverseLookup(int id, string *key) const;

  // Searches the key string in the trie, and returns its id.
  // Returns -1 if not found.
  int GetIdFromKey(const string &key) const;

 private:
  storage::louds::LoudsTrie trie_;
  const storage::louds::KeyExpansionTable *key_expansion_table_;

  DISALLOW_COPY_AND_ASSIGN(LoudsTrieAdapter);
};

}  // namespace louds
}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_LOUDS_LOUDS_TRIE_ADAPTER_H_
