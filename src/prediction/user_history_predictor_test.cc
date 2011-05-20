// Copyright 2010-2011, Google Inc.
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

#include "prediction/user_history_predictor.h"

#include "base/password_manager.h"
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

class UserHistoryPredictorTest : public testing::Test {
 protected:
  virtual void SetUp() {
    kUseMockPasswordManager = true;
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::ConfigHandler::GetDefaultConfig(&default_config_);
    config::ConfigHandler::SetConfig(default_config_);
  }

  virtual void TearDown() {
    config::ConfigHandler::SetConfig(default_config_);
  }

  virtual void EnableZeroQuerySuggestion() {
  }

  virtual void DisableZeroQuerySuggestion() {
  }

 private:
  config::Config default_config_;
};

void MakeSegmentsForSuggestion(const string key,
                               Segments *segments) {
  segments->set_max_prediction_candidates_size(10);
  segments->set_request_type(Segments::SUGGESTION);
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FIXED_VALUE);
}

void MakeSegmentsForPrediction(const string key,
                               Segments *segments) {
  segments->set_max_prediction_candidates_size(10);
  segments->set_request_type(Segments::PREDICTION);
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FIXED_VALUE);
}

void MakeSegmentsForConversion(const string key,
                               Segments *segments) {
  segments->set_request_type(Segments::CONVERSION);
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FIXED_VALUE);
}

void AddCandidate(size_t index, const string &value, Segments *segments) {
  Segment::Candidate *candidate =
      segments->mutable_segment(index)->add_candidate();
  CHECK(candidate);
  candidate->Init();
  candidate->value = value;
  candidate->content_value = value;
  candidate->key = segments->segment(index).key();
  candidate->content_key = segments->segment(index).key();
}

void AddCandidateWithDescription(size_t index,
                                 const string &value,
                                 const string &desc,
                                 Segments *segments) {
  Segment::Candidate *candidate =
      segments->mutable_segment(index)->add_candidate();
  CHECK(candidate);
  candidate->Init();
  candidate->value = value;
  candidate->content_value = value;
  candidate->key = segments->segment(index).key();
  candidate->content_key = segments->segment(index).key();
  candidate->description = desc;
}

void AddCandidate(const string &value, Segments *segments) {
  AddCandidate(0, value, segments);
}

void AddCandidateWithDescription(const string &value,
                                 const string &desc,
                                 Segments *segments) {
  AddCandidateWithDescription(0, value, desc, segments);
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

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorTest) {
  {
    UserHistoryPredictor predictor;
    predictor.WaitForSyncer();

    // Nothing happen
    {
      Segments segments;
      // "わたしのなまえはなかのです"
      MakeSegmentsForSuggestion(
          "\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8", &segments);
      EXPECT_FALSE(predictor.Predict(&segments));
      EXPECT_EQ(segments.segment(0).candidates_size(), 0);
    }

    // Nothing happen
    {
      Segments segments;
      // "てすと"
      MakeSegmentsForPrediction(
          "\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8", &segments);
      EXPECT_FALSE(predictor.Predict(&segments));
      EXPECT_EQ(segments.segment(0).candidates_size(), 0);
    }

    // Insert two items
    {
      Segments segments;
      // "わたしのなまえはなかのです"
      MakeSegmentsForConversion
          ("\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE"
           "\xE3\x81\xAA\xE3\x81\xBE\xE3\x81\x88\xE3\x81\xAF"
           "\xE3\x81\xAA\xE3\x81\x8B\xE3\x81\xAE\xE3\x81\xA7"
           "\xE3\x81\x99", &segments);
      // "私の名前は中野です"
      AddCandidate(
          "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
          "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7"
          "\xE3\x81\x99", &segments);
      predictor.Finish(&segments);

      // "わたしの"
      MakeSegmentsForSuggestion(
          "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
      EXPECT_TRUE(predictor.Predict(&segments));
      // "私の名前は中野です"
      EXPECT_EQ(segments.segment(0).candidate(0).value,
                "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
                "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7"
                "\xE3\x81\x99");

      segments.Clear();
      // "わたしの"
      MakeSegmentsForPrediction(
          "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
      EXPECT_TRUE(predictor.Predict(&segments));
      // "私の名前は中野です"
      EXPECT_EQ(segments.segment(0).candidate(0).value,
                "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
                "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7"
                "\xE3\x81\x99");
    }

    // sync
    predictor.Sync();
    Util::Sleep(500);
  }

  // reload
  {
    UserHistoryPredictor predictor;
    predictor.WaitForSyncer();
    Segments segments;

    // turn off
    {
      Segments segments;
      config::Config config;
      config.set_use_history_suggest(false);
      config::ConfigHandler::SetConfig(config);

      // "わたしの"
      MakeSegmentsForSuggestion(
          "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
      EXPECT_FALSE(predictor.Predict(&segments));

      config.set_use_history_suggest(true);
      config.set_incognito_mode(true);
      config::ConfigHandler::SetConfig(config);

      // "わたしの"
      MakeSegmentsForSuggestion(
          "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
      EXPECT_FALSE(predictor.Predict(&segments));
    }

    // turn on
    {
      config::Config config;
      config::ConfigHandler::SetConfig(config);
    }

    // reproducesd
    // "わたしの"
    MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_TRUE(predictor.Predict(&segments));
    // "私の名前は中野です"
    EXPECT_EQ(segments.segment(0).candidate(0).value,
              "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99");

    segments.Clear();
    // "わたしの"
    MakeSegmentsForPrediction(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_TRUE(predictor.Predict(&segments));
    // "私の名前は中野です"
    EXPECT_EQ(segments.segment(0).candidate(0).value,
              "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99");

    // Exact Match
    segments.Clear();
    // "わたしのなまえはなかのです"
    MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE"
        "\xE3\x81\xAA\xE3\x81\xBE\xE3\x81\x88\xE3\x81\xAF"
        "\xE3\x81\xAA\xE3\x81\x8B\xE3\x81\xAE\xE3\x81\xA7\xE3\x81\x99",
        &segments);
    EXPECT_TRUE(predictor.Predict(&segments));
    // "私の名前は中野です"
    EXPECT_EQ(segments.segment(0).candidate(0).value,
              "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99");

    segments.Clear();
    // "わたしのなまえはなかのです"
    MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE"
        "\xE3\x81\xAA\xE3\x81\xBE\xE3\x81\x88\xE3\x81\xAF\xE3\x81\xAA"
        "\xE3\x81\x8B\xE3\x81\xAE\xE3\x81\xA7\xE3\x81\x99", &segments);
    EXPECT_TRUE(predictor.Predict(&segments));
    // "私の名前は中野です"
    EXPECT_EQ(segments.segment(0).candidate(0).value,
              "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99");

    // clear
    predictor.ClearAllHistory();
    predictor.WaitForSyncer();
  }

  // nothing happen
  {
    UserHistoryPredictor predictor;
    predictor.WaitForSyncer();
    Segments segments;

    // reproducesd
    // "わたしの"
    MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_FALSE(predictor.Predict(&segments));

    // "わたしの"
    MakeSegmentsForPrediction(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_FALSE(predictor.Predict(&segments));
  }

  // nothing happen
  {
    UserHistoryPredictor predictor;
    predictor.WaitForSyncer();
    Segments segments;

    // reproducesd
    // "わたしの"
    MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_FALSE(predictor.Predict(&segments));

    // "わたしの"
    MakeSegmentsForPrediction(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_FALSE(predictor.Predict(&segments));
  }
}

TEST_F(UserHistoryPredictorTest, DescriptionTest) {
  {
    UserHistoryPredictor predictor;
    predictor.WaitForSyncer();

    // Insert two items
    {
      Segments segments;
      // "わたしのなまえはなかのです"
      MakeSegmentsForConversion
          ("\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE"
           "\xE3\x81\xAA\xE3\x81\xBE\xE3\x81\x88\xE3\x81\xAF"
           "\xE3\x81\xAA\xE3\x81\x8B\xE3\x81\xAE\xE3\x81\xA7"
           "\xE3\x81\x99", &segments);
      // "私の名前は中野です"
      AddCandidateWithDescription(
          "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
          "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7"
          "\xE3\x81\x99",
          // "テスト"
          "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88",
          &segments);
      predictor.Finish(&segments);

      // "わたしの"
      MakeSegmentsForSuggestion(
          "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
      EXPECT_TRUE(predictor.Predict(&segments));
      // "私の名前は中野です"
      EXPECT_EQ(segments.segment(0).candidate(0).value,
                "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
                "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7"
                "\xE3\x81\x99");
      EXPECT_EQ(segments.segment(0).candidate(0).description,
                // "テスト"
                "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88");


      segments.Clear();
      // "わたしの"
      MakeSegmentsForPrediction(
          "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
      EXPECT_TRUE(predictor.Predict(&segments));
      // "私の名前は中野です"
      EXPECT_EQ(segments.segment(0).candidate(0).value,
                "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
                "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7"
                "\xE3\x81\x99");
      EXPECT_EQ(segments.segment(0).candidate(0).description,
                // "テスト"
                "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88");
    }

    // sync
    predictor.Sync();
  }

  // reload
  {
    UserHistoryPredictor predictor;
    predictor.WaitForSyncer();
    Segments segments;

    // turn off
    {
      Segments segments;
      config::Config config;
      config.set_use_history_suggest(false);
      config::ConfigHandler::SetConfig(config);
      predictor.WaitForSyncer();

      // "わたしの"
      MakeSegmentsForSuggestion(
          "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
      EXPECT_FALSE(predictor.Predict(&segments));

      config.set_use_history_suggest(true);
      config.set_incognito_mode(true);
      config::ConfigHandler::SetConfig(config);

      // "わたしの"
      MakeSegmentsForSuggestion(
          "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
      EXPECT_FALSE(predictor.Predict(&segments));
    }

    // turn on
    {
      config::Config config;
      config::ConfigHandler::SetConfig(config);
      predictor.WaitForSyncer();
    }

    // reproducesd
    // "わたしの"
    MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_TRUE(predictor.Predict(&segments));
    // "私の名前は中野です"
    EXPECT_EQ(segments.segment(0).candidate(0).value,
              "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99");
    EXPECT_EQ(segments.segment(0).candidate(0).description,
              // "テスト"
              "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88");

    segments.Clear();
    // "わたしの"
    MakeSegmentsForPrediction(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_TRUE(predictor.Predict(&segments));
    // "私の名前は中野です"
    EXPECT_EQ(segments.segment(0).candidate(0).value,
              "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99");
    EXPECT_EQ(segments.segment(0).candidate(0).description,
              // "テスト"
              "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88");

    // Exact Match
    segments.Clear();
    // "わたしのなまえはなかのです"
    MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE"
        "\xE3\x81\xAA\xE3\x81\xBE\xE3\x81\x88\xE3\x81\xAF"
        "\xE3\x81\xAA\xE3\x81\x8B\xE3\x81\xAE\xE3\x81\xA7\xE3\x81\x99",
        &segments);
    EXPECT_TRUE(predictor.Predict(&segments));
    // "私の名前は中野です"
    EXPECT_EQ(segments.segment(0).candidate(0).value,
              "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99");
    EXPECT_EQ(segments.segment(0).candidate(0).description,
              // "テスト"
              "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88");

    segments.Clear();
    // "わたしのなまえはなかのです"
    MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE"
        "\xE3\x81\xAA\xE3\x81\xBE\xE3\x81\x88\xE3\x81\xAF\xE3\x81\xAA"
        "\xE3\x81\x8B\xE3\x81\xAE\xE3\x81\xA7\xE3\x81\x99", &segments);
    EXPECT_TRUE(predictor.Predict(&segments));
    // "私の名前は中野です"
    EXPECT_EQ(segments.segment(0).candidate(0).value,
              "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99");
    EXPECT_EQ(segments.segment(0).candidate(0).description,
              // "テスト"
              "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88");

    // clear
    predictor.ClearAllHistory();
    predictor.WaitForSyncer();
  }

  // nothing happen
  {
    UserHistoryPredictor predictor;
    predictor.WaitForSyncer();
    Segments segments;

    // reproducesd
    // "わたしの"
    MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_FALSE(predictor.Predict(&segments));

    // "わたしの"
    MakeSegmentsForPrediction(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_FALSE(predictor.Predict(&segments));
  }

  // nothing happen
  {
    UserHistoryPredictor predictor;
    predictor.WaitForSyncer();
    Segments segments;

    // reproducesd
    // "わたしの"
    MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_FALSE(predictor.Predict(&segments));

    // "わたしの"
    MakeSegmentsForPrediction(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_FALSE(predictor.Predict(&segments));
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorUnusedHistoryTest) {
  {
    UserHistoryPredictor predictor;
    predictor.WaitForSyncer();

    Segments segments;
    // "わたしのなまえはなかのです"
    MakeSegmentsForConversion
        ("\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE"
         "\xE3\x81\xAA\xE3\x81\xBE\xE3\x81\x88\xE3\x81\xAF"
         "\xE3\x81\xAA\xE3\x81\x8B\xE3\x81\xAE\xE3\x81\xA7"
         "\xE3\x81\x99", &segments);
    // "私の名前は中野です"
    AddCandidate
        ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
         "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7"
         "\xE3\x81\x99", &segments);

    // once
    segments.set_request_type(Segments::SUGGESTION);
    predictor.Finish(&segments);

    segments.Clear();
    // "ひろすえりょうこ"
    MakeSegmentsForConversion
        ("\xE3\x81\xB2\xE3\x82\x8D\xE3\x81\x99\xE3\x81\x88"
         "\xE3\x82\x8A\xE3\x82\x87\xE3\x81\x86\xE3\x81\x93", &segments);
    // "広末涼子"
    AddCandidate("\xE5\xBA\x83\xE6\x9C\xAB\xE6\xB6\xBC\xE5\xAD\x90", &segments);

    segments.set_request_type(Segments::CONVERSION);

    // conversion
    predictor.Finish(&segments);

    // sync
    predictor.Sync();
  }

  {
    UserHistoryPredictor predictor;
    predictor.WaitForSyncer();
    Segments segments;

    // "わたしの"
    MakeSegmentsForSuggestion("\xE3\x82\x8F\xE3\x81\x9F"
                              "\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_TRUE(predictor.Predict(&segments));
    // "私の名前は中野です"
    EXPECT_EQ(segments.segment(0).candidate(0).value,
              "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99");

    segments.Clear();
    // "ひろすえ"
    MakeSegmentsForSuggestion("\xE3\x81\xB2\xE3\x82\x8D"
                              "\xE3\x81\x99\xE3\x81\x88", &segments);
    EXPECT_TRUE(predictor.Predict(&segments));
    // "広末涼子"
    EXPECT_EQ(segments.segment(0).candidate(0).value,
              "\xE5\xBA\x83\xE6\x9C\xAB\xE6\xB6\xBC\xE5\xAD\x90");

    predictor.ClearUnusedHistory();
    predictor.WaitForSyncer();

    segments.Clear();
    // "わたしの"
    MakeSegmentsForSuggestion("\xE3\x82\x8F\xE3\x81\x9F"
                              "\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_TRUE(predictor.Predict(&segments));
    // "私の名前は中野です"
    EXPECT_EQ(segments.segment(0).candidate(0).value,
              "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99");

    segments.Clear();
    // "ひろすえ"
    MakeSegmentsForSuggestion("\xE3\x81\xB2\xE3\x82\x8D"
                              "\xE3\x81\x99\xE3\x81\x88", &segments);
    EXPECT_FALSE(predictor.Predict(&segments));

    predictor.Sync();
  }

  {
    UserHistoryPredictor predictor;
    predictor.WaitForSyncer();
    Segments segments;

    // "わたしの"
    MakeSegmentsForSuggestion("\xE3\x82\x8F\xE3\x81\x9F"
                              "\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_TRUE(predictor.Predict(&segments));
    // "私の名前は中野です"
    EXPECT_EQ(segments.segment(0).candidate(0).value,
              "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99");

    segments.Clear();
    // "ひろすえ"
    MakeSegmentsForSuggestion("\xE3\x81\xB2\xE3\x82\x8D"
                              "\xE3\x81\x99\xE3\x81\x88", &segments);
    EXPECT_FALSE(predictor.Predict(&segments));
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorRevertTest) {
  UserHistoryPredictor predictor;
  predictor.WaitForSyncer();
  predictor.ClearAllHistory();
  predictor.WaitForSyncer();

  Segments segments, segments2;
  // "わたしのなまえはなかのです"
  MakeSegmentsForConversion
      ("\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE"
       "\xE3\x81\xAA\xE3\x81\xBE\xE3\x81\x88\xE3\x81\xAF"
       "\xE3\x81\xAA\xE3\x81\x8B\xE3\x81\xAE\xE3\x81\xA7"
       "\xE3\x81\x99", &segments);
  // "私の名前は中野です"
  AddCandidate(
      "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
      "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7"
      "\xE3\x81\x99", &segments);

  predictor.Finish(&segments);

  // Before Revert, Suggest works
  // "わたしの"
  MakeSegmentsForSuggestion("\xE3\x82\x8F\xE3\x81\x9F"
                            "\xE3\x81\x97\xE3\x81\xAE", &segments2);
  EXPECT_TRUE(predictor.Predict(&segments2));
  // "私の名前は中野です"
  EXPECT_EQ(segments.segment(0).candidate(0).value,
            "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
            "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99");

  // Call revert here
  predictor.Revert(&segments);

  segments.Clear();
  // "わたしの"
  MakeSegmentsForSuggestion("\xE3\x82\x8F\xE3\x81\x9F"
                            "\xE3\x81\x97\xE3\x81\xAE", &segments);

  EXPECT_FALSE(predictor.Predict(&segments));
  EXPECT_EQ(segments.segment(0).candidates_size(), 0);

  EXPECT_FALSE(predictor.Predict(&segments));
  EXPECT_EQ(segments.segment(0).candidates_size(), 0);
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorClearTest) {
  UserHistoryPredictor predictor;
  predictor.WaitForSyncer();

  // input "testtest" 10 times
  for (int i = 0; i < 10; ++i) {
    Segments segments;
    MakeSegmentsForConversion("testtest", &segments);
    // "テストテスト"
    AddCandidate("\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88"
                 "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88", &segments);
    predictor.Finish(&segments);
  }

  predictor.ClearAllHistory();
  predictor.WaitForSyncer();

  // input "testtest" 1 time
  for (int i = 0; i < 1; ++i) {
    Segments segments;
    MakeSegmentsForConversion("testtest", &segments);
    // "テストテスト"
    AddCandidate("\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88"
                 "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88", &segments);
    predictor.Finish(&segments);
  }

  // frequency is cleared as well.
  {
    Segments segments;
    MakeSegmentsForSuggestion("t", &segments);
    EXPECT_FALSE(predictor.Predict(&segments));

    segments.Clear();
    MakeSegmentsForSuggestion("testte", &segments);
    EXPECT_TRUE(predictor.Predict(&segments));
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorTailingPunctuation) {
  UserHistoryPredictor predictor;
  predictor.WaitForSyncer();
  predictor.ClearAllHistory();
  predictor.WaitForSyncer();

  Segments segments;

  // "わたしのなまえはなかのです"
  MakeSegmentsForConversion
      ("\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE"
       "\xE3\x81\xAA\xE3\x81\xBE\xE3\x81\x88\xE3\x81\xAF"
       "\xE3\x81\xAA\xE3\x81\x8B\xE3\x81\xAE\xE3\x81\xA7"
       "\xE3\x81\x99", &segments);

  // "私の名前は中野です"
  AddCandidate(
      0,
      "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
      "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7"
      "\xE3\x81\x99", &segments);

  // "。"
  MakeSegmentsForConversion("\xE3\x80\x82", &segments);
  AddCandidate(1, "\xE3\x80\x82", &segments);

  predictor.Finish(&segments);

  segments.Clear();
  // "わたしの"
  MakeSegmentsForPrediction("\xE3\x82\x8F\xE3\x81\x9F"
                            "\xE3\x81\x97\xE3\x81\xAE", &segments);
  EXPECT_TRUE(predictor.Predict(&segments));
  EXPECT_EQ(segments.segment(0).candidates_size(), 2);
  // "私の名前は中野です"
  EXPECT_EQ(segments.segment(0).candidate(0).value,
            "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
            "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99");
  // "私の名前は中野です。"
  EXPECT_EQ(segments.segment(0).candidate(1).value,
            "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
            "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99"
            "\xE3\x80\x82");

  segments.Clear();
  // "わたしの"
  MakeSegmentsForSuggestion("\xE3\x82\x8F\xE3\x81\x9F"
                            "\xE3\x81\x97\xE3\x81\xAE", &segments);

  EXPECT_TRUE(predictor.Predict(&segments));
  EXPECT_EQ(segments.segment(0).candidates_size(), 2);
  // "私の名前は中野です"
  EXPECT_EQ(segments.segment(0).candidate(0).value,
            "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
            "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99");
  // "私の名前は中野です。"
  EXPECT_EQ(segments.segment(0).candidate(1).value,
            "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
            "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99"
            "\xE3\x80\x82");
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorPreceedingPunctuation) {
  UserHistoryPredictor predictor;
  predictor.WaitForSyncer();
  predictor.ClearAllHistory();
  predictor.WaitForSyncer();

  Segments segments;

  // "。"
  MakeSegmentsForConversion("\xE3\x80\x82", &segments);
  AddCandidate(0, "\xE3\x80\x82", &segments);

  // "わたしのなまえはなかのです"
  MakeSegmentsForConversion
      ("\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE"
       "\xE3\x81\xAA\xE3\x81\xBE\xE3\x81\x88\xE3\x81\xAF"
       "\xE3\x81\xAA\xE3\x81\x8B\xE3\x81\xAE\xE3\x81\xA7"
       "\xE3\x81\x99", &segments);

  // "私の名前は中野です"
  AddCandidate(
      1,
      "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
      "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7"
      "\xE3\x81\x99", &segments);

  predictor.Finish(&segments);

  segments.Clear();
  // "わたしの"
  MakeSegmentsForPrediction("\xE3\x82\x8F\xE3\x81\x9F"
                            "\xE3\x81\x97\xE3\x81\xAE", &segments);

  EXPECT_TRUE(predictor.Predict(&segments));
  EXPECT_EQ(segments.segment(0).candidates_size(), 1);
  // "私の名前は中野です"
  EXPECT_EQ(segments.segment(0).candidate(0).value,
            "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
            "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99");


  segments.Clear();
  // "わたしの"
  MakeSegmentsForSuggestion("\xE3\x82\x8F\xE3\x81\x9F"
                            "\xE3\x81\x97\xE3\x81\xAE", &segments);
  EXPECT_TRUE(predictor.Predict(&segments));
  EXPECT_EQ(segments.segment(0).candidates_size(), 1);
  // "私の名前は中野です"
  EXPECT_EQ(segments.segment(0).candidate(0).value,
            "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
            "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99");
}


TEST_F(UserHistoryPredictorTest, MultiSegmentsMultiInput) {
  UserHistoryPredictor predictor;
  predictor.WaitForSyncer();
  predictor.ClearAllHistory();
  predictor.WaitForSyncer();

  Segments segments;

  // "たろうは/太郎は"
  MakeSegmentsForConversion("\xE3\x81\x9F\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF", &segments);
  AddCandidate(0, "\xE5\xA4\xAA\xE9\x83\x8E\xE3\x81\xAF", &segments);
  predictor.Finish(&segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  // "はなこに/花子に"
  MakeSegmentsForConversion("\xE3\x81\xAF\xE3\x81\xAA"
                            "\xE3\x81\x93\xE3\x81\xAB", &segments);
  AddCandidate(1, "\xE8\x8A\xB1\xE5\xAD\x90\xE3\x81\xAB", &segments);
  predictor.Finish(&segments);
  segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);

  // "むずかしい/難しい"
  segments.clear_conversion_segments();
  MakeSegmentsForConversion("\xE3\x82\x80\xE3\x81\x9A"
                            "\xE3\x81\x8B\xE3\x81\x97"
                            "\xE3\x81\x84", &segments);
  AddCandidate(2, "\xE9\x9B\xA3\xE3\x81\x97\xE3\x81\x84", &segments);
  predictor.Finish(&segments);
  segments.mutable_segment(2)->set_segment_type(Segment::HISTORY);

  // "ほんを/本を"
  segments.clear_conversion_segments();
  MakeSegmentsForConversion("\xE3\x81\xBB"
                            "\xE3\x82\x93\xE3\x82\x92", &segments);
  AddCandidate(3, "\xE6\x9C\xAC\xE3\x82\x92", &segments);
  predictor.Finish(&segments);
  segments.mutable_segment(3)->set_segment_type(Segment::HISTORY);

  // "よませた/読ませた"
  segments.clear_conversion_segments();
  MakeSegmentsForConversion("\xE3\x82\x88\xE3\x81\xBE"
                            "\xE3\x81\x9B\xE3\x81\x9F", &segments);
  AddCandidate(4, "\xE8\xAA\xAD\xE3\x81\xBE"
               "\xE3\x81\x9B\xE3\x81\x9F", &segments);
  predictor.Finish(&segments);

  // "た", Too short inputs
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\x9F", &segments);
  EXPECT_FALSE(predictor.Predict(&segments));

  // "たろうは"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\x9F\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF", &segments);
  EXPECT_TRUE(predictor.Predict(&segments));

  // "ろうは", suggests only from segment boundary.
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF", &segments);
  EXPECT_FALSE(predictor.Predict(&segments));

  // "たろうははな"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\x9F\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF"
                            "\xE3\x81\xAF\xE3\x81\xAA", &segments);
  EXPECT_TRUE(predictor.Predict(&segments));

  // "はなこにむ"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\xAF\xE3\x81\xAA"
                            "\xE3\x81\x93\xE3\x81\xAB\xE3\x82\x80", &segments);
  EXPECT_TRUE(predictor.Predict(&segments));

  // "むずかし"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x82\x80\xE3\x81\x9A"
                            "\xE3\x81\x8B\xE3\x81\x97", &segments);
  EXPECT_TRUE(predictor.Predict(&segments));

  // "はなこにむずかしいほ"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\xAF\xE3\x81\xAA"
                            "\xE3\x81\x93\xE3\x81\xAB"
                            "\xE3\x82\x80\xE3\x81\x9A"
                            "\xE3\x81\x8B\xE3\x81\x97"
                            "\xE3\x81\x84\xE3\x81\xBB", &segments);
  EXPECT_TRUE(predictor.Predict(&segments));

  // "ほんをよま"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\xBB\xE3\x82\x93"
                            "\xE3\x82\x92\xE3\x82\x88\xE3\x81\xBE", &segments);
  EXPECT_TRUE(predictor.Predict(&segments));

  Util::Sleep(1000);

  // Add new entry "たろうはよしこに/太郎は良子に"
  segments.Clear();
  MakeSegmentsForConversion("\xE3\x81\x9F\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF", &segments);
  AddCandidate(0, "\xE5\xA4\xAA\xE9\x83\x8E\xE3\x81\xAF", &segments);
  predictor.Finish(&segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  MakeSegmentsForConversion("\xE3\x82\x88\xE3\x81\x97"
                            "\xE3\x81\x93\xE3\x81\xAB", &segments);
  AddCandidate(1, "\xE8\x89\xAF\xE5\xAD\x90\xE3\x81\xAB", &segments);
  predictor.Finish(&segments);
  segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);

  // "たろうは"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\x9F\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF", &segments);
  EXPECT_TRUE(predictor.Predict(&segments));
  EXPECT_EQ(segments.segment(0).candidate(0).value,
            "\xE5\xA4\xAA\xE9\x83\x8E\xE3\x81\xAF"
            "\xE8\x89\xAF\xE5\xAD\x90\xE3\x81\xAB");
}

TEST_F(UserHistoryPredictorTest, MultiSegmentsSingleInput) {
  UserHistoryPredictor predictor;
  predictor.WaitForSyncer();
  predictor.ClearAllHistory();
  predictor.WaitForSyncer();

  Segments segments;

  // "たろうは/太郎は"
  MakeSegmentsForConversion("\xE3\x81\x9F\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF", &segments);
  AddCandidate(0, "\xE5\xA4\xAA\xE9\x83\x8E\xE3\x81\xAF", &segments);

  // "はなこに/花子に"
  MakeSegmentsForConversion("\xE3\x81\xAF\xE3\x81\xAA"
                            "\xE3\x81\x93\xE3\x81\xAB", &segments);
  AddCandidate(1, "\xE8\x8A\xB1\xE5\xAD\x90\xE3\x81\xAB", &segments);

  // "むずかしい/難しい"
  MakeSegmentsForConversion("\xE3\x82\x80\xE3\x81\x9A"
                            "\xE3\x81\x8B\xE3\x81\x97"
                            "\xE3\x81\x84", &segments);
  AddCandidate(2, "\xE9\x9B\xA3\xE3\x81\x97\xE3\x81\x84", &segments);

  MakeSegmentsForConversion("\xE3\x81\xBB"
                            "\xE3\x82\x93\xE3\x82\x92", &segments);
  AddCandidate(3, "\xE6\x9C\xAC\xE3\x82\x92", &segments);

  // "よませた/読ませた"
  MakeSegmentsForConversion("\xE3\x82\x88\xE3\x81\xBE"
                            "\xE3\x81\x9B\xE3\x81\x9F", &segments);
  AddCandidate(4, "\xE8\xAA\xAD\xE3\x81\xBE"
               "\xE3\x81\x9B\xE3\x81\x9F", &segments);

  predictor.Finish(&segments);

  // "たろうは"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\x9F\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF", &segments);
  EXPECT_TRUE(predictor.Predict(&segments));

  // "た", Too short input
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\x9F", &segments);
  EXPECT_FALSE(predictor.Predict(&segments));

  // "たろうははな"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\x9F\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF"
                            "\xE3\x81\xAF\xE3\x81\xAA", &segments);
  EXPECT_TRUE(predictor.Predict(&segments));

  // "ろうははな", suggest only from segment boundary
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF"
                            "\xE3\x81\xAF\xE3\x81\xAA", &segments);
  EXPECT_FALSE(predictor.Predict(&segments));

  // "はなこにむ"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\xAF\xE3\x81\xAA"
                            "\xE3\x81\x93\xE3\x81\xAB\xE3\x82\x80", &segments);
  EXPECT_TRUE(predictor.Predict(&segments));

  // "むずかし"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x82\x80\xE3\x81\x9A"
                            "\xE3\x81\x8B\xE3\x81\x97", &segments);
  EXPECT_TRUE(predictor.Predict(&segments));

  // "はなこにむずかしいほ"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\xAF\xE3\x81\xAA"
                            "\xE3\x81\x93\xE3\x81\xAB"
                            "\xE3\x82\x80\xE3\x81\x9A"
                            "\xE3\x81\x8B\xE3\x81\x97"
                            "\xE3\x81\x84\xE3\x81\xBB", &segments);
  EXPECT_TRUE(predictor.Predict(&segments));

  // "ほんをよま"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\xBB\xE3\x82\x93"
                            "\xE3\x82\x92\xE3\x82\x88\xE3\x81\xBE", &segments);
  EXPECT_TRUE(predictor.Predict(&segments));

  Util::Sleep(1000);

  // Add new entry "たろうはよしこに/太郎は良子に"
  segments.Clear();
  MakeSegmentsForConversion("\xE3\x81\x9F\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF", &segments);
  AddCandidate(0, "\xE5\xA4\xAA\xE9\x83\x8E\xE3\x81\xAF", &segments);
  predictor.Finish(&segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  MakeSegmentsForConversion("\xE3\x82\x88\xE3\x81\x97"
                            "\xE3\x81\x93\xE3\x81\xAB", &segments);
  AddCandidate(1, "\xE8\x89\xAF\xE5\xAD\x90\xE3\x81\xAB", &segments);
  predictor.Finish(&segments);
  segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);

  // "たろうは"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\x9F\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF", &segments);
  EXPECT_TRUE(predictor.Predict(&segments));
  EXPECT_EQ(segments.segment(0).candidate(0).value,
            "\xE5\xA4\xAA\xE9\x83\x8E\xE3\x81\xAF"
            "\xE8\x89\xAF\xE5\xAD\x90\xE3\x81\xAB");
}

TEST_F(UserHistoryPredictorTest, Regression2843371_Case1) {
  UserHistoryPredictor predictor;
  predictor.WaitForSyncer();
  predictor.ClearAllHistory();
  predictor.WaitForSyncer();

  Segments segments;

  // "とうきょうは"
  MakeSegmentsForConversion("\xE3\x81\xA8\xE3\x81\x86"
                            "\xE3\x81\x8D\xE3\x82\x87"
                            "\xE3\x81\x86\xE3\x81\xAF", &segments);
  AddCandidate(0,
               "\xE6\x9D\xB1\xE4\xBA\xAC"
               "\xE3\x81\xAF", &segments);

  // "、"
  MakeSegmentsForConversion("\xE3\x80\x81", &segments);
  AddCandidate(1, "\xE3\x80\x81", &segments);

  // "にほんです"
  MakeSegmentsForConversion("\xE3\x81\xAB\xE3\x81\xBB"
                            "\xE3\x82\x93\xE3\x81\xA7\xE3\x81\x99",
                            &segments);
  AddCandidate(2,
               "\xE6\x97\xA5\xE6\x9C\xAC"
               "\xE3\x81\xA7\xE3\x81\x99", &segments);

  // "。"
  MakeSegmentsForConversion("\xE3\x80\x82", &segments);
  AddCandidate(3, "\xE3\x80\x82", &segments);

  predictor.Finish(&segments);

  segments.Clear();

  Util::Sleep(1000);

  // "らーめんは"
  MakeSegmentsForConversion("\xE3\x82\x89\xE3\x83\xBC"
                            "\xE3\x82\x81\xE3\x82\x93"
                            "\xE3\x81\xAF", &segments);
  AddCandidate(0,
               "\xE3\x83\xA9\xE3\x83\xBC"
               "\xE3\x83\xA1\xE3\x83\xB3\xE3\x81\xAF", &segments);

  // "、"
  MakeSegmentsForConversion("\xE3\x80\x81", &segments);
  AddCandidate(1, "\xE3\x80\x81", &segments);

  // "めんるいです"
  MakeSegmentsForConversion("\xE3\x82\x81\xE3\x82\x93"
                            "\xE3\x82\x8B\xE3\x81\x84"
                            "\xE3\x81\xA7\xE3\x81\x99", &segments);
  AddCandidate(2,
               "\xE9\xBA\xBA\xE9\xA1\x9E"
               "\xE3\x81\xA7\xE3\x81\x99", &segments);

  // "。"
  MakeSegmentsForConversion("\xE3\x80\x82", &segments);
  AddCandidate(3, "\xE3\x80\x82", &segments);

  predictor.Finish(&segments);

  segments.Clear();

  // "とうきょうは"
  MakeSegmentsForSuggestion("\xE3\x81\xA8\xE3\x81\x86"
                            "\xE3\x81\x8D\xE3\x82\x87"
                            "\xE3\x81\x86\xE3\x81\xAF\xE3\x80\x81",
                            &segments);
  EXPECT_TRUE(predictor.Predict(&segments));

  // "東京は、日本です"
  EXPECT_EQ(segments.segment(0).candidate(0).value,
            "\xE6\x9D\xB1\xE4\xBA\xAC"
            "\xE3\x81\xAF\xE3\x80\x81"
            "\xE6\x97\xA5\xE6\x9C\xAC"
            "\xE3\x81\xA7\xE3\x81\x99");
}

TEST_F(UserHistoryPredictorTest, Regression2843371_Case2) {
  UserHistoryPredictor predictor;
  predictor.WaitForSyncer();
  predictor.ClearAllHistory();
  predictor.WaitForSyncer();

  Segments segments;

  // "えど/江戸"
  MakeSegmentsForConversion("\xE3\x81\x88\xE3\x81\xA9", &segments);
  AddCandidate(0, "\xE6\xB1\x9F\xE6\x88\xB8", &segments);

  // "("
  MakeSegmentsForConversion("(", &segments);
  AddCandidate(1, "(", &segments);

  // "とうきょう/東京"
  MakeSegmentsForConversion(
      "\xE3\x81\xA8\xE3\x81\x86\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86",
      &segments);
  AddCandidate(2, "\xE6\x9D\xB1\xE4\xBA\xAC", &segments);

  // ")"
  MakeSegmentsForConversion(")", &segments);
  AddCandidate(3, ")", &segments);

  // "は"
  MakeSegmentsForConversion("\xE3\x81\xAF", &segments);
  AddCandidate(4, "\xE3\x81\xAF", &segments);

  // "えぞ/蝦夷"
  MakeSegmentsForConversion("\xE3\x81\x88\xE3\x81\x9E", &segments);
  AddCandidate(5, "\xE8\x9D\xA6\xE5\xA4\xB7", &segments);

  // "("
  MakeSegmentsForConversion("(", &segments);
  AddCandidate(6, "(", &segments);

  // "ほっかいどう/北海道"
  MakeSegmentsForConversion("\xE3\x81\xBB\xE3\x81\xA3\xE3\x81\x8B"
                            "\xE3\x81\x84\xE3\x81\xA9\xE3\x81\x86",
                            &segments);
  AddCandidate(7, "\xE5\x8C\x97\xE6\xB5\xB7\xE9\x81\x93",
               &segments);

  // ")"
  MakeSegmentsForConversion(")", &segments);
  AddCandidate(8, ")", &segments);

  // "ではない"
  MakeSegmentsForConversion("\xE3\x81\xA7\xE3\x81\xAF"
                            "\xE3\x81\xAA\xE3\x81\x84", &segments);
  AddCandidate(9, "\xE3\x81\xA7\xE3\x81\xAF"
               "\xE3\x81\xAA\xE3\x81\x84", &segments);

  // "。"
  MakeSegmentsForConversion("\xE3\x80\x82", &segments);
  AddCandidate(10, "\xE3\x80\x82", &segments);

  predictor.Finish(&segments);

  segments.Clear();

  // "えど("
  MakeSegmentsForSuggestion("\xE3\x81\x88\xE3\x81\xA9(", &segments);
  EXPECT_TRUE(predictor.Predict(&segments));
  EXPECT_EQ(segments.segment(0).candidate(0).value,
            "\xE6\xB1\x9F\xE6\x88\xB8(\xE6\x9D\xB1\xE4\xBA\xAC");

  EXPECT_TRUE(predictor.Predict(&segments));

  // "江戸(東京"
  EXPECT_EQ(segments.segment(0).candidate(0).value,
            "\xE6\xB1\x9F\xE6\x88\xB8(\xE6\x9D\xB1\xE4\xBA\xAC");
}

TEST_F(UserHistoryPredictorTest, Regression2843371_Case3) {
  UserHistoryPredictor predictor;
  predictor.WaitForSyncer();
  predictor.ClearAllHistory();
  predictor.WaitForSyncer();

  Segments segments;

  // "「"
  MakeSegmentsForConversion("\xE3\x80\x8C", &segments);
  AddCandidate(0, "\xE3\x80\x8C", &segments);

  // "やま/山"
  MakeSegmentsForConversion("\xE3\x82\x84\xE3\x81\xBE", &segments);
  AddCandidate(1, "\xE5\xB1\xB1", &segments);

  // "」"
  MakeSegmentsForConversion("\xE3\x80\x8D", &segments);
  AddCandidate(2, "\xE3\x80\x8D", &segments);

  // "は"
  MakeSegmentsForConversion("\xE3\x81\xAF", &segments);
  AddCandidate(3, "\xE3\x81\xAF", &segments);

  // "たかい/高い"
  MakeSegmentsForConversion("\xE3\x81\x9F\xE3\x81\x8B\xE3\x81\x84",
                            &segments);
  AddCandidate(4, "\xE9\xAB\x98\xE3\x81\x84", &segments);

  // "。"
  MakeSegmentsForConversion("\xE3\x80\x82", &segments);
  AddCandidate(5, "\xE3\x80\x82", &segments);

  predictor.Finish(&segments);

  Util::Sleep(2000);

  segments.Clear();

  // "「"
  MakeSegmentsForConversion("\xE3\x80\x8C", &segments);
  AddCandidate(0, "\xE3\x80\x8C", &segments);

  // "うみ/海"
  MakeSegmentsForConversion("\xE3\x81\x86\xE3\x81\xBF", &segments);
  AddCandidate(1, "\xE6\xB5\xB7", &segments);

  // "」"
  MakeSegmentsForConversion("\xE3\x80\x8D", &segments);
  AddCandidate(2, "\xE3\x80\x8D", &segments);

  // "は"
  MakeSegmentsForConversion("\xE3\x81\xAF", &segments);
  AddCandidate(3, "\xE3\x81\xAF", &segments);

  // "たかい/高い"
  MakeSegmentsForConversion("\xE3\x81\xB5\xE3\x81\x8B\xE3\x81\x84", &segments);
  AddCandidate(4, "\xE6\xB7\xB1\xE3\x81\x84", &segments);

  // "。"
  MakeSegmentsForConversion("\xE3\x80\x82", &segments);
  AddCandidate(5, "\xE3\x80\x82", &segments);

  predictor.Finish(&segments);

  segments.Clear();

  // "「やま」は"
  MakeSegmentsForSuggestion("\xE3\x80\x8C\xE3\x82\x84"
                            "\xE3\x81\xBE\xE3\x80\x8D\xE3\x81\xAF",
                            &segments);
  EXPECT_TRUE(predictor.Predict(&segments));

  // "「山」は高い"
  EXPECT_EQ(segments.segment(0).candidate(0).value,
            "\xE3\x80\x8C\xE5\xB1\xB1\xE3\x80\x8D"
            "\xE3\x81\xAF\xE9\xAB\x98\xE3\x81\x84");
}

TEST_F(UserHistoryPredictorTest, Regression2843775) {
  UserHistoryPredictor predictor;
  predictor.WaitForSyncer();
  predictor.ClearAllHistory();
  predictor.WaitForSyncer();

  Segments segments;

  // "そうです"
  MakeSegmentsForConversion("\xE3\x81\x9D\xE3\x81\x86"
                            "\xE3\x81\xA7\xE3\x81\x99", &segments);
  AddCandidate(0,
               "\xE3\x81\x9D\xE3\x81\x86"
               "\xE3\x81\xA7\xE3\x81\x99", &segments);

  // "。よろしくおねがいします/。よろしくお願いします"
  MakeSegmentsForConversion("\xE3\x80\x82\xE3\x82\x88"
                            "\xE3\x82\x8D\xE3\x81\x97"
                            "\xE3\x81\x8F\xE3\x81\x8A"
                            "\xE3\x81\xAD\xE3\x81\x8C"
                            "\xE3\x81\x84\xE3\x81\x97"
                            "\xE3\x81\xBE\xE3\x81\x99", &segments);
  AddCandidate(1,
               "\xE3\x80\x82\xE3\x82\x88"
               "\xE3\x82\x8D\xE3\x81\x97"
               "\xE3\x81\x8F\xE3\x81\x8A"
               "\xE9\xA1\x98\xE3\x81\x84"
               "\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x99", &segments);

  predictor.Finish(&segments);

  segments.Clear();

  // "そうです"
  MakeSegmentsForSuggestion("\xE3\x81\x9D\xE3\x81\x86"
                            "\xE3\x81\xA7\xE3\x81\x99", &segments);
  EXPECT_TRUE(predictor.Predict(&segments));

  // "そうです。よろしくお願いします"
  EXPECT_EQ(segments.segment(0).candidate(0).value,
            "\xE3\x81\x9D\xE3\x81\x86"
            "\xE3\x81\xA7\xE3\x81\x99"
            "\xE3\x80\x82\xE3\x82\x88"
            "\xE3\x82\x8D\xE3\x81\x97"
            "\xE3\x81\x8F\xE3\x81\x8A"
            "\xE9\xA1\x98\xE3\x81\x84"
            "\xE3\x81\x97\xE3\x81\xBE"
            "\xE3\x81\x99");
}

TEST_F(UserHistoryPredictorTest, DuplicateString) {
  UserHistoryPredictor predictor;
  predictor.WaitForSyncer();
  predictor.ClearAllHistory();
  predictor.WaitForSyncer();

  Segments segments;

  // "らいおん/ライオン"
  MakeSegmentsForConversion("\xE3\x82\x89\xE3\x81\x84\xE3\x81\x8A\xE3\x82\x93",
                            &segments);
  AddCandidate(0, "\xE3\x83\xA9\xE3\x82\xA4\xE3\x82\xAA\xE3\x83\xB3",
               &segments);

  // "（/（"
  MakeSegmentsForConversion("\xEF\xBC\x88", &segments);
  AddCandidate(1, "\xEF\xBC\x88", &segments);

  // "もうじゅう/猛獣"
  MakeSegmentsForConversion(
      "\xE3\x82\x82\xE3\x81\x86\xE3\x81\x98\xE3\x82\x85\xE3\x81\x86",
      &segments);
  AddCandidate(2, "\xE7\x8C\x9B\xE7\x8D\xA3", &segments);

  // "）と/）と"
  MakeSegmentsForConversion("\xEF\xBC\x89\xE3\x81\xA8", &segments);
  AddCandidate(3, "\xEF\xBC\x89\xE3\x81\xA8", &segments);

  // "ぞうりむし/ゾウリムシ"
  MakeSegmentsForConversion(
      "\xE3\x81\x9E\xE3\x81\x86\xE3\x82\x8A\xE3\x82\x80\xE3\x81\x97",
      &segments);
  AddCandidate(
      4,
      "\xE3\x82\xBE\xE3\x82\xA6\xE3\x83\xAA\xE3\x83\xA0\xE3\x82\xB7",
      &segments);

  // "（/（"
  MakeSegmentsForConversion("\xEF\xBC\x88", &segments);
  AddCandidate(5, "\xEF\xBC\x88", &segments);

  // "びせいぶつ/微生物"
  MakeSegmentsForConversion(
      "\xE3\x81\xB3\xE3\x81\x9B\xE3\x81\x84\xE3\x81\xB6\xE3\x81\xA4",
      &segments);
  AddCandidate(6, "\xE5\xBE\xAE\xE7\x94\x9F\xE7\x89\xA9", &segments);

  // "）/）"
  MakeSegmentsForConversion("\xEF\xBC\x89", &segments);
  AddCandidate(7, "\xEF\xBC\x89", &segments);

  predictor.Finish(&segments);

  segments.Clear();

  // "ぞうりむし"
  MakeSegmentsForSuggestion(
      "\xE3\x81\x9E\xE3\x81\x86\xE3\x82\x8A\xE3\x82\x80\xE3\x81\x97",
      &segments);
  EXPECT_TRUE(predictor.Predict(&segments));

  for (int i = 0; i < segments.segment(0).candidates_size(); ++i) {
    EXPECT_EQ(string::npos,
              segments.segment(0).candidate(i).value.find(
                  "\xE7\x8C\x9B\xE7\x8D\xA3"));  // "猛獣" should not be found
  }

  segments.Clear();

  // "らいおん"
  MakeSegmentsForSuggestion(
      "\xE3\x82\x89\xE3\x81\x84\xE3\x81\x8A\xE3\x82\x93",
      &segments);
  EXPECT_TRUE(predictor.Predict(&segments));

  // "ライオン（微生物" should not be found
  for (int i = 0; i < segments.segment(0).candidates_size(); ++i) {
    EXPECT_EQ(string::npos,
              segments.segment(0).candidate(i).value.find(
                  "\xE3\x83\xA9\xE3\x82\xA4\xE3\x82\xAA\xE3\x83\xB3"
                  "\xEF\xBC\x88\xE5\xBE\xAE\xE7\x94\x9F\xE7\x89\xA9"));
  }
}

struct Command {
  enum Type {
    LOOKUP,
    INSERT,
    SYNC,
    WAIT,
  };
  Type type;
  string key;
  string value;
  Command() : type(LOOKUP) {}
};

int Random(int size) {
  return static_cast<int> (1.0 * size * rand() / (RAND_MAX + 1.0));
}

TEST_F(UserHistoryPredictorTest, SyncTest) {
  UserHistoryPredictor predictor;
  predictor.WaitForSyncer();

  vector<Command> commands(10000);
  for (size_t i = 0; i < commands.size(); ++i) {
    commands[i].key = Util::SimpleItoa(i) + "key";
    commands[i].value = Util::SimpleItoa(i) + "value";
    const int n = Random(100);
    if (n == 0) {
      commands[i].type = Command::WAIT;
    } else if (n < 10) {
      commands[i].type = Command::SYNC;
    } else if (n < 50) {
      commands[i].type = Command::INSERT;
    } else {
      commands[i].type = Command::LOOKUP;
    }
  }

  // Kind of stress test
  Segments segments;
  for (size_t i = 0; i < commands.size(); ++i) {
    switch (commands[i].type) {
      case Command::SYNC:
        predictor.Sync();
        break;
      case Command::WAIT:
        predictor.WaitForSyncer();
        break;
      case Command::INSERT:
        segments.Clear();
        MakeSegmentsForConversion(commands[i].key, &segments);
        AddCandidate(commands[i].value, &segments);
        predictor.Finish(&segments);
        break;
      case Command::LOOKUP:
        segments.Clear();
        MakeSegmentsForSuggestion(commands[i].key, &segments);
        predictor.Predict(&segments);
      default:
        break;
    }
  }
}

TEST_F(UserHistoryPredictorTest, GetMatchTypeTest) {
  EXPECT_EQ(UserHistoryPredictor::NO_MATCH,
            UserHistoryPredictor::GetMatchType("test", ""));

  EXPECT_EQ(UserHistoryPredictor::NO_MATCH,
            UserHistoryPredictor::GetMatchType("", ""));

  EXPECT_EQ(UserHistoryPredictor::LEFT_EMPTY_MATCH,
            UserHistoryPredictor::GetMatchType("", "test"));

  EXPECT_EQ(UserHistoryPredictor::NO_MATCH,
            UserHistoryPredictor::GetMatchType("foo", "bar"));

  EXPECT_EQ(UserHistoryPredictor::EXACT_MATCH,
            UserHistoryPredictor::GetMatchType("foo", "foo"));

  EXPECT_EQ(UserHistoryPredictor::LEFT_PREFIX_MATCH,
            UserHistoryPredictor::GetMatchType("foo", "foobar"));

  EXPECT_EQ(UserHistoryPredictor::RIGHT_PREFIX_MATCH,
            UserHistoryPredictor::GetMatchType("foobar", "foo"));
}

TEST_F(UserHistoryPredictorTest, FingerPrintTest) {
  const char kKey[] = "abc";
  const char kValue[] = "ABC";

  UserHistoryPredictor::Entry entry;
  entry.set_key(kKey);
  entry.set_value(kValue);

  const uint32 entry_fp1 =
      UserHistoryPredictor::Fingerprint(kKey, kValue);
  const uint32 entry_fp2 =
      UserHistoryPredictor::EntryFingerprint(entry);

  const uint32 entry_fp3 =
      UserHistoryPredictor::Fingerprint(
          kKey, kValue,
          UserHistoryPredictor::Entry::DEFAULT_ENTRY);

  const uint32 entry_fp4 =
      UserHistoryPredictor::Fingerprint(
          kKey, kValue,
          UserHistoryPredictor::Entry::CLEAN_ALL_EVENT);

  const uint32 entry_fp5 =
      UserHistoryPredictor::Fingerprint(
          kKey, kValue,
          UserHistoryPredictor::Entry::CLEAN_UNUSED_EVENT);

  Segment segment;
  segment.set_key(kKey);
  Segment::Candidate *c = segment.add_candidate();
  c->key = kKey;
  c->content_key = kKey;
  c->value = kValue;
  c->content_value = kValue;

  const uint32 segment_fp =
      UserHistoryPredictor::SegmentFingerprint(segment);

  Segment segment2;
  segment2.set_key("ab");
  Segment::Candidate *c2 = segment2.add_candidate();
  c2->key = kKey;
  c2->content_key = kKey;
  c2->value = kValue;
  c2->content_value = kValue;

  const uint32 segment_fp2 =
      UserHistoryPredictor::SegmentFingerprint(segment2);

  EXPECT_EQ(entry_fp1, entry_fp2);
  EXPECT_EQ(entry_fp1, entry_fp3);
  EXPECT_NE(entry_fp1, entry_fp4);
  EXPECT_NE(entry_fp1, entry_fp5);
  EXPECT_NE(entry_fp4, entry_fp5);
  EXPECT_EQ(segment_fp, entry_fp2);
  EXPECT_EQ(segment_fp, entry_fp1);
  EXPECT_EQ(segment_fp, segment_fp2);
}

TEST_F(UserHistoryPredictorTest, Uint32ToStringTest) {
  EXPECT_EQ(123,
            UserHistoryPredictor::StringToUint32(
                UserHistoryPredictor::Uint32ToString(123)));

  EXPECT_EQ(12141,
            UserHistoryPredictor::StringToUint32(
                UserHistoryPredictor::Uint32ToString(12141)));

  for (uint32 i = 0; i < 10000; ++i) {
    EXPECT_EQ(i,
              UserHistoryPredictor::StringToUint32(
                  UserHistoryPredictor::Uint32ToString(i)));
  }

  // invalid input
  EXPECT_EQ(0, UserHistoryPredictor::StringToUint32(""));

  // not 4byte
  EXPECT_EQ(0, UserHistoryPredictor::StringToUint32("abcdef"));
}

TEST_F(UserHistoryPredictorTest, GetScore) {
  // latest value has higher score.
  {
    UserHistoryPredictor::Entry entry1, entry2;

    entry1.set_key("abc");
    entry1.set_value("ABC");
    entry1.set_last_access_time(10);

    entry2.set_key("foo");
    entry2.set_value("ABC");
    entry2.set_last_access_time(20);

    EXPECT_GT(UserHistoryPredictor::GetScore(entry2),
              UserHistoryPredictor::GetScore(entry1));
  }

  // shorter value has higher score.
  {
    UserHistoryPredictor::Entry entry1, entry2;

    entry1.set_key("abc");
    entry1.set_value("ABC");
    entry1.set_last_access_time(10);

    entry2.set_key("foo");
    entry2.set_value("ABCD");
    entry2.set_last_access_time(10);

    EXPECT_GT(UserHistoryPredictor::GetScore(entry1),
              UserHistoryPredictor::GetScore(entry2));
  }

  // bigram boost makes the entry stronger
  {
    UserHistoryPredictor::Entry entry1, entry2;

    entry1.set_key("abc");
    entry1.set_value("ABC");
    entry1.set_last_access_time(10);

    entry2.set_key("foo");
    entry2.set_value("ABC");
    entry2.set_last_access_time(10);
    entry2.set_bigram_boost(true);

    EXPECT_GT(UserHistoryPredictor::GetScore(entry2),
              UserHistoryPredictor::GetScore(entry1));
  }

  // bigram boost makes the entry stronger
  {
    UserHistoryPredictor::Entry entry1, entry2;

    entry1.set_key("abc");
    entry1.set_value("ABCD");
    entry1.set_last_access_time(10);
    entry1.set_bigram_boost(true);

    entry2.set_key("foo");
    entry2.set_value("ABC");
    entry2.set_last_access_time(50);

    EXPECT_GT(UserHistoryPredictor::GetScore(entry1),
              UserHistoryPredictor::GetScore(entry2));
  }
}

TEST_F(UserHistoryPredictorTest, IsValidEntry) {
  UserHistoryPredictor::Entry entry;

  EXPECT_TRUE(UserHistoryPredictor::IsValidEntry(entry));

  entry.set_key("key");
  entry.set_key("value");

  EXPECT_TRUE(UserHistoryPredictor::IsValidEntry(entry));

  entry.set_removed(true);
  EXPECT_FALSE(UserHistoryPredictor::IsValidEntry(entry));

  entry.set_removed(false);
  EXPECT_TRUE(UserHistoryPredictor::IsValidEntry(entry));

  entry.set_entry_type(UserHistoryPredictor::Entry::CLEAN_ALL_EVENT);
  EXPECT_FALSE(UserHistoryPredictor::IsValidEntry(entry));

  entry.set_entry_type(UserHistoryPredictor::Entry::CLEAN_UNUSED_EVENT);
  EXPECT_FALSE(UserHistoryPredictor::IsValidEntry(entry));

  entry.set_removed(true);
  EXPECT_FALSE(UserHistoryPredictor::IsValidEntry(entry));

  entry.Clear();
  EXPECT_TRUE(UserHistoryPredictor::IsValidEntry(entry));
}

TEST_F(UserHistoryPredictorTest, IsValidSuggestion) {
  UserHistoryPredictor::Entry entry;

  EXPECT_FALSE(UserHistoryPredictor::IsValidSuggestion(false, 1, entry));

  entry.set_bigram_boost(true);
  EXPECT_TRUE(UserHistoryPredictor::IsValidSuggestion(false, 1, entry));

  entry.set_bigram_boost(false);
  EXPECT_TRUE(UserHistoryPredictor::IsValidSuggestion(true, 1, entry));

  entry.set_bigram_boost(false);
  entry.set_conversion_freq(10);
  EXPECT_TRUE(UserHistoryPredictor::IsValidSuggestion(false, 1, entry));
}

TEST_F(UserHistoryPredictorTest, EntryPriorityQueueTest) {
  // removed automatically
  const int kSize = 10000;
  {
    UserHistoryPredictor::EntryPriorityQueue queue;
    for (int i = 0; i < 10000; ++i) {
      EXPECT_TRUE(queue.NewEntry());
    }
  }

  {
    UserHistoryPredictor::EntryPriorityQueue queue;
    vector<UserHistoryPredictor::Entry *> expected;
    for (int i = 0; i < kSize; ++i) {
      UserHistoryPredictor::Entry *entry = queue.NewEntry();
      entry->set_key("test" + Util::SimpleItoa(i));
      entry->set_value("test" + Util::SimpleItoa(i));
      entry->set_last_access_time(i + 1000);
      expected.push_back(entry);
      EXPECT_TRUE(queue.Push(entry));
    }

    int n = kSize - 1;
    while (true) {
      const UserHistoryPredictor::Entry *entry = queue.Pop();
      if (entry == NULL) {
        break;
      }
      EXPECT_EQ(entry, expected[n]);
      --n;
    }
    EXPECT_EQ(-1, n);
  }

  {
    UserHistoryPredictor::EntryPriorityQueue queue;
    for (int i = 0; i < 5; ++i) {
      UserHistoryPredictor::Entry *entry = queue.NewEntry();
      entry->set_key("test");
      entry->set_value("test");
      queue.Push(entry);
    }
    EXPECT_EQ(1, queue.size());

    for (int i = 0; i < 5; ++i) {
      UserHistoryPredictor::Entry *entry = queue.NewEntry();
      entry->set_key("foo");
      entry->set_value("bar");
      queue.Push(entry);
    }

    EXPECT_EQ(2, queue.size());
  }
}

TEST_F(UserHistoryPredictorTest, PrivacySensitiveTest) {
  UserHistoryPredictor predictor;
  predictor.WaitForSyncer();

  {
    Segments segments;
    MakeSegmentsForConversion("123abc!", &segments);
    AddCandidate("123abc!", &segments);
    predictor.Finish(&segments);
  }

  // no suggestion for password-like input
  {
    Segments segments;
    MakeSegmentsForSuggestion("123abc", &segments);
    EXPECT_FALSE(predictor.Predict(&segments));
    segments.Clear();
    MakeSegmentsForSuggestion("123abc!", &segments);
    EXPECT_FALSE(predictor.Predict(&segments));
  }

  // no prediction for password-like input
  {
    Segments segments;
    MakeSegmentsForPrediction("123abc", &segments);
    EXPECT_FALSE(predictor.Predict(&segments));
    segments.Clear();
    MakeSegmentsForPrediction("123abc!", &segments);
    EXPECT_FALSE(predictor.Predict(&segments));
  }

  predictor.ClearAllHistory();
  predictor.WaitForSyncer();
  {
    Segments segments;
    MakeSegmentsForConversion("123", &segments);
    MakeSegmentsForConversion("abc!", &segments);
    AddCandidate(0, "123", &segments);
    AddCandidate(1, "abc!", &segments);
    predictor.Finish(&segments);
  }

  // treat as not privacy sensitive but conversion result
  {
    Segments segments;
    MakeSegmentsForSuggestion("123abc", &segments);
    EXPECT_TRUE(predictor.Predict(&segments));
    segments.Clear();
    MakeSegmentsForSuggestion("123abc!", &segments);
    EXPECT_TRUE(predictor.Predict(&segments));
  }

  {
    Segments segments;
    MakeSegmentsForPrediction("123abc", &segments);
    EXPECT_TRUE(predictor.Predict(&segments));
    segments.Clear();
    MakeSegmentsForPrediction("123abc!", &segments);
    EXPECT_TRUE(predictor.Predict(&segments));
  }

  predictor.ClearAllHistory();
  predictor.WaitForSyncer();
  {
    Segments segments;
    // "ぐーぐる"
    MakeSegmentsForConversion(
        "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B",
        &segments);
    AddCandidate(0, "Google", &segments);
    predictor.Finish(&segments);
  }

  // treat as not privacy sensitive but conversion result
  {
    Segments segments;
    // "ぐーぐ"
    MakeSegmentsForSuggestion(
        "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90", &segments);
    EXPECT_TRUE(predictor.Predict(&segments));
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryStorage) {
  const string filename =
      Util::JoinPath(Util::GetUserProfileDirectory(), "test");

  UserHistoryStorage storage1(filename);
  EXPECT_EQ(filename, storage1.filename());

  UserHistoryPredictor::Entry *entry = storage1.add_entries();
  CHECK(entry);
  entry->set_key("key");
  entry->set_key("value");
  storage1.Save();
  UserHistoryStorage storage2(filename);
  storage2.Load();

  EXPECT_EQ(storage1.DebugString(), storage2.DebugString());
  Util::Unlink(filename);
}
}  // namespace
}  // namespace mozc
