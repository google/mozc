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

#include "prediction/user_history_predictor.h"

#include "base/password_manager.h"
#include "base/util.h"
#include "converter/converter_interface.h"
#include "converter/converter_mock.h"
#include "converter/segments.h"
#include "session/config.pb.h"
#include "session/config_handler.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {

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
  segments->set_request_type(Segments::SUGGESTION);
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

void AddCandidate(const string &value, Segments *segments) {
  Segment::Candidate *candidate =
      segments->mutable_segment(0)->add_candidate();
  CHECK(candidate);
  candidate->Init();
  candidate->value = value;
  candidate->content_value = value;
  candidate->content_key = segments->segment(0).key();
}

void AddCandidateWithDescription(const string &value,
                                 const string &desc,
                                 Segments *segments) {
  Segment::Candidate *candidate =
      segments->mutable_segment(0)->add_candidate();
  CHECK(candidate);
  candidate->Init();
  candidate->value = value;
  candidate->content_value = value;
  candidate->content_key = segments->segment(0).key();
  candidate->description = desc;
}

TEST(UserHistoryPredictor, UserHistoryPredictorTest) {
  kUseMockPasswordManager = true;
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

  {
    UserHistoryPredictor predictor;
    predictor.WaitForSyncer();

    // Nothing happen
    {
      Segments segments;
      // "わたしのなまえはなかのです"
      MakeSegmentsForSuggestion(
          "\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8", &segments);
      EXPECT_FALSE(predictor.Suggest(&segments));
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
      EXPECT_TRUE(predictor.Suggest(&segments));
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
      EXPECT_FALSE(predictor.Suggest(&segments));

      config.set_use_history_suggest(true);
      config.set_incognito_mode(true);
      config::ConfigHandler::SetConfig(config);

      // "わたしの"
      MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
      EXPECT_FALSE(predictor.Suggest(&segments));
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
    EXPECT_TRUE(predictor.Suggest(&segments));
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

    //Exact Match
    segments.Clear();
    // "わたしのなまえはなかのです"
    MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE"
        "\xE3\x81\xAA\xE3\x81\xBE\xE3\x81\x88\xE3\x81\xAF"
        "\xE3\x81\xAA\xE3\x81\x8B\xE3\x81\xAE\xE3\x81\xA7\xE3\x81\x99",
        &segments);
    EXPECT_TRUE(predictor.Suggest(&segments));
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
    EXPECT_FALSE(predictor.Suggest(&segments));

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
    EXPECT_FALSE(predictor.Suggest(&segments));

    // "わたしの"
    MakeSegmentsForPrediction(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_FALSE(predictor.Predict(&segments));
  }
}

TEST(UserHistoryPredictor, DescriptionTest) {
  kUseMockPasswordManager = true;
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

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
      string info;
      segments.DebugString(&info);
      LOG(INFO) << info;
      predictor.Finish(&segments);

      // "わたしの"
      MakeSegmentsForSuggestion(
          "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
      EXPECT_TRUE(predictor.Suggest(&segments));
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
      EXPECT_FALSE(predictor.Suggest(&segments));

      config.set_use_history_suggest(true);
      config.set_incognito_mode(true);
      config::ConfigHandler::SetConfig(config);

      // "わたしの"
      MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
      EXPECT_FALSE(predictor.Suggest(&segments));
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
    EXPECT_TRUE(predictor.Suggest(&segments));
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

    //Exact Match
    segments.Clear();
    // "わたしのなまえはなかのです"
    MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE"
        "\xE3\x81\xAA\xE3\x81\xBE\xE3\x81\x88\xE3\x81\xAF"
        "\xE3\x81\xAA\xE3\x81\x8B\xE3\x81\xAE\xE3\x81\xA7\xE3\x81\x99",
        &segments);
    EXPECT_TRUE(predictor.Suggest(&segments));
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
    EXPECT_FALSE(predictor.Suggest(&segments));

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
    EXPECT_FALSE(predictor.Suggest(&segments));

    // "わたしの"
    MakeSegmentsForPrediction(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_FALSE(predictor.Predict(&segments));
  }
}

TEST(UserHistoryPredictor, UserHistoryPredictorUnusedHistoryTest) {
  kUseMockPasswordManager = true;
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

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
    EXPECT_TRUE(predictor.Suggest(&segments));
    // "私の名前は中野です"
    EXPECT_EQ(segments.segment(0).candidate(0).value,
              "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99");

    segments.Clear();
    // "ひろすえ"
    MakeSegmentsForSuggestion("\xE3\x81\xB2\xE3\x82\x8D"
                              "\xE3\x81\x99\xE3\x81\x88", &segments);
    EXPECT_TRUE(predictor.Suggest(&segments));
    // "広末涼子"
    EXPECT_EQ(segments.segment(0).candidate(0).value,
              "\xE5\xBA\x83\xE6\x9C\xAB\xE6\xB6\xBC\xE5\xAD\x90");

    predictor.ClearUnusedHistory();
    predictor.WaitForSyncer();

    segments.Clear();
    // "わたしの"
    MakeSegmentsForSuggestion("\xE3\x82\x8F\xE3\x81\x9F"
                              "\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_TRUE(predictor.Suggest(&segments));
    // "私の名前は中野です"
    EXPECT_EQ(segments.segment(0).candidate(0).value,
              "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99");

    segments.Clear();
    // "ひろすえ"
    MakeSegmentsForSuggestion("\xE3\x81\xB2\xE3\x82\x8D"
                              "\xE3\x81\x99\xE3\x81\x88", &segments);
    EXPECT_FALSE(predictor.Suggest(&segments));

    predictor.Sync();
  }

  {
    UserHistoryPredictor predictor;
    predictor.WaitForSyncer();
    Segments segments;

    // "わたしの"
    MakeSegmentsForSuggestion("\xE3\x82\x8F\xE3\x81\x9F"
                              "\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_TRUE(predictor.Suggest(&segments));
    // "私の名前は中野です"
    EXPECT_EQ(segments.segment(0).candidate(0).value,
              "\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99");

    segments.Clear();
    // "ひろすえ"
    MakeSegmentsForSuggestion("\xE3\x81\xB2\xE3\x82\x8D"
                              "\xE3\x81\x99\xE3\x81\x88", &segments);
    EXPECT_FALSE(predictor.Suggest(&segments));
  }
}

TEST(UserHistoryPredictor, UserHistoryPredictorRevertTest) {
  kUseMockPasswordManager = true;
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  UserHistoryPredictor predictor;
  predictor.WaitForSyncer();
  predictor.ClearAllHistory();

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
  EXPECT_TRUE(predictor.Suggest(&segments2));
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

  EXPECT_FALSE(predictor.Suggest(&segments));
  EXPECT_EQ(segments.segment(0).candidates_size(), 0);
}

TEST(UserHistoryPredictor, UserHistoryPredictorClearTest) {
  kUseMockPasswordManager = true;
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

  UserHistoryPredictor predictor;
  predictor.WaitForSyncer();

  // input "012-3456" 10 times
  for (int i = 0; i < 10; ++i) {
    Segments segments;
    MakeSegmentsForConversion("012-3456", &segments);
    AddCandidate("012-3456", &segments);
    predictor.Finish(&segments);
  }

  predictor.ClearAllHistory();
  predictor.WaitForSyncer();

  // input "012-3456" 1 time
  for (int i = 0; i < 1; ++i) {
    Segments segments;
    MakeSegmentsForConversion("012-3456", &segments);
    AddCandidate("012-3456", &segments);
    predictor.Finish(&segments);
  }

  // frequency is cleared as well.
  {
    Segments segments;
    MakeSegmentsForSuggestion("0", &segments);
    EXPECT_FALSE(predictor.Suggest(&segments));

    segments.clear();
    MakeSegmentsForSuggestion("012-345", &segments);
    EXPECT_TRUE(predictor.Suggest(&segments));
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

TEST(UserHistoryPredictor, SyncTest) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

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
        segments.clear();
        MakeSegmentsForConversion(commands[i].key, &segments);
        AddCandidate(commands[i].value, &segments);
        predictor.Finish(&segments);
        break;
      case Command::LOOKUP:
        segments.clear();
        MakeSegmentsForSuggestion(commands[i].key, &segments);
        predictor.Suggest(&segments);
      default:
        break;
    }
  }
}
}  // namespace
}  // namespace mozc
