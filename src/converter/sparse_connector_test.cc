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

#include <string>
#include "base/file_stream.h"
#include "base/mmap.h"
#include "base/util.h"
#include "converter/connector_interface.h"
#include "converter/sparse_connector.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {
int GetFakeCost(int l, int r) {
  return (3 * l + r) * 1000;
}
}  // namespace

TEST(SparseConnectorTest, SparseConnecterOpenTest) {
  const string input_filename
      = mozc::Util::JoinPath(FLAGS_test_tmpdir, "connector.txt");
  const string id_filename
      = mozc::Util::JoinPath(FLAGS_test_tmpdir, "id.def");
  const string special_pos_filename
      = mozc::Util::JoinPath(FLAGS_test_tmpdir, "special_pos.def");
  const string output_filename
      = mozc::Util::JoinPath(FLAGS_test_tmpdir, "connector.db");

  {
    mozc::OutputFileStream ofs(input_filename.c_str());
    EXPECT_TRUE(ofs);

    ofs << "3 3" << endl;  // 3x3 matrix
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        // lid, rid, cost
        ofs << i << " " << j << " " << GetFakeCost(i, j) << endl;
      }
    }
  }

  {
    mozc::OutputFileStream ofs(id_filename.c_str());
    EXPECT_TRUE(ofs);
    ofs << "0 foo" << endl;
    ofs << "1 bar" << endl;
    ofs << "2 buzz" << endl;
  }

  {
    mozc::OutputFileStream ofs(special_pos_filename.c_str());
    EXPECT_TRUE(ofs);
    ofs << "extra1" << endl;
    ofs << "extra2" << endl;
  }

  SparseConnectorBuilder::Compile(input_filename,
                                  id_filename,
                                  special_pos_filename,
                                  output_filename);

  Mmap<char> cmmap;
  CHECK(cmmap.Open(output_filename.c_str()))
      << "Failed to open matrix image" << output_filename;
  CHECK_GE(cmmap.Size(), 4) << "Malformed matrix image";
  scoped_ptr<SparseConnector> connector(
      new SparseConnector(cmmap.begin(), cmmap.GetFileSize()));

  const int cost_resolution = connector->GetResolution();
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      const int diff = GetFakeCost(i, j) - connector->GetTransitionCost(i, j);
      EXPECT_LT(abs(diff), cost_resolution);
    }
  }

  const int16 invalid_cost = ConnectorInterface::kInvalidCost;
  EXPECT_EQ(invalid_cost, connector->GetTransitionCost(1, 3));
  EXPECT_EQ(invalid_cost, connector->GetTransitionCost(3, 4));
  EXPECT_EQ(0, connector->GetTransitionCost(0, 3));
  EXPECT_EQ(0, connector->GetTransitionCost(0, 4));

  EXPECT_EQ(invalid_cost, connector->GetTransitionCost(3, 1));
  EXPECT_EQ(invalid_cost, connector->GetTransitionCost(4, 3));
  EXPECT_EQ(0, connector->GetTransitionCost(0, 3));
  EXPECT_EQ(0, connector->GetTransitionCost(4, 0));
}

TEST(SparseConnectorTest, key_coding) {
  int key;
  key = SparseConnector::EncodeKey(0, 0);
  EXPECT_EQ(key, 0);

  key = SparseConnector::EncodeKey(0xaabb, 0xccdd);
  EXPECT_EQ(key, 0xccddaabb);
}
}  // namespace mozc
