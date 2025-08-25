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

#ifndef MOZC_CONVERTER_LATTICE_H_
#define MOZC_CONVERTER_LATTICE_H_

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "converter/node.h"
#include "converter/node_allocator.h"

namespace mozc {

class Lattice {
 public:
  Lattice() : node_allocator_(std::make_unique<NodeAllocator>()) {}

  NodeAllocator* node_allocator() const { return node_allocator_.get(); }

  // set key and initializes lattice with key.
  void SetKey(std::string key);

  // return key.
  absl::string_view key() const { return key_; }

  // allocate new node.
  Node* NewNode() { return node_allocator_->NewNode(); }

  // return the array of nodes starting with `pos`.
  absl::Span<Node* const> begin_nodes(size_t pos) const {
    DCHECK_LE(pos, key_.size());
    return begin_nodes_[pos];
  }

  // return the array of nodes ending with `pos`.
  absl::Span<Node* const> end_nodes(size_t pos) const {
    DCHECK_LE(pos, key_.size());
    return end_nodes_[pos];
  }

  // return bos or eos node.
  Node* bos_node() {
    DCHECK_EQ(end_nodes_[0].size(), 1);
    return end_nodes_[0].front();
  }

  Node* eos_node() {
    DCHECK_EQ(begin_nodes_[key_.size()].size(), 1);
    return begin_nodes_[key_.size()].front();
  }

  const Node* bos_node() const {
    DCHECK_EQ(end_nodes_[0].size(), 1);
    return end_nodes_[0].front();
  }

  const Node* eos_node() const {
    DCHECK_EQ(begin_nodes_[key_.size()].size(), 1);
    return begin_nodes_[key_.size()].front();
  }

  // inset one node to the position `pos`.
  void Insert(size_t pos, Node* node);

  // inset multiple nodes to the position `pos`.
  void Insert(size_t pos, absl::Span<Node* const> nodes);

  // return true if this instance has a valid lattice.
  bool has_lattice() const { return !begin_nodes_.empty(); }

  // Dump the best path and the path that contains the designated string.
  std::string DebugString() const;

 private:
  // clear all lattice and nodes allocated with NewNode method
  // Only called via Setkey().
  void Clear();

  std::string key_;
  std::vector<std::vector<Node*>> begin_nodes_;
  std::vector<std::vector<Node*>> end_nodes_;
  std::unique_ptr<NodeAllocator> node_allocator_;
};

// RAII class to insert nodes in detractor.
// Adding a node while iterating through a vector/span is generally unsafe
// because it can invalidate the vector's internal iterators. To avoid this, we
// can use this helper class. Nodes are inserted in a batch during the
// destructor.
//
// ScopedLatticeNodeInserter inserter(&lattice);
// for (const Node *node : lattice.begin_nodes(pos)) {
//   inserter.Insert(pos, new_node);
// }
class ScopedLatticeNodeInserter {
 public:
  explicit ScopedLatticeNodeInserter(Lattice* lattice) : lattice_(lattice) {}
  ~ScopedLatticeNodeInserter() {
    for (auto [pos, node] : inserted_) {
      lattice_->Insert(pos, node);
    }
  }

  bool IsInserted() const { return !inserted_.empty(); }

  void Insert(size_t pos, Node* node) { inserted_.emplace_back(pos, node); }

 private:
  Lattice* lattice_ = nullptr;
  std::vector<std::pair<size_t, Node*>> inserted_;
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_LATTICE_H_
