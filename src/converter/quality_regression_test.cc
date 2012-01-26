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

#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/converter.h"
#include "converter/converter_interface.h"
#include "converter/immutable_converter.h"
#include "converter/quality_regression_test_data.h"
#include "converter/quality_regression_util.h"
#include "dictionary/dictionary_interface.h"
#include "prediction/dictionary_predictor.h"
#include "prediction/predictor.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

using mozc::quality_regression::QualityRegressionUtil;

namespace mozc {
namespace {

class QualityRegressionTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
    DictionaryFactory::SetDictionary(NULL);
    PredictorFactory::SetDictionaryPredictor(NULL);
  }

  virtual void TearDown() {
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
    DictionaryFactory::SetDictionary(NULL);
    PredictorFactory::SetDictionaryPredictor(NULL);
  }

  void RunTestForPlatform(uint32 platform, QualityRegressionUtil *util) {
    CHECK(util);
    map<string, vector<pair<float, string> > > results;

    int testcase_count = 0;
    for (size_t i = 0; i < ARRAYSIZE(kTestData); ++i) {
      QualityRegressionUtil::TestItem item;
      CHECK(item.ParseFromTSV(kTestData[i]));
      if (!(item.platform & platform)) {
        continue;
      }
      string actual_value;
      const  bool test_result = util->ConvertAndTest(item, &actual_value);
      const string &label = item.label;
      string line = kTestData[i];
      line += "\tActual: ";
      line += actual_value;
      if (test_result) {
        // use "-1.0" as a dummy expected ratio
        results[label].push_back(make_pair(-1.0, line));
      } else {
        results[label].push_back(make_pair(item.accuracy, line));
      }
      ++testcase_count;
    }

    for (map<string, vector<pair<float, string > > >::iterator
             it = results.begin(); it != results.end(); ++it) {
      vector<pair<float, string> > &values = it->second;
      sort(values.begin(), values.end());
      size_t correct = 0;
      for (int n = 0; n < values.size(); ++n) {
        const float accuracy = values[n].first;
        if (accuracy < 0) {
          ++correct;
          continue;
        }
        // Print failed example for failed label
        const float actual_ratio = 1.0 * correct / values.size();
        EXPECT_TRUE(accuracy < actual_ratio) << values[n].second
                                             << " " << accuracy
                                             << " " << actual_ratio;
      }
      LOG(INFO) << "Accuracy: " << it->first << " "
                << 1.0 * correct / values.size();
    }
    LOG(INFO) << "Tested " << testcase_count << " entries.";
  }
};


// Test for desktop
TEST_F(QualityRegressionTest, BasicTest) {
  scoped_ptr<ImmutableConverterImpl> immutable_converter(
      new ImmutableConverterImpl);
  ImmutableConverterFactory::SetImmutableConverter(immutable_converter.get());

  scoped_ptr<DictionaryPredictor> dictionary_predictor(new DictionaryPredictor);
  PredictorFactory::SetDictionaryPredictor(dictionary_predictor.get());

  scoped_ptr<ConverterImpl> converter_impl(new ConverterImpl);
  ConverterInterface *converter = converter_impl.get();
  CHECK(converter);

  QualityRegressionUtil util(converter);
  RunTestForPlatform(QualityRegressionUtil::DESKTOP, &util);
}
}  // namespace
}  // namespace mozc
