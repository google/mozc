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

#include "prediction/dictionary_prediction_aggregator.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/container/serialized_string_array.h"
#include "base/util.h"
#include "composer/query.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/candidate.h"
#include "converter/converter_interface.h"
#include "converter/converter_mock.h"
#include "converter/immutable_converter_interface.h"
#include "converter/segments.h"
#include "data_manager/data_manager.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "engine/modules.h"
#include "engine/supplemental_model_interface.h"
#include "engine/supplemental_model_mock.h"
#include "prediction/result.h"
#include "prediction/single_kanji_prediction_aggregator.h"
#include "prediction/zero_query_dict.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "request/request_test_util.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "transliteration/transliteration.h"

namespace mozc {
namespace prediction {

using ::mozc::composer::TypeCorrectedQuery;

class DictionaryPredictionAggregatorTestPeer {
 public:
  explicit DictionaryPredictionAggregatorTestPeer(
      std::unique_ptr<DictionaryPredictionAggregator> aggregator)
      : aggregator_(std::move(aggregator)) {}

#define DEFINE_PEER(func_name)                                  \
  template <typename... Args>                                   \
  auto func_name(Args &&...args) const {                        \
    return aggregator_->func_name(std::forward<Args>(args)...); \
  }

  // Make them public via peer class.
  DEFINE_PEER(AggregateResults);
  DEFINE_PEER(AggregateTypingCorrectedResults);
  DEFINE_PEER(AggregateUnigram);
  DEFINE_PEER(AggregateBigram);
  DEFINE_PEER(AggregateRealtime);
  DEFINE_PEER(AggregateZeroQuery);
  DEFINE_PEER(AggregateEnglish);
  DEFINE_PEER(AggregateUnigramForMixedConversion);
  DEFINE_PEER(GetRealtimeCandidateMaxSize);
  DEFINE_PEER(GetZeroQueryCandidatesForKey);

#undef DEFINE_PEER

 private:
  std::unique_ptr<DictionaryPredictionAggregator> aggregator_;
};

namespace {

using ::mozc::dictionary::DictionaryInterface;
using ::mozc::dictionary::MockDictionary;
using ::mozc::dictionary::PosMatcher;
using ::mozc::dictionary::Token;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::Truly;
using ::testing::WithParamInterface;

// Action to call the third argument of LookupPrefix/LookupPredictive with the
// token <key, value>.
struct InvokeCallbackWithOneToken {
  template <class T, class U>
  void operator()(T, U, DictionaryInterface::Callback *callback) {
    callback->OnToken(key, key, token);
  }

  std::string key;
  Token token;
};

struct InvokeCallbackWithTokens {
  using Callback = DictionaryInterface::Callback;

  template <class T, class U>
  void operator()(T, U, Callback *callback) {
    for (const Token &token : tokens) {
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
  void operator()(T, U, Callback *callback) {
    for (const auto &[key, value] : kv_list) {
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
  converter::Candidate *c = seg->add_candidate();
  c->key.assign(key.data(), key.size());
  c->content_key = c->key;
  c->value.assign(value.data(), value.size());
  c->content_value = c->value;
}

void SetUpInputForSuggestion(absl::string_view key,
                             composer::Composer *composer, Segments *segments) {
  composer->Reset();
  composer->SetPreeditTextForTestOnly(key);
  InitSegmentsWithKey(key, segments);
}

void SetUpInputForSuggestionWithHistory(absl::string_view key,
                                        absl::string_view hist_key,
                                        absl::string_view hist_value,
                                        composer::Composer *composer,
                                        Segments *segments) {
  SetUpInputForSuggestion(key, composer, segments);
  PrependHistorySegments(hist_key, hist_value, segments);
}

void GenerateKeyEvents(absl::string_view text,
                       std::vector<commands::KeyEvent> *keys) {
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

void InsertInputSequence(absl::string_view text, composer::Composer *composer) {
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

bool FindResultByValue(absl::Span<const Result> results,
                       const absl::string_view value) {
  for (const auto &result : results) {
    if (result.value == value && !result.removed) {
      return true;
    }
  }
  return false;
}

bool FindResultByKeyValue(absl::Span<const Result> results,
                          const absl::string_view key,
                          const absl::string_view value) {
  for (const auto &result : results) {
    if (result.key == key && result.value == value && !result.removed) {
      return true;
    }
  }
  return false;
}

PredictionTypes GetMergedTypes(absl::Span<const Result> results) {
  PredictionTypes merged = NO_PREDICTION;
  for (const auto &result : results) {
    merged |= result.types;
  }
  return merged;
}

// Simple immutable converter mock for the realtime conversion test
class MockImmutableConverter : public ImmutableConverterInterface {
 public:
  MockImmutableConverter() = default;
  ~MockImmutableConverter() override = default;

  MOCK_METHOD(bool, ConvertForRequest,
              (const ConversionRequest &request, Segments *segments),
              (const, override));

  static bool ConvertForRequestImpl(const ConversionRequest &request,
                                    Segments *segments) {
    if (!segments || segments->conversion_segments_size() != 1 ||
        segments->conversion_segment(0).key().empty()) {
      return false;
    }
    absl::string_view key = segments->conversion_segment(0).key();
    Segment *segment = segments->mutable_conversion_segment(0);
    converter::Candidate *candidate = segment->add_candidate();
    candidate->value = key;
    candidate->key = key;
    return true;
  }
};

class MockSingleKanjiPredictionAggregator
    : public SingleKanjiPredictionAggregator {
 public:
  explicit MockSingleKanjiPredictionAggregator(const DataManager &data_manager,
                                               const PosMatcher &pos_matcher)
      : SingleKanjiPredictionAggregator(data_manager, pos_matcher) {}
  ~MockSingleKanjiPredictionAggregator() override = default;
  MOCK_METHOD(std::vector<Result>, AggregateResults,
              (const ConversionRequest &request), (const, override));
};

// Helper class to hold dictionary data and aggregator object.
class MockDataAndAggregator {
 public:
  // Initializes aggregator with the given suffix_dictionary and
  // supplemental_model.  When nullptr is passed to the |suffix_dictionary|,
  // MockDataManager's suffix dictionary is used. Note that |suffix_dictionary|
  // is owned by Modules.
  void Init(
      std::unique_ptr<DictionaryInterface> suffix_dictionary,
      std::unique_ptr<engine::SupplementalModelInterface> supplemental_model) {
    auto dictionary = std::make_unique<MockDictionary>();
    // TODO(taku): avoid sharing the pointer owned by std::unique_ptr.
    mock_dictionary_ = dictionary.get();

    auto data_manager = std::make_unique<testing::MockDataManager>();

    const PosMatcher pos_matcher(data_manager->GetPosMatcherData());

    auto kanji_aggregator =
        std::make_unique<MockSingleKanjiPredictionAggregator>(*data_manager,
                                                              pos_matcher);
    // TODO(taku): avoid sharing the pointer owned by std::unique_ptr.
    single_kanji_prediction_aggregator_ = kanji_aggregator.get();

    modules_ =
        engine::ModulesPresetBuilder()
            .PresetDictionary(std::move(dictionary))
            .PresetSingleKanjiPredictionAggregator(std::move(kanji_aggregator))
            .PresetSuffixDictionary(std::move(suffix_dictionary))    // nullable
            .PresetSupplementalModel(std::move(supplemental_model))  // nullable
            .Build(std::move(data_manager))
            .value();

    auto aggregator = std::make_unique<DictionaryPredictionAggregator>(
        *modules_, converter_, mock_immutable_converter_);
    aggregator_ = std::make_unique<DictionaryPredictionAggregatorTestPeer>(
        std::move(aggregator));
  }

  void Init() { return Init(nullptr, nullptr); }

  MockDictionary *mutable_dictionary() { return mock_dictionary_; }
  MockConverter *mutable_converter() { return &converter_; }
  MockImmutableConverter *mutable_immutable_converter() {
    return &mock_immutable_converter_;
  }
  MockSingleKanjiPredictionAggregator *
  mutable_single_kanji_prediction_aggregator() {
    return single_kanji_prediction_aggregator_;
  }
  const PosMatcher &pos_matcher() const { return modules_->GetPosMatcher(); }
  const DictionaryPredictionAggregatorTestPeer &aggregator() {
    return *aggregator_;
  }

 private:
  MockConverter converter_;
  MockImmutableConverter mock_immutable_converter_;
  std::unique_ptr<engine::Modules> modules_;

  MockDictionary *mock_dictionary_;
  MockSingleKanjiPredictionAggregator *single_kanji_prediction_aggregator_;

  std::unique_ptr<DictionaryPredictionAggregatorTestPeer> aggregator_;
};

class DictionaryPredictionAggregatorTest
    : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    request_ = std::make_unique<commands::Request>();
    config_ = std::make_unique<config::Config>();
    config::ConfigHandler::GetDefaultConfig(config_.get());
    table_ = std::make_shared<composer::Table>();
    composer_ =
        std::make_unique<composer::Composer>(table_, *request_, *config_);
  }

  ConversionRequest CreateConversionRequest(
      ConversionRequest::Options &&options, const Segments &segments) const {
    return ConversionRequestBuilder()
        .SetComposer(*composer_)
        .SetRequestView(*request_)
        .SetConfigView(*config_)
        .SetOptions(std::move(options))
        .SetHistorySegmentsView(segments)
        .SetKey(segments.conversion_segment(0).key())
        .Build();
  }
  ConversionRequest CreateSuggestionConversionRequest(
      const Segments &segments) const {
    ConversionRequest::Options options;
    options.request_type = ConversionRequest::SUGGESTION;
    return CreateConversionRequest(std::move(options), segments);
  }
  ConversionRequest CreatePredictionConversionRequest(
      const Segments &segments) const {
    ConversionRequest::Options options;
    options.request_type = ConversionRequest::PREDICTION;
    return CreateConversionRequest(std::move(options), segments);
  }

  static std::unique_ptr<MockDataAndAggregator> CreateAggregatorWithMockData(
      std::unique_ptr<DictionaryInterface> suffix_dictionary,
      std::unique_ptr<engine::SupplementalModelInterface> supplemental_model) {
    auto ret = std::make_unique<MockDataAndAggregator>();
    ret->Init(std::move(suffix_dictionary), std::move(supplemental_model));
    AddWordsToMockDic(ret->mutable_dictionary());
    AddDefaultImplToMockImmutableConverter(ret->mutable_immutable_converter());
    return ret;
  }

  static std::unique_ptr<MockDataAndAggregator> CreateAggregatorWithMockData() {
    return CreateAggregatorWithMockData(/*suffix dictionary=*/nullptr,
                                        /*supplemental_model=*/nullptr);
  }

  static void AddWordsToMockDic(MockDictionary *mock) {
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

  static void AddDefaultImplToMockImmutableConverter(
      MockImmutableConverter *mock) {
    EXPECT_CALL(*mock, ConvertForRequest(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(MockImmutableConverter::ConvertForRequestImpl));
  }

  std::unique_ptr<composer::Composer> composer_;
  std::shared_ptr<composer::Table> table_;
  std::unique_ptr<config::Config> config_;
  std::unique_ptr<commands::Request> request_;
};

TEST_F(DictionaryPredictionAggregatorTest, OnOffTest) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  {
    // turn off
    Segments segments;
    config_->set_use_dictionary_suggest(false);
    config_->set_use_realtime_conversion(false);

    SetUpInputForSuggestion("ぐーぐるあ", composer_.get(), &segments);
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    EXPECT_TRUE(aggregator.AggregateResults(convreq).empty());
  }
  {
    // turn on
    Segments segments;
    config_->set_use_dictionary_suggest(true);
    SetUpInputForSuggestion("ぐーぐるあ", composer_.get(), &segments);
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    EXPECT_FALSE(aggregator.AggregateResults(convreq).empty());
  }
  {
    // empty query
    Segments segments;
    SetUpInputForSuggestion("", composer_.get(), &segments);
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    EXPECT_TRUE(aggregator.AggregateResults(convreq).empty());
  }
}

TEST_F(DictionaryPredictionAggregatorTest, PartialSuggestion) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  Segments segments;
  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(true);
  // turn on mobile mode
  request_->set_mixed_conversion(true);

  Segment *seg = segments.add_segment();
  seg->set_key("ぐーぐるあ");
  seg->set_segment_type(Segment::FREE);
  const ConversionRequest convreq = CreateConversionRequest(
      {.request_type = ConversionRequest::PARTIAL_SUGGESTION}, segments);
  EXPECT_FALSE(aggregator.AggregateResults(convreq).empty());
}

TEST_F(DictionaryPredictionAggregatorTest,
       PartialSuggestionWithRealtimeConversion) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  Segments segments;
  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(true);
  // turn on mobile mode
  request_->set_mixed_conversion(true);

  SetUpInputForSuggestion("ぐーぐるあ", composer_.get(), &segments);
  composer_->MoveCursorLeft();
  segments.mutable_conversion_segment(0)->set_key("ぐーぐる");

  const ConversionRequest convreq = CreateConversionRequest(
      {.request_type = ConversionRequest::PARTIAL_SUGGESTION,
       .use_actual_converter_for_realtime_conversion = true},
      segments);

  // StartConversion should not be called for partial.
  EXPECT_CALL(*data_and_aggregator->mutable_converter(), StartConversion(_, _))
      .Times(0);

  Segments segments2;
  Segment *seg = segments2.add_segment();
  seg->set_key("ぐーぐる");
  seg->add_candidate()->value = "グーグル";
  MockImmutableConverter *immutable_converter =
      data_and_aggregator->mutable_immutable_converter();
  ::testing::Mock::VerifyAndClearExpectations(immutable_converter);
  EXPECT_CALL(*immutable_converter, ConvertForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments2), Return(true)));

  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  EXPECT_TRUE(GetMergedTypes(results) & REALTIME);
}

TEST_F(DictionaryPredictionAggregatorTest, BigramTest) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  Segments segments;
  config_->set_use_dictionary_suggest(true);

  InitSegmentsWithKey("あ", &segments);

  // history is "グーグル"
  PrependHistorySegments("ぐーぐる", "グーグル", &segments);

  // "グーグルアドセンス" will be returned.
  const ConversionRequest convreq = CreateSuggestionConversionRequest(segments);
  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  EXPECT_TRUE(BIGRAM | GetMergedTypes(results));
}

TEST_F(DictionaryPredictionAggregatorTest, BigramTestWithZeroQuery) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  Segments segments;
  config_->set_use_dictionary_suggest(true);
  request_->set_zero_query_suggestion(true);

  // current query is empty
  InitSegmentsWithKey("", &segments);

  // history is "グーグル"
  PrependHistorySegments("ぐーぐる", "グーグル", &segments);

  const ConversionRequest convreq = CreateSuggestionConversionRequest(segments);
  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  EXPECT_TRUE(BIGRAM | GetMergedTypes(results));
}

TEST_F(DictionaryPredictionAggregatorTest, BigramTestWithZeroQueryFilterMode) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  Segments segments;
  config_->set_use_dictionary_suggest(true);
  request_->set_zero_query_suggestion(true);
  request_->mutable_decoder_experiment_params()->set_bigram_nwp_filtering_mode(
      commands::DecoderExperimentParams::FILTER_ALL);

  // current query is empty
  InitSegmentsWithKey("", &segments);

  // history is "グーグル"
  PrependHistorySegments("ぐーぐる", "グーグル", &segments);

  const ConversionRequest convreq = CreateSuggestionConversionRequest(segments);
  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  EXPECT_FALSE(BIGRAM & GetMergedTypes(results));
}

// Check that previous candidate never be shown at the current candidate.
TEST_F(DictionaryPredictionAggregatorTest, Regression3042706) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  Segments segments;
  config_->set_use_dictionary_suggest(true);

  InitSegmentsWithKey("だい", &segments);

  // history is "きょうと/京都"
  PrependHistorySegments("きょうと", "京都", &segments);

  const ConversionRequest convreq = CreateSuggestionConversionRequest(segments);
  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  EXPECT_TRUE(REALTIME | GetMergedTypes(results));
  for (auto r : results) {
    EXPECT_FALSE(r.value.starts_with("京都"));
    EXPECT_TRUE(r.key.starts_with("だい"));
  }
}

enum Platform { DESKTOP, MOBILE };

class TriggerConditionsTest : public DictionaryPredictionAggregatorTest,
                              public WithParamInterface<Platform> {};

TEST_P(TriggerConditionsTest, TriggerConditions) {
  const bool is_mobile = (GetParam() == MOBILE);

  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  Segments segments;
  std::vector<Result> results;

  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(false);
  if (is_mobile) {
    request_test_util::FillMobileRequest(request_.get());
  }

  // Set up realtime conversion.
  {
    Segments segments2;
    Segment *seg = segments2.add_segment();
    seg->set_key("test");
    seg->add_candidate()->value = "test";
    MockImmutableConverter *immutable_converter =
        data_and_aggregator->mutable_immutable_converter();
    ::testing::Mock::VerifyAndClearExpectations(immutable_converter);
    EXPECT_CALL(*immutable_converter, ConvertForRequest(_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(segments2), Return(true)));
  }

  // Keys of normal lengths.
  {
    // Unigram is triggered in suggestion and prediction if key length (in UTF8
    // character count) is long enough.
    SetUpInputForSuggestion("ぐーぐる", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    const std::vector<Result> results = aggregator.AggregateResults(convreq);
    EXPECT_EQ(GetMergedTypes(results),
              AddDefaultPredictionTypes(UNIGRAM, is_mobile));
  }

  // Short keys.
  {
    if (is_mobile) {
      // Unigram is triggered even if key length is short.
      SetUpInputForSuggestion("てす", composer_.get(), &segments);
      composer_->SetInputMode(transliteration::HIRAGANA);
      const ConversionRequest suggestion_convreq =
          CreateSuggestionConversionRequest(segments);
      const std::vector<Result> results1 =
          aggregator.AggregateResults(suggestion_convreq);
      EXPECT_EQ(GetMergedTypes(results1), (UNIGRAM | REALTIME | PREFIX));

      const ConversionRequest prediction_convreq =
          CreatePredictionConversionRequest(segments);
      const std::vector<Result> results2 =
          aggregator.AggregateResults(prediction_convreq);
      EXPECT_EQ(GetMergedTypes(results2), (UNIGRAM | REALTIME | PREFIX));
    } else {
      // Unigram is not triggered for SUGGESTION if key length is short.
      SetUpInputForSuggestion("てす", composer_.get(), &segments);
      composer_->SetInputMode(transliteration::HIRAGANA);
      const ConversionRequest suggestion_convreq =
          CreateSuggestionConversionRequest(segments);
      EXPECT_TRUE(aggregator.AggregateResults(suggestion_convreq).empty());
      const ConversionRequest prediction_convreq =
          CreatePredictionConversionRequest(segments);
      const std::vector<Result> results =
          aggregator.AggregateResults(prediction_convreq);
      EXPECT_EQ(GetMergedTypes(results), UNIGRAM);
    }
  }

  // Zipcode-like keys.
  {
    SetUpInputForSuggestion("0123", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    EXPECT_TRUE(aggregator.AggregateResults(convreq).empty());
  }

  // History is short => UNIGRAM
  {
    SetUpInputForSuggestionWithHistory("てすとだ", "A", "A", composer_.get(),
                                       &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    const std::vector<Result> results = aggregator.AggregateResults(convreq);
    EXPECT_EQ(GetMergedTypes(results),
              AddDefaultPredictionTypes(UNIGRAM, is_mobile));
  }

  // Both history and current segment are long => UNIGRAM or BIGRAM
  {
    SetUpInputForSuggestionWithHistory("てすとだ", "これは", "これは",
                                       composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    const std::vector<Result> results = aggregator.AggregateResults(convreq);
    EXPECT_EQ(GetMergedTypes(results),
              AddDefaultPredictionTypes(UNIGRAM | BIGRAM, is_mobile));
  }

  // Current segment is short
  {
    if (is_mobile) {
      // For mobile, UNIGRAM and REALTIME are added to BIGRAM.
      SetUpInputForSuggestionWithHistory("てす", "てすとだよ", "テストだよ",
                                         composer_.get(), &segments);
      composer_->SetInputMode(transliteration::HIRAGANA);
      const ConversionRequest convreq =
          CreateSuggestionConversionRequest(segments);
      const std::vector<Result> results = aggregator.AggregateResults(convreq);
      EXPECT_EQ(GetMergedTypes(results),
                (UNIGRAM | BIGRAM | REALTIME | PREFIX));
    } else {
      // No UNIGRAM.
      SetUpInputForSuggestionWithHistory("てす", "てすとだよ", "テストだよ",
                                         composer_.get(), &segments);
      composer_->SetInputMode(transliteration::HIRAGANA);
      const ConversionRequest convreq =
          CreateSuggestionConversionRequest(segments);
      const std::vector<Result> results = aggregator.AggregateResults(convreq);
      EXPECT_EQ(GetMergedTypes(results), BIGRAM);
    }
  }

  // Typing correction shouldn't be appended.
  {
    SetUpInputForSuggestion("ｐはよう", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    const std::vector<Result> results = aggregator.AggregateResults(convreq);
    EXPECT_FALSE(TYPING_CORRECTION & GetMergedTypes(results));
  }

  // When romaji table is qwerty mobile => ENGLISH is included depending on
  // the language aware input setting.
  {
    const auto orig_input_mode = composer_->GetInputMode();
    const auto orig_table = request_->special_romanji_table();
    const auto orig_lang_aware = request_->language_aware_input();
    const bool orig_use_dictionary_suggest = config_->use_dictionary_suggest();

    SetUpInputForSuggestion("てすとだよ", composer_.get(), &segments);
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
          CreateSuggestionConversionRequest(segments);
      std::vector<Result> results;
      results = aggregator.AggregateResults(convreq1);
      EXPECT_FALSE(GetMergedTypes(results) & ENGLISH);

      // Language aware input is off: No English prediction.
      request_->set_language_aware_input(
          commands::Request::NO_LANGUAGE_AWARE_INPUT);
      const ConversionRequest convreq2 =
          CreateSuggestionConversionRequest(segments);
      results = aggregator.AggregateResults(convreq2);
      EXPECT_FALSE(GetMergedTypes(results) & ENGLISH);

      // Language aware input is on: English prediction is included.
      request_->set_language_aware_input(
          commands::Request::LANGUAGE_AWARE_SUGGESTION);
      const ConversionRequest convreq3 =
          CreateSuggestionConversionRequest(segments);
      results = aggregator.AggregateResults(convreq3);
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
          CreateSuggestionConversionRequest(segments);
      std::vector<Result> results;
      results = aggregator.AggregateResults(convreq1);
      EXPECT_FALSE(GetMergedTypes(results) & ENGLISH);

      // Language aware input is off.
      request_->set_language_aware_input(
          commands::Request::NO_LANGUAGE_AWARE_INPUT);
      const ConversionRequest convreq2 =
          CreateSuggestionConversionRequest(segments);
      results = aggregator.AggregateResults(convreq2);
      EXPECT_FALSE(GetMergedTypes(results) & ENGLISH);

      // Language aware input is on.
      request_->set_language_aware_input(
          commands::Request::LANGUAGE_AWARE_SUGGESTION);
      const ConversionRequest convreq3 =
          CreateSuggestionConversionRequest(segments);
      results = aggregator.AggregateResults(convreq3);
      EXPECT_FALSE(GetMergedTypes(results) & ENGLISH);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    TriggerConditionsForPlatforms, TriggerConditionsTest,
    ::testing::Values(DESKTOP, MOBILE),
    [](const ::testing::TestParamInfo<TriggerConditionsTest::ParamType> &info) {
      if (info.param == DESKTOP) {
        return "DESKTOP";
      }
      return "MOBILE";
    });

TEST_F(DictionaryPredictionAggregatorTest, TriggerConditionsLatinInputMode) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  struct TestCase {
    Platform platform;
    transliteration::TransliterationType input_mode;
  } kTestCases[] = {
      {DESKTOP, transliteration::HALF_ASCII},
      {DESKTOP, transliteration::FULL_ASCII},
      {MOBILE, transliteration::HALF_ASCII},
      {MOBILE, transliteration::FULL_ASCII},
  };

  for (const auto &test_case : kTestCases) {
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

    Segments segments;
    std::vector<Result> results;

    // Implementation note: SetUpInputForSuggestion() resets the state of
    // composer. So we have to call SetInputMode() after this method.
    SetUpInputForSuggestion("hel", composer_.get(), &segments);
    composer_->SetInputMode(test_case.input_mode);

    config_->set_use_dictionary_suggest(true);

    // Input mode is Latin(HALF_ASCII or FULL_ASCII) => ENGLISH
    config_->set_use_realtime_conversion(false);
    const ConversionRequest convreq1 =
        CreateSuggestionConversionRequest(segments);
    results = aggregator.AggregateResults(convreq1);
    EXPECT_EQ(GetMergedTypes(results),
              AddDefaultPredictionTypes(ENGLISH, is_mobile));

    config_->set_use_realtime_conversion(true);
    const ConversionRequest convreq2 =
        CreateSuggestionConversionRequest(segments);
    results = aggregator.AggregateResults(convreq2);
    EXPECT_EQ(GetMergedTypes(results),
              AddDefaultPredictionTypes(ENGLISH | REALTIME, is_mobile));

    // When dictionary suggest is turned off, English prediction should be
    // disabled.
    config_->set_use_dictionary_suggest(false);
    const ConversionRequest convreq3 =
        CreateSuggestionConversionRequest(segments);
    EXPECT_TRUE(aggregator.AggregateResults(convreq3).empty());

    // Has realtime results for PARTIAL_SUGGESTION request.
    config_->set_use_dictionary_suggest(true);
    const ConversionRequest partial_suggestion_convreq =
        CreateConversionRequest(
            {.request_type = ConversionRequest::PARTIAL_SUGGESTION}, segments);
    results = aggregator.AggregateResults(partial_suggestion_convreq);
    EXPECT_EQ(GetMergedTypes(results), REALTIME);
  }
}

TEST_F(DictionaryPredictionAggregatorTest, AggregateUnigramCandidate) {
  Segments segments;
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  constexpr absl::string_view kKey = "ぐーぐるあ";
  SetUpInputForSuggestion(kKey, composer_.get(), &segments);

  const ConversionRequest convreq = CreateSuggestionConversionRequest(segments);
  std::vector<Result> results;
  int min_unigram_key_len = 0;
  aggregator.AggregateUnigram(convreq, &results, &min_unigram_key_len);
  EXPECT_FALSE(results.empty());

  for (const auto &result : results) {
    EXPECT_EQ(result.types, UNIGRAM);
    EXPECT_TRUE(result.key.starts_with(kKey));
  }
}

TEST_F(DictionaryPredictionAggregatorTest,
       LookupUnigramCandidateForMixedConversion) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  constexpr absl::string_view kHiraganaA = "あ";
  constexpr absl::string_view kHiraganaAA = "ああ";
  constexpr auto kCost = MockDictionary::kDefaultCost;
  constexpr auto kPosId = MockDictionary::kDefaultPosId;
  const int kUnknownId = data_and_aggregator->pos_matcher().GetUnknownId();

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

  MockDictionary *mock_dict = data_and_aggregator->mutable_dictionary();
  EXPECT_CALL(*mock_dict, LookupPredictive(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(*mock_dict, LookupPredictive(StrEq(kHiraganaA), _, _))
      .WillRepeatedly(InvokeCallbackWithTokens{a_tokens});
  EXPECT_CALL(*mock_dict, LookupPredictive(StrEq(kHiraganaAA), _, _))
      .WillRepeatedly(InvokeCallbackWithTokens{aa_tokens});

  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(false);
  table_->LoadFromFile("system://12keys-hiragana.tsv");

  auto is_user_dictionary_result = [](const Result &res) {
    return (res.candidate_attributes & converter::Candidate::USER_DICTIONARY) !=
           0;
  };

  {
    // Test prediction from input あ.
    InsertInputSequence(kHiraganaA, composer_.get());
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kHiraganaA);

    std::vector<Result> results;
    const ConversionRequest convreq =
        CreatePredictionConversionRequest(segments);
    aggregator.AggregateUnigramForMixedConversion(convreq, &results);

    // Check if "aaa" is not filtered.
    auto iter =
        std::find_if(results.begin(), results.end(), [&](const Result &res) {
          return res.key == kHiraganaA && res.value == "aaa" &&
                 is_user_dictionary_result(res);
        });
    EXPECT_NE(results.end(), iter);

    // "bbb" is looked up from input "あ" but it will be filtered because it is
    // from user dictionary with unknown POS ID.
    iter = std::find_if(results.begin(), results.end(), [&](const Result &res) {
      return res.key == kHiraganaAA && res.value == "bbb" &&
             is_user_dictionary_result(res);
    });
    EXPECT_EQ(iter, results.end());
  }

  {
    // Test prediction from input ああ.
    composer_->Reset();
    InsertInputSequence(kHiraganaAA, composer_.get());
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kHiraganaAA);

    std::vector<Result> results;
    const ConversionRequest convreq =
        CreatePredictionConversionRequest(segments);
    aggregator.AggregateUnigramForMixedConversion(convreq, &results);

    // Check if "aaa" is not found as its key is あ.
    auto iter =
        std::find_if(results.begin(), results.end(), [&](const Result &res) {
          return res.key == kHiraganaA && res.value == "aaa" &&
                 is_user_dictionary_result(res);
        });
    EXPECT_EQ(iter, results.end());

    // Unlike the above case for "あ", "bbb" is now found because input key is
    // exactly "ああ".
    iter = std::find_if(results.begin(), results.end(), [&](const Result &res) {
      return res.key == kHiraganaAA && res.value == "bbb" &&
             is_user_dictionary_result(res);
    });
    EXPECT_NE(results.end(), iter);
  }
}

TEST_F(DictionaryPredictionAggregatorTest, MobileUnigram) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  Segments segments;
  constexpr absl::string_view kKey = "とうきょう";
  SetUpInputForSuggestion(kKey, composer_.get(), &segments);

  request_test_util::FillMobileRequest(request_.get());

  {
    constexpr auto kPosId = MockDictionary::kDefaultPosId;
    MockDictionary *mock = data_and_aggregator->mutable_dictionary();
    EXPECT_CALL(*mock, LookupPrefix(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(*mock, LookupPredictive(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(*mock, LookupPredictive(StrEq("とうきょう"), _, _))
        .WillRepeatedly(InvokeCallbackWithTokens{{
            {"とうきょう", "東京", 100, kPosId, kPosId, Token::NONE},
            {"とうきょう", "TOKYO", 200, kPosId, kPosId, Token::NONE},
            {"とうきょうと", "東京都", 110, kPosId, kPosId, Token::NONE},
            {"とうきょう", "東京", 120, kPosId, kPosId, Token::NONE},
            {"とうきょう", "TOKYO", 120, kPosId, kPosId, Token::NONE},
            {"とうきょうわん", "東京湾", 120, kPosId, kPosId, Token::NONE},
            {"とうきょうえき", "東京駅", 130, kPosId, kPosId, Token::NONE},
            {"とうきょうべい", "東京ベイ", 140, kPosId, kPosId, Token::NONE},
            {"とうきょうゆき", "東京行", 150, kPosId, kPosId, Token::NONE},
            {"とうきょうしぶ", "東京支部", 160, kPosId, kPosId, Token::NONE},
            {"とうきょうてん", "東京店", 170, kPosId, kPosId, Token::NONE},
            {"とうきょうがす", "東京ガス", 180, kPosId, kPosId, Token::NONE},
            {"とうきょう!", "東京!", 1100, kPosId, kPosId, Token::NONE},
            {"とうきょう!?", "東京!?", 1200, kPosId, kPosId, Token::NONE},
            {"とうきょう", "東京❤", 1300, kPosId, kPosId, Token::NONE},
        }});
  }

  std::vector<Result> results;
  const ConversionRequest convreq = CreatePredictionConversionRequest(segments);
  aggregator.AggregateUnigramForMixedConversion(convreq, &results);

  EXPECT_TRUE(FindResultByValue(results, "東京"));

  int prefix_count = 0;
  for (const auto &result : results) {
    if (result.value.starts_with("東京")) {
      ++prefix_count;
    }
  }
  // Should not have same prefix candidates a lot.
  EXPECT_LE(prefix_count, 11);
  // Candidates that predict symbols should not be handled as the redundant
  // candidates.
  const absl::string_view kExpected[] = {
      "東京", "TOKYO", "東京!", "東京!?", "東京❤",
  };
  for (int i = 0; i < std::size(kExpected); ++i) {
    EXPECT_EQ(results[i].value, kExpected[i]);
  }
}

// We are not sure what should we suggest after the end of sentence for now.
// However, we decided to show zero query suggestion rather than stopping
// zero query completely. Users may be confused if they cannot see suggestion
// window only after the certain conditions.
// TODO(toshiyuki): Show useful zero query suggestions after EOS.
TEST_F(DictionaryPredictionAggregatorTest, DISABLED_MobileZeroQueryAfterEOS) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  const PosMatcher &pos_matcher = data_and_aggregator->pos_matcher();

  const struct TestCase {
    const char *key;
    const char *value;
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

  for (const auto &test_case : kTestcases) {
    Segments segments;
    InitSegmentsWithKey("", &segments);

    Segment *seg = segments.push_front_segment();
    seg->set_segment_type(Segment::HISTORY);
    seg->set_key(test_case.key);
    converter::Candidate *c = seg->add_candidate();
    c->key = test_case.key;
    c->content_key = test_case.key;
    c->value = test_case.value;
    c->content_value = test_case.value;
    c->rid = test_case.rid;

    const ConversionRequest convreq =
        CreatePredictionConversionRequest(segments);
    const std::vector<Result> results = aggregator.AggregateResults(convreq);
    EXPECT_EQ(!results.empty(), test_case.expected_result);
  }
}

TEST_F(DictionaryPredictionAggregatorTest, AggregateBigramPrediction) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  {
    Segments segments;

    InitSegmentsWithKey("あ", &segments);

    // history is "グーグル"
    constexpr absl::string_view kHistoryKey = "ぐーぐる";
    constexpr absl::string_view kHistoryValue = "グーグル";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<Result> results;

    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    aggregator.AggregateBigram(convreq, &results);
    EXPECT_FALSE(results.empty());

    for (size_t i = 0; i < results.size(); ++i) {
      // "グーグルアドセンス", "グーグル", "アドセンス"
      // are in the dictionary.
      if (results[i].value == "アドセンス") {
        EXPECT_FALSE(results[i].removed);
      } else {
        EXPECT_TRUE(results[i].removed);
      }
      EXPECT_EQ(results[i].types, BIGRAM);
      EXPECT_FALSE(results[i].key.starts_with(kHistoryKey));
      EXPECT_FALSE(results[i].value.starts_with(kHistoryValue));
      EXPECT_TRUE(results[i].key.starts_with("あ"));
      EXPECT_TRUE(results[i].value.starts_with("ア"));
      // Not zero query
      EXPECT_EQ(results[i].zero_query_type, 0);
    }

    EXPECT_EQ(segments.conversion_segments_size(), 1);
  }

  {
    Segments segments;

    InitSegmentsWithKey("あ", &segments);

    constexpr absl::string_view kHistoryKey = "てす";
    constexpr absl::string_view kHistoryValue = "テス";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<Result> results;

    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    aggregator.AggregateBigram(convreq, &results);
    EXPECT_TRUE(results.empty());
  }
}

// Zero query bigram is deprecated and disabled.
// Keep this test to confirm that no suggestions are shown.
TEST_F(DictionaryPredictionAggregatorTest, AggregateZeroQueryBigramPrediction) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  request_test_util::FillMobileRequest(request_.get());

  {
    Segments segments;

    // Zero query
    InitSegmentsWithKey("", &segments);

    // history is "グーグル"
    constexpr absl::string_view kHistoryKey = "ぐーぐる";
    constexpr absl::string_view kHistoryValue = "グーグル";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<Result> results;

    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    aggregator.AggregateBigram(convreq, &results);
    EXPECT_TRUE(results.empty());
  }

  {
    constexpr absl::string_view kHistory = "ありがとう";

    MockDictionary *mock = data_and_aggregator->mutable_dictionary();
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

    Segments segments;

    // Zero query
    InitSegmentsWithKey("", &segments);

    PrependHistorySegments(kHistory, kHistory, &segments);

    std::vector<Result> results;

    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    aggregator.AggregateBigram(convreq, &results);
    EXPECT_TRUE(results.empty());
  }
}

TEST_F(DictionaryPredictionAggregatorTest,
       AggregateZeroQueryPredictionLatinInputMode) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  request_test_util::FillMobileRequest(request_.get());

  {
    Segments segments;

    // Zero query
    SetUpInputForSuggestion("", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HALF_ASCII);

    // No history
    constexpr absl::string_view kHistoryKey = "";
    constexpr absl::string_view kHistoryValue = "";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<Result> results;

    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    aggregator.AggregateZeroQuery(convreq, &results);
    EXPECT_TRUE(results.empty());
  }

  {
    Segments segments;

    // Zero query
    SetUpInputForSuggestion("", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HALF_ASCII);

    constexpr absl::string_view kHistoryKey = "when";
    constexpr absl::string_view kHistoryValue = "when";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<Result> results;

    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    aggregator.AggregateZeroQuery(convreq, &results);
    EXPECT_TRUE(results.empty());
  }

  {
    Segments segments;

    // Zero query
    SetUpInputForSuggestion("", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HALF_ASCII);

    // We can input numbers from Latin input mode.
    constexpr absl::string_view kHistoryKey = "12";
    constexpr absl::string_view kHistoryValue = "12";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<Result> results;

    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    aggregator.AggregateZeroQuery(convreq, &results);
    EXPECT_FALSE(results.empty());  // Should have results.
  }

  {
    Segments segments;

    // Zero query
    SetUpInputForSuggestion("", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HALF_ASCII);

    // We can input some symbols from Latin input mode.
    constexpr absl::string_view kHistoryKey = "@";
    constexpr absl::string_view kHistoryValue = "@";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<Result> results;

    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    aggregator.AggregateZeroQuery(convreq, &results);
    EXPECT_FALSE(results.empty());  // Should have results.
  }
}

TEST_F(DictionaryPredictionAggregatorTest, GetRealtimeCandidateMaxSize) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  Segments segments;

  // GetRealtimeCandidateMaxSize has some heuristics so here we test following
  // conditions.
  // - The result must be equal or less than kMaxSize;
  // - If mixed_conversion is the same, the result of SUGGESTION is
  //        equal or less than PREDICTION.
  // - If mixed_conversion is the same, the result of PARTIAL_SUGGESTION is
  //        equal or less than PARTIAL_PREDICTION.
  // - Partial version has equal or greater than non-partial version.

  constexpr size_t kMaxSize = 100;
  segments.push_back_segment();

  const ConversionRequest suggestion_convreq = CreateConversionRequest(
      {
          .request_type = ConversionRequest::SUGGESTION,
          .max_dictionary_prediction_candidates_size = kMaxSize,
      },
      segments);
  const ConversionRequest prediction_convreq =
      CreatePredictionConversionRequest(segments);

  // non-partial, non-mixed-conversion
  const size_t prediction_no_mixed =
      aggregator.GetRealtimeCandidateMaxSize(prediction_convreq, false);
  EXPECT_GE(kMaxSize, prediction_no_mixed);

  const size_t suggestion_no_mixed =
      aggregator.GetRealtimeCandidateMaxSize(suggestion_convreq, false);
  EXPECT_GE(kMaxSize, suggestion_no_mixed);
  EXPECT_LE(suggestion_no_mixed, prediction_no_mixed);

  // non-partial, mixed-conversion
  const size_t prediction_mixed =
      aggregator.GetRealtimeCandidateMaxSize(prediction_convreq, true);
  EXPECT_GE(kMaxSize, prediction_mixed);

  const size_t suggestion_mixed =
      aggregator.GetRealtimeCandidateMaxSize(suggestion_convreq, true);
  EXPECT_GE(kMaxSize, suggestion_mixed);

  // partial, non-mixed-conversion
  const ConversionRequest partial_suggestion_convreq = CreateConversionRequest(
      {.request_type = ConversionRequest::PARTIAL_SUGGESTION}, segments);
  const ConversionRequest partial_prediction_convreq = CreateConversionRequest(
      {.request_type = ConversionRequest::PARTIAL_PREDICTION}, segments);

  const size_t partial_prediction_no_mixed =
      aggregator.GetRealtimeCandidateMaxSize(partial_prediction_convreq, false);
  EXPECT_GE(kMaxSize, partial_prediction_no_mixed);

  const size_t partial_suggestion_no_mixed =
      aggregator.GetRealtimeCandidateMaxSize(partial_suggestion_convreq, false);
  EXPECT_GE(kMaxSize, partial_suggestion_no_mixed);
  EXPECT_LE(partial_suggestion_no_mixed, partial_prediction_no_mixed);

  // partial, mixed-conversion
  const size_t partial_prediction_mixed =
      aggregator.GetRealtimeCandidateMaxSize(partial_prediction_convreq, true);
  EXPECT_GE(kMaxSize, partial_prediction_mixed);

  const size_t partial_suggestion_mixed =
      aggregator.GetRealtimeCandidateMaxSize(partial_suggestion_convreq, true);
  EXPECT_GE(kMaxSize, partial_suggestion_mixed);
  EXPECT_LE(partial_suggestion_mixed, partial_prediction_mixed);

  EXPECT_GE(partial_prediction_no_mixed, prediction_no_mixed);
  EXPECT_GE(partial_prediction_mixed, prediction_mixed);
  EXPECT_GE(partial_suggestion_no_mixed, suggestion_no_mixed);
  EXPECT_GE(partial_suggestion_mixed, suggestion_mixed);
}

TEST_F(DictionaryPredictionAggregatorTest,
       GetRealtimeCandidateMaxSizeForMixed) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  constexpr size_t kMaxSize = 100;

  Segments short_segments;
  short_segments.InitForConvert("short");

  const ConversionRequest suggestion_short_convreq = CreateConversionRequest(
      {.request_type = ConversionRequest::SUGGESTION,
       .max_dictionary_prediction_candidates_size = kMaxSize},
      short_segments);
  const ConversionRequest prediction_short_convreq = CreateConversionRequest(
      {.request_type = ConversionRequest::PREDICTION,
       .max_dictionary_prediction_candidates_size = kMaxSize},
      short_segments);

  // for short key, try to provide many results as possible
  const size_t short_suggestion_mixed =
      aggregator.GetRealtimeCandidateMaxSize(suggestion_short_convreq, true);
  EXPECT_GE(kMaxSize, short_suggestion_mixed);

  const size_t short_prediction_mixed =
      aggregator.GetRealtimeCandidateMaxSize(prediction_short_convreq, true);
  EXPECT_GE(kMaxSize, short_prediction_mixed);

  Segments long_segments;
  long_segments.InitForConvert("long_request_key");

  const ConversionRequest suggestion_long_convreq = CreateConversionRequest(
      {.request_type = ConversionRequest::SUGGESTION,
       .max_dictionary_prediction_candidates_size = kMaxSize},
      long_segments);
  const ConversionRequest prediction_long_convreq = CreateConversionRequest(
      {.request_type = ConversionRequest::PREDICTION,
       .max_dictionary_prediction_candidates_size = kMaxSize},
      long_segments);

  const size_t long_suggestion_mixed =
      aggregator.GetRealtimeCandidateMaxSize(suggestion_long_convreq, true);
  EXPECT_GE(kMaxSize, long_suggestion_mixed);
  EXPECT_GT(short_suggestion_mixed, long_suggestion_mixed);

  const size_t long_prediction_mixed =
      aggregator.GetRealtimeCandidateMaxSize(prediction_long_convreq, true);
  EXPECT_GE(kMaxSize, long_prediction_mixed);
  EXPECT_GT(kMaxSize, long_prediction_mixed + long_suggestion_mixed);
  EXPECT_GT(short_prediction_mixed, long_prediction_mixed);
}

TEST_F(DictionaryPredictionAggregatorTest, AggregateRealtimeConversion) {
  auto data_and_aggregator = std::make_unique<MockDataAndAggregator>();
  data_and_aggregator->Init();

  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  constexpr absl::string_view kKey = "わたしのなまえはなかのです";

  // Set up mock converter
  {
    // Make segments like:
    // "わたしの"    | "なまえは" | "なかのです"
    // "Watashino" | "Namaeha" | "Nakanodesu"
    Segments segments;

    auto add_segment = [&segments](absl::string_view key,
                                   absl::string_view value) {
      Segment *segment = segments.add_segment();
      segment->set_key(key);
      converter::Candidate *candidate = segment->add_candidate();
      candidate->key = std::string(key);
      candidate->value = std::string(value);
    };

    add_segment("わたしの", "Watashino");
    add_segment("なまえは", "Namaeha");
    add_segment("なかのです", "Nakanodesu");

    EXPECT_CALL(*data_and_aggregator->mutable_converter(),
                StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }
  // Set up mock immutable converter
  {
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("わたしのなまえはなかのです");
    converter::Candidate *candidate = segment->add_candidate();
    candidate->value = "私の名前は中野です";
    candidate->key = ("わたしのなまえはなかのです");
    // "わたしの, 私の", "わたし, 私"
    candidate->PushBackInnerSegmentBoundary(12, 6, 9, 3);
    // "なまえは, 名前は", "なまえ, 名前"
    candidate->PushBackInnerSegmentBoundary(12, 9, 9, 6);
    // "なかのです, 中野です", "なかの, 中野"
    candidate->PushBackInnerSegmentBoundary(15, 12, 9, 6);
    EXPECT_CALL(*data_and_aggregator->mutable_immutable_converter(),
                ConvertForRequest(_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  // A test case with use_actual_converter_for_realtime_conversion being
  // false, i.e., realtime conversion result is generated by
  // ImmutableConverterMock.
  {
    Segments segments;

    InitSegmentsWithKey(kKey, &segments);

    // User history predictor can add candidates before dictionary predictor
    segments.mutable_conversion_segment(0)->add_candidate()->value = "history1";
    segments.mutable_conversion_segment(0)->add_candidate()->value = "history2";

    std::vector<Result> results;
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    aggregator.AggregateRealtime(convreq, 10, false, &results);
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].types, REALTIME);
    EXPECT_EQ(results[0].key, kKey);
    EXPECT_EQ(results[0].inner_segment_boundary.size(), 3);
    EXPECT_TRUE(results[0].candidate_attributes &
                converter::Candidate::NO_VARIANTS_EXPANSION);
  }

  // A test case with use_actual_converter_for_realtime_conversion being
  // true, i.e., realtime conversion result is generated by MockConverter.
  {
    Segments segments;

    InitSegmentsWithKey(kKey, &segments);

    // User history predictor can add candidates before dictionary predictor
    segments.mutable_conversion_segment(0)->add_candidate()->value = "history1";
    segments.mutable_conversion_segment(0)->add_candidate()->value = "history2";

    std::vector<Result> results;

    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    aggregator.AggregateRealtime(convreq, 10, true, &results);

    // When |request.use_actual_converter_for_realtime_conversion| is true,
    // the extra label REALTIME_TOP is expected to be added.
    ASSERT_EQ(2, results.size());
    bool realtime_top_found = false;
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_TRUE(results[i].types & REALTIME);
      EXPECT_TRUE(results[i].candidate_attributes &
                  converter::Candidate::NO_VARIANTS_EXPANSION);
      if (results[i].key == kKey &&
          results[i].value == "WatashinoNamaehaNakanodesu" &&
          results[i].inner_segment_boundary.size() == 3) {
        EXPECT_TRUE(results[i].types & REALTIME_TOP);
        realtime_top_found = true;
      }
    }
    EXPECT_TRUE(realtime_top_found);
  }
}

TEST_F(DictionaryPredictionAggregatorTest, PropagateUserHistoryAttribute) {
  auto data_and_aggregator = std::make_unique<MockDataAndAggregator>();
  data_and_aggregator->Init();

  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  constexpr absl::string_view kKey = "わたしのなまえはなかのです";

  // Set up mock converter
  {
    // Make segments like:
    // "わたしの"    | "なまえは" | "なかのです"
    // "Watashino" | "Namaeha" | "Nakanodesu"
    Segments segments;

    auto add_segment = [&segments](absl::string_view key,
                                   absl::string_view value) {
      Segment *segment = segments.add_segment();
      segment->set_key(key);
      converter::Candidate *candidate = segment->add_candidate();
      candidate->key = std::string(key);
      candidate->value = std::string(value);
    };

    add_segment("わたしの", "Watashino");
    add_segment("なまえは", "Namaeha");
    add_segment("なかのです", "Nakanodesu");
    segments.mutable_segment(1)->mutable_candidate(0)->attributes =
        converter::Candidate::USER_SEGMENT_HISTORY_REWRITER;

    EXPECT_CALL(*data_and_aggregator->mutable_converter(),
                StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }
  // Set up mock immutable converter
  {
    Segments segments;
    EXPECT_CALL(*data_and_aggregator->mutable_immutable_converter(),
                ConvertForRequest(_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  {
    Segments segments;
    InitSegmentsWithKey(kKey, &segments);

    std::vector<Result> results;
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    aggregator.AggregateRealtime(convreq, 10, true, &results);

    ASSERT_EQ(1, results.size());
    EXPECT_TRUE(results[0].types & REALTIME);
    EXPECT_TRUE(results[0].types & REALTIME_TOP);
    EXPECT_TRUE(results[0].candidate_attributes &
                converter::Candidate::NO_VARIANTS_EXPANSION);
    EXPECT_TRUE(results[0].candidate_attributes &
                converter::Candidate::USER_SEGMENT_HISTORY_REWRITER);
    EXPECT_EQ(results[0].key, kKey);
    EXPECT_EQ(results[0].value, "WatashinoNamaehaNakanodesu");
    EXPECT_EQ(results[0].inner_segment_boundary.size(), 3);
  }
}

TEST_F(DictionaryPredictionAggregatorTest, UseActualConverterRequest) {
  auto data_and_aggregator = std::make_unique<MockDataAndAggregator>();
  data_and_aggregator->Init();

  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  constexpr absl::string_view kKey = "わたしのなまえはなかのです";

  // Set up mock converter
  {
    // Make segments like:
    // "わたしの"    | "なまえは" | "なかのです"
    // "Watashino" | "Namaeha" | "Nakanodesu"
    Segments segments;

    auto add_segment = [&segments](absl::string_view key,
                                   absl::string_view value) {
      Segment *segment = segments.add_segment();
      segment->set_key(key);
      converter::Candidate *candidate = segment->add_candidate();
      candidate->key = std::string(key);
      candidate->value = std::string(value);
    };

    add_segment("わたしの", "Watashino");
    add_segment("なまえは", "Namaeha");
    add_segment("なかのです", "Nakanodesu");

    EXPECT_CALL(*data_and_aggregator->mutable_converter(),
                StartConversion(_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));
  }
  // Set up mock immutable converter
  {
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("わたしのなまえはなかのです");
    converter::Candidate *candidate = segment->add_candidate();
    candidate->value = "私の名前は中野です";
    candidate->key = ("わたしのなまえはなかのです");
    // "わたしの, 私の", "わたし, 私"
    candidate->PushBackInnerSegmentBoundary(12, 6, 9, 3);
    // "なまえは, 名前は", "なまえ, 名前"
    candidate->PushBackInnerSegmentBoundary(12, 9, 9, 6);
    // "なかのです, 中野です", "なかの, 中野"
    candidate->PushBackInnerSegmentBoundary(15, 12, 9, 6);
    EXPECT_CALL(*data_and_aggregator->mutable_immutable_converter(),
                ConvertForRequest(_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  {
    Segments segments;

    InitSegmentsWithKey(kKey, &segments);
    const ConversionRequest convreq = CreateConversionRequest(
        {.request_type = ConversionRequest::SUGGESTION,
         .use_actual_converter_for_realtime_conversion = true},
        segments);
    const std::vector<Result> results = aggregator.AggregateResults(convreq);
    ASSERT_GT(results.size(), 0);
    bool has_realtime_top = false;
    for (const Result &r : results) {
      if (r.types & PredictionType::REALTIME_TOP) {
        has_realtime_top = true;
        break;
      }
    }
    EXPECT_TRUE(has_realtime_top);
  }

  {
    Segments segments;

    InitSegmentsWithKey(kKey, &segments);
    const ConversionRequest convreq = CreateConversionRequest(
        {.request_type = ConversionRequest::SUGGESTION,
         .use_actual_converter_for_realtime_conversion = false},
        segments);
    const std::vector<Result> results = aggregator.AggregateResults(convreq);
    ASSERT_GT(results.size(), 0);
    bool has_realtime_top = false;
    for (const Result &r : results) {
      if (r.types & PredictionType::REALTIME_TOP) {
        has_realtime_top = true;
        break;
      }
    }
    EXPECT_FALSE(has_realtime_top);
  }
}

/*
TEST_F(DictionaryPredictionAggregatorTest, GetCandidateCutoffThreshold) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  Segments segments;

  const size_t prediction =
      aggregator.GetCandidateCutoffThreshold(ConversionRequest::PREDICTION);
  const size_t suggestion =
      aggregator.GetCandidateCutoffThreshold(ConversionRequest::SUGGESTION);
  EXPECT_LE(suggestion, prediction);
}
*/

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

  void LookupPredictive(absl::string_view key,
                        const ConversionRequest &conversion_request,
                        Callback *callback) const override {
    Token token;
    for (size_t i = 0; i < std::size(kSuffixTokens); ++i) {
      const SimpleSuffixToken &suffix_token = kSuffixTokens[i];
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

  void LookupPrefix(absl::string_view key,
                    const ConversionRequest &conversion_request,
                    Callback *callback) const override {}

  void LookupExact(absl::string_view key,
                   const ConversionRequest &conversion_request,
                   Callback *callback) const override {}

  void LookupReverse(absl::string_view str,
                     const ConversionRequest &conversion_request,
                     Callback *callback) const override {}
};

}  // namespace

TEST_F(DictionaryPredictionAggregatorTest, AggregateSuffixPrediction) {
  auto data_and_aggregator = std::make_unique<MockDataAndAggregator>();
  data_and_aggregator->Init(std::make_unique<TestSuffixDictionary>(), nullptr);

  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  Segments segments;

  request_->set_zero_query_suggestion(true);

  // history is "グーグル"
  constexpr absl::string_view kHistoryKey = "ぐーぐる";
  constexpr absl::string_view kHistoryValue = "グーグル";

  // Since SuffixDictionary only returns for key "い", the result
  // should be empty for "あ".
  std::vector<Result> results;
  SetUpInputForSuggestionWithHistory("あ", kHistoryKey, kHistoryValue,
                                     composer_.get(), &segments);
  const ConversionRequest convreq1 =
      CreateSuggestionConversionRequest(segments);
  aggregator.AggregateZeroQuery(convreq1, &results);
  EXPECT_TRUE(results.empty());

  // Candidates generated by AggregateSuffixPrediction from nonempty
  // key should have SUFFIX type.
  results.clear();
  SetUpInputForSuggestionWithHistory("い", kHistoryKey, kHistoryValue,
                                     composer_.get(), &segments);
  const ConversionRequest convreq2 =
      CreateSuggestionConversionRequest(segments);
  aggregator.AggregateZeroQuery(convreq2, &results);
  EXPECT_FALSE(results.empty());
  EXPECT_TRUE(GetMergedTypes(results) & SUFFIX);
  for (const auto &result : results) {
    EXPECT_EQ(result.types, SUFFIX);
    EXPECT_EQ(result.zero_query_type, ZERO_QUERY_SUFFIX);
  }
}

TEST_F(DictionaryPredictionAggregatorTest, AggregateZeroQuerySuffixPrediction) {
  auto data_and_aggregator = std::make_unique<MockDataAndAggregator>();
  data_and_aggregator->Init(std::make_unique<TestSuffixDictionary>(), nullptr);

  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  request_test_util::FillMobileRequest(request_.get());
  Segments segments;

  // Zero query
  InitSegmentsWithKey("", &segments);

  // history is "グーグル"
  constexpr absl::string_view kHistoryKey = "ぐーぐる";
  constexpr absl::string_view kHistoryValue = "グーグル";

  PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

  {
    std::vector<Result> results;

    // Candidates generated by AggregateZeroQuerySuffixPrediction should
    // have SUFFIX type.
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    aggregator.AggregateZeroQuery(convreq, &results);
    EXPECT_FALSE(results.empty());
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_EQ(results[i].types, SUFFIX);
      // Zero query
      EXPECT_EQ(results[i].zero_query_type, ZERO_QUERY_SUFFIX);
    }
  }
  {
    // If the feature is disabled and `results` is nonempty, nothing should be
    // generated.
    request_->mutable_decoder_experiment_params()
        ->set_disable_zero_query_suffix_prediction(true);
    std::vector<Result> results = {Result()};
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    aggregator.AggregateZeroQuery(convreq, &results);
    EXPECT_EQ(results.size(), 1);
  }
  {
    // Suffix entries should be aggregated for handwriting
    request_->set_is_handwriting(true);
    std::vector<Result> results = {Result()};
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    aggregator.AggregateZeroQuery(convreq, &results);
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
    : public DictionaryPredictionAggregatorTest,
      public WithParamInterface<EnglishPredictionTestEntry> {};

TEST_P(AggregateEnglishPredictionTest, AggregateEnglishPrediction) {
  const EnglishPredictionTestEntry &entry = GetParam();
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  table_->LoadFromFile("system://romanji-hiragana.tsv");
  composer_->Reset();
  composer_->SetInputMode(entry.input_mode);
  InsertInputSequence(entry.key, composer_.get());

  Segments segments;
  InitSegmentsWithKey(entry.key, &segments);

  std::vector<Result> results;

  const ConversionRequest convreq = CreatePredictionConversionRequest(segments);
  aggregator.AggregateEnglish(convreq, &results);

  std::set<std::string> values;
  for (const auto &result : results) {
    EXPECT_EQ(result.types, ENGLISH);
    EXPECT_TRUE(result.value.starts_with(entry.expected_prefix))
        << result.value << " doesn't start with " << entry.expected_prefix;
    values.insert(result.value);
  }
  for (const auto &expected_value : entry.expected_values) {
    EXPECT_TRUE(values.find(expected_value) != values.end())
        << expected_value << " isn't in the results";
  }
}

const std::vector<EnglishPredictionTestEntry> *kEnglishPredictionTestEntries =
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

INSTANTIATE_TEST_SUITE_P(
    AggregateEnglishPredictioForInputMode, AggregateEnglishPredictionTest,
    ::testing::ValuesIn(*kEnglishPredictionTestEntries),
    [](const ::testing::TestParamInfo<AggregateEnglishPredictionTest::ParamType>
           &info) { return info.param.name; });

TEST_F(DictionaryPredictionAggregatorTest,
       AggregateExtendedTypeCorrectingPrediction) {
  auto mock = std::make_unique<engine::MockSupplementalModel>();

  std::vector<TypeCorrectedQuery> expected;

  auto add_expected = [&](const std::string &key, uint8_t type) {
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

  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData(/*suffix dictionary=*/nullptr,
                                   std::move(mock));
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  config_->set_use_typing_correction(true);

  Segments segments;
  SetUpInputForSuggestionWithHistory("よろさく", "ほんじつは", "本日は",
                                     composer_.get(), &segments);

  ConversionRequest convreq = CreatePredictionConversionRequest(segments);
  const std::vector<Result> results =
      aggregator.AggregateTypingCorrectedResults(convreq);

  EXPECT_EQ(results.size(), 5);
  for (int i = 0; i < results.size(); ++i) {
    EXPECT_EQ(results[i].key, expected[i].correction);
    if (i == 2) {
      // "よろさくです" is COMPLETION only.
      EXPECT_FALSE(results[i].types & TYPING_CORRECTION);
    } else {
      EXPECT_TRUE(results[i].types & TYPING_CORRECTION);
    }
  }
}

TEST_F(DictionaryPredictionAggregatorTest,
       AggregateExtendedTypeCorrectingPredictionWithCharacterForm) {
  auto mock = std::make_unique<engine::MockSupplementalModel>();

  std::vector<TypeCorrectedQuery> expected;
  expected.emplace_back(
      TypeCorrectedQuery{"よろしく!", TypeCorrectedQuery::CORRECTION});

  EXPECT_CALL(*mock, CorrectComposition(_)).WillOnce(Return(expected));

  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData(nullptr /* suffix_dictionary */,
                                   std::move(mock));
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  config_->set_use_typing_correction(true);

  Segments segments;
  SetUpInputForSuggestionWithHistory("よろさく!", "", "", composer_.get(),
                                     &segments);

  ConversionRequest convreq = CreatePredictionConversionRequest(segments);
  const std::vector<Result> results =
      aggregator.AggregateTypingCorrectedResults(convreq);

  EXPECT_EQ(results.size(), 1);

  EXPECT_EQ(results[0].key, expected[0].correction);
  EXPECT_EQ(results[0].value, "よろしく！");  // default is full width.
}

TEST_F(DictionaryPredictionAggregatorTest,
       AggregateExtendedTypeCorrectingWithNumberDecoder) {
  auto mock = std::make_unique<engine::MockSupplementalModel>();
  std::vector<TypeCorrectedQuery> expected;
  expected.emplace_back(
      TypeCorrectedQuery{"にじゅうご", TypeCorrectedQuery::CORRECTION});

  EXPECT_CALL(*mock, CorrectComposition(_)).WillRepeatedly(Return(expected));

  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData(nullptr /* suffix_dictionary */,
                                   std::move(mock));
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  config_->set_use_typing_correction(true);

  Segments segments;
  SetUpInputForSuggestionWithHistory("にしゆうこ", "", "", composer_.get(),
                                     &segments);

  ConversionRequest convreq = CreatePredictionConversionRequest(segments);
  const std::vector<Result> results =
      aggregator.AggregateTypingCorrectedResults(convreq);
  EXPECT_EQ(results.size(), 2);
  EXPECT_EQ(results[1].value, "２５");  // default is full width.
}

TEST_F(DictionaryPredictionAggregatorTest, ZeroQuerySuggestionAfterNumbers) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  const PosMatcher &pos_matcher = data_and_aggregator->pos_matcher();
  Segments segments;

  request_->set_zero_query_suggestion(true);

  {
    InitSegmentsWithKey("", &segments);

    constexpr absl::string_view kHistoryKey = "12";
    constexpr absl::string_view kHistoryValue = "12";
    constexpr absl::string_view kExpectedValue = "月";
    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);
    std::vector<Result> results;
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    aggregator.AggregateZeroQuery(convreq, &results);
    EXPECT_FALSE(results.empty());

    auto target = results.end();
    for (auto it = results.begin(); it != results.end(); ++it) {
      EXPECT_EQ(it->types, SUFFIX);
      EXPECT_EQ(it->zero_query_type, ZERO_QUERY_NUMBER_SUFFIX);

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
    InitSegmentsWithKey("", &segments);

    constexpr absl::string_view kHistoryKey = "66050713";  // A random number
    constexpr absl::string_view kHistoryValue = "66050713";
    constexpr absl::string_view kExpectedValue = "個";
    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);
    std::vector<Result> results;
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    aggregator.AggregateZeroQuery(convreq, &results);
    EXPECT_FALSE(results.empty());

    bool found = false;
    for (auto it = results.begin(); it != results.end(); ++it) {
      EXPECT_EQ(it->types, SUFFIX);
      if (it->value == kExpectedValue) {
        EXPECT_EQ(it->zero_query_type, ZERO_QUERY_NUMBER_SUFFIX);
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
  }
}

TEST_F(DictionaryPredictionAggregatorTest, TriggerNumberZeroQuerySuggestion) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  const PosMatcher &pos_matcher = data_and_aggregator->pos_matcher();

  const struct TestCase {
    const char *history_key;
    const char *history_value;
    const char *find_suffix_value;
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

  for (const auto &test_case : kTestCases) {
    Segments segments;
    InitSegmentsWithKey("", &segments);

    PrependHistorySegments(test_case.history_key, test_case.history_value,
                           &segments);
    std::vector<Result> results;
    request_->set_zero_query_suggestion(true);
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    aggregator.AggregateZeroQuery(convreq, &results);
    EXPECT_FALSE(results.empty());

    bool found = false;
    for (auto it = results.begin(); it != results.end(); ++it) {
      EXPECT_EQ(it->types, SUFFIX);
      if (it->value == test_case.find_suffix_value &&
          it->lid == pos_matcher.GetCounterSuffixWordId()) {
        EXPECT_EQ(it->zero_query_type, ZERO_QUERY_NUMBER_SUFFIX);
        found = true;
        break;
      }
    }
    EXPECT_EQ(found, test_case.expected_result) << test_case.history_value;
  }
}

TEST_F(DictionaryPredictionAggregatorTest, TriggerZeroQuerySuggestion) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  const struct TestCase {
    const char *history_key;
    const char *history_value;
    const char *find_value;
    int expected_rank;  // -1 when don't appear.
  } kTestCases[] = {
      {"@", "@", "gmail.com", 0},      {"@", "@", "docomo.ne.jp", 1},
      {"@", "@", "ezweb.ne.jp", 2},    {"@", "@", "i.softbank.jp", 3},
      {"@", "@", "softbank.ne.jp", 4}, {"!", "!", "?", -1},
  };

  for (const auto &test_case : kTestCases) {
    Segments segments;
    InitSegmentsWithKey("", &segments);

    PrependHistorySegments(test_case.history_key, test_case.history_value,
                           &segments);
    std::vector<Result> results;
    request_->set_zero_query_suggestion(true);
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    aggregator.AggregateZeroQuery(convreq, &results);
    EXPECT_FALSE(results.empty());

    int rank = -1;
    for (size_t i = 0; i < results.size(); ++i) {
      const auto &result = results[i];
      EXPECT_EQ(result.types, SUFFIX);
      if (result.value == test_case.find_value && result.lid == 0 /* EOS */) {
        rank = static_cast<int>(i);
        break;
      }
    }
    EXPECT_EQ(rank, test_case.expected_rank) << test_case.history_value;
  }
}

TEST_F(DictionaryPredictionAggregatorTest, ZipCodeRequest) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  struct TestCase {
    const bool is_suggestion;
    const char *key;
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

  for (const auto &test_case : kTestCases) {
    Segments segments;
    InitSegmentsWithKey(test_case.key, &segments);
    const ConversionRequest convreq =
        test_case.is_suggestion ? CreateSuggestionConversionRequest(segments)
                                : CreatePredictionConversionRequest(segments);
    const std::vector<Result> results = aggregator.AggregateResults(convreq);
    const bool has_result = !results.empty();
    EXPECT_EQ(has_result, test_case.should_aggregate) << test_case.key;
  }
}

TEST_F(DictionaryPredictionAggregatorTest, MobileZipcodeEntries) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  request_test_util::FillMobileRequest(request_.get());

  const PosMatcher &pos_matcher = data_and_aggregator->pos_matcher();
  MockDictionary *mock = data_and_aggregator->mutable_dictionary();
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
    Segments segments;
    SetUpInputForSuggestion("101-000", composer_.get(), &segments);
    const ConversionRequest convreq =
        CreatePredictionConversionRequest(segments);
    const std::vector<Result> results = aggregator.AggregateResults(convreq);
    EXPECT_FALSE(FindResultByValue(results, "東京都千代田"));
  }
  {
    // Aggregate zip code entries only for exact key match.
    Segments segments;
    SetUpInputForSuggestion("101-0001", composer_.get(), &segments);
    const ConversionRequest convreq =
        CreatePredictionConversionRequest(segments);
    const std::vector<Result> results = aggregator.AggregateResults(convreq);
    EXPECT_TRUE(FindResultByValue(results, "東京都千代田"));
  }
}

TEST_F(DictionaryPredictionAggregatorTest,
       RealtimeConversionStartingWithAlphabets) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  Segments segments;
  // turn on real-time conversion
  config_->set_use_dictionary_suggest(false);
  config_->set_use_realtime_conversion(true);

  constexpr absl::string_view kKey = "PCてすと";
  const absl::string_view kExpectedSuggestionValues[] = {
      "PCテスト",
      "PCてすと",
  };

  {
    MockImmutableConverter *immutable_converter =
        data_and_aggregator->mutable_immutable_converter();
    ::testing::Mock::VerifyAndClearExpectations(immutable_converter);
    auto has_conversion_segment_key = [kKey](Segments *segments) {
      if (segments->conversion_segments_size() != 1) {
        return false;
      }
      return segments->conversion_segment(0).key() == kKey;
    };
    Segments segments;
    Segment *seg = segments.add_segment();
    seg->set_key(kKey);
    seg->add_candidate()->value = "PCテスト";
    seg->add_candidate()->value = "PCてすと";
    EXPECT_CALL(*immutable_converter,
                ConvertForRequest(_, Truly(has_conversion_segment_key)))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  InitSegmentsWithKey(kKey, &segments);

  std::vector<Result> results;

  const ConversionRequest convreq = CreateSuggestionConversionRequest(segments);
  aggregator.AggregateRealtime(convreq, 10, false, &results);
  ASSERT_EQ(2, results.size());

  EXPECT_EQ(results[0].types, REALTIME);
  EXPECT_EQ(results[0].value, kExpectedSuggestionValues[0]);
  EXPECT_EQ(results[1].value, kExpectedSuggestionValues[1]);
}

TEST_F(DictionaryPredictionAggregatorTest,
       RealtimeConversionWithSpellingCorrection) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  Segments segments;
  // turn on real-time conversion
  config_->set_use_dictionary_suggest(false);
  config_->set_use_realtime_conversion(true);

  constexpr absl::string_view kCapriHiragana = "かぷりちょうざ";

  {
    // No realtime conversion result
    MockImmutableConverter *immutable_converter =
        data_and_aggregator->mutable_immutable_converter();
    ::testing::Mock::VerifyAndClearExpectations(immutable_converter);
    EXPECT_CALL(*immutable_converter, ConvertForRequest(_, _))
        .WillRepeatedly(Return(false));
  }
  std::vector<Result> results;
  SetUpInputForSuggestion(kCapriHiragana, composer_.get(), &segments);
  const ConversionRequest convreq1 = CreateConversionRequest(
      {.request_type = ConversionRequest::SUGGESTION,
       .use_actual_converter_for_realtime_conversion = false},
      segments);
  int min_unigram_key_len = 0;
  aggregator.AggregateUnigram(convreq1, &results, &min_unigram_key_len);
  ASSERT_FALSE(results.empty());
  EXPECT_TRUE(results[0].candidate_attributes &
              converter::Candidate::SPELLING_CORRECTION);  // From unigram

  results.clear();

  constexpr absl::string_view kKeyWithDe = "かぷりちょうざで";
  constexpr absl::string_view kExpectedSuggestionValueWithDe =
      "カプリチョーザで";
  {
    MockImmutableConverter *immutable_converter =
        data_and_aggregator->mutable_immutable_converter();
    ::testing::Mock::VerifyAndClearExpectations(immutable_converter);
    auto has_conversion_segment_key = [kKeyWithDe](Segments *segments) {
      if (segments->conversion_segments_size() != 1) {
        return false;
      }
      return segments->conversion_segment(0).key() == kKeyWithDe;
    };
    Segments segments;
    Segment *seg = segments.add_segment();
    seg->set_key(kKeyWithDe);
    converter::Candidate *candidate = seg->add_candidate();
    candidate->value = kExpectedSuggestionValueWithDe;
    candidate->attributes = converter::Candidate::SPELLING_CORRECTION;
    EXPECT_CALL(*immutable_converter,
                ConvertForRequest(_, Truly(has_conversion_segment_key)))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  SetUpInputForSuggestion(kKeyWithDe, composer_.get(), &segments);
  const ConversionRequest convreq2 =
      CreateSuggestionConversionRequest(segments);
  aggregator.AggregateRealtime(convreq2, 1, false, &results);
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].types, REALTIME);
  EXPECT_NE(0, (results[0].candidate_attributes &
                converter::Candidate::SPELLING_CORRECTION));
  EXPECT_EQ(results[0].value, kExpectedSuggestionValueWithDe);
}

TEST_F(DictionaryPredictionAggregatorTest, PropagateUserDictionaryAttribute) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(true);

  {
    // No realtime conversion result
    MockImmutableConverter *immutable_converter =
        data_and_aggregator->mutable_immutable_converter();
    ::testing::Mock::VerifyAndClearExpectations(immutable_converter);
    EXPECT_CALL(*immutable_converter, ConvertForRequest(_, _))
        .WillOnce(Return(false));

    Segments segments;
    SetUpInputForSuggestion("ゆーざー", composer_.get(), &segments);
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    const std::vector<Result> results = aggregator.AggregateResults(convreq);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, "ユーザー");
    EXPECT_TRUE(results[0].candidate_attributes &
                converter::Candidate::USER_DICTIONARY);
  }

  constexpr absl::string_view kKey = "ゆーざーの";
  constexpr absl::string_view kValue = "ユーザーの";
  {
    MockImmutableConverter *immutable_converter =
        data_and_aggregator->mutable_immutable_converter();
    ::testing::Mock::VerifyAndClearExpectations(immutable_converter);
    auto has_conversion_segment_key = [kKey](Segments *segments) {
      if (segments->conversion_segments_size() != 1) {
        return false;
      }
      return segments->conversion_segment(0).key() == kKey;
    };
    Segments segments;
    Segment *seg = segments.add_segment();
    seg->set_key(kKey);
    converter::Candidate *candidate = seg->add_candidate();
    candidate->value = kValue;
    candidate->attributes = converter::Candidate::USER_DICTIONARY;
    EXPECT_CALL(*immutable_converter,
                ConvertForRequest(_, Truly(has_conversion_segment_key)))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  {
    Segments segments;
    SetUpInputForSuggestion(kKey, composer_.get(), &segments);
    const ConversionRequest convreq =
        CreateSuggestionConversionRequest(segments);
    const std::vector<Result> results = aggregator.AggregateResults(convreq);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, kValue);
    EXPECT_TRUE(results[0].candidate_attributes &
                converter::Candidate::USER_DICTIONARY);
  }
}

TEST_F(DictionaryPredictionAggregatorTest, EnrichPartialCandidates) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  request_test_util::FillMobileRequest(request_.get());

  Segments segments;
  SetUpInputForSuggestion("ぐーぐる", composer_.get(), &segments);

  const ConversionRequest convreq = CreatePredictionConversionRequest(segments);
  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  EXPECT_TRUE(GetMergedTypes(results) & PREFIX);
}

TEST_F(DictionaryPredictionAggregatorTest, PrefixCandidates) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  request_test_util::FillMobileRequest(request_.get());

  Segments segments;
  SetUpInputForSuggestion("ぐーぐるあ", composer_.get(), &segments);

  const ConversionRequest convreq = CreatePredictionConversionRequest(segments);
  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  EXPECT_TRUE(GetMergedTypes(results) & PREFIX);
  for (const auto &r : results) {
    if (r.types == PREFIX) {
      EXPECT_TRUE(r.candidate_attributes &
                  converter::Candidate::PARTIALLY_KEY_CONSUMED);
      EXPECT_NE(r.consumed_key_size, 0);
    }
  }
}

TEST_F(DictionaryPredictionAggregatorTest, CandidatesFromUserDictionary) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  const PosMatcher &pos_matcher = data_and_aggregator->pos_matcher();

  request_test_util::FillMobileRequest(request_.get());

  {
    MockDictionary *mock = data_and_aggregator->mutable_dictionary();
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
    Segments segments;
    SetUpInputForSuggestion("しょーとか", composer_.get(), &segments);

    const ConversionRequest convreq =
        CreatePredictionConversionRequest(segments);
    const std::vector<Result> results = aggregator.AggregateResults(convreq);
    EXPECT_TRUE(GetMergedTypes(results) & UNIGRAM);
    EXPECT_TRUE(FindResultByValue(results, "しょうとかっと"));
    EXPECT_FALSE(FindResultByValue(results, "ショートカット"));
  }
  {
    Segments segments;
    SetUpInputForSuggestion("しょーとかっと", composer_.get(), &segments);

    const ConversionRequest convreq =
        CreatePredictionConversionRequest(segments);
    const std::vector<Result> results = aggregator.AggregateResults(convreq);
    EXPECT_TRUE(GetMergedTypes(results) & UNIGRAM);
    EXPECT_TRUE(FindResultByValue(results, "しょうとかっと"));
    EXPECT_TRUE(FindResultByValue(results, "ショートカット"));
  }
}

namespace {
constexpr char kTestZeroQueryTokenArray[] =
    // The last two items must be 0x00, because they are now unused field.
    // {"あ", "❕", ZERO_QUERY_EMOJI, 0x00, 0x00}
    "\x04\x00\x00\x00"
    "\x02\x00\x00\x00"
    "\x03\x00"
    "\x00\x00"
    "\x00\x00\x00\x00"
    // {"ああ", "( •̀ㅁ•́;)", ZERO_QUERY_EMOTICON, 0x00, 0x00}
    "\x05\x00\x00\x00"
    "\x01\x00\x00\x00"
    "\x02\x00"
    "\x00\x00"
    "\x00\x00\x00\x00"
    // {"あい", "❕", ZERO_QUERY_EMOJI, 0x00, 0x00}
    "\x06\x00\x00\x00"
    "\x02\x00\x00\x00"
    "\x03\x00"
    "\x00\x00"
    "\x00\x00\x00\x00"
    // {"あい", "❣", ZERO_QUERY_NONE, 0x00, 0x00}
    "\x06\x00\x00\x00"
    "\x03\x00\x00\x00"
    "\x00\x00"
    "\x00\x00"
    "\x00\x00\x00\x00"
    // {"猫", "😾", ZERO_QUERY_EMOJI, 0x00, 0x00}
    "\x07\x00\x00\x00"
    "\x08\x00\x00\x00"
    "\x03\x00"
    "\x00\x00"
    "\x00\x00\x00\x00";

const char *kTestZeroQueryStrings[] = {"",     "( •̀ㅁ•́;)", "❕", "❣", "あ",
                                       "ああ", "あい",     "猫", "😾"};
}  // namespace

TEST_F(DictionaryPredictionAggregatorTest, GetZeroQueryCandidates) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  // Create test zero query data.
  std::unique_ptr<uint32_t[]> string_data_buffer;
  ZeroQueryDict zero_query_dict;
  {
    // kTestZeroQueryTokenArray contains a trailing '\0', so create a
    // absl::string_view that excludes it by subtracting 1.
    const absl::string_view token_array_data(
        kTestZeroQueryTokenArray, std::size(kTestZeroQueryTokenArray) - 1);
    std::vector<absl::string_view> strs;
    for (const char *str : kTestZeroQueryStrings) {
      strs.push_back(str);
    }
    const absl::string_view string_array_data =
        SerializedStringArray::SerializeToBuffer(strs, &string_data_buffer);
    zero_query_dict.Init(token_array_data, string_array_data);
  }

  struct TestCase {
    std::string key;
    bool expected_result;
    // candidate value and ZeroQueryType.
    std::vector<std::string> expected_candidates;
    std::vector<ZeroQueryType> expected_types;

    std::string DebugString() const {
      const std::string candidates = absl::StrJoin(expected_candidates, ", ");
      std::string types;
      for (size_t i = 0; i < expected_types.size(); ++i) {
        if (i != 0) {
          types.append(", ");
        }
        absl::StrAppendFormat(&types, "%d", types[i]);
      }
      return absl::StrFormat(
          "key: %s\n"
          "expected_result: %d\n"
          "expected_candidates: %s\n"
          "expected_types: %s",
          key, expected_result, candidates, types);
    }
  } kTestCases[] = {
      {"あい", true, {"❕", "❣"}, {ZERO_QUERY_EMOJI, ZERO_QUERY_NONE}},
      {"猫", true, {"😾"}, {ZERO_QUERY_EMOJI}},
      {"あ", false, {}, {}},  // Do not look up for one-char non-Kanji key
      {"あい", true, {"❕", "❣"}, {ZERO_QUERY_EMOJI, ZERO_QUERY_NONE}},
      {"あいう", false, {}, {}},
      {"", false, {}, {}},
      {"ああ", true, {"( •̀ㅁ•́;)"}, {ZERO_QUERY_EMOTICON}}};

  for (const auto &test_case : kTestCases) {
    ASSERT_EQ(test_case.expected_candidates.size(),
              test_case.expected_types.size());

    const ConversionRequest request;
    std::vector<Result> results;
    constexpr uint16_t kId = 0;  // EOS
    aggregator.GetZeroQueryCandidatesForKey(
        request, test_case.key, zero_query_dict, kId, kId, &results);
    EXPECT_EQ(results.size(), test_case.expected_candidates.size());
    for (size_t i = 0; i < test_case.expected_candidates.size(); ++i) {
      EXPECT_EQ(results[i].value, test_case.expected_candidates[i]);
      EXPECT_EQ(results[i].zero_query_type, test_case.expected_types[i]);
    }
  }
}

// b/235917071
TEST_F(DictionaryPredictionAggregatorTest, DoNotModifyHistorySegment) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  {
    // Set up mock immutable converter.
    MockImmutableConverter *immutable_converter =
        data_and_aggregator->mutable_immutable_converter();
    ::testing::Mock::VerifyAndClearExpectations(immutable_converter);

    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_segment_type(Segment::HISTORY);
    converter::Candidate *candidate = segment->add_candidate();
    candidate->key = "key_can_be_modified";
    candidate->value = "history_value";

    segment = segments.add_segment();
    candidate = segment->add_candidate();
    candidate->value = "conversion_result";

    EXPECT_CALL(*immutable_converter, ConvertForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(true);
  request_->set_mixed_conversion(true);

  Segments segments;
  SetUpInputForSuggestionWithHistory("てすと", "103", "103", composer_.get(),
                                     &segments);
  const ConversionRequest convreq = CreateConversionRequest(
      {.request_type = ConversionRequest::PREDICTION,
       .use_actual_converter_for_realtime_conversion = false},
      segments);

  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].value, "conversion_result");
  EXPECT_EQ(segments.history_segment(0).candidate(0).value, "103");
}

TEST_F(DictionaryPredictionAggregatorTest, NumberDecoderCandidates) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  request_test_util::FillMobileRequest(request_.get());

  Segments segments;
  SetUpInputForSuggestion("よんじゅうごかい", composer_.get(), &segments);

  const ConversionRequest convreq = CreatePredictionConversionRequest(segments);
  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  const auto &result =
      std::find_if(results.begin(), results.end(),
                   [](Result r) { return r.value == "45" && !r.removed; });
  ASSERT_NE(result, results.end());
  EXPECT_TRUE(result->candidate_attributes &
              converter::Candidate::PARTIALLY_KEY_CONSUMED);
  EXPECT_TRUE(result->candidate_attributes &
              converter::Candidate::NO_SUGGEST_LEARNING);
}

TEST_F(DictionaryPredictionAggregatorTest, DoNotPredictNoisyNumberEntries) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  request_test_util::FillMobileRequest(request_.get());

  {
    MockDictionary *mock = data_and_aggregator->mutable_dictionary();
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
  Segments segments;
  SetUpInputForSuggestion("1", composer_.get(), &segments);

  const ConversionRequest convreq = CreatePredictionConversionRequest(segments);
  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  EXPECT_FALSE(FindResultByValue(results, "10時"));
  EXPECT_FALSE(FindResultByValue(results, "十時"));
  EXPECT_FALSE(FindResultByValue(results, "1時過ぎ"));
  EXPECT_FALSE(FindResultByValue(results, "19時"));

  EXPECT_TRUE(FindResultByValue(results, "一"));
  EXPECT_TRUE(FindResultByValue(results, "一時"));
  EXPECT_TRUE(FindResultByValue(results, "1時"));
}

TEST_F(DictionaryPredictionAggregatorTest, SingleKanji) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  request_test_util::FillMobileRequest(request_.get());

  {
    auto create_single_kanji_result = [](absl::string_view key,
                                         absl::string_view value) {
      Result result;
      result.key = std::string(key);
      result.value = std::string(value);
      result.SetTypesAndTokenAttributes(SINGLE_KANJI, Token::NONE);
      return result;
    };
    MockSingleKanjiPredictionAggregator *mock =
        data_and_aggregator->mutable_single_kanji_prediction_aggregator();
    EXPECT_CALL(*mock, AggregateResults(_))
        .WillOnce(Return(
            std::vector<Result>{create_single_kanji_result("て", "手")}));
  }

  Segments segments;
  SetUpInputForSuggestion("てすと", composer_.get(), &segments);

  const ConversionRequest convreq = CreatePredictionConversionRequest(segments);
  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  EXPECT_TRUE(GetMergedTypes(results) & SINGLE_KANJI);
  for (const auto &result : results) {
    if (!(result.types & SINGLE_KANJI)) {
      EXPECT_GT(Util::CharsLen(result.value), 1);
    }
  }
}

TEST_F(DictionaryPredictionAggregatorTest,
       SingleKanjiForMobileHardwareKeyboard) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  request_test_util::FillMobileRequestWithHardwareKeyboard(request_.get());

  {
    MockSingleKanjiPredictionAggregator *mock =
        data_and_aggregator->mutable_single_kanji_prediction_aggregator();
    EXPECT_CALL(*mock, AggregateResults(_)).Times(0);
  }

  Segments segments;
  SetUpInputForSuggestion("てすと", composer_.get(), &segments);

  const ConversionRequest convreq = CreatePredictionConversionRequest(segments);
  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  EXPECT_FALSE(GetMergedTypes(results) & SINGLE_KANJI);
}

TEST_F(DictionaryPredictionAggregatorTest, Handwriting) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  MockDictionary *mock_dict = data_and_aggregator->mutable_dictionary();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  constexpr int kCostOffset = 3000;
  Segments segments;
  // Handwriting request
  request_test_util::FillMobileRequestForHandwriting(request_.get());
  request_->mutable_decoder_experiment_params()
      ->set_max_composition_event_to_process(1);
  request_->mutable_decoder_experiment_params()
      ->set_handwriting_conversion_candidate_cost_offset(kCostOffset);
  {
    commands::SessionCommand command;
    commands::SessionCommand::CompositionEvent *composition_event =
        command.add_composition_events();
    composition_event->set_composition_string("かん字じ典");
    composition_event->set_probability(0.99);
    composition_event = command.add_composition_events();
    composition_event->set_composition_string("かlv字じ典");
    composition_event->set_probability(0.01);
    composer_->Reset();
    composer_->SetCompositionsForHandwriting(command.composition_events());

    Segment *seg = segments.add_segment();
    seg->set_key("かん字じ典");
    seg->set_segment_type(Segment::FREE);
  }

  // reverse conversion
  {
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("かん");
    converter::Candidate *candidate = segment->add_candidate();
    candidate->value = "かん";
    candidate->key = "かん";

    segment = segments.add_segment();
    segment->set_key("字じ");
    candidate = segment->add_candidate();
    candidate->value = "じじ";
    candidate->key = "字じ";

    segment = segments.add_segment();
    segment->set_key("典");
    candidate = segment->add_candidate();
    candidate->value = "てん";
    candidate->key = "典";

    EXPECT_CALL(
        *data_and_aggregator->mutable_immutable_converter(),
        ConvertForRequest(Truly([](const ConversionRequest &request) {
                            return request.request_type() ==
                                   ConversionRequest::REVERSE_CONVERSION;
                          }),
                          _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
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

  const ConversionRequest convreq = CreatePredictionConversionRequest(segments);
  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  EXPECT_TRUE(GetMergedTypes(results) & UNIGRAM);

  EXPECT_GE(results.size(), 5);
  // composition from handwriting output
  EXPECT_TRUE(FindResultByKeyValue(results, "かんじじてん", "かん字じ典"));
  EXPECT_TRUE(FindResultByKeyValue(results, "かlv字じ典", "かlv字じ典"));
  // look-up results
  EXPECT_TRUE(FindResultByKeyValue(results, "かんじじてん", "漢字辞典"));
  EXPECT_TRUE(FindResultByKeyValue(results, "かんじじてん", "漢字字典"));
  EXPECT_TRUE(FindResultByKeyValue(results, "かんじじてん", "換字字典"));

  for (const Result &result : results) {
    if (result.value == "かん字じ典") {
      // Top recognition result
      EXPECT_EQ(result.wcost, 0);
    } else if (result.key == "かんじじてん") {
      EXPECT_GE(result.wcost, kCostOffset);
    }
  }
}

TEST_F(DictionaryPredictionAggregatorTest, HandwritingT13N) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  MockDictionary *mock_dict = data_and_aggregator->mutable_dictionary();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  Segments segments;
  // Handwriting request
  request_test_util::FillMobileRequestForHandwriting(request_.get());
  request_->mutable_decoder_experiment_params()
      ->set_max_composition_event_to_process(1);
  {
    commands::SessionCommand command;
    commands::SessionCommand::CompositionEvent *composition_event =
        command.add_composition_events();
    composition_event->set_composition_string("キた");
    composition_event->set_probability(0.99);
    composition_event = command.add_composition_events();
    composition_event->set_composition_string("もた");
    composition_event->set_probability(0.01);
    composer_->Reset();
    composer_->SetCompositionsForHandwriting(command.composition_events());

    Segment *seg = segments.add_segment();
    seg->set_key("キた");
    seg->set_segment_type(Segment::FREE);
  }

  // reverse conversion
  {
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("キた");
    converter::Candidate *candidate = segment->add_candidate();
    candidate->value = "きた";
    candidate->key = "きた";  // T13N key can be looked up

    EXPECT_CALL(
        *data_and_aggregator->mutable_immutable_converter(),
        ConvertForRequest(Truly([](const ConversionRequest &request) {
                            return request.request_type() ==
                                   ConversionRequest::REVERSE_CONVERSION;
                          }),
                          _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  EXPECT_CALL(*mock_dict, LookupPredictive(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(*mock_dict, LookupExact(StrEq("きた"), _, _))
      .WillRepeatedly(InvokeCallbackWithKeyValues{{
          {"きた", "きた"},
          {"きた", "北"},
      }});

  const ConversionRequest convreq = CreatePredictionConversionRequest(segments);
  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  EXPECT_TRUE(GetMergedTypes(results) & UNIGRAM);

  EXPECT_GE(results.size(), 2);
  // composition from handwriting output
  EXPECT_TRUE(FindResultByKeyValue(results, "きた", "キた"));
  EXPECT_TRUE(FindResultByKeyValue(results, "もた", "もた"));
  // No "きた", "北"
}

TEST_F(DictionaryPredictionAggregatorTest, HandwritingNoHiragana) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  MockDictionary *mock_dict = data_and_aggregator->mutable_dictionary();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  Segments segments;
  // Handwriting request
  request_test_util::FillMobileRequestForHandwriting(request_.get());
  request_->mutable_decoder_experiment_params()
      ->set_max_composition_event_to_process(1);
  {
    commands::SessionCommand command;
    commands::SessionCommand::CompositionEvent *composition_event =
        command.add_composition_events();
    composition_event->set_composition_string("南");
    composition_event->set_probability(0.9);
    composer_->Reset();
    composer_->SetCompositionsForHandwriting(command.composition_events());

    Segment *seg = segments.add_segment();
    seg->set_key("南");
    seg->set_segment_type(Segment::FREE);
  }

  // reverse conversion will not be called
  {
    EXPECT_CALL(
        *data_and_aggregator->mutable_immutable_converter(),
        ConvertForRequest(Truly([](const ConversionRequest &request) {
                            return request.request_type() ==
                                   ConversionRequest::REVERSE_CONVERSION;
                          }),
                          _))
        .Times(0);
  }

  EXPECT_CALL(*mock_dict, LookupPredictive(_, _, _)).Times(0);
  EXPECT_CALL(*mock_dict, LookupExact(_, _, _)).Times(0);

  const ConversionRequest convreq = CreatePredictionConversionRequest(segments);
  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  EXPECT_TRUE(GetMergedTypes(results) & UNIGRAM);
  EXPECT_GE(results.size(), 1);
  // composition from handwriting output
  EXPECT_TRUE(FindResultByKeyValue(results, "南", "南"));
}

TEST_F(DictionaryPredictionAggregatorTest, HandwritingRealtime) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  Segments segments;
  // Handwriting request
  request_test_util::FillMobileRequestForHandwriting(request_.get());
  request_->mutable_decoder_experiment_params()
      ->set_max_composition_event_to_process(1);
  {
    commands::SessionCommand command;
    commands::SessionCommand::CompositionEvent *composition_event =
        command.add_composition_events();
    composition_event->set_composition_string("ばらが");
    composition_event->set_probability(0.9);
    composer_->Reset();
    composer_->SetCompositionsForHandwriting(command.composition_events());

    Segment *seg = segments.add_segment();
    seg->set_key("ばらが");
    seg->set_segment_type(Segment::FREE);
  }

  // reverse conversion
  {
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("ばらが");
    converter::Candidate *candidate = segment->add_candidate();
    candidate->key = "ばらが";
    candidate->value = "薔薇が";
    candidate->content_key = "ばら";
    candidate->content_value = "薔薇";

    EXPECT_CALL(*data_and_aggregator->mutable_immutable_converter(),
                ConvertForRequest(Truly([](const ConversionRequest &request) {
                                    return request.request_type() ==
                                           ConversionRequest::PREDICTION;
                                  }),
                                  _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  const ConversionRequest convreq = CreatePredictionConversionRequest(segments);
  const std::vector<Result> results = aggregator.AggregateResults(convreq);
  EXPECT_TRUE(GetMergedTypes(results) & UNIGRAM);

  EXPECT_GE(results.size(), 2);
  // composition from handwriting output
  EXPECT_TRUE(FindResultByKeyValue(results, "ばらが", "ばらが"));
  EXPECT_TRUE(FindResultByKeyValue(results, "ばらが", "薔薇が"));
}

}  // namespace
}  // namespace prediction
}  // namespace mozc
