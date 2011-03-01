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

#include "prediction/conversion_predictor.h"

#include "base/base.h"
#include "base/util.h"
#include "base/password_manager.h"
#include "converter/segments.h"
#include "session/config.pb.h"
#include "session/config_handler.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {

void MakeSegments(const string key,
                               Segments *segments) {
  segments->Clear();
  segments->set_max_prediction_candidates_size(10);
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FREE);
}

TEST(ConversionPredictorTest, PredictTest) {
  kUseMockPasswordManager = true;
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

  config::Config config;
  config.set_use_realtime_conversion(true);
  config::ConfigHandler::SetConfig(config);

  Segments segments;
  ConversionPredictor predictor;

  EXPECT_FALSE(predictor.Predict(NULL));

  // request_type should be PREDICTION|CONVERSION
  {
    // "てすと"
    MakeSegments("\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8",
                 &segments);
    segments.set_request_type(Segments::CONVERSION);
    EXPECT_FALSE(predictor.Predict(&segments));
  }

  // empty request
  {
    MakeSegments("", &segments);
    segments.set_request_type(Segments::PREDICTION);
    EXPECT_FALSE(predictor.Predict(&segments));
  }

  // Valid request
  {
    // "てすと"
    MakeSegments("\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8", &segments);
    segments.set_request_type(Segments::PREDICTION);
    EXPECT_TRUE(predictor.Predict(&segments));
  }

  // conversion_segments_size shoud always be 1.
  {
    // "わたしのなまえはくどうです"
    MakeSegments("\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97"
                 "\xE3\x81\xAE\xE3\x81\xAA\xE3\x81\xBE"
                 "\xE3\x81\x88\xE3\x81\xAF\xE3\x81\x8F"
                 "\xE3\x81\xA9\xE3\x81\x86\xE3\x81\xA7\xE3\x81\x99",
                 &segments);
    segments.set_request_type(Segments::PREDICTION);
    EXPECT_TRUE(predictor.Predict(&segments));
    EXPECT_EQ(1, segments.conversion_segments_size());
  }

  config.set_use_realtime_conversion(false);
  config::ConfigHandler::SetConfig(config);
  {
    // "てすと"
    MakeSegments("\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8", &segments);
    segments.set_request_type(Segments::PREDICTION);
    EXPECT_FALSE(predictor.Predict(&segments));
  }
}
}  // namespace
}  // namespace mozc
