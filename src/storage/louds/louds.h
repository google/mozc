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

#ifndef MOZC_STORAGE_LOUDS_LOUDS_H_
#define MOZC_STORAGE_LOUDS_LOUDS_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "storage/louds/simple_succinct_bit_vector_index.h"

namespace mozc {
namespace storage {
namespace louds {

// Implementation of the Level-Ordered Unary Degree Sequence (LOUDS).
//
// LOUDS represents a tree structure using a bit sequence.  A node having N
// children is represented by N 1's and one trailing 0.  For example, "110"
// represents a node with 2 children, while "0" represents a leaf.  The bit
// sequence starts with the representation of super-root, "10", and is followed
// by representations of nodes in order of breadth first manner; see the
// following example:
//
//              0 (super root)
//              |
//              1 (root)
//            /   \                                                           .
//           2     3
//                / \                                                         .
//               4   5
//
//  Node:  0   1    2  3    4  5
// LOUDS:  10  110  0  110  0  0
//
// This class provides basic APIs to traverse a tree structure.  Performance
// critical methods are inlined.
class Louds {
 public:
  // Represents and stores location (tree node) for tree traversal.  By storing
  // instances of this class, traversal state can be restored.  This class is
  // copy-efficient.
  class Node {
   public:
    constexpr int node_id() const { return node_id_; }

    friend constexpr bool operator==(const Node& x, const Node& y) {
      return x.edge_index_ == y.edge_index_ && x.node_id_ == y.node_id_;
    }

   private:
    // Default instance represents the root node (not the super-root).
    int edge_index_ = 0;
    int node_id_ = 1;
    friend class Louds;
  };

  Louds() = default;

  Louds(const Louds&) = delete;
  Louds& operator=(const Louds&) = delete;

  Louds(Louds&&) = default;
  Louds& operator=(Louds&&) = default;

  ~Louds() = default;

  // Initializes this LOUDS from bit array.  To improve the performance of
  // downward traversal (i.e., from root to leaves), set |bitvec_lb0_cache_size|
  // and |select0_cache_size| to larger values.  On the other hand, to improve
  // the performance of upward traversal (i.e., from leaves to the root), set
  // |bitvec_lb1_cache_size| and |select1_cache_size| to larger values.
  void Init(const uint8_t* image, int length, size_t bitvec_lb0_cache_size,
            size_t bitvec_lb1_cache_size, size_t select0_cache_size,
            size_t select1_cache_size);

  // Explicitly clears the internal bit array.
  void Reset();

  // APIs for traversal (all the methods are inline for performance).

  // Initializes a Node instance from node ID.
  // Note: to get the root node, just allocate a default Node instance.
  void InitNodeFromNodeId(int node_id, Node* node) const {
    node->node_id_ = node_id;
    node->edge_index_ = node_id < select1_cache_size_
                            ? select1_cache_ptr_[node_id]
                            : index_.Select1(node_id);
  }

  // Returns true if the given node is the root.
  static bool IsRoot(const Node& node) { return node.node_id_ == 1; }

  // Moves the given node to its first (most left) child.  If |node| is a leaf,
  // the resulting node becomes invalid.  For example, in the above diagram of
  // tree, moves are as follows:
  //   * node 1 -> node 2
  //   * node 3 -> node 4
  //   * node 4 -> invalid node
  // REQUIRES: |node| is valid.
  void MoveToFirstChild(Node* node) const {
    node->edge_index_ = node->node_id_ < select0_cache_size_
                            ? select_cache_[node->node_id_]
                            : index_.Select0(node->node_id_) + 1;
    node->node_id_ = node->edge_index_ - node->node_id_ + 1;
  }

  // Moves the given node to its next (right) sibling.  If there's no sibling
  // for |node|, the resulting node becomes invalid. For example, in the above
  // diagram of tree, moves are as follows:
  //   * node 1 -> invalid node
  //   * node 2 -> node 3
  //   * node 5 -> invalid node
  // REQUIRES: |node| is valid.
  static void MoveToNextSibling(Node* node) {
    ++node->edge_index_;
    ++node->node_id_;
  }

  // Moves the given node to its unique parent.  For example, in the above
  // diagram of tree, moves are as follows:
  //   * node 2 -> node 1
  //   * node 3 -> node 1
  //   * node 5 -> node 3
  // REQUIRES: |node| is valid and not root.
  void MoveToParent(Node* node) const {
    node->node_id_ = node->edge_index_ - node->node_id_ + 1;
    node->edge_index_ = node->node_id_ < select1_cache_size_
                            ? select1_cache_ptr_[node->node_id_]
                            : index_.Select1(node->node_id_);
  }

  // Returns true if |node| is in a valid state.
  bool IsValidNode(const Node& node) const {
    return index_.Get(node.edge_index_) != 0;
  }

 private:
  SimpleSuccinctBitVectorIndex index_;
  size_t select0_cache_size_ = 0;
  size_t select1_cache_size_ = 0;
  std::unique_ptr<int[]> select_cache_;
  int* select1_cache_ptr_;  // = select_cache_.get() + select0_cache_size_
};

}  // namespace louds
}  // namespace storage
}  // namespace mozc

#endif  // MOZC_STORAGE_LOUDS_LOUDS_H_
