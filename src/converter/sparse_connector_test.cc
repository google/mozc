// Copyright 2010-2014, Google Inc.
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

#include "converter/sparse_connector.h"

#include <string>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/mmap.h"
#include "base/scoped_ptr.h"
#include "data_manager/connection_file_reader.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_srcdir);

namespace mozc {

namespace {
const char kTestConnectionDataImagePath[] =
    "data_manager/testing/connection_data.data";
const char kTestConnectionFilePath[] =
    "data_manager/testing/connection_single_column.txt";
}  // namespace

TEST(SarseConnectorTest, SparseConnectorTest) {
  const string path = FileUtil::JoinPath(
      FLAGS_test_srcdir, kTestConnectionDataImagePath);
  Mmap cmmap;
  ASSERT_TRUE(cmmap.Open(path.c_str())) << "Failed to open image: " << path;
  scoped_ptr<SparseConnector> connector(
      new SparseConnector(cmmap.begin(), cmmap.size()));
  ASSERT_EQ(1, connector->GetResolution());

  const string connection_text_path =
      FileUtil::JoinPath(FLAGS_test_srcdir, kTestConnectionFilePath);
  for (ConnectionFileReader reader(connection_text_path);
       !reader.done(); reader.Next()) {
    const uint16 rid = reader.rid_of_left_node();
    const uint16 lid = reader.lid_of_right_node();
    const int cost = reader.cost();
    EXPECT_EQ(cost, connector->GetTransitionCost(rid, lid));
  }
}

}  // namespace mozc
