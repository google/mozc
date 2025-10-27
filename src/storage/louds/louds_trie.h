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

#ifndef MOZC_STORAGE_LOUDS_LOUDS_TRIE_H_
#define MOZC_STORAGE_LOUDS_LOUDS_TRIE_H_

#include <cstddef>
#include <cstdint>

#include "absl/strings/string_view.h"
#include "storage/louds/louds.h"
#include "storage/louds/simple_succinct_bit_vector_index.h"

namespace mozc {
namespace storage {
namespace louds {

class LoudsTrie {
 public:
  // The max depth of the trie.
  static constexpr size_t kMaxDepth = 256;

  // This class stores a traversal state.
  typedef Louds::Node Node;

  LoudsTrie() = default;

  LoudsTrie(const LoudsTrie&) = delete;
  LoudsTrie& operator=(const LoudsTrie&) = delete;

  LoudsTrie(LoudsTrie&&) = default;
  LoudsTrie& operator=(LoudsTrie&&) = default;

  // Opens the binary image and constructs the data structure.  The first four
  // cache sizes are passed to the underlying LOUDS.  See louds.h for more
  // information of cache size.  The last one is passed to the underlying
  // terminal bit vector.  This class doesn't own the "data", so it is caller's
  // responsibility to keep the data alive until Close is invoked.  See .cc file
  // for the detailed format of the binary image.
  bool Open(const uint8_t* image, size_t louds_lb0_cache_size,
            size_t louds_lb1_cache_size, size_t louds_select0_cache_size,
            size_t louds_select1_cache_size, size_t termvec_lb1_cache_size);

  bool Open(const uint8_t* data) { return Open(data, 0, 0, 0, 0, 0); }

  // Destructs the internal data structure explicitly (the destructor will do
  // clean up too).
  void Close();

  // Generic APIs for tree traversal, some of which are delegated from Louds
  // class; see louds.h.

  // Returns true if |node| is in a valid state (returns true both for terminal
  // and non-terminal nodes).
  bool IsValidNode(const Node& node) const { return louds_.IsValidNode(node); }

  // Returns true if |node| is a terminal node.
  bool IsTerminalNode(const Node& node) const {
    return terminal_bit_vector_.Get(node.node_id() - 1) != 0;
  }

  // Returns the label of the edge from |node|'s parent (predecessor) to |node|.
  char GetEdgeLabelToParentNode(const Node& node) const {
    return edge_character_[node.node_id() - 1];
  }

  // Computes the ID of key that reaches to |node|.
  // REQUIRES: |node| is a terminal node.
  int GetKeyIdOfTerminalNode(const Node& node) const {
    return terminal_bit_vector_.Rank1(node.node_id() - 1);
  }

  // Initializes a node corresponding to |key_id|.
  // REQUIRES: |key_id| is a valid ID.
  void GetTerminalNodeFromKeyId(int key_id, Node* node) const {
    const int node_id = terminal_bit_vector_.Select1(key_id + 1) + 1;
    louds_.InitNodeFromNodeId(node_id, node);
  }

  Node GetTerminalNodeFromKeyId(int key_id) const {
    Node node;
    GetTerminalNodeFromKeyId(key_id, &node);
    return node;
  }

  // Restores the key string that reaches to |node|.  The caller is
  // responsible for allocating a buffer for the result string view, which needs
  // to be passed in |buf|.  The returned string view points to a piece of
  // |buf|.
  // REQUIRES: |buf| is longer than kMaxDepth + 1.
  absl::string_view RestoreKeyString(Node node, char* buf) const;

  // Restores the key string corresponding to |key_id|.  The caller is
  // responsible for allocating a buffer for the result string view, which needs
  // to be passed in |buf|.  The returned string view points to a piece of
  // |buf|.
  // REQUIRES: |buf| is longer than kMaxDepth + 1.
  absl::string_view RestoreKeyString(int key_id, char* buf) const {
    // TODO(noriyukit): Check if it's necessary to handle negative IDs.
    return key_id < 0 ? absl::string_view()
                      : RestoreKeyString(GetTerminalNodeFromKeyId(key_id), buf);
  }

  // Methods for moving node exported from Louds class; see louds.h.
  void MoveToFirstChild(Node* node) const { louds_.MoveToFirstChild(node); }
  Node MoveToFirstChild(Node node) const {
    MoveToFirstChild(&node);
    return node;
  }
  static void MoveToNextSibling(Node* node) { Louds::MoveToNextSibling(node); }
  static Node MoveToNextSibling(Node node) {
    MoveToNextSibling(&node);
    return node;
  }

  // Moves |node| to its child connected by the edge with |label|.  If there's
  // no edge having |label|, |node| becomes invalid and false is returned.
  bool MoveToChildByLabel(char label, Node* node) const;

  // Traverses a trie for |key|, starting from |node|, and modifies |node| to
  // the destination terminal node.  Here, |node| is not necessarily the root.
  // Returns false if there's no node reachable by |key|.
  bool Traverse(absl::string_view key, Node* node) const;

  // Higher level APIs.

  // Returns true if |key| is in this trie.
  bool HasKey(absl::string_view key) const {
    Node node;  // Root
    return Traverse(key, &node) && IsTerminalNode(node);
  }

  // Searches this trie for the key that exactly matches the given key. Returns
  // -1 if such key doesn't exist.
  // NOTE: When you only need to check if |key| is in this trie, use HasKey()
  // method, which is more efficient.
  int ExactSearch(absl::string_view key) const;

  // Runs a functor for the prefixes of |key| that exist in the trie.
  // |callback| needs to have the following signature:
  //
  // void(StringPiece key, StringPiece::size_type prefix_len,
  //      const LoudsTrie &trie, LoudsTrie::Node node)
  //
  // where
  //   key: The original input key (i.e., the same as the input |key|).
  //   prefix_len: The length of prefix, i.e., key.substr(0, prefix_len)
  //               is the matched prefix.
  //   trie: This trie.
  //   node: The location information, from which key ID can be recovered by
  //         LoudsTrie::GetKeyIdOfTerminalNode() method.
  template <typename Func>
  void PrefixSearch(absl::string_view key, Func callback) const {
    Node node;
    for (absl::string_view::size_type i = 0; i < key.size();) {
      if (!MoveToChildByLabel(key[i], &node)) {
        return;
      }
      ++i;  // Increment here for next loop and call |callback|.
      if (IsTerminalNode(node)) {
        callback(key, i, *this, node);
      }
    }
  }

 private:
  Louds louds_;  // Tree structure representation by LOUDS.

  // Bit-vector to represent whether each node in LOUDS tree is terminal.
  // This bit vector doesn't include "super root" in the LOUDS.
  // In other words, id=1 in louds_ corresponds to id=0 in terminal_bit_vector_,
  // id=10 in louds_ corresponds to id=9 in terminal_bit_vector_, and so on.
  // TODO(noriyukit): Simplify the id-mapping by introducing a bit for the
  // super root in this bit vector.
  SimpleSuccinctBitVectorIndex terminal_bit_vector_;

  // A sequence of characters, annotated to each edge.
  // This array also doesn't have an entry for super root.
  // In other words, id=2 in louds_ corresponds to edge_character_[1].
  const char* edge_character_ = nullptr;
};

}  // namespace louds
}  // namespace storage
}  // namespace mozc

#endif  // MOZC_STORAGE_LOUDS_LOUDS_TRIE_H_
