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

#include "prediction/dictionary_predictor.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/strings/assign.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/connector.h"
#include "converter/converter_interface.h"
#include "converter/converter_mock.h"
#include "converter/immutable_converter_interface.h"
#include "converter/segmenter.h"
#include "converter/segments.h"
#include "converter/segments_matchers.h"
#include "data_manager/data_manager_interface.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "prediction/prediction_aggregator_interface.h"
#include "prediction/rescorer_interface.h"
#include "prediction/rescorer_mock.h"
#include "prediction/result.h"
#include "prediction/suggestion_filter.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "session/request_test_util.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"
#include "absl/memory/memory.h"
#include "absl/random/random.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace mozc::prediction {

class DictionaryPredictorTestPeer {
 public:
  DictionaryPredictorTestPeer(
      std::unique_ptr<const prediction::PredictionAggregatorInterface>
          aggregator,
      const DataManagerInterface &data_manager,
      const ImmutableConverterInterface *immutable_converter,
      const Connector &connector, const Segmenter *segmenter,
      const dictionary::PosMatcher pos_matcher,
      const SuggestionFilter &suggestion_filter,
      const prediction::RescorerInterface *rescorer = nullptr)
      : predictor_("DictionaryPredictorForTest", std::move(aggregator),
                   data_manager, immutable_converter, connector, segmenter,
                   pos_matcher, suggestion_filter, rescorer) {}

  bool PredictForRequest(const ConversionRequest &request,
                         Segments *segments) const {
    return predictor_.PredictForRequest(request, segments);
  }

  void Finish(const ConversionRequest &request, Segments *segments) {
    return predictor_.Finish(request, segments);
  }

  static bool IsAggressiveSuggestion(size_t query_len, size_t key_len, int cost,
                                     bool is_suggestion,
                                     size_t total_candidates_size) {
    return DictionaryPredictor::IsAggressiveSuggestion(
        query_len, key_len, cost, is_suggestion, total_candidates_size);
  }

  static size_t GetMissSpelledPosition(const absl::string_view key,
                                       const absl::string_view value) {
    return DictionaryPredictor::GetMissSpelledPosition(key, value);
  }

  static void RemoveMissSpelledCandidates(size_t request_key_len,
                                          std::vector<Result> *results) {
    return DictionaryPredictor::RemoveMissSpelledCandidates(request_key_len,
                                                            results);
  }

  static void ApplyPenaltyForKeyExpansion(const Segments &segments,
                                          std::vector<Result> *results) {
    return DictionaryPredictor::ApplyPenaltyForKeyExpansion(segments, results);
  }

  static void SetDebugDescription(PredictionTypes types,
                                  Segment::Candidate *candidate) {
    DictionaryPredictor::SetDebugDescription(types, candidate);
  }

  int GetLMCost(const Result &result, int rid) const {
    return predictor_.GetLMCost(result, rid);
  }

  void SetPredictionCostForMixedConversion(const ConversionRequest &request,
                                           const Segments &segments,
                                           std::vector<Result> *results) const {
    return predictor_.SetPredictionCostForMixedConversion(request, segments,
                                                          results);
  }

  bool AddPredictionToCandidates(const ConversionRequest &request,
                                 Segments *segments,
                                 absl::Span<Result> results) const {
    return predictor_.AddPredictionToCandidates(request, segments, results);
  }

  static void MaybeSuppressAggressiveTypingCorrection(
      const ConversionRequest &request, Segments *segments) {
    DictionaryPredictor::MaybeSuppressAggressiveTypingCorrection(request,
                                                                 segments);
  }

  static void AddRescoringDebugDescription(Segments *segments) {
    DictionaryPredictor::AddRescoringDebugDescription(segments);
  }

 private:
  DictionaryPredictor predictor_;
};

namespace {
using ::mozc::dictionary::PosMatcher;
using ::mozc::dictionary::Token;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgPointee;

constexpr int kInfinity = (2 << 20);

Result CreateResult4(absl::string_view key, absl::string_view value,
                     PredictionTypes types,
                     Token::AttributesBitfield token_attrs) {
  Result result;
  strings::Assign(result.key, key);
  strings::Assign(result.value, value);
  result.SetTypesAndTokenAttributes(types, token_attrs);
  return result;
}

Result CreateResult5(absl::string_view key, absl::string_view value, int wcost,
                     PredictionTypes types,
                     Token::AttributesBitfield token_attrs) {
  Result result;
  strings::Assign(result.key, key);
  strings::Assign(result.value, value);
  result.wcost = wcost;
  result.SetTypesAndTokenAttributes(types, token_attrs);
  return result;
}

Result CreateResult6(absl::string_view key, absl::string_view value, int wcost,
                     int cost, PredictionTypes types,
                     Token::AttributesBitfield token_attrs) {
  Result result;
  strings::Assign(result.key, key);
  strings::Assign(result.value, value);
  result.wcost = wcost;
  result.cost = cost;
  result.SetTypesAndTokenAttributes(types, token_attrs);
  return result;
}

void PushBackInnerSegmentBoundary(size_t key_len, size_t value_len,
                                  size_t content_key_len,
                                  size_t content_value_len, Result *result) {
  uint32_t encoded;
  if (!Segment::Candidate::EncodeLengths(key_len, value_len, content_key_len,
                                         content_value_len, &encoded)) {
    return;
  }
  result->inner_segment_boundary.push_back(encoded);
}

void SetSegmentForCommit(absl::string_view candidate_value,
                         int candidate_source_info, Segments *segments) {
  segments->Clear();
  Segment *segment = segments->add_segment();
  segment->set_key("");
  segment->set_segment_type(Segment::FIXED_VALUE);
  Segment::Candidate *candidate = segment->add_candidate();
  strings::Assign(candidate->key, candidate_value);
  strings::Assign(candidate->content_key, candidate_value);
  strings::Assign(candidate->value, candidate_value);
  strings::Assign(candidate->content_value, candidate_value);
  candidate->source_info = candidate_source_info;
}

void InitSegmentsWithKey(absl::string_view key, Segments *segments) {
  segments->Clear();

  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FREE);
}

void PrependHistorySegments(absl::string_view key, absl::string_view value,
                            Segments *segments) {
  Segment *seg = segments->push_front_segment();
  seg->set_segment_type(Segment::HISTORY);
  seg->set_key(key);
  Segment::Candidate *c = seg->add_candidate();
  c->key.assign(key.data(), key.size());
  c->content_key = c->key;
  c->value.assign(value.data(), value.size());
  c->content_value = c->value;
}

void GenerateKeyEvents(absl::string_view text,
                       std::vector<commands::KeyEvent> *keys) {
  keys->clear();
  for (const char32_t w : Util::Utf8ToUtf32(text)) {
    commands::KeyEvent key;
    if (w <= 0x7F) {  // IsAscii, w is unsigned.
      key.set_key_code(w);
    } else {
      key.set_key_code('?');
      Util::Ucs4ToUtf8(w, key.mutable_key_string());
    }
    keys->push_back(key);
  }
}

void InsertInputSequence(absl::string_view text, composer::Composer *composer) {
  std::vector<commands::KeyEvent> keys;
  GenerateKeyEvents(text, &keys);

  for (size_t i = 0; i < keys.size(); ++i) {
    composer->InsertCharacterKeyEvent(keys[i]);
  }
}

bool FindCandidateByKeyValue(const Segment &segment, absl::string_view key,
                             absl::string_view value) {
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    const Segment::Candidate &c = segment.candidate(i);
    if (c.key == key && c.value == value) {
      return true;
    }
  }
  return false;
}

bool FindCandidateByValue(const Segment &segment, absl::string_view value) {
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    const Segment::Candidate &c = segment.candidate(i);
    if (c.value == value) {
      return true;
    }
  }
  return false;
}

// Simple immutable converter mock
class MockImmutableConverter : public ImmutableConverterInterface {
 public:
  MOCK_METHOD(bool, ConvertForRequest,
              (const ConversionRequest &request, Segments *segments),
              (const override));
};

class MockAggregator : public prediction::PredictionAggregatorInterface {
 public:
  MOCK_METHOD(std::vector<prediction::Result>, AggregateResults,
              (const ConversionRequest &request, const Segments &segments),
              (const override));
};

// Helper class to hold predictor objects.
class MockDataAndPredictor {
 public:
  explicit MockDataAndPredictor(
      const prediction::RescorerInterface *rescorer = nullptr)
      : data_manager_(),
        mock_immutable_converter_(),
        mock_aggregator_(new MockAggregator()),
        pos_matcher_(data_manager_.GetPosMatcherData()),
        connector_(Connector::CreateFromDataManager(data_manager_).value()),
        segmenter_(Segmenter::CreateFromDataManager(data_manager_)),
        suggestion_filter_(SuggestionFilter::CreateOrDie(
            data_manager_.GetSuggestionFilterData())) {
    CHECK(segmenter_);

    predictor_ = std::make_unique<DictionaryPredictorTestPeer>(
        absl::WrapUnique(mock_aggregator_), data_manager_,
        &mock_immutable_converter_, connector_, segmenter_.get(), pos_matcher_,
        suggestion_filter_, rescorer);
  }

  MockImmutableConverter *mutable_immutable_converter() {
    return &mock_immutable_converter_;
  }

  MockAggregator *mutable_aggregator() { return mock_aggregator_; }
  const Connector &connector() { return connector_; }
  const PosMatcher &pos_matcher() { return pos_matcher_; }

  const DictionaryPredictorTestPeer &predictor() { return *predictor_; }
  DictionaryPredictorTestPeer *mutable_predictor() { return predictor_.get(); }

 private:
  const testing::MockDataManager data_manager_;
  MockImmutableConverter mock_immutable_converter_;
  MockAggregator *mock_aggregator_;
  PosMatcher pos_matcher_;
  Connector connector_;
  std::unique_ptr<const Segmenter> segmenter_;
  SuggestionFilter suggestion_filter_;
  MockConverter converter_;

  std::unique_ptr<DictionaryPredictorTestPeer> predictor_;
};

class DictionaryPredictorTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    request_ = std::make_unique<commands::Request>();
    config_ = std::make_unique<config::Config>();
    config::ConfigHandler::GetDefaultConfig(config_.get());
    table_ = std::make_unique<composer::Table>();
    composer_ = std::make_unique<composer::Composer>(
        table_.get(), request_.get(), config_.get());
    convreq_for_suggestion_ = std::make_unique<ConversionRequest>(
        composer_.get(), request_.get(), config_.get());
    convreq_for_suggestion_->set_request_type(ConversionRequest::SUGGESTION);
    convreq_for_prediction_ = std::make_unique<ConversionRequest>(
        composer_.get(), request_.get(), config_.get());
    convreq_for_prediction_->set_request_type(ConversionRequest::PREDICTION);

    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
  }

  void TearDown() override {
    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
  }

  static std::unique_ptr<MockDataAndPredictor>
  CreateDictionaryPredictorWithMockData(
      const prediction::RescorerInterface *rescorer = nullptr) {
    return std::make_unique<MockDataAndPredictor>(rescorer);
  }

  std::unique_ptr<composer::Composer> composer_;
  std::unique_ptr<composer::Table> table_;
  std::unique_ptr<ConversionRequest> convreq_for_suggestion_;
  std::unique_ptr<ConversionRequest> convreq_for_prediction_;
  std::unique_ptr<config::Config> config_;
  std::unique_ptr<commands::Request> request_;

 private:
  mozc::usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;
};

TEST_F(DictionaryPredictorTest, IsAggressiveSuggestion) {
  // "ただしい",
  // "ただしいけめんにかぎる",
  EXPECT_TRUE(DictionaryPredictorTestPeer::IsAggressiveSuggestion(
      4,     // query_len
      11,    // key_len
      6000,  // cost
      true,  // is_suggestion
      20));  // total_candidates_size

  // cost <= 4000
  EXPECT_FALSE(DictionaryPredictorTestPeer::IsAggressiveSuggestion(4, 11, 4000,
                                                                   true, 20));

  // not suggestion
  EXPECT_FALSE(DictionaryPredictorTestPeer::IsAggressiveSuggestion(4, 11, 4000,
                                                                   false, 20));

  // total_candidates_size is small
  EXPECT_FALSE(DictionaryPredictorTestPeer::IsAggressiveSuggestion(4, 11, 4000,
                                                                   true, 5));

  // query_length = 5
  EXPECT_FALSE(DictionaryPredictorTestPeer::IsAggressiveSuggestion(5, 11, 6000,
                                                                   true, 20));

  // "それでも",
  // "それでもぼくはやっていない",
  EXPECT_TRUE(DictionaryPredictorTestPeer::IsAggressiveSuggestion(4, 13, 6000,
                                                                  true, 20));

  // cost <= 4000
  EXPECT_FALSE(DictionaryPredictorTestPeer::IsAggressiveSuggestion(4, 13, 4000,
                                                                   true, 20));
}

TEST_F(DictionaryPredictorTest, GetMissSpelledPosition) {
  EXPECT_EQ(DictionaryPredictorTestPeer::GetMissSpelledPosition("", ""), 0);
  EXPECT_EQ(DictionaryPredictorTestPeer::GetMissSpelledPosition("れみおめろん",
                                                                "レミオロメン"),
            3);
  EXPECT_EQ(DictionaryPredictorTestPeer::GetMissSpelledPosition("とーとばっく",
                                                                "トートバッグ"),
            5);
  EXPECT_EQ(DictionaryPredictorTestPeer::GetMissSpelledPosition(
                "おーすとりらあ", "オーストラリア"),
            4);
  EXPECT_EQ(DictionaryPredictorTestPeer::GetMissSpelledPosition(
                "おーすとりあ", "おーすとらりあ"),
            4);
  EXPECT_EQ(DictionaryPredictorTestPeer::GetMissSpelledPosition(
                "じきそうしょう", "時期尚早"),
            7);
}

TEST_F(DictionaryPredictorTest, RemoveMissSpelledCandidates) {
  {
    std::vector<Result> results = {
        CreateResult4("ばっく", "バッグ", prediction::UNIGRAM,
                      Token::SPELLING_CORRECTION),
        CreateResult4("ばっぐ", "バッグ", prediction::UNIGRAM, Token::NONE),
        CreateResult4("ばっく", "バッく", prediction::UNIGRAM, Token::NONE),
    };
    DictionaryPredictorTestPeer::RemoveMissSpelledCandidates(1, &results);

    ASSERT_EQ(3, results.size());
    EXPECT_TRUE(results[0].removed);
    EXPECT_FALSE(results[1].removed);
    EXPECT_TRUE(results[2].removed);
    EXPECT_EQ(results[0].types, prediction::UNIGRAM);
    EXPECT_EQ(results[1].types, prediction::UNIGRAM);
    EXPECT_EQ(results[2].types, prediction::UNIGRAM);
  }
  {
    std::vector<Result> results = {
        CreateResult4("ばっく", "バッグ", prediction::UNIGRAM,
                      Token::SPELLING_CORRECTION),
        CreateResult4("てすと", "テスト", prediction::UNIGRAM, Token::NONE),
    };
    DictionaryPredictorTestPeer::RemoveMissSpelledCandidates(1, &results);

    CHECK_EQ(2, results.size());
    EXPECT_FALSE(results[0].removed);
    EXPECT_FALSE(results[1].removed);
    EXPECT_EQ(results[0].types, prediction::UNIGRAM);
    EXPECT_EQ(results[1].types, prediction::UNIGRAM);
  }
  {
    std::vector<Result> results = {
        CreateResult4("ばっく", "バッグ", prediction::UNIGRAM,
                      Token::SPELLING_CORRECTION),
        CreateResult4("ばっく", "バック", prediction::UNIGRAM, Token::NONE),
    };
    DictionaryPredictorTestPeer::RemoveMissSpelledCandidates(1, &results);

    CHECK_EQ(2, results.size());
    EXPECT_TRUE(results[0].removed);
    EXPECT_TRUE(results[1].removed);
  }
  {
    std::vector<Result> results = {
        CreateResult4("ばっく", "バッグ", prediction::UNIGRAM,
                      Token::SPELLING_CORRECTION),
        CreateResult4("ばっく", "バック", prediction::UNIGRAM, Token::NONE),
    };
    DictionaryPredictorTestPeer::RemoveMissSpelledCandidates(3, &results);

    CHECK_EQ(2, results.size());
    EXPECT_FALSE(results[0].removed);
    EXPECT_TRUE(results[1].removed);
    EXPECT_EQ(results[0].types, prediction::UNIGRAM);
    EXPECT_EQ(results[1].types, prediction::UNIGRAM);
  }
}

TEST_F(DictionaryPredictorTest, ExpansionPenaltyForRomanTest) {
  table_->LoadFromFile("system://romanji-hiragana.tsv");
  composer_->SetTable(table_.get());

  Segments segments;
  InsertInputSequence("ak", composer_.get());
  std::string predicton_query;
  composer_->GetQueryForPrediction(&predicton_query);
  EXPECT_EQ(predicton_query, "あ");
  InitSegmentsWithKey(predicton_query, &segments);

  std::vector<Result> results = {
      CreateResult4("あか", "赤", prediction::UNIGRAM, Token::NONE),
      CreateResult4("あき", "秋", prediction::UNIGRAM, Token::NONE),
      CreateResult4("あかぎ", "アカギ", prediction::UNIGRAM, Token::NONE),
  };
  EXPECT_EQ(results.size(), 3);
  EXPECT_EQ(results[0].cost, 0);
  EXPECT_EQ(results[1].cost, 0);
  EXPECT_EQ(results[2].cost, 0);

  DictionaryPredictorTestPeer::ApplyPenaltyForKeyExpansion(segments, &results);

  // no penalties
  EXPECT_EQ(results[0].cost, 0);
  EXPECT_EQ(results[1].cost, 0);
  EXPECT_EQ(results[2].cost, 0);
}

TEST_F(DictionaryPredictorTest, ExpansionPenaltyForKanaTest) {
  table_->LoadFromFile("system://kana.tsv");
  composer_->SetTable(table_.get());

  Segments segments;
  InsertInputSequence("あし", composer_.get());
  std::string predicton_query;
  composer_->GetQueryForPrediction(&predicton_query);
  EXPECT_EQ(predicton_query, "あし");
  InitSegmentsWithKey(predicton_query, &segments);

  std::vector<Result> results{
      CreateResult4("あし", "足", prediction::UNIGRAM, Token::NONE),
      CreateResult4("あじ", "味", prediction::UNIGRAM, Token::NONE),
      CreateResult4("あした", "明日", prediction::UNIGRAM, Token::NONE),
      CreateResult4("あじあ", "アジア", prediction::UNIGRAM, Token::NONE),
  };
  EXPECT_EQ(results.size(), 4);
  EXPECT_EQ(results[0].cost, 0);
  EXPECT_EQ(results[1].cost, 0);
  EXPECT_EQ(results[2].cost, 0);
  EXPECT_EQ(results[3].cost, 0);

  DictionaryPredictorTestPeer::ApplyPenaltyForKeyExpansion(segments, &results);

  EXPECT_EQ(results[0].cost, 0);
  EXPECT_LT(0, results[1].cost);
  EXPECT_EQ(results[2].cost, 0);
  EXPECT_LT(0, results[3].cost);
}

TEST_F(DictionaryPredictorTest, GetLMCost) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();
  const Connector &connector = data_and_predictor->connector();

  Result result;
  result.wcost = 64;

  for (int rid = 0; rid < 100; ++rid) {
    for (int lid = 0; lid < 100; ++lid) {
      result.lid = lid;
      const int c1 = connector.GetTransitionCost(rid, result.lid);
      const int c2 = connector.GetTransitionCost(0, result.lid);
      result.types = prediction::SUFFIX;
      EXPECT_EQ(predictor.GetLMCost(result, rid), c1 + result.wcost);

      result.types = prediction::REALTIME;
      EXPECT_EQ(predictor.GetLMCost(result, rid),
                std::min(c1, c2) + result.wcost);
    }
  }
}

TEST_F(DictionaryPredictorTest, SetPredictionCostForMixedConversion) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();

  Segments segments;
  InitSegmentsWithKey("てすと", &segments);

  std::vector<Result> results = {
      CreateResult4("てすと", "てすと", prediction::UNIGRAM, Token::NONE),
      CreateResult4("てすと", "テスト", prediction::UNIGRAM, Token::NONE),
      CreateResult4("てすとてすと", "テストテスト", prediction::UNIGRAM,
                    Token::NONE),
  };

  predictor.SetPredictionCostForMixedConversion(*convreq_for_prediction_,
                                                segments, &results);

  EXPECT_EQ(results.size(), 3);
  EXPECT_EQ(results[0].value, "てすと");
  EXPECT_EQ(results[1].value, "テスト");
  EXPECT_EQ(results[2].value, "テストテスト");
  EXPECT_GT(results[2].cost, results[0].cost);
  EXPECT_GT(results[2].cost, results[1].cost);
}

TEST_F(DictionaryPredictorTest, SetLMCostForUserDictionaryWord) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();

  const char *kAikaHiragana = "あいか";
  const char *kAikaKanji = "愛佳";

  Segments segments;
  InitSegmentsWithKey(kAikaHiragana, &segments);

  {
    // Cost of words in user dictionary should be decreased.
    constexpr int kOriginalWordCost = 10000;
    std::vector<Result> results = {
        CreateResult5(kAikaHiragana, kAikaKanji, kOriginalWordCost,
                      prediction::UNIGRAM, Token::USER_DICTIONARY),
    };

    predictor.SetPredictionCostForMixedConversion(*convreq_for_prediction_,
                                                  segments, &results);

    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].value, kAikaKanji);
    EXPECT_GT(kOriginalWordCost, results[0].cost);
    EXPECT_LE(1, results[0].cost);
  }

  {
    // Cost of words in user dictionary should not be decreased to below 1.
    constexpr int kOriginalWordCost = 10;
    std::vector<Result> results = {
        CreateResult5(kAikaHiragana, kAikaKanji, kOriginalWordCost,
                      prediction::UNIGRAM, Token::USER_DICTIONARY),
    };

    predictor.SetPredictionCostForMixedConversion(*convreq_for_prediction_,
                                                  segments, &results);

    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].value, kAikaKanji);
    EXPECT_GT(kOriginalWordCost, results[0].cost);
    EXPECT_LE(1, results[0].cost);
  }

  {
    // Cost of general symbols should not be decreased.
    constexpr int kOriginalWordCost = 10000;
    std::vector<Result> results = {
        CreateResult5(kAikaHiragana, kAikaKanji, kOriginalWordCost,
                      prediction::UNIGRAM, Token::USER_DICTIONARY),
    };
    ASSERT_EQ(1, results.size());
    results[0].lid = data_and_predictor->pos_matcher().GetGeneralSymbolId();
    results[0].rid = results[0].lid;

    predictor.SetPredictionCostForMixedConversion(*convreq_for_prediction_,
                                                  segments, &results);

    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].value, kAikaKanji);
    EXPECT_LE(kOriginalWordCost, results[0].cost);
  }

  {
    // Cost of words not in user dictionary should not be decreased.
    constexpr int kOriginalWordCost = 10000;
    std::vector<Result> results = {
        CreateResult5(kAikaHiragana, kAikaKanji, kOriginalWordCost,
                      prediction::UNIGRAM, Token::NONE),
    };

    predictor.SetPredictionCostForMixedConversion(*convreq_for_prediction_,
                                                  segments, &results);

    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].value, kAikaKanji);
    EXPECT_EQ(results[0].cost, kOriginalWordCost);
  }
}

TEST_F(DictionaryPredictorTest, SuggestSpellingCorrection) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();
  MockAggregator *aggregator = data_and_predictor->mutable_aggregator();
  EXPECT_CALL(*aggregator, AggregateResults(_, _))
      .WillOnce(Return(std::vector<Result>{
          CreateResult5("あぼがど", "アボカド", 500, prediction::UNIGRAM,
                        Token::SPELLING_CORRECTION),
          CreateResult5("あぼがど", "アボガド", 500, prediction::UNIGRAM,
                        Token::NONE)}));

  Segments segments;
  InitSegmentsWithKey("あぼがど", &segments);

  predictor.PredictForRequest(*convreq_for_prediction_, &segments);

  EXPECT_TRUE(FindCandidateByValue(segments.conversion_segment(0), "アボカド"));
}

TEST_F(DictionaryPredictorTest, DoNotSuggestSpellingCorrectionBeforeMismatch) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();
  MockAggregator *aggregator = data_and_predictor->mutable_aggregator();

  EXPECT_CALL(*aggregator, AggregateResults(_, _))
      .WillOnce(Return(std::vector<Result>{
          CreateResult5("あぼがど", "アボカド", 500, prediction::UNIGRAM,
                        Token::SPELLING_CORRECTION),
          CreateResult5("あぼがど", "アボガド", 500, prediction::UNIGRAM,
                        Token::NONE),
      }));

  Segments segments;
  InitSegmentsWithKey("あぼが", &segments);

  predictor.PredictForRequest(*convreq_for_prediction_, &segments);

  EXPECT_FALSE(
      FindCandidateByValue(segments.conversion_segment(0), "アボカド"));
}

TEST_F(DictionaryPredictorTest, MobileZeroQuery) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();
  MockAggregator *aggregator = data_and_predictor->mutable_aggregator();

  EXPECT_CALL(*aggregator, AggregateResults(_, _))
      .WillOnce(Return(std::vector<Result>{
          CreateResult5("だいがく", "大学", 500, prediction::BIGRAM,
                        Token::NONE),
          CreateResult5("だいがくいん", "大学院", 600, prediction::BIGRAM,
                        Token::NONE),
          CreateResult5("だいがくせい", "大学生", 600, prediction::BIGRAM,
                        Token::NONE),
          CreateResult5("だいがくやきゅう", "大学野球", 1000,
                        prediction::BIGRAM, Token::NONE),
          CreateResult5("だいがくじゅけん", "大学受験", 1000,
                        prediction::BIGRAM, Token::NONE),
          CreateResult5("だいがくにゅうし", "大学入試", 1000,
                        prediction::BIGRAM, Token::NONE),
          CreateResult5("だいがくにゅうしせんたー", "大学入試センター", 2000,
                        prediction::BIGRAM, Token::NONE),
      }));

  Segments segments;
  InitSegmentsWithKey("", &segments);

  PrependHistorySegments("だいがく", "大学", &segments);

  commands::RequestForUnitTest::FillMobileRequest(request_.get());
  predictor.PredictForRequest(*convreq_for_prediction_, &segments);

  EXPECT_TRUE(FindCandidateByKeyValue(segments.conversion_segment(0),
                                      "にゅうし", "入試"));
  EXPECT_TRUE(FindCandidateByKeyValue(segments.conversion_segment(0),
                                      "にゅうしせんたー", "入試センター"));
}

TEST_F(DictionaryPredictorTest, PredictivePenaltyForBigramResults) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();
  MockAggregator *aggregator = data_and_predictor->mutable_aggregator();

  EXPECT_CALL(*aggregator, AggregateResults(_, _))
      .WillOnce(Return(std::vector<Result>{
          CreateResult5("だいがくにゅうし", "大学入試", 3000,
                        prediction::BIGRAM, Token::NONE),
          CreateResult5("だいがくにゅうしせんたー", "大学入試センター", 4000,
                        prediction::BIGRAM, Token::NONE),
          CreateResult5("だいがくにゅうしせんたーしけんたいさく",
                        "大学入試センター試験対策", 5000, prediction::BIGRAM,
                        Token::NONE),
          CreateResult5("にゅうし", "乳歯", 2000, prediction::UNIGRAM,
                        Token::NONE)}));

  Segments segments;
  InitSegmentsWithKey("にゅうし", &segments);
  PrependHistorySegments("だいがく", "大学", &segments);

  commands::RequestForUnitTest::FillMobileRequest(request_.get());
  predictor.PredictForRequest(*convreq_for_prediction_, &segments);

  auto get_rank_by_value = [&](absl::string_view value) {
    const Segment &seg = segments.conversion_segment(0);
    for (int i = 0; i < seg.candidates_size(); ++i) {
      if (seg.candidate(i).value == value) {
        return i;
      }
    }
    return -1;
  };
  EXPECT_LT(get_rank_by_value("乳歯"),
            get_rank_by_value("入試センター試験対策"));
}

TEST_F(DictionaryPredictorTest, PropagateAttributes) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();
  MockAggregator *aggregator = data_and_predictor->mutable_aggregator();
  MockImmutableConverter *immutable_converter =
      data_and_predictor->mutable_immutable_converter();

  // Exact key will not be filtered in mobile request
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  // Small prefix penalty
  {
    Segments segments;
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->cost = 10;
    EXPECT_CALL(*immutable_converter, ConvertForRequest(_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  auto get_top_candidate = [&aggregator, &predictor, this](
                               const Result &aggregator_result,
                               PredictionTypes prediction_types,
                               Segment::Candidate *candidate) {
    EXPECT_CALL(*aggregator, AggregateResults(_, _))
        .WillOnce(Return(std::vector<Result>({aggregator_result})));
    Segments segments;
    InitSegmentsWithKey("てすと", &segments);
    if (!predictor.PredictForRequest(*convreq_for_prediction_, &segments) ||
        segments.conversion_segments_size() != 1 ||
        segments.conversion_segment(0).candidates_size() != 1) {
      return false;
    }
    *candidate = segments.conversion_segment(0).candidate(0);
    return true;
  };

  Segment::Candidate c;
  {
    // PREFIX: consumed_key_size
    Result result =
        CreateResult5("てす", "てす", 50, prediction::PREFIX, Token::NONE);
    result.consumed_key_size = Util::CharsLen("てす");

    EXPECT_TRUE(get_top_candidate(result, prediction::PREFIX, &c));
    EXPECT_EQ(c.value, "てす");
    EXPECT_EQ(c.attributes, Segment::Candidate::PARTIALLY_KEY_CONSUMED |
                                Segment::Candidate::AUTO_PARTIAL_SUGGESTION);
    EXPECT_EQ(c.consumed_key_size, 2);
  }
  {
    // REALTIME_TOP
    Result result = CreateResult5(
        "てすと", "リアルタイムトップ", 100,
        prediction::REALTIME_TOP | prediction::REALTIME, Token::NONE);

    Segment::Candidate c;
    EXPECT_TRUE(get_top_candidate(result, prediction::REALTIME_TOP, &c));
    EXPECT_EQ(c.value, "リアルタイムトップ");
    EXPECT_EQ(c.attributes, Segment::Candidate::REALTIME_CONVERSION |
                                Segment::Candidate::NO_VARIANTS_EXPANSION);
  }
  {
    // REALTIME: inner_segment_boundary
    Result result = CreateResult5("てすと", "リアルタイム", 100,
                                  prediction::REALTIME, Token::NONE);
    uint32_t encoded;
    Segment::Candidate::EncodeLengths(strlen("てす"), strlen("リアル"),
                                      strlen("て"), strlen("リア"), &encoded);
    result.inner_segment_boundary.push_back(encoded);
    Segment::Candidate::EncodeLengths(strlen("と"), strlen("タイム"),
                                      strlen("と"), strlen("タイム"), &encoded);
    result.inner_segment_boundary.push_back(encoded);

    EXPECT_TRUE(get_top_candidate(result, prediction::REALTIME, &c));
    EXPECT_EQ(c.value, "リアルタイム");
    EXPECT_EQ(c.attributes, Segment::Candidate::REALTIME_CONVERSION);
    EXPECT_EQ(c.inner_segment_boundary.size(), 2);
  }
  {
    // SPELLING_CORRECTION
    Result result =
        CreateResult5("てすと", "SPELLING_CORRECTION", 300, prediction::UNIGRAM,
                      Token::SPELLING_CORRECTION);

    EXPECT_TRUE(get_top_candidate(result, prediction::UNIGRAM, &c));
    EXPECT_EQ(c.value, "SPELLING_CORRECTION");
    EXPECT_EQ(c.attributes, Segment::Candidate::SPELLING_CORRECTION);
  }
  {
    // TYPING_CORRECTION
    Result result = CreateResult5("てすと", "TYPING_CORRECTION", 300,
                                  prediction::TYPING_CORRECTION, Token::NONE);

    EXPECT_TRUE(get_top_candidate(result, prediction::TYPING_CORRECTION, &c));
    EXPECT_EQ(c.value, "TYPING_CORRECTION");
    EXPECT_EQ(c.attributes, Segment::Candidate::TYPING_CORRECTION);
  }
  {
    // USER_DICTIONARY
    Result result = CreateResult5("てすと", "ユーザー辞書", 300,
                                  prediction::UNIGRAM, Token::USER_DICTIONARY);

    EXPECT_TRUE(get_top_candidate(result, prediction::UNIGRAM, &c));
    EXPECT_EQ(c.value, "ユーザー辞書");
    EXPECT_EQ(c.attributes, Segment::Candidate::USER_DICTIONARY |
                                Segment::Candidate::NO_MODIFICATION |
                                Segment::Candidate::NO_VARIANTS_EXPANSION);
  }
  {
    // removed
    Result result = CreateResult5("てすと", "REMOVED", 300, prediction::BIGRAM,
                                  Token::NONE);
    result.removed = true;

    EXPECT_FALSE(get_top_candidate(result, prediction::UNIGRAM, &c));
  }
}

TEST_F(DictionaryPredictorTest, SetDebugDescription) {
  {
    Segment::Candidate candidate;
    const PredictionTypes types = prediction::UNIGRAM | prediction::ENGLISH;
    DictionaryPredictorTestPeer::SetDebugDescription(types, &candidate);
    EXPECT_EQ(candidate.description, "UE");
  }
  {
    Segment::Candidate candidate;
    candidate.description = "description";
    const PredictionTypes types = prediction::REALTIME | prediction::BIGRAM;
    DictionaryPredictorTestPeer::SetDebugDescription(types, &candidate);
    EXPECT_EQ(candidate.description, "description BR");
  }
  {
    Segment::Candidate candidate;
    const PredictionTypes types =
        prediction::BIGRAM | prediction::REALTIME | prediction::SUFFIX;
    DictionaryPredictorTestPeer::SetDebugDescription(types, &candidate);
    EXPECT_EQ(candidate.description, "BRS");
  }
}

TEST_F(DictionaryPredictorTest, MergeAttributesForDebug) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();

  std::vector<Result> results = {
      CreateResult4("a0", "A0", prediction::REALTIME, Token::NONE),
      CreateResult4("a1", "A1", prediction::REALTIME, Token::NONE),
      CreateResult4("a2", "A2", prediction::REALTIME, Token::NONE),
      CreateResult4("a3", "A3", prediction::REALTIME, Token::NONE),
      CreateResult4("a0", "A0", prediction::SUFFIX, Token::NONE),
      CreateResult4("a1", "A1", prediction::SUFFIX, Token::NONE),
      CreateResult4("a2", "A2", prediction::SUFFIX, Token::NONE),
      CreateResult4("a3", "A3", prediction::SUFFIX, Token::NONE),
  };

  absl::BitGen urbg;
  std::shuffle(results.begin(), results.end(), urbg);

  Segments segments;
  InitSegmentsWithKey("test", &segments);

  // Enables debug mode.
  config_->set_verbose_level(1);
  predictor.AddPredictionToCandidates(*convreq_for_suggestion_, &segments,
                                      absl::MakeSpan(results));

  EXPECT_EQ(segments.conversion_segments_size(), 1);
  const Segment &segment = segments.conversion_segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    EXPECT_EQ(segment.candidate(i).description, "RS");
  }
}

TEST_F(DictionaryPredictorTest, SetDescription) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();

  std::vector<Result> results = {
      CreateResult6("ほせい", "補正", 0, 0, prediction::TYPING_CORRECTION,
                    Token::NONE),
      CreateResult6("あ", "亞", 0, 10, prediction::UNIGRAM, Token::NONE),
      CreateResult6("たんご", "単語", 0, 20, prediction::UNIGRAM, Token::NONE),
  };

  Segments segments;
  InitSegmentsWithKey("test", &segments);

  predictor.AddPredictionToCandidates(*convreq_for_prediction_, &segments,
                                      absl::MakeSpan(results));

  EXPECT_EQ(segments.conversion_segments_size(), 1);
  const Segment &segment = segments.conversion_segment(0);
  EXPECT_EQ(segment.candidates_size(), 3);
  EXPECT_EQ(segment.candidate(0).value, "補正");
  EXPECT_EQ(segment.candidate(1).value, "亞");
  // "亜の旧字体"
  // We cannot compare the description as-is, since the other description
  // may be appended in the dbg build.
  EXPECT_TRUE(absl::StrContains(segment.candidate(1).description, "の"));
  EXPECT_EQ(segment.candidate(2).value, "単語");
  EXPECT_FALSE(absl::StrContains(segment.candidate(2).description, "の"));
}

TEST_F(DictionaryPredictorTest, PropagateResultCosts) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();

  constexpr int kTestSize = 20;
  std::vector<Result> results(kTestSize);
  for (size_t i = 0; i < kTestSize; ++i) {
    Result *result = &results[i];
    result->key = std::string(1, 'a' + i);
    result->value = std::string(1, 'A' + i);
    result->wcost = i;
    result->cost = i + 1000;
    result->SetTypesAndTokenAttributes(prediction::REALTIME, Token::NONE);
  }
  absl::BitGen urbg;
  std::shuffle(results.begin(), results.end(), urbg);

  Segments segments;
  InitSegmentsWithKey("test", &segments);
  convreq_for_suggestion_->set_max_dictionary_prediction_candidates_size(
      kTestSize);

  predictor.AddPredictionToCandidates(*convreq_for_suggestion_, &segments,
                                      absl::MakeSpan(results));

  EXPECT_EQ(segments.conversion_segments_size(), 1);
  ASSERT_EQ(kTestSize, segments.conversion_segment(0).candidates_size());
  const Segment &segment = segments.conversion_segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    EXPECT_EQ(segment.candidate(i).cost, i + 1000);
  }
}

TEST_F(DictionaryPredictorTest, PredictNCandidates) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();

  constexpr int kTotalCandidateSize = 100;
  constexpr int kLowCostCandidateSize = 5;
  std::vector<Result> results(kTotalCandidateSize);
  for (size_t i = 0; i < kTotalCandidateSize; ++i) {
    Result *result = &results[i];
    result->key = std::string(1, 'a' + i);
    result->value = std::string(1, 'A' + i);
    result->wcost = i;
    result->SetTypesAndTokenAttributes(prediction::REALTIME, Token::NONE);
    if (i < kLowCostCandidateSize) {
      result->cost = i + 1000;
    } else {
      result->cost = i + kInfinity;
    }
  }
  absl::BitGen urbg;
  std::shuffle(results.begin(), results.end(), urbg);

  Segments segments;
  InitSegmentsWithKey("test", &segments);
  convreq_for_suggestion_->set_max_dictionary_prediction_candidates_size(
      kLowCostCandidateSize + 1);

  predictor.AddPredictionToCandidates(*convreq_for_suggestion_, &segments,
                                      absl::MakeSpan(results));

  ASSERT_EQ(1, segments.conversion_segments_size());
  ASSERT_EQ(kLowCostCandidateSize,
            segments.conversion_segment(0).candidates_size());
  const Segment &segment = segments.conversion_segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    EXPECT_EQ(segment.candidate(i).cost, i + 1000);
  }
}

TEST_F(DictionaryPredictorTest, SuggestFilteredwordForExactMatchOnMobile) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();
  // turn on mobile mode
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  {
    MockAggregator *aggregator = data_and_predictor->mutable_aggregator();

    EXPECT_CALL(*aggregator, AggregateResults(_, _))
        .WillRepeatedly(Return(std::vector<Result>{
            CreateResult5("ふぃるたーたいしょう", "フィルター対象", 100,
                          prediction::UNIGRAM, Token::NONE),
            CreateResult5("ふぃるたーたいしょう", "フィルター大将", 200,
                          prediction::UNIGRAM, Token::NONE),
        }));
  }

  Segments segments;
  // Note: The suggestion filter entry "フィルター" for test is not
  // appropriate here, as Katakana entry will be added by realtime
  // conversion. Here, we want to confirm the behavior including unigram
  // prediction.
  InitSegmentsWithKey("ふぃるたーたいしょう", &segments);

  EXPECT_TRUE(predictor.PredictForRequest(*convreq_for_suggestion_, &segments));
  EXPECT_TRUE(
      FindCandidateByValue(segments.conversion_segment(0), "フィルター対象"));
  EXPECT_TRUE(
      FindCandidateByValue(segments.conversion_segment(0), "フィルター大将"));

  // However, filtered word should not be the top.
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).value,
            "フィルター大将");

  // Should not be there for non-exact suggestion.
  InitSegmentsWithKey("ふぃるたーたいし", &segments);
  EXPECT_TRUE(predictor.PredictForRequest(*convreq_for_suggestion_, &segments));
  EXPECT_FALSE(
      FindCandidateByValue(segments.conversion_segment(0), "フィルター対象"));
}

TEST_F(DictionaryPredictorTest, SuppressFilteredwordForExactMatch) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();

  {
    MockAggregator *aggregator = data_and_predictor->mutable_aggregator();

    EXPECT_CALL(*aggregator, AggregateResults(_, _))
        .WillRepeatedly(Return(std::vector<Result>{
            CreateResult5("ふぃるたーたいしょう", "フィルター対象", 100,
                          prediction::UNIGRAM, Token::NONE),
            CreateResult5("ふぃるたーたいしょう", "フィルター大将", 200,
                          prediction::UNIGRAM, Token::NONE),
        }));
  }

  Segments segments;
  // Note: The suggestion filter entry "フィルター" for test is not
  // appropriate here, as Katakana entry will be added by realtime
  // conversion. Here, we want to confirm the behavior including unigram
  // prediction.
  InitSegmentsWithKey("ふぃるたーたいしょう", &segments);

  EXPECT_TRUE(predictor.PredictForRequest(*convreq_for_suggestion_, &segments));
  EXPECT_FALSE(
      FindCandidateByValue(segments.conversion_segment(0), "フィルター対象"));
}

TEST_F(DictionaryPredictorTest, DoNotFilterExactUnigramOnMobile) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  {
    MockAggregator *aggregator = data_and_predictor->mutable_aggregator();

    std::vector<Result> results;
    for (int i = 0; i < 30; ++i) {
      // Exact entries
      results.push_back(CreateResult5("てすと", absl::StrCat(i, "テストE"),
                                      5000 + i, prediction::UNIGRAM,
                                      Token::NONE));
      // Predictive entries
      results.push_back(CreateResult5("てすとて", absl::StrCat(i, "テストP"),
                                      100 + i, prediction::UNIGRAM,
                                      Token::NONE));
    }

    EXPECT_CALL(*aggregator, AggregateResults(_, _)).WillOnce(Return(results));
  }

  Segments segments;
  InitSegmentsWithKey("てすと", &segments);

  convreq_for_prediction_->set_max_dictionary_prediction_candidates_size(100);
  EXPECT_TRUE(predictor.PredictForRequest(*convreq_for_prediction_, &segments));
  int exact_count = 0;
  for (int i = 0; i < segments.segment(0).candidates_size(); ++i) {
    const auto candidate = segments.segment(0).candidate(i);
    if (absl::StrContains(candidate.value, "テストE")) {
      exact_count++;
    }
  }
  EXPECT_EQ(exact_count, 30);
}

TEST_F(DictionaryPredictorTest, DoNotFilterZeroQueryCandidatesOnMobile) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  {
    MockAggregator *aggregator = data_and_predictor->mutable_aggregator();

    // Entries for zero query
    std::vector<Result> results;
    for (int i = 0; i < 10; ++i) {
      results.push_back(CreateResult5("てすと", absl::StrCat(i, "テストS"), 100,
                                      prediction::SUFFIX, Token::NONE));
    }

    EXPECT_CALL(*aggregator, AggregateResults(_, _))
        .WillRepeatedly(Return(results));
  }

  Segments segments;
  InitSegmentsWithKey("", &segments);
  PrependHistorySegments("わたし", "私", &segments);
  EXPECT_TRUE(predictor.PredictForRequest(*convreq_for_prediction_, &segments));
  EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 10);
}

TEST_F(DictionaryPredictorTest,
       DoNotFilterOneSegmentRealtimeCandidatesOnMobile) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();
  // turn on mobile mode
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  {
    MockAggregator *aggregator = data_and_predictor->mutable_aggregator();
    std::vector<Result> results;
    results.push_back(CreateResult5(
        "かった", "買った", 300,
        prediction::REALTIME_TOP | prediction::REALTIME, Token::NONE));
    PushBackInnerSegmentBoundary(9, 9, 9, 9, &results.back());
    results.push_back(CreateResult5("かった", "飼った", 1000,
                                    prediction::REALTIME, Token::NONE));
    PushBackInnerSegmentBoundary(9, 9, 9, 9, &results.back());
    results.push_back(CreateResult5("かつた", "勝田", 1001,
                                    prediction::REALTIME, Token::NONE));
    PushBackInnerSegmentBoundary(9, 6, 9, 6, &results.back());
    results.push_back(CreateResult5("かつた", "勝太", 1002,
                                    prediction::REALTIME, Token::NONE));
    PushBackInnerSegmentBoundary(9, 6, 9, 6, &results.back());
    results.push_back(CreateResult5("かつた", "鹿田", 1003,
                                    prediction::REALTIME, Token::NONE));
    PushBackInnerSegmentBoundary(9, 6, 9, 6, &results.back());
    results.push_back(CreateResult5("かつた", "かつた", 1004,
                                    prediction::REALTIME, Token::NONE));
    PushBackInnerSegmentBoundary(9, 9, 9, 9, &results.back());
    results.push_back(CreateResult5("かった", "刈った", 1005,
                                    prediction::REALTIME, Token::NONE));
    PushBackInnerSegmentBoundary(9, 9, 9, 9, &results.back());
    results.push_back(CreateResult5("かった", "勝った", 1006,
                                    prediction::REALTIME, Token::NONE));
    PushBackInnerSegmentBoundary(9, 9, 9, 9, &results.back());

    EXPECT_CALL(*aggregator, AggregateResults(_, _))
        .WillRepeatedly(Return(results));
  }

  Segments segments;
  InitSegmentsWithKey("かつた", &segments);
  EXPECT_TRUE(predictor.PredictForRequest(*convreq_for_prediction_, &segments));
  EXPECT_GE(segments.conversion_segment(0).candidates_size(), 8);
}

TEST_F(DictionaryPredictorTest, FixSRealtimeTopCandidatesCostOnMobile) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();
  // turn on mobile mode
  commands::RequestForUnitTest::FillMobileRequest(request_.get());
  request_->mutable_decoder_experiment_params()
      ->set_apply_user_segment_history_rewriter_for_prediction(true);

  {
    MockAggregator *aggregator = data_and_predictor->mutable_aggregator();
    std::vector<Result> results;
    results.push_back(CreateResult5(
        "かった", "買った", 1002,
        prediction::REALTIME_TOP | prediction::REALTIME, Token::NONE));
    PushBackInnerSegmentBoundary(9, 9, 9, 9, &results.back());
    results.push_back(CreateResult5("かった", "飼った", 1000,
                                    prediction::REALTIME, Token::NONE));
    PushBackInnerSegmentBoundary(9, 9, 9, 9, &results.back());
    results.push_back(CreateResult5("かつた", "勝田", 1001,
                                    prediction::REALTIME, Token::NONE));
    PushBackInnerSegmentBoundary(9, 6, 9, 6, &results.back());
    EXPECT_CALL(*aggregator, AggregateResults(_, _))
        .WillRepeatedly(Return(results));
  }

  Segments segments;
  InitSegmentsWithKey("かった", &segments);
  EXPECT_TRUE(predictor.PredictForRequest(*convreq_for_prediction_, &segments));
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "買った");
}

TEST_F(DictionaryPredictorTest, SingleKanjiCost) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();
  // turn on mobile mode
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  {
    MockAggregator *aggregator = data_and_predictor->mutable_aggregator();
    std::vector<Result> results;
    results.push_back(
        CreateResult5("さか", "坂", 400, prediction::REALTIME, Token::NONE));
    results.push_back(
        CreateResult5("さが", "佐賀", 500, prediction::REALTIME, Token::NONE));
    results.push_back(
        CreateResult5("さか", "咲か", 2000, prediction::UNIGRAM, Token::NONE));
    results.push_back(
        CreateResult5("さか", "阪", 2500, prediction::UNIGRAM, Token::NONE));
    results.push_back(
        CreateResult5("さか", "サカ", 10000, prediction::UNIGRAM, Token::NONE));
    results.push_back(
        CreateResult5("さがす", "探す", 300, prediction::UNIGRAM, Token::NONE));
    results.push_back(CreateResult5("さがし", "探し", 3000, prediction::UNIGRAM,
                                    Token::NONE));
    results.push_back(
        CreateResult5("さかい", "堺", 800, prediction::UNIGRAM, Token::NONE));
    results.push_back(
        CreateResult5("さか", "坂", 9000, prediction::UNIGRAM, Token::NONE));
    results.push_back(
        CreateResult5("さか", "逆", 0, prediction::SINGLE_KANJI, Token::NONE));
    results.push_back(
        CreateResult5("さか", "坂", 1, prediction::SINGLE_KANJI, Token::NONE));
    results.push_back(
        CreateResult5("さか", "酒", 2, prediction::SINGLE_KANJI, Token::NONE));
    results.push_back(
        CreateResult5("さか", "栄", 3, prediction::SINGLE_KANJI, Token::NONE));
    results.push_back(
        CreateResult5("さか", "盛", 4, prediction::SINGLE_KANJI, Token::NONE));
    results.push_back(
        CreateResult5("さ", "差", 1000, prediction::SINGLE_KANJI, Token::NONE));
    results.push_back(
        CreateResult5("さ", "佐", 1001, prediction::SINGLE_KANJI, Token::NONE));
    for (int i = 0; i < results.size(); ++i) {
      if (results[i].types == prediction::SINGLE_KANJI) {
        results[i].lid = data_and_predictor->pos_matcher().GetGeneralSymbolId();
        results[i].rid = data_and_predictor->pos_matcher().GetGeneralSymbolId();
      } else {
        results[i].lid = data_and_predictor->pos_matcher().GetGeneralNounId();
        results[i].rid = data_and_predictor->pos_matcher().GetGeneralNounId();
      }
    }

    EXPECT_CALL(*aggregator, AggregateResults(_, _))
        .WillRepeatedly(Return(results));
  }

  Segments segments;
  auto get_rank_by_value = [&](absl::string_view value) {
    const Segment &seg = segments.conversion_segment(0);
    for (int i = 0; i < seg.candidates_size(); ++i) {
      if (seg.candidate(i).value == value) {
        return i;
      }
    }
    return -1;
  };

  {
    InitSegmentsWithKey("さか", &segments);
    EXPECT_TRUE(
        predictor.PredictForRequest(*convreq_for_prediction_, &segments));
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_NE(get_rank_by_value("佐"), -1);
    EXPECT_LT(get_rank_by_value("佐"),
              segments.conversion_segment(0).candidates_size() - 1);
    EXPECT_LT(get_rank_by_value("坂"), get_rank_by_value("逆"));
    EXPECT_LT(get_rank_by_value("咲か"), get_rank_by_value("逆"));
    EXPECT_LT(get_rank_by_value("阪"), get_rank_by_value("逆"));
    EXPECT_LT(get_rank_by_value("逆"), get_rank_by_value("差"));
  }
}

TEST_F(DictionaryPredictorTest, SingleKanjiFallbackOffsetCost) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();
  // turn on mobile mode
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  {
    MockAggregator *aggregator = data_and_predictor->mutable_aggregator();
    std::vector<Result> results;
    results.push_back(
        CreateResult5("ああ", "ああ", 5000, prediction::UNIGRAM, Token::NONE));
    results.push_back(
        CreateResult5("ああ", "アア", 4500, prediction::UNIGRAM, Token::NONE));
    results.push_back(
        CreateResult5("ああ", "吁", 0, prediction::SINGLE_KANJI, Token::NONE));
    results.push_back(
        CreateResult5("ああ", "咨", 1, prediction::SINGLE_KANJI, Token::NONE));
    results.push_back(
        CreateResult5("ああ", "噫", 2, prediction::SINGLE_KANJI, Token::NONE));
    results.push_back(
        CreateResult5("あ", "亜", 1000, prediction::SINGLE_KANJI, Token::NONE));
    results.push_back(
        CreateResult5("あ", "亞", 1001, prediction::SINGLE_KANJI, Token::NONE));
    for (int i = 0; i < results.size(); ++i) {
      if (results[i].types == prediction::SINGLE_KANJI) {
        results[i].lid = data_and_predictor->pos_matcher().GetGeneralSymbolId();
        results[i].rid = data_and_predictor->pos_matcher().GetGeneralSymbolId();
      } else {
        results[i].lid = data_and_predictor->pos_matcher().GetGeneralNounId();
        results[i].rid = data_and_predictor->pos_matcher().GetGeneralNounId();
      }
    }

    EXPECT_CALL(*aggregator, AggregateResults(_, _))
        .WillRepeatedly(Return(results));
  }

  Segments segments;
  InitSegmentsWithKey("ああ", &segments);
  EXPECT_TRUE(predictor.PredictForRequest(*convreq_for_prediction_, &segments));
  EXPECT_EQ(segments.conversion_segments_size(), 1);
  ASSERT_EQ(segments.conversion_segment(0).candidates_size(), 7);
  ASSERT_EQ(segments.conversion_segment(0).candidate(0).value, "アア");
  ASSERT_EQ(segments.conversion_segment(0).candidate(1).value, "ああ");
}

TEST_F(DictionaryPredictorTest, Dedup) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();
  // turn on mobile mode
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  {
    constexpr int kSize = 5;
    std::vector<Result> results;
    for (int i = 0; i < kSize; ++i) {
      results.push_back(CreateResult6("test", absl::StrCat("value", i), 0, i,
                                      prediction::REALTIME, Token::NONE));
      results.push_back(CreateResult6("test", absl::StrCat("value", i), 0,
                                      kSize + i, prediction::PREFIX,
                                      Token::NONE));
      results.push_back(
          CreateResult6("test", absl::StrCat("value", i), 0, 2 * kSize + i,
                        prediction::TYPING_CORRECTION, Token::NONE));
      results.push_back(CreateResult6("test", absl::StrCat("value", i), 0,
                                      3 * kSize + i, prediction::UNIGRAM,
                                      Token::NONE));
    }

    Segments segments;
    InitSegmentsWithKey("test", &segments);
    predictor.AddPredictionToCandidates(*convreq_for_prediction_, &segments,
                                        absl::MakeSpan(results));

    ASSERT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidates_size(), kSize);
  }
  {
    request_->mutable_decoder_experiment_params()
        ->set_typing_correction_max_count(5);
    std::vector<Result> results = {
        CreateResult6("test", "value0", 0, 0, prediction::TYPING_CORRECTION,
                      Token::NONE),
        CreateResult6("test", "value0", 0, 1, prediction::TYPING_CORRECTION,
                      Token::NONE),
        CreateResult6("test", "value0", 0, 2, prediction::TYPING_CORRECTION,
                      Token::NONE),
        CreateResult6("test", "value0", 0, 3, prediction::TYPING_CORRECTION,
                      Token::NONE),
        CreateResult6("test", "value0", 0, 4, prediction::TYPING_CORRECTION,
                      Token::NONE),
        CreateResult6("test", "value1", 0, 5, prediction::TYPING_CORRECTION,
                      Token::NONE),
        CreateResult6("test", "value2", 0, 6, prediction::TYPING_CORRECTION,
                      Token::NONE),
    };
    Segments segments;
    InitSegmentsWithKey("test", &segments);
    predictor.AddPredictionToCandidates(*convreq_for_prediction_, &segments,
                                        absl::MakeSpan(results));

    ASSERT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 3);
  }
}

TEST_F(DictionaryPredictorTest, TypingCorrectionResultsLimit) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();
  // turn on mobile mode
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  request_->mutable_decoder_experiment_params()
      ->set_typing_correction_max_count(5);
  std::vector<Result> results = {
      CreateResult6("tc_key0", "tc_value0", 0, 0, prediction::TYPING_CORRECTION,
                    Token::NONE),
      CreateResult6("tc_key0", "tc_value1", 0, 1, prediction::TYPING_CORRECTION,
                    Token::NONE),
      CreateResult6("tc_key0", "tc_value2", 0, 2, prediction::TYPING_CORRECTION,
                    Token::NONE),
      CreateResult6("tc_key1", "tc_value3", 0, 3, prediction::TYPING_CORRECTION,
                    Token::NONE),
      CreateResult6("tc_key1", "tc_value4", 0, 4, prediction::TYPING_CORRECTION,
                    Token::NONE),
      CreateResult6("tc_key1", "tc_value5", 0, 5, prediction::TYPING_CORRECTION,
                    Token::NONE),
      CreateResult6("tc_key1", "tc_value6", 0, 6, prediction::TYPING_CORRECTION,
                    Token::NONE),
  };
  for (auto &result : results) {
    result.non_expanded_original_key = result.key;
  }

  Segments segments;
  InitSegmentsWithKey("original_key", &segments);
  predictor.AddPredictionToCandidates(*convreq_for_prediction_, &segments,
                                      absl::MakeSpan(results));
  ASSERT_EQ(segments.conversion_segments_size(), 1);
  const Segment segment = segments.conversion_segment(0);
  EXPECT_EQ(segment.candidates_size(), 5);
  EXPECT_TRUE(FindCandidateByValue(segment, "tc_value0"));
  EXPECT_TRUE(FindCandidateByValue(segment, "tc_value1"));
  EXPECT_TRUE(FindCandidateByValue(segment, "tc_value3"));
  EXPECT_TRUE(FindCandidateByValue(segment, "tc_value4"));
}

TEST_F(DictionaryPredictorTest, SortResult) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();
  // turn on mobile mode
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  std::vector<Result> results = {
      CreateResult6("test", "テストＡ", 0, 10, prediction::UNIGRAM,
                    Token::NONE),
      CreateResult6("test", "テストＢ", 0, 100, prediction::UNIGRAM,
                    Token::NONE),
      CreateResult6("test", "テスト０００", 0, 1, prediction::UNIGRAM,
                    Token::NONE),
      CreateResult6("test", "テスト００", 0, 1, prediction::UNIGRAM,
                    Token::NONE),
      CreateResult6("test", "テスト１０", 0, 1, prediction::UNIGRAM,
                    Token::NONE),
      CreateResult6("test", "テスト０", 0, 1, prediction::UNIGRAM, Token::NONE),
      CreateResult6("test", "テスト１", 0, 1, prediction::UNIGRAM, Token::NONE),
  };
  Segments segments;
  InitSegmentsWithKey("test", &segments);
  predictor.AddPredictionToCandidates(*convreq_for_prediction_, &segments,
                                      absl::MakeSpan(results));

  ASSERT_EQ(segments.conversion_segments_size(), 1);
  const Segment &segment = segments.conversion_segment(0);
  ASSERT_EQ(segment.candidates_size(), 7);
  ASSERT_EQ(segment.candidate(0).value, "テスト０");      // cost:1
  ASSERT_EQ(segment.candidate(1).value, "テスト１");      // cost:1
  ASSERT_EQ(segment.candidate(2).value, "テスト００");    // cost:1
  ASSERT_EQ(segment.candidate(3).value, "テスト１０");    // cost:1
  ASSERT_EQ(segment.candidate(4).value, "テスト０００");  // cost:1
  ASSERT_EQ(segment.candidate(5).value, "テストＡ");      // cost:10
  ASSERT_EQ(segment.candidate(6).value, "テストＢ");      // cost:100
}

TEST_F(DictionaryPredictorTest, SetCostForRealtimeTopCandidate) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();

  {
    MockAggregator *aggregator = data_and_predictor->mutable_aggregator();

    EXPECT_CALL(*aggregator, AggregateResults(_, _))
        .WillOnce(Return(std::vector<Result>{
            CreateResult5("あいう", "会いう", 100,
                          prediction::REALTIME_TOP | prediction::REALTIME,
                          Token::NONE),
            CreateResult5("あいうえ", "会いうえ", 1000, prediction::REALTIME,
                          Token::NONE)}));
  }

  Segments segments;
  request_->set_mixed_conversion(false);
  convreq_for_suggestion_->set_use_actual_converter_for_realtime_conversion(
      true);
  InitSegmentsWithKey("あいう", &segments);

  EXPECT_TRUE(predictor.PredictForRequest(*convreq_for_suggestion_, &segments));
  EXPECT_EQ(segments.segments_size(), 1);
  EXPECT_EQ(segments.segment(0).candidates_size(), 2);
  EXPECT_EQ(segments.segment(0).candidate(0).value, "会いう");
}

TEST_F(DictionaryPredictorTest, UsageStats) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  DictionaryPredictorTestPeer *predictor =
      data_and_predictor->mutable_predictor();

  Segments segments;
  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeNone", 0);
  SetSegmentForCommit(
      "★", Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_NONE, &segments);
  predictor->Finish(*convreq_for_suggestion_, &segments);
  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeNone", 1);

  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeNumberSuffix", 0);
  SetSegmentForCommit(
      "個", Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_NUMBER_SUFFIX,
      &segments);
  predictor->Finish(*convreq_for_suggestion_, &segments);
  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeNumberSuffix", 1);

  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeEmoticon", 0);
  SetSegmentForCommit(
      "＼(^o^)／", Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_EMOTICON,
      &segments);
  predictor->Finish(*convreq_for_suggestion_, &segments);
  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeEmoticon", 1);

  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeEmoji", 0);
  SetSegmentForCommit("❕",
                      Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_EMOJI,
                      &segments);
  predictor->Finish(*convreq_for_suggestion_, &segments);
  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeEmoji", 1);

  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeBigram", 0);
  SetSegmentForCommit(
      "ヒルズ", Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_BIGRAM,
      &segments);
  predictor->Finish(*convreq_for_suggestion_, &segments);
  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeBigram", 1);

  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeSuffix", 0);
  SetSegmentForCommit(
      "が", Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX,
      &segments);
  predictor->Finish(*convreq_for_suggestion_, &segments);
  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeSuffix", 1);
}

TEST_F(DictionaryPredictorTest, InvalidPrefixCandidate) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData();
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();
  MockAggregator *aggregator = data_and_predictor->mutable_aggregator();
  MockImmutableConverter *immutable_converter =
      data_and_predictor->mutable_immutable_converter();

  // Exact key will not be filtered in mobile request
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  {
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("ーひー");
    // Dummy candidate
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = "ーひー";
    candidate->key = "ーひー";
    candidate->cost = 0;
    EXPECT_CALL(*immutable_converter, ConvertForRequest(_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  {
    EXPECT_CALL(*aggregator, AggregateResults(_, _))
        .WillRepeatedly(Return(std::vector<Result>{
            CreateResult6("こ", "子", 0, 10, prediction::PREFIX, Token::NONE),
            CreateResult6("こーひー", "コーヒー", 0, 100, prediction::UNIGRAM,
                          Token::NONE),
            CreateResult6("こーひー", "珈琲", 0, 200, prediction::UNIGRAM,
                          Token::NONE),
            CreateResult6("こーひー", "coffee", 0, 300, prediction::UNIGRAM,
                          Token::NONE)}));
  }

  Segments segments;
  InitSegmentsWithKey("こーひー", &segments);
  EXPECT_TRUE(predictor.PredictForRequest(*convreq_for_prediction_, &segments));
  EXPECT_FALSE(FindCandidateByValue(segments.conversion_segment(0), "子"));
}

TEST_F(DictionaryPredictorTest, MaybeSuppressAggressiveTypingCorrectionTest) {
  Segments segments;
  InitSegmentsWithKey("key", &segments);

  Segment *segment = segments.mutable_conversion_segment(0);
  for (int i = 0; i < 10; ++i) {
    auto *candidate = segment->add_candidate();
    candidate->key = absl::StrCat("key_", i);
    candidate->value = absl::StrCat("value_", i);
  }

  auto get_top_value = [&segments]() {
    return segments.conversion_segment(0).candidate(0).value;
  };

  DictionaryPredictorTestPeer::MaybeSuppressAggressiveTypingCorrection(
      *convreq_for_suggestion_, &segments);

  // Top is still literal
  for (int i = 1; i <= 2; ++i) {
    segment->mutable_candidate(i)->attributes |=
        Segment::Candidate::TYPING_CORRECTION;
  }

  segment->mutable_candidate(0)->cost = 100;
  segment->mutable_candidate(3)->cost = 500;

  DictionaryPredictorTestPeer::MaybeSuppressAggressiveTypingCorrection(
      *convreq_for_suggestion_, &segments);
  EXPECT_EQ(get_top_value(), "value_0");

  // Top is TYPING CORRECTION, but
  // `typing_correction_conversion_cost_max_diff` is 0.
  segment->mutable_candidate(0)->attributes |=
      Segment::Candidate::TYPING_CORRECTION;
  DictionaryPredictorTestPeer::MaybeSuppressAggressiveTypingCorrection(
      *convreq_for_suggestion_, &segments);
  EXPECT_EQ(get_top_value(), "value_0");

  // `typing_correction_conversion_cost_max_diff` is 100.
  // Do not move because 400 > 100.
  request_->mutable_decoder_experiment_params()
      ->set_typing_correction_conversion_cost_max_diff(100);
  DictionaryPredictorTestPeer::MaybeSuppressAggressiveTypingCorrection(
      *convreq_for_suggestion_, &segments);
  EXPECT_EQ(get_top_value(), "value_0");

  // `typing_correction_conversion_cost_max_diff` is 1000;
  // Move because 400 < 1000.
  request_->mutable_decoder_experiment_params()
      ->set_typing_correction_conversion_cost_max_diff(1000);
  DictionaryPredictorTestPeer::MaybeSuppressAggressiveTypingCorrection(
      *convreq_for_suggestion_, &segments);
  EXPECT_EQ(get_top_value(), "value_3");

  request_->mutable_decoder_experiment_params()
      ->set_typing_correction_conversion_cost_max_diff(0);
}

TEST_F(DictionaryPredictorTest, Rescoring) {
  prediction::MockRescorer rescorer;
  EXPECT_CALL(rescorer, RescoreResults(_, _, _))
      .WillRepeatedly(
          Invoke([](const ConversionRequest &request, absl::string_view history,
                    absl::Span<Result> results) {
            for (Result &r : results) r.cost = 100;
          }));

  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreateDictionaryPredictorWithMockData(&rescorer);
  const DictionaryPredictorTestPeer &predictor =
      data_and_predictor->predictor();
  MockAggregator *aggregator = data_and_predictor->mutable_aggregator();
  EXPECT_CALL(*aggregator, AggregateResults(_, _))
      .WillOnce(Return(std::vector<Result>{
          CreateResult5("こーひー", "コーヒー", 500, prediction::UNIGRAM,
                        Token::NONE),
          CreateResult5("こーひー", "珈琲", 600, prediction::UNIGRAM,
                        Token::NONE),
          CreateResult5("こーひー", "coffee", 700, prediction::UNIGRAM,
                        Token::NONE),
      }));

  Segments segments;
  InitSegmentsWithKey("こーひー", &segments);

  predictor.PredictForRequest(*convreq_for_prediction_, &segments);

  ASSERT_EQ(segments.conversion_segments_size(), 1);
  EXPECT_THAT(segments.conversion_segment(0),
              CandidatesAreArray({
                  Field(&Segment::Candidate::cost, 100),
                  Field(&Segment::Candidate::cost, 100),
                  Field(&Segment::Candidate::cost, 100),
              }));
}

TEST_F(DictionaryPredictorTest, AddRescoringDebugDescription) {
  Segments segments;
  Segment *segment = segments.add_segment();

  Segment::Candidate *cand1 = segment->push_back_candidate();
  cand1->key = "Cand1";
  cand1->cost = 1000;
  cand1->cost_before_rescoring = 3000;

  Segment::Candidate *cand2 = segment->push_back_candidate();
  cand2->key = "Cand2";
  cand2->cost = 2000;
  cand2->cost_before_rescoring = 2000;

  Segment::Candidate *cand3 = segment->push_back_candidate();
  cand3->key = "Cand3";
  cand3->cost = 3000;
  cand3->cost_before_rescoring = 1000;

  DictionaryPredictorTestPeer::AddRescoringDebugDescription(&segments);

  EXPECT_EQ(cand1->description, "3→1");
  EXPECT_EQ(cand2->description, "2→2");
  EXPECT_EQ(cand3->description, "1→3");
}

}  // namespace
}  // namespace mozc::prediction
