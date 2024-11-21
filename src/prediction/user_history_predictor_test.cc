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

#include "prediction/user_history_predictor.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "base/clock_mock.h"
#include "base/container/trie.h"
#include "base/file/temp_dir.h"
#include "base/file_util.h"
#include "base/random.h"
#include "base/strings/unicode.h"
#include "base/system_util.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/query.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/suppression_dictionary.h"
#include "engine/modules.h"
#include "engine/supplemental_model_interface.h"
#include "engine/supplemental_model_mock.h"
#include "prediction/user_history_predictor.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "request/request_test_util.h"
#include "storage/encrypted_string_storage.h"
#include "storage/lru_cache.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"

namespace mozc::prediction {
namespace {

using ::mozc::commands::Request;
using ::mozc::composer::TypeCorrectedQuery;
using ::mozc::config::Config;
using ::mozc::dictionary::MockDictionary;
using ::mozc::dictionary::SuppressionDictionary;
using ::testing::_;
using ::testing::Return;

}  // namespace

class UserHistoryPredictorTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    request_.Clear();
    config::ConfigHandler::GetDefaultConfig(&config_);
    config_.set_use_typing_correction(true);
    table_ = std::make_unique<composer::Table>();
    composer_ = composer::Composer(table_.get(), &request_, &config_);
    data_and_predictor_ = CreateDataAndPredictor();

    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
  }

  void TearDown() override {
    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
  }

  ConversionRequest CreateConversionRequest(
      const composer::Composer &composer) const {
    ConversionRequest convreq(composer, request_, context_, &config_);
    convreq.set_max_user_history_prediction_candidates_size(10);
    convreq.set_max_user_history_prediction_candidates_size_for_zero_query(10);
    return convreq;
  }

  UserHistoryPredictor *GetUserHistoryPredictor() {
    return data_and_predictor_->predictor.get();
  }

  void WaitForSyncer(UserHistoryPredictor *predictor) {
    predictor->WaitForSyncer();
  }

  UserHistoryPredictor *GetUserHistoryPredictorWithClearedHistory() {
    UserHistoryPredictor *predictor = data_and_predictor_->predictor.get();
    predictor->WaitForSyncer();
    predictor->ClearAllHistory();
    predictor->WaitForSyncer();
    return predictor;
  }

  SuppressionDictionary *GetSuppressionDictionary() {
    return data_and_predictor_->modules.GetMutableSuppressionDictionary();
  }

  bool IsSuggested(UserHistoryPredictor *predictor, const absl::string_view key,
                   const absl::string_view value) {
    ConversionRequest conversion_request;
    Segments segments;
    MakeSegments(key, &segments);
    conversion_request.set_request_type(ConversionRequest::SUGGESTION);
    return predictor->PredictForRequest(conversion_request, &segments) &&
           FindCandidateByValue(value, segments);
  }

  bool IsPredicted(UserHistoryPredictor *predictor, const absl::string_view key,
                   const absl::string_view value) {
    ConversionRequest conversion_request;
    Segments segments;
    MakeSegments(key, &segments);
    conversion_request.set_request_type(ConversionRequest::PREDICTION);
    return predictor->PredictForRequest(conversion_request, &segments) &&
           FindCandidateByValue(value, segments);
  }

  bool IsSuggestedAndPredicted(UserHistoryPredictor *predictor,
                               const absl::string_view key,
                               const absl::string_view value) {
    return IsSuggested(predictor, key, value) &&
           IsPredicted(predictor, key, value);
  }

  static UserHistoryPredictor::Entry *InsertEntry(
      UserHistoryPredictor *predictor, const absl::string_view key,
      const absl::string_view value) {
    UserHistoryPredictor::Entry *e =
        &predictor->dic_->Insert(predictor->Fingerprint(key, value))->value;
    e->set_key(std::string(key));
    e->set_value(std::string(value));
    e->set_removed(false);
    return e;
  }

  static UserHistoryPredictor::Entry *AppendEntry(
      UserHistoryPredictor *predictor, const absl::string_view key,
      const absl::string_view value, UserHistoryPredictor::Entry *prev) {
    prev->add_next_entries()->set_entry_fp(predictor->Fingerprint(key, value));
    UserHistoryPredictor::Entry *e = InsertEntry(predictor, key, value);
    return e;
  }

  static size_t EntrySize(const UserHistoryPredictor &predictor) {
    return predictor.dic_->Size();
  }

  static bool LoadStorage(UserHistoryPredictor *predictor,
                          const UserHistoryStorage &history) {
    return predictor->Load(history);
  }

  static bool IsConnected(const UserHistoryPredictor::Entry &prev,
                          const UserHistoryPredictor::Entry &next) {
    const uint32_t fp =
        UserHistoryPredictor::Fingerprint(next.key(), next.value());
    for (size_t i = 0; i < prev.next_entries_size(); ++i) {
      if (prev.next_entries(i).entry_fp() == fp) {
        return true;
      }
    }
    return false;
  }

  // Helper function to create a test case for bigram history deletion.
  void InitHistory_JapaneseInput(UserHistoryPredictor *predictor,
                                 UserHistoryPredictor::Entry **japaneseinput,
                                 UserHistoryPredictor::Entry **japanese,
                                 UserHistoryPredictor::Entry **input) {
    // Make the history for ("japaneseinput", "JapaneseInput"). It's assumed
    // that this sentence consists of two segments, "japanese" and "input". So,
    // the following history entries are constructed:
    //   ("japaneseinput", "JapaneseInput")  // Unigram
    //   ("japanese", "Japanese") --- ("input", "Input")  // Bigram chain
    *japaneseinput = InsertEntry(predictor, "japaneseinput", "JapaneseInput");
    *japanese = InsertEntry(predictor, "japanese", "Japanese");
    *input = AppendEntry(predictor, "input", "Input", *japanese);
    (*japaneseinput)->set_last_access_time(1);
    (*japanese)->set_last_access_time(1);
    (*input)->set_last_access_time(1);

    // Check the predictor functionality for the above history structure.
    EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
    EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
    EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "input", "Input"));
  }

  // Helper function to create a test case for trigram history deletion.
  void InitHistory_JapaneseInputMethod(
      UserHistoryPredictor *predictor,
      UserHistoryPredictor::Entry **japaneseinputmethod,
      UserHistoryPredictor::Entry **japanese,
      UserHistoryPredictor::Entry **input,
      UserHistoryPredictor::Entry **method) {
    // Make the history for ("japaneseinputmethod", "JapaneseInputMethod"). It's
    // assumed that this sentence consists of three segments, "japanese",
    // "input" and "method". So, the following history entries are constructed:
    //   ("japaneseinputmethod", "JapaneseInputMethod")  // Unigram
    //   ("japanese", "Japanese") -- ("input", "Input") -- ("method", "Method")
    *japaneseinputmethod =
        InsertEntry(predictor, "japaneseinputmethod", "JapaneseInputMethod");
    *japanese = InsertEntry(predictor, "japanese", "Japanese");
    *input = AppendEntry(predictor, "input", "Input", *japanese);
    *method = AppendEntry(predictor, "method", "Method", *input);
    (*japaneseinputmethod)->set_last_access_time(1);
    (*japanese)->set_last_access_time(1);
    (*input)->set_last_access_time(1);
    (*method)->set_last_access_time(1);

    // Check the predictor functionality for the above history structure.
    EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
    EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
    EXPECT_TRUE(
        IsSuggestedAndPredicted(predictor, "japan", "JapaneseInputMethod"));
    EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "Input"));
    EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "InputMethod"));
    EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "meth", "Method"));
  }

  static void AddSegment(absl::string_view key, Segments *segments) {
    Segment *seg = segments->add_segment();
    seg->set_key(key);
    seg->set_segment_type(Segment::FIXED_VALUE);
  }

  static void MakeSegments(absl::string_view key, Segments *segments) {
    segments->Clear();
    AddSegment(key, segments);
  }

  ConversionRequest SetUpInputForSuggestion(absl::string_view key,
                                            composer::Composer *composer,
                                            Segments *segments) const {
    composer->Reset();
    composer->SetPreeditTextForTestOnly(key);
    MakeSegments(key, segments);
    ConversionRequest convreq = CreateConversionRequest(*composer);
    convreq.set_request_type(ConversionRequest::SUGGESTION);
    return convreq;
  }

  static void PrependHistorySegments(absl::string_view key,
                                     absl::string_view value,
                                     Segments *segments) {
    Segment *seg = segments->push_front_segment();
    seg->set_segment_type(Segment::HISTORY);
    seg->set_key(key);
    Segment::Candidate *c = seg->add_candidate();
    c->key = std::string(key);
    c->content_key = std::string(key);
    c->value = std::string(value);
    c->content_value = std::string(value);
  }

  ConversionRequest SetUpInputForSuggestionWithHistory(
      absl::string_view key, absl::string_view hist_key,
      absl::string_view hist_value, composer::Composer *composer,
      Segments *segments) const {
    const ConversionRequest convreq =
        SetUpInputForSuggestion(key, composer, segments);
    PrependHistorySegments(hist_key, hist_value, segments);
    return convreq;
  }

  ConversionRequest SetUpInputForPrediction(absl::string_view key,
                                            composer::Composer *composer,
                                            Segments *segments) const {
    composer->Reset();
    composer->SetPreeditTextForTestOnly(key);
    MakeSegments(key, segments);
    ConversionRequest convreq = CreateConversionRequest(*composer);
    convreq.set_request_type(ConversionRequest::PREDICTION);
    return convreq;
  }

  ConversionRequest SetUpInputForPredictionWithHistory(
      absl::string_view key, absl::string_view hist_key,
      absl::string_view hist_value, composer::Composer *composer,
      Segments *segments) const {
    const ConversionRequest convreq =
        SetUpInputForPrediction(key, composer, segments);
    PrependHistorySegments(hist_key, hist_value, segments);
    return convreq;
  }

  ConversionRequest SetUpInputForConversion(absl::string_view key,
                                            composer::Composer *composer,
                                            Segments *segments) const {
    composer->Reset();
    composer->SetPreeditTextForTestOnly(key);
    MakeSegments(key, segments);
    ConversionRequest convreq = CreateConversionRequest(*composer);
    convreq.set_request_type(ConversionRequest::CONVERSION);
    return convreq;
  }

  ConversionRequest SetUpInputForConversionWithHistory(
      absl::string_view key, absl::string_view hist_key,
      absl::string_view hist_value, composer::Composer *composer,
      Segments *segments) const {
    const ConversionRequest convreq =
        SetUpInputForConversion(key, composer, segments);
    PrependHistorySegments(hist_key, hist_value, segments);
    return convreq;
  }

  ConversionRequest InitSegmentsFromInputSequence(const absl::string_view text,
                                                  composer::Composer *composer,
                                                  Segments *segments) const {
    DCHECK(composer);
    DCHECK(segments);
    for (const UnicodeChar ch : Utf8AsUnicodeChar(text)) {
      commands::KeyEvent key;
      const char32_t codepoint = ch.char32();
      if (codepoint <= 0x7F) {  // IsAscii, w is unsigned.
        key.set_key_code(codepoint);
      } else {
        key.set_key_code('?');
        key.set_key_string(ch.utf8());
      }
      composer->InsertCharacterKeyEvent(key);
    }

    ConversionRequest convreq = CreateConversionRequest(*composer);
    convreq.set_request_type(ConversionRequest::PREDICTION);
    Segment *segment = segments->add_segment();
    CHECK(segment);
    std::string query = composer->GetQueryForPrediction();
    segment->set_key(query);
    return convreq;
  }

  static void AddCandidate(size_t index, absl::string_view value,
                           Segments *segments) {
    Segment::Candidate *candidate =
        segments->mutable_segment(index)->add_candidate();
    CHECK(candidate);
    candidate->value = std::string(value);
    candidate->content_value = std::string(value);
    candidate->key = segments->segment(index).key();
    candidate->content_key = segments->segment(index).key();
  }

  static void AddCandidateWithDescription(size_t index, absl::string_view value,
                                          absl::string_view desc,
                                          Segments *segments) {
    Segment::Candidate *candidate =
        segments->mutable_segment(index)->add_candidate();
    CHECK(candidate);
    candidate->value = std::string(value);
    candidate->content_value = std::string(value);
    candidate->key = segments->segment(index).key();
    candidate->content_key = segments->segment(index).key();
    candidate->description = std::string(desc);
  }

  static void AddCandidate(absl::string_view value, Segments *segments) {
    AddCandidate(0, value, segments);
  }

  static void AddCandidateWithDescription(absl::string_view value,
                                          absl::string_view desc,
                                          Segments *segments) {
    AddCandidateWithDescription(0, value, desc, segments);
  }

  static std::optional<int> FindCandidateByValue(absl::string_view value,
                                                 const Segments &segments) {
    for (size_t i = 0; i < segments.conversion_segment(0).candidates_size();
         ++i) {
      if (segments.conversion_segment(0).candidate(i).value == value) {
        return i;
      }
    }
    return std::nullopt;
  }

  void SetSupplementalModel(
      const engine::SupplementalModelInterface *supplemental_model) {
    data_and_predictor_->modules.SetSupplementalModel(supplemental_model);
  }

  composer::Composer composer_;
  std::unique_ptr<composer::Table> table_;
  Config config_;
  Request request_;
  commands::Context context_;

 private:
  struct DataAndPredictor {
    engine::Modules modules;
    std::unique_ptr<UserHistoryPredictor> predictor;
  };

  std::unique_ptr<DataAndPredictor> CreateDataAndPredictor() const {
    auto ret = std::make_unique<DataAndPredictor>();
    ret->modules.PresetDictionary(std::make_unique<MockDictionary>());
    CHECK_OK(ret->modules.Init(std::make_unique<testing::MockDataManager>()));
    ret->predictor =
        std::make_unique<UserHistoryPredictor>(ret->modules, false);
    ret->predictor->WaitForSyncer();
    return ret;
  }

  std::unique_ptr<DataAndPredictor> data_and_predictor_;
  mozc::usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;
};

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorTest) {
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);

    // Nothing happen
    {
      Segments segments;
      const ConversionRequest convreq =
          SetUpInputForSuggestion("てすと", &composer_, &segments);
      EXPECT_FALSE(predictor->PredictForRequest(convreq, &segments));
      EXPECT_EQ(segments.segment(0).candidates_size(), 0);
    }

    // Nothing happen
    {
      Segments segments;
      const ConversionRequest convreq =
          SetUpInputForSuggestion("てすと", &composer_, &segments);
      EXPECT_FALSE(predictor->PredictForRequest(convreq, &segments));
      EXPECT_EQ(segments.segment(0).candidates_size(), 0);
    }

    // Insert two items
    {
      Segments segments;
      const ConversionRequest convreq1 = SetUpInputForSuggestion(
          "わたしのなまえはなかのです", &composer_, &segments);
      AddCandidate("私の名前は中野です", &segments);
      predictor->Finish(convreq1, &segments);

      segments.Clear();
      const ConversionRequest convreq2 =
          SetUpInputForSuggestion("わたしの", &composer_, &segments);
      EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
      EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は中野です");
      EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
                  Segment::Candidate::USER_HISTORY_PREDICTOR);

      segments.Clear();
      const ConversionRequest convreq3 =
          SetUpInputForSuggestion("わたしの", &composer_, &segments);
      EXPECT_TRUE(predictor->PredictForRequest(convreq3, &segments));
      EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は中野です");
      EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
                  Segment::Candidate::USER_HISTORY_PREDICTOR);
    }

    // Insert without learning (nothing happen).
    {
      config::Config::HistoryLearningLevel no_learning_levels[] = {
          config::Config::READ_ONLY, config::Config::NO_HISTORY};
      for (config::Config::HistoryLearningLevel level : no_learning_levels) {
        config_.set_history_learning_level(level);

        Segments segments;
        const ConversionRequest convreq1 = SetUpInputForSuggestion(
            "こんにちはさようなら", &composer_, &segments);
        AddCandidate("今日はさようなら", &segments);
        predictor->Finish(convreq1, &segments);

        segments.Clear();
        const ConversionRequest convreq2 =
            SetUpInputForSuggestion("こんにちは", &composer_, &segments);
        EXPECT_FALSE(predictor->PredictForRequest(convreq2, &segments));
        const ConversionRequest convreq3 =
            SetUpInputForSuggestion("こんにちは", &composer_, &segments);
        EXPECT_FALSE(predictor->PredictForRequest(convreq3, &segments));
      }
      config_.set_history_learning_level(config::Config::DEFAULT_HISTORY);
    }

    // sync
    predictor->Sync();
    absl::SleepFor(absl::Milliseconds(500));
  }

  // reload
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);
    Segments segments;

    // turn off
    {
      Segments segments;
      config_.set_use_history_suggest(false);

      const ConversionRequest convreq1 =
          SetUpInputForSuggestion("わたしの", &composer_, &segments);
      EXPECT_FALSE(predictor->PredictForRequest(convreq1, &segments));

      config_.set_use_history_suggest(true);
      config_.set_incognito_mode(true);

      const ConversionRequest convreq2 =
          SetUpInputForSuggestion("わたしの", &composer_, &segments);
      EXPECT_FALSE(predictor->PredictForRequest(convreq2, &segments));

      config_.set_incognito_mode(false);
      config_.set_history_learning_level(config::Config::NO_HISTORY);

      const ConversionRequest convreq3 =
          SetUpInputForSuggestion("わたしの", &composer_, &segments);
      EXPECT_FALSE(predictor->PredictForRequest(convreq3, &segments));
    }

    // turn on
    {
      config::ConfigHandler::GetDefaultConfig(&config_);
    }

    // reproduced
    const ConversionRequest convreq1 =
        SetUpInputForSuggestion("わたしの", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq1, &segments));
    EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は中野です");

    segments.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForSuggestion("わたしの", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
    EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は中野です");

    // Exact Match
    segments.Clear();
    const ConversionRequest convreq3 = SetUpInputForSuggestion(
        "わたしのなまえはなかのです", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq3, &segments));
    EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は中野です");

    segments.Clear();
    const ConversionRequest convreq4 = SetUpInputForSuggestion(
        "わたしのなまえはなかのです", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq4, &segments));
    EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は中野です");

    segments.Clear();
    const ConversionRequest convreq5 =
        SetUpInputForSuggestion("こんにちはさようなら", &composer_, &segments);
    EXPECT_FALSE(predictor->PredictForRequest(convreq5, &segments));

    segments.Clear();
    const ConversionRequest convreq6 =
        SetUpInputForSuggestion("こんにちはさようなら", &composer_, &segments);
    EXPECT_FALSE(predictor->PredictForRequest(convreq6, &segments));

    // Read only mode should show suggestion.
    {
      config_.set_history_learning_level(config::Config::READ_ONLY);
      const ConversionRequest convreq1 =
          SetUpInputForSuggestion("わたしの", &composer_, &segments);
      EXPECT_TRUE(predictor->PredictForRequest(convreq1, &segments));
      EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は中野です");

      segments.Clear();
      const ConversionRequest convreq2 =
          SetUpInputForSuggestion("わたしの", &composer_, &segments);
      EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
      EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は中野です");
      config_.set_history_learning_level(config::Config::DEFAULT_HISTORY);
    }

    // clear
    predictor->ClearAllHistory();
    WaitForSyncer(predictor);
  }

  // nothing happen
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);
    Segments segments;

    // reproduced
    const ConversionRequest convreq1 =
        SetUpInputForSuggestion("わたしの", &composer_, &segments);
    EXPECT_FALSE(predictor->PredictForRequest(convreq1, &segments));

    const ConversionRequest convreq2 =
        SetUpInputForSuggestion("わたしの", &composer_, &segments);
    EXPECT_FALSE(predictor->PredictForRequest(convreq2, &segments));
  }

  // nothing happen
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);
    Segments segments;

    // reproduced
    const ConversionRequest convreq1=
        SetUpInputForSuggestion("わたしの", &composer_, &segments);
    EXPECT_FALSE(predictor->PredictForRequest(convreq1, &segments));

    const ConversionRequest convreq2 =
        SetUpInputForSuggestion("わたしの", &composer_, &segments);
    EXPECT_FALSE(predictor->PredictForRequest(convreq2, &segments));
  }
}

TEST_F(UserHistoryPredictorTest, RemoveUnselectedHistoryPrediction) {
  request_test_util::FillMobileRequest(&request_);

  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();
  WaitForSyncer(predictor);

  auto insert_target = [&]() {
    Segments segments;
    const ConversionRequest convreq =
        SetUpInputForPrediction("わたしの", &composer_, &segments);
    AddCandidate("私の", &segments);
    predictor->Finish(convreq, &segments);
  };

  auto find_target = [&]() {
    Segments segments;
    const ConversionRequest convreq =
        SetUpInputForPrediction("わたしの", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq, &segments));
    return FindCandidateByValue("私の", segments);
  };

  // Returns true if the target is found.
  auto select_target = [&]() {
    Segments segments;
    const ConversionRequest convreq =
        SetUpInputForPrediction("わたしの", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq, &segments));
    EXPECT_TRUE(FindCandidateByValue("私の", segments));
    predictor->Finish(convreq, &segments);
  };

  auto select_other = [&]() {
    Segments segments;
    const ConversionRequest convreq =
        SetUpInputForPrediction("わたしの", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq, &segments));
    EXPECT_TRUE(FindCandidateByValue("私の", segments));
    auto find = FindCandidateByValue("わたしの", segments);
    if (!find) {
      AddCandidate("わたしの", &segments);
      segments.mutable_segment(0)->move_candidate(1, 0);
    } else {
      segments.mutable_segment(0)->move_candidate(find.value(), 0);
    }
    predictor->Finish(convreq, &segments);  // Select "わたしの"
  };

  auto input_other_key = [&]() {
    Segments segments;
    const ConversionRequest convreq =
        SetUpInputForPrediction("てすと", &composer_, &segments);
    predictor->PredictForRequest(convreq, &segments);
    predictor->Finish(convreq, &segments);
  };

  // min selected ratio threshold is 0.05

  {
    insert_target();
    for (int i = 0; i < 20; ++i) {
      EXPECT_TRUE(find_target());
      select_other();
    }
    // select: 1, shown: 1+20, ratio: 1/21 < 0.05
    EXPECT_FALSE(find_target());
  }

  {
    insert_target();
    for (int i = 0; i < 19; ++i) {
      EXPECT_TRUE(find_target());
      select_other();
    }
    // select: 1, shown 1+19, ratio: 1/20 >= 0.05
    EXPECT_TRUE(find_target());

    // other key does not matter
    for (int i = 0; i < 20; ++i) {
      input_other_key();
    }
    EXPECT_TRUE(find_target());

    select_target();  // select: 2, shown 1+19+1, ratio: 2/21 >= 0.05
    for (int i = 0; i < 20; ++i) {
      EXPECT_TRUE(find_target());
      select_other();
    }
    // select: 2, shown: 1+19+1+20, ratio: 2/41 < 0.05
    EXPECT_FALSE(find_target());
  }
}

// We did not support such Segments which has multiple segments and
// has type != CONVERSION.
// To support such Segments, this test case is created separately.
TEST_F(UserHistoryPredictorTest, UserHistoryPredictorTestSuggestion) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Register input histories via Finish method.
  {
    Segments segments;
    const ConversionRequest convreq =
        SetUpInputForSuggestion("かまた", &composer_, &segments);
    AddCandidate(0, "火魔汰", &segments);
    AddSegment("ま", &segments);
    AddCandidate(1, "摩", &segments);
    predictor->Finish(convreq, &segments);

    // All added items must be suggestion entries.
    for (const UserHistoryPredictor::DicCache::Element &element :
         *predictor->dic_) {
      if (!element.next) {
        break;  // Except the last one.
      }
      const user_history_predictor::UserHistory::Entry &entry = element.value;
      EXPECT_TRUE(entry.has_suggestion_freq() && entry.suggestion_freq() == 1);
      EXPECT_TRUE(!entry.has_conversion_freq() && entry.conversion_freq() == 0);
    }
  }

  // Obtain input histories via Predict method.
  {
    Segments segments;
    const ConversionRequest convreq =
        SetUpInputForSuggestion("かま", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq, &segments));
    std::set<std::string, std::less<>> expected_candidates;
    expected_candidates.insert("火魔汰");
    // We can get this entry even if Segmtnts's type is not CONVERSION.
    expected_candidates.insert("火魔汰摩");
    for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
      SCOPED_TRACE(segments.segment(0).candidate(i).value);
      EXPECT_EQ(
          expected_candidates.erase(segments.segment(0).candidate(i).value), 1);
    }
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorPreprocessInput) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  {
    // Commit can be triggered by space in alphanumeric keyboard layout.
    // In this case, trailing white space is included to the key and value.
    Segments segments;
    const ConversionRequest convreq =
        SetUpInputForSuggestion("android ", &composer_, &segments);
    AddCandidate(0, "android ", &segments);
    predictor->Finish(convreq, &segments);
  }

  {
    Segments segments;
    const ConversionRequest convreq =
        SetUpInputForSuggestion("androi", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq, &segments));
    // Preprocessed value should be learned.
    EXPECT_TRUE(FindCandidateByValue("android", segments));
    EXPECT_FALSE(FindCandidateByValue("android ", segments));
  }
}

TEST_F(UserHistoryPredictorTest, DescriptionTest) {
#ifdef DEBUG
  constexpr char kDescription[] = "テスト History";
#else   // DEBUG
  constexpr char kDescription[] = "テスト";
#endif  // DEBUG

  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);

    // Insert two items
    {
      Segments segments;
      const ConversionRequest convreq = SetUpInputForConversion(
          "わたしのなまえはなかのです", &composer_, &segments);
      AddCandidateWithDescription("私の名前は中野です", kDescription,
                                  &segments);
      predictor->Finish(convreq, &segments);

      const ConversionRequest convreq1 =
          SetUpInputForSuggestion("わたしの", &composer_, &segments);
      EXPECT_TRUE(predictor->PredictForRequest(convreq1, &segments));
      EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は中野です");
      EXPECT_EQ(segments.segment(0).candidate(0).description, kDescription);

      segments.Clear();
      const ConversionRequest convreq2 =
          SetUpInputForPrediction("わたしの", &composer_, &segments);
      EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
      EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は中野です");
      EXPECT_EQ(segments.segment(0).candidate(0).description, kDescription);
    }

    // sync
    predictor->Sync();
  }

  // reload
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);
    Segments segments;

    // turn off
    {
      Segments segments;
      config_.set_use_history_suggest(false);
      WaitForSyncer(predictor);

      const ConversionRequest convreq1 =
          SetUpInputForSuggestion("わたしの", &composer_, &segments);
      EXPECT_FALSE(predictor->PredictForRequest(convreq1, &segments));

      config_.set_use_history_suggest(true);
      config_.set_incognito_mode(true);

      const ConversionRequest convreq2 =
          SetUpInputForSuggestion("わたしの", &composer_, &segments);
      EXPECT_FALSE(predictor->PredictForRequest(convreq2, &segments));
    }

    // turn on
    {
      config::ConfigHandler::GetDefaultConfig(&config_);
      WaitForSyncer(predictor);
    }

    // reproduced
    const ConversionRequest convreq1 =
        SetUpInputForSuggestion("わたしの", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq1, &segments));
    EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は中野です");
    EXPECT_EQ(segments.segment(0).candidate(0).description, kDescription);

    segments.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForPrediction("わたしの", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
    EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は中野です");
    EXPECT_EQ(segments.segment(0).candidate(0).description, kDescription);

    // Exact Match
    segments.Clear();
    const ConversionRequest convreq3 = SetUpInputForSuggestion(
        "わたしのなまえはなかのです", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq3, &segments));
    EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は中野です");
    EXPECT_EQ(segments.segment(0).candidate(0).description, kDescription);

    segments.Clear();
    const ConversionRequest convreq4 = SetUpInputForSuggestion(
        "わたしのなまえはなかのです", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq4, &segments));
    EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は中野です");
    EXPECT_EQ(segments.segment(0).candidate(0).description, kDescription);

    // clear
    predictor->ClearAllHistory();
    WaitForSyncer(predictor);
  }

  // nothing happen
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);
    Segments segments;

    // reproduced
    const ConversionRequest convreq1 =
        SetUpInputForSuggestion("わたしの", &composer_, &segments);
    EXPECT_FALSE(predictor->PredictForRequest(convreq1, &segments));

    const ConversionRequest convreq2 =
        SetUpInputForPrediction("わたしの", &composer_, &segments);
    EXPECT_FALSE(predictor->PredictForRequest(convreq2, &segments));
  }

  // nothing happen
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);
    Segments segments;

    // reproduced
    const ConversionRequest convreq1 =
        SetUpInputForSuggestion("わたしの", &composer_, &segments);
    EXPECT_FALSE(predictor->PredictForRequest(convreq1, &segments));

    const ConversionRequest convreq2 =
        SetUpInputForPrediction("わたしの", &composer_, &segments);
    EXPECT_FALSE(predictor->PredictForRequest(convreq2, &segments));
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorUnusedHistoryTest) {
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);

    Segments segments;
    const ConversionRequest convreq1 = SetUpInputForSuggestion(
        "わたしのなまえはなかのです", &composer_, &segments);
    AddCandidate("私の名前は中野です", &segments);

    // once
    predictor->Finish(convreq1, &segments);

    segments.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForConversion("ひろすえりょうこ", &composer_, &segments);
    AddCandidate("広末涼子", &segments);

    // conversion
    predictor->Finish(convreq2, &segments);

    // sync
    predictor->Sync();
  }

  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);
    Segments segments;

    const ConversionRequest convreq1 =
        SetUpInputForSuggestion("わたしの", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq1, &segments));
    EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は中野です");

    segments.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForSuggestion("ひろすえ", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
    EXPECT_EQ(segments.segment(0).candidate(0).value, "広末涼子");

    predictor->ClearUnusedHistory();
    WaitForSyncer(predictor);

    segments.Clear();
    const ConversionRequest convreq3 =
        SetUpInputForSuggestion("わたしの", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq3, &segments));
    EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は中野です");

    segments.Clear();
    const ConversionRequest convreq4 =
        SetUpInputForSuggestion("ひろすえ", &composer_, &segments);
    EXPECT_FALSE(predictor->PredictForRequest(convreq4, &segments));

    predictor->Sync();
  }

  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);
    Segments segments;

    const ConversionRequest convreq1 =
        SetUpInputForSuggestion("わたしの", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq1, &segments));
    EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は中野です");

    segments.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForSuggestion("ひろすえ", &composer_, &segments);
    EXPECT_FALSE(predictor->PredictForRequest(convreq2, &segments));
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorRevertTest) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments, segments2;
  const ConversionRequest convreq1 = SetUpInputForConversion(
      "わたしのなまえはなかのです", &composer_, &segments);
  AddCandidate("私の名前は中野です", &segments);

  predictor->Finish(convreq1, &segments);

  // Before Revert, Suggest works
  const ConversionRequest convreq2 =
      SetUpInputForSuggestion("わたしの", &composer_, &segments2);
  EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments2));
  EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は中野です");

  // Call revert here
  predictor->Revert(&segments);

  segments.Clear();
  const ConversionRequest convreq3 =
      SetUpInputForSuggestion("わたしの", &composer_, &segments);

  EXPECT_FALSE(predictor->PredictForRequest(convreq3, &segments));
  EXPECT_EQ(segments.segment(0).candidates_size(), 0);

  EXPECT_FALSE(predictor->PredictForRequest(convreq3, &segments));
  EXPECT_EQ(segments.segment(0).candidates_size(), 0);
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorClearTest) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  WaitForSyncer(predictor);

  // input "testtest" 10 times
  for (int i = 0; i < 10; ++i) {
    Segments segments;
    const ConversionRequest convreq =
        SetUpInputForConversion("testtest", &composer_, &segments);
    AddCandidate("テストテスト", &segments);
    predictor->Finish(convreq, &segments);
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  // input "testtest" 1 time
  for (int i = 0; i < 1; ++i) {
    Segments segments;
    const ConversionRequest convreq =
        SetUpInputForConversion("testtest", &composer_, &segments);
    AddCandidate("テストテスト", &segments);
    predictor->Finish(convreq, &segments);
  }

  // frequency is cleared as well.
  {
    Segments segments;
    const ConversionRequest convreq1 =
        SetUpInputForSuggestion("t", &composer_, &segments);
    EXPECT_FALSE(predictor->PredictForRequest(convreq1, &segments));

    segments.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForSuggestion("testte", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorTrailingPunctuation) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;

  const ConversionRequest convreq1 = SetUpInputForConversion(
      "わたしのなまえはなかのです", &composer_, &segments);

  AddCandidate(0, "私の名前は中野です", &segments);

  AddSegment("。", &segments);
  AddCandidate(1, "。", &segments);

  predictor->Finish(convreq1, &segments);

  segments.Clear();
  const ConversionRequest convreq2 =
      SetUpInputForPrediction("わたしの", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
  EXPECT_EQ(segments.segment(0).candidates_size(), 2);
  EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は中野です");
  EXPECT_EQ(segments.segment(0).candidate(1).value, "私の名前は中野です。");

  segments.Clear();
  const ConversionRequest convreq3 =
      SetUpInputForSuggestion("わたしの", &composer_, &segments);

  EXPECT_TRUE(predictor->PredictForRequest(convreq3, &segments));
  EXPECT_EQ(segments.segment(0).candidates_size(), 2);
  EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は中野です");
  EXPECT_EQ(segments.segment(0).candidate(1).value, "私の名前は中野です。");
}

TEST_F(UserHistoryPredictorTest, TrailingPunctuationMobile) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();
  request_test_util::FillMobileRequest(&request_);
  Segments segments;

  const ConversionRequest convreq1 =
      SetUpInputForConversion("です。", &composer_, &segments);

  AddCandidate(0, "です。", &segments);

  predictor->Finish(convreq1, &segments);

  segments.Clear();

  const ConversionRequest convreq2 =
      SetUpInputForPrediction("です", &composer_, &segments);
  EXPECT_FALSE(predictor->PredictForRequest(convreq2, &segments));
}

TEST_F(UserHistoryPredictorTest, HistoryToPunctuation) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;

  // Scenario 1: A user have committed "亜" by prediction and then commit "。".
  // Then, the unigram "亜" is learned but the bigram "亜。" shouldn't.
  const ConversionRequest convreq1 =
      SetUpInputForPrediction("あ", &composer_, &segments);
  AddCandidate(0, "亜", &segments);
  predictor->Finish(convreq1, &segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  AddSegment("。", &segments);
  AddCandidate(1, "。", &segments);
  predictor->Finish(convreq1, &segments);

  segments.Clear();
  const ConversionRequest convreq2 =
      SetUpInputForPrediction("あ", &composer_, &segments);  // "あ"
  ASSERT_TRUE(predictor->PredictForRequest(convreq2, &segments))
      << segments.DebugString();
  EXPECT_EQ(segments.segment(0).candidate(0).value, "亜");

  segments.Clear();

  // Scenario 2: the opposite case to Scenario 1, i.e., "。亜".  Nothing is
  // suggested from symbol "。".
  const ConversionRequest convreq3 =
      SetUpInputForPrediction("。", &composer_, &segments);
  AddCandidate(0, "。", &segments);
  predictor->Finish(convreq3, &segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  AddSegment("あ", &segments);
  AddCandidate(1, "亜", &segments);
  predictor->Finish(convreq3, &segments);

  segments.Clear();
  const ConversionRequest convreq4 =
      SetUpInputForPrediction("。", &composer_, &segments);  // "。"
  EXPECT_FALSE(predictor->PredictForRequest(convreq4, &segments))
      << segments.DebugString();

  segments.Clear();

  // Scenario 3: If the history segment looks like a sentence and committed
  // value is a punctuation, the concatenated entry is also learned.
  const ConversionRequest convreq5 =
      SetUpInputForPrediction("おつかれさまです", &composer_, &segments);
  AddCandidate(0, "お疲れ様です", &segments);
  predictor->Finish(convreq5, &segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  AddSegment("。", &segments);
  AddCandidate(1, "。", &segments);
  predictor->Finish(convreq5, &segments);

  segments.Clear();
  const ConversionRequest convreq6 =
      SetUpInputForPrediction("おつかれ", &composer_, &segments);
  ASSERT_TRUE(predictor->PredictForRequest(convreq6, &segments))
      << segments.DebugString();
  EXPECT_EQ(segments.segment(0).candidate(0).value, "お疲れ様です");
  EXPECT_EQ(segments.segment(0).candidate(1).value, "お疲れ様です。");
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorPrecedingPunctuation) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;

  const ConversionRequest convreq1 =
      SetUpInputForConversion("。", &composer_, &segments);
  AddCandidate(0, "。", &segments);

  AddSegment("わたしのなまえはなかのです", &segments);

  AddCandidate(1, "私の名前は中野です", &segments);

  predictor->Finish(convreq1, &segments);

  segments.Clear();
  const ConversionRequest convreq2 =
      SetUpInputForPrediction("わたしの", &composer_, &segments);

  EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
  EXPECT_EQ(segments.segment(0).candidates_size(), 1);
  EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は中野です");

  segments.Clear();
  const ConversionRequest convreq3 =
      SetUpInputForSuggestion("わたしの", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq3, &segments));
  EXPECT_EQ(segments.segment(0).candidates_size(), 1);
  EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は中野です");
}

namespace {
struct StartsWithPunctuationsTestData {
  const char *first_character;
  bool expected_result;
};
}  // namespace

TEST_F(UserHistoryPredictorTest, StartsWithPunctuations) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  const StartsWithPunctuationsTestData kTestCases[] = {
      {"。", false}, {"、", false},    {"？", false},
      {"！", false}, {"あああ", true},
  };

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    WaitForSyncer(predictor);
    predictor->ClearAllHistory();
    WaitForSyncer(predictor);

    Segments segments;
    const std::string first_char = kTestCases[i].first_character;
    {
      // Learn from two segments
      const ConversionRequest convreq =
          SetUpInputForConversion(first_char, &composer_, &segments);
      AddCandidate(0, first_char, &segments);
      AddSegment("てすとぶんしょう", &segments);
      AddCandidate(1, "テスト文章", &segments);
      predictor->Finish(convreq, &segments);
    }
    segments.Clear();
    {
      // Learn from one segment
      const ConversionRequest convreq = SetUpInputForConversion(
          first_char + "てすとぶんしょう", &composer_, &segments);
      AddCandidate(0, first_char + "テスト文章", &segments);
      predictor->Finish(convreq, &segments);
    }
    segments.Clear();
    {
      // Suggestion
      const ConversionRequest convreq =
          SetUpInputForSuggestion(first_char, &composer_, &segments);
      AddCandidate(0, first_char, &segments);
      EXPECT_EQ(predictor->PredictForRequest(convreq, &segments),
                kTestCases[i].expected_result)
          << "Suggest from " << first_char;
    }
    segments.Clear();
    {
      // Prediction
      const ConversionRequest convreq =
          SetUpInputForPrediction(first_char, &composer_, &segments);
      EXPECT_EQ(predictor->PredictForRequest(convreq, &segments),
                kTestCases[i].expected_result)
          << "Predict from " << first_char;
    }
  }
}

TEST_F(UserHistoryPredictorTest, ZeroQuerySuggestionTest) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  request_.set_zero_query_suggestion(true);

  commands::Request non_zero_query_request;
  non_zero_query_request.set_zero_query_suggestion(false);
  commands::Context context;
  Segments segments;

  // No history segments
  segments.Clear();
  const ConversionRequest convreq =
      SetUpInputForSuggestion("", &composer_, &segments);
  EXPECT_FALSE(predictor->PredictForRequest(convreq, &segments));

  {
    segments.Clear();

    const ConversionRequest convreq1 =
        SetUpInputForConversion("たろうは", &composer_, &segments);
    AddCandidate(0, "太郎は", &segments);
    predictor->Finish(convreq1, &segments);

    const ConversionRequest convreq2 = SetUpInputForConversionWithHistory(
        "はなこに", "たろうは", "太郎は", &composer_, &segments);
    AddCandidate(1, "花子に", &segments);
    predictor->Finish(convreq2, &segments);

    const ConversionRequest convreq3 = SetUpInputForConversionWithHistory(
        "きょうと", "たろうは", "太郎は", &composer_, &segments);
    AddCandidate(1, "京都", &segments);
    absl::SleepFor(absl::Seconds(2));
    predictor->Finish(convreq3, &segments);

    const ConversionRequest convreq4 = SetUpInputForConversionWithHistory(
        "おおさか", "たろうは", "太郎は", &composer_, &segments);
    AddCandidate(1, "大阪", &segments);
    absl::SleepFor(absl::Seconds(2));
    predictor->Finish(convreq4, &segments);

    // Zero query suggestion is disabled.
    SetUpInputForSuggestionWithHistory("", "たろうは", "太郎は", &composer_,
                                       &segments);
    // convreq5 is not zero query suggestion unlike other convreqs.
    ConversionRequest convreq5(composer_, non_zero_query_request, context,
                               &config_);
    EXPECT_FALSE(predictor->PredictForRequest(convreq5, &segments));

    const ConversionRequest convreq6 = SetUpInputForSuggestionWithHistory(
        "", "たろうは", "太郎は", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq6, &segments));
    ASSERT_EQ(2, segments.segments_size());
    // last-pushed segment is "大阪"
    EXPECT_EQ(segments.segment(1).candidate(0).value, "大阪");
    EXPECT_EQ(segments.segment(1).candidate(0).key, "おおさか");
    EXPECT_TRUE(segments.segment(1).candidate(0).source_info &
                Segment::Candidate::USER_HISTORY_PREDICTOR);

    for (const char *key : {"は", "た", "き", "お"}) {
      const ConversionRequest convreq = SetUpInputForSuggestionWithHistory(
          key, "たろうは", "太郎は", &composer_, &segments);
      EXPECT_TRUE(predictor->PredictForRequest(convreq, &segments));
    }
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    segments.Clear();
    const ConversionRequest convreq1 =
        SetUpInputForConversion("たろうは", &composer_, &segments);
    AddCandidate(0, "太郎は", &segments);

    AddSegment("はなこに", &segments);
    AddCandidate(1, "花子に", &segments);
    predictor->Finish(convreq1, &segments);

    segments.Clear();
    ConversionRequest convreq2 =
        SetUpInputForConversion("たろうは", &composer_, &segments);
    AddCandidate(0, "太郎は", &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
    convreq2.set_request_type(ConversionRequest::SUGGESTION);

    // Zero query suggestion is disabled.
    ConversionRequest non_zero_query_convreq(composer_, non_zero_query_request,
                                             context, &config_);
    AddSegment("", &segments);  // empty request
    EXPECT_FALSE(
        predictor->PredictForRequest(non_zero_query_convreq, &segments));

    segments.pop_back_segment();
    AddSegment("", &segments);  // empty request
    EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));

    segments.pop_back_segment();
    AddSegment("は", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));

    segments.pop_back_segment();
    AddSegment("た", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
  }
}

TEST_F(UserHistoryPredictorTest, MultiSegmentsMultiInput) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;

  const ConversionRequest convreq1 =
      SetUpInputForConversion("たろうは", &composer_, &segments);
  AddCandidate(0, "太郎は", &segments);
  predictor->Finish(convreq1, &segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  AddSegment("はなこに", &segments);
  AddCandidate(1, "花子に", &segments);
  predictor->Finish(convreq1, &segments);
  segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);

  segments.clear_conversion_segments();
  AddSegment("むずかしい", &segments);
  AddCandidate(2, "難しい", &segments);
  predictor->Finish(convreq1, &segments);
  segments.mutable_segment(2)->set_segment_type(Segment::HISTORY);

  segments.clear_conversion_segments();
  AddSegment("ほんを", &segments);
  AddCandidate(3, "本を", &segments);
  predictor->Finish(convreq1, &segments);
  segments.mutable_segment(3)->set_segment_type(Segment::HISTORY);

  segments.clear_conversion_segments();
  AddSegment("よませた", &segments);
  AddCandidate(4, "読ませた", &segments);
  predictor->Finish(convreq1, &segments);

  segments.Clear();
  const ConversionRequest convreq2 =
      SetUpInputForSuggestion("た", &composer_, &segments);
  EXPECT_FALSE(predictor->PredictForRequest(convreq2, &segments));

  segments.Clear();
  const ConversionRequest convreq3 =
      SetUpInputForSuggestion("たろうは", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq3, &segments));

  segments.Clear();
  const ConversionRequest convreq4 =
      SetUpInputForSuggestion("ろうは", &composer_, &segments);
  EXPECT_FALSE(predictor->PredictForRequest(convreq4, &segments));

  segments.Clear();
  const ConversionRequest convreq5 =
      SetUpInputForSuggestion("たろうははな", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq5, &segments));

  segments.Clear();
  const ConversionRequest convreq6 =
      SetUpInputForSuggestion("はなこにむ", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq6, &segments));

  segments.Clear();
  const ConversionRequest convreq7 =
      SetUpInputForSuggestion("むずかし", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq7, &segments));

  segments.Clear();
  const ConversionRequest convreq8 =
      SetUpInputForSuggestion("はなこにむずかしいほ", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq8, &segments));

  segments.Clear();
  const ConversionRequest convreq9 =
      SetUpInputForSuggestion("ほんをよま", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq9, &segments));

  absl::SleepFor(absl::Seconds(1));

  // Add new entry "たろうはよしこに/太郎は良子に"
  segments.Clear();
  const ConversionRequest convreq10 =
      SetUpInputForConversion("たろうは", &composer_, &segments);
  AddCandidate(0, "太郎は", &segments);
  predictor->Finish(convreq10, &segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  AddSegment("よしこに", &segments);
  AddCandidate(1, "良子に", &segments);
  predictor->Finish(convreq10, &segments);
  segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);

  segments.Clear();
  const ConversionRequest convreq11 =
      SetUpInputForSuggestion("たろうは", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq11, &segments));
  EXPECT_EQ(segments.segment(0).candidate(0).value, "太郎は良子に");
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
}

TEST_F(UserHistoryPredictorTest, MultiSegmentsSingleInput) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;

  const ConversionRequest convreq1 =
      SetUpInputForConversion("たろうは", &composer_, &segments);
  AddCandidate(0, "太郎は", &segments);

  AddSegment("はなこに", &segments);
  AddCandidate(1, "花子に", &segments);

  AddSegment("むずかしい", &segments);
  AddCandidate(2, "難しい", &segments);

  AddSegment("ほんを", &segments);
  AddCandidate(3, "本を", &segments);

  AddSegment("よませた", &segments);
  AddCandidate(4, "読ませた", &segments);

  predictor->Finish(convreq1, &segments);

  segments.Clear();
  const ConversionRequest convreq2 =
      SetUpInputForSuggestion("たろうは", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));

  segments.Clear();
  const ConversionRequest convreq3 =
      SetUpInputForSuggestion("た", &composer_, &segments);
  EXPECT_FALSE(predictor->PredictForRequest(convreq3, &segments));

  segments.Clear();
  const ConversionRequest convreq4 =
      SetUpInputForSuggestion("たろうははな", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq4, &segments));

  segments.Clear();
  const ConversionRequest convreq5 =
      SetUpInputForSuggestion("ろうははな", &composer_, &segments);
  EXPECT_FALSE(predictor->PredictForRequest(convreq5, &segments));

  segments.Clear();
  const ConversionRequest convreq6 =
      SetUpInputForSuggestion("はなこにむ", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq6, &segments));

  segments.Clear();
  const ConversionRequest convreq7 =
      SetUpInputForSuggestion("むずかし", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq7, &segments));

  segments.Clear();
  const ConversionRequest convreq8 =
      SetUpInputForSuggestion("はなこにむずかしいほ", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq8, &segments));

  segments.Clear();
  const ConversionRequest convreq9 =
      SetUpInputForSuggestion("ほんをよま", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq9, &segments));

  absl::SleepFor(absl::Seconds(1));

  // Add new entry "たろうはよしこに/太郎は良子に"
  segments.Clear();
  const ConversionRequest convreq10 =
      SetUpInputForConversion("たろうは", &composer_, &segments);
  AddCandidate(0, "太郎は", &segments);
  predictor->Finish(convreq10, &segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  AddSegment("よしこに", &segments);
  AddCandidate(1, "良子に", &segments);
  predictor->Finish(convreq10, &segments);
  segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);

  segments.Clear();
  const ConversionRequest convreq11 =
      SetUpInputForSuggestion("たろうは", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq11, &segments));
  EXPECT_EQ(segments.segment(0).candidate(0).value, "太郎は良子に");
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
}

TEST_F(UserHistoryPredictorTest, Regression2843371Case1) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;

  const ConversionRequest convreq1 =
      SetUpInputForConversion("とうきょうは", &composer_, &segments);
  AddCandidate(0, "東京は", &segments);

  AddSegment("、", &segments);
  AddCandidate(1, "、", &segments);

  AddSegment("にほんです", &segments);
  AddCandidate(2, "日本です", &segments);

  AddSegment("。", &segments);
  AddCandidate(3, "。", &segments);

  predictor->Finish(convreq1, &segments);

  segments.Clear();

  absl::SleepFor(absl::Seconds(1));

  const ConversionRequest convreq2 =
      SetUpInputForConversion("らーめんは", &composer_, &segments);
  AddCandidate(0, "ラーメンは", &segments);

  AddSegment("、", &segments);
  AddCandidate(1, "、", &segments);

  AddSegment("めんるいです", &segments);
  AddCandidate(2, "麺類です", &segments);

  AddSegment("。", &segments);
  AddCandidate(3, "。", &segments);

  predictor->Finish(convreq2, &segments);

  segments.Clear();

  const ConversionRequest convreq3 =
      SetUpInputForSuggestion("とうきょうは、", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq3, &segments));

  EXPECT_EQ(segments.segment(0).candidate(0).value, "東京は、日本です");
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
}

TEST_F(UserHistoryPredictorTest, Regression2843371Case2) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;

  const ConversionRequest convreq1 =
      SetUpInputForConversion("えど", &composer_, &segments);
  AddCandidate(0, "江戸", &segments);

  AddSegment("(", &segments);
  AddCandidate(1, "(", &segments);

  AddSegment("とうきょう", &segments);
  AddCandidate(2, "東京", &segments);

  AddSegment(")", &segments);
  AddCandidate(3, ")", &segments);

  AddSegment("は", &segments);
  AddCandidate(4, "は", &segments);

  AddSegment("えぞ", &segments);
  AddCandidate(5, "蝦夷", &segments);

  AddSegment("(", &segments);
  AddCandidate(6, "(", &segments);

  AddSegment("ほっかいどう", &segments);
  AddCandidate(7, "北海道", &segments);

  AddSegment(")", &segments);
  AddCandidate(8, ")", &segments);

  AddSegment("ではない", &segments);
  AddCandidate(9, "ではない", &segments);

  AddSegment("。", &segments);
  AddCandidate(10, "。", &segments);

  predictor->Finish(convreq1, &segments);

  segments.Clear();

  const ConversionRequest convreq2 =
      SetUpInputForSuggestion("えど(", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
  EXPECT_EQ(segments.segment(0).candidate(0).value, "江戸(東京");
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);

  EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));

  EXPECT_EQ(segments.segment(0).candidate(0).value, "江戸(東京");
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
}

TEST_F(UserHistoryPredictorTest, Regression2843371Case3) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;

  const ConversionRequest convreq1 =
      SetUpInputForConversion("「", &composer_, &segments);
  AddCandidate(0, "「", &segments);

  AddSegment("やま", &segments);
  AddCandidate(1, "山", &segments);

  AddSegment("」", &segments);
  AddCandidate(2, "」", &segments);

  AddSegment("は", &segments);
  AddCandidate(3, "は", &segments);

  AddSegment("たかい", &segments);
  AddCandidate(4, "高い", &segments);

  AddSegment("。", &segments);
  AddCandidate(5, "。", &segments);

  predictor->Finish(convreq1, &segments);

  absl::SleepFor(absl::Seconds(2));

  segments.Clear();

  const ConversionRequest convreq2 =
      SetUpInputForConversion("「", &composer_, &segments);
  AddCandidate(0, "「", &segments);

  AddSegment("うみ", &segments);
  AddCandidate(1, "海", &segments);

  AddSegment("」", &segments);
  AddCandidate(2, "」", &segments);

  AddSegment("は", &segments);
  AddCandidate(3, "は", &segments);

  AddSegment("ふかい", &segments);
  AddCandidate(4, "深い", &segments);

  AddSegment("。", &segments);
  AddCandidate(5, "。", &segments);

  predictor->Finish(convreq2, &segments);

  segments.Clear();

  const ConversionRequest convreq3 =
      SetUpInputForSuggestion("「やま」は", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq3, &segments));

  EXPECT_EQ(segments.segment(0).candidate(0).value, "「山」は高い");
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
}

TEST_F(UserHistoryPredictorTest, Regression2843775) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;

  const ConversionRequest convreq1 =
      SetUpInputForConversion("そうです", &composer_, &segments);
  AddCandidate(0, "そうです", &segments);

  AddSegment("。よろしくおねがいします", &segments);
  AddCandidate(1, "。よろしくお願いします", &segments);

  predictor->Finish(convreq1, &segments);

  segments.Clear();

  const ConversionRequest convreq2 =
      SetUpInputForSuggestion("そうです", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));

  EXPECT_EQ(segments.segment(0).candidate(0).value,
            "そうです。よろしくお願いします");
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
}

TEST_F(UserHistoryPredictorTest, DuplicateString) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;

  const ConversionRequest convreq1 =
      SetUpInputForConversion("らいおん", &composer_, &segments);
  AddCandidate(0, "ライオン", &segments);

  AddSegment("（", &segments);
  AddCandidate(1, "（", &segments);

  AddSegment("もうじゅう", &segments);
  AddCandidate(2, "猛獣", &segments);

  AddSegment("）と", &segments);
  AddCandidate(3, "）と", &segments);

  AddSegment("ぞうりむし", &segments);
  AddCandidate(4, "ゾウリムシ", &segments);

  AddSegment("（", &segments);
  AddCandidate(5, "（", &segments);

  AddSegment("びせいぶつ", &segments);
  AddCandidate(6, "微生物", &segments);

  AddSegment("）", &segments);
  AddCandidate(7, "）", &segments);

  predictor->Finish(convreq1, &segments);

  segments.Clear();

  const ConversionRequest convreq2 =
      SetUpInputForSuggestion("ぞうりむし", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));

  for (int i = 0; i < segments.segment(0).candidates_size(); ++i) {
    EXPECT_EQ(segments.segment(0).candidate(i).value.find("猛獣"),
              std::string::npos);  // "猛獣" should not be found
  }

  segments.Clear();

  const ConversionRequest convreq3 =
      SetUpInputForSuggestion("らいおん", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq3, &segments));

  for (int i = 0; i < segments.segment(0).candidates_size(); ++i) {
    EXPECT_EQ(segments.segment(0).candidate(i).value.find("ライオン（微生物"),
              std::string::npos);
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
  std::string key;
  std::string value;
  Command() : type(LOOKUP) {}
};

TEST_F(UserHistoryPredictorTest, SyncTest) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  WaitForSyncer(predictor);

  absl::BitGen gen;
  std::vector<Command> commands(10000);
  for (size_t i = 0; i < commands.size(); ++i) {
    commands[i].key = std::to_string(static_cast<uint32_t>(i)) + "key";
    commands[i].value = std::to_string(static_cast<uint32_t>(i)) + "value";
    const int n = absl::Uniform(gen, 0, 100);
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
        predictor->Sync();
        break;
      case Command::WAIT:
        WaitForSyncer(predictor);
        break;
      case Command::INSERT: {
        segments.Clear();
        const ConversionRequest convreq =
            SetUpInputForConversion(commands[i].key, &composer_, &segments);
        AddCandidate(commands[i].value, &segments);
        predictor->Finish(convreq, &segments);
        break;
      }
      case Command::LOOKUP: {
        segments.Clear();
        const ConversionRequest convreq =
            SetUpInputForSuggestion(commands[i].key, &composer_, &segments);
        predictor->PredictForRequest(convreq, &segments);
        break;
      }
      default:
        break;
    }
  }
}

TEST_F(UserHistoryPredictorTest, GetMatchTypeTest) {
  EXPECT_EQ(UserHistoryPredictor::GetMatchType("test", ""),
            UserHistoryPredictor::NO_MATCH);

  EXPECT_EQ(UserHistoryPredictor::GetMatchType("", ""),
            UserHistoryPredictor::NO_MATCH);

  EXPECT_EQ(UserHistoryPredictor::GetMatchType("", "test"),
            UserHistoryPredictor::LEFT_EMPTY_MATCH);

  EXPECT_EQ(UserHistoryPredictor::GetMatchType("foo", "bar"),
            UserHistoryPredictor::NO_MATCH);

  EXPECT_EQ(UserHistoryPredictor::GetMatchType("foo", "foo"),
            UserHistoryPredictor::EXACT_MATCH);

  EXPECT_EQ(UserHistoryPredictor::GetMatchType("foo", "foobar"),
            UserHistoryPredictor::LEFT_PREFIX_MATCH);

  EXPECT_EQ(UserHistoryPredictor::GetMatchType("foobar", "foo"),
            UserHistoryPredictor::RIGHT_PREFIX_MATCH);
}

TEST_F(UserHistoryPredictorTest, FingerPrintTest) {
  constexpr char kKey[] = "abc";
  constexpr char kValue[] = "ABC";

  UserHistoryPredictor::Entry entry;
  entry.set_key(kKey);
  entry.set_value(kValue);

  const uint32_t entry_fp1 = UserHistoryPredictor::Fingerprint(kKey, kValue);
  const uint32_t entry_fp2 = UserHistoryPredictor::EntryFingerprint(entry);

  const uint32_t entry_fp3 = UserHistoryPredictor::Fingerprint(
      kKey, kValue, UserHistoryPredictor::Entry::DEFAULT_ENTRY);

  const uint32_t entry_fp4 = UserHistoryPredictor::Fingerprint(
      kKey, kValue, UserHistoryPredictor::Entry::CLEAN_ALL_EVENT);

  const uint32_t entry_fp5 = UserHistoryPredictor::Fingerprint(
      kKey, kValue, UserHistoryPredictor::Entry::CLEAN_UNUSED_EVENT);

  Segment segment;
  segment.set_key(kKey);
  Segment::Candidate *c = segment.add_candidate();
  c->key = kKey;
  c->content_key = kKey;
  c->value = kValue;
  c->content_value = kValue;

  const uint32_t segment_fp = UserHistoryPredictor::SegmentFingerprint(segment);

  Segment segment2;
  segment2.set_key("ab");
  Segment::Candidate *c2 = segment2.add_candidate();
  c2->key = kKey;
  c2->content_key = kKey;
  c2->value = kValue;
  c2->content_value = kValue;

  const uint32_t segment_fp2 =
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
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();

  UserHistoryPredictor::Entry entry;

  EXPECT_TRUE(predictor->IsValidEntry(entry));

  entry.set_key("key");
  entry.set_value("value");

  EXPECT_TRUE(predictor->IsValidEntry(entry));
  EXPECT_TRUE(predictor->IsValidEntryIgnoringRemovedField(entry));

  entry.set_removed(true);
  EXPECT_FALSE(predictor->IsValidEntry(entry));
  EXPECT_TRUE(predictor->IsValidEntryIgnoringRemovedField(entry));

  entry.set_removed(false);
  EXPECT_TRUE(predictor->IsValidEntry(entry));
  EXPECT_TRUE(predictor->IsValidEntryIgnoringRemovedField(entry));

  entry.set_entry_type(UserHistoryPredictor::Entry::CLEAN_ALL_EVENT);
  EXPECT_FALSE(predictor->IsValidEntry(entry));
  EXPECT_FALSE(predictor->IsValidEntryIgnoringRemovedField(entry));

  entry.set_entry_type(UserHistoryPredictor::Entry::CLEAN_UNUSED_EVENT);
  EXPECT_FALSE(predictor->IsValidEntry(entry));
  EXPECT_FALSE(predictor->IsValidEntryIgnoringRemovedField(entry));

  entry.set_removed(true);
  EXPECT_FALSE(predictor->IsValidEntry(entry));
  EXPECT_FALSE(predictor->IsValidEntryIgnoringRemovedField(entry));

  entry.Clear();
  EXPECT_TRUE(predictor->IsValidEntry(entry));
  EXPECT_TRUE(predictor->IsValidEntryIgnoringRemovedField(entry));

  entry.Clear();
  entry.set_key("key");
  entry.set_value("value");
  entry.set_description("絵文字");
  EXPECT_TRUE(predictor->IsValidEntry(entry));
  EXPECT_TRUE(predictor->IsValidEntryIgnoringRemovedField(entry));

  // An android pua emoji. It is obsolete and should return false.
  *entry.mutable_value() = Util::CodepointToUtf8(0xFE000);
  EXPECT_FALSE(predictor->IsValidEntry(entry));
  EXPECT_FALSE(predictor->IsValidEntryIgnoringRemovedField(entry));

  SuppressionDictionary *d = GetSuppressionDictionary();
  DCHECK(d);
  d->Lock();
  d->AddEntry("foo", "bar");
  d->UnLock();

  entry.set_key("key");
  entry.set_value("value");
  EXPECT_TRUE(predictor->IsValidEntry(entry));
  EXPECT_TRUE(predictor->IsValidEntryIgnoringRemovedField(entry));

  entry.set_key("foo");
  entry.set_value("bar");
  EXPECT_FALSE(predictor->IsValidEntry(entry));
  EXPECT_FALSE(predictor->IsValidEntryIgnoringRemovedField(entry));

  d->Lock();
  d->Clear();
  d->UnLock();
}

TEST_F(UserHistoryPredictorTest, IsValidSuggestion) {
  UserHistoryPredictor::Entry entry;

  EXPECT_FALSE(UserHistoryPredictor::IsValidSuggestion(
      UserHistoryPredictor::DEFAULT, 1, entry));

  entry.set_bigram_boost(true);
  EXPECT_TRUE(UserHistoryPredictor::IsValidSuggestion(
      UserHistoryPredictor::DEFAULT, 1, entry));

  entry.set_bigram_boost(false);
  EXPECT_TRUE(UserHistoryPredictor::IsValidSuggestion(
      UserHistoryPredictor::ZERO_QUERY_SUGGESTION, 1, entry));

  entry.set_bigram_boost(false);
  entry.set_conversion_freq(10);
  EXPECT_TRUE(UserHistoryPredictor::IsValidSuggestion(
      UserHistoryPredictor::DEFAULT, 1, entry));
}

TEST_F(UserHistoryPredictorTest, IsValidSuggestionForMixedConversion) {
  UserHistoryPredictor::Entry entry;
  const ConversionRequest conversion_request;

  entry.set_suggestion_freq(1);
  EXPECT_TRUE(UserHistoryPredictor::IsValidSuggestionForMixedConversion(
      conversion_request, 1, entry));

  entry.set_value("よろしくおねがいします。");  // too long
  EXPECT_FALSE(UserHistoryPredictor::IsValidSuggestionForMixedConversion(
      conversion_request, 1, entry));
}

TEST_F(UserHistoryPredictorTest, EntryPriorityQueueTest) {
  // removed automatically
  constexpr int kSize = 10000;
  {
    UserHistoryPredictor::EntryPriorityQueue queue;
    for (int i = 0; i < 10000; ++i) {
      EXPECT_NE(nullptr, queue.NewEntry());
    }
  }

  {
    UserHistoryPredictor::EntryPriorityQueue queue;
    std::vector<UserHistoryPredictor::Entry *> expected;
    for (int i = 0; i < kSize; ++i) {
      UserHistoryPredictor::Entry *entry = queue.NewEntry();
      entry->set_key("test" + std::to_string(i));
      entry->set_value("test" + std::to_string(i));
      entry->set_last_access_time(i + 1000);
      expected.push_back(entry);
      EXPECT_TRUE(queue.Push(entry));
    }

    int n = kSize - 1;
    while (true) {
      const UserHistoryPredictor::Entry *entry = queue.Pop();
      if (entry == nullptr) {
        break;
      }
      EXPECT_EQ(entry, expected[n]);
      --n;
    }
    EXPECT_EQ(n, -1);
  }

  {
    UserHistoryPredictor::EntryPriorityQueue queue;
    for (int i = 0; i < 5; ++i) {
      UserHistoryPredictor::Entry *entry = queue.NewEntry();
      entry->set_key("test");
      entry->set_value("test");
      queue.Push(entry);
    }
    EXPECT_EQ(queue.size(), 1);

    for (int i = 0; i < 5; ++i) {
      UserHistoryPredictor::Entry *entry = queue.NewEntry();
      entry->set_key("foo");
      entry->set_value("bar");
      queue.Push(entry);
    }

    EXPECT_EQ(queue.size(), 2);
  }
}

namespace {

std::string RemoveLastCodepointCharacter(const absl::string_view input) {
  const size_t codepoint_count = Util::CharsLen(input);
  if (codepoint_count == 0) {
    return "";
  }

  size_t codepoint_processed = 0;
  std::string output;
  for (ConstChar32Iterator iter(input);
       !iter.Done() && (codepoint_processed < codepoint_count - 1);
       iter.Next(), ++codepoint_processed) {
    Util::CodepointToUtf8Append(iter.Get(), &output);
  }
  return output;
}

struct PrivacySensitiveTestData {
  bool is_sensitive;
  const char *scenario_description;
  const char *input;
  const char *output;
};

constexpr bool kSensitive = true;
constexpr bool kNonSensitive = false;

const PrivacySensitiveTestData kNonSensitiveCases[] = {
    {kNonSensitive,  // We might want to revisit this behavior
     "Type privacy sensitive number but it is committed as full-width number "
     "by mistake.",
     "0007", "０００７"},
    {kNonSensitive, "Type a ZIP number.", "100-0001", "東京都千代田区千代田"},
    {kNonSensitive,  // We might want to revisit this behavior
     "Type privacy sensitive number but the result contains one or more "
     "non-ASCII character such as full-width dash.",
     "1111-1111", "1111－1111"},
    {kNonSensitive,  // We might want to revisit this behavior
     "User dictionary contains a credit card number.", "かーどばんごう",
     "0000-0000-0000-0000"},
    {kNonSensitive,  // We might want to revisit this behavior
     "User dictionary contains a credit card number.", "かーどばんごう",
     "0000000000000000"},
    {kNonSensitive,  // We might want to revisit this behavior
     "User dictionary contains privacy sensitive information.", "ぱすわーど",
     "ywwz1sxm"},
    {kNonSensitive,  // We might want to revisit this behavior
     "Input privacy sensitive text by Roman-input mode by mistake and then "
     "hit F10 key to convert it to half-alphanumeric text. In this case "
     "we assume all the alphabetical characters are consumed by Roman-input "
     "rules.",
     "いあ1ぼ3ぅ", "ia1bo3xu"},
    {kNonSensitive,
     "Katakana to English transliteration.",  // http://b/4394325
     "おれんじ", "Orange"},
    {kNonSensitive,
     "Input a very common English word which should be included in our "
     "system dictionary by Roman-input mode by mistake and "
     "then hit F10 key to convert it to half-alphanumeric text.",
     "おらんげ", "orange"},
    {
        kNonSensitive,
        "Input a password-like text.",
        "123abc!",
        "123abc!",
    },
    {kNonSensitive,
     "Input privacy sensitive text by Roman-input mode by mistake and then "
     "hit F10 key to convert it to half-alphanumeric text. In this case, "
     "there may remain one or more alphabetical characters, which have not "
     "been consumed by Roman-input rules.",
     "yっwz1sxm", "ywwz1sxm"},
    {kNonSensitive,
     "Type a very common English word all in lower case which should be "
     "included in our system dictionary without capitalization.",
     "variable", "variable"},
    {kNonSensitive,
     "Type a very common English word all in upper case whose lower case "
     "should be included in our system dictionary.",
     "VARIABLE", "VARIABLE"},
    {kNonSensitive,
     "Type a very common English word with capitalization whose lower case "
     "should be included in our system dictionary.",
     "Variable", "Variable"},
    {kNonSensitive,  // We might want to revisit this behavior
     "Type a very common English word with random capitalization, which "
     "should be treated as case SENSITIVE.",
     "vArIaBle", "vArIaBle"},
    {
        kNonSensitive,
        "Type an English word in lower case but only its upper case form is "
        "stored in dictionary.",
        "upper",
        "upper",
    },
    {kSensitive,  // We might want to revisit this behavior
     "Type just a number.", "2398402938402934", "2398402938402934"},
    {kNonSensitive,  // We might want to revisit this behavior
     "Type a common English word which might be included in our system "
     "dictionary with number postfix.",
     "Orange10000", "Orange10000"},
};

}  // namespace

TEST_F(UserHistoryPredictorTest, PrivacySensitiveTest) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();

  for (const PrivacySensitiveTestData &data : kNonSensitiveCases) {
    predictor->ClearAllHistory();
    WaitForSyncer(predictor);

    const std::string description(data.scenario_description);
    const std::string input(data.input);
    const std::string output(data.output);
    const std::string partial_input(RemoveLastCodepointCharacter(input));
    const bool expect_sensitive = data.is_sensitive;

    // Initial commit.
    {
      Segments segments;
      const ConversionRequest convreq =
          SetUpInputForConversion(input, &composer_, &segments);
      AddCandidate(0, output, &segments);
      predictor->Finish(convreq, &segments);
    }

    // TODO(yukawa): Refactor the scenario runner below by making
    //     some utility functions.

    // Check suggestion
    {
      Segments segments;
      const ConversionRequest convreq1 =
          SetUpInputForSuggestion(partial_input, &composer_, &segments);
      if (expect_sensitive) {
        EXPECT_FALSE(predictor->PredictForRequest(convreq1, &segments))
            << description << " input: " << input << " output: " << output;
      } else {
        EXPECT_TRUE(predictor->PredictForRequest(convreq1, &segments))
            << description << " input: " << input << " output: " << output;
      }
      segments.Clear();
      const ConversionRequest convreq2 =
          SetUpInputForPrediction(input, &composer_, &segments);
      if (expect_sensitive) {
        EXPECT_FALSE(predictor->PredictForRequest(convreq2, &segments))
            << description << " input: " << input << " output: " << output;
      } else {
        EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments))
            << description << " input: " << input << " output: " << output;
      }
    }

    // Check Prediction
    {
      Segments segments;
      const ConversionRequest convreq1 =
          SetUpInputForPrediction(partial_input, &composer_, &segments);
      if (expect_sensitive) {
        EXPECT_FALSE(predictor->PredictForRequest(convreq1, &segments))
            << description << " input: " << input << " output: " << output;
      } else {
        EXPECT_TRUE(predictor->PredictForRequest(convreq1, &segments))
            << description << " input: " << input << " output: " << output;
      }
      segments.Clear();
      const ConversionRequest convreq2 =
          SetUpInputForPrediction(input, &composer_, &segments);
      if (expect_sensitive) {
        EXPECT_FALSE(predictor->PredictForRequest(convreq2, &segments))
            << description << " input: " << input << " output: " << output;
      } else {
        EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments))
            << description << " input: " << input << " output: " << output;
      }
    }
  }
}

TEST_F(UserHistoryPredictorTest, PrivacySensitiveMultiSegmentsTest) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  WaitForSyncer(predictor);

  // If a password-like input consists of multiple segments, it is not
  // considered to be privacy sensitive when the input is committed.
  // Currently this is a known issue.
  {
    Segments segments;
    const ConversionRequest convreq =
        SetUpInputForConversion("123", &composer_, &segments);
    AddSegment("abc!", &segments);
    AddCandidate(0, "123", &segments);
    AddCandidate(1, "abc!", &segments);
    predictor->Finish(convreq, &segments);
  }

  {
    Segments segments;
    const ConversionRequest convreq1 =
        SetUpInputForSuggestion("123abc", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq1, &segments));
    segments.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForSuggestion("123abc!", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
  }

  {
    Segments segments;
    const ConversionRequest convreq1 =
        SetUpInputForPrediction("123abc", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq1, &segments));
    segments.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForPrediction("123abc!", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryStorage) {
  const std::string filename =
      FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), "test");

  UserHistoryStorage storage1(filename);

  UserHistoryPredictor::Entry *entry = storage1.GetProto().add_entries();
  CHECK(entry);
  entry->set_key("key");
  entry->set_key("value");
  storage1.Save();
  UserHistoryStorage storage2(filename);
  storage2.Load();

  EXPECT_EQ(absl::StrCat(storage1.GetProto()),
            absl::StrCat(storage2.GetProto()));
  EXPECT_OK(FileUtil::UnlinkIfExists(filename));
}

TEST_F(UserHistoryPredictorTest, UserHistoryStorageContainingOldEntries) {
  ScopedClockMock clock(absl::FromUnixSeconds(1));
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();

  // Create a history proto containing old entries (timestamp = 1).
  user_history_predictor::UserHistory history;
  for (int i = 0; i < 10; ++i) {
    auto *entry = history.add_entries();
    entry->set_key(absl::StrFormat("old_key%d", i));
    entry->set_value(absl::StrFormat("old_value%d", i));
    entry->set_last_access_time(absl::ToUnixSeconds(clock->GetAbslTime()));
  }
  clock->Advance(absl::Hours(24 * 63));  // Advance clock for 63 days.
  for (int i = 0; i < 10; ++i) {
    auto *entry = history.add_entries();
    entry->set_key(absl::StrFormat("new_key%d", i));
    entry->set_value(absl::StrFormat("new_value%d", i));
    entry->set_last_access_time(absl::ToUnixSeconds(clock->GetAbslTime()));
  }

  // Test Load().
  {
    const std::string filename =
        FileUtil::JoinPath(temp_dir.path(), "testload");
    // Write directly to the file to keep old entries for testing.
    storage::EncryptedStringStorage file_storage(filename);
    ASSERT_TRUE(file_storage.Save(history.SerializeAsString()));

    UserHistoryStorage storage(filename);
    ASSERT_TRUE(storage.Load());
    // Only the new entries are loaded.
    EXPECT_EQ(storage.GetProto().entries_size(), 10);
    for (const auto &entry : storage.GetProto().entries()) {
      EXPECT_TRUE(absl::StartsWith(entry.key(), "new_"));
      EXPECT_TRUE(absl::StartsWith(entry.value(), "new_"));
    }
    EXPECT_OK(FileUtil::Unlink(filename));
  }

  // Test Save().
  {
    const std::string filename =
        FileUtil::JoinPath(temp_dir.path(), "testsave");
    UserHistoryStorage storage(filename);
    storage.GetProto() = history;
    ASSERT_TRUE(storage.Save());

    // Directly open the file to check the actual entries written.
    storage::EncryptedStringStorage file_storage(filename);
    std::string content;
    ASSERT_TRUE(file_storage.Load(&content));
    user_history_predictor::UserHistory modified_history;
    ASSERT_TRUE(modified_history.ParseFromString(content));
    EXPECT_EQ(modified_history.entries_size(), 10);
    for (const auto &entry : storage.GetProto().entries()) {
      EXPECT_TRUE(absl::StartsWith(entry.key(), "new_"));
      EXPECT_TRUE(absl::StartsWith(entry.value(), "new_"));
    }
    EXPECT_OK(FileUtil::Unlink(filename));
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryStorageContainingInvalidEntries) {
  // This test checks invalid entries are not loaded into dic_.
  ScopedClockMock clock(absl::FromUnixSeconds(1));
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();

  // Create a history proto containing invalid entries (timestamp = 1).
  user_history_predictor::UserHistory history;

  // Invalid UTF8.
  for (const char *value : {
           "\xC2\xC2 ",
           "\xE0\xE0\xE0 ",
           "\xF0\xF0\xF0\xF0 ",
           "\xFF ",
           "\xFE ",
           "\xC0\xAF",
           "\xE0\x80\xAF",
           // Real-world examples from b/116826494.
           "\xEF",
           "\xBC\x91\xE5",
       }) {
    auto *entry = history.add_entries();
    entry->set_key("key");
    entry->set_value(value);
  }

  // Test Load().
  {
    const std::string filename =
        FileUtil::JoinPath(temp_dir.path(), "testload");
    // Write directly to the file to keep invalid entries for testing.
    storage::EncryptedStringStorage file_storage(filename);
    ASSERT_TRUE(file_storage.Save(history.SerializeAsString()));

    UserHistoryStorage storage(filename);
    ASSERT_TRUE(storage.Load());

    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    EXPECT_TRUE(LoadStorage(predictor, storage));

    // Only the valid entries are loaded.
    EXPECT_EQ(storage.GetProto().entries_size(), 9);
    EXPECT_EQ(EntrySize(*predictor), 0);
  }
}

TEST_F(UserHistoryPredictorTest, RomanFuzzyPrefixMatch) {
  // same
  EXPECT_FALSE(UserHistoryPredictor::RomanFuzzyPrefixMatch("abc", "abc"));
  EXPECT_FALSE(UserHistoryPredictor::RomanFuzzyPrefixMatch("a", "a"));

  // exact prefix
  EXPECT_FALSE(UserHistoryPredictor::RomanFuzzyPrefixMatch("abc", "a"));
  EXPECT_FALSE(UserHistoryPredictor::RomanFuzzyPrefixMatch("abc", "ab"));
  EXPECT_FALSE(UserHistoryPredictor::RomanFuzzyPrefixMatch("abc", ""));

  // swap
  EXPECT_TRUE(UserHistoryPredictor::RomanFuzzyPrefixMatch("ab", "ba"));
  EXPECT_TRUE(UserHistoryPredictor::RomanFuzzyPrefixMatch("abfoo", "bafoo"));
  EXPECT_TRUE(UserHistoryPredictor::RomanFuzzyPrefixMatch("fooab", "fooba"));
  EXPECT_TRUE(
      UserHistoryPredictor::RomanFuzzyPrefixMatch("fooabfoo", "foobafoo"));

  // swap + prefix
  EXPECT_TRUE(UserHistoryPredictor::RomanFuzzyPrefixMatch("fooabfoo", "fooba"));

  // deletion
  EXPECT_TRUE(UserHistoryPredictor::RomanFuzzyPrefixMatch("abcd", "acd"));
  EXPECT_TRUE(UserHistoryPredictor::RomanFuzzyPrefixMatch("abcd", "bcd"));

  // deletion + prefix
  EXPECT_TRUE(UserHistoryPredictor::RomanFuzzyPrefixMatch("abcdf", "acd"));
  EXPECT_TRUE(UserHistoryPredictor::RomanFuzzyPrefixMatch("abcdfoo", "bcd"));

  // voice sound mark
  EXPECT_TRUE(
      UserHistoryPredictor::RomanFuzzyPrefixMatch("gu-guru", "gu^guru"));
  EXPECT_TRUE(
      UserHistoryPredictor::RomanFuzzyPrefixMatch("gu-guru", "gu=guru"));
  EXPECT_TRUE(UserHistoryPredictor::RomanFuzzyPrefixMatch("gu-guru", "gu^gu"));
  EXPECT_FALSE(UserHistoryPredictor::RomanFuzzyPrefixMatch("gu-guru", "gugu"));

  // Invalid
  EXPECT_FALSE(UserHistoryPredictor::RomanFuzzyPrefixMatch("", ""));
  EXPECT_FALSE(UserHistoryPredictor::RomanFuzzyPrefixMatch("", "a"));
  EXPECT_FALSE(UserHistoryPredictor::RomanFuzzyPrefixMatch("abcde", "defe"));
}

TEST_F(UserHistoryPredictorTest, MaybeRomanMisspelledKey) {
  EXPECT_TRUE(UserHistoryPredictor::MaybeRomanMisspelledKey("こんぴゅーｔ"));
  EXPECT_TRUE(UserHistoryPredictor::MaybeRomanMisspelledKey("こんぴゅーt"));
  EXPECT_FALSE(UserHistoryPredictor::MaybeRomanMisspelledKey("こんぴゅーた"));
  EXPECT_TRUE(UserHistoryPredictor::MaybeRomanMisspelledKey("ぱｓこん"));
  EXPECT_FALSE(UserHistoryPredictor::MaybeRomanMisspelledKey("ぱそこん"));
  EXPECT_TRUE(
      UserHistoryPredictor::MaybeRomanMisspelledKey("おねがいしまうｓ"));
  EXPECT_FALSE(UserHistoryPredictor::MaybeRomanMisspelledKey("おねがいします"));
  EXPECT_TRUE(UserHistoryPredictor::MaybeRomanMisspelledKey("いんた=ねっと"));
  EXPECT_FALSE(UserHistoryPredictor::MaybeRomanMisspelledKey("ｔ"));
  EXPECT_TRUE(UserHistoryPredictor::MaybeRomanMisspelledKey("ーｔ"));
  EXPECT_FALSE(
      UserHistoryPredictor::MaybeRomanMisspelledKey("おｎがいしまうｓ"));
  // Two unknowns
  EXPECT_FALSE(
      UserHistoryPredictor::MaybeRomanMisspelledKey("お＆がい＄しまう"));
  // One alpha and one unknown
  EXPECT_FALSE(
      UserHistoryPredictor::MaybeRomanMisspelledKey("お＆がいしまうｓ"));
}

TEST_F(UserHistoryPredictorTest, GetRomanMisspelledKey) {
  Segments segments;
  Segment *seg = segments.add_segment();
  seg->set_segment_type(Segment::FREE);
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->value = "test";

  const ConversionRequest convreq = CreateConversionRequest(composer_);

  config_.set_preedit_method(config::Config::ROMAN);
  seg->set_key("");
  EXPECT_EQ(UserHistoryPredictor::GetRomanMisspelledKey(convreq, segments), "");

  seg->set_key("おねがいしまうs");
  EXPECT_EQ(UserHistoryPredictor::GetRomanMisspelledKey(convreq, segments),
            "onegaisimaus");

  seg->set_key("おねがいします");
  EXPECT_EQ(UserHistoryPredictor::GetRomanMisspelledKey(convreq, segments), "");

  config_.set_preedit_method(config::Config::KANA);

  seg->set_key("おねがいしまうs");
  EXPECT_EQ(UserHistoryPredictor::GetRomanMisspelledKey(convreq, segments), "");

  seg->set_key("おねがいします");
  EXPECT_EQ(UserHistoryPredictor::GetRomanMisspelledKey(convreq, segments), "");
}

TEST_F(UserHistoryPredictorTest, RomanFuzzyLookupEntry) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  UserHistoryPredictor::Entry entry;
  UserHistoryPredictor::EntryPriorityQueue results;

  entry.set_key("");
  EXPECT_FALSE(predictor->RomanFuzzyLookupEntry("", &entry, &results));

  entry.set_key("よろしく");
  EXPECT_TRUE(predictor->RomanFuzzyLookupEntry("yorosku", &entry, &results));
  EXPECT_TRUE(predictor->RomanFuzzyLookupEntry("yrosiku", &entry, &results));
  EXPECT_TRUE(predictor->RomanFuzzyLookupEntry("yorsiku", &entry, &results));
  EXPECT_FALSE(predictor->RomanFuzzyLookupEntry("yrsk", &entry, &results));
  EXPECT_FALSE(predictor->RomanFuzzyLookupEntry("yorosiku", &entry, &results));

  entry.set_key("ぐーぐる");
  EXPECT_TRUE(predictor->RomanFuzzyLookupEntry("gu=guru", &entry, &results));
  EXPECT_FALSE(predictor->RomanFuzzyLookupEntry("gu-guru", &entry, &results));
  EXPECT_FALSE(predictor->RomanFuzzyLookupEntry("g=guru", &entry, &results));
}

namespace {
struct LookupTestData {
  absl::string_view entry_key;
  bool expect_result;
};
}  // namespace

TEST_F(UserHistoryPredictorTest, ExpandedLookupRoman) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  UserHistoryPredictor::Entry entry;
  UserHistoryPredictor::EntryPriorityQueue results;

  // Roman
  // preedit: "あｋ"
  // input_key: "あｋ"
  // key_base: "あ"
  // key_expanded: "か","き","く","け", "こ"
  auto expanded = std::make_unique<Trie<std::string>>();
  expanded->AddEntry("か", "");
  expanded->AddEntry("き", "");
  expanded->AddEntry("く", "");
  expanded->AddEntry("け", "");
  expanded->AddEntry("こ", "");

  constexpr LookupTestData kTests1[] = {
      {"", false},       {"あか", true},    {"あき", true},  {"あかい", true},
      {"あまい", false}, {"あ", false},     {"さか", false}, {"さき", false},
      {"さかい", false}, {"さまい", false}, {"さ", false},
  };

  // with expanded
  for (size_t i = 0; i < std::size(kTests1); ++i) {
    entry.set_key(kTests1[i].entry_key);
    EXPECT_EQ(
        predictor->LookupEntry(UserHistoryPredictor::DEFAULT, "あｋ", "あ",
                               expanded.get(), &entry, nullptr, &results),
        kTests1[i].expect_result)
        << kTests1[i].entry_key;
  }

  // only expanded
  // preedit: "k"
  // input_key: ""
  // key_base: ""
  // key_expanded: "か","き","く","け", "こ"

  constexpr LookupTestData kTests2[] = {
      {"", false},    {"か", true},    {"き", true},
      {"かい", true}, {"まい", false}, {"も", false},
  };

  for (size_t i = 0; i < std::size(kTests2); ++i) {
    entry.set_key(kTests2[i].entry_key);
    EXPECT_EQ(predictor->LookupEntry(UserHistoryPredictor::DEFAULT, "", "",
                                     expanded.get(), &entry, nullptr, &results),
              kTests2[i].expect_result)
        << kTests2[i].entry_key;
  }
}

TEST_F(UserHistoryPredictorTest, ExpandedLookupKana) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  UserHistoryPredictor::Entry entry;
  UserHistoryPredictor::EntryPriorityQueue results;

  // Kana
  // preedit: "あし"
  // input_key: "あし"
  // key_base: "あ"
  // key_expanded: "し","じ"
  auto expanded = std::make_unique<Trie<std::string>>();
  expanded->AddEntry("し", "");
  expanded->AddEntry("じ", "");

  constexpr LookupTestData kTests1[] = {
      {"", false},           {"あ", false},         {"あし", true},
      {"あじ", true},        {"あしかゆい", true},  {"あじうまい", true},
      {"あまにがい", false}, {"あめ", false},       {"まし", false},
      {"まじ", false},       {"ましなあじ", false}, {"まじうまい", false},
      {"ままにがい", false}, {"まめ", false},
  };

  // with expanded
  for (size_t i = 0; i < std::size(kTests1); ++i) {
    entry.set_key(kTests1[i].entry_key);
    EXPECT_EQ(
        predictor->LookupEntry(UserHistoryPredictor::DEFAULT, "あし", "あ",
                               expanded.get(), &entry, nullptr, &results),
        kTests1[i].expect_result)
        << kTests1[i].entry_key;
  }

  // only expanded
  // input_key: "し"
  // key_base: ""
  // key_expanded: "し","じ"
  constexpr LookupTestData kTests2[] = {
      {"", false},          {"し", true},         {"じ", true},
      {"しかうまい", true}, {"じゅうかい", true}, {"ま", false},
      {"まめ", false},
  };

  for (size_t i = 0; i < std::size(kTests2); ++i) {
    entry.set_key(kTests2[i].entry_key);
    EXPECT_EQ(predictor->LookupEntry(UserHistoryPredictor::DEFAULT, "し", "",
                                     expanded.get(), &entry, nullptr, &results),
              kTests2[i].expect_result)
        << kTests2[i].entry_key;
  }
}

TEST_F(UserHistoryPredictorTest, GetMatchTypeFromInputRoman) {
  // We have to define this here,
  // because UserHistoryPredictor::MatchType is private
  struct MatchTypeTestData {
    absl::string_view target;
    UserHistoryPredictor::MatchType expect_type;
  };

  // Roman
  // preedit: "あｋ"
  // input_key: "あ"
  // key_base: "あ"
  // key_expanded: "か","き","く","け", "こ"
  auto expanded = std::make_unique<Trie<std::string>>();
  expanded->AddEntry("か", "か");
  expanded->AddEntry("き", "き");
  expanded->AddEntry("く", "く");
  expanded->AddEntry("け", "け");
  expanded->AddEntry("こ", "こ");

  constexpr MatchTypeTestData kTests1[] = {
      {"", UserHistoryPredictor::NO_MATCH},
      {"い", UserHistoryPredictor::NO_MATCH},
      {"あ", UserHistoryPredictor::RIGHT_PREFIX_MATCH},
      {"あい", UserHistoryPredictor::NO_MATCH},
      {"あか", UserHistoryPredictor::LEFT_PREFIX_MATCH},
      {"あかい", UserHistoryPredictor::LEFT_PREFIX_MATCH},
  };

  for (size_t i = 0; i < std::size(kTests1); ++i) {
    EXPECT_EQ(UserHistoryPredictor::GetMatchTypeFromInput(
                  "あ", "あ", expanded.get(), kTests1[i].target),
              kTests1[i].expect_type)
        << kTests1[i].target;
  }

  // only expanded
  // preedit: "ｋ"
  // input_key: ""
  // key_base: ""
  // key_expanded: "か","き","く","け", "こ"
  constexpr MatchTypeTestData kTests2[] = {
      {"", UserHistoryPredictor::NO_MATCH},
      {"い", UserHistoryPredictor::NO_MATCH},
      {"いか", UserHistoryPredictor::NO_MATCH},
      {"か", UserHistoryPredictor::LEFT_PREFIX_MATCH},
      {"かいがい", UserHistoryPredictor::LEFT_PREFIX_MATCH},
  };

  for (size_t i = 0; i < std::size(kTests2); ++i) {
    EXPECT_EQ(UserHistoryPredictor::GetMatchTypeFromInput(
                  "", "", expanded.get(), kTests2[i].target),
              kTests2[i].expect_type)
        << kTests2[i].target;
  }
}

TEST_F(UserHistoryPredictorTest, GetMatchTypeFromInputKana) {
  // We have to define this here,
  // because UserHistoryPredictor::MatchType is private
  struct MatchTypeTestData {
    absl::string_view target;
    UserHistoryPredictor::MatchType expect_type;
  };

  // Kana
  // preedit: "あし"
  // input_key: "あし"
  // key_base: "あ"
  // key_expanded: "し","じ"
  auto expanded = std::make_unique<Trie<std::string>>();
  expanded->AddEntry("し", "し");
  expanded->AddEntry("じ", "じ");

  constexpr MatchTypeTestData kTests1[] = {
      {"", UserHistoryPredictor::NO_MATCH},
      {"い", UserHistoryPredictor::NO_MATCH},
      {"いし", UserHistoryPredictor::NO_MATCH},
      {"あ", UserHistoryPredictor::RIGHT_PREFIX_MATCH},
      {"あし", UserHistoryPredictor::EXACT_MATCH},
      {"あじ", UserHistoryPredictor::LEFT_PREFIX_MATCH},
      {"あした", UserHistoryPredictor::LEFT_PREFIX_MATCH},
      {"あじしお", UserHistoryPredictor::LEFT_PREFIX_MATCH},
  };

  for (size_t i = 0; i < std::size(kTests1); ++i) {
    EXPECT_EQ(UserHistoryPredictor::GetMatchTypeFromInput(
                  "あし", "あ", expanded.get(), kTests1[i].target),
              kTests1[i].expect_type)
        << kTests1[i].target;
  }

  // only expanded
  // preedit: "し"
  // input_key: "し"
  // key_base: ""
  // key_expanded: "し","じ"
  constexpr MatchTypeTestData kTests2[] = {
      {"", UserHistoryPredictor::NO_MATCH},
      {"い", UserHistoryPredictor::NO_MATCH},
      {"し", UserHistoryPredictor::EXACT_MATCH},
      {"じ", UserHistoryPredictor::LEFT_PREFIX_MATCH},
      {"しじみ", UserHistoryPredictor::LEFT_PREFIX_MATCH},
      {"じかん", UserHistoryPredictor::LEFT_PREFIX_MATCH},
  };

  for (size_t i = 0; i < std::size(kTests2); ++i) {
    EXPECT_EQ(UserHistoryPredictor::GetMatchTypeFromInput(
                  "し", "", expanded.get(), kTests2[i].target),
              kTests2[i].expect_type)
        << kTests2[i].target;
  }
}

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsRoman) {
  table_->LoadFromFile("system://romanji-hiragana.tsv");
  composer_.SetTable(table_.get());
  Segments segments;

  const ConversionRequest convreq =
      InitSegmentsFromInputSequence("gu-g", &composer_, &segments);
  std::string input_key;
  std::string base;
  std::unique_ptr<Trie<std::string>> expanded;
  UserHistoryPredictor::GetInputKeyFromSegments(convreq, segments, &input_key,
                                                &base, &expanded);
  EXPECT_EQ(input_key, "ぐーｇ");
  EXPECT_EQ(base, "ぐー");
  EXPECT_TRUE(expanded != nullptr);
  std::string value;
  size_t key_length = 0;
  bool has_subtrie = false;
  EXPECT_TRUE(expanded->LookUpPrefix("ぐ", &value, &key_length, &has_subtrie));
  EXPECT_EQ(value, "ぐ");
}

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsRomanRandom) {
  table_->LoadFromFile("system://romanji-hiragana.tsv");
  composer_.SetTable(table_.get());
  Segments segments;
  Random random;

  for (size_t i = 0; i < 1000; ++i) {
    composer_.Reset();
    const std::string input = random.Utf8StringRandomLen(4, ' ', '~');
    const ConversionRequest convreq =
        InitSegmentsFromInputSequence(input, &composer_, &segments);
    std::string input_key;
    std::string base;
    std::unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(convreq, segments, &input_key,
                                                  &base, &expanded);
  }
}

// Found by random test.
// input_key != base by composer modification.
TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsShouldNotCrash) {
  table_->LoadFromFile("system://romanji-hiragana.tsv");
  composer_.SetTable(table_.get());
  Segments segments;

  {
    const ConversionRequest convreq =
        InitSegmentsFromInputSequence("8,+", &composer_, &segments);
    std::string input_key;
    std::string base;
    std::unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(convreq, segments, &input_key,
                                                  &base, &expanded);
  }
}

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsRomanN) {
  table_->LoadFromFile("system://romanji-hiragana.tsv");
  composer_.SetTable(table_.get());
  Segments segments;

  {
    const ConversionRequest convreq =
        InitSegmentsFromInputSequence("n", &composer_, &segments);
    std::string input_key;
    std::string base;
    std::unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(convreq, segments, &input_key,
                                                  &base, &expanded);
    EXPECT_EQ(input_key, "ｎ");
    EXPECT_EQ(base, "");
    EXPECT_TRUE(expanded != nullptr);
    std::string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_TRUE(
        expanded->LookUpPrefix("な", &value, &key_length, &has_subtrie));
    EXPECT_EQ(value, "な");
  }

  composer_.Reset();
  segments.Clear();
  {
    const ConversionRequest convreq =
        InitSegmentsFromInputSequence("nn", &composer_, &segments);
    std::string input_key;
    std::string base;
    std::unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(convreq, segments, &input_key,
                                                  &base, &expanded);
    EXPECT_EQ(input_key, "ん");
    EXPECT_EQ(base, "ん");
    EXPECT_TRUE(expanded == nullptr);
  }

  composer_.Reset();
  segments.Clear();
  {
    const ConversionRequest convreq =
        InitSegmentsFromInputSequence("n'", &composer_, &segments);
    std::string input_key;
    std::string base;
    std::unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(convreq, segments, &input_key,
                                                  &base, &expanded);
    EXPECT_EQ(input_key, "ん");
    EXPECT_EQ(base, "ん");
    EXPECT_TRUE(expanded == nullptr);
  }

  composer_.Reset();
  segments.Clear();
  {
    const ConversionRequest convreq =
        InitSegmentsFromInputSequence("n'n", &composer_, &segments);
    std::string input_key;
    std::string base;
    std::unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(convreq, segments, &input_key,
                                                  &base, &expanded);
    EXPECT_EQ(input_key, "んｎ");
    EXPECT_EQ(base, "ん");
    EXPECT_TRUE(expanded != nullptr);
    std::string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_TRUE(
        expanded->LookUpPrefix("な", &value, &key_length, &has_subtrie));
    EXPECT_EQ(value, "な");
  }
}

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsFlickN) {
  table_->LoadFromFile("system://flick-hiragana.tsv");
  composer_.SetTable(table_.get());
  Segments segments;

  {
    const ConversionRequest convreq =
        InitSegmentsFromInputSequence("/", &composer_, &segments);
    std::string input_key;
    std::string base;
    std::unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(convreq, segments, &input_key,
                                                  &base, &expanded);
    EXPECT_EQ(input_key, "ん");
    EXPECT_EQ(base, "");
    EXPECT_TRUE(expanded != nullptr);
    std::string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_TRUE(
        expanded->LookUpPrefix("ん", &value, &key_length, &has_subtrie));
    EXPECT_EQ(value, "ん");
  }
}

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegments12KeyN) {
  table_->LoadFromFile("system://12keys-hiragana.tsv");
  composer_.SetTable(table_.get());
  Segments segments;

  {
    const ConversionRequest convreq =
        InitSegmentsFromInputSequence("わ00", &composer_, &segments);
    std::string input_key;
    std::string base;
    std::unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(convreq, segments, &input_key,
                                                  &base, &expanded);
    EXPECT_EQ(input_key, "ん");
    EXPECT_EQ(base, "");
    EXPECT_TRUE(expanded != nullptr);
    std::string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_TRUE(
        expanded->LookUpPrefix("ん", &value, &key_length, &has_subtrie));
    EXPECT_EQ(value, "ん");
  }
}

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsKana) {
  table_->LoadFromFile("system://kana.tsv");
  composer_.SetTable(table_.get());
  Segments segments;

  const ConversionRequest convreq =
      InitSegmentsFromInputSequence("あか", &composer_, &segments);

  {
    std::string input_key;
    std::string base;
    std::unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(convreq, segments, &input_key,
                                                  &base, &expanded);
    EXPECT_EQ(input_key, "あか");
    EXPECT_EQ(base, "あ");
    EXPECT_TRUE(expanded != nullptr);
    std::string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_TRUE(
        expanded->LookUpPrefix("が", &value, &key_length, &has_subtrie));
    EXPECT_EQ(value, "が");
  }
}

TEST_F(UserHistoryPredictorTest, RealtimeConversionInnerSegment) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;
  {
    constexpr char kKey[] = "わたしのなまえはなかのです";
    constexpr char kValue[] = "私の名前は中野です";
    const ConversionRequest convreq1 =
        SetUpInputForPrediction(kKey, &composer_, &segments);
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->add_candidate();
    CHECK(candidate);
    candidate->value = kValue;
    candidate->content_value = kValue;
    candidate->key = kKey;
    candidate->content_key = kKey;
    // "わたしの, 私の", "わたし, 私"
    candidate->PushBackInnerSegmentBoundary(12, 6, 9, 3);
    // "なまえは, 名前は", "なまえ, 名前"
    candidate->PushBackInnerSegmentBoundary(12, 9, 9, 6);
    // "なかのです, 中野です", "なかの, 中野"
    candidate->PushBackInnerSegmentBoundary(15, 12, 9, 6);
    predictor->Finish(convreq1, &segments);
  }
  segments.Clear();

  const ConversionRequest convreq2 =
      SetUpInputForPrediction("なかの", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
  EXPECT_TRUE(FindCandidateByValue("中野です", segments));

  segments.Clear();
  const ConversionRequest convreq3 =
      SetUpInputForPrediction("なまえ", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq3, &segments));
  EXPECT_TRUE(FindCandidateByValue("名前は", segments));
  EXPECT_TRUE(FindCandidateByValue("名前は中野です", segments));
}

TEST_F(UserHistoryPredictorTest, ZeroQueryFromRealtimeConversion) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;
  ConversionRequest convreq1;
  {
    constexpr char kKey[] = "わたしのなまえはなかのです";
    constexpr char kValue[] = "私の名前は中野です";
    const ConversionRequest convreq =
        SetUpInputForPrediction(kKey, &composer_, &segments);
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->add_candidate();
    CHECK(candidate);
    candidate->value = kValue;
    candidate->content_value = kValue;
    candidate->key = kKey;
    candidate->content_key = kKey;
    // "わたしの, 私の", "わたし, 私"
    candidate->PushBackInnerSegmentBoundary(12, 6, 9, 3);
    // "なまえは, 名前は", "なまえ, 名前"
    candidate->PushBackInnerSegmentBoundary(12, 9, 9, 6);
    // "なかのです, 中野です", "なかの, 中野"
    candidate->PushBackInnerSegmentBoundary(15, 12, 9, 6);
  }
  predictor->Finish(convreq1, &segments);
  segments.Clear();

  const ConversionRequest convreq2 =
      SetUpInputForConversion("わたしの", &composer_, &segments);
  AddCandidate(0, "私の", &segments);
  predictor->Finish(convreq2, &segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  commands::Request request;
  request_.set_zero_query_suggestion(true);
  const ConversionRequest convreq3 = SetUpInputForSuggestionWithHistory(
      "", "わたしの", "私の", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq3, &segments));
  EXPECT_TRUE(FindCandidateByValue("名前は", segments));
}

TEST_F(UserHistoryPredictorTest, LongCandidateForMobile) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  request_test_util::FillMobileRequest(&request_);

  Segments segments;
  for (size_t i = 0; i < 3; ++i) {
    constexpr char kKey[] = "よろしくおねがいします";
    constexpr char kValue[] = "よろしくお願いします";
    const ConversionRequest convreq =
        SetUpInputForPrediction(kKey, &composer_, &segments);
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->add_candidate();
    CHECK(candidate);
    candidate->value = kValue;
    candidate->content_value = kValue;
    candidate->key = kKey;
    candidate->content_key = kKey;
    predictor->Finish(convreq, &segments);
    segments.Clear();
  }

  const ConversionRequest convreq =
      SetUpInputForPrediction("よろ", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq, &segments));
  EXPECT_TRUE(FindCandidateByValue("よろしくお願いします", segments));
}

TEST_F(UserHistoryPredictorTest, EraseNextEntries) {
  UserHistoryPredictor::Entry e;
  e.add_next_entries()->set_entry_fp(100);
  e.add_next_entries()->set_entry_fp(10);
  e.add_next_entries()->set_entry_fp(30);
  e.add_next_entries()->set_entry_fp(10);
  e.add_next_entries()->set_entry_fp(100);

  UserHistoryPredictor::EraseNextEntries(1234, &e);
  EXPECT_EQ(e.next_entries_size(), 5);

  UserHistoryPredictor::EraseNextEntries(30, &e);
  ASSERT_EQ(e.next_entries_size(), 4);
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_NE(e.next_entries(i).entry_fp(), 30);
  }

  UserHistoryPredictor::EraseNextEntries(10, &e);
  ASSERT_EQ(e.next_entries_size(), 2);
  for (size_t i = 0; i < 2; ++i) {
    EXPECT_NE(e.next_entries(i).entry_fp(), 10);
  }

  UserHistoryPredictor::EraseNextEntries(100, &e);
  EXPECT_EQ(e.next_entries_size(), 0);
}

TEST_F(UserHistoryPredictorTest, RemoveNgramChain) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Set up the following chain of next entries:
  // ("abc", "ABC")
  // (  "a",   "A") --- ("b", "B") --- ("c", "C")
  UserHistoryPredictor::Entry *abc = InsertEntry(predictor, "abc", "ABC");
  UserHistoryPredictor::Entry *a = InsertEntry(predictor, "a", "A");
  UserHistoryPredictor::Entry *b = AppendEntry(predictor, "b", "B", a);
  UserHistoryPredictor::Entry *c = AppendEntry(predictor, "c", "C", b);

  std::vector<UserHistoryPredictor::Entry *> entries;
  entries.push_back(abc);
  entries.push_back(a);
  entries.push_back(b);
  entries.push_back(c);

  // The method should return NOT_FOUND for key-value pairs not in the chain.
  for (size_t i = 0; i < entries.size(); ++i) {
    std::vector<absl::string_view> dummy1, dummy2;
    EXPECT_EQ(predictor->RemoveNgramChain("hoge", "HOGE", entries[i], &dummy1,
                                          0, &dummy2, 0),
              UserHistoryPredictor::NOT_FOUND);
  }
  // Moreover, all nodes and links should be kept.
  for (size_t i = 0; i < entries.size(); ++i) {
    EXPECT_FALSE(entries[i]->removed());
  }
  EXPECT_TRUE(IsConnected(*a, *b));
  EXPECT_TRUE(IsConnected(*b, *c));

  {
    // Try deleting the chain for "abc". Only the link from "b" to "c" should be
    // removed.
    std::vector<absl::string_view> dummy1, dummy2;
    EXPECT_EQ(
        predictor->RemoveNgramChain("abc", "ABC", a, &dummy1, 0, &dummy2, 0),
        UserHistoryPredictor::DONE);
    for (size_t i = 0; i < entries.size(); ++i) {
      EXPECT_FALSE(entries[i]->removed());
    }
    EXPECT_TRUE(IsConnected(*a, *b));
    EXPECT_FALSE(IsConnected(*b, *c));
  }
  {
    // Try deleting the chain for "a". Since this is the head of the chain, the
    // function returns TAIL and nothing should be removed.
    std::vector<absl::string_view> dummy1, dummy2;
    EXPECT_EQ(predictor->RemoveNgramChain("a", "A", a, &dummy1, 0, &dummy2, 0),
              UserHistoryPredictor::TAIL);
    for (size_t i = 0; i < entries.size(); ++i) {
      EXPECT_FALSE(entries[i]->removed());
    }
    EXPECT_TRUE(IsConnected(*a, *b));
    EXPECT_FALSE(IsConnected(*b, *c));
  }
  {
    // Further delete the chain for "ab".  Now all the links should be removed.
    std::vector<absl::string_view> dummy1, dummy2;
    EXPECT_EQ(
        predictor->RemoveNgramChain("ab", "AB", a, &dummy1, 0, &dummy2, 0),
        UserHistoryPredictor::DONE);
    for (size_t i = 0; i < entries.size(); ++i) {
      EXPECT_FALSE(entries[i]->removed());
    }
    EXPECT_FALSE(IsConnected(*a, *b));
    EXPECT_FALSE(IsConnected(*b, *c));
  }
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntryUnigram) {
  ScopedClockMock clock(absl::FromUnixSeconds(1));

  // Tests ClearHistoryEntry() for unigram history.
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Add a unigram history ("japanese", "Japanese").
  UserHistoryPredictor::Entry *e =
      InsertEntry(predictor, "japanese", "Japanese");
  e->set_last_access_time(1);

  // "Japanese" should be suggested and predicted from "japan".
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));

  // Delete the history.
  EXPECT_TRUE(predictor->ClearHistoryEntry("japanese", "Japanese"));

  EXPECT_TRUE(e->removed());

  // "Japanese" should be never be suggested nor predicted.
  constexpr absl::string_view kKey = "japanese";
  for (size_t i = 0; i < kKey.size(); ++i) {
    const absl::string_view prefix = kKey.substr(0, i);
    EXPECT_FALSE(IsSuggested(predictor, prefix, "Japanese"));
    EXPECT_FALSE(IsPredicted(predictor, prefix, "Japanese"));
  }
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntryBigramDeleteWhole) {
  ScopedClockMock clock(absl::FromUnixSeconds(1));

  // Tests ClearHistoryEntry() for bigram history.  This case tests the deletion
  // of whole sentence.
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the history for ("japaneseinput", "JapaneseInput"). It's assumed that
  // this sentence consists of two segments, "japanese" and "input". So, the
  // following history entries are constructed:
  //   ("japaneseinput", "JapaneseInput")  // Unigram
  //   ("japanese", "Japanese") --- ("input", "Input")  // Bigram chain
  UserHistoryPredictor::Entry *japaneseinput;
  UserHistoryPredictor::Entry *japanese;
  UserHistoryPredictor::Entry *input;
  InitHistory_JapaneseInput(predictor, &japaneseinput, &japanese, &input);

  // Check the predictor functionality for the above history structure.
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "input", "Input"));

  // Delete the unigram ("japaneseinput", "JapaneseInput").
  EXPECT_TRUE(predictor->ClearHistoryEntry("japaneseinput", "JapaneseInput"));

  EXPECT_TRUE(japaneseinput->removed());
  EXPECT_FALSE(japanese->removed());
  EXPECT_FALSE(input->removed());
  EXPECT_FALSE(IsConnected(*japanese, *input));

  // Now "JapaneseInput" should never be suggested nor predicted.
  constexpr absl::string_view kKey = "japaneseinput";
  for (size_t i = 0; i < kKey.size(); ++i) {
    const absl::string_view prefix = kKey.substr(0, i);
    EXPECT_FALSE(IsSuggested(predictor, prefix, "Japaneseinput"));
    EXPECT_FALSE(IsPredicted(predictor, prefix, "Japaneseinput"));
  }

  // However, predictor should show "Japanese" and "Input".
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "Input"));
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntryBigramDeleteFirst) {
  ScopedClockMock clock(absl::FromUnixSeconds(1));

  // Tests ClearHistoryEntry() for bigram history.  This case tests the deletion
  // of the first node of the bigram chain.
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the history for ("japaneseinput", "JapaneseInput"), i.e., the same
  // history structure as ClearHistoryEntry_Bigram_DeleteWhole is constructed.
  UserHistoryPredictor::Entry *japaneseinput;
  UserHistoryPredictor::Entry *japanese;
  UserHistoryPredictor::Entry *input;
  InitHistory_JapaneseInput(predictor, &japaneseinput, &japanese, &input);

  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "input", "Input"));

  // Delete the first bigram node ("japanese", "Japanese").
  EXPECT_TRUE(predictor->ClearHistoryEntry("japanese", "Japanese"));

  // Note that the first node was removed but the connection to the second node
  // is still valid.
  EXPECT_FALSE(japaneseinput->removed());
  EXPECT_TRUE(japanese->removed());
  EXPECT_FALSE(input->removed());
  EXPECT_TRUE(IsConnected(*japanese, *input));

  // Now "Japanese" should never be suggested nor predicted.
  constexpr absl::string_view kKey = "japaneseinput";
  for (size_t i = 0; i < kKey.size(); ++i) {
    const absl::string_view prefix = kKey.substr(0, i);
    EXPECT_FALSE(IsSuggested(predictor, prefix, "Japanese"));
    EXPECT_FALSE(IsPredicted(predictor, prefix, "Japanese"));
  }

  // However, predictor should show "JapaneseInput" and "Input".
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "Input"));
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntryBigramDeleteSecond) {
  ScopedClockMock clock(absl::FromUnixSeconds(1));

  // Tests ClearHistoryEntry() for bigram history.  This case tests the deletion
  // of the first node of the bigram chain.
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the history for ("japaneseinput", "JapaneseInput"), i.e., the same
  // history structure as ClearHistoryEntry_Bigram_DeleteWhole is constructed.
  UserHistoryPredictor::Entry *japaneseinput;
  UserHistoryPredictor::Entry *japanese;
  UserHistoryPredictor::Entry *input;
  InitHistory_JapaneseInput(predictor, &japaneseinput, &japanese, &input);

  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "input", "Input"));

  // Delete the second bigram node ("input", "Input").
  EXPECT_TRUE(predictor->ClearHistoryEntry("input", "Input"));

  EXPECT_FALSE(japaneseinput->removed());
  EXPECT_FALSE(japanese->removed());
  EXPECT_TRUE(input->removed());
  EXPECT_TRUE(IsConnected(*japanese, *input));

  // Now "Input" should never be suggested nor predicted.
  constexpr absl::string_view kKey = "input";
  for (size_t i = 0; i < kKey.size(); ++i) {
    const absl::string_view prefix = kKey.substr(0, i);
    EXPECT_FALSE(IsSuggested(predictor, prefix, "Input"));
    EXPECT_FALSE(IsPredicted(predictor, prefix, "Input"));
  }

  // However, predictor should show "Japanese" and "JapaneseInput".
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntryTrigramDeleteWhole) {
  ScopedClockMock clock(absl::FromUnixSeconds(1));

  // Tests ClearHistoryEntry() for trigram history.  This case tests the
  // deletion of the whole sentence.
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the history for ("japaneseinputmethod", "JapaneseInputMethod"). It's
  // assumed that this sentence consists of three segments, "japanese", "input"
  // and "method". So, the following history entries are constructed:
  //   ("japaneseinputmethod", "JapaneseInputMethod")  // Unigram
  //   ("japanese", "Japanese") -- ("input", "Input") -- ("method", "Method")
  UserHistoryPredictor::Entry *japaneseinputmethod;
  UserHistoryPredictor::Entry *japanese;
  UserHistoryPredictor::Entry *input;
  UserHistoryPredictor::Entry *method;
  InitHistory_JapaneseInputMethod(predictor, &japaneseinputmethod, &japanese,
                                  &input, &method);

  // Delete the history of the whole sentence.
  EXPECT_TRUE(predictor->ClearHistoryEntry("japaneseinputmethod",
                                           "JapaneseInputMethod"));

  // Note that only the link from "input" to "method" was removed.
  EXPECT_TRUE(japaneseinputmethod->removed());
  EXPECT_FALSE(japanese->removed());
  EXPECT_FALSE(input->removed());
  EXPECT_FALSE(method->removed());
  EXPECT_TRUE(IsConnected(*japanese, *input));
  EXPECT_FALSE(IsConnected(*input, *method));

  {
    // Now "JapaneseInputMethod" should never be suggested nor predicted.
    constexpr absl::string_view kKey = "japaneseinputmethod";
    for (size_t i = 0; i < kKey.size(); ++i) {
      const absl::string_view prefix = kKey.substr(0, i);
      EXPECT_FALSE(IsSuggested(predictor, prefix, "JapaneseInputMethod"));
      EXPECT_FALSE(IsPredicted(predictor, prefix, "JapaneseInputMethod"));
    }
  }
  {
    // Here's a limitation of chain cut.  Since we have cut the link from
    // "input" to "method", the predictor cannot show "InputMethod" although it
    // could before.  However, since "InputMethod" is not the direct input by
    // the user (user's input was "JapaneseInputMethod" in this case), this
    // limitation would be acceptable.
    constexpr absl::string_view kKey = "inputmethod";
    for (size_t i = 0; i < kKey.size(); ++i) {
      const absl::string_view prefix = kKey.substr(0, i);
      EXPECT_FALSE(IsSuggested(predictor, prefix, "InputMethod"));
      EXPECT_FALSE(IsPredicted(predictor, prefix, "InputMethod"));
    }
  }

  // The following can be still suggested and predicted.
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "Input"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "meth", "Method"));
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntryTrigramDeleteFirst) {
  ScopedClockMock clock(absl::FromUnixSeconds(1));

  // Tests ClearHistoryEntry() for trigram history.  This case tests the
  // deletion of the first node of trigram.
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the same history structure as ClearHistoryEntry_Trigram_DeleteWhole.
  UserHistoryPredictor::Entry *japaneseinputmethod;
  UserHistoryPredictor::Entry *japanese;
  UserHistoryPredictor::Entry *input;
  UserHistoryPredictor::Entry *method;
  InitHistory_JapaneseInputMethod(predictor, &japaneseinputmethod, &japanese,
                                  &input, &method);

  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(
      IsSuggestedAndPredicted(predictor, "japan", "JapaneseInputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "Input"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "InputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "meth", "Method"));

  // Delete the first node of the chain.
  EXPECT_TRUE(predictor->ClearHistoryEntry("japanese", "Japanese"));

  // Note that the two links are still alive.
  EXPECT_FALSE(japaneseinputmethod->removed());
  EXPECT_TRUE(japanese->removed());
  EXPECT_FALSE(input->removed());
  EXPECT_FALSE(method->removed());
  EXPECT_TRUE(IsConnected(*japanese, *input));
  EXPECT_TRUE(IsConnected(*input, *method));

  {
    // Now "Japanese" should never be suggested nor predicted.
    constexpr absl::string_view kKey = "japaneseinputmethod";
    for (size_t i = 0; i < kKey.size(); ++i) {
      const absl::string_view prefix = kKey.substr(0, i);
      EXPECT_FALSE(IsSuggested(predictor, prefix, "Japanese"));
      EXPECT_FALSE(IsPredicted(predictor, prefix, "Japanese"));
    }
  }

  // The following are still suggested and predicted.
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(
      IsSuggestedAndPredicted(predictor, "japan", "JapaneseInputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "Input"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "InputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "meth", "Method"));
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntryTrigramDeleteSecond) {
  ScopedClockMock clock(absl::FromUnixSeconds(1));

  // Tests ClearHistoryEntry() for trigram history.  This case tests the
  // deletion of the second node of trigram.
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the same history structure as ClearHistoryEntry_Trigram_DeleteWhole.
  UserHistoryPredictor::Entry *japaneseinputmethod;
  UserHistoryPredictor::Entry *japanese;
  UserHistoryPredictor::Entry *input;
  UserHistoryPredictor::Entry *method;
  InitHistory_JapaneseInputMethod(predictor, &japaneseinputmethod, &japanese,
                                  &input, &method);

  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(
      IsSuggestedAndPredicted(predictor, "japan", "JapaneseInputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "Input"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "InputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "meth", "Method"));

  // Delete the second node of the chain.
  EXPECT_TRUE(predictor->ClearHistoryEntry("input", "Input"));

  // Note that the two links are still alive.
  EXPECT_FALSE(japaneseinputmethod->removed());
  EXPECT_FALSE(japanese->removed());
  EXPECT_TRUE(input->removed());
  EXPECT_FALSE(method->removed());
  EXPECT_TRUE(IsConnected(*japanese, *input));
  EXPECT_TRUE(IsConnected(*input, *method));

  {
    // Now "Input" should never be suggested nor predicted.
    constexpr absl::string_view kKey = "inputmethod";
    for (size_t i = 0; i < kKey.size(); ++i) {
      const absl::string_view prefix = kKey.substr(0, i);
      EXPECT_FALSE(IsSuggested(predictor, prefix, "Input"));
      EXPECT_FALSE(IsPredicted(predictor, prefix, "Input"));
    }
  }

  // The following can still be shown by the predictor.
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(
      IsSuggestedAndPredicted(predictor, "japan", "JapaneseInputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "InputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "meth", "Method"));
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntryTrigramDeleteThird) {
  ScopedClockMock clock(absl::FromUnixSeconds(1));

  // Tests ClearHistoryEntry() for trigram history.  This case tests the
  // deletion of the third node of trigram.
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the same history structure as ClearHistoryEntry_Trigram_DeleteWhole.
  UserHistoryPredictor::Entry *japaneseinputmethod;
  UserHistoryPredictor::Entry *japanese;
  UserHistoryPredictor::Entry *input;
  UserHistoryPredictor::Entry *method;
  InitHistory_JapaneseInputMethod(predictor, &japaneseinputmethod, &japanese,
                                  &input, &method);

  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(
      IsSuggestedAndPredicted(predictor, "japan", "JapaneseInputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "Input"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "InputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "meth", "Method"));

  // Delete the third node, "method".
  EXPECT_TRUE(predictor->ClearHistoryEntry("method", "Method"));

  // Note that the two links are still alive.
  EXPECT_FALSE(japaneseinputmethod->removed());
  EXPECT_FALSE(japanese->removed());
  EXPECT_FALSE(input->removed());
  EXPECT_TRUE(method->removed());
  EXPECT_TRUE(IsConnected(*japanese, *input));
  EXPECT_TRUE(IsConnected(*input, *method));

  {
    // Now "Method" should never be suggested nor predicted.
    constexpr absl::string_view kKey = "method";
    for (size_t i = 0; i < kKey.size(); ++i) {
      const absl::string_view prefix = kKey.substr(0, i);
      EXPECT_FALSE(IsSuggested(predictor, prefix, "Method"));
      EXPECT_FALSE(IsPredicted(predictor, prefix, "Method"));
    }
  }

  // The following can still be shown by the predictor.
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(
      IsSuggestedAndPredicted(predictor, "japan", "JapaneseInputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "Input"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "InputMethod"));
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntryTrigramDeleteFirstBigram) {
  ScopedClockMock clock(absl::FromUnixSeconds(1));

  // Tests ClearHistoryEntry() for trigram history.  This case tests the
  // deletion of the first bigram of trigram.
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the same history structure as ClearHistoryEntry_Trigram_DeleteWhole.
  UserHistoryPredictor::Entry *japaneseinputmethod;
  UserHistoryPredictor::Entry *japanese;
  UserHistoryPredictor::Entry *input;
  UserHistoryPredictor::Entry *method;
  InitHistory_JapaneseInputMethod(predictor, &japaneseinputmethod, &japanese,
                                  &input, &method);

  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(
      IsSuggestedAndPredicted(predictor, "japan", "JapaneseInputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "Input"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "InputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "meth", "Method"));

  // Delete the sentence consisting of the first two nodes.
  EXPECT_TRUE(predictor->ClearHistoryEntry("japaneseinput", "JapaneseInput"));

  // Note that the node "japaneseinput" and the link from "japanese" to "input"
  // were removed.
  EXPECT_FALSE(japaneseinputmethod->removed());
  EXPECT_FALSE(japanese->removed());
  EXPECT_FALSE(input->removed());
  EXPECT_FALSE(method->removed());
  EXPECT_FALSE(IsConnected(*japanese, *input));
  EXPECT_TRUE(IsConnected(*input, *method));

  {
    // Now "JapaneseInput" should never be suggested nor predicted.
    constexpr absl::string_view kKey = "japaneseinputmethod";
    for (size_t i = 0; i < kKey.size(); ++i) {
      const absl::string_view prefix = kKey.substr(0, i);
      EXPECT_FALSE(IsSuggested(predictor, prefix, "JapaneseInput"));
      EXPECT_FALSE(IsPredicted(predictor, prefix, "JapaneseInput"));
    }
  }

  // However, the following can still be available, including
  // "JapaneseInputMethod".
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(
      IsSuggestedAndPredicted(predictor, "japan", "JapaneseInputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "Input"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "InputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "meth", "Method"));
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntryTrigramDeleteSecondBigram) {
  ScopedClockMock clock(absl::FromUnixSeconds(1));

  // Tests ClearHistoryEntry() for trigram history.  This case tests the
  // deletion of the latter bigram of trigram.
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the same history structure as ClearHistoryEntry_Trigram_DeleteWhole.
  UserHistoryPredictor::Entry *japaneseinputmethod;
  UserHistoryPredictor::Entry *japanese;
  UserHistoryPredictor::Entry *input;
  UserHistoryPredictor::Entry *method;
  InitHistory_JapaneseInputMethod(predictor, &japaneseinputmethod, &japanese,
                                  &input, &method);

  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(
      IsSuggestedAndPredicted(predictor, "japan", "JapaneseInputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "Input"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "InputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "meth", "Method"));

  // Delete the latter bigram.
  EXPECT_TRUE(predictor->ClearHistoryEntry("inputmethod", "InputMethod"));

  // Note that only link from "input" to "method" was removed.
  EXPECT_FALSE(japaneseinputmethod->removed());
  EXPECT_FALSE(japanese->removed());
  EXPECT_FALSE(input->removed());
  EXPECT_FALSE(method->removed());
  EXPECT_TRUE(IsConnected(*japanese, *input));
  EXPECT_FALSE(IsConnected(*input, *method));

  {
    // Now "InputMethod" should never be suggested.
    constexpr absl::string_view kKey = "inputmethod";
    for (size_t i = 0; i < kKey.size(); ++i) {
      const absl::string_view prefix = kKey.substr(0, i);
      EXPECT_FALSE(IsSuggested(predictor, prefix, "InputMethod"));
      EXPECT_FALSE(IsPredicted(predictor, prefix, "InputMethod"));
    }
  }

  // However, the following are available.
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(
      IsSuggestedAndPredicted(predictor, "japan", "JapaneseInputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "Input"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "meth", "Method"));
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntryScenario1) {
  // Tests a common scenario: First, a user accidentally inputs an incomplete
  // romaji sequence and the predictor learns it.  Then, the user deletes it.
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Set up history. Convert "ぐーぐｒ" to "グーグr" 3 times.  This emulates a
  // case that a user accidentally input incomplete sequence.
  for (int i = 0; i < 3; ++i) {
    Segments segments;
    const ConversionRequest convreq =
        SetUpInputForConversion("ぐーぐｒ", &composer_, &segments);
    AddCandidate("グーグr", &segments);
    predictor->Finish(convreq, &segments);
  }

  // Test if the predictor learned "グーグr".
  EXPECT_TRUE(IsSuggested(predictor, "ぐーぐ", "グーグr"));
  EXPECT_TRUE(IsPredicted(predictor, "ぐーぐ", "グーグr"));

  // The user tris deleting the history ("ぐーぐｒ", "グーグr").
  EXPECT_TRUE(predictor->ClearHistoryEntry("ぐーぐｒ", "グーグr"));

  // The predictor shouldn't show "グーグr" both for suggestion and prediction.
  EXPECT_FALSE(IsSuggested(predictor, "ぐーぐ", "グーグr"));
  EXPECT_FALSE(IsPredicted(predictor, "ぐーぐ", "グーグr"));
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntryScenario2) {
  // Tests a common scenario: First, a user inputs a sentence ending with a
  // symbol and it's learned by the predictor.  Then, the user deletes the
  // history containing the symbol.
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Set up history. Convert "きょうもいいてんき！" to "今日もいい天気!" 3 times
  // so that the predictor learns the sentence. We assume that this sentence
  // consists of three segments: "今日も|いい天気|!".
  const ConversionRequest convreq = CreateConversionRequest(composer_);
  for (int i = 0; i < 3; ++i) {
    Segments segments;

    // The first segment: ("きょうも", "今日も")
    Segment *seg = segments.add_segment();
    seg->set_key("きょうも");
    seg->set_segment_type(Segment::FIXED_VALUE);
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->value = "今日も";
    candidate->content_value = "今日";
    candidate->key = seg->key();
    candidate->content_key = "きょう";

    // The second segment: ("いいてんき", "いい天気")
    seg = segments.add_segment();
    seg->set_key("いいてんき");
    seg->set_segment_type(Segment::FIXED_VALUE);
    candidate = seg->add_candidate();
    candidate->value = "いい天気";
    candidate->content_value = candidate->value;
    candidate->key = seg->key();
    candidate->content_key = seg->key();

    // The third segment: ("！", "!")
    seg = segments.add_segment();
    seg->set_key("！");
    seg->set_segment_type(Segment::FIXED_VALUE);
    candidate = seg->add_candidate();
    candidate->value = "!";
    candidate->content_value = "!";
    candidate->key = seg->key();
    candidate->content_key = seg->key();

    predictor->Finish(convreq, &segments);
  }

  // Check if the predictor learned the sentence.  Since the symbol is contained
  // in one segment, both "今日もいい天気" and "今日もいい天気!" should be
  // suggested and predicted.
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "きょうも", "今日もいい天気"));
  EXPECT_TRUE(
      IsSuggestedAndPredicted(predictor, "きょうも", "今日もいい天気!"));

  // Now the user deletes the sentence containing the "!".
  EXPECT_TRUE(
      predictor->ClearHistoryEntry("きょうもいいてんき！", "今日もいい天気!"));

  // The sentence "今日もいい天気" should still be suggested and predicted.
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "きょうも", "今日もいい天気"));

  // However, "今日もいい天気!" should be neither suggested nor predicted.
  EXPECT_FALSE(IsSuggested(predictor, "きょうも", "今日もいい天気!"));
  EXPECT_FALSE(IsPredicted(predictor, "きょうも", "今日もいい天気!"));
}

TEST_F(UserHistoryPredictorTest, ContentWordLearningFromInnerSegmentBoundary) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();
  predictor->set_content_word_learning_enabled(true);

  Segments segments;
  {
    constexpr char kKey[] = "とうきょうかなごやにいきたい";
    constexpr char kValue[] = "東京か名古屋に行きたい";
    const ConversionRequest convreq1 =
        SetUpInputForPrediction(kKey, &composer_, &segments);
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->add_candidate();
    candidate->key = kKey;
    candidate->value = kValue;
    candidate->content_key = kKey;
    candidate->content_value = kValue;
    candidate->PushBackInnerSegmentBoundary(18, 9, 15, 6);
    candidate->PushBackInnerSegmentBoundary(12, 12, 9, 9);
    candidate->PushBackInnerSegmentBoundary(12, 12, 12, 12);
    predictor->Finish(convreq1, &segments);
  }

  segments.Clear();
  const ConversionRequest convreq2 =
      SetUpInputForPrediction("と", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
  EXPECT_TRUE(FindCandidateByValue("東京", segments));
  EXPECT_TRUE(FindCandidateByValue("東京か", segments));

  segments.Clear();
  const ConversionRequest convreq3 =
      SetUpInputForPrediction("な", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq3, &segments));
  EXPECT_TRUE(FindCandidateByValue("名古屋", segments));
  EXPECT_TRUE(FindCandidateByValue("名古屋に", segments));

  segments.Clear();
  const ConversionRequest convreq4 =
      SetUpInputForPrediction("い", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq4, &segments));
  EXPECT_TRUE(FindCandidateByValue("行きたい", segments));
}

TEST_F(UserHistoryPredictorTest, JoinedSegmentsTestMobile) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();
  request_test_util::FillMobileRequest(&request_);
  Segments segments;

  const ConversionRequest convreq1 =
      SetUpInputForConversion("わたしの", &composer_, &segments);
  AddCandidate(0, "私の", &segments);

  AddSegment("なまえは", &segments);
  AddCandidate(1, "名前は", &segments);

  predictor->Finish(convreq1, &segments);
  segments.Clear();

  const ConversionRequest convreq2 =
      SetUpInputForSuggestion("わたし", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
  EXPECT_EQ(segments.segment(0).candidates_size(), 1);
  EXPECT_EQ(segments.segment(0).candidate(0).value, "私の");
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
  segments.Clear();

  const ConversionRequest convreq3 =
      SetUpInputForPrediction("わたしの", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq3, &segments));
  EXPECT_EQ(segments.segment(0).candidates_size(), 1);
  EXPECT_EQ(segments.segment(0).candidate(0).value, "私の");
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
  segments.Clear();

  const ConversionRequest convreq4 =
      SetUpInputForPrediction("わたしのな", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq4, &segments));
  EXPECT_EQ(segments.segment(0).candidates_size(), 1);
  EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は");
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
  segments.Clear();
}

TEST_F(UserHistoryPredictorTest, JoinedSegmentsTestDesktop) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;

  const ConversionRequest convreq1 =
      SetUpInputForConversion("わたしの", &composer_, &segments);
  AddCandidate(0, "私の", &segments);

  AddSegment("なまえは", &segments);
  AddCandidate(1, "名前は", &segments);

  predictor->Finish(convreq1, &segments);

  segments.Clear();

  const ConversionRequest convreq2 =
      SetUpInputForSuggestion("わたし", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
  EXPECT_EQ(segments.segment(0).candidates_size(), 2);
  EXPECT_EQ(segments.segment(0).candidate(0).value, "私の");
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
  EXPECT_EQ(segments.segment(0).candidate(1).value, "私の名前は");
  EXPECT_TRUE(segments.segment(0).candidate(1).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
  segments.Clear();

  const ConversionRequest convreq3 =
      SetUpInputForPrediction("わたしの", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq3, &segments));
  EXPECT_EQ(segments.segment(0).candidates_size(), 1);
  EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は");
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
  segments.Clear();

  const ConversionRequest convreq4 =
      SetUpInputForPrediction("わたしのな", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq4, &segments));
  EXPECT_EQ(segments.segment(0).candidates_size(), 1);
  EXPECT_EQ(segments.segment(0).candidate(0).value, "私の名前は");
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
  segments.Clear();
}

TEST_F(UserHistoryPredictorTest, UsageStats) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;
  EXPECT_COUNT_STATS("CommitUserHistoryPredictor", 0);
  EXPECT_COUNT_STATS("CommitUserHistoryPredictorZeroQuery", 0);

  const ConversionRequest convreq1 =
      SetUpInputForConversion("なまえは", &composer_, &segments);
  AddCandidate(0, "名前は", &segments);
  segments.mutable_conversion_segment(0)->mutable_candidate(0)->source_info |=
      Segment::Candidate::USER_HISTORY_PREDICTOR;
  predictor->Finish(convreq1, &segments);

  EXPECT_COUNT_STATS("CommitUserHistoryPredictor", 1);
  EXPECT_COUNT_STATS("CommitUserHistoryPredictorZeroQuery", 0);

  segments.Clear();

  // Zero query
  const ConversionRequest convreq2 =
      SetUpInputForConversion("", &composer_, &segments);
  AddCandidate(0, "名前は", &segments);
  segments.mutable_conversion_segment(0)->mutable_candidate(0)->source_info |=
      Segment::Candidate::USER_HISTORY_PREDICTOR;
  predictor->Finish(convreq2, &segments);

  // UserHistoryPredictor && ZeroQuery
  EXPECT_COUNT_STATS("CommitUserHistoryPredictor", 2);
  EXPECT_COUNT_STATS("CommitUserHistoryPredictorZeroQuery", 1);
}

TEST_F(UserHistoryPredictorTest, PunctuationLinkMobile) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();
  request_test_util::FillMobileRequest(&request_);
  Segments segments;
  {
    const ConversionRequest convreq1 =
        SetUpInputForConversion("ございます", &composer_, &segments);
    AddCandidate(0, "ございます", &segments);
    predictor->Finish(convreq1, &segments);

    const ConversionRequest convreq2 = SetUpInputForConversionWithHistory(
        "!", "ございます", "ございます", &composer_, &segments);
    AddCandidate(1, "！", &segments);
    predictor->Finish(convreq2, &segments);

    segments.Clear();
    const ConversionRequest convreq3 =
        SetUpInputForSuggestion("ございま", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq3, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "ございます");
    EXPECT_FALSE(FindCandidateByValue("ございます！", segments));

    // Zero query from "ございます" -> "！"
    segments.Clear();
    // Output of SetupInputForConversion is not used here.
    SetUpInputForConversion("ございます", &composer_, &segments);
    AddCandidate(0, "ございます", &segments);
    const ConversionRequest convreq4 = SetUpInputForSuggestionWithHistory(
        "", "ございます", "ございます", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq4, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "！");
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    const ConversionRequest convreq1 =
        SetUpInputForConversion("!", &composer_, &segments);
    AddCandidate(0, "！", &segments);
    predictor->Finish(convreq1, &segments);

    const ConversionRequest convreq2 = SetUpInputForSuggestionWithHistory(
        "ございます", "!", "！", &composer_, &segments);
    AddCandidate(1, "ございます", &segments);
    predictor->Finish(convreq2, &segments);

    // Zero query from "！" -> no suggestion
    segments.Clear();
    const ConversionRequest convreq3 = SetUpInputForSuggestionWithHistory(
        "", "!", "！", &composer_, &segments);
    EXPECT_FALSE(predictor->PredictForRequest(convreq3, &segments));
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    const ConversionRequest convreq1 =
        SetUpInputForConversion("ございます!", &composer_, &segments);
    AddCandidate(0, "ございます！", &segments);

    predictor->Finish(convreq1, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    AddSegment("よろしくおねがいします", &segments);
    AddCandidate(1, "よろしくお願いします", &segments);
    predictor->Finish(convreq1, &segments);

    // Zero query from "！" -> no suggestion
    segments.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForConversion("!", &composer_, &segments);
    AddCandidate(0, "！", &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
    AddSegment("", &segments);  // empty request
    EXPECT_FALSE(predictor->PredictForRequest(convreq2, &segments));

    // Zero query from "ございます！" -> no suggestion
    segments.Clear();
    const ConversionRequest convreq3 =
        SetUpInputForConversion("ございます!", &composer_, &segments);
    AddCandidate(0, "ございます！", &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
    AddSegment("", &segments);  // empty request
    EXPECT_FALSE(predictor->PredictForRequest(convreq3, &segments));
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    const ConversionRequest convreq1 =
        SetUpInputForConversion("ございます", &composer_, &segments);
    AddCandidate(0, "ございます", &segments);
    predictor->Finish(convreq1, &segments);

    const ConversionRequest convreq2 = SetUpInputForConversionWithHistory(
        "!よろしくおねがいします", "ございます", "ございます", &composer_,
        &segments);
    AddCandidate(1, "！よろしくお願いします", &segments);
    predictor->Finish(convreq2, &segments);

    segments.Clear();
    const ConversionRequest convreq3 =
        SetUpInputForSuggestion("ございま", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq3, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "ございます");
    EXPECT_FALSE(
        FindCandidateByValue("ございます！よろしくお願いします", segments));

    // Zero query from "ございます" -> no suggestion
    const ConversionRequest convreq4 = SetUpInputForConversionWithHistory(
        "", "ございます", "ございます", &composer_, &segments);
    AddSegment("", &segments);  // empty request
    EXPECT_FALSE(predictor->PredictForRequest(convreq4, &segments));
  }
}

TEST_F(UserHistoryPredictorTest, PunctuationLinkDesktop) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();
  Segments segments;
  {
    const ConversionRequest convreq1 =
        SetUpInputForConversion("ございます", &composer_, &segments);
    AddCandidate(0, "ございます", &segments);

    predictor->Finish(convreq1, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    AddSegment("!", &segments);
    AddCandidate(1, "！", &segments);
    predictor->Finish(convreq1, &segments);

    segments.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForSuggestion("ございま", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "ございます");
    EXPECT_FALSE(FindCandidateByValue("ございます！", segments));

    segments.Clear();
    const ConversionRequest convreq3 =
        SetUpInputForSuggestion("ございます", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq3, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "ございます");
    EXPECT_FALSE(FindCandidateByValue("ございます！", segments));
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    const ConversionRequest convreq1 =
        SetUpInputForConversion("!", &composer_, &segments);
    AddCandidate(0, "！", &segments);

    predictor->Finish(convreq1, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    AddSegment("よろしくおねがいします", &segments);
    AddCandidate(1, "よろしくお願いします", &segments);
    predictor->Finish(convreq1, &segments);

    segments.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForSuggestion("!", &composer_, &segments);
    EXPECT_FALSE(predictor->PredictForRequest(convreq2, &segments));
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    const ConversionRequest convreq1 =
        SetUpInputForConversion("ございます!", &composer_, &segments);
    AddCandidate(0, "ございます！", &segments);

    predictor->Finish(convreq1, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    AddSegment("よろしくおねがいします", &segments);
    AddCandidate(1, "よろしくお願いします", &segments);
    predictor->Finish(convreq1, &segments);

    segments.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForSuggestion("ございます", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value,
              "ございます！");
    EXPECT_FALSE(
        FindCandidateByValue("ございます！よろしくお願いします", segments));

    segments.Clear();
    const ConversionRequest convreq3 =
        SetUpInputForSuggestion("ございます!", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq3, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value,
              "ございます！");
    EXPECT_FALSE(
        FindCandidateByValue("ございます！よろしくお願いします", segments));
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    const ConversionRequest convreq1 =
        SetUpInputForConversion("ございます", &composer_, &segments);
    AddCandidate(0, "ございます", &segments);

    predictor->Finish(convreq1, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    AddSegment("!よろしくおねがいします", &segments);
    AddCandidate(1, "！よろしくお願いします", &segments);
    predictor->Finish(convreq1, &segments);

    segments.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForSuggestion("ございます", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "ございます");
    EXPECT_FALSE(FindCandidateByValue("ございます！", segments));
    EXPECT_FALSE(
        FindCandidateByValue("ございます！よろしくお願いします", segments));
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    // Note that "よろしくお願いします:よろしくおねがいします" is the sentence
    // like candidate. Please refer to user_history_predictor.cc
    const ConversionRequest convreq1 = SetUpInputForConversion(
        "よろしくおねがいします", &composer_, &segments);
    AddCandidate(0, "よろしくお願いします", &segments);

    predictor->Finish(convreq1, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    AddSegment("!", &segments);
    AddCandidate(1, "！", &segments);
    predictor->Finish(convreq1, &segments);

    segments.Clear();
    const ConversionRequest convreq2 = SetUpInputForSuggestion(
        "よろしくおねがいします", &composer_, &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
    EXPECT_TRUE(FindCandidateByValue("よろしくお願いします！", segments));
  }
}

TEST_F(UserHistoryPredictorTest, 62DayOldEntriesAreDeletedAtSync) {
  ScopedClockMock clock(absl::FromUnixSeconds(1));

  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Let the predictor learn "私の名前は中野です".
  Segments segments;
  const ConversionRequest convreq1 = SetUpInputForConversion(
      "わたしのなまえはなかのです", &composer_, &segments);
  AddCandidate("私の名前は中野です", &segments);
  predictor->Finish(convreq1, &segments);

  // Verify that "私の名前は中野です" is predicted.
  segments.Clear();
  const ConversionRequest convreq2 =
      SetUpInputForPrediction("わたしの", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
  EXPECT_TRUE(FindCandidateByValue("私の名前は中野です", segments));

  // Now, simulate the case where 63 days passed.
  clock->Advance(absl::Hours(63 * 24));

  // Let the predictor learn "私の名前は高橋です".
  segments.Clear();
  const ConversionRequest convreq3 = SetUpInputForConversion(
      "わたしのなまえはたかはしです", &composer_, &segments);
  AddCandidate("私の名前は高橋です", &segments);
  predictor->Finish(convreq3, &segments);

  // Verify that "私の名前は高橋です" is predicted but "私の名前は中野です" is
  // not.  The latter one is still in on-memory data structure but lookup is
  // prevented.  The entry is removed when the data is written to disk.
  segments.Clear();
  const ConversionRequest convreq4 =
      SetUpInputForPrediction("わたしの", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq4, &segments));
  EXPECT_TRUE(FindCandidateByValue("私の名前は高橋です", segments));
  EXPECT_FALSE(FindCandidateByValue("私の名前は中野です", segments));

  // Here, write the history to a storage.
  ASSERT_TRUE(predictor->Sync());
  WaitForSyncer(predictor);

  // Verify that "私の名前は中野です" is no longer predicted because it was
  // learned 63 days before.
  segments.Clear();
  const ConversionRequest convreq5 =
      SetUpInputForPrediction("わたしの", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq5, &segments));
  EXPECT_TRUE(FindCandidateByValue("私の名前は高橋です", segments));
  EXPECT_FALSE(FindCandidateByValue("私の名前は中野です", segments));

  // Verify also that on-memory data structure doesn't contain node for 中野.
  bool found_takahashi = false;
  for (const auto &elem : *predictor->dic_) {
    EXPECT_EQ(elem.value.value().find("中野"), std::string::npos);
    if (elem.value.value().find("高橋")) {
      found_takahashi = true;
    }
  }
  EXPECT_TRUE(found_takahashi);
}

TEST_F(UserHistoryPredictorTest, FutureTimestamp) {
  // Test the case where history has "future" timestamps.
  ScopedClockMock clock(absl::FromUnixSeconds(10000));

  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Let the predictor learn "私の名前は中野です".
  Segments segments;
  const ConversionRequest convreq1 = SetUpInputForConversion(
      "わたしのなまえはなかのです", &composer_, &segments);
  AddCandidate("私の名前は中野です", &segments);
  predictor->Finish(convreq1, &segments);

  // Verify that "私の名前は中野です" is predicted.
  segments.Clear();
  const ConversionRequest convreq2 =
      SetUpInputForPrediction("わたしの", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
  EXPECT_TRUE(FindCandidateByValue("私の名前は中野です", segments));

  // Now, go back to the past.
  clock->SetTime(absl::FromUnixSeconds(1));

  // Verify that "私の名前は中野です" is predicted without crash.
  segments.Clear();
  const ConversionRequest convreq3 =
      SetUpInputForPrediction("わたしの", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq3, &segments));
  EXPECT_TRUE(FindCandidateByValue("私の名前は中野です", segments));
}

TEST_F(UserHistoryPredictorTest, MaxPredictionCandidatesSize) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();
  Segments segments;
  {
    const ConversionRequest convreq =
        SetUpInputForPrediction("てすと", &composer_, &segments);
    AddCandidate(0, "てすと", &segments);
    predictor->Finish(convreq, &segments);
  }
  {
    const ConversionRequest convreq =
        SetUpInputForPrediction("てすと", &composer_, &segments);
    AddCandidate(0, "テスト", &segments);
    predictor->Finish(convreq, &segments);
  }
  {
    const ConversionRequest convreq =
        SetUpInputForPrediction("てすと", &composer_, &segments);
    AddCandidate(0, "Test", &segments);
    predictor->Finish(convreq, &segments);
  }
  {
    ConversionRequest convreq = CreateConversionRequest(composer_);
    convreq.set_max_user_history_prediction_candidates_size(2);
    convreq.set_request_type(ConversionRequest::SUGGESTION);
    MakeSegments("てすと", &segments);

    EXPECT_TRUE(predictor->PredictForRequest(convreq, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    EXPECT_EQ(segments.segment(0).candidates_size(), 2);

    convreq.set_request_type(ConversionRequest::PREDICTION);
    MakeSegments("てすと", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(convreq, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    EXPECT_EQ(segments.segment(0).candidates_size(), 2);
  }
  {
    ConversionRequest convreq1 =
        SetUpInputForSuggestion("てすと", &composer_, &segments);
    convreq1.set_max_user_history_prediction_candidates_size(3);
    EXPECT_TRUE(predictor->PredictForRequest(convreq1, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    EXPECT_EQ(segments.segment(0).candidates_size(), 3);

    ConversionRequest convreq2 =
        SetUpInputForPrediction("てすと", &composer_, &segments);
    convreq2.set_max_user_history_prediction_candidates_size(3);
    EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    EXPECT_EQ(segments.segment(0).candidates_size(), 3);
  }

  {
    // Only 3 candidates in user history
    ConversionRequest convreq1 =
        SetUpInputForSuggestion("てすと", &composer_, &segments);
    convreq1.set_max_user_history_prediction_candidates_size(4);
    EXPECT_TRUE(predictor->PredictForRequest(convreq1, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    EXPECT_EQ(segments.segment(0).candidates_size(), 3);

    ConversionRequest convreq2 =
        SetUpInputForPrediction("てすと", &composer_, &segments);
    convreq2.set_max_user_history_prediction_candidates_size(4);
    EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    EXPECT_EQ(segments.segment(0).candidates_size(), 3);
  }
}

TEST_F(UserHistoryPredictorTest, MaxPredictionCandidatesSizeForZeroQuery) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();
  request_test_util::FillMobileRequest(&request_);
  Segments segments;

  const ConversionRequest convreq =
      SetUpInputForPrediction("てすと", &composer_, &segments);
  {
    AddCandidate(0, "てすと", &segments);
    predictor->Finish(convreq, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
  }
  {
    AddSegment("かお", &segments);
    AddCandidate(1, "😀", &segments);
    predictor->Finish(convreq, &segments);
  }
  {
    Segment::Candidate *candidate =
        segments.mutable_segment(1)->mutable_candidate(0);
    candidate->value = "😎";
    candidate->content_value = candidate->value;
    predictor->Finish(convreq, &segments);
  }
  {
    Segment::Candidate *candidate =
        segments.mutable_segment(1)->mutable_candidate(0);
    candidate->value = "😂";
    candidate->content_value = candidate->value;
    predictor->Finish(convreq, &segments);
  }

  // normal prediction candidates size
  {
    ConversionRequest convreq1 =
        SetUpInputForSuggestion("かお", &composer_, &segments);
    convreq1.set_max_user_history_prediction_candidates_size(2);
    convreq1.set_max_user_history_prediction_candidates_size_for_zero_query(3);
    EXPECT_TRUE(predictor->PredictForRequest(convreq1, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    EXPECT_EQ(segments.segment(0).candidates_size(), 2);

    ConversionRequest convreq2 =
        SetUpInputForPrediction("かお", &composer_, &segments);
    convreq2.set_max_user_history_prediction_candidates_size(2);
    convreq2.set_max_user_history_prediction_candidates_size_for_zero_query(3);
    EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    EXPECT_EQ(segments.segment(0).candidates_size(), 2);
  }

  // prediction candidates for zero query
  {
    ConversionRequest convreq1 = SetUpInputForSuggestionWithHistory(
        "", "てすと", "てすと", &composer_, &segments);
    convreq1.set_max_user_history_prediction_candidates_size(2);
    convreq1.set_max_user_history_prediction_candidates_size_for_zero_query(3);
    EXPECT_TRUE(predictor->PredictForRequest(convreq1, &segments));
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 3);

    ConversionRequest convreq2 = SetUpInputForPredictionWithHistory(
        "", "てすと", "てすと", &composer_, &segments);
    convreq2.set_max_user_history_prediction_candidates_size(2);
    convreq2.set_max_user_history_prediction_candidates_size_for_zero_query(3);
    EXPECT_TRUE(predictor->PredictForRequest(convreq2, &segments));
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 3);
  }
}

TEST_F(UserHistoryPredictorTest, TypingCorrection) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();
  ScopedClockMock clock(absl::FromUnixSeconds(1));

  Segments segments;
  {
    clock->Advance(absl::Hours(1));
    const ConversionRequest convreq =
        SetUpInputForPrediction("がっこう", &composer_, &segments);
    AddCandidate(0, "学校", &segments);
    predictor->Finish(convreq, &segments);
  }

  {
    clock->Advance(absl::Hours(1));
    const ConversionRequest convreq =
        SetUpInputForPrediction("がっこう", &composer_, &segments);
    AddCandidate(0, "ガッコウ", &segments);
    predictor->Finish(convreq, &segments);
  }

  {
    clock->Advance(absl::Hours(1));
    const ConversionRequest convreq =
        SetUpInputForPrediction("かっこう", &composer_, &segments);
    AddCandidate(0, "格好", &segments);
    predictor->Finish(convreq, &segments);
  }

  request_.mutable_decoder_experiment_params()
      ->set_typing_correction_apply_user_history_size(1);

  const ConversionRequest convreq1 =
      SetUpInputForSuggestion("がっこ", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq1, &segments));

  // No typing correction.
  const ConversionRequest convreq2 =
      SetUpInputForSuggestion("かつこ", &composer_, &segments);
  EXPECT_FALSE(predictor->PredictForRequest(convreq2, &segments));

  std::vector<TypeCorrectedQuery> expected;
  auto add_expected = [&](const std::string &key) {
    expected.emplace_back(
        TypeCorrectedQuery{key, TypeCorrectedQuery::CORRECTION, 1.0});
  };

  // かつこ -> がっこ and かっこ
  add_expected("がっこ");
  add_expected("かっこ");
  engine::MockSupplementalModel mock;
  EXPECT_CALL(mock, CorrectComposition(_, _)).WillRepeatedly(Return(expected));
  SetSupplementalModel(&mock);

  // set_typing_correction_apply_user_history_size=0
  request_.mutable_decoder_experiment_params()
      ->set_typing_correction_apply_user_history_size(0);
  const ConversionRequest convreq3 =
      SetUpInputForSuggestion("かつこ", &composer_, &segments);
  EXPECT_FALSE(predictor->PredictForRequest(convreq3, &segments));

  // set_typing_correction_apply_user_history_size=1
  request_.mutable_decoder_experiment_params()
      ->set_typing_correction_apply_user_history_size(1);
  const ConversionRequest convreq4 =
      SetUpInputForSuggestion("かつこ", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq4, &segments));
  ASSERT_EQ(segments.segments_size(), 1);
  ASSERT_EQ(segments.segment(0).candidates_size(), 2);
  EXPECT_EQ(segments.segment(0).candidate(0).value, "ガッコウ");
  EXPECT_EQ(segments.segment(0).candidate(1).value, "学校");

  // set_typing_correction_apply_user_history_size=2
  request_.mutable_decoder_experiment_params()
      ->set_typing_correction_apply_user_history_size(2);
  const ConversionRequest convreq5 =
      SetUpInputForSuggestion("かつこ", &composer_, &segments);
  EXPECT_TRUE(predictor->PredictForRequest(convreq5, &segments));
  ASSERT_EQ(segments.segments_size(), 1);
  ASSERT_EQ(segments.segment(0).candidates_size(), 3);
  EXPECT_EQ(segments.segment(0).candidate(0).value, "格好");
  EXPECT_EQ(segments.segment(0).candidate(1).value, "ガッコウ");
  EXPECT_EQ(segments.segment(0).candidate(2).value, "学校");

  SetSupplementalModel(nullptr);
  const ConversionRequest convreq6 =
      SetUpInputForSuggestion("かつこ", &composer_, &segments);
  EXPECT_FALSE(predictor->PredictForRequest(convreq6, &segments));
}

TEST_F(UserHistoryPredictorTest, MaxCharCoverage) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();
  Segments segments;

  {
    const ConversionRequest convreq1 =
        SetUpInputForPrediction("てすと", &composer_, &segments);
    AddCandidate(0, "てすと", &segments);
    predictor->Finish(convreq1, &segments);
  }
  {
    const ConversionRequest convreq2 =
        SetUpInputForPrediction("てすと", &composer_, &segments);
    AddCandidate(0, "テスト", &segments);
    predictor->Finish(convreq2, &segments);
  }
  {
    const ConversionRequest convreq3 =
        SetUpInputForPrediction("てすと", &composer_, &segments);
    AddCandidate(0, "Test", &segments);
    predictor->Finish(convreq3, &segments);
  }

  // [max_char_coverage, expected_candidate_size]
  const std::vector<std::pair<int, int>> kTestCases = {
      {1, 1}, {2, 1}, {3, 1}, {4, 1},  {5, 1}, {6, 2},
      {7, 2}, {8, 2}, {9, 2}, {10, 3}, {11, 3}};

  for (const auto &[coverage, candidates_size] : kTestCases) {
    request_.mutable_decoder_experiment_params()
        ->set_user_history_prediction_max_char_coverage(coverage);
    MakeSegments("てすと", &segments);
    ConversionRequest convreq = CreateConversionRequest(composer_);
    convreq.set_request_type(ConversionRequest::SUGGESTION);

    EXPECT_TRUE(predictor->PredictForRequest(convreq, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    EXPECT_EQ(segments.segment(0).candidates_size(), candidates_size);
  }
}

TEST_F(UserHistoryPredictorTest, RemoveRedundantCandidates) {
  // pass the input candidates and expected (filtered) candidates.
  auto run_test = [this](int filter_mode,
                         absl::Span<const absl::string_view> candidates,
                         absl::Span<const absl::string_view> expected) {
    ScopedClockMock clock(absl::FromUnixSeconds(1));
    UserHistoryPredictor *predictor =
        GetUserHistoryPredictorWithClearedHistory();
    Segments segments;
    // Insert in reverse order to emulate LRU.
    for (auto it = candidates.rbegin(); it != candidates.rend(); ++it) {
      clock->Advance(absl::Hours(1));
      const ConversionRequest convreq =
          SetUpInputForPrediction("とうき", &composer_, &segments);
      AddCandidate(0, *it, &segments);
      predictor->Finish(convreq, &segments);
    }
    request_.mutable_decoder_experiment_params()
        ->set_user_history_prediction_filter_redundant_candidates_mode(
            filter_mode);
    MakeSegments("とうき", &segments);
    ConversionRequest convreq = CreateConversionRequest(composer_);
    convreq.set_request_type(ConversionRequest::SUGGESTION);
    convreq.set_max_user_history_prediction_candidates_size(10);
    EXPECT_TRUE(predictor->PredictForRequest(convreq, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    ASSERT_EQ(expected.size(), segments.segment(0).candidates_size());
    for (int i = 0; i < expected.size(); ++i) {
      EXPECT_EQ(expected[i], segments.segment(0).candidate(i).value);
    }
  };

  // filter long entries.
  run_test(1, {"東京", "東京は"}, {"東京"});
  run_test(1, {"東京", "東京は", "東京で"}, {"東京"});
  run_test(1, {"東京は", "東京で", "東京"}, {"東京は", "東京で", "東京"});

  // filter short entries.
  run_test(2, {"東京", "東京は"}, {"東京", "東京は"});
  run_test(2, {"東京", "東京は", "東京で"}, {"東京", "東京は", "東京で"});
  run_test(2, {"東京は", "東京で", "東京"}, {"東京は", "東京で"});

  // replace short entries.
  run_test(4, {"東京", "東京は"}, {"東京", "東京は"});
  run_test(4, {"東京は", "東京"}, {"東京"});
  // only replace the first entry.
  run_test(4, {"東京は", "東京で", "東京"}, {"東京", "東京で"});

  // filter long and short entries.
  run_test(1 + 2, {"東京は", "東京", "大阪", "大阪は"}, {"東京は", "大阪"});
  run_test(1 + 2, {"東京", "東京は", "大阪は", "大阪"}, {"東京", "大阪は"});

  // filter long and replace short entries.
  run_test(1 + 4, {"東京は", "東京", "大阪", "大阪は"}, {"東京", "大阪"});
  run_test(1 + 4, {"東京", "東京は", "大阪は", "大阪"}, {"東京", "大阪"});

  // Suffix is non hiragana
  // Default setting doesn't allow non-hiragana suffix.
  run_test(1 + 2, {"東京駅", "東京", "大阪", "大阪駅"},
           {"東京駅", "東京", "大阪", "大阪駅"});
  run_test(1 + 2, {"東京", "東京駅", "大阪駅", "大阪"},
           {"東京", "東京駅", "大阪駅", "大阪"});

  // filter long and replace short entries.
  run_test(1 + 4, {"東京駅", "東京", "大阪", "大阪駅"},
           {"東京駅", "東京", "大阪", "大阪駅"});
  run_test(1 + 4, {"東京", "東京駅", "大阪駅", "大阪"},
           {"東京", "東京駅", "大阪駅", "大阪"});

  // filter long and short entries.
  run_test(1 + 2, {"東京は", "東京", "大阪", "大阪駅"},
           {"東京は", "大阪", "大阪駅"});
  run_test(1 + 2, {"東京", "東京は", "大阪駅", "大阪"},
           {"東京", "大阪駅", "大阪"});

  // filter long and replace short entries.
  run_test(1 + 4, {"東京は", "東京", "大阪", "大阪駅"},
           {"東京", "大阪", "大阪駅"});
  run_test(1 + 4, {"東京", "東京は", "大阪駅", "大阪"},
           {"東京", "大阪駅", "大阪"});

  // Allows non-hiragana suffix.
  // filter long and short entries.
  run_test(1 + 2 + 8, {"東京駅", "東京", "大阪", "大阪駅"}, {"東京駅", "大阪"});
  run_test(1 + 2 + 8, {"東京", "東京駅", "大阪駅", "大阪"}, {"東京", "大阪駅"});

  // filter long and replace short entries.
  run_test(1 + 4 + 8, {"東京駅", "東京", "大阪", "大阪駅"}, {"東京", "大阪"});
  run_test(1 + 4 + 8, {"東京", "東京駅", "大阪駅", "大阪"}, {"東京", "大阪"});

  // filter long and short entries.
  run_test(1 + 2 + 8, {"東京は", "東京", "大阪", "大阪駅"}, {"東京は", "大阪"});
  run_test(1 + 2 + 8, {"東京", "東京は", "大阪駅", "大阪"}, {"東京", "大阪駅"});

  // filter long and replace short entries.
  run_test(1 + 4 + 8, {"東京は", "東京", "大阪", "大阪駅"}, {"東京", "大阪"});
  run_test(1 + 4 + 8, {"東京", "東京は", "大阪駅", "大阪"}, {"東京", "大阪"});
}

TEST_F(UserHistoryPredictorTest, ContentValueZeroQuery) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  request_.mutable_decoder_experiment_params()
      ->set_user_history_prediction_aggressive_bigram(true);

  // Remember 私の名前は中野です
  Segments segments;
  {
    constexpr absl::string_view kKey = "わたしのなまえはなかのです";
    constexpr absl::string_view kValue = "私の名前は中野です";
    const ConversionRequest convreq =
        SetUpInputForPrediction(kKey, &composer_, &segments);
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->add_candidate();
    CHECK(candidate);
    candidate->value = kValue;
    candidate->content_value = kValue;
    candidate->key = kKey;
    candidate->content_key = kKey;
    // "わたしの, 私の", "わたし, 私"
    candidate->PushBackInnerSegmentBoundary(12, 6, 9, 3);
    // "なまえは, 名前は", "なまえ, 名前"
    candidate->PushBackInnerSegmentBoundary(12, 9, 9, 6);
    // "なかのです, 中野です", "なかの, 中野"
    candidate->PushBackInnerSegmentBoundary(15, 12, 9, 6);
    predictor->Finish(convreq, &segments);
  }

  // Zero query from content values. suffix is suggested.
  const std::vector<
      std::tuple<absl::string_view, absl::string_view, absl::string_view>>
      kZeroQueryTest = {{"わたし", "私", "の"},
                        {"なまえ", "名前", "は"},
                        {"なかの", "中野", "です"},
                        {"わたしの", "私の", "名前"},
                        {"なまえは", "名前は", "中野"}};
  for (const auto &[hist_key, hist_value, suggestion] : kZeroQueryTest) {
    segments.Clear();
    const ConversionRequest convreq1 =
        SetUpInputForConversion(hist_key, &composer_, &segments);
    AddCandidate(0, hist_value, &segments);
    predictor->Finish(convreq1, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
    request_.set_zero_query_suggestion(true);
    const ConversionRequest convreq2 = SetUpInputForSuggestionWithHistory(
        "", hist_key, hist_value, &composer_, &segments);
    ASSERT_TRUE(predictor->PredictForRequest(convreq2, &segments));
  }

  // Bigram History.
  {
    segments.Clear();
    const ConversionRequest convreq1 =
        SetUpInputForSuggestion("", &composer_, &segments);
    PrependHistorySegments("の", "の", &segments);
    PrependHistorySegments("わたし", "私", &segments);
    request_.set_zero_query_suggestion(true);
    ASSERT_TRUE(predictor->PredictForRequest(convreq1, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "名前");

    segments.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForSuggestion("", &composer_, &segments);
    PrependHistorySegments("は", "は", &segments);
    PrependHistorySegments("なまえ", "名前", &segments);
    request_.set_zero_query_suggestion(true);
    ASSERT_TRUE(predictor->PredictForRequest(convreq2, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "中野");
  }
}
}  // namespace mozc::prediction
