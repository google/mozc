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

// immutable_converter_main.cc
//
// A tool to convert a query using ImmutableConverter to dump the lattice in TSV
// format.
//
// Usage:
// immutable_converter_main --dictionary oss --query へんかん
//   --output /tmp/lattice.tsv
//
// Output:
/*
id	key	value	begin_pos	end_pos	lid	rid	wcost	cost	prev	next
1		BOS	0	0	0	0	0	0	0	2
3	へ	へ	0	3	1841	1841	32767	34068	1	0
4	へんかん	返還	0	12	1841	1841	5030	6331	1	0
5	へんかん	偏官	0	12	1841	1949	8024	9325	1	0
2	へんかん	変換	0	12	1845	1845	300	5508	1	6
7	へんか	返歌	0	9	1841	1841	6794	8095	1	0
8	へんか	変化	0	9	1841	1841	3308	4609	1	0
9	へん	へん	0	6	42	42	2001	7965	1	0
...
*/

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "base/file_stream.h"
#include "base/init_mozc.h"
#include "converter/immutable_converter.h"
#include "converter/lattice.h"
#include "converter/node.h"
#include "converter/segments.h"
#include "data_manager/data_manager.h"
#include "data_manager/oss/oss_data_manager.h"
#include "data_manager/testing/mock_data_manager.h"
#include "engine/modules.h"
#include "request/conversion_request.h"

ABSL_FLAG(std::string, query, "", "Query input to be converted");
ABSL_FLAG(std::string, dictionary, "", "Dictionary: 'oss' or 'test'");
ABSL_FLAG(std::string, output, "", "Output file");

namespace mozc {
namespace {

std::string DumpNodes(const Lattice &lattice) {
  // Map from Node pointer to a unique ID.
  absl::flat_hash_map<const Node *, uint32_t> node_id_map;
  node_id_map[nullptr] = 0;

  // Returns a unique ID for the given node.
  auto node_id = [&node_id_map](const Node *node) -> uint32_t {
    auto it = node_id_map.find(node);
    if (it != node_id_map.end()) {
      return it->second;
    }
    uint32_t id = node_id_map.size();
    node_id_map[node] = id;
    return id;
  };

  // Returns a string representation of the given node in TSV.
  auto dump_node = [&node_id](const Node &node) -> std::string {
    return absl::StrCat(node_id(&node), "\t", node.key, "\t", node.value, "\t",
                        node.begin_pos, "\t", node.end_pos, "\t", node.lid,
                        "\t", node.rid, "\t", node.wcost, "\t", node.cost, "\t",
                        node_id(node.bnext), "\t", node_id(node.enext), "\t",
                        node_id(node.prev), "\t", node_id(node.next), "\n");
  };

  std::string output;

  // Output header.
  output =
      absl::StrJoin({"id", "key", "value", "begin_pos", "end_pos", "lid", "rid",
                     "wcost", "cost", "bnext", "enext", "prev", "next"},
                    "\t");
  absl::StrAppend(&output, "\n");

  // Output BOS node(s).
  for (Node *node = lattice.bos_nodes(); node != nullptr; node = node->bnext) {
    absl::StrAppend(&output, dump_node(*node));
  }

  // Make position nodes.
  std::vector<Node> pos_nodes;
  for (size_t i = 0; i <= lattice.key().size(); ++i) {
    Node *begin_node = lattice.begin_nodes(i);
    if (begin_node == nullptr) {
      continue;
    }

    Node pos_node;
    pos_node.begin_pos = i;
    pos_node.end_pos = i;
    pos_node.value = "POS";
    pos_node.bnext = begin_node;
    pos_nodes.push_back(std::move(pos_node));
  }
  for (size_t i = 0; i < pos_nodes.size() - 1; ++i) {
    pos_nodes[i].next = &pos_nodes[i + 1];
  }

  // Output position nodes.
  for (const Node &pos_node : pos_nodes) {
    absl::StrAppend(&output, dump_node(pos_node));
  }

  // Output nodes.
  for (size_t i = 0; i <= lattice.key().size(); ++i) {
    Node *begin_node = lattice.begin_nodes(i);
    if (begin_node == nullptr) {
      continue;
    }
    {
      Node pos_node;
      pos_node.begin_pos = i;
      pos_node.end_pos = i;
      pos_node.value = "POS";
      pos_node.bnext = begin_node;
      pos_nodes.push_back(std::move(pos_node));
    }

    for (const Node *node = begin_node; node != nullptr; node = node->bnext) {
      absl::StrAppend(&output, dump_node(*node));
    }
  }
  return output;
}

bool ExecCommand(const ImmutableConverter &immutable_converter,
                 absl::string_view query, const std::string &output) {
  ConversionRequest::Options options = {
      .request_type = ConversionRequest::CONVERSION,
      .use_actual_converter_for_realtime_conversion = true,
      .create_partial_candidates = false,
  };
  const ConversionRequest conversion_request =
      ConversionRequestBuilder()
          .SetOptions(std::move(options))
          .SetKey(query)
          .Build();

  Segments segments;
  segments.InitForConvert(conversion_request.key());
  if (!immutable_converter.Convert(conversion_request, &segments)) {
    return false;
  }

  mozc::OutputFileStream os(output);
  os << DumpNodes(*segments.mutable_cached_lattice());
  return true;
}

std::unique_ptr<const DataManager> CreateDataManager(
    const std::string &dictionary) {
  if (dictionary == "oss") {
    return std::make_unique<const oss::OssDataManager>();
  }
  if (dictionary == "mock") {
    return std::make_unique<const testing::MockDataManager>();
  }
  if (!dictionary.empty()) {
    std::cout << "ERROR: Unknown dictionary name: " << dictionary << std::endl;
  }

  return std::make_unique<const oss::OssDataManager>();
}

}  // namespace
}  // namespace mozc

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  std::unique_ptr<const mozc::DataManager> data_manager =
      mozc::CreateDataManager(absl::GetFlag(FLAGS_dictionary));

  std::unique_ptr<mozc::engine::Modules> modules =
      mozc::engine::Modules::Create(std::move(data_manager)).value();
  mozc::ImmutableConverter immutable_converter(*modules);

  std::string query = absl::GetFlag(FLAGS_query);
  std::string output = absl::GetFlag(FLAGS_output);
  if (!mozc::ExecCommand(immutable_converter, query, output)) {
    std::cerr << "ExecCommand() return false" << std::endl;
  }
  return 0;
}
