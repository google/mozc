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

#include "converter/connector.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/random/random.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/bits.h"
#include "base/mmap.h"
#include "base/vlog.h"
#include "data_manager/connection_file_reader.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

struct ConnectionDataEntry {
  uint16_t rid;
  uint16_t lid;
  int cost;
};

TEST(ConnectorTest, CompareWithRawData) {
  const std::string path = testing::GetSourceFileOrDie(
      {"data_manager", "testing", "connection.data"});
  absl::StatusOr<Mmap> cmmap = Mmap::Map(path);
  ASSERT_OK(cmmap) << cmmap.status();
  auto status_or_connector = Connector::Create(cmmap->string_view());
  ASSERT_OK(status_or_connector);
  auto connector = std::move(status_or_connector).value();
  ASSERT_EQ(1, connector.GetResolution());

  const std::string connection_text_path = testing::GetSourceFileOrDie(
      {"data", "test", "dictionary", "connection_single_column.txt"});
  std::vector<ConnectionDataEntry> data;
  for (ConnectionFileReader reader(connection_text_path); !reader.done();
       reader.Next()) {
    ConnectionDataEntry entry;
    entry.rid = reader.rid_of_left_node();
    entry.lid = reader.lid_of_right_node();
    entry.cost = reader.cost();
    data.push_back(entry);
  }

  absl::BitGen urbg;
  for (int trial = 0; trial < 3; ++trial) {
    // Lookup in random order for a few times.
    std::shuffle(data.begin(), data.end(), urbg);
    for (size_t i = 0; i < data.size(); ++i) {
      int actual = connector.GetTransitionCost(data[i].rid, data[i].lid);
      EXPECT_EQ(actual, data[i].cost);

      // Cache hit case.
      actual = connector.GetTransitionCost(data[i].rid, data[i].lid);
      EXPECT_EQ(actual, data[i].cost);
    }
  }
}

TEST(ConnectorTest, BrokenData) {
  const std::string path = testing::GetSourceFileOrDie(
      {"data_manager", "testing", "connection.data"});
  absl::StatusOr<Mmap> cmmap = Mmap::Map(path);
  ASSERT_OK(cmmap) << cmmap.status();
  std::string data;

  // Invalid magic number.
  {
    data.assign(cmmap->begin(), cmmap->size());
    StoreUnaligned<uint16_t>(0, &data[0]);
    const auto status = Connector::Create(data).status();
    MOZC_VLOG(1) << status;
    EXPECT_FALSE(status.ok());
  }
  // Not square.
  {
    data.assign(cmmap->begin(), cmmap->size());
    uint16_t* array = reinterpret_cast<uint16_t*>(&data[0]);
    array[2] = 100;
    array[3] = 200;
    const auto status = Connector::Create(data).status();
    MOZC_VLOG(1) << status;
    EXPECT_FALSE(status.ok());
  }
  // Incomplete data.
  {
    data.assign(cmmap->begin(), cmmap->size());
    for (size_t divider : {2, 3, 5, 7, 10, 100, 1000}) {
      const auto size = data.size() / divider;
      const auto status =
          Connector::Create(absl::string_view(data.data(), size)).status();
      MOZC_VLOG(1) << "Divider=" << divider << ": " << status;
      EXPECT_FALSE(status.ok());
    }
  }
  // Not aligned at 32-bit boundary.
  {
    data.resize(cmmap->size() + 2);
    data.insert(2, cmmap->begin(), cmmap->size());  // Align at 16-bit boundary.
    const auto status =
        Connector::Create(absl::string_view(data.data() + 2, cmmap->size()))
            .status();
    MOZC_VLOG(1) << status;
    EXPECT_FALSE(status.ok());
  }
}

TEST(ConnectorTest, GetTransitionCost_OneByteCost) {
  // A 2x2 connection matrix with 1-byte costs (resolution 64) in the format
  // described in connector.cc:
  //   rid 0: lid 0 is the mode value 100, lid 1 is the invalid cost.
  //   rid 1: lid 0 is 10 * 64 = 640, lid 1 is the mode value 200.
  alignas(uint32_t) static constexpr uint8_t kData[] = {
      0xAB, 0xCD,              // magic
      0x40, 0x00,              // resolution = 64
      0x02, 0x00, 0x02, 0x00,  // rsize = 2, lsize = 2
      0x64, 0x00, 0xC8, 0x00,  // mode values: 100, 200
      // rid 0
      0x04, 0x00, 0x04, 0x00,  // compact_bits_size = 4, values_size = 4
      0x01, 0x00, 0x00, 0x00,  // chunk bits: chunk 0 has a value
      0x02, 0x00, 0x00, 0x00,  // compact bits: lid 1 has a value
      0xFF, 0x00, 0x00, 0x00,  // values: 255 = invalid cost
      // rid 1
      0x04, 0x00, 0x04, 0x00,  // compact_bits_size = 4, values_size = 4
      0x01, 0x00, 0x00, 0x00,  // chunk bits: chunk 0 has a value
      0x01, 0x00, 0x00, 0x00,  // compact bits: lid 0 has a value
      0x0A, 0x00, 0x00, 0x00,  // values: 10, i.e. 640 with resolution 64
  };
  absl::StatusOr<Connector> connector = Connector::Create(
      absl::string_view(reinterpret_cast<const char*>(kData), sizeof(kData)));
  ASSERT_OK(connector);
  EXPECT_EQ(connector->GetResolution(), 64);
  EXPECT_EQ(connector->GetTransitionCost(0, 1), Connector::kInvalidCost);
  EXPECT_EQ(connector->GetTransitionCost(1, 0), 640);
  EXPECT_EQ(connector->GetTransitionCost(1, 1), 200);
}

TEST(ConnectorTest, GetTransitionCost_TwoByteCost) {
  // The same 2x2 connection matrix as above with 2-byte costs (resolution 1):
  //   rid 0: lid 0 is the mode value 100, lid 1 is the invalid cost.
  //   rid 1: lid 0 is 640, lid 1 is the mode value 200.
  alignas(uint32_t) static constexpr uint8_t kData[] = {
      0xAB, 0xCD,              // magic
      0x01, 0x00,              // resolution = 1
      0x02, 0x00, 0x02, 0x00,  // rsize = 2, lsize = 2
      0x64, 0x00, 0xC8, 0x00,  // mode values: 100, 200
      // rid 0
      0x04, 0x00, 0x04, 0x00,  // compact_bits_size = 4, values_size = 4
      0x01, 0x00, 0x00, 0x00,  // chunk bits: chunk 0 has a value
      0x02, 0x00, 0x00, 0x00,  // compact bits: lid 1 has a value
      0x30, 0x75, 0x00, 0x00,  // values: 30000 = invalid cost
      // rid 1
      0x04, 0x00, 0x04, 0x00,  // compact_bits_size = 4, values_size = 4
      0x01, 0x00, 0x00, 0x00,  // chunk bits: chunk 0 has a value
      0x01, 0x00, 0x00, 0x00,  // compact bits: lid 0 has a value
      0x80, 0x02, 0x00, 0x00,  // values: 640
  };
  absl::StatusOr<Connector> connector = Connector::Create(
      absl::string_view(reinterpret_cast<const char*>(kData), sizeof(kData)));
  ASSERT_OK(connector);
  EXPECT_EQ(connector->GetResolution(), 1);
  EXPECT_EQ(connector->GetTransitionCost(0, 1), Connector::kInvalidCost);
  EXPECT_EQ(connector->GetTransitionCost(1, 0), 640);
  EXPECT_EQ(connector->GetTransitionCost(1, 1), 200);
}

}  // namespace
}  // namespace mozc
