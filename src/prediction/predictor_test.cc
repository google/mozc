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

#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "prediction/predictor_interface.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

using ::testing::AtMost;
using ::testing::Return;
using ::testing::_;

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {
class CheckCandSizePredictor : public PredictorInterface {
 public:
  explicit CheckCandSizePredictor(int expected_cand_size) :
      expected_cand_size_(expected_cand_size) {
  }
  bool Predict(Segments *segments) const {
    EXPECT_EQ(expected_cand_size_, segments->max_prediction_candidates_size());
    return true;
  }
 private:
  int expected_cand_size_;
};

class NullPredictor : public PredictorInterface {
 public:
  explicit NullPredictor(bool ret)
      : return_value_(ret), predict_called_(false) {}
  bool Predict(Segments *segments) const {
    predict_called_ = true;
    return return_value_;
  }

  bool predict_called() const {
    return predict_called_;
  }

 private:
  bool return_value_;
  mutable bool predict_called_;
};

class MockPredictor : public PredictorInterface {
 public:
  MockPredictor() {}
  virtual ~MockPredictor() {}

  MOCK_CONST_METHOD1(Predict, bool (Segments *segments));
  MOCK_CONST_METHOD2(
      PredictForRequest,
      bool (const ConversionRequest &request, Segments *segments));
};

}  // namespace

class PredictorTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

  virtual void TearDown() {
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
    PredictorFactory::SetUserHistoryPredictor(NULL);
    PredictorFactory::SetDictionaryPredictor(NULL);
  }
};

TEST_F(PredictorTest, AllPredictorsReturnTrue) {
  NullPredictor predictor1(true);
  NullPredictor predictor2(true);
  PredictorFactory::SetUserHistoryPredictor(&predictor1);
  PredictorFactory::SetDictionaryPredictor(&predictor2);
  Segments segments;
  {
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  PredictorInterface *predictor = PredictorFactory::GetPredictor();
  EXPECT_TRUE(predictor->Predict(&segments));
}

TEST_F(PredictorTest, MixedReturnValue) {
  NullPredictor predictor1(true);
  NullPredictor predictor2(false);
  PredictorFactory::SetUserHistoryPredictor(&predictor1);
  PredictorFactory::SetDictionaryPredictor(&predictor2);
  Segments segments;
  {
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  PredictorInterface *predictor = PredictorFactory::GetPredictor();
  EXPECT_TRUE(predictor->Predict(&segments));
}

TEST_F(PredictorTest, AllPredictorsReturnFalse) {
  NullPredictor predictor1(false);
  NullPredictor predictor2(false);
  PredictorFactory::SetUserHistoryPredictor(&predictor1);
  PredictorFactory::SetDictionaryPredictor(&predictor2);
  Segments segments;
  {
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  PredictorInterface *predictor = PredictorFactory::GetPredictor();
  EXPECT_FALSE(predictor->Predict(&segments));
}

TEST_F(PredictorTest, CallPredictorsForSuggestion) {
  CheckCandSizePredictor predictor1(GET_CONFIG(suggestions_size));
  CheckCandSizePredictor predictor2(GET_CONFIG(suggestions_size));
  PredictorFactory::SetUserHistoryPredictor(&predictor1);
  PredictorFactory::SetDictionaryPredictor(&predictor2);
  Segments segments;
  {
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  PredictorInterface *predictor = PredictorFactory::GetPredictor();
  EXPECT_TRUE(predictor->Predict(&segments));
}

TEST_F(PredictorTest, CallPredictorsForPrediction) {
  const int kPredictionSize = 100;
  CheckCandSizePredictor predictor1(kPredictionSize);
  CheckCandSizePredictor predictor2(kPredictionSize);
  PredictorFactory::SetUserHistoryPredictor(&predictor1);
  PredictorFactory::SetDictionaryPredictor(&predictor2);
  Segments segments;
  {
    segments.set_request_type(Segments::PREDICTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  PredictorInterface *predictor = PredictorFactory::GetPredictor();
  EXPECT_TRUE(predictor->Predict(&segments));
}

TEST_F(PredictorTest, CallPredictForRequet) {
  MockPredictor predictor1;
  MockPredictor predictor2;
  PredictorFactory::SetUserHistoryPredictor(&predictor1);
  PredictorFactory::SetDictionaryPredictor(&predictor2);
  Segments segments;
  {
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  PredictorInterface *predictor = PredictorFactory::GetPredictor();
  EXPECT_CALL(predictor1, PredictForRequest(_, _))
      .Times(AtMost(1)).WillOnce(Return(true));
  EXPECT_CALL(predictor2, PredictForRequest(_, _))
      .Times(AtMost(1)).WillOnce(Return(true));
  EXPECT_TRUE(predictor->Predict(&segments));
}






TEST_F(PredictorTest, DisableAllSuggestion) {
  NullPredictor predictor1(true);
  NullPredictor predictor2(true);
  PredictorFactory::SetUserHistoryPredictor(&predictor1);
  PredictorFactory::SetDictionaryPredictor(&predictor2);
  Segments segments;
  {
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  PredictorInterface *predictor = PredictorFactory::GetPredictor();

  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);

  config.set_presentation_mode(true);
  config::ConfigHandler::SetConfig(config);
  EXPECT_FALSE(predictor->Predict(&segments));
  EXPECT_FALSE(predictor1.predict_called());
  EXPECT_FALSE(predictor2.predict_called());

  config.set_presentation_mode(false);
  config::ConfigHandler::SetConfig(config);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_TRUE(predictor1.predict_called());
  EXPECT_TRUE(predictor2.predict_called());

}
}  // namespace mozc
