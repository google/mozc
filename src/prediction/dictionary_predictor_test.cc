// Copyright 2010-2012, Google Inc.
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

#include <utility>

#include "base/freelist.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/character_form_manager.h"
#include "converter/converter_interface.h"
#include "converter/immutable_converter_interface.h"
#include "converter/node.h"
#include "converter/node_allocator.h"
#include "converter/segments.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/suffix_dictionary.h"
#include "dictionary/pos_matcher.h"
#include "session/commands.pb.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"


using ::testing::Return;
using ::testing::_;

DECLARE_string(test_tmpdir);
DECLARE_bool(enable_expansion_for_dictionary_predictor);

namespace mozc {

class DictionaryPredictorTest : public testing::Test {
 public:
  DictionaryPredictorTest() :
      default_expansion_flag_(
          FLAGS_enable_expansion_for_dictionary_predictor) {}

  virtual ~DictionaryPredictorTest() {
    FLAGS_enable_expansion_for_dictionary_predictor = default_expansion_flag_;
  }

 protected:
  virtual void SetUp() {
    FLAGS_enable_expansion_for_dictionary_predictor = false;
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::ConfigHandler::GetDefaultConfig(&default_config_);
    config::ConfigHandler::SetConfig(default_config_);
    GetMockDic()->ClearAll();
    AddWordsToMockDic();
    DictionaryFactory::SetDictionary(GetMockDic());
  }

  DictionaryMock *GetMockDic() {
    return DictionaryMock::GetDictionaryMock();
  }

  void AddWordsToMockDic() {
    // "ぐーぐるあ"
    const char kGoogleA[] = "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B"
        "\xE3\x81\x82";

    const char kGoogleAdsenseHiragana[] = "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90"
        "\xE3\x82\x8B\xE3\x81\x82\xE3\x81\xA9"
        "\xE3\x81\x9B\xE3\x82\x93\xE3\x81\x99";
    // "グーグルアドセンス"
    const char kGoogleAdsenseKatakana[] = "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0"
        "\xE3\x83\xAB\xE3\x82\xA2\xE3\x83\x89"
        "\xE3\x82\xBB\xE3\x83\xB3\xE3\x82\xB9";
    GetMockDic()->AddLookupPredictive(kGoogleA, kGoogleAdsenseHiragana,
                                      kGoogleAdsenseKatakana,
                                      Node::DEFAULT_ATTRIBUTE);

    // "ぐーぐるあどわーず"
    const char kGoogleAdwordsHiragana[] =
        "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B"
        "\xE3\x81\x82\xE3\x81\xA9\xE3\x82\x8F\xE3\x83\xBC\xE3\x81\x9A";
    const char kGoogleAdwordsKatakana[] =
        "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0\xE3\x83\xAB"
        "\xE3\x82\xA2\xE3\x83\x89\xE3\x83\xAF\xE3\x83\xBC\xE3\x82\xBA";
    GetMockDic()->AddLookupPredictive(kGoogleA, kGoogleAdwordsHiragana,
                                      kGoogleAdwordsKatakana,
                                      Node::DEFAULT_ATTRIBUTE);

    // "ぐーぐる"
    const char kGoogle[] = "\xE3\x81\x90\xE3\x83\xBC"
        "\xE3\x81\x90\xE3\x82\x8B";
    GetMockDic()->AddLookupPredictive(kGoogle, kGoogleAdsenseHiragana,
                                      kGoogleAdsenseKatakana,
                                      Node::DEFAULT_ATTRIBUTE);
    GetMockDic()->AddLookupPredictive(kGoogle, kGoogleAdwordsHiragana,
                                      kGoogleAdwordsKatakana,
                                      Node::DEFAULT_ATTRIBUTE);

    // "グーグル"
    const char kGoogleKatakana[] = "\xE3\x82\xB0\xE3\x83\xBC"
        "\xE3\x82\xB0\xE3\x83\xAB";
    GetMockDic()->AddLookupPrefix(kGoogle, kGoogleKatakana,
                                  kGoogleKatakana,
                                  Node::DEFAULT_ATTRIBUTE);

    // "あどせんす"
    const char kAdsense[] = "\xE3\x81\x82\xE3\x81\xA9\xE3\x81\x9B"
        "\xE3\x82\x93\xE3\x81\x99";
    const char kAdsenseKatakana[] = "\xE3\x82\xA2\xE3\x83\x89"
        "\xE3\x82\xBB\xE3\x83\xB3\xE3\x82\xB9";
    GetMockDic()->AddLookupPrefix(kAdsense, kAdsenseKatakana,
                                  kAdsenseKatakana,
                                  Node::DEFAULT_ATTRIBUTE);

    // "てすと"
    const char kTestHiragana[] = "\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8";

    // "テスト"
    const char kTestKatakana[] = "\xE3\x83\x86\xE3\x82\xB9\xE3\x83\x88";

    GetMockDic()->AddLookupPrefix(kTestHiragana, kTestHiragana,
                                  kTestKatakana, Node::DEFAULT_ATTRIBUTE);

    // "かぷりちょうざ"
    const char kWrongCapriHiragana[] = "\xE3\x81\x8B\xE3\x81\xB7\xE3\x82\x8A"
        "\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86\xE3\x81\x96";

    // "かぷりちょーざ"
    const char kRightCapriHiragana[] = "\xE3\x81\x8B\xE3\x81\xB7\xE3\x82\x8A"
        "\xE3\x81\xA1\xE3\x82\x87\xE3\x83\xBC\xE3\x81\x96";

    // "カプリチョーザ"
    const char kCapriKatakana[] = "\xE3\x82\xAB\xE3\x83\x97\xE3\x83\xAA"
        "\xE3\x83\x81\xE3\x83\xA7\xE3\x83\xBC\xE3\x82\xB6";

    GetMockDic()->AddLookupPrefix(kWrongCapriHiragana,
                                  kRightCapriHiragana,
                                  kCapriKatakana,
                                  Node::SPELLING_CORRECTION);

    GetMockDic()->AddLookupPredictive(kWrongCapriHiragana,
                                      kRightCapriHiragana,
                                      kCapriKatakana,
                                      Node::SPELLING_CORRECTION);

    // "で"
    const char kDe[] = "\xE3\x81\xA7";

    GetMockDic()->AddLookupPrefix(kDe, kDe, kDe, Node::DEFAULT_ATTRIBUTE);

    // "ひろすえ/広末"
    const char kHirosueHiragana[] = "\xE3\x81\xB2\xE3\x82\x8D"
        "\xE3\x81\x99\xE3\x81\x88";
    const char kHirosue[] = "\xE5\xBA\x83\xE6\x9C\xAB";

    GetMockDic()->AddLookupPrefix(kHirosueHiragana,
                                  kHirosueHiragana,
                                  kHirosue, Node::DEFAULT_ATTRIBUTE);
  }

 private:
  config::Config default_config_;
  const bool default_expansion_flag_;
};

void MakeSegmentsForSuggestion(const string key,
                               Segments *segments) {
  segments->Clear();
  segments->set_max_prediction_candidates_size(10);
  segments->set_request_type(Segments::SUGGESTION);
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FREE);
}

void PrependHistorySegments(const string &key,
                            const string &value,
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

TEST_F(DictionaryPredictorTest, OnOffTest) {
  DictionaryPredictor predictor;

  // turn off
  Segments segments;
  config::Config config;
  config.set_use_dictionary_suggest(false);
  config.set_use_realtime_conversion(false);
  config::ConfigHandler::SetConfig(config);

  // "ぐーぐるあ"
  MakeSegmentsForSuggestion
      ("\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B\xE3\x81\x82",
       &segments);
  EXPECT_FALSE(predictor.Predict(&segments));

  // turn on
  // "ぐーぐるあ"
  config.set_use_dictionary_suggest(true);
  config::ConfigHandler::SetConfig(config);
  MakeSegmentsForSuggestion
      ("\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B\xE3\x81\x82",
       &segments);
  EXPECT_TRUE(predictor.Predict(&segments));

  // empty query
  MakeSegmentsForSuggestion("", &segments);
  EXPECT_FALSE(predictor.Predict(&segments));
}


TEST_F(DictionaryPredictorTest, BigramTest) {
  Segments segments;
  config::Config config;
  config.set_use_dictionary_suggest(true);
  config::ConfigHandler::SetConfig(config);

  // "あ"
  MakeSegmentsForSuggestion("\xE3\x81\x82", &segments);

  // history is "グーグル"
  PrependHistorySegments("\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B",
                         "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0\xE3\x83\xAB",
                         &segments);

  DictionaryPredictor predictor;
  // "グーグルアドセンス" will be returned.
  EXPECT_TRUE(predictor.Predict(&segments));
}


// Check that previous candidate never be shown at the current candidate.
TEST_F(DictionaryPredictorTest, Regression3042706) {
  Segments segments;
  config::Config config;
  config.set_use_dictionary_suggest(true);
  config::ConfigHandler::SetConfig(config);

  // "だい"
  MakeSegmentsForSuggestion("\xE3\x81\xA0\xE3\x81\x84", &segments);

  // history is "きょうと/京都"
  PrependHistorySegments("\xE3\x81\x8D\xE3\x82\x87"
                         "\xE3\x81\x86\xE3\x81\xA8",
                         "\xE4\xBA\xAC\xE9\x83\xBD",
                         &segments);

  DictionaryPredictor predictor;
  EXPECT_TRUE(predictor.Predict(&segments));
  EXPECT_EQ(2, segments.segments_size());   // history + current
  for (int i = 0; i < segments.segment(1).candidates_size(); ++i) {
    const Segment::Candidate &candidate = segments.segment(1).candidate(i);
    // "京都"
    EXPECT_FALSE(Util::StartsWith(candidate.content_value,
                                  "\xE4\xBA\xAC\xE9\x83\xBD"));
    // "だい"
    EXPECT_TRUE(Util::StartsWith(candidate.content_key,
                                 "\xE3\x81\xA0\xE3\x81\x84"));
  }
}

TEST_F(DictionaryPredictorTest, GetPredictionType) {
  Segments segments;
  config::Config config;
  config.set_use_dictionary_suggest(true);
  config.set_use_realtime_conversion(false);
  config::ConfigHandler::SetConfig(config);

  DictionaryPredictor predictor;

  // empty segments
  {
    EXPECT_EQ(
        DictionaryPredictor::NO_PREDICTION,
        predictor.GetPredictionType(segments));
  }

  // normal segments
  {
    // "てすとだよ"
    MakeSegmentsForSuggestion("\xE3\x81\xA6\xE3\x81\x99\xE3"
                              "\x81\xA8\xE3\x81\xA0\xE3\x82\x88",
                              &segments);
    EXPECT_EQ(
        DictionaryPredictor::UNIGRAM,
        predictor.GetPredictionType(segments));

    segments.set_request_type(Segments::PREDICTION);
    EXPECT_EQ(
        DictionaryPredictor::UNIGRAM,
        predictor.GetPredictionType(segments));

    segments.set_request_type(Segments::CONVERSION);
    EXPECT_EQ(
        DictionaryPredictor::NO_PREDICTION,
        predictor.GetPredictionType(segments));
  }

  // short key
  {
    // "てす"
    MakeSegmentsForSuggestion("\xE3\x81\xA6\xE3\x81\x99",
                              &segments);
    EXPECT_EQ(
        DictionaryPredictor::NO_PREDICTION,
        predictor.GetPredictionType(segments));

    // on prediction mode, return UNIGRAM
    segments.set_request_type(Segments::PREDICTION);
    EXPECT_EQ(
        DictionaryPredictor::UNIGRAM,
        predictor.GetPredictionType(segments));
  }

  // zipcode-like key
  {
    MakeSegmentsForSuggestion("0123", &segments);
    EXPECT_EQ(
        DictionaryPredictor::NO_PREDICTION,
        predictor.GetPredictionType(segments));
  }

  // History is short => UNIGRAM
  {
    // "てすとだよ"
    MakeSegmentsForSuggestion("\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8"
                              "\xE3\x81\xA0\xE3\x82\x88", &segments);
    PrependHistorySegments("A", "A", &segments);
    EXPECT_EQ(
        DictionaryPredictor::UNIGRAM,
        predictor.GetPredictionType(segments));
  }

  // both History and current segment are long => UNIGRAM|BIGRAM
  {
    // "てすとだよ"
    MakeSegmentsForSuggestion("\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8"
                              "\xE3\x81\xA0\xE3\x82\x88", &segments);
    PrependHistorySegments("\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8"
                           "\xE3\x81\xA0\xE3\x82\x88", "abc", &segments);
    EXPECT_EQ(
        DictionaryPredictor::UNIGRAM | DictionaryPredictor::BIGRAM,
        predictor.GetPredictionType(segments));
  }

  // Current segment is short => BIGRAM
  {
    MakeSegmentsForSuggestion("A", &segments);
    // "てすとだよ"
    PrependHistorySegments("\xE3\x81\xA6\xE3\x81\x99"
                           "\xE3\x81\xA8\xE3\x81\xA0\xE3\x82\x88",
                           "abc", &segments);
    EXPECT_EQ(
        DictionaryPredictor::BIGRAM,
        predictor.GetPredictionType(segments));
  }
}


TEST_F(DictionaryPredictorTest,
       AggregateUnigramPrediction) {
  Segments segments;
  DictionaryPredictor predictor;

  // "ぐーぐるあ"
  const char kKey[] = "\xE3\x81\x90\xE3\x83\xBC"
      "\xE3\x81\x90\xE3\x82\x8B\xE3\x81\x82";

  MakeSegmentsForSuggestion(kKey, &segments);

  vector<DictionaryPredictor::Result> results;
  NodeAllocator allocator;

  predictor.AggregateUnigramPrediction(
      DictionaryPredictor::BIGRAM,
      &segments, &allocator, &results);
  EXPECT_TRUE(results.empty());

  predictor.AggregateUnigramPrediction(
      DictionaryPredictor::REALTIME,
      &segments, &allocator, &results);
  EXPECT_TRUE(results.empty());

  predictor.AggregateUnigramPrediction(
      DictionaryPredictor::UNIGRAM,
      &segments, &allocator, &results);
  EXPECT_FALSE(results.empty());

  for (size_t i = 0; i < results.size(); ++i) {
    EXPECT_EQ(DictionaryPredictor::UNIGRAM, results[i].type);
    EXPECT_TRUE(Util::StartsWith(results[i].node->key, kKey));
  }

  EXPECT_EQ(1, segments.conversion_segments_size());
}

TEST_F(DictionaryPredictorTest,
       AggregateBigramPrediction) {
  DictionaryPredictor predictor;
  NodeAllocator allocator;

  {
    Segments segments;

    // "あ"
    MakeSegmentsForSuggestion("\xE3\x81\x82", &segments);

    // history is "グーグル"
    const char kHistoryKey[] =
        "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B";
    const char kHistoryValue[] =
        "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0\xE3\x83\xAB";

    PrependHistorySegments(kHistoryKey, kHistoryValue,
                           &segments);

    vector<DictionaryPredictor::Result> results;

    predictor.AggregateBigramPrediction(
        DictionaryPredictor::UNIGRAM,
        &segments, &allocator, &results);
    EXPECT_TRUE(results.empty());

    predictor.AggregateBigramPrediction(
        DictionaryPredictor::REALTIME,
        &segments, &allocator, &results);
    EXPECT_TRUE(results.empty());

    predictor.AggregateBigramPrediction(
        DictionaryPredictor::BIGRAM,
        &segments, &allocator, &results);
    EXPECT_FALSE(results.empty());

    for (size_t i = 0; i < results.size(); ++i) {
      // only "グーグルアドセンス" is in the dictionary.
      if (results[i].node->value == "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0"
          "\xE3\x83\xAB\xE3\x82\xA2\xE3\x83\x89"
          "\xE3\x82\xBB\xE3\x83\xB3\xE3\x82\xB9") {
        EXPECT_EQ(DictionaryPredictor::BIGRAM, results[i].type);
      } else {
        EXPECT_EQ(DictionaryPredictor::NO_PREDICTION, results[i].type);
      }
      EXPECT_TRUE(Util::StartsWith(results[i].node->key, kHistoryKey));
      EXPECT_TRUE(Util::StartsWith(results[i].node->value, kHistoryValue));
    }

    EXPECT_EQ(1, segments.conversion_segments_size());
  }

  {
    Segments segments;

    // "と"
    MakeSegmentsForSuggestion("\xE3\x81\x82", &segments);

    // "てす"
    const char kHistoryKey[] = "\xE3\x81\xA6\xE3\x81\x99";
    // "テス"
    const char kHistoryValue[] = "\xE3\x83\x86\xE3\x82\xB9";

    PrependHistorySegments(kHistoryKey, kHistoryValue,
                           &segments);

    vector<DictionaryPredictor::Result> results;

    predictor.AggregateBigramPrediction(
        DictionaryPredictor::BIGRAM,
        &segments, &allocator, &results);
    EXPECT_TRUE(results.empty());
  }
}

TEST_F(DictionaryPredictorTest, GetRealtimeCandidateMaxSize) {
  DictionaryPredictor predictor;
  Segments segments;

  // GetRealtimeCandidateMaxSize has some heuristics so here we test following
  // conditions.
  // - The result must be equal or less than kMaxSize;
  // - If mixed_conversion is the same, the result of SUGGESTION is
  //        equal or less than PREDICTION.
  // - If mixed_conversion is the same, the result of PARTIAL_SUGGESTION is
  //        equal or less than PARTIAL_PREDICTION.
  // - Partial version has equal or greater than non-partial version.

  const size_t kMaxSize = 100;

  // non-partial, non-mixed-conversion
  segments.set_request_type(Segments::PREDICTION);
  const size_t prediction_no_mixed =
      predictor.GetRealtimeCandidateMaxSize(segments, false, kMaxSize);
  EXPECT_GE(kMaxSize, prediction_no_mixed);

  segments.set_request_type(Segments::SUGGESTION);
  const size_t suggestion_no_mixed =
      predictor.GetRealtimeCandidateMaxSize(segments, false, kMaxSize);
  EXPECT_GE(kMaxSize, suggestion_no_mixed);
  EXPECT_LE(suggestion_no_mixed, prediction_no_mixed);

  // non-partial, mixed-conversion
  segments.set_request_type(Segments::PREDICTION);
  const size_t prediction_mixed =
      predictor.GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, prediction_mixed);

  segments.set_request_type(Segments::SUGGESTION);
  const size_t suggestion_mixed =
      predictor.GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, suggestion_mixed);
  EXPECT_EQ(kMaxSize, prediction_mixed + suggestion_mixed);

  // partial, non-mixed-conversion
  segments.set_request_type(Segments::PARTIAL_PREDICTION);
  const size_t partial_prediction_no_mixed =
      predictor.GetRealtimeCandidateMaxSize(segments, false, kMaxSize);
  EXPECT_GE(kMaxSize, partial_prediction_no_mixed);

  segments.set_request_type(Segments::PARTIAL_SUGGESTION);
  const size_t partial_suggestion_no_mixed =
      predictor.GetRealtimeCandidateMaxSize(segments, false, kMaxSize);
  EXPECT_GE(kMaxSize, partial_suggestion_no_mixed);
  EXPECT_LE(partial_suggestion_no_mixed, partial_prediction_no_mixed);

  // partial, mixed-conversion
  segments.set_request_type(Segments::PARTIAL_PREDICTION);
  const size_t partial_prediction_mixed =
      predictor.GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, partial_prediction_mixed);

  segments.set_request_type(Segments::PARTIAL_SUGGESTION);
  const size_t partial_suggestion_mixed =
      predictor.GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, partial_suggestion_mixed);
  EXPECT_LE(partial_suggestion_mixed, partial_prediction_mixed);

  EXPECT_GE(partial_prediction_no_mixed, prediction_no_mixed);
  EXPECT_GE(partial_prediction_mixed, prediction_mixed);
  EXPECT_GE(partial_suggestion_no_mixed, suggestion_no_mixed);
  EXPECT_GE(partial_suggestion_mixed, suggestion_mixed);
}

TEST_F(DictionaryPredictorTest, GetRealtimeCandidateMaxSizeForMixed) {
  DictionaryPredictor predictor;
  Segments segments;
  Segment *segment = segments.add_segment();

  const size_t kMaxSize = 100;

  // for short key, try to provide many results as possible
  segment->set_key("short");
  segments.set_request_type(Segments::SUGGESTION);
  const size_t short_suggestion_mixed =
      predictor.GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, short_suggestion_mixed);

  segments.set_request_type(Segments::PREDICTION);
  const size_t short_prediction_mixed =
      predictor.GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, short_prediction_mixed);
  EXPECT_EQ(kMaxSize, short_prediction_mixed + short_suggestion_mixed);

  // for long key, provide few results
  segment->set_key("long_request_key");
  segments.set_request_type(Segments::SUGGESTION);
  const size_t long_suggestion_mixed =
      predictor.GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, long_suggestion_mixed);
  EXPECT_GT(short_suggestion_mixed, long_suggestion_mixed);

  segments.set_request_type(Segments::PREDICTION);
  const size_t long_prediction_mixed =
      predictor.GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, long_prediction_mixed);
  EXPECT_GT(kMaxSize, long_prediction_mixed + long_suggestion_mixed);
  EXPECT_GT(short_prediction_mixed, long_prediction_mixed);
}

TEST_F(DictionaryPredictorTest,
       AggregateRealtimeConversion) {
  // Simple immutable converte mock for the test
  // TODO(toshiyuki): Implement proper mock under converter directory if needed.
  class ImmutableConverterMock : public ImmutableConverterInterface {
   public:
    ImmutableConverterMock() {
      Segment *segment = segments_.add_segment();
      // "わたしのなまえはなかのです"
      segment->set_key("\xe3\x82\x8f\xe3\x81\x9f\xe3\x81\x97\xe3\x81\xae"
                       "\xe3\x81\xaa\xe3\x81\xbe\xe3\x81\x88\xe3\x81\xaf"
                       "\xe3\x81\xaa\xe3\x81\x8b\xe3\x81\xae\xe3\x81\xa7"
                       "\xe3\x81\x99");
      Segment::Candidate *candidate = segment->add_candidate();
      // "私の名前は中野です"
      candidate->value = "\xe7\xa7\x81\xe3\x81\xae\xe5\x90\x8d\xe5\x89\x8d"
          "\xe3\x81\xaf\xe4\xb8\xad\xe9\x87\x8e\xe3\x81\xa7\xe3\x81\x99";
      // "わたしのなまえはなかのです"
      candidate->key = ("\xe3\x82\x8f\xe3\x81\x9f\xe3\x81\x97\xe3\x81\xae"
                        "\xe3\x81\xaa\xe3\x81\xbe\xe3\x81\x88\xe3\x81\xaf"
                        "\xe3\x81\xaa\xe3\x81\x8b\xe3\x81\xae\xe3\x81\xa7"
                        "\xe3\x81\x99");
    }

    virtual bool Convert(Segments *segments) const {
      segments->CopyFrom(segments_);
      return true;
    }

   private:
    Segments segments_;
  };

  ImmutableConverterMock immutable_converter_mock;
  ImmutableConverterFactory::SetImmutableConverter(&immutable_converter_mock);
  Segments segments;
  DictionaryPredictor predictor;
  NodeAllocator allocator;

  // "わたしのなまえはなかのです"
  const char kKey[] =
      "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97"
      "\xE3\x81\xAE\xE3\x81\xAA\xE3\x81\xBE"
      "\xE3\x81\x88\xE3\x81\xAF\xE3\x81\xAA"
      "\xE3\x81\x8B\xE3\x81\xAE\xE3\x81\xA7\xE3\x81\x99";

  MakeSegmentsForSuggestion(kKey, &segments);

  vector<DictionaryPredictor::Result> results;

  predictor.AggregateRealtimeConversion(
      DictionaryPredictor::UNIGRAM,
      &segments, &allocator, &results);
  EXPECT_TRUE(results.empty());

  predictor.AggregateRealtimeConversion(
      DictionaryPredictor::BIGRAM,
      &segments, &allocator, &results);
  EXPECT_TRUE(results.empty());

  predictor.AggregateRealtimeConversion(
      DictionaryPredictor::REALTIME,
      &segments, &allocator, &results);
  EXPECT_FALSE(results.empty());

  for (size_t i = 0; i < results.size(); ++i) {
    EXPECT_EQ(DictionaryPredictor::REALTIME, results[i].type);
    EXPECT_EQ(kKey, results[i].node->key);
  }

  EXPECT_EQ(1, segments.conversion_segments_size());

  ImmutableConverterFactory::SetImmutableConverter(NULL);
}

namespace {
struct SuffixToken {
  const char *key;
  const char *value;
};

const SuffixToken kSuffixTokens[] = {
  //  { "いか",   "以下" }
  { "\xE3\x81\x84\xE3\x81\x8B",   "\xE4\xBB\xA5\xE4\xB8\x8B" }
};

class TestSuffixDictionary : public DictionaryInterface {
 public:
  TestSuffixDictionary() {}
  virtual ~TestSuffixDictionary() {}

  virtual Node *LookupPredictive(const char *str, int size,
                                 NodeAllocatorInterface *allocator) const {
    string input_key(str, size);
    Node *result = NULL;
    for (size_t i = 0; i < arraysize(kSuffixTokens); ++i) {
      const SuffixToken *token = &kSuffixTokens[i];
      DCHECK(token);
      DCHECK(token->key);
      if (!input_key.empty() && !Util::StartsWith(token->key, input_key)) {
        continue;
      }
      Node *node = allocator->NewNode();
      DCHECK(node);
      node->Init();
      node->wcost = 1000;
      node->key = token->key;
      node->value = token->value;
      node->lid = 0;
      node->rid = 0;
      node->bnext = result;
      result = node;
    }
    return result;
  }

  virtual Node *LookupPredictiveWithLimit(
      const char *str, int size,
      const Limit &limit,
      NodeAllocatorInterface *allocator) const {
    return NULL;
  }

  virtual Node *LookupPrefixWithLimit(
      const char *str, int size,
      const Limit &limit,
      NodeAllocatorInterface *allocator) const {
    return NULL;
  }

  virtual Node *LookupPrefix(
      const char *str, int size,
      NodeAllocatorInterface *allocator) const {
    return NULL;
  }

  virtual Node *LookupReverse(const char *str, int size,
                              NodeAllocatorInterface *allocator) const {
    return NULL;
  }
};

class CallCheckDictionary : public DictionaryInterface {
 public:
  CallCheckDictionary() {}
  virtual ~CallCheckDictionary() {}

  MOCK_CONST_METHOD3(LookupPredictive,
                     Node *(const char *str, int size,
                            NodeAllocatorInterface *allocator));
  MOCK_CONST_METHOD4(LookupPredictiveWithLimit,
                     Node *(const char *str, int size,
                            const Limit &limit,
                            NodeAllocatorInterface *allocator));
  MOCK_CONST_METHOD3(LookupPrefix,
                     Node *(const char *str, int size,
                            NodeAllocatorInterface *allocator));
  MOCK_CONST_METHOD4(LookupPrefixWithLimit,
                     Node *(const char *str, int size,
                            const Limit &limit,
                            NodeAllocatorInterface *allocator));
  MOCK_CONST_METHOD3(LookupReverse,
                     Node *(const char *str, int size,
                            NodeAllocatorInterface *allocator));
};
}  // namespace

TEST_F(DictionaryPredictorTest, GetUnigramCandidateCutoffThreshold) {
  DictionaryPredictor predictor;
  Segments segments;

  // GetUnigramCandidateCutoffThreshold has some heuristics so here we test
  // following conditions.
  // - The result of SUGGESTION is equal or less than PREDICTION.
  //     - If this condition is broken, expanding suggestion will corrupt
  //       because SessionConverter::AppendCandidateList doesn't expect
  //       such situation.

  // non-partial, mixed-conversion
  segments.set_request_type(Segments::PREDICTION);
  const size_t prediction_mixed =
      predictor.GetUnigramCandidateCutoffThreshold(segments, true);

  segments.set_request_type(Segments::SUGGESTION);
  const size_t suggestion_mixed =
      predictor.GetUnigramCandidateCutoffThreshold(segments, true);
  EXPECT_LE(suggestion_mixed, prediction_mixed);

  // non-partial, non-mixed-conversion
  segments.set_request_type(Segments::PREDICTION);
  const size_t prediction_no_mixed =
      predictor.GetUnigramCandidateCutoffThreshold(segments, false);

  segments.set_request_type(Segments::SUGGESTION);
  const size_t suggestion_no_mixed =
      predictor.GetUnigramCandidateCutoffThreshold(segments, false);
  EXPECT_LE(suggestion_no_mixed, prediction_no_mixed);
}

TEST_F(DictionaryPredictorTest,
       AggregateSuffixPrediction) {
  TestSuffixDictionary dictionary;
  SuffixDictionaryFactory::SetSuffixDictionary(&dictionary);
  DictionaryPredictor predictor;
  NodeAllocator allocator;

  Segments segments;

  // "あ"
  MakeSegmentsForSuggestion("\xE3\x81\x82", &segments);

  // history is "グーグル"
  const char kHistoryKey[] =
      "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B";
  const char kHistoryValue[] =
      "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0\xE3\x83\xAB";

  PrependHistorySegments(kHistoryKey, kHistoryValue,
                         &segments);

  vector<DictionaryPredictor::Result> results;

  // Since SuffixDictionary only returns when key is "い".
  // result should be empty.
  predictor.AggregateSuffixPrediction(
      DictionaryPredictor::SUFFIX,
      &segments, &allocator, &results);
  EXPECT_TRUE(results.empty());

  results.clear();
  segments.mutable_conversion_segment(0)->set_key("");
  predictor.AggregateSuffixPrediction(
      DictionaryPredictor::SUFFIX,
      &segments, &allocator, &results);
  EXPECT_FALSE(results.empty());

  results.clear();
  predictor.AggregateSuffixPrediction(
      DictionaryPredictor::UNIGRAM,
      &segments, &allocator, &results);
  EXPECT_TRUE(results.empty());

  predictor.AggregateSuffixPrediction(
      DictionaryPredictor::REALTIME,
      &segments, &allocator, &results);
  EXPECT_TRUE(results.empty());

  predictor.AggregateSuffixPrediction(
      DictionaryPredictor::BIGRAM,
      &segments, &allocator, &results);
  EXPECT_TRUE(results.empty());

  SuffixDictionaryFactory::SetSuffixDictionary(NULL);
}


TEST_F(DictionaryPredictorTest, GetHistoryKeyAndValue) {
  Segments segments;
  DictionaryPredictor predictor;

  MakeSegmentsForSuggestion("test", &segments);

  string key, value;
  EXPECT_FALSE(predictor.GetHistoryKeyAndValue(segments, &key, &value));

  PrependHistorySegments("key", "value", &segments);
  EXPECT_TRUE(predictor.GetHistoryKeyAndValue(segments, &key, &value));
  EXPECT_EQ("key", key);
  EXPECT_EQ("value", value);
}

TEST_F(DictionaryPredictorTest, IsZipCodeRequest) {
  DictionaryPredictor predictor;
  EXPECT_FALSE(predictor.IsZipCodeRequest(""));
  EXPECT_TRUE(predictor.IsZipCodeRequest("000"));
  EXPECT_TRUE(predictor.IsZipCodeRequest("000"));
  EXPECT_FALSE(predictor.IsZipCodeRequest("ABC"));
  EXPECT_TRUE(predictor.IsZipCodeRequest("---"));
  EXPECT_TRUE(predictor.IsZipCodeRequest("0124-"));
  EXPECT_TRUE(predictor.IsZipCodeRequest("0124-0"));
  EXPECT_TRUE(predictor.IsZipCodeRequest("012-0"));
  EXPECT_TRUE(predictor.IsZipCodeRequest("012-3456"));
  // "０１２-０"
  EXPECT_FALSE(predictor.IsZipCodeRequest(
      "\xef\xbc\x90\xef\xbc\x91\xef\xbc\x92\x2d\xef\xbc\x90"));
}

TEST_F(DictionaryPredictorTest, IsAggressiveSuggestion) {
  DictionaryPredictor predictor;

  // "ただしい",
  // "ただしいけめんにかぎる",
  EXPECT_TRUE(predictor.IsAggressiveSuggestion(
      4,      // query_len
      11,     // key_len
      6000,   // cost
      true,   // is_suggestion
      20));   // total_candidates_size

  // cost <= 4000
  EXPECT_FALSE(predictor.IsAggressiveSuggestion(
      4,
      11,
      4000,
      true,
      20));

  // not suggestion
  EXPECT_FALSE(predictor.IsAggressiveSuggestion(
      4,
      11,
      4000,
      false,
      20));

  // total_candidates_size is small
  EXPECT_FALSE(predictor.IsAggressiveSuggestion(
      4,
      11,
      4000,
      true,
      5));

  // query_length = 5
  EXPECT_FALSE(predictor.IsAggressiveSuggestion(
      5,
      11,
      6000,
      true,
      20));

  // "それでも",
  // "それでもぼくはやっていない",
  EXPECT_TRUE(predictor.IsAggressiveSuggestion(
      4,
      13,
      6000,
      true,
      20));

  // cost <= 4000
  EXPECT_FALSE(predictor.IsAggressiveSuggestion(
      4,
      13,
      4000,
      true,
      20));
}

TEST_F(DictionaryPredictorTest,
       RealtimeConversionStartingWithAlphabets) {
  Segments segments;
  NodeAllocator allocator;
  // turn on real-time conversion
  config::Config config;
  config.set_use_dictionary_suggest(false);
  config.set_use_realtime_conversion(true);
  config::ConfigHandler::SetConfig(config);
  DictionaryPredictor predictor;

  // "PCてすと"
  const char kKey[] = "PC\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8";
  // "PCテスト"
  const char kExpectedSuggestionValue[] =
      "PC\xE3\x83\x86\xE3\x82\xB9\xE3\x83\x88";

  MakeSegmentsForSuggestion(kKey, &segments);

  vector<DictionaryPredictor::Result> results;

  predictor.AggregateRealtimeConversion(
      DictionaryPredictor::REALTIME,
      &segments, &allocator, &results);
  EXPECT_FALSE(results.empty());

  EXPECT_EQ(DictionaryPredictor::REALTIME, results[0].type);
  EXPECT_EQ(kExpectedSuggestionValue, results[0].node->value);
  EXPECT_EQ(1, segments.conversion_segments_size());
}

TEST_F(DictionaryPredictorTest,
       RealtimeConversionWithSpellingCorrection) {
  Segments segments;
  NodeAllocator allocator;
  // turn on real-time conversion
  config::Config config;
  config.set_use_dictionary_suggest(false);
  config.set_use_realtime_conversion(true);
  config::ConfigHandler::SetConfig(config);
  DictionaryPredictor predictor;

  // "かぷりちょうざ"
  const char kCapriHiragana[] = "\xE3\x81\x8B\xE3\x81\xB7\xE3\x82\x8A"
      "\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86\xE3\x81\x96";

  MakeSegmentsForSuggestion(kCapriHiragana, &segments);

  vector<DictionaryPredictor::Result> results;

  predictor.AggregateUnigramPrediction(
      DictionaryPredictor::UNIGRAM,
      &segments, &allocator, &results);

  EXPECT_FALSE(results.empty());
  EXPECT_TRUE(results[0].node->attributes & Node::SPELLING_CORRECTION);

  results.clear();

  // "かぷりちょうざで"
  const char kKeyWithDe[] = "\xE3\x81\x8B\xE3\x81\xB7\xE3\x82\x8A"
      "\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86\xE3\x81\x96\xE3\x81\xA7";
  // "カプリチョーザで"
  const char kExpectedSuggestionValueWithDe[] = "\xE3\x82\xAB\xE3\x83\x97"
      "\xE3\x83\xAA\xE3\x83\x81\xE3\x83\xA7\xE3\x83\xBC\xE3\x82\xB6"
      "\xE3\x81\xA7";

  MakeSegmentsForSuggestion(kKeyWithDe, &segments);
  predictor.AggregateRealtimeConversion(
      DictionaryPredictor::REALTIME,
      &segments, &allocator, &results);
  EXPECT_FALSE(results.empty());

  EXPECT_EQ(results[0].type, DictionaryPredictor::REALTIME);
  EXPECT_TRUE(results[0].node->attributes &
              Node::SPELLING_CORRECTION);
  EXPECT_EQ(kExpectedSuggestionValueWithDe, results[0].node->value);
  EXPECT_EQ(1, segments.conversion_segments_size());
}

TEST_F(DictionaryPredictorTest, GetMissSpelledPosition) {
  DictionaryPredictor predictor;

  EXPECT_EQ(0, predictor.GetMissSpelledPosition("", ""));

  // EXPECT_EQ(3, predictor.GetMissSpelledPosition(
  //              "れみおめろん",
  //              "レミオロメン"));
  EXPECT_EQ(3, predictor.GetMissSpelledPosition(
      "\xE3\x82\x8C\xE3\x81\xBF\xE3\x81\x8A"
      "\xE3\x82\x81\xE3\x82\x8D\xE3\x82\x93",
      "\xE3\x83\xAC\xE3\x83\x9F\xE3\x82\xAA"
      "\xE3\x83\xAD\xE3\x83\xA1\xE3\x83\xB3"));

  // EXPECT_EQ(5, predictor.GetMissSpelledPosition
  //              "とーとばっく",
  //              "トートバッグ"));
  EXPECT_EQ(5, predictor.GetMissSpelledPosition(
      "\xE3\x81\xA8\xE3\x83\xBC\xE3\x81\xA8\xE3\x81\xB0"
      "\xE3\x81\xA3\xE3\x81\x8F",
      "\xE3\x83\x88\xE3\x83\xBC\xE3\x83\x88\xE3\x83\x90"
      "\xE3\x83\x83\xE3\x82\xB0"));

  // EXPECT_EQ(4, predictor.GetMissSpelledPosition(
  //               "おーすとりらあ",
  //               "オーストラリア"));
  EXPECT_EQ(4, predictor.GetMissSpelledPosition(
      "\xE3\x81\x8A\xE3\x83\xBC\xE3\x81\x99\xE3\x81\xA8"
      "\xE3\x82\x8A\xE3\x82\x89\xE3\x81\x82",
      "\xE3\x82\xAA\xE3\x83\xBC\xE3\x82\xB9\xE3\x83\x88"
      "\xE3\x83\xA9\xE3\x83\xAA\xE3\x82\xA2"));

  // EXPECT_EQ(7, predictor.GetMissSpelledPosition(
  //               "じきそうしょう",
  //               "時期尚早"));
  EXPECT_EQ(7, predictor.GetMissSpelledPosition(
      "\xE3\x81\x98\xE3\x81\x8D\xE3\x81\x9D\xE3\x81\x86"
      "\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86",
      "\xE6\x99\x82\xE6\x9C\x9F\xE5\xB0\x9A\xE6\x97\xA9"));
}

TEST_F(DictionaryPredictorTest, RemoveMissSpelledCandidates) {
  DictionaryPredictor predictor;
  FreeList<Node> freelist(64);
  Node *node;

  {
    vector<DictionaryPredictor::Result> results;
    DictionaryPredictor::Result result;
    node = freelist.Alloc(1);
    node->Init();
    // node->key = "ばっく";
    // node->value = "バッグ";
    node->key = "\xE3\x81\xB0\xE3\x81\xA3\xE3\x81\x8F";
    node->value = "\xE3\x83\x90\xE3\x83\x83\xE3\x82\xB0";
    node->attributes = Node::SPELLING_CORRECTION;
    results.push_back(DictionaryPredictor::Result(
        node, DictionaryPredictor::UNIGRAM));

    node = freelist.Alloc(1);
    node->Init();
    // node->key = "ばっぐ";
    // node->value = "バッグ";
    node->key = "\xE3\x81\xB0\xE3\x81\xA3\xE3\x81\x90";
    node->value = "\xE3\x83\x90\xE3\x83\x83\xE3\x82\xB0";
    results.push_back(DictionaryPredictor::Result(
        node, DictionaryPredictor::UNIGRAM));

    node = freelist.Alloc(1);
    node->Init();
    // node->key = "ばっく";
    // node->value = "バック";
    node->key = "\xE3\x81\xB0\xE3\x81\xA3\xE3\x81\x8F";
    node->value = "\xE3\x83\x90\xE3\x83\x83\xE3\x82\xAF";
    results.push_back(DictionaryPredictor::Result(
        node, DictionaryPredictor::UNIGRAM));

    predictor.RemoveMissSpelledCandidates(1, &results);
    CHECK_EQ(3, results.size());

    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION,
              results[0].type);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM,
              results[1].type);
    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION,
              results[2].type);
  }

  {
    vector<DictionaryPredictor::Result> results;
    DictionaryPredictor::Result result;
    node = freelist.Alloc(1);
    node->Init();
    // node->key = "ばっく";
    // node->value = "バッグ";
    node->key = "\xE3\x81\xB0\xE3\x81\xA3\xE3\x81\x8F";
    node->value = "\xE3\x83\x90\xE3\x83\x83\xE3\x82\xB0";
    node->attributes = Node::SPELLING_CORRECTION;
    results.push_back(DictionaryPredictor::Result(
        node, DictionaryPredictor::UNIGRAM));

    node = freelist.Alloc(1);
    node->Init();
    // node->key = "てすと";
    // node->value = "テスト";
    node->key = "\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8";
    node->value = "\xE3\x83\x86\xE3\x82\xB9\xE3\x83\x88";
    results.push_back(DictionaryPredictor::Result(
        node, DictionaryPredictor::UNIGRAM));

    predictor.RemoveMissSpelledCandidates(1, &results);
    CHECK_EQ(2, results.size());

    EXPECT_EQ(DictionaryPredictor::UNIGRAM,
              results[0].type);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM,
              results[1].type);
  }

  {
    vector<DictionaryPredictor::Result> results;
    DictionaryPredictor::Result result;
    node = freelist.Alloc(1);
    node->Init();
    // node->key = "ばっく";
    // node->value = "バッグ";
    node->key = "\xE3\x81\xB0\xE3\x81\xA3\xE3\x81\x8F";
    node->value = "\xE3\x83\x90\xE3\x83\x83\xE3\x82\xB0";
    node->attributes = Node::SPELLING_CORRECTION;
    results.push_back(DictionaryPredictor::Result(
        node, DictionaryPredictor::UNIGRAM));

    node = freelist.Alloc(1);
    node->Init();
    // node->key = "ばっく";
    // node->value = "バック";
    node->key = "\xE3\x81\xB0\xE3\x81\xA3\xE3\x81\x8F";
    node->value = "\xE3\x83\x90\xE3\x83\x83\xE3\x82\xAF";
    results.push_back(DictionaryPredictor::Result(
        node, DictionaryPredictor::UNIGRAM));

    predictor.RemoveMissSpelledCandidates(1, &results);
    CHECK_EQ(2, results.size());

    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION,
              results[0].type);
    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION,
              results[1].type);
  }

  {
    vector<DictionaryPredictor::Result> results;
    DictionaryPredictor::Result result;
    node = freelist.Alloc(1);
    node->Init();
    // node->key = "ばっく";
    // node->value = "バッグ";
    node->key = "\xE3\x81\xB0\xE3\x81\xA3\xE3\x81\x8F";
    node->value = "\xE3\x83\x90\xE3\x83\x83\xE3\x82\xB0";
    node->attributes = Node::SPELLING_CORRECTION;
    results.push_back(DictionaryPredictor::Result(
        node, DictionaryPredictor::UNIGRAM));

    node = freelist.Alloc(1);
    node->Init();
    // node->key = "ばっく";
    // node->value = "バック";
    node->key = "\xE3\x81\xB0\xE3\x81\xA3\xE3\x81\x8F";
    node->value = "\xE3\x83\x90\xE3\x83\x83\xE3\x82\xAF";
    results.push_back(DictionaryPredictor::Result(
        node, DictionaryPredictor::UNIGRAM));

    predictor.RemoveMissSpelledCandidates(3, &results);
    CHECK_EQ(2, results.size());

    EXPECT_EQ(DictionaryPredictor::UNIGRAM,
              results[0].type);
    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION,
              results[1].type);
  }
}

TEST_F(DictionaryPredictorTest, LookupKeyValueFromDictionary) {
  Segments segments;
  DictionaryPredictor predictor;
  NodeAllocator allocator;

  // "てすと/テスト"
  EXPECT_TRUE(NULL != predictor.LookupKeyValueFromDictionary(
      "\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8",
      "\xE3\x83\x86\xE3\x82\xB9\xE3\x83\x88",
      &allocator));

  // "て/テ"
  EXPECT_TRUE(NULL == predictor.LookupKeyValueFromDictionary(
      "\xE3\x81\xA6",
      "\xE3\x83\x86",
      &allocator));
}

namespace {
void InsertInputSequence(const string &text, composer::Composer *composer) {
  const char *begin = text.data();
  const char *end = text.data() + text.size();
  size_t mblen = 0;

  while (begin < end) {
    commands::KeyEvent key;
    const char32 w = Util::UTF8ToUCS4(begin, end, &mblen);
    if (Util::GetCharacterSet(w) == Util::ASCII) {
      key.set_key_code(*begin);
    } else {
      key.set_key_code('?');
      key.set_key_string(string(begin, mblen));
    }
    begin += mblen;
    composer->InsertCharacterKeyEvent(key);
  }
}

class TestableDictionaryPredictor : public DictionaryPredictor {
  // Test-only subclass: Just changing access levels
 public:
  using DictionaryPredictor::NO_PREDICTION;
  using DictionaryPredictor::UNIGRAM;
  using DictionaryPredictor::BIGRAM;
  using DictionaryPredictor::REALTIME;
  using DictionaryPredictor::SUFFIX;
  using DictionaryPredictor::Result;
  using DictionaryPredictor::MakeResult;
  using DictionaryPredictor::AggregateUnigramPrediction;
  using DictionaryPredictor::AggregateBigramPrediction;
  using DictionaryPredictor::AggregateSuffixPrediction;
  using DictionaryPredictor::ApplyPenaltyForKeyExpansion;
};

void ExpansionForUnigramTestHelper(bool use_expansion) {
  config::Config config;
  config.set_use_dictionary_suggest(true);
  config.set_use_realtime_conversion(false);
  config::ConfigHandler::SetConfig(config);

  scoped_ptr<CallCheckDictionary> check_dictionary(new CallCheckDictionary);
  DictionaryFactory::SetDictionary(check_dictionary.get());

  composer::Table table;
  table.LoadFromFile("system://romanji-hiragana.tsv");
  TestableDictionaryPredictor predictor;
  NodeAllocator allocator;

  {
    Segments segments;
    segments.set_request_type(Segments::PREDICTION);
    composer::Composer composer;
    composer.SetTableForUnittest(&table);
    InsertInputSequence("gu-g", &composer);
    segments.set_composer(&composer);
    Segment *segment = segments.add_segment();
    CHECK(segment);
    string query;
    composer.GetQueryForPrediction(&query);
    segment->set_key(query);

    if (use_expansion) {
      EXPECT_CALL(*check_dictionary.get(),
                  LookupPredictiveWithLimit(_, _, _, _));
    } else {
      EXPECT_CALL(*check_dictionary.get(),
                  LookupPredictive(_, _, _));
    }

    vector<TestableDictionaryPredictor::Result> results;
    predictor.AggregateUnigramPrediction(TestableDictionaryPredictor::UNIGRAM,
                                         &segments, &allocator, &results);
  }
}

void ExpansionForBigramTestHelper(bool use_expansion) {
  config::Config config;
  config.set_use_dictionary_suggest(true);
  config.set_use_realtime_conversion(false);
  config::ConfigHandler::SetConfig(config);

  scoped_ptr<CallCheckDictionary> check_dictionary(new CallCheckDictionary);
  DictionaryFactory::SetDictionary(check_dictionary.get());
  composer::Table table;
  table.LoadFromFile("system://romanji-hiragana.tsv");
  TestableDictionaryPredictor predictor;
  NodeAllocator allocator;

  {
    Segments segments;
    segments.set_request_type(Segments::PREDICTION);
    // History segment's key and value should be in the dictionary
    Segment *segment = segments.add_segment();
    CHECK(segment);
    segment->set_segment_type(Segment::HISTORY);
    // "ぐーぐる"
    segment->set_key("\xe3\x81\x90\xe3\x83\xbc\xe3\x81\x90\xe3\x82\x8b");
    Segment::Candidate *cand = segment->add_candidate();
    // "ぐーぐる"
    cand->key = "\xe3\x81\x90\xe3\x83\xbc\xe3\x81\x90\xe3\x82\x8b";
    // "ぐーぐる"
    cand->content_key = "\xe3\x81\x90\xe3\x83\xbc\xe3\x81\x90\xe3\x82\x8b";
    // "グーグル"
    cand->value = "\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83\xab";
    // "グーグル"
    cand->content_value = "\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83\xab";

    segment = segments.add_segment();
    CHECK(segment);

    composer::Composer composer;
    composer.SetTableForUnittest(&table);
    InsertInputSequence("m", &composer);
    segments.set_composer(&composer);
    string query;
    composer.GetQueryForPrediction(&query);
    segment->set_key(query);

    Node return_node_for_history;
    // "ぐーぐる"
    return_node_for_history.key =
        "\xe3\x81\x90\xe3\x83\xbc\xe3\x81\x90\xe3\x82\x8b";
    // "グーグル"
    return_node_for_history.value =
        "\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83\xab";
    return_node_for_history.lid = 1;
    return_node_for_history.rid = 1;
    // history key and value should be in the dictionary
    EXPECT_CALL(*check_dictionary.get(),
                LookupPrefix(_, _, _))
        .WillOnce(Return(&return_node_for_history));
    if (use_expansion) {
      EXPECT_CALL(*check_dictionary.get(),
                  LookupPredictiveWithLimit(_, _, _, _));
    } else {
      EXPECT_CALL(*check_dictionary.get(),
                  LookupPredictive(_, _, _));
    }

    vector<TestableDictionaryPredictor::Result> results;
    predictor.AggregateBigramPrediction(TestableDictionaryPredictor::BIGRAM,
                                        &segments, &allocator, &results);
  }
}

void ExpansionForSuffixTestHelper(bool use_expansion) {
  config::Config config;
  config.set_use_dictionary_suggest(true);
  config.set_use_realtime_conversion(false);
  config::ConfigHandler::SetConfig(config);

  scoped_ptr<CallCheckDictionary> check_dictionary(new CallCheckDictionary);
  SuffixDictionaryFactory::SetSuffixDictionary(check_dictionary.get());

  composer::Table table;
  table.LoadFromFile("system://romanji-hiragana.tsv");
  TestableDictionaryPredictor predictor;
  NodeAllocator allocator;

  {
    Segments segments;
    segments.set_request_type(Segments::PREDICTION);
    Segment *segment = segments.add_segment();
    CHECK(segment);

    composer::Composer composer;
    composer.SetTableForUnittest(&table);
    InsertInputSequence("des", &composer);
    segments.set_composer(&composer);
    string query;
    composer.GetQueryForPrediction(&query);
    segment->set_key(query);

    if (use_expansion) {
      EXPECT_CALL(*check_dictionary.get(),
                  LookupPredictiveWithLimit(_, _, _, _));
    } else {
      EXPECT_CALL(*check_dictionary.get(),
                  LookupPredictive(_, _, _));
    }

    vector<TestableDictionaryPredictor::Result> results;
    predictor.AggregateSuffixPrediction(TestableDictionaryPredictor::SUFFIX,
                                        &segments, &allocator, &results);
  }
}
}  // namespace

TEST_F(DictionaryPredictorTest, UseExpansionForUnigramTest) {
  FLAGS_enable_expansion_for_dictionary_predictor = true;
  ExpansionForUnigramTestHelper(true);
}

TEST_F(DictionaryPredictorTest, UnuseExpansionForUnigramTest) {
  FLAGS_enable_expansion_for_dictionary_predictor = false;
  ExpansionForUnigramTestHelper(false);
}

TEST_F(DictionaryPredictorTest, UseExpansionForBigramTest) {
  FLAGS_enable_expansion_for_dictionary_predictor = true;
  ExpansionForBigramTestHelper(true);
}

TEST_F(DictionaryPredictorTest, UnuseExpansionForBigramTest) {
  FLAGS_enable_expansion_for_dictionary_predictor = false;
  ExpansionForBigramTestHelper(false);
}

TEST_F(DictionaryPredictorTest, UseExpansionForSuffixTest) {
  FLAGS_enable_expansion_for_dictionary_predictor = true;
  ExpansionForSuffixTestHelper(true);
}

TEST_F(DictionaryPredictorTest, UnuseExpansionForSuffixTest) {
  FLAGS_enable_expansion_for_dictionary_predictor = false;
  ExpansionForSuffixTestHelper(false);
}

TEST_F(DictionaryPredictorTest, ExpansionPenaltyForRomanTest) {
  DictionaryFactory::SetDictionary(GetMockDic());
  FLAGS_enable_expansion_for_dictionary_predictor = true;
  config::Config config;
  config.set_use_dictionary_suggest(true);
  config.set_use_realtime_conversion(false);
  config::ConfigHandler::SetConfig(config);

  composer::Table table;
  table.LoadFromFile("system://romanji-hiragana.tsv");
  TestableDictionaryPredictor predictor;
  NodeAllocator allocator;

  Segments segments;
  segments.set_request_type(Segments::PREDICTION);
  composer::Composer composer;
  composer.SetTableForUnittest(&table);
  InsertInputSequence("ak", &composer);
  segments.set_composer(&composer);
  Segment *segment = segments.add_segment();
  CHECK(segment);
  {
    string query;
    composer.GetQueryForPrediction(&query);
    segment->set_key(query);
    // "あ"
    EXPECT_EQ("\xe3\x81\x82", query);
  }
  {
    string base;
    set<string> expanded;
    composer.GetQueriesForPrediction(&base, &expanded);
    // "あ"
    EXPECT_EQ("\xe3\x81\x82", base);
    EXPECT_GT(expanded.size(), 5);
  }

  vector<TestableDictionaryPredictor::Result> results;
  Node node1;
  // "あか"
  node1.key = "\xe3\x81\x82\xe3\x81\x8b";
  // "赤"
  node1.value = "\xe8\xb5\xa4";
  results.push_back(TestableDictionaryPredictor::MakeResult(
      &node1, TestableDictionaryPredictor::UNIGRAM));
  Node node2;
  // "あき"
  node2.key = "\xe3\x81\x82\xe3\x81\x8d";
  // "秋"
  node2.value = "\xe7\xa7\x8b";
  results.push_back(TestableDictionaryPredictor::MakeResult(
      &node2, TestableDictionaryPredictor::UNIGRAM));
  Node node3;
  // "あかぎ"
  node3.key = "\xe3\x81\x82\xe3\x81\x8b\xe3\x81\x8e";
  // "アカギ"
  node3.value = "\xe3\x82\xa2\xe3\x82\xab\xe3\x82\xae";
  results.push_back(TestableDictionaryPredictor::MakeResult(
      &node3, TestableDictionaryPredictor::UNIGRAM));

  EXPECT_EQ(3, results.size());
  EXPECT_EQ(0, results[0].cost);
  EXPECT_EQ(0, results[1].cost);
  EXPECT_EQ(0, results[2].cost);

  predictor.ApplyPenaltyForKeyExpansion(segments, &results);

  // no penalties
  EXPECT_EQ(0, results[0].cost);
  EXPECT_EQ(0, results[1].cost);
  EXPECT_EQ(0, results[2].cost);
}

TEST_F(DictionaryPredictorTest, ExpansionPenaltyForKanaTest) {
  DictionaryFactory::SetDictionary(GetMockDic());
  FLAGS_enable_expansion_for_dictionary_predictor = true;
  config::Config config;
  config.set_use_dictionary_suggest(true);
  config.set_use_realtime_conversion(false);
  config::ConfigHandler::SetConfig(config);

  composer::Table table;
  table.LoadFromFile("system://kana.tsv");
  TestableDictionaryPredictor predictor;
  NodeAllocator allocator;

  Segments segments;
  segments.set_request_type(Segments::PREDICTION);
  composer::Composer composer;
  composer.SetTableForUnittest(&table);
  // "あし"
  InsertInputSequence("\xe3\x81\x82\xe3\x81\x97", &composer);
  segments.set_composer(&composer);
  Segment *segment = segments.add_segment();
  CHECK(segment);
  {
    string query;
    composer.GetQueryForPrediction(&query);
    segment->set_key(query);
    // "あし"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x97", query);
  }
  {
    string base;
    set<string> expanded;
    composer.GetQueriesForPrediction(&base, &expanded);
    // "あ"
    EXPECT_EQ("\xe3\x81\x82", base);
    EXPECT_EQ(2, expanded.size());
  }

  vector<TestableDictionaryPredictor::Result> results;
  Node node1;
  // "あし"
  node1.key = "\xe3\x81\x82\xe3\x81\x97";
  // "足"
  node1.value = "\xe8\xb6\xb3";
  results.push_back(TestableDictionaryPredictor::MakeResult(
      &node1, TestableDictionaryPredictor::UNIGRAM));
  Node node2;
  // "あじ"
  node2.key = "\xe3\x81\x82\xe3\x81\x98";
  // "味"
  node2.value = "\xe5\x91\xb3";
  results.push_back(TestableDictionaryPredictor::MakeResult(
      &node2, TestableDictionaryPredictor::UNIGRAM));
  Node node3;
  // "あした"
  node3.key = "\xe3\x81\x82\xe3\x81\x97\xe3\x81\x9f";
  // "明日"
  node3.value = "\xe6\x98\x8e\xe6\x97\xa5";
  results.push_back(TestableDictionaryPredictor::MakeResult(
      &node3, TestableDictionaryPredictor::UNIGRAM));
  Node node4;
  // "あじあ"
  node4.key = "\xe3\x81\x82\xe3\x81\x98\xe3\x81\x82";
  // "アジア"
  node4.value = "\xe3\x82\xa2\xe3\x82\xb8\xe3\x82\xa2";
  results.push_back(TestableDictionaryPredictor::MakeResult(
      &node4, TestableDictionaryPredictor::UNIGRAM));

  EXPECT_EQ(4, results.size());
  EXPECT_EQ(0, results[0].cost);
  EXPECT_EQ(0, results[1].cost);
  EXPECT_EQ(0, results[2].cost);
  EXPECT_EQ(0, results[3].cost);

  predictor.ApplyPenaltyForKeyExpansion(segments, &results);

  EXPECT_EQ(0, results[0].cost);
  EXPECT_LT(0, results[1].cost);
  EXPECT_EQ(0, results[2].cost);
  EXPECT_LT(0, results[3].cost);
}

TEST_F(DictionaryPredictorTest, SetLMCost) {
  TestableDictionaryPredictor predictor;

  Segments segments;
  segments.set_request_type(Segments::PREDICTION);
  Segment *segment = segments.add_segment();
  CHECK(segment);
  // "てすと"
  segment->set_key("\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8");

  vector<TestableDictionaryPredictor::Result> results;
  Node node1;
  // "てすと"
  node1.key = "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8";
  // "てすと"
  node1.value = "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8";
  results.push_back(TestableDictionaryPredictor::MakeResult(
      &node1, TestableDictionaryPredictor::UNIGRAM));
  Node node2;
  // "てすと"
  node2.key = "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8";
  // "テスト"
  node2.value = "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88";
  results.push_back(TestableDictionaryPredictor::MakeResult(
      &node2, TestableDictionaryPredictor::UNIGRAM));
  Node node3;
  // "てすとてすと"
  node3.key = "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8"
      "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8";
  // "テストテスト"
  node3.value = "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88"
      "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88";
  results.push_back(TestableDictionaryPredictor::MakeResult(
      &node3, TestableDictionaryPredictor::UNIGRAM));

  predictor.SetLMCost(segments, &results);

  EXPECT_EQ(3, results.size());
  // "てすと"
  EXPECT_EQ("\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8", results[0].node->value);
  // "テスト"
  EXPECT_EQ("\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88", results[1].node->value);
  // "テストテスト"
  EXPECT_EQ("\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88"
            "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88", results[2].node->value);
  EXPECT_GT(results[2].cost, results[0].cost);
  EXPECT_GT(results[2].cost, results[1].cost);
}
}  // namespace mozc
