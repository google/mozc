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
#include <cstdint>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/singleton.h"
#include "base/util.h"
#include "converter/node.h"
#include "converter/node_allocator.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {

Node *InitBOSNode(Lattice *lattice, uint16_t length) {
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
  bos_node->enext = nullptr;
  return bos_node;
}

Node *InitEOSNode(Lattice *lattice, uint16_t length) {
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
  eos_node->bnext = nullptr;
  return eos_node;
}

bool PathContainsString(const Node *node, size_t begin_pos, size_t end_pos,
                        const std::string &str) {
  CHECK(node);
  for (; node->prev != nullptr; node = node->prev) {
    if (node->begin_pos == begin_pos && node->end_pos == end_pos &&
        node->value == str) {
      return true;
    }
  }
  return false;
}

std::string GetDebugStringForNode(const Node *node, const Node *prev_node) {
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

std::string GetDebugStringForPath(const Node *end_node) {
  CHECK(end_node);
  std::stringstream os;
  std::vector<const Node *> node_vector;

  for (const Node *node = end_node; node; node = node->prev) {
    node_vector.push_back(node);
  }
  const Node *prev_node = nullptr;

  for (int i = static_cast<int>(node_vector.size()) - 1; i >= 0; --i) {
    const Node *node = node_vector[i];
    os << GetDebugStringForNode(node, prev_node);
    prev_node = node;
  }
  return os.str();
}

std::string GetCommonPrefix(const std::string &str1, const std::string &str2) {
  std::vector<std::string> split1, split2;
  Util::SplitStringToUtf8Chars(str1, &split1);
  Util::SplitStringToUtf8Chars(str2, &split2);
  std::string common_prefix = "";
  for (int i = 0; i < std::min(split1.size(), split2.size()); ++i) {
    if (split1[i] == split2[i]) {
      common_prefix += split1[i];
    } else {
      break;
    }
  }
  return common_prefix;
}

}  // namespace

struct LatticeDisplayNodeInfo {
  size_t display_node_begin_pos;
  size_t display_node_end_pos;
  std::string display_node_str;
};

void Lattice::SetKey(absl::string_view key) {
  Clear();
  key_.assign(key.data(), key.size());
  const size_t size = key.size();
  begin_nodes_.resize(size + 4);
  end_nodes_.resize(size + 4);
  cache_info_.resize(size + 4);

  std::fill(begin_nodes_.begin(), begin_nodes_.end(),
            static_cast<Node *>(nullptr));
  std::fill(end_nodes_.begin(), end_nodes_.end(), static_cast<Node *>(nullptr));
  std::fill(cache_info_.begin(), cache_info_.end(), 0);

  end_nodes_[0] = InitBOSNode(this, static_cast<uint16_t>(0));
  begin_nodes_[key_.size()] =
      InitEOSNode(this, static_cast<uint16_t>(key_.size()));
}

void Lattice::Insert(size_t pos, Node *node) {
  for (Node *rnode = node; rnode != nullptr; rnode = rnode->bnext) {
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
    for (Node *rnode = node; rnode != nullptr; rnode = rnode->bnext) {
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
  cache_info_.clear();
  history_end_pos_ = 0;
}

void Lattice::SetDebugDisplayNode(size_t begin_pos, size_t end_pos,
                                  const std::string &str) {
  LatticeDisplayNodeInfo *info = Singleton<LatticeDisplayNodeInfo>::get();
  info->display_node_begin_pos = begin_pos;
  info->display_node_end_pos = end_pos;
  info->display_node_str = str;
}

void Lattice::ResetDebugDisplayNode() {
  LatticeDisplayNodeInfo *info = Singleton<LatticeDisplayNodeInfo>::get();
  info->display_node_str.clear();
}

void Lattice::UpdateKey(const std::string &new_key) {
  const std::string old_key = key_;
  const std::string common_prefix = GetCommonPrefix(new_key, old_key);

  // if the length of common prefix is too short, call SetKey
  if (common_prefix.size() <= old_key.size() / 2) {
    SetKey(new_key);
    return;
  }

  // if node_allocator has many nodes, then clean up
  const size_t size_threshold = node_allocator_->max_nodes_size();
  if (node_allocator_->node_count() > size_threshold) {
    SetKey(new_key);
    return;
  }

  // erase the suffix of old_key so that the key becomes common_prefix
  ShrinkKey(common_prefix.size());
  // add a suffix so that the key becomes new_key
  AddSuffix(new_key.substr(common_prefix.size()));
}

void Lattice::AddSuffix(const std::string &suffix_key) {
  if (suffix_key.empty()) {
    return;
  }
  const size_t old_size = key_.size();
  const size_t new_size = key_.size() + suffix_key.size();

  // update begin_nodes and end_nodes
  begin_nodes_.resize(new_size + 4);
  end_nodes_.resize(new_size + 4);

  std::fill(begin_nodes_.begin() + old_size, begin_nodes_.end(),
            static_cast<Node *>(nullptr));
  std::fill(end_nodes_.begin() + old_size + 1, end_nodes_.end(),
            static_cast<Node *>(nullptr));

  end_nodes_[0] = InitBOSNode(this, static_cast<uint16_t>(0));
  begin_nodes_[new_size] = InitEOSNode(this, static_cast<uint16_t>(new_size));

  // update cache_info
  cache_info_.resize(new_size + 4, 0);

  // update key
  key_ += suffix_key;
}

void Lattice::ShrinkKey(const size_t new_len) {
  const size_t old_len = key_.size();
  CHECK_LE(new_len, old_len);
  if (new_len == old_len) {
    return;
  }

  // erase nodes whose end position exceeds new_len
  for (size_t i = 0; i < new_len; ++i) {
    Node *begin = begin_nodes_[i];
    if (begin == nullptr) {
      continue;
    }

    for (Node *prev = begin, *curr = begin->bnext; curr != nullptr;) {
      CHECK(prev);
      if (curr->end_pos > new_len) {
        prev->bnext = curr->bnext;
        curr = curr->bnext;
      } else {
        prev = prev->bnext;
        curr = curr->bnext;
      }
    }
    if (begin->end_pos > new_len) {
      begin_nodes_[i] = begin->bnext;
    }
  }

  // update begin_nodes and end_nodes
  for (size_t i = new_len; i <= old_len; ++i) {
    begin_nodes_[i] = nullptr;
  }
  for (size_t i = new_len + 1; i <= old_len; ++i) {
    end_nodes_[i] = nullptr;
  }
  begin_nodes_[new_len] = InitEOSNode(this, static_cast<uint16_t>(new_len));

  // update cache_info
  for (size_t i = 0; i < new_len; ++i) {
    cache_info_[i] = std::min(cache_info_[i], new_len - i);
  }
  std::fill(cache_info_.begin() + new_len, cache_info_.end(), 0);

  // update key
  key_.erase(new_len);
}

void Lattice::ResetNodeCost() {
  for (size_t i = 0; i <= key_.size(); ++i) {
    if (begin_nodes_[i] != nullptr) {
      Node *prev = nullptr;
      for (Node *node = begin_nodes_[i]; node != nullptr; node = node->bnext) {
        // do not process BOS / EOS nodes
        if (node->node_type == Node::BOS_NODE ||
            node->node_type == Node::EOS_NODE) {
          continue;
        }
        // if the node has ENABLE_CACHE attribute, then revert its wcost.
        // Otherwise, erase the node from the lattice.
        if (node->attributes & Node::ENABLE_CACHE) {
          node->wcost = node->raw_wcost;
        } else {
          if (node == begin_nodes_[i]) {
            if (node->bnext == nullptr) {
              begin_nodes_[i] = nullptr;
            } else {
              begin_nodes_[i] = node->bnext;
            }
          } else {
            CHECK(prev);
            CHECK_EQ(prev->bnext, node);
            prev->next = node->bnext;
          }
        }
        // traverse a next node
        prev = node;
      }
    }

    if (end_nodes_[i] != nullptr) {
      Node *prev = nullptr;
      for (Node *node = end_nodes_[i]; node != nullptr; node = node->enext) {
        if (node->node_type == Node::BOS_NODE ||
            node->node_type == Node::EOS_NODE) {
          continue;
        }
        if (node->attributes & Node::ENABLE_CACHE) {
          node->wcost = node->raw_wcost;
        } else {
          if (node == end_nodes_[i]) {
            if (node->enext == nullptr) {
              end_nodes_[i] = nullptr;
            } else {
              end_nodes_[i] = node->enext;
            }
          } else {
            CHECK(prev);
            CHECK_EQ(prev->enext, node);
            prev->next = node->enext;
          }
        }
        prev = node;
      }
    }
  }
}

std::string Lattice::DebugString() const {
  std::stringstream os;
  if (!has_lattice()) {
    return "";
  }

  std::vector<const Node *> best_path_nodes;

  const Node *node = eos_nodes();
  // Print the best path
  os << "Best path: ";
  os << GetDebugStringForPath(node);
  os << std::endl;

  LatticeDisplayNodeInfo *info = Singleton<LatticeDisplayNodeInfo>::get();

  if (info->display_node_str.empty()) {
    return os.str();
  }

  for (; node != nullptr; node = node->prev) {
    best_path_nodes.push_back(node);
  }

  // Print the path that contains the designated node
  for (std::vector<const Node *>::const_iterator it = best_path_nodes.begin();
       it != best_path_nodes.end(); ++it) {
    const Node *best_path_node = *it;
    if (best_path_node->begin_pos < info->display_node_end_pos) {
      break;
    }
    for (const Node *prev_node = end_nodes(best_path_node->begin_pos);
         prev_node; prev_node = prev_node->enext) {
      if (!PathContainsString(prev_node, info->display_node_begin_pos,
                              info->display_node_end_pos,
                              info->display_node_str)) {
        continue;
      }
      os << "The path " << GetDebugStringForPath(prev_node)
         << " ( + connection cost + wcost: " << best_path_node->wcost << ")"
         << std::endl
         << "was defeated"
         << " by the path " << std::endl
         << GetDebugStringForPath(best_path_node->prev)
         << " connecting to the node "
         << GetDebugStringForNode(best_path_node, best_path_node->prev)
         << std::endl;
    }
  }

  return os.str();
}

}  // namespace mozc
