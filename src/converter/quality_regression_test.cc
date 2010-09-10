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

#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "converter/segments.h"
#include "converter/converter_interface.h"
#include "converter/quality_regression_test_data.h"
#include "session/config_handler.h"
#include "session/config.pb.h"
#include "testing/base/public/gunit.h"

// Do not rase exception even if error occurs
DEFINE_bool(dryrun, false, "dryrun mode");
DECLARE_string(test_tmpdir);

namespace mozc {
namespace {

const char kConversionExpect[]    = "Conversion Expect";
const char kConversionNotExpect[] = "Conversion Not Expect";
const char kPredictionExpect[]    = "Prediction Expect";
const char kPredictionNotExpect[] = "Prediction Not Expect";

// copied from evaluation/quality_regression/evaluator.cc
int GetRank(const string &value, const Segments *segments,
              size_t current_pos, size_t current_segment) {
  if (current_segment == segments->segments_size()) {
    if (current_pos == value.size()) {
      return 0;
    } else {
      return -1;
    }
  }
  const Segment &seg = segments->segment(current_segment);
  for (size_t i = 0; i < seg.candidates_size(); ++i) {
    const string &cand_value = seg.candidate(i).value;
    const size_t len = cand_value.size();
    if (current_pos + len > value.size()) {
      continue;
    }
    if (strncmp(cand_value.c_str(),
                value.c_str() + current_pos, len) != 0) {
      continue;
    }
    const int rest = GetRank(value, segments,
                             current_pos + len, current_segment + 1);
    if (rest == -1) {
      continue;
    }
    return i + rest;
  }
  return -1;
}


bool ConvertAndTest(const ConverterInterface *converter,
                    const string &key,
                    const string &expected_value,
                    const string &command,
                    uint32 expected_rank,
                    Segments *segments,
                    string *actual_value) {
  CHECK(segments);
  CHECK(actual_value);

  segments->Clear();
  converter->ResetConversion(segments);
  actual_value->clear();

  const size_t expand_size = expected_rank + 32;

  if (command == kConversionExpect ||
      command == kConversionNotExpect) {
    converter->StartConversion(segments, key);
    if (expected_rank > 0) {
      converter->GetCandidates(segments, 0, expand_size);
    }
  } else if (command == kPredictionExpect ||
             command == kPredictionNotExpect) {
    converter->StartPrediction(segments, key);
  } else {
    LOG(FATAL) << "Unknown command: " << command;
  }

  // No results is OK if "prediction not expect" command
  if (command == kPredictionNotExpect &&
      (segments->segments_size() == 0 ||
       (segments->segments_size() >= 1 &&
        segments->segment(0).candidates_size() == 0))) {
    return true;
  }

  for (size_t i = 0; i < segments->segments_size(); ++i) {
    *actual_value += segments->segment(i).candidate(0).value;
  }

  const int32 actual_rank = GetRank(expected_value, segments, 0, 0);

  bool result = (actual_rank >= 0 && actual_rank <= expected_rank);

  if (command == kConversionNotExpect ||
      command == kPredictionNotExpect) {
    result = !result;
  }

  return result;
}

TEST(QualityRegressionTest, BasicTest) {
  ConverterInterface *converter = ConverterFactory::GetConverter();
  CHECK(converter);

  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  config::ConfigHandler::SetConfig(config);

  Segments segments;
  map<string, vector<pair<float, string> > > results;

  for (size_t i = 0; i < arraysize(kTestData); ++i) {
    string line = kTestData[i];
    vector<string> tokens;
    Util::SplitStringUsing(line, "\t", &tokens);
    CHECK_GE(tokens.size(), 6);
    const string &group          = tokens[0];
    const string &key            = tokens[1];
    const string &expected_value = tokens[2];
    const string &command        = tokens[3];
    const uint32  expected_rank  = atoi(tokens[4].c_str());
    const float   expected_ratio = atof(tokens[5].c_str());
    CHECK_GT(expected_ratio, 0.0);
    CHECK_LE(expected_ratio, 1.0);

    string actual_value;
    const  bool test_result = ConvertAndTest(converter, key,
                                             expected_value,
                                             command, expected_rank,
                                             &segments, &actual_value);
    line += "\tActual: ";
    line += actual_value;
    if (test_result) {
      // use "-1.0" as a dummy expected ratio
      results[group].push_back(make_pair(-1.0, line));
    } else {
      results[group].push_back(make_pair(expected_ratio, line));
    }
  }

  for (map<string, vector<pair<float, string > > >::iterator
           it = results.begin(); it != results.end(); ++it) {
    vector<pair<float, string> > &values = it->second;
    sort(values.begin(), values.end());
    size_t correct = 0;
    for (int n = values.size() - 1; n >= 0; --n) {
      const float expected_ratio = values[n].first;
      const float actual_ratio = 1.0 * n / values.size();
      if (expected_ratio < 0) {
        ++correct;
      }
      LOG_IF(INFO, expected_ratio >= 0.0) << "Error: " << values[n].second;
      EXPECT_TRUE(expected_ratio < actual_ratio) << values[n].second;
    }
    LOG(INFO) << "Accuracy: " << it->first << " "
              << 1.0 * correct / values.size();
  }
}
}  // namespace
}  // namespace mozc
