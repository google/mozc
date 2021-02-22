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

#include <memory>
#include <set>
#include <string>

#include "base/clock_mock.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/password_manager.h"
#include "base/port.h"
#include "base/system_util.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/suppression_dictionary.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "session/request_test_util.h"
#include "storage/encrypted_string_storage.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"
#include "absl/flags/flag.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {

using std::unique_ptr;

using commands::Request;
using config::Config;
using dictionary::DictionaryMock;
using dictionary::SuppressionDictionary;
using dictionary::Token;

void AddSegmentForSuggestion(const std::string &key, Segments *segments) {
  segments->set_max_prediction_candidates_size(10);
  segments->set_request_type(Segments::SUGGESTION);
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FIXED_VALUE);
}

void MakeSegmentsForSuggestion(const std::string &key, Segments *segments) {
  segments->Clear();
  AddSegmentForSuggestion(key, segments);
}

void SetUpInputForSuggestion(const std::string &key,
                             composer::Composer *composer, Segments *segments) {
  composer->Reset();
  composer->SetPreeditTextForTestOnly(key);
  MakeSegmentsForSuggestion(key, segments);
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

void SetUpInputForSuggestionWithHistory(const std::string &key,
                                        const std::string &hist_key,
                                        const std::string &hist_value,
                                        composer::Composer *composer,
                                        Segments *segments) {
  SetUpInputForSuggestion(key, composer, segments);
  PrependHistorySegments(hist_key, hist_value, segments);
}

void AddSegmentForPrediction(const std::string &key, Segments *segments) {
  segments->set_max_prediction_candidates_size(10);
  segments->set_request_type(Segments::PREDICTION);
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FIXED_VALUE);
}

void MakeSegmentsForPrediction(const std::string &key, Segments *segments) {
  segments->Clear();
  AddSegmentForPrediction(key, segments);
}

void SetUpInputForPrediction(const std::string &key,
                             composer::Composer *composer, Segments *segments) {
  composer->Reset();
  composer->SetPreeditTextForTestOnly(key);
  MakeSegmentsForPrediction(key, segments);
}

void AddSegmentForConversion(const std::string &key, Segments *segments) {
  segments->set_request_type(Segments::CONVERSION);
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FIXED_VALUE);
}

void MakeSegmentsForConversion(const std::string &key, Segments *segments) {
  segments->Clear();
  AddSegmentForConversion(key, segments);
}

void SetUpInputForConversion(const std::string &key,
                             composer::Composer *composer, Segments *segments) {
  composer->Reset();
  composer->SetPreeditTextForTestOnly(key);
  MakeSegmentsForConversion(key, segments);
}

void SetUpInputForConversionWithHistory(const std::string &key,
                                        const std::string &hist_key,
                                        const std::string &hist_value,
                                        composer::Composer *composer,
                                        Segments *segments) {
  SetUpInputForConversion(key, composer, segments);
  PrependHistorySegments(hist_key, hist_value, segments);
}

void AddCandidate(size_t index, const std::string &value, Segments *segments) {
  Segment::Candidate *candidate =
      segments->mutable_segment(index)->add_candidate();
  CHECK(candidate);
  candidate->Init();
  candidate->value = value;
  candidate->content_value = value;
  candidate->key = segments->segment(index).key();
  candidate->content_key = segments->segment(index).key();
}

void AddCandidateWithDescription(size_t index, const std::string &value,
                                 const std::string &desc, Segments *segments) {
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

void AddCandidate(const std::string &value, Segments *segments) {
  AddCandidate(0, value, segments);
}

void AddCandidateWithDescription(const std::string &value,
                                 const std::string &desc, Segments *segments) {
  AddCandidateWithDescription(0, value, desc, segments);
}

bool FindCandidateByValue(const std::string &value, const Segments &segments) {
  for (size_t i = 0; i < segments.conversion_segment(0).candidates_size();
       ++i) {
    if (segments.conversion_segment(0).candidate(i).value == value) {
      return true;
    }
  }
  return false;
}
}  // namespace

class UserHistoryPredictorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    SystemUtil::SetUserProfileDirectory(absl::GetFlag(FLAGS_test_tmpdir));
    request_ = absl::make_unique<Request>();
    config_ = absl::make_unique<Config>();
    config::ConfigHandler::GetDefaultConfig(config_.get());
    table_ = absl::make_unique<composer::Table>();
    composer_ = absl::make_unique<composer::Composer>(
        table_.get(), request_.get(), config_.get());
    convreq_ = absl::make_unique<ConversionRequest>(
        composer_.get(), request_.get(), config_.get());
    data_and_predictor_.reset(CreateDataAndPredictor());

    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
  }

  void TearDown() override {
    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
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

  DictionaryMock *GetDictionaryMock() {
    return data_and_predictor_->dictionary.get();
  }

  SuppressionDictionary *GetSuppressionDictionary() {
    return data_and_predictor_->suppression_dictionary.get();
  }

  static bool IsSuggested(UserHistoryPredictor *predictor,
                          const std::string &key, const std::string &value) {
    const ConversionRequest conversion_request;
    Segments segments;
    MakeSegmentsForSuggestion(key, &segments);
    return predictor->PredictForRequest(conversion_request, &segments) &&
           FindCandidateByValue(value, segments);
  }

  static bool IsPredicted(UserHistoryPredictor *predictor,
                          const std::string &key, const std::string &value) {
    const ConversionRequest conversion_request;
    Segments segments;
    MakeSegmentsForPrediction(key, &segments);
    return predictor->PredictForRequest(conversion_request, &segments) &&
           FindCandidateByValue(value, segments);
  }

  static bool IsSuggestedAndPredicted(UserHistoryPredictor *predictor,
                                      const std::string &key,
                                      const std::string &value) {
    return IsSuggested(predictor, key, value) &&
           IsPredicted(predictor, key, value);
  }

  static UserHistoryPredictor::Entry *InsertEntry(
      UserHistoryPredictor *predictor, const std::string &key,
      const std::string &value) {
    UserHistoryPredictor::Entry *e =
        &predictor->dic_->Insert(predictor->Fingerprint(key, value))->value;
    e->set_key(key);
    e->set_value(value);
    e->set_removed(false);
    return e;
  }

  static UserHistoryPredictor::Entry *AppendEntry(
      UserHistoryPredictor *predictor, const std::string &key,
      const std::string &value, UserHistoryPredictor::Entry *prev) {
    prev->add_next_entries()->set_entry_fp(predictor->Fingerprint(key, value));
    UserHistoryPredictor::Entry *e = InsertEntry(predictor, key, value);
    return e;
  }

  static bool IsConnected(const UserHistoryPredictor::Entry &prev,
                          const UserHistoryPredictor::Entry &next) {
    const uint32 fp =
        UserHistoryPredictor::Fingerprint(next.key(), next.value());
    for (size_t i = 0; i < prev.next_entries_size(); ++i) {
      if (prev.next_entries(i).entry_fp() == fp) {
        return true;
      }
    }
    return false;
  }

  // Helper function to create a test case for bigram history deletion.
  static void InitHistory_JapaneseInput(
      UserHistoryPredictor *predictor,
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
  static void InitHistory_JapaneseInputMethod(
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

  unique_ptr<composer::Composer> composer_;
  unique_ptr<composer::Table> table_;
  unique_ptr<ConversionRequest> convreq_;
  unique_ptr<Config> config_;
  unique_ptr<Request> request_;

 private:
  struct DataAndPredictor {
    unique_ptr<DictionaryMock> dictionary;
    unique_ptr<SuppressionDictionary> suppression_dictionary;
    unique_ptr<UserHistoryPredictor> predictor;
    dictionary::POSMatcher pos_matcher;
  };

  DataAndPredictor *CreateDataAndPredictor() const {
    DataAndPredictor *ret = new DataAndPredictor;
    testing::MockDataManager data_manager;
    ret->dictionary = absl::make_unique<DictionaryMock>();
    ret->suppression_dictionary = absl::make_unique<SuppressionDictionary>();
    ret->pos_matcher.Set(data_manager.GetPOSMatcherData());
    ret->predictor = absl::make_unique<UserHistoryPredictor>(
        ret->dictionary.get(), &ret->pos_matcher,
        ret->suppression_dictionary.get(), false);
    ret->predictor->WaitForSyncer();
    return ret;
  }

  unique_ptr<DataAndPredictor> data_and_predictor_;
  mozc::usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;
};

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorTest) {
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);

    // Nothing happen
    {
      Segments segments;
      SetUpInputForSuggestion("てすと", composer_.get(), &segments);
      EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
      EXPECT_EQ(0, segments.segment(0).candidates_size());
    }

    // Nothing happen
    {
      Segments segments;
      SetUpInputForSuggestion("てすと", composer_.get(), &segments);
      EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
      EXPECT_EQ(0, segments.segment(0).candidates_size());
    }

    // Insert two items
    {
      Segments segments;
      SetUpInputForSuggestion("わたしのなまえはなかのです", composer_.get(),
                              &segments);
      AddCandidate("私の名前は中野です", &segments);
      predictor->Finish(*convreq_, &segments);

      segments.Clear();
      SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
      EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
      EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);
      EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
                  Segment::Candidate::USER_HISTORY_PREDICTOR);

      segments.Clear();
      SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
      EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
      EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);
      EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
                  Segment::Candidate::USER_HISTORY_PREDICTOR);
    }

    // Insert without learning (nothing happen).
    {
      config::Config::HistoryLearningLevel no_learning_levels[] = {
          config::Config::READ_ONLY, config::Config::NO_HISTORY};
      for (config::Config::HistoryLearningLevel level : no_learning_levels) {
        config_->set_history_learning_level(level);

        Segments segments;
        SetUpInputForSuggestion("こんにちはさようなら", composer_.get(),
                                &segments);
        AddCandidate("今日はさようなら", &segments);
        predictor->Finish(*convreq_, &segments);

        segments.Clear();
        SetUpInputForSuggestion("こんにちは", composer_.get(), &segments);
        EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
        SetUpInputForSuggestion("こんにちは", composer_.get(), &segments);
        EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
      }
      config_->set_history_learning_level(config::Config::DEFAULT_HISTORY);
    }

    // sync
    predictor->Sync();
    Util::Sleep(500);
  }

  // reload
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);
    Segments segments;

    // turn off
    {
      Segments segments;
      config_->set_use_history_suggest(false);

      SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
      EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

      config_->set_use_history_suggest(true);
      config_->set_incognito_mode(true);

      SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
      EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

      config_->set_incognito_mode(false);
      config_->set_history_learning_level(config::Config::NO_HISTORY);

      SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
      EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
    }

    // turn on
    { config::ConfigHandler::GetDefaultConfig(config_.get()); }

    // reproducesd
    SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);

    segments.Clear();
    SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);

    // Exact Match
    segments.Clear();
    SetUpInputForSuggestion("わたしのなまえはなかのです", composer_.get(),
                            &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);

    segments.Clear();
    SetUpInputForSuggestion("わたしのなまえはなかのです", composer_.get(),
                            &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);

    segments.Clear();
    SetUpInputForSuggestion("こんにちはさようなら", composer_.get(), &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

    segments.Clear();
    SetUpInputForSuggestion("こんにちはさようなら", composer_.get(), &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

    // Read only mode should show suggestion.
    {
      config_->set_history_learning_level(config::Config::READ_ONLY);
      SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
      EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
      EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);

      segments.Clear();
      SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
      EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
      EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);
      config_->set_history_learning_level(config::Config::DEFAULT_HISTORY);
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

    // reproducesd
    SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

    SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
  }

  // nothing happen
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);
    Segments segments;

    // reproducesd
    SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

    SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
  }
}

// We did not support such Segments which has multiple segments and
// has type != CONVERSION.
// To support such Segments, this test case is created separately.
TEST_F(UserHistoryPredictorTest, UserHistoryPredictorTest_suggestion) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Register input histories via Finish method.
  {
    Segments segments;
    SetUpInputForSuggestion("かまた", composer_.get(), &segments);
    AddCandidate(0, "火魔汰", &segments);
    AddSegmentForSuggestion("ま", &segments);
    AddCandidate(1, "摩", &segments);
    predictor->Finish(*convreq_, &segments);

    // All added items must be suggestion entries.
    const UserHistoryPredictor::DicCache::Element *element;
    for (element = predictor->dic_->Head(); element->next;
         element = element->next) {
      const user_history_predictor::UserHistory::Entry &entry = element->value;
      EXPECT_TRUE(entry.has_suggestion_freq() && entry.suggestion_freq() == 1);
      EXPECT_TRUE(!entry.has_conversion_freq() && entry.conversion_freq() == 0);
    }
  }

  // Obtain input histories via Predict method.
  {
    Segments segments;
    SetUpInputForSuggestion("かま", composer_.get(), &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    std::set<std::string> expected_candidates;
    expected_candidates.insert("火魔汰");
    // We can get this entry even if Segmtnts's type is not CONVERSION.
    expected_candidates.insert("火魔汰摩");
    for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
      SCOPED_TRACE(segments.segment(0).candidate(i).value);
      EXPECT_EQ(
          1, expected_candidates.erase(segments.segment(0).candidate(i).value));
    }
  }
}

TEST_F(UserHistoryPredictorTest, DescriptionTest) {
#ifdef DEBUG
  const char kDescription[] = "テスト History";
#else
  const char kDescription[] = "テスト";
#endif  // DEBUG

  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);

    // Insert two items
    {
      Segments segments;
      SetUpInputForConversion("わたしのなまえはなかのです", composer_.get(),
                              &segments);
      AddCandidateWithDescription("私の名前は中野です", kDescription,
                                  &segments);
      predictor->Finish(*convreq_, &segments);

      SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
      EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
      EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);
      EXPECT_EQ(kDescription, segments.segment(0).candidate(0).description);

      segments.Clear();
      SetUpInputForPrediction("わたしの", composer_.get(), &segments);
      EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
      EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);
      EXPECT_EQ(kDescription, segments.segment(0).candidate(0).description);
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
      config_->set_use_history_suggest(false);
      WaitForSyncer(predictor);

      SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
      EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

      config_->set_use_history_suggest(true);
      config_->set_incognito_mode(true);

      SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
      EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
    }

    // turn on
    {
      config::ConfigHandler::GetDefaultConfig(config_.get());
      WaitForSyncer(predictor);
    }

    // reproducesd
    SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);
    EXPECT_EQ(kDescription, segments.segment(0).candidate(0).description);

    segments.Clear();
    SetUpInputForPrediction("わたしの", composer_.get(), &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);
    EXPECT_EQ(kDescription, segments.segment(0).candidate(0).description);

    // Exact Match
    segments.Clear();
    SetUpInputForSuggestion("わたしのなまえはなかのです", composer_.get(),
                            &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);
    EXPECT_EQ(kDescription, segments.segment(0).candidate(0).description);

    segments.Clear();
    SetUpInputForSuggestion("わたしのなまえはなかのです", composer_.get(),
                            &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);
    EXPECT_EQ(kDescription, segments.segment(0).candidate(0).description);

    // clear
    predictor->ClearAllHistory();
    WaitForSyncer(predictor);
  }

  // nothing happen
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);
    Segments segments;

    // reproducesd
    SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

    SetUpInputForPrediction("わたしの", composer_.get(), &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
  }

  // nothing happen
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);
    Segments segments;

    // reproducesd
    SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

    SetUpInputForPrediction("わたしの", composer_.get(), &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorUnusedHistoryTest) {
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);

    Segments segments;
    SetUpInputForConversion("わたしのなまえはなかのです", composer_.get(),
                            &segments);
    AddCandidate("私の名前は中野です", &segments);

    // once
    segments.set_request_type(Segments::SUGGESTION);
    predictor->Finish(*convreq_, &segments);

    segments.Clear();
    SetUpInputForConversion("ひろすえりょうこ", composer_.get(), &segments);
    AddCandidate("広末涼子", &segments);

    segments.set_request_type(Segments::CONVERSION);

    // conversion
    predictor->Finish(*convreq_, &segments);

    // sync
    predictor->Sync();
  }

  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);
    Segments segments;

    SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
    EXPECT_TRUE(predictor->Predict(&segments));
    EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);

    segments.Clear();
    SetUpInputForSuggestion("ひろすえ", composer_.get(), &segments);
    EXPECT_TRUE(predictor->Predict(&segments));
    EXPECT_EQ("広末涼子", segments.segment(0).candidate(0).value);

    predictor->ClearUnusedHistory();
    WaitForSyncer(predictor);

    segments.Clear();
    SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
    EXPECT_TRUE(predictor->Predict(&segments));
    EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);

    segments.Clear();
    SetUpInputForSuggestion("ひろすえ", composer_.get(), &segments);
    EXPECT_FALSE(predictor->Predict(&segments));

    predictor->Sync();
  }

  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);
    Segments segments;

    SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
    EXPECT_TRUE(predictor->Predict(&segments));
    EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);

    segments.Clear();
    SetUpInputForSuggestion("ひろすえ", composer_.get(), &segments);
    EXPECT_FALSE(predictor->Predict(&segments));
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorRevertTest) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments, segments2;
  SetUpInputForConversion("わたしのなまえはなかのです", composer_.get(),
                          &segments);
  AddCandidate("私の名前は中野です", &segments);

  predictor->Finish(*convreq_, &segments);

  // Before Revert, Suggest works
  SetUpInputForSuggestion("わたしの", composer_.get(), &segments2);
  EXPECT_TRUE(predictor->Predict(&segments2));
  EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);

  // Call revert here
  predictor->Revert(&segments);

  segments.Clear();
  SetUpInputForSuggestion("わたしの", composer_.get(), &segments);

  EXPECT_FALSE(predictor->Predict(&segments));
  EXPECT_EQ(0, segments.segment(0).candidates_size());

  EXPECT_FALSE(predictor->Predict(&segments));
  EXPECT_EQ(0, segments.segment(0).candidates_size());
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorClearTest) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  WaitForSyncer(predictor);

  // input "testtest" 10 times
  for (int i = 0; i < 10; ++i) {
    Segments segments;
    SetUpInputForConversion("testtest", composer_.get(), &segments);
    AddCandidate("テストテスト", &segments);
    predictor->Finish(*convreq_, &segments);
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  // input "testtest" 1 time
  for (int i = 0; i < 1; ++i) {
    Segments segments;
    SetUpInputForConversion("testtest", composer_.get(), &segments);
    AddCandidate("テストテスト", &segments);
    predictor->Finish(*convreq_, &segments);
  }

  // frequency is cleared as well.
  {
    Segments segments;
    SetUpInputForSuggestion("t", composer_.get(), &segments);
    EXPECT_FALSE(predictor->Predict(&segments));

    segments.Clear();
    SetUpInputForSuggestion("testte", composer_.get(), &segments);
    EXPECT_TRUE(predictor->Predict(&segments));
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorTrailingPunctuation) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;

  SetUpInputForConversion("わたしのなまえはなかのです", composer_.get(),
                          &segments);

  AddCandidate(0, "私の名前は中野です", &segments);

  AddSegmentForConversion("。", &segments);
  AddCandidate(1, "。", &segments);

  predictor->Finish(*convreq_, &segments);

  segments.Clear();
  SetUpInputForPrediction("わたしの", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_EQ(2, segments.segment(0).candidates_size());
  EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);
  EXPECT_EQ("私の名前は中野です。", segments.segment(0).candidate(1).value);

  segments.Clear();
  SetUpInputForSuggestion("わたしの", composer_.get(), &segments);

  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_EQ(2, segments.segment(0).candidates_size());
  EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);
  EXPECT_EQ("私の名前は中野です。", segments.segment(0).candidate(1).value);
}

TEST_F(UserHistoryPredictorTest, TrailingPunctuation_Mobile) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();
  commands::RequestForUnitTest::FillMobileRequest(request_.get());
  Segments segments;

  SetUpInputForConversion("です。", composer_.get(), &segments);

  AddCandidate(0, "です。", &segments);

  predictor->Finish(*convreq_, &segments);

  segments.Clear();

  SetUpInputForPrediction("です", composer_.get(), &segments);
  EXPECT_FALSE(predictor->Predict(&segments));
}

TEST_F(UserHistoryPredictorTest, HistoryToPunctuation) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;

  // Scenario 1: A user have committed "亜" by prediction and then commit "。".
  // Then, the unigram "亜" is learned but the bigram "亜。" shouldn't.
  SetUpInputForPrediction("あ", composer_.get(), &segments);
  AddCandidate(0, "亜", &segments);
  predictor->Finish(*convreq_, &segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  AddSegmentForPrediction("。", &segments);
  AddCandidate(1, "。", &segments);
  predictor->Finish(*convreq_, &segments);

  segments.Clear();
  SetUpInputForPrediction("あ", composer_.get(), &segments);  // "あ"
  ASSERT_TRUE(predictor->Predict(&segments)) << segments.DebugString();
  EXPECT_EQ("亜", segments.segment(0).candidate(0).value);

  segments.Clear();

  // Scenario 2: the opposite case to Scenario 1, i.e., "。亜".  Nothing is
  // suggested from symbol "。".
  SetUpInputForPrediction("。", composer_.get(), &segments);
  AddCandidate(0, "。", &segments);
  predictor->Finish(*convreq_, &segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  AddSegmentForPrediction("あ", &segments);
  AddCandidate(1, "亜", &segments);
  predictor->Finish(*convreq_, &segments);

  segments.Clear();
  SetUpInputForPrediction("。", composer_.get(), &segments);  // "。"
  EXPECT_FALSE(predictor->Predict(&segments)) << segments.DebugString();

  segments.Clear();

  // Scenario 3: If the history segment looks like a sentence and committed
  // value is a punctuation, the concatenated entry is also learned.
  SetUpInputForPrediction("おつかれさまです", composer_.get(), &segments);
  AddCandidate(0, "お疲れ様です", &segments);
  predictor->Finish(*convreq_, &segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  AddSegmentForPrediction("。", &segments);
  AddCandidate(1, "。", &segments);
  predictor->Finish(*convreq_, &segments);

  segments.Clear();
  SetUpInputForPrediction("おつかれ", composer_.get(), &segments);
  ASSERT_TRUE(predictor->Predict(&segments)) << segments.DebugString();
  EXPECT_EQ("お疲れ様です", segments.segment(0).candidate(0).value);
  EXPECT_EQ("お疲れ様です。", segments.segment(0).candidate(1).value);
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorPrecedingPunctuation) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;

  SetUpInputForConversion("。", composer_.get(), &segments);
  AddCandidate(0, "。", &segments);

  AddSegmentForConversion("わたしのなまえはなかのです", &segments);

  AddCandidate(1, "私の名前は中野です", &segments);

  predictor->Finish(*convreq_, &segments);

  segments.Clear();
  SetUpInputForPrediction("わたしの", composer_.get(), &segments);

  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_EQ(1, segments.segment(0).candidates_size());
  EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);

  segments.Clear();
  SetUpInputForSuggestion("わたしの", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_EQ(1, segments.segment(0).candidates_size());
  EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);
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
      {"。", false}, {"、", false}, {"？", false}, {"！", false}, {"ぬ", true},
  };

  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    WaitForSyncer(predictor);
    predictor->ClearAllHistory();
    WaitForSyncer(predictor);

    Segments segments;
    const std::string first_char = kTestCases[i].first_character;
    {
      // Learn from two segments
      SetUpInputForConversion(first_char, composer_.get(), &segments);
      AddCandidate(0, first_char, &segments);
      AddSegmentForConversion("てすとぶんしょう", &segments);
      AddCandidate(1, "テスト文章", &segments);
      predictor->Finish(*convreq_, &segments);
    }
    segments.Clear();
    {
      // Learn from one segment
      SetUpInputForConversion(first_char + "てすとぶんしょう", composer_.get(),
                              &segments);
      AddCandidate(0, first_char + "テスト文章", &segments);
      predictor->Finish(*convreq_, &segments);
    }
    segments.Clear();
    {
      // Suggestion
      SetUpInputForSuggestion(first_char, composer_.get(), &segments);
      AddCandidate(0, first_char, &segments);
      EXPECT_EQ(kTestCases[i].expected_result, predictor->Predict(&segments))
          << "Suggest from " << first_char;
    }
    segments.Clear();
    {
      // Prediciton
      SetUpInputForPrediction(first_char, composer_.get(), &segments);
      EXPECT_EQ(kTestCases[i].expected_result, predictor->Predict(&segments))
          << "Predict from " << first_char;
    }
  }
}

TEST_F(UserHistoryPredictorTest, ZeroQuerySuggestionTest) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  request_->set_zero_query_suggestion(true);

  commands::Request non_zero_query_request;
  non_zero_query_request.set_zero_query_suggestion(false);
  ConversionRequest non_zero_query_conversion_request(
      composer_.get(), &non_zero_query_request, config_.get());

  Segments segments;

  // No history segments
  segments.Clear();
  SetUpInputForSuggestion("", composer_.get(), &segments);
  EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

  {
    segments.Clear();

    SetUpInputForConversion("たろうは", composer_.get(), &segments);
    AddCandidate(0, "太郎は", &segments);
    predictor->Finish(*convreq_, &segments);

    SetUpInputForConversionWithHistory("はなこに", "たろうは", "太郎は",
                                       composer_.get(), &segments);
    AddCandidate(1, "花子に", &segments);
    predictor->Finish(*convreq_, &segments);

    SetUpInputForConversionWithHistory("きょうと", "たろうは", "太郎は",
                                       composer_.get(), &segments);
    AddCandidate(1, "京都", &segments);
    Util::Sleep(2000);
    predictor->Finish(*convreq_, &segments);

    SetUpInputForConversionWithHistory("おおさか", "たろうは", "太郎は",
                                       composer_.get(), &segments);
    AddCandidate(1, "大阪", &segments);
    Util::Sleep(2000);
    predictor->Finish(*convreq_, &segments);

    // Zero query suggestion is disabled.
    SetUpInputForSuggestionWithHistory("", "たろうは", "太郎は",
                                       composer_.get(), &segments);
    EXPECT_FALSE(predictor->PredictForRequest(non_zero_query_conversion_request,
                                              &segments));

    SetUpInputForSuggestionWithHistory("", "たろうは", "太郎は",
                                       composer_.get(), &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    ASSERT_EQ(2, segments.segments_size());
    // last-pushed segment is "大阪"
    EXPECT_EQ("大阪", segments.segment(1).candidate(0).value);
    EXPECT_EQ("おおさか", segments.segment(1).candidate(0).key);
    EXPECT_TRUE(segments.segment(1).candidate(0).source_info &
                Segment::Candidate::USER_HISTORY_PREDICTOR);

    for (const char *key : {"は", "た", "き", "お"}) {
      SetUpInputForSuggestionWithHistory(key, "たろうは", "太郎は",
                                         composer_.get(), &segments);
      EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    }
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    segments.Clear();
    SetUpInputForConversion("たろうは", composer_.get(), &segments);
    AddCandidate(0, "太郎は", &segments);

    AddSegmentForConversion("はなこに", &segments);
    AddCandidate(1, "花子に", &segments);
    predictor->Finish(*convreq_, &segments);

    segments.Clear();
    SetUpInputForConversion("たろうは", composer_.get(), &segments);
    AddCandidate(0, "太郎は", &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    // Zero query suggestion is disabled.
    AddSegmentForSuggestion("", &segments);  // empty request
    EXPECT_FALSE(predictor->PredictForRequest(non_zero_query_conversion_request,
                                              &segments));

    segments.pop_back_segment();
    AddSegmentForSuggestion("", &segments);  // empty request
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));

    segments.pop_back_segment();
    AddSegmentForSuggestion("は", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));

    segments.pop_back_segment();
    AddSegmentForSuggestion("た", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  }
}

TEST_F(UserHistoryPredictorTest, MultiSegmentsMultiInput) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;

  SetUpInputForConversion("たろうは", composer_.get(), &segments);
  AddCandidate(0, "太郎は", &segments);
  predictor->Finish(*convreq_, &segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  AddSegmentForConversion("はなこに", &segments);
  AddCandidate(1, "花子に", &segments);
  predictor->Finish(*convreq_, &segments);
  segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);

  segments.clear_conversion_segments();
  AddSegmentForConversion("むずかしい", &segments);
  AddCandidate(2, "難しい", &segments);
  predictor->Finish(*convreq_, &segments);
  segments.mutable_segment(2)->set_segment_type(Segment::HISTORY);

  segments.clear_conversion_segments();
  AddSegmentForConversion("ほんを", &segments);
  AddCandidate(3, "本を", &segments);
  predictor->Finish(*convreq_, &segments);
  segments.mutable_segment(3)->set_segment_type(Segment::HISTORY);

  segments.clear_conversion_segments();
  AddSegmentForConversion("よませた", &segments);
  AddCandidate(4, "読ませた", &segments);
  predictor->Finish(*convreq_, &segments);

  segments.Clear();
  SetUpInputForSuggestion("た", composer_.get(), &segments);
  EXPECT_FALSE(predictor->Predict(&segments));

  segments.Clear();
  SetUpInputForSuggestion("たろうは", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  segments.Clear();
  SetUpInputForSuggestion("ろうは", composer_.get(), &segments);
  EXPECT_FALSE(predictor->Predict(&segments));

  segments.Clear();
  SetUpInputForSuggestion("たろうははな", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  segments.Clear();
  SetUpInputForSuggestion("はなこにむ", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  segments.Clear();
  SetUpInputForSuggestion("むずかし", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  segments.Clear();
  SetUpInputForSuggestion("はなこにむずかしいほ", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  segments.Clear();
  SetUpInputForSuggestion("ほんをよま", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  Util::Sleep(1000);

  // Add new entry "たろうはよしこに/太郎は良子に"
  segments.Clear();
  SetUpInputForConversion("たろうは", composer_.get(), &segments);
  AddCandidate(0, "太郎は", &segments);
  predictor->Finish(*convreq_, &segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  AddSegmentForConversion("よしこに", &segments);
  AddCandidate(1, "良子に", &segments);
  predictor->Finish(*convreq_, &segments);
  segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);

  segments.Clear();
  SetUpInputForSuggestion("たろうは", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_EQ("太郎は良子に", segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
}

TEST_F(UserHistoryPredictorTest, MultiSegmentsSingleInput) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;

  SetUpInputForConversion("たろうは", composer_.get(), &segments);
  AddCandidate(0, "太郎は", &segments);

  AddSegmentForConversion("はなこに", &segments);
  AddCandidate(1, "花子に", &segments);

  AddSegmentForConversion("むずかしい", &segments);
  AddCandidate(2, "難しい", &segments);

  AddSegmentForConversion("ほんを", &segments);
  AddCandidate(3, "本を", &segments);

  AddSegmentForConversion("よませた", &segments);
  AddCandidate(4, "読ませた", &segments);

  predictor->Finish(*convreq_, &segments);

  segments.Clear();
  SetUpInputForSuggestion("たろうは", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  segments.Clear();
  SetUpInputForSuggestion("た", composer_.get(), &segments);
  EXPECT_FALSE(predictor->Predict(&segments));

  segments.Clear();
  SetUpInputForSuggestion("たろうははな", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  segments.Clear();
  SetUpInputForSuggestion("ろうははな", composer_.get(), &segments);
  EXPECT_FALSE(predictor->Predict(&segments));

  segments.Clear();
  SetUpInputForSuggestion("はなこにむ", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  segments.Clear();
  SetUpInputForSuggestion("むずかし", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  segments.Clear();
  SetUpInputForSuggestion("はなこにむずかしいほ", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  segments.Clear();
  SetUpInputForSuggestion("ほんをよま", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  Util::Sleep(1000);

  // Add new entry "たろうはよしこに/太郎は良子に"
  segments.Clear();
  SetUpInputForConversion("たろうは", composer_.get(), &segments);
  AddCandidate(0, "太郎は", &segments);
  predictor->Finish(*convreq_, &segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  AddSegmentForConversion("よしこに", &segments);
  AddCandidate(1, "良子に", &segments);
  predictor->Finish(*convreq_, &segments);
  segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);

  segments.Clear();
  SetUpInputForSuggestion("たろうは", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_EQ("太郎は良子に", segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
}

TEST_F(UserHistoryPredictorTest, Regression2843371_Case1) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;

  SetUpInputForConversion("とうきょうは", composer_.get(), &segments);
  AddCandidate(0, "東京は", &segments);

  AddSegmentForConversion("、", &segments);
  AddCandidate(1, "、", &segments);

  AddSegmentForConversion("にほんです", &segments);
  AddCandidate(2, "日本です", &segments);

  AddSegmentForConversion("。", &segments);
  AddCandidate(3, "。", &segments);

  predictor->Finish(*convreq_, &segments);

  segments.Clear();

  Util::Sleep(1000);

  SetUpInputForConversion("らーめんは", composer_.get(), &segments);
  AddCandidate(0, "ラーメンは", &segments);

  AddSegmentForConversion("、", &segments);
  AddCandidate(1, "、", &segments);

  AddSegmentForConversion("めんるいです", &segments);
  AddCandidate(2, "麺類です", &segments);

  AddSegmentForConversion("。", &segments);
  AddCandidate(3, "。", &segments);

  predictor->Finish(*convreq_, &segments);

  segments.Clear();

  SetUpInputForSuggestion("とうきょうは、", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  EXPECT_EQ("東京は、日本です", segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
}

TEST_F(UserHistoryPredictorTest, Regression2843371_Case2) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;

  SetUpInputForConversion("えど", composer_.get(), &segments);
  AddCandidate(0, "江戸", &segments);

  AddSegmentForConversion("(", &segments);
  AddCandidate(1, "(", &segments);

  AddSegmentForConversion("とうきょう", &segments);
  AddCandidate(2, "東京", &segments);

  AddSegmentForConversion(")", &segments);
  AddCandidate(3, ")", &segments);

  AddSegmentForConversion("は", &segments);
  AddCandidate(4, "は", &segments);

  AddSegmentForConversion("えぞ", &segments);
  AddCandidate(5, "蝦夷", &segments);

  AddSegmentForConversion("(", &segments);
  AddCandidate(6, "(", &segments);

  AddSegmentForConversion("ほっかいどう", &segments);
  AddCandidate(7, "北海道", &segments);

  AddSegmentForConversion(")", &segments);
  AddCandidate(8, ")", &segments);

  AddSegmentForConversion("ではない", &segments);
  AddCandidate(9, "ではない", &segments);

  AddSegmentForConversion("。", &segments);
  AddCandidate(10, "。", &segments);

  predictor->Finish(*convreq_, &segments);

  segments.Clear();

  SetUpInputForSuggestion("えど(", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_EQ("江戸(東京", segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);

  EXPECT_TRUE(predictor->Predict(&segments));

  EXPECT_EQ("江戸(東京", segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
}

TEST_F(UserHistoryPredictorTest, Regression2843371_Case3) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;

  SetUpInputForConversion("「", composer_.get(), &segments);
  AddCandidate(0, "「", &segments);

  AddSegmentForConversion("やま", &segments);
  AddCandidate(1, "山", &segments);

  AddSegmentForConversion("」", &segments);
  AddCandidate(2, "」", &segments);

  AddSegmentForConversion("は", &segments);
  AddCandidate(3, "は", &segments);

  AddSegmentForConversion("たかい", &segments);
  AddCandidate(4, "高い", &segments);

  AddSegmentForConversion("。", &segments);
  AddCandidate(5, "。", &segments);

  predictor->Finish(*convreq_, &segments);

  Util::Sleep(2000);

  segments.Clear();

  SetUpInputForConversion("「", composer_.get(), &segments);
  AddCandidate(0, "「", &segments);

  AddSegmentForConversion("うみ", &segments);
  AddCandidate(1, "海", &segments);

  AddSegmentForConversion("」", &segments);
  AddCandidate(2, "」", &segments);

  AddSegmentForConversion("は", &segments);
  AddCandidate(3, "は", &segments);

  AddSegmentForConversion("ふかい", &segments);
  AddCandidate(4, "深い", &segments);

  AddSegmentForConversion("。", &segments);
  AddCandidate(5, "。", &segments);

  predictor->Finish(*convreq_, &segments);

  segments.Clear();

  SetUpInputForSuggestion("「やま」は", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  EXPECT_EQ("「山」は高い", segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
}

TEST_F(UserHistoryPredictorTest, Regression2843775) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;

  SetUpInputForConversion("そうです", composer_.get(), &segments);
  AddCandidate(0, "そうです", &segments);

  AddSegmentForConversion("。よろしくおねがいします", &segments);
  AddCandidate(1, "。よろしくお願いします", &segments);

  predictor->Finish(*convreq_, &segments);

  segments.Clear();

  SetUpInputForSuggestion("そうです", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  EXPECT_EQ("そうです。よろしくお願いします",
            segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
}

TEST_F(UserHistoryPredictorTest, DuplicateString) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;

  SetUpInputForConversion("らいおん", composer_.get(), &segments);
  AddCandidate(0, "ライオン", &segments);

  AddSegmentForConversion("（", &segments);
  AddCandidate(1, "（", &segments);

  AddSegmentForConversion("もうじゅう", &segments);
  AddCandidate(2, "猛獣", &segments);

  AddSegmentForConversion("）と", &segments);
  AddCandidate(3, "）と", &segments);

  AddSegmentForConversion("ぞうりむし", &segments);
  AddCandidate(4, "ゾウリムシ", &segments);

  AddSegmentForConversion("（", &segments);
  AddCandidate(5, "（", &segments);

  AddSegmentForConversion("びせいぶつ", &segments);
  AddCandidate(6, "微生物", &segments);

  AddSegmentForConversion("）", &segments);
  AddCandidate(7, "）", &segments);

  predictor->Finish(*convreq_, &segments);

  segments.Clear();

  SetUpInputForSuggestion("ぞうりむし", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  for (int i = 0; i < segments.segment(0).candidates_size(); ++i) {
    EXPECT_EQ(std::string::npos,
              segments.segment(0).candidate(i).value.find(
                  "猛獣"));  // "猛獣" should not be found
  }

  segments.Clear();

  SetUpInputForSuggestion("らいおん", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  for (int i = 0; i < segments.segment(0).candidates_size(); ++i) {
    EXPECT_EQ(std::string::npos,
              segments.segment(0).candidate(i).value.find("ライオン（微生物"));
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

  std::vector<Command> commands(10000);
  for (size_t i = 0; i < commands.size(); ++i) {
    commands[i].key = std::to_string(static_cast<uint32>(i)) + "key";
    commands[i].value = std::to_string(static_cast<uint32>(i)) + "value";
    const int n = Util::Random(100);
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
      case Command::INSERT:
        segments.Clear();
        SetUpInputForConversion(commands[i].key, composer_.get(), &segments);
        AddCandidate(commands[i].value, &segments);
        predictor->Finish(*convreq_, &segments);
        break;
      case Command::LOOKUP:
        segments.Clear();
        SetUpInputForSuggestion(commands[i].key, composer_.get(), &segments);
        predictor->Predict(&segments);
        break;
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

  const uint32 entry_fp1 = UserHistoryPredictor::Fingerprint(kKey, kValue);
  const uint32 entry_fp2 = UserHistoryPredictor::EntryFingerprint(entry);

  const uint32 entry_fp3 = UserHistoryPredictor::Fingerprint(
      kKey, kValue, UserHistoryPredictor::Entry::DEFAULT_ENTRY);

  const uint32 entry_fp4 = UserHistoryPredictor::Fingerprint(
      kKey, kValue, UserHistoryPredictor::Entry::CLEAN_ALL_EVENT);

  const uint32 entry_fp5 = UserHistoryPredictor::Fingerprint(
      kKey, kValue, UserHistoryPredictor::Entry::CLEAN_UNUSED_EVENT);

  Segment segment;
  segment.set_key(kKey);
  Segment::Candidate *c = segment.add_candidate();
  c->key = kKey;
  c->content_key = kKey;
  c->value = kValue;
  c->content_value = kValue;

  const uint32 segment_fp = UserHistoryPredictor::SegmentFingerprint(segment);

  Segment segment2;
  segment2.set_key("ab");
  Segment::Candidate *c2 = segment2.add_candidate();
  c2->key = kKey;
  c2->content_key = kKey;
  c2->value = kValue;
  c2->content_value = kValue;

  const uint32 segment_fp2 = UserHistoryPredictor::SegmentFingerprint(segment2);

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
  EXPECT_EQ(123, UserHistoryPredictor::StringToUint32(
                     UserHistoryPredictor::Uint32ToString(123)));

  EXPECT_EQ(12141, UserHistoryPredictor::StringToUint32(
                       UserHistoryPredictor::Uint32ToString(12141)));

  for (uint32 i = 0; i < 10000; ++i) {
    EXPECT_EQ(i, UserHistoryPredictor::StringToUint32(
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
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();

  UserHistoryPredictor::Entry entry;

  EXPECT_TRUE(predictor->IsValidEntry(entry, Request::UNICODE_EMOJI));

  entry.set_key("key");
  entry.set_value("value");

  EXPECT_TRUE(predictor->IsValidEntry(entry, Request::UNICODE_EMOJI));
  EXPECT_TRUE(predictor->IsValidEntryIgnoringRemovedField(
      entry, Request::UNICODE_EMOJI));

  entry.set_removed(true);
  EXPECT_FALSE(predictor->IsValidEntry(entry, Request::UNICODE_EMOJI));
  EXPECT_TRUE(predictor->IsValidEntryIgnoringRemovedField(
      entry, Request::UNICODE_EMOJI));

  entry.set_removed(false);
  EXPECT_TRUE(predictor->IsValidEntry(entry, Request::UNICODE_EMOJI));
  EXPECT_TRUE(predictor->IsValidEntryIgnoringRemovedField(
      entry, Request::UNICODE_EMOJI));

  entry.set_entry_type(UserHistoryPredictor::Entry::CLEAN_ALL_EVENT);
  EXPECT_FALSE(predictor->IsValidEntry(entry, Request::UNICODE_EMOJI));
  EXPECT_FALSE(predictor->IsValidEntryIgnoringRemovedField(
      entry, Request::UNICODE_EMOJI));

  entry.set_entry_type(UserHistoryPredictor::Entry::CLEAN_UNUSED_EVENT);
  EXPECT_FALSE(predictor->IsValidEntry(entry, Request::UNICODE_EMOJI));
  EXPECT_FALSE(predictor->IsValidEntryIgnoringRemovedField(
      entry, Request::UNICODE_EMOJI));

  entry.set_removed(true);
  EXPECT_FALSE(predictor->IsValidEntry(entry, Request::UNICODE_EMOJI));
  EXPECT_FALSE(predictor->IsValidEntryIgnoringRemovedField(
      entry, Request::UNICODE_EMOJI));

  entry.Clear();
  EXPECT_TRUE(predictor->IsValidEntry(entry, Request::UNICODE_EMOJI));
  EXPECT_TRUE(predictor->IsValidEntryIgnoringRemovedField(
      entry, Request::UNICODE_EMOJI));

  entry.Clear();
  entry.set_key("key");
  entry.set_value("value");
  entry.set_description("絵文字");
  EXPECT_TRUE(predictor->IsValidEntry(entry, Request::UNICODE_EMOJI));
  EXPECT_TRUE(predictor->IsValidEntryIgnoringRemovedField(
      entry, Request::UNICODE_EMOJI));
  EXPECT_FALSE(predictor->IsValidEntry(entry, 0));
  EXPECT_FALSE(predictor->IsValidEntryIgnoringRemovedField(entry, 0));

  // An android pua emoji example. (Note: 0xFE000 is in the region).
  Util::UCS4ToUTF8(0xFE000, entry.mutable_value());
  EXPECT_FALSE(predictor->IsValidEntry(entry, Request::UNICODE_EMOJI));
  EXPECT_FALSE(predictor->IsValidEntry(entry, 0));
  EXPECT_TRUE(predictor->IsValidEntry(entry, Request::DOCOMO_EMOJI));
  EXPECT_TRUE(predictor->IsValidEntry(entry, Request::SOFTBANK_EMOJI));
  EXPECT_TRUE(predictor->IsValidEntry(entry, Request::KDDI_EMOJI));

  EXPECT_FALSE(predictor->IsValidEntryIgnoringRemovedField(
      entry, Request::UNICODE_EMOJI));
  EXPECT_FALSE(predictor->IsValidEntryIgnoringRemovedField(entry, 0));
  EXPECT_TRUE(predictor->IsValidEntryIgnoringRemovedField(
      entry, Request::DOCOMO_EMOJI));
  EXPECT_TRUE(predictor->IsValidEntryIgnoringRemovedField(
      entry, Request::SOFTBANK_EMOJI));
  EXPECT_TRUE(
      predictor->IsValidEntryIgnoringRemovedField(entry, Request::KDDI_EMOJI));

  // Invalid UTF8.
  entry.Clear();
  entry.set_key("key");
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
    entry.set_value(value);
    EXPECT_FALSE(predictor->IsValidEntry(entry, 0)) << entry.Utf8DebugString();
    EXPECT_FALSE(predictor->IsValidEntryIgnoringRemovedField(entry, 0))
        << entry.Utf8DebugString();
  }

  SuppressionDictionary *d = GetSuppressionDictionary();
  DCHECK(d);
  d->Lock();
  d->AddEntry("foo", "bar");
  d->UnLock();

  entry.set_key("key");
  entry.set_value("value");
  EXPECT_TRUE(predictor->IsValidEntry(entry, Request::UNICODE_EMOJI));
  EXPECT_TRUE(predictor->IsValidEntryIgnoringRemovedField(
      entry, Request::UNICODE_EMOJI));

  entry.set_key("foo");
  entry.set_value("bar");
  EXPECT_FALSE(predictor->IsValidEntry(entry, Request::UNICODE_EMOJI));
  EXPECT_FALSE(predictor->IsValidEntryIgnoringRemovedField(
      entry, Request::UNICODE_EMOJI));

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

TEST_F(UserHistoryPredictorTest, EntryPriorityQueueTest) {
  // removed automatically
  const int kSize = 10000;
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
      EXPECT_EQ(expected[n], entry);
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

namespace {

std::string RemoveLastUCS4Character(const std::string &input) {
  const size_t ucs4_count = Util::CharsLen(input);
  if (ucs4_count == 0) {
    return "";
  }

  size_t ucs4_processed = 0;
  std::string output;
  for (ConstChar32Iterator iter(input);
       !iter.Done() && (ucs4_processed < ucs4_count - 1);
       iter.Next(), ++ucs4_processed) {
    Util::UCS4ToUTF8Append(iter.Get(), &output);
  }
  return output;
}

struct PrivacySensitiveTestData {
  bool is_sensitive;
  const char *scenario_description;
  const char *input;
  const char *output;
};

const bool kSensitive = true;
const bool kNonSensitive = false;

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
     "Type an common English word which might be included in our system "
     "dictionary with number postfix.",
     "Orange10000", "Orange10000"},
};

}  // namespace

TEST_F(UserHistoryPredictorTest, PrivacySensitiveTest) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();

  // Add those words to the mock dictionary that are assumed to exist in privacy
  // sensitive filtering.
  const char *kEnglishWords[] = {
      "variable",
      "UPPER",
  };
  for (size_t i = 0; i < arraysize(kEnglishWords); ++i) {
    // LookupPredictive is used in UserHistoryPredictor::IsPrivacySensitive().
    GetDictionaryMock()->AddLookupExact(kEnglishWords[i], kEnglishWords[i],
                                        kEnglishWords[i], Token::NONE);
  }

  for (size_t i = 0; i < arraysize(kNonSensitiveCases); ++i) {
    predictor->ClearAllHistory();
    WaitForSyncer(predictor);

    const PrivacySensitiveTestData &data = kNonSensitiveCases[i];
    const std::string description(data.scenario_description);
    const std::string input(data.input);
    const std::string output(data.output);
    const std::string &partial_input = RemoveLastUCS4Character(input);
    const bool expect_sensitive = data.is_sensitive;

    // Initial commit.
    {
      Segments segments;
      SetUpInputForConversion(input, composer_.get(), &segments);
      AddCandidate(0, output, &segments);
      predictor->Finish(*convreq_, &segments);
    }

    // TODO(yukawa): Refactor the scenario runner below by making
    //     some utility functions.

    // Check suggestion
    {
      Segments segments;
      SetUpInputForSuggestion(partial_input, composer_.get(), &segments);
      if (expect_sensitive) {
        EXPECT_FALSE(predictor->Predict(&segments))
            << description << " input: " << input << " output: " << output;
      } else {
        EXPECT_TRUE(predictor->Predict(&segments))
            << description << " input: " << input << " output: " << output;
      }
      segments.Clear();
      SetUpInputForPrediction(input, composer_.get(), &segments);
      if (expect_sensitive) {
        EXPECT_FALSE(predictor->Predict(&segments))
            << description << " input: " << input << " output: " << output;
      } else {
        EXPECT_TRUE(predictor->Predict(&segments))
            << description << " input: " << input << " output: " << output;
      }
    }

    // Check Prediction
    {
      Segments segments;
      SetUpInputForPrediction(partial_input, composer_.get(), &segments);
      if (expect_sensitive) {
        EXPECT_FALSE(predictor->Predict(&segments))
            << description << " input: " << input << " output: " << output;
      } else {
        EXPECT_TRUE(predictor->Predict(&segments))
            << description << " input: " << input << " output: " << output;
      }
      segments.Clear();
      SetUpInputForPrediction(input, composer_.get(), &segments);
      if (expect_sensitive) {
        EXPECT_FALSE(predictor->Predict(&segments))
            << description << " input: " << input << " output: " << output;
      } else {
        EXPECT_TRUE(predictor->Predict(&segments))
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
    SetUpInputForConversion("123", composer_.get(), &segments);
    AddSegmentForConversion("abc!", &segments);
    AddCandidate(0, "123", &segments);
    AddCandidate(1, "abc!", &segments);
    predictor->Finish(*convreq_, &segments);
  }

  {
    Segments segments;
    SetUpInputForSuggestion("123abc", composer_.get(), &segments);
    EXPECT_TRUE(predictor->Predict(&segments));
    segments.Clear();
    SetUpInputForSuggestion("123abc!", composer_.get(), &segments);
    EXPECT_TRUE(predictor->Predict(&segments));
  }

  {
    Segments segments;
    SetUpInputForPrediction("123abc", composer_.get(), &segments);
    EXPECT_TRUE(predictor->Predict(&segments));
    segments.Clear();
    SetUpInputForPrediction("123abc!", composer_.get(), &segments);
    EXPECT_TRUE(predictor->Predict(&segments));
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

  EXPECT_EQ(storage1.GetProto().DebugString(),
            storage2.GetProto().DebugString());
  FileUtil::Unlink(filename);
}

TEST_F(UserHistoryPredictorTest, UserHistoryStorageContainingOldEntries) {
  ScopedClockMock clock(1, 0);

  // Create a history proto containing old entries (timestamp = 1).
  user_history_predictor::UserHistory history;
  for (int i = 0; i < 10; ++i) {
    auto *entry = history.add_entries();
    entry->set_key(Util::StringPrintf("old_key%d", i));
    entry->set_value(Util::StringPrintf("old_value%d", i));
    entry->set_last_access_time(clock->GetTime());
  }
  clock->PutClockForward(63 * 24 * 60 * 60, 0);  // Advance clock for 63 days.
  for (int i = 0; i < 10; ++i) {
    auto *entry = history.add_entries();
    entry->set_key(Util::StringPrintf("new_key%d", i));
    entry->set_value(Util::StringPrintf("new_value%d", i));
    entry->set_last_access_time(clock->GetTime());
  }

  // Test Load().
  {
    const std::string filename =
        FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "testload");
    // Write directly to the file to keep old entries for testing.
    storage::EncryptedStringStorage file_storage(filename);
    ASSERT_TRUE(file_storage.Save(history.SerializeAsString()));

    UserHistoryStorage storage(filename);
    ASSERT_TRUE(storage.Load());
    // Only the new entries are loaded.
    EXPECT_EQ(10, storage.GetProto().entries_size());
    for (const auto &entry : storage.GetProto().entries()) {
      EXPECT_TRUE(Util::StartsWith(entry.key(), "new_"));
      EXPECT_TRUE(Util::StartsWith(entry.value(), "new_"));
    }
    FileUtil::Unlink(filename);
  }

  // Test Save().
  {
    const std::string filename =
        FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "testsave");
    UserHistoryStorage storage(filename);
    storage.GetProto() = history;
    ASSERT_TRUE(storage.Save());

    // Directly open the file to check the actual entries written.
    storage::EncryptedStringStorage file_storage(filename);
    std::string content;
    ASSERT_TRUE(file_storage.Load(&content));
    user_history_predictor::UserHistory modified_history;
    ASSERT_TRUE(modified_history.ParseFromString(content));
    EXPECT_EQ(10, modified_history.entries_size());
    for (const auto &entry : storage.GetProto().entries()) {
      EXPECT_TRUE(Util::StartsWith(entry.key(), "new_"));
      EXPECT_TRUE(Util::StartsWith(entry.value(), "new_"));
    }
    FileUtil::Unlink(filename);
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

  config_->set_preedit_method(config::Config::ROMAN);

  seg->set_key("");
  EXPECT_EQ("",
            UserHistoryPredictor::GetRomanMisspelledKey(*convreq_, segments));

  seg->set_key("おねがいしまうs");
  EXPECT_EQ("onegaisimaus",
            UserHistoryPredictor::GetRomanMisspelledKey(*convreq_, segments));

  seg->set_key("おねがいします");
  EXPECT_EQ("",
            UserHistoryPredictor::GetRomanMisspelledKey(*convreq_, segments));

  config_->set_preedit_method(config::Config::KANA);

  seg->set_key("おねがいしまうs");
  EXPECT_EQ("",
            UserHistoryPredictor::GetRomanMisspelledKey(*convreq_, segments));

  seg->set_key("おねがいします");
  EXPECT_EQ("",
            UserHistoryPredictor::GetRomanMisspelledKey(*convreq_, segments));
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
  const std::string entry_key;
  const bool expect_result;
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
  unique_ptr<Trie<std::string>> expanded(new Trie<std::string>);
  expanded->AddEntry("か", "");
  expanded->AddEntry("き", "");
  expanded->AddEntry("く", "");
  expanded->AddEntry("け", "");
  expanded->AddEntry("こ", "");

  const LookupTestData kTests1[] = {
      {"", false},       {"あか", true},    {"あき", true},  {"あかい", true},
      {"あまい", false}, {"あ", false},     {"さか", false}, {"さき", false},
      {"さかい", false}, {"さまい", false}, {"さ", false},
  };

  // with expanded
  for (size_t i = 0; i < arraysize(kTests1); ++i) {
    entry.set_key(kTests1[i].entry_key);
    EXPECT_EQ(
        kTests1[i].expect_result,
        predictor->LookupEntry(UserHistoryPredictor::DEFAULT, "あｋ", "あ",
                               expanded.get(), &entry, nullptr, &results))
        << kTests1[i].entry_key;
  }

  // only expanded
  // preedit: "k"
  // input_key: ""
  // key_base: ""
  // key_expanded: "か","き","く","け", "こ"

  const LookupTestData kTests2[] = {
      {"", false},    {"か", true},    {"き", true},
      {"かい", true}, {"まい", false}, {"も", false},
  };

  for (size_t i = 0; i < arraysize(kTests2); ++i) {
    entry.set_key(kTests2[i].entry_key);
    EXPECT_EQ(kTests2[i].expect_result,
              predictor->LookupEntry(UserHistoryPredictor::DEFAULT, "", "",
                                     expanded.get(), &entry, nullptr, &results))
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
  unique_ptr<Trie<std::string>> expanded(new Trie<std::string>);
  expanded->AddEntry("し", "");
  expanded->AddEntry("じ", "");

  const LookupTestData kTests1[] = {
      {"", false},           {"あ", false},         {"あし", true},
      {"あじ", true},        {"あしかゆい", true},  {"あじうまい", true},
      {"あまにがい", false}, {"あめ", false},       {"まし", false},
      {"まじ", false},       {"ましなあじ", false}, {"まじうまい", false},
      {"ままにがい", false}, {"まめ", false},
  };

  // with expanded
  for (size_t i = 0; i < arraysize(kTests1); ++i) {
    entry.set_key(kTests1[i].entry_key);
    EXPECT_EQ(
        kTests1[i].expect_result,
        predictor->LookupEntry(UserHistoryPredictor::DEFAULT, "あし", "あ",
                               expanded.get(), &entry, nullptr, &results))
        << kTests1[i].entry_key;
  }

  // only expanded
  // input_key: "し"
  // key_base: ""
  // key_expanded: "し","じ"
  const LookupTestData kTests2[] = {
      {"", false},          {"し", true},         {"じ", true},
      {"しかうまい", true}, {"じゅうかい", true}, {"ま", false},
      {"まめ", false},
  };

  for (size_t i = 0; i < arraysize(kTests2); ++i) {
    entry.set_key(kTests2[i].entry_key);
    EXPECT_EQ(kTests2[i].expect_result,
              predictor->LookupEntry(UserHistoryPredictor::DEFAULT, "し", "",
                                     expanded.get(), &entry, nullptr, &results))
        << kTests2[i].entry_key;
  }
}

TEST_F(UserHistoryPredictorTest, GetMatchTypeFromInputRoman) {
  // We have to define this here,
  // because UserHistoryPredictor::MatchType is private
  struct MatchTypeTestData {
    const std::string target;
    const UserHistoryPredictor::MatchType expect_type;
  };

  // Roman
  // preedit: "あｋ"
  // input_key: "あ"
  // key_base: "あ"
  // key_expanded: "か","き","く","け", "こ"
  unique_ptr<Trie<std::string>> expanded(new Trie<std::string>);
  expanded->AddEntry("か", "か");
  expanded->AddEntry("き", "き");
  expanded->AddEntry("く", "く");
  expanded->AddEntry("け", "け");
  expanded->AddEntry("こ", "こ");

  const MatchTypeTestData kTests1[] = {
      {"", UserHistoryPredictor::NO_MATCH},
      {"い", UserHistoryPredictor::NO_MATCH},
      {"あ", UserHistoryPredictor::RIGHT_PREFIX_MATCH},
      {"あい", UserHistoryPredictor::NO_MATCH},
      {"あか", UserHistoryPredictor::LEFT_PREFIX_MATCH},
      {"あかい", UserHistoryPredictor::LEFT_PREFIX_MATCH},
  };

  for (size_t i = 0; i < arraysize(kTests1); ++i) {
    EXPECT_EQ(kTests1[i].expect_type,
              UserHistoryPredictor::GetMatchTypeFromInput(
                  "あ", "あ", expanded.get(), kTests1[i].target))
        << kTests1[i].target;
  }

  // only expanded
  // preedit: "ｋ"
  // input_key: ""
  // key_base: ""
  // key_expanded: "か","き","く","け", "こ"
  const MatchTypeTestData kTests2[] = {
      {"", UserHistoryPredictor::NO_MATCH},
      {"い", UserHistoryPredictor::NO_MATCH},
      {"いか", UserHistoryPredictor::NO_MATCH},
      {"か", UserHistoryPredictor::LEFT_PREFIX_MATCH},
      {"かいがい", UserHistoryPredictor::LEFT_PREFIX_MATCH},
  };

  for (size_t i = 0; i < arraysize(kTests2); ++i) {
    EXPECT_EQ(kTests2[i].expect_type,
              UserHistoryPredictor::GetMatchTypeFromInput(
                  "", "", expanded.get(), kTests2[i].target))
        << kTests2[i].target;
  }
}

TEST_F(UserHistoryPredictorTest, GetMatchTypeFromInputKana) {
  // We have to define this here,
  // because UserHistoryPredictor::MatchType is private
  struct MatchTypeTestData {
    const std::string target;
    const UserHistoryPredictor::MatchType expect_type;
  };

  // Kana
  // preedit: "あし"
  // input_key: "あし"
  // key_base: "あ"
  // key_expanded: "し","じ"
  unique_ptr<Trie<std::string>> expanded(new Trie<std::string>);
  expanded->AddEntry("し", "し");
  expanded->AddEntry("じ", "じ");

  const MatchTypeTestData kTests1[] = {
      {"", UserHistoryPredictor::NO_MATCH},
      {"い", UserHistoryPredictor::NO_MATCH},
      {"いし", UserHistoryPredictor::NO_MATCH},
      {"あ", UserHistoryPredictor::RIGHT_PREFIX_MATCH},
      {"あし", UserHistoryPredictor::EXACT_MATCH},
      {"あじ", UserHistoryPredictor::LEFT_PREFIX_MATCH},
      {"あした", UserHistoryPredictor::LEFT_PREFIX_MATCH},
      {"あじしお", UserHistoryPredictor::LEFT_PREFIX_MATCH},
  };

  for (size_t i = 0; i < arraysize(kTests1); ++i) {
    EXPECT_EQ(kTests1[i].expect_type,
              UserHistoryPredictor::GetMatchTypeFromInput(
                  "あし", "あ", expanded.get(), kTests1[i].target))
        << kTests1[i].target;
  }

  // only expanded
  // preedit: "し"
  // input_key: "し"
  // key_base: ""
  // key_expanded: "し","じ"
  const MatchTypeTestData kTests2[] = {
      {"", UserHistoryPredictor::NO_MATCH},
      {"い", UserHistoryPredictor::NO_MATCH},
      {"し", UserHistoryPredictor::EXACT_MATCH},
      {"じ", UserHistoryPredictor::LEFT_PREFIX_MATCH},
      {"しじみ", UserHistoryPredictor::LEFT_PREFIX_MATCH},
      {"じかん", UserHistoryPredictor::LEFT_PREFIX_MATCH},
  };

  for (size_t i = 0; i < arraysize(kTests2); ++i) {
    EXPECT_EQ(kTests2[i].expect_type,
              UserHistoryPredictor::GetMatchTypeFromInput(
                  "し", "", expanded.get(), kTests2[i].target))
        << kTests2[i].target;
  }
}

namespace {
void InitSegmentsFromInputSequence(const std::string &text,
                                   composer::Composer *composer,
                                   ConversionRequest *request,
                                   Segments *segments) {
  DCHECK(composer);
  DCHECK(request);
  DCHECK(segments);
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
      key.set_key_string(std::string(begin, mblen));
    }
    begin += mblen;
    composer->InsertCharacterKeyEvent(key);
  }

  request->set_composer(composer);

  segments->set_request_type(Segments::PREDICTION);
  Segment *segment = segments->add_segment();
  CHECK(segment);
  std::string query;
  composer->GetQueryForPrediction(&query);
  segment->set_key(query);
}
}  // namespace

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsRoman) {
  table_->LoadFromFile("system://romanji-hiragana.tsv");
  composer_->SetTable(table_.get());
  Segments segments;

  InitSegmentsFromInputSequence("gu-g", composer_.get(), convreq_.get(),
                                &segments);
  std::string input_key;
  std::string base;
  unique_ptr<Trie<std::string>> expanded;
  UserHistoryPredictor::GetInputKeyFromSegments(*convreq_, segments, &input_key,
                                                &base, &expanded);
  EXPECT_EQ("ぐーｇ", input_key);
  EXPECT_EQ("ぐー", base);
  EXPECT_TRUE(expanded != nullptr);
  std::string value;
  size_t key_length = 0;
  bool has_subtrie = false;
  EXPECT_TRUE(expanded->LookUpPrefix("ぐ", &value, &key_length, &has_subtrie));
  EXPECT_EQ("ぐ", value);
}

namespace {
uint32 GetRandomAscii() {
  return static_cast<uint32>(' ') +
         Util::Random(static_cast<uint32>('~' - ' '));
}
}  // namespace

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsRomanRandom) {
  table_->LoadFromFile("system://romanji-hiragana.tsv");
  composer_->SetTable(table_.get());
  Segments segments;

  for (size_t i = 0; i < 1000; ++i) {
    composer_->Reset();
    const int len = 1 + Util::Random(4);
    DCHECK_GE(len, 1);
    DCHECK_LE(len, 5);
    std::string input;
    for (size_t j = 0; j < len; ++j) {
      input += GetRandomAscii();
    }
    InitSegmentsFromInputSequence(input, composer_.get(), convreq_.get(),
                                  &segments);
    std::string input_key;
    std::string base;
    unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*convreq_, segments,
                                                  &input_key, &base, &expanded);
  }
}

// Found by random test.
// input_key != base by compoesr modification.
TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsShouldNotCrash) {
  table_->LoadFromFile("system://romanji-hiragana.tsv");
  composer_->SetTable(table_.get());
  Segments segments;

  {
    InitSegmentsFromInputSequence("8,+", composer_.get(), convreq_.get(),
                                  &segments);
    std::string input_key;
    std::string base;
    unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*convreq_, segments,
                                                  &input_key, &base, &expanded);
  }
}

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsRomanN) {
  table_->LoadFromFile("system://romanji-hiragana.tsv");
  composer_->SetTable(table_.get());
  Segments segments;

  {
    InitSegmentsFromInputSequence("n", composer_.get(), convreq_.get(),
                                  &segments);
    std::string input_key;
    std::string base;
    unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*convreq_, segments,
                                                  &input_key, &base, &expanded);
    EXPECT_EQ("ｎ", input_key);
    EXPECT_EQ("", base);
    EXPECT_TRUE(expanded != nullptr);
    std::string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_TRUE(
        expanded->LookUpPrefix("な", &value, &key_length, &has_subtrie));
    EXPECT_EQ("な", value);
  }

  composer_->Reset();
  segments.Clear();
  {
    InitSegmentsFromInputSequence("nn", composer_.get(), convreq_.get(),
                                  &segments);
    std::string input_key;
    std::string base;
    unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*convreq_, segments,
                                                  &input_key, &base, &expanded);
    EXPECT_EQ("ん", input_key);
    EXPECT_EQ("ん", base);
    EXPECT_TRUE(expanded == nullptr);
  }

  composer_->Reset();
  segments.Clear();
  {
    InitSegmentsFromInputSequence("n'", composer_.get(), convreq_.get(),
                                  &segments);
    std::string input_key;
    std::string base;
    unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*convreq_, segments,
                                                  &input_key, &base, &expanded);
    EXPECT_EQ("ん", input_key);
    EXPECT_EQ("ん", base);
    EXPECT_TRUE(expanded == nullptr);
  }

  composer_->Reset();
  segments.Clear();
  {
    InitSegmentsFromInputSequence("n'n", composer_.get(), convreq_.get(),
                                  &segments);
    std::string input_key;
    std::string base;
    unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*convreq_, segments,
                                                  &input_key, &base, &expanded);
    EXPECT_EQ("んｎ", input_key);
    EXPECT_EQ("ん", base);
    EXPECT_TRUE(expanded != nullptr);
    std::string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_TRUE(
        expanded->LookUpPrefix("な", &value, &key_length, &has_subtrie));
    EXPECT_EQ("な", value);
  }
}

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsFlickN) {
  table_->LoadFromFile("system://flick-hiragana.tsv");
  composer_->SetTable(table_.get());
  Segments segments;

  {
    InitSegmentsFromInputSequence("/", composer_.get(), convreq_.get(),
                                  &segments);
    std::string input_key;
    std::string base;
    unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*convreq_, segments,
                                                  &input_key, &base, &expanded);
    EXPECT_EQ("ん", input_key);
    EXPECT_EQ("", base);
    EXPECT_TRUE(expanded != nullptr);
    std::string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_TRUE(
        expanded->LookUpPrefix("ん", &value, &key_length, &has_subtrie));
    EXPECT_EQ("ん", value);
  }
}

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegments12KeyN) {
  table_->LoadFromFile("system://12keys-hiragana.tsv");
  composer_->SetTable(table_.get());
  Segments segments;

  {
    InitSegmentsFromInputSequence("わ00", composer_.get(), convreq_.get(),
                                  &segments);
    std::string input_key;
    std::string base;
    unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*convreq_, segments,
                                                  &input_key, &base, &expanded);
    EXPECT_EQ("ん", input_key);
    EXPECT_EQ("", base);
    EXPECT_TRUE(expanded != nullptr);
    std::string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_TRUE(
        expanded->LookUpPrefix("ん", &value, &key_length, &has_subtrie));
    EXPECT_EQ("ん", value);
  }
}

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsKana) {
  table_->LoadFromFile("system://kana.tsv");
  composer_->SetTable(table_.get());
  Segments segments;

  InitSegmentsFromInputSequence("あか", composer_.get(), convreq_.get(),
                                &segments);

  {
    std::string input_key;
    std::string base;
    unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*convreq_, segments,
                                                  &input_key, &base, &expanded);
    EXPECT_EQ("あか", input_key);
    EXPECT_EQ("あ", base);
    EXPECT_TRUE(expanded != nullptr);
    std::string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_TRUE(
        expanded->LookUpPrefix("が", &value, &key_length, &has_subtrie));
    EXPECT_EQ("が", value);
  }
}

TEST_F(UserHistoryPredictorTest, RealtimeConversionInnerSegment) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;
  {
    const char kKey[] = "わたしのなまえはなかのです";
    const char kValue[] = "私の名前は中野です";
    SetUpInputForPrediction(kKey, composer_.get(), &segments);
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->add_candidate();
    CHECK(candidate);
    candidate->Init();
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
  predictor->Finish(*convreq_, &segments);
  segments.Clear();

  SetUpInputForPrediction("なかの", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_TRUE(FindCandidateByValue("中野です", segments));

  segments.Clear();
  SetUpInputForPrediction("なまえ", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_TRUE(FindCandidateByValue("名前は", segments));
  EXPECT_TRUE(FindCandidateByValue("名前は中野です", segments));
}

TEST_F(UserHistoryPredictorTest, ZeroQueryFromRealtimeConversion) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;
  {
    const char kKey[] = "わたしのなまえはなかのです";
    const char kValue[] = "私の名前は中野です";
    SetUpInputForPrediction(kKey, composer_.get(), &segments);
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->add_candidate();
    CHECK(candidate);
    candidate->Init();
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
  predictor->Finish(*convreq_, &segments);
  segments.Clear();

  SetUpInputForConversion("わたしの", composer_.get(), &segments);
  AddCandidate(0, "私の", &segments);
  predictor->Finish(*convreq_, &segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  SetUpInputForSuggestionWithHistory("", "わたしの", "私の", composer_.get(),
                                     &segments);
  commands::Request request;
  request_->set_zero_query_suggestion(true);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_TRUE(FindCandidateByValue("名前は", segments));
}

TEST_F(UserHistoryPredictorTest, LongCandidateForMobile) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  Segments segments;
  for (size_t i = 0; i < 3; ++i) {
    const char kKey[] = "よろしくおねがいします";
    const char kValue[] = "よろしくお願いします";
    SetUpInputForPrediction(kKey, composer_.get(), &segments);
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->add_candidate();
    CHECK(candidate);
    candidate->Init();
    candidate->value = kValue;
    candidate->content_value = kValue;
    candidate->key = kKey;
    candidate->content_key = kKey;
    predictor->Finish(*convreq_, &segments);
    segments.Clear();
  }

  SetUpInputForPrediction("よろ", composer_.get(), &segments);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
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
  EXPECT_EQ(5, e.next_entries_size());

  UserHistoryPredictor::EraseNextEntries(30, &e);
  ASSERT_EQ(4, e.next_entries_size());
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_NE(30, e.next_entries(i).entry_fp());
  }

  UserHistoryPredictor::EraseNextEntries(10, &e);
  ASSERT_EQ(2, e.next_entries_size());
  for (size_t i = 0; i < 2; ++i) {
    EXPECT_NE(10, e.next_entries(i).entry_fp());
  }

  UserHistoryPredictor::EraseNextEntries(100, &e);
  EXPECT_EQ(0, e.next_entries_size());
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
    EXPECT_EQ(UserHistoryPredictor::NOT_FOUND,
              predictor->RemoveNgramChain("hoge", "HOGE", entries[i], &dummy1,
                                          0, &dummy2, 0));
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
        UserHistoryPredictor::DONE,
        predictor->RemoveNgramChain("abc", "ABC", a, &dummy1, 0, &dummy2, 0));
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
    EXPECT_EQ(UserHistoryPredictor::TAIL,
              predictor->RemoveNgramChain("a", "A", a, &dummy1, 0, &dummy2, 0));
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
        UserHistoryPredictor::DONE,
        predictor->RemoveNgramChain("ab", "AB", a, &dummy1, 0, &dummy2, 0));
    for (size_t i = 0; i < entries.size(); ++i) {
      EXPECT_FALSE(entries[i]->removed());
    }
    EXPECT_FALSE(IsConnected(*a, *b));
    EXPECT_FALSE(IsConnected(*b, *c));
  }
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntry_Unigram) {
  ScopedClockMock clock(1, 0);

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
  const std::string key = "japanese";
  for (size_t i = 0; i < key.size(); ++i) {
    const std::string &prefix = key.substr(0, i);
    EXPECT_FALSE(IsSuggested(predictor, prefix, "Japanese"));
    EXPECT_FALSE(IsPredicted(predictor, prefix, "Japanese"));
  }
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntry_Bigram_DeleteWhole) {
  ScopedClockMock clock(1, 0);

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
  const std::string key = "japaneseinput";
  for (size_t i = 0; i < key.size(); ++i) {
    const std::string &prefix = key.substr(0, i);
    EXPECT_FALSE(IsSuggested(predictor, prefix, "Japaneseinput"));
    EXPECT_FALSE(IsPredicted(predictor, prefix, "Japaneseinput"));
  }

  // However, predictor should show "Japanese" and "Input".
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "Input"));
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntry_Bigram_DeleteFirst) {
  ScopedClockMock clock(1, 0);

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
  const std::string key = "japaneseinput";
  for (size_t i = 0; i < key.size(); ++i) {
    const std::string &prefix = key.substr(0, i);
    EXPECT_FALSE(IsSuggested(predictor, prefix, "Japanese"));
    EXPECT_FALSE(IsPredicted(predictor, prefix, "Japanese"));
  }

  // However, predictor should show "JapaneseInput" and "Input".
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "Input"));
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntry_Bigram_DeleteSecond) {
  ScopedClockMock clock(1, 0);

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
  const std::string key = "input";
  for (size_t i = 0; i < key.size(); ++i) {
    const std::string &prefix = key.substr(0, i);
    EXPECT_FALSE(IsSuggested(predictor, prefix, "Input"));
    EXPECT_FALSE(IsPredicted(predictor, prefix, "Input"));
  }

  // However, predictor should show "Japanese" and "JapaneseInput".
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntry_Trigram_DeleteWhole) {
  ScopedClockMock clock(1, 0);

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
    const std::string key = "japaneseinputmethod";
    for (size_t i = 0; i < key.size(); ++i) {
      const std::string &prefix = key.substr(0, i);
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
    const std::string key = "inputmethod";
    for (size_t i = 0; i < key.size(); ++i) {
      const std::string &prefix = key.substr(0, i);
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

TEST_F(UserHistoryPredictorTest, ClearHistoryEntry_Trigram_DeleteFirst) {
  ScopedClockMock clock(1, 0);

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
    const std::string key = "japaneseinputmethod";
    for (size_t i = 0; i < key.size(); ++i) {
      const std::string &prefix = key.substr(0, i);
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

TEST_F(UserHistoryPredictorTest, ClearHistoryEntry_Trigram_DeleteSecond) {
  ScopedClockMock clock(1, 0);

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
    const std::string key = "inputmethod";
    for (size_t i = 0; i < key.size(); ++i) {
      const std::string &prefix = key.substr(0, i);
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

TEST_F(UserHistoryPredictorTest, ClearHistoryEntry_Trigram_DeleteThird) {
  ScopedClockMock clock(1, 0);

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
    const std::string key = "method";
    for (size_t i = 0; i < key.size(); ++i) {
      const std::string &prefix = key.substr(0, i);
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

TEST_F(UserHistoryPredictorTest, ClearHistoryEntry_Trigram_DeleteFirstBigram) {
  ScopedClockMock clock(1, 0);

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
    const std::string key = "japaneseinputmethod";
    for (size_t i = 0; i < key.size(); ++i) {
      const std::string &prefix = key.substr(0, i);
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

TEST_F(UserHistoryPredictorTest, ClearHistoryEntry_Trigram_DeleteSecondBigram) {
  ScopedClockMock clock(1, 0);

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
    const std::string key = "inputmethod";
    for (size_t i = 0; i < key.size(); ++i) {
      const std::string &prefix = key.substr(0, i);
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

TEST_F(UserHistoryPredictorTest, ClearHistoryEntry_Scenario1) {
  // Tests a common scenario: First, a user accidentally inputs an incomplete
  // romaji sequence and the predictor learns it.  Then, the user deletes it.
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Set up history. Convert "ぐーぐｒ" to "グーグr" 3 times.  This emulates a
  // case that a user accidentally input incomplete sequence.
  for (int i = 0; i < 3; ++i) {
    Segments segments;
    SetUpInputForConversion("ぐーぐｒ", composer_.get(), &segments);
    AddCandidate("グーグr", &segments);
    predictor->Finish(*convreq_, &segments);
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

TEST_F(UserHistoryPredictorTest, ClearHistoryEntry_Scenario2) {
  // Tests a common scenario: First, a user inputs a sentence ending with a
  // symbol and it's learned by the predictor.  Then, the user deletes the
  // history containing the symbol.
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Set up history. Convert "きょうもいいてんき！" to "今日もいい天気!" 3 times
  // so that the predictor learns the sentence. We assume that this sentence
  // consists of three segments: "今日も|いい天気|!".
  for (int i = 0; i < 3; ++i) {
    Segments segments;
    segments.set_request_type(Segments::CONVERSION);

    // The first segment: ("きょうも", "今日も")
    Segment *seg = segments.add_segment();
    seg->set_key("きょうも");
    seg->set_segment_type(Segment::FIXED_VALUE);
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "今日も";
    candidate->content_value = "今日";
    candidate->key = seg->key();
    candidate->content_key = "きょう";

    // The second segment: ("いいてんき", "いい天気")
    seg = segments.add_segment();
    seg->set_key("いいてんき");
    seg->set_segment_type(Segment::FIXED_VALUE);
    candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "いい天気";
    candidate->content_value = candidate->value;
    candidate->key = seg->key();
    candidate->content_key = seg->key();

    // The third segment: ("！", "!")
    seg = segments.add_segment();
    seg->set_key("！");
    seg->set_segment_type(Segment::FIXED_VALUE);
    candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "!";
    candidate->content_value = "!";
    candidate->key = seg->key();
    candidate->content_key = seg->key();

    predictor->Finish(*convreq_, &segments);
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
    const char kKey[] = "とうきょうかなごやにいきたい";
    const char kValue[] = "東京か名古屋に行きたい";
    SetUpInputForPrediction(kKey, composer_.get(), &segments);
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->add_candidate();
    candidate->Init();
    candidate->key = kKey;
    candidate->value = kValue;
    candidate->content_key = kKey;
    candidate->content_value = kValue;
    candidate->PushBackInnerSegmentBoundary(18, 9, 15, 6);
    candidate->PushBackInnerSegmentBoundary(12, 12, 9, 9);
    candidate->PushBackInnerSegmentBoundary(12, 12, 12, 12);
    predictor->Finish(*convreq_, &segments);
  }

  segments.Clear();
  SetUpInputForPrediction("と", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_TRUE(FindCandidateByValue("東京", segments));
  EXPECT_TRUE(FindCandidateByValue("東京か", segments));

  segments.Clear();
  SetUpInputForPrediction("な", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_TRUE(FindCandidateByValue("名古屋", segments));
  EXPECT_TRUE(FindCandidateByValue("名古屋に", segments));

  segments.Clear();
  SetUpInputForPrediction("い", composer_.get(), &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_TRUE(FindCandidateByValue("行きたい", segments));
}

TEST_F(UserHistoryPredictorTest, JoinedSegmentsTest_Mobile) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();
  commands::RequestForUnitTest::FillMobileRequest(request_.get());
  Segments segments;

  SetUpInputForConversion("わたしの", composer_.get(), &segments);
  AddCandidate(0, "私の", &segments);

  AddSegmentForConversion("なまえは", &segments);
  AddCandidate(1, "名前は", &segments);

  predictor->Finish(*convreq_, &segments);
  segments.Clear();

  SetUpInputForSuggestion("わたし", composer_.get(), &segments);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_EQ(1, segments.segment(0).candidates_size());
  EXPECT_EQ("私の", segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
  segments.Clear();

  SetUpInputForPrediction("わたしの", composer_.get(), &segments);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_EQ(1, segments.segment(0).candidates_size());
  EXPECT_EQ("私の", segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
  segments.Clear();

  SetUpInputForPrediction("わたしのな", composer_.get(), &segments);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_EQ(1, segments.segment(0).candidates_size());
  EXPECT_EQ("私の名前は", segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
  segments.Clear();
}

TEST_F(UserHistoryPredictorTest, JoinedSegmentsTest_Desktop) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;

  SetUpInputForConversion("わたしの", composer_.get(), &segments);
  AddCandidate(0, "私の", &segments);

  AddSegmentForConversion("なまえは", &segments);
  AddCandidate(1, "名前は", &segments);

  predictor->Finish(*convreq_, &segments);

  segments.Clear();

  SetUpInputForSuggestion("わたし", composer_.get(), &segments);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_EQ(2, segments.segment(0).candidates_size());
  EXPECT_EQ("私の", segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
  EXPECT_EQ("私の名前は", segments.segment(0).candidate(1).value);
  EXPECT_TRUE(segments.segment(0).candidate(1).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
  segments.Clear();

  SetUpInputForPrediction("わたしの", composer_.get(), &segments);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_EQ(1, segments.segment(0).candidates_size());
  EXPECT_EQ("私の名前は", segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
  segments.Clear();

  SetUpInputForPrediction("わたしのな", composer_.get(), &segments);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_EQ(1, segments.segment(0).candidates_size());
  EXPECT_EQ("私の名前は", segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
  segments.Clear();
}

TEST_F(UserHistoryPredictorTest, UsageStats) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  Segments segments;
  EXPECT_COUNT_STATS("CommitUserHistoryPredictor", 0);
  EXPECT_COUNT_STATS("CommitUserHistoryPredictorZeroQuery", 0);

  SetUpInputForConversion("なまえは", composer_.get(), &segments);
  AddCandidate(0, "名前は", &segments);
  segments.mutable_conversion_segment(0)->mutable_candidate(0)->source_info |=
      Segment::Candidate::USER_HISTORY_PREDICTOR;
  predictor->Finish(*convreq_, &segments);

  EXPECT_COUNT_STATS("CommitUserHistoryPredictor", 1);
  EXPECT_COUNT_STATS("CommitUserHistoryPredictorZeroQuery", 0);

  segments.Clear();

  // Zero query
  SetUpInputForConversion("", composer_.get(), &segments);
  AddCandidate(0, "名前は", &segments);
  segments.mutable_conversion_segment(0)->mutable_candidate(0)->source_info |=
      Segment::Candidate::USER_HISTORY_PREDICTOR;
  predictor->Finish(*convreq_, &segments);

  // UserHistoryPredictor && ZeroQuery
  EXPECT_COUNT_STATS("CommitUserHistoryPredictor", 2);
  EXPECT_COUNT_STATS("CommitUserHistoryPredictorZeroQuery", 1);
}

TEST_F(UserHistoryPredictorTest, PunctuationLink_Mobile) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();
  commands::RequestForUnitTest::FillMobileRequest(request_.get());
  Segments segments;
  {
    SetUpInputForConversion("ございます", composer_.get(), &segments);
    AddCandidate(0, "ございます", &segments);
    predictor->Finish(*convreq_, &segments);

    SetUpInputForConversionWithHistory("!", "ございます", "ございます",
                                       composer_.get(), &segments);
    AddCandidate(1, "！", &segments);
    predictor->Finish(*convreq_, &segments);

    segments.Clear();
    SetUpInputForSuggestion("ございま", composer_.get(), &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("ございます", segments.conversion_segment(0).candidate(0).value);
    EXPECT_FALSE(FindCandidateByValue("ございます！", segments));

    // Zero query from "ございます" -> "！"
    segments.Clear();
    SetUpInputForConversion("ございます", composer_.get(), &segments);
    AddCandidate(0, "ございます", &segments);
    SetUpInputForSuggestionWithHistory("", "ございます", "ございます",
                                       composer_.get(), &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("！", segments.conversion_segment(0).candidate(0).value);
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    SetUpInputForConversion("!", composer_.get(), &segments);
    AddCandidate(0, "！", &segments);
    predictor->Finish(*convreq_, &segments);

    SetUpInputForSuggestionWithHistory("ございます", "!", "！", composer_.get(),
                                       &segments);
    AddCandidate(1, "ございます", &segments);
    predictor->Finish(*convreq_, &segments);

    // Zero query from "！" -> no suggestion
    segments.Clear();
    SetUpInputForSuggestionWithHistory("", "!", "！", composer_.get(),
                                       &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    SetUpInputForConversion("ございます!", composer_.get(), &segments);
    AddCandidate(0, "ございます！", &segments);

    predictor->Finish(*convreq_, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    AddSegmentForConversion("よろしくおねがいします", &segments);
    AddCandidate(1, "よろしくお願いします", &segments);
    predictor->Finish(*convreq_, &segments);

    // Zero query from "！" -> no suggestion
    segments.Clear();
    SetUpInputForConversion("!", composer_.get(), &segments);
    AddCandidate(0, "！", &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
    AddSegmentForSuggestion("", &segments);  // empty request
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

    // Zero query from "ございます！" -> no suggestion
    segments.Clear();
    SetUpInputForConversion("ございます!", composer_.get(), &segments);
    AddCandidate(0, "ございます！", &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
    AddSegmentForSuggestion("", &segments);  // empty request
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    SetUpInputForConversion("ございます", composer_.get(), &segments);
    AddCandidate(0, "ございます", &segments);
    predictor->Finish(*convreq_, &segments);

    SetUpInputForConversionWithHistory("!よろしくおねがいします", "ございます",
                                       "ございます", composer_.get(),
                                       &segments);
    AddCandidate(1, "！よろしくお願いします", &segments);
    predictor->Finish(*convreq_, &segments);

    segments.Clear();
    SetUpInputForSuggestion("ございま", composer_.get(), &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("ございます", segments.conversion_segment(0).candidate(0).value);
    EXPECT_FALSE(
        FindCandidateByValue("ございます！よろしくお願いします", segments));

    // Zero query from "ございます" -> no suggestion
    SetUpInputForConversionWithHistory("", "ございます", "ございます",
                                       composer_.get(), &segments);
    AddSegmentForSuggestion("", &segments);  // empty request
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
  }
}

TEST_F(UserHistoryPredictorTest, PunctuationLink_Desktop) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();
  Segments segments;
  {
    SetUpInputForConversion("ございます", composer_.get(), &segments);
    AddCandidate(0, "ございます", &segments);

    predictor->Finish(*convreq_, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    AddSegmentForConversion("!", &segments);
    AddCandidate(1, "！", &segments);
    predictor->Finish(*convreq_, &segments);

    segments.Clear();
    SetUpInputForSuggestion("ございま", composer_.get(), &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("ございます", segments.conversion_segment(0).candidate(0).value);
    EXPECT_FALSE(FindCandidateByValue("ございます！", segments));

    segments.Clear();
    SetUpInputForSuggestion("ございます", composer_.get(), &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("ございます", segments.conversion_segment(0).candidate(0).value);
    EXPECT_FALSE(FindCandidateByValue("ございます！", segments));
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    SetUpInputForConversion("!", composer_.get(), &segments);
    AddCandidate(0, "！", &segments);

    predictor->Finish(*convreq_, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    AddSegmentForConversion("よろしくおねがいします", &segments);
    AddCandidate(1, "よろしくお願いします", &segments);
    predictor->Finish(*convreq_, &segments);

    segments.Clear();
    SetUpInputForSuggestion("!", composer_.get(), &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    SetUpInputForConversion("ございます!", composer_.get(), &segments);
    AddCandidate(0, "ございます！", &segments);

    predictor->Finish(*convreq_, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    AddSegmentForConversion("よろしくおねがいします", &segments);
    AddCandidate(1, "よろしくお願いします", &segments);
    predictor->Finish(*convreq_, &segments);

    segments.Clear();
    SetUpInputForSuggestion("ございます", composer_.get(), &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("ございます！",
              segments.conversion_segment(0).candidate(0).value);
    EXPECT_FALSE(
        FindCandidateByValue("ございます！よろしくお願いします", segments));

    segments.Clear();
    SetUpInputForSuggestion("ございます!", composer_.get(), &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("ございます！",
              segments.conversion_segment(0).candidate(0).value);
    EXPECT_FALSE(
        FindCandidateByValue("ございます！よろしくお願いします", segments));
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    SetUpInputForConversion("ございます", composer_.get(), &segments);
    AddCandidate(0, "ございます", &segments);

    predictor->Finish(*convreq_, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    AddSegmentForConversion("!よろしくおねがいします", &segments);
    AddCandidate(1, "！よろしくお願いします", &segments);
    predictor->Finish(*convreq_, &segments);

    segments.Clear();
    SetUpInputForSuggestion("ございます", composer_.get(), &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("ございます", segments.conversion_segment(0).candidate(0).value);
    EXPECT_FALSE(FindCandidateByValue("ございます！", segments));
    EXPECT_FALSE(
        FindCandidateByValue("ございます！よろしくお願いします", segments));
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    // Note that "よろしくお願いします:よろしくおねがいします" is the sentence
    // like candidate. Please refer to user_history_predictor.cc
    SetUpInputForConversion("よろしくおねがいします", composer_.get(),
                            &segments);
    AddCandidate(0, "よろしくお願いします", &segments);

    predictor->Finish(*convreq_, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    AddSegmentForConversion("!", &segments);
    AddCandidate(1, "！", &segments);
    predictor->Finish(*convreq_, &segments);

    segments.Clear();
    SetUpInputForSuggestion("よろしくおねがいします", composer_.get(),
                            &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_TRUE(FindCandidateByValue("よろしくお願いします！", segments));
  }
}

TEST_F(UserHistoryPredictorTest, 62DayOldEntriesAreDeletedAtSync) {
  ScopedClockMock clock(1, 0);

  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Let the predictor learn "私の名前は中野です".
  Segments segments;
  SetUpInputForConversion("わたしのなまえはなかのです", composer_.get(),
                          &segments);
  AddCandidate("私の名前は中野です", &segments);
  predictor->Finish(*convreq_, &segments);

  // Verify that "私の名前は中野です" is predicted.
  segments.Clear();
  SetUpInputForPrediction("わたしの", composer_.get(), &segments);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_TRUE(FindCandidateByValue("私の名前は中野です", segments));

  // Now, simulate the case where 63 days passed.
  clock->PutClockForward(63 * 24 * 60 * 60, 0);

  // Let the predictor learn "私の名前は高橋です".
  segments.Clear();
  SetUpInputForConversion("わたしのなまえはたかはしです", composer_.get(),
                          &segments);
  AddCandidate("私の名前は高橋です", &segments);
  predictor->Finish(*convreq_, &segments);

  // Verify that "私の名前は高橋です" is predicted but "私の名前は中野です" is
  // not.  The latter one is still in on-memory data structure but lookup is
  // prevented.  The entry is removed when the data is written to disk.
  segments.Clear();
  SetUpInputForPrediction("わたしの", composer_.get(), &segments);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_TRUE(FindCandidateByValue("私の名前は高橋です", segments));
  EXPECT_FALSE(FindCandidateByValue("私の名前は中野です", segments));

  // Here, write the history to a storage.
  ASSERT_TRUE(predictor->Sync());
  WaitForSyncer(predictor);

  // Verify that "私の名前は中野です" is no longer predicted because it was
  // learned 63 days before.
  segments.Clear();
  SetUpInputForPrediction("わたしの", composer_.get(), &segments);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_TRUE(FindCandidateByValue("私の名前は高橋です", segments));
  EXPECT_FALSE(FindCandidateByValue("私の名前は中野です", segments));

  // Verify also that on-memory data structure doesn't contain node for 中野.
  bool found_takahashi = false;
  for (const auto *elem = predictor->dic_->Head(); elem != nullptr;
       elem = elem->next) {
    EXPECT_EQ(std::string::npos, elem->value.value().find("中野"));
    if (elem->value.value().find("高橋")) {
      found_takahashi = true;
    }
  }
  EXPECT_TRUE(found_takahashi);
}

TEST_F(UserHistoryPredictorTest, FutureTimestamp) {
  // Test the case where history has "future" timestamps.
  ScopedClockMock clock(10000, 0);

  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Let the predictor learn "私の名前は中野です".
  Segments segments;
  SetUpInputForConversion("わたしのなまえはなかのです", composer_.get(),
                          &segments);
  AddCandidate("私の名前は中野です", &segments);
  predictor->Finish(*convreq_, &segments);

  // Verify that "私の名前は中野です" is predicted.
  segments.Clear();
  SetUpInputForPrediction("わたしの", composer_.get(), &segments);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_TRUE(FindCandidateByValue("私の名前は中野です", segments));

  // Now, go back to the past.
  clock->SetTime(1, 0);

  // Verify that "私の名前は中野です" is predicted without crash.
  segments.Clear();
  SetUpInputForPrediction("わたしの", composer_.get(), &segments);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_TRUE(FindCandidateByValue("私の名前は中野です", segments));
}

}  // namespace mozc
