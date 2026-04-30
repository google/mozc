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

#include "converter/debug_util.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "converter/lattice.h"
#include "converter/node.h"

namespace mozc {
namespace converter {

std::string DumpNodes(const Lattice& lattice) {
  // Map from Node pointer to a unique ID.
  absl::flat_hash_map<const Node*, uint32_t> node_id_map;
  node_id_map[nullptr] = 0;

  // Returns a unique ID for the given node.
  auto node_id = [&node_id_map](const Node* node) -> uint32_t {
    auto it = node_id_map.find(node);
    if (it != node_id_map.end()) {
      return it->second;
    }
    uint32_t id = node_id_map.size();
    node_id_map[node] = id;
    return id;
  };

  auto bnext_id = [&node_id, &lattice](const Node& node) -> uint32_t {
    const absl::Span<const Node* const>& begin_nodes =
        lattice.begin_nodes(node.begin_pos);
    // If the node is a POS node, returns the first begin node.
    if (node.key.empty() && node.value == "POS") {
      return node_id(begin_nodes[0]);
    }
    for (size_t i = 0; i < begin_nodes.size() - 1; ++i) {
      if (begin_nodes[i] == &node) {
        return node_id(begin_nodes[i + 1]);
      }
    }
    return 0;
  };

  auto enext_id = [&](const Node& node) -> uint32_t {
    const absl::Span<const Node* const>& end_nodes =
        lattice.end_nodes(node.end_pos);
    for (size_t i = 1; i < end_nodes.size(); ++i) {
      if (end_nodes[i] == &node) {
        return node_id(end_nodes[i - 1]);
      }
    }
    return 0;
  };

  // Returns a string representation of the given node in TSV.
  auto dump_node = [&](const Node& node) -> std::string {
    return absl::StrCat(node_id(&node), "\t", node.key, "\t", node.value, "\t",
                        node.begin_pos, "\t", node.end_pos, "\t", node.lid,
                        "\t", node.rid, "\t", node.wcost, "\t", node.cost, "\t",
                        bnext_id(node), "\t", enext_id(node), "\t",
                        node_id(node.prev), "\t", node_id(node.next), "\n");
  };

  std::string output;

  // Output header.
  output =
      absl::StrJoin({"id", "key", "value", "begin_pos", "end_pos", "lid", "rid",
                     "wcost", "cost", "bnext", "enext", "prev", "next"},
                    "\t");
  absl::StrAppend(&output, "\n");

  // Output BOS node.
  absl::StrAppend(&output, dump_node(*lattice.bos_node()));

  // Make position nodes.
  std::vector<Node> pos_nodes;
  for (size_t i = 0; i <= lattice.key().size(); ++i) {
    if (lattice.begin_nodes(i).empty()) {
      continue;
    }

    Node pos_node;
    pos_node.begin_pos = i;
    pos_node.end_pos = i;
    pos_node.value = "POS";
    pos_nodes.push_back(std::move(pos_node));
  }
  for (size_t i = 0; i < pos_nodes.size() - 1; ++i) {
    pos_nodes[i].next = &pos_nodes[i + 1];
  }

  // Output position nodes.
  for (const Node& pos_node : pos_nodes) {
    absl::StrAppend(&output, dump_node(pos_node));
  }

  // Output nodes.
  for (size_t i = 0; i <= lattice.key().size(); ++i) {
    if (lattice.begin_nodes(i).empty()) {
      continue;
    }
    for (const Node* node : lattice.begin_nodes(i)) {
      absl::StrAppend(&output, dump_node(*node));
    }
  }
  return output;
}

}  // namespace converter
}  // namespace mozc
