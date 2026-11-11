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

#ifndef MOZC_BASE_CONTAINER_TRIE_H_
#define MOZC_BASE_CONTAINER_TRIE_H_

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "base/util.h"

namespace mozc {

// UTF-8 codepoint (`char32_t`) trie mapping string keys to values of type `T`.
//
// Nodes are stored in `nodes_` (`nodes_[0]` is the root when non-empty) and
// reference children by `uint32_t` index rather than pointers. Keeping each
// `absl::flat_hash_map<char32_t, uint32_t>` slot at 8 bytes enables Abseil's
// inline Small Object Optimization (SOO) for nodes with 0 or 1 child, avoiding
// per-node heap allocations.
template <typename T>
class Trie final {
 public:
  Trie() = default;
  Trie(const Trie&) = default;
  Trie& operator=(const Trie&) = default;
  Trie(Trie&&) noexcept = default;
  Trie& operator=(Trie&&) noexcept = default;
  ~Trie() = default;

  // Inserts or overwrites the entry for `key` with `data`. An empty `key`
  // stores `data` at the root.
  template <typename U>
  void AddEntry(absl::string_view key, U&& data) {
    if (nodes_.empty()) {
      nodes_.emplace_back();
    }
    uint32_t index = 0;
    char32_t first_char = 0;
    absl::string_view rest;
    while (!key.empty()) {
      if (!Util::SplitFirstChar32(key, &first_char, &rest)) {
        return;
      }
      DCHECK_LT(nodes_.size(), std::numeric_limits<uint32_t>::max());
      const uint32_t new_index = static_cast<uint32_t>(nodes_.size());
      auto [it, inserted] =
          nodes_[index].children.try_emplace(first_char, new_index);
      // Copy `it->second` before `emplace_back()` may reallocate `nodes_`.
      index = it->second;
      if (inserted) {
        nodes_.emplace_back();
      }
      key = rest;
    }
    nodes_[index].data = std::forward<U>(data);
  }

  // Looks up the exact `key`. If found, copies its value to `*data` and returns
  // true; otherwise leaves `*data` unchanged and returns false.
  bool LookUp(absl::string_view key, T* data) const {
    const Node* node = FindNode(key);
    if (node == nullptr || !node->data.has_value()) {
      return false;
    }
    *data = *node->data;
    return true;
  }

  // Traverses the longest prefix of `key` that exists as a path in the trie and
  // sets `*key_length` to that path's byte length:
  // - If the reached node has data, copies it to `*data` and returns true.
  //   `*fixed` is set to false when that node has children (longer keys in the
  //   trie share the matched prefix, so the match is not yet unambiguous if
  //   more input follows), and true when it is a leaf node
  //   (`children.empty()`).
  // - If the reached node has no data, leaves `*data` unchanged, sets `*fixed`
  //   to true, and returns false without falling back to shorter prefixes
  //   (unlike `LongestMatch`).
  //
  // For example, if a trie has data for 'abc', 'abd', and 'a':
  //  - 'a'    -> true,  data='a',   key_length=1, fixed=false ('a' has child
  //                                                            'b')
  //  - 'ac'   -> true,  data='a',   key_length=1, fixed=false ('a' has child
  //                                                            'b')
  //  - 'abc'  -> true,  data='abc', key_length=3, fixed=true  ('abc' is a leaf)
  //  - 'abcd' -> true,  data='abc', key_length=3, fixed=true  ('abc' is a leaf)
  //  - 'abe'  -> false,             key_length=2, fixed=true  ('ab' has no
  //                                                            data; does not
  //                                                            refer to 'a')
  bool LookUpPrefix(absl::string_view key, T* data, size_t* key_length,
                    bool* fixed) const {
    if (nodes_.empty()) {
      *key_length = 0;
      *fixed = true;
      return false;
    }
    const size_t initial_size = key.size();
    uint32_t index = 0;
    char32_t first_char = 0;
    absl::string_view rest;
    while (!key.empty() && Util::SplitFirstChar32(key, &first_char, &rest)) {
      const auto it = nodes_[index].children.find(first_char);
      if (it == nodes_[index].children.end()) {
        break;
      }
      index = it->second;
      key = rest;
    }
    *key_length = initial_size - key.size();
    const Node& node = nodes_[index];
    if (!node.data.has_value()) {
      *fixed = true;
      return false;
    }
    *data = *node.data;
    *fixed = node.children.empty();
    return true;
  }

  // Finds the longest prefix of `key` with an associated value (falling back to
  // shorter prefixes when a deeper path has no value). If found, copies the
  // value to `*data`, sets `*key_length` to the matched byte length, and
  // returns true; otherwise sets `*key_length` to 0 and returns false.
  //
  // For example, if a trie has data for 'abc', 'abd', and 'a':
  //  - 'abc'  -> true, data='abc', key_length=3
  //  - 'abcd' -> true, data='abc', key_length=3
  //  - 'abe'  -> true, data='a',   key_length=1
  //  - 'ac'   -> true, data='a',   key_length=1
  bool LongestMatch(absl::string_view key, T* data, size_t* key_length) const {
    if (nodes_.empty()) {
      *key_length = 0;
      return false;
    }
    const size_t initial_size = key.size();
    uint32_t index = 0;
    const T* matched_data =
        nodes_[0].data.has_value() ? &*nodes_[0].data : nullptr;
    size_t matched_length = 0;
    char32_t first_char = 0;
    absl::string_view rest;
    while (!key.empty() && Util::SplitFirstChar32(key, &first_char, &rest)) {
      const auto it = nodes_[index].children.find(first_char);
      if (it == nodes_[index].children.end()) {
        break;
      }
      index = it->second;
      key = rest;
      if (nodes_[index].data.has_value()) {
        matched_data = &*nodes_[index].data;
        matched_length = initial_size - key.size();
      }
    }
    if (matched_data == nullptr) {
      *key_length = 0;
      return false;
    }
    *data = *matched_data;
    *key_length = matched_length;
    return true;
  }

  // Appends to `*data_list` all values whose keys start with `key`.
  // For example, if a trie has data for 'abc', 'abd', and 'a':
  //  - 'a'  -> appends 'abc', 'abd', 'a'
  //  - 'ab' -> appends 'abc', 'abd'
  //  - 'b'  -> appends nothing
  void LookUpPredictiveAll(absl::string_view key,
                           std::vector<T>* data_list) const {
    DCHECK(data_list);
    const Node* root = FindNode(key);
    if (root == nullptr) {
      return;
    }
    if (root->data.has_value()) {
      data_list->push_back(*root->data);
    }
    if (root->children.empty()) {
      return;
    }
    absl::InlinedVector<const Node*, 16> stack;
    stack.push_back(root);
    while (!stack.empty()) {
      const Node* node = stack.back();
      stack.pop_back();
      for (const auto& [unused_char, child_index] : node->children) {
        const Node& child = nodes_[child_index];
        if (child.data.has_value()) {
          data_list->push_back(*child.data);
        }
        if (!child.children.empty()) {
          stack.push_back(&child);
        }
      }
    }
  }

  // Returns true if `key` is non-empty and matches a path in the trie.
  bool HasSubTrie(absl::string_view key) const {
    return !key.empty() && FindNode(key) != nullptr;
  }

  void swap(Trie& other) noexcept { nodes_.swap(other.nodes_); }

  friend void swap(Trie& lhs, Trie& rhs) noexcept { lhs.swap(rhs); }

 private:
  using Children = absl::flat_hash_map<char32_t, uint32_t>;
  static_assert(sizeof(typename Children::value_type) == 8,
                "8-byte slot enables Abseil flat_hash_map inline SOO.");

  struct Node {
    Children children;
    std::optional<T> data;
  };

  const Node* FindNode(absl::string_view key) const {
    if (nodes_.empty()) {
      return nullptr;
    }
    uint32_t index = 0;
    char32_t first_char = 0;
    absl::string_view rest;
    while (!key.empty()) {
      if (!Util::SplitFirstChar32(key, &first_char, &rest)) {
        return nullptr;
      }
      const auto it = nodes_[index].children.find(first_char);
      if (it == nodes_[index].children.end()) {
        return nullptr;
      }
      index = it->second;
      key = rest;
    }
    return &nodes_[index];
  }

  std::vector<Node> nodes_;
};

}  // namespace mozc

#endif  // MOZC_BASE_CONTAINER_TRIE_H_
