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

#ifndef MOZC_STORAGE_LOUDS_LOUDS_H_
#define MOZC_STORAGE_LOUDS_LOUDS_H_

#include "base/port.h"
#include "storage/louds/simple_succinct_bit_vector_index.h"

namespace mozc {
namespace storage {
namespace louds {

// Implementation of the LOUDS (Level-Ordered Unary degree sequence).
// LOUDS is a representation of tree by bit sequence, which supports "rank"
// and "select" operations.
// The representation of each node is "n 1-bits, followed by a 0-bit," where
// n is the number of its children.
// So, for example, if a node has two children, the erepresentation of
// the node is "110". Another example is that a leaf is represented by "0".
// The nodes are stored in breadth first order, and each node has its id
// (0-origin, i.e. the root node is '0', the first child of the root is '1'
// and so on).
// This class provides some utilities between the "index of the bit array
// representation" and "node id of the tree".
// In this representation, we can think that each '1'-bit is corresponding
// to an edge.
class Louds {
 public:
  Louds() {
  }

  void Open(const uint8 *image, int length) {
    index_.Init(image, length);
  }
  void Close() {
    index_.Reset();
  }

  // Returns true, if the corresponding bit represents an edge.
  // TODO(hidehiko): Check the performance of the conversion between int and
  // bool, because this method should be invoked so many times.
  inline bool IsEdgeBit(int bit_index) const {
    return index_.Get(bit_index);
  }

  // Returns the child node id of the edge corresponding to the given bit
  // index.
  // Note: the bit at the given index must be '1'.
  inline int GetChildNodeId(int bit_index) const {
    return index_.Rank1(bit_index) + 1;
  }

  // Returns the parent node id of the edge corresponding to the given bit
  // index.
  // Note: this takes the bit index (as the argument variable name),
  // so it is OK to pass the bit index even if the bit is '0'.
  inline int GetParentNodeId(int bit_index) const {
    return index_.Rank0(bit_index);
  }

  // Returns the bit index corresponding to the node's first edge.
  inline int GetFirstEdgeBitIndex(int node_id) const {
    return index_.Select0(node_id) + 1;
  }

  // Returns the bit index corresponding to "the node to its parent" edge.
  // Note: the root node has no parent, so node_id must > 0.
  inline int GetParentEdgeBitIndex(int node_id) const {
    return index_.Select1(node_id);
  }

  // Returns the node id for the first child node of the given node.
  // The bit_index must be the bit index of the first edge bit index of the
  // node. This method calculates the id effectively, at least more effective
  // than calculating by Rank.
  static inline int GetChildNodeId(int node_id, int bit_index) {
    // Note: node_id == Rank0(bit_index),
    // so "bit_index - node_id" == Rank1(bit_index).
    // The first child_node_id is "Rank1(bit_index) + 1".
    return bit_index - node_id + 1;
  }

 private:
  SimpleSuccinctBitVectorIndex index_;

  DISALLOW_COPY_AND_ASSIGN(Louds);
};

}  // namespace louds
}  // namespace storage
}  // namespace mozc

#endif  // MOZC_STORAGE_LOUDS_LOUDS_H_
