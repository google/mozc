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
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/random.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/strings/assign.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/query.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/attribute.h"
#include "converter/connector.h"
#include "converter/inner_segment.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/single_kanji_dictionary.h"
#include "engine/modules.h"
#include "engine/supplemental_model_interface.h"
#include "engine/supplemental_model_mock.h"
#include "prediction/realtime_decoder.h"
#include "prediction/result.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "request/options.h"
#include "request/request_test_util.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "testing/test_peer.h"
#include "transliteration/transliteration.h"

namespace mozc::prediction {

using ::mozc::composer::TypeCorrectedQuery;
using ::mozc::converter::Attribute;
using ::mozc::dictionary::DictionaryInterface;
using ::mozc::dictionary::MockDictionary;
using ::mozc::dictionary::PosMatcher;
using ::mozc::dictionary::Token;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Field;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::Truly;
using ::testing::WithParamInterface;

class DictionaryPredictorTestPeer
    : public testing::TestPeer<DictionaryPredictor> {
 public:
  explicit DictionaryPredictorTestPeer(DictionaryPredictor& predictor)
      : testing::TestPeer<DictionaryPredictor>(predictor) {}

#define PEER_CONST_METHOD(func_name)                      \
  template <typename... Args>                             \
  auto func_name(Args&&... args) const {                  \
    return value_.func_name(std::forward<Args>(args)...); \
  }

  PEER_STATIC_METHOD(IsAggressiveSuggestion);
  PEER_STATIC_METHOD(RemoveMissSpelledCandidates);
  PEER_STATIC_METHOD(GetRealtimeCandidateMaxSize);
  PEER_CONST_METHOD(GetLMCost);
  PEER_CONST_METHOD(RerankAndFilterResults);
  PEER_CONST_METHOD(AggregateTypingCorrectedResultsForMixedConversion);
  PEER_CONST_METHOD(AggregateUnigram);
  PEER_CONST_METHOD(AggregateRealtime);
  PEER_CONST_METHOD(SetPredictionCostForMixedConversion);
  PEER_CONST_METHOD(MaybeGetPreviousTopResult);
  PEER_CONST_METHOD(MaybeApplyPostCorrection);

  std::vector<Result> AggregateResultsForTesting(
      const ConversionRequest& request) const {
    return request.request().mixed_conversion()
               ? value_.AggregateResultsForMixedConversion(request)
               : value_.AggregateResultsForDesktop(request);
  }

  void AggregateBigram(const ConversionRequest& request,
                       std::vector<Result>* results) const {
    value_.dictionary_decoder_.AggregateBigram(request, results);
  }
  void AggregateZeroQuery(const ConversionRequest& request,
                          std::vector<Result>* results) const {
    absl::c_move(value_.zero_query_decoder_.Decode(request),
                 std::back_inserter(*results));
  }
  void AggregateEnglish(const ConversionRequest& request,
                        std::vector<Result>* results) const {
    absl::c_move(value_.english_decoder_.Decode(request),
                 std::back_inserter(*results));
  }

#undef PEER_CONST_METHOD
};

class MockRealtimeDecoder : public RealtimeDecoder {
 public:
  ~MockRealtimeDecoder() override = default;

  MOCK_METHOD(std::vector<Result>, Decode, (const ConversionRequest& request),
              (const, override));
  MOCK_METHOD(std::vector<Result>, ReverseDecode,
              (const ConversionRequest& request), (const, override));

  static std::vector<Result> DecodeImpl(const ConversionRequest& request) {
    Result result;
    result.key = request.key();
    result.value = request.key();
    result.attributes = REALTIME;
    return {result};
  }
};

class MockSingleKanjiDictionary : public dictionary::SingleKanjiDictionary {
 public:
  ~MockSingleKanjiDictionary() override = default;
  MOCK_METHOD(std::vector<std::string>, LookupKanjiEntries,
              (absl::string_view key, bool use_svs), (const, override));
};

class MockAggregator : public DictionaryPredictor {
 public:
  using DictionaryPredictor::DictionaryPredictor;
  MOCK_METHOD(std::vector<prediction::Result>, AggregateResultsForDesktop,
              (const ConversionRequest& request), (const, override));
  MOCK_METHOD(std::vector<prediction::Result>,
              AggregateResultsForMixedConversion,
              (const ConversionRequest& request), (const, override));
};

// Helper class to hold dictionary data and predictor objects.
class MockDataAndPredictor {
 public:
  MockDataAndPredictor() : MockDataAndPredictor(nullptr) {}

  explicit MockDataAndPredictor(
      std::unique_ptr<engine::SupplementalModelInterface> supplemental_model) {
    mock_decoder_ = std::make_unique<MockRealtimeDecoder>();
    absl::StatusOr<std::unique_ptr<engine::Modules>> modules_status =
        engine::ModulesPresetBuilder()
            .PresetSupplementalModel(std::move(supplemental_model))
            .Build(std::make_unique<testing::MockDataManager>());
    CHECK_OK(modules_status.status());
    modules_ = *std::move(modules_status);
    auto mock_aggregator =
        std::make_unique<MockAggregator>(*modules_, *mock_decoder_);
    mock_aggregator_ = mock_aggregator.get();
    predictor_ = std::move(mock_aggregator);
    predictor_peer_ =
        std::make_unique<DictionaryPredictorTestPeer>(*predictor_);
  }

  // Initializes predictor with the given suffix_dictionary and
  // supplemental_model using the real aggregation pipeline.
  void Init(std::unique_ptr<DictionaryInterface> suffix_dictionary = nullptr,
            std::unique_ptr<engine::SupplementalModelInterface>
                supplemental_model = nullptr) {
    auto dictionary = std::make_unique<MockDictionary>();
    mock_dictionary_ = dictionary.get();

    auto data_manager = std::make_unique<testing::MockDataManager>();

    auto single_kanji_dictionary =
        std::make_unique<MockSingleKanjiDictionary>();
    single_kanji_dictionary_ = single_kanji_dictionary.get();

    mock_decoder_ = std::make_unique<MockRealtimeDecoder>();

    absl::StatusOr<std::unique_ptr<engine::Modules>> modules_status =
        engine::ModulesPresetBuilder()
            .PresetDictionary(std::move(dictionary))
            .PresetSingleKanjiDictionary(std::move(single_kanji_dictionary))
            .PresetSuffixDictionary(std::move(suffix_dictionary))    // nullable
            .PresetSupplementalModel(std::move(supplemental_model))  // nullable
            .Build(std::move(data_manager));
    CHECK_OK(modules_status.status());
    modules_ = *std::move(modules_status);

    mock_aggregator_ = nullptr;
    predictor_ =
        std::make_unique<DictionaryPredictor>(*modules_, *mock_decoder_);
    predictor_peer_ =
        std::make_unique<DictionaryPredictorTestPeer>(*predictor_);
  }

  MockAggregator* mutable_aggregator() { return mock_aggregator_; }
  MockDictionary* mutable_dictionary() { return mock_dictionary_; }
  MockRealtimeDecoder* mutable_realtime_decoder() {
    return mock_decoder_.get();
  }
  MockSingleKanjiDictionary* mutable_single_kanji_dictionary() {
    return single_kanji_dictionary_;
  }

  const Connector& connector() const { return modules_->GetConnector(); }
  const PosMatcher& pos_matcher() const { return modules_->GetPosMatcher(); }

  const DictionaryPredictor& predictor() const { return *predictor_; }
  DictionaryPredictor* mutable_predictor() { return predictor_.get(); }

  const DictionaryPredictorTestPeer& predictor_peer() const {
    return *predictor_peer_;
  }

 private:
  MockAggregator* mock_aggregator_ = nullptr;
  std::unique_ptr<MockRealtimeDecoder> mock_decoder_;
  std::unique_ptr<engine::Modules> modules_;
  MockDictionary* mock_dictionary_ = nullptr;
  MockSingleKanjiDictionary* single_kanji_dictionary_ = nullptr;
  std::unique_ptr<DictionaryPredictor> predictor_;
  std::unique_ptr<DictionaryPredictorTestPeer> predictor_peer_;
};

namespace {

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
                                  size_t content_value_len, Result* result) {
  result->inner_segment_boundary.push_back(
      converter::EncodeLengths(key_len, value_len, content_key_len,
                               content_value_len)
          .value());
}

bool FindCandidateByKeyValue(absl::Span<const Result> results,
                             absl::string_view key, absl::string_view value) {
  return absl::c_find_if(results, [&](const auto& result) {
           return (result.key == key && result.value == value &&
                   !result.removed);
         }) != results.end();
}

bool FindCandidateByValue(absl::Span<const Result> results,
                          absl::string_view value) {
  return absl::c_find_if(results, [&](const auto& result) {
           return (result.value == value && !result.removed);
         }) != results.end();
}

struct InvokeCallbackWithTokens {
  using Callback = DictionaryInterface::Callback;

  template <class T, class U>
  void operator()(T, U, Callback* callback) {
    for (const Token& token : tokens) {
      if (callback->OnKey(token.key) != Callback::TRAVERSE_CONTINUE ||
          callback->OnActualKey(token.key, token.key, false) !=
              Callback::TRAVERSE_CONTINUE) {
        return;
      }
      if (callback->OnToken(token.key, token.key, token) !=
          Callback::TRAVERSE_CONTINUE) {
        return;
      }
    }
  }

  std::vector<Token> tokens;
};

struct InvokeCallbackWithKeyValues {
  using Callback = DictionaryInterface::Callback;

  template <class T, class U>
  void operator()(T, U, Callback* callback) {
    for (const auto& [key, value] : kv_list) {
      if (callback->OnKey(key) != Callback::TRAVERSE_CONTINUE ||
          callback->OnActualKey(key, key, false) !=
              Callback::TRAVERSE_CONTINUE) {
        return;
      }
      const Token token(key, value, MockDictionary::kDefaultCost,
                        MockDictionary::kDefaultPosId,
                        MockDictionary::kDefaultPosId, token_attribute);
      if (callback->OnToken(key, key, token) != Callback::TRAVERSE_CONTINUE) {
        return;
      }
    }
  }

  std::vector<std::pair<absl::string_view, absl::string_view>> kv_list;
  Token::Attribute token_attribute = Token::NONE;
};

class DictionaryPredictorTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    request_ = std::make_unique<commands::Request>();
    config_ = std::make_unique<config::Config>();
    config::ConfigHandler::GetDefaultConfig(config_.get());
    table_ = std::make_shared<composer::Table>();
    composer_ =
        std::make_unique<composer::Composer>(table_, *request_, *config_);
  }

  void PrependHistory(absl::string_view key, absl::string_view value) {
    history_result_.value = absl::StrCat(value, history_result_.value);
    history_result_.key = absl::StrCat(key, history_result_.key);
  }

  void InitHistory(absl::string_view key, absl::string_view value,
                   int rid = 1) {
    history_result_.key = std::string(key);
    history_result_.value = std::string(value);
    history_result_.rid = rid;
  }

  ConversionRequest CreateConversionRequestWithOptions(
      ConversionRequest::Options&& options, absl::string_view key = "",
      bool init_composer = false) const {
    if (init_composer) {
      composer_->Reset();
      composer_->SetPreeditTextForTestOnly(key);
    }
    return ConversionRequestBuilder()
        .SetComposer(*composer_)
        .SetRequestView(*request_)
        .SetContextView(context_)
        .SetConfigView(*config_)
        .SetOptions(std::move(options))
        .SetHistoryResultView(history_result_)
        .SetKey(key)
        .Build();
  }

  ConversionRequest CreateConversionRequest(
      ConversionRequest::Options&& options, absl::string_view key = "",
      bool init_composer = true) const {
    return CreateConversionRequestWithOptions(std::move(options), key,
                                              init_composer);
  }

  ConversionRequest CreateConversionRequest(
      ConversionRequest::RequestType request_type,
      absl::string_view key = "") const {
    ConversionRequest::Options options;
    options.request_type = request_type;
    return CreateConversionRequestWithOptions(std::move(options), key);
  }

  ConversionRequest CreateSuggestionConversionRequest(
      absl::string_view key, bool init_composer = true) const {
    ConversionRequest::Options options;
    options.request_type = ConversionRequest::SUGGESTION;
    return CreateConversionRequest(std::move(options), key, init_composer);
  }

  ConversionRequest CreatePredictionConversionRequest(
      absl::string_view key, bool init_composer = true) const {
    ConversionRequest::Options options;
    options.request_type = ConversionRequest::PREDICTION;
    return CreateConversionRequest(std::move(options), key, init_composer);
  }

  static std::unique_ptr<MockDataAndPredictor> CreatePredictorWithMockData(
      std::unique_ptr<DictionaryInterface> suffix_dictionary,
      std::unique_ptr<engine::SupplementalModelInterface> supplemental_model) {
    auto ret = std::make_unique<MockDataAndPredictor>();
    ret->Init(std::move(suffix_dictionary), std::move(supplemental_model));
    AddWordsToMockDic(ret->mutable_dictionary());
    AddDefaultImplToMockRealtimeDecoder(ret->mutable_realtime_decoder());
    return ret;
  }

  static std::unique_ptr<MockDataAndPredictor> CreatePredictorWithMockData() {
    return CreatePredictorWithMockData(/*suffix dictionary=*/nullptr,
                                       /*supplemental_model=*/nullptr);
  }

  static void AddWordsToMockDic(MockDictionary* mock) {
    EXPECT_CALL(*mock, LookupPredictive(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(*mock, LookupPrefix(_, _, _)).Times(AnyNumber());

    EXPECT_CALL(*mock, LookupPredictive(StrEq("ぐーぐるあ"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"ぐーぐるあどせんす", "グーグルアドセンス"},
            {"ぐーぐるあどわーず", "グーグルアドワーズ"},
        }});
    EXPECT_CALL(*mock, LookupPredictive(StrEq("ぐーぐる"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"ぐーぐるあどせんす", "グーグルアドセンス"},
            {"ぐーぐるあどわーず", "グーグルアドワーズ"},
        }});
    EXPECT_CALL(*mock, LookupPrefix(StrEq("ぐーぐる"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"ぐーぐる", "グーグル"},
        }});
    EXPECT_CALL(*mock, LookupPrefix(StrEq("ぐーぐ"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"ぐー", "グー"},
        }});
    EXPECT_CALL(*mock, LookupPrefix(StrEq("あどせんす"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"あどせんす", "アドセンス"},
        }});
    EXPECT_CALL(*mock, LookupPrefix(StrEq("てすと"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"てすと", "テスト"},
        }});
    EXPECT_CALL(*mock, LookupPredictive(StrEq("てす"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"てすと", "テスト"},
        }});
    EXPECT_CALL(*mock, LookupPredictive(StrEq("てすとだ"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"てすとだよ", "テストだよ"},
        }});
    EXPECT_CALL(*mock, LookupPrefix(StrEq("て"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"て", "テスト"},
        }});
    // Bigram entry of "これは|テストだよ”
    EXPECT_CALL(*mock, LookupPredictive(StrEq("これはてすとだ"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"これはてすとだよ", "これはテストだよ"},
        }});
    // Previous context must exist in the dictionary when bigram is triggered.
    EXPECT_CALL(*mock, LookupPrefix(StrEq("これは"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"これは", "これは"},
        }});
    EXPECT_CALL(*mock, LookupPredictive(StrEq("てすとだよてす"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"てすとだよてすと", "テストだよテスト"},
        }});
    EXPECT_CALL(*mock, LookupPrefix(StrEq("てすとだよ"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"てすとだよ", "テストだよ"},
        }});

    // SpellingCorrection entry
    EXPECT_CALL(*mock, LookupPredictive(StrEq("かぷりちょうざ"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{
            {
                {"かぷりちょーざ", "カプリチョーザ"},
            },
            Token::SPELLING_CORRECTION});

    // user dictionary entry
    EXPECT_CALL(*mock, LookupPredictive(StrEq("ゆーざー"), _, _))
        .WillRepeatedly(
            InvokeCallbackWithKeyValues{{
                                            {"ゆーざー", "ユーザー"},
                                        },
                                        Token::USER_DICTIONARY});

    // Some English entries
    EXPECT_CALL(*mock, LookupPredictive(StrEq("conv"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"converge", "converge"},
            {"converged", "converged"},
            {"convergent", "convergent"},
        }});
    EXPECT_CALL(*mock, LookupPredictive(StrEq("con"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"contraction", "contraction"},
            {"control", "control"},
        }});
    EXPECT_CALL(*mock, LookupPredictive(StrEq("hel"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"hello", "hello"},
        }});
    // Prefix lookup doesn't allow the prefix match, e.g. "he" -> "h" by
    // default, so add Hiragana values to let prefix-lookup return
    // some results.
    EXPECT_CALL(*mock, LookupPrefix(StrEq("he"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"はろー", "はろー"},
        }});
  }

  static void AddDefaultImplToMockRealtimeDecoder(MockRealtimeDecoder* mock) {
    EXPECT_CALL(*mock, Decode(_))
        .Times(AnyNumber())
        .WillRepeatedly(MockRealtimeDecoder::DecodeImpl);
    EXPECT_CALL(*mock, ReverseDecode(_))
        .Times(AnyNumber())
        .WillRepeatedly(MockRealtimeDecoder::DecodeImpl);
  }

  std::unique_ptr<composer::Composer> composer_;
  std::shared_ptr<composer::Table> table_;
  std::unique_ptr<config::Config> config_;
  std::unique_ptr<commands::Request> request_;
  commands::Context context_;
  Result history_result_;
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

TEST_F(DictionaryPredictorTest, RemoveMissSpelledCandidates) {
  const ConversionRequest req_len1 =
      ConversionRequestBuilder().SetKey("1").Build();
  const ConversionRequest req_len3 =
      ConversionRequestBuilder().SetKey("111").Build();

  {
    std::vector<Result> results = {
        CreateResult4("ばっく", "バッグ", prediction::UNIGRAM,
                      Token::SPELLING_CORRECTION),
        CreateResult4("ばっぐ", "バッグ", prediction::UNIGRAM, Token::NONE),
        CreateResult4("ばっく", "バッく", prediction::UNIGRAM, Token::NONE),
    };
    DictionaryPredictorTestPeer::RemoveMissSpelledCandidates(
        req_len1, absl::MakeSpan(results));

    ASSERT_EQ(3, results.size());
    EXPECT_TRUE(results[0].removed);
    EXPECT_FALSE(results[1].removed);
    EXPECT_TRUE(results[2].removed);
    EXPECT_EQ(results[0].GetPredictionTypesForTesting(), prediction::UNIGRAM);
    EXPECT_EQ(results[1].GetPredictionTypesForTesting(), prediction::UNIGRAM);
    EXPECT_EQ(results[2].GetPredictionTypesForTesting(), prediction::UNIGRAM);
  }
  {
    std::vector<Result> results = {
        CreateResult4("ばっく", "バッグ", prediction::UNIGRAM,
                      Token::SPELLING_CORRECTION),
        CreateResult4("てすと", "テスト", prediction::UNIGRAM, Token::NONE),
    };
    DictionaryPredictorTestPeer::RemoveMissSpelledCandidates(
        req_len1, absl::MakeSpan(results));

    CHECK_EQ(2, results.size());
    EXPECT_FALSE(results[0].removed);
    EXPECT_FALSE(results[1].removed);
    EXPECT_EQ(results[0].GetPredictionTypesForTesting(), prediction::UNIGRAM);
    EXPECT_EQ(results[1].GetPredictionTypesForTesting(), prediction::UNIGRAM);
  }
  {
    std::vector<Result> results = {
        CreateResult4("ばっく", "バッグ", prediction::UNIGRAM,
                      Token::SPELLING_CORRECTION),
        CreateResult4("ばっく", "バック", prediction::UNIGRAM, Token::NONE),
    };
    DictionaryPredictorTestPeer::RemoveMissSpelledCandidates(
        req_len1, absl::MakeSpan(results));

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
    DictionaryPredictorTestPeer::RemoveMissSpelledCandidates(
        req_len3, absl::MakeSpan(results));

    CHECK_EQ(2, results.size());
    EXPECT_FALSE(results[0].removed);
    EXPECT_TRUE(results[1].removed);
    EXPECT_EQ(results[0].GetPredictionTypesForTesting(), prediction::UNIGRAM);
    EXPECT_EQ(results[1].GetPredictionTypesForTesting(), prediction::UNIGRAM);
  }
}

TEST_F(DictionaryPredictorTest, GetLMCost) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  DictionaryPredictorTestPeer predictor_peer =
      data_and_predictor->predictor_peer();
  const Connector& connector = data_and_predictor->connector();

  Result result;
  result.wcost = 64;

  for (int rid = 0; rid < 100; ++rid) {
    for (int lid = 0; lid < 100; ++lid) {
      result.lid = lid;
      const int c1 = connector.GetTransitionCost(rid, result.lid);
      const int c2 = connector.GetTransitionCost(0, result.lid);
      result.attributes = prediction::SUFFIX;
      EXPECT_EQ(predictor_peer.GetLMCost(result, rid), c1 + result.wcost);

      result.attributes = prediction::REALTIME;
      EXPECT_EQ(predictor_peer.GetLMCost(result, rid),
                std::min(c1, c2) + result.wcost);
    }
  }
}

TEST_F(DictionaryPredictorTest, SetPredictionCostForMixedConversion) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  DictionaryPredictorTestPeer predictor_peer =
      data_and_predictor->predictor_peer();

  std::vector<Result> results = {
      CreateResult4("てすと", "てすと", prediction::UNIGRAM, Token::NONE),
      CreateResult4("てすと", "テスト", prediction::UNIGRAM, Token::NONE),
      CreateResult4("てすとてすと", "テストテスト", prediction::UNIGRAM,
                    Token::NONE),
  };

  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::PREDICTION, "てすと");
  predictor_peer.SetPredictionCostForMixedConversion(convreq,
                                                     absl::MakeSpan(results));

  EXPECT_EQ(results.size(), 3);
  EXPECT_EQ(results[0].value, "てすと");
  EXPECT_EQ(results[1].value, "テスト");
  EXPECT_EQ(results[2].value, "テストテスト");
  EXPECT_GT(results[2].cost, results[0].cost);
  EXPECT_GT(results[2].cost, results[1].cost);
}

TEST_F(DictionaryPredictorTest, SetLMCostForUserDictionaryWord) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  DictionaryPredictorTestPeer predictor_peer =
      data_and_predictor->predictor_peer();

  constexpr absl::string_view kAikaHiragana = "あいか";
  constexpr absl::string_view kAikaKanji = "愛佳";

  {
    // Cost of words in user dictionary should be decreased.
    constexpr int kOriginalWordCost = 10000;
    std::vector<Result> results = {
        CreateResult5(kAikaHiragana, kAikaKanji, kOriginalWordCost,
                      prediction::UNIGRAM, Token::USER_DICTIONARY),
    };

    const ConversionRequest convreq =
        CreateConversionRequest(ConversionRequest::PREDICTION, kAikaHiragana);
    predictor_peer.SetPredictionCostForMixedConversion(convreq,
                                                       absl::MakeSpan(results));

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

    const ConversionRequest convreq =
        CreateConversionRequest(ConversionRequest::PREDICTION, kAikaHiragana);
    predictor_peer.SetPredictionCostForMixedConversion(convreq,
                                                       absl::MakeSpan(results));

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

    const ConversionRequest convreq =
        CreateConversionRequest(ConversionRequest::PREDICTION, kAikaHiragana);
    predictor_peer.SetPredictionCostForMixedConversion(convreq,
                                                       absl::MakeSpan(results));

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

    const ConversionRequest convreq =
        CreateConversionRequest(ConversionRequest::PREDICTION, kAikaHiragana);
    predictor_peer.SetPredictionCostForMixedConversion(convreq,
                                                       absl::MakeSpan(results));

    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].value, kAikaKanji);
    EXPECT_EQ(results[0].cost, kOriginalWordCost);
  }
}

TEST_F(DictionaryPredictorTest, SuggestSpellingCorrection) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  const DictionaryPredictor& predictor = data_and_predictor->predictor();
  MockAggregator* aggregator = data_and_predictor->mutable_aggregator();
  EXPECT_CALL(*aggregator, AggregateResultsForDesktop(_))
      .WillOnce(Return(std::vector<Result>{
          CreateResult5("あぼがど", "アボカド", 500, prediction::UNIGRAM,
                        Token::SPELLING_CORRECTION),
          CreateResult5("あぼがど", "アボガド", 500, prediction::UNIGRAM,
                        Token::NONE)}));

  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::PREDICTION, "あぼがど");
  const std::vector<Result> results = predictor.Predict(convreq);

  EXPECT_TRUE(FindCandidateByValue(results, "アボカド"));
}

TEST_F(DictionaryPredictorTest, DoNotSuggestSpellingCorrectionBeforeMismatch) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  const DictionaryPredictor& predictor = data_and_predictor->predictor();
  MockAggregator* aggregator = data_and_predictor->mutable_aggregator();

  EXPECT_CALL(*aggregator, AggregateResultsForDesktop(_))
      .WillOnce(Return(std::vector<Result>{
          CreateResult5("あぼがど", "アボカド", 500, prediction::UNIGRAM,
                        Token::SPELLING_CORRECTION),
          CreateResult5("あぼがど", "アボガド", 500, prediction::UNIGRAM,
                        Token::NONE),
      }));

  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::PREDICTION, "あぼが");
  const std::vector<Result> results = predictor.Predict(convreq);

  EXPECT_FALSE(FindCandidateByValue(results, "アボカド"));
}

TEST_F(DictionaryPredictorTest, MobileZeroQuery) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  const DictionaryPredictor& predictor = data_and_predictor->predictor();
  MockAggregator* aggregator = data_and_predictor->mutable_aggregator();

  EXPECT_CALL(*aggregator, AggregateResultsForMixedConversion(_))
      .WillOnce(Return(std::vector<Result>{
          CreateResult5("", "", 500, prediction::BIGRAM, Token::NONE),
          CreateResult5("いん", "院", 600, prediction::BIGRAM, Token::NONE),
          CreateResult5("せい", "生", 600, prediction::BIGRAM, Token::NONE),
          CreateResult5("やきゅう", "野球", 1000, prediction::BIGRAM,
                        Token::NONE),
          CreateResult5("じゅけん", "受験", 1000, prediction::BIGRAM,
                        Token::NONE),
          CreateResult5("にゅうし", "入試", 1000, prediction::BIGRAM,
                        Token::NONE),
          CreateResult5("にゅうしせんたー", "入試センター", 2000,
                        prediction::BIGRAM, Token::NONE),
      }));

  InitHistory("だいがく", "大学");
  PrependHistory("とうきょう", "東京");  // not used

  request_test_util::FillMobileRequest(request_.get());
  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::PREDICTION, "");
  const std::vector<Result> results = predictor.Predict(convreq);

  EXPECT_TRUE(FindCandidateByKeyValue(results, "にゅうし", "入試"));
  EXPECT_TRUE(
      FindCandidateByKeyValue(results, "にゅうしせんたー", "入試センター"));
}

TEST_F(DictionaryPredictorTest, PredictivePenaltyForBigramResults) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  const DictionaryPredictor& predictor = data_and_predictor->predictor();
  MockAggregator* aggregator = data_and_predictor->mutable_aggregator();

  EXPECT_CALL(*aggregator, AggregateResultsForMixedConversion(_))
      .WillOnce(Return(std::vector<Result>{
          CreateResult5("にゅうし", "入試", 3000, prediction::BIGRAM,
                        Token::NONE),
          CreateResult5("にゅうしせんたー", "入試センター", 4000,
                        prediction::BIGRAM, Token::NONE),
          CreateResult5("にゅうしせんたーしけんたいさく",
                        "入試センター試験対策", 5000, prediction::BIGRAM,
                        Token::NONE),
          CreateResult5("にゅうし", "乳歯", 2000, prediction::UNIGRAM,
                        Token::NONE)}));

  InitHistory("だいがく", "大学");
  PrependHistory("とうきょう", "東京");  // not used

  request_test_util::FillMobileRequest(request_.get());
  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::PREDICTION, "にゅうし");
  const std::vector<Result> results = predictor.Predict(convreq);

  auto get_rank_by_value = [&](absl::string_view value) {
    for (int i = 0; i < results.size(); ++i) {
      if (results[i].value == value) {
        return i;
      }
    }
    return -1;
  };
  EXPECT_LT(get_rank_by_value("乳歯"),
            get_rank_by_value("入試センター試験対策"));
}

TEST_F(DictionaryPredictorTest, PropagateAttributes) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  const DictionaryPredictor& predictor = data_and_predictor->predictor();
  MockAggregator* aggregator = data_and_predictor->mutable_aggregator();
  MockRealtimeDecoder* realtime_decoder =
      data_and_predictor->mutable_realtime_decoder();

  // Exact key will not be filtered in mobile request
  request_test_util::FillMobileRequest(request_.get());

  // Small prefix penalty
  {
    Result result;
    result.cost = 10;
    EXPECT_CALL(*realtime_decoder, Decode(_))
        .WillRepeatedly(Return(std::vector<Result>({result})));
  }

  auto get_top_result = [&aggregator, &predictor, this](
                            const Result& aggregator_result,
                            PredictionTypes prediction_types, Result* result) {
    EXPECT_CALL(*aggregator, AggregateResultsForMixedConversion(_))
        .WillOnce(Return(std::vector<Result>({aggregator_result})));
    const ConversionRequest convreq =
        CreateConversionRequest(ConversionRequest::PREDICTION, "てすと");
    const std::vector<Result> results = predictor.Predict(convreq);
    if (results.empty()) {
      return false;
    }
    *result = results[0];
    return true;
  };

  Result c;
  {
    // PREFIX: consumed_key_size
    Result result = CreateResult5("てす", "てす", 50, PREFIX, Token::NONE);
    result.consumed_key_size = Util::CharsLen("てす");

    EXPECT_TRUE(get_top_result(result, PREFIX, &c));
    EXPECT_EQ(c.value, "てす");
    EXPECT_EQ(
        c.GetBehavioralAttributes(),
        Attribute::PARTIALLY_KEY_CONSUMED | Attribute::AUTO_PARTIAL_SUGGESTION);
    EXPECT_EQ(c.consumed_key_size, 2);
  }
  {
    // REALTIME_TOP
    Result result = CreateResult5("てすと", "リアルタイムトップ", 100,
                                  REALTIME_TOP | REALTIME, Token::NONE);

    EXPECT_TRUE(get_top_result(result, REALTIME_TOP, &c));
    EXPECT_EQ(c.value, "リアルタイムトップ");
    EXPECT_EQ(
        c.GetBehavioralAttributes(),
        Attribute::REALTIME_CONVERSION | Attribute::NO_VARIANTS_EXPANSION);
  }
  {
    // REALTIME: inner_segment_boundary
    Result result =
        CreateResult5("てすと", "リアルタイム", 100, REALTIME, Token::NONE);
    result.inner_segment_boundary = converter::BuildInnerSegmentBoundary(
        {{strlen("てす"), strlen("リアル"), strlen("て"), strlen("リア")},
         {strlen("と"), strlen("タイム"), strlen("と"), strlen("タイム")}},
        result.key, result.value);
    EXPECT_TRUE(get_top_result(result, REALTIME, &c));
    EXPECT_EQ(c.value, "リアルタイム");
    EXPECT_EQ(c.GetBehavioralAttributes(), Attribute::REALTIME_CONVERSION);
    EXPECT_EQ(c.inner_segment_boundary.size(), 2);
  }
  {
    // SPELLING_CORRECTION
    Result result = CreateResult5("てすと", "SPELLING_CORRECTION", 300, UNIGRAM,
                                  Token::SPELLING_CORRECTION);

    EXPECT_TRUE(get_top_result(result, UNIGRAM, &c));
    EXPECT_EQ(c.value, "SPELLING_CORRECTION");
    EXPECT_EQ(c.GetBehavioralAttributes(), Attribute::SPELLING_CORRECTION);
  }
  {
    // TYPING_CORRECTION
    Result result = CreateResult5("てすと", "TYPING_CORRECTION", 300,
                                  TYPING_CORRECTION, Token::NONE);

    EXPECT_TRUE(get_top_result(result, TYPING_CORRECTION, &c));
    EXPECT_EQ(c.value, "TYPING_CORRECTION");
    EXPECT_EQ(c.GetBehavioralAttributes(), Attribute::TYPING_CORRECTION);
  }
  {
    // USER_DICTIONARY
    Result result = CreateResult5("てすと", "ユーザー辞書", 300, UNIGRAM,
                                  Token::USER_DICTIONARY);

    EXPECT_TRUE(get_top_result(result, UNIGRAM, &c));
    EXPECT_EQ(c.value, "ユーザー辞書");
    EXPECT_EQ(c.GetBehavioralAttributes(),
              Attribute::USER_DICTIONARY | Attribute::NO_MODIFICATION |
                  Attribute::NO_VARIANTS_EXPANSION);
  }
  {
    // removed
    Result result =
        CreateResult5("てすと", "REMOVED", 300, BIGRAM, Token::NONE);
    result.removed = true;

    EXPECT_FALSE(get_top_result(result, UNIGRAM, &c));
  }
}

TEST_F(DictionaryPredictorTest, MergeAttributesForDebug) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  DictionaryPredictorTestPeer predictor_peer =
      data_and_predictor->predictor_peer();

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

  // Enables debug mode.
  config_->set_verbose_level(1);
  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::SUGGESTION, "test");
  results = predictor_peer.RerankAndFilterResults(convreq, results);

  for (size_t i = 0; i < results.size(); ++i) {
    EXPECT_EQ(results[i].description, "RS");
  }
}

TEST_F(DictionaryPredictorTest, PropagateResultCosts) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  DictionaryPredictorTestPeer predictor_peer =
      data_and_predictor->predictor_peer();

  constexpr int kTestSize = 20;
  std::vector<Result> results(kTestSize);
  for (size_t i = 0; i < kTestSize; ++i) {
    Result* result = &results[i];
    result->key = std::string(1, 'a' + i);
    result->value = std::string(1, 'A' + i);
    result->wcost = i;
    result->cost = i + 1000;
    result->SetTypesAndTokenAttributes(prediction::REALTIME, Token::NONE);
  }
  absl::BitGen urbg;
  std::shuffle(results.begin(), results.end(), urbg);

  const ConversionRequest convreq = CreateConversionRequestWithOptions(
      {
          .request_type = ConversionRequest::SUGGESTION,
          .max_dictionary_prediction_candidates_size = kTestSize,
      },
      "test");

  results = predictor_peer.RerankAndFilterResults(convreq, results);

  ASSERT_EQ(kTestSize, results.size());
  for (size_t i = 0; i < results.size(); ++i) {
    EXPECT_EQ(results[i].cost, i + 1000);
  }
}

TEST_F(DictionaryPredictorTest, PredictNCandidates) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  DictionaryPredictorTestPeer predictor_peer =
      data_and_predictor->predictor_peer();

  constexpr int kTotalCandidateSize = 100;
  constexpr int kLowCostCandidateSize = 5;
  std::vector<Result> results(kTotalCandidateSize);
  for (size_t i = 0; i < kTotalCandidateSize; ++i) {
    Result* result = &results[i];
    result->key = std::string(1, 'a' + i);
    result->value = std::string(1, 'A' + i);
    result->wcost = i;
    result->SetTypesAndTokenAttributes(prediction::REALTIME, Token::NONE);
    if (i < kLowCostCandidateSize) {
      result->cost = i + 1000;
    } else {
      result->cost = i + Result::kInvalidCost;
    }
  }
  absl::BitGen urbg;
  std::shuffle(results.begin(), results.end(), urbg);

  const ConversionRequest convreq = CreateConversionRequestWithOptions(
      {
          .request_type = ConversionRequest::SUGGESTION,
          .max_dictionary_prediction_candidates_size =
              kLowCostCandidateSize + 1,
      },
      "test");

  results = predictor_peer.RerankAndFilterResults(convreq, results);

  ASSERT_EQ(kLowCostCandidateSize, results.size());
  for (size_t i = 0; i < results.size(); ++i) {
    EXPECT_EQ(results[i].cost, i + 1000);
  }
}

TEST_F(DictionaryPredictorTest, SuggestFilteredwordForExactMatchOnMobile) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  const DictionaryPredictor& predictor = data_and_predictor->predictor();
  // turn on mobile mode
  request_test_util::FillMobileRequest(request_.get());

  {
    MockAggregator* aggregator = data_and_predictor->mutable_aggregator();

    EXPECT_CALL(*aggregator, AggregateResultsForMixedConversion(_))
        .WillRepeatedly(Return(std::vector<Result>{
            CreateResult5("ふぃるたーたいしょう", "フィルター対象", 100,
                          prediction::UNIGRAM, Token::NONE),
            CreateResult5("ふぃるたーたいしょう", "フィルター大将", 200,
                          prediction::UNIGRAM, Token::NONE),
        }));
  }

  // Note: The suggestion filter entry "フィルター" for test is not
  // appropriate here, as Katakana entry will be added by real time
  // conversion. Here, we want to confirm the behavior including unigram
  // prediction.
  const ConversionRequest convreq1 = CreateConversionRequest(
      ConversionRequest::SUGGESTION, "ふぃるたーたいしょう");
  std::vector<Result> results = predictor.Predict(convreq1);
  EXPECT_TRUE(FindCandidateByValue(results, "フィルター対象"));
  EXPECT_TRUE(FindCandidateByValue(results, "フィルター大将"));

  // However, filtered word should not be the top.
  EXPECT_EQ(results[0].value, "フィルター大将");

  // Should not be there for non-exact suggestion.
  const ConversionRequest convreq2 = CreateConversionRequest(
      ConversionRequest::SUGGESTION, "ふぃるたーたいし");
  results = predictor.Predict(convreq2);
  EXPECT_FALSE(FindCandidateByValue(results, "フィルター対象"));
}

TEST_F(DictionaryPredictorTest, SuppressFilteredwordForExactMatch) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  const DictionaryPredictor& predictor = data_and_predictor->predictor();

  {
    MockAggregator* aggregator = data_and_predictor->mutable_aggregator();

    EXPECT_CALL(*aggregator, AggregateResultsForDesktop(_))
        .WillRepeatedly(Return(std::vector<Result>{
            CreateResult5("ふぃるたーたいしょう", "フィルター対象", 100,
                          prediction::UNIGRAM, Token::NONE),
            CreateResult5("ふぃるたーたいしょう", "フィルター大将", 200,
                          prediction::UNIGRAM, Token::NONE),
        }));
  }

  // Note: The suggestion filter entry "フィルター" for test is not
  // appropriate here, as Katakana entry will be added by real time
  // conversion. Here, we want to confirm the behavior including unigram
  // prediction.
  const ConversionRequest convreq = CreateConversionRequest(
      ConversionRequest::SUGGESTION, "ふぃるたーたいしょう");
  const std::vector<Result> results = predictor.Predict(convreq);
  EXPECT_FALSE(FindCandidateByValue(results, "フィルター対象"));
}

TEST_F(DictionaryPredictorTest, DoNotFilterExactUnigramOnMobile) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  const DictionaryPredictor& predictor = data_and_predictor->predictor();
  request_test_util::FillMobileRequest(request_.get());

  {
    MockAggregator* aggregator = data_and_predictor->mutable_aggregator();

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

    EXPECT_CALL(*aggregator, AggregateResultsForMixedConversion(_))
        .WillOnce(Return(results));
  }

  const ConversionRequest convreq = CreateConversionRequestWithOptions(
      {
          .request_type = ConversionRequest::PREDICTION,
          .max_dictionary_prediction_candidates_size = 100,
      },
      "てすと");
  const std::vector<Result> results = predictor.Predict(convreq);
  int exact_count = 0;
  for (const Result& result : results) {
    if (absl::StrContains(result.value, "テストE")) {
      exact_count++;
    }
  }
  EXPECT_EQ(exact_count, 30);
}

TEST_F(DictionaryPredictorTest, DoNotFilterUnigrmsForHandwriting) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  const DictionaryPredictor& predictor = data_and_predictor->predictor();

  // Destkop doesn't support handwriting.
  request_test_util::FillMobileRequest(request_.get());

  // Fill handwriting request and composer
  {
    request_->set_zero_query_suggestion(true);
    request_->set_mixed_conversion(true);
    request_->set_kana_modifier_insensitive_conversion(false);
    request_->set_auto_partial_suggestion(false);

    commands::SessionCommand command;
    commands::SessionCommand::CompositionEvent* composition_event =
        command.add_composition_events();
    composition_event->set_composition_string("かん字");
    composition_event->set_probability(1.0);
    composer_->SetCompositionsForHandwriting(command.composition_events());
  }

  {
    MockAggregator* aggregator = data_and_predictor->mutable_aggregator();

    std::vector<Result> results;
    for (int i = 0; i < 10; ++i) {
      // Exact entries
      results.push_back(CreateResult5("かん字", absl::StrCat(i, "漢字E"),
                                      5000 + i, prediction::UNIGRAM,
                                      Token::NONE));
    }
    for (int i = 0; i < 10; ++i) {
      // Keys can be longer than the segment key
      results.push_back(CreateResult5("かんじよみ", absl::StrCat(i, "漢字E"),
                                      5000 + i, prediction::UNIGRAM,
                                      Token::NONE));
    }

    EXPECT_CALL(*aggregator, AggregateResultsForMixedConversion(_))
        .WillOnce(Return(results));
  }

  const ConversionRequest convreq_for_prediction =
      CreateConversionRequestWithOptions(
          {
              .request_type = ConversionRequest::PREDICTION,
              .max_dictionary_prediction_candidates_size = 100,
          },
          "かん字");
  const std::vector<Result> results = predictor.Predict(convreq_for_prediction);
  int exact_count = 0;
  for (const Result& result : results) {
    if (absl::StrContains(result.value, "漢字E")) {
      exact_count++;
    }
  }
  EXPECT_EQ(exact_count, 20);
}

TEST_F(DictionaryPredictorTest, DoNotFilterZeroQueryCandidatesOnMobile) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  const DictionaryPredictor& predictor = data_and_predictor->predictor();
  request_test_util::FillMobileRequest(request_.get());

  {
    MockAggregator* aggregator = data_and_predictor->mutable_aggregator();

    // Entries for zero query
    std::vector<Result> results;
    for (int i = 0; i < 10; ++i) {
      results.push_back(CreateResult5("てすと", absl::StrCat(i, "テストS"), 100,
                                      prediction::SUFFIX, Token::NONE));
    }

    EXPECT_CALL(*aggregator, AggregateResultsForMixedConversion(_))
        .WillRepeatedly(Return(results));
  }

  InitHistory("わたし", "私");
  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::PREDICTION, "");
  const std::vector<Result> results = predictor.Predict(convreq);
  EXPECT_EQ(results.size(), 10);
}

TEST_F(DictionaryPredictorTest,
       DoNotFilterOneSegmentRealtimeCandidatesOnMobile) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  const DictionaryPredictor& predictor = data_and_predictor->predictor();
  // turn on mobile mode
  request_test_util::FillMobileRequest(request_.get());

  {
    MockAggregator* aggregator = data_and_predictor->mutable_aggregator();
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

    EXPECT_CALL(*aggregator, AggregateResultsForMixedConversion(_))
        .WillRepeatedly(Return(results));
  }

  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::PREDICTION, "かつた");
  const std::vector<Result> results = predictor.Predict(convreq);
  EXPECT_GE(results.size(), 8);
}

TEST_F(DictionaryPredictorTest, FixSRealtimeTopCandidatesCostOnMobile) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  const DictionaryPredictor& predictor = data_and_predictor->predictor();
  // turn on mobile mode
  request_test_util::FillMobileRequest(request_.get());

  {
    MockAggregator* aggregator = data_and_predictor->mutable_aggregator();
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
    EXPECT_CALL(*aggregator, AggregateResultsForMixedConversion(_))
        .WillRepeatedly(Return(results));
  }

  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::PREDICTION, "かった");
  const std::vector<Result> results = predictor.Predict(convreq);
  EXPECT_EQ(results[0].value, "買った");
}

TEST_F(DictionaryPredictorTest, SingleKanjiCost) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  const DictionaryPredictor& predictor = data_and_predictor->predictor();
  // turn on mobile mode
  request_test_util::FillMobileRequest(request_.get());

  {
    MockAggregator* aggregator = data_and_predictor->mutable_aggregator();
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
      if (results[i].attributes == prediction::SINGLE_KANJI) {
        results[i].lid = data_and_predictor->pos_matcher().GetGeneralSymbolId();
        results[i].rid = data_and_predictor->pos_matcher().GetGeneralSymbolId();
      } else {
        results[i].lid = data_and_predictor->pos_matcher().GetGeneralNounId();
        results[i].rid = data_and_predictor->pos_matcher().GetGeneralNounId();
      }
    }

    EXPECT_CALL(*aggregator, AggregateResultsForMixedConversion(_))
        .WillRepeatedly(Return(results));
  }

  std::vector<Result> results;
  auto get_rank_by_value = [&](absl::string_view value) {
    for (int i = 0; i < results.size(); ++i) {
      if (results[i].value == value) {
        return i;
      }
    }
    return -1;
  };

  {
    const ConversionRequest convreq =
        CreateConversionRequest(ConversionRequest::PREDICTION, "さか");
    results = predictor.Predict(convreq);
    EXPECT_NE(get_rank_by_value("佐"), -1);
    EXPECT_LT(get_rank_by_value("佐"), results.size() - 1);
    EXPECT_LT(get_rank_by_value("坂"), get_rank_by_value("逆"));
    EXPECT_LT(get_rank_by_value("咲か"), get_rank_by_value("逆"));
    EXPECT_LT(get_rank_by_value("阪"), get_rank_by_value("逆"));
    EXPECT_LT(get_rank_by_value("逆"), get_rank_by_value("差"));
  }
}

TEST_F(DictionaryPredictorTest, SingleKanjiFallbackOffsetCost) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  const DictionaryPredictor& predictor = data_and_predictor->predictor();
  // turn on mobile mode
  request_test_util::FillMobileRequest(request_.get());

  {
    MockAggregator* aggregator = data_and_predictor->mutable_aggregator();
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
      if (results[i].attributes == prediction::SINGLE_KANJI) {
        results[i].lid = data_and_predictor->pos_matcher().GetGeneralSymbolId();
        results[i].rid = data_and_predictor->pos_matcher().GetGeneralSymbolId();
      } else {
        results[i].lid = data_and_predictor->pos_matcher().GetGeneralNounId();
        results[i].rid = data_and_predictor->pos_matcher().GetGeneralNounId();
      }
    }

    EXPECT_CALL(*aggregator, AggregateResultsForMixedConversion(_))
        .WillRepeatedly(Return(results));
  }

  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::PREDICTION, "ああ");
  const std::vector<Result> results = predictor.Predict(convreq);
  ASSERT_EQ(results.size(), 7);
  ASSERT_EQ(results[0].value, "アア");
  ASSERT_EQ(results[1].value, "ああ");
}

TEST_F(DictionaryPredictorTest, Dedup) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  DictionaryPredictorTestPeer predictor_peer =
      data_and_predictor->predictor_peer();
  // turn on mobile mode
  request_test_util::FillMobileRequest(request_.get());

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

    const ConversionRequest convreq =
        CreateConversionRequest(ConversionRequest::PREDICTION, "test");
    results = predictor_peer.RerankAndFilterResults(convreq, results);
    EXPECT_EQ(results.size(), kSize);
  }
}

TEST_F(DictionaryPredictorTest, TypingCorrectionResultsLimit) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  DictionaryPredictorTestPeer predictor_peer =
      data_and_predictor->predictor_peer();

  // turn on mobile mode
  request_test_util::FillMobileRequest(request_.get());

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

  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::PREDICTION, "original_key");
  results = predictor_peer.RerankAndFilterResults(convreq, results);

  EXPECT_EQ(results.size(), 3);
  EXPECT_TRUE(FindCandidateByValue(results, "tc_value0"));
  EXPECT_TRUE(FindCandidateByValue(results, "tc_value1"));
  EXPECT_TRUE(FindCandidateByValue(results, "tc_value2"));
}

TEST_F(DictionaryPredictorTest, SortResult) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  DictionaryPredictorTestPeer predictor_peer =
      data_and_predictor->predictor_peer();
  // turn on mobile mode
  request_test_util::FillMobileRequest(request_.get());

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
  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::PREDICTION, "test");
  results = predictor_peer.RerankAndFilterResults(convreq, results);

  ASSERT_EQ(results.size(), 7);
  ASSERT_EQ(results[0].value, "テスト０");      // cost:1
  ASSERT_EQ(results[1].value, "テスト１");      // cost:1
  ASSERT_EQ(results[2].value, "テスト００");    // cost:1
  ASSERT_EQ(results[3].value, "テスト１０");    // cost:1
  ASSERT_EQ(results[4].value, "テスト０００");  // cost:1
  ASSERT_EQ(results[5].value, "テストＡ");      // cost:10
  ASSERT_EQ(results[6].value, "テストＢ");      // cost:100
}

TEST_F(DictionaryPredictorTest, SetCostForRealtimeTopCandidate) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  const DictionaryPredictor& predictor = data_and_predictor->predictor();

  {
    MockAggregator* aggregator = data_and_predictor->mutable_aggregator();

    EXPECT_CALL(*aggregator, AggregateResultsForDesktop(_))
        .WillOnce(Return(std::vector<Result>{
            CreateResult5("あいう", "会いう", 100,
                          prediction::REALTIME_TOP | prediction::REALTIME,
                          Token::NONE),
            CreateResult5("あいうえ", "会いうえ", 1000, prediction::REALTIME,
                          Token::NONE)}));
  }

  request_->set_mixed_conversion(false);
  const ConversionRequest convreq = CreateConversionRequestWithOptions(
      {.request_type = ConversionRequest::SUGGESTION,
       .use_actual_converter_for_realtime_conversion = true},
      "あいう");

  const std::vector<Result> results = predictor.Predict(convreq);
  EXPECT_EQ(results.size(), 2);
  EXPECT_EQ(results[0].value, "会いう");
}

TEST_F(DictionaryPredictorTest, InvalidPrefixCandidate) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  const DictionaryPredictor& predictor = data_and_predictor->predictor();
  MockAggregator* aggregator = data_and_predictor->mutable_aggregator();
  MockRealtimeDecoder* realtime_decoder =
      data_and_predictor->mutable_realtime_decoder();

  // Exact key will not be filtered in mobile request
  request_test_util::FillMobileRequest(request_.get());

  {
    Result result;
    result.key = "ーひー";
    result.value = "ーひー";
    result.key = "ーひー";
    result.cost = 0;
    EXPECT_CALL(*realtime_decoder, Decode(_))
        .WillRepeatedly(Return(std::vector<Result>({result})));
  }

  {
    EXPECT_CALL(*aggregator, AggregateResultsForDesktop(_))
        .WillRepeatedly(Return(std::vector<Result>{
            CreateResult6("こ", "子", 0, 10, prediction::PREFIX, Token::NONE),
            CreateResult6("こーひー", "コーヒー", 0, 100, prediction::UNIGRAM,
                          Token::NONE),
            CreateResult6("こーひー", "珈琲", 0, 200, prediction::UNIGRAM,
                          Token::NONE),
            CreateResult6("こーひー", "coffee", 0, 300, prediction::UNIGRAM,
                          Token::NONE)}));
  }

  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::PREDICTION, "こーひー");
  const std::vector<Result> results = predictor.Predict(convreq);
  EXPECT_FALSE(FindCandidateByValue(results, "子"));
}

TEST_F(DictionaryPredictorTest, AggregateTypingCorrectedResultsTest) {
  auto supplemental_model = std::make_unique<engine::MockSupplementalModel>();
  EXPECT_CALL(*supplemental_model, CorrectComposition(_))
      .WillRepeatedly(Return(std::vector<TypeCorrectedQuery>{
          {"とうきょう",
           TypeCorrectedQuery::CORRECTION | TypeCorrectedQuery::COMPLETION},
          {"とうきょう", TypeCorrectedQuery::COMPLETION},
      }));
  auto data_and_predictor =
      std::make_unique<MockDataAndPredictor>(std::move(supplemental_model));
  EXPECT_CALL(*data_and_predictor->mutable_realtime_decoder(), Decode(_))
      .WillRepeatedly(MockRealtimeDecoder::DecodeImpl);

  config_->set_use_typing_correction(true);

  DictionaryPredictorTestPeer predictor_peer =
      data_and_predictor->predictor_peer();

  // 0.8 900
  {
    const ConversionRequest convreq =
        CreateConversionRequest(ConversionRequest::PREDICTION, "とあきよう");
    const std::vector<Result> results =
        predictor_peer.AggregateTypingCorrectedResultsForMixedConversion(
            convreq);
    EXPECT_EQ(results.size(), 2);
  }

  // disable typing correction.
  {
    config_->set_use_typing_correction(false);
    const ConversionRequest convreq =
        CreateConversionRequest(ConversionRequest::PREDICTION, "とあきよう");
    const std::vector<Result> results =
        predictor_peer.AggregateTypingCorrectedResultsForMixedConversion(
            convreq);
    EXPECT_TRUE(results.empty());
  }
}

TEST_F(DictionaryPredictorTest, Rescoring) {
  auto supplemental_model = std::make_unique<engine::MockSupplementalModel>();
  EXPECT_CALL(*supplemental_model, RescoreResults(_, _))
      .WillRepeatedly(
          [](const ConversionRequest& request, absl::Span<Result> results) {
            for (Result& r : results) r.cost = 100;
          });

  auto data_and_predictor =
      std::make_unique<MockDataAndPredictor>(std::move(supplemental_model));
  const DictionaryPredictor& predictor = data_and_predictor->predictor();
  MockAggregator* aggregator = data_and_predictor->mutable_aggregator();
  EXPECT_CALL(*aggregator, AggregateResultsForDesktop(_))
      .WillOnce(Return(std::vector<Result>{
          CreateResult5("こーひー", "コーヒー", 500, prediction::UNIGRAM,
                        Token::NONE),
          CreateResult5("こーひー", "珈琲", 600, prediction::UNIGRAM,
                        Token::NONE),
          CreateResult5("こーひー", "coffee", 700, prediction::UNIGRAM,
                        Token::NONE),
      }));

  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::PREDICTION, "こーひー");
  const std::vector<Result> results = predictor.Predict(convreq);

  ASSERT_EQ(results.size(), 3);
  EXPECT_THAT(results, ::testing::ElementsAreArray({
                           Field(&Result::cost, 100),
                           Field(&Result::cost, 100),
                           Field(&Result::cost, 100),
                       }));
}

TEST_F(DictionaryPredictorTest, DoNotRescoreHandwriting) {
  // Use StrictMock to make sure that RescoreResults(), PostCorrect() are not be
  // called
  auto supplemental_model =
      std::make_unique<StrictMock<engine::MockSupplementalModel>>();
  auto data_and_predictor =
      std::make_unique<MockDataAndPredictor>(std::move(supplemental_model));

  // Fill handwriting config, request and composer
  {
    config_->set_use_typing_correction(false);
    request_->set_zero_query_suggestion(true);
    request_->set_mixed_conversion(false);
    request_->set_kana_modifier_insensitive_conversion(false);
    request_->set_auto_partial_suggestion(false);

    commands::SessionCommand command;
    commands::SessionCommand::CompositionEvent* composition_event =
        command.add_composition_events();
    composition_event->set_composition_string("かん字");
    composition_event->set_probability(1.0);
    composer_->SetCompositionsForHandwriting(command.composition_events());
  }

  const DictionaryPredictor& predictor = data_and_predictor->predictor();
  MockAggregator* aggregator = data_and_predictor->mutable_aggregator();
  EXPECT_CALL(*aggregator, AggregateResultsForDesktop(_))
      .WillOnce(Return(std::vector<Result>{
          CreateResult5("かんじ", "かん字", 0, prediction::UNIGRAM,
                        Token::NONE),
          CreateResult5("かんじ", "漢字", 500, prediction::UNIGRAM,
                        Token::NONE)}));

  ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::PREDICTION, "かんじ");
  predictor.Predict(convreq);
}

TEST_F(DictionaryPredictorTest, DoNotApplyPostCorrection) {
  // Use StrictMock to make sure that PostCorrect() is not be called
  auto supplemental_model = std::make_unique<engine::MockSupplementalModel>();
  auto data_and_predictor =
      std::make_unique<MockDataAndPredictor>(std::move(supplemental_model));

  config_->set_use_typing_correction(false);

  const DictionaryPredictor& predictor = data_and_predictor->predictor();
  MockAggregator* aggregator = data_and_predictor->mutable_aggregator();
  EXPECT_CALL(*aggregator, AggregateResultsForDesktop(_))
      .WillOnce(Return(std::vector<Result>{
          CreateResult5("かんじ", "かん字", 0, prediction::UNIGRAM,
                        Token::NONE),
          CreateResult5("かんじ", "漢字", 500, prediction::UNIGRAM,
                        Token::NONE)}));

  const ConversionRequest convreq =
      CreateConversionRequest(ConversionRequest::PREDICTION, "かんじ");
  predictor.Predict(convreq);
}

TEST_F(DictionaryPredictorTest, MaybeGetPreviousTopResultTest) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  DictionaryPredictorTestPeer predictor_peer =
      data_and_predictor->predictor_peer();

  // Result for しがこ (Initialize the prev_top).
  Result init_top =
      CreateResult4("しがこ", "志賀湖", prediction::UNIGRAM, Token::NONE);

  // Result for しがこう
  Result pre_top = CreateResult4("しがこうげん", "志賀高原",
                                 prediction::UNIGRAM, Token::NONE);

  // Result for しがこうげ. Inconsistent with prev top.
  Result cur_top =
      CreateResult4("しがこうげ", "子が原", prediction::UNIGRAM, Token::NONE);

  // Result for しがこうげ, but already consistent with the prev_top.
  Result cur_already_consintent_top = CreateResult4(
      "しがこうげんすきー", "志賀高原スキー", prediction::UNIGRAM, Token::NONE);

  pre_top.cost = 1000;
  cur_top.cost = 500;
  cur_already_consintent_top.cost = 500;

  auto* params = request_->mutable_decoder_experiment_params();

  auto create_request = [&](absl::string_view key) {
    return CreateConversionRequest(ConversionRequest::SUGGESTION, key);
  };

  // max diff is zero. No insertion happens.
  {
    params->set_candidate_consistency_cost_max_diff(0);

    EXPECT_FALSE(predictor_peer.MaybeGetPreviousTopResult(
        init_top, create_request("しが")));

    EXPECT_FALSE(predictor_peer.MaybeGetPreviousTopResult(
        pre_top, create_request("しがこう")));

    EXPECT_FALSE(predictor_peer.MaybeGetPreviousTopResult(
        pre_top, create_request("しがこうげ")));
  }

  // max diff is 2000.
  {
    params->set_candidate_consistency_cost_max_diff(2000);

    EXPECT_FALSE(predictor_peer.MaybeGetPreviousTopResult(
        init_top, create_request("しが")));

    EXPECT_FALSE(predictor_peer.MaybeGetPreviousTopResult(
        pre_top, create_request("しがこう")));

    auto result = predictor_peer.MaybeGetPreviousTopResult(
        cur_top, create_request("しがこうげ"));
    EXPECT_TRUE(result);
    EXPECT_EQ(result->value, "志賀高原");
  }

  // top is partial
  {
    EXPECT_FALSE(predictor_peer.MaybeGetPreviousTopResult(
        init_top, create_request("しが")));

    EXPECT_FALSE(predictor_peer.MaybeGetPreviousTopResult(
        pre_top, create_request("しがこう")));

    auto cur_top_prefix = cur_top;
    cur_top_prefix.attributes |= PREFIX;
    EXPECT_FALSE(predictor_peer.MaybeGetPreviousTopResult(
        cur_top_prefix, create_request("しがこうげ")));
  }

  // Already consistent.
  {
    EXPECT_FALSE(predictor_peer.MaybeGetPreviousTopResult(
        init_top, create_request("しが")));

    EXPECT_FALSE(predictor_peer.MaybeGetPreviousTopResult(
        pre_top, create_request("しがこう")));

    EXPECT_FALSE(predictor_peer.MaybeGetPreviousTopResult(
        cur_already_consintent_top, create_request("しがこうげ")));
  }

  // max diff is 200 -> not inserted
  {
    params->set_candidate_consistency_cost_max_diff(200);

    EXPECT_FALSE(predictor_peer.MaybeGetPreviousTopResult(
        init_top, create_request("しが")));

    EXPECT_FALSE(predictor_peer.MaybeGetPreviousTopResult(
        pre_top, create_request("しがこう")));

    EXPECT_FALSE(predictor_peer.MaybeGetPreviousTopResult(
        cur_top, create_request("しがこうげ")));
  }

  // No insertion happens when typing backspaces.
  {
    params->set_candidate_consistency_cost_max_diff(2000);

    EXPECT_FALSE(predictor_peer.MaybeGetPreviousTopResult(
        cur_top, create_request("しがこうげ")));

    EXPECT_FALSE(predictor_peer.MaybeGetPreviousTopResult(
        pre_top, create_request("しがこう")));

    EXPECT_FALSE(predictor_peer.MaybeGetPreviousTopResult(
        init_top, create_request("しが")));
  }
}

TEST_F(DictionaryPredictorTest, FilterNwpSuffixCandidates) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  const DictionaryPredictor& predictor = data_and_predictor->predictor();
  const Connector& connector = data_and_predictor->connector();
  request_test_util::FillMobileRequest(request_.get());
  constexpr int kThreshold = 1000;
  request_->mutable_decoder_experiment_params()
      ->set_suffix_nwp_transition_cost_threshold(kThreshold);

  {
    MockAggregator* aggregator = data_and_predictor->mutable_aggregator();

    std::vector<Result> results;
    {
      Result result;
      strings::Assign(result.key, "てすと");
      strings::Assign(result.value, "テスト");
      result.attributes = prediction::SUFFIX;
      result.cost = 1000;
      result.lid = data_and_predictor->pos_matcher().GetGeneralNounId();
      result.rid = data_and_predictor->pos_matcher().GetGeneralNounId();
      results.push_back(result);
    }

    EXPECT_CALL(*aggregator, AggregateResultsForMixedConversion(_))
        .WillRepeatedly(Return(results));
  }

  const std::vector<int> test_ids = {
      data_and_predictor->pos_matcher().GetGeneralNounId(),
      data_and_predictor->pos_matcher().GetGeneralSymbolId(),
      data_and_predictor->pos_matcher().GetFunctionalId(),
      data_and_predictor->pos_matcher().GetAdverbId(),
      data_and_predictor->pos_matcher().GetCounterSuffixWordId(),
  };

  for (int id : test_ids) {
    InitHistory("こみっと", "コミット");
    history_result_.rid = id;
    const ConversionRequest convreq = CreateConversionRequestWithOptions(
        {
            .request_type = ConversionRequest::PREDICTION,
            .max_dictionary_prediction_candidates_size = 100,
        },
        "");
    if (connector.GetTransitionCost(
            id, data_and_predictor->pos_matcher().GetGeneralNounId()) >
        kThreshold) {
      EXPECT_TRUE(predictor.Predict(convreq).empty());
    } else {
      const std::vector<Result> results = predictor.Predict(convreq);
      EXPECT_EQ(results.size(), 1);
      EXPECT_EQ(results[0].value, "テスト");
    }
  }
}

// Action to call the third argument of LookupPrefix/LookupPredictive with the
// token <key, value>.
struct InvokeCallbackWithOneToken {
  template <class T, class U>
  void operator()(T, U, DictionaryInterface::Callback* callback) {
    callback->OnToken(key, key, token);
  }

  std::string key;
  Token token;
};

void GenerateKeyEvents(absl::string_view text,
                       std::vector<commands::KeyEvent>* keys) {
  keys->clear();
  for (const char32_t codepoint : Util::Utf8ToUtf32(text)) {
    commands::KeyEvent key;
    if (codepoint <= 0x7F) {  // IsAscii, w is unsigned.
      key.set_key_code(codepoint);
    } else {
      key.set_key_code('?');
      *key.mutable_key_string() = Util::CodepointToUtf8(codepoint);
    }
    keys->push_back(key);
  }
}

void InsertInputSequence(absl::string_view text, composer::Composer* composer) {
  std::vector<commands::KeyEvent> keys;
  GenerateKeyEvents(text, &keys);

  for (size_t i = 0; i < keys.size(); ++i) {
    composer->InsertCharacterKeyEvent(keys[i]);
  }
}

PredictionTypes AddDefaultPredictionTypes(PredictionTypes types,
                                          bool is_mobile) {
  if (!is_mobile) {
    return types;
  }
  return types | REALTIME | PREFIX;
}

PredictionTypes GetMergedTypes(absl::Span<const Result> results) {
  PredictionTypes merged = NO_PREDICTION;
  for (const auto& result : results) {
    merged |= result.GetPredictionTypesForTesting();
  }
  return merged;
}

TEST_F(DictionaryPredictorTest, OnOffTest) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();
  {
    // turn off
    config_->set_use_dictionary_suggest(false);
    config_->set_use_realtime_conversion(false);

    const ConversionRequest convreq =
        CreateSuggestionConversionRequest("ぐーぐるあ");
    EXPECT_TRUE(predictor_peer.AggregateResultsForTesting(convreq).empty());
  }
  {
    // turn on
    config_->set_use_dictionary_suggest(true);
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest("ぐーぐるあ");
    EXPECT_FALSE(predictor_peer.AggregateResultsForTesting(convreq).empty());
  }
  {
    // empty query
    const ConversionRequest convreq = CreateSuggestionConversionRequest("");
    EXPECT_TRUE(predictor_peer.AggregateResultsForTesting(convreq).empty());
  }
}

TEST_F(DictionaryPredictorTest, PartialSuggestion) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(true);
  // turn on mobile mode
  request_->set_mixed_conversion(true);

  const ConversionRequest convreq = CreateConversionRequest(
      {.request_type = ConversionRequest::PARTIAL_SUGGESTION}, "ぐーぐるあ");
  EXPECT_FALSE(predictor_peer.AggregateResultsForTesting(convreq).empty());
}

TEST_F(DictionaryPredictorTest, PartialSuggestionWithRealtimeConversion) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(true);
  // turn on mobile mode
  request_->set_mixed_conversion(true);

  composer_->Reset();
  composer_->SetPreeditTextForTestOnly("ぐーぐるあ");
  composer_->MoveCursorLeft();

  const ConversionRequest convreq = CreateConversionRequest(
      {.request_type = ConversionRequest::PARTIAL_SUGGESTION,
       .use_actual_converter_for_realtime_conversion = true},
      "ぐーぐる", false /* init composer */);

  Result result;
  result.key = "ぐーぐる";
  result.value = "グーグル";
  result.attributes = REALTIME;
  MockRealtimeDecoder* realtime_decoder =
      data_and_predictor->mutable_realtime_decoder();
  ::testing::Mock::VerifyAndClearExpectations(realtime_decoder);
  EXPECT_CALL(*realtime_decoder, Decode(_))
      .WillOnce(Return(std::vector<Result>({result})));

  const std::vector<Result> results =
      predictor_peer.AggregateResultsForTesting(convreq);
  EXPECT_TRUE(GetMergedTypes(results) & REALTIME);
}

TEST_F(DictionaryPredictorTest, BigramTest) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  config_->set_use_dictionary_suggest(true);

  // history is "グーグル"
  InitHistory("ぐーぐる", "グーグル");

  // "グーグルアドセンス" will be returned.
  const ConversionRequest convreq = CreateSuggestionConversionRequest("あ");
  const std::vector<Result> results =
      predictor_peer.AggregateResultsForTesting(convreq);
  EXPECT_TRUE(BIGRAM | GetMergedTypes(results));
}

TEST_F(DictionaryPredictorTest, BigramTestWithZeroQuery) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  config_->set_use_dictionary_suggest(true);
  request_->set_zero_query_suggestion(true);

  // history is "グーグル"
  InitHistory("ぐーぐる", "グーグル");

  const ConversionRequest convreq = CreateSuggestionConversionRequest("");
  const std::vector<Result> results =
      predictor_peer.AggregateResultsForTesting(convreq);
  EXPECT_TRUE(BIGRAM | GetMergedTypes(results));
}

TEST_F(DictionaryPredictorTest, BigramTestWithZeroQueryFilterMode) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  config_->set_use_dictionary_suggest(true);
  request_->set_zero_query_suggestion(true);

  // history is "グーグル"
  InitHistory("ぐーぐる", "グーグル");

  const ConversionRequest convreq = CreateSuggestionConversionRequest("");
  const std::vector<Result> results =
      predictor_peer.AggregateResultsForTesting(convreq);
  EXPECT_FALSE(BIGRAM & GetMergedTypes(results));
}

// Check that previous candidate never be shown at the current candidate.
TEST_F(DictionaryPredictorTest, Regression3042706) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  config_->set_use_dictionary_suggest(true);

  // history is "きょうと/京都"
  InitHistory("きょうと", "京都");

  const ConversionRequest convreq = CreateSuggestionConversionRequest("だい");
  const std::vector<Result> results =
      predictor_peer.AggregateResultsForTesting(convreq);
  EXPECT_TRUE(REALTIME | GetMergedTypes(results));
  for (auto r : results) {
    EXPECT_FALSE(r.value.starts_with("京都"));
    EXPECT_TRUE(r.key.starts_with("だい"));
  }
}

enum Platform { DESKTOP, MOBILE };

class TriggerConditionsTest : public DictionaryPredictorTest,
                              public WithParamInterface<Platform> {};

TEST_P(TriggerConditionsTest, TriggerConditions) {
  const bool is_mobile = (GetParam() == MOBILE);

  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  std::vector<Result> results;

  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(false);
  if (is_mobile) {
    request_test_util::FillMobileRequest(request_.get());
  }

  // Set up realtime conversion.
  {
    Result result;
    result.key = "test";
    result.value = "test";
    result.attributes = REALTIME;
    MockRealtimeDecoder* realtime_decoder =
        data_and_predictor->mutable_realtime_decoder();
    ::testing::Mock::VerifyAndClearExpectations(realtime_decoder);
    EXPECT_CALL(*realtime_decoder, Decode(_))
        .WillRepeatedly(Return(std::vector<Result>({result})));
  }

  // Keys of normal lengths.
  {
    // Unigram is triggered in suggestion and prediction if key length (in UTF8
    // character count) is long enough.
    composer_->SetInputMode(transliteration::HIRAGANA);
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest("ぐーぐる");
    const std::vector<Result> results =
        predictor_peer.AggregateResultsForTesting(convreq);
    EXPECT_EQ(GetMergedTypes(results),
              AddDefaultPredictionTypes(UNIGRAM, is_mobile));
  }

  // Short keys.
  {
    if (is_mobile) {
      // Unigram is triggered even if key length is short.
      composer_->SetInputMode(transliteration::HIRAGANA);
      const ConversionRequest suggestion_convreq =
          CreateSuggestionConversionRequest("てす");
      const std::vector<Result> results1 =
          predictor_peer.AggregateResultsForTesting(suggestion_convreq);
      EXPECT_EQ(GetMergedTypes(results1), (UNIGRAM | REALTIME | PREFIX));

      const ConversionRequest prediction_convreq =
          CreatePredictionConversionRequest("てす");
      const std::vector<Result> results2 =
          predictor_peer.AggregateResultsForTesting(prediction_convreq);
      EXPECT_EQ(GetMergedTypes(results2), (UNIGRAM | REALTIME | PREFIX));
    } else {
      // Unigram is not triggered for SUGGESTION if key length is short.
      composer_->SetInputMode(transliteration::HIRAGANA);
      const ConversionRequest suggestion_convreq =
          CreateSuggestionConversionRequest("てす");
      EXPECT_TRUE(predictor_peer.AggregateResultsForTesting(suggestion_convreq)
                      .empty());
      const ConversionRequest prediction_convreq =
          CreatePredictionConversionRequest("てす");
      const std::vector<Result> results =
          predictor_peer.AggregateResultsForTesting(prediction_convreq);
      EXPECT_EQ(GetMergedTypes(results), UNIGRAM);
    }
  }

  // Zipcode-like keys.
  {
    composer_->SetInputMode(transliteration::HIRAGANA);
    const ConversionRequest convreq = CreateSuggestionConversionRequest("0123");
    EXPECT_TRUE(predictor_peer.AggregateResultsForTesting(convreq).empty());
  }

  // History is short => UNIGRAM
  {
    InitHistory("A", "A");
    composer_->SetInputMode(transliteration::HIRAGANA);
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest("てすとだ");
    const std::vector<Result> results =
        predictor_peer.AggregateResultsForTesting(convreq);
    EXPECT_EQ(GetMergedTypes(results),
              AddDefaultPredictionTypes(UNIGRAM, is_mobile));
  }

  // Both history and current segment are long => UNIGRAM or BIGRAM
  {
    InitHistory("これは", "これは");
    composer_->SetInputMode(transliteration::HIRAGANA);
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest("てすとだ");
    const std::vector<Result> results =
        predictor_peer.AggregateResultsForTesting(convreq);
    EXPECT_EQ(GetMergedTypes(results),
              AddDefaultPredictionTypes(UNIGRAM | BIGRAM, is_mobile));
  }

  // Current segment is short
  {
    if (is_mobile) {
      // For mobile, UNIGRAM and REALTIME are added to BIGRAM.
      InitHistory("てすとだよ", "テストだよ");
      composer_->SetInputMode(transliteration::HIRAGANA);
      const ConversionRequest convreq =
          CreateSuggestionConversionRequest("てす");
      const std::vector<Result> results =
          predictor_peer.AggregateResultsForTesting(convreq);
      EXPECT_EQ(GetMergedTypes(results),
                (UNIGRAM | BIGRAM | REALTIME | PREFIX));
    } else {
      // No UNIGRAM.
      InitHistory("てすとだよ", "テストだよ");
      composer_->SetInputMode(transliteration::HIRAGANA);
      const ConversionRequest convreq =
          CreateSuggestionConversionRequest("てす");
      const std::vector<Result> results =
          predictor_peer.AggregateResultsForTesting(convreq);
      EXPECT_EQ(GetMergedTypes(results), BIGRAM);
    }
  }

  // Typing correction shouldn't be appended.
  {
    composer_->SetInputMode(transliteration::HIRAGANA);
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest("ｐはよう");
    const std::vector<Result> results =
        predictor_peer.AggregateResultsForTesting(convreq);
    EXPECT_FALSE(TYPING_CORRECTION & GetMergedTypes(results));
  }

  // When romaji table is qwerty mobile => ENGLISH is included depending on
  // the language aware input setting.
  {
    const auto orig_input_mode = composer_->GetInputMode();
    const auto orig_table = request_->special_romanji_table();
    const auto orig_lang_aware = request_->language_aware_input();
    const bool orig_use_dictionary_suggest = config_->use_dictionary_suggest();

    composer_->SetInputMode(transliteration::HIRAGANA);
    config_->set_use_dictionary_suggest(true);

    // The case where romaji table is set to qwerty.  ENGLISH is turned on if
    // language aware input is enabled.
    for (const auto table :
         {commands::Request::QWERTY_MOBILE_TO_HIRAGANA,
          commands::Request::QWERTY_MOBILE_TO_HALFWIDTHASCII}) {
      config_->set_use_dictionary_suggest(orig_use_dictionary_suggest);
      request_->set_language_aware_input(orig_lang_aware);
      request_->set_special_romanji_table(orig_table);
      composer_->SetInputMode(orig_input_mode);

      request_->set_special_romanji_table(table);

      // Language aware input is default: No English prediction.
      request_->set_language_aware_input(
          commands::Request::DEFAULT_LANGUAGE_AWARE_BEHAVIOR);
      const ConversionRequest convreq1 =
          CreateSuggestionConversionRequest("てすとだよ");
      std::vector<Result> results;
      results = predictor_peer.AggregateResultsForTesting(convreq1);
      EXPECT_FALSE(GetMergedTypes(results) & ENGLISH);

      // Language aware input is off: No English prediction.
      request_->set_language_aware_input(
          commands::Request::NO_LANGUAGE_AWARE_INPUT);
      const ConversionRequest convreq2 =
          CreateSuggestionConversionRequest("てすとだよ");
      results = predictor_peer.AggregateResultsForTesting(convreq2);
      EXPECT_FALSE(GetMergedTypes(results) & ENGLISH);

      // Language aware input is on: English prediction is included.
      request_->set_language_aware_input(
          commands::Request::LANGUAGE_AWARE_SUGGESTION);
      const ConversionRequest convreq3 =
          CreateSuggestionConversionRequest("てすとだよ");
      results = predictor_peer.AggregateResultsForTesting(convreq3);
      EXPECT_FALSE(GetMergedTypes(results) & ENGLISH);
    }

    // The case where romaji table is not qwerty.  ENGLISH is turned off
    // regardless of language aware input setting.
    for (const auto table : {
             commands::Request::FLICK_TO_HALFWIDTHASCII,
             commands::Request::FLICK_TO_HIRAGANA,
             commands::Request::GODAN_TO_HALFWIDTHASCII,
             commands::Request::GODAN_TO_HIRAGANA,
             commands::Request::NOTOUCH_TO_HALFWIDTHASCII,
             commands::Request::NOTOUCH_TO_HIRAGANA,
             commands::Request::TOGGLE_FLICK_TO_HALFWIDTHASCII,
             commands::Request::TOGGLE_FLICK_TO_HIRAGANA,
             commands::Request::TWELVE_KEYS_TO_HALFWIDTHASCII,
             commands::Request::TWELVE_KEYS_TO_HIRAGANA,
         }) {
      config_->set_use_dictionary_suggest(orig_use_dictionary_suggest);
      request_->set_language_aware_input(orig_lang_aware);
      request_->set_special_romanji_table(orig_table);
      composer_->SetInputMode(orig_input_mode);

      request_->set_special_romanji_table(table);

      // Language aware input is default.
      request_->set_language_aware_input(
          commands::Request::DEFAULT_LANGUAGE_AWARE_BEHAVIOR);
      const ConversionRequest convreq1 =
          CreateSuggestionConversionRequest("てすとだよ");
      std::vector<Result> results;
      results = predictor_peer.AggregateResultsForTesting(convreq1);
      EXPECT_FALSE(GetMergedTypes(results) & ENGLISH);

      // Language aware input is off.
      request_->set_language_aware_input(
          commands::Request::NO_LANGUAGE_AWARE_INPUT);
      const ConversionRequest convreq2 =
          CreateSuggestionConversionRequest("てすとだよ");
      results = predictor_peer.AggregateResultsForTesting(convreq2);
      EXPECT_FALSE(GetMergedTypes(results) & ENGLISH);

      // Language aware input is on.
      request_->set_language_aware_input(
          commands::Request::LANGUAGE_AWARE_SUGGESTION);
      const ConversionRequest convreq3 =
          CreateSuggestionConversionRequest("てすとだよ");
      results = predictor_peer.AggregateResultsForTesting(convreq3);
      EXPECT_FALSE(GetMergedTypes(results) & ENGLISH);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    TriggerConditionsForPlatforms, TriggerConditionsTest,
    ::testing::Values(DESKTOP, MOBILE),
    [](const ::testing::TestParamInfo<TriggerConditionsTest::ParamType>& info) {
      if (info.param == DESKTOP) {
        return "DESKTOP";
      }
      return "MOBILE";
    });

TEST_F(DictionaryPredictorTest, TriggerConditionsLatinInputMode) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  struct TestCase {
    Platform platform;
    transliteration::TransliterationType input_mode;
  } kTestCases[] = {
      {DESKTOP, transliteration::HALF_ASCII},
      {DESKTOP, transliteration::FULL_ASCII},
      {MOBILE, transliteration::HALF_ASCII},
      {MOBILE, transliteration::FULL_ASCII},
  };

  for (const auto& test_case : kTestCases) {
    config::ConfigHandler::GetDefaultConfig(config_.get());
    // Resets to default value.
    // Implementation note: Since the value of |request_| is used to initialize
    // composer_ and convreq_, it is not safe to reset |request_| with new
    // instance.
    request_->Clear();
    const bool is_mobile = test_case.platform == MOBILE;
    if (is_mobile) {
      request_test_util::FillMobileRequest(request_.get());
    }

    std::vector<Result> results;

    // Implementation note: SetUpInputForSuggestion() resets the state of
    // composer. So we have to call SetInputMode() after this method.
    composer_->SetInputMode(test_case.input_mode);

    config_->set_use_dictionary_suggest(true);

    // Input mode is Latin(HALF_ASCII or FULL_ASCII) => ENGLISH
    config_->set_use_realtime_conversion(false);
    const ConversionRequest convreq1 = CreateSuggestionConversionRequest("hel");
    results = predictor_peer.AggregateResultsForTesting(convreq1);
    EXPECT_EQ(GetMergedTypes(results),
              AddDefaultPredictionTypes(ENGLISH, is_mobile));

    config_->set_use_realtime_conversion(true);
    const ConversionRequest convreq2 = CreateSuggestionConversionRequest("hel");
    results = predictor_peer.AggregateResultsForTesting(convreq2);
    EXPECT_EQ(GetMergedTypes(results),
              AddDefaultPredictionTypes(ENGLISH | REALTIME, is_mobile));

    // When dictionary suggest is turned off, English prediction should be
    // disabled.
    config_->set_use_dictionary_suggest(false);
    const ConversionRequest convreq3 = CreateSuggestionConversionRequest("hel");
    EXPECT_TRUE(predictor_peer.AggregateResultsForTesting(convreq3).empty());

    // Has realtime results for PARTIAL_SUGGESTION request.
    config_->set_use_dictionary_suggest(true);
    const ConversionRequest partial_suggestion_convreq =
        CreateConversionRequest(
            {.request_type = ConversionRequest::PARTIAL_SUGGESTION}, "hel");
    results =
        predictor_peer.AggregateResultsForTesting(partial_suggestion_convreq);
    EXPECT_EQ(GetMergedTypes(results), REALTIME);
  }
}

TEST_F(DictionaryPredictorTest, AggregateUnigramCandidate) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  constexpr absl::string_view kKey = "ぐーぐるあ";

  const ConversionRequest convreq = CreateSuggestionConversionRequest(kKey);
  std::vector<Result> results;
  int min_unigram_key_len = 0;
  predictor_peer.AggregateUnigram(convreq, &results, &min_unigram_key_len);
  EXPECT_FALSE(results.empty());

  for (const auto& result : results) {
    EXPECT_EQ(result.GetPredictionTypesForTesting(), UNIGRAM);
    EXPECT_TRUE(result.key.starts_with(kKey));
  }
}

TEST_F(DictionaryPredictorTest, LookupUnigramCandidateForMixedConversion) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  constexpr absl::string_view kHiraganaA = "あ";
  constexpr absl::string_view kHiraganaAA = "ああ";
  constexpr auto kCost = MockDictionary::kDefaultCost;
  constexpr auto kPosId = MockDictionary::kDefaultPosId;
  const uint16_t kUnknownId = data_and_predictor->pos_matcher().GetUnknownId();

  const std::vector<Token> a_tokens = {
      // A system dictionary entry "a".
      {kHiraganaA, "a", kCost, kPosId, kPosId, Token::NONE},
      // System dictionary entries "a0", ..., "a9", which are detected as
      // redundant
      // by MaybeRedundant(); see dictionary_predictor.cc.
      {kHiraganaA, "a0", kCost, kPosId, kPosId, Token::NONE},
      {kHiraganaA, "a1", kCost, kPosId, kPosId, Token::NONE},
      {kHiraganaA, "a2", kCost, kPosId, kPosId, Token::NONE},
      {kHiraganaA, "a3", kCost, kPosId, kPosId, Token::NONE},
      {kHiraganaA, "a4", kCost, kPosId, kPosId, Token::NONE},
      {kHiraganaA, "a5", kCost, kPosId, kPosId, Token::NONE},
      {kHiraganaA, "a6", kCost, kPosId, kPosId, Token::NONE},
      {kHiraganaA, "a7", kCost, kPosId, kPosId, Token::NONE},
      {kHiraganaA, "a8", kCost, kPosId, kPosId, Token::NONE},
      {kHiraganaA, "a9", kCost, kPosId, kPosId, Token::NONE},
      // A user dictionary entry "aaa".  MaybeRedundant() detects this entry as
      // redundant but it should not be filtered in prediction.
      {kHiraganaA, "aaa", kCost, kPosId, kPosId, Token::USER_DICTIONARY},
      {kHiraganaAA, "bbb", 0, kUnknownId, kUnknownId, Token::USER_DICTIONARY},
  };
  const std::vector<Token> aa_tokens = {
      {kHiraganaAA, "bbb", 0, kUnknownId, kUnknownId, Token::USER_DICTIONARY},
  };

  MockDictionary* mock_dict = data_and_predictor->mutable_dictionary();
  EXPECT_CALL(*mock_dict, LookupPredictive(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(*mock_dict, LookupPredictive(StrEq(kHiraganaA), _, _))
      .WillRepeatedly(InvokeCallbackWithTokens{a_tokens});
  EXPECT_CALL(*mock_dict, LookupPredictive(StrEq(kHiraganaAA), _, _))
      .WillRepeatedly(InvokeCallbackWithTokens{aa_tokens});

  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(false);
  table_->LoadFromFile("system://12keys-hiragana.tsv");

  auto is_user_dictionary_result = [](const Result& res) {
    return (res.attributes & Attribute::USER_DICTIONARY) != 0;
  };

  {
    // Test prediction from input あ.
    InsertInputSequence(kHiraganaA, composer_.get());

    std::vector<Result> results;
    const ConversionRequest convreq = CreatePredictionConversionRequest(
        kHiraganaA, false /* init_composer */);
    int min_unigram_key_len = 0;
    predictor_peer.AggregateUnigram(convreq, &results, &min_unigram_key_len);

    // Check if "aaa" is not filtered.
    auto iter =
        std::find_if(results.begin(), results.end(), [&](const Result& res) {
          return res.key == kHiraganaA && res.value == "aaa" &&
                 is_user_dictionary_result(res);
        });
    EXPECT_NE(results.end(), iter);

    // "bbb" is looked up from input "あ" but it will be filtered because it is
    // from user dictionary with unknown POS ID.
    iter = std::find_if(results.begin(), results.end(), [&](const Result& res) {
      return res.key == kHiraganaAA && res.value == "bbb" &&
             is_user_dictionary_result(res);
    });
    EXPECT_EQ(iter, results.end());
  }

  {
    // Test prediction from input ああ.
    composer_->Reset();
    InsertInputSequence(kHiraganaAA, composer_.get());

    std::vector<Result> results;
    const ConversionRequest convreq = CreatePredictionConversionRequest(
        kHiraganaAA, false /* init_composer */);
    int min_unigram_key_len = 0;
    predictor_peer.AggregateUnigram(convreq, &results, &min_unigram_key_len);

    // Check if "aaa" is not found as its key is あ.
    auto iter =
        std::find_if(results.begin(), results.end(), [&](const Result& res) {
          return res.key == kHiraganaA && res.value == "aaa" &&
                 is_user_dictionary_result(res);
        });
    EXPECT_EQ(iter, results.end());

    // Unlike the above case for "あ", "bbb" is now found because input key is
    // exactly "ああ".
    iter = std::find_if(results.begin(), results.end(), [&](const Result& res) {
      return res.key == kHiraganaAA && res.value == "bbb" &&
             is_user_dictionary_result(res);
    });
    EXPECT_NE(results.end(), iter);
  }
}

// We are not sure what should we suggest after the end of sentence for now.
// However, we decided to show zero query suggestion rather than stopping
// zero query completely. Users may be confused if they cannot see suggestion
// window only after the certain conditions.
// TODO(toshiyuki): Show useful zero query suggestions after EOS.
TEST_F(DictionaryPredictorTest, DISABLED_MobileZeroQueryAfterEOS) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  const PosMatcher& pos_matcher = data_and_predictor->pos_matcher();

  const struct TestCase {
    const char* key;
    const char* value;
    int rid;
    bool expected_result;
  } kTestcases[] = {
      {"ですよね｡", "ですよね。", pos_matcher.GetEOSSymbolId(), false},
      {"｡", "。", pos_matcher.GetEOSSymbolId(), false},
      {"まるいち", "①", pos_matcher.GetEOSSymbolId(), false},
      {"そう", "そう", pos_matcher.GetGeneralNounId(), true},
      {"そう!", "そう！", pos_matcher.GetGeneralNounId(), false},
      {"むすめ。", "娘。", pos_matcher.GetUniqueNounId(), true},
  };

  request_test_util::FillMobileRequest(request_.get());

  for (const auto& test_case : kTestcases) {
    InitHistory(test_case.key, test_case.value, test_case.rid);
    const ConversionRequest convreq = CreatePredictionConversionRequest("");
    const std::vector<Result> results =
        predictor_peer.AggregateResultsForTesting(convreq);
    EXPECT_EQ(!results.empty(), test_case.expected_result);
  }
}

TEST_F(DictionaryPredictorTest, AggregateBigramPrediction) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  {
    // history is "グーグル"
    constexpr absl::string_view kHistoryKey = "ぐーぐる";
    constexpr absl::string_view kHistoryValue = "グーグル";

    InitHistory(kHistoryKey, kHistoryValue);

    std::vector<Result> results;

    const ConversionRequest convreq = CreateSuggestionConversionRequest("あ");
    predictor_peer.AggregateBigram(convreq, &results);
    EXPECT_FALSE(results.empty());

    for (size_t i = 0; i < results.size(); ++i) {
      // "グーグルアドセンス", "グーグル", "アドセンス"
      // are in the dictionary.
      if (results[i].value == "アドセンス") {
        EXPECT_FALSE(results[i].removed);
      } else {
        EXPECT_TRUE(results[i].removed);
      }
      EXPECT_EQ(results[i].GetPredictionTypesForTesting(), BIGRAM);
      EXPECT_FALSE(results[i].key.starts_with(kHistoryKey));
      EXPECT_FALSE(results[i].value.starts_with(kHistoryValue));
      EXPECT_TRUE(results[i].key.starts_with("あ"));
      EXPECT_TRUE(results[i].value.starts_with("ア"));
    }
  }

  {
    constexpr absl::string_view kHistoryKey = "てす";
    constexpr absl::string_view kHistoryValue = "テス";

    InitHistory(kHistoryKey, kHistoryValue);

    std::vector<Result> results;

    const ConversionRequest convreq = CreateSuggestionConversionRequest("あ");
    predictor_peer.AggregateBigram(convreq, &results);
    EXPECT_TRUE(results.empty());
  }
}

// Zero query bigram is deprecated and disabled.
// Keep this test to confirm that no suggestions are shown.
TEST_F(DictionaryPredictorTest, AggregateZeroQueryBigramPrediction) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();
  request_test_util::FillMobileRequest(request_.get());

  {
    // history is "グーグル"
    constexpr absl::string_view kHistoryKey = "ぐーぐる";
    constexpr absl::string_view kHistoryValue = "グーグル";

    InitHistory(kHistoryKey, kHistoryValue);

    std::vector<Result> results;

    const ConversionRequest convreq = CreateSuggestionConversionRequest("");
    predictor_peer.AggregateBigram(convreq, &results);
    EXPECT_TRUE(results.empty());
  }

  {
    constexpr absl::string_view kHistory = "ありがとう";

    MockDictionary* mock = data_and_predictor->mutable_dictionary();
    EXPECT_CALL(*mock, LookupPrefix(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(*mock, LookupPredictive(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(*mock, LookupPrefix(StrEq(kHistory), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {kHistory, kHistory},
        }});
    EXPECT_CALL(*mock, LookupPredictive(StrEq(kHistory), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"ありがとうございます", "ありがとうございます"},
            {"ありがとうございます", "ありがとう御座います"},
            {"ありがとうございました", "ありがとうございました"},
            {"ありがとうございました", "ありがとう御座いました"},

            {"ございます", "ございます"},
            {"ございます", "御座います"},
            // ("ございました", "ございました") is not in the dictionary.
            {"ございました", "御座いました"},

            // Word less than 10.
            {"ありがとうね", "ありがとうね"},
            {"ね", "ね"},
        }});
    EXPECT_CALL(*mock, HasKey(StrEq("ございます")))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock, HasKey(StrEq("ございました")))
        .WillRepeatedly(Return(true));

    InitHistory(kHistory, kHistory);

    std::vector<Result> results;

    const ConversionRequest convreq = CreateSuggestionConversionRequest("");
    predictor_peer.AggregateBigram(convreq, &results);
    EXPECT_TRUE(results.empty());
  }
}

TEST_F(DictionaryPredictorTest, AggregateZeroQueryPredictionLatinInputMode) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();
  request_test_util::FillMobileRequest(request_.get());

  {
    // Zero query
    composer_->SetInputMode(transliteration::HALF_ASCII);

    // No history
    constexpr absl::string_view kHistoryKey = "";
    constexpr absl::string_view kHistoryValue = "";

    InitHistory(kHistoryKey, kHistoryValue);

    std::vector<Result> results;

    const ConversionRequest convreq = CreateSuggestionConversionRequest("");
    predictor_peer.AggregateZeroQuery(convreq, &results);
    EXPECT_TRUE(results.empty());
  }

  {
    // Zero query
    composer_->SetInputMode(transliteration::HALF_ASCII);

    constexpr absl::string_view kHistoryKey = "when";
    constexpr absl::string_view kHistoryValue = "when";

    InitHistory(kHistoryKey, kHistoryValue);

    std::vector<Result> results;

    const ConversionRequest convreq = CreateSuggestionConversionRequest("");
    predictor_peer.AggregateZeroQuery(convreq, &results);
    EXPECT_TRUE(results.empty());
  }

  {
    // Zero query
    composer_->SetInputMode(transliteration::HALF_ASCII);

    // We can input numbers from Latin input mode.
    constexpr absl::string_view kHistoryKey = "12";
    constexpr absl::string_view kHistoryValue = "12";

    InitHistory(kHistoryKey, kHistoryValue);

    std::vector<Result> results;

    const ConversionRequest convreq = CreateSuggestionConversionRequest("");
    predictor_peer.AggregateZeroQuery(convreq, &results);
    EXPECT_FALSE(results.empty());  // Should have results.
  }

  {
    // Zero query
    composer_->SetInputMode(transliteration::HALF_ASCII);

    // We can input some symbols from Latin input mode.
    constexpr absl::string_view kHistoryKey = "@";
    constexpr absl::string_view kHistoryValue = "@";

    InitHistory(kHistoryKey, kHistoryValue);

    std::vector<Result> results;

    const ConversionRequest convreq = CreateSuggestionConversionRequest("");
    predictor_peer.AggregateZeroQuery(convreq, &results);
    EXPECT_FALSE(results.empty());  // Should have results.
  }
}

TEST_F(DictionaryPredictorTest, GetRealtimeCandidateMaxSize) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  // GetRealtimeCandidateMaxSize has some heuristics so here we test following
  // conditions.
  // - The result must be equal or less than kMaxSize;
  // - If mixed_conversion is the same, the result of SUGGESTION is
  //        equal or less than PREDICTION.
  // - If mixed_conversion is the same, the result of PARTIAL_SUGGESTION is
  //        equal or less than PARTIAL_PREDICTION.
  // - Partial version has equal or greater than non-partial version.

  constexpr size_t kMaxSize = 100;

  request_->Clear();
  const ConversionRequest suggestion_convreq_no_mixed =
      CreateConversionRequest({
          .request_type = ConversionRequest::SUGGESTION,
          .max_dictionary_prediction_candidates_size = kMaxSize,
      });
  const ConversionRequest prediction_convreq_no_mixed =
      CreatePredictionConversionRequest("");

  request_test_util::FillMobileRequest(request_.get());
  const ConversionRequest suggestion_convreq_mixed = CreateConversionRequest({
      .request_type = ConversionRequest::SUGGESTION,
      .max_dictionary_prediction_candidates_size = kMaxSize,
  });
  const ConversionRequest prediction_convreq_mixed =
      CreatePredictionConversionRequest("");

  // non-partial, non-mixed-conversion
  const size_t prediction_no_mixed =
      predictor_peer.GetRealtimeCandidateMaxSize(prediction_convreq_no_mixed);
  EXPECT_GE(kMaxSize, prediction_no_mixed);

  const size_t suggestion_no_mixed =
      predictor_peer.GetRealtimeCandidateMaxSize(suggestion_convreq_no_mixed);
  EXPECT_GE(kMaxSize, suggestion_no_mixed);
  EXPECT_LE(suggestion_no_mixed, prediction_no_mixed);

  // non-partial, mixed-conversion
  const size_t prediction_mixed =
      predictor_peer.GetRealtimeCandidateMaxSize(prediction_convreq_mixed);
  EXPECT_GE(kMaxSize, prediction_mixed);

  const size_t suggestion_mixed =
      predictor_peer.GetRealtimeCandidateMaxSize(suggestion_convreq_mixed);
  EXPECT_GE(kMaxSize, suggestion_mixed);

  // partial, non-mixed-conversion
  request_->Clear();
  const ConversionRequest partial_suggestion_convreq_no_mixed =
      CreateConversionRequest(
          {.request_type = ConversionRequest::PARTIAL_SUGGESTION});
  const ConversionRequest partial_prediction_convreq_no_mixed =
      CreateConversionRequest(
          {.request_type = ConversionRequest::PARTIAL_PREDICTION});

  request_test_util::FillMobileRequest(request_.get());
  const ConversionRequest partial_suggestion_convreq_mixed =
      CreateConversionRequest(
          {.request_type = ConversionRequest::PARTIAL_SUGGESTION});
  const ConversionRequest partial_prediction_convreq_mixed =
      CreateConversionRequest(
          {.request_type = ConversionRequest::PARTIAL_PREDICTION});

  const size_t partial_prediction_no_mixed =
      predictor_peer.GetRealtimeCandidateMaxSize(
          partial_prediction_convreq_no_mixed);
  EXPECT_GE(kMaxSize, partial_prediction_no_mixed);

  const size_t partial_suggestion_no_mixed =
      predictor_peer.GetRealtimeCandidateMaxSize(
          partial_suggestion_convreq_no_mixed);
  EXPECT_GE(kMaxSize, partial_suggestion_no_mixed);
  EXPECT_LE(partial_suggestion_no_mixed, partial_prediction_no_mixed);

  // partial, mixed-conversion
  const size_t partial_prediction_mixed =
      predictor_peer.GetRealtimeCandidateMaxSize(
          partial_prediction_convreq_mixed);
  EXPECT_GE(kMaxSize, partial_prediction_mixed);

  const size_t partial_suggestion_mixed =
      predictor_peer.GetRealtimeCandidateMaxSize(
          partial_suggestion_convreq_mixed);
  EXPECT_GE(kMaxSize, partial_suggestion_mixed);
  EXPECT_LE(partial_suggestion_mixed, partial_prediction_mixed);

  EXPECT_GE(partial_prediction_no_mixed, prediction_no_mixed);
  EXPECT_GE(partial_prediction_mixed, prediction_mixed);
  EXPECT_GE(partial_suggestion_no_mixed, suggestion_no_mixed);
  EXPECT_GE(partial_suggestion_mixed, suggestion_mixed);
}

TEST_F(DictionaryPredictorTest, GetRealtimeCandidateMaxSizeForMixed) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  constexpr size_t kMaxSize = 100;

  request_test_util::FillMobileRequest(
      request_.get());  // enables mixed conversion.
  const ConversionRequest suggestion_short_convreq = CreateConversionRequest(
      {.request_type = ConversionRequest::SUGGESTION,
       .max_dictionary_prediction_candidates_size = kMaxSize},
      "short");
  const ConversionRequest prediction_short_convreq = CreateConversionRequest(
      {.request_type = ConversionRequest::PREDICTION,
       .max_dictionary_prediction_candidates_size = kMaxSize},
      "short");

  // for short key, try to provide many results as possible
  const size_t short_suggestion_mixed =
      predictor_peer.GetRealtimeCandidateMaxSize(suggestion_short_convreq);
  EXPECT_GE(kMaxSize, short_suggestion_mixed);

  const size_t short_prediction_mixed =
      predictor_peer.GetRealtimeCandidateMaxSize(prediction_short_convreq);
  EXPECT_GE(kMaxSize, short_prediction_mixed);

  const ConversionRequest suggestion_long_convreq = CreateConversionRequest(
      {.request_type = ConversionRequest::SUGGESTION,
       .max_dictionary_prediction_candidates_size = kMaxSize},
      "long_request_key");
  const ConversionRequest prediction_long_convreq = CreateConversionRequest(
      {.request_type = ConversionRequest::PREDICTION,
       .max_dictionary_prediction_candidates_size = kMaxSize},
      "long_request_key");

  const size_t long_suggestion_mixed =
      predictor_peer.GetRealtimeCandidateMaxSize(suggestion_long_convreq);
  EXPECT_GE(kMaxSize, long_suggestion_mixed);
  EXPECT_GT(short_suggestion_mixed, long_suggestion_mixed);

  const size_t long_prediction_mixed =
      predictor_peer.GetRealtimeCandidateMaxSize(prediction_long_convreq);
  EXPECT_GE(kMaxSize, long_prediction_mixed);
  EXPECT_GT(kMaxSize, long_prediction_mixed + long_suggestion_mixed);
  EXPECT_GT(short_prediction_mixed, long_prediction_mixed);
}

TEST_F(DictionaryPredictorTest, AggregateRealtimeConversion) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  data_and_predictor->Init();

  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  constexpr absl::string_view kKey = "わたしのなまえはなかのです";

  for (int candidates_size : {10, 20}) {
    for (bool use_actual_converter : {false, true}) {
      {
        std::vector<Result> results(1);
        results[0].key = kKey;
        results[0].value = "私の名前は中野です";
        results[0].attributes = Attribute::REALTIME_CONVERSION |
                                Attribute::REALTIME_TOP |
                                Attribute::NO_VARIANTS_EXPANSION;

        EXPECT_CALL(
            *data_and_predictor->mutable_realtime_decoder(),
            Decode(Truly([&](const ConversionRequest& request) {
              return (request.options().max_conversion_candidates_size ==
                          candidates_size &&
                      request.options()
                              .use_actual_converter_for_realtime_conversion ==
                          use_actual_converter);
            })))
            .WillRepeatedly(Return(results));
      }

      const ConversionRequest convreq = CreateSuggestionConversionRequest(kKey);
      std::vector<Result> results;
      predictor_peer.AggregateRealtime(convreq, candidates_size,
                                       use_actual_converter, &results);
      ASSERT_EQ(results.size(), 1);
      EXPECT_EQ(results[0].GetPredictionTypesForTesting(),
                REALTIME | REALTIME_TOP);
      EXPECT_EQ(results[0].key, kKey);
      EXPECT_TRUE(results[0].attributes & Attribute::NO_VARIANTS_EXPANSION);
    }
  }
}

namespace {
struct SimpleSuffixToken {
  absl::string_view key;
  absl::string_view value;
};

const SimpleSuffixToken kSuffixTokens[] = {{"いか", "以下"}};

class TestSuffixDictionary : public DictionaryInterface {
 public:
  TestSuffixDictionary() = default;
  ~TestSuffixDictionary() override = default;

  bool HasKey(absl::string_view value) const override { return false; }

  bool HasValue(absl::string_view value) const override { return false; }

  void LookupPredictive(absl::string_view key, const ConversionOptions& options,
                        Callback* callback) const override {
    Token token;
    for (size_t i = 0; i < std::size(kSuffixTokens); ++i) {
      const SimpleSuffixToken& suffix_token = kSuffixTokens[i];
      if (!key.empty() && !suffix_token.key.starts_with(key)) {
        continue;
      }
      switch (callback->OnKey(suffix_token.key)) {
        case Callback::TRAVERSE_DONE:
          return;
        case Callback::TRAVERSE_NEXT_KEY:
          continue;
        case Callback::TRAVERSE_CULL:
          LOG(FATAL) << "Culling is not supported.";
          break;
        default:
          break;
      }
      token.key = suffix_token.key;
      token.value = suffix_token.value;
      token.cost = 1000;
      token.lid = token.rid = 0;
      if (callback->OnToken(token.key, token.key, token) ==
          Callback::TRAVERSE_DONE) {
        break;
      }
    }
  }

  void LookupPrefix(absl::string_view key, const ConversionOptions& options,
                    Callback* callback) const override {}

  void LookupExact(absl::string_view key, const ConversionOptions& options,
                   Callback* callback) const override {}

  void LookupReverse(absl::string_view str, const ConversionOptions& options,
                     Callback* callback) const override {}
};

}  // namespace

TEST_F(DictionaryPredictorTest, AggregateSuffixPrediction) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  data_and_predictor->Init(std::make_unique<TestSuffixDictionary>(), nullptr);

  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  request_->set_zero_query_suggestion(true);

  // history is "グーグル"
  constexpr absl::string_view kHistoryKey = "ぐーぐる";
  constexpr absl::string_view kHistoryValue = "グーグル";

  // Since SuffixDictionary only returns for key "い", the result
  // should be empty for "あ".
  std::vector<Result> results;
  InitHistory(kHistoryKey, kHistoryValue);
  const ConversionRequest convreq1 = CreateSuggestionConversionRequest("あ");
  predictor_peer.AggregateZeroQuery(convreq1, &results);
  EXPECT_TRUE(results.empty());

  // Candidates generated by AggregateSuffixPrediction from nonempty
  // key should have SUFFIX type.
  results.clear();
  InitHistory(kHistoryKey, kHistoryValue);
  composer_->Reset();
  const ConversionRequest convreq2 = CreateSuggestionConversionRequest("い");
  predictor_peer.AggregateZeroQuery(convreq2, &results);
  EXPECT_FALSE(results.empty());
  EXPECT_TRUE(GetMergedTypes(results) & SUFFIX);
  for (const auto& result : results) {
    EXPECT_EQ(result.GetPredictionTypesForTesting(), SUFFIX);
  }
}

TEST_F(DictionaryPredictorTest, AggregateZeroQuerySuffixPrediction) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  data_and_predictor->Init(std::make_unique<TestSuffixDictionary>(), nullptr);

  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();
  request_test_util::FillMobileRequest(request_.get());

  // history is "グーグル"
  constexpr absl::string_view kHistoryKey = "ぐーぐる";
  constexpr absl::string_view kHistoryValue = "グーグル";

  InitHistory(kHistoryKey, kHistoryValue);

  {
    std::vector<Result> results;

    // Candidates generated by AggregateZeroQuerySuffixPrediction should
    // have SUFFIX type.
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest("", false /* init_composer */);
    predictor_peer.AggregateZeroQuery(convreq, &results);
    EXPECT_FALSE(results.empty());
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_EQ(results[i].GetPredictionTypesForTesting(), SUFFIX);
    }
  }
  {
    // If the feature is disabled and `results` is nonempty, nothing should be
    // generated.
    request_->mutable_decoder_experiment_params()
        ->set_disable_zero_query_suffix_prediction(true);
    std::vector<Result> results = {Result()};
    const ConversionRequest convreq = CreateSuggestionConversionRequest("");
    predictor_peer.AggregateZeroQuery(convreq, &results);
    EXPECT_EQ(results.size(), 1);
  }
  {
    // Suffix entries should be aggregated for handwriting
    request_->set_is_handwriting(true);
    std::vector<Result> results = {Result()};
    const ConversionRequest convreq = CreateSuggestionConversionRequest("");
    predictor_peer.AggregateZeroQuery(convreq, &results);
    EXPECT_FALSE(results.empty());
  }
}

struct EnglishPredictionTestEntry {
  std::string name;
  transliteration::TransliterationType input_mode;
  std::string key;
  std::string expected_prefix;
  std::vector<std::string> expected_values;
};

class AggregateEnglishPredictionTest
    : public DictionaryPredictorTest,
      public WithParamInterface<EnglishPredictionTestEntry> {};

TEST_P(AggregateEnglishPredictionTest, AggregateEnglishPrediction) {
  const EnglishPredictionTestEntry& entry = GetParam();
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  table_->LoadFromFile("system://romanji-hiragana.tsv");
  composer_->Reset();
  composer_->SetInputMode(entry.input_mode);
  InsertInputSequence(entry.key, composer_.get());

  std::vector<Result> results;

  const ConversionRequest convreq =
      CreatePredictionConversionRequest(entry.key, false /* init_composer */);
  predictor_peer.AggregateEnglish(convreq, &results);

  std::set<std::string> values;
  for (const auto& result : results) {
    EXPECT_EQ(result.GetPredictionTypesForTesting(), ENGLISH);
    EXPECT_TRUE(result.value.starts_with(entry.expected_prefix))
        << result.value << " doesn't start with " << entry.expected_prefix;
    values.insert(result.value);
  }
  for (const auto& expected_value : entry.expected_values) {
    EXPECT_TRUE(values.find(expected_value) != values.end())
        << expected_value << " isn't in the results";
  }
}

const std::vector<EnglishPredictionTestEntry>* kEnglishPredictionTestEntries =
    new std::vector<EnglishPredictionTestEntry>(
        {{"HALF_ASCII_lower_case",
          transliteration::HALF_ASCII,
          "conv",
          "conv",
          {"converge", "converged", "convergent"}},
         {"HALF_ASCII_upper_case",
          transliteration::HALF_ASCII,
          "CONV",
          "CONV",
          {"CONVERGE", "CONVERGED", "CONVERGENT"}},
         {"HALF_ASCII_capitalized",
          transliteration::HALF_ASCII,
          "Conv",
          "Conv",
          {"Converge", "Converged", "Convergent"}},
         {"FULL_ASCII_lower_case",
          transliteration::FULL_ASCII,
          "conv",
          "ｃｏｎｖ",
          {"ｃｏｎｖｅｒｇｅ", "ｃｏｎｖｅｒｇｅｄ", "ｃｏｎｖｅｒｇｅｎｔ"}},
         {"FULL_ASCII_upper_case",
          transliteration::FULL_ASCII,
          "CONV",
          "ＣＯＮＶ",
          {"ＣＯＮＶＥＲＧＥ", "ＣＯＮＶＥＲＧＥＤ", "ＣＯＮＶＥＲＧＥＮＴ"}},
         {"FULL_ASCII_capitalized",
          transliteration::FULL_ASCII,
          "Conv",
          "Ｃｏｎｖ",
          {"Ｃｏｎｖｅｒｇｅ", "Ｃｏｎｖｅｒｇｅｄ", "Ｃｏｎｖｅｒｇｅｎｔ"}}});

INSTANTIATE_TEST_SUITE_P(AggregateEnglishPredictioForInputMode,
                         AggregateEnglishPredictionTest,
                         ::testing::ValuesIn(*kEnglishPredictionTestEntries),
                         [](const ::testing::TestParamInfo<
                             AggregateEnglishPredictionTest::ParamType>& info) {
                           return info.param.name;
                         });

TEST_F(DictionaryPredictorTest, AggregateExtendedTypeCorrectingPrediction) {
  auto mock = std::make_unique<engine::MockSupplementalModel>();

  std::vector<TypeCorrectedQuery> expected;

  auto add_expected = [&](const std::string& key, uint8_t type) {
    expected.emplace_back(TypeCorrectedQuery{key, type});
  };

  add_expected("よろしく", TypeCorrectedQuery::CORRECTION);
  add_expected("よろざく",
               TypeCorrectedQuery::CORRECTION |
                   TypeCorrectedQuery::KANA_MODIFIER_INSENTIVE_ONLY);
  add_expected("よろさくです", TypeCorrectedQuery::COMPLETION);
  add_expected("よろしくです",
               TypeCorrectedQuery::CORRECTION | TypeCorrectedQuery::COMPLETION);
  add_expected("よろざくです",
               TypeCorrectedQuery::CORRECTION | TypeCorrectedQuery::COMPLETION |
                   TypeCorrectedQuery::KANA_MODIFIER_INSENTIVE_ONLY);

  EXPECT_CALL(*mock, CorrectComposition(_)).WillOnce(Return(expected));

  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData(/*suffix dictionary=*/nullptr,
                                  std::move(mock));
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  config_->set_use_typing_correction(true);

  InitHistory("ほんじつは", "本日は");
  ConversionRequest convreq = CreatePredictionConversionRequest("よろさく");
  const std::vector<Result> results =
      predictor_peer.AggregateTypingCorrectedResultsForMixedConversion(convreq);

  EXPECT_EQ(results.size(), 5);
  for (int i = 0; i < results.size(); ++i) {
    EXPECT_EQ(results[i].key, expected[i].correction);
    if (i == 2) {
      // "よろさくです" is COMPLETION only.
      EXPECT_FALSE(results[i].attributes & TYPING_CORRECTION);
    } else {
      EXPECT_TRUE(results[i].attributes & TYPING_CORRECTION);
    }
  }
}

TEST_F(DictionaryPredictorTest,
       AggregateExtendedTypeCorrectingPredictionWithCharacterForm) {
  auto mock = std::make_unique<engine::MockSupplementalModel>();

  std::vector<TypeCorrectedQuery> expected;
  expected.emplace_back(
      TypeCorrectedQuery{"よろしく!", TypeCorrectedQuery::CORRECTION});

  EXPECT_CALL(*mock, CorrectComposition(_)).WillOnce(Return(expected));

  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData(nullptr /* suffix_dictionary */,
                                  std::move(mock));
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  config_->set_use_typing_correction(true);

  InitHistory("", "");

  ConversionRequest convreq = CreatePredictionConversionRequest("よろさく!");
  const std::vector<Result> results =
      predictor_peer.AggregateTypingCorrectedResultsForMixedConversion(convreq);

  EXPECT_EQ(results.size(), 1);

  EXPECT_EQ(results[0].key, expected[0].correction);
  EXPECT_EQ(results[0].value, "よろしく！");  // default is full width.
}

TEST_F(DictionaryPredictorTest,
       AggregateExtendedTypeCorrectingWithNumberDecoder) {
  auto mock = std::make_unique<engine::MockSupplementalModel>();
  std::vector<TypeCorrectedQuery> expected;
  expected.emplace_back(
      TypeCorrectedQuery{"にじゅうご", TypeCorrectedQuery::CORRECTION});

  EXPECT_CALL(*mock, CorrectComposition(_)).WillRepeatedly(Return(expected));

  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData(nullptr /* suffix_dictionary */,
                                  std::move(mock));
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  config_->set_use_typing_correction(true);

  InitHistory("", "");

  ConversionRequest convreq = CreatePredictionConversionRequest("にしゆうこ");
  const std::vector<Result> results =
      predictor_peer.AggregateTypingCorrectedResultsForMixedConversion(convreq);
  EXPECT_EQ(results.size(), 2);
  EXPECT_EQ(results[1].value, "２５");  // default is full width.
}

TEST_F(DictionaryPredictorTest, ZeroQuerySuggestionAfterNumbers) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();
  const PosMatcher& pos_matcher = data_and_predictor->pos_matcher();

  request_->set_zero_query_suggestion(true);

  {
    constexpr absl::string_view kHistoryKey = "12";
    constexpr absl::string_view kHistoryValue = "12";
    constexpr absl::string_view kExpectedValue = "月";
    InitHistory(kHistoryKey, kHistoryValue);
    std::vector<Result> results;
    const ConversionRequest convreq = CreateSuggestionConversionRequest("");
    predictor_peer.AggregateZeroQuery(convreq, &results);
    EXPECT_FALSE(results.empty());

    auto target = results.end();
    for (auto it = results.begin(); it != results.end(); ++it) {
      EXPECT_EQ(it->GetPredictionTypesForTesting(), SUFFIX);

      if (it->value == kExpectedValue) {
        target = it;
        break;
      }
    }
    EXPECT_NE(results.end(), target);
    EXPECT_EQ(target->value, kExpectedValue);
    EXPECT_EQ(target->lid, pos_matcher.GetCounterSuffixWordId());
    EXPECT_EQ(target->rid, pos_matcher.GetCounterSuffixWordId());
  }

  {
    constexpr absl::string_view kHistoryKey = "66050713";  // A random number
    constexpr absl::string_view kHistoryValue = "66050713";
    constexpr absl::string_view kExpectedValue = "個";
    InitHistory(kHistoryKey, kHistoryValue);
    std::vector<Result> results;
    const ConversionRequest convreq = CreateSuggestionConversionRequest("");
    predictor_peer.AggregateZeroQuery(convreq, &results);
    EXPECT_FALSE(results.empty());

    bool found = false;
    for (auto it = results.begin(); it != results.end(); ++it) {
      EXPECT_EQ(it->GetPredictionTypesForTesting(), SUFFIX);
      if (it->value == kExpectedValue) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
  }
}

TEST_F(DictionaryPredictorTest, TriggerNumberZeroQuerySuggestion) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();
  const PosMatcher& pos_matcher = data_and_predictor->pos_matcher();

  const struct TestCase {
    const char* history_key;
    const char* history_value;
    const char* find_suffix_value;
    bool expected_result;
  } kTestCases[] = {
      {"12", "12", "月", true},      {"12", "１２", "月", true},
      {"12", "壱拾弐", "月", false}, {"12", "十二", "月", false},
      {"12", "一二", "月", false},   {"12", "Ⅻ", "月", false},
      {"あか", "12", "月", true},    // T13N
      {"あか", "１２", "月", true},  // T13N
      {"じゅう", "10", "時", true},  {"じゅう", "１０", "時", true},
      {"じゅう", "十", "時", false}, {"じゅう", "拾", "時", false},
  };

  for (const auto& test_case : kTestCases) {
    InitHistory(test_case.history_key, test_case.history_value);
    std::vector<Result> results;
    request_->set_zero_query_suggestion(true);
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest("", false /* init composer */);
    predictor_peer.AggregateZeroQuery(convreq, &results);
    EXPECT_FALSE(results.empty());

    bool found = false;
    for (auto it = results.begin(); it != results.end(); ++it) {
      EXPECT_EQ(it->GetPredictionTypesForTesting(), SUFFIX);
      if (it->value == test_case.find_suffix_value &&
          it->lid == pos_matcher.GetCounterSuffixWordId()) {
        found = true;
        break;
      }
    }
    EXPECT_EQ(found, test_case.expected_result) << test_case.history_value;
  }
}

TEST_F(DictionaryPredictorTest, GetNumberHistoryWithPrecedingText) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();
  const PosMatcher& pos_matcher = data_and_predictor->pos_matcher();

  // No history but has preceding text.
  InitHistory("", "");
  composer_->Reset();

  commands::Context context;
  context.set_preceding_text("12");

  request_->set_zero_query_suggestion(true);

  // Manually build ConversionRequest with context
  ConversionRequest::Options options;
  options.request_type = ConversionRequest::SUGGESTION;
  const ConversionRequest convreq = ConversionRequestBuilder()
                                        .SetComposer(*composer_)
                                        .SetRequest(*request_)
                                        .SetConfig(*config_)
                                        .SetOptions(std::move(options))
                                        .SetHistoryResultView(history_result_)
                                        .SetContext(context)
                                        .SetKey("")
                                        .Build();

  std::vector<Result> results;
  predictor_peer.AggregateZeroQuery(convreq, &results);
  EXPECT_FALSE(results.empty());

  bool found = false;
  for (const Result& result : results) {
    if ((result.attributes & SUFFIX) && result.value == "月" &&
        result.lid == pos_matcher.GetCounterSuffixWordId()) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}

TEST_F(DictionaryPredictorTest, TriggerZeroQuerySuggestion) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  const struct TestCase {
    const char* history_key;
    const char* history_value;
    const char* find_value;
    int expected_rank;  // -1 when don't appear.
  } kTestCases[] = {
      {"@", "@", "gmail.com", 0},
      {"username@", "username@", "gmail.com",
       0},  // Special treatment for email
      {"@", "@", "yahoo.co.jp", 1},
      {"@", "@", "docomo.ne.jp", 2},
      {"!", "!", "?", -1},
  };

  for (const auto& test_case : kTestCases) {
    InitHistory(test_case.history_key, test_case.history_value);
    std::vector<Result> results;
    request_->set_zero_query_suggestion(true);
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest("", false /* init_composer */);
    predictor_peer.AggregateZeroQuery(convreq, &results);
    EXPECT_FALSE(results.empty());

    int rank = -1;
    for (size_t i = 0; i < results.size(); ++i) {
      const auto& result = results[i];
      EXPECT_EQ(result.GetPredictionTypesForTesting(), SUFFIX);
      if (result.value == test_case.find_value && result.lid == 0 /* EOS */) {
        rank = static_cast<int>(i);
        break;
      }
    }
    EXPECT_EQ(rank, test_case.expected_rank) << test_case.history_value;
  }
}

TEST_F(DictionaryPredictorTest, ZipCodeRequest) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  struct TestCase {
    const bool is_suggestion;
    const char* key;
    const bool should_aggregate;
  } kTestCases[] = {
      {true, "", false},  // No ZeroQuery entry
      {true, "000", false},     {true, "---", false},
      {true, "0124-", false},   {true, "012-0", false},
      {true, "0124-0", true},    // key length >= 6
      {true, "012-3456", true},  // key length >= 6
      {true, "ABC", true},      {true, "０１２-０", true},

      {false, "", false},  // No ZeroQuery entry
      {false, "000", true},     {false, "---", true},
      {false, "0124-", true},   {false, "012-0", true},
      {false, "0124-0", true},  {false, "012-3456", true},
      {false, "ABC", true},     {false, "０１２-０", true},
  };

  for (const auto& test_case : kTestCases) {
    const ConversionRequest convreq =
        test_case.is_suggestion
            ? CreateSuggestionConversionRequest(test_case.key)
            : CreatePredictionConversionRequest(test_case.key);
    const std::vector<Result> results =
        predictor_peer.AggregateResultsForTesting(convreq);
    const bool has_result = !results.empty();
    EXPECT_EQ(has_result, test_case.should_aggregate) << test_case.key;
  }
}

TEST_F(DictionaryPredictorTest, MobileZipcodeEntries) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();
  request_test_util::FillMobileRequest(request_.get());

  const PosMatcher& pos_matcher = data_and_predictor->pos_matcher();
  MockDictionary* mock = data_and_predictor->mutable_dictionary();
  EXPECT_CALL(*mock, LookupPredictive(StrEq("101-000"), _, _))
      .WillOnce(InvokeCallbackWithOneToken{
          .key = "101-0001",
          .token = Token("101-0001", "東京都千代田", 100 /* cost */,
                         pos_matcher.GetZipcodeId(), pos_matcher.GetZipcodeId(),
                         Token::NONE)});
  EXPECT_CALL(*mock, LookupPredictive(StrEq("101-0001"), _, _))
      .WillOnce(InvokeCallbackWithOneToken{
          .key = "101-0001",
          .token = Token("101-0001", "東京都千代田", 100 /* cost */,
                         pos_matcher.GetZipcodeId(), pos_matcher.GetZipcodeId(),
                         Token::NONE)});
  {
    const ConversionRequest convreq =
        CreatePredictionConversionRequest("101-000");
    const std::vector<Result> results =
        predictor_peer.AggregateResultsForTesting(convreq);
    EXPECT_FALSE(FindCandidateByValue(results, "東京都千代田"));
  }
  {
    // Aggregate zip code entries only for exact key match.
    const ConversionRequest convreq =
        CreatePredictionConversionRequest("101-0001");
    const std::vector<Result> results =
        predictor_peer.AggregateResultsForTesting(convreq);
    EXPECT_TRUE(FindCandidateByValue(results, "東京都千代田"));
  }
}

TEST_F(DictionaryPredictorTest, RealtimeConversionStartingWithAlphabets) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  // turn on real-time conversion
  config_->set_use_dictionary_suggest(false);
  config_->set_use_realtime_conversion(true);

  constexpr absl::string_view kKey = "PCてすと";
  const absl::string_view kExpectedSuggestionValues[] = {
      "PCテスト",
      "PCてすと",
  };

  {
    MockRealtimeDecoder* realtime_decoder =
        data_and_predictor->mutable_realtime_decoder();
    ::testing::Mock::VerifyAndClearExpectations(realtime_decoder);
    std::vector<Result> results(2);
    results[0].key = kKey;
    results[0].value = "PCテスト";
    results[0].attributes = REALTIME;
    results[1].key = kKey;
    results[1].value = "PCてすと";
    results[1].attributes = REALTIME;
    EXPECT_CALL(*realtime_decoder,
                Decode(Truly([&kKey](const ConversionRequest& request) {
                  return request.key() == kKey;
                })))
        .WillOnce(Return(results));
  }

  std::vector<Result> results;
  const ConversionRequest convreq = CreateSuggestionConversionRequest(kKey);
  predictor_peer.AggregateRealtime(convreq, 10, false, &results);
  ASSERT_EQ(2, results.size());

  EXPECT_EQ(results[0].GetPredictionTypesForTesting(), REALTIME);
  EXPECT_EQ(results[1].GetPredictionTypesForTesting(), REALTIME);
  EXPECT_EQ(results[0].value, kExpectedSuggestionValues[0]);
  EXPECT_EQ(results[1].value, kExpectedSuggestionValues[1]);
}

TEST_F(DictionaryPredictorTest, RealtimeConversionWithSpellingCorrection) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  // turn on real-time conversion
  config_->set_use_dictionary_suggest(false);
  config_->set_use_realtime_conversion(true);

  constexpr absl::string_view kCapriHiragana = "かぷりちょうざ";

  {
    // No realtime conversion result
    MockRealtimeDecoder* realtime_decoder =
        data_and_predictor->mutable_realtime_decoder();
    ::testing::Mock::VerifyAndClearExpectations(realtime_decoder);
    EXPECT_CALL(*realtime_decoder, Decode(_))
        .WillRepeatedly(Return(std::vector<Result>({})));
  }
  std::vector<Result> results;
  const ConversionRequest convreq1 = CreateConversionRequest(
      {.request_type = ConversionRequest::SUGGESTION,
       .use_actual_converter_for_realtime_conversion = false},
      kCapriHiragana);
  int min_unigram_key_len = 0;
  predictor_peer.AggregateUnigram(convreq1, &results, &min_unigram_key_len);
  ASSERT_FALSE(results.empty());
  EXPECT_TRUE(results[0].attributes &
              Attribute::SPELLING_CORRECTION);  // From unigram

  results.clear();

  constexpr absl::string_view kKeyWithDe = "かぷりちょうざで";
  constexpr absl::string_view kExpectedSuggestionValueWithDe =
      "カプリチョーザで";
  {
    MockRealtimeDecoder* realtime_decoder =
        data_and_predictor->mutable_realtime_decoder();
    ::testing::Mock::VerifyAndClearExpectations(realtime_decoder);
    Result result;
    result.key = kKeyWithDe;
    result.value = kExpectedSuggestionValueWithDe;
    result.attributes =
        Attribute::REALTIME_CONVERSION | Attribute::SPELLING_CORRECTION;
    EXPECT_CALL(*realtime_decoder,
                Decode(Truly([&kKeyWithDe](const ConversionRequest& request) {
                  return request.key() == kKeyWithDe;
                })))
        .WillOnce(Return(std::vector<Result>({result})));
  }

  const ConversionRequest convreq2 =
      CreateSuggestionConversionRequest(kKeyWithDe);
  predictor_peer.AggregateRealtime(convreq2, 1, false, &results);
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].GetPredictionTypesForTesting(), REALTIME);
  EXPECT_NE(0, (results[0].attributes & Attribute::SPELLING_CORRECTION));
  EXPECT_EQ(results[0].value, kExpectedSuggestionValueWithDe);
}

TEST_F(DictionaryPredictorTest, PropagateUserDictionaryAttribute) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(true);

  {
    // No realtime conversion result
    MockRealtimeDecoder* realtime_decoder =
        data_and_predictor->mutable_realtime_decoder();
    ::testing::Mock::VerifyAndClearExpectations(realtime_decoder);
    EXPECT_CALL(*realtime_decoder, Decode(_))
        .WillOnce(Return(std::vector<Result>({})));

    const ConversionRequest convreq =
        CreateSuggestionConversionRequest("ゆーざー");
    const std::vector<Result> results =
        predictor_peer.AggregateResultsForTesting(convreq);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, "ユーザー");
    EXPECT_TRUE(results[0].attributes & Attribute::USER_DICTIONARY);
  }

  constexpr absl::string_view kKey = "ゆーざーの";
  constexpr absl::string_view kValue = "ユーザーの";
  {
    MockRealtimeDecoder* realtime_decoder =
        data_and_predictor->mutable_realtime_decoder();
    ::testing::Mock::VerifyAndClearExpectations(realtime_decoder);
    Result result;
    result.key = kKey;
    result.value = kValue;
    result.attributes = Attribute::USER_DICTIONARY;
    EXPECT_CALL(*realtime_decoder,
                Decode(Truly([kKey](const ConversionRequest& request) {
                  return request.key() == kKey;
                })))
        .WillOnce(Return(std::vector<Result>({result})));
  }

  {
    const ConversionRequest convreq = CreateSuggestionConversionRequest(kKey);
    const std::vector<Result> results =
        predictor_peer.AggregateResultsForTesting(convreq);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, kValue);
    EXPECT_TRUE(results[0].attributes & Attribute::USER_DICTIONARY);
  }
}

TEST_F(DictionaryPredictorTest, EnrichPartialCandidates) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();
  request_test_util::FillMobileRequest(request_.get());

  const ConversionRequest convreq =
      CreatePredictionConversionRequest("ぐーぐる");
  const std::vector<Result> results =
      predictor_peer.AggregateResultsForTesting(convreq);
  EXPECT_TRUE(GetMergedTypes(results) & PREFIX);
}

TEST_F(DictionaryPredictorTest, PrefixCandidates) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();
  request_test_util::FillMobileRequest(request_.get());

  const ConversionRequest convreq =
      CreatePredictionConversionRequest("ぐーぐるあ");
  const std::vector<Result> results =
      predictor_peer.AggregateResultsForTesting(convreq);
  EXPECT_TRUE(GetMergedTypes(results) & PREFIX);
  for (const auto& r : results) {
    if (r.attributes & PREFIX) {
      EXPECT_TRUE(r.attributes & Attribute::PARTIALLY_KEY_CONSUMED);
      EXPECT_NE(r.consumed_key_size, 0);
    }
  }
}

TEST_F(DictionaryPredictorTest, CandidatesFromUserDictionary) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();
  const PosMatcher& pos_matcher = data_and_predictor->pos_matcher();

  request_test_util::FillMobileRequest(request_.get());

  {
    MockDictionary* mock = data_and_predictor->mutable_dictionary();
    ::testing::Mock::VerifyAndClearExpectations(mock);
    const std::vector<Token> tokens = {
        // Suggest-only (only for exact key) USER DICTIONARY entry
        {"しょーとかっと", "ショートカット", 0, pos_matcher.GetUnknownId(),
         pos_matcher.GetUnknownId(), Token::USER_DICTIONARY},
        // Normal USER DICTIONARY entry
        {"しょーとかっと", "しょうとかっと", 0, pos_matcher.GetGeneralNounId(),
         pos_matcher.GetGeneralNounId(), Token::USER_DICTIONARY},
    };
    EXPECT_CALL(*mock, LookupPredictive(_, _, _))
        .WillRepeatedly(InvokeCallbackWithTokens{tokens});
    EXPECT_CALL(*mock, LookupPrefix(_, _, _)).Times(AnyNumber());
  }

  {
    const ConversionRequest convreq =
        CreatePredictionConversionRequest("しょーとか");
    const std::vector<Result> results =
        predictor_peer.AggregateResultsForTesting(convreq);
    EXPECT_TRUE(GetMergedTypes(results) & UNIGRAM);
    EXPECT_TRUE(FindCandidateByValue(results, "しょうとかっと"));
    EXPECT_FALSE(FindCandidateByValue(results, "ショートカット"));
  }
  {
    const ConversionRequest convreq =
        CreatePredictionConversionRequest("しょーとかっと");
    const std::vector<Result> results =
        predictor_peer.AggregateResultsForTesting(convreq);
    EXPECT_TRUE(GetMergedTypes(results) & UNIGRAM);
    EXPECT_TRUE(FindCandidateByValue(results, "しょうとかっと"));
    EXPECT_TRUE(FindCandidateByValue(results, "ショートカット"));
  }
}

TEST_F(DictionaryPredictorTest, NumberDecoderCandidates) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();
  request_test_util::FillMobileRequest(request_.get());

  const ConversionRequest convreq =
      CreatePredictionConversionRequest("よんじゅうごかい");
  const std::vector<Result> results =
      predictor_peer.AggregateResultsForTesting(convreq);
  const auto& result =
      std::find_if(results.begin(), results.end(),
                   [](Result r) { return r.value == "45" && !r.removed; });
  ASSERT_NE(result, results.end());
  EXPECT_TRUE(result->attributes & Attribute::PARTIALLY_KEY_CONSUMED);
  EXPECT_TRUE(result->attributes & Attribute::NO_SUGGEST_LEARNING);
}

TEST_F(DictionaryPredictorTest, DoNotPredictNoisyNumberEntries) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();
  request_test_util::FillMobileRequest(request_.get());

  {
    MockDictionary* mock = data_and_predictor->mutable_dictionary();
    EXPECT_CALL(*mock, LookupPredictive(StrEq("1"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{{"1", "一"},
                                                     {"1じ", "一時"},
                                                     {"1じ", "1時"},
                                                     {"10じ", "10時"},
                                                     {"10じ", "十時"},
                                                     {"1じすぎ", "1時過ぎ"},
                                                     {"19じ", "19時"}}});
  }

  composer_->SetInputMode(transliteration::HALF_ASCII);

  const ConversionRequest convreq = CreatePredictionConversionRequest("1");
  const std::vector<Result> results =
      predictor_peer.AggregateResultsForTesting(convreq);
  EXPECT_FALSE(FindCandidateByValue(results, "10時"));
  EXPECT_FALSE(FindCandidateByValue(results, "十時"));
  EXPECT_FALSE(FindCandidateByValue(results, "1時過ぎ"));
  EXPECT_FALSE(FindCandidateByValue(results, "19時"));

  EXPECT_TRUE(FindCandidateByValue(results, "一"));
  EXPECT_TRUE(FindCandidateByValue(results, "一時"));
  EXPECT_TRUE(FindCandidateByValue(results, "1時"));
}

TEST_F(DictionaryPredictorTest, SingleKanji) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();
  request_test_util::FillMobileRequest(request_.get());

  {
    MockSingleKanjiDictionary* mock =
        data_and_predictor->mutable_single_kanji_dictionary();
    EXPECT_CALL(*mock, LookupKanjiEntries(_, _))
        .WillRepeatedly(Return(std::vector<std::string>{"手"}));
  }

  const ConversionRequest convreq = CreatePredictionConversionRequest("てすと");
  const std::vector<Result> results =
      predictor_peer.AggregateResultsForTesting(convreq);
  EXPECT_TRUE(GetMergedTypes(results) & SINGLE_KANJI);
  for (const auto& result : results) {
    if (!(result.attributes & SINGLE_KANJI)) {
      EXPECT_GT(Util::CharsLen(result.value), 1);
    }
  }
}

TEST_F(DictionaryPredictorTest, SingleKanjiForMobileHardwareKeyboard) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();
  request_test_util::FillMobileRequestWithHardwareKeyboard(request_.get());

  {
    MockSingleKanjiDictionary* mock =
        data_and_predictor->mutable_single_kanji_dictionary();
    EXPECT_CALL(*mock, LookupKanjiEntries(_, _)).Times(0);
  }

  const ConversionRequest convreq = CreatePredictionConversionRequest("てすと");
  const std::vector<Result> results =
      predictor_peer.AggregateResultsForTesting(convreq);
  EXPECT_FALSE(GetMergedTypes(results) & SINGLE_KANJI);
}

TEST_F(DictionaryPredictorTest, Handwriting) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  MockDictionary* mock_dict = data_and_predictor->mutable_dictionary();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();
  constexpr int kCostOffset = 3000;

  // Handwriting request
  request_test_util::FillMobileRequestForHandwriting(request_.get());
  request_->mutable_decoder_experiment_params()
      ->set_max_composition_event_to_process(1);
  request_->mutable_decoder_experiment_params()
      ->set_handwriting_conversion_candidate_cost_offset(kCostOffset);
  {
    commands::SessionCommand command;
    commands::SessionCommand::CompositionEvent* composition_event =
        command.add_composition_events();
    composition_event->set_composition_string("かん字じ典");
    composition_event->set_probability(0.99);
    composition_event = command.add_composition_events();
    composition_event->set_composition_string("かlv字じ典");
    composition_event->set_probability(0.01);
    composer_->Reset();
    composer_->SetCompositionsForHandwriting(command.composition_events());
  }

  // reverse conversion
  {
    Result result;
    result.key = "かん字じ典";
    result.value = "かんじじてん";

    EXPECT_CALL(*data_and_predictor->mutable_realtime_decoder(),
                ReverseDecode(Truly([](const ConversionRequest& request) {
                  return request.request_type() ==
                             ConversionRequest::REVERSE_CONVERSION &&
                         request.key() == "かん字じ典";
                })))
        .WillOnce(Return(std::vector<Result>({result})));
  }

  EXPECT_CALL(*mock_dict, LookupPredictive(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(*mock_dict, LookupExact(StrEq("かんじじてん"), _, _))
      .WillRepeatedly(InvokeCallbackWithKeyValues{{
          {"かんじじてん", "漢字辞典"},
          {"かんじじてん", "漢字字典"},
          {"かんじじてん", "感じじてん"},
          {"かんじじてん", "幹事時点"},
          {"かんじじてん", "換字字典"},
          {"かんじじてん", "換字自転"},
          {"かんじじてん", "換字じてん"},
      }});

  const ConversionRequest convreq = CreatePredictionConversionRequest(
      "かん字じ典", false /* init_composer */);
  const std::vector<Result> results =
      predictor_peer.AggregateResultsForTesting(convreq);
  EXPECT_TRUE(GetMergedTypes(results) & UNIGRAM);

  EXPECT_GE(results.size(), 5);
  // composition from handwriting output
  EXPECT_TRUE(FindCandidateByKeyValue(results, "かんじじてん", "かん字じ典"));
  EXPECT_TRUE(FindCandidateByKeyValue(results, "かlv字じ典", "かlv字じ典"));
  // look-up results
  EXPECT_TRUE(FindCandidateByKeyValue(results, "かんじじてん", "漢字辞典"));
  EXPECT_TRUE(FindCandidateByKeyValue(results, "かんじじてん", "漢字字典"));
  EXPECT_TRUE(FindCandidateByKeyValue(results, "かんじじてん", "換字字典"));

  for (const Result& result : results) {
    if (result.value == "かん字じ典") {
      // Top recognition result
      EXPECT_EQ(result.wcost, 0);
    } else if (result.key == "かんじじてん") {
      EXPECT_GE(result.wcost, kCostOffset);
    }
  }
}

TEST_F(DictionaryPredictorTest, HandwritingT13N) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  MockDictionary* mock_dict = data_and_predictor->mutable_dictionary();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();
  // Handwriting request
  request_test_util::FillMobileRequestForHandwriting(request_.get());
  request_->mutable_decoder_experiment_params()
      ->set_max_composition_event_to_process(1);
  {
    commands::SessionCommand command;
    commands::SessionCommand::CompositionEvent* composition_event =
        command.add_composition_events();
    composition_event->set_composition_string("キた");
    composition_event->set_probability(0.99);
    composition_event = command.add_composition_events();
    composition_event->set_composition_string("もた");
    composition_event->set_probability(0.01);
    composer_->Reset();
    composer_->SetCompositionsForHandwriting(command.composition_events());
  }

  // reverse conversion
  {
    Result result;
    result.key = "きた";  // T13N key can be looked up
    result.value = "きた";

    EXPECT_CALL(*data_and_predictor->mutable_realtime_decoder(),
                ReverseDecode(Truly([](const ConversionRequest& request) {
                  return request.request_type() ==
                             ConversionRequest::REVERSE_CONVERSION &&
                         request.key() == "キた";
                })))
        .WillOnce(Return(std::vector<Result>({result})));
  }

  EXPECT_CALL(*mock_dict, LookupPredictive(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(*mock_dict, LookupExact(StrEq("きた"), _, _))
      .WillRepeatedly(InvokeCallbackWithKeyValues{{
          {"きた", "きた"},
          {"きた", "北"},
      }});

  const ConversionRequest convreq =
      CreatePredictionConversionRequest("キタ", false /* init composer */);
  const std::vector<Result> results =
      predictor_peer.AggregateResultsForTesting(convreq);
  EXPECT_TRUE(GetMergedTypes(results) & UNIGRAM);

  EXPECT_GE(results.size(), 2);
  // composition from handwriting output
  EXPECT_TRUE(FindCandidateByKeyValue(results, "きた", "キた"));
  EXPECT_TRUE(FindCandidateByKeyValue(results, "もた", "もた"));
  // No "きた", "北"
}

TEST_F(DictionaryPredictorTest, HandwritingNoHiragana) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  MockDictionary* mock_dict = data_and_predictor->mutable_dictionary();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  // Handwriting request
  request_test_util::FillMobileRequestForHandwriting(request_.get());
  request_->mutable_decoder_experiment_params()
      ->set_max_composition_event_to_process(1);
  {
    commands::SessionCommand command;
    commands::SessionCommand::CompositionEvent* composition_event =
        command.add_composition_events();
    composition_event->set_composition_string("南");
    composition_event->set_probability(0.9);
    composer_->Reset();
    composer_->SetCompositionsForHandwriting(command.composition_events());
  }

  // reverse conversion will not be called
  EXPECT_CALL(*data_and_predictor->mutable_realtime_decoder(), ReverseDecode(_))
      .Times(0);

  EXPECT_CALL(*mock_dict, LookupPredictive(_, _, _)).Times(0);
  EXPECT_CALL(*mock_dict, LookupExact(_, _, _)).Times(0);

  const ConversionRequest convreq =
      CreatePredictionConversionRequest("南", false /* init_composer */);
  const std::vector<Result> results =
      predictor_peer.AggregateResultsForTesting(convreq);
  EXPECT_TRUE(GetMergedTypes(results) & UNIGRAM);
  EXPECT_GE(results.size(), 1);
  // composition from handwriting output
  EXPECT_TRUE(FindCandidateByKeyValue(results, "南", "南"));
}

TEST_F(DictionaryPredictorTest, HandwritingRealtime) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();
  // Handwriting request
  request_test_util::FillMobileRequestForHandwriting(request_.get());
  request_->mutable_decoder_experiment_params()
      ->set_max_composition_event_to_process(1);
  {
    commands::SessionCommand command;
    commands::SessionCommand::CompositionEvent* composition_event =
        command.add_composition_events();
    composition_event->set_composition_string("ばらが");
    composition_event->set_probability(0.9);
    composer_->Reset();
    composer_->SetCompositionsForHandwriting(command.composition_events());
  }

  // Decode is called instead of ReverseDecode.
  {
    Result result;
    result.key = "ばらが";
    result.value = "薔薇が";

    EXPECT_CALL(*data_and_predictor->mutable_realtime_decoder(),
                Decode(Truly([](const ConversionRequest& request) {
                  return request.request_type() ==
                             ConversionRequest::PREDICTION &&
                         request.key() == "ばらが";
                })))
        .WillOnce(Return(std::vector<Result>({result})));
  }

  const ConversionRequest convreq =
      CreatePredictionConversionRequest("ばらが", false /* init_composer */);
  const std::vector<Result> results =
      predictor_peer.AggregateResultsForTesting(convreq);
  EXPECT_TRUE(GetMergedTypes(results) & UNIGRAM);

  EXPECT_GE(results.size(), 2);
  // composition from handwriting output
  EXPECT_TRUE(FindCandidateByKeyValue(results, "ばらが", "ばらが"));
  EXPECT_TRUE(FindCandidateByKeyValue(results, "ばらが", "薔薇が"));
}

TEST_F(DictionaryPredictorTest, AggregateZeroQueryWithPrecedingText) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor =
      CreatePredictorWithMockData();
  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();

  // No history but has preceding text "@".
  InitHistory("", "");
  composer_->Reset();

  commands::Context context;
  context.set_preceding_text("@");

  request_->set_zero_query_suggestion(true);
  request_test_util::FillMobileRequest(request_.get());

  ConversionRequest convreq = ConversionRequestBuilder()
                                  .SetComposer(*composer_)
                                  .SetRequest(*request_)
                                  .SetConfig(*config_)
                                  .SetContext(context)
                                  .SetKey("")
                                  .Build();

  std::vector<Result> results;
  predictor_peer.AggregateZeroQuery(convreq, &results);
  EXPECT_FALSE(results.empty());

  // "@" -> "gmail.com" should be found in zero_query_dict_
  EXPECT_TRUE(FindCandidateByValue(results, "gmail.com"));
}

TEST_F(DictionaryPredictorTest, ZeroQuerySuffixSkipWithoutHistory) {
  auto data_and_predictor = std::make_unique<MockDataAndPredictor>();
  data_and_predictor->Init(std::make_unique<TestSuffixDictionary>(), nullptr);

  const DictionaryPredictorTestPeer& predictor_peer =
      data_and_predictor->predictor_peer();
  request_test_util::FillMobileRequest(request_.get());

  // rid == 0, so suffix dictionary should be skipped.
  InitHistory("key", "value", 0);
  request_->set_zero_query_suggestion(true);

  commands::Context context;
  context.set_preceding_text("東京");

  ConversionRequest convreq = ConversionRequestBuilder()
                                  .SetComposer(*composer_)
                                  .SetRequest(*request_)
                                  .SetConfig(*config_)
                                  .SetContext(context)
                                  .SetKey("")
                                  .Build();

  std::vector<Result> results;
  predictor_peer.AggregateZeroQuery(convreq, &results);

  // results should NOT have candidates from TestSuffixDictionary ("以下")
  EXPECT_FALSE(FindCandidateByValue(results, "以下"));
}

}  // namespace
}  // namespace mozc::prediction
