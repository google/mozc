// Copyright 2010-2014, Google Inc.
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

#include <set>
#include <string>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/number_util.h"
#include "base/password_manager.h"
#include "base/port.h"
#include "base/system_util.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/node.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/suppression_dictionary.h"
#include "session/commands.pb.h"
#include "session/request_test_util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);
DECLARE_bool(enable_expansion_for_user_history_predictor);

namespace mozc {

using commands::Request;

namespace {

void MakeSegmentsForSuggestion(const string &key, Segments *segments) {
  segments->set_max_prediction_candidates_size(10);
  segments->set_request_type(Segments::SUGGESTION);
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FIXED_VALUE);
}

void MakeSegmentsForPrediction(const string &key, Segments *segments) {
  segments->set_max_prediction_candidates_size(10);
  segments->set_request_type(Segments::PREDICTION);
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FIXED_VALUE);
}

void MakeSegmentsForConversion(const string &key, Segments *segments) {
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

bool FindCandidateByValue(const string &value, const Segments &segments) {
  for (size_t i = 0;
       i < segments.conversion_segment(0).candidates_size(); ++i) {
    if (segments.conversion_segment(0).candidate(i).value == value) {
      return true;
    }
  }
  return false;
}
}   // anonymous namespace

class UserHistoryPredictorTest : public ::testing::Test {
 public:
  UserHistoryPredictorTest() :
      default_expansion_(FLAGS_enable_expansion_for_user_history_predictor) {}

  virtual ~UserHistoryPredictorTest() {
    FLAGS_enable_expansion_for_user_history_predictor = default_expansion_;
  }

 protected:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::ConfigHandler::GetDefaultConfig(&default_config_);
    config::ConfigHandler::SetConfig(default_config_);
    data_and_predictor_.reset(CreateDataAndPredictor());
  }

  virtual void TearDown() {
    config::ConfigHandler::SetConfig(default_config_);
    FLAGS_enable_expansion_for_user_history_predictor = default_expansion_;
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

  const commands::Request &default_request() const {
    return default_request_;
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

 private:
  struct DataAndPredictor {
    scoped_ptr<DictionaryMock> dictionary;
    scoped_ptr<SuppressionDictionary> suppression_dictionary;
    scoped_ptr<UserHistoryPredictor> predictor;
  };

  DataAndPredictor *CreateDataAndPredictor() const {
    DataAndPredictor *ret = new DataAndPredictor;
    testing::MockDataManager data_manager;
    ret->dictionary.reset(new DictionaryMock);
    ret->suppression_dictionary.reset(new SuppressionDictionary);
    ret->predictor.reset(
        new UserHistoryPredictor(ret->dictionary.get(),
                                 data_manager.GetPOSMatcher(),
                                 ret->suppression_dictionary.get()));
    return ret;
  }

  config::Config default_config_;
  const commands::Request default_request_;
  const bool default_expansion_;
  scoped_ptr<DataAndPredictor> data_and_predictor_;
};

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorTest) {
  const ConversionRequest conversion_request;

  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    predictor->WaitForSyncer();

    // Nothing happen
    {
      Segments segments;
      // "てすと"
      MakeSegmentsForSuggestion(
          "\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8", &segments);
      EXPECT_FALSE(predictor->PredictForRequest(conversion_request, &segments));
      EXPECT_EQ(0, segments.segment(0).candidates_size());
    }

    // Nothing happen
    {
      Segments segments;
      // "てすと"
      MakeSegmentsForPrediction(
          "\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8", &segments);
      EXPECT_FALSE(predictor->PredictForRequest(conversion_request, &segments));
      EXPECT_EQ(0, segments.segment(0).candidates_size());
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
      predictor->Finish(&segments);

      // "わたしの"
      MakeSegmentsForSuggestion(
          "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
      EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));
      // "私の名前は中野です"
      EXPECT_EQ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
                "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7"
                "\xE3\x81\x99",
                segments.segment(0).candidate(0).value);

      segments.Clear();
      // "わたしの"
      MakeSegmentsForPrediction(
          "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
      EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));
      // "私の名前は中野です"
      EXPECT_EQ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
                "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7"
                "\xE3\x81\x99",
                segments.segment(0).candidate(0).value);
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
      config::Config config;
      config.set_use_history_suggest(false);
      config::ConfigHandler::SetConfig(config);

      // "わたしの"
      MakeSegmentsForSuggestion(
          "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
      EXPECT_FALSE(predictor->PredictForRequest(conversion_request, &segments));

      config.set_use_history_suggest(true);
      config.set_incognito_mode(true);
      config::ConfigHandler::SetConfig(config);

      // "わたしの"
      MakeSegmentsForSuggestion(
          "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
      EXPECT_FALSE(predictor->PredictForRequest(conversion_request, &segments));
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
    EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));
    // "私の名前は中野です"
    EXPECT_EQ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99",
              segments.segment(0).candidate(0).value);

    segments.Clear();
    // "わたしの"
    MakeSegmentsForPrediction(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));
    // "私の名前は中野です"
    EXPECT_EQ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99",
              segments.segment(0).candidate(0).value);

    // Exact Match
    segments.Clear();
    // "わたしのなまえはなかのです"
    MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE"
        "\xE3\x81\xAA\xE3\x81\xBE\xE3\x81\x88\xE3\x81\xAF"
        "\xE3\x81\xAA\xE3\x81\x8B\xE3\x81\xAE\xE3\x81\xA7\xE3\x81\x99",
        &segments);
    EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));
    // "私の名前は中野です"
    EXPECT_EQ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99",
              segments.segment(0).candidate(0).value);

    segments.Clear();
    // "わたしのなまえはなかのです"
    MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE"
        "\xE3\x81\xAA\xE3\x81\xBE\xE3\x81\x88\xE3\x81\xAF\xE3\x81\xAA"
        "\xE3\x81\x8B\xE3\x81\xAE\xE3\x81\xA7\xE3\x81\x99", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));
    // "私の名前は中野です"
    EXPECT_EQ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99",
              segments.segment(0).candidate(0).value);

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
    // "わたしの"
    MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_FALSE(predictor->PredictForRequest(conversion_request, &segments));

    // "わたしの"
    MakeSegmentsForPrediction(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_FALSE(predictor->PredictForRequest(conversion_request, &segments));
  }

  // nothing happen
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    predictor->WaitForSyncer();
    Segments segments;

    // reproducesd
    // "わたしの"
    MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_FALSE(predictor->PredictForRequest(conversion_request, &segments));

    // "わたしの"
    MakeSegmentsForPrediction(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_FALSE(predictor->PredictForRequest(conversion_request, &segments));
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
    // "かまた"
    MakeSegmentsForSuggestion("\xE3\x81\x8B\xE3\x81\xBE\xE3\x81\x9F",
                              &segments);
    // "火魔汰"
    AddCandidate(0, "\xE7\x81\xAB\xE9\xAD\x94\xE6\xB1\xB0", &segments);
    // "ま"
    MakeSegmentsForSuggestion("\xE3\x81\xBE", &segments);
    // "摩"
    AddCandidate(1, "\xE6\x91\xA9", &segments);
    predictor->Finish(&segments);

    // All added items must be suggestion entries.
    const UserHistoryPredictor::DicCache::Element *element;
    for (element = predictor->dic_->Head();
         element->next;
         element = element->next) {
      const user_history_predictor::UserHistory::Entry &entry = element->value;
      EXPECT_TRUE(entry.has_suggestion_freq() && entry.suggestion_freq() == 1);
      EXPECT_TRUE(!entry.has_conversion_freq() && entry.conversion_freq() == 0);
    }
  }

  // Obtain input histories via Predict method.
  {
    const ConversionRequest conversion_request;
    Segments segments;
    // "かま"
    MakeSegmentsForSuggestion("\xE3\x81\x8B\xE3\x81\xBE", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));
    set<string> expected_candidates;
    // "火魔汰"
    expected_candidates.insert("\xE7\x81\xAB\xE9\xAD\x94\xE6\xB1\xB0");
    // "火魔汰摩"
    // We can get this entry even if Segmtnts's type is not CONVERSION.
    expected_candidates.insert(
        "\xE7\x81\xAB\xE9\xAD\x94\xE6\xB1\xB0\xE6\x91\xA9");
    for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
      SCOPED_TRACE(segments.segment(0).candidate(i).value);
      EXPECT_EQ(
          1,
          expected_candidates.erase(segments.segment(0).candidate(i).value));
    }
  }
}

TEST_F(UserHistoryPredictorTest, DescriptionTest) {
#ifdef DEBUG
  // "テスト History"
  const char kDescription[] = "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88 History";
#else
  // "テスト"
  const char kDescription[] = "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88";
#endif  // DEBUG

  const ConversionRequest conversion_request;

  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    predictor->WaitForSyncer();

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
          kDescription,
          &segments);
      predictor->Finish(&segments);

      // "わたしの"
      MakeSegmentsForSuggestion(
          "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
      EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));
      // "私の名前は中野です"
      EXPECT_EQ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
                "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7"
                "\xE3\x81\x99",
                segments.segment(0).candidate(0).value);
      EXPECT_EQ(kDescription, segments.segment(0).candidate(0).description);

      segments.Clear();
      // "わたしの"
      MakeSegmentsForPrediction(
          "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
      EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));
      // "私の名前は中野です"
      EXPECT_EQ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
                "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7"
                "\xE3\x81\x99",
                segments.segment(0).candidate(0).value);
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
      config::Config config;
      config.set_use_history_suggest(false);
      config::ConfigHandler::SetConfig(config);
      predictor->WaitForSyncer();

      // "わたしの"
      MakeSegmentsForSuggestion(
          "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
      EXPECT_FALSE(predictor->PredictForRequest(conversion_request, &segments));

      config.set_use_history_suggest(true);
      config.set_incognito_mode(true);
      config::ConfigHandler::SetConfig(config);

      // "わたしの"
      MakeSegmentsForSuggestion(
          "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
      EXPECT_FALSE(predictor->PredictForRequest(conversion_request, &segments));
    }

    // turn on
    {
      config::Config config;
      config::ConfigHandler::SetConfig(config);
      predictor->WaitForSyncer();
    }

    // reproducesd
    // "わたしの"
    MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));
    // "私の名前は中野です"
    EXPECT_EQ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99",
              segments.segment(0).candidate(0).value);
    EXPECT_EQ(kDescription, segments.segment(0).candidate(0).description);

    segments.Clear();
    // "わたしの"
    MakeSegmentsForPrediction(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));
    // "私の名前は中野です"
    EXPECT_EQ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99",
              segments.segment(0).candidate(0).value);
    EXPECT_EQ(kDescription, segments.segment(0).candidate(0).description);

    // Exact Match
    segments.Clear();
    // "わたしのなまえはなかのです"
    MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE"
        "\xE3\x81\xAA\xE3\x81\xBE\xE3\x81\x88\xE3\x81\xAF"
        "\xE3\x81\xAA\xE3\x81\x8B\xE3\x81\xAE\xE3\x81\xA7\xE3\x81\x99",
        &segments);
    EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));
    // "私の名前は中野です"
    EXPECT_EQ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99",
              segments.segment(0).candidate(0).value);
    EXPECT_EQ(kDescription, segments.segment(0).candidate(0).description);

    segments.Clear();
    // "わたしのなまえはなかのです"
    MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE"
        "\xE3\x81\xAA\xE3\x81\xBE\xE3\x81\x88\xE3\x81\xAF\xE3\x81\xAA"
        "\xE3\x81\x8B\xE3\x81\xAE\xE3\x81\xA7\xE3\x81\x99", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));
    // "私の名前は中野です"
    EXPECT_EQ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99",
              segments.segment(0).candidate(0).value);
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
    // "わたしの"
    MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_FALSE(predictor->PredictForRequest(conversion_request, &segments));

    // "わたしの"
    MakeSegmentsForPrediction(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_FALSE(predictor->PredictForRequest(conversion_request, &segments));
  }

  // nothing happen
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    predictor->WaitForSyncer();
    Segments segments;

    // reproducesd
    // "わたしの"
    MakeSegmentsForSuggestion(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_FALSE(predictor->PredictForRequest(conversion_request, &segments));

    // "わたしの"
    MakeSegmentsForPrediction(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_FALSE(predictor->PredictForRequest(conversion_request, &segments));
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorUnusedHistoryTest) {
  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    predictor->WaitForSyncer();

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
    predictor->Finish(&segments);

    segments.Clear();
    // "ひろすえりょうこ"
    MakeSegmentsForConversion
        ("\xE3\x81\xB2\xE3\x82\x8D\xE3\x81\x99\xE3\x81\x88"
         "\xE3\x82\x8A\xE3\x82\x87\xE3\x81\x86\xE3\x81\x93", &segments);
    // "広末涼子"
    AddCandidate("\xE5\xBA\x83\xE6\x9C\xAB\xE6\xB6\xBC\xE5\xAD\x90", &segments);

    segments.set_request_type(Segments::CONVERSION);

    // conversion
    predictor->Finish(&segments);

    // sync
    predictor->Sync();
  }

  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    predictor->WaitForSyncer();
    Segments segments;

    // "わたしの"
    MakeSegmentsForSuggestion("\xE3\x82\x8F\xE3\x81\x9F"
                              "\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_TRUE(predictor->Predict(&segments));
    // "私の名前は中野です"
    EXPECT_EQ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99",
              segments.segment(0).candidate(0).value);

    segments.Clear();
    // "ひろすえ"
    MakeSegmentsForSuggestion("\xE3\x81\xB2\xE3\x82\x8D"
                              "\xE3\x81\x99\xE3\x81\x88", &segments);
    EXPECT_TRUE(predictor->Predict(&segments));
    // "広末涼子"
    EXPECT_EQ("\xE5\xBA\x83\xE6\x9C\xAB\xE6\xB6\xBC\xE5\xAD\x90",
              segments.segment(0).candidate(0).value);

    predictor->ClearUnusedHistory();
    predictor->WaitForSyncer();

    segments.Clear();
    // "わたしの"
    MakeSegmentsForSuggestion("\xE3\x82\x8F\xE3\x81\x9F"
                              "\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_TRUE(predictor->Predict(&segments));
    // "私の名前は中野です"
    EXPECT_EQ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99",
              segments.segment(0).candidate(0).value);

    segments.Clear();
    // "ひろすえ"
    MakeSegmentsForSuggestion("\xE3\x81\xB2\xE3\x82\x8D"
                              "\xE3\x81\x99\xE3\x81\x88", &segments);
    EXPECT_FALSE(predictor->Predict(&segments));

    predictor->Sync();
  }

  {
    UserHistoryPredictor *predictor = GetUserHistoryPredictor();
    predictor->WaitForSyncer();
    Segments segments;

    // "わたしの"
    MakeSegmentsForSuggestion("\xE3\x82\x8F\xE3\x81\x9F"
                              "\xE3\x81\x97\xE3\x81\xAE", &segments);
    EXPECT_TRUE(predictor->Predict(&segments));
    // "私の名前は中野です"
    EXPECT_EQ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
              "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99",
              segments.segment(0).candidate(0).value);

    segments.Clear();
    // "ひろすえ"
    MakeSegmentsForSuggestion("\xE3\x81\xB2\xE3\x82\x8D"
                              "\xE3\x81\x99\xE3\x81\x88", &segments);
    EXPECT_FALSE(predictor->Predict(&segments));
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorRevertTest) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

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

  predictor->Finish(&segments);

  // Before Revert, Suggest works
  // "わたしの"
  MakeSegmentsForSuggestion("\xE3\x82\x8F\xE3\x81\x9F"
                            "\xE3\x81\x97\xE3\x81\xAE", &segments2);
  EXPECT_TRUE(predictor->Predict(&segments2));
  // "私の名前は中野です"
  EXPECT_EQ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
            "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99",
            segments.segment(0).candidate(0).value);

  // Call revert here
  predictor->Revert(&segments);

  segments.Clear();
  // "わたしの"
  MakeSegmentsForSuggestion("\xE3\x82\x8F\xE3\x81\x9F"
                            "\xE3\x81\x97\xE3\x81\xAE", &segments);

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
    // "テストテスト"
    AddCandidate("\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88"
                 "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88", &segments);
    predictor->Finish(&segments);
  }

  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  // input "testtest" 1 time
  for (int i = 0; i < 1; ++i) {
    Segments segments;
    MakeSegmentsForConversion("testtest", &segments);
    // "テストテスト"
    AddCandidate("\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88"
                 "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88", &segments);
    predictor->Finish(&segments);
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

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorTailingPunctuation) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

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

  predictor->Finish(&segments);

  segments.Clear();
  // "わたしの"
  MakeSegmentsForPrediction("\xE3\x82\x8F\xE3\x81\x9F"
                            "\xE3\x81\x97\xE3\x81\xAE", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_EQ(2, segments.segment(0).candidates_size());
  // "私の名前は中野です"
  EXPECT_EQ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
            "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99",
            segments.segment(0).candidate(0).value);
  // "私の名前は中野です。"
  EXPECT_EQ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
            "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99"
            "\xE3\x80\x82",
            segments.segment(0).candidate(1).value);

  segments.Clear();
  // "わたしの"
  MakeSegmentsForSuggestion("\xE3\x82\x8F\xE3\x81\x9F"
                            "\xE3\x81\x97\xE3\x81\xAE", &segments);

  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_EQ(2, segments.segment(0).candidates_size());
  // "私の名前は中野です"
  EXPECT_EQ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
            "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99",
            segments.segment(0).candidate(0).value);
  // "私の名前は中野です。"
  EXPECT_EQ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
            "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99"
            "\xE3\x80\x82",
            segments.segment(0).candidate(1).value);
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorPreceedingPunctuation) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

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

  predictor->Finish(&segments);

  segments.Clear();
  // "わたしの"
  MakeSegmentsForPrediction("\xE3\x82\x8F\xE3\x81\x9F"
                            "\xE3\x81\x97\xE3\x81\xAE", &segments);

  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_EQ(1, segments.segment(0).candidates_size());
  // "私の名前は中野です"
  EXPECT_EQ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
            "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99",
            segments.segment(0).candidate(0).value);


  segments.Clear();
  // "わたしの"
  MakeSegmentsForSuggestion("\xE3\x82\x8F\xE3\x81\x9F"
                            "\xE3\x81\x97\xE3\x81\xAE", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_EQ(1, segments.segment(0).candidates_size());
  // "私の名前は中野です"
  EXPECT_EQ("\xE7\xA7\x81\xE3\x81\xAE\xE5\x90\x8D\xE5\x89\x8D"
            "\xE3\x81\xAF\xE4\xB8\xAD\xE9\x87\x8E\xE3\x81\xA7\xE3\x81\x99",
            segments.segment(0).candidate(0).value);
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
    // "。"
    { "\xe3\x80\x82", false },
    // "、"
    { "\xe3\x80\x81", false },
    // "？"
    { "\xef\xbc\x9f", false },
    // "！"
    { "\xef\xbc\x81", false },
    // "ぬ"
    { "\xe3\x81\xac", true },
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
      // "てすとぶんしょう"
      MakeSegmentsForConversion(
          "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8\xe3\x81\xb6"
          "\xe3\x82\x93\xe3\x81\x97\xe3\x82\x87\xe3\x81\x86", &segments);
      // "テスト文章"
      AddCandidate(1,
                   "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88"
                   "\xe6\x96\x87\xe7\xab\xa0",
                   &segments);
      predictor->Finish(&segments);
    }
    segments.Clear();
    {
      // Learn from one segment
      // "てすとぶんしょう"
      MakeSegmentsForConversion(
          first_char +
          "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8\xe3\x81\xb6"
          "\xe3\x82\x93\xe3\x81\x97\xe3\x82\x87\xe3\x81\x86", &segments);
      // "テスト文章"
      AddCandidate(0,
                   first_char +
                   "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88"
                   "\xe6\x96\x87\xe7\xab\xa0", &segments);
      predictor->Finish(&segments);
    }
    segments.Clear();
    {
      // Suggestion
      MakeSegmentsForSuggestion(first_char, &segments);
      AddCandidate(0, first_char, &segments);
      EXPECT_EQ(kTestCases[i].expected_result,
                predictor->Predict(&segments)) << "Suggest from " << first_char;
    }
    segments.Clear();
    {
      // Prediciton
      MakeSegmentsForPrediction(first_char, &segments);
      EXPECT_EQ(kTestCases[i].expected_result,
                predictor->Predict(&segments)) << "Predict from " << first_char;
    }
  }
}

TEST_F(UserHistoryPredictorTest, ZeroQuerySuggestionTest) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  commands::Request request;
  request.set_zero_query_suggestion(true);
  const ConversionRequest conversion_request(NULL, &request);
  commands::Request non_zero_query_request;
  non_zero_query_request.set_zero_query_suggestion(false);
  const ConversionRequest non_zero_query_conversion_request(
      NULL, &non_zero_query_request);

  Segments segments;

  // No history segments
  segments.Clear();
  MakeSegmentsForSuggestion("", &segments);
  EXPECT_FALSE(predictor->PredictForRequest(conversion_request, &segments));

  {
    segments.Clear();

    // "たろうは/太郎は"
    MakeSegmentsForConversion("\xE3\x81\x9F\xE3\x82\x8D"
                              "\xE3\x81\x86\xE3\x81\xAF", &segments);
    AddCandidate(0, "\xE5\xA4\xAA\xE9\x83\x8E\xE3\x81\xAF", &segments);
    predictor->Finish(&segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    // "はなこに/花子に"
    MakeSegmentsForConversion("\xE3\x81\xAF\xE3\x81\xAA"
                              "\xE3\x81\x93\xE3\x81\xAB", &segments);
    AddCandidate(1, "\xE8\x8A\xB1\xE5\xAD\x90\xE3\x81\xAB", &segments);
    predictor->Finish(&segments);
    segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);

    // "きょうと/京都"
    segments.pop_back_segment();
    MakeSegmentsForConversion(
        "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x81\xA8",
        &segments);
    AddCandidate(1, "\xE4\xBA\xAC\xE9\x83\xBD",
                 &segments);
    Util::Sleep(2000);
    predictor->Finish(&segments);
    segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);

    // "おおさか/大阪"
    segments.pop_back_segment();
    MakeSegmentsForConversion(
        "\xE3\x81\x8A\xE3\x81\x8A\xE3\x81\x95\xE3\x81\x8B",
        &segments);
    AddCandidate(1, "\xE5\xA4\xA7\xE9\x98\xAA",
                 &segments);
    Util::Sleep(2000);
    predictor->Finish(&segments);
    segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);

    // Zero query suggestion is disabled.
    segments.pop_back_segment();
    MakeSegmentsForSuggestion("", &segments);  // empty request
    EXPECT_FALSE(predictor->PredictForRequest(
        non_zero_query_conversion_request, &segments));

    segments.pop_back_segment();
    MakeSegmentsForSuggestion("", &segments);   // empty request
    EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));
    // last-pushed segment is "大阪"
    // "大阪"
    EXPECT_EQ("\xE5\xA4\xA7\xE9\x98\xAA",
              segments.segment(1).candidate(0).value);
    // "おおさか"
    EXPECT_EQ("\xE3\x81\x8A\xE3\x81\x8A\xE3\x81\x95\xE3\x81\x8B",
              segments.segment(1).candidate(0).key);

    segments.pop_back_segment();
    // "は"
    MakeSegmentsForSuggestion("\xE3\x81\xAF", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));

    segments.pop_back_segment();
    // "た"
    MakeSegmentsForSuggestion("\xE3\x81\x9F", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));

    segments.pop_back_segment();
    // "き"
    MakeSegmentsForSuggestion("\xE3\x81\x8D", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));

    segments.pop_back_segment();
    // "お"
    MakeSegmentsForSuggestion("\xE3\x81\x8A", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));
  }

  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  {
    segments.Clear();
    // "たろうは/太郎は"
    MakeSegmentsForConversion("\xE3\x81\x9F\xE3\x82\x8D"
                              "\xE3\x81\x86\xE3\x81\xAF", &segments);
    AddCandidate(0, "\xE5\xA4\xAA\xE9\x83\x8E\xE3\x81\xAF", &segments);

    // "はなこに/花子に"
    MakeSegmentsForConversion("\xE3\x81\xAF\xE3\x81\xAA"
                              "\xE3\x81\x93\xE3\x81\xAB", &segments);
    AddCandidate(1, "\xE8\x8A\xB1\xE5\xAD\x90\xE3\x81\xAB", &segments);
    predictor->Finish(&segments);

    segments.Clear();
    // "たろうは/太郎は"
    MakeSegmentsForConversion("\xE3\x81\x9F\xE3\x82\x8D"
                              "\xE3\x81\x86\xE3\x81\xAF", &segments);
    AddCandidate(0, "\xE5\xA4\xAA\xE9\x83\x8E\xE3\x81\xAF", &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

    // Zero query suggestion is disabled.
    MakeSegmentsForSuggestion("", &segments);  // empty request
    EXPECT_FALSE(predictor->PredictForRequest(
        non_zero_query_conversion_request, &segments));

    segments.pop_back_segment();
    MakeSegmentsForSuggestion("", &segments);   // empty request
    EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));

    segments.pop_back_segment();
    // "は"
    MakeSegmentsForSuggestion("\xE3\x81\xAF", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));

    segments.pop_back_segment();
    // "た"
    MakeSegmentsForSuggestion("\xE3\x81\x9F", &segments);
    EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));
  }
}

TEST_F(UserHistoryPredictorTest, MultiSegmentsMultiInput) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  Segments segments;

  // "たろうは/太郎は"
  MakeSegmentsForConversion("\xE3\x81\x9F\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF", &segments);
  AddCandidate(0, "\xE5\xA4\xAA\xE9\x83\x8E\xE3\x81\xAF", &segments);
  predictor->Finish(&segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  // "はなこに/花子に"
  MakeSegmentsForConversion("\xE3\x81\xAF\xE3\x81\xAA"
                            "\xE3\x81\x93\xE3\x81\xAB", &segments);
  AddCandidate(1, "\xE8\x8A\xB1\xE5\xAD\x90\xE3\x81\xAB", &segments);
  predictor->Finish(&segments);
  segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);

  // "むずかしい/難しい"
  segments.clear_conversion_segments();
  MakeSegmentsForConversion("\xE3\x82\x80\xE3\x81\x9A"
                            "\xE3\x81\x8B\xE3\x81\x97"
                            "\xE3\x81\x84", &segments);
  AddCandidate(2, "\xE9\x9B\xA3\xE3\x81\x97\xE3\x81\x84", &segments);
  predictor->Finish(&segments);
  segments.mutable_segment(2)->set_segment_type(Segment::HISTORY);

  // "ほんを/本を"
  segments.clear_conversion_segments();
  MakeSegmentsForConversion("\xE3\x81\xBB"
                            "\xE3\x82\x93\xE3\x82\x92", &segments);
  AddCandidate(3, "\xE6\x9C\xAC\xE3\x82\x92", &segments);
  predictor->Finish(&segments);
  segments.mutable_segment(3)->set_segment_type(Segment::HISTORY);

  // "よませた/読ませた"
  segments.clear_conversion_segments();
  MakeSegmentsForConversion("\xE3\x82\x88\xE3\x81\xBE"
                            "\xE3\x81\x9B\xE3\x81\x9F", &segments);
  AddCandidate(4, "\xE8\xAA\xAD\xE3\x81\xBE"
               "\xE3\x81\x9B\xE3\x81\x9F", &segments);
  predictor->Finish(&segments);

  // "た", Too short inputs
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\x9F", &segments);
  EXPECT_FALSE(predictor->Predict(&segments));

  // "たろうは"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\x9F\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  // "ろうは", suggests only from segment boundary.
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF", &segments);
  EXPECT_FALSE(predictor->Predict(&segments));

  // "たろうははな"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\x9F\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF"
                            "\xE3\x81\xAF\xE3\x81\xAA", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  // "はなこにむ"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\xAF\xE3\x81\xAA"
                            "\xE3\x81\x93\xE3\x81\xAB\xE3\x82\x80", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  // "むずかし"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x82\x80\xE3\x81\x9A"
                            "\xE3\x81\x8B\xE3\x81\x97", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  // "はなこにむずかしいほ"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\xAF\xE3\x81\xAA"
                            "\xE3\x81\x93\xE3\x81\xAB"
                            "\xE3\x82\x80\xE3\x81\x9A"
                            "\xE3\x81\x8B\xE3\x81\x97"
                            "\xE3\x81\x84\xE3\x81\xBB", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  // "ほんをよま"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\xBB\xE3\x82\x93"
                            "\xE3\x82\x92\xE3\x82\x88\xE3\x81\xBE", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  Util::Sleep(1000);

  // Add new entry "たろうはよしこに/太郎は良子に"
  segments.Clear();
  MakeSegmentsForConversion("\xE3\x81\x9F\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF", &segments);
  AddCandidate(0, "\xE5\xA4\xAA\xE9\x83\x8E\xE3\x81\xAF", &segments);
  predictor->Finish(&segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  MakeSegmentsForConversion("\xE3\x82\x88\xE3\x81\x97"
                            "\xE3\x81\x93\xE3\x81\xAB", &segments);
  AddCandidate(1, "\xE8\x89\xAF\xE5\xAD\x90\xE3\x81\xAB", &segments);
  predictor->Finish(&segments);
  segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);

  // "たろうは"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\x9F\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_EQ("\xE5\xA4\xAA\xE9\x83\x8E\xE3\x81\xAF"
            "\xE8\x89\xAF\xE5\xAD\x90\xE3\x81\xAB",
            segments.segment(0).candidate(0).value);
}

TEST_F(UserHistoryPredictorTest, MultiSegmentsSingleInput) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

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

  predictor->Finish(&segments);

  // "たろうは"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\x9F\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  // "た", Too short input
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\x9F", &segments);
  EXPECT_FALSE(predictor->Predict(&segments));

  // "たろうははな"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\x9F\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF"
                            "\xE3\x81\xAF\xE3\x81\xAA", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  // "ろうははな", suggest only from segment boundary
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF"
                            "\xE3\x81\xAF\xE3\x81\xAA", &segments);
  EXPECT_FALSE(predictor->Predict(&segments));

  // "はなこにむ"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\xAF\xE3\x81\xAA"
                            "\xE3\x81\x93\xE3\x81\xAB\xE3\x82\x80", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  // "むずかし"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x82\x80\xE3\x81\x9A"
                            "\xE3\x81\x8B\xE3\x81\x97", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  // "はなこにむずかしいほ"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\xAF\xE3\x81\xAA"
                            "\xE3\x81\x93\xE3\x81\xAB"
                            "\xE3\x82\x80\xE3\x81\x9A"
                            "\xE3\x81\x8B\xE3\x81\x97"
                            "\xE3\x81\x84\xE3\x81\xBB", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  // "ほんをよま"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\xBB\xE3\x82\x93"
                            "\xE3\x82\x92\xE3\x82\x88\xE3\x81\xBE", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  Util::Sleep(1000);

  // Add new entry "たろうはよしこに/太郎は良子に"
  segments.Clear();
  MakeSegmentsForConversion("\xE3\x81\x9F\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF", &segments);
  AddCandidate(0, "\xE5\xA4\xAA\xE9\x83\x8E\xE3\x81\xAF", &segments);
  predictor->Finish(&segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  MakeSegmentsForConversion("\xE3\x82\x88\xE3\x81\x97"
                            "\xE3\x81\x93\xE3\x81\xAB", &segments);
  AddCandidate(1, "\xE8\x89\xAF\xE5\xAD\x90\xE3\x81\xAB", &segments);
  predictor->Finish(&segments);
  segments.mutable_segment(1)->set_segment_type(Segment::HISTORY);

  // "たろうは"
  segments.Clear();
  MakeSegmentsForSuggestion("\xE3\x81\x9F\xE3\x82\x8D"
                            "\xE3\x81\x86\xE3\x81\xAF", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_EQ("\xE5\xA4\xAA\xE9\x83\x8E\xE3\x81\xAF"
            "\xE8\x89\xAF\xE5\xAD\x90\xE3\x81\xAB",
            segments.segment(0).candidate(0).value);
}

TEST_F(UserHistoryPredictorTest, Regression2843371_Case1) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

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

  predictor->Finish(&segments);

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

  predictor->Finish(&segments);

  segments.Clear();

  // "とうきょうは"
  MakeSegmentsForSuggestion("\xE3\x81\xA8\xE3\x81\x86"
                            "\xE3\x81\x8D\xE3\x82\x87"
                            "\xE3\x81\x86\xE3\x81\xAF\xE3\x80\x81",
                            &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  // "東京は、日本です"
  EXPECT_EQ("\xE6\x9D\xB1\xE4\xBA\xAC"
            "\xE3\x81\xAF\xE3\x80\x81"
            "\xE6\x97\xA5\xE6\x9C\xAC"
            "\xE3\x81\xA7\xE3\x81\x99",
            segments.segment(0).candidate(0).value);
}

TEST_F(UserHistoryPredictorTest, Regression2843371_Case2) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

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

  predictor->Finish(&segments);

  segments.Clear();

  // "えど("
  MakeSegmentsForSuggestion("\xE3\x81\x88\xE3\x81\xA9(", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  EXPECT_EQ("\xE6\xB1\x9F\xE6\x88\xB8(\xE6\x9D\xB1\xE4\xBA\xAC",
            segments.segment(0).candidate(0).value);

  EXPECT_TRUE(predictor->Predict(&segments));

  // "江戸(東京"
  EXPECT_EQ("\xE6\xB1\x9F\xE6\x88\xB8(\xE6\x9D\xB1\xE4\xBA\xAC",
            segments.segment(0).candidate(0).value);
}

TEST_F(UserHistoryPredictorTest, Regression2843371_Case3) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

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

  predictor->Finish(&segments);

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

  predictor->Finish(&segments);

  segments.Clear();

  // "「やま」は"
  MakeSegmentsForSuggestion("\xE3\x80\x8C\xE3\x82\x84"
                            "\xE3\x81\xBE\xE3\x80\x8D\xE3\x81\xAF",
                            &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  // "「山」は高い"
  EXPECT_EQ("\xE3\x80\x8C\xE5\xB1\xB1\xE3\x80\x8D"
            "\xE3\x81\xAF\xE9\xAB\x98\xE3\x81\x84",
            segments.segment(0).candidate(0).value);
}

TEST_F(UserHistoryPredictorTest, Regression2843775) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

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

  predictor->Finish(&segments);

  segments.Clear();

  // "そうです"
  MakeSegmentsForSuggestion("\xE3\x81\x9D\xE3\x81\x86"
                            "\xE3\x81\xA7\xE3\x81\x99", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

  // "そうです。よろしくお願いします"
  EXPECT_EQ("\xE3\x81\x9D\xE3\x81\x86"
            "\xE3\x81\xA7\xE3\x81\x99"
            "\xE3\x80\x82\xE3\x82\x88"
            "\xE3\x82\x8D\xE3\x81\x97"
            "\xE3\x81\x8F\xE3\x81\x8A"
            "\xE9\xA1\x98\xE3\x81\x84"
            "\xE3\x81\x97\xE3\x81\xBE"
            "\xE3\x81\x99",
            segments.segment(0).candidate(0).value);
}

TEST_F(UserHistoryPredictorTest, DuplicateString) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

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

  predictor->Finish(&segments);

  segments.Clear();

  // "ぞうりむし"
  MakeSegmentsForSuggestion(
      "\xE3\x81\x9E\xE3\x81\x86\xE3\x82\x8A\xE3\x82\x80\xE3\x81\x97",
      &segments);
  EXPECT_TRUE(predictor->Predict(&segments));

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
  EXPECT_TRUE(predictor->Predict(&segments));

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

TEST_F(UserHistoryPredictorTest, SyncTest) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();

  vector<Command> commands(10000);
  for (size_t i = 0; i < commands.size(); ++i) {
    commands[i].key = NumberUtil::SimpleItoa(static_cast<uint32>(i)) + "key";
    commands[i].value = NumberUtil::SimpleItoa(static_cast<uint32>(i)) +
                        "value";
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
        predictor->Finish(&segments);
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
  // "絵文字"
  entry.set_description("\xE7\xB5\xB5\xE6\x96\x87\xE5\xAD\x97");
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
  EXPECT_TRUE(predictor->IsValidEntryIgnoringRemovedField(
      entry, Request::KDDI_EMOJI));

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
      EXPECT_TRUE(queue.NewEntry());
    }
  }

  {
    UserHistoryPredictor::EntryPriorityQueue queue;
    vector<UserHistoryPredictor::Entry *> expected;
    for (int i = 0; i < kSize; ++i) {
      UserHistoryPredictor::Entry *entry = queue.NewEntry();
      entry->set_key("test" + NumberUtil::SimpleItoa(i));
      entry->set_value("test" + NumberUtil::SimpleItoa(i));
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
    // "０００７"
    "\xef\xbc\x90\xef\xbc\x90\xef\xbc\x90\xef\xbc\x97"
  }, {
    kNonSensitive,
    "Type a ZIP number.",
    "100-0001",
    // "東京都千代田区千代田"
    "\xE6\x9D\xB1\xE4\xBA\xAC\xE9\x83\xBD\xE5\x8D\x83\xE4\xBB\xA3"
    "\xE7\x94\xB0\xE5\x8C\xBA\xE5\x8D\x83\xE4\xBB\xA3\xE7\x94\xB0"
  }, {
    kNonSensitive,  // We might want to revisit this behavior
    "Type privacy sensitive number but the result contains one or more "
    "non-ASCII character such as full-width dash.",
    "1111-1111",
    // "1111－1111"
    "1111" "\xEF\xBC\x8D" "1111"
  }, {
    kNonSensitive,  // We might want to revisit this behavior
    "User dictionary contains a credit card number.",
    // "かーどばんごう"
    "\xE3\x81\x8B\xE3\x83\xBC\xE3\x81\xA9\xE3\x81\xB0\xE3\x82\x93"
    "\xE3\x81\x94\xE3\x81\x86",
    "0000-0000-0000-0000"
  }, {
    kNonSensitive,  // We might want to revisit this behavior
    "User dictionary contains a credit card number.",
    // "かーどばんごう"
    "\xE3\x81\x8B\xE3\x83\xBC\xE3\x81\xA9\xE3\x81\xB0\xE3\x82\x93"
    "\xE3\x81\x94\xE3\x81\x86",
    "0000000000000000"
  }, {
    kNonSensitive,  // We might want to revisit this behavior
    "User dictionary contains privacy sensitive information.",
    // "ぱすわーど"
    "\xE3\x81\xB1\xE3\x81\x99\xE3\x82\x8F\xE3\x83\xBC\xE3\x81\xA9",
    "ywwz1sxm"
  }, {
    kNonSensitive,  // We might want to revisit this behavior
    "Input privacy sensitive text by Roman-input mode by mistake and then "
    "hit F10 key to convert it to half-alphanumeric text. In this case "
    "we assume all the alphabetical characters are consumed by Roman-input "
    "rules.",
    // "いあ1ぼ3ぅ"
    "\343\201\204\343\201\202" "1" "\343\201\274" "3" "\343\201\205",
    "ia1bo3xu"
  }, {
    kNonSensitive,
    "Katakana to English transliteration.",  // http://b/4394325
    // "おれんじ"
      "\xE3\x81\x8A\xE3\x82\x8C\xE3\x82\x93\xE3\x81\x98",
    "Orange"
  }, {
    kNonSensitive,
    "Input a very common English word which should be included in our "
    "system dictionary by Roman-input mode by mistake and "
    "then hit F10 key to convert it to half-alphanumeric text.",
    // おらんげ"
    "\xE3\x81\x8A\xE3\x82\x89\xE3\x82\x93\xE3\x81\x92",
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
    // "yっwz1sxm"
    "y" "\343\201\243" "wz1sxm",
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
        kEnglishWords[i], kEnglishWords[i], kEnglishWords[i],
        Node::DEFAULT_ATTRIBUTE);
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
      predictor->Finish(&segments);
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
    MakeSegmentsForConversion("abc!", &segments);
    AddCandidate(0, "123", &segments);
    AddCandidate(1, "abc!", &segments);
    predictor->Finish(&segments);
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
  // EXPECT_TRUE(UserHistoryPredictor::MaybeRomanMisspelledKey(
  // "こんぴゅーｔ"));
  EXPECT_TRUE(UserHistoryPredictor::MaybeRomanMisspelledKey(
      "\xE3\x81\x93\xE3\x82\x93\xE3\x81\xB4"
      "\xE3\x82\x85\xE3\x83\xBC\xEF\xBD\x94"));

  // EXPECT_TRUE(UserHistoryPredictor::MaybeRomanMisspelledKey(
  // "こんぴゅーt"));
  EXPECT_TRUE(UserHistoryPredictor::MaybeRomanMisspelledKey(
      "\xE3\x81\x93\xE3\x82\x93\xE3\x81\xB4"
      "\xE3\x82\x85\xE3\x83\xBCt"));

  // EXPECT_FALSE(UserHistoryPredictor::MaybeRomanMisspelledKey(
  // "こんぴゅーた"));
  EXPECT_FALSE(UserHistoryPredictor::MaybeRomanMisspelledKey(
      "\xE3\x81\x93\xE3\x82\x93\xE3\x81\xB4"
      "\xE3\x82\x85\xE3\x83\xBC\xE3\x81\x9F"));

  // EXPECT_TRUE(UserHistoryPredictor::MaybeRomanMisspelledKey(
  // "ぱｓこん"));
  EXPECT_TRUE(UserHistoryPredictor::MaybeRomanMisspelledKey(
      "\xE3\x81\xB1\xEF\xBD\x93\xE3\x81\x93\xE3\x82\x93"));

  // EXPECT_FALSE(UserHistoryPredictor::MaybeRomanMisspelledKey(
  // "ぱそこん"));
  EXPECT_FALSE(UserHistoryPredictor::MaybeRomanMisspelledKey(
      "\xE3\x81\xB1\xE3\x81\x9D\xE3\x81\x93\xE3\x82\x93"));

  // EXPECT_TRUE(UserHistoryPredictor::MaybeRomanMisspelledKey(
  // "おねがいしまうｓ"));
  EXPECT_TRUE(UserHistoryPredictor::MaybeRomanMisspelledKey(
      "\xE3\x81\x8A\xE3\x81\xAD\xE3\x81\x8C"
      "\xE3\x81\x84\xE3\x81\x97\xE3\x81\xBE"
      "\xE3\x81\x86\xEF\xBD\x93"));

  // EXPECT_FALSE(UserHistoryPredictor::MaybeRomanMisspelledKey(
  // "おねがいします"));
  EXPECT_FALSE(UserHistoryPredictor::MaybeRomanMisspelledKey(
      "\xE3\x81\x8A\xE3\x81\xAD\xE3\x81\x8C"
      "\xE3\x81\x84\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x99"));

  // EXPECT_TRUE(UserHistoryPredictor::MaybeRomanMisspelledKey(
  // "いんた=ねっと"));
  EXPECT_TRUE(UserHistoryPredictor::MaybeRomanMisspelledKey(
      "\xE3\x81\x84\xE3\x82\x93\xE3\x81\x9F"
      "="
      "\xE3\x81\xAD\xE3\x81\xA3\xE3\x81\xA8"));

  // EXPECT_FALSE(UserHistoryPredictor::MaybeRomanMisspelledKey("ｔ"));
  EXPECT_FALSE(UserHistoryPredictor::MaybeRomanMisspelledKey(
      "\xEF\xBD\x94"));

  // EXPECT_TRUE(UserHistoryPredictor::MaybeRomanMisspelledKey("ーｔ"));
  EXPECT_TRUE(UserHistoryPredictor::MaybeRomanMisspelledKey(
      "\xE3\x83\xBC\xEF\xBD\x94"));

  // Two alphas
  // EXPECT_FALSE(UserHistoryPredictor::MaybeRomanMisspelledKey(
  // "おｎがいしまうｓ"));
  EXPECT_FALSE(UserHistoryPredictor::MaybeRomanMisspelledKey(
      "\xE3\x81\x8A\xEF\xBD\x8E\xE3\x81\x8C\xE3\x81\x84"
      "\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x86\xEF\xBD\x93"));
  // Two unknowns
  // EXPECT_FALSE(UserHistoryPredictor::MaybeRomanMisspelledKey(
  // "お＆がい＄しまう"));
  EXPECT_FALSE(UserHistoryPredictor::MaybeRomanMisspelledKey(
      "\xE3\x81\x8A\xEF\xBC\x86\xE3\x81\x8C"
      "\xE3\x81\x84\xEF\xBC\x84\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x86"));
  // One alpha and one unknown
  // EXPECT_FALSE(UserHistoryPredictor::MaybeRomanMisspelledKey(
  // "お＆がいしまうｓ"));
  EXPECT_FALSE(UserHistoryPredictor::MaybeRomanMisspelledKey(
      "\xE3\x81\x8A\xEF\xBC\x86\xE3\x81\x8C\xE3\x81\x84"
      "\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x86\xEF\xBD\x93"));
}

TEST_F(UserHistoryPredictorTest, GetRomanMisspelledKey) {
  Segments segments;
  Segment *seg = segments.add_segment();
  seg->set_segment_type(Segment::FREE);
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->value = "test";

  config::Config config;
  config.set_preedit_method(config::Config::ROMAN);
  config::ConfigHandler::SetConfig(config);

  seg->set_key("");
  EXPECT_EQ("", UserHistoryPredictor::GetRomanMisspelledKey(segments));

  //  seg->set_key("おねがいしまうs");
  seg->set_key("\xE3\x81\x8A\xE3\x81\xAD\xE3\x81\x8C"
               "\xE3\x81\x84\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x86s");
  EXPECT_EQ("onegaisimaus",
            UserHistoryPredictor::GetRomanMisspelledKey(segments));

  //  seg->set_key("おねがいします");
  seg->set_key("\xE3\x81\x8A\xE3\x81\xAD\xE3\x81\x8C"
               "\xE3\x81\x84\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x99");
  EXPECT_EQ("", UserHistoryPredictor::GetRomanMisspelledKey(segments));

  config.set_preedit_method(config::Config::KANA);
  config::ConfigHandler::SetConfig(config);

  //  seg->set_key("おねがいします");
  seg->set_key("\xE3\x81\x8A\xE3\x81\xAD\xE3\x81\x8C"
               "\xE3\x81\x84\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x99");
  EXPECT_EQ("", UserHistoryPredictor::GetRomanMisspelledKey(segments));
}


TEST_F(UserHistoryPredictorTest, RomanFuzzyLookupEntry) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  UserHistoryPredictor::Entry entry;
  UserHistoryPredictor::EntryPriorityQueue results;

  entry.set_key("");
  EXPECT_FALSE(predictor->RomanFuzzyLookupEntry("", &entry, &results));

  //  entry.set_key("よろしく");
  entry.set_key("\xE3\x82\x88\xE3\x82\x8D\xE3\x81\x97\xE3\x81\x8F");
  EXPECT_TRUE(predictor->RomanFuzzyLookupEntry("yorosku", &entry, &results));
  EXPECT_TRUE(predictor->RomanFuzzyLookupEntry("yrosiku", &entry, &results));
  EXPECT_TRUE(predictor->RomanFuzzyLookupEntry("yorsiku", &entry, &results));
  EXPECT_FALSE(predictor->RomanFuzzyLookupEntry("yrsk", &entry, &results));
  EXPECT_FALSE(predictor->RomanFuzzyLookupEntry("yorosiku", &entry, &results));

  // entry.set_key("ぐーぐる");
  entry.set_key("\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B");
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
  scoped_ptr<Trie<string> > expanded(new Trie<string>);
  // "か"
  expanded->AddEntry("\xe3\x81\x8b", "");
  // "き"
  expanded->AddEntry("\xe3\x81\x8d", "");
  // "く"
  expanded->AddEntry("\xe3\x81\x8f", "");
  // "け"
  expanded->AddEntry("\xe3\x81\x91", "");
  // "こ"
  expanded->AddEntry("\xe3\x81\x93", "");

  const LookupTestData kTests1[] = {
    { "", false },
    // "あか"
    { "\xe3\x81\x82\xe3\x81\x8b", true },
    // "あき"
    { "\xe3\x81\x82\xe3\x81\x8d", true },
    // "あかい"
    { "\xe3\x81\x82\xe3\x81\x8b\xe3\x81\x84", true },
    // "あまい"
    { "\xe3\x81\x82\xe3\x81\xbe\xe3\x81\x84", false },
    // "あ"
    { "\xe3\x81\x82", false },
    // "さか"
    { "\xe3\x81\x95\xe3\x81\x8b", false },
    // "さき"
    { "\xe3\x81\x95\xe3\x81\x8d", false },
    // "さかい"
    { "\xe3\x81\x95\xe3\x81\x8b\xe3\x81\x84", false },
    // "さまい"
    { "\xe3\x81\x95\xe3\x81\xbe\xe3\x81\x84", false },
    // "さ"
    { "\xe3\x81\x95", false },
  };

  // with expanded
  for (size_t i = 0; i < arraysize(kTests1); ++i) {
    entry.set_key(kTests1[i].entry_key);
    EXPECT_EQ(kTests1[i].expect_result, predictor->LookupEntry(
        // "あｋ", "あ"
        "\xe3\x81\x82\xef\xbd\x8b", "\xe3\x81\x82",
        expanded.get(), &entry, NULL, &results))
        << kTests1[i].entry_key;
  }

  // only expanded
  // preedit: "k"
  // input_key: ""
  // key_base: ""
  // key_expanded: "か","き","く","け", "こ"

  const LookupTestData kTests2[] = {
    { "", false },
    // "か"
    { "\xe3\x81\x8b", true },
    // "き"
    { "\xe3\x81\x8d", true },
    // "かい"
    { "\xe3\x81\x8b\xe3\x81\x84", true },
    // "まい"
    { "\xe3\x81\xbe\xe3\x81\x84", false },
    // "も"
    { "\xe3\x82\x82", false },
  };

  for (size_t i = 0; i < arraysize(kTests2); ++i) {
    entry.set_key(kTests2[i].entry_key);
    EXPECT_EQ(kTests2[i].expect_result, predictor->LookupEntry(
        "", "", expanded.get(), &entry, NULL, &results))
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
  scoped_ptr<Trie<string> > expanded(new Trie<string>);
  // "し"
  expanded->AddEntry("\xe3\x81\x97", "");
  // "じ"
  expanded->AddEntry("\xe3\x81\x98", "");

  const LookupTestData kTests1[] = {
    { "", false },
    // "あ"
    { "\xe3\x81\x82", false },
    // "あし"
    { "\xe3\x81\x82\xe3\x81\x97", true },
    // "あじ"
    { "\xe3\x81\x82\xe3\x81\x98", true },
    // "あしかゆい"
    { "\xe3\x81\x82\xe3\x81\x97\xe3\x81\x8b\xe3\x82\x86\xe3\x81\x84", true },
    // "あじうまい"
    { "\xe3\x81\x82\xe3\x81\x98\xe3\x81\x86\xe3\x81\xbe\xe3\x81\x84", true },
    // "あまにがい"
    { "\xe3\x81\x82\xe3\x81\xbe\xe3\x81\xab\xe3\x81\x8c\xe3\x81\x84", false },
    // "あめ"
    { "\xe3\x81\x82\xe3\x82\x81", false },
    // "まし"
    { "\xe3\x81\xbe\xe3\x81\x97", false },
    // "まじ"
    { "\xe3\x81\xbe\xe3\x81\x98", false },
    // "ましなあじ"
    { "\xe3\x81\xbe\xe3\x81\x97\xe3\x81\xaa\xe3\x81\x82\xe3\x81\x98", false },
    // "まじうまい"
    { "\xe3\x81\xbe\xe3\x81\x98\xe3\x81\x86\xe3\x81\xbe\xe3\x81\x84", false },
    // "ままにがい"
    { "\xe3\x81\xbe\xe3\x81\xbe\xe3\x81\xab\xe3\x81\x8c\xe3\x81\x84", false },
    // "まめ"
    { "\xe3\x81\xbe\xe3\x82\x81", false },
  };

  // with expanded
  for (size_t i = 0; i < arraysize(kTests1); ++i) {
    entry.set_key(kTests1[i].entry_key);
    EXPECT_EQ(kTests1[i].expect_result, predictor->LookupEntry(
        // "あし", "あ"
        "\xe3\x81\x82\xe3\x81\x97", "\xe3\x81\x82",
        expanded.get(), &entry, NULL, &results))
        << kTests1[i].entry_key;
  }

  // only expanded
  // input_key: "し"
  // key_base: ""
  // key_expanded: "し","じ"
  const LookupTestData kTests2[] = {
    { "", false },
    // "し"
    { "\xe3\x81\x97", true },
    // "じ"
    { "\xe3\x81\x98", true },
    // "しかうまい"
    { "\xe3\x81\x97\xe3\x81\x8b\xe3\x81\x86\xe3\x81\xbe\xe3\x81\x84", true },
    // "じゅうかい"
    { "\xe3\x81\x98\xe3\x82\x85\xe3\x81\x86\xe3\x81\x8b\xe3\x81\x84", true },
    // "ま"
    { "\xe3\x81\xbe", false },
    // "まめ"
    { "\xe3\x81\xbe\xe3\x82\x81", false },
  };

  for (size_t i = 0; i < arraysize(kTests2); ++i) {
    entry.set_key(kTests2[i].entry_key);
    EXPECT_EQ(kTests2[i].expect_result, predictor->LookupEntry(
        // "し"
        "\xe3\x81\x97", "", expanded.get(), &entry, NULL, &results))
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
  scoped_ptr<Trie<string> > expanded(new Trie<string>);
  // "か", "か"
  expanded->AddEntry("\xe3\x81\x8b", "\xe3\x81\x8b");
  // "き", "き"
  expanded->AddEntry("\xe3\x81\x8d", "\xe3\x81\x8d");
  // "く", "く"
  expanded->AddEntry("\xe3\x81\x8f", "\xe3\x81\x8f");
  // "け", "け"
  expanded->AddEntry("\xe3\x81\x91", "\xe3\x81\x91");
  // "こ", "こ"
  expanded->AddEntry("\xe3\x81\x93", "\xe3\x81\x93");

  const MatchTypeTestData kTests1[] = {
    { "", UserHistoryPredictor::NO_MATCH },
    // "い"
    { "\xe3\x81\x84", UserHistoryPredictor::NO_MATCH },
    // "あ"
    { "\xe3\x81\x82", UserHistoryPredictor::RIGHT_PREFIX_MATCH },
    // "あい"
    { "\xe3\x81\x82\xe3\x81\x84", UserHistoryPredictor::NO_MATCH },
    // "あか"
    { "\xe3\x81\x82\xe3\x81\x8b", UserHistoryPredictor::LEFT_PREFIX_MATCH },
    // "あかい"
    { "\xe3\x81\x82\xe3\x81\x8b\xe3\x81\x84",
      UserHistoryPredictor::LEFT_PREFIX_MATCH },
  };

  for (size_t i = 0; i < arraysize(kTests1); ++i) {
    EXPECT_EQ(kTests1[i].expect_type,
              UserHistoryPredictor::GetMatchTypeFromInput(
                  // "あ", "あ"
                  "\xe3\x81\x82", "\xe3\x81\x82",
                  expanded.get(), kTests1[i].target))
        << kTests1[i].target;
  }

  // only expanded
  // preedit: "ｋ"
  // input_key: ""
  // key_base: ""
  // key_expanded: "か","き","く","け", "こ"
  const MatchTypeTestData kTests2[] = {
    { "", UserHistoryPredictor::NO_MATCH },
    // "い"
    { "\xe3\x81\x84", UserHistoryPredictor::NO_MATCH },
    // "いか"
    { "\xe3\x81\x84\xe3\x81\x8b", UserHistoryPredictor::NO_MATCH },
    // "か"
    { "\xe3\x81\x8b", UserHistoryPredictor::LEFT_PREFIX_MATCH },
    // "かいがい"
    { "\xe3\x81\x8b\xe3\x81\x84\xe3\x81\x8c\xe3\x81\x84",
      UserHistoryPredictor::LEFT_PREFIX_MATCH },
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
  scoped_ptr<Trie<string> > expanded(new Trie<string>);
  // "し", "し"
  expanded->AddEntry("\xe3\x81\x97", "\xe3\x81\x97");
  // "じ", "じ"
  expanded->AddEntry("\xe3\x81\x98", "\xe3\x81\x98");

  const MatchTypeTestData kTests1[] = {
    { "", UserHistoryPredictor::NO_MATCH },
    // "い"
    { "\xe3\x81\x84", UserHistoryPredictor::NO_MATCH },
    // "いし"
    { "\xe3\x81\x84\xe3\x81\x97", UserHistoryPredictor::NO_MATCH },
    // "あ"
    { "\xe3\x81\x82", UserHistoryPredictor::RIGHT_PREFIX_MATCH },
    // "あし"
    { "\xe3\x81\x82\xe3\x81\x97", UserHistoryPredictor::EXACT_MATCH },
    // "あじ"
    { "\xe3\x81\x82\xe3\x81\x98",
      UserHistoryPredictor::LEFT_PREFIX_MATCH },
    // "あした"
    { "\xe3\x81\x82\xe3\x81\x97\xe3\x81\x9f",
      UserHistoryPredictor::LEFT_PREFIX_MATCH },
    // "あじしお"
    { "\xe3\x81\x82\xe3\x81\x98\xe3\x81\x97\xe3\x81\x8a",
      UserHistoryPredictor::LEFT_PREFIX_MATCH },
  };

  for (size_t i = 0; i < arraysize(kTests1); ++i) {
    EXPECT_EQ(kTests1[i].expect_type,
              UserHistoryPredictor::GetMatchTypeFromInput(
                  // "あし", "あ"
                  "\xe3\x81\x82\xe3\x81\x97", "\xe3\x81\x82",
                  expanded.get(), kTests1[i].target))
        << kTests1[i].target;
  }

  // only expanded
  // preedit: "し"
  // input_key: "し"
  // key_base: ""
  // key_expanded: "し","じ"
  const MatchTypeTestData kTests2[] = {
    { "", UserHistoryPredictor::NO_MATCH },
    // "い"
    { "\xe3\x81\x84", UserHistoryPredictor::NO_MATCH },
    // "し"
    { "\xe3\x81\x97", UserHistoryPredictor::EXACT_MATCH },
    // "じ"
    { "\xe3\x81\x98", UserHistoryPredictor::LEFT_PREFIX_MATCH },
    // "しじみ"
    { "\xe3\x81\x97\xe3\x81\x98\xe3\x81\xbf",
      UserHistoryPredictor::LEFT_PREFIX_MATCH },
    // "じかん"
    { "\xe3\x81\x98\xe3\x81\x8b\xe3\x82\x93",
      UserHistoryPredictor::LEFT_PREFIX_MATCH },
  };

  for (size_t i = 0; i < arraysize(kTests2); ++i) {
    EXPECT_EQ(kTests2[i].expect_type,
              UserHistoryPredictor::GetMatchTypeFromInput(
                  // "し"
                  "\xe3\x81\x97", "", expanded.get(), kTests2[i].target))
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
  scoped_ptr<composer::Table> table(new composer::Table);
  table->LoadFromFile("system://romanji-hiragana.tsv");
  scoped_ptr<composer::Composer> composer(
      new composer::Composer(table.get(), &default_request()));
  ConversionRequest conversion_request;
  Segments segments;

  InitSegmentsFromInputSequence("gu-g",
                                composer.get(),
                                &conversion_request,
                                &segments);

  {
    FLAGS_enable_expansion_for_user_history_predictor = true;
    string input_key;
    string base;
    scoped_ptr<Trie<string> > expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(conversion_request,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
    // "ぐーｇ"
    EXPECT_EQ("\xe3\x81\x90\xe3\x83\xbc\xef\xbd\x87", input_key);
    // "ぐー"
    EXPECT_EQ("\xe3\x81\x90\xe3\x83\xbc", base);
    EXPECT_TRUE(expanded.get() != NULL);
    string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_TRUE(
        // "ぐ"
        expanded->LookUpPrefix("\xe3\x81\x90",
                               &value, &key_length, &has_subtrie));
    // "ぐ"
    EXPECT_EQ("\xe3\x81\x90", value);
  }

  {
    FLAGS_enable_expansion_for_user_history_predictor = false;
    string input_key;
    string base;
    scoped_ptr<Trie<string> > expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(conversion_request,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
    // "ぐー"
    EXPECT_EQ("\xe3\x81\x90\xe3\x83\xbc", input_key);
    // "ぐー"
    EXPECT_EQ("\xe3\x81\x90\xe3\x83\xbc", base);
    EXPECT_TRUE(expanded.get() == NULL);
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
  scoped_ptr<composer::Table> table(new composer::Table);
  table->LoadFromFile("system://romanji-hiragana.tsv");
  scoped_ptr<composer::Composer> composer(
      new composer::Composer(table.get(), &default_request()));
  ConversionRequest conversion_request;
  Segments segments;

  for (size_t i = 0; i < 1000; ++i) {
    composer.reset(new composer::Composer(table.get(), &default_request()));
    const int len = 1 + Util::Random(4);
    DCHECK_GE(len, 1);
    DCHECK_LE(len, 5);
    string input;
    for (size_t j = 0; j < len; ++j) {
      input += GetRandomAscii();
    }
    InitSegmentsFromInputSequence(input,
                                  composer.get(),
                                  &conversion_request,
                                  &segments);
    string input_key;
    string base;
    scoped_ptr<Trie<string> > expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(conversion_request,
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
  scoped_ptr<composer::Table> table(new composer::Table);
  table->LoadFromFile("system://romanji-hiragana.tsv");
  scoped_ptr<composer::Composer> composer(
      new composer::Composer(table.get(), &default_request()));
  ConversionRequest conversion_request;
  Segments segments;

  {
    InitSegmentsFromInputSequence("8,+",
                                  composer.get(),
                                  &conversion_request,
                                  &segments);
    string input_key;
    string base;
    scoped_ptr<Trie<string> > expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(conversion_request,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
  }
}

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsRomanN) {
  FLAGS_enable_expansion_for_user_history_predictor = true;
  scoped_ptr<composer::Table> table(new composer::Table);
  table->LoadFromFile("system://romanji-hiragana.tsv");
  scoped_ptr<composer::Composer> composer(
      new composer::Composer(table.get(), &default_request()));
  scoped_ptr<ConversionRequest> conversion_request;
  Segments segments;

  {
    conversion_request.reset(new ConversionRequest);
    InitSegmentsFromInputSequence("n", composer.get(),
                                  conversion_request.get(), &segments);
    string input_key;
    string base;
    scoped_ptr<Trie<string> > expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*conversion_request,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
    // "ｎ"
    EXPECT_EQ("\xef\xbd\x8e", input_key);
    EXPECT_EQ("", base);
    EXPECT_TRUE(expanded.get() != NULL);
    string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_TRUE(
        // "な"
        expanded->LookUpPrefix("\xe3\x81\xaa",
                               &value, &key_length, &has_subtrie));
    // "な"
    EXPECT_EQ("\xe3\x81\xaa", value);
  }

  composer.reset(new composer::Composer(table.get(), &default_request()));
  segments.Clear();
  {
    conversion_request.reset(new ConversionRequest);
    InitSegmentsFromInputSequence("nn", composer.get(),
                                  conversion_request.get(), &segments);
    string input_key;
    string base;
    scoped_ptr<Trie<string> > expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*conversion_request,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
    // "ん"
    EXPECT_EQ("\xe3\x82\x93", input_key);
    // "ん"
    EXPECT_EQ("\xe3\x82\x93", base);
    EXPECT_TRUE(expanded.get() == NULL);
  }

  composer.reset(new composer::Composer(table.get(), &default_request()));
  segments.Clear();
  {
    conversion_request.reset(new ConversionRequest);
    InitSegmentsFromInputSequence("n'", composer.get(),
                                  conversion_request.get(), &segments);
    string input_key;
    string base;
    scoped_ptr<Trie<string> > expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*conversion_request,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
    // "ん"
    EXPECT_EQ("\xe3\x82\x93", input_key);
    // "ん"
    EXPECT_EQ("\xe3\x82\x93", base);
    EXPECT_TRUE(expanded.get() == NULL);
  }

  composer.reset(new composer::Composer(table.get(), &default_request()));
  segments.Clear();
  {
    conversion_request.reset(new ConversionRequest);
    InitSegmentsFromInputSequence("n'n", composer.get(),
                                  conversion_request.get(), &segments);
    string input_key;
    string base;
    scoped_ptr<Trie<string> > expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(*conversion_request,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
    // "んｎ"
    EXPECT_EQ("\xe3\x82\x93\xef\xbd\x8e", input_key);
    // "ん"
    EXPECT_EQ("\xe3\x82\x93", base);
    EXPECT_TRUE(expanded.get() != NULL);
    string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_TRUE(
        // "な"
        expanded->LookUpPrefix("\xe3\x81\xaa",
                               &value, &key_length, &has_subtrie));
    // "な"
    EXPECT_EQ("\xe3\x81\xaa", value);
  }
}

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsFlickN) {
  FLAGS_enable_expansion_for_user_history_predictor = true;
  scoped_ptr<composer::Table> table(new composer::Table);
  table->LoadFromFile("system://flick-hiragana.tsv");
  scoped_ptr<composer::Composer> composer(
      new composer::Composer(table.get(), &default_request()));
  ConversionRequest conversion_request;
  Segments segments;

  {
    InitSegmentsFromInputSequence("/", composer.get(), &conversion_request,
                                  &segments);
    string input_key;
    string base;
    scoped_ptr<Trie<string> > expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(conversion_request,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
    // "ん"
    EXPECT_EQ("\xe3\x82\x93", input_key);
    EXPECT_EQ("", base);
    EXPECT_TRUE(expanded.get() != NULL);
    string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_TRUE(
        // "ん"
        expanded->LookUpPrefix("\xe3\x82\x93",
                               &value, &key_length, &has_subtrie));
    // "ん"
    EXPECT_EQ("\xe3\x82\x93", value);
  }
}

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegments12KeyN) {
  FLAGS_enable_expansion_for_user_history_predictor = true;
  scoped_ptr<composer::Table> table(new composer::Table);
  table->LoadFromFile("system://12keys-hiragana.tsv");
  scoped_ptr<composer::Composer> composer(
      new composer::Composer(table.get(), &default_request()));
  ConversionRequest conversion_request;
  Segments segments;

  {
    // "わ00"
    InitSegmentsFromInputSequence("\xe3\x82\x8f\x30\x30",
                                  composer.get(),
                                  &conversion_request,
                                  &segments);
    string input_key;
    string base;
    scoped_ptr<Trie<string> > expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(conversion_request,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
    // "ん"
    EXPECT_EQ("\xe3\x82\x93", input_key);
    EXPECT_EQ("", base);
    EXPECT_TRUE(expanded.get() != NULL);
    string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_TRUE(
        // "ん"
        expanded->LookUpPrefix("\xe3\x82\x93",
                               &value, &key_length, &has_subtrie));
    // "ん"
    EXPECT_EQ("\xe3\x82\x93", value);
  }
}

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsKana) {
  scoped_ptr<composer::Table> table(new composer::Table);
  table->LoadFromFile("system://kana.tsv");
  scoped_ptr<composer::Composer> composer(
      new composer::Composer(table.get(), &default_request()));
  ConversionRequest conversion_request;
  Segments segments;

  // "あか"
  InitSegmentsFromInputSequence("\xe3\x81\x82\xe3\x81\x8b",
                                composer.get(), &conversion_request, &segments);

  {
    FLAGS_enable_expansion_for_user_history_predictor = true;
    string input_key;
    string base;
    scoped_ptr<Trie<string> > expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(conversion_request,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
    // "あか"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8b", input_key);
    // "あ"
    EXPECT_EQ("\xe3\x81\x82", base);
    EXPECT_TRUE(expanded.get() != NULL);
    string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_TRUE(
        // "が"
        expanded->LookUpPrefix("\xe3\x81\x8c",
                               &value, &key_length, &has_subtrie));
    // "が"
    EXPECT_EQ("\xe3\x81\x8c", value);
  }

  {
    FLAGS_enable_expansion_for_user_history_predictor = false;
    string input_key;
    string base;
    scoped_ptr<Trie<string> > expanded;
    UserHistoryPredictor::GetInputKeyFromSegments(conversion_request,
                                                  segments,
                                                  &input_key,
                                                  &base,
                                                  &expanded);
    // "あか"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8b", input_key);
    // "あか"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x8b", base);
    EXPECT_TRUE(expanded.get() == NULL);
  }
}

TEST_F(UserHistoryPredictorTest, RealtimeConversionInnerSegment) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  Segments segments;
  {
    // "わたしのなまえはなかのです"
    const char kKey[] = "\xe3\x82\x8f\xe3\x81\x9f\xe3\x81\x97\xe3\x81\xae\xe3"
        "\x81\xaa\xe3\x81\xbe\xe3\x81\x88\xe3\x81\xaf\xe3\x81\xaa\xe3\x81\x8b"
        "\xe3\x81\xae\xe3\x81\xa7\xe3\x81\x99";
    // "私の名前は中野です"
    const char kValue[] = "\xe7\xa7\x81\xe3\x81\xae\xe5\x90\x8d\xe5\x89\x8d"
        "\xe3\x81\xaf\xe4\xb8\xad\xe9\x87\x8e\xe3\x81\xa7\xe3\x81\x99";
    MakeSegmentsForPrediction(kKey, &segments);
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->add_candidate();
    CHECK(candidate);
    candidate->Init();
    candidate->value = kValue;
    candidate->content_value = kValue;
    candidate->key = kKey;
    candidate->content_key = kKey;
    // "わたしの, 私の"
    candidate->inner_segment_boundary.push_back(pair<int, int>(4, 2));
    // "なまえは, 名前は"
    candidate->inner_segment_boundary.push_back(pair<int, int>(4, 3));
    // "なかのです, 中野です"
    candidate->inner_segment_boundary.push_back(pair<int, int>(5, 4));
  }
  predictor->Finish(&segments);
  segments.Clear();

  // "なかの"
  MakeSegmentsForPrediction("\xe3\x81\xaa\xe3\x81\x8b\xe3\x81\xae", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  // "中野です"
  EXPECT_TRUE(FindCandidateByValue(
      "\xe4\xb8\xad\xe9\x87\x8e\xe3\x81\xa7\xe3\x81\x99", segments));

  segments.Clear();
  // "なまえ"
  MakeSegmentsForPrediction("\xe3\x81\xaa\xe3\x81\xbe\xe3\x81\x88", &segments);
  EXPECT_TRUE(predictor->Predict(&segments));
  // "名前は"
  EXPECT_TRUE(FindCandidateByValue(
      "\xe5\x90\x8d\xe5\x89\x8d\xe3\x81\xaf", segments));
  // "名前は中野です"
  EXPECT_TRUE(FindCandidateByValue(
      "\xe5\x90\x8d\xe5\x89\x8d\xe3\x81\xaf\xe4\xb8\xad\xe9\x87\x8e"
      "\xe3\x81\xa7\xe3\x81\x99", segments));
}

TEST_F(UserHistoryPredictorTest, ZeroQueryFromRealtimeConversion) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  Segments segments;
  {
    // "わたしのなまえはなかのです"
    const char kKey[] = "\xe3\x82\x8f\xe3\x81\x9f\xe3\x81\x97\xe3\x81\xae"
        "\xe3\x81\xaa\xe3\x81\xbe\xe3\x81\x88\xe3\x81\xaf\xe3\x81\xaa\xe3"
        "\x81\x8b\xe3\x81\xae\xe3\x81\xa7\xe3\x81\x99";
    // "私の名前は中野です"
    const char kValue[] = "\xe7\xa7\x81\xe3\x81\xae\xe5\x90\x8d\xe5\x89\x8d"
        "\xe3\x81\xaf\xe4\xb8\xad\xe9\x87\x8e\xe3\x81\xa7\xe3\x81\x99";
    MakeSegmentsForPrediction(kKey, &segments);
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->add_candidate();
    CHECK(candidate);
    candidate->Init();
    candidate->value = kValue;
    candidate->content_value = kValue;
    candidate->key = kKey;
    candidate->content_key = kKey;
    // "わたしの, 私の"
    candidate->inner_segment_boundary.push_back(pair<int, int>(4, 2));
    // "なまえは, 名前は"
    candidate->inner_segment_boundary.push_back(pair<int, int>(4, 3));
    // "なかのです, 中野です"
    candidate->inner_segment_boundary.push_back(pair<int, int>(5, 4));
  }
  predictor->Finish(&segments);
  segments.Clear();

  // "わたしの"
  MakeSegmentsForConversion(
      "\xe3\x82\x8f\xe3\x81\x9f\xe3\x81\x97\xe3\x81\xae", &segments);
  // "私の"
  AddCandidate(0, "\xe7\xa7\x81\xe3\x81\xae", &segments);
  predictor->Finish(&segments);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);

  MakeSegmentsForSuggestion("", &segments);   // empty request
  commands::Request request;
  request.set_zero_query_suggestion(true);
  const ConversionRequest conversion_request(NULL, &request);
  EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));
  // "名前は"
  EXPECT_TRUE(FindCandidateByValue(
      "\xe5\x90\x8d\xe5\x89\x8d\xe3\x81\xaf", segments));
}

TEST_F(UserHistoryPredictorTest, LongCandidateForMobile) {
  UserHistoryPredictor *predictor = GetUserHistoryPredictor();
  predictor->WaitForSyncer();
  predictor->ClearAllHistory();
  predictor->WaitForSyncer();

  commands::Request request;
  commands::RequestForUnitTest::FillMobileRequest(&request);
  const ConversionRequest conversion_request(NULL, &request);

  Segments segments;
  for (size_t i = 0; i < 3; ++i) {
    // "よろしくおねがいします"
    const char kKey[] = "\xe3\x82\x88\xe3\x82\x8d\xe3\x81\x97\xe3"
        "\x81\x8f\xe3\x81\x8a\xe3\x81\xad\xe3\x81\x8c\xe3\x81\x84"
        "\xe3\x81\x97\xe3\x81\xbe\xe3\x81\x99";
    // "よろしくお願いします"
    const char kValue[] = "\xe3\x82\x88\xe3\x82\x8d\xe3\x81\x97\xe3"
        "\x81\x8f\xe3\x81\x8a\xe9\xa1\x98\xe3\x81\x84\xe3\x81\x97\xe3"
        "\x81\xbe\xe3\x81\x99";
    MakeSegmentsForPrediction(kKey, &segments);
    Segment::Candidate *candidate =
        segments.mutable_segment(0)->add_candidate();
    CHECK(candidate);
    candidate->Init();
    candidate->value = kValue;
    candidate->content_value = kValue;
    candidate->key = kKey;
    candidate->content_key = kKey;
    predictor->Finish(&segments);
    segments.Clear();
  }

  // "よろ"
  MakeSegmentsForPrediction("\xe3\x82\x88\xe3\x82\x8d", &segments);
  EXPECT_TRUE(predictor->PredictForRequest(conversion_request, &segments));
  EXPECT_TRUE(FindCandidateByValue(
      // "よろしくお願いします"
      "\xe3\x82\x88\xe3\x82\x8d\xe3\x81\x97\xe3\x81\x8f\xe3\x81\x8a\xe9\xa1"
      "\x98\xe3\x81\x84\xe3\x81\x97\xe3\x81\xbe\xe3\x81\x99", segments));
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

  vector<UserHistoryPredictor::Entry *> entries;
  entries.push_back(abc);
  entries.push_back(a);
  entries.push_back(b);
  entries.push_back(c);

  // The method should return NOT_FOUND for key-value pairs not in the chain.
  for (size_t i = 0; i < entries.size(); ++i) {
    vector<StringPiece> dummy1, dummy2;
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
    vector<StringPiece> dummy1, dummy2;
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
    vector<StringPiece> dummy1, dummy2;
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
    vector<StringPiece> dummy1, dummy2;
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
    MakeSegmentsForConversion(
        "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xEF\xBD\x92",  // "ぐーぐｒ"
        &segments);
    AddCandidate(
        "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0\x72",  // "グーグr"
        &segments);
    predictor->Finish(&segments);
  }

  // Test if the predictor learned "グーグr".
  EXPECT_TRUE(IsSuggested(
      predictor,
      "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90",  // "ぐーぐ"
      "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0\x72"));  // "グーグr"
  EXPECT_TRUE(IsPredicted(
      predictor,
      "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90",  // "ぐーぐ"
      "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0\x72"));  // "グーグr"

  // The user tris deleting the history ("ぐーぐｒ", "グーグr").
  EXPECT_TRUE(predictor->ClearHistoryEntry(
      "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xEF\xBD\x92",  // "ぐーぐｒ"
      "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0\x72"));  // "グーグr"

  // The predictor shouldn't show "グーグr" both for suggestion and prediction.
  EXPECT_FALSE(IsSuggested(
      predictor,
      "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90",  // "ぐーぐ"
      "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0\x72"));  // "グーグr"
  EXPECT_FALSE(IsPredicted(
      predictor,
      "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90",  // "ぐーぐ"
      "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0\x72"));  // "グーグr"
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
    seg->set_key(
        // "きょうも"
        "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x82\x82");
    seg->set_segment_type(Segment::FIXED_VALUE);
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "\xE4\xBB\x8A\xE6\x97\xA5\xE3\x82\x82";  // "今日も"
    candidate->content_value = "\xE4\xBB\x8A\xE6\x97\xA5";  // "今日"
    candidate->key = seg->key();
    candidate->content_key =
        "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86";  // "きょう"

    // The second segment: ("いいてんき", "いい天気")
    seg = segments.add_segment();
    seg->set_key(
        // "いいてんき
        "\xE3\x81\x84\xE3\x81\x84\xE3\x81\xA6\xE3\x82\x93\xE3\x81\x8D");
    seg->set_segment_type(Segment::FIXED_VALUE);
    candidate = seg->add_candidate();
    candidate->Init();
    candidate->value =
        // "いい天気"
        "\xE3\x81\x84\xE3\x81\x84\xE5\xA4\xA9\xE6\xB0\x97";
    candidate->content_value = candidate->value;
    candidate->key = seg->key();
    candidate->content_key = seg->key();

    // The third segment: ("！", "!")
    seg = segments.add_segment();
    seg->set_key("\xEF\xBC\x81");  // "！"
    seg->set_segment_type(Segment::FIXED_VALUE);
    candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "!";
    candidate->content_value = "!";
    candidate->key = seg->key();
    candidate->content_key = seg->key();

    predictor->Finish(&segments);
  }

  // Check if the predictor learned the sentence.  Since the symbol is contained
  // in one segment, both "今日もいい天気" and "今日もいい天気!" should be
  // suggested and predicted.
  EXPECT_TRUE(IsSuggestedAndPredicted(
      predictor,
      // "きょうも"
      "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x82\x82",
      // "今日もいい天気"
      "\xE4\xBB\x8A\xE6\x97\xA5\xE3\x82\x82\xE3\x81\x84"
      "\xE3\x81\x84\xE5\xA4\xA9\xE6\xB0\x97"));
  EXPECT_TRUE(IsSuggestedAndPredicted(
      predictor,
      // "きょうも"
      "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x82\x82",
      // "今日もいい天気!"
      "\xE4\xBB\x8A\xE6\x97\xA5\xE3\x82\x82\xE3\x81\x84"
      "\xE3\x81\x84\xE5\xA4\xA9\xE6\xB0\x97\x21"));

  // Now the user deletes the sentence containing the "!".
  EXPECT_TRUE(predictor->ClearHistoryEntry(
      // "きょうもいいてんき！"
      "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x82\x82"
      "\xE3\x81\x84\xE3\x81\x84\xE3\x81\xA6\xE3\x82\x93"
      "\xE3\x81\x8D\xEF\xBC\x81",
      // "今日もいい天気!"
      "\xE4\xBB\x8A\xE6\x97\xA5\xE3\x82\x82\xE3\x81\x84"
      "\xE3\x81\x84\xE5\xA4\xA9\xE6\xB0\x97\x21"));

  // The sentence "今日もいい天気" should still be suggested and predicted.
  EXPECT_TRUE(IsSuggestedAndPredicted(
      predictor,
      // "きょうも"
      "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x82\x82",
      // "今日もいい天気"
      "\xE4\xBB\x8A\xE6\x97\xA5\xE3\x82\x82\xE3\x81\x84"
      "\xE3\x81\x84\xE5\xA4\xA9\xE6\xB0\x97"));

  // However, "今日もいい天気!" should be neither suggested nor predicted.
  EXPECT_FALSE(IsSuggested(
      predictor,
      // "きょうも"
      "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x82\x82",
      // "今日もいい天気!"
      "\xE4\xBB\x8A\xE6\x97\xA5\xE3\x82\x82\xE3\x81\x84"
      "\xE3\x81\x84\xE5\xA4\xA9\xE6\xB0\x97\x21"));
  EXPECT_FALSE(IsPredicted(
      predictor,
      // "きょうも"
      "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x82\x82",
      // "今日もいい天気!"
      "\xE4\xBB\x8A\xE6\x97\xA5\xE3\x82\x82\xE3\x81\x84"
      "\xE3\x81\x84\xE5\xA4\xA9\xE6\xB0\x97\x21"));
}

}  // namespace mozc
