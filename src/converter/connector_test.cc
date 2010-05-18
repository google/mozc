// Copyright 2010, Google Inc.
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
#include "base/base.h"
#include "base/file_stream.h"
#include "base/mmap.h"
#include "base/util.h"
#include "converter/connector.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);
DECLARE_bool(use_sparse_connector);

namespace mozc {

class ConnectorTest : public testing::Test {
 protected:
  void ConnectorOpenTest() {
    const string input_filename
        = mozc::Util::JoinPath(FLAGS_test_tmpdir, "connector.txt");

    const string output_filename
        = mozc::Util::JoinPath(FLAGS_test_tmpdir, "connector.db");

    {
      mozc::OutputFileStream ofs(input_filename.c_str());
      EXPECT_TRUE(ofs);

      ofs << "3 3" << endl;  // 3x3 matrix
      for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
          // lid, rid, cost
          ofs << i << " " << j << " " << 3 * i + j << endl;
        }
      }
    }

    ConnectorInterface::Compile(input_filename.c_str(),
                                output_filename.c_str());

    Mmap<char> cmmap;
    CHECK(cmmap.Open(output_filename.c_str()))
        << "Failed to open matrix image" << output_filename;
    CHECK_GE(cmmap.Size(), 4) << "Malformed matrix image";
    scoped_ptr<ConnectorInterface> connector(
        ConnectorInterface::OpenFromArray(cmmap.begin(), cmmap.GetFileSize()));

    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        EXPECT_EQ(3 * i + j, connector->GetTransitionCost(i, j));
      }
    }
  }
};

// This code is a kind of kludge to use 2 implementations of transition
// cost matrix. We might remove this later.
TEST_F(ConnectorTest, DenseConnecterOpenTest) {
  FLAGS_use_sparse_connector = false;
  ConnectorOpenTest();
}

TEST_F(ConnectorTest, SparseConnecterOpenTest) {
  FLAGS_use_sparse_connector = true;
  ConnectorOpenTest();
}

TEST(SparseConnectorTest, key_coding) {
  int key;
  key = SparseConnector::EncodeKey(0, 0);
  EXPECT_EQ(key, 0);

  key = SparseConnector::EncodeKey(0xaabb, 0xccdd);
  EXPECT_EQ(key, 0xccddaabb);
}
}  // namespace mozc
