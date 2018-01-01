// Copyright 2010-2018, Google Inc.
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
#include <memory>
#include <string>

#include "base/logging.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "composer/composer.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "prediction/predictor_interface.h"
#include "prediction/user_history_predictor.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "session/request_test_util.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

using std::unique_ptr;

using mozc::dictionary::DictionaryMock;
using mozc::dictionary::SuppressionDictionary;
using testing::AtMost;
using testing::Return;
using testing::_;

namespace mozc {
namespace {

class CheckCandSizePredictor : public PredictorInterface {
 public:
  explicit CheckCandSizePredictor(int expected_cand_size)
      : expected_cand_size_(expected_cand_size),
        predictor_name_("CheckCandSizePredictor") {}

  bool PredictForRequest(const ConversionRequest &request,
                         Segments *segments) const override {
    EXPECT_EQ(expected_cand_size_, segments->max_prediction_candidates_size());
    return true;
  }

  const string &GetPredictorName() const override {
    return predictor_name_;
  }

 private:
  const int expected_cand_size_;
  const string predictor_name_;
};

class NullPredictor : public PredictorInterface {
 public:
  explicit NullPredictor(bool ret)
      : return_value_(ret), predict_called_(false),
        predictor_name_("NullPredictor") {}

  bool PredictForRequest(const ConversionRequest &request,
                         Segments *segments) const override {
    predict_called_ = true;
    return return_value_;
  }

  bool predict_called() const {
    return predict_called_;
  }

  void Clear() {
    predict_called_ = false;
  }

  const string &GetPredictorName() const override {
    return predictor_name_;
  }

 private:
  bool return_value_;
  mutable bool predict_called_;
  const string predictor_name_;
};

class MockPredictor : public PredictorInterface {
 public:
  MockPredictor() = default;
  ~MockPredictor() override = default;
  MOCK_CONST_METHOD2(
      PredictForRequest,
      bool(const ConversionRequest &request, Segments *segments));
  MOCK_CONST_METHOD0(GetPredictorName, const string &());
};

}  // namespace

class MobilePredictorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.reset(new config::Config);
    config::ConfigHandler::GetDefaultConfig(config_.get());

    request_.reset(new commands::Request);
    commands::RequestForUnitTest::FillMobileRequest(request_.get());
    composer_.reset(new composer::Composer(
        nullptr, request_.get(), config_.get()));

    convreq_.reset(
        new ConversionRequest(composer_.get(), request_.get(), config_.get()));
  }

  unique_ptr<mozc::composer::Composer> composer_;
  unique_ptr<commands::Request> request_;
  unique_ptr<config::Config> config_;
  unique_ptr<ConversionRequest> convreq_;
};

TEST_F(MobilePredictorTest, CallPredictorsForMobileSuggestion) {
  unique_ptr<MobilePredictor> predictor(
      new MobilePredictor(new CheckCandSizePredictor(20),
                          new CheckCandSizePredictor(3)));
  Segments segments;
  {
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(MobilePredictorTest, CallPredictorsForMobilePartialSuggestion) {
  unique_ptr<MobilePredictor> predictor(
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
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(MobilePredictorTest, CallPredictorsForMobilePrediction) {
  unique_ptr<MobilePredictor> predictor(
      new MobilePredictor(new CheckCandSizePredictor(200),
                          new CheckCandSizePredictor(3)));
  Segments segments;
  {
    segments.set_request_type(Segments::PREDICTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(MobilePredictorTest, CallPredictorsForMobilePartialPrediction) {
  DictionaryMock dictionary_mock;
  testing::MockDataManager data_manager;
  const dictionary::POSMatcher pos_matcher(data_manager.GetPOSMatcherData());
  unique_ptr<MobilePredictor> predictor(
      new MobilePredictor(
          new CheckCandSizePredictor(200),
          new UserHistoryPredictor(
              &dictionary_mock,
              &pos_matcher,
              Singleton<SuppressionDictionary>::get(),
              true)));
  Segments segments;
  {
    segments.set_request_type(Segments::PARTIAL_PREDICTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(MobilePredictorTest, CallPredictForRequetMobile) {
  // Will be owned by MobilePredictor
  MockPredictor *predictor1 = new MockPredictor;
  MockPredictor *predictor2 = new MockPredictor;
  unique_ptr<MobilePredictor> predictor(
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
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}


class PredictorTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    config_.reset(new config::Config);
    config::ConfigHandler::GetDefaultConfig(config_.get());

    request_.reset(new commands::Request);
    composer_.reset(new composer::Composer(
        nullptr, request_.get(), config_.get()));

    convreq_.reset(
        new ConversionRequest(composer_.get(), request_.get(), config_.get()));
  }

  unique_ptr<mozc::composer::Composer> composer_;
  unique_ptr<commands::Request> request_;
  unique_ptr<config::Config> config_;
  unique_ptr<ConversionRequest> convreq_;
};

TEST_F(PredictorTest, AllPredictorsReturnTrue) {
  unique_ptr<DefaultPredictor> predictor(
      new DefaultPredictor(new NullPredictor(true),
                           new NullPredictor(true)));
  Segments segments;
  {
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(PredictorTest, MixedReturnValue) {
  unique_ptr<DefaultPredictor> predictor(
      new DefaultPredictor(new NullPredictor(true),
                           new NullPredictor(false)));
  Segments segments;
  {
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(PredictorTest, AllPredictorsReturnFalse) {
  unique_ptr<DefaultPredictor> predictor(
      new DefaultPredictor(new NullPredictor(false),
                           new NullPredictor(false)));
  Segments segments;
  {
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(PredictorTest, CallPredictorsForSuggestion) {
  const int suggestions_size =
      config::ConfigHandler::DefaultConfig().suggestions_size();
  unique_ptr<DefaultPredictor> predictor(
      new DefaultPredictor(
          new CheckCandSizePredictor(suggestions_size),
          new CheckCandSizePredictor(suggestions_size)));
  Segments segments;
  {
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(PredictorTest, CallPredictorsForPrediction) {
  const int kPredictionSize = 100;
  unique_ptr<DefaultPredictor> predictor(
      new DefaultPredictor(new CheckCandSizePredictor(kPredictionSize),
                           new CheckCandSizePredictor(kPredictionSize)));
  Segments segments;
  {
    segments.set_request_type(Segments::PREDICTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(PredictorTest, CallPredictForRequet) {
  // To be owned by DefaultPredictor
  MockPredictor *predictor1 = new MockPredictor;
  MockPredictor *predictor2 = new MockPredictor;
  unique_ptr<DefaultPredictor> predictor(new DefaultPredictor(predictor1,
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
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}


TEST_F(PredictorTest, DisableAllSuggestion) {
  NullPredictor *predictor1 = new NullPredictor(true);
  NullPredictor *predictor2 = new NullPredictor(true);
  unique_ptr<DefaultPredictor> predictor(new DefaultPredictor(predictor1,
                                                              predictor2));
  Segments segments;
  {
    segments.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments.add_segment();
    CHECK(segment);
  }

  config_->set_presentation_mode(true);
  EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_FALSE(predictor1->predict_called());
  EXPECT_FALSE(predictor2->predict_called());

  config_->set_presentation_mode(false);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_TRUE(predictor1->predict_called());
  EXPECT_TRUE(predictor2->predict_called());
}

}  // namespace mozc
