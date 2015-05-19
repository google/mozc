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

#include "prediction/predictor.h"

#include <cstddef>
#include <string>

#include "base/logging.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "composer/composer.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "data_manager/user_pos_manager.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/suppression_dictionary.h"
#include "prediction/predictor_interface.h"
#include "prediction/user_history_predictor.h"
#include "session/commands.pb.h"
#include "session/request_test_util.h"
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
      expected_cand_size_(expected_cand_size),
      predictor_name_("CheckCandSizePredictor") {
  }
  virtual bool PredictForRequest(const ConversionRequest &request,
                                 Segments *segments) const {
    EXPECT_EQ(expected_cand_size_, segments->max_prediction_candidates_size());
    return true;
  }
  virtual const string &GetPredictorName() const {
    return predictor_name_;
  }
 private:
  int expected_cand_size_;
  const string predictor_name_;
};

class NullPredictor : public PredictorInterface {
 public:
  explicit NullPredictor(bool ret)
      : return_value_(ret), predict_called_(false),
        predictor_name_("NullPredictor") {}
  virtual bool PredictForRequest(const ConversionRequest &request,
                                 Segments *segments) const {
    predict_called_ = true;
    return return_value_;
  }

  bool predict_called() const {
    return predict_called_;
  }

  virtual void Clear() {
    predict_called_ = false;
  }

  virtual const string &GetPredictorName() const {
    return predictor_name_;
  }

 private:
  bool return_value_;
  mutable bool predict_called_;
  const string predictor_name_;
};

class MockPredictor : public PredictorInterface {
 public:
  MockPredictor() {}
  virtual ~MockPredictor() {}

  MOCK_CONST_METHOD1(Predict, bool(Segments *segments));
  MOCK_CONST_METHOD2(
      PredictForRequest,
      bool(const ConversionRequest &request, Segments *segments));
  MOCK_CONST_METHOD0(GetPredictorName, const string &());
};

}  // namespace

class PredictorTest : public testing::Test {
 protected:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);

    mobile_client_request_.reset(new commands::Request);
    mozc::commands::RequestForUnitTest::FillMobileRequest(
        mobile_client_request_.get());
    mobile_composer_.reset(new composer::Composer(
        NULL, mobile_client_request_.get()));

    default_request_.reset(new ConversionRequest);
    mobile_request_.reset(new ConversionRequest(mobile_composer_.get(),
                                                mobile_client_request_.get()));
  }

  virtual void TearDown() {
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

  scoped_ptr<commands::Request> mobile_client_request_;
  scoped_ptr<mozc::composer::Composer> mobile_composer_;
  scoped_ptr<ConversionRequest> default_request_;
  scoped_ptr<ConversionRequest> mobile_request_;
};

TEST_F(PredictorTest, AllPredictorsReturnTrue) {
  scoped_ptr<DefaultPredictor> predictor(
      new DefaultPredictor(new NullPredictor(true),
                           new NullPredictor(true)));
  Segments segments;
  {
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  EXPECT_TRUE(predictor->PredictForRequest(*default_request_, &segments));
}

TEST_F(PredictorTest, MixedReturnValue) {
  scoped_ptr<DefaultPredictor> predictor(
      new DefaultPredictor(new NullPredictor(true),
                           new NullPredictor(false)));
  Segments segments;
  {
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  EXPECT_TRUE(predictor->PredictForRequest(*default_request_, &segments));
}

TEST_F(PredictorTest, AllPredictorsReturnFalse) {
  scoped_ptr<DefaultPredictor> predictor(
      new DefaultPredictor(new NullPredictor(false),
                           new NullPredictor(false)));
  Segments segments;
  {
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  EXPECT_FALSE(predictor->PredictForRequest(*default_request_, &segments));
}

TEST_F(PredictorTest, CallPredictorsForSuggestion) {
  scoped_ptr<DefaultPredictor> predictor(
      new DefaultPredictor(
          new CheckCandSizePredictor(GET_CONFIG(suggestions_size)),
          new CheckCandSizePredictor(GET_CONFIG(suggestions_size))));
  Segments segments;
  {
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  EXPECT_TRUE(predictor->PredictForRequest(*default_request_, &segments));
}

TEST_F(PredictorTest, CallPredictorsForPrediction) {
  const int kPredictionSize = 100;
  scoped_ptr<DefaultPredictor> predictor(
      new DefaultPredictor(new CheckCandSizePredictor(kPredictionSize),
                           new CheckCandSizePredictor(kPredictionSize)));
  Segments segments;
  {
    segments.set_request_type(Segments::PREDICTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  EXPECT_TRUE(predictor->PredictForRequest(*default_request_, &segments));
}

TEST_F(PredictorTest, CallPredictForRequet) {
  // To be owned by DefaultPredictor
  MockPredictor *predictor1 = new MockPredictor;
  MockPredictor *predictor2 = new MockPredictor;
  scoped_ptr<DefaultPredictor> predictor(new DefaultPredictor(predictor1,
                                                              predictor2));
  Segments segments;
  {
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  EXPECT_CALL(*predictor1, PredictForRequest(_, _))
      .Times(AtMost(1)).WillOnce(Return(true));
  EXPECT_CALL(*predictor2, PredictForRequest(_, _))
      .Times(AtMost(1)).WillOnce(Return(true));
  EXPECT_TRUE(predictor->PredictForRequest(*default_request_, &segments));
}

TEST_F(PredictorTest, CallPredictorsForMobileSuggestion) {
  scoped_ptr<MobilePredictor> predictor(
      new MobilePredictor(new CheckCandSizePredictor(20),
                          new CheckCandSizePredictor(3)));
  Segments segments;
  {
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  EXPECT_TRUE(predictor->PredictForRequest(*mobile_request_, &segments));
}

TEST_F(PredictorTest, CallPredictorsForMobilePartialSuggestion) {
  scoped_ptr<MobilePredictor> predictor(
      new MobilePredictor(new CheckCandSizePredictor(20),
                          // We don't call history predictior
                          new CheckCandSizePredictor(-1)));
  Segments segments;
  {
    segments.set_request_type(Segments::PARTIAL_SUGGESTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  EXPECT_TRUE(predictor->PredictForRequest(*mobile_request_, &segments));
}

TEST_F(PredictorTest, CallPredictorsForMobilePrediction) {
  scoped_ptr<MobilePredictor> predictor(
      new MobilePredictor(new CheckCandSizePredictor(1000),
                          new CheckCandSizePredictor(3)));
  Segments segments;
  {
    segments.set_request_type(Segments::PREDICTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  EXPECT_TRUE(predictor->PredictForRequest(*mobile_request_, &segments));
}

TEST_F(PredictorTest, CallPredictorsForMobilePartialPrediction) {
  DictionaryMock dictionary_mock;
  scoped_ptr<MobilePredictor> predictor(
      new MobilePredictor(
          new CheckCandSizePredictor(1000),
          new UserHistoryPredictor(
              &dictionary_mock,
              UserPosManager::GetUserPosManager()->GetPOSMatcher(),
              Singleton<SuppressionDictionary>::get())));
  Segments segments;
  {
    segments.set_request_type(Segments::PARTIAL_PREDICTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  EXPECT_TRUE(predictor->PredictForRequest(*mobile_request_, &segments));
}

TEST_F(PredictorTest, CallPredictForRequetMobile) {
  // Will be owned by MobilePredictor
  MockPredictor *predictor1 = new MockPredictor;
  MockPredictor *predictor2 = new MockPredictor;
  scoped_ptr<MobilePredictor> predictor(
      new MobilePredictor(predictor1, predictor2));
  Segments segments;
  {
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  EXPECT_CALL(*predictor1, PredictForRequest(_, _))
      .Times(AtMost(1)).WillOnce(Return(true));
  EXPECT_CALL(*predictor2, PredictForRequest(_, _))
      .Times(AtMost(1)).WillOnce(Return(true));
  EXPECT_TRUE(predictor->PredictForRequest(*mobile_request_, &segments));
}

TEST_F(PredictorTest, DisableAllSuggestion) {
  NullPredictor *predictor1 = new NullPredictor(true);
  NullPredictor *predictor2 = new NullPredictor(true);
  scoped_ptr<DefaultPredictor> predictor(new DefaultPredictor(predictor1,
                                                              predictor2));
  Segments segments;
  {
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }

  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);

  config.set_presentation_mode(true);
  config::ConfigHandler::SetConfig(config);
  EXPECT_FALSE(predictor->PredictForRequest(*default_request_, &segments));
  EXPECT_FALSE(predictor1->predict_called());
  EXPECT_FALSE(predictor2->predict_called());

  config.set_presentation_mode(false);
  config::ConfigHandler::SetConfig(config);
  EXPECT_TRUE(predictor->PredictForRequest(*default_request_, &segments));
  EXPECT_TRUE(predictor1->predict_called());
  EXPECT_TRUE(predictor2->predict_called());
}
}  // namespace mozc
