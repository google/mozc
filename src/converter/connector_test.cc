// Copyright 2010-2018, Google Inc.
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

#include "converter/connector.h"

#include <algorithm>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "base/mmap.h"
#include "data_manager/connection_file_reader.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/mozctest.h"

namespace mozc {
namespace {

struct ConnectionDataEntry {
  uint16 rid;
  uint16 lid;
  int cost;
};

#ifndef OS_NACL
// Disabled on NaCl since it uses a mock file system.
TEST(ConnectorTest, CompareWithRawData) {
  const string path = testing::GetSourceFileOrDie({
      "data_manager", "testing", "connection.data"});
  Mmap cmmap;
  ASSERT_TRUE(cmmap.Open(path.c_str())) << "Failed to open image: " << path;
  std::unique_ptr<Connector> connector(
      new Connector(cmmap.begin(), cmmap.size(), 256));
  ASSERT_EQ(1, connector->GetResolution());

  const string connection_text_path = testing::GetSourceFileOrDie({
      "data_manager", "testing", "connection_single_column.txt"});
  std::vector<ConnectionDataEntry> data;
  for (ConnectionFileReader reader(connection_text_path);
       !reader.done(); reader.Next()) {
    ConnectionDataEntry entry;
    entry.rid = reader.rid_of_left_node();
    entry.lid = reader.lid_of_right_node();
    entry.cost = reader.cost();
    data.push_back(entry);
  }

  for (int trial = 0; trial < 3; ++trial) {
    // Lookup in random order for a few times.
    std::random_device rd;
    std::mt19937 urbg(rd());
    std::shuffle(data.begin(), data.end(), urbg);
    for (size_t i = 0; i < data.size(); ++i) {
      int actual = connector->GetTransitionCost(data[i].rid, data[i].lid);
      EXPECT_EQ(data[i].cost, actual);

      // Cache hit case.
      actual = connector->GetTransitionCost(data[i].rid, data[i].lid);
      EXPECT_EQ(data[i].cost, actual);
    }
  }
}
#endif  // !OS_NACL

}  // namespace
}  // namespace mozc
