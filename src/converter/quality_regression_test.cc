// Copyright 2010-2011, Google Inc.
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

#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "converter/quality_regression_test_data.h"
#include "converter/quality_regression_util.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

using mozc::quality_regression::QualityRegressionUtil;

namespace mozc {
namespace {

TEST(QualityRegressionTest, BasicTest) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  QualityRegressionUtil util;

  map<string, vector<pair<float, string> > > results;

  for (size_t i = 0; i < ARRAYSIZE(kTestData); ++i) {
    QualityRegressionUtil::TestItem item;
    CHECK(item.ParseFromTSV(kTestData[i]));
    string actual_value;
    const  bool test_result = util.ConvertAndTest(item, &actual_value);
    const string& label = item.label;
    string line = kTestData[i];
    line += "\tActual: ";
    line += actual_value;
    if (test_result) {
      // use "-1.0" as a dummy expected ratio
      results[label].push_back(make_pair(-1.0, line));
    } else {
      results[label].push_back(make_pair(item.accuracy, line));
    }
  }

  for (map<string, vector<pair<float, string > > >::iterator
           it = results.begin(); it != results.end(); ++it) {
    vector<pair<float, string> > &values = it->second;
    sort(values.begin(), values.end());
    size_t correct = 0;
    for (int n = values.size() - 1; n >= 0; --n) {
      const float accuracy = values[n].first;
      const float actual_ratio = 1.0 * n / values.size();
      if (accuracy < 0) {
        ++correct;
      }
      EXPECT_TRUE(accuracy < actual_ratio) << values[n].second
                                           << " " << accuracy
                                           << " " << actual_ratio;
    }
    LOG(INFO) << "Accuracy: " << it->first << " "
              << 1.0 * correct / values.size();
  }
}
}  // namespace
}  // namespace mozc
