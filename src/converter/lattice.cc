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
#include "base/singleton.h"
#include "base/strings/unicode.h"
#include "converter/node.h"
#include "converter/node_allocator.h"

namespace mozc {
namespace {

Node* InitBOSNode(Lattice* lattice, uint16_t length) {
  Node* bos_node = lattice->NewNode();
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
  bos_node->enext = nullptr;
  return bos_node;
}

Node* InitEOSNode(Lattice* lattice, uint16_t length) {
  Node* eos_node = lattice->NewNode();
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
  eos_node->bnext = nullptr;
  return eos_node;
}

bool PathContainsString(const Node* node, size_t begin_pos, size_t end_pos,
                        const absl::string_view str) {
  CHECK(node);
  for (; node->prev != nullptr; node = node->prev) {
    if (node->begin_pos == begin_pos && node->end_pos == end_pos &&
        node->value == str) {
      return true;
    }
  }
  return false;
}

std::string GetDebugStringForNode(const Node* node, const Node* prev_node) {
  CHECK(node);
  std::stringstream os;
  os << "[con:" << node->cost - (prev_node ? prev_node->cost : 0) - node->wcost
     << "]";
  os << "[lid:" << node->lid << "]";
  os << "\"" << node->value << "\"";
  os << "[wcost:" << node->wcost << "]";
  os << "[cost:" << node->cost << "]";
  os << "[rid:" << node->rid << "]";
  return os.str();
}

std::string GetDebugStringForPath(const Node* end_node) {
  CHECK(end_node);
  std::stringstream os;
  std::vector<const Node*> node_vector;

  for (const Node* node = end_node; node; node = node->prev) {
    node_vector.push_back(node);
  }
  const Node* prev_node = nullptr;

  for (int i = static_cast<int>(node_vector.size()) - 1; i >= 0; --i) {
    const Node* node = node_vector[i];
    os << GetDebugStringForNode(node, prev_node);
    prev_node = node;
  }
  return os.str();
}

absl::string_view GetCommonPrefix(absl::string_view str1,
                                  absl::string_view str2) {
  const absl::string_view orig_str = str1;
  size_t common_size = 0;
  while (!str1.empty() && !str2.empty()) {
    absl::string_view c1, c2;
    std::tie(c1, str1) = strings::FrontChar(str1);
    std::tie(c2, str2) = strings::FrontChar(str2);
    if (c1 != c2) {
      break;
    }
    common_size += c1.size();
  }
  return orig_str.substr(0, common_size);
}

}  // namespace

struct LatticeDisplayNodeInfo {
  size_t display_node_begin_pos;
  size_t display_node_end_pos;
  std::string display_node_str;
};

void Lattice::SetKey(std::string key) {
  Clear();
  const size_t size = key.size();
  key_ = std::move(key);
  begin_nodes_.resize(size + 4, nullptr);
  end_nodes_.resize(size + 4, nullptr);

  end_nodes_[0] = InitBOSNode(this, static_cast<uint16_t>(0));
  begin_nodes_[key_.size()] =
      InitEOSNode(this, static_cast<uint16_t>(key_.size()));
}

void Lattice::Insert(size_t pos, Node* node) {
  for (Node* rnode = node; rnode != nullptr; rnode = rnode->bnext) {
    const size_t end_pos = std::min(rnode->key.size() + pos, key_.size());
    rnode->begin_pos = static_cast<uint16_t>(pos);
    rnode->end_pos = static_cast<uint16_t>(end_pos);
    rnode->prev = nullptr;
    rnode->next = nullptr;
    rnode->cost = 0;
    rnode->enext = end_nodes_[end_pos];
    end_nodes_[end_pos] = rnode;
  }

  if (begin_nodes_[pos] == nullptr) {
    begin_nodes_[pos] = node;
  } else {
    for (Node* rnode = node; rnode != nullptr; rnode = rnode->bnext) {
      if (rnode->bnext == nullptr) {
        rnode->bnext = begin_nodes_[pos];
        begin_nodes_[pos] = node;
        break;
      }
    }
  }
}

void Lattice::Clear() {
  key_.clear();
  begin_nodes_.clear();
  end_nodes_.clear();
  node_allocator_->Free();
}

void Lattice::SetDebugDisplayNode(size_t begin_pos, size_t end_pos,
                                  std::string str) {
  LatticeDisplayNodeInfo* info = Singleton<LatticeDisplayNodeInfo>::get();
  info->display_node_begin_pos = begin_pos;
  info->display_node_end_pos = end_pos;
  info->display_node_str = std::move(str);
}

void Lattice::ResetDebugDisplayNode() {
  LatticeDisplayNodeInfo* info = Singleton<LatticeDisplayNodeInfo>::get();
  info->display_node_str.clear();
}

std::string Lattice::DebugString() const {
  std::stringstream os;
  if (!has_lattice()) {
    return "";
  }

  std::vector<const Node*> best_path_nodes;

  const Node* node = eos_nodes();
  // Print the best path
  os << "Best path: ";
  os << GetDebugStringForPath(node);
  os << std::endl;

  LatticeDisplayNodeInfo* info = Singleton<LatticeDisplayNodeInfo>::get();

  if (info->display_node_str.empty()) {
    return os.str();
  }

  for (; node != nullptr; node = node->prev) {
    best_path_nodes.push_back(node);
  }

  // Print the path that contains the designated node
  for (std::vector<const Node*>::const_iterator it = best_path_nodes.begin();
       it != best_path_nodes.end(); ++it) {
    const Node* best_path_node = *it;
    if (best_path_node->begin_pos < info->display_node_end_pos) {
      break;
    }
    for (const Node* prev_node = end_nodes(best_path_node->begin_pos);
         prev_node; prev_node = prev_node->enext) {
      if (!PathContainsString(prev_node, info->display_node_begin_pos,
                              info->display_node_end_pos,
                              info->display_node_str)) {
        continue;
      }
      os << "The path " << GetDebugStringForPath(prev_node)
         << " ( + connection cost + wcost: " << best_path_node->wcost << ")"
         << std::endl
         << "was defeated" << " by the path " << std::endl
         << GetDebugStringForPath(best_path_node->prev)
         << " connecting to the node "
         << GetDebugStringForNode(best_path_node, best_path_node->prev)
         << std::endl;
    }
  }

  return os.str();
}

}  // namespace mozc
