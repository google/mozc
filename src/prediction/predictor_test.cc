// Copyright 2010-2021, Google Inc.
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

namespace mozc {
namespace {

using ::mozc::dictionary::DictionaryMock;
using ::mozc::dictionary::SuppressionDictionary;
using ::testing::_;
using ::testing::AtMost;
using ::testing::Return;

class CheckCandSizeDictionaryPredictor : public PredictorInterface {
 public:
  explicit CheckCandSizeDictionaryPredictor(int expected_cand_size)
      : expected_cand_size_(expected_cand_size),
        predictor_name_("CheckCandSizeDictionaryPredictor") {}

  bool PredictForRequest(const ConversionRequest &request,
                         Segments *segments) const override {
    EXPECT_EQ(expected_cand_size_,
              request.max_dictionary_prediction_candidates_size());
    return true;
  }

  const std::string &GetPredictorName() const override {
    return predictor_name_;
  }

 private:
  const int expected_cand_size_;
  const std::string predictor_name_;
};

class CheckCandSizeUserHistoryPredictor : public PredictorInterface {
 public:
  CheckCandSizeUserHistoryPredictor(int expected_cand_size,
                                    int expected_cand_size_for_zero_query)
      : expected_cand_size_(expected_cand_size),
        expected_cand_size_for_zero_query_(expected_cand_size_for_zero_query),
        predictor_name_("CheckCandSizeUserHistoryPredictor") {}

  bool PredictForRequest(const ConversionRequest &request,
                         Segments *segments) const override {
    EXPECT_EQ(expected_cand_size_,
              request.max_user_history_prediction_candidates_size());
    EXPECT_EQ(
        expected_cand_size_for_zero_query_,
        request.max_user_history_prediction_candidates_size_for_zero_query());
    return true;
  }

  const std::string &GetPredictorName() const override {
    return predictor_name_;
  }

 private:
  const int expected_cand_size_;
  const int expected_cand_size_for_zero_query_;
  const std::string predictor_name_;
};

class NullPredictor : public PredictorInterface {
 public:
  explicit NullPredictor(bool ret)
      : return_value_(ret),
        predict_called_(false),
        predictor_name_("NullPredictor") {}

  bool PredictForRequest(const ConversionRequest &request,
                         Segments *segments) const override {
    predict_called_ = true;
    return return_value_;
  }

  bool predict_called() const { return predict_called_; }

  void Clear() { predict_called_ = false; }

  const std::string &GetPredictorName() const override {
    return predictor_name_;
  }

 private:
  bool return_value_;
  mutable bool predict_called_;
  const std::string predictor_name_;
};

class MockPredictor : public PredictorInterface {
 public:
  MockPredictor() = default;
  ~MockPredictor() override = default;
  MOCK_METHOD(bool, PredictForRequest,
              (const ConversionRequest &request, Segments *segments),
              (const, override));
  MOCK_METHOD(const std::string &, GetPredictorName, (), (const, override));
};

}  // namespace

class MobilePredictorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_ = std::make_unique<config::Config>();
    config::ConfigHandler::GetDefaultConfig(config_.get());

    request_ = std::make_unique<commands::Request>();
    commands::RequestForUnitTest::FillMobileRequest(request_.get());
    composer_ = std::make_unique<composer::Composer>(nullptr, request_.get(),
                                                     config_.get());

    convreq_ = std::make_unique<ConversionRequest>(
        composer_.get(), request_.get(), config_.get());
  }

  std::unique_ptr<mozc::composer::Composer> composer_;
  std::unique_ptr<commands::Request> request_;
  std::unique_ptr<config::Config> config_;
  std::unique_ptr<ConversionRequest> convreq_;
};

TEST_F(MobilePredictorTest, CallPredictorsForMobileSuggestion) {
  auto predictor = std::make_unique<MobilePredictor>(
      std::make_unique<CheckCandSizeDictionaryPredictor>(20),
      std::make_unique<CheckCandSizeUserHistoryPredictor>(3, 4));
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::SUGGESTION);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(MobilePredictorTest, CallPredictorsForMobilePartialSuggestion) {
  auto predictor = std::make_unique<MobilePredictor>(
      std::make_unique<CheckCandSizeDictionaryPredictor>(20),
      // We don't call history predictior
      std::make_unique<CheckCandSizeUserHistoryPredictor>(-1, -1));
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::PARTIAL_SUGGESTION);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(MobilePredictorTest, CallPredictorsForMobilePrediction) {
  auto predictor = std::make_unique<MobilePredictor>(
      std::make_unique<CheckCandSizeDictionaryPredictor>(200),
      std::make_unique<CheckCandSizeUserHistoryPredictor>(3, 4));
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::PREDICTION);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(MobilePredictorTest, CallPredictorsForMobilePartialPrediction) {
  DictionaryMock dictionary_mock;
  testing::MockDataManager data_manager;
  const dictionary::PosMatcher pos_matcher(data_manager.GetPosMatcherData());
  const SuppressionDictionary suppression_dictionary;
  auto predictor = std::make_unique<MobilePredictor>(
      std::make_unique<CheckCandSizeDictionaryPredictor>(200),
      std::make_unique<UserHistoryPredictor>(&dictionary_mock, &pos_matcher,
                                             &suppression_dictionary, true));
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::PARTIAL_PREDICTION);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(MobilePredictorTest, CallPredictForRequestMobile) {
  auto predictor1 = std::make_unique<MockPredictor>();
  auto predictor2 = std::make_unique<MockPredictor>();
  EXPECT_CALL(*predictor1, PredictForRequest(_, _))
      .Times(AtMost(1))
      .WillOnce(Return(true));
  EXPECT_CALL(*predictor2, PredictForRequest(_, _))
      .Times(AtMost(1))
      .WillOnce(Return(true));

  auto predictor = std::make_unique<MobilePredictor>(std::move(predictor1),
                                                     std::move(predictor2));
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::SUGGESTION);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

class PredictorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_ = std::make_unique<config::Config>();
    config::ConfigHandler::GetDefaultConfig(config_.get());

    request_ = std::make_unique<commands::Request>();
    composer_ = std::make_unique<composer::Composer>(nullptr, request_.get(),
                                                     config_.get());
    convreq_ = std::make_unique<ConversionRequest>(
        composer_.get(), request_.get(), config_.get());
  }

  std::unique_ptr<mozc::composer::Composer> composer_;
  std::unique_ptr<commands::Request> request_;
  std::unique_ptr<config::Config> config_;
  std::unique_ptr<ConversionRequest> convreq_;
};

TEST_F(PredictorTest, AllPredictorsReturnTrue) {
  auto predictor =
      std::make_unique<DefaultPredictor>(std::make_unique<NullPredictor>(true),
                                         std::make_unique<NullPredictor>(true));
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::SUGGESTION);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(PredictorTest, MixedReturnValue) {
  auto predictor = std::make_unique<DefaultPredictor>(
      std::make_unique<NullPredictor>(true),
      std::make_unique<NullPredictor>(false));
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::SUGGESTION);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(PredictorTest, AllPredictorsReturnFalse) {
  auto predictor = std::make_unique<DefaultPredictor>(
      std::make_unique<NullPredictor>(false),
      std::make_unique<NullPredictor>(false));
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::SUGGESTION);
  EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(PredictorTest, CallPredictorsForSuggestion) {
  const int suggestions_size =
      config::ConfigHandler::DefaultConfig().suggestions_size();
  auto predictor = std::make_unique<DefaultPredictor>(
      std::make_unique<CheckCandSizeDictionaryPredictor>(suggestions_size),
      std::make_unique<CheckCandSizeUserHistoryPredictor>(suggestions_size,
                                                          suggestions_size));
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::SUGGESTION);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(PredictorTest, CallPredictorsForPrediction) {
  constexpr int kPredictionSize = 100;
  auto predictor = std::make_unique<DefaultPredictor>(
      std::make_unique<CheckCandSizeDictionaryPredictor>(kPredictionSize),
      std::make_unique<CheckCandSizeUserHistoryPredictor>(kPredictionSize,
                                                          kPredictionSize));
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::PREDICTION);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(PredictorTest, CallPredictForRequest) {
  auto predictor1 = std::make_unique<MockPredictor>();
  auto predictor2 = std::make_unique<MockPredictor>();
  EXPECT_CALL(*predictor1, PredictForRequest(_, _))
      .Times(AtMost(1))
      .WillOnce(Return(true));
  EXPECT_CALL(*predictor2, PredictForRequest(_, _))
      .Times(AtMost(1))
      .WillOnce(Return(true));

  auto predictor = std::make_unique<DefaultPredictor>(std::move(predictor1),
                                                      std::move(predictor2));
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::SUGGESTION);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(PredictorTest, DisableAllSuggestion) {
  auto predictor1 = std::make_unique<NullPredictor>(true);
  auto predictor2 = std::make_unique<NullPredictor>(true);
  const auto *pred1 = predictor1.get();  // Keep the reference
  const auto *pred2 = predictor2.get();  // Keep the reference
  auto predictor = std::make_unique<DefaultPredictor>(std::move(predictor1),
                                                      std::move(predictor2));
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    CHECK(segment);
  }
  convreq_->set_request_type(ConversionRequest::SUGGESTION);

  config_->set_presentation_mode(true);
  EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_FALSE(pred1->predict_called());
  EXPECT_FALSE(pred2->predict_called());

  config_->set_presentation_mode(false);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_TRUE(pred1->predict_called());
  EXPECT_TRUE(pred2->predict_called());
}

}  // namespace mozc
