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

#include "converter/lattice.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/strings/unicode.h"
#include "converter/node.h"
#include "converter/node_allocator.h"

namespace mozc {
namespace {

Node* InitBOSNode(Node* bos_node, uint16_t position) {
  DCHECK(bos_node);
  bos_node->rid = 0;  // 0 is reserved for EOS/BOS
  bos_node->lid = 0;
  bos_node->key.clear();
  bos_node->value = "BOS";
  bos_node->node_type = Node::BOS_NODE;
  bos_node->wcost = 0;
  bos_node->cost = 0;
  bos_node->begin_pos = position;
  bos_node->end_pos = position;
  return bos_node;
}

Node* InitEOSNode(Node* eos_node, uint16_t position) {
  DCHECK(eos_node);
  eos_node->rid = 0;  // 0 is reserved for EOS/BOS
  eos_node->lid = 0;
  eos_node->key.clear();
  eos_node->value = "EOS";
  eos_node->node_type = Node::EOS_NODE;
  eos_node->wcost = 0;
  eos_node->cost = 0;
  eos_node->begin_pos = position;
  eos_node->end_pos = position;
  return eos_node;
}

}  // namespace

void Lattice::SetKey(std::string key) {
  Clear();
  key_ = std::move(key);
  begin_nodes_.resize(key_.size() + 1);
  end_nodes_.resize(key_.size() + 1);

  for (std::vector<Node*>& nodes : begin_nodes_) {
    nodes.clear();
    nodes.reserve(32);
  }

  for (std::vector<Node*>& nodes : end_nodes_) {
    nodes.clear();
    nodes.reserve(32);
  }

  Node* bos_node = InitBOSNode(NewNode(), 0);
  Node* eos_node = InitEOSNode(NewNode(), static_cast<uint16_t>(key_.size()));

  end_nodes_[0].push_back(bos_node);
  begin_nodes_[key_.size()].push_back(eos_node);
}

void Lattice::Insert(size_t pos, Node* node) {
  const size_t end_pos = std::min(node->key.size() + pos, key_.size());
  node->begin_pos = static_cast<uint16_t>(pos);
  node->end_pos = static_cast<uint16_t>(end_pos);
  node->prev = nullptr;
  node->next = nullptr;
  node->cost = 0;
  begin_nodes_[pos].emplace_back(node);
  end_nodes_[end_pos].emplace_back(node);
}

void Lattice::Insert(size_t pos, absl::Span<Node* const> nodes) {
  std::vector<Node*>& begin_nodes = begin_nodes_[pos];
  begin_nodes.reserve(begin_nodes.size() + nodes.size());
  for (Node* node : nodes) {
    Insert(pos, node);
  }
}

void Lattice::Clear() {
  key_.clear();
  begin_nodes_.clear();
  end_nodes_.clear();
  node_allocator_->Free();
}

std::string Lattice::DebugString() const {
  if (!has_lattice()) {
    return "";
  }

  std::vector<const Node*> node_vector;
  for (const Node* node = eos_node(); node; node = node->prev) {
    node_vector.push_back(node);
  }

  std::stringstream os;
  const Node* prev_node = nullptr;
  for (auto it = node_vector.rbegin(); it != node_vector.rend(); ++it) {
    const Node* node = *it;
    os << "[con:"
       << node->cost - (prev_node ? prev_node->cost : 0) - node->wcost << "]";
    os << "[lid:" << node->lid << "]";
    os << "\"" << node->value << "\"";
    os << "[wcost:" << node->wcost << "]";
    os << "[cost:" << node->cost << "]";
    os << "[rid:" << node->rid << "]";
    prev_node = node;
  }

  return os.str();
}

}  // namespace mozc
