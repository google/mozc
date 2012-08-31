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

#include "dictionary/louds/louds_trie_adapter.h"

#include <algorithm>
#include <limits>

namespace mozc {
namespace dictionary {
namespace louds {

using ::mozc::storage::louds::LoudsTrie;
using ::mozc::storage::louds::KeyExpansionTable;

LoudsTrieAdapter::LoudsTrieAdapter()
    : key_expansion_table_(&KeyExpansionTable::GetDefaultInstance()) {
}

LoudsTrieAdapter::~LoudsTrieAdapter() {
}

bool LoudsTrieAdapter::OpenImage(const uint8 *image) {
  return trie_.Open(image);
}

void LoudsTrieAdapter::SetExpansionTable(const KeyExpansionTable *table) {
  key_expansion_table_ =
      (table == NULL) ? &KeyExpansionTable::GetDefaultInstance() : table;
}

namespace {
class LoudsTrieAdapterCallback : public LoudsTrie::Callback {
 public:
  LoudsTrieAdapterCallback(
      const string &original_key, int limit, vector<Entry> *entry_list)
      : original_key_(original_key),
        limit_(limit),
        entry_list_(entry_list) {
  }

  ~LoudsTrieAdapterCallback() {
  }

  virtual bool Run(const char *s, size_t len, int key_id) {
    if (limit_ <= 0) {
      // Finish the search.
      return true;
    }

    --limit_;
    entry_list_->push_back(Entry());
    Entry& entry = entry_list_->back();

    entry.actual_key.assign(s, len);
    // TODO(hidehiko): Check performance if it is worth to split this class
    // into two, tuned for PrefixSearch and for PredictiveSearch.
    if (original_key_.length() < len) {
      entry.key.reserve(len);
      entry.key.assign(original_key_);
      entry.key.append(
          s + original_key_.length(), len - original_key_.length());
    } else {
      entry.key.assign(original_key_, 0, len);
    }

    entry.id = key_id;
    return false;
  }

 private:
  const string &original_key_;
  int limit_;
  vector<Entry> *entry_list_;

  DISALLOW_COPY_AND_ASSIGN(LoudsTrieAdapterCallback);
};
}  // namespace.

void LoudsTrieAdapter::PrefixSearch(
    const string &key, vector<Entry> *result) const {
  PrefixSearchWithLimit(key, numeric_limits<int>::max(), result);
}

void LoudsTrieAdapter::PredictiveSearch(
    const string &key, vector<Entry> *result) const {
  PredictiveSearchWithLimit(key, numeric_limits<int>::max(), result);
}

void LoudsTrieAdapter::PrefixSearchWithLimit(
    const string &key, int limit, vector<Entry> *result) const {
  LoudsTrieAdapterCallback callback(key, limit, result);
  trie_.PrefixSearchWithKeyExpansion(
      key.c_str(), *key_expansion_table_, &callback);
}

void LoudsTrieAdapter::PredictiveSearchWithLimit(
    const string &key, int limit, vector<Entry> *result) const {
  LoudsTrieAdapterCallback callback(key, limit, result);
  trie_.PredictiveSearchWithKeyExpansion(
      key.c_str(), *key_expansion_table_, &callback);
}

void LoudsTrieAdapter::ReverseLookup(int id, string *key) const {
  char buffer[LoudsTrie::kMaxDepth + 1];
  key->assign(trie_.Reverse(id, buffer));
}

namespace {
class LoudsTrieAdapterIdCallback : public LoudsTrie::Callback {
 public:
  explicit LoudsTrieAdapterIdCallback(const string &original_key)
      : id_(-1),  // Initialize with -1 for not found.
        original_key_(original_key) {
  }
  int id() const { return id_; }

  virtual bool Run(const char *s, size_t len, int id) {
    // Remember the ID and quit the search.
    if (original_key_.length() == len) {
      // We should run this callback without key-expansion table,
      // so if "same length" means "s is exactly as same as original_key_".
      id_ = id;
    }

    // Regardless of whether the key is actually found or not,
    // we don't need continuing to search.
    return true;
  }

 private:
  int id_;
  const string &original_key_;

  DISALLOW_COPY_AND_ASSIGN(LoudsTrieAdapterIdCallback);
};
}  // namespace

int LoudsTrieAdapter::GetIdFromKey(const string &key) const {
  LoudsTrieAdapterIdCallback callback(key);
  trie_.PredictiveSearch(key.c_str(), &callback);
  return callback.id();
}

}  // namespace louds
}  // namespace dictionary
}  // namespace mozc
