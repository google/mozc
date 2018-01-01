// Copyright 2010-2018, Google Inc.
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
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"

DECLARE_bool(enable_expansion_for_user_history_predictor);

namespace mozc {
namespace {

using std::unique_ptr;

using commands::Request;
using config::Config;
using dictionary::DictionaryMock;
using dictionary::SuppressionDictionary;
using dictionary::Token;

void AddSegmentForSuggestion(const string &key, Segments *segments) {
  segments->set_max_prediction_candidates_size(10);
  segments->set_request_type(Segments::SUGGESTION);
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FIXED_VALUE);
}

void MakeSegmentsForSuggestion(const string &key, Segments *segments) {
  segments->Clear();
  AddSegmentForSuggestion(key, segments);
}

void AddSegmentForPrediction(const string &key, Segments *segments) {
  segments->set_max_prediction_candidates_size(10);
  segments->set_request_type(Segments::PREDICTION);
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FIXED_VALUE);
}

void MakeSegmentsForPrediction(const string &key, Segments *segments) {
  segments->Clear();
  AddSegmentForPrediction(key, segments);
}

void AddSegmentForConversion(const string &key, Segments *segments) {
  segments->set_request_type(Segments::CONVERSION);
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FIXED_VALUE);
}

void MakeSegmentsForConversion(const string &key, Segments *segments) {
  segments->Clear();
  AddSegmentForConversion(key, segments);
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

bool FindCandidateByValue(const string &value, const Segments &segments) {
  for (size_t i = 0;
       i < segments.conversion_segment(0).candidates_size(); ++i) {
    if (segments.conversion_segment(0).candidate(i).value == value) {
      return true;
    }
  }
  return false;
}
}   // namespace

class UserHistoryPredictorTest : public ::testing::Test {
 public:
  UserHistoryPredictorTest()
      : default_expansion_(FLAGS_enable_expansion_for_user_history_predictor) {
  }

  ~UserHistoryPredictorTest() override {
    FLAGS_enable_expansion_for_user_history_predictor = default_expansion_;
  }

 protected:
  void SetUp() override {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    request_.reset(new Request);
    config_.reset(new Config);
    config::ConfigHandler::GetDefaultConfig(config_.get());
    table_.reset(new composer::Table);
    composer_.reset(
        new composer::Composer(table_.get(), request_.get(), config_.get()));
    convreq_.reset(
        new ConversionRequest(composer_.get(), request_.get(), config_.get()));
    data_and_predictor_.reset(CreateDataAndPredictor());

    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
  }

  void TearDown() override {
    FLAGS_enable_expansion_for_user_history_predictor = default_expansion_;

    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
  }

  UserHistoryPredictor *GetUserHistoryPredictor() {
    return data_and_predictor_->predictor.get();
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
                          const string &key, const string &value) {
    const ConversionRequest conversion_request;
    Segments segments;
    MakeSegmentsForSuggestion(key, &segments);
    return predictor->PredictForRequest(conversion_request, &segments) &&
           FindCandidateByValue(value, segments);
  }

  static bool IsPredicted(UserHistoryPredictor *predictor,
                          const string &key, const string &value) {
    const ConversionRequest conversion_request;
    Segments segments;
    MakeSegmentsForPrediction(key, &segments);
    return predictor->PredictForRequest(conversion_request, &segments) &&
           FindCandidateByValue(value, segments);
  }

  static bool IsSuggestedAndPredicted(UserHistoryPredictor *predictor,
                                      const string &key, const string &value) {
    return IsSuggested(predictor, key, value) &&
           IsPredicted(predictor, key, value);
  }

  static UserHistoryPredictor::Entry *InsertEntry(
      UserHistoryPredictor *predictor,
      const string &key, const string &value) {
    UserHistoryPredictor::Entry *e =
        &predictor->dic_->Insert(predictor->Fingerprint(key, value))->value;
    e->set_key(key);
    e->set_value(value);
    e->set_removed(false);
    return e;
  }

  static UserHistoryPredictor::Entry *AppendEntry(
      UserHistoryPredictor *predictor,
      const string &key, const string &value,
      UserHistoryPredictor::Entry *prev) {
    prev->add_next_entries()->set_entry_fp(
        predictor->Fingerprint(key, value));
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

    // Check the predictor functionality for the above history structure.
    EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
    EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
    EXPECT_TRUE(IsSuggestedAndPredicted(predictor,
                                        "japan", "JapaneseInputMethod"));
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
    ret->dictionary.reset(new DictionaryMock);
    ret->suppression_dictionary.reset(new SuppressionDictionary);
    ret->pos_matcher.Set(data_manager.GetPOSMatcherData());
    ret->predictor.reset(
        new UserHistoryPredictor(ret->dictionary.get(),
                                 &ret->pos_matcher,
                                 ret->suppression_dictionary.get(),
                                 false));
    return ret;
  }

  const bool default_expansion_;
  unique_ptr<DataAndPredictor> data_and_predictor_;
  mozc::usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;
};

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorTest) {
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    predictor->WaitForSyncer();

    // Nothing happen
    {
      Segments segments;
      MakeSegmentsForSuggestion("てすと", &segments);
      EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
      EXPECT_EQ(0, segments.segment(0).candidates_size());
    }

    // Nothing happen
    {
      Segments segments;
      MakeSegmentsForPrediction("てすと", &segments);
      EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
      EXPECT_EQ(0, segments.segment(0).candidates_size());
    }

    // Insert two items
    {
      Segments segments;
      MakeSegmentsForConversion("わたしのなまえはなかのです", &segments);
      AddCandidate("私の名前は中野です", &segments);
      predictor->Finish(*convreq_, &segments);

      segments.Clear();
      MakeSegmentsForSuggestion("わたしの", &segments);
      EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
      EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);
      EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
                  Segment::Candidate::USER_HISTORY_PREDICTOR);

      segments.Clear();
      MakeSegmentsForPrediction("わたしの", &segments);
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
        MakeSegmentsForConversion("こんにちはさようなら", &segments);
        AddCandidate("今日はさようなら", &segments);
        predictor->Finish(*convreq_, &segments);

        segments.Clear();
        MakeSegmentsForSuggestion("こんにちは", &segments);
        EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
        MakeSegmentsForPrediction("こんにちは", &segments);
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
    predictor->WaitForSyncer();
    Segments segments;

    // turn off
    {
      Segments segments;
      config_->set_use_history_suggest(false);

      MakeSegmentsForSuggestion("わたしの", &segments);
      EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

      config_->set_use_history_suggest(true);
      config_->set_incognito_mode(true);

      MakeSegmentsForSuggestion("わたしの", &segments);
      EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

      config_->set_incognito_mode(false);
      config_->set_history_learning_level(config::Config::NO_HISTORY);

      MakeSegmentsForSuggestion("わたしの", &segments);
      EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
    }

    // turn on
    { config::ConfigHandler::GetDefaultConfig(config_.get()); }

    // reproducesd
    MakeSegmentsForSuggestion("わたしの", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);

    segments.Clear();
    MakeSegmentsForPrediction("わたしの", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);

    // Exact Match
    segments.Clear();
    MakeSegmentsForSuggestion("わたしのなまえはなかのです", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);

    segments.Clear();
    MakeSegmentsForPrediction("わたしのなまえはなかのです", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);

    segments.Clear();
    MakeSegmentsForSuggestion("こんにちはさようなら", &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

    segments.Clear();
    MakeSegmentsForPrediction("こんにちはさようなら", &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

    // Read only mode should show suggestion.
    {
      config_->set_history_learning_level(config::Config::READ_ONLY);
      MakeSegmentsForSuggestion("わたしの", &segments);
      EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
      EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);

      segments.Clear();
      MakeSegmentsForPrediction("わたしの", &segments);
      EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
      EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);
      config_->set_history_learning_level(config::Config::DEFAULT_HISTORY);
    }

    // clear
    predictor->ClearAllHistory();
    predictor->WaitForSyncer();
  }

  // nothing happen
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    predictor->WaitForSyncer();
    Segments segments;

    // reproducesd
    MakeSegmentsForSuggestion("わたしの", &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

    MakeSegmentsForPrediction("わたしの", &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
  }

  // nothing happen
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    predictor->WaitForSyncer();
    Segments segments;

    // reproducesd
    MakeSegmentsForSuggestion("わたしの", &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

    MakeSegmentsForPrediction("わたしの", &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
  }
}

// We did not support such Segments which has multiple segments and
// has type != CONVERSION.
// To support such Segments, this test case is created separately.
TEST_F(UserHistoryPredictorTest, UserHistoryPredictorTest_suggestion) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  // Register input histories via Finish method.
  {
    Segments segments;
    MakeSegmentsForSuggestion("かまた", &segments);
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
    MakeSegmentsForSuggestion("かま", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    std::set<string> expected_candidates;
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
    predictor->WaitForSyncer();

    // Insert two items
    {
      Segments segments;
      MakeSegmentsForConversion("わたしのなまえはなかのです", &segments);
      AddCandidateWithDescription("私の名前は中野です", kDescription,
                                  &segments);
      predictor->Finish(*convreq_, &segments);

      MakeSegmentsForSuggestion("わたしの", &segments);
      EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
      EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);
      EXPECT_EQ(kDescription, segments.segment(0).candidate(0).description);

      segments.Clear();
      MakeSegmentsForPrediction("わたしの", &segments);
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
    predictor->WaitForSyncer();
    Segments segments;

    // turn off
    {
      Segments segments;
      config_->set_use_history_suggest(false);
      predictor->WaitForSyncer();

      MakeSegmentsForSuggestion("わたしの", &segments);
      EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

      config_->set_use_history_suggest(true);
      config_->set_incognito_mode(true);

      MakeSegmentsForSuggestion("わたしの", &segments);
      EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
    }

    // turn on
    {
      config::ConfigHandler::GetDefaultConfig(config_.get());
      predictor->WaitForSyncer();
    }

    // reproducesd
    MakeSegmentsForSuggestion("わたしの", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);
    EXPECT_EQ(kDescription, segments.segment(0).candidate(0).description);

    segments.Clear();
    MakeSegmentsForPrediction("わたしの", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);
    EXPECT_EQ(kDescription, segments.segment(0).candidate(0).description);

    // Exact Match
    segments.Clear();
    MakeSegmentsForSuggestion("わたしのなまえはなかのです", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);
    EXPECT_EQ(kDescription, segments.segment(0).candidate(0).description);

    segments.Clear();
    MakeSegmentsForSuggestion("わたしのなまえはなかのです", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);
    EXPECT_EQ(kDescription, segments.segment(0).candidate(0).description);

    // clear
    predictor->ClearAllHistory();
    predictor->WaitForSyncer();
  }

  // nothing happen
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    predictor->WaitForSyncer();
    Segments segments;

    // reproducesd
    MakeSegmentsForSuggestion("わたしの", &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

    MakeSegmentsForPrediction("わたしの", &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
  }

  // nothing happen
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    predictor->WaitForSyncer();
    Segments segments;

    // reproducesd
    MakeSegmentsForSuggestion("わたしの", &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

    MakeSegmentsForPrediction("わたしの", &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorUnusedHistoryTest) {
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    predictor->WaitForSyncer();

    Segments segments;
    MakeSegmentsForConversion("わたしのなまえはなかのです", &segments);
    AddCandidate("私の名前は中野です", &segments);

    // once
    segments.set_request_type(Segments::SUGGESTION);
    predictor->Finish(*convreq_, &segments);

    segments.Clear();
    MakeSegmentsForConversion("ひろすえりょうこ", &segments);
    AddCandidate("広末涼子", &segments);

    segments.set_request_type(Segments::CONVERSION);

    // conversion
    predictor->Finish(*convreq_, &segments);

    // sync
    predictor->Sync();
  }

  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    predictor->WaitForSyncer();
    Segments segments;

    MakeSegmentsForSuggestion("わたしの", &segments);
    EXPECT_TRUE(predictor->Predict(&segments));
    EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);

    segments.Clear();
    MakeSegmentsForSuggestion("ひろすえ", &segments);
    EXPECT_TRUE(predictor->Predict(&segments));
    EXPECT_EQ("広末涼子", segments.segment(0).candidate(0).value);

    predictor->ClearUnusedHistory();
    predictor->WaitForSyncer();

    segments.Clear();
    MakeSegmentsForSuggestion("わたしの", &segments);
    EXPECT_TRUE(predictor->Predict(&segments));
    EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);

    segments.Clear();
    MakeSegmentsForSuggestion("ひろすえ", &segments);
    EXPECT_FALSE(predictor->Predict(&segments));

    predictor->Sync();
  }

  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    predictor->WaitForSyncer();
    Segments segments;

    MakeSegmentsForSuggestion("わたしの", &segments);
    EXPECT_TRUE(predictor->Predict(&segments));
    EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);

    segments.Clear();
    MakeSegmentsForSuggestion("ひろすえ", &segments);
    EXPECT_FALSE(predictor->Predict(&segments));
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorRevertTest) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  Segments segments, segments2;
  MakeSegmentsForConversion("わたしのなまえはなかのです", &segments);
  AddCandidate("私の名前は中野です", &segments);

  predictor->Finish(*convreq_, &segments);

  // Before Revert, Suggest works
  MakeSegmentsForSuggestion("わたしの", &segments2);
  EXPECT_TRUE(predictor->Predict(&segments2));
  EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);

  // Call revert here
  predictor->Revert(&segments);

  segments.Clear();
  MakeSegmentsForSuggestion("わたしの", &segments);

  EXPECT_FALSE(predictor->Predict(&segments));
  EXPECT_EQ(0, segments.segment(0).candidates_size());

  EXPECT_FALSE(predictor->Predict(&segments));
  EXPECT_EQ(0, segments.segment(0).candidates_size());
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorClearTest) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();

  // input "testtest" 10 times
  for (int i = 0; i < 10; ++i) {
    Segments segments;
    MakeSegmentsForConversion("testtest", &segments);
    AddCandidate("テストテスト", &segments);
    predictor->Finish(*convreq_, &segments);
  }

  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  // input "testtest" 1 time
  for (int i = 0; i < 1; ++i) {
    Segments segments;
    MakeSegmentsForConversion("testtest", &segments);
    AddCandidate("テストテスト", &segments);
    predictor->Finish(*convreq_, &segments);
  }

  // frequency is cleared as well.
  {
    Segments segments;
    MakeSegmentsForSuggestion("t", &segments);
    EXPECT_FALSE(predictor->Predict(&segments));

    segments.Clear();
    MakeSegmentsForSuggestion("testte", &segments);
    EXPECT_TRUE(predictor->Predict(&segments));
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorTrailingPunctuation) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  Segments segments;

  MakeSegmentsForConversion("わたしのなまえはなかのです", &segments);

  AddCandidate(0, "私の名前は中野です", &segments);

  AddSegmentForConversion("。", &segments);
  AddCandidate(1, "。", &segments);

  predictor->Finish(*convreq_, &segments);

  segments.Clear();
  MakeSegmentsForPrediction("わたしの", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_EQ(2, segments.segment(0).candidates_size());
  EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);
  EXPECT_EQ("私の名前は中野です。", segments.segment(0).candidate(1).value);

  segments.Clear();
  MakeSegmentsForSuggestion("わたしの", &segments);

  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_EQ(2, segments.segment(0).candidates_size());
  EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);
  EXPECT_EQ("私の名前は中野です。", segments.segment(0).candidate(1).value);
}

TEST_F(UserHistoryPredictorTest, TrailingPunctuation_Mobile) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();
  commands::RequestForUnitTest::FillMobileRequest(request_.get());
  Segments segments;

  MakeSegmentsForConversion("です。", &segments);

  AddCandidate(0, "です。", &segments);

  predictor->Finish(*convreq_, &segments);

  segments.Clear();

  MakeSegmentsForPrediction("です", &segments);
  EXPECT_FALSE(predictor->Predict(&segments));
}

TEST_F(UserHistoryPredictorTest, HistoryToPunctuation) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  Segments segments;

  // Scenario 1: A user have commited "亜" by prediction and then commit "。".
  // Then, the unigram "亜" is learned but the bigram "亜。" shouldn't.
  MakeSegmentsForPrediction("あ", &segments);
  AddCandidate(0, "亜", &segments);
  predictor->Finish(*convreq_, &segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  AddSegmentForPrediction("。", &segments);
  AddCandidate(1, "。", &segments);
  predictor->Finish(*convreq_, &segments);

  segments.Clear();
  MakeSegmentsForPrediction("あ", &segments);  // "あ"
  ASSERT_TRUE(predictor->Predict(&segments)) << segments.DebugString();
  EXPECT_EQ("亜", segments.segment(0).candidate(0).value);

  segments.Clear();

  // Scenario 2: the opposite case to Scenario 1, i.e., "。亜".  Nothing is
  // suggested from symbol "。".
  MakeSegmentsForPrediction("。", &segments);
  AddCandidate(0, "。", &segments);
  predictor->Finish(*convreq_, &segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  AddSegmentForPrediction("あ", &segments);
  AddCandidate(1, "亜", &segments);
  predictor->Finish(*convreq_, &segments);

  segments.Clear();
  MakeSegmentsForPrediction("。", &segments);  // "。"
  EXPECT_FALSE(predictor->Predict(&segments)) << segments.DebugString();

  segments.Clear();

  // Scenario 3: If the history segment looks like a sentence and committed
  // value is a punctuation, the concatenated entry is also learned.
  MakeSegmentsForPrediction("おつかれさまです", &segments);
  AddCandidate(0, "お疲れ様です", &segments);
  predictor->Finish(*convreq_, &segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  AddSegmentForPrediction("。", &segments);
  AddCandidate(1, "。", &segments);
  predictor->Finish(*convreq_, &segments);

  segments.Clear();
  MakeSegmentsForPrediction("おつかれ", &segments);
  ASSERT_TRUE(predictor->Predict(&segments)) << segments.DebugString();
  EXPECT_EQ("お疲れ様です", segments.segment(0).candidate(0).value);
  EXPECT_EQ("お疲れ様です。", segments.segment(0).candidate(1).value);
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorPreceedingPunctuation) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  Segments segments;

  MakeSegmentsForConversion("。", &segments);
  AddCandidate(0, "。", &segments);

  AddSegmentForConversion("わたしのなまえはなかのです", &segments);

  AddCandidate(1, "私の名前は中野です", &segments);

  predictor->Finish(*convreq_, &segments);

  segments.Clear();
  MakeSegmentsForPrediction("わたしの", &segments);

  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_EQ(1, segments.segment(0).candidates_size());
  EXPECT_EQ("私の名前は中野です", segments.segment(0).candidate(0).value);

  segments.Clear();
  MakeSegmentsForSuggestion("わたしの", &segments);
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
    predictor->WaitForSyncer();
    predictor->ClearAllHistory();
    predictor->WaitForSyncer();

    Segments segments;
    const string first_char = kTestCases[i].first_character;
    {
      // Learn from two segments
      MakeSegmentsForConversion(first_char, &segments);
      AddCandidate(0, first_char, &segments);
      AddSegmentForConversion("てすとぶんしょう", &segments);
      AddCandidate(1, "テスト文章", &segments);
      predictor->Finish(*convreq_, &segments);
    }
    segments.Clear();
    {
      // Learn from one segment
      MakeSegmentsForConversion(first_char + "てすとぶんしょう", &segments);
      AddCandidate(0, first_char + "テスト文章", &segments);
      predictor->Finish(*convreq_, &segments);
    }
    segments.Clear();
    {
      // Suggestion
      MakeSegmentsForSuggestion(first_char, &segments);
      AddCandidate(0, first_char, &segments);
      EXPECT_EQ(kTestCases[i].expected_result, predictor->Predict(&segments))
          << "Suggest from " << first_char;
    }
    segments.Clear();
    {
      // Prediciton
      MakeSegmentsForPrediction(first_char, &segments);
      EXPECT_EQ(kTestCases[i].expected_result, predictor->Predict(&segments))
          << "Predict from " << first_char;
    }
  }
}

TEST_F(UserHistoryPredictorTest, ZeroQuerySuggestionTest) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  request_->set_zero_query_suggestion(true);

  commands::Request non_zero_query_request;
  non_zero_query_request.set_zero_query_suggestion(false);
  ConversionRequest non_zero_query_conversion_request(
      composer_.get(), &non_zero_query_request, config_.get());

  Segments segments;

  // No history segments
  segments.Clear();
  MakeSegmentsForSuggestion("", &segments);
  EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

  {
    segments.Clear();

    MakeSegmentsForConversion("たろうは", &segments);
    AddCandidate(0, "太郎は", &segments);
    predictor->Finish(*convreq_, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    AddSegmentForConversion("はなこに", &segments);
    AddCandidate(1, "花子に", &segments);
    predictor->Finish(*convreq_, &segments);
    segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);

    segments.pop_back_segment();
    AddSegmentForConversion("きょうと", &segments);
    AddCandidate(1, "京都", &segments);
    Util::Sleep(2000);
    predictor->Finish(*convreq_, &segments);
    segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);

    segments.pop_back_segment();
    AddSegmentForConversion("おおさか", &segments);
    AddCandidate(1, "大阪", &segments);
    Util::Sleep(2000);
    predictor->Finish(*convreq_, &segments);
    segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);

    // Zero query suggestion is disabled.
    segments.pop_back_segment();
    AddSegmentForSuggestion("", &segments);  // empty request
    EXPECT_FALSE(predictor->PredictForRequest(non_zero_query_conversion_request,
                                              &segments));

    segments.pop_back_segment();
    AddSegmentForSuggestion("", &segments);  // empty request
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    // last-pushed segment is "大阪"
    EXPECT_EQ("大阪", segments.segment(1).candidate(0).value);
    EXPECT_EQ("おおさか", segments.segment(1).candidate(0).key);
    EXPECT_TRUE(segments.segment(1).candidate(0).source_info &
                Segment::Candidate::USER_HISTORY_PREDICTOR);

    segments.pop_back_segment();
    AddSegmentForSuggestion("は", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));

    segments.pop_back_segment();
    AddSegmentForSuggestion("た", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));

    segments.pop_back_segment();
    AddSegmentForSuggestion("き", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));

    segments.pop_back_segment();
    AddSegmentForSuggestion("お", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  }

  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  {
    segments.Clear();
    MakeSegmentsForConversion("たろうは", &segments);
    AddCandidate(0, "太郎は", &segments);

    AddSegmentForConversion("はなこに", &segments);
    AddCandidate(1, "花子に", &segments);
    predictor->Finish(*convreq_, &segments);

    segments.Clear();
    MakeSegmentsForConversion("たろうは", &segments);
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
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  Segments segments;

  MakeSegmentsForConversion("たろうは", &segments);
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
  MakeSegmentsForSuggestion("た", &segments);
  EXPECT_FALSE(predictor->Predict(&segments));

  segments.Clear();
  MakeSegmentsForSuggestion("たろうは", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  segments.Clear();
  MakeSegmentsForSuggestion("ろうは", &segments);
  EXPECT_FALSE(predictor->Predict(&segments));

  segments.Clear();
  MakeSegmentsForSuggestion("たろうははな", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  segments.Clear();
  MakeSegmentsForSuggestion("はなこにむ", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  segments.Clear();
  MakeSegmentsForSuggestion("むずかし", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  segments.Clear();
  MakeSegmentsForSuggestion("はなこにむずかしいほ", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  segments.Clear();
  MakeSegmentsForSuggestion("ほんをよま", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  Util::Sleep(1000);

  // Add new entry "たろうはよしこに/太郎は良子に"
  segments.Clear();
  MakeSegmentsForConversion("たろうは", &segments);
  AddCandidate(0, "太郎は", &segments);
  predictor->Finish(*convreq_, &segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  AddSegmentForConversion("よしこに", &segments);
  AddCandidate(1, "良子に", &segments);
  predictor->Finish(*convreq_, &segments);
  segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);

  segments.Clear();
  MakeSegmentsForSuggestion("たろうは", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_EQ("太郎は良子に", segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
}

TEST_F(UserHistoryPredictorTest, MultiSegmentsSingleInput) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  Segments segments;

  MakeSegmentsForConversion("たろうは", &segments);
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
  MakeSegmentsForSuggestion("たろうは", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  segments.Clear();
  MakeSegmentsForSuggestion("た", &segments);
  EXPECT_FALSE(predictor->Predict(&segments));

  segments.Clear();
  MakeSegmentsForSuggestion("たろうははな", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  segments.Clear();
  MakeSegmentsForSuggestion("ろうははな", &segments);
  EXPECT_FALSE(predictor->Predict(&segments));

  segments.Clear();
  MakeSegmentsForSuggestion("はなこにむ", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  segments.Clear();
  MakeSegmentsForSuggestion("むずかし", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  segments.Clear();
  MakeSegmentsForSuggestion("はなこにむずかしいほ", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  segments.Clear();
  MakeSegmentsForSuggestion("ほんをよま", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  Util::Sleep(1000);

  // Add new entry "たろうはよしこに/太郎は良子に"
  segments.Clear();
  MakeSegmentsForConversion("たろうは", &segments);
  AddCandidate(0, "太郎は", &segments);
  predictor->Finish(*convreq_, &segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  AddSegmentForConversion("よしこに", &segments);
  AddCandidate(1, "良子に", &segments);
  predictor->Finish(*convreq_, &segments);
  segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);

  segments.Clear();
  MakeSegmentsForSuggestion("たろうは", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_EQ("太郎は良子に", segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
}

TEST_F(UserHistoryPredictorTest, Regression2843371_Case1) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  Segments segments;

  MakeSegmentsForConversion("とうきょうは", &segments);
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

  MakeSegmentsForConversion("らーめんは", &segments);
  AddCandidate(0, "ラーメンは", &segments);

  AddSegmentForConversion("、", &segments);
  AddCandidate(1, "、", &segments);

  AddSegmentForConversion("めんるいです", &segments);
  AddCandidate(2, "麺類です", &segments);

  AddSegmentForConversion("。", &segments);
  AddCandidate(3, "。", &segments);

  predictor->Finish(*convreq_, &segments);

  segments.Clear();

  MakeSegmentsForSuggestion("とうきょうは、", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  EXPECT_EQ("東京は、日本です", segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
}

TEST_F(UserHistoryPredictorTest, Regression2843371_Case2) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  Segments segments;

  MakeSegmentsForConversion("えど", &segments);
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

  MakeSegmentsForSuggestion("えど(", &segments);
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
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  Segments segments;

  MakeSegmentsForConversion("「", &segments);
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

  MakeSegmentsForConversion("「", &segments);
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

  MakeSegmentsForSuggestion("「やま」は", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  EXPECT_EQ("「山」は高い", segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
}

TEST_F(UserHistoryPredictorTest, Regression2843775) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  Segments segments;

  MakeSegmentsForConversion("そうです", &segments);
  AddCandidate(0, "そうです", &segments);

  AddSegmentForConversion("。よろしくおねがいします", &segments);
  AddCandidate(1, "。よろしくお願いします", &segments);

  predictor->Finish(*convreq_, &segments);

  segments.Clear();

  MakeSegmentsForSuggestion("そうです", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  EXPECT_EQ("そうです。よろしくお願いします",
            segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
}

TEST_F(UserHistoryPredictorTest, DuplicateString) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  Segments segments;

  MakeSegmentsForConversion("らいおん", &segments);
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

  MakeSegmentsForSuggestion("ぞうりむし", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  for (int i = 0; i < segments.segment(0).candidates_size(); ++i) {
    EXPECT_EQ(string::npos,
              segments.segment(0).candidate(i).value.find(
                  "猛獣"));  // "猛獣" should not be found
  }

  segments.Clear();

  MakeSegmentsForSuggestion("らいおん", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  for (int i = 0; i < segments.segment(0).candidates_size(); ++i) {
    EXPECT_EQ(string::npos,
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
  string key;
  string value;
  Command() : type(LOOKUP) {}
};

TEST_F(UserHistoryPredictorTest, SyncTest) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();

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
        predictor->WaitForSyncer();
        break;
      case Command::INSERT:
        segments.Clear();
        MakeSegmentsForConversion(commands[i].key, &segments);
        AddCandidate(commands[i].value, &segments);
        predictor->Finish(*convreq_, &segments);
        break;
      case Command::LOOKUP:
        segments.Clear();
        MakeSegmentsForSuggestion(commands[i].key, &segments);
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

string RemoveLastUCS4Character(const string &input) {
  const size_t ucs4_count = Util::CharsLen(input);
  if (ucs4_count == 0) {
    return "";
  }

  size_t ucs4_processed = 0;
  string output;
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
  {
    kNonSensitive,  // We might want to revisit this behavior
    "Type privacy sensitive number but it is commited as full-width number "
    "by mistake.",
    "0007",
    "０００７"
  }, {
    kNonSensitive,
    "Type a ZIP number.",
    "100-0001",
    "東京都千代田区千代田"
  }, {
    kNonSensitive,  // We might want to revisit this behavior
    "Type privacy sensitive number but the result contains one or more "
    "non-ASCII character such as full-width dash.",
    "1111-1111",
    "1111－1111"
  }, {
    kNonSensitive,  // We might want to revisit this behavior
    "User dictionary contains a credit card number.",
    "かーどばんごう",
    "0000-0000-0000-0000"
  }, {
    kNonSensitive,  // We might want to revisit this behavior
    "User dictionary contains a credit card number.",
    "かーどばんごう",
    "0000000000000000"
  }, {
    kNonSensitive,  // We might want to revisit this behavior
    "User dictionary contains privacy sensitive information.",
    "ぱすわーど",
    "ywwz1sxm"
  }, {
    kNonSensitive,  // We might want to revisit this behavior
    "Input privacy sensitive text by Roman-input mode by mistake and then "
    "hit F10 key to convert it to half-alphanumeric text. In this case "
    "we assume all the alphabetical characters are consumed by Roman-input "
    "rules.",
    "いあ1ぼ3ぅ",
    "ia1bo3xu"
  }, {
    kNonSensitive,
    "Katakana to English transliteration.",  // http://b/4394325
    "おれんじ",
    "Orange"
  }, {
    kNonSensitive,
    "Input a very common English word which should be included in our "
    "system dictionary by Roman-input mode by mistake and "
    "then hit F10 key to convert it to half-alphanumeric text.",
    "おらんげ",
    "orange"
  }, {
    kSensitive,
    "Input a password-like text.",
    "123abc!",
    "123abc!",
  }, {
    kSensitive,
    "Input privacy sensitive text by Roman-input mode by mistake and then "
    "hit F10 key to convert it to half-alphanumeric text. In this case, "
    "there may remain one or more alphabetical characters, which have not "
    "been consumed by Roman-input rules.",
    "yっwz1sxm",
    "ywwz1sxm"
  }, {
    kNonSensitive,
    "Type a very common English word all in lower case which should be "
    "included in our system dictionary without capitalization.",
    "variable",
    "variable"
  }, {
    kNonSensitive,
    "Type a very common English word all in upper case whose lower case "
    "should be included in our system dictionary.",
    "VARIABLE",
    "VARIABLE"
  }, {
    kNonSensitive,
    "Type a very common English word with capitalization whose lower case "
    "should be included in our system dictionary.",
    "Variable",
    "Variable"
  }, {
    kSensitive,  // We might want to revisit this behavior
    "Type a very common English word with random capitalization, which "
    "should be treated as case SENSITIVE.",
    "vArIaBle",
    "vArIaBle"
  }, {
    kSensitive,
    "Type an English word in lower case but only its upper case form is "
    "stored in dictionary.",
    "upper",
    "upper",
  }, {
    kSensitive,  // We might want to revisit this behavior
    "Type just a number.",
    "2398402938402934",
    "2398402938402934"
  }, {
    kSensitive,  // We might want to revisit this behavior
    "Type an common English word which might be included in our system "
    "dictionary with number postfix.",
    "Orange10000",
    "Orange10000"
  },
};

}  // namespace

TEST_F(UserHistoryPredictorTest, PrivacySensitiveTest) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();

  // Add those words to the mock dictionary that are assumed to exist in privacy
  // sensitive filtering.
  const char *kEnglishWords[] = {
    "variable", "UPPER",
  };
  for (size_t i = 0; i < arraysize(kEnglishWords); ++i) {
    // LookupPredictive is used in UserHistoryPredictor::IsPrivacySensitive().
    GetDictionaryMock()->AddLookupExact(
        kEnglishWords[i], kEnglishWords[i], kEnglishWords[i], Token::NONE);
  }

  for (size_t i = 0; i < arraysize(kNonSensitiveCases); ++i) {
    predictor->ClearAllHistory();
    predictor->WaitForSyncer();

    const PrivacySensitiveTestData &data = kNonSensitiveCases[i];
    const string description(data.scenario_description);
    const string input(data.input);
    const string output(data.output);
    const string &partial_input = RemoveLastUCS4Character(input);
    const bool expect_sensitive = data.is_sensitive;

    // Initial commit.
    {
      Segments segments;
      MakeSegmentsForConversion(input, &segments);
      AddCandidate(0, output, &segments);
      predictor->Finish(*convreq_, &segments);
    }

    // TODO(yukawa): Refactor the scenario runner below by making
    //     some utility functions.

    // Check suggestion
    {
      Segments segments;
      MakeSegmentsForSuggestion(partial_input, &segments);
      if (expect_sensitive) {
        EXPECT_FALSE(predictor->Predict(&segments))
          << description << " input: " << input << " output: " << output;
      } else {
        EXPECT_TRUE(predictor->Predict(&segments))
          << description << " input: " << input << " output: " << output;
      }
      segments.Clear();
      MakeSegmentsForPrediction(input, &segments);
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
      MakeSegmentsForPrediction(partial_input, &segments);
      if (expect_sensitive) {
        EXPECT_FALSE(predictor->Predict(&segments))
          << description << " input: " << input << " output: " << output;
      } else {
        EXPECT_TRUE(predictor->Predict(&segments))
          << description << " input: " << input << " output: " << output;
      }
      segments.Clear();
      MakeSegmentsForPrediction(input, &segments);
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
  predictor->WaitForSyncer();

  // If a password-like input consists of multiple segments, it is not
  // considered to be privacy sensitive when the input is committed.
  // Currently this is a known issue.
  {
    Segments segments;
    MakeSegmentsForConversion("123", &segments);
    AddSegmentForConversion("abc!", &segments);
    AddCandidate(0, "123", &segments);
    AddCandidate(1, "abc!", &segments);
    predictor->Finish(*convreq_, &segments);
  }

  {
    Segments segments;
    MakeSegmentsForSuggestion("123abc", &segments);
    EXPECT_TRUE(predictor->Predict(&segments));
    segments.Clear();
    MakeSegmentsForSuggestion("123abc!", &segments);
    EXPECT_TRUE(predictor->Predict(&segments));
  }

  {
    Segments segments;
    MakeSegmentsForPrediction("123abc", &segments);
    EXPECT_TRUE(predictor->Predict(&segments));
    segments.Clear();
    MakeSegmentsForPrediction("123abc!", &segments);
    EXPECT_TRUE(predictor->Predict(&segments));
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryStorage) {
  const string filename =
      FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), "test");

  UserHistoryStorage storage1(filename);

  UserHistoryPredictor::Entry *entry = storage1.add_entries();
  CHECK(entry);
  entry->set_key("key");
  entry->set_key("value");
  storage1.Save();
  UserHistoryStorage storage2(filename);
  storage2.Load();

  EXPECT_EQ(storage1.DebugString(), storage2.DebugString());
  FileUtil::Unlink(filename);
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
  EXPECT_TRUE(UserHistoryPredictor::RomanFuzzyPrefixMatch("fooabfoo",
                                                          "foobafoo"));

  // swap + prefix
  EXPECT_TRUE(UserHistoryPredictor::RomanFuzzyPrefixMatch("fooabfoo",
                                                          "fooba"));

  // deletion
  EXPECT_TRUE(UserHistoryPredictor::RomanFuzzyPrefixMatch("abcd", "acd"));
  EXPECT_TRUE(UserHistoryPredictor::RomanFuzzyPrefixMatch("abcd", "bcd"));

  // deletion + prefix
  EXPECT_TRUE(UserHistoryPredictor::RomanFuzzyPrefixMatch("abcdf",   "acd"));
  EXPECT_TRUE(UserHistoryPredictor::RomanFuzzyPrefixMatch("abcdfoo", "bcd"));

  // voice sound mark
  EXPECT_TRUE(UserHistoryPredictor::RomanFuzzyPrefixMatch("gu-guru",
                                                          "gu^guru"));
  EXPECT_TRUE(UserHistoryPredictor::RomanFuzzyPrefixMatch("gu-guru",
                                                          "gu=guru"));
  EXPECT_TRUE(UserHistoryPredictor::RomanFuzzyPrefixMatch("gu-guru",
                                                          "gu^gu"));
  EXPECT_FALSE(UserHistoryPredictor::RomanFuzzyPrefixMatch("gu-guru",
                                                           "gugu"));

  // Invalid
  EXPECT_FALSE(UserHistoryPredictor::RomanFuzzyPrefixMatch("", ""));
  EXPECT_FALSE(UserHistoryPredictor::RomanFuzzyPrefixMatch("", "a"));
  EXPECT_FALSE(UserHistoryPredictor::RomanFuzzyPrefixMatch("abcde",
                                                           "defe"));
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
  const string entry_key;
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
  unique_ptr<Trie<string>> expanded(new Trie<string>);
  expanded->AddEntry("か", "");
  expanded->AddEntry("き", "");
  expanded->AddEntry("く", "");
  expanded->AddEntry("け", "");
  expanded->AddEntry("こ", "");

  const LookupTestData kTests1[] = {
    { "", false },
    { "あか", true },
    { "あき", true },
    { "あかい", true },
    { "あまい", false },
    { "あ", false },
    { "さか", false },
    { "さき", false },
    { "さかい", false },
    { "さまい", false },
    { "さ", false },
  };

  // with expanded
  for (size_t i = 0; i < arraysize(kTests1); ++i) {
    entry.set_key(kTests1[i].entry_key);
    EXPECT_EQ(kTests1[i].expect_result, predictor->LookupEntry(
        UserHistoryPredictor::DEFAULT,
        "あｋ", "あ",
        expanded.get(), &entry, nullptr, &results))
        << kTests1[i].entry_key;
  }

  // only expanded
  // preedit: "k"
  // input_key: ""
  // key_base: ""
  // key_expanded: "か","き","く","け", "こ"

  const LookupTestData kTests2[] = {
    { "", false },
    { "か", true },
    { "き", true },
    { "かい", true },
    { "まい", false },
    { "も", false },
  };

  for (size_t i = 0; i < arraysize(kTests2); ++i) {
    entry.set_key(kTests2[i].entry_key);
    EXPECT_EQ(kTests2[i].expect_result, predictor->LookupEntry(
        UserHistoryPredictor::DEFAULT,
        "", "", expanded.get(), &entry, nullptr, &results))
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
  unique_ptr<Trie<string>> expanded(new Trie<string>);
  expanded->AddEntry("し", "");
  expanded->AddEntry("じ", "");

  const LookupTestData kTests1[] = {
      { "", false },
      { "あ", false },
      { "あし", true },
      { "あじ", true },
      { "あしかゆい", true },
      { "あじうまい", true },
      { "あまにがい", false },
      { "あめ", false },
      { "まし", false },
      { "まじ", false },
      { "ましなあじ", false },
      { "まじうまい", false },
      { "ままにがい", false },
      { "まめ", false },
  };

  // with expanded
  for (size_t i = 0; i < arraysize(kTests1); ++i) {
    entry.set_key(kTests1[i].entry_key);
    EXPECT_EQ(kTests1[i].expect_result, predictor->LookupEntry(
        UserHistoryPredictor::DEFAULT,
        "あし", "あ",
        expanded.get(), &entry, nullptr, &results))
        << kTests1[i].entry_key;
  }

  // only expanded
  // input_key: "し"
  // key_base: ""
  // key_expanded: "し","じ"
  const LookupTestData kTests2[] = {
      { "", false },
      { "し", true },
      { "じ", true },
      { "しかうまい", true },
      { "じゅうかい", true },
      { "ま", false },
      { "まめ", false },
  };

  for (size_t i = 0; i < arraysize(kTests2); ++i) {
    entry.set_key(kTests2[i].entry_key);
    EXPECT_EQ(kTests2[i].expect_result, predictor->LookupEntry(
        UserHistoryPredictor::DEFAULT,
        "し", "", expanded.get(), &entry, nullptr, &results))
        << kTests2[i].entry_key;
  }
}

TEST_F(UserHistoryPredictorTest, GetMatchTypeFromInputRoman) {
  // We have to define this here,
  // because UserHistoryPredictor::MatchType is private
  struct MatchTypeTestData {
    const string target;
    const UserHistoryPredictor::MatchType expect_type;
  };

  // Roman
  // preedit: "あｋ"
  // input_key: "あ"
  // key_base: "あ"
  // key_expanded: "か","き","く","け", "こ"
  unique_ptr<Trie<string>> expanded(new Trie<string>);
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
                  "あ", "あ",
                  expanded.get(), kTests1[i].target))
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
    const string target;
    const UserHistoryPredictor::MatchType expect_type;
  };

  // Kana
  // preedit: "あし"
  // input_key: "あし"
  // key_base: "あ"
  // key_expanded: "し","じ"
  unique_ptr<Trie<string>> expanded(new Trie<string>);
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
                  "あし", "あ",
                  expanded.get(), kTests1[i].target))
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
void InitSegmentsFromInputSequence(const string &text,
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
      key.set_key_string(string(begin, mblen));
    }
    begin += mblen;
    composer->InsertCharacterKeyEvent(key);
  }

  request->set_composer(composer);

  segments->set_request_type(Segments::PREDICTION);
  Segment *segment = segments->add_segment();
  CHECK(segment);
  string query;
  composer->GetQueryForPrediction(&query);
  segment->set_key(query);
}
}  // namespace

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsRoman) {
  table_->LoadFromFile("system://romanji-hiragana.tsv");
  composer_->SetTable(table_.get());
  Segments segments;

  InitSegmentsFromInputSequence("gu-g",
                                composer_.get(),
                                convreq_.get(),
                                &segments);

  {
    FLAGS_enable_expansion_for_user_history_predictor = true;
    string input_key;
    string base;
    unique_ptr<Trie<string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*convreq_,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
    EXPECT_EQ("ぐーｇ", input_key);
    EXPECT_EQ("ぐー", base);
    EXPECT_TRUE(expanded != nullptr);
    string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_TRUE(
        expanded->LookUpPrefix("ぐ", &value, &key_length, &has_subtrie));
    EXPECT_EQ("ぐ", value);
  }

  {
    FLAGS_enable_expansion_for_user_history_predictor = false;
    string input_key;
    string base;
    unique_ptr<Trie<string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*convreq_,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
    EXPECT_EQ("ぐー", input_key);
    EXPECT_EQ("ぐー", base);
    EXPECT_TRUE(expanded == nullptr);
  }
}

namespace {
uint32 GetRandomAscii() {
  return static_cast<uint32>(' ') +
      Util::Random(static_cast<uint32>('~' - ' '));
}
}  // namespace

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsRomanRandom) {
  FLAGS_enable_expansion_for_user_history_predictor = true;
  table_->LoadFromFile("system://romanji-hiragana.tsv");
  composer_->SetTable(table_.get());
  Segments segments;

  for (size_t i = 0; i < 1000; ++i) {
    composer_->Reset();
    const int len = 1 + Util::Random(4);
    DCHECK_GE(len, 1);
    DCHECK_LE(len, 5);
    string input;
    for (size_t j = 0; j < len; ++j) {
      input += GetRandomAscii();
    }
    InitSegmentsFromInputSequence(input,
                                  composer_.get(),
                                  convreq_.get(),
                                  &segments);
    string input_key;
    string base;
    unique_ptr<Trie<string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*convreq_,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
  }
}

// Found by random test.
// input_key != base by compoesr modification.
TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsShouldNotCrash) {
  FLAGS_enable_expansion_for_user_history_predictor = true;
  table_->LoadFromFile("system://romanji-hiragana.tsv");
  composer_->SetTable(table_.get());
  Segments segments;

  {
    InitSegmentsFromInputSequence("8,+",
                                  composer_.get(),
                                  convreq_.get(),
                                  &segments);
    string input_key;
    string base;
    unique_ptr<Trie<string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*convreq_,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
  }
}

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsRomanN) {
  FLAGS_enable_expansion_for_user_history_predictor = true;
  table_->LoadFromFile("system://romanji-hiragana.tsv");
  composer_->SetTable(table_.get());
  Segments segments;

  {
    InitSegmentsFromInputSequence(
        "n", composer_.get(), convreq_.get(), &segments);
    string input_key;
    string base;
    unique_ptr<Trie<string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*convreq_,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
    EXPECT_EQ("ｎ", input_key);
    EXPECT_EQ("", base);
    EXPECT_TRUE(expanded != nullptr);
    string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_TRUE(
        expanded->LookUpPrefix("な", &value, &key_length, &has_subtrie));
    EXPECT_EQ("な", value);
  }

  composer_->Reset();
  segments.Clear();
  {
    InitSegmentsFromInputSequence(
        "nn", composer_.get(), convreq_.get(), &segments);
    string input_key;
    string base;
    unique_ptr<Trie<string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*convreq_,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
    EXPECT_EQ("ん", input_key);
    EXPECT_EQ("ん", base);
    EXPECT_TRUE(expanded == nullptr);
  }

  composer_->Reset();
  segments.Clear();
  {
    InitSegmentsFromInputSequence("n'", composer_.get(),
                                  convreq_.get(), &segments);
    string input_key;
    string base;
    unique_ptr<Trie<string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*convreq_,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
    EXPECT_EQ("ん", input_key);
    EXPECT_EQ("ん", base);
    EXPECT_TRUE(expanded == nullptr);
  }

  composer_->Reset();
  segments.Clear();
  {
    InitSegmentsFromInputSequence("n'n", composer_.get(),
                                  convreq_.get(), &segments);
    string input_key;
    string base;
    unique_ptr<Trie<string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*convreq_,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
    EXPECT_EQ("んｎ", input_key);
    EXPECT_EQ("ん", base);
    EXPECT_TRUE(expanded != nullptr);
    string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_TRUE(
        expanded->LookUpPrefix("な",
                               &value, &key_length, &has_subtrie));
    EXPECT_EQ("な", value);
  }
}

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsFlickN) {
  FLAGS_enable_expansion_for_user_history_predictor = true;
  table_->LoadFromFile("system://flick-hiragana.tsv");
  composer_->SetTable(table_.get());
  Segments segments;

  {
    InitSegmentsFromInputSequence("/", composer_.get(), convreq_.get(),
                                  &segments);
    string input_key;
    string base;
    unique_ptr<Trie<string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*convreq_,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
    EXPECT_EQ("ん", input_key);
    EXPECT_EQ("", base);
    EXPECT_TRUE(expanded != nullptr);
    string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_TRUE(
        expanded->LookUpPrefix("ん", &value, &key_length, &has_subtrie));
    EXPECT_EQ("ん", value);
  }
}

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegments12KeyN) {
  FLAGS_enable_expansion_for_user_history_predictor = true;
  table_->LoadFromFile("system://12keys-hiragana.tsv");
  composer_->SetTable(table_.get());
  Segments segments;

  {
    InitSegmentsFromInputSequence("わ00",
                                  composer_.get(),
                                  convreq_.get(),
                                  &segments);
    string input_key;
    string base;
    unique_ptr<Trie<string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*convreq_,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
    EXPECT_EQ("ん", input_key);
    EXPECT_EQ("", base);
    EXPECT_TRUE(expanded != nullptr);
    string value;
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

  InitSegmentsFromInputSequence("あか",
                                composer_.get(), convreq_.get(), &segments);

  {
    FLAGS_enable_expansion_for_user_history_predictor = true;
    string input_key;
    string base;
    unique_ptr<Trie<string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*convreq_,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
    EXPECT_EQ("あか", input_key);
    EXPECT_EQ("あ", base);
    EXPECT_TRUE(expanded != nullptr);
    string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_TRUE(
        expanded->LookUpPrefix("が",
                               &value, &key_length, &has_subtrie));
    EXPECT_EQ("が", value);
  }

  {
    FLAGS_enable_expansion_for_user_history_predictor = false;
    string input_key;
    string base;
    unique_ptr<Trie<string>> expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*convreq_,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
    EXPECT_EQ("あか", input_key);
    EXPECT_EQ("あか", base);
    EXPECT_TRUE(expanded == nullptr);
  }
}

TEST_F(UserHistoryPredictorTest, RealtimeConversionInnerSegment) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  Segments segments;
  {
    const char kKey[] = "わたしのなまえはなかのです";
    const char kValue[] = "私の名前は中野です";
    MakeSegmentsForPrediction(kKey, &segments);
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

  MakeSegmentsForPrediction("なかの", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_TRUE(FindCandidateByValue("中野です", segments));

  segments.Clear();
  MakeSegmentsForPrediction("なまえ", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_TRUE(FindCandidateByValue("名前は", segments));
  EXPECT_TRUE(FindCandidateByValue("名前は中野です", segments));
}

TEST_F(UserHistoryPredictorTest, ZeroQueryFromRealtimeConversion) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  Segments segments;
  {
    const char kKey[] = "わたしのなまえはなかのです";
    const char kValue[] = "私の名前は中野です";
    MakeSegmentsForPrediction(kKey, &segments);
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

  MakeSegmentsForConversion("わたしの", &segments);
  AddCandidate(0, "私の", &segments);
  predictor->Finish(*convreq_, &segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  AddSegmentForSuggestion("", &segments);  // empty request
  commands::Request request;
  request_->set_zero_query_suggestion(true);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_TRUE(FindCandidateByValue("名前は", segments));
}

TEST_F(UserHistoryPredictorTest, LongCandidateForMobile) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  Segments segments;
  for (size_t i = 0; i < 3; ++i) {
    const char kKey[] = "よろしくおねがいします";
    const char kValue[] = "よろしくお願いします";
    MakeSegmentsForPrediction(kKey, &segments);
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

  MakeSegmentsForPrediction("よろ", &segments);
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
    std::vector<StringPiece> dummy1, dummy2;
    EXPECT_EQ(UserHistoryPredictor::NOT_FOUND,
              predictor->RemoveNgramChain("hoge", "HOGE", entries[i],
                                          &dummy1, 0, &dummy2, 0));
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
    std::vector<StringPiece> dummy1, dummy2;
    EXPECT_EQ(UserHistoryPredictor::DONE,
              predictor->RemoveNgramChain("abc", "ABC", a,
                                          &dummy1, 0, &dummy2, 0));
    for (size_t i = 0; i < entries.size(); ++i) {
      EXPECT_FALSE(entries[i]->removed());
    }
    EXPECT_TRUE(IsConnected(*a, *b));
    EXPECT_FALSE(IsConnected(*b, *c));
  }
  {
    // Try deleting the chain for "a". Since this is the head of the chain, the
    // function returns TAIL and nothing should be removed.
    std::vector<StringPiece> dummy1, dummy2;
    EXPECT_EQ(UserHistoryPredictor::TAIL,
              predictor->RemoveNgramChain("a", "A", a,
                                          &dummy1, 0, &dummy2, 0));
    for (size_t i = 0; i < entries.size(); ++i) {
      EXPECT_FALSE(entries[i]->removed());
    }
    EXPECT_TRUE(IsConnected(*a, *b));
    EXPECT_FALSE(IsConnected(*b, *c));
  }
  {
    // Further delete the chain for "ab".  Now all the links should be removed.
    std::vector<StringPiece> dummy1, dummy2;
    EXPECT_EQ(UserHistoryPredictor::DONE,
              predictor->RemoveNgramChain("ab", "AB", a,
                                          &dummy1, 0, &dummy2, 0));
    for (size_t i = 0; i < entries.size(); ++i) {
      EXPECT_FALSE(entries[i]->removed());
    }
    EXPECT_FALSE(IsConnected(*a, *b));
    EXPECT_FALSE(IsConnected(*b, *c));
  }
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntry_Unigram) {
  // Tests ClearHistoryEntry() for unigram history.
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Add a unigram history ("japanese", "Japanese").
  UserHistoryPredictor::Entry *e =
      InsertEntry(predictor, "japanese", "Japanese");

  // "Japanese" should be suggested and predicted from "japan".
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));

  // Delete the history.
  EXPECT_TRUE(predictor->ClearHistoryEntry("japanese", "Japanese"));

  EXPECT_TRUE(e->removed());

  // "Japanese" should be never be suggested nor predicted.
  const string key = "japanese";
  for (size_t i = 0; i < key.size(); ++i) {
    const string &prefix = key.substr(0, i);
    EXPECT_FALSE(IsSuggested(predictor, prefix, "Japanese"));
    EXPECT_FALSE(IsPredicted(predictor, prefix, "Japanese"));
  }
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntry_Bigram_DeleteWhole) {
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
  const string key = "japaneseinput";
  for (size_t i = 0; i < key.size(); ++i) {
    const string &prefix = key.substr(0, i);
    EXPECT_FALSE(IsSuggested(predictor, prefix, "Japaneseinput"));
    EXPECT_FALSE(IsPredicted(predictor, prefix, "Japaneseinput"));
  }

  // However, predictor should show "Japanese" and "Input".
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "Input"));
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntry_Bigram_DeleteFirst) {
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
  const string key = "japaneseinput";
  for (size_t i = 0; i < key.size(); ++i) {
    const string &prefix = key.substr(0, i);
    EXPECT_FALSE(IsSuggested(predictor, prefix, "Japanese"));
    EXPECT_FALSE(IsPredicted(predictor, prefix, "Japanese"));
  }

  // However, predictor should show "JapaneseInput" and "Input".
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "Input"));
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntry_Bigram_DeleteSecond) {
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
  const string key = "input";
  for (size_t i = 0; i < key.size(); ++i) {
    const string &prefix = key.substr(0, i);
    EXPECT_FALSE(IsSuggested(predictor, prefix, "Input"));
    EXPECT_FALSE(IsPredicted(predictor, prefix, "Input"));
  }

  // However, predictor should show "Japanese" and "JapaneseInput".
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntry_Trigram_DeleteWhole) {
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
  InitHistory_JapaneseInputMethod(predictor, &japaneseinputmethod,
                                  &japanese, &input, &method);

  // Delete the history of the whole sentence.
  EXPECT_TRUE(predictor->ClearHistoryEntry(
      "japaneseinputmethod", "JapaneseInputMethod"));

  // Note that only the link from "input" to "method" was removed.
  EXPECT_TRUE(japaneseinputmethod->removed());
  EXPECT_FALSE(japanese->removed());
  EXPECT_FALSE(input->removed());
  EXPECT_FALSE(method->removed());
  EXPECT_TRUE(IsConnected(*japanese, *input));
  EXPECT_FALSE(IsConnected(*input, *method));

  {
    // Now "JapaneseInputMethod" should never be suggested nor predicted.
    const string key = "japaneseinputmethod";
    for (size_t i = 0; i < key.size(); ++i) {
      const string &prefix = key.substr(0, i);
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
    const string key = "inputmethod";
    for (size_t i = 0; i < key.size(); ++i) {
      const string &prefix = key.substr(0, i);
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
  // Tests ClearHistoryEntry() for trigram history.  This case tests the
  // deletion of the first node of trigram.
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the same history structure as ClearHistoryEntry_Trigram_DeleteWhole.
  UserHistoryPredictor::Entry *japaneseinputmethod;
  UserHistoryPredictor::Entry *japanese;
  UserHistoryPredictor::Entry *input;
  UserHistoryPredictor::Entry *method;
  InitHistory_JapaneseInputMethod(predictor, &japaneseinputmethod,
                                  &japanese, &input, &method);

  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor,
                                      "japan", "JapaneseInputMethod"));
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
    const string key = "japaneseinputmethod";
    for (size_t i = 0; i < key.size(); ++i) {
      const string &prefix = key.substr(0, i);
      EXPECT_FALSE(IsSuggested(predictor, prefix, "Japanese"));
      EXPECT_FALSE(IsPredicted(predictor, prefix, "Japanese"));
    }
  }

  // The following are still suggested and predicted.
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor,
                                      "japan", "JapaneseInputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "Input"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "InputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "meth", "Method"));
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntry_Trigram_DeleteSecond) {
  // Tests ClearHistoryEntry() for trigram history.  This case tests the
  // deletion of the second node of trigram.
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the same history structure as ClearHistoryEntry_Trigram_DeleteWhole.
  UserHistoryPredictor::Entry *japaneseinputmethod;
  UserHistoryPredictor::Entry *japanese;
  UserHistoryPredictor::Entry *input;
  UserHistoryPredictor::Entry *method;
  InitHistory_JapaneseInputMethod(predictor, &japaneseinputmethod,
                                  &japanese, &input, &method);

  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor,
                                      "japan", "JapaneseInputMethod"));
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
    const string key = "inputmethod";
    for (size_t i = 0; i < key.size(); ++i) {
      const string &prefix = key.substr(0, i);
      EXPECT_FALSE(IsSuggested(predictor, prefix, "Input"));
      EXPECT_FALSE(IsPredicted(predictor, prefix, "Input"));
    }
  }

  // The following can still be shown by the predictor.
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor,
                                      "japan", "JapaneseInputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "InputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "meth", "Method"));
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntry_Trigram_DeleteThird) {
  // Tests ClearHistoryEntry() for trigram history.  This case tests the
  // deletion of the third node of trigram.
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the same history structure as ClearHistoryEntry_Trigram_DeleteWhole.
  UserHistoryPredictor::Entry *japaneseinputmethod;
  UserHistoryPredictor::Entry *japanese;
  UserHistoryPredictor::Entry *input;
  UserHistoryPredictor::Entry *method;
  InitHistory_JapaneseInputMethod(predictor, &japaneseinputmethod,
                                  &japanese, &input, &method);

  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor,
                                      "japan", "JapaneseInputMethod"));
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
    const string key = "method";
    for (size_t i = 0; i < key.size(); ++i) {
      const string &prefix = key.substr(0, i);
      EXPECT_FALSE(IsSuggested(predictor, prefix, "Method"));
      EXPECT_FALSE(IsPredicted(predictor, prefix, "Method"));
    }
  }

  // The following can still be shown by the predictor.
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor,
                                      "japan", "JapaneseInputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "Input"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "InputMethod"));
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntry_Trigram_DeleteFirstBigram) {
  // Tests ClearHistoryEntry() for trigram history.  This case tests the
  // deletion of the first bigram of trigram.
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the same history structure as ClearHistoryEntry_Trigram_DeleteWhole.
  UserHistoryPredictor::Entry *japaneseinputmethod;
  UserHistoryPredictor::Entry *japanese;
  UserHistoryPredictor::Entry *input;
  UserHistoryPredictor::Entry *method;
  InitHistory_JapaneseInputMethod(predictor, &japaneseinputmethod,
                                  &japanese, &input, &method);

  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor,
                                      "japan", "JapaneseInputMethod"));
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
    const string key = "japaneseinputmethod";
    for (size_t i = 0; i < key.size(); ++i) {
      const string &prefix = key.substr(0, i);
      EXPECT_FALSE(IsSuggested(predictor, prefix, "JapaneseInput"));
      EXPECT_FALSE(IsPredicted(predictor, prefix, "JapaneseInput"));
    }
  }

  // However, the following can still be available, including
  // "JapaneseInputMethod".
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor,
                                      "japan", "JapaneseInputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "Input"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "InputMethod"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "meth", "Method"));
}

TEST_F(UserHistoryPredictorTest, ClearHistoryEntry_Trigram_DeleteSecondBigram) {
  // Tests ClearHistoryEntry() for trigram history.  This case tests the
  // deletion of the latter bigram of trigram.
  UserHistoryPredictor *predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the same history structure as ClearHistoryEntry_Trigram_DeleteWhole.
  UserHistoryPredictor::Entry *japaneseinputmethod;
  UserHistoryPredictor::Entry *japanese;
  UserHistoryPredictor::Entry *input;
  UserHistoryPredictor::Entry *method;
  InitHistory_JapaneseInputMethod(predictor, &japaneseinputmethod,
                                  &japanese, &input, &method);

  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor,
                                      "japan", "JapaneseInputMethod"));
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
    const string key = "inputmethod";
    for (size_t i = 0; i < key.size(); ++i) {
      const string &prefix = key.substr(0, i);
      EXPECT_FALSE(IsSuggested(predictor, prefix, "InputMethod"));
      EXPECT_FALSE(IsPredicted(predictor, prefix, "InputMethod"));
    }
  }

  // However, the following are available.
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
  EXPECT_TRUE(IsSuggestedAndPredicted(predictor,
                                      "japan", "JapaneseInputMethod"));
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
    MakeSegmentsForConversion("ぐーぐｒ", &segments);
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
    MakeSegmentsForPrediction(kKey, &segments);
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
  MakeSegmentsForPrediction("と", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_TRUE(FindCandidateByValue("東京", segments));
  EXPECT_TRUE(FindCandidateByValue("東京か", segments));

  segments.Clear();
  MakeSegmentsForPrediction("な", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_TRUE(FindCandidateByValue("名古屋", segments));
  EXPECT_TRUE(FindCandidateByValue("名古屋に", segments));

  segments.Clear();
  MakeSegmentsForPrediction("い", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_TRUE(FindCandidateByValue("行きたい", segments));
}

TEST_F(UserHistoryPredictorTest, JoinedSegmentsTest_Mobile) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();
  commands::RequestForUnitTest::FillMobileRequest(request_.get());
  Segments segments;

  MakeSegmentsForConversion("わたしの", &segments);
  AddCandidate(0, "私の", &segments);

  AddSegmentForConversion("なまえは", &segments);
  AddCandidate(1, "名前は", &segments);

  predictor->Finish(*convreq_, &segments);
  segments.Clear();

  MakeSegmentsForSuggestion("わたし", &segments);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_EQ(1, segments.segment(0).candidates_size());
  EXPECT_EQ("私の", segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
  segments.Clear();

  MakeSegmentsForPrediction("わたしの", &segments);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_EQ(1, segments.segment(0).candidates_size());
  EXPECT_EQ("私の", segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
  segments.Clear();

  MakeSegmentsForPrediction("わたしのな", &segments);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_EQ(1, segments.segment(0).candidates_size());
  EXPECT_EQ("私の名前は", segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
  segments.Clear();
}

TEST_F(UserHistoryPredictorTest, JoinedSegmentsTest_Desktop) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  Segments segments;

  MakeSegmentsForConversion("わたしの", &segments);
  AddCandidate(0, "私の", &segments);

  AddSegmentForConversion("なまえは", &segments);
  AddCandidate(1, "名前は", &segments);

  predictor->Finish(*convreq_, &segments);

  segments.Clear();

  MakeSegmentsForSuggestion("わたし", &segments);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_EQ(2, segments.segment(0).candidates_size());
  EXPECT_EQ("私の", segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
  EXPECT_EQ("私の名前は", segments.segment(0).candidate(1).value);
  EXPECT_TRUE(segments.segment(0).candidate(1).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
  segments.Clear();

  MakeSegmentsForPrediction("わたしの", &segments);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_EQ(1, segments.segment(0).candidates_size());
  EXPECT_EQ("私の名前は", segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
  segments.Clear();

  MakeSegmentsForPrediction("わたしのな", &segments);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_EQ(1, segments.segment(0).candidates_size());
  EXPECT_EQ("私の名前は", segments.segment(0).candidate(0).value);
  EXPECT_TRUE(segments.segment(0).candidate(0).source_info &
              Segment::Candidate::USER_HISTORY_PREDICTOR);
  segments.Clear();
}

TEST_F(UserHistoryPredictorTest, UsageStats) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  Segments segments;
  EXPECT_COUNT_STATS("CommitUserHistoryPredictor", 0);
  EXPECT_COUNT_STATS("CommitUserHistoryPredictorZeroQuery", 0);

  MakeSegmentsForConversion("なまえは", &segments);
  AddCandidate(0, "名前は", &segments);
  segments.mutable_conversion_segment(0)->mutable_candidate(0)->source_info |=
      Segment::Candidate::USER_HISTORY_PREDICTOR;
  predictor->Finish(*convreq_, &segments);

  EXPECT_COUNT_STATS("CommitUserHistoryPredictor", 1);
  EXPECT_COUNT_STATS("CommitUserHistoryPredictorZeroQuery", 0);

  segments.Clear();

  // Zero query
  MakeSegmentsForConversion("", &segments);
  AddCandidate(0, "名前は", &segments);
  segments.mutable_conversion_segment(0)->mutable_candidate(0)->source_info |=
      Segment::Candidate::USER_HISTORY_PREDICTOR;
  predictor->Finish(*convreq_, &segments);

  // UserHistoryPredictor && ZeroQuery
  EXPECT_COUNT_STATS("CommitUserHistoryPredictor", 2);
  EXPECT_COUNT_STATS("CommitUserHistoryPredictorZeroQuery", 1);
}

TEST_F(UserHistoryPredictorTest, PunctuationLink_Mobile) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();
  commands::RequestForUnitTest::FillMobileRequest(request_.get());
  Segments segments;
  {
    MakeSegmentsForConversion("ございます", &segments);
    AddCandidate(0, "ございます", &segments);

    predictor->Finish(*convreq_, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    AddSegmentForConversion("!", &segments);
    AddCandidate(1, "！", &segments);
    predictor->Finish(*convreq_, &segments);

    segments.Clear();
    MakeSegmentsForSuggestion("ございま", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("ございます", segments.conversion_segment(0).candidate(0).value);
    EXPECT_FALSE(FindCandidateByValue("ございます！", segments));

    // Zero query from "ございます" -> "！"
    segments.Clear();
    MakeSegmentsForConversion("ございます", &segments);
    AddCandidate(0, "ございます", &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
    AddSegmentForSuggestion("", &segments);  // empty request
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("！", segments.conversion_segment(0).candidate(0).value);
  }

  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  {
    MakeSegmentsForConversion("!", &segments);
    AddCandidate(0, "！", &segments);

    predictor->Finish(*convreq_, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    AddSegmentForConversion("ございます", &segments);
    AddCandidate(1, "ございます", &segments);
    predictor->Finish(*convreq_, &segments);

    // Zero query from "！" -> no suggestion
    segments.Clear();
    MakeSegmentsForConversion("!", &segments);
    AddCandidate(0, "！", &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
    AddSegmentForSuggestion("", &segments);  // empty request
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
  }

  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  {
    MakeSegmentsForConversion("ございます!", &segments);
    AddCandidate(0, "ございます！", &segments);

    predictor->Finish(*convreq_, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    AddSegmentForConversion("よろしくおねがいします", &segments);
    AddCandidate(1, "よろしくお願いします", &segments);
    predictor->Finish(*convreq_, &segments);

    // Zero query from "！" -> no suggestion
    segments.Clear();
    MakeSegmentsForConversion("!", &segments);
    AddCandidate(0, "！", &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
    AddSegmentForSuggestion("", &segments);  // empty request
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

    // Zero query from "ございます！" -> no suggestion
    segments.Clear();
    MakeSegmentsForConversion("ございます!", &segments);
    AddCandidate(0, "ございます！", &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
    AddSegmentForSuggestion("", &segments);  // empty request
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
  }

  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  {
    MakeSegmentsForConversion("ございます", &segments);
    AddCandidate(0, "ございます", &segments);

    predictor->Finish(*convreq_, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    AddSegmentForConversion("!よろしくおねがいします", &segments);
    AddCandidate(1, "！よろしくお願いします", &segments);
    predictor->Finish(*convreq_, &segments);

    segments.Clear();
    MakeSegmentsForSuggestion("ございま", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("ございます", segments.conversion_segment(0).candidate(0).value);
    EXPECT_FALSE(
        FindCandidateByValue("ございます！よろしくお願いします", segments));

    // Zero query from "ございます" -> no suggestion
    segments.Clear();
    MakeSegmentsForConversion("ございます", &segments);
    AddCandidate(0, "ございます", &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
    AddSegmentForSuggestion("", &segments);  // empty request
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
  }
}

TEST_F(UserHistoryPredictorTest, PunctuationLink_Desktop) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();
  Segments segments;
  {
    MakeSegmentsForConversion("ございます", &segments);
    AddCandidate(0, "ございます", &segments);

    predictor->Finish(*convreq_, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    AddSegmentForConversion("!", &segments);
    AddCandidate(1, "！", &segments);
    predictor->Finish(*convreq_, &segments);

    segments.Clear();
    MakeSegmentsForSuggestion("ございま", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("ございます", segments.conversion_segment(0).candidate(0).value);
    EXPECT_FALSE(FindCandidateByValue("ございます！", segments));

    segments.Clear();
    MakeSegmentsForSuggestion("ございます", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("ございます", segments.conversion_segment(0).candidate(0).value);
    EXPECT_FALSE(FindCandidateByValue("ございます！", segments));
  }

  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  {
    MakeSegmentsForConversion("!", &segments);
    AddCandidate(0, "！", &segments);

    predictor->Finish(*convreq_, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    AddSegmentForConversion("よろしくおねがいします", &segments);
    AddCandidate(1, "よろしくお願いします", &segments);
    predictor->Finish(*convreq_, &segments);

    segments.Clear();
    MakeSegmentsForSuggestion("!", &segments);
    EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
  }

  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  {
    MakeSegmentsForConversion("ございます!", &segments);
    AddCandidate(0, "ございます！", &segments);

    predictor->Finish(*convreq_, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    AddSegmentForConversion("よろしくおねがいします", &segments);
    AddCandidate(1, "よろしくお願いします", &segments);
    predictor->Finish(*convreq_, &segments);

    segments.Clear();
    MakeSegmentsForSuggestion("ございます", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("ございます！",
              segments.conversion_segment(0).candidate(0).value);
    EXPECT_FALSE(
        FindCandidateByValue("ございます！よろしくお願いします", segments));

    segments.Clear();
    MakeSegmentsForSuggestion("ございます!", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("ございます！",
              segments.conversion_segment(0).candidate(0).value);
    EXPECT_FALSE(
        FindCandidateByValue("ございます！よろしくお願いします", segments));
  }

  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  {
    MakeSegmentsForConversion("ございます", &segments);
    AddCandidate(0, "ございます", &segments);

    predictor->Finish(*convreq_, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    AddSegmentForConversion("!よろしくおねがいします", &segments);
    AddCandidate(1, "！よろしくお願いします", &segments);
    predictor->Finish(*convreq_, &segments);

    segments.Clear();
    MakeSegmentsForSuggestion("ございます", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_EQ("ございます", segments.conversion_segment(0).candidate(0).value);
    EXPECT_FALSE(FindCandidateByValue("ございます！", segments));
    EXPECT_FALSE(
        FindCandidateByValue("ございます！よろしくお願いします", segments));
  }

  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  {
    // Note that "よろしくお願いします:よろしくおねがいします" is the sentence
    // like candidate. Please refer to user_history_predictor.cc
    MakeSegmentsForConversion("よろしくおねがいします", &segments);
    AddCandidate(0, "よろしくお願いします", &segments);

    predictor->Finish(*convreq_, &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    AddSegmentForConversion("!", &segments);
    AddCandidate(1, "！", &segments);
    predictor->Finish(*convreq_, &segments);

    segments.Clear();
    MakeSegmentsForSuggestion("よろしくおねがいします", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
    EXPECT_TRUE(FindCandidateByValue("よろしくお願いします！", segments));
  }
}

}  // namespace mozc
