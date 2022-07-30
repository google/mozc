// Copyright 2010-2021, Google Inc.
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
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/util.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"

namespace mozc {

template <typename T>
class Trie final {
 public:
  Trie() = default;

  Trie(const Trie &) = delete;
  Trie &operator=(const Trie &) = delete;

  ~Trie() = default;

  template <typename U>
  void AddEntry(absl::string_view key, U &&data) {
    if (key.empty()) {
      data_ = std::forward<U>(data);
      return;
    }
    FindResult res = FindSubTrie(key);
    if (res.trie == nullptr) {
      auto t = std::make_unique<Trie>();
      res.trie = t.get();
      trie_[res.first_char] = std::move(t);
    }
    res.trie->AddEntry(res.rest, std::forward<U>(data));
  }

  bool DeleteEntry(absl::string_view key) {
    if (key.empty()) {
      if (trie_.empty()) {
        return true;
      }
      data_.reset();
      return false;
    }
    if (const FindResult res = FindSubTrie(key);
        res.trie != nullptr && res.trie->DeleteEntry(res.rest)) {
      trie_.erase(res.first_char);
      return trie_.empty();
    }
    return false;
  }

  bool LookUp(absl::string_view key, T *data) const {
    if (key.empty()) {
      if (!data_.has_value()) {
        return false;
      }
      *data = *data_;
      return true;
    }
    const FindResult res = FindSubTrie(key);
    return res.trie != nullptr && res.trie->LookUp(res.rest, data);
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
  bool LookUpPrefix(absl::string_view key, T *data, size_t *key_length,
                    bool *fixed) const {
    const FindResult res = FindSubTrie(key);
    if (res.trie == nullptr) {
      *key_length = 0;
      if (data_.has_value()) {
        *data = *data_;
        *fixed = trie_.empty();
        return true;
      }
      *fixed = true;
      return false;
    }
    const bool found =
        res.trie->LookUpPrefix(res.rest, data, key_length, fixed);
    const size_t first_char_len = key.size() - res.rest.size();
    *key_length += first_char_len;
    return found;
  }

  // If key's prefix matches to trie with data, return true and set data
  // For example, if a trie has data for 'abc', 'abd', and 'a';
  //  - Return true for the key, 'abc'
  //  -- Matches in exact
  //  - Return true for the key, 'abcd'
  //  -- Matches in prefix
  //  - Return TRUE for the key, 'abe'
  //  -- Matches in prefix by 'a'.
  //  - Return true for the key, 'ac'
  //  -- Matches in prefix by 'a', and 'a' have data
  bool LongestMatch(absl::string_view key, T *data, size_t *key_length) const {
    const FindResult res = FindSubTrie(key);
    if (res.trie == nullptr) {
      *key_length = 0;
      if (data_.has_value()) {
        *data = *data_;
        return true;
      }
      return false;
    }

    bool found = false;
    if (data_.has_value()) {
      *data = *data_;
      found = true;
    }

    if (res.trie->LongestMatch(res.rest, data, key_length)) {
      const size_t first_char_len = key.size() - res.rest.size();
      *key_length += first_char_len;
      return true;
    }
    return found;
  }

  // Return all result starts with key
  // For example, if a trie has data for 'abc', 'abd', and 'a';
  //  - Return 'abc', 'abd', 'a', for the key 'a'
  //  - Return 'abc', 'abd' for the key 'ab'
  //  - Return nothing for the key 'b'
  void LookUpPredictiveAll(absl::string_view key,
                           std::vector<T> *data_list) const {
    DCHECK(data_list);
    if (!key.empty()) {
      if (const FindResult res = FindSubTrie(key); res.trie != nullptr) {
        res.trie->LookUpPredictiveAll(res.rest, data_list);
      }
      return;
    }

    if (data_.has_value()) {
      data_list->push_back(*data_);
    }

    for (auto &[unused, trie] : trie_) {
      trie->LookUpPredictiveAll("", data_list);
    }
  }

  bool HasSubTrie(absl::string_view key) const {
    const FindResult res = FindSubTrie(key);
    if (res.trie == nullptr) {
      return false;
    }
    return res.rest.empty() || res.trie->HasSubTrie(res.rest);
  }

 private:
  using SubTrie = absl::flat_hash_map<char32, std::unique_ptr<Trie<T>>>;

  struct FindResult {
    Trie<T> *trie = nullptr;
    char32 first_char = 0;
    absl::string_view rest;
  };

  // Finds the subtrie that is reachable by the first UTF8 character of `key`.
  // If `key` is empty or no such trie exists, `FindResult::trie` is set to
  // nullptr. If `key` is nonempty, this method also sets the value of char32
  // for the first character and the rest of strings regardless of the find
  // result.
  FindResult FindSubTrie(absl::string_view key) const {
    FindResult res;
    if (!key.empty()) {
      Util::SplitFirstChar32(key, &res.first_char, &res.rest);
      if (const auto it = trie_.find(res.first_char); it != trie_.end()) {
        res.trie = it->second.get();
      }
    }
    return res;
  }

  SubTrie trie_;
  std::optional<T> data_;
};

}  // namespace mozc

#endif  // MOZC_BASE_TRIE_H_
