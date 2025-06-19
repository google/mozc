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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "composer/composer.h"
#include "config/config_handler.h"
#include "converter/converter_mock.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_mock.h"
#include "engine/modules.h"
#include "prediction/predictor_interface.h"
#include "prediction/result.h"
#include "prediction/user_history_predictor.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "request/request_test_util.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc::prediction {
namespace {

using ::mozc::dictionary::MockDictionary;
using ::testing::_;
using ::testing::AtMost;
using ::testing::Return;
using ::testing::Unused;

class CheckCandSizeDictionaryPredictor : public PredictorInterface {
 public:
  explicit CheckCandSizeDictionaryPredictor(int expected_cand_size)
      : expected_cand_size_(expected_cand_size),
        predictor_name_("CheckCandSizeDictionaryPredictor") {}

  std::vector<Result> Predict(const ConversionRequest &request) const override {
    EXPECT_EQ(request.max_dictionary_prediction_candidates_size(),
              expected_cand_size_);
    return std::vector<Result>(1);
  }

  absl::string_view GetPredictorName() const override {
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

  std::vector<Result> Predict(const ConversionRequest &request) const override {
    EXPECT_EQ(request.max_user_history_prediction_candidates_size(),
              expected_cand_size_);
    EXPECT_EQ(
        request.max_user_history_prediction_candidates_size_for_zero_query(),
        expected_cand_size_for_zero_query_);
    return std::vector<Result>(1);
  }

  absl::string_view GetPredictorName() const override {
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

  std::vector<Result> Predict(const ConversionRequest &request) const override {
    predict_called_ = true;
    std::vector<Result> results(return_value_ ? 1 : 0);
    return results;
  }

  bool predict_called() const { return predict_called_; }

  void Clear() { predict_called_ = false; }

  absl::string_view GetPredictorName() const override {
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
  MOCK_METHOD(std::vector<Result>, Predict, (const ConversionRequest &request),
              (const, override));
  MOCK_METHOD(absl::string_view, GetPredictorName, (), (const, override));
};

}  // namespace

class MixedDecodingPredictorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_ = std::make_unique<config::Config>();
    config::ConfigHandler::GetDefaultConfig(config_.get());

    request_ = std::make_unique<commands::Request>();
    request_test_util::FillMobileRequest(request_.get());
    composer_ = std::make_unique<composer::Composer>(*request_, *config_);
  }

  ConversionRequest CreateConversionRequest(
      ConversionRequest::RequestType request_type) const {
    ConversionRequest::Options options = {.request_type = request_type};
    return ConversionRequestBuilder()
        .SetComposer(*composer_)
        .SetRequestView(*request_)
        .SetContextView(context_)
        .SetConfigView(*config_)
        .SetHistorySegmentsView(segments_)
        .SetOptions(std::move(options))
        .Build();
  }

  std::unique_ptr<mozc::composer::Composer> composer_;
  std::unique_ptr<commands::Request> request_;
  std::unique_ptr<config::Config> config_;
  Segments segments_;
  commands::Context context_;
};

TEST_F(MixedDecodingPredictorTest, CallPredictorsForMobileSuggestion) {
  MockConverter converter;
  auto predictor = std::make_unique<Predictor>(
      std::make_unique<CheckCandSizeDictionaryPredictor>(20),
      std::make_unique<CheckCandSizeUserHistoryPredictor>(3, 4), converter);
  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::SUGGESTION);
  EXPECT_FALSE(predictor->Predict(convreq).empty());
}

TEST_F(MixedDecodingPredictorTest, CallPredictorsForMobilePartialSuggestion) {
  MockConverter converter;
  auto predictor = std::make_unique<Predictor>(
      std::make_unique<CheckCandSizeDictionaryPredictor>(20),
      // We don't call history predictor
      std::make_unique<CheckCandSizeUserHistoryPredictor>(-1, -1), converter);
  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::PARTIAL_SUGGESTION);
  EXPECT_FALSE(predictor->Predict(convreq).empty());
}

TEST_F(MixedDecodingPredictorTest, CallPredictorsForMobilePrediction) {
  MockConverter converter;
  auto predictor = std::make_unique<Predictor>(
      std::make_unique<CheckCandSizeDictionaryPredictor>(200),
      std::make_unique<CheckCandSizeUserHistoryPredictor>(3, 4), converter);
  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::PREDICTION);
  EXPECT_FALSE(predictor->Predict(convreq).empty());
}

TEST_F(MixedDecodingPredictorTest, CallPredictorsForMobilePartialPrediction) {
  MockConverter converter;
  std::unique_ptr<engine::Modules> modules =
      engine::ModulesPresetBuilder()
          .PresetDictionary(std::make_unique<MockDictionary>())
          .Build(std::make_unique<testing::MockDataManager>())
          .value();
  auto predictor = std::make_unique<Predictor>(
      std::make_unique<CheckCandSizeDictionaryPredictor>(200),
      std::make_unique<UserHistoryPredictor>(*modules), converter);
  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::PARTIAL_PREDICTION);
  EXPECT_FALSE(predictor->Predict(convreq).empty());
}

TEST_F(MixedDecodingPredictorTest, CallPredictForRequestMobile) {
  auto predictor1 = std::make_unique<MockPredictor>();
  auto predictor2 = std::make_unique<MockPredictor>();
  const std::vector<Result> results(2);
  EXPECT_CALL(*predictor1, Predict(_))
      .Times(AtMost(1))
      .WillOnce(Return(results));
  EXPECT_CALL(*predictor2, Predict(_))
      .Times(AtMost(1))
      .WillOnce(Return(results));

  MockConverter converter;
  auto predictor = std::make_unique<Predictor>(
      std::move(predictor1), std::move(predictor2), converter);
  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::SUGGESTION);
  EXPECT_FALSE(predictor->Predict(convreq).empty());
}

class PredictorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_ = std::make_unique<config::Config>();
    config::ConfigHandler::GetDefaultConfig(config_.get());

    request_ = std::make_unique<commands::Request>();
    composer_ = std::make_unique<composer::Composer>(*request_, *config_);
  }

  ConversionRequest CreateConversionRequest(
      ConversionRequest::RequestType request_type) const {
    ConversionRequest::Options options = {.request_type = request_type};
    return ConversionRequestBuilder()
        .SetComposer(*composer_)
        .SetRequestView(*request_)
        .SetContextView(context_)
        .SetConfigView(*config_)
        .SetHistorySegmentsView(segments_)
        .SetOptions(std::move(options))
        .Build();
  }

  std::unique_ptr<mozc::composer::Composer> composer_;
  std::unique_ptr<commands::Request> request_;
  std::unique_ptr<config::Config> config_;
  commands::Context context_;
  Segments segments_;
};

TEST_F(PredictorTest, AllPredictorsReturnTrue) {
  MockConverter converter;
  auto predictor = std::make_unique<Predictor>(
      std::make_unique<NullPredictor>(true),
      std::make_unique<NullPredictor>(true), converter);
  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::SUGGESTION);
}

TEST_F(PredictorTest, MixedReturnValue) {
  MockConverter converter;
  auto predictor = std::make_unique<Predictor>(
      std::make_unique<NullPredictor>(true),
      std::make_unique<NullPredictor>(false), converter);
  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::SUGGESTION);
  EXPECT_FALSE(predictor->Predict(convreq).empty());
}

TEST_F(PredictorTest, AllPredictorsReturnFalse) {
  MockConverter converter;
  auto predictor = std::make_unique<Predictor>(
      std::make_unique<NullPredictor>(false),
      std::make_unique<NullPredictor>(false), converter);
  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::SUGGESTION);
  EXPECT_TRUE(predictor->Predict(convreq).empty());
}

TEST_F(PredictorTest, CallPredictorsForSuggestion) {
  MockConverter converter;
  const int suggestions_size =
      config::ConfigHandler::DefaultConfig().suggestions_size();
  auto predictor = std::make_unique<Predictor>(
      // -1 as UserHistoryPredictor returns 1 result.
      std::make_unique<CheckCandSizeDictionaryPredictor>(suggestions_size - 1),
      std::make_unique<CheckCandSizeUserHistoryPredictor>(suggestions_size,
                                                          suggestions_size),
      converter);
  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::SUGGESTION);
  EXPECT_FALSE(predictor->Predict(convreq).empty());
}

TEST_F(PredictorTest, CallPredictorsForPrediction) {
  MockConverter converter;
  constexpr int kPredictionSize = 100;
  auto predictor = std::make_unique<Predictor>(
      // -1 as UserHistoryPredictor returns 1 result.
      std::make_unique<CheckCandSizeDictionaryPredictor>(kPredictionSize - 1),
      std::make_unique<CheckCandSizeUserHistoryPredictor>(kPredictionSize,
                                                          kPredictionSize),
      converter);
  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::PREDICTION);
  EXPECT_FALSE(predictor->Predict(convreq).empty());
}

TEST_F(PredictorTest, CallPredictForRequest) {
  auto predictor1 = std::make_unique<MockPredictor>();
  auto predictor2 = std::make_unique<MockPredictor>();
  std::vector<Result> results(1);
  EXPECT_CALL(*predictor1, Predict(_))
      .Times(AtMost(1))
      .WillOnce(Return(results));
  EXPECT_CALL(*predictor2, Predict(_))
      .Times(AtMost(1))
      .WillOnce(Return(results));

  MockConverter converter;
  auto predictor = std::make_unique<Predictor>(
      std::move(predictor1), std::move(predictor2), converter);
  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::SUGGESTION);
  EXPECT_FALSE(predictor->Predict(convreq).empty());
}

TEST_F(PredictorTest, DisableAllSuggestion) {
  auto predictor1 = std::make_unique<NullPredictor>(true);
  auto predictor2 = std::make_unique<NullPredictor>(true);
  const auto *pred1 = predictor1.get();  // Keep the reference
  const auto *pred2 = predictor2.get();  // Keep the reference
  MockConverter converter;
  auto predictor = std::make_unique<Predictor>(
      std::move(predictor1), std::move(predictor2), converter);
  config_->set_presentation_mode(true);
  const ConversionRequest convreq1 =
      CreateConversionRequest(ConversionRequest::SUGGESTION);

  EXPECT_TRUE(predictor->Predict(convreq1).empty());
  EXPECT_FALSE(pred1->predict_called());
  EXPECT_FALSE(pred2->predict_called());

  config_->set_presentation_mode(false);
  const ConversionRequest convreq2 =
      CreateConversionRequest(ConversionRequest::SUGGESTION);
  EXPECT_FALSE(predictor->Predict(convreq2).empty());
  EXPECT_TRUE(pred1->predict_called());
  EXPECT_TRUE(pred2->predict_called());
}

TEST_F(MixedDecodingPredictorTest, FillPos) {
  auto mock_dictionary_predictor = std::make_unique<MockPredictor>();
  auto mock_history_predictor = std::make_unique<MockPredictor>();
  auto add_candidate = [](absl::string_view key, absl::string_view value,
                          int lid, int rid, int cost,
                          std::vector<Result> &results) {
    Result result;
    result.key = key;
    result.value = value;
    result.lid = lid;
    result.rid = rid;
    result.cost = cost;
    result.wcost = cost;
    results.emplace_back(std::move(result));
  };

  std::vector<Result> predictor_results, history_results;
  {
    add_candidate("key", "value", 0, 0, 1, predictor_results);
    add_candidate("key", "value", 2, 3, 100, predictor_results);
  }
  EXPECT_CALL(*mock_history_predictor, Predict(_))
      .WillOnce(Return(history_results));
  EXPECT_CALL(*mock_dictionary_predictor, Predict(_))
      .WillOnce(Return(predictor_results));

  MockConverter converter;
  auto predictor =
      std::make_unique<Predictor>(std::move(mock_dictionary_predictor),
                                  std::move(mock_history_predictor), converter);

  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::SUGGESTION);
  const std::vector<Result> results = predictor->Predict(convreq);

  EXPECT_EQ(results.size(), 2);
  const Result &result = results[0];
  EXPECT_EQ(result.key, "key");
  EXPECT_EQ(result.value, "value");
  // lid and rid are filled from another result.
  EXPECT_EQ(result.lid, 2);
  EXPECT_EQ(result.rid, 3);
  // cost is not changed.
  EXPECT_EQ(result.cost, 1);
}

}  // namespace mozc::prediction
