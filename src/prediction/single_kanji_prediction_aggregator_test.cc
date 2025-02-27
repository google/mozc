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
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "prediction/result.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "request/request_test_util.h"
#include "testing/gunit.h"

namespace mozc::prediction {
namespace {

void SetUpInputWithKey(absl::string_view key, composer::Composer *composer,
                       Segments *segments) {
  composer->SetPreeditTextForTestOnly(key);
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FREE);
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
  SingleKanjiPredictionAggregatorTest() {
    data_manager_ = std::make_unique<testing::MockDataManager>();
    pos_matcher_ = std::make_unique<dictionary::PosMatcher>(
        data_manager_->GetPosMatcherData());
  }

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

  std::unique_ptr<composer::Composer> composer_;
  std::unique_ptr<config::Config> config_;
  std::unique_ptr<commands::Request> request_;
  commands::Context context_;

  std::unique_ptr<testing::MockDataManager> data_manager_;
  std::unique_ptr<dictionary::PosMatcher> pos_matcher_;
};

TEST_F(SingleKanjiPredictionAggregatorTest, NoResult) {
  Segments segments;
  SetUpInputWithKey("ん", composer_.get(), &segments);
  SingleKanjiPredictionAggregator aggregator(*data_manager_);
  const ConversionRequest convreq = CreateConversionRequest();
  const std::vector<Result> results =
      aggregator.AggregateResults(convreq, segments);
  EXPECT_TRUE(results.empty());
}

TEST_F(SingleKanjiPredictionAggregatorTest, NoResultForHardwareKeyboard) {
  Segments segments;
  SetUpInputWithKey("あけぼのの", composer_.get(), &segments);
  SingleKanjiPredictionAggregator aggregator(*data_manager_);
  request_test_util::FillMobileRequestWithHardwareKeyboard(request_.get());
  const ConversionRequest convreq = CreateConversionRequest();
  const std::vector<Result> results =
      aggregator.AggregateResults(convreq, segments);
  EXPECT_EQ(results.size(), 0);
}

TEST_F(SingleKanjiPredictionAggregatorTest, ResultsFromPrefix) {
  Segments segments;
  SetUpInputWithKey("あけぼのの", composer_.get(), &segments);
  SingleKanjiPredictionAggregator aggregator(*data_manager_);
  const ConversionRequest convreq = CreateConversionRequest();
  const std::vector<Result> results =
      aggregator.AggregateResults(convreq, segments);
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
  Segments segments;
  SetUpInputWithKey("あけぼの", composer_.get(), &segments);
  SingleKanjiPredictionAggregator aggregator(*data_manager_);
  const ConversionRequest convreq = CreateConversionRequest();
  const std::vector<Result> results =
      aggregator.AggregateResults(convreq, segments);
  EXPECT_GT(results.size(), 1);
  const auto &result = results[0];
  EXPECT_EQ(result.key, "あけぼの");
  EXPECT_EQ(result.types, SINGLE_KANJI);
  EXPECT_EQ(result.lid, pos_matcher_->GetGeneralSymbolId());
  EXPECT_EQ(result.rid, pos_matcher_->GetGeneralSymbolId());
  EXPECT_FALSE(result.candidate_attributes &
               Segment::Candidate::PARTIALLY_KEY_CONSUMED);
  EXPECT_EQ(result.consumed_key_size, 0);
}

TEST_F(SingleKanjiPredictionAggregatorTest, PrefixResult) {
  Segments segments;
  SetUpInputWithKey("あけぼのの", composer_.get(), &segments);
  SingleKanjiPredictionAggregator aggregator(*data_manager_);
  const ConversionRequest convreq = CreateConversionRequest();
  const std::vector<Result> results =
      aggregator.AggregateResults(convreq, segments);
  EXPECT_GT(results.size(), 1);
  const auto &result = results[0];
  EXPECT_EQ(result.key, "あけぼの");
  EXPECT_EQ(result.types, SINGLE_KANJI);
  EXPECT_EQ(result.lid, pos_matcher_->GetGeneralSymbolId());
  EXPECT_EQ(result.rid, pos_matcher_->GetGeneralSymbolId());
  EXPECT_TRUE(result.candidate_attributes &
              Segment::Candidate::PARTIALLY_KEY_CONSUMED);
  EXPECT_EQ(result.consumed_key_size, strings::CharsLen("あけぼの"));
}

TEST_F(SingleKanjiPredictionAggregatorTest, NoPrefixResult) {
  request_->set_auto_partial_suggestion(false);
  Segments segments;
  SetUpInputWithKey("あけぼのの", composer_.get(), &segments);
  SingleKanjiPredictionAggregator aggregator(*data_manager_);
  const ConversionRequest convreq = CreateConversionRequest();
  const std::vector<Result> results =
      aggregator.AggregateResults(convreq, segments);
  EXPECT_EQ(results.size(), 0);  // No "あけぼの"
}

TEST_F(SingleKanjiPredictionAggregatorTest, SvsVariation) {
  Segments segments;
  SetUpInputWithKey("かみ", composer_.get(), &segments);
  SingleKanjiPredictionAggregator aggregator(*data_manager_);
  request_->mutable_decoder_experiment_params()->set_variation_character_types(
      commands::DecoderExperimentParams::SVS_JAPANESE);
  const ConversionRequest convreq = CreateConversionRequest();
  const std::vector<Result> results =
      aggregator.AggregateResults(convreq, segments);
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
