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

#include "prediction/single_kanji_prediction_aggregator.h"

#include <memory>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/strings/unicode.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/candidate.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/single_kanji_dictionary.h"
#include "engine/modules.h"
#include "prediction/result.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "request/request_test_util.h"
#include "testing/gunit.h"

namespace mozc::prediction {
namespace {

void SetUpInputWithKey(absl::string_view key, composer::Composer *composer) {
  composer->SetPreeditTextForTestOnly(key);
}

bool FindResultByKey(absl::Span<const Result> results,
                     const absl::string_view key) {
  for (const auto &result : results) {
    if (result.key == key && !result.removed) {
      return true;
    }
  }
  return false;
}

class SingleKanjiPredictionAggregatorTest : public ::testing::Test {
 protected:
  SingleKanjiPredictionAggregatorTest()
      : modules_(engine::Modules::Create(
                     std::make_unique<testing::MockDataManager>())
                     .value()) {}

  ~SingleKanjiPredictionAggregatorTest() override = default;

 protected:
  void SetUp() override {
    request_ = std::make_unique<commands::Request>();
    request_test_util::FillMobileRequest(request_.get());
    config_ = std::make_unique<config::Config>();
    config::ConfigHandler::GetDefaultConfig(config_.get());
    composer_ = std::make_unique<composer::Composer>(
        composer::Table::GetSharedDefaultTable(), *request_, *config_);
  }

  ConversionRequest CreateConversionRequest() const {
    ConversionRequest::Options options = {
        .request_type = ConversionRequest::PREDICTION,
    };
    return ConversionRequestBuilder()
        .SetComposer(*composer_)
        .SetRequestView(*request_)
        .SetConfigView(*config_)
        .SetOptions(std::move(options))
        .Build();
  }

  const dictionary::PosMatcher &pos_matcher() const {
    return modules_->GetPosMatcher();
  }

  const dictionary::SingleKanjiDictionary &single_kanji_dictionary() const {
    return modules_->GetSingleKanjiDictionary();
  }

  std::unique_ptr<composer::Composer> composer_;
  std::unique_ptr<config::Config> config_;
  std::unique_ptr<commands::Request> request_;
  commands::Context context_;
  std::unique_ptr<engine::Modules> modules_;
};

TEST_F(SingleKanjiPredictionAggregatorTest, NoResult) {
  SetUpInputWithKey("ん", composer_.get());
  const SingleKanjiPredictionAggregator aggregator(pos_matcher(),
                                                   single_kanji_dictionary());

  const ConversionRequest convreq = CreateConversionRequest();
  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  EXPECT_TRUE(results.empty());
}

TEST_F(SingleKanjiPredictionAggregatorTest, NoResultForHardwareKeyboard) {
  SetUpInputWithKey("あけぼのの", composer_.get());
  const SingleKanjiPredictionAggregator aggregator(pos_matcher(),
                                                   single_kanji_dictionary());
  request_test_util::FillMobileRequestWithHardwareKeyboard(request_.get());
  const ConversionRequest convreq = CreateConversionRequest();
  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  EXPECT_EQ(results.size(), 0);
}

TEST_F(SingleKanjiPredictionAggregatorTest, ResultsFromPrefix) {
  SetUpInputWithKey("あけぼのの", composer_.get());
  const SingleKanjiPredictionAggregator aggregator(pos_matcher(),
                                                   single_kanji_dictionary());
  const ConversionRequest convreq = CreateConversionRequest();
  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  EXPECT_GT(results.size(), 1);
  EXPECT_TRUE(FindResultByKey(results, "あけぼの"));
  EXPECT_TRUE(FindResultByKey(results, "あけ"));
  for (int i = 0; i < results.size(); ++i) {
    if (results[i].key == "あけぼの") {
      EXPECT_EQ(results[i].wcost, i);
    } else {
      EXPECT_GT(results[i].wcost, i);  // Cost offset should be added
    }
  }
}

TEST_F(SingleKanjiPredictionAggregatorTest, Result) {
  SetUpInputWithKey("あけぼの", composer_.get());
  const SingleKanjiPredictionAggregator aggregator(pos_matcher(),
                                                   single_kanji_dictionary());
  const ConversionRequest convreq = CreateConversionRequest();
  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  EXPECT_GT(results.size(), 1);
  const auto &result = results[0];
  EXPECT_EQ(result.key, "あけぼの");
  EXPECT_EQ(result.types, SINGLE_KANJI);
  EXPECT_EQ(result.lid, pos_matcher().GetGeneralSymbolId());
  EXPECT_EQ(result.rid, pos_matcher().GetGeneralSymbolId());
  EXPECT_FALSE(result.candidate_attributes &
               converter::Candidate::PARTIALLY_KEY_CONSUMED);
  EXPECT_EQ(result.consumed_key_size, 0);
}

TEST_F(SingleKanjiPredictionAggregatorTest, PrefixResult) {
  SetUpInputWithKey("あけぼのの", composer_.get());
  const SingleKanjiPredictionAggregator aggregator(pos_matcher(),
                                                   single_kanji_dictionary());
  const ConversionRequest convreq = CreateConversionRequest();
  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  EXPECT_GT(results.size(), 1);
  const auto &result = results[0];
  EXPECT_EQ(result.key, "あけぼの");
  EXPECT_EQ(result.types, SINGLE_KANJI);
  EXPECT_EQ(result.lid, pos_matcher().GetGeneralSymbolId());
  EXPECT_EQ(result.rid, pos_matcher().GetGeneralSymbolId());
  EXPECT_TRUE(result.candidate_attributes &
              converter::Candidate::PARTIALLY_KEY_CONSUMED);
  EXPECT_EQ(result.consumed_key_size, strings::CharsLen("あけぼの"));
}

TEST_F(SingleKanjiPredictionAggregatorTest, NoPrefixResult) {
  request_->set_auto_partial_suggestion(false);
  SetUpInputWithKey("あけぼのの", composer_.get());
  const SingleKanjiPredictionAggregator aggregator(pos_matcher(),
                                                   single_kanji_dictionary());
  const ConversionRequest convreq = CreateConversionRequest();
  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  EXPECT_EQ(results.size(), 0);  // No "あけぼの"
}

TEST_F(SingleKanjiPredictionAggregatorTest, SvsVariation) {
  SetUpInputWithKey("かみ", composer_.get());
  const SingleKanjiPredictionAggregator aggregator(pos_matcher(),
                                                   single_kanji_dictionary());
  request_->mutable_decoder_experiment_params()->set_variation_character_types(
      commands::DecoderExperimentParams::SVS_JAPANESE);
  const ConversionRequest convreq = CreateConversionRequest();
  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  EXPECT_GT(results.size(), 1);

  auto contains = [&](absl::string_view value) {
    for (const auto &result : results) {
      if (result.value == value) {
        return true;
      }
    }
    return false;
  };
  EXPECT_TRUE(contains("\u795E\uFE00"));  // 神︀ SVS character.
  EXPECT_FALSE(contains("\uFA19"));       // 神 CJK compat ideograph.
}

}  // namespace
}  // namespace mozc::prediction
