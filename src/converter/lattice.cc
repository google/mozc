// Copyright 2010-2011, Google Inc.
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

#include "converter/lattice.h"

#include <string>

#include "base/base.h"
#include "converter/node.h"
#include "converter/node_allocator.h"
#include "dictionary/pos_matcher.h"

namespace mozc {

namespace {

Node *InitBOSNode(Lattice *lattice, uint16 length) {
  Node *bos_node = lattice->NewNode();
  DCHECK(bos_node);
  bos_node->rid = 0;  // 0 is reserved for EOS/BOS
  bos_node->lid = 0;
  bos_node->key.clear();
  bos_node->value = "BOS";
  bos_node->node_type = Node::BOS_NODE;
  bos_node->wcost = 0;
  bos_node->cost = 0;
  bos_node->begin_pos = length;
  bos_node->end_pos = length;
  bos_node->enext = NULL;
  return bos_node;
}

Node *InitEOSNode(Lattice *lattice, uint16 length) {
  Node *eos_node = lattice->NewNode();
  DCHECK(eos_node);
  eos_node->rid = 0;  // 0 is reserved for EOS/BOS
  eos_node->lid = 0;
  eos_node->key.clear();
  eos_node->value = "EOS";
  eos_node->node_type = Node::EOS_NODE;
  eos_node->wcost = 0;
  eos_node->cost = 0;
  eos_node->begin_pos = length;
  eos_node->end_pos = length;
  eos_node->bnext = NULL;
  return eos_node;
}
}  // namespace

Lattice::Lattice() : node_allocator_(new NodeAllocator) {}

Lattice::~Lattice() {}

NodeAllocatorInterface *Lattice::node_allocator() const {
  return node_allocator_.get();
}

Node *Lattice::NewNode() {
  return node_allocator_->NewNode();
}

Node *Lattice::begin_nodes(size_t pos) const {
  return begin_nodes_[pos];
}

Node *Lattice::end_nodes(size_t pos) const {
  return end_nodes_[pos];
}

void Lattice::SetKey(const string &key) {
  Clear();
  key_ = key;
  const size_t size = key.size();
  begin_nodes_.resize(size + 4);
  end_nodes_.resize(size + 4);

  fill(begin_nodes_.begin(), begin_nodes_.end(),
       static_cast<Node *>(NULL));
  fill(end_nodes_.begin(), end_nodes_.end(),
       static_cast<Node *>(NULL));

  end_nodes_[0] = InitBOSNode(this,
                              static_cast<uint16>(0));
  begin_nodes_[key_.size()] =
      InitEOSNode(this, static_cast<uint16>(key_.size()));
}

Node *Lattice::bos_nodes() const {
  return end_nodes_[0];
}

Node *Lattice::eos_nodes() const {
  return begin_nodes_[key_.size()];
}

void Lattice::Insert(size_t pos, Node *node) {
  for (Node *rnode = node; rnode != NULL; rnode = rnode->bnext) {
    rnode->begin_pos = static_cast<uint16>(pos);
    rnode->end_pos = static_cast<uint16>(pos + rnode->key.size());
    rnode->prev = NULL;
    rnode->next = NULL;
    rnode->cost = 0;
    const size_t x = rnode->key.size() + pos;
    rnode->enext = end_nodes_[x];
    end_nodes_[x] = rnode;
  }

  if (begin_nodes_[pos] == NULL) {
    begin_nodes_[pos] = node;
  } else {
    for (Node *rnode = node; rnode != NULL; rnode = rnode->bnext) {
      if (rnode->bnext == NULL) {
        rnode->bnext = begin_nodes_[pos];
        begin_nodes_[pos] = node;
        break;
      }
    }
  }
}

const string &Lattice::key() const {
  return key_;
}

bool Lattice::has_lattice() const {
  return !begin_nodes_.empty();
}

void Lattice::Clear() {
  key_.clear();
  begin_nodes_.clear();
  end_nodes_.clear();
  node_allocator_->Free();
}
}  // namespace mozc
