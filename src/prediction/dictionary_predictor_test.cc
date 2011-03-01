// Copyright 2010, Google Inc.
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

#include "base/util.h"
#include "converter/converter_interface.h"
#include "converter/converter_mock.h"
#include "converter/segments.h"
#include "session/commands.pb.h"
#include "session/config.pb.h"
#include "session/config_handler.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"


DECLARE_string(test_tmpdir);

namespace mozc {
namespace {

class DictionaryPredictorTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::ConfigHandler::GetDefaultConfig(&default_config_);
    config::ConfigHandler::SetConfig(default_config_);
  }

 private:
  config::Config default_config_;
};

void MakeSegmentsForSuggestion(const string key,
                               Segments *segments) {
  segments->Clear();
  segments->set_max_prediction_candidates_size(10);
  segments->set_request_type(Segments::SUGGESTION);
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FIXED_VALUE);
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

TEST_F(DictionaryPredictorTest, GetPredictionTypeTest) {
  Segments segments;
  config::Config config;
  config.set_use_dictionary_suggest(true);
  config::ConfigHandler::SetConfig(config);

  // empty segments
  {
    EXPECT_EQ(
        DictionaryPredictor::NO_PREDICTION,
        DictionaryPredictor::GetPredictionType(segments));
  }

  // normal segments
  {
    // "てすとだよ"
    MakeSegmentsForSuggestion("\xE3\x81\xA6\xE3\x81\x99\xE3"
                              "\x81\xA8\xE3\x81\xA0\xE3\x82\x88",
                              &segments);
    EXPECT_EQ(
        DictionaryPredictor::UNIGRAM,
        DictionaryPredictor::GetPredictionType(segments));

    segments.set_request_type(Segments::PREDICTION);
    EXPECT_EQ(
        DictionaryPredictor::UNIGRAM,
        DictionaryPredictor::GetPredictionType(segments));

    segments.set_request_type(Segments::CONVERSION);
    EXPECT_EQ(
        DictionaryPredictor::NO_PREDICTION,
        DictionaryPredictor::GetPredictionType(segments));
  }

  // short key
  {
    // "てす"
    MakeSegmentsForSuggestion("\xE3\x81\xA6\xE3\x81\x99",
                              &segments);
    EXPECT_EQ(
        DictionaryPredictor::NO_PREDICTION,
        DictionaryPredictor::GetPredictionType(segments));

    // on prediction mode, return UNIGRAM
    segments.set_request_type(Segments::PREDICTION);
    EXPECT_EQ(
        DictionaryPredictor::UNIGRAM,
        DictionaryPredictor::GetPredictionType(segments));
  }

  // zipcode-like key
  {
    MakeSegmentsForSuggestion("0123", &segments);
    EXPECT_EQ(
        DictionaryPredictor::NO_PREDICTION,
        DictionaryPredictor::GetPredictionType(segments));
  }

  // History is short => UNIGRAM
  {
    // "てすとだよ"
    MakeSegmentsForSuggestion("\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8"
                              "\xE3\x81\xA0\xE3\x82\x88", &segments);
    PrependHistorySegments("A", "A", &segments);
    EXPECT_EQ(
        DictionaryPredictor::UNIGRAM,
        DictionaryPredictor::GetPredictionType(segments));
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
        DictionaryPredictor::GetPredictionType(segments));
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
        DictionaryPredictor::GetPredictionType(segments));
  }
}


TEST_F(DictionaryPredictorTest, IsZipCodeRequestTest) {
  EXPECT_FALSE(DictionaryPredictor::IsZipCodeRequest(""));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("000"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("000"));
  EXPECT_FALSE(DictionaryPredictor::IsZipCodeRequest("ABC"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("---"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("0124-"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("0124-0"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("012-0"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("012-3456"));
  // "０１２-０"
  EXPECT_FALSE(DictionaryPredictor::IsZipCodeRequest(
      "\xef\xbc\x90\xef\xbc\x91\xef\xbc\x92\x2d\xef\xbc\x90"));
}

TEST_F(DictionaryPredictorTest, GetSVMScoreTest) {
  vector<pair<int, double> > feature;

  EXPECT_EQ(INT_MIN, DictionaryPredictor::GetSVMScore(
      // "ただしい",
      // "ただしいけめんにかぎる",
      // "ただしイケメンに限る",
      "\xE3\x81\x9F\xE3\x81\xA0\xE3\x81\x97\xE3\x81\x84",
      "\xE3\x81\x9F\xE3\x81\xA0\xE3\x81\x97\xE3\x81\x84"
      "\xE3\x81\x91\xE3\x82\x81\xE3\x82\x93\xE3\x81\xAB"
      "\xE3\x81\x8B\xE3\x81\x8E\xE3\x82\x8B",
      "\xE3\x81\x9F\xE3\x81\xA0\xE3\x81\x97\xE3\x82\xA4"
      "\xE3\x82\xB1\xE3\x83\xA1\xE3\x83\xB3\xE3\x81\xAB"
      "\xE9\x99\x90\xE3\x82\x8B",
      6000,
      0,
      false,
      true,
      20,
      &feature));

  // cost <= 4000
  EXPECT_NE(INT_MIN, DictionaryPredictor::GetSVMScore(
      // "ただしい",
      // "ただしいけめんにかぎる",
      // "ただしイケメンに限る",
      "\xE3\x81\x9F\xE3\x81\xA0\xE3\x81\x97\xE3\x81\x84",
      "\xE3\x81\x9F\xE3\x81\xA0\xE3\x81\x97\xE3\x81\x84"
      "\xE3\x81\x91\xE3\x82\x81\xE3\x82\x93\xE3\x81\xAB"
      "\xE3\x81\x8B\xE3\x81\x8E\xE3\x82\x8B",
      "\xE3\x81\x9F\xE3\x81\xA0\xE3\x81\x97\xE3\x82\xA4"
      "\xE3\x82\xB1\xE3\x83\xA1\xE3\x83\xB3\xE3\x81\xAB"
      "\xE9\x99\x90\xE3\x82\x8B",
      4000,
      0,
      false,
      true,
      20,
      &feature));

  // not suggestion
  EXPECT_NE(INT_MIN, DictionaryPredictor::GetSVMScore(
      // "ただしい",
      // "ただしいけめんにかぎる",
      // "ただしイケメンに限る",
      "\xE3\x81\x9F\xE3\x81\xA0\xE3\x81\x97\xE3\x81\x84",
      "\xE3\x81\x9F\xE3\x81\xA0\xE3\x81\x97\xE3\x81\x84"
      "\xE3\x81\x91\xE3\x82\x81\xE3\x82\x93\xE3\x81\xAB"
      "\xE3\x81\x8B\xE3\x81\x8E\xE3\x82\x8B",
      "\xE3\x81\x9F\xE3\x81\xA0\xE3\x81\x97\xE3\x82\xA4"
      "\xE3\x82\xB1\xE3\x83\xA1\xE3\x83\xB3\xE3\x81\xAB"
      "\xE9\x99\x90\xE3\x82\x8B",
      6000,
      0,
      false,
      false,
      20,
      &feature));

  // total_candidates_size is small
  EXPECT_NE(INT_MIN, DictionaryPredictor::GetSVMScore(
      // "ただしい",
      // "ただしいけめんにかぎる",
      // "ただしイケメンに限る",
      "\xE3\x81\x9F\xE3\x81\xA0\xE3\x81\x97\xE3\x81\x84",
      "\xE3\x81\x9F\xE3\x81\xA0\xE3\x81\x97\xE3\x81\x84"
      "\xE3\x81\x91\xE3\x82\x81\xE3\x82\x93\xE3\x81\xAB"
      "\xE3\x81\x8B\xE3\x81\x8E\xE3\x82\x8B",
      "\xE3\x81\x9F\xE3\x81\xA0\xE3\x81\x97\xE3\x82\xA4"
      "\xE3\x82\xB1\xE3\x83\xA1\xE3\x83\xB3\xE3\x81\xAB"
      "\xE9\x99\x90\xE3\x82\x8B",
      6000,
      0,
      false,
      true,
      5,
      &feature));

  EXPECT_NE(INT_MIN, DictionaryPredictor::GetSVMScore(
      // "ただしい",
      // "ただしいけめんにかぎる",
      // "ただしイケメンに限る",
      "\xE3\x81\x9F\xE3\x81\xA0\xE3\x81\x97\xE3\x81\x84\xE3\x81\x91",
      "\xE3\x81\x9F\xE3\x81\xA0\xE3\x81\x97\xE3\x81\x84"
      "\xE3\x81\x91\xE3\x82\x81\xE3\x82\x93\xE3\x81\xAB"
      "\xE3\x81\x8B\xE3\x81\x8E\xE3\x82\x8B",
      "\xE3\x81\x9F\xE3\x81\xA0\xE3\x81\x97\xE3\x82\xA4"
      "\xE3\x82\xB1\xE3\x83\xA1\xE3\x83\xB3\xE3\x81\xAB"
      "\xE9\x99\x90\xE3\x82\x8B",
      6000,
      0,
      false,
      true,
      20,
      &feature));

  EXPECT_EQ(INT_MIN, DictionaryPredictor::GetSVMScore(
      // "それでも",
      // "それでもぼくはやっていない",
      // "それでもボクはやってない",
      "\xE3\x81\x9D\xE3\x82\x8C\xE3\x81\xA7\xE3\x82\x82",
      "\xE3\x81\x9D\xE3\x82\x8C\xE3\x81\xA7\xE3\x82\x82"
      "\xE3\x81\xBC\xE3\x81\x8F\xE3\x81\xAF\xE3\x82\x84"
      "\xE3\x81\xA3\xE3\x81\xA6\xE3\x81\x84\xE3\x81\xAA\xE3\x81\x84",
      "\xE3\x81\x9D\xE3\x82\x8C\xE3\x81\xA7\xE3\x82\x82"
      "\xE3\x83\x9C\xE3\x82\xAF\xE3\x81\xAF\xE3\x82\x84"
      "\xE3\x81\xA3\xE3\x81\xA6\xE3\x81\xAA\xE3\x81\x84",
      6000,
      0,
      false,
      true,
      20,
      &feature));

  // cost <= 4000
  EXPECT_NE(INT_MIN, DictionaryPredictor::GetSVMScore(
      // "それでも",
      // "それでもぼくはやっていない",
      // "それでもボクはやってない",
      "\xE3\x81\x9D\xE3\x82\x8C\xE3\x81\xA7\xE3\x82\x82",
      "\xE3\x81\x9D\xE3\x82\x8C\xE3\x81\xA7\xE3\x82\x82"
      "\xE3\x81\xBC\xE3\x81\x8F\xE3\x81\xAF\xE3\x82\x84"
      "\xE3\x81\xA3\xE3\x81\xA6\xE3\x81\x84\xE3\x81\xAA\xE3\x81\x84",
      "\xE3\x81\x9D\xE3\x82\x8C\xE3\x81\xA7\xE3\x82\x82"
      "\xE3\x83\x9C\xE3\x82\xAF\xE3\x81\xAF\xE3\x82\x84"
      "\xE3\x81\xA3\xE3\x81\xA6\xE3\x81\xAA\xE3\x81\x84",
      3000,
      0,
      false,
      true,
      20,
      &feature));
}
}  // namespace
}  // mozc
