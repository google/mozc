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

// Trie tree library

#ifndef MOZC_BASE_TRIE_H_
#define MOZC_BASE_TRIE_H_

#include <map>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/string_piece.h"
#include "base/util.h"

namespace mozc {

template<typename T>
class Trie {
 public:
  Trie() : has_data_(false) {}

  virtual ~Trie() {
    typename SubTrie::iterator it;
    for (it = trie_.begin(); it != trie_.end(); ++it) {
      delete it->second;
    }
  }

  void AddEntry(StringPiece key, T data) {
    if (key.empty()) {
      data_ = data;
      has_data_ = true;
      return;
    }

    Trie *sub_trie;
    if (HasSubTrie(GetKeyHead(key))) {
      sub_trie = trie_[GetKeyHead(key).as_string()];
    } else {
      sub_trie = new Trie();
      trie_[GetKeyHead(key).as_string()] = sub_trie;
    }

    const StringPiece key_tail = GetKeyTail(key);
    sub_trie->AddEntry(key_tail, data);
  }

  bool DeleteEntry(StringPiece key) {
    if (key.empty()) {
      if (trie_.empty()) {
        return true;
      } else {
        has_data_ = false;
        return false;
      }
    }

    if (!HasSubTrie(GetKeyHead(key))) {
      return false;
    }

    Trie *sub_trie = GetSubTrie(key);
    const StringPiece sub_key = GetKeyTail(key);
    const bool should_delete_subtrie = sub_trie->DeleteEntry(sub_key);
    if (should_delete_subtrie) {
      delete sub_trie;
      trie_.erase(GetKeyHead(key).as_string());
      // If the size of trie_ is 0, This trie should be deleted.
      return trie_.size() == 0;
    } else {
      return false;
    }
  }

  bool LookUp(StringPiece key, T *data) const {
    if (key.empty()) {
      if (!has_data_) {
        return false;
      }
      *data = data_;
      return true;
    }

    if (!HasSubTrie(GetKeyHead(key))) {
      return false;
    }

    Trie *sub_trie = GetSubTrie(key);
    const StringPiece sub_key = GetKeyTail(key);
    return sub_trie->LookUp(sub_key, data);
  }

  // If key's prefix matches to trie with data, return true and set data
  // For example, if a trie has data for 'abc', 'abd', and 'a';
  //  - Return true for the key, 'abc'
  //  -- Matches in exact
  //  - Return true for the key, 'abcd'
  //  -- Matches in prefix
  //  - Return FALSE for the key, 'abe'
  //  -- Matches in prefix by 'ab', and 'ab' does not have data in trie tree
  //  -- Do not refer 'a' here.
  //  - Return true for the key, 'ac'
  //  -- Matches in prefix by 'a', and 'a' have data
  bool LookUpPrefix(StringPiece key,
                    T *data,
                    size_t *key_length,
                    bool *fixed) const {
    if (key.empty() || !HasSubTrie(GetKeyHead(key))) {
      *key_length = 0;
      if (has_data_) {
        *data = data_;
        *fixed = trie_.empty();
        return true;
      } else {
        *fixed = true;
        return false;
      }
    }

    Trie *sub_trie = GetSubTrie(key);
    const StringPiece sub_key = GetKeyTail(key);
    if (sub_trie->LookUpPrefix(sub_key, data, key_length, fixed)) {
      *key_length += GetKeyHeadLength(key);
      return true;
    } else if (HasSubTrie(GetKeyHead(key))) {
      *key_length += GetKeyHeadLength(key);
      return false;
    } else if (has_data_) {
      *data = data_;
      *key_length = 0;
      return true;
    } else {
      *key_length += GetKeyHeadLength(key);
      return false;
    }
  }

  // Return all result starts with key
  // For example, if a trie has data for 'abc', 'abd', and 'a';
  //  - Return 'abc', 'abd', 'a', for the key 'a'
  //  - Return 'abc', 'abd' for the key 'ab'
  //  - Return nothing for the key 'b'
  void LookUpPredictiveAll(StringPiece key,
                           vector<T> *data_list) const {
    DCHECK(data_list);
    if (!key.empty()) {
      if (!HasSubTrie(GetKeyHead(key))) {
        return;
      }
      const Trie *sub_trie = GetSubTrie(key);
      const StringPiece sub_key = GetKeyTail(key);
      return sub_trie->LookUpPredictiveAll(sub_key, data_list);
    }

    if (has_data_) {
      data_list->push_back(data_);
    }

    for (typename SubTrie::const_iterator it = trie_.begin();
         it != trie_.end(); ++it) {
      it->second->LookUpPredictiveAll("", data_list);
    }
  }

  bool HasSubTrie(StringPiece key) const {
    const StringPiece head = GetKeyHead(key);

    const typename SubTrie::const_iterator it = trie_.find(head.as_string());
    if (it == trie_.end()) {
      return false;
    }

    if (key.size() == head.size()) {
      return true;
    }

    return it->second->HasSubTrie(GetKeyTail(key));
  }

 private:
  StringPiece GetKeyHead(StringPiece key) const {
    return Util::SubStringPiece(key, 0, 1);
  }

  size_t GetKeyHeadLength(StringPiece key) const {
    return Util::OneCharLen(key.data());
  }

  StringPiece GetKeyTail(StringPiece key) const {
    return key.substr(Util::OneCharLen(key.data()));
  }

  Trie<T> *GetSubTrie(StringPiece key) const {
    return trie_.find(GetKeyHead(key).as_string())->second;
  }

  typedef map<const string, Trie<T> *> SubTrie;
  SubTrie trie_;
  bool has_data_;
  T data_;
};

}  // namespace mozc

#endif  // MOZC_BASE_TRIE_H_
