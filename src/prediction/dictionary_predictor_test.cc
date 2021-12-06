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
#include <cstdint>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/port.h"
#include "base/serialized_string_array.h"
#include "base/system_util.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/internal/typing_model.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/connector.h"
#include "converter/converter_interface.h"
#include "converter/converter_mock.h"
#include "converter/immutable_converter.h"
#include "converter/immutable_converter_interface.h"
#include "converter/node_allocator.h"
#include "converter/segmenter.h"
#include "converter/segments.h"
#include "data_manager/data_manager_interface.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suffix_dictionary.h"
#include "dictionary/suppression_dictionary.h"
#include "dictionary/system/system_dictionary.h"
#include "prediction/suggestion_filter.h"
#include "prediction/zero_query_dict.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "session/request_test_util.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "transliteration/transliteration.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"
#include "absl/flags/flag.h"
#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {

using dictionary::DictionaryInterface;
using dictionary::DictionaryMock;
using dictionary::PosGroup;
using dictionary::PosMatcher;
using dictionary::SuffixDictionary;
using dictionary::SuppressionDictionary;
using dictionary::SystemDictionary;
using dictionary::Token;
using ::testing::_;
using ::testing::WithParamInterface;

constexpr int kInfinity = (2 << 20);

absl::StatusOr<std::unique_ptr<SystemDictionary>>
CreateSystemDictionaryFromDataManager(
    const DataManagerInterface &data_manager) {
  const char *data = nullptr;
  int size = 0;
  data_manager.GetSystemDictionaryData(&data, &size);
  using mozc::dictionary::SystemDictionary;
  return SystemDictionary::Builder(data, size).Build();
}

DictionaryInterface *CreateSuffixDictionaryFromDataManager(
    const DataManagerInterface &data_manager) {
  absl::string_view suffix_key_array_data, suffix_value_array_data;
  const uint32_t *token_array;
  data_manager.GetSuffixDictionaryData(&suffix_key_array_data,
                                       &suffix_value_array_data, &token_array);
  return new SuffixDictionary(suffix_key_array_data, suffix_value_array_data,
                              token_array);
}

SuggestionFilter *CreateSuggestionFilter(
    const DataManagerInterface &data_manager) {
  const char *data = nullptr;
  size_t size = 0;
  data_manager.GetSuggestionFilterData(&data, &size);
  return new SuggestionFilter(data, size);
}

enum Platform { DESKTOP, MOBILE };

// Simple immutable converter mock for the realtime conversion test
class ImmutableConverterMock : public ImmutableConverterInterface {
 public:
  ImmutableConverterMock() {
    Segment *segment = segments_.add_segment();
    segment->set_key("わたしのなまえはなかのです");
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = "私の名前は中野です";
    candidate->key = ("わたしのなまえはなかのです");
    // "わたしの, 私の", "わたし, 私"
    candidate->PushBackInnerSegmentBoundary(12, 6, 9, 3);
    // "なまえは, 名前は", "なまえ, 名前"
    candidate->PushBackInnerSegmentBoundary(12, 9, 9, 6);
    // "なかのです, 中野です", "なかの, 中野"
    candidate->PushBackInnerSegmentBoundary(15, 12, 9, 6);
  }

  bool ConvertForRequest(const ConversionRequest &request,
                         Segments *segments) const override {
    *segments = segments_;
    return true;
  }

 private:
  Segments segments_;
};

class TestableDictionaryPredictor : public DictionaryPredictor {
  // Test-only subclass: Just changing access levels
 public:
  TestableDictionaryPredictor(
      const DataManagerInterface &data_manager,
      const ConverterInterface *converter,
      const ImmutableConverterInterface *immutable_converter,
      const DictionaryInterface *dictionary,
      const DictionaryInterface *suffix_dictionary, const Connector *connector,
      const Segmenter *segmenter, const PosMatcher *pos_matcher,
      const SuggestionFilter *suggestion_filter)
      : DictionaryPredictor(data_manager, converter, immutable_converter,
                            dictionary, suffix_dictionary, connector, segmenter,
                            pos_matcher, suggestion_filter) {}

  using DictionaryPredictor::AddPredictionToCandidates;
  using DictionaryPredictor::AggregateBigramPrediction;
  using DictionaryPredictor::AggregateEnglishPrediction;
  using DictionaryPredictor::AggregateRealtimeConversion;
  using DictionaryPredictor::AggregateSuffixPrediction;
  using DictionaryPredictor::AggregateTypeCorrectingPrediction;
  using DictionaryPredictor::AggregateUnigramCandidate;
  using DictionaryPredictor::AggregateUnigramCandidateForMixedConversion;
  using DictionaryPredictor::AggregateZeroQuerySuffixPrediction;
  using DictionaryPredictor::ApplyPenaltyForKeyExpansion;
  using DictionaryPredictor::BIGRAM;
  using DictionaryPredictor::ENGLISH;
  using DictionaryPredictor::MakeEmptyResult;
  using DictionaryPredictor::NO_PREDICTION;
  using DictionaryPredictor::PredictionTypes;
  using DictionaryPredictor::REALTIME;
  using DictionaryPredictor::REALTIME_TOP;
  using DictionaryPredictor::Result;
  using DictionaryPredictor::SUFFIX;
  using DictionaryPredictor::TYPING_CORRECTION;
  using DictionaryPredictor::UNIGRAM;
};

// Helper class to hold dictionary data and predictor objects.
class MockDataAndPredictor {
 public:
  // Initializes predictor with given dictionary and suffix_dictionary.  When
  // nullptr is passed to the first argument |dictionary|, the default
  // DictionaryMock is used. For the second, the default is MockDataManager's
  // suffix dictionary. Note that |dictionary| is owned by this class but
  // |suffix_dictionary| is NOT owned because the current design assumes that
  // suffix dictionary is singleton.
  void Init(const DictionaryInterface *dictionary = nullptr,
            const DictionaryInterface *suffix_dictionary = nullptr) {
    pos_matcher_.Set(data_manager_.GetPosMatcherData());
    suppression_dictionary_ = absl::make_unique<SuppressionDictionary>();
    if (!dictionary) {
      dictionary_mock_ = new DictionaryMock;
      dictionary_.reset(dictionary_mock_);
    } else {
      dictionary_mock_ = nullptr;
      dictionary_.reset(dictionary);
    }
    if (!suffix_dictionary) {
      suffix_dictionary_.reset(
          CreateSuffixDictionaryFromDataManager(data_manager_));
    } else {
      suffix_dictionary_.reset(suffix_dictionary);
    }
    CHECK(suffix_dictionary_.get());

    connector_ = Connector::CreateFromDataManager(data_manager_).value();

    segmenter_.reset(Segmenter::CreateFromDataManager(data_manager_));
    CHECK(segmenter_.get());

    pos_group_ = absl::make_unique<PosGroup>(data_manager_.GetPosGroupData());
    suggestion_filter_.reset(CreateSuggestionFilter(data_manager_));
    immutable_converter_ = absl::make_unique<ImmutableConverterImpl>(
        dictionary_.get(), suffix_dictionary_.get(),
        suppression_dictionary_.get(), connector_.get(), segmenter_.get(),
        &pos_matcher_, pos_group_.get(), suggestion_filter_.get());
    converter_ = absl::make_unique<ConverterMock>();
    dictionary_predictor_ = absl::make_unique<TestableDictionaryPredictor>(
        data_manager_, converter_.get(), immutable_converter_.get(),
        dictionary_.get(), suffix_dictionary_.get(), connector_.get(),
        segmenter_.get(), &pos_matcher_, suggestion_filter_.get());
  }

  const PosMatcher &pos_matcher() const { return pos_matcher_; }

  DictionaryMock *mutable_dictionary() { return dictionary_mock_; }

  ConverterMock *mutable_converter_mock() { return converter_.get(); }

  const TestableDictionaryPredictor *dictionary_predictor() {
    return dictionary_predictor_.get();
  }

  TestableDictionaryPredictor *mutable_dictionary_predictor() {
    return dictionary_predictor_.get();
  }

 private:
  const testing::MockDataManager data_manager_;
  PosMatcher pos_matcher_;
  std::unique_ptr<SuppressionDictionary> suppression_dictionary_;
  std::unique_ptr<const Connector> connector_;
  std::unique_ptr<const Segmenter> segmenter_;
  std::unique_ptr<const DictionaryInterface> suffix_dictionary_;
  std::unique_ptr<const DictionaryInterface> dictionary_;
  DictionaryMock *dictionary_mock_;
  std::unique_ptr<const PosGroup> pos_group_;
  std::unique_ptr<ImmutableConverterInterface> immutable_converter_;
  std::unique_ptr<ConverterMock> converter_;
  std::unique_ptr<const SuggestionFilter> suggestion_filter_;
  std::unique_ptr<TestableDictionaryPredictor> dictionary_predictor_;
};

class CallCheckDictionary : public DictionaryInterface {
 public:
  CallCheckDictionary() = default;
  ~CallCheckDictionary() override = default;

  MOCK_METHOD(bool, HasKey, (absl::string_view), (const, override));
  MOCK_METHOD(bool, HasValue, (absl::string_view), (const, override));
  MOCK_METHOD(void, LookupPredictive,
              (absl::string_view key, const ConversionRequest &convreq,
               Callback *callback),
              (const, override));
  MOCK_METHOD(void, LookupPrefix,
              (absl::string_view key, const ConversionRequest &convreq,
               Callback *callback),
              (const, override));
  MOCK_METHOD(void, LookupExact,
              (absl::string_view key, const ConversionRequest &convreq,
               Callback *callback),
              (const, override));
  MOCK_METHOD(void, LookupReverse,
              (absl::string_view str, const ConversionRequest &convreq,
               Callback *callback),
              (const, override));
};

// Action to call the third argument of LookupPrefix with the token
// <key, value>.
ACTION_P4(LookupPrefixOneToken, key, value, lid, rid) {
  Token token;
  token.key = key;
  token.value = value;
  token.lid = lid;
  token.rid = rid;
  arg2->OnToken(key, key, token);
}

void MakeSegmentsForSuggestion(const std::string key, Segments *segments) {
  segments->Clear();
  segments->set_request_type(Segments::SUGGESTION);
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FREE);
}

void MakeSegmentsForPrediction(const std::string key, Segments *segments) {
  segments->Clear();
  segments->set_request_type(Segments::PREDICTION);
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FREE);
}

void PrependHistorySegments(const std::string &key, const std::string &value,
                            Segments *segments) {
  Segment *seg = segments->push_front_segment();
  seg->set_segment_type(Segment::HISTORY);
  seg->set_key(key);
  Segment::Candidate *c = seg->add_candidate();
  c->key = key;
  c->content_key = key;
  c->value = value;
  c->content_value = value;
}

void SetUpInputForSuggestion(const std::string &key,
                             composer::Composer *composer, Segments *segments) {
  composer->Reset();
  composer->SetPreeditTextForTestOnly(key);
  MakeSegmentsForSuggestion(key, segments);
}

void SetUpInputForSuggestionWithHistory(const std::string &key,
                                        const std::string &hist_key,
                                        const std::string &hist_value,
                                        composer::Composer *composer,
                                        Segments *segments) {
  SetUpInputForSuggestion(key, composer, segments);
  PrependHistorySegments(hist_key, hist_value, segments);
}

class MockTypingModel : public mozc::composer::TypingModel {
 public:
  MockTypingModel() : TypingModel(nullptr, 0, nullptr, 0, nullptr) {}
  ~MockTypingModel() override = default;
  int GetCost(absl::string_view key) const override { return 10; }
};

}  // namespace

class DictionaryPredictorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    SystemUtil::SetUserProfileDirectory(absl::GetFlag(FLAGS_test_tmpdir));
    request_ = absl::make_unique<commands::Request>();
    config_ = absl::make_unique<config::Config>();
    config::ConfigHandler::GetDefaultConfig(config_.get());
    table_ = absl::make_unique<composer::Table>();
    composer_ = absl::make_unique<composer::Composer>(
        table_.get(), request_.get(), config_.get());
    convreq_ = absl::make_unique<ConversionRequest>(
        composer_.get(), request_.get(), config_.get());
    convreq_for_suggestion_ = absl::make_unique<ConversionRequest>(
        composer_.get(), request_.get(), config_.get());
    convreq_for_suggestion_->set_max_dictionary_prediction_candidates_size(10);
    convreq_for_prediction_ = absl::make_unique<ConversionRequest>(
        composer_.get(), request_.get(), config_.get());
    convreq_for_prediction_->set_max_dictionary_prediction_candidates_size(50);

    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
  }

  void TearDown() override {
    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
  }

  static void AddWordsToMockDic(DictionaryMock *mock) {
    constexpr char kGoogleA[] = "ぐーぐるあ";

    constexpr char kGoogleAdsenseHiragana[] = "ぐーぐるあどせんす";
    constexpr char kGoogleAdsenseKatakana[] = "グーグルアドセンス";
    mock->AddLookupPredictive(kGoogleA, kGoogleAdsenseHiragana,
                              kGoogleAdsenseKatakana, Token::NONE);

    constexpr char kGoogleAdwordsHiragana[] = "ぐーぐるあどわーず";
    constexpr char kGoogleAdwordsKatakana[] = "グーグルアドワーズ";
    mock->AddLookupPredictive(kGoogleA, kGoogleAdwordsHiragana,
                              kGoogleAdwordsKatakana, Token::NONE);

    constexpr char kGoogle[] = "ぐーぐる";
    mock->AddLookupPredictive(kGoogle, kGoogleAdsenseHiragana,
                              kGoogleAdsenseKatakana, Token::NONE);
    mock->AddLookupPredictive(kGoogle, kGoogleAdwordsHiragana,
                              kGoogleAdwordsKatakana, Token::NONE);

    constexpr char kGoogleKatakana[] = "グーグル";
    mock->AddLookupPrefix(kGoogle, kGoogleKatakana, kGoogleKatakana,
                          Token::NONE);

    constexpr char kAdsense[] = "あどせんす";
    constexpr char kAdsenseKatakana[] = "アドセンス";
    mock->AddLookupPrefix(kAdsense, kAdsenseKatakana, kAdsenseKatakana,
                          Token::NONE);

    constexpr char kTestHiragana[] = "てすと";
    constexpr char kTestKatakana[] = "テスト";
    mock->AddLookupPrefix(kTestHiragana, kTestHiragana, kTestKatakana,
                          Token::NONE);

    constexpr char kFilterHiragana[] = "ふぃるたーたいしょう";
    constexpr char kFilterPrefixHiragana[] = "ふぃるたーたいし";

    // Note: This is in the filter
    constexpr char kFilterWord[] = "フィルター対象";

    // Note: This is NOT in the filter
    constexpr char kNonFilterWord[] = "フィルター大将";

    mock->AddLookupPrefix(kFilterHiragana, kFilterHiragana, kFilterWord,
                          Token::NONE);

    mock->AddLookupPrefix(kFilterHiragana, kFilterHiragana, kNonFilterWord,
                          Token::NONE);

    mock->AddLookupPredictive(kFilterHiragana, kFilterHiragana, kFilterWord,
                              Token::NONE);

    mock->AddLookupPredictive(kFilterHiragana, kFilterPrefixHiragana,
                              kFilterWord, Token::NONE);

    constexpr char kWrongCapriHiragana[] = "かぷりちょうざ";
    constexpr char kRightCapriHiragana[] = "かぷりちょーざ";
    constexpr char kCapriKatakana[] = "カプリチョーザ";

    mock->AddLookupPrefix(kWrongCapriHiragana, kRightCapriHiragana,
                          kCapriKatakana, Token::SPELLING_CORRECTION);

    mock->AddLookupPredictive(kWrongCapriHiragana, kRightCapriHiragana,
                              kCapriKatakana, Token::SPELLING_CORRECTION);

    constexpr char kDe[] = "で";

    mock->AddLookupPrefix(kDe, kDe, kDe, Token::NONE);

    constexpr char kHirosueHiragana[] = "ひろすえ";
    constexpr char kHirosue[] = "広末";

    mock->AddLookupPrefix(kHirosueHiragana, kHirosueHiragana, kHirosue,
                          Token::NONE);

    constexpr char kYuzaHiragana[] = "ゆーざー";
    constexpr char kYuza[] = "ユーザー";
    // For dictionary suggestion
    mock->AddLookupPredictive(kYuzaHiragana, kYuzaHiragana, kYuza,
                              Token::USER_DICTIONARY);
    // For realtime conversion
    mock->AddLookupPrefix(kYuzaHiragana, kYuzaHiragana, kYuza,
                          Token::USER_DICTIONARY);

    // Some English entries
    mock->AddLookupPredictive("conv", "converge", "converge", Token::NONE);
    mock->AddLookupPredictive("conv", "converged", "converged", Token::NONE);
    mock->AddLookupPredictive("conv", "convergent", "convergent", Token::NONE);
    mock->AddLookupPredictive("con", "contraction", "contraction", Token::NONE);
    mock->AddLookupPredictive("con", "control", "control", Token::NONE);
  }

  MockDataAndPredictor *CreateDictionaryPredictorWithMockData() {
    MockDataAndPredictor *ret = new MockDataAndPredictor;
    ret->Init();
    AddWordsToMockDic(ret->mutable_dictionary());
    return ret;
  }

  void GenerateKeyEvents(const std::string &text,
                         std::vector<commands::KeyEvent> *keys) {
    keys->clear();

    const char *begin = text.data();
    const char *end = text.data() + text.size();
    size_t mblen = 0;

    while (begin < end) {
      commands::KeyEvent key;
      const char32 w = Util::Utf8ToUcs4(begin, end, &mblen);
      if (0 <= w && w <= 0x7F) {  // IsAscii
        key.set_key_code(*begin);
      } else {
        key.set_key_code('?');
        key.set_key_string(std::string(begin, mblen));
      }
      begin += mblen;
      keys->push_back(key);
    }
  }

  void InsertInputSequence(const std::string &text,
                           composer::Composer *composer) {
    std::vector<commands::KeyEvent> keys;
    GenerateKeyEvents(text, &keys);

    for (size_t i = 0; i < keys.size(); ++i) {
      composer->InsertCharacterKeyEvent(keys[i]);
    }
  }

  void InsertInputSequenceForProbableKeyEvent(
      const std::string &text, const uint32_t *corrected_key_codes,
      composer::Composer *composer) {
    std::vector<commands::KeyEvent> keys;
    GenerateKeyEvents(text, &keys);

    for (size_t i = 0; i < keys.size(); ++i) {
      if (keys[i].key_code() != corrected_key_codes[i]) {
        commands::KeyEvent::ProbableKeyEvent *probable_key_event;

        probable_key_event = keys[i].add_probable_key_event();
        probable_key_event->set_key_code(keys[i].key_code());
        probable_key_event->set_probability(0.9f);

        probable_key_event = keys[i].add_probable_key_event();
        probable_key_event->set_key_code(corrected_key_codes[i]);
        probable_key_event->set_probability(0.1f);
      }
      composer->InsertCharacterKeyEvent(keys[i]);
    }
  }

  void ExpansionForUnigramTestHelper(bool use_expansion) {
    config_->set_use_dictionary_suggest(true);
    config_->set_use_realtime_conversion(false);
    config_->set_use_kana_modifier_insensitive_conversion(use_expansion);

    table_->LoadFromFile("system://romanji-hiragana.tsv");
    composer_->SetTable(table_.get());
    std::unique_ptr<MockDataAndPredictor> data_and_predictor(
        new MockDataAndPredictor);
    // CallCheckDictionary is managed by data_and_predictor;
    CallCheckDictionary *check_dictionary = new CallCheckDictionary;
    data_and_predictor->Init(check_dictionary, nullptr);
    const TestableDictionaryPredictor *predictor =
        data_and_predictor->dictionary_predictor();

    {
      Segments segments;
      segments.set_request_type(Segments::PREDICTION);
      request_->set_kana_modifier_insensitive_conversion(use_expansion);
      InsertInputSequence("gu-g", composer_.get());
      Segment *segment = segments.add_segment();
      CHECK(segment);
      std::string query;
      composer_->GetQueryForPrediction(&query);
      segment->set_key(query);

      EXPECT_CALL(*check_dictionary,
                  LookupPredictive(::testing::Ne(""),
                                   ::testing::Ref(*convreq_for_prediction_), _))
          .Times(::testing::AtLeast(1));

      std::vector<TestableDictionaryPredictor::Result> results;
      predictor->AggregateUnigramCandidate(*convreq_for_prediction_, segments,
                                           &results);
    }
  }

  void ExpansionForBigramTestHelper(bool use_expansion) {
    config_->set_use_dictionary_suggest(true);
    config_->set_use_realtime_conversion(false);
    config_->set_use_kana_modifier_insensitive_conversion(use_expansion);

    table_->LoadFromFile("system://romanji-hiragana.tsv");
    composer_->SetTable(table_.get());
    std::unique_ptr<MockDataAndPredictor> data_and_predictor(
        new MockDataAndPredictor);
    // CallCheckDictionary is managed by data_and_predictor;
    CallCheckDictionary *check_dictionary = new CallCheckDictionary;
    data_and_predictor->Init(check_dictionary, nullptr);
    const TestableDictionaryPredictor *predictor =
        data_and_predictor->dictionary_predictor();

    {
      Segments segments;
      segments.set_request_type(Segments::PREDICTION);
      // History segment's key and value should be in the dictionary
      Segment *segment = segments.add_segment();
      CHECK(segment);
      segment->set_segment_type(Segment::HISTORY);
      segment->set_key("ぐーぐる");
      Segment::Candidate *cand = segment->add_candidate();
      cand->key = "ぐーぐる";
      cand->content_key = "ぐーぐる";
      cand->value = "グーグル";
      cand->content_value = "グーグル";

      segment = segments.add_segment();
      CHECK(segment);

      request_->set_kana_modifier_insensitive_conversion(use_expansion);
      InsertInputSequence("m", composer_.get());
      std::string query;
      composer_->GetQueryForPrediction(&query);
      segment->set_key(query);

      // History key and value should be in the dictionary.
      EXPECT_CALL(*check_dictionary,
                  LookupPrefix(_, ::testing::Ref(*convreq_for_prediction_), _))
          .WillOnce(LookupPrefixOneToken("ぐーぐる", "グーグル", 1, 1));
      EXPECT_CALL(
          *check_dictionary,
          LookupPredictive(_, ::testing::Ref(*convreq_for_prediction_), _));

      std::vector<TestableDictionaryPredictor::Result> results;
      predictor->AggregateBigramPrediction(*convreq_for_prediction_, segments,
                                           Segment::Candidate::SOURCE_INFO_NONE,
                                           &results);
    }
  }

  void ExpansionForSuffixTestHelper(bool use_expansion) {
    config_->set_use_dictionary_suggest(true);
    config_->set_use_realtime_conversion(false);
    config_->set_use_kana_modifier_insensitive_conversion(use_expansion);

    table_->LoadFromFile("system://romanji-hiragana.tsv");
    composer_->SetTable(table_.get());
    std::unique_ptr<MockDataAndPredictor> data_and_predictor(
        new MockDataAndPredictor);
    // CallCheckDictionary is managed by data_and_predictor.
    CallCheckDictionary *check_dictionary = new CallCheckDictionary;
    data_and_predictor->Init(nullptr, check_dictionary);
    const TestableDictionaryPredictor *predictor =
        data_and_predictor->dictionary_predictor();

    {
      Segments segments;
      segments.set_request_type(Segments::PREDICTION);
      Segment *segment = segments.add_segment();
      CHECK(segment);

      request_->set_kana_modifier_insensitive_conversion(use_expansion);
      InsertInputSequence("des", composer_.get());
      std::string query;
      composer_->GetQueryForPrediction(&query);
      segment->set_key(query);

      EXPECT_CALL(*check_dictionary,
                  LookupPredictive(::testing::Ne(""),
                                   ::testing::Ref(*convreq_for_prediction_), _))
          .Times(::testing::AtLeast(1));

      std::vector<TestableDictionaryPredictor::Result> results;
      predictor->AggregateSuffixPrediction(*convreq_for_prediction_, segments,
                                           &results);
    }
  }

  bool FindCandidateByValue(const Segment &segment, const std::string &value) {
    for (size_t i = 0; i < segment.candidates_size(); ++i) {
      const Segment::Candidate &c = segment.candidate(i);
      if (c.value == value) {
        return true;
      }
    }
    return false;
  }

  bool FindResultByValue(
      const std::vector<TestableDictionaryPredictor::Result> &results,
      const std::string &value) {
    for (const auto &result : results) {
      if (result.value == value && !result.removed) {
        return true;
      }
    }
    return false;
  }

  void AggregateEnglishPredictionTestHelper(
      transliteration::TransliterationType input_mode, const char *key,
      const char *expected_prefix, const char *expected_values[],
      size_t expected_values_size) {
    std::unique_ptr<MockDataAndPredictor> data_and_predictor(
        CreateDictionaryPredictorWithMockData());
    const TestableDictionaryPredictor *predictor =
        data_and_predictor->dictionary_predictor();

    table_->LoadFromFile("system://romanji-hiragana.tsv");
    composer_->Reset();
    composer_->SetTable(table_.get());
    composer_->SetInputMode(input_mode);
    InsertInputSequence(key, composer_.get());

    Segments segments;
    MakeSegmentsForPrediction(key, &segments);

    std::vector<TestableDictionaryPredictor::Result> results;
    predictor->AggregateEnglishPrediction(*convreq_for_prediction_, segments,
                                          &results);

    std::set<std::string> values;
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_EQ(TestableDictionaryPredictor::ENGLISH, results[i].types);
      EXPECT_TRUE(absl::StartsWith(results[i].value, expected_prefix))
          << results[i].value << " doesn't start with " << expected_prefix;
      values.insert(results[i].value);
    }
    for (size_t i = 0; i < expected_values_size; ++i) {
      EXPECT_TRUE(values.find(expected_values[i]) != values.end())
          << expected_values[i] << " isn't in the results";
    }
  }

  void AggregateTypeCorrectingTestHelper(const char *key,
                                         const uint32_t *corrected_key_codes,
                                         const char *expected_values[],
                                         size_t expected_values_size) {
    request_->set_special_romanji_table(
        commands::Request::QWERTY_MOBILE_TO_HIRAGANA);

    std::unique_ptr<MockDataAndPredictor> data_and_predictor(
        CreateDictionaryPredictorWithMockData());
    const TestableDictionaryPredictor *predictor =
        data_and_predictor->dictionary_predictor();

    table_->LoadFromFile("system://qwerty_mobile-hiragana.tsv");
    table_->typing_model_ = absl::make_unique<MockTypingModel>();
    InsertInputSequenceForProbableKeyEvent(key, corrected_key_codes,
                                           composer_.get());

    Segments segments;
    MakeSegmentsForPrediction(key, &segments);

    std::vector<TestableDictionaryPredictor::Result> results;
    predictor->AggregateTypeCorrectingPrediction(*convreq_for_prediction_,
                                                 segments, &results);

    std::set<std::string> values;
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_EQ(TestableDictionaryPredictor::TYPING_CORRECTION,
                results[i].types);
      values.insert(results[i].value);
    }
    for (size_t i = 0; i < expected_values_size; ++i) {
      EXPECT_TRUE(values.find(expected_values[i]) != values.end())
          << expected_values[i] << " isn't in the results";
    }
  }

  DictionaryPredictor::PredictionTypes AddRealtimeForMobile(
      DictionaryPredictor::PredictionTypes types, bool is_mobile) {
    if (!is_mobile) {
      return types;
    }
    return types | DictionaryPredictor::REALTIME;
  }

  commands::DecoderExperimentParams *mutable_decoder_experiment_params() {
    return request_->mutable_decoder_experiment_params();
  }

  std::unique_ptr<composer::Composer> composer_;
  std::unique_ptr<composer::Table> table_;
  std::unique_ptr<ConversionRequest> convreq_;
  std::unique_ptr<ConversionRequest> convreq_for_suggestion_;
  std::unique_ptr<ConversionRequest> convreq_for_prediction_;
  std::unique_ptr<config::Config> config_;
  std::unique_ptr<commands::Request> request_;

 private:
  std::unique_ptr<ImmutableConverterInterface> immutable_converter_;
  mozc::usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;
};

TEST_F(DictionaryPredictorTest, OnOffTest) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  // turn off
  Segments segments;
  config_->set_use_dictionary_suggest(false);
  config_->set_use_realtime_conversion(false);

  SetUpInputForSuggestion("ぐーぐるあ", composer_.get(), &segments);
  EXPECT_FALSE(
      predictor->PredictForRequest(*convreq_for_suggestion_, &segments));

  // turn on
  config_->set_use_dictionary_suggest(true);
  SetUpInputForSuggestion("ぐーぐるあ", composer_.get(), &segments);
  EXPECT_TRUE(
      predictor->PredictForRequest(*convreq_for_suggestion_, &segments));

  // empty query
  SetUpInputForSuggestion("", composer_.get(), &segments);
  EXPECT_FALSE(
      predictor->PredictForRequest(*convreq_for_suggestion_, &segments));
}

TEST_F(DictionaryPredictorTest, PartialSuggestion) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  {
    // Set up mock converter.
    Segments segments;
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = "Realtime top result";
    ConverterMock *converter = data_and_predictor->mutable_converter_mock();
    converter->SetStartConversionForRequest(&segments, true);
  }
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(true);
  // turn on mobile mode
  request_->set_mixed_conversion(true);

  segments.Clear();
  segments.set_request_type(Segments::PARTIAL_SUGGESTION);
  Segment *seg = segments.add_segment();
  seg->set_key("ぐーぐるあ");
  seg->set_segment_type(Segment::FREE);
  EXPECT_TRUE(
      predictor->PredictForRequest(*convreq_for_suggestion_, &segments));
}

TEST_F(DictionaryPredictorTest, BigramTest) {
  Segments segments;
  config_->set_use_dictionary_suggest(true);

  MakeSegmentsForSuggestion("あ", &segments);

  // history is "グーグル"
  PrependHistorySegments("ぐーぐる", "グーグル", &segments);

  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  // "グーグルアドセンス" will be returned.
  EXPECT_TRUE(
      predictor->PredictForRequest(*convreq_for_suggestion_, &segments));
}

TEST_F(DictionaryPredictorTest, BigramTestWithZeroQuery) {
  Segments segments;
  config_->set_use_dictionary_suggest(true);
  request_->set_zero_query_suggestion(true);

  // current query is empty
  MakeSegmentsForSuggestion("", &segments);

  // history is "グーグル"
  PrependHistorySegments("ぐーぐる", "グーグル", &segments);

  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  EXPECT_TRUE(
      predictor->PredictForRequest(*convreq_for_suggestion_, &segments));
}

// Check that previous candidate never be shown at the current candidate.
TEST_F(DictionaryPredictorTest, Regression3042706) {
  Segments segments;
  config_->set_use_dictionary_suggest(true);

  MakeSegmentsForSuggestion("だい", &segments);

  // history is "きょうと/京都"
  PrependHistorySegments("きょうと", "京都", &segments);

  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  EXPECT_TRUE(
      predictor->PredictForRequest(*convreq_for_suggestion_, &segments));
  EXPECT_EQ(2, segments.segments_size());  // history + current
  for (int i = 0; i < segments.segment(1).candidates_size(); ++i) {
    const Segment::Candidate &candidate = segments.segment(1).candidate(i);
    EXPECT_FALSE(absl::StartsWith(candidate.content_value, "京都"));
    EXPECT_TRUE(absl::StartsWith(candidate.content_key, "だい"));
  }
}

class TriggerConditionsTest : public DictionaryPredictorTest,
                              public WithParamInterface<Platform> {};

TEST_P(TriggerConditionsTest, TriggerConditions) {
  bool is_mobile = (GetParam() == MOBILE);
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  std::vector<DictionaryPredictor::Result> results;

  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(false);
  if (is_mobile) {
    commands::RequestForUnitTest::FillMobileRequest(request_.get());
  }

  // Keys of normal lengths.
  {
    // Unigram is triggered in suggestion and prediction if key length (in UTF8
    // character count) is long enough.
    SetUpInputForSuggestion("てすとだよ", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    EXPECT_EQ(AddRealtimeForMobile(DictionaryPredictor::UNIGRAM, is_mobile),
              predictor->AggregatePredictionForRequest(*convreq_for_suggestion_,
                                                       &segments, &results));

    segments.set_request_type(Segments::PREDICTION);
    EXPECT_EQ(AddRealtimeForMobile(DictionaryPredictor::UNIGRAM, is_mobile),
              predictor->AggregatePredictionForRequest(*convreq_for_prediction_,
                                                       &segments, &results));
  }

  // Short keys.
  {
    if (is_mobile) {
      // Unigram is triggered even if key length is short.
      SetUpInputForSuggestion("てす", composer_.get(), &segments);
      composer_->SetInputMode(transliteration::HIRAGANA);
      EXPECT_EQ(DictionaryPredictor::UNIGRAM | DictionaryPredictor::REALTIME,
                predictor->AggregatePredictionForRequest(
                    *convreq_for_suggestion_, &segments, &results));

      segments.set_request_type(Segments::PREDICTION);
      EXPECT_EQ(DictionaryPredictor::UNIGRAM | DictionaryPredictor::REALTIME,
                predictor->AggregatePredictionForRequest(
                    *convreq_for_prediction_, &segments, &results));
    } else {
      // Unigram is not triggered for SUGGESTION if key length is short.
      SetUpInputForSuggestion("てす", composer_.get(), &segments);
      composer_->SetInputMode(transliteration::HIRAGANA);
      EXPECT_EQ(DictionaryPredictor::NO_PREDICTION,
                predictor->AggregatePredictionForRequest(
                    *convreq_for_suggestion_, &segments, &results));

      segments.set_request_type(Segments::PREDICTION);
      EXPECT_EQ(DictionaryPredictor::UNIGRAM,
                predictor->AggregatePredictionForRequest(
                    *convreq_for_prediction_, &segments, &results));
    }
  }

  // Zipcode-like keys.
  {
    SetUpInputForSuggestion("0123", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION,
              predictor->AggregatePredictionForRequest(*convreq_for_suggestion_,
                                                       &segments, &results));
  }

  // History is short => UNIGRAM
  {
    SetUpInputForSuggestionWithHistory("てすとだよ", "A", "A", composer_.get(),
                                       &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    EXPECT_EQ(AddRealtimeForMobile(DictionaryPredictor::UNIGRAM, is_mobile),
              predictor->AggregatePredictionForRequest(*convreq_for_suggestion_,
                                                       &segments, &results));
  }

  // Both history and current segment are long => UNIGRAM or BIGRAM
  {
    SetUpInputForSuggestionWithHistory("てすとだよ", "てすとだよ", "abc",
                                       composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    EXPECT_EQ(AddRealtimeForMobile(
                  DictionaryPredictor::UNIGRAM | DictionaryPredictor::BIGRAM,
                  is_mobile),
              predictor->AggregatePredictionForRequest(*convreq_for_suggestion_,
                                                       &segments, &results));
  }

  // Current segment is short
  {
    if (is_mobile) {
      // For mobile, UNIGRAM and REALTIME are added to BIGRAM.
      SetUpInputForSuggestionWithHistory("A", "てすとだよ", "abc",
                                         composer_.get(), &segments);
      composer_->SetInputMode(transliteration::HIRAGANA);
      EXPECT_EQ(DictionaryPredictor::UNIGRAM | DictionaryPredictor::BIGRAM |
                    DictionaryPredictor::REALTIME,
                predictor->AggregatePredictionForRequest(
                    *convreq_for_suggestion_, &segments, &results));
    } else {
      // No UNIGRAM.
      SetUpInputForSuggestionWithHistory("A", "てすとだよ", "abc",
                                         composer_.get(), &segments);
      composer_->SetInputMode(transliteration::HIRAGANA);
      EXPECT_EQ(DictionaryPredictor::BIGRAM,
                predictor->AggregatePredictionForRequest(
                    *convreq_for_suggestion_, &segments, &results));
    }
  }

  // Typing correction shouldn't be appended.
  {
    SetUpInputForSuggestion("ｐはよう", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    const auto ret = predictor->AggregatePredictionForRequest(
        *convreq_for_suggestion_, &segments, &results);
    EXPECT_EQ(0, DictionaryPredictor::TYPING_CORRECTION & ret);
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
      request_->set_special_romanji_table(table);

      // Language aware input is default: No English prediction.
      request_->set_language_aware_input(
          commands::Request::DEFAULT_LANGUAGE_AWARE_BEHAVIOR);
      auto type = predictor->AggregatePredictionForRequest(
          *convreq_for_suggestion_, &segments, &results);
      EXPECT_EQ(0, DictionaryPredictor::ENGLISH & type);

      // Language aware input is off: No English prediction.
      request_->set_language_aware_input(
          commands::Request::NO_LANGUAGE_AWARE_INPUT);
      type = predictor->AggregatePredictionForRequest(*convreq_for_suggestion_,
                                                      &segments, &results);
      EXPECT_EQ(0, type & DictionaryPredictor::ENGLISH);

      // Language aware input is on: English prediction is included.
      request_->set_language_aware_input(
          commands::Request::LANGUAGE_AWARE_SUGGESTION);
      type = predictor->AggregatePredictionForRequest(*convreq_for_suggestion_,
                                                      &segments, &results);
      EXPECT_EQ(DictionaryPredictor::ENGLISH,
                type & DictionaryPredictor::ENGLISH);
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
      request_->set_special_romanji_table(table);

      // Language aware input is default.
      request_->set_language_aware_input(
          commands::Request::DEFAULT_LANGUAGE_AWARE_BEHAVIOR);
      auto type = predictor->AggregatePredictionForRequest(
          *convreq_for_suggestion_, &segments, &results);
      EXPECT_EQ(0, type & DictionaryPredictor::ENGLISH);

      // Language aware input is off.
      request_->set_language_aware_input(
          commands::Request::NO_LANGUAGE_AWARE_INPUT);
      type = predictor->AggregatePredictionForRequest(*convreq_for_suggestion_,
                                                      &segments, &results);
      EXPECT_EQ(0, type & DictionaryPredictor::ENGLISH);

      // Language aware input is on.
      request_->set_language_aware_input(
          commands::Request::LANGUAGE_AWARE_SUGGESTION);
      type = predictor->AggregatePredictionForRequest(*convreq_for_suggestion_,
                                                      &segments, &results);
      EXPECT_EQ(0, type & DictionaryPredictor::ENGLISH);
    }

    config_->set_use_dictionary_suggest(orig_use_dictionary_suggest);
    request_->set_language_aware_input(orig_lang_aware);
    request_->set_special_romanji_table(orig_table);
    composer_->SetInputMode(orig_input_mode);
  }
}

INSTANTIATE_TEST_SUITE_P(TriggerConditionsForPlatforms, TriggerConditionsTest,
                         ::testing::Values(DESKTOP, MOBILE));

TEST_F(DictionaryPredictorTest, TriggerConditions_Mobile) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  std::vector<DictionaryPredictor::Result> results;

  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(false);
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  // Keys of normal lengths.
  {
    // Unigram is triggered in suggestion and prediction if key length (in UTF8
    // character count) is long enough.
    SetUpInputForSuggestion("てすとだよ", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM | DictionaryPredictor::REALTIME,
              predictor->AggregatePredictionForRequest(*convreq_for_suggestion_,
                                                       &segments, &results));

    segments.set_request_type(Segments::PREDICTION);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM | DictionaryPredictor::REALTIME,
              predictor->AggregatePredictionForRequest(*convreq_for_prediction_,
                                                       &segments, &results));
  }

  // Short keys. In mobile, we trigger suggestion and prediction even for short
  // keys.
  {
    // Unigram is not triggered if key length is short.
    SetUpInputForSuggestion("て", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM | DictionaryPredictor::REALTIME,
              predictor->AggregatePredictionForRequest(*convreq_for_suggestion_,
                                                       &segments, &results));

    segments.set_request_type(Segments::PREDICTION);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM | DictionaryPredictor::REALTIME,
              predictor->AggregatePredictionForRequest(*convreq_for_prediction_,
                                                       &segments, &results));
  }

  // Zipcode-like keys.
  {
    SetUpInputForSuggestion("0123", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION,
              predictor->AggregatePredictionForRequest(*convreq_for_suggestion_,
                                                       &segments, &results));
  }

  // History is short => UNIGRAM
  {
    SetUpInputForSuggestionWithHistory("てすとだよ", "A", "A", composer_.get(),
                                       &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM | DictionaryPredictor::REALTIME,
              predictor->AggregatePredictionForRequest(*convreq_for_suggestion_,
                                                       &segments, &results));
  }

  // Both history and current segment are long => UNIGRAM or BIGRAM
  {
    SetUpInputForSuggestionWithHistory("てすとだよ", "てすとだよ", "abc",
                                       composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM | DictionaryPredictor::BIGRAM |
                  DictionaryPredictor::REALTIME,
              predictor->AggregatePredictionForRequest(*convreq_for_suggestion_,
                                                       &segments, &results));
  }

  // No matter if the current segment is short.
  {
    SetUpInputForSuggestionWithHistory("て", "てすとだよ", "abc",
                                       composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM | DictionaryPredictor::BIGRAM |
                  DictionaryPredictor::REALTIME,
              predictor->AggregatePredictionForRequest(*convreq_for_suggestion_,
                                                       &segments, &results));
  }

  // Typing correction shouldn't be appended.
  {
    SetUpInputForSuggestion("ｐはよう", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    const auto ret = predictor->AggregatePredictionForRequest(
        *convreq_for_suggestion_, &segments, &results);
    EXPECT_EQ(0, DictionaryPredictor::TYPING_CORRECTION & ret);
  }

  // When romaji table is qwerty mobile => ENGLISH is included depending on the
  // language aware input setting.
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
      request_->set_special_romanji_table(table);

      // Language aware input is default: No English prediction.
      request_->set_language_aware_input(
          commands::Request::DEFAULT_LANGUAGE_AWARE_BEHAVIOR);
      auto type = predictor->AggregatePredictionForRequest(
          *convreq_for_suggestion_, &segments, &results);
      EXPECT_EQ(0, DictionaryPredictor::ENGLISH & type);

      // Language aware input is off: No English prediction.
      request_->set_language_aware_input(
          commands::Request::NO_LANGUAGE_AWARE_INPUT);
      type = predictor->AggregatePredictionForRequest(*convreq_for_suggestion_,
                                                      &segments, &results);
      EXPECT_EQ(0, type & DictionaryPredictor::ENGLISH);

      // Language aware input is on: English prediction is included.
      request_->set_language_aware_input(
          commands::Request::LANGUAGE_AWARE_SUGGESTION);
      type = predictor->AggregatePredictionForRequest(*convreq_for_suggestion_,
                                                      &segments, &results);
      EXPECT_EQ(DictionaryPredictor::ENGLISH,
                type & DictionaryPredictor::ENGLISH);
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
      request_->set_special_romanji_table(table);

      // Language aware input is default.
      request_->set_language_aware_input(
          commands::Request::DEFAULT_LANGUAGE_AWARE_BEHAVIOR);
      auto type = predictor->AggregatePredictionForRequest(
          *convreq_for_suggestion_, &segments, &results);
      EXPECT_EQ(0, type & DictionaryPredictor::ENGLISH);

      // Language aware input is off.
      request_->set_language_aware_input(
          commands::Request::NO_LANGUAGE_AWARE_INPUT);
      type = predictor->AggregatePredictionForRequest(*convreq_for_suggestion_,
                                                      &segments, &results);
      EXPECT_EQ(0, type & DictionaryPredictor::ENGLISH);

      // Language aware input is on.
      request_->set_language_aware_input(
          commands::Request::LANGUAGE_AWARE_SUGGESTION);
      type = predictor->AggregatePredictionForRequest(*convreq_for_suggestion_,
                                                      &segments, &results);
      EXPECT_EQ(0, type & DictionaryPredictor::ENGLISH);
    }

    config_->set_use_dictionary_suggest(orig_use_dictionary_suggest);
    request_->set_language_aware_input(orig_lang_aware);
    request_->set_special_romanji_table(orig_table);
    composer_->SetInputMode(orig_input_mode);
  }
}

TEST_F(DictionaryPredictorTest, TriggerConditions_LatinInputMode) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  enum Platform { DESKTOP, MOBILE };

  struct {
    Platform platform;
    transliteration::TransliterationType input_mode;
  } kTestCases[] = {
      {DESKTOP, transliteration::HALF_ASCII},
      {DESKTOP, transliteration::FULL_ASCII},
      {MOBILE, transliteration::HALF_ASCII},
      {MOBILE, transliteration::FULL_ASCII},
  };

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    config::ConfigHandler::GetDefaultConfig(config_.get());
    // Resets to default value.
    // Implementation note: Since the value of |request_| is used to initialize
    // composer_ and convreq_, it is not safe to reset |request_| with new
    // instance.
    request_->Clear();
    const bool is_mobile = kTestCases[i].platform == MOBILE;
    if (is_mobile) {
      commands::RequestForUnitTest::FillMobileRequest(request_.get());
    }

    Segments segments;
    std::vector<DictionaryPredictor::Result> results;

    // Implementation note: SetUpInputForSuggestion() resets the state of
    // composer. So we have to call SetInputMode() after this method.
    SetUpInputForSuggestion("hel", composer_.get(), &segments);
    composer_->SetInputMode(kTestCases[i].input_mode);

    config_->set_use_dictionary_suggest(true);

    // Input mode is Latin(HALF_ASCII or FULL_ASCII) => ENGLISH
    config_->set_use_realtime_conversion(false);
    EXPECT_EQ(AddRealtimeForMobile(DictionaryPredictor::ENGLISH, is_mobile),
              predictor->AggregatePredictionForRequest(*convreq_for_suggestion_,
                                                       &segments, &results));

    config_->set_use_realtime_conversion(true);
    EXPECT_EQ(DictionaryPredictor::ENGLISH | DictionaryPredictor::REALTIME,
              predictor->AggregatePredictionForRequest(*convreq_for_suggestion_,
                                                       &segments, &results));

    // When dictionary suggest is turned off, English prediction should be
    // disabled.
    config_->set_use_dictionary_suggest(false);
    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION,
              predictor->AggregatePredictionForRequest(*convreq_for_suggestion_,
                                                       &segments, &results));

    // Has realtime results for PARTIAL_SUGGESTION request.
    config_->set_use_dictionary_suggest(true);
    segments.set_request_type(Segments::PARTIAL_SUGGESTION);
    EXPECT_EQ(DictionaryPredictor::REALTIME,
              predictor->AggregatePredictionForRequest(*convreq_for_suggestion_,
                                                       &segments, &results));
  }
}

TEST_F(DictionaryPredictorTest, AggregateUnigramCandidate) {
  Segments segments;
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  constexpr char kKey[] = "ぐーぐるあ";

  SetUpInputForSuggestion(kKey, composer_.get(), &segments);

  std::vector<DictionaryPredictor::Result> results;

  predictor->AggregateUnigramCandidate(*convreq_for_suggestion_, segments,
                                       &results);
  EXPECT_FALSE(results.empty());

  for (const auto &result : results) {
    EXPECT_EQ(DictionaryPredictor::UNIGRAM, result.types);
    EXPECT_TRUE(absl::StartsWith(result.key, kKey));
  }

  EXPECT_EQ(1, segments.conversion_segments_size());
}

TEST_F(DictionaryPredictorTest, AggregateUnigramCandidateForMixedConversion) {
  constexpr char kHiraganaA[] = "あ";
  constexpr char kHiraganaAA[] = "ああ";

  DictionaryMock mock_dict;
  // A system dictionary entry "a".
  mock_dict.AddLookupPredictive(kHiraganaA, kHiraganaA, "a", Token::NONE);
  // System dictionary entries "a0", ..., "a9", which are detected as redundant
  // by MaybeRedundant(); see dictionary_predictor.cc.
  for (int i = 0; i < 10; ++i) {
    mock_dict.AddLookupPredictive(kHiraganaA, kHiraganaA,
                                  absl::StrFormat("a%d", i), Token::NONE);
  }
  // A user dictionary entry "aaa".  MaybeRedundant() detects this entry as
  // redundant but it should not be filtered in prediction.
  mock_dict.AddLookupPredictive(kHiraganaA, kHiraganaA, "aaa",
                                Token::USER_DICTIONARY);

  constexpr int kUnknownId = 100;
  mock_dict.AddLookupPredictive(kHiraganaA, kHiraganaAA, "bbb", 0, kUnknownId,
                                kUnknownId, Token::USER_DICTIONARY);
  mock_dict.AddLookupPredictive(kHiraganaAA, kHiraganaAA, "bbb", 0, kUnknownId,
                                kUnknownId, Token::USER_DICTIONARY);

  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(false);
  table_->LoadFromFile("system://12keys-hiragana.tsv");
  composer_->SetTable(table_.get());

  {
    // Test prediction from input あ.
    InsertInputSequence(kHiraganaA, composer_.get());
    Segments segments;
    segments.set_request_type(Segments::PREDICTION);
    Segment *segment = segments.add_segment();
    segment->set_key(kHiraganaA);

    std::vector<DictionaryPredictor::Result> results;
    DictionaryPredictor::AggregateUnigramCandidateForMixedConversion(
        mock_dict, *convreq_for_prediction_, segments, kUnknownId, &results);

    // Check if "aaa" is not filtered.
    auto iter =
        std::find_if(results.begin(), results.end(),
                     [&kHiraganaA](const DictionaryPredictor::Result &res) {
                       return res.key == kHiraganaA && res.value == "aaa" &&
                              res.IsUserDictionaryResult();
                     });
    EXPECT_NE(results.end(), iter);

    // "bbb" is looked up from input "あ" but it will be filtered because it is
    // from user dictionary with unknown POS ID.
    iter = std::find_if(results.begin(), results.end(),
                        [&kHiraganaAA](const DictionaryPredictor::Result &res) {
                          return res.key == kHiraganaAA && res.value == "bbb" &&
                                 res.IsUserDictionaryResult();
                        });
    EXPECT_EQ(results.end(), iter);
  }

  {
    // Test prediction from input ああ.
    composer_->Reset();
    InsertInputSequence(kHiraganaAA, composer_.get());
    Segments segments;
    segments.set_request_type(Segments::PREDICTION);
    Segment *segment = segments.add_segment();
    segment->set_key(kHiraganaAA);

    std::vector<DictionaryPredictor::Result> results;
    DictionaryPredictor::AggregateUnigramCandidateForMixedConversion(
        mock_dict, *convreq_for_prediction_, segments, kUnknownId, &results);

    // Check if "aaa" is not found as its key is あ.
    auto iter =
        std::find_if(results.begin(), results.end(),
                     [&kHiraganaA](const DictionaryPredictor::Result &res) {
                       return res.key == kHiraganaA && res.value == "aaa" &&
                              res.IsUserDictionaryResult();
                     });
    EXPECT_EQ(results.end(), iter);

    // Unlike the above case for "あ", "bbb" is now found because input key is
    // exactly "ああ".
    iter = std::find_if(results.begin(), results.end(),
                        [&kHiraganaAA](const DictionaryPredictor::Result &res) {
                          return res.key == kHiraganaAA && res.value == "bbb" &&
                                 res.IsUserDictionaryResult();
                        });
    EXPECT_NE(results.end(), iter);
  }
}

TEST_F(DictionaryPredictorTest, AggregateBigramPrediction) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  {
    Segments segments;

    MakeSegmentsForSuggestion("あ", &segments);

    // history is "グーグル"
    constexpr char kHistoryKey[] = "ぐーぐる";
    constexpr char kHistoryValue[] = "グーグル";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<DictionaryPredictor::Result> results;

    predictor->AggregateBigramPrediction(*convreq_for_suggestion_, segments,
                                         Segment::Candidate::SOURCE_INFO_NONE,
                                         &results);
    EXPECT_FALSE(results.empty());

    for (size_t i = 0; i < results.size(); ++i) {
      // "グーグルアドセンス", "グーグル", "アドセンス"
      // are in the dictionary.
      if (results[i].value == "グーグルアドセンス") {
        EXPECT_FALSE(results[i].removed);
      } else {
        EXPECT_TRUE(results[i].removed);
      }
      EXPECT_EQ(DictionaryPredictor::BIGRAM, results[i].types);
      EXPECT_TRUE(absl::StartsWith(results[i].key, kHistoryKey));
      EXPECT_TRUE(absl::StartsWith(results[i].value, kHistoryValue));
      // Not zero query
      EXPECT_FALSE(results[i].source_info &
                   Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX);
    }

    EXPECT_EQ(1, segments.conversion_segments_size());
  }

  {
    Segments segments;

    MakeSegmentsForSuggestion("あ", &segments);

    constexpr char kHistoryKey[] = "てす";
    constexpr char kHistoryValue[] = "テス";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<DictionaryPredictor::Result> results;

    predictor->AggregateBigramPrediction(*convreq_for_suggestion_, segments,
                                         Segment::Candidate::SOURCE_INFO_NONE,
                                         &results);
    EXPECT_TRUE(results.empty());
  }
}

TEST_F(DictionaryPredictorTest, AggregateZeroQueryBigramPrediction) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  {
    Segments segments;

    // Zero query
    MakeSegmentsForSuggestion("", &segments);

    // history is "グーグル"
    constexpr char kHistoryKey[] = "ぐーぐる";
    constexpr char kHistoryValue[] = "グーグル";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<DictionaryPredictor::Result> results;

    predictor->AggregateBigramPrediction(
        *convreq_for_suggestion_, segments,
        Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_BIGRAM, &results);
    EXPECT_FALSE(results.empty());

    for (const auto &result : results) {
      EXPECT_TRUE(absl::StartsWith(result.key, kHistoryKey));
      EXPECT_TRUE(absl::StartsWith(result.value, kHistoryValue));
      // Zero query
      EXPECT_FALSE(result.source_info &
                   Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX);
    }
  }

  {
    constexpr char kHistory[] = "ありがとう";

    DictionaryMock *mock = data_and_predictor->mutable_dictionary();
    mock->AddLookupPrefix(kHistory, kHistory, kHistory, Token::NONE);

    auto add_word = [&mock](const std::string &key, const std::string &value) {
      mock->AddLookupPredictive("ありがとう", key, value, Token::NONE);
    };
    add_word("ありがとうございます", "ありがとうございます");
    add_word("ありがとうございます", "ありがとう御座います");
    add_word("ありがとうございました", "ありがとうございました");
    add_word("ありがとうございました", "ありがとう御座いました");

    add_word("ございます", "ございます");
    add_word("ございます", "御座います");
    // ("ございました", "ございました") is not in the dictionary.
    add_word("ございました", "御座いました");

    // Word less than 10.
    add_word("ありがとうね", "ありがとうね");
    add_word("ね", "ね");

    Segments segments;

    // Zero query
    MakeSegmentsForSuggestion("", &segments);

    PrependHistorySegments(kHistory, kHistory, &segments);

    std::vector<DictionaryPredictor::Result> results;

    predictor->AggregateBigramPrediction(
        *convreq_for_suggestion_, segments,
        Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_BIGRAM, &results);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results.size(), 5);

    EXPECT_TRUE(FindResultByValue(results, "ありがとうございます"));
    EXPECT_TRUE(FindResultByValue(results, "ありがとう御座います"));
    EXPECT_TRUE(FindResultByValue(results, "ありがとう御座いました"));
    // "ございました" is not in the dictionary, but suggested
    // because it is used as the key of other words (i.e. 御座いました).
    EXPECT_TRUE(FindResultByValue(results, "ありがとうございました"));
    // "ね" is in the dictionary, but filtered due to the word length.
    EXPECT_FALSE(FindResultByValue(results, "ありがとうね"));

    for (const auto &result : results) {
      EXPECT_TRUE(absl::StartsWith(result.key, kHistory));
      EXPECT_TRUE(absl::StartsWith(result.value, kHistory));
      // Zero query
      EXPECT_FALSE(result.source_info &
                   Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX);
      if (result.key == "ありがとうね") {
        EXPECT_TRUE(result.removed);
      } else {
        EXPECT_FALSE(result.removed);
      }
    }
  }
}

TEST_F(DictionaryPredictorTest, AggregateZeroQueryPrediction_LatinInputMode) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  {
    Segments segments;

    // Zero query
    SetUpInputForSuggestion("", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HALF_ASCII);

    // No history
    constexpr char kHistoryKey[] = "";
    constexpr char kHistoryValue[] = "";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<DictionaryPredictor::Result> results;

    predictor->AggregateZeroQuerySuffixPrediction(*convreq_for_suggestion_,
                                                  segments, &results);
    EXPECT_TRUE(results.empty());
  }

  {
    Segments segments;

    // Zero query
    SetUpInputForSuggestion("", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HALF_ASCII);

    constexpr char kHistoryKey[] = "when";
    constexpr char kHistoryValue[] = "when";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<DictionaryPredictor::Result> results;

    predictor->AggregateZeroQuerySuffixPrediction(*convreq_for_suggestion_,
                                                  segments, &results);
    EXPECT_TRUE(results.empty());
  }

  {
    Segments segments;

    // Zero query
    SetUpInputForSuggestion("", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HALF_ASCII);

    // We can input numbers from Latin input mode.
    constexpr char kHistoryKey[] = "12";
    constexpr char kHistoryValue[] = "12";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<DictionaryPredictor::Result> results;

    predictor->AggregateZeroQuerySuffixPrediction(*convreq_for_suggestion_,
                                                  segments, &results);
    EXPECT_FALSE(results.empty());  // Should have results.
  }

  {
    Segments segments;

    // Zero query
    SetUpInputForSuggestion("", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HALF_ASCII);

    // We can input some symbols from Latin input mode.
    constexpr char kHistoryKey[] = "@";
    constexpr char kHistoryValue[] = "@";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<DictionaryPredictor::Result> results;

    predictor->AggregateZeroQuerySuffixPrediction(*convreq_for_suggestion_,
                                                  segments, &results);
    EXPECT_FALSE(results.empty());  // Should have results.
  }
}

TEST_F(DictionaryPredictorTest, GetRealtimeCandidateMaxSize) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
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
  convreq_->set_max_dictionary_prediction_candidates_size(kMaxSize);

  // non-partial, non-mixed-conversion
  segments.set_request_type(Segments::PREDICTION);
  const size_t prediction_no_mixed =
      predictor->GetRealtimeCandidateMaxSize(*convreq_, segments, false);
  EXPECT_GE(kMaxSize, prediction_no_mixed);

  segments.set_request_type(Segments::SUGGESTION);
  const size_t suggestion_no_mixed =
      predictor->GetRealtimeCandidateMaxSize(*convreq_, segments, false);
  EXPECT_GE(kMaxSize, suggestion_no_mixed);
  EXPECT_LE(suggestion_no_mixed, prediction_no_mixed);

  // non-partial, mixed-conversion
  segments.set_request_type(Segments::PREDICTION);
  const size_t prediction_mixed =
      predictor->GetRealtimeCandidateMaxSize(*convreq_, segments, true);
  EXPECT_GE(kMaxSize, prediction_mixed);

  segments.set_request_type(Segments::SUGGESTION);
  const size_t suggestion_mixed =
      predictor->GetRealtimeCandidateMaxSize(*convreq_, segments, true);
  EXPECT_GE(kMaxSize, suggestion_mixed);

  // partial, non-mixed-conversion
  segments.set_request_type(Segments::PARTIAL_PREDICTION);
  const size_t partial_prediction_no_mixed =
      predictor->GetRealtimeCandidateMaxSize(*convreq_, segments, false);
  EXPECT_GE(kMaxSize, partial_prediction_no_mixed);

  segments.set_request_type(Segments::PARTIAL_SUGGESTION);
  const size_t partial_suggestion_no_mixed =
      predictor->GetRealtimeCandidateMaxSize(*convreq_, segments, false);
  EXPECT_GE(kMaxSize, partial_suggestion_no_mixed);
  EXPECT_LE(partial_suggestion_no_mixed, partial_prediction_no_mixed);

  // partial, mixed-conversion
  segments.set_request_type(Segments::PARTIAL_PREDICTION);
  const size_t partial_prediction_mixed =
      predictor->GetRealtimeCandidateMaxSize(*convreq_, segments, true);
  EXPECT_GE(kMaxSize, partial_prediction_mixed);

  segments.set_request_type(Segments::PARTIAL_SUGGESTION);
  const size_t partial_suggestion_mixed =
      predictor->GetRealtimeCandidateMaxSize(*convreq_, segments, true);
  EXPECT_GE(kMaxSize, partial_suggestion_mixed);
  EXPECT_LE(partial_suggestion_mixed, partial_prediction_mixed);

  EXPECT_GE(partial_prediction_no_mixed, prediction_no_mixed);
  EXPECT_GE(partial_prediction_mixed, prediction_mixed);
  EXPECT_GE(partial_suggestion_no_mixed, suggestion_no_mixed);
  EXPECT_GE(partial_suggestion_mixed, suggestion_mixed);
}

TEST_F(DictionaryPredictorTest, GetRealtimeCandidateMaxSizeForMixed) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  Segments segments;
  Segment *segment = segments.add_segment();

  constexpr size_t kMaxSize = 100;
  convreq_->set_max_dictionary_prediction_candidates_size(kMaxSize);

  // for short key, try to provide many results as possible
  segment->set_key("short");
  segments.set_request_type(Segments::SUGGESTION);
  const size_t short_suggestion_mixed =
      predictor->GetRealtimeCandidateMaxSize(*convreq_, segments, true);
  EXPECT_GE(kMaxSize, short_suggestion_mixed);

  segments.set_request_type(Segments::PREDICTION);
  const size_t short_prediction_mixed =
      predictor->GetRealtimeCandidateMaxSize(*convreq_, segments, true);
  EXPECT_GE(kMaxSize, short_prediction_mixed);

  // for long key, provide few results
  segment->set_key("long_request_key");
  segments.set_request_type(Segments::SUGGESTION);
  const size_t long_suggestion_mixed =
      predictor->GetRealtimeCandidateMaxSize(*convreq_, segments, true);
  EXPECT_GE(kMaxSize, long_suggestion_mixed);
  EXPECT_GT(short_suggestion_mixed, long_suggestion_mixed);

  segments.set_request_type(Segments::PREDICTION);
  const size_t long_prediction_mixed =
      predictor->GetRealtimeCandidateMaxSize(*convreq_, segments, true);
  EXPECT_GE(kMaxSize, long_prediction_mixed);
  EXPECT_GT(kMaxSize, long_prediction_mixed + long_suggestion_mixed);
  EXPECT_GT(short_prediction_mixed, long_prediction_mixed);
}

TEST_F(DictionaryPredictorTest, AggregateRealtimeConversion) {
  testing::MockDataManager data_manager;
  std::unique_ptr<const DictionaryInterface> dictionary(new DictionaryMock);
  std::unique_ptr<ConverterMock> converter(new ConverterMock);
  std::unique_ptr<ImmutableConverterInterface> immutable_converter(
      new ImmutableConverterMock);
  std::unique_ptr<const DictionaryInterface> suffix_dictionary(
      CreateSuffixDictionaryFromDataManager(data_manager));
  std::unique_ptr<const Connector> connector =
      Connector::CreateFromDataManager(data_manager).value();
  std::unique_ptr<const Segmenter> segmenter(
      Segmenter::CreateFromDataManager(data_manager));
  std::unique_ptr<const SuggestionFilter> suggestion_filter(
      CreateSuggestionFilter(data_manager));
  const dictionary::PosMatcher pos_matcher(data_manager.GetPosMatcherData());
  std::unique_ptr<TestableDictionaryPredictor> predictor(
      new TestableDictionaryPredictor(
          data_manager, converter.get(), immutable_converter.get(),
          dictionary.get(), suffix_dictionary.get(), connector.get(),
          segmenter.get(), &pos_matcher, suggestion_filter.get()));

  constexpr char kKey[] = "わたしのなまえはなかのです";

  // Set up mock converter
  {
    // Make segments like:
    // "わたしの"    | "なまえは" | "なかのです"
    // "Watashino" | "Namaeha" | "Nakanodesu"
    Segments segments;

    Segment *segment = segments.add_segment();
    segment->set_key("わたしの");
    segment->add_candidate()->value = "Watashino";

    segment = segments.add_segment();
    segment->set_key("なまえは");
    segment->add_candidate()->value = "Namaeha";

    segment = segments.add_segment();
    segment->set_key("なかのです");
    segment->add_candidate()->value = "Nakanodesu";

    converter->SetStartConversionForRequest(&segments, true);
  }

  // A test case with use_actual_converter_for_realtime_conversion being false,
  // i.e., realtime conversion result is generated by ImmutableConverterMock.
  {
    Segments segments;

    MakeSegmentsForSuggestion(kKey, &segments);

    std::vector<TestableDictionaryPredictor::Result> results;
    convreq_->set_use_actual_converter_for_realtime_conversion(false);

    predictor->AggregateRealtimeConversion(*convreq_for_suggestion_, 10,
                                           &segments, &results);
    ASSERT_EQ(1, results.size());
    EXPECT_EQ(TestableDictionaryPredictor::REALTIME, results[0].types);
    EXPECT_EQ(kKey, results[0].key);
    EXPECT_EQ(3, results[0].inner_segment_boundary.size());
  }

  // A test case with use_actual_converter_for_realtime_conversion being true,
  // i.e., realtime conversion result is generated by ConverterMock.
  {
    Segments segments;

    MakeSegmentsForSuggestion(kKey, &segments);

    std::vector<TestableDictionaryPredictor::Result> results;
    convreq_for_suggestion_->set_use_actual_converter_for_realtime_conversion(
        true);

    predictor->AggregateRealtimeConversion(*convreq_for_suggestion_, 10,
                                           &segments, &results);

    // When |request.use_actual_converter_for_realtime_conversion| is true, the
    // extra label REALTIME_TOP is expected to be added.
    ASSERT_EQ(2, results.size());
    bool realtime_top_found = false;
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_EQ(TestableDictionaryPredictor::REALTIME |
                    TestableDictionaryPredictor::REALTIME_TOP,
                results[i].types);
      if (results[i].key == kKey &&
          results[i].value == "WatashinoNamaehaNakanodesu" &&
          results[i].inner_segment_boundary.size() == 3) {
        realtime_top_found = true;
        break;
      }
    }
    EXPECT_TRUE(realtime_top_found);
  }
}

namespace {
struct SimpleSuffixToken {
  const char *key;
  const char *value;
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
      if (!key.empty() && !absl::StartsWith(suffix_token.key, key)) {
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

TEST_F(DictionaryPredictorTest, GetCandidateCutoffThreshold) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  Segments segments;

  segments.set_request_type(Segments::PREDICTION);
  const size_t prediction = predictor->GetCandidateCutoffThreshold(segments);

  segments.set_request_type(Segments::SUGGESTION);
  const size_t suggestion = predictor->GetCandidateCutoffThreshold(segments);
  EXPECT_LE(suggestion, prediction);
}

TEST_F(DictionaryPredictorTest, AggregateSuffixPrediction) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      new MockDataAndPredictor);
  data_and_predictor->Init(nullptr, new TestSuffixDictionary());

  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;

  // history is "グーグル"
  constexpr char kHistoryKey[] = "ぐーぐる";
  constexpr char kHistoryValue[] = "グーグル";

  // Since SuffixDictionary only returns for key "い", the result
  // should be empty for "あ".
  std::vector<DictionaryPredictor::Result> results;
  SetUpInputForSuggestionWithHistory("あ", kHistoryKey, kHistoryValue,
                                     composer_.get(), &segments);
  predictor->AggregateSuffixPrediction(*convreq_for_suggestion_, segments,
                                       &results);
  EXPECT_TRUE(results.empty());

  // Candidates generated by AggregateSuffixPrediction from nonempty
  // key should have SUFFIX type.
  results.clear();
  SetUpInputForSuggestionWithHistory("い", kHistoryKey, kHistoryValue,
                                     composer_.get(), &segments);
  predictor->AggregateSuffixPrediction(*convreq_for_suggestion_, segments,
                                       &results);
  EXPECT_FALSE(results.empty());
  for (const auto &result : results) {
    EXPECT_EQ(DictionaryPredictor::SUFFIX, result.types);
    // Not zero query
    EXPECT_FALSE(Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX &
                 result.source_info);
  }
}

TEST_F(DictionaryPredictorTest, AggregateZeroQuerySuffixPrediction) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      new MockDataAndPredictor);
  data_and_predictor->Init(nullptr, new TestSuffixDictionary());

  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  commands::RequestForUnitTest::FillMobileRequest(request_.get());
  Segments segments;

  // Zero query
  MakeSegmentsForSuggestion("", &segments);

  // history is "グーグル"
  constexpr char kHistoryKey[] = "ぐーぐる";
  constexpr char kHistoryValue[] = "グーグル";

  PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

  std::vector<DictionaryPredictor::Result> results;

  // Candidates generated by AggregateZeroQuerySuffixPrediction should
  // have SUFFIX type.
  predictor->AggregateZeroQuerySuffixPrediction(*convreq_for_suggestion_,
                                                segments, &results);
  EXPECT_FALSE(results.empty());
  for (size_t i = 0; i < results.size(); ++i) {
    EXPECT_EQ(DictionaryPredictor::SUFFIX, results[i].types);
    // Zero query
    EXPECT_TRUE(Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX &
                results[i].source_info);
  }
}

TEST_F(DictionaryPredictorTest, AggregateEnglishPrediction) {
  // Input mode: HALF_ASCII, Key: lower case
  //   => Prediction should be in half-width lower case.
  {
    const char *kExpectedValues[] = {
        "converge",
        "converged",
        "convergent",
    };
    AggregateEnglishPredictionTestHelper(transliteration::HALF_ASCII, "conv",
                                         "conv", kExpectedValues,
                                         std::size(kExpectedValues));
  }
  // Input mode: HALF_ASCII, Key: upper case
  //   => Prediction should be in half-width upper case.
  {
    const char *kExpectedValues[] = {
        "CONVERGE",
        "CONVERGED",
        "CONVERGENT",
    };
    AggregateEnglishPredictionTestHelper(transliteration::HALF_ASCII, "CONV",
                                         "CONV", kExpectedValues,
                                         std::size(kExpectedValues));
  }
  // Input mode: HALF_ASCII, Key: capitalized
  //   => Prediction should be half-width and capitalized
  {
    const char *kExpectedValues[] = {
        "Converge",
        "Converged",
        "Convergent",
    };
    AggregateEnglishPredictionTestHelper(transliteration::HALF_ASCII, "Conv",
                                         "Conv", kExpectedValues,
                                         std::size(kExpectedValues));
  }
  // Input mode: FULL_ASCII, Key: lower case
  //   => Prediction should be in full-width lower case.
  {
    const char *kExpectedValues[] = {
        "ｃｏｎｖｅｒｇｅ",
        "ｃｏｎｖｅｒｇｅｄ",
        "ｃｏｎｖｅｒｇｅｎｔ",
    };
    AggregateEnglishPredictionTestHelper(transliteration::FULL_ASCII, "conv",
                                         "ｃｏｎｖ", kExpectedValues,
                                         std::size(kExpectedValues));
  }
  // Input mode: FULL_ASCII, Key: upper case
  //   => Prediction should be in full-width upper case.
  {
    const char *kExpectedValues[] = {
        "ＣＯＮＶＥＲＧＥ",
        "ＣＯＮＶＥＲＧＥＤ",
        "ＣＯＮＶＥＲＧＥＮＴ",
    };
    AggregateEnglishPredictionTestHelper(transliteration::FULL_ASCII, "CONV",
                                         "ＣＯＮＶ", kExpectedValues,
                                         std::size(kExpectedValues));
  }
  // Input mode: FULL_ASCII, Key: capitalized
  //   => Prediction should be full-width and capitalized
  {
    const char *kExpectedValues[] = {
        "Ｃｏｎｖｅｒｇｅ",
        "Ｃｏｎｖｅｒｇｅｄ",
        "Ｃｏｎｖｅｒｇｅｎｔ",
    };
    AggregateEnglishPredictionTestHelper(transliteration::FULL_ASCII, "Conv",
                                         "Ｃｏｎｖ", kExpectedValues,
                                         std::size(kExpectedValues));
  }
}

TEST_F(DictionaryPredictorTest, AggregateTypeCorrectingPrediction) {
  config_->set_use_typing_correction(true);

  constexpr char kInputText[] = "gu-huru";
  constexpr uint32_t kCorrectedKeyCodes[] = {'g', 'u', '-', 'g', 'u', 'r', 'u'};
  const char *kExpectedValues[] = {
      "グーグルアドセンス",
      "グーグルアドワーズ",
  };
  AggregateTypeCorrectingTestHelper(kInputText, kCorrectedKeyCodes,
                                    kExpectedValues,
                                    std::size(kExpectedValues));
}

TEST_F(DictionaryPredictorTest, ZeroQuerySuggestionAfterNumbers) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  const PosMatcher &pos_matcher = data_and_predictor->pos_matcher();
  Segments segments;

  {
    MakeSegmentsForSuggestion("", &segments);

    constexpr char kHistoryKey[] = "12";
    constexpr char kHistoryValue[] = "12";
    constexpr char kExpectedValue[] = "月";
    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);
    std::vector<DictionaryPredictor::Result> results;
    predictor->AggregateZeroQuerySuffixPrediction(*convreq_for_suggestion_,
                                                  segments, &results);
    EXPECT_FALSE(results.empty());

    auto target = results.end();
    for (auto it = results.begin(); it != results.end(); ++it) {
      EXPECT_EQ(it->types, DictionaryPredictor::SUFFIX);

      EXPECT_TRUE(
          Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_NUMBER_SUFFIX &
          it->source_info);

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
    MakeSegmentsForSuggestion("", &segments);

    constexpr char kHistoryKey[] = "66050713";  // A random number
    constexpr char kHistoryValue[] = "66050713";
    constexpr char kExpectedValue[] = "個";
    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);
    std::vector<DictionaryPredictor::Result> results;
    predictor->AggregateZeroQuerySuffixPrediction(*convreq_for_suggestion_,
                                                  segments, &results);
    EXPECT_FALSE(results.empty());

    bool found = false;
    for (auto it = results.begin(); it != results.end(); ++it) {
      EXPECT_EQ(it->types, DictionaryPredictor::SUFFIX);
      if (it->value == kExpectedValue) {
        EXPECT_TRUE(
            Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_NUMBER_SUFFIX &
            it->source_info);
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
  }
}

TEST_F(DictionaryPredictorTest, TriggerNumberZeroQuerySuggestion) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  const PosMatcher &pos_matcher = data_and_predictor->pos_matcher();

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

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    Segments segments;
    MakeSegmentsForSuggestion("", &segments);

    const TestCase &test_case = kTestCases[i];
    PrependHistorySegments(test_case.history_key, test_case.history_value,
                           &segments);
    std::vector<DictionaryPredictor::Result> results;
    predictor->AggregateZeroQuerySuffixPrediction(*convreq_for_suggestion_,
                                                  segments, &results);
    EXPECT_FALSE(results.empty());

    bool found = false;
    for (auto it = results.begin(); it != results.end(); ++it) {
      EXPECT_EQ(it->types, DictionaryPredictor::SUFFIX);
      if (it->value == test_case.find_suffix_value &&
          it->lid == pos_matcher.GetCounterSuffixWordId()) {
        EXPECT_TRUE(
            Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_NUMBER_SUFFIX &
            it->source_info);
        found = true;
        break;
      }
    }
    EXPECT_EQ(test_case.expected_result, found) << test_case.history_value;
  }
}

TEST_F(DictionaryPredictorTest, TriggerZeroQuerySuggestion) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

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

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    Segments segments;
    MakeSegmentsForSuggestion("", &segments);

    const TestCase &test_case = kTestCases[i];
    PrependHistorySegments(test_case.history_key, test_case.history_value,
                           &segments);
    std::vector<DictionaryPredictor::Result> results;
    predictor->AggregateZeroQuerySuffixPrediction(*convreq_for_suggestion_,
                                                  segments, &results);
    EXPECT_FALSE(results.empty());

    int rank = -1;
    for (size_t i = 0; i < results.size(); ++i) {
      const auto &result = results[i];
      EXPECT_EQ(result.types, DictionaryPredictor::SUFFIX);
      if (result.value == test_case.find_value && result.lid == 0 /* EOS */) {
        rank = static_cast<int>(i);
        break;
      }
    }
    EXPECT_EQ(test_case.expected_rank, rank) << test_case.history_value;
  }
}

TEST_F(DictionaryPredictorTest, GetHistoryKeyAndValue) {
  Segments segments;
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  MakeSegmentsForSuggestion("test", &segments);

  std::string key, value;
  EXPECT_FALSE(predictor->GetHistoryKeyAndValue(segments, &key, &value));

  PrependHistorySegments("key", "value", &segments);
  EXPECT_TRUE(predictor->GetHistoryKeyAndValue(segments, &key, &value));
  EXPECT_EQ("key", key);
  EXPECT_EQ("value", value);
}

TEST_F(DictionaryPredictorTest, IsZipCodeRequest) {
  EXPECT_FALSE(DictionaryPredictor::IsZipCodeRequest(""));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("000"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("000"));
  EXPECT_FALSE(DictionaryPredictor::IsZipCodeRequest("ABC"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("---"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("0124-"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("0124-0"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("012-0"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("012-3456"));
  EXPECT_FALSE(DictionaryPredictor::IsZipCodeRequest("０１２-０"));
}

TEST_F(DictionaryPredictorTest, IsAggressiveSuggestion) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  // "ただしい",
  // "ただしいけめんにかぎる",
  EXPECT_TRUE(predictor->IsAggressiveSuggestion(4,     // query_len
                                                11,    // key_len
                                                6000,  // cost
                                                true,  // is_suggestion
                                                20));  // total_candidates_size

  // cost <= 4000
  EXPECT_FALSE(predictor->IsAggressiveSuggestion(4, 11, 4000, true, 20));

  // not suggestion
  EXPECT_FALSE(predictor->IsAggressiveSuggestion(4, 11, 4000, false, 20));

  // total_candidates_size is small
  EXPECT_FALSE(predictor->IsAggressiveSuggestion(4, 11, 4000, true, 5));

  // query_length = 5
  EXPECT_FALSE(predictor->IsAggressiveSuggestion(5, 11, 6000, true, 20));

  // "それでも",
  // "それでもぼくはやっていない",
  EXPECT_TRUE(predictor->IsAggressiveSuggestion(4, 13, 6000, true, 20));

  // cost <= 4000
  EXPECT_FALSE(predictor->IsAggressiveSuggestion(4, 13, 4000, true, 20));
}

TEST_F(DictionaryPredictorTest, RealtimeConversionStartingWithAlphabets) {
  Segments segments;
  // turn on real-time conversion
  config_->set_use_dictionary_suggest(false);
  config_->set_use_realtime_conversion(true);

  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  constexpr char kKey[] = "PCてすと";
  const char *kExpectedSuggestionValues[] = {
      "Realtime top result",
      "PCテスト",
      "PCてすと",
  };

  // Set up mock converter for realtime top result.
  {
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kKey);
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = kExpectedSuggestionValues[0];
    ConverterMock *converter = data_and_predictor->mutable_converter_mock();
    converter->SetStartConversionForRequest(&segments, true);
  }

  MakeSegmentsForSuggestion(kKey, &segments);

  std::vector<DictionaryPredictor::Result> results;

  convreq_->set_use_actual_converter_for_realtime_conversion(false);
  predictor->AggregateRealtimeConversion(*convreq_for_suggestion_, 10,
                                         &segments, &results);
  ASSERT_EQ(2, results.size());

  EXPECT_EQ(DictionaryPredictor::REALTIME, results[0].types);
  EXPECT_EQ(kExpectedSuggestionValues[1], results[0].value);
  EXPECT_EQ(kExpectedSuggestionValues[2], results[1].value);
  EXPECT_EQ(1, segments.conversion_segments_size());
}

TEST_F(DictionaryPredictorTest, RealtimeConversionWithSpellingCorrection) {
  Segments segments;
  // turn on real-time conversion
  config_->set_use_dictionary_suggest(false);
  config_->set_use_realtime_conversion(true);

  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  constexpr char kCapriHiragana[] = "かぷりちょうざ";

  // Set up mock converter for realtime top result.
  {
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kCapriHiragana);
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = "Dummy";
    ConverterMock *converter = data_and_predictor->mutable_converter_mock();
    converter->SetStartConversionForRequest(&segments, true);
  }

  std::vector<DictionaryPredictor::Result> results;
  SetUpInputForSuggestion(kCapriHiragana, composer_.get(), &segments);
  convreq_->set_use_actual_converter_for_realtime_conversion(false);
  predictor->AggregateUnigramCandidate(*convreq_for_suggestion_, segments,
                                       &results);
  ASSERT_FALSE(results.empty());
  EXPECT_NE(0, (results[0].candidate_attributes &
                Segment::Candidate::SPELLING_CORRECTION));

  results.clear();
  constexpr char kKeyWithDe[] = "かぷりちょうざで";
  constexpr char kExpectedSuggestionValueWithDe[] = "カプリチョーザで";
  SetUpInputForSuggestion(kKeyWithDe, composer_.get(), &segments);
  predictor->AggregateRealtimeConversion(*convreq_for_suggestion_, 1, &segments,
                                         &results);
  EXPECT_EQ(1, results.size());
  EXPECT_EQ(results[0].types, DictionaryPredictor::REALTIME);
  EXPECT_NE(0, (results[0].candidate_attributes &
                Segment::Candidate::SPELLING_CORRECTION));
  EXPECT_EQ(kExpectedSuggestionValueWithDe, results[0].value);
  EXPECT_EQ(1, segments.conversion_segments_size());
}

TEST_F(DictionaryPredictorTest, GetMissSpelledPosition) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  EXPECT_EQ(0, predictor->GetMissSpelledPosition("", ""));
  EXPECT_EQ(3,
            predictor->GetMissSpelledPosition("れみおめろん", "レミオロメン"));
  EXPECT_EQ(5,
            predictor->GetMissSpelledPosition("とーとばっく", "トートバッグ"));
  EXPECT_EQ(
      4, predictor->GetMissSpelledPosition("おーすとりらあ", "オーストラリア"));
  EXPECT_EQ(7, predictor->GetMissSpelledPosition("じきそうしょう", "時期尚早"));
}

TEST_F(DictionaryPredictorTest, RemoveMissSpelledCandidates) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  {
    std::vector<DictionaryPredictor::Result> results;
    DictionaryPredictor::Result *result;

    results.push_back(DictionaryPredictor::Result());
    result = &results.back();
    result->key = "ばっく";
    result->value = "バッグ";
    result->SetTypesAndTokenAttributes(DictionaryPredictor::UNIGRAM,
                                       Token::SPELLING_CORRECTION);

    results.push_back(DictionaryPredictor::Result());
    result = &results.back();
    result->key = "ばっぐ";
    result->value = "バッグ";
    result->SetTypesAndTokenAttributes(DictionaryPredictor::UNIGRAM,
                                       Token::NONE);

    results.push_back(DictionaryPredictor::Result());
    result = &results.back();
    result->key = "ばっく";
    result->value = "バック";
    result->SetTypesAndTokenAttributes(DictionaryPredictor::UNIGRAM,
                                       Token::NONE);

    predictor->RemoveMissSpelledCandidates(1, &results);
    ASSERT_EQ(3, results.size());

    EXPECT_TRUE(results[0].removed);
    EXPECT_FALSE(results[1].removed);
    EXPECT_TRUE(results[2].removed);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM, results[0].types);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM, results[1].types);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM, results[2].types);
  }

  {
    std::vector<DictionaryPredictor::Result> results;
    DictionaryPredictor::Result *result;

    results.push_back(DictionaryPredictor::Result());
    result = &results.back();
    result->key = "ばっく";
    result->value = "バッグ";
    result->SetTypesAndTokenAttributes(DictionaryPredictor::UNIGRAM,
                                       Token::SPELLING_CORRECTION);

    results.push_back(DictionaryPredictor::Result());
    result = &results.back();
    result->key = "てすと";
    result->value = "テスト";
    result->SetTypesAndTokenAttributes(DictionaryPredictor::UNIGRAM,
                                       Token::NONE);

    predictor->RemoveMissSpelledCandidates(1, &results);
    CHECK_EQ(2, results.size());

    EXPECT_EQ(DictionaryPredictor::UNIGRAM, results[0].types);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM, results[1].types);
  }

  {
    std::vector<DictionaryPredictor::Result> results;
    DictionaryPredictor::Result *result;

    results.push_back(DictionaryPredictor::Result());
    result = &results.back();
    result->key = "ばっく";
    result->value = "バッグ";
    result->SetTypesAndTokenAttributes(DictionaryPredictor::UNIGRAM,
                                       Token::SPELLING_CORRECTION);

    results.push_back(DictionaryPredictor::Result());
    result = &results.back();
    result->key = "ばっく";
    result->value = "バック";
    result->SetTypesAndTokenAttributes(DictionaryPredictor::UNIGRAM,
                                       Token::NONE);

    predictor->RemoveMissSpelledCandidates(1, &results);
    CHECK_EQ(2, results.size());

    EXPECT_TRUE(results[0].removed);
    EXPECT_TRUE(results[1].removed);
  }

  {
    std::vector<DictionaryPredictor::Result> results;
    DictionaryPredictor::Result *result;

    results.push_back(DictionaryPredictor::Result());
    result = &results.back();
    result->key = "ばっく";
    result->value = "バッグ";
    result->SetTypesAndTokenAttributes(DictionaryPredictor::UNIGRAM,
                                       Token::SPELLING_CORRECTION);

    results.push_back(DictionaryPredictor::Result());
    result = &results.back();
    result->key = "ばっく";
    result->value = "バック";
    result->SetTypesAndTokenAttributes(DictionaryPredictor::UNIGRAM,
                                       Token::NONE);

    predictor->RemoveMissSpelledCandidates(3, &results);
    CHECK_EQ(2, results.size());

    EXPECT_FALSE(results[0].removed);
    EXPECT_TRUE(results[1].removed);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM, results[0].types);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM, results[1].types);
  }
}

TEST_F(DictionaryPredictorTest, UseExpansionForUnigramTest) {
  ExpansionForUnigramTestHelper(true);
}

TEST_F(DictionaryPredictorTest, UnuseExpansionForUnigramTest) {
  ExpansionForUnigramTestHelper(false);
}

TEST_F(DictionaryPredictorTest, UseExpansionForBigramTest) {
  ExpansionForBigramTestHelper(true);
}

TEST_F(DictionaryPredictorTest, UnuseExpansionForBigramTest) {
  ExpansionForBigramTestHelper(false);
}

TEST_F(DictionaryPredictorTest, UseExpansionForSuffixTest) {
  ExpansionForSuffixTestHelper(true);
}

TEST_F(DictionaryPredictorTest, UnuseExpansionForSuffixTest) {
  ExpansionForSuffixTestHelper(false);
}

TEST_F(DictionaryPredictorTest, ExpansionPenaltyForRomanTest) {
  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(false);

  table_->LoadFromFile("system://romanji-hiragana.tsv");
  composer_->SetTable(table_.get());
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  segments.set_request_type(Segments::PREDICTION);
  InsertInputSequence("ak", composer_.get());
  Segment *segment = segments.add_segment();
  CHECK(segment);
  {
    std::string query;
    composer_->GetQueryForPrediction(&query);
    segment->set_key(query);
    EXPECT_EQ("あ", query);
  }
  {
    std::string base;
    std::set<std::string> expanded;
    composer_->GetQueriesForPrediction(&base, &expanded);
    EXPECT_EQ("あ", base);
    EXPECT_GT(expanded.size(), 5);
  }

  std::vector<TestableDictionaryPredictor::Result> results;
  TestableDictionaryPredictor::Result *result;

  results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
  result = &results.back();
  result->key = "あか";
  result->value = "赤";
  result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::UNIGRAM,
                                     Token::NONE);

  results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
  result = &results.back();
  result->key = "あき";
  result->value = "秋";
  result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::UNIGRAM,
                                     Token::NONE);

  results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
  result = &results.back();
  result->key = "あかぎ";
  result->value = "アカギ";
  result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::UNIGRAM,
                                     Token::NONE);

  EXPECT_EQ(3, results.size());
  EXPECT_EQ(0, results[0].cost);
  EXPECT_EQ(0, results[1].cost);
  EXPECT_EQ(0, results[2].cost);

  predictor->ApplyPenaltyForKeyExpansion(segments, &results);

  // no penalties
  EXPECT_EQ(0, results[0].cost);
  EXPECT_EQ(0, results[1].cost);
  EXPECT_EQ(0, results[2].cost);
}

TEST_F(DictionaryPredictorTest, ExpansionPenaltyForKanaTest) {
  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(false);

  table_->LoadFromFile("system://kana.tsv");
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  segments.set_request_type(Segments::PREDICTION);
  InsertInputSequence("あし", composer_.get());

  Segment *segment = segments.add_segment();
  CHECK(segment);
  {
    std::string query;
    composer_->GetQueryForPrediction(&query);
    segment->set_key(query);
    EXPECT_EQ("あし", query);
  }
  {
    std::string base;
    std::set<std::string> expanded;
    composer_->GetQueriesForPrediction(&base, &expanded);
    EXPECT_EQ("あ", base);
    EXPECT_EQ(2, expanded.size());
  }

  std::vector<TestableDictionaryPredictor::Result> results;
  TestableDictionaryPredictor::Result *result;

  results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
  result = &results.back();
  result->key = "あし";
  result->value = "足";
  result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::UNIGRAM,
                                     Token::NONE);

  results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
  result = &results.back();
  result->key = "あじ";
  result->value = "味";
  result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::UNIGRAM,
                                     Token::NONE);

  results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
  result = &results.back();
  result->key = "あした";
  result->value = "明日";
  result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::UNIGRAM,
                                     Token::NONE);

  results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
  result = &results.back();
  result->key = "あじあ";
  result->value = "アジア";
  result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::UNIGRAM,
                                     Token::NONE);

  EXPECT_EQ(4, results.size());
  EXPECT_EQ(0, results[0].cost);
  EXPECT_EQ(0, results[1].cost);
  EXPECT_EQ(0, results[2].cost);
  EXPECT_EQ(0, results[3].cost);

  predictor->ApplyPenaltyForKeyExpansion(segments, &results);

  EXPECT_EQ(0, results[0].cost);
  EXPECT_LT(0, results[1].cost);
  EXPECT_EQ(0, results[2].cost);
  EXPECT_LT(0, results[3].cost);
}

TEST_F(DictionaryPredictorTest, GetLMCost) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  TestableDictionaryPredictor::Result result;
  result.wcost = 64;

  for (int rid = 0; rid < 100; ++rid) {
    for (int lid = 0; lid < 100; ++lid) {
      result.lid = lid;
      const int c1 = predictor->connector_->GetTransitionCost(rid, result.lid);
      const int c2 = predictor->connector_->GetTransitionCost(0, result.lid);
      result.types = TestableDictionaryPredictor::SUFFIX;
      EXPECT_EQ(c1 + result.wcost, predictor->GetLMCost(result, rid));

      result.types = TestableDictionaryPredictor::REALTIME;
      EXPECT_EQ(std::min(c1, c2) + result.wcost,
                predictor->GetLMCost(result, rid));
    }
  }
}

TEST_F(DictionaryPredictorTest, SetPredictionCostForMixedConversion) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  segments.set_request_type(Segments::PREDICTION);
  Segment *segment = segments.add_segment();
  CHECK(segment);
  segment->set_key("てすと");

  std::vector<TestableDictionaryPredictor::Result> results;
  TestableDictionaryPredictor::Result *result;

  results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
  result = &results.back();
  result->key = "てすと";
  result->value = "てすと";
  result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::UNIGRAM,
                                     Token::NONE);

  results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
  result = &results.back();
  result->key = "てすと";
  result->value = "テスト";
  result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::UNIGRAM,
                                     Token::NONE);

  results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
  result = &results.back();
  result->key = "てすとてすと";
  result->value = "テストテスト";
  result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::UNIGRAM,
                                     Token::NONE);

  predictor->SetPredictionCostForMixedConversion(segments, &results);

  EXPECT_EQ(3, results.size());
  EXPECT_EQ("てすと", results[0].value);
  EXPECT_EQ("テスト", results[1].value);
  EXPECT_EQ("テストテスト", results[2].value);
  EXPECT_GT(results[2].cost, results[0].cost);
  EXPECT_GT(results[2].cost, results[1].cost);
}

namespace {
void AddTestableDictionaryPredictorResult(
    const char *key, const char *value, int wcost,
    TestableDictionaryPredictor::PredictionTypes prediction_types,
    Token::AttributesBitfield attributes,
    std::vector<TestableDictionaryPredictor::Result> *results) {
  results->push_back(TestableDictionaryPredictor::MakeEmptyResult());
  TestableDictionaryPredictor::Result *result = &results->back();
  result->key = key;
  result->value = value;
  result->wcost = wcost;
  result->SetTypesAndTokenAttributes(prediction_types, attributes);
}

}  // namespace

TEST_F(DictionaryPredictorTest, SetLMCostForUserDictionaryWord) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  const char *kAikaHiragana = "あいか";
  const char *kAikaKanji = "愛佳";

  Segments segments;
  segments.set_request_type(Segments::PREDICTION);
  Segment *segment = segments.add_segment();
  ASSERT_NE(nullptr, segment);
  segment->set_key(kAikaHiragana);

  {
    // Cost of words in user dictionary should be decreased.
    constexpr int kOriginalWordCost = 10000;
    std::vector<TestableDictionaryPredictor::Result> results;
    AddTestableDictionaryPredictorResult(
        kAikaHiragana, kAikaKanji, kOriginalWordCost,
        TestableDictionaryPredictor::UNIGRAM, Token::USER_DICTIONARY, &results);

    predictor->SetPredictionCostForMixedConversion(segments, &results);

    EXPECT_EQ(1, results.size());
    EXPECT_EQ(kAikaKanji, results[0].value);
    EXPECT_GT(kOriginalWordCost, results[0].cost);
    EXPECT_LE(1, results[0].cost);
  }

  {
    // Cost of words in user dictionary should not be decreased to below 1.
    constexpr int kOriginalWordCost = 10;
    std::vector<TestableDictionaryPredictor::Result> results;
    AddTestableDictionaryPredictorResult(
        kAikaHiragana, kAikaKanji, kOriginalWordCost,
        TestableDictionaryPredictor::UNIGRAM, Token::USER_DICTIONARY, &results);

    predictor->SetPredictionCostForMixedConversion(segments, &results);

    EXPECT_EQ(1, results.size());
    EXPECT_EQ(kAikaKanji, results[0].value);
    EXPECT_GT(kOriginalWordCost, results[0].cost);
    EXPECT_LE(1, results[0].cost);
  }

  {
    // Cost of general symbols should not be decreased.
    constexpr int kOriginalWordCost = 10000;
    std::vector<TestableDictionaryPredictor::Result> results;
    AddTestableDictionaryPredictorResult(
        kAikaHiragana, kAikaKanji, kOriginalWordCost,
        TestableDictionaryPredictor::UNIGRAM, Token::USER_DICTIONARY, &results);
    ASSERT_EQ(1, results.size());
    results[0].lid = data_and_predictor->pos_matcher().GetGeneralSymbolId();
    results[0].rid = results[0].lid;
    predictor->SetPredictionCostForMixedConversion(segments, &results);

    EXPECT_EQ(1, results.size());
    EXPECT_EQ(kAikaKanji, results[0].value);
    EXPECT_LE(kOriginalWordCost, results[0].cost);
  }

  {
    // Cost of words not in user dictionary should not be decreased.
    constexpr int kOriginalWordCost = 10000;
    std::vector<TestableDictionaryPredictor::Result> results;
    AddTestableDictionaryPredictorResult(
        kAikaHiragana, kAikaKanji, kOriginalWordCost,
        TestableDictionaryPredictor::UNIGRAM, Token::NONE, &results);

    predictor->SetPredictionCostForMixedConversion(segments, &results);

    EXPECT_EQ(1, results.size());
    EXPECT_EQ(kAikaKanji, results[0].value);
    EXPECT_EQ(kOriginalWordCost, results[0].cost);
  }
}

TEST_F(DictionaryPredictorTest, SuggestSpellingCorrection) {
  testing::MockDataManager data_manager;

  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      new MockDataAndPredictor());
  data_and_predictor->Init(
      CreateSystemDictionaryFromDataManager(data_manager).value().release(),
      CreateSuffixDictionaryFromDataManager(data_manager));

  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  MakeSegmentsForPrediction("あぼがど", &segments);

  predictor->PredictForRequest(*convreq_for_prediction_, &segments);

  EXPECT_TRUE(FindCandidateByValue(segments.conversion_segment(0), "アボカド"));
}

TEST_F(DictionaryPredictorTest, DoNotSuggestSpellingCorrectionBeforeMismatch) {
  testing::MockDataManager data_manager;

  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      new MockDataAndPredictor());
  data_and_predictor->Init(
      CreateSystemDictionaryFromDataManager(data_manager).value().release(),
      CreateSuffixDictionaryFromDataManager(data_manager));

  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  MakeSegmentsForPrediction("あぼが", &segments);

  predictor->PredictForRequest(*convreq_for_prediction_, &segments);

  EXPECT_FALSE(
      FindCandidateByValue(segments.conversion_segment(0), "アボカド"));
}

TEST_F(DictionaryPredictorTest, MobileUnigramSuggestion) {
  testing::MockDataManager data_manager;

  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      new MockDataAndPredictor());
  data_and_predictor->Init(
      CreateSystemDictionaryFromDataManager(data_manager).value().release(),
      CreateSuffixDictionaryFromDataManager(data_manager));

  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  constexpr char kKey[] = "とうきょう";
  SetUpInputForSuggestion(kKey, composer_.get(), &segments);

  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  std::vector<TestableDictionaryPredictor::Result> results;
  predictor->AggregateUnigramCandidateForMixedConversion(
      *convreq_for_suggestion_, segments, &results);

  EXPECT_TRUE(FindResultByValue(results, "東京"));

  int prefix_count = 0;
  for (const auto &result : results) {
    if (absl::StartsWith(result.value, "東京")) {
      ++prefix_count;
    }
  }
  // Should not have same prefix candidates a lot.
  EXPECT_LE(prefix_count, 6);
}

TEST_F(DictionaryPredictorTest, MobileZeroQuerySuggestion) {
  testing::MockDataManager data_manager;

  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      new MockDataAndPredictor());
  data_and_predictor->Init(
      CreateSystemDictionaryFromDataManager(data_manager).value().release(),
      CreateSuffixDictionaryFromDataManager(data_manager));

  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  MakeSegmentsForPrediction("", &segments);

  PrependHistorySegments("だいがく", "大学", &segments);

  commands::RequestForUnitTest::FillMobileRequest(request_.get());
  predictor->PredictForRequest(*convreq_for_prediction_, &segments);

  EXPECT_TRUE(FindCandidateByValue(segments.conversion_segment(0), "入試"));
  EXPECT_TRUE(
      FindCandidateByValue(segments.conversion_segment(0), "入試センター"));
}

// We are not sure what should we suggest after the end of sentence for now.
// However, we decided to show zero query suggestion rather than stopping
// zero query completely. Users may be confused if they cannot see suggestion
// window only after the certain conditions.
// TODO(toshiyuki): Show useful zero query suggestions after EOS.
TEST_F(DictionaryPredictorTest, DISABLED_MobileZeroQuerySuggestionAfterEOS) {
  testing::MockDataManager data_manager;

  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      new MockDataAndPredictor());
  data_and_predictor->Init(
      CreateSystemDictionaryFromDataManager(data_manager).value().release(),
      CreateSuffixDictionaryFromDataManager(data_manager));

  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  const PosMatcher &pos_matcher = data_and_predictor->pos_matcher();

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

  for (size_t i = 0; i < std::size(kTestcases); ++i) {
    const TestCase &test_case = kTestcases[i];

    Segments segments;
    MakeSegmentsForPrediction("", &segments);

    Segment *seg = segments.push_front_segment();
    seg->set_segment_type(Segment::HISTORY);
    seg->set_key(test_case.key);
    Segment::Candidate *c = seg->add_candidate();
    c->key = test_case.key;
    c->content_key = test_case.key;
    c->value = test_case.value;
    c->content_value = test_case.value;
    c->rid = test_case.rid;

    predictor->PredictForRequest(*convreq_for_prediction_, &segments);
    const bool candidates_inserted =
        segments.conversion_segment(0).candidates_size() > 0;
    EXPECT_EQ(test_case.expected_result, candidates_inserted);
  }
}

TEST_F(DictionaryPredictorTest, PropagateUserDictionaryAttribute) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(true);

  {
    segments.Clear();
    segments.set_request_type(Segments::SUGGESTION);
    Segment *seg = segments.add_segment();
    seg->set_key("ゆーざー");
    seg->set_segment_type(Segment::FREE);
    EXPECT_TRUE(
        predictor->PredictForRequest(*convreq_for_suggestion_, &segments));
    EXPECT_EQ(1, segments.conversion_segments_size());
    bool find_yuza_candidate = false;
    for (size_t i = 0; i < segments.conversion_segment(0).candidates_size();
         ++i) {
      const Segment::Candidate &cand =
          segments.conversion_segment(0).candidate(i);
      if (cand.value == "ユーザー" &&
          (cand.attributes & (Segment::Candidate::NO_VARIANTS_EXPANSION |
                              Segment::Candidate::USER_DICTIONARY))) {
        find_yuza_candidate = true;
      }
    }
    EXPECT_TRUE(find_yuza_candidate);
  }

  {
    segments.Clear();
    segments.set_request_type(Segments::SUGGESTION);
    Segment *seg = segments.add_segment();
    seg->set_key("ゆーざーの");
    seg->set_segment_type(Segment::FREE);
    EXPECT_TRUE(
        predictor->PredictForRequest(*convreq_for_suggestion_, &segments));
    EXPECT_EQ(1, segments.conversion_segments_size());
    bool find_yuza_candidate = false;
    for (size_t i = 0; i < segments.conversion_segment(0).candidates_size();
         ++i) {
      const Segment::Candidate &cand =
          segments.conversion_segment(0).candidate(i);
      if ((cand.value == "ユーザーの") &&
          (cand.attributes & (Segment::Candidate::NO_VARIANTS_EXPANSION |
                              Segment::Candidate::USER_DICTIONARY))) {
        find_yuza_candidate = true;
      }
    }
    EXPECT_TRUE(find_yuza_candidate);
  }
}

TEST_F(DictionaryPredictorTest, SetDescription) {
  {
    std::string description;
    DictionaryPredictor::SetDescription(
        TestableDictionaryPredictor::TYPING_CORRECTION, 0, &description);
    EXPECT_EQ("補正", description);

    description.clear();
    DictionaryPredictor::SetDescription(
        0, Segment::Candidate::AUTO_PARTIAL_SUGGESTION, &description);
    EXPECT_EQ("部分", description);
  }
}

TEST_F(DictionaryPredictorTest, SetDebugDescription) {
  {
    std::string description;
    const TestableDictionaryPredictor::PredictionTypes types =
        TestableDictionaryPredictor::UNIGRAM |
        TestableDictionaryPredictor::ENGLISH;
    DictionaryPredictor::SetDebugDescription(types, &description);
    EXPECT_EQ("UE", description);
  }
  {
    std::string description = "description";
    const TestableDictionaryPredictor::PredictionTypes types =
        TestableDictionaryPredictor::REALTIME |
        TestableDictionaryPredictor::BIGRAM;
    DictionaryPredictor::SetDebugDescription(types, &description);
    EXPECT_EQ("description BR", description);
  }
  {
    std::string description;
    const TestableDictionaryPredictor::PredictionTypes types =
        TestableDictionaryPredictor::BIGRAM |
        TestableDictionaryPredictor::REALTIME |
        TestableDictionaryPredictor::SUFFIX;
    DictionaryPredictor::SetDebugDescription(types, &description);
    EXPECT_EQ("BRS", description);
  }
}

TEST_F(DictionaryPredictorTest, MergeAttributesForDebug) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  std::vector<TestableDictionaryPredictor::Result> results;
  for (size_t i = 0; i < 5; ++i) {
    results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
    TestableDictionaryPredictor::Result *result = &results.back();
    result->key = std::string(1, 'a' + i);
    result->value = std::string(1, 'A' + i);
    result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::REALTIME,
                                       Token::NONE);
  }

  for (size_t i = 0; i < 5; ++i) {
    results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
    TestableDictionaryPredictor::Result *result = &results.back();
    result->key = std::string(1, 'a' + i);
    result->value = std::string(1, 'A' + i);
    result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::SUFFIX,
                                       Token::NONE);
  }

  std::random_device rd;
  std::mt19937 urbg(rd());
  std::shuffle(results.begin(), results.end(), urbg);

  Segments segments;
  MakeSegmentsForSuggestion("test", &segments);

  // Enables debug mode.
  config_->set_verbose_level(1);
  predictor->AddPredictionToCandidates(
      *convreq_for_suggestion_,
      false,  // Do not include expect same key result
      &segments, &results);

  EXPECT_EQ(1, segments.conversion_segments_size());
  const Segment &segment = segments.conversion_segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    EXPECT_EQ("RS", segment.candidate(i).description);
  }
}

TEST_F(DictionaryPredictorTest, PropagateRealtimeConversionBoundary) {
  testing::MockDataManager data_manager;
  std::unique_ptr<const DictionaryInterface> dictionary(new DictionaryMock);
  std::unique_ptr<ConverterInterface> converter(new ConverterMock);
  std::unique_ptr<ImmutableConverterInterface> immutable_converter(
      new ImmutableConverterMock);
  std::unique_ptr<const DictionaryInterface> suffix_dictionary(
      CreateSuffixDictionaryFromDataManager(data_manager));
  std::unique_ptr<const Connector> connector =
      Connector::CreateFromDataManager(data_manager).value();
  std::unique_ptr<const Segmenter> segmenter(
      Segmenter::CreateFromDataManager(data_manager));
  std::unique_ptr<const SuggestionFilter> suggestion_filter(
      CreateSuggestionFilter(data_manager));
  const dictionary::PosMatcher pos_matcher(data_manager.GetPosMatcherData());
  std::unique_ptr<TestableDictionaryPredictor> predictor(
      new TestableDictionaryPredictor(
          data_manager, converter.get(), immutable_converter.get(),
          dictionary.get(), suffix_dictionary.get(), connector.get(),
          segmenter.get(), &pos_matcher, suggestion_filter.get()));
  Segments segments;
  constexpr char kKey[] = "わたしのなまえはなかのです";
  MakeSegmentsForSuggestion(kKey, &segments);

  std::vector<TestableDictionaryPredictor::Result> results;
  predictor->AggregateRealtimeConversion(*convreq_for_suggestion_, 10,
                                         &segments, &results);

  // mock results
  EXPECT_EQ(1, results.size());
  predictor->AddPredictionToCandidates(
      *convreq_for_suggestion_,
      false,  // Do not include exact same key results.
      &segments, &results);
  EXPECT_EQ(1, segments.conversion_segments_size());
  EXPECT_EQ(1, segments.conversion_segment(0).candidates_size());
  const Segment::Candidate &cand = segments.conversion_segment(0).candidate(0);
  EXPECT_EQ("わたしのなまえはなかのです", cand.key);
  EXPECT_EQ("私の名前は中野です", cand.value);
  EXPECT_EQ(3, cand.inner_segment_boundary.size());
}

TEST_F(DictionaryPredictorTest, PropagateResultCosts) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  std::vector<TestableDictionaryPredictor::Result> results;
  constexpr int kTestSize = 20;
  for (size_t i = 0; i < kTestSize; ++i) {
    results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
    TestableDictionaryPredictor::Result *result = &results.back();
    result->key = std::string(1, 'a' + i);
    result->value = std::string(1, 'A' + i);
    result->wcost = i;
    result->cost = i + 1000;
    result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::REALTIME,
                                       Token::NONE);
  }
  std::random_device rd;
  std::mt19937 urbg(rd());
  std::shuffle(results.begin(), results.end(), urbg);

  Segments segments;
  MakeSegmentsForSuggestion("test", &segments);
  convreq_for_suggestion_->set_max_dictionary_prediction_candidates_size(
      kTestSize);

  predictor->AddPredictionToCandidates(
      *convreq_for_suggestion_,
      false,  // Do not include expect same key result
      &segments, &results);

  EXPECT_EQ(1, segments.conversion_segments_size());
  ASSERT_EQ(kTestSize, segments.conversion_segment(0).candidates_size());
  const Segment &segment = segments.conversion_segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    EXPECT_EQ(i + 1000, segment.candidate(i).cost);
  }
}

TEST_F(DictionaryPredictorTest, PredictNCandidates) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  std::vector<TestableDictionaryPredictor::Result> results;
  constexpr int kTotalCandidateSize = 100;
  constexpr int kLowCostCandidateSize = 5;
  for (size_t i = 0; i < kTotalCandidateSize; ++i) {
    results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
    TestableDictionaryPredictor::Result *result = &results.back();
    result->key = std::string(1, 'a' + i);
    result->value = std::string(1, 'A' + i);
    result->wcost = i;
    result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::REALTIME,
                                       Token::NONE);
    if (i < kLowCostCandidateSize) {
      result->cost = i + 1000;
    } else {
      result->cost = i + kInfinity;
    }
  }
  std::shuffle(results.begin(), results.end(),
               std::mt19937(std::random_device()()));

  Segments segments;
  MakeSegmentsForSuggestion("test", &segments);
  convreq_for_suggestion_->set_max_dictionary_prediction_candidates_size(
      kLowCostCandidateSize + 1);

  predictor->AddPredictionToCandidates(
      *convreq_for_suggestion_,
      false,  // Do not include expect same key result
      &segments, &results);

  ASSERT_EQ(1, segments.conversion_segments_size());
  ASSERT_EQ(kLowCostCandidateSize,
            segments.conversion_segment(0).candidates_size());
  const Segment &segment = segments.conversion_segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    EXPECT_EQ(i + 1000, segment.candidate(i).cost);
  }
}

TEST_F(DictionaryPredictorTest, SuggestFilteredwordForExactMatchOnMobile) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  // turn on mobile mode
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  Segments segments;
  // Note: The suggestion filter entry "フィルター" for test is not
  // appropriate here, as Katakana entry will be added by realtime conversion.
  // Here, we want to confirm the behavior including unigram prediction.
  MakeSegmentsForSuggestion("ふぃるたーたいしょう", &segments);

  EXPECT_TRUE(
      predictor->PredictForRequest(*convreq_for_suggestion_, &segments));
  EXPECT_TRUE(
      FindCandidateByValue(segments.conversion_segment(0), "フィルター対象"));
  EXPECT_TRUE(
      FindCandidateByValue(segments.conversion_segment(0), "フィルター大将"));

  // However, filtered word should not be the top.
  EXPECT_EQ("フィルター大将",
            segments.conversion_segment(0).candidate(0).value);

  // Should not be there for non-exact suggestion.
  MakeSegmentsForSuggestion("ふぃるたーたいし", &segments);
  EXPECT_TRUE(
      predictor->PredictForRequest(*convreq_for_suggestion_, &segments));
  EXPECT_FALSE(
      FindCandidateByValue(segments.conversion_segment(0), "フィルター対象"));
}

TEST_F(DictionaryPredictorTest, SuppressFilteredwordForExactMatch) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  // Note: The suggestion filter entry "フィルター" for test is not
  // appropriate here, as Katakana entry will be added by realtime conversion.
  // Here, we want to confirm the behavior including unigram prediction.
  MakeSegmentsForSuggestion("ふぃるたーたいしょう", &segments);

  EXPECT_TRUE(
      predictor->PredictForRequest(*convreq_for_suggestion_, &segments));
  EXPECT_FALSE(
      FindCandidateByValue(segments.conversion_segment(0), "フィルター対象"));
}

namespace {
constexpr char kTestTokenArray[] =
    // {"あ", "", ZERO_QUERY_EMOJI, EMOJI_DOCOMO | EMOJI_SOFTBANK, 0xfeb04}
    "\x04\x00\x00\x00"
    "\x00\x00\x00\x00"
    "\x03\x00"
    "\x06\x00"
    "\x04\xeb\x0f\x00"
    // {"あ", "❕", ZERO_QUERY_EMOJI, EMOJI_UNICODE, 0xfeb0b},
    "\x04\x00\x00\x00"
    "\x02\x00\x00\x00"
    "\x03\x00"
    "\x01\x00"
    "\x0b\xeb\x0f\x00"
    // {"あ", "❣", ZERO_QUERY_NONE, EMOJI_NONE, 0x00},
    "\x04\x00\x00\x00"
    "\x03\x00\x00\x00"
    "\x00\x00"
    "\x00\x00"
    "\x00\x00\x00\x00"
    // {"ああ", "( •̀ㅁ•́;)", ZERO_QUERY_EMOTICON, EMOJI_NONE, 0x00}
    "\x05\x00\x00\x00"
    "\x01\x00\x00\x00"
    "\x02\x00"
    "\x00\x00"
    "\x00\x00\x00\x00";

const char *kTestStrings[] = {
    "", "( •̀ㅁ•́;)", "❕", "❣", "あ", "ああ",
};

struct TestEntry {
  int32_t available_emoji_carrier;
  std::string key;
  bool expected_result;
  // candidate value and ZeroQueryType.
  std::vector<std::string> expected_candidates;
  std::vector<int32_t> expected_types;

  std::string DebugString() const {
    const std::string candidates = absl::StrJoin(expected_candidates, ", ");
    std::string types;
    for (size_t i = 0; i < expected_types.size(); ++i) {
      if (i != 0) {
        types.append(", ");
      }
      types.append(absl::StrFormat("%d", types[i]));
    }
    return absl::StrFormat(
        "available_emoji_carrier: %d\n"
        "key: %s\n"
        "expected_result: %d\n"
        "expected_candidates: %s\n"
        "expected_types: %s",
        available_emoji_carrier, key.c_str(), expected_result,
        candidates.c_str(), types.c_str());
  }
};

}  // namespace

TEST_F(DictionaryPredictorTest, GetZeroQueryCandidates) {
  // Create test zero query data.
  std::unique_ptr<uint32_t[]> string_data_buffer;
  ZeroQueryDict zero_query_dict;
  {
    // kTestTokenArray contains a trailing '\0', so create a absl::string_view
    // that excludes it by subtracting 1.
    const absl::string_view token_array_data(kTestTokenArray,
                                             std::size(kTestTokenArray) - 1);
    std::vector<absl::string_view> strs;
    for (const char *str : kTestStrings) {
      strs.push_back(str);
    }
    const absl::string_view string_array_data =
        SerializedStringArray::SerializeToBuffer(strs, &string_data_buffer);
    zero_query_dict.Init(token_array_data, string_array_data);
  }

  std::vector<TestEntry> test_entries;
  {
    TestEntry entry;
    entry.available_emoji_carrier = 0;
    entry.key = "a";
    entry.expected_result = false;
    entry.expected_candidates.clear();
    entry.expected_types.clear();
    test_entries.push_back(entry);
  }
  {
    TestEntry entry;
    entry.available_emoji_carrier = 0;
    entry.key = "ん";
    entry.expected_result = false;
    entry.expected_candidates.clear();
    entry.expected_types.clear();
    test_entries.push_back(entry);
  }
  {
    TestEntry entry;
    entry.available_emoji_carrier = 0;
    entry.key = "ああ";
    entry.expected_result = true;
    entry.expected_candidates.push_back("( •̀ㅁ•́;)");
    entry.expected_types.push_back(ZERO_QUERY_EMOTICON);
    test_entries.push_back(entry);
  }
  {
    TestEntry entry;
    entry.available_emoji_carrier = 0;
    entry.key = "あ";
    entry.expected_result = true;
    entry.expected_candidates.push_back("❣");
    entry.expected_types.push_back(ZERO_QUERY_NONE);
    test_entries.push_back(entry);
  }
  {
    TestEntry entry;
    entry.available_emoji_carrier = commands::Request::UNICODE_EMOJI;
    entry.key = "あ";
    entry.expected_result = true;
    entry.expected_candidates.push_back("❕");
    entry.expected_types.push_back(ZERO_QUERY_EMOJI);

    entry.expected_candidates.push_back("❣");
    entry.expected_types.push_back(ZERO_QUERY_NONE);
    test_entries.push_back(entry);
  }
  {
    TestEntry entry;
    entry.available_emoji_carrier = commands::Request::DOCOMO_EMOJI;
    entry.key = "あ";
    entry.expected_result = true;
    std::string candidate;
    Util::Ucs4ToUtf8(0xfeb04, &candidate);  // exclamation
    entry.expected_candidates.push_back(candidate);
    entry.expected_types.push_back(ZERO_QUERY_EMOJI);

    entry.expected_candidates.push_back("❣");
    entry.expected_types.push_back(ZERO_QUERY_NONE);
    test_entries.push_back(entry);
  }
  {
    TestEntry entry;
    entry.available_emoji_carrier = commands::Request::KDDI_EMOJI;
    entry.key = "あ";
    entry.expected_result = true;
    entry.expected_candidates.push_back("❣");
    entry.expected_types.push_back(ZERO_QUERY_NONE);
    test_entries.push_back(entry);
  }
  {
    TestEntry entry;
    entry.available_emoji_carrier =
        (commands::Request::DOCOMO_EMOJI | commands::Request::SOFTBANK_EMOJI |
         commands::Request::UNICODE_EMOJI);
    entry.key = "あ";
    entry.expected_result = true;
    std::string candidate;
    Util::Ucs4ToUtf8(0xfeb04, &candidate);  // exclamation
    entry.expected_candidates.push_back(candidate);
    entry.expected_types.push_back(ZERO_QUERY_EMOJI);

    entry.expected_candidates.push_back("❕");
    entry.expected_types.push_back(ZERO_QUERY_EMOJI);

    entry.expected_candidates.push_back("❣");
    entry.expected_types.push_back(ZERO_QUERY_NONE);
    test_entries.push_back(entry);
  }

  for (size_t i = 0; i < test_entries.size(); ++i) {
    const TestEntry &test_entry = test_entries[i];
    ASSERT_EQ(test_entry.expected_candidates.size(),
              test_entry.expected_types.size());

    commands::Request client_request;
    client_request.set_available_emoji_carrier(
        test_entry.available_emoji_carrier);
    composer::Table table;
    const config::Config &config = config::ConfigHandler::DefaultConfig();
    composer::Composer composer(&table, &client_request, &config);
    const ConversionRequest request(&composer, &client_request, &config);

    std::vector<DictionaryPredictor::ZeroQueryResult> actual_candidates;
    const bool actual_result =
        DictionaryPredictor::GetZeroQueryCandidatesForKey(
            request, test_entry.key, zero_query_dict, &actual_candidates);
    EXPECT_EQ(test_entry.expected_result, actual_result)
        << test_entry.DebugString();
    for (size_t j = 0; j < test_entry.expected_candidates.size(); ++j) {
      EXPECT_EQ(test_entry.expected_candidates[j], actual_candidates[j].first)
          << "Failed at " << j << " : " << test_entry.DebugString();
      EXPECT_EQ(test_entry.expected_types[j], actual_candidates[j].second)
          << "Failed at " << j << " : " << test_entry.DebugString();
    }
  }
}

namespace {
void SetSegmentForCommit(const std::string &candidate_value,
                         int candidate_source_info, Segments *segments) {
  segments->Clear();
  Segment *segment = segments->add_segment();
  segment->set_key("");
  segment->set_segment_type(Segment::FIXED_VALUE);
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->key = candidate_value;
  candidate->content_key = candidate_value;
  candidate->value = candidate_value;
  candidate->content_value = candidate_value;
  candidate->source_info = candidate_source_info;
}
}  // namespace

TEST_F(DictionaryPredictorTest, UsageStats) {
  std::unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  DictionaryPredictor *predictor =
      data_and_predictor->mutable_dictionary_predictor();

  Segments segments;
  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeNone", 0);
  SetSegmentForCommit(
      "★", Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_NONE, &segments);
  predictor->Finish(*convreq_, &segments);
  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeNone", 1);

  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeNumberSuffix", 0);
  SetSegmentForCommit(
      "個", Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_NUMBER_SUFFIX,
      &segments);
  predictor->Finish(*convreq_, &segments);
  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeNumberSuffix", 1);

  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeEmoticon", 0);
  SetSegmentForCommit(
      "＼(^o^)／", Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_EMOTICON,
      &segments);
  predictor->Finish(*convreq_, &segments);
  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeEmoticon", 1);

  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeEmoji", 0);
  SetSegmentForCommit("❕",
                      Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_EMOJI,
                      &segments);
  predictor->Finish(*convreq_, &segments);
  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeEmoji", 1);

  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeBigram", 0);
  SetSegmentForCommit(
      "ヒルズ", Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_BIGRAM,
      &segments);
  predictor->Finish(*convreq_, &segments);
  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeBigram", 1);

  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeSuffix", 0);
  SetSegmentForCommit(
      "が", Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX,
      &segments);
  predictor->Finish(*convreq_, &segments);
  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeSuffix", 1);
}

}  // namespace mozc
