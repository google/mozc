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
#include <vector>
#include "base/file_stream.h"
#include "base/singleton.h"
#include "base/util.h"
#include "converter/connector.h"
#include "testing/base/public/gunit.h"

#ifdef MOZC_USE_SEPARATE_CONNECTION_DATA
#include "converter/connection_data_injected_environment.h"
namespace {
const ::testing::Environment *kConnectionDataInjectedEnvironment =
    ::testing::AddGlobalTestEnvironment(
        new ::mozc::ConnectionDataInjectedEnvironment());
}  // namespace
#endif  // MOZC_USE_SEPARATE_CONNECTION_DATA

DECLARE_string(test_srcdir);
DEFINE_string(connection_text_file, "data/dictionary/connection.txt",
              "Path to connection file.");

namespace mozc {

namespace {

bool ParseLine(const string &line, uint16 *rid, uint16 *lid, int *cost) {
  DCHECK(rid);
  DCHECK(lid);
  DCHECK(cost);
  vector<string> tokens;
  Util::SplitStringUsing(line, " ", &tokens);
  if (tokens.size() != 3) {
    return false;
  }

  *rid = atoi32(tokens[0].c_str());
  *lid = atoi32(tokens[1].c_str());
  *cost = atoi32(tokens[2].c_str());
  return true;
}

// Get actual file path for testing
string GetFilePath(const string &path) {
  return Util::JoinPath(FLAGS_test_srcdir, path);
}
}  // namespace

class ConnectorTest : public testing::Test {
 protected:
  ConnectorTest() : connector_(new Connector),
                    data_file_(GetFilePath(FLAGS_connection_text_file)) {}

  scoped_ptr<Connector> connector_;
  const string data_file_;
};

TEST_F(ConnectorTest, RandomValueCheck) {
  InputFileStream ifs(data_file_.c_str());
  CHECK(ifs);
  string header_line;
  EXPECT_TRUE(getline(ifs, header_line));

  string line;
  const int resolution = connector_->GetResolution();
  while (getline(ifs, line)) {
    uint16 rid = 0, lid = 0;
    int cost = -1;
    // connection data have several millions of entries
    if (Util::Random(100000) != 0) {
      continue;
    }
    EXPECT_TRUE(ParseLine(line, &rid, &lid, &cost));
    EXPECT_GE(cost, 0);
    const int actual_cost = connector_->GetTransitionCost(rid, lid);
    if (cost == ConnectorInterface::kInvalidCost) {
      EXPECT_EQ(cost, actual_cost);
    } else {
      EXPECT_TRUE(cost == actual_cost ||
                  (cost - cost % resolution) == actual_cost)
          << "cost: " << cost << ", actual_cost: " << actual_cost;
    }
  }
}


}  // namespace mozc
