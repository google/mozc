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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
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
#include "absl/log/log.h"
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
#include "converter/inner_segment.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_mock.h"
#include "engine/modules.h"
#include "engine/supplemental_model_mock.h"
#include "prediction/result.h"
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
#include "testing/test_peer.h"

namespace mozc::prediction {
namespace {

using ::mozc::commands::Request;
using ::mozc::composer::TypeCorrectedQuery;
using ::mozc::config::Config;
using ::mozc::dictionary::MockDictionary;
using ::mozc::dictionary::UserDictionaryInterface;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

constexpr int kRevertId = 1234;

}  // namespace

class UserHistoryPredictorTestPeer
    : public testing::TestPeer<UserHistoryPredictor> {
 public:
  explicit UserHistoryPredictorTestPeer(UserHistoryPredictor& predictor)
      : testing::TestPeer<UserHistoryPredictor>(predictor) {}

  PEER_STATIC_METHOD(GetScore);
  PEER_STATIC_METHOD(GetMatchType);
  PEER_STATIC_METHOD(IsValidSuggestion);
  PEER_STATIC_METHOD(IsValidSuggestionForMixedConversion);
  PEER_STATIC_METHOD(RomanFuzzyPrefixMatch);
  PEER_STATIC_METHOD(MaybeRomanMisspelledKey);
  PEER_STATIC_METHOD(GetRomanMisspelledKey);
  PEER_STATIC_METHOD(GetMatchTypeFromInput);
  PEER_STATIC_METHOD(GetInputKeyFromRequest);
  PEER_STATIC_METHOD(EraseNextEntries);
  PEER_STATIC_METHOD(GuessRevertedValueOffset);
  PEER_METHOD(IsValidEntry);
  PEER_METHOD(IsValidEntryIgnoringRemovedField);
  PEER_METHOD(RomanFuzzyLookupEntry);
  PEER_METHOD(LookupEntry);
  PEER_METHOD(RemoveNgramChain);
  PEER_METHOD(WaitForSyncer);
  PEER_METHOD(Save);
  PEER_METHOD(SetEntryLifetimeDays);
  PEER_METHOD(SetCacheStoreSize);
  PEER_VARIABLE(cache_store_size_);
  PEER_VARIABLE(entry_lifetime_days_);
  PEER_VARIABLE(dic_);
  PEER_DECLARE(MatchType);
  PEER_DECLARE(RemoveNgramChainResult);
  PEER_DECLARE(EntryPriorityQueue);
};

// Needs to call UpdateHistoryResult() to update history_result_.
// The reference of history_result_ is shared by ConversionRequest.
class SegmentsProxy {
 public:
  void AddSegment(absl::string_view key) {
    Segment segment;
    segment.key = key;
    segments_.emplace_back(std::move(segment));
    UpdateHistoryResult();
  }

  void MakeSegments(absl::string_view key) {
    segments_.clear();
    AddSegment(key);
    UpdateHistoryResult();
  }

  void AddCandidate(size_t segment_index, absl::string_view value) {
    Result result;
    result.key = segments_[segment_index].key;
    result.value = value;
    segments_[segment_index].results.emplace_back(std::move(result));
    UpdateHistoryResult();
  }

  void SetCandidate(size_t segment_index, int candidate_index,
                    absl::string_view value) {
    segments_[segment_index].results[candidate_index].value = value;
    UpdateHistoryResult();
  }

  void PushBackInnerSegmentBoundary(int segment_index, int candidate_index,
                                    size_t key_len, size_t value_len,
                                    size_t content_key_len,
                                    size_t content_value_len) {
    segments_[segment_index]
        .results[candidate_index]
        .inner_segment_boundary.push_back(
            converter::EncodeLengths(key_len, value_len, content_key_len,
                                     content_value_len)
                .value());
    UpdateHistoryResult();
  }

  void AddCandidateWithDescription(size_t segment_index,
                                   absl::string_view value,
                                   absl::string_view desc) {
    Result result;
    result.value = value;
    result.key = segments_[segment_index].key;
    result.description = desc;
    segments_[segment_index].results.emplace_back(std::move(result));
    UpdateHistoryResult();
  }

  void MoveCandidate(int segment_index, int src, int dst) {
    auto& results = segments_[segment_index].results;
    auto it_old = results.begin() + src;
    auto it_new = results.begin() + dst;
    std::rotate(it_new, it_old, it_old + 1);
    UpdateHistoryResult();
  }

  void PrependHistory(absl::string_view key, absl::string_view value) {
    Segment segment;
    segment.key = key;
    segment.is_history = true;
    Result result;
    result.key = key;
    result.value = value;
    segment.results.emplace_back(std::move(result));
    segments_.emplace_front(std::move(segment));
    UpdateHistoryResult();
  }

  void SetHistoryType(int segment_index) {
    segments_[segment_index].is_history = true;
    UpdateHistoryResult();
  }

  void pop_back_segment() {
    if (!segments_.empty()) segments_.pop_back();
    UpdateHistoryResult();
  }

  void clear_conversion_segments() {
    while (!segments_.empty()) {
      if (segments_.back().is_history) break;
      segments_.pop_back();
    }
    UpdateHistoryResult();
  }

  absl::string_view key() const {
    for (const auto& segment : segments_) {
      if (!segment.is_history) return segment.key;
    }
    return "";
  }

  const Result& history_result() const { return history_result_; }

  std::vector<Result> MakeLearningResults() const {
    return MakeResults(false /* is_history */);
  }

  void Clear() { segments_.clear(); }

 private:
  std::vector<Result> MakeResults(bool is_history) const {
    std::vector<Result> results;
    if (segments_.empty()) return results;

    std::vector<const Segment*> sub_segments;
    for (const auto& segment : segments_) {
      if (segment.is_history != is_history) continue;
      if (segment.results.empty()) return results;
      sub_segments.emplace_back(&segment);
    }

    if (sub_segments.size() == 1) {
      return sub_segments.front()->results;
    } else {
      Result result;
      for (const auto* segment : sub_segments) {
        const Result& top_result = segment->results.front();
        absl::StrAppend(&result.key, top_result.key);
        absl::StrAppend(&result.value, top_result.value);
        absl::StrAppend(&result.description, top_result.description);
        if (top_result.inner_segment_boundary.empty()) {
          result.inner_segment_boundary.push_back(
              converter::EncodeLengths(
                  top_result.key.size(), top_result.value.size(),
                  top_result.key.size(), top_result.value.size())
                  .value());
        } else {
          for (uint32_t encoded : top_result.inner_segment_boundary) {
            result.inner_segment_boundary.push_back(encoded);
          }
        }
      }
      results.emplace_back(std::move(result));
    }

    return results;
  }

  void UpdateHistoryResult() {
    history_result_ = MakeResults(true /* is_history */).front();
  }

  struct Segment {
    std::string key;
    std::vector<Result> results;
    bool is_history = false;
  };

  std::deque<Segment> segments_;
  Result history_result_;
};

class UserHistoryPredictorTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    request_.Clear();
    config::ConfigHandler::GetDefaultConfig(&config_);
    config_.set_use_typing_correction(true);
    table_ = std::make_shared<composer::Table>();
    composer_ = composer::Composer(table_, request_, config_);
    data_and_predictor_ = CreateDataAndPredictor();
  }

  void TearDown() override {}

  ConversionRequest CreateConversionRequestWithOptions(
      const composer::Composer& composer, ConversionRequest::Options options,
      const SegmentsProxy& segments_proxy) const {
    return ConversionRequestBuilder()
        .SetComposer(composer_)
        .SetRequestView(request_)
        .SetContextView(context_)
        .SetConfigView(config_)
        .SetOptions(std::move(options))
        .SetHistoryResultView(segments_proxy.history_result())
        .SetKey(segments_proxy.key())
        .Build();
  }

  ConversionRequest CreateConversionRequest(
      const composer::Composer& composer,
      const SegmentsProxy& segments_proxy) const {
    ConversionRequest::Options options = {
        .max_user_history_prediction_candidates_size = 10,
        .max_user_history_prediction_candidates_size_for_zero_query = 10,
    };
    return ConversionRequestBuilder()
        .SetComposer(composer_)
        .SetRequestView(request_)
        .SetContextView(context_)
        .SetConfigView(config_)
        .SetOptions(std::move(options))
        .SetHistoryResultView(segments_proxy.history_result())
        .SetKey(segments_proxy.key())
        .Build();
  }

  UserHistoryPredictor* GetUserHistoryPredictor() {
    return data_and_predictor_->predictor.get();
  }

  void WaitForSyncer(UserHistoryPredictor* predictor) {
    predictor->WaitForSyncer();
  }

  UserHistoryPredictor* GetUserHistoryPredictorWithClearedHistory() {
    UserHistoryPredictor* predictor = data_and_predictor_->predictor.get();
    predictor->WaitForSyncer();
    predictor->ClearAllHistory();
    predictor->WaitForSyncer();
    return predictor;
  }

  UserDictionaryInterface& GetUserDictionary() {
    return data_and_predictor_->modules->GetUserDictionary();
  }

  bool IsSuggested(const UserHistoryPredictor* predictor,
                   const absl::string_view key, const absl::string_view value) {
    composer::Composer composer;
    composer.SetPreeditTextForTestOnly(key);
    SegmentsProxy segments_proxy;
    segments_proxy.MakeSegments(key);
    const ConversionRequest conversion_request =
        ConversionRequestBuilder()
            .SetComposer(composer)
            .SetHistoryResultView(segments_proxy.history_result())
            .SetRequestType(ConversionRequest::SUGGESTION)
            .Build();
    const std::vector<Result> results = predictor->Predict(conversion_request);
    return !results.empty() && FindCandidateByValue(value, results);
  }

  bool IsPredicted(const UserHistoryPredictor* predictor,
                   const absl::string_view key, const absl::string_view value) {
    composer::Composer composer;
    composer.SetPreeditTextForTestOnly(key);
    SegmentsProxy segments_proxy;
    segments_proxy.MakeSegments(key);

    const ConversionRequest conversion_request =
        ConversionRequestBuilder()
            .SetComposer(composer)
            .SetHistoryResultView(segments_proxy.history_result())
            .SetRequestType(ConversionRequest::PREDICTION)
            .Build();
    const std::vector<Result> results = predictor->Predict(conversion_request);
    return !results.empty() && FindCandidateByValue(value, results);
  }

  bool IsSuggestedAndPredicted(const UserHistoryPredictor* predictor,
                               const absl::string_view key,
                               const absl::string_view value) {
    return IsSuggested(predictor, key, value) &&
           IsPredicted(predictor, key, value);
  }

  static UserHistoryPredictor::Entry* InsertEntry(
      UserHistoryPredictor* predictor, const absl::string_view key,
      const absl::string_view value) {
    UserHistoryPredictor::Entry* e =
        &predictor->dic_->Insert(predictor->Fingerprint(key, value))->value;
    e->set_key(std::string(key));
    e->set_value(std::string(value));
    e->set_removed(false);
    return e;
  }

  static UserHistoryPredictor::Entry* AppendEntry(
      UserHistoryPredictor* predictor, const absl::string_view key,
      const absl::string_view value, UserHistoryPredictor::Entry* prev) {
    prev->add_next_entries()->set_entry_fp(predictor->Fingerprint(key, value));
    UserHistoryPredictor::Entry* e = InsertEntry(predictor, key, value);
    return e;
  }

  static size_t EntrySize(const UserHistoryPredictor& predictor) {
    return predictor.dic_->Size();
  }

  static bool LoadStorage(UserHistoryPredictor* predictor,
                          UserHistoryStorage&& history) {
    return predictor->Load(std::move(history));
  }

  static bool IsConnected(const UserHistoryPredictor::Entry& prev,
                          const UserHistoryPredictor::Entry& next) {
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
  void InitHistory_JapaneseInput(UserHistoryPredictor* predictor,
                                 UserHistoryPredictor::Entry** japaneseinput,
                                 UserHistoryPredictor::Entry** japanese,
                                 UserHistoryPredictor::Entry** input) {
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
      UserHistoryPredictor* predictor,
      UserHistoryPredictor::Entry** japaneseinputmethod,
      UserHistoryPredictor::Entry** japanese,
      UserHistoryPredictor::Entry** input,
      UserHistoryPredictor::Entry** method) {
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
    // TODO(taku): joined suggestion will not work when
    // user_history_prediction_disable_joined_prediction is true.
    // Updates the following tests when this mode is used as default.
    EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "Japanese"));
    EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "japan", "JapaneseInput"));
    EXPECT_TRUE(
        IsSuggestedAndPredicted(predictor, "japan", "JapaneseInputMethod"));
    EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "Input"));
    EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "inpu", "InputMethod"));
    EXPECT_TRUE(IsSuggestedAndPredicted(predictor, "meth", "Method"));
  }

  void SetUpInput(absl::string_view key, composer::Composer* composer,
                  SegmentsProxy* segments_proxy) const {
    composer->Reset();
    composer->SetPreeditTextForTestOnly(key);
    segments_proxy->MakeSegments(key);
  }

  ConversionRequest SetUpInputForSuggestion(
      absl::string_view key, composer::Composer* composer,
      SegmentsProxy* segments_proxy) const {
    SetUpInput(key, composer, segments_proxy);
    ConversionRequest::Options options = {.request_type =
                                              ConversionRequest::SUGGESTION};
    return CreateConversionRequestWithOptions(*composer, std::move(options),
                                              *segments_proxy);
  }

  ConversionRequest SetUpInputForSuggestionWithHistory(
      absl::string_view key, absl::string_view hist_key,
      absl::string_view hist_value, composer::Composer* composer,
      SegmentsProxy* segments_proxy) const {
    const ConversionRequest convreq =
        SetUpInputForSuggestion(key, composer, segments_proxy);
    segments_proxy->PrependHistory(hist_key, hist_value);
    return convreq;
  }

  ConversionRequest SetUpInputForPrediction(
      absl::string_view key, composer::Composer* composer,
      SegmentsProxy* segments_proxy) const {
    SetUpInput(key, composer, segments_proxy);
    ConversionRequest::Options options = {.request_type =
                                              ConversionRequest::PREDICTION};
    return CreateConversionRequestWithOptions(*composer, std::move(options),
                                              *segments_proxy);
  }

  ConversionRequest SetUpInputForPredictionWithHistory(
      absl::string_view key, absl::string_view hist_key,
      absl::string_view hist_value, composer::Composer* composer,
      SegmentsProxy* segments_proxy) const {
    const ConversionRequest convreq =
        SetUpInputForPrediction(key, composer, segments_proxy);
    segments_proxy->PrependHistory(hist_key, hist_value);
    return convreq;
  }

  ConversionRequest SetUpInputForConversion(
      absl::string_view key, composer::Composer* composer,
      SegmentsProxy* segments_proxy) const {
    SetUpInput(key, composer, segments_proxy);
    ConversionRequest::Options options = {.request_type =
                                              ConversionRequest::CONVERSION};
    return CreateConversionRequestWithOptions(*composer, std::move(options),
                                              *segments_proxy);
  }

  ConversionRequest SetUpInputForConversionWithHistory(
      absl::string_view key, absl::string_view hist_key,
      absl::string_view hist_value, composer::Composer* composer,
      SegmentsProxy* segments_proxy) const {
    const ConversionRequest convreq =
        SetUpInputForConversion(key, composer, segments_proxy);
    segments_proxy->PrependHistory(hist_key, hist_value);
    return convreq;
  }

  ConversionRequest InitSegmentsFromInputSequence(
      const absl::string_view text, composer::Composer* composer,
      SegmentsProxy* segments_proxy) const {
    DCHECK(composer);
    DCHECK(segments_proxy);
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

    std::string query = composer->GetQueryForPrediction();
    segments_proxy->AddSegment(query);

    ConversionRequest::Options options = {.request_type =
                                              ConversionRequest::PREDICTION};
    return CreateConversionRequestWithOptions(*composer, std::move(options),
                                              *segments_proxy);
  }

  static std::optional<int> FindCandidateByValue(
      absl::string_view value, absl::Span<const Result> results) {
    for (size_t i = 0; i < results.size(); ++i) {
      if (results[i].value == value) {
        return i;
      }
    }
    return std::nullopt;
  }

  composer::Composer composer_;
  std::shared_ptr<composer::Table> table_;
  Config config_;
  Request request_;
  commands::Context context_;

 private:
  struct DataAndPredictor {
    std::unique_ptr<engine::Modules> modules;
    std::unique_ptr<UserHistoryPredictor> predictor;
  };

  std::unique_ptr<DataAndPredictor> CreateDataAndPredictor() const {
    auto ret = std::make_unique<DataAndPredictor>();
    ret->modules = engine::ModulesPresetBuilder()
                       .PresetDictionary(std::make_unique<MockDictionary>())
                       .Build(std::make_unique<testing::MockDataManager>())
                       .value();
    ret->predictor = std::make_unique<UserHistoryPredictor>(*ret->modules);
    ret->predictor->WaitForSyncer();
    return ret;
  }

  std::unique_ptr<DataAndPredictor> data_and_predictor_;
};

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorTest) {
  {
    UserHistoryPredictor* predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);

    std::vector<Result> results;

    // Nothing happen
    {
      SegmentsProxy segments_proxy;
      const ConversionRequest convreq =
          SetUpInputForSuggestion("てすと", &composer_, &segments_proxy);
      results = predictor->Predict(convreq);
      EXPECT_TRUE(results.empty());
      EXPECT_EQ(results.size(), 0);
    }

    // Nothing happen
    {
      SegmentsProxy segments_proxy;
      const ConversionRequest convreq =
          SetUpInputForSuggestion("てすと", &composer_, &segments_proxy);
      results = predictor->Predict(convreq);
      EXPECT_TRUE(results.empty());
      EXPECT_EQ(results.size(), 0);
    }

    // Insert two items
    {
      SegmentsProxy segments_proxy;
      const ConversionRequest convreq1 = SetUpInputForSuggestion(
          "わたしのなまえはなかのです", &composer_, &segments_proxy);
      segments_proxy.AddCandidate(0, "私の名前は中野です");
      predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                        kRevertId);

      segments_proxy.Clear();
      const ConversionRequest convreq2 =
          SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);
      results = predictor->Predict(convreq2);
      EXPECT_FALSE(results.empty());
      EXPECT_EQ(results[0].value, "私の名前は中野です");

      segments_proxy.Clear();
      const ConversionRequest convreq3 =
          SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);
      results = predictor->Predict(convreq3);
      EXPECT_FALSE(results.empty());
      EXPECT_EQ(results[0].value, "私の名前は中野です");
    }

    // Insert without learning (nothing happen).
    {
      config::Config::HistoryLearningLevel no_learning_levels[] = {
          config::Config::READ_ONLY, config::Config::NO_HISTORY};
      for (config::Config::HistoryLearningLevel level : no_learning_levels) {
        config_.set_history_learning_level(level);

        SegmentsProxy segments_proxy;
        const ConversionRequest convreq1 = SetUpInputForSuggestion(
            "こんにちはさようなら", &composer_, &segments_proxy);
        segments_proxy.AddCandidate(0, "今日はさようなら");
        predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                          kRevertId);

        segments_proxy.Clear();
        const ConversionRequest convreq2 =
            SetUpInputForSuggestion("こんにちは", &composer_, &segments_proxy);
        results = predictor->Predict(convreq2);
        EXPECT_TRUE(results.empty());
        const ConversionRequest convreq3 =
            SetUpInputForSuggestion("こんにちは", &composer_, &segments_proxy);
        results = predictor->Predict(convreq3);
        EXPECT_TRUE(results.empty());
      }
      config_.set_history_learning_level(config::Config::DEFAULT_HISTORY);
    }

    // sync
    predictor->Sync();
    absl::SleepFor(absl::Milliseconds(500));
  }

  // reload
  {
    UserHistoryPredictor* predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);
    SegmentsProxy segments_proxy;
    std::vector<Result> results;

    // turn off
    {
      SegmentsProxy segments_proxy;
      config_.set_use_history_suggest(false);

      const ConversionRequest convreq1 =
          SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);
      results = predictor->Predict(convreq1);
      EXPECT_TRUE(results.empty());

      config_.set_use_history_suggest(true);
      config_.set_incognito_mode(true);

      const ConversionRequest convreq2 =
          SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);
      results = predictor->Predict(convreq2);
      EXPECT_TRUE(results.empty());

      config_.set_incognito_mode(false);
      config_.set_history_learning_level(config::Config::NO_HISTORY);

      const ConversionRequest convreq3 =
          SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);
      results = predictor->Predict(convreq3);
      EXPECT_TRUE(results.empty());
    }

    // turn on
    {
      config::ConfigHandler::GetDefaultConfig(&config_);
    }

    // reproduced

    const ConversionRequest convreq1 =
        SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);
    results = predictor->Predict(convreq1);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, "私の名前は中野です");

    segments_proxy.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);
    results = predictor->Predict(convreq2);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, "私の名前は中野です");

    // Exact Match
    segments_proxy.Clear();
    const ConversionRequest convreq3 = SetUpInputForSuggestion(
        "わたしのなまえはなかのです", &composer_, &segments_proxy);
    results = predictor->Predict(convreq3);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, "私の名前は中野です");

    segments_proxy.Clear();
    const ConversionRequest convreq4 = SetUpInputForSuggestion(
        "わたしのなまえはなかのです", &composer_, &segments_proxy);
    results = predictor->Predict(convreq4);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, "私の名前は中野です");

    segments_proxy.Clear();
    const ConversionRequest convreq5 = SetUpInputForSuggestion(
        "こんにちはさようなら", &composer_, &segments_proxy);
    results = predictor->Predict(convreq5);
    EXPECT_TRUE(results.empty());

    segments_proxy.Clear();
    const ConversionRequest convreq6 = SetUpInputForSuggestion(
        "こんにちはさようなら", &composer_, &segments_proxy);
    results = predictor->Predict(convreq6);
    EXPECT_TRUE(results.empty());

    // Read only mode should show suggestion.
    {
      config_.set_history_learning_level(config::Config::READ_ONLY);
      const ConversionRequest convreq1 =
          SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);
      results = predictor->Predict(convreq1);
      EXPECT_FALSE(results.empty());
      EXPECT_EQ(results[0].value, "私の名前は中野です");

      segments_proxy.Clear();
      const ConversionRequest convreq2 =
          SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);
      results = predictor->Predict(convreq2);
      EXPECT_FALSE(results.empty());
      EXPECT_EQ(results[0].value, "私の名前は中野です");
      config_.set_history_learning_level(config::Config::DEFAULT_HISTORY);
    }

    // clear
    predictor->ClearAllHistory();
    WaitForSyncer(predictor);
  }

  // nothing happen
  {
    UserHistoryPredictor* predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);
    SegmentsProxy segments_proxy;
    std::vector<Result> results;

    // reproduced
    const ConversionRequest convreq1 =
        SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);
    results = predictor->Predict(convreq1);
    EXPECT_TRUE(results.empty());

    const ConversionRequest convreq2 =
        SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);
    results = predictor->Predict(convreq2);
    EXPECT_TRUE(results.empty());
  }

  // nothing happen
  {
    UserHistoryPredictor* predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);
    SegmentsProxy segments_proxy;
    std::vector<Result> results;

    // reproduced
    const ConversionRequest convreq1 =
        SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);
    results = predictor->Predict(convreq1);
    EXPECT_TRUE(results.empty());

    const ConversionRequest convreq2 =
        SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);
    results = predictor->Predict(convreq2);
    EXPECT_TRUE(results.empty());
  }
}

TEST_F(UserHistoryPredictorTest, RemoveUnselectedHistoryPrediction) {
  request_test_util::FillMobileRequest(&request_);

  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();
  WaitForSyncer(predictor);

  std::vector<Result> results;

  auto make_segments = [](absl::Span<const Result> results,
                          SegmentsProxy* segments_proxy) {
    for (const auto& result : results) {
      segments_proxy->AddCandidate(0, result.value);
    }
  };

  auto insert_target = [&]() {
    SegmentsProxy segments_proxy;
    const ConversionRequest convreq =
        SetUpInputForPrediction("わたしの", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "私の");
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
  };

  auto find_target = [&]() {
    SegmentsProxy segments_proxy;
    const ConversionRequest convreq =
        SetUpInputForPrediction("わたしの", &composer_, &segments_proxy);
    results = predictor->Predict(convreq);
    EXPECT_FALSE(results.empty());
    return FindCandidateByValue("私の", results);
  };

  // Returns true if the target is found.
  auto select_target = [&]() {
    SegmentsProxy segments_proxy;
    const ConversionRequest convreq =
        SetUpInputForPrediction("わたしの", &composer_, &segments_proxy);
    results = predictor->Predict(convreq);
    EXPECT_FALSE(results.empty());
    EXPECT_TRUE(FindCandidateByValue("私の", results));
    make_segments(results, &segments_proxy);
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
  };

  auto select_other = [&]() {
    SegmentsProxy segments_proxy;
    const ConversionRequest convreq =
        SetUpInputForPrediction("わたしの", &composer_, &segments_proxy);
    results = predictor->Predict(convreq);
    EXPECT_FALSE(results.empty());
    EXPECT_TRUE(FindCandidateByValue("私の", results));
    make_segments(results, &segments_proxy);
    auto find = FindCandidateByValue("わたしの", results);
    if (!find) {
      segments_proxy.AddCandidate(0, "わたしの");
      segments_proxy.MoveCandidate(0, 1, 0);
    } else {
      segments_proxy.MoveCandidate(0, find.value(), 0);
    }
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(),
                      kRevertId);  // Select "わたしの"
  };

  auto input_other_key = [&]() {
    SegmentsProxy segments_proxy;
    const ConversionRequest convreq =
        SetUpInputForPrediction("てすと", &composer_, &segments_proxy);
    results = predictor->Predict(convreq);
    make_segments(results, &segments_proxy);
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
  };

  // min selected ratio threshold is 0.05

  {
    insert_target();
    for (int i = 0; i < 20; ++i) {
      EXPECT_TRUE(find_target());
      select_other();
    }
    // select: 1, shown: 1+20, ratio: 1/21 < 0.05
    EXPECT_FALSE(find_target());  // **
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
    EXPECT_FALSE(find_target());  // **

    SegmentsProxy segments_proxy;
    predictor->Revert(kRevertId);
    EXPECT_TRUE(find_target());
  }
}

// We did not support such Segments which has multiple segments and
// has type != CONVERSION.
// To support such Segments, this test case is created separately.
TEST_F(UserHistoryPredictorTest, UserHistoryPredictorTestSuggestion) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();
  UserHistoryPredictorTestPeer predictor_peer(*predictor);

  // Register input histories via Finish method.
  {
    SegmentsProxy segments_proxy;
    const ConversionRequest convreq =
        SetUpInputForSuggestion("かまた", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "火魔汰");
    segments_proxy.AddSegment("ま");
    segments_proxy.AddCandidate(1, "摩");
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);

    // All added items must be suggestion entries.
    for (const auto& element : *predictor_peer.dic_()) {
      if (!element.next) {
        break;  // Except the last one.
      }
      const user_history_predictor::UserHistory::Entry& entry = element.value;
      EXPECT_EQ(entry.suggestion_freq(), 1);
    }
  }

  // Obtain input histories via Predict method.
  {
    SegmentsProxy segments_proxy;
    std::vector<Result> results;
    const ConversionRequest convreq =
        SetUpInputForSuggestion("かま", &composer_, &segments_proxy);
    results = predictor->Predict(convreq);
    EXPECT_FALSE(results.empty());
    std::set<std::string, std::less<>> expected_candidates;
    expected_candidates.insert("火魔汰");
    // We can get this entry even if Segmtnts's type is not CONVERSION.
    expected_candidates.insert("火魔汰摩");
    for (size_t i = 0; i < results.size(); ++i) {
      SCOPED_TRACE(results[i].value);
      EXPECT_EQ(expected_candidates.erase(results[i].value), 1);
    }
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorPreprocessInput) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  {
    // Commit can be triggered by space in alphanumeric keyboard layout.
    // In this case, trailing white space is included to the key and value.
    SegmentsProxy segments_proxy;
    const ConversionRequest convreq =
        SetUpInputForSuggestion("android ", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "android ");
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
  }

  std::vector<Result> results;

  {
    SegmentsProxy segments_proxy;
    const ConversionRequest convreq =
        SetUpInputForSuggestion("androi", &composer_, &segments_proxy);
    results = predictor->Predict(convreq);
    EXPECT_FALSE(results.empty());
    // Preprocessed value should be learned.
    EXPECT_TRUE(FindCandidateByValue("android", results));
    EXPECT_FALSE(FindCandidateByValue("android ", results));
  }
}

TEST_F(UserHistoryPredictorTest, DescriptionTest) {
#ifdef DEBUG
  constexpr absl::string_view kDescription = "テスト History";
#else   // DEBUG
  constexpr absl::string_view kDescription = "テスト";
#endif  // DEBUG

  std::vector<Result> results;

  {
    UserHistoryPredictor* predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);

    // Insert two items
    {
      SegmentsProxy segments_proxy;
      const ConversionRequest convreq = SetUpInputForConversion(
          "わたしのなまえはなかのです", &composer_, &segments_proxy);
      segments_proxy.AddCandidateWithDescription(0, "私の名前は中野です",
                                                 kDescription);
      predictor->Finish(convreq, segments_proxy.MakeLearningResults(),
                        kRevertId);

      const ConversionRequest convreq1 =
          SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);
      results = predictor->Predict(convreq1);
      EXPECT_FALSE(results.empty());
      EXPECT_EQ(results[0].value, "私の名前は中野です");
      EXPECT_EQ(results[0].description, kDescription);

      segments_proxy.Clear();
      const ConversionRequest convreq2 =
          SetUpInputForPrediction("わたしの", &composer_, &segments_proxy);
      results = predictor->Predict(convreq2);
      EXPECT_FALSE(results.empty());
      EXPECT_EQ(results[0].value, "私の名前は中野です");
      EXPECT_EQ(results[0].description, kDescription);
    }

    // sync
    predictor->Sync();
  }

  // reload
  {
    UserHistoryPredictor* predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);
    SegmentsProxy segments_proxy;

    // turn off
    {
      SegmentsProxy segments_proxy;
      config_.set_use_history_suggest(false);
      WaitForSyncer(predictor);

      const ConversionRequest convreq1 =
          SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);
      results = predictor->Predict(convreq1);
      EXPECT_TRUE(results.empty());

      config_.set_use_history_suggest(true);
      config_.set_incognito_mode(true);

      const ConversionRequest convreq2 =
          SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);
      results = predictor->Predict(convreq2);
      EXPECT_TRUE(results.empty());
    }

    // turn on
    {
      config::ConfigHandler::GetDefaultConfig(&config_);
      WaitForSyncer(predictor);
    }

    // reproduced
    const ConversionRequest convreq1 =
        SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);
    results = predictor->Predict(convreq1);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, "私の名前は中野です");
    EXPECT_EQ(results[0].description, kDescription);

    segments_proxy.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForPrediction("わたしの", &composer_, &segments_proxy);
    results = predictor->Predict(convreq2);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, "私の名前は中野です");
    EXPECT_EQ(results[0].description, kDescription);

    // Exact Match
    segments_proxy.Clear();
    const ConversionRequest convreq3 = SetUpInputForSuggestion(
        "わたしのなまえはなかのです", &composer_, &segments_proxy);
    results = predictor->Predict(convreq3);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, "私の名前は中野です");
    EXPECT_EQ(results[0].description, kDescription);

    segments_proxy.Clear();
    const ConversionRequest convreq4 = SetUpInputForSuggestion(
        "わたしのなまえはなかのです", &composer_, &segments_proxy);
    results = predictor->Predict(convreq4);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, "私の名前は中野です");
    EXPECT_EQ(results[0].description, kDescription);

    // clear
    predictor->ClearAllHistory();
    WaitForSyncer(predictor);
  }

  // nothing happen
  {
    UserHistoryPredictor* predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);
    SegmentsProxy segments_proxy;

    // reproduced
    const ConversionRequest convreq1 =
        SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);
    results = predictor->Predict(convreq1);
    EXPECT_TRUE(results.empty());

    const ConversionRequest convreq2 =
        SetUpInputForPrediction("わたしの", &composer_, &segments_proxy);
    results = predictor->Predict(convreq2);
    EXPECT_TRUE(results.empty());
  }

  // nothing happen
  {
    UserHistoryPredictor* predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);
    SegmentsProxy segments_proxy;

    // reproduced
    const ConversionRequest convreq1 =
        SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);
    results = predictor->Predict(convreq1);
    EXPECT_TRUE(results.empty());

    const ConversionRequest convreq2 =
        SetUpInputForPrediction("わたしの", &composer_, &segments_proxy);
    results = predictor->Predict(convreq2);
    EXPECT_TRUE(results.empty());
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorUnusedHistoryTest) {
  std::vector<Result> results;

  constexpr absl::string_view kKey = "わたしのなまえはなかのです";
  constexpr absl::string_view kValue = "私の名前は中野です";

  {
    UserHistoryPredictor* predictor = GetUserHistoryPredictor();
    WaitForSyncer(predictor);

    SegmentsProxy segments_proxy;
    const ConversionRequest convreq =
        SetUpInputForSuggestion(kKey, &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, kValue);

    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);

    predictor->Sync();
  }

  {
    UserHistoryPredictor* predictor = GetUserHistoryPredictor();
    UserHistoryPredictorTestPeer predictor_peer(*predictor);
    WaitForSyncer(predictor);
    SegmentsProxy segments_proxy;

    const ConversionRequest convreq =
        SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);
    results = predictor->Predict(convreq);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, "私の名前は中野です");

    // Generates unused entry.
    UserHistoryPredictor::Entry* entry =
        predictor_peer.dic_()->MutableLookupWithoutInsert(
            UserHistoryPredictor::Fingerprint(kKey, kValue));
    ASSERT_TRUE(entry);
    // Implementing Revert at zero frequency is a valid and
    // consistent way to implement redo.
    entry->set_suggestion_freq(0);  // set unused.
  }

  {
    UserHistoryPredictor* predictor = GetUserHistoryPredictor();
    predictor->ClearUnusedHistory();
    WaitForSyncer(predictor);

    SegmentsProxy segments_proxy;
    const ConversionRequest convreq =
        SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);

    // No suggestion after clear unused history.
    results = predictor->Predict(convreq);
    EXPECT_TRUE(results.empty());
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorRevertTest) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  std::vector<Result> results;

  SegmentsProxy segments_proxy, segments_proxy2;
  const ConversionRequest convreq1 = SetUpInputForConversion(
      "わたしのなまえはなかのです", &composer_, &segments_proxy);
  segments_proxy.AddCandidate(0, "私の名前は中野です");

  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), kRevertId);

  // Before Revert, Suggest works
  const ConversionRequest convreq2 =
      SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy2);
  results = predictor->Predict(convreq2);
  EXPECT_FALSE(results.empty());
  EXPECT_EQ(results[0].value, "私の名前は中野です");

  // Call revert here
  predictor->Revert(kRevertId);

  segments_proxy.Clear();
  const ConversionRequest convreq3 =
      SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);

  results = predictor->Predict(convreq3);
  EXPECT_TRUE(results.empty());
  EXPECT_EQ(results.size(), 0);
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorRevertFreqTest) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();
  UserHistoryPredictorTestPeer predictor_peer(*predictor);

  std::vector<Result> results;

  constexpr absl::string_view kKey = "わたしのなまえはなかのです";
  constexpr absl::string_view kValue = "私の名前は中野です";

  SegmentsProxy segments_proxy;
  const ConversionRequest convreq1 =
      SetUpInputForConversion(kKey, &composer_, &segments_proxy);
  segments_proxy.AddCandidate(0, kValue);

  auto freq_eq = [&](int expected_freq) {
    auto* entry = predictor_peer.dic_()->MutableLookupWithoutInsert(
        UserHistoryPredictor::Fingerprint(kKey, kValue));
    if (expected_freq == 0) {
      EXPECT_TRUE(!entry || entry->suggestion_freq() == expected_freq);
    } else {
      ASSERT_TRUE(entry);
      EXPECT_EQ(entry->suggestion_freq(), expected_freq);
    }
  };

  freq_eq(0);

  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), 1);
  freq_eq(1);

  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), 2);
  freq_eq(2);

  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), 3);
  freq_eq(3);

  predictor->Revert(3);
  freq_eq(2);

  predictor->Revert(2);
  freq_eq(1);

  predictor->Revert(1);
  freq_eq(0);
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorClearTest) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictor();
  WaitForSyncer(predictor);

  std::vector<Result> results;

  // input "testtest" 10 times
  for (int i = 0; i < 10; ++i) {
    SegmentsProxy segments_proxy;
    const ConversionRequest convreq =
        SetUpInputForConversion("testtest", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "テストテスト");
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  // input "testtest" 1 time
  for (int i = 0; i < 1; ++i) {
    SegmentsProxy segments_proxy;
    const ConversionRequest convreq =
        SetUpInputForConversion("testtest", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "テストテスト");
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
  }

  // frequency is cleared as well.
  {
    SegmentsProxy segments_proxy;
    const ConversionRequest convreq1 =
        SetUpInputForSuggestion("t", &composer_, &segments_proxy);
    results = predictor->Predict(convreq1);
    EXPECT_TRUE(results.empty());

    segments_proxy.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForSuggestion("testte", &composer_, &segments_proxy);
    results = predictor->Predict(convreq2);
    EXPECT_FALSE(results.empty());
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorTrailingPunctuation) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  SegmentsProxy segments_proxy;
  std::vector<Result> results;

  const ConversionRequest convreq1 = SetUpInputForConversion(
      "わたしのなまえはなかのです", &composer_, &segments_proxy);

  segments_proxy.AddCandidate(0, "私の名前は中野です");
  segments_proxy.AddSegment("。");
  segments_proxy.AddCandidate(1, "。");

  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), kRevertId);

  segments_proxy.Clear();
  const ConversionRequest convreq2 =
      SetUpInputForPrediction("わたしの", &composer_, &segments_proxy);
  results = predictor->Predict(convreq2);
  EXPECT_FALSE(results.empty());
  EXPECT_EQ(results.size(), 2);
  EXPECT_EQ(results[0].value, "私の名前は中野です");
  EXPECT_EQ(results[1].value, "私の名前は中野です。");

  segments_proxy.Clear();
  const ConversionRequest convreq3 =
      SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);

  results = predictor->Predict(convreq3);
  EXPECT_FALSE(results.empty());
  EXPECT_EQ(results.size(), 2);
  EXPECT_EQ(results[0].value, "私の名前は中野です");
  EXPECT_EQ(results[1].value, "私の名前は中野です。");
}

TEST_F(UserHistoryPredictorTest, TrailingPunctuationMobile) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();
  request_test_util::FillMobileRequest(&request_);
  SegmentsProxy segments_proxy;

  const ConversionRequest convreq1 =
      SetUpInputForConversion("です。", &composer_, &segments_proxy);

  segments_proxy.AddCandidate(0, "です。");

  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), kRevertId);

  segments_proxy.Clear();

  const ConversionRequest convreq2 =
      SetUpInputForPrediction("です", &composer_, &segments_proxy);
  const std::vector<Result> results = predictor->Predict(convreq2);
  EXPECT_TRUE(results.empty());
}

TEST_F(UserHistoryPredictorTest, HistoryToPunctuation) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  SegmentsProxy segments_proxy;
  std::vector<Result> results;

  // Scenario 1: A user have committed "亜" by prediction and then commit "。".
  // Then, the unigram "亜" is learned but the bigram "亜。" shouldn't.
  const ConversionRequest convreq1 =
      SetUpInputForPrediction("あ", &composer_, &segments_proxy);
  segments_proxy.AddCandidate(0, "亜");
  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), kRevertId);
  segments_proxy.SetHistoryType(0);

  segments_proxy.AddSegment("。");
  segments_proxy.AddCandidate(1, "。");
  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), kRevertId);

  segments_proxy.Clear();
  const ConversionRequest convreq2 =
      SetUpInputForPrediction("あ", &composer_, &segments_proxy);  // "あ"
  results = predictor->Predict(convreq2);
  ASSERT_FALSE(results.empty());
  EXPECT_EQ(results[0].value, "亜");

  segments_proxy.Clear();

  // Scenario 2: the opposite case to Scenario 1, i.e., "。亜".  Nothing is
  // suggested from symbol "。".
  const ConversionRequest convreq3 =
      SetUpInputForPrediction("。", &composer_, &segments_proxy);
  segments_proxy.AddCandidate(0, "。");
  predictor->Finish(convreq3, segments_proxy.MakeLearningResults(), kRevertId);
  segments_proxy.SetHistoryType(0);

  segments_proxy.AddSegment("あ");
  segments_proxy.AddCandidate(1, "亜");
  predictor->Finish(convreq3, segments_proxy.MakeLearningResults(), kRevertId);

  segments_proxy.Clear();
  const ConversionRequest convreq4 =
      SetUpInputForPrediction("。", &composer_, &segments_proxy);  // "。"
  results = predictor->Predict(convreq4);
  EXPECT_TRUE(results.empty());

  segments_proxy.Clear();

  // Scenario 3: If the history segment looks like a sentence and committed
  // value is a punctuation, the concatenated entry is also learned.
  const ConversionRequest convreq5 =
      SetUpInputForPrediction("おつかれさまです", &composer_, &segments_proxy);
  segments_proxy.AddCandidate(0, "お疲れ様です");
  predictor->Finish(convreq5, segments_proxy.MakeLearningResults(), kRevertId);
  segments_proxy.SetHistoryType(0);

  segments_proxy.AddSegment("。");
  segments_proxy.AddCandidate(1, "。");
  predictor->Finish(convreq5, segments_proxy.MakeLearningResults(), kRevertId);

  segments_proxy.Clear();
  const ConversionRequest convreq6 =
      SetUpInputForPrediction("おつかれ", &composer_, &segments_proxy);
  results = predictor->Predict(convreq6);
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].value, "お疲れ様です");
}

TEST_F(UserHistoryPredictorTest, UserHistoryPredictorPrecedingPunctuation) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  SegmentsProxy segments_proxy;
  std::vector<Result> results;

  const ConversionRequest convreq1 =
      SetUpInputForConversion("。", &composer_, &segments_proxy);

  segments_proxy.AddCandidate(0, "。");
  segments_proxy.AddSegment("わたしのなまえはなかのです");
  segments_proxy.AddCandidate(1, "私の名前は中野です");

  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), kRevertId);

  segments_proxy.Clear();
  const ConversionRequest convreq2 =
      SetUpInputForPrediction("わたしの", &composer_, &segments_proxy);

  results = predictor->Predict(convreq2);
  EXPECT_FALSE(results.empty());
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].value, "私の名前は中野です");

  segments_proxy.Clear();
  const ConversionRequest convreq3 =
      SetUpInputForSuggestion("わたしの", &composer_, &segments_proxy);
  results = predictor->Predict(convreq3);
  EXPECT_FALSE(results.empty());
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].value, "私の名前は中野です");
}

namespace {
struct StartsWithPunctuationsTestData {
  const char* first_character;
  bool expected_result;
};
}  // namespace

TEST_F(UserHistoryPredictorTest, StartsWithPunctuations) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictor();
  const StartsWithPunctuationsTestData kTestCases[] = {
      {"。", false}, {"、", false},    {"？", false},
      {"！", false}, {"あああ", true},
  };

  std::vector<Result> results;

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    WaitForSyncer(predictor);
    predictor->ClearAllHistory();
    WaitForSyncer(predictor);

    SegmentsProxy segments_proxy;
    std::vector<Result> results;
    const std::string first_char = kTestCases[i].first_character;
    {
      // Learn from two segments
      const ConversionRequest convreq =
          SetUpInputForConversion(first_char, &composer_, &segments_proxy);
      segments_proxy.AddCandidate(0, first_char);
      segments_proxy.AddSegment("てすとぶんしょう");
      segments_proxy.AddCandidate(1, "テスト文章");
      predictor->Finish(convreq, segments_proxy.MakeLearningResults(),
                        kRevertId);
    }
    segments_proxy.Clear();
    {
      // Learn from one segment
      const ConversionRequest convreq = SetUpInputForConversion(
          first_char + "てすとぶんしょう", &composer_, &segments_proxy);
      segments_proxy.AddCandidate(0, first_char + "テスト文章");
      predictor->Finish(convreq, segments_proxy.MakeLearningResults(),
                        kRevertId);
    }
    segments_proxy.Clear();
    {
      // Suggestion
      const ConversionRequest convreq =
          SetUpInputForSuggestion(first_char, &composer_, &segments_proxy);
      segments_proxy.AddCandidate(0, first_char);
      results = predictor->Predict(convreq);
      EXPECT_EQ(!results.empty(), kTestCases[i].expected_result)
          << "Suggest from " << first_char;
    }
    segments_proxy.Clear();
    {
      // Prediction
      const ConversionRequest convreq =
          SetUpInputForPrediction(first_char, &composer_, &segments_proxy);
      results = predictor->Predict(convreq);
      EXPECT_EQ(!results.empty(), kTestCases[i].expected_result)
          << "Predict from " << first_char;
    }
  }
}

TEST_F(UserHistoryPredictorTest, ZeroQuerySuggestionTest) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  request_.set_zero_query_suggestion(true);

  commands::Request non_zero_query_request;
  non_zero_query_request.set_zero_query_suggestion(false);
  commands::Context context;
  SegmentsProxy segments_proxy;
  std::vector<Result> results;

  // No history segments
  segments_proxy.Clear();
  const ConversionRequest convreq =
      SetUpInputForSuggestion("", &composer_, &segments_proxy);
  results = predictor->Predict(convreq);
  EXPECT_TRUE(results.empty());

  {
    segments_proxy.Clear();

    const ConversionRequest convreq1 =
        SetUpInputForConversion("たろうは", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "太郎は");
    predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                      kRevertId);

    const ConversionRequest convreq2 = SetUpInputForConversionWithHistory(
        "はなこに", "たろうは", "太郎は", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(1, "花子に");
    predictor->Finish(convreq2, segments_proxy.MakeLearningResults(),
                      kRevertId);

    const ConversionRequest convreq3 = SetUpInputForConversionWithHistory(
        "きょうと", "たろうは", "太郎は", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(1, "京都");
    absl::SleepFor(absl::Seconds(2));
    predictor->Finish(convreq3, segments_proxy.MakeLearningResults(),
                      kRevertId);

    const ConversionRequest convreq4 = SetUpInputForConversionWithHistory(
        "おおさか", "たろうは", "太郎は", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(1, "大阪");
    absl::SleepFor(absl::Seconds(2));
    predictor->Finish(convreq4, segments_proxy.MakeLearningResults(),
                      kRevertId);

    // Zero query suggestion is disabled.
    SetUpInputForSuggestionWithHistory("", "たろうは", "太郎は", &composer_,
                                       &segments_proxy);
    // convreq5 is not zero query suggestion unlike other convreqs.
    const ConversionRequest convreq5 =
        ConversionRequestBuilder()
            .SetComposer(composer_)
            .SetRequestView(non_zero_query_request)
            .SetContextView(context)
            .SetConfigView(config_)
            .Build();
    results = predictor->Predict(convreq5);
    EXPECT_TRUE(results.empty());

    const ConversionRequest convreq6 = SetUpInputForSuggestionWithHistory(
        "", "たろうは", "太郎は", &composer_, &segments_proxy);
    results = predictor->Predict(convreq6);
    EXPECT_FALSE(results.empty());
    // last-pushed segment is "大阪"
    EXPECT_EQ(results[0].value, "大阪");
    EXPECT_EQ(results[0].key, "おおさか");

    for (const char* key : {"は", "た", "き", "お"}) {
      const ConversionRequest convreq = SetUpInputForSuggestionWithHistory(
          key, "たろうは", "太郎は", &composer_, &segments_proxy);
      results = predictor->Predict(convreq);
      EXPECT_FALSE(results.empty());
    }
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    segments_proxy.Clear();
    const ConversionRequest convreq1 =
        SetUpInputForConversion("たろうは", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "太郎は");

    segments_proxy.AddSegment("はなこに");
    segments_proxy.AddCandidate(1, "花子に");
    predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                      kRevertId);

    segments_proxy.Clear();
    ConversionRequest convreq2 =
        SetUpInputForSuggestion("たろうは", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "太郎は");
    segments_proxy.SetHistoryType(0);

    // Zero query suggestion is disabled.
    const ConversionRequest non_zero_query_convreq =
        ConversionRequestBuilder()
            .SetComposer(composer_)
            .SetRequestView(non_zero_query_request)
            .SetContextView(context)
            .SetConfigView(config_)
            .SetHistoryResultView(segments_proxy.history_result())
            .Build();

    segments_proxy.AddSegment("");  // empty request
    EXPECT_TRUE(predictor->Predict(non_zero_query_convreq).empty());

    auto convreq = [&]() {
      composer_.Reset();
      composer_.SetPreeditTextForTestOnly(segments_proxy.key());
      ConversionRequest::Options options = {.request_type =
                                                ConversionRequest::SUGGESTION};
      return CreateConversionRequestWithOptions(composer_, std::move(options),
                                                segments_proxy);
    };

    segments_proxy.pop_back_segment();
    segments_proxy.AddSegment("");  // empty request
    results = predictor->Predict(convreq());
    EXPECT_FALSE(results.empty());

    segments_proxy.pop_back_segment();
    segments_proxy.AddSegment("は");
    results = predictor->Predict(convreq());
    EXPECT_FALSE(results.empty());

    segments_proxy.pop_back_segment();
    segments_proxy.AddSegment("た");
    results = predictor->Predict(convreq());
    EXPECT_FALSE(results.empty());
  }
}

TEST_F(UserHistoryPredictorTest, MultiSegmentsMultiInput) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  SegmentsProxy segments_proxy;
  std::vector<Result> results;

  const ConversionRequest convreq1 =
      SetUpInputForConversion("たろうは", &composer_, &segments_proxy);
  segments_proxy.AddCandidate(0, "太郎は");
  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), kRevertId);
  segments_proxy.SetHistoryType(0);

  segments_proxy.AddSegment("はなこに");
  segments_proxy.AddCandidate(1, "花子に");
  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), kRevertId);
  segments_proxy.SetHistoryType(1);

  segments_proxy.clear_conversion_segments();
  segments_proxy.AddSegment("むずかしい");
  segments_proxy.AddCandidate(2, "難しい");
  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), kRevertId);
  segments_proxy.SetHistoryType(2);

  segments_proxy.clear_conversion_segments();
  segments_proxy.AddSegment("ほんを");
  segments_proxy.AddCandidate(3, "本を");
  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), kRevertId);
  segments_proxy.SetHistoryType(3);

  segments_proxy.clear_conversion_segments();
  segments_proxy.AddSegment("よませた");
  segments_proxy.AddCandidate(4, "読ませた");
  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), kRevertId);

  segments_proxy.Clear();
  const ConversionRequest convreq2 =
      SetUpInputForSuggestion("た", &composer_, &segments_proxy);
  results = predictor->Predict(convreq2);
  EXPECT_TRUE(results.empty());

  segments_proxy.Clear();
  const ConversionRequest convreq3 =
      SetUpInputForSuggestion("たろうは", &composer_, &segments_proxy);
  results = predictor->Predict(convreq3);
  EXPECT_FALSE(results.empty());

  segments_proxy.Clear();
  const ConversionRequest convreq4 =
      SetUpInputForSuggestion("ろうは", &composer_, &segments_proxy);
  results = predictor->Predict(convreq4);
  EXPECT_TRUE(results.empty());

  segments_proxy.Clear();
  const ConversionRequest convreq5 =
      SetUpInputForSuggestion("たろうははな", &composer_, &segments_proxy);
  results = predictor->Predict(convreq5);
  EXPECT_FALSE(results.empty());

  segments_proxy.Clear();
  const ConversionRequest convreq6 =
      SetUpInputForSuggestion("はなこにむ", &composer_, &segments_proxy);
  results = predictor->Predict(convreq6);
  EXPECT_FALSE(results.empty());

  segments_proxy.Clear();
  const ConversionRequest convreq7 =
      SetUpInputForSuggestion("むずかし", &composer_, &segments_proxy);
  results = predictor->Predict(convreq7);
  EXPECT_FALSE(results.empty());

  segments_proxy.Clear();
  const ConversionRequest convreq8 = SetUpInputForSuggestion(
      "はなこにむずかしいほ", &composer_, &segments_proxy);
  results = predictor->Predict(convreq8);
  EXPECT_FALSE(results.empty());

  segments_proxy.Clear();
  const ConversionRequest convreq9 =
      SetUpInputForSuggestion("ほんをよま", &composer_, &segments_proxy);
  results = predictor->Predict(convreq9);
  EXPECT_FALSE(results.empty());

  absl::SleepFor(absl::Seconds(1));

  // Add new entry "たろうはよしこに/太郎は良子に"
  segments_proxy.Clear();
  const ConversionRequest convreq10 =
      SetUpInputForConversion("たろうは", &composer_, &segments_proxy);
  segments_proxy.AddCandidate(0, "太郎は");
  predictor->Finish(convreq10, segments_proxy.MakeLearningResults(), kRevertId);
  segments_proxy.SetHistoryType(0);

  segments_proxy.AddSegment("よしこに");
  segments_proxy.AddCandidate(1, "良子に");
  predictor->Finish(convreq10, segments_proxy.MakeLearningResults(), kRevertId);
  segments_proxy.SetHistoryType(1);

  segments_proxy.Clear();
  const ConversionRequest convreq11 =
      SetUpInputForSuggestion("たろうは", &composer_, &segments_proxy);
  results = predictor->Predict(convreq11);
  EXPECT_FALSE(results.empty());
  EXPECT_EQ(results[0].value, "太郎は良子に");
}

TEST_F(UserHistoryPredictorTest, MultiSegmentsSingleInput) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  SegmentsProxy segments_proxy;
  std::vector<Result> results;

  const ConversionRequest convreq1 =
      SetUpInputForConversion("たろうは", &composer_, &segments_proxy);
  segments_proxy.AddCandidate(0, "太郎は");

  segments_proxy.AddSegment("はなこに");
  segments_proxy.AddCandidate(1, "花子に");

  segments_proxy.AddSegment("むずかしい");
  segments_proxy.AddCandidate(2, "難しい");

  segments_proxy.AddSegment("ほんを");
  segments_proxy.AddCandidate(3, "本を");

  segments_proxy.AddSegment("よませた");
  segments_proxy.AddCandidate(4, "読ませた");

  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), kRevertId);

  segments_proxy.Clear();
  const ConversionRequest convreq2 =
      SetUpInputForSuggestion("たろうは", &composer_, &segments_proxy);
  results = predictor->Predict(convreq2);
  EXPECT_FALSE(results.empty());

  segments_proxy.Clear();
  const ConversionRequest convreq3 =
      SetUpInputForSuggestion("た", &composer_, &segments_proxy);
  results = predictor->Predict(convreq3);
  EXPECT_TRUE(results.empty());

  segments_proxy.Clear();
  const ConversionRequest convreq4 =
      SetUpInputForSuggestion("たろうははな", &composer_, &segments_proxy);
  results = predictor->Predict(convreq4);
  EXPECT_FALSE(results.empty());

  segments_proxy.Clear();
  const ConversionRequest convreq5 =
      SetUpInputForSuggestion("ろうははな", &composer_, &segments_proxy);
  results = predictor->Predict(convreq5);
  EXPECT_TRUE(results.empty());

  segments_proxy.Clear();
  const ConversionRequest convreq6 =
      SetUpInputForSuggestion("はなこにむ", &composer_, &segments_proxy);
  results = predictor->Predict(convreq6);
  EXPECT_FALSE(results.empty());

  segments_proxy.Clear();
  const ConversionRequest convreq7 =
      SetUpInputForSuggestion("むずかし", &composer_, &segments_proxy);
  results = predictor->Predict(convreq7);
  EXPECT_FALSE(results.empty());

  segments_proxy.Clear();
  const ConversionRequest convreq8 = SetUpInputForSuggestion(
      "はなこにむずかしいほ", &composer_, &segments_proxy);
  results = predictor->Predict(convreq8);
  EXPECT_FALSE(results.empty());

  segments_proxy.Clear();
  const ConversionRequest convreq9 =
      SetUpInputForSuggestion("ほんをよま", &composer_, &segments_proxy);
  results = predictor->Predict(convreq9);
  EXPECT_FALSE(results.empty());

  absl::SleepFor(absl::Seconds(1));

  // Add new entry "たろうはよしこに/太郎は良子に"
  segments_proxy.Clear();
  const ConversionRequest convreq10 =
      SetUpInputForConversion("たろうは", &composer_, &segments_proxy);
  segments_proxy.AddCandidate(0, "太郎は");
  predictor->Finish(convreq10, segments_proxy.MakeLearningResults(), kRevertId);
  segments_proxy.SetHistoryType(0);

  segments_proxy.AddSegment("よしこに");
  segments_proxy.AddCandidate(1, "良子に");
  predictor->Finish(convreq10, segments_proxy.MakeLearningResults(), kRevertId);
  segments_proxy.SetHistoryType(1);

  segments_proxy.Clear();
  const ConversionRequest convreq11 =
      SetUpInputForSuggestion("たろうは", &composer_, &segments_proxy);
  results = predictor->Predict(convreq11);
  EXPECT_FALSE(results.empty());
  EXPECT_EQ(results[0].value, "太郎は良子に");
}

TEST_F(UserHistoryPredictorTest, Regression2843371Case1) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  SegmentsProxy segments_proxy;
  std::vector<Result> results;

  const ConversionRequest convreq1 =
      SetUpInputForConversion("とうきょうは", &composer_, &segments_proxy);
  segments_proxy.AddCandidate(0, "東京は");

  segments_proxy.AddSegment("、");
  segments_proxy.AddCandidate(1, "、");

  segments_proxy.AddSegment("にほんです");
  segments_proxy.AddCandidate(2, "日本です");

  segments_proxy.AddSegment("。");
  segments_proxy.AddCandidate(3, "。");

  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), kRevertId);

  segments_proxy.Clear();

  absl::SleepFor(absl::Seconds(1));

  const ConversionRequest convreq2 =
      SetUpInputForConversion("らーめんは", &composer_, &segments_proxy);
  segments_proxy.AddCandidate(0, "ラーメンは");

  segments_proxy.AddSegment("、");
  segments_proxy.AddCandidate(1, "、");

  segments_proxy.AddSegment("めんるいです");
  segments_proxy.AddCandidate(2, "麺類です");

  segments_proxy.AddSegment("。");
  segments_proxy.AddCandidate(3, "。");

  predictor->Finish(convreq2, segments_proxy.MakeLearningResults(), kRevertId);

  segments_proxy.Clear();

  const ConversionRequest convreq3 =
      SetUpInputForSuggestion("とうきょうは、", &composer_, &segments_proxy);
  results = predictor->Predict(convreq3);
  EXPECT_FALSE(results.empty());

  EXPECT_EQ(results[0].value, "東京は、日本です");
}

TEST_F(UserHistoryPredictorTest, Regression2843371Case2) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  SegmentsProxy segments_proxy;
  std::vector<Result> results;

  const ConversionRequest convreq1 =
      SetUpInputForConversion("えど", &composer_, &segments_proxy);
  segments_proxy.AddCandidate(0, "江戸");

  segments_proxy.AddSegment("(");
  segments_proxy.AddCandidate(1, "(");

  segments_proxy.AddSegment("とうきょう");
  segments_proxy.AddCandidate(2, "東京");

  segments_proxy.AddSegment(")");
  segments_proxy.AddCandidate(3, ")");

  segments_proxy.AddSegment("は");
  segments_proxy.AddCandidate(4, "は");

  segments_proxy.AddSegment("えぞ");
  segments_proxy.AddCandidate(5, "蝦夷");

  segments_proxy.AddSegment("(");
  segments_proxy.AddCandidate(6, "(");

  segments_proxy.AddSegment("ほっかいどう");
  segments_proxy.AddCandidate(7, "北海道");

  segments_proxy.AddSegment(")");
  segments_proxy.AddCandidate(8, ")");

  segments_proxy.AddSegment("ではない");
  segments_proxy.AddCandidate(9, "ではない");

  segments_proxy.AddSegment("。");
  segments_proxy.AddCandidate(10, "。");

  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), kRevertId);

  segments_proxy.Clear();

  const ConversionRequest convreq2 =
      SetUpInputForSuggestion("えど(", &composer_, &segments_proxy);
  results = predictor->Predict(convreq2);
  EXPECT_FALSE(results.empty());
  EXPECT_EQ(results[0].value, "江戸(東京");

  results = predictor->Predict(convreq2);
  EXPECT_FALSE(results.empty());

  EXPECT_EQ(results[0].value, "江戸(東京");
}

TEST_F(UserHistoryPredictorTest, Regression2843371Case3) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  SegmentsProxy segments_proxy;
  std::vector<Result> results;

  const ConversionRequest convreq1 =
      SetUpInputForConversion("「", &composer_, &segments_proxy);
  segments_proxy.AddCandidate(0, "「");

  segments_proxy.AddSegment("やま");
  segments_proxy.AddCandidate(1, "山");

  segments_proxy.AddSegment("」");
  segments_proxy.AddCandidate(2, "」");

  segments_proxy.AddSegment("は");
  segments_proxy.AddCandidate(3, "は");

  segments_proxy.AddSegment("たかい");
  segments_proxy.AddCandidate(4, "高い");

  segments_proxy.AddSegment("。");
  segments_proxy.AddCandidate(5, "。");

  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), kRevertId);

  absl::SleepFor(absl::Seconds(2));

  segments_proxy.Clear();

  const ConversionRequest convreq2 =
      SetUpInputForConversion("「", &composer_, &segments_proxy);
  segments_proxy.AddCandidate(0, "「");

  segments_proxy.AddSegment("うみ");
  segments_proxy.AddCandidate(1, "海");

  segments_proxy.AddSegment("」");
  segments_proxy.AddCandidate(2, "」");

  segments_proxy.AddSegment("は");
  segments_proxy.AddCandidate(3, "は");

  segments_proxy.AddSegment("ふかい");
  segments_proxy.AddCandidate(4, "深い");

  segments_proxy.AddSegment("。");
  segments_proxy.AddCandidate(5, "。");

  predictor->Finish(convreq2, segments_proxy.MakeLearningResults(), kRevertId);

  segments_proxy.Clear();

  const ConversionRequest convreq3 =
      SetUpInputForSuggestion("「やま」は", &composer_, &segments_proxy);
  results = predictor->Predict(convreq3);
  EXPECT_FALSE(results.empty());

  EXPECT_EQ(results[0].value, "「山」は高い");
}

TEST_F(UserHistoryPredictorTest, Regression2843775) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  SegmentsProxy segments_proxy;
  std::vector<Result> results;

  const ConversionRequest convreq1 =
      SetUpInputForConversion("そうです", &composer_, &segments_proxy);
  segments_proxy.AddCandidate(0, "そうです");

  segments_proxy.AddSegment("。よろしくおねがいします");
  segments_proxy.AddCandidate(1, "。よろしくお願いします");

  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), kRevertId);

  segments_proxy.Clear();

  const ConversionRequest convreq2 =
      SetUpInputForSuggestion("そうです", &composer_, &segments_proxy);
  results = predictor->Predict(convreq2);
  EXPECT_FALSE(results.empty());

  EXPECT_EQ(results[0].value, "そうです。よろしくお願いします");
}

TEST_F(UserHistoryPredictorTest, DuplicateString) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  SegmentsProxy segments_proxy;
  std::vector<Result> results;

  const ConversionRequest convreq1 =
      SetUpInputForConversion("らいおん", &composer_, &segments_proxy);
  segments_proxy.AddCandidate(0, "ライオン");

  segments_proxy.AddSegment("（");
  segments_proxy.AddCandidate(1, "（");

  segments_proxy.AddSegment("もうじゅう");
  segments_proxy.AddCandidate(2, "猛獣");

  segments_proxy.AddSegment("）と");
  segments_proxy.AddCandidate(3, "）と");

  segments_proxy.AddSegment("ぞうりむし");
  segments_proxy.AddCandidate(4, "ゾウリムシ");

  segments_proxy.AddSegment("（");
  segments_proxy.AddCandidate(5, "（");

  segments_proxy.AddSegment("びせいぶつ");
  segments_proxy.AddCandidate(6, "微生物");

  segments_proxy.AddSegment("）");
  segments_proxy.AddCandidate(7, "）");

  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), kRevertId);

  segments_proxy.Clear();

  const ConversionRequest convreq2 =
      SetUpInputForSuggestion("ぞうりむし", &composer_, &segments_proxy);
  results = predictor->Predict(convreq2);
  EXPECT_FALSE(results.empty());

  for (int i = 0; i < results.size(); ++i) {
    EXPECT_EQ(results[i].value.find("猛獣"),
              std::string::npos);  // "猛獣" should not be found
  }

  segments_proxy.Clear();

  const ConversionRequest convreq3 =
      SetUpInputForSuggestion("らいおん", &composer_, &segments_proxy);
  results = predictor->Predict(convreq3);
  EXPECT_FALSE(results.empty());

  for (int i = 0; i < results.size(); ++i) {
    EXPECT_EQ(results[i].value.find("ライオン（微生物"), std::string::npos);
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
  UserHistoryPredictor* predictor = GetUserHistoryPredictor();
  WaitForSyncer(predictor);

  std::vector<Result> results;

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
  SegmentsProxy segments_proxy;
  for (size_t i = 0; i < commands.size(); ++i) {
    switch (commands[i].type) {
      case Command::SYNC:
        predictor->Sync();
        break;
      case Command::WAIT:
        WaitForSyncer(predictor);
        break;
      case Command::INSERT: {
        segments_proxy.Clear();
        const ConversionRequest convreq = SetUpInputForConversion(
            commands[i].key, &composer_, &segments_proxy);
        segments_proxy.AddCandidate(0, commands[i].value);
        predictor->Finish(convreq, segments_proxy.MakeLearningResults(),
                          kRevertId);
        break;
      }
      case Command::LOOKUP: {
        segments_proxy.Clear();
        const ConversionRequest convreq = SetUpInputForSuggestion(
            commands[i].key, &composer_, &segments_proxy);
        predictor->Predict(convreq);
        break;
      }
      default:
        break;
    }
  }
}

TEST_F(UserHistoryPredictorTest, GetMatchTypeTest) {
  using MatchType = UserHistoryPredictorTestPeer::MatchType;
  EXPECT_EQ(UserHistoryPredictorTestPeer::GetMatchType("test", ""),
            MatchType::NO_MATCH);

  EXPECT_EQ(UserHistoryPredictorTestPeer::GetMatchType("", ""),
            MatchType::NO_MATCH);

  EXPECT_EQ(UserHistoryPredictorTestPeer::GetMatchType("", "test"),
            MatchType::LEFT_EMPTY_MATCH);

  EXPECT_EQ(UserHistoryPredictorTestPeer::GetMatchType("foo", "bar"),
            MatchType::NO_MATCH);

  EXPECT_EQ(UserHistoryPredictorTestPeer::GetMatchType("foo", "foo"),
            MatchType::EXACT_MATCH);

  EXPECT_EQ(UserHistoryPredictorTestPeer::GetMatchType("foo", "foobar"),
            MatchType::LEFT_PREFIX_MATCH);

  EXPECT_EQ(UserHistoryPredictorTestPeer::GetMatchType("foobar", "foo"),
            MatchType::RIGHT_PREFIX_MATCH);
}

TEST_F(UserHistoryPredictorTest, FingerPrintTest) {
  constexpr absl::string_view kKey = "abc";
  constexpr absl::string_view kValue = "ABC";

  UserHistoryPredictor::Entry entry;
  entry.set_key(kKey);
  entry.set_value(kValue);

  const uint32_t entry_fp1 = UserHistoryPredictor::Fingerprint(kKey, kValue);
  const uint32_t entry_fp2 = UserHistoryPredictor::EntryFingerprint(entry);

  EXPECT_EQ(entry_fp1, entry_fp2);
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

    EXPECT_GT(UserHistoryPredictorTestPeer::GetScore(entry2),
              UserHistoryPredictorTestPeer::GetScore(entry1));
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

    EXPECT_GT(UserHistoryPredictorTestPeer::GetScore(entry1),
              UserHistoryPredictorTestPeer::GetScore(entry2));
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

    EXPECT_GT(UserHistoryPredictorTestPeer::GetScore(entry2),
              UserHistoryPredictorTestPeer::GetScore(entry1));
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

    EXPECT_GT(UserHistoryPredictorTestPeer::GetScore(entry1),
              UserHistoryPredictorTestPeer::GetScore(entry2));
  }
}

TEST_F(UserHistoryPredictorTest, IsValidEntry) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictor();
  UserHistoryPredictorTestPeer predictor_peer(*predictor);

  UserHistoryPredictor::Entry entry;

  EXPECT_TRUE(predictor_peer.IsValidEntry(entry));

  entry.set_key("key");
  entry.set_value("value");

  EXPECT_TRUE(predictor_peer.IsValidEntry(entry));
  EXPECT_TRUE(predictor_peer.IsValidEntryIgnoringRemovedField(entry));

  entry.set_removed(true);
  EXPECT_FALSE(predictor_peer.IsValidEntry(entry));
  EXPECT_TRUE(predictor_peer.IsValidEntryIgnoringRemovedField(entry));

  entry.set_removed(false);
  EXPECT_TRUE(predictor_peer.IsValidEntry(entry));
  EXPECT_TRUE(predictor_peer.IsValidEntryIgnoringRemovedField(entry));

  entry.set_removed(true);
  EXPECT_FALSE(predictor_peer.IsValidEntry(entry));
  EXPECT_TRUE(predictor_peer.IsValidEntryIgnoringRemovedField(entry));

  entry.Clear();
  EXPECT_TRUE(predictor_peer.IsValidEntry(entry));
  EXPECT_TRUE(predictor_peer.IsValidEntryIgnoringRemovedField(entry));

  entry.Clear();
  entry.set_key("key");
  entry.set_value("value");
  entry.set_description("絵文字");
  EXPECT_TRUE(predictor_peer.IsValidEntry(entry));
  EXPECT_TRUE(predictor_peer.IsValidEntryIgnoringRemovedField(entry));

  // An android pua emoji. It is obsolete and should return false.
  *entry.mutable_value() = Util::CodepointToUtf8(0xFE000);
  EXPECT_FALSE(predictor_peer.IsValidEntry(entry));
  EXPECT_FALSE(predictor_peer.IsValidEntryIgnoringRemovedField(entry));

  // Set up suppression dictionary
  {
    user_dictionary::UserDictionaryStorage storage;
    auto* entry = storage.add_dictionaries()->add_entries();
    entry->set_key("foo");
    entry->set_value("bar");
    entry->set_pos(user_dictionary::UserDictionary::SUPPRESSION_WORD);
    GetUserDictionary().Load(std::move(storage));
    GetUserDictionary().WaitForReloader();
  }

  entry.set_key("key");
  entry.set_value("value");
  EXPECT_TRUE(predictor_peer.IsValidEntry(entry));
  EXPECT_TRUE(predictor_peer.IsValidEntryIgnoringRemovedField(entry));

  entry.set_key("foo");
  entry.set_value("bar");
  EXPECT_FALSE(predictor_peer.IsValidEntry(entry));
  EXPECT_FALSE(predictor_peer.IsValidEntryIgnoringRemovedField(entry));
}

TEST_F(UserHistoryPredictorTest, IsValidSuggestion) {
  UserHistoryPredictor::Entry entry;

  Request request;
  request.set_zero_query_suggestion(false);
  const ConversionRequest convreq =
      ConversionRequestBuilder().SetRequestView(request).Build();

  EXPECT_FALSE(
      UserHistoryPredictorTestPeer::IsValidSuggestion(convreq, 1, entry));

  entry.set_bigram_boost(true);
  EXPECT_TRUE(
      UserHistoryPredictorTestPeer::IsValidSuggestion(convreq, 1, entry));

  entry.set_bigram_boost(false);
  entry.set_suggestion_freq(10);
  EXPECT_TRUE(
      UserHistoryPredictorTestPeer::IsValidSuggestion(convreq, 1, entry));

  entry.set_bigram_boost(false);
  request.set_zero_query_suggestion(true);
  EXPECT_TRUE(
      UserHistoryPredictorTestPeer::IsValidSuggestion(convreq, 1, entry));
}

TEST_F(UserHistoryPredictorTest, IsValidSuggestionForMixedConversion) {
  UserHistoryPredictor::Entry entry;
  const ConversionRequest conversion_request;

  entry.set_suggestion_freq(1);
  EXPECT_TRUE(UserHistoryPredictorTestPeer::IsValidSuggestionForMixedConversion(
      conversion_request, 1, entry));

  entry.set_value("よろしくおねがいします。");  // too long
  EXPECT_FALSE(
      UserHistoryPredictorTestPeer::IsValidSuggestionForMixedConversion(
          conversion_request, 1, entry));
}

TEST_F(UserHistoryPredictorTest, EntryPriorityQueueTest) {
  // removed automatically
  constexpr int kSize = 10000;
  {
    UserHistoryPredictorTestPeer::EntryPriorityQueue queue;
    for (int i = 0; i < 10000; ++i) {
      EXPECT_NE(nullptr, queue.NewEntry());
    }
  }

  {
    UserHistoryPredictorTestPeer::EntryPriorityQueue queue;
    std::vector<UserHistoryPredictor::Entry*> expected;
    for (int i = 0; i < kSize; ++i) {
      UserHistoryPredictor::Entry* entry = queue.NewEntry();
      entry->set_key("test" + std::to_string(i));
      entry->set_value("test" + std::to_string(i));
      entry->set_last_access_time(i + 1000);
      expected.push_back(entry);
      EXPECT_TRUE(queue.Push(entry));
    }

    int n = kSize - 1;
    while (true) {
      const UserHistoryPredictor::Entry* entry = queue.Pop();
      if (entry == nullptr) {
        break;
      }
      EXPECT_EQ(entry, expected[n]);
      --n;
    }
    EXPECT_EQ(n, -1);
  }

  {
    UserHistoryPredictorTestPeer::EntryPriorityQueue queue;
    for (int i = 0; i < 5; ++i) {
      UserHistoryPredictor::Entry* entry = queue.NewEntry();
      entry->set_key("test");
      entry->set_value("test");
      queue.Push(entry);
    }
    EXPECT_EQ(queue.size(), 1);

    for (int i = 0; i < 5; ++i) {
      UserHistoryPredictor::Entry* entry = queue.NewEntry();
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
  const char* scenario_description;
  const char* input;
  const char* output;
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
  UserHistoryPredictor* predictor = GetUserHistoryPredictor();
  std::vector<Result> results;

  for (const PrivacySensitiveTestData& data : kNonSensitiveCases) {
    predictor->ClearAllHistory();
    WaitForSyncer(predictor);

    const std::string description(data.scenario_description);
    const std::string input(data.input);
    const std::string output(data.output);
    const std::string partial_input(RemoveLastCodepointCharacter(input));
    const bool expect_sensitive = data.is_sensitive;

    // Initial commit.
    {
      SegmentsProxy segments_proxy;
      const ConversionRequest convreq =
          SetUpInputForConversion(input, &composer_, &segments_proxy);
      segments_proxy.AddCandidate(0, output);
      predictor->Finish(convreq, segments_proxy.MakeLearningResults(),
                        kRevertId);
    }

    // TODO(yukawa): Refactor the scenario runner below by making
    //     some utility functions.

    // Check suggestion
    {
      SegmentsProxy segments_proxy;
      const ConversionRequest convreq1 =
          SetUpInputForSuggestion(partial_input, &composer_, &segments_proxy);
      if (expect_sensitive) {
        results = predictor->Predict(convreq1);
        EXPECT_TRUE(results.empty())
            << description << " input: " << input << " output: " << output;
      } else {
        results = predictor->Predict(convreq1);
        EXPECT_FALSE(results.empty())
            << description << " input: " << input << " output: " << output;
      }
      segments_proxy.Clear();
      const ConversionRequest convreq2 =
          SetUpInputForPrediction(input, &composer_, &segments_proxy);
      if (expect_sensitive) {
        results = predictor->Predict(convreq2);
        EXPECT_TRUE(results.empty())
            << description << " input: " << input << " output: " << output;
      } else {
        results = predictor->Predict(convreq2);
        EXPECT_FALSE(results.empty())
            << description << " input: " << input << " output: " << output;
      }
    }

    // Check Prediction
    {
      SegmentsProxy segments_proxy;
      const ConversionRequest convreq1 =
          SetUpInputForPrediction(partial_input, &composer_, &segments_proxy);
      if (expect_sensitive) {
        results = predictor->Predict(convreq1);
        EXPECT_TRUE(results.empty())
            << description << " input: " << input << " output: " << output;
      } else {
        results = predictor->Predict(convreq1);
        EXPECT_FALSE(results.empty())
            << description << " input: " << input << " output: " << output;
      }
      segments_proxy.Clear();
      const ConversionRequest convreq2 =
          SetUpInputForPrediction(input, &composer_, &segments_proxy);
      if (expect_sensitive) {
        results = predictor->Predict(convreq2);
        EXPECT_TRUE(results.empty())
            << description << " input: " << input << " output: " << output;
      } else {
        results = predictor->Predict(convreq2);
        EXPECT_FALSE(results.empty())
            << description << " input: " << input << " output: " << output;
      }
    }
  }
}

TEST_F(UserHistoryPredictorTest, PrivacySensitiveMultiSegmentsTest) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictor();
  WaitForSyncer(predictor);
  std::vector<Result> results;

  // If a password-like input consists of multiple segments, it is not
  // considered to be privacy sensitive when the input is committed.
  // Currently this is a known issue.
  {
    SegmentsProxy segments_proxy;
    const ConversionRequest convreq =
        SetUpInputForConversion("123", &composer_, &segments_proxy);
    segments_proxy.AddSegment("abc!");
    segments_proxy.AddCandidate(0, "123");
    segments_proxy.AddCandidate(1, "abc!");
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
  }

  {
    SegmentsProxy segments_proxy;
    const ConversionRequest convreq1 =
        SetUpInputForSuggestion("123abc", &composer_, &segments_proxy);
    results = predictor->Predict(convreq1);
    EXPECT_FALSE(results.empty());
    segments_proxy.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForSuggestion("123abc!", &composer_, &segments_proxy);
    results = predictor->Predict(convreq2);
    EXPECT_FALSE(results.empty());
  }

  {
    SegmentsProxy segments_proxy;
    const ConversionRequest convreq1 =
        SetUpInputForPrediction("123abc", &composer_, &segments_proxy);
    results = predictor->Predict(convreq1);
    EXPECT_FALSE(results.empty());
    segments_proxy.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForPrediction("123abc!", &composer_, &segments_proxy);
    results = predictor->Predict(convreq2);
    EXPECT_FALSE(results.empty());
  }
}

TEST_F(UserHistoryPredictorTest, UserHistoryStorage) {
  const std::string filename =
      FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), "test");

  UserHistoryStorage storage1(filename);

  UserHistoryPredictor::Entry* entry = storage1.GetProto().add_entries();
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

TEST_F(UserHistoryPredictorTest, UserHistoryStorageContainingInvalidEntries) {
  // This test checks invalid entries are not loaded into dic_.
  ScopedClockMock clock(absl::FromUnixSeconds(1));
  TempDirectory temp_dir = testing::MakeTempDirectoryOrDie();

  // Create a history proto containing invalid entries (timestamp = 1).
  user_history_predictor::UserHistory history;

  // Invalid UTF8.
  for (const char* value : {
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
    auto* entry = history.add_entries();
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

    // Only the valid entries are loaded.
    EXPECT_EQ(storage.GetProto().entries_size(), 9);

    UserHistoryPredictor* predictor = GetUserHistoryPredictor();
    EXPECT_TRUE(LoadStorage(predictor, std::move(storage)));
    EXPECT_EQ(EntrySize(*predictor), 0);
  }
}

TEST_F(UserHistoryPredictorTest, RomanFuzzyPrefixMatch) {
  // same
  EXPECT_FALSE(
      UserHistoryPredictorTestPeer::RomanFuzzyPrefixMatch("abc", "abc"));
  EXPECT_FALSE(UserHistoryPredictorTestPeer::RomanFuzzyPrefixMatch("a", "a"));

  // exact prefix
  EXPECT_FALSE(UserHistoryPredictorTestPeer::RomanFuzzyPrefixMatch("abc", "a"));
  EXPECT_FALSE(
      UserHistoryPredictorTestPeer::RomanFuzzyPrefixMatch("abc", "ab"));
  EXPECT_FALSE(UserHistoryPredictorTestPeer::RomanFuzzyPrefixMatch("abc", ""));

  // swap
  EXPECT_TRUE(UserHistoryPredictorTestPeer::RomanFuzzyPrefixMatch("ab", "ba"));
  EXPECT_TRUE(
      UserHistoryPredictorTestPeer::RomanFuzzyPrefixMatch("abfoo", "bafoo"));
  EXPECT_TRUE(
      UserHistoryPredictorTestPeer::RomanFuzzyPrefixMatch("fooab", "fooba"));
  EXPECT_TRUE(UserHistoryPredictorTestPeer::RomanFuzzyPrefixMatch("fooabfoo",
                                                                  "foobafoo"));

  // swap + prefix
  EXPECT_TRUE(
      UserHistoryPredictorTestPeer::RomanFuzzyPrefixMatch("fooabfoo", "fooba"));

  // deletion
  EXPECT_TRUE(
      UserHistoryPredictorTestPeer::RomanFuzzyPrefixMatch("abcd", "acd"));
  EXPECT_TRUE(
      UserHistoryPredictorTestPeer::RomanFuzzyPrefixMatch("abcd", "bcd"));

  // deletion + prefix
  EXPECT_TRUE(
      UserHistoryPredictorTestPeer::RomanFuzzyPrefixMatch("abcdf", "acd"));
  EXPECT_TRUE(
      UserHistoryPredictorTestPeer::RomanFuzzyPrefixMatch("abcdfoo", "bcd"));

  // voice sound mark
  EXPECT_TRUE(UserHistoryPredictorTestPeer::RomanFuzzyPrefixMatch("gu-guru",
                                                                  "gu^guru"));
  EXPECT_TRUE(UserHistoryPredictorTestPeer::RomanFuzzyPrefixMatch("gu-guru",
                                                                  "gu=guru"));
  EXPECT_TRUE(
      UserHistoryPredictorTestPeer::RomanFuzzyPrefixMatch("gu-guru", "gu^gu"));
  EXPECT_FALSE(
      UserHistoryPredictorTestPeer::RomanFuzzyPrefixMatch("gu-guru", "gugu"));

  // Invalid
  EXPECT_FALSE(UserHistoryPredictorTestPeer::RomanFuzzyPrefixMatch("", ""));
  EXPECT_FALSE(UserHistoryPredictorTestPeer::RomanFuzzyPrefixMatch("", "a"));
  EXPECT_FALSE(
      UserHistoryPredictorTestPeer::RomanFuzzyPrefixMatch("abcde", "defe"));
}

TEST_F(UserHistoryPredictorTest, MaybeRomanMisspelledKey) {
  EXPECT_TRUE(
      UserHistoryPredictorTestPeer::MaybeRomanMisspelledKey("こんぴゅーｔ"));
  EXPECT_TRUE(
      UserHistoryPredictorTestPeer::MaybeRomanMisspelledKey("こんぴゅーt"));
  EXPECT_FALSE(
      UserHistoryPredictorTestPeer::MaybeRomanMisspelledKey("こんぴゅーた"));
  EXPECT_TRUE(
      UserHistoryPredictorTestPeer::MaybeRomanMisspelledKey("ぱｓこん"));
  EXPECT_FALSE(
      UserHistoryPredictorTestPeer::MaybeRomanMisspelledKey("ぱそこん"));
  EXPECT_TRUE(UserHistoryPredictorTestPeer::MaybeRomanMisspelledKey(
      "おねがいしまうｓ"));
  EXPECT_FALSE(
      UserHistoryPredictorTestPeer::MaybeRomanMisspelledKey("おねがいします"));
  EXPECT_TRUE(
      UserHistoryPredictorTestPeer::MaybeRomanMisspelledKey("いんた=ねっと"));
  EXPECT_FALSE(UserHistoryPredictorTestPeer::MaybeRomanMisspelledKey("ｔ"));
  EXPECT_TRUE(UserHistoryPredictorTestPeer::MaybeRomanMisspelledKey("ーｔ"));
  EXPECT_FALSE(UserHistoryPredictorTestPeer::MaybeRomanMisspelledKey(
      "おｎがいしまうｓ"));
  // Two unknowns
  EXPECT_FALSE(UserHistoryPredictorTestPeer::MaybeRomanMisspelledKey(
      "お＆がい＄しまう"));
  // One alpha and one unknown
  EXPECT_FALSE(UserHistoryPredictorTestPeer::MaybeRomanMisspelledKey(
      "お＆がいしまうｓ"));
}

TEST_F(UserHistoryPredictorTest, GetRomanMisspelledKey) {
  SegmentsProxy segments_proxy;

  config_.set_preedit_method(config::Config::ROMAN);

  auto convreq = [&](absl::string_view key) {
    return ConversionRequestBuilder()
        .SetConfigView(config_)
        .SetKey(key)
        .Build();
  };

  EXPECT_EQ(UserHistoryPredictorTestPeer::GetRomanMisspelledKey(convreq("")),
            "");

  EXPECT_EQ(UserHistoryPredictorTestPeer::GetRomanMisspelledKey(
                convreq("おねがいしまうs")),
            "onegaisimaus");

  EXPECT_EQ(UserHistoryPredictorTestPeer::GetRomanMisspelledKey(
                convreq("おねがいします")),
            "");

  config_.set_preedit_method(config::Config::KANA);

  EXPECT_EQ(UserHistoryPredictorTestPeer::GetRomanMisspelledKey(
                convreq("おねがいしまうs")),
            "");

  EXPECT_EQ(UserHistoryPredictorTestPeer::GetRomanMisspelledKey(
                convreq("おねがいします")),
            "");
}

TEST_F(UserHistoryPredictorTest, RomanFuzzyLookupEntry) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictor();
  UserHistoryPredictorTestPeer predictor_peer(*predictor);
  UserHistoryPredictor::Entry entry;
  UserHistoryPredictorTestPeer::EntryPriorityQueue results;

  entry.set_key("");
  EXPECT_FALSE(predictor_peer.RomanFuzzyLookupEntry("", &entry, &results));

  entry.set_key("よろしく");
  EXPECT_TRUE(
      predictor_peer.RomanFuzzyLookupEntry("yorosku", &entry, &results));
  EXPECT_TRUE(
      predictor_peer.RomanFuzzyLookupEntry("yrosiku", &entry, &results));
  EXPECT_TRUE(
      predictor_peer.RomanFuzzyLookupEntry("yorsiku", &entry, &results));
  EXPECT_FALSE(predictor_peer.RomanFuzzyLookupEntry("yrsk", &entry, &results));
  EXPECT_FALSE(
      predictor_peer.RomanFuzzyLookupEntry("yorosiku", &entry, &results));

  entry.set_key("ぐーぐる");
  EXPECT_TRUE(
      predictor_peer.RomanFuzzyLookupEntry("gu=guru", &entry, &results));
  EXPECT_FALSE(
      predictor_peer.RomanFuzzyLookupEntry("gu-guru", &entry, &results));
  EXPECT_FALSE(
      predictor_peer.RomanFuzzyLookupEntry("g=guru", &entry, &results));
}

namespace {
struct LookupTestData {
  absl::string_view entry_key;
  bool expect_result;
};
}  // namespace

TEST_F(UserHistoryPredictorTest, ExpandedLookupRoman) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictor();
  UserHistoryPredictorTestPeer predictor_peer(*predictor);
  UserHistoryPredictor::Entry entry;
  UserHistoryPredictorTestPeer::EntryPriorityQueue results;

  // Roman
  // preedit: "あｋ"
  // request_key: "あｋ"
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

  const ConversionRequest convreq = ConversionRequestBuilder().Build();

  // with expanded
  for (size_t i = 0; i < std::size(kTests1); ++i) {
    entry.set_key(kTests1[i].entry_key);
    EXPECT_EQ(predictor_peer.LookupEntry(convreq, "あｋ", "あ", expanded.get(),
                                         &entry, nullptr, &results),
              kTests1[i].expect_result)
        << kTests1[i].entry_key;
  }

  // only expanded
  // preedit: "k"
  // request_key: ""
  // key_base: ""
  // key_expanded: "か","き","く","け", "こ"

  constexpr LookupTestData kTests2[] = {
      {"", false},    {"か", true},    {"き", true},
      {"かい", true}, {"まい", false}, {"も", false},
  };

  for (size_t i = 0; i < std::size(kTests2); ++i) {
    entry.set_key(kTests2[i].entry_key);
    EXPECT_EQ(predictor_peer.LookupEntry(convreq, "", "", expanded.get(),
                                         &entry, nullptr, &results),
              kTests2[i].expect_result)
        << kTests2[i].entry_key;
  }
}

TEST_F(UserHistoryPredictorTest, ExpandedLookupKana) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictor();
  UserHistoryPredictorTestPeer predictor_peer(*predictor);
  UserHistoryPredictor::Entry entry;
  UserHistoryPredictorTestPeer::EntryPriorityQueue results;

  // Kana
  // preedit: "あし"
  // request_key: "あし"
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

  const ConversionRequest convreq = ConversionRequestBuilder().Build();

  // with expanded
  for (size_t i = 0; i < std::size(kTests1); ++i) {
    entry.set_key(kTests1[i].entry_key);
    EXPECT_EQ(predictor_peer.LookupEntry(convreq, "あし", "あ", expanded.get(),
                                         &entry, nullptr, &results),
              kTests1[i].expect_result)
        << kTests1[i].entry_key;
  }

  // only expanded
  // request_key: "し"
  // key_base: ""
  // key_expanded: "し","じ"
  constexpr LookupTestData kTests2[] = {
      {"", false},          {"し", true},         {"じ", true},
      {"しかうまい", true}, {"じゅうかい", true}, {"ま", false},
      {"まめ", false},
  };

  for (size_t i = 0; i < std::size(kTests2); ++i) {
    entry.set_key(kTests2[i].entry_key);
    EXPECT_EQ(predictor_peer.LookupEntry(convreq, "し", "", expanded.get(),
                                         &entry, nullptr, &results),
              kTests2[i].expect_result)
        << kTests2[i].entry_key;
  }
}

TEST_F(UserHistoryPredictorTest, GetMatchTypeFromInputRoman) {
  // We have to define this here,
  using MatchType = UserHistoryPredictorTestPeer::MatchType;

  struct MatchTypeTestData {
    absl::string_view target;
    MatchType expect_type;
  };

  // Roman
  // preedit: "あｋ"
  // request_key: "あ"
  // key_base: "あ"
  // key_expanded: "か","き","く","け", "こ"
  auto expanded = std::make_unique<Trie<std::string>>();
  expanded->AddEntry("か", "か");
  expanded->AddEntry("き", "き");
  expanded->AddEntry("く", "く");
  expanded->AddEntry("け", "け");
  expanded->AddEntry("こ", "こ");

  constexpr MatchTypeTestData kTests1[] = {
      {"", MatchType::NO_MATCH},
      {"い", MatchType::NO_MATCH},
      {"あ", MatchType::RIGHT_PREFIX_MATCH},
      {"あい", MatchType::NO_MATCH},
      {"あか", MatchType::LEFT_PREFIX_MATCH},
      {"あかい", MatchType::LEFT_PREFIX_MATCH},
  };

  for (size_t i = 0; i < std::size(kTests1); ++i) {
    EXPECT_EQ(UserHistoryPredictorTestPeer::GetMatchTypeFromInput(
                  "あ", "あ", expanded.get(), kTests1[i].target),
              kTests1[i].expect_type)
        << kTests1[i].target;
  }

  // only expanded
  // preedit: "ｋ"
  // request_key: ""
  // key_base: ""
  // key_expanded: "か","き","く","け", "こ"
  constexpr MatchTypeTestData kTests2[] = {
      {"", MatchType::NO_MATCH},
      {"い", MatchType::NO_MATCH},
      {"いか", MatchType::NO_MATCH},
      {"か", MatchType::LEFT_PREFIX_MATCH},
      {"かいがい", MatchType::LEFT_PREFIX_MATCH},
  };

  for (size_t i = 0; i < std::size(kTests2); ++i) {
    EXPECT_EQ(UserHistoryPredictorTestPeer::GetMatchTypeFromInput(
                  "", "", expanded.get(), kTests2[i].target),
              kTests2[i].expect_type)
        << kTests2[i].target;
  }
}

TEST_F(UserHistoryPredictorTest, GetMatchTypeFromInputKana) {
  using MatchType = UserHistoryPredictorTestPeer::MatchType;

  // We have to define this here,
  // because UserHistoryPredictor::MatchType is private
  struct MatchTypeTestData {
    absl::string_view target;
    MatchType expect_type;
  };

  // Kana
  // preedit: "あし"
  // request_key: "あし"
  // key_base: "あ"
  // key_expanded: "し","じ"
  auto expanded = std::make_unique<Trie<std::string>>();
  expanded->AddEntry("し", "し");
  expanded->AddEntry("じ", "じ");

  constexpr MatchTypeTestData kTests1[] = {
      {"", MatchType::NO_MATCH},
      {"い", MatchType::NO_MATCH},
      {"いし", MatchType::NO_MATCH},
      {"あ", MatchType::RIGHT_PREFIX_MATCH},
      {"あし", MatchType::EXACT_MATCH},
      {"あじ", MatchType::LEFT_PREFIX_MATCH},
      {"あした", MatchType::LEFT_PREFIX_MATCH},
      {"あじしお", MatchType::LEFT_PREFIX_MATCH},
  };

  for (size_t i = 0; i < std::size(kTests1); ++i) {
    EXPECT_EQ(UserHistoryPredictorTestPeer::GetMatchTypeFromInput(
                  "あし", "あ", expanded.get(), kTests1[i].target),
              kTests1[i].expect_type)
        << kTests1[i].target;
  }

  // only expanded
  // preedit: "し"
  // request_key: "し"
  // key_base: ""
  // key_expanded: "し","じ"
  constexpr MatchTypeTestData kTests2[] = {
      {"", MatchType::NO_MATCH},
      {"い", MatchType::NO_MATCH},
      {"し", MatchType::EXACT_MATCH},
      {"じ", MatchType::LEFT_PREFIX_MATCH},
      {"しじみ", MatchType::LEFT_PREFIX_MATCH},
      {"じかん", MatchType::LEFT_PREFIX_MATCH},
  };

  for (size_t i = 0; i < std::size(kTests2); ++i) {
    EXPECT_EQ(UserHistoryPredictorTestPeer::GetMatchTypeFromInput(
                  "し", "", expanded.get(), kTests2[i].target),
              kTests2[i].expect_type)
        << kTests2[i].target;
  }
}

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsRoman) {
  table_->LoadFromFile("system://romanji-hiragana.tsv");
  SegmentsProxy segments_proxy;

  const ConversionRequest convreq =
      InitSegmentsFromInputSequence("gu-g", &composer_, &segments_proxy);
  std::string request_key;
  std::string base;
  std::unique_ptr<Trie<std::string>> expanded;
  UserHistoryPredictorTestPeer::GetInputKeyFromRequest(convreq, &request_key,
                                                       &base, &expanded);
  EXPECT_EQ(request_key, "ぐーｇ");
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
  SegmentsProxy segments_proxy;
  Random random;

  for (size_t i = 0; i < 1000; ++i) {
    composer_.Reset();
    const std::string input = random.Utf8StringRandomLen(4, ' ', '~');
    const ConversionRequest convreq =
        InitSegmentsFromInputSequence(input, &composer_, &segments_proxy);
    std::string request_key;
    std::string base;
    std::unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictorTestPeer::GetInputKeyFromRequest(convreq, &request_key,
                                                         &base, &expanded);
  }
}

// Found by random test.
// request_key != base by composer modification.
TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsShouldNotCrash) {
  table_->LoadFromFile("system://romanji-hiragana.tsv");
  SegmentsProxy segments_proxy;

  {
    const ConversionRequest convreq =
        InitSegmentsFromInputSequence("8,+", &composer_, &segments_proxy);
    std::string request_key;
    std::string base;
    std::unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictorTestPeer::GetInputKeyFromRequest(convreq, &request_key,
                                                         &base, &expanded);
  }
}

TEST_F(UserHistoryPredictorTest, GetInputKeyFromSegmentsRomanN) {
  table_->LoadFromFile("system://romanji-hiragana.tsv");
  SegmentsProxy segments_proxy;

  {
    const ConversionRequest convreq =
        InitSegmentsFromInputSequence("n", &composer_, &segments_proxy);
    std::string request_key;
    std::string base;
    std::unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictorTestPeer::GetInputKeyFromRequest(convreq, &request_key,
                                                         &base, &expanded);
    EXPECT_EQ(request_key, "ｎ");
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
  segments_proxy.Clear();
  {
    const ConversionRequest convreq =
        InitSegmentsFromInputSequence("nn", &composer_, &segments_proxy);
    std::string request_key;
    std::string base;
    std::unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictorTestPeer::GetInputKeyFromRequest(convreq, &request_key,
                                                         &base, &expanded);
    EXPECT_EQ(request_key, "ん");
    EXPECT_EQ(base, "ん");
    EXPECT_TRUE(expanded == nullptr);
  }

  composer_.Reset();
  segments_proxy.Clear();
  {
    const ConversionRequest convreq =
        InitSegmentsFromInputSequence("n'", &composer_, &segments_proxy);
    std::string request_key;
    std::string base;
    std::unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictorTestPeer::GetInputKeyFromRequest(convreq, &request_key,
                                                         &base, &expanded);
    EXPECT_EQ(request_key, "ん");
    EXPECT_EQ(base, "ん");
    EXPECT_TRUE(expanded == nullptr);
  }

  composer_.Reset();
  segments_proxy.Clear();
  {
    const ConversionRequest convreq =
        InitSegmentsFromInputSequence("n'n", &composer_, &segments_proxy);
    std::string request_key;
    std::string base;
    std::unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictorTestPeer::GetInputKeyFromRequest(convreq, &request_key,
                                                         &base, &expanded);
    EXPECT_EQ(request_key, "んｎ");
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
  SegmentsProxy segments_proxy;

  {
    const ConversionRequest convreq =
        InitSegmentsFromInputSequence("/", &composer_, &segments_proxy);
    std::string request_key;
    std::string base;
    std::unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictorTestPeer::GetInputKeyFromRequest(convreq, &request_key,
                                                         &base, &expanded);
    EXPECT_EQ(request_key, "ん");
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
  SegmentsProxy segments_proxy;

  {
    const ConversionRequest convreq =
        InitSegmentsFromInputSequence("わ00", &composer_, &segments_proxy);
    std::string request_key;
    std::string base;
    std::unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictorTestPeer::GetInputKeyFromRequest(convreq, &request_key,
                                                         &base, &expanded);
    EXPECT_EQ(request_key, "ん");
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
  SegmentsProxy segments_proxy;

  const ConversionRequest convreq =
      InitSegmentsFromInputSequence("あか", &composer_, &segments_proxy);

  {
    std::string request_key;
    std::string base;
    std::unique_ptr<Trie<std::string>> expanded;
    UserHistoryPredictorTestPeer::GetInputKeyFromRequest(convreq, &request_key,
                                                         &base, &expanded);
    EXPECT_EQ(request_key, "あか");
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
  for (bool mixed_conversion : {true, false}) {
    UserHistoryPredictor* predictor =
        GetUserHistoryPredictorWithClearedHistory();

    SegmentsProxy segments_proxy;
    std::vector<Result> results;

    request_.set_mixed_conversion(mixed_conversion);

    {
      constexpr absl::string_view kKey = "わたしのなまえはなかのです";
      constexpr absl::string_view kValue = "私の名前は中野です";
      const ConversionRequest convreq1 =
          SetUpInputForPrediction(kKey, &composer_, &segments_proxy);
      segments_proxy.AddCandidate(0, kValue);
      // "わたしの, 私の", "わたし, 私"
      segments_proxy.PushBackInnerSegmentBoundary(0, 0, 12, 6, 9, 3);
      // "なまえは, 名前は", "なまえ, 名前"
      segments_proxy.PushBackInnerSegmentBoundary(0, 0, 12, 9, 9, 6);
      // "なかのです, 中野です", "なかの, 中野"
      segments_proxy.PushBackInnerSegmentBoundary(0, 0, 15, 12, 9, 6);
      predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                        kRevertId);
    }
    segments_proxy.Clear();

    const ConversionRequest convreq2 =
        SetUpInputForPrediction("なかの", &composer_, &segments_proxy);
    results = predictor->Predict(convreq2);
    EXPECT_FALSE(results.empty());
    if (mixed_conversion) {
      EXPECT_TRUE(FindCandidateByValue("中野", results));
    } else {
      EXPECT_TRUE(FindCandidateByValue("中野です", results));
    }
    segments_proxy.Clear();

    const ConversionRequest convreq3 =
        SetUpInputForPrediction("なかので", &composer_, &segments_proxy);
    results = predictor->Predict(convreq3);
    EXPECT_FALSE(results.empty());
    EXPECT_TRUE(FindCandidateByValue("中野です", results));

    for (const bool disable_joined_prediction : {true, false}) {
      request_.mutable_decoder_experiment_params()
          ->set_user_history_disable_joined_prediction(
              disable_joined_prediction);
      segments_proxy.Clear();
      const ConversionRequest convreq4 =
          SetUpInputForPrediction("なまえ", &composer_, &segments_proxy);
      results = predictor->Predict(convreq4);
      EXPECT_FALSE(results.empty());
      if (mixed_conversion) {
        EXPECT_TRUE(FindCandidateByValue("名前", results));
        if (!disable_joined_prediction) {
          // "名前は", "中野" were typed in different segments, but predict
          // joined phrase.
          EXPECT_TRUE(FindCandidateByValue("名前は中野", results));
        }
      } else {
        EXPECT_TRUE(FindCandidateByValue("名前は", results));
        if (!disable_joined_prediction) {
          // "名前は", "中野です" were typed in different segments, but predict
          // joined phrase.
          EXPECT_TRUE(FindCandidateByValue("名前は中野です", results));
        }
      }
    }
  }
}

TEST_F(UserHistoryPredictorTest, ZeroQueryFromRealtimeConversion) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();
  request_.set_mixed_conversion(true);

  SegmentsProxy segments_proxy;
  {
    constexpr absl::string_view kKey = "わたしのなまえはなかのです";
    constexpr absl::string_view kValue = "私の名前は中野です";
    const ConversionRequest convreq =
        SetUpInputForPrediction(kKey, &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, kValue);
    // "わたしの, 私の", "わたし, 私"
    segments_proxy.PushBackInnerSegmentBoundary(0, 0, 12, 6, 9, 3);
    // "なまえは, 名前は", "なまえ, 名前"
    segments_proxy.PushBackInnerSegmentBoundary(0, 0, 12, 9, 9, 6);
    // "なかのです, 中野です", "なかの, 中野"
    segments_proxy.PushBackInnerSegmentBoundary(0, 0, 15, 12, 9, 6);
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
  }
  segments_proxy.Clear();

  const ConversionRequest convreq2 =
      SetUpInputForConversion("わたしの", &composer_, &segments_proxy);
  segments_proxy.AddCandidate(0, "私の");
  predictor->Finish(convreq2, segments_proxy.MakeLearningResults(), kRevertId);
  segments_proxy.SetHistoryType(0);

  commands::Request request;
  request_.set_zero_query_suggestion(true);
  const ConversionRequest convreq3 = SetUpInputForSuggestionWithHistory(
      "", "わたしの", "私の", &composer_, &segments_proxy);
  const std::vector<Result> results = predictor->Predict(convreq3);
  EXPECT_FALSE(results.empty());
  EXPECT_TRUE(FindCandidateByValue("名前", results));
}

TEST_F(UserHistoryPredictorTest, LongCandidateForMobile) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  request_test_util::FillMobileRequest(&request_);
  std::vector<Result> results;

  SegmentsProxy segments_proxy;
  for (size_t i = 0; i < 3; ++i) {
    constexpr absl::string_view kKey = "よろしくおねがいします";
    constexpr absl::string_view kValue = "よろしくお願いします";
    const ConversionRequest convreq =
        SetUpInputForPrediction(kKey, &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, kValue);
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
    segments_proxy.Clear();
  }

  const ConversionRequest convreq =
      SetUpInputForPrediction("よろ", &composer_, &segments_proxy);
  results = predictor->Predict(convreq);
  EXPECT_FALSE(results.empty());
  EXPECT_TRUE(FindCandidateByValue("よろしくお願いします", results));
}

TEST_F(UserHistoryPredictorTest, EraseNextEntries) {
  UserHistoryPredictor::Entry e;
  e.add_next_entries()->set_entry_fp(100);
  e.add_next_entries()->set_entry_fp(10);
  e.add_next_entries()->set_entry_fp(30);
  e.add_next_entries()->set_entry_fp(10);
  e.add_next_entries()->set_entry_fp(100);

  UserHistoryPredictorTestPeer::EraseNextEntries(1234, &e);
  EXPECT_EQ(e.next_entries_size(), 5);

  UserHistoryPredictorTestPeer::EraseNextEntries(30, &e);
  ASSERT_EQ(e.next_entries_size(), 4);
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_NE(e.next_entries(i).entry_fp(), 30);
  }

  UserHistoryPredictorTestPeer::EraseNextEntries(10, &e);
  ASSERT_EQ(e.next_entries_size(), 2);
  for (size_t i = 0; i < 2; ++i) {
    EXPECT_NE(e.next_entries(i).entry_fp(), 10);
  }

  UserHistoryPredictorTestPeer::EraseNextEntries(100, &e);
  EXPECT_EQ(e.next_entries_size(), 0);
}

TEST_F(UserHistoryPredictorTest, RemoveNgramChain) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();
  UserHistoryPredictorTestPeer predictor_peer(*predictor);

  // Set up the following chain of next entries:
  // ("abc", "ABC")
  // (  "a",   "A") --- ("b", "B") --- ("c", "C")
  UserHistoryPredictor::Entry* abc = InsertEntry(predictor, "abc", "ABC");
  UserHistoryPredictor::Entry* a = InsertEntry(predictor, "a", "A");
  UserHistoryPredictor::Entry* b = AppendEntry(predictor, "b", "B", a);
  UserHistoryPredictor::Entry* c = AppendEntry(predictor, "c", "C", b);

  std::vector<UserHistoryPredictor::Entry*> entries;
  entries.push_back(abc);
  entries.push_back(a);
  entries.push_back(b);
  entries.push_back(c);

  using RemoveNgramChainResult =
      UserHistoryPredictorTestPeer::RemoveNgramChainResult;

  // The method should return NOT_FOUND for key-value pairs not in the chain.
  for (size_t i = 0; i < entries.size(); ++i) {
    std::vector<absl::string_view> dummy1, dummy2;
    EXPECT_EQ(predictor_peer.RemoveNgramChain("hoge", "HOGE", entries[i],
                                              &dummy1, 0, &dummy2, 0),
              RemoveNgramChainResult::NOT_FOUND);
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
    EXPECT_EQ(predictor_peer.RemoveNgramChain("abc", "ABC", a, &dummy1, 0,
                                              &dummy2, 0),
              RemoveNgramChainResult::DONE);
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
    EXPECT_EQ(
        predictor_peer.RemoveNgramChain("a", "A", a, &dummy1, 0, &dummy2, 0),
        RemoveNgramChainResult::TAIL);
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
        predictor_peer.RemoveNgramChain("ab", "AB", a, &dummy1, 0, &dummy2, 0),
        RemoveNgramChainResult::DONE);
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
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  // Add a unigram history ("japanese", "Japanese").
  UserHistoryPredictor::Entry* e =
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
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the history for ("japaneseinput", "JapaneseInput"). It's assumed that
  // this sentence consists of two segments, "japanese" and "input". So, the
  // following history entries are constructed:
  //   ("japaneseinput", "JapaneseInput")  // Unigram
  //   ("japanese", "Japanese") --- ("input", "Input")  // Bigram chain
  UserHistoryPredictor::Entry* japaneseinput;
  UserHistoryPredictor::Entry* japanese;
  UserHistoryPredictor::Entry* input;
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
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the history for ("japaneseinput", "JapaneseInput"), i.e., the same
  // history structure as ClearHistoryEntry_Bigram_DeleteWhole is constructed.
  UserHistoryPredictor::Entry* japaneseinput;
  UserHistoryPredictor::Entry* japanese;
  UserHistoryPredictor::Entry* input;
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
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the history for ("japaneseinput", "JapaneseInput"), i.e., the same
  // history structure as ClearHistoryEntry_Bigram_DeleteWhole is constructed.
  UserHistoryPredictor::Entry* japaneseinput;
  UserHistoryPredictor::Entry* japanese;
  UserHistoryPredictor::Entry* input;
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
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the history for ("japaneseinputmethod", "JapaneseInputMethod"). It's
  // assumed that this sentence consists of three segments, "japanese", "input"
  // and "method". So, the following history entries are constructed:
  //   ("japaneseinputmethod", "JapaneseInputMethod")  // Unigram
  //   ("japanese", "Japanese") -- ("input", "Input") -- ("method", "Method")
  UserHistoryPredictor::Entry* japaneseinputmethod;
  UserHistoryPredictor::Entry* japanese;
  UserHistoryPredictor::Entry* input;
  UserHistoryPredictor::Entry* method;
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
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the same history structure as ClearHistoryEntry_Trigram_DeleteWhole.
  UserHistoryPredictor::Entry* japaneseinputmethod;
  UserHistoryPredictor::Entry* japanese;
  UserHistoryPredictor::Entry* input;
  UserHistoryPredictor::Entry* method;
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
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the same history structure as ClearHistoryEntry_Trigram_DeleteWhole.
  UserHistoryPredictor::Entry* japaneseinputmethod;
  UserHistoryPredictor::Entry* japanese;
  UserHistoryPredictor::Entry* input;
  UserHistoryPredictor::Entry* method;
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
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the same history structure as ClearHistoryEntry_Trigram_DeleteWhole.
  UserHistoryPredictor::Entry* japaneseinputmethod;
  UserHistoryPredictor::Entry* japanese;
  UserHistoryPredictor::Entry* input;
  UserHistoryPredictor::Entry* method;
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
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the same history structure as ClearHistoryEntry_Trigram_DeleteWhole.
  UserHistoryPredictor::Entry* japaneseinputmethod;
  UserHistoryPredictor::Entry* japanese;
  UserHistoryPredictor::Entry* input;
  UserHistoryPredictor::Entry* method;
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
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  // Make the same history structure as ClearHistoryEntry_Trigram_DeleteWhole.
  UserHistoryPredictor::Entry* japaneseinputmethod;
  UserHistoryPredictor::Entry* japanese;
  UserHistoryPredictor::Entry* input;
  UserHistoryPredictor::Entry* method;
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
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  // Set up history. Convert "ぐーぐｒ" to "グーグr" 3 times.  This emulates a
  // case that a user accidentally input incomplete sequence.
  for (int i = 0; i < 3; ++i) {
    SegmentsProxy segments_proxy;
    const ConversionRequest convreq =
        SetUpInputForConversion("ぐーぐｒ", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "グーグr");
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
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
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  // Set up history. Convert "きょうもいいてんき！" to "今日もいい天気!" 3 times
  // so that the predictor learns the sentence. We assume that this sentence
  // consists of three segments: "今日も|いい天気|!".
  for (int i = 0; i < 3; ++i) {
    SegmentsProxy segments_proxy;

    segments_proxy.AddSegment("きょうも");
    segments_proxy.AddSegment("いいてんき");
    segments_proxy.AddSegment("！");
    segments_proxy.AddCandidate(0, "今日も");
    segments_proxy.AddCandidate(1, "いい天気");
    segments_proxy.AddCandidate(2, "!");

    const ConversionRequest convreq =
        CreateConversionRequest(composer_, segments_proxy);

    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
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
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  request_.set_mixed_conversion(true);

  SegmentsProxy segments_proxy;
  std::vector<Result> results;

  {
    constexpr absl::string_view kKey = "とうきょうかなごやにいきたい";
    constexpr absl::string_view kValue = "東京か名古屋に行きたい";
    const ConversionRequest convreq1 =
        SetUpInputForPrediction(kKey, &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, kValue);
    segments_proxy.PushBackInnerSegmentBoundary(0, 0, 18, 9, 15, 6);
    segments_proxy.PushBackInnerSegmentBoundary(0, 0, 12, 12, 9, 9);
    segments_proxy.PushBackInnerSegmentBoundary(0, 0, 12, 12, 12, 12);
    predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                      kRevertId);
  }

  segments_proxy.Clear();
  const ConversionRequest convreq2 =
      SetUpInputForPrediction("と", &composer_, &segments_proxy);
  results = predictor->Predict(convreq2);
  EXPECT_FALSE(results.empty());
  EXPECT_TRUE(FindCandidateByValue("東京", results));
  EXPECT_FALSE(FindCandidateByValue("東京か", results));

  segments_proxy.Clear();
  const ConversionRequest convreq3 =
      SetUpInputForPrediction("な", &composer_, &segments_proxy);
  results = predictor->Predict(convreq3);
  EXPECT_FALSE(results.empty());
  EXPECT_TRUE(FindCandidateByValue("名古屋", results));
  EXPECT_FALSE(FindCandidateByValue("名古屋に", results));

  segments_proxy.Clear();
  const ConversionRequest convreq4 =
      SetUpInputForPrediction("い", &composer_, &segments_proxy);
  results = predictor->Predict(convreq4);
  EXPECT_FALSE(results.empty());
  EXPECT_TRUE(FindCandidateByValue("行きたい", results));
}

TEST_F(UserHistoryPredictorTest, JoinedSegmentsTestMobile) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();
  request_test_util::FillMobileRequest(&request_);
  SegmentsProxy segments_proxy;
  std::vector<Result> results;

  const ConversionRequest convreq1 =
      SetUpInputForConversion("わたしの", &composer_, &segments_proxy);
  segments_proxy.AddCandidate(0, "私の");

  segments_proxy.AddSegment("なまえは");
  segments_proxy.AddCandidate(1, "名前は");

  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), kRevertId);
  segments_proxy.Clear();

  const ConversionRequest convreq2 =
      SetUpInputForSuggestion("わたし", &composer_, &segments_proxy);
  results = predictor->Predict(convreq2);
  EXPECT_FALSE(results.empty());
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].value, "私の");
  segments_proxy.Clear();

  const ConversionRequest convreq3 =
      SetUpInputForPrediction("わたしの", &composer_, &segments_proxy);
  results = predictor->Predict(convreq3);
  EXPECT_FALSE(results.empty());
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].value, "私の");
  segments_proxy.Clear();

  const ConversionRequest convreq4 =
      SetUpInputForPrediction("わたしのな", &composer_, &segments_proxy);
  results = predictor->Predict(convreq4);
  EXPECT_FALSE(results.empty());
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].value, "私の名前は");
  segments_proxy.Clear();
}

TEST_F(UserHistoryPredictorTest, JoinedSegmentsTestDesktop) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  SegmentsProxy segments_proxy;
  std::vector<Result> results;

  const ConversionRequest convreq1 =
      SetUpInputForConversion("わたしの", &composer_, &segments_proxy);
  segments_proxy.AddCandidate(0, "私の");

  segments_proxy.AddSegment("なまえは");
  segments_proxy.AddCandidate(1, "名前は");

  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), kRevertId);

  segments_proxy.Clear();

  const ConversionRequest convreq2 =
      SetUpInputForSuggestion("わたし", &composer_, &segments_proxy);
  results = predictor->Predict(convreq2);
  EXPECT_FALSE(results.empty());
  EXPECT_EQ(results.size(), 2);
  EXPECT_EQ(results[0].value, "私の");
  EXPECT_EQ(results[1].value, "私の名前は");
  segments_proxy.Clear();

  const ConversionRequest convreq3 =
      SetUpInputForPrediction("わたしの", &composer_, &segments_proxy);
  results = predictor->Predict(convreq3);
  EXPECT_FALSE(results.empty());
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].value, "私の名前は");
  segments_proxy.Clear();

  const ConversionRequest convreq4 =
      SetUpInputForPrediction("わたしのな", &composer_, &segments_proxy);
  results = predictor->Predict(convreq4);
  EXPECT_FALSE(results.empty());
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].value, "私の名前は");
  segments_proxy.Clear();
}

TEST_F(UserHistoryPredictorTest, PunctuationLinkMobile) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();
  request_test_util::FillMobileRequest(&request_);
  SegmentsProxy segments_proxy;
  std::vector<Result> results;

  {
    const ConversionRequest convreq1 =
        SetUpInputForConversion("ございます", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "ございます");
    predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                      kRevertId);

    const ConversionRequest convreq2 = SetUpInputForConversionWithHistory(
        "!", "ございます", "ございます", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(1, "！");
    predictor->Finish(convreq2, segments_proxy.MakeLearningResults(),
                      kRevertId);

    segments_proxy.Clear();
    const ConversionRequest convreq3 =
        SetUpInputForSuggestion("ございま", &composer_, &segments_proxy);
    results = predictor->Predict(convreq3);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, "ございます");
    EXPECT_FALSE(FindCandidateByValue("ございます！", results));

    // Zero query from "ございます" -> "！"
    segments_proxy.Clear();
    // Output of SetupInputForConversion is not used here.
    SetUpInputForConversion("ございます", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "ございます");
    const ConversionRequest convreq4 = SetUpInputForSuggestionWithHistory(
        "", "ございます", "ございます", &composer_, &segments_proxy);
    results = predictor->Predict(convreq4);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, "！");
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    const ConversionRequest convreq1 =
        SetUpInputForConversion("!", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "！");
    predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                      kRevertId);

    const ConversionRequest convreq2 = SetUpInputForSuggestionWithHistory(
        "ございます", "!", "！", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(1, "ございます");
    predictor->Finish(convreq2, segments_proxy.MakeLearningResults(),
                      kRevertId);

    // Zero query from "！" -> no suggestion
    segments_proxy.Clear();
    const ConversionRequest convreq3 = SetUpInputForSuggestionWithHistory(
        "", "!", "！", &composer_, &segments_proxy);
    results = predictor->Predict(convreq3);
    EXPECT_TRUE(results.empty());
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    const ConversionRequest convreq1 =
        SetUpInputForConversion("ございます!", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "ございます！");

    predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                      kRevertId);
    segments_proxy.SetHistoryType(0);

    segments_proxy.AddSegment("よろしくおねがいします");
    segments_proxy.AddCandidate(1, "よろしくお願いします");
    predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                      kRevertId);

    // Zero query from "！" -> no suggestion
    segments_proxy.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForConversion("!", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "！");
    segments_proxy.SetHistoryType(0);
    segments_proxy.AddSegment("");  // empty request
    results = predictor->Predict(convreq2);
    EXPECT_TRUE(results.empty());

    // Zero query from "ございます！" -> no suggestion
    segments_proxy.Clear();
    const ConversionRequest convreq3 =
        SetUpInputForConversion("ございます!", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "ございます！");
    segments_proxy.SetHistoryType(0);
    segments_proxy.AddSegment("");  // empty request
    results = predictor->Predict(convreq3);
    EXPECT_TRUE(results.empty());
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    const ConversionRequest convreq1 =
        SetUpInputForConversion("ございます", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "ございます");
    predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                      kRevertId);

    const ConversionRequest convreq2 = SetUpInputForConversionWithHistory(
        "!よろしくおねがいします", "ございます", "ございます", &composer_,
        &segments_proxy);
    segments_proxy.AddCandidate(1, "！よろしくお願いします");
    predictor->Finish(convreq2, segments_proxy.MakeLearningResults(),
                      kRevertId);

    segments_proxy.Clear();
    const ConversionRequest convreq3 =
        SetUpInputForSuggestion("ございま", &composer_, &segments_proxy);
    results = predictor->Predict(convreq3);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, "ございます");
    EXPECT_FALSE(
        FindCandidateByValue("ございます！よろしくお願いします", results));

    // Zero query from "ございます" -> no suggestion
    const ConversionRequest convreq4 = SetUpInputForConversionWithHistory(
        "", "ございます", "ございます", &composer_, &segments_proxy);
    segments_proxy.AddSegment("");  // empty request
    results = predictor->Predict(convreq4);
    EXPECT_TRUE(results.empty());
  }
}

TEST_F(UserHistoryPredictorTest, PunctuationLinkDesktop) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();
  SegmentsProxy segments_proxy;
  std::vector<Result> results;

  {
    const ConversionRequest convreq1 =
        SetUpInputForConversion("ございます", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "ございます");

    predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                      kRevertId);
    segments_proxy.SetHistoryType(0);

    segments_proxy.AddSegment("!");
    segments_proxy.AddCandidate(1, "！");
    predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                      kRevertId);

    segments_proxy.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForSuggestion("ございま", &composer_, &segments_proxy);
    results = predictor->Predict(convreq2);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, "ございます");
    EXPECT_FALSE(FindCandidateByValue("ございます！", results));

    segments_proxy.Clear();
    const ConversionRequest convreq3 =
        SetUpInputForSuggestion("ございます", &composer_, &segments_proxy);
    results = predictor->Predict(convreq3);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, "ございます");
    EXPECT_FALSE(FindCandidateByValue("ございます！", results));
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    const ConversionRequest convreq1 =
        SetUpInputForConversion("!", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "！");

    predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                      kRevertId);
    segments_proxy.SetHistoryType(0);

    segments_proxy.AddSegment("よろしくおねがいします");
    segments_proxy.AddCandidate(1, "よろしくお願いします");
    predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                      kRevertId);

    segments_proxy.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForSuggestion("!", &composer_, &segments_proxy);
    results = predictor->Predict(convreq2);
    EXPECT_TRUE(results.empty());
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    const ConversionRequest convreq1 =
        SetUpInputForConversion("ございます!", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "ございます！");

    predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                      kRevertId);
    segments_proxy.SetHistoryType(0);

    segments_proxy.AddSegment("よろしくおねがいします");
    segments_proxy.AddCandidate(1, "よろしくお願いします");
    predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                      kRevertId);

    segments_proxy.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForSuggestion("ございます", &composer_, &segments_proxy);
    results = predictor->Predict(convreq2);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, "ございます！");
    EXPECT_FALSE(
        FindCandidateByValue("ございます！よろしくお願いします", results));

    segments_proxy.Clear();
    const ConversionRequest convreq3 =
        SetUpInputForSuggestion("ございます!", &composer_, &segments_proxy);
    results = predictor->Predict(convreq3);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, "ございます！");
    EXPECT_FALSE(
        FindCandidateByValue("ございます！よろしくお願いします", results));
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    const ConversionRequest convreq1 =
        SetUpInputForConversion("ございます", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "ございます");

    predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                      kRevertId);
    segments_proxy.SetHistoryType(0);

    segments_proxy.AddSegment("!よろしくおねがいします");
    segments_proxy.AddCandidate(1, "！よろしくお願いします");
    predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                      kRevertId);

    segments_proxy.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForSuggestion("ございます", &composer_, &segments_proxy);
    results = predictor->Predict(convreq2);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, "ございます");
    EXPECT_FALSE(FindCandidateByValue("ございます！", results));
    EXPECT_FALSE(
        FindCandidateByValue("ございます！よろしくお願いします", results));
  }

  predictor->ClearAllHistory();
  WaitForSyncer(predictor);

  {
    const ConversionRequest convreq1 = SetUpInputForConversion(
        "よろしくおねがいします", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "よろしくお願いします");

    predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                      kRevertId);
    segments_proxy.SetHistoryType(0);

    segments_proxy.AddSegment("!");
    segments_proxy.AddCandidate(1, "！");
    predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                      kRevertId);

    segments_proxy.Clear();
    const ConversionRequest convreq2 = SetUpInputForSuggestion(
        "よろしくおねがいします", &composer_, &segments_proxy);
    results = predictor->Predict(convreq2);
    EXPECT_FALSE(results.empty());
    EXPECT_TRUE(FindCandidateByValue("よろしくお願いします", results));
  }
}

TEST_F(UserHistoryPredictorTest, EntriesMaxTrialSize) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();
  UserHistoryPredictorTestPeer predictor_peer(*predictor);

  // Insert one entry per day.
  for (int i = 0; i < 30; ++i) {
    SegmentsProxy segments_proxy;
    const ConversionRequest convreq = SetUpInputForConversion(
        absl::StrFormat("わたしのなまえ%2d", i), &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, absl::StrFormat("私の名前%2d", i));
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
  }

  for (int trial : {10, 20}) {
    request_.mutable_decoder_experiment_params()
        ->set_user_history_max_suggestion_trial(trial);
    for (int i = 29; i >= 0; --i) {
      SegmentsProxy segments_proxy;
      const ConversionRequest convreq = SetUpInputForSuggestion(
          absl::StrFormat("わたしのなまえ%2d", i), &composer_, &segments_proxy);
      const std::vector<Result> results = predictor->Predict(convreq);
      const int lookup_trial = 29 - i;
      if (lookup_trial < trial) {
        EXPECT_FALSE(results.empty());
      } else {
        EXPECT_TRUE(results.empty());
      }
    }
  }

  request_.mutable_decoder_experiment_params()
      ->set_user_history_max_suggestion_trial(0);
}

TEST_F(UserHistoryPredictorTest, EntriesAreDeletedAtSync) {
  // mode 0 -> delete by lifetime
  // mode 1 -> delete by cache size.

  for (const int mode : {0, 1}) {
    for (const int limit : {10, 20, 30, 40}) {
      ScopedClockMock clock(absl::FromUnixSeconds(1));
      UserHistoryPredictor* predictor =
          GetUserHistoryPredictorWithClearedHistory();
      UserHistoryPredictorTestPeer predictor_peer(*predictor);

      if (mode == 0) {
        predictor_peer.SetEntryLifetimeDays(limit);
        EXPECT_EQ(predictor_peer.entry_lifetime_days_(), limit);
      } else {
        predictor_peer.SetCacheStoreSize(limit);
        EXPECT_EQ(predictor_peer.cache_store_size_(), limit);
      }

      // Insert one entry per day.
      for (int i = 0; i < 50; ++i) {
        SegmentsProxy segments_proxy;
        const ConversionRequest convreq =
            SetUpInputForConversion(absl::StrFormat("わたしのなまえ%2d", i),
                                    &composer_, &segments_proxy);
        segments_proxy.AddCandidate(0, absl::StrFormat("私の名前%2d", i));
        predictor->Finish(convreq, segments_proxy.MakeLearningResults(),
                          kRevertId);
        if (mode == 0) {
          clock->Advance(absl::Hours(24));  // advance one day.
        }
      }

      predictor_peer.Save();

      auto lookup_key = [&](absl::string_view key) -> std::string {
        SegmentsProxy segments_proxy;
        const ConversionRequest convreq =
            SetUpInputForPrediction(key, &composer_, &segments_proxy);
        const std::vector<Result> results = predictor->Predict(convreq);
        return results.empty() ? "" : results[0].value;
      };

      const int deleted = 50 - limit;
      for (int i = 0; i < deleted; ++i) {
        EXPECT_EQ(lookup_key(absl::StrFormat("わたしのなまえ%2d", i)), "");
      }

      for (int i = deleted; i < limit; ++i) {
        EXPECT_EQ(lookup_key(absl::StrFormat("わたしのなまえ%2d", i)),
                  absl::StrFormat("私の名前%2d", i));
      }

      predictor_peer.SetEntryLifetimeDays(0);
      predictor_peer.SetCacheStoreSize(0);
      EXPECT_EQ(predictor_peer.entry_lifetime_days_(), 62);
      EXPECT_EQ(predictor_peer.cache_store_size_(), 0);
    }
  }
}

TEST_F(UserHistoryPredictorTest, 62DayOldEntriesAreDeletedAtSync) {
  ScopedClockMock clock(absl::FromUnixSeconds(1));

  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();
  UserHistoryPredictorTestPeer predictor_peer(*predictor);
  std::vector<Result> results;

  // Let the predictor learn "私の名前は中野です".
  SegmentsProxy segments_proxy;
  const ConversionRequest convreq1 = SetUpInputForConversion(
      "わたしのなまえはなかのです", &composer_, &segments_proxy);
  segments_proxy.AddCandidate(0, "私の名前は中野です");
  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), kRevertId);

  // Verify that "私の名前は中野です" is predicted.
  segments_proxy.Clear();
  const ConversionRequest convreq2 =
      SetUpInputForPrediction("わたしの", &composer_, &segments_proxy);
  results = predictor->Predict(convreq2);
  EXPECT_FALSE(results.empty());
  EXPECT_TRUE(FindCandidateByValue("私の名前は中野です", results));

  // Now, simulate the case where 63 days passed.
  clock->Advance(absl::Hours(63 * 24));

  // Let the predictor learn "私の名前は高橋です".
  segments_proxy.Clear();
  const ConversionRequest convreq3 = SetUpInputForConversion(
      "わたしのなまえはたかはしです", &composer_, &segments_proxy);
  segments_proxy.AddCandidate(0, "私の名前は高橋です");
  predictor->Finish(convreq3, segments_proxy.MakeLearningResults(), kRevertId);

  // Verify that "私の名前は高橋です" is predicted but "私の名前は中野です" is
  // not.  The latter one is still in on-memory data structure but lookup is
  // prevented.  The entry is removed when the data is written to disk.
  segments_proxy.Clear();
  const ConversionRequest convreq4 =
      SetUpInputForPrediction("わたしの", &composer_, &segments_proxy);
  results = predictor->Predict(convreq4);
  EXPECT_FALSE(results.empty());
  EXPECT_TRUE(FindCandidateByValue("私の名前は高橋です", results));
  EXPECT_FALSE(FindCandidateByValue("私の名前は中野です", results));

  // Here, write the history to a storage.
  ASSERT_TRUE(predictor->Sync());
  WaitForSyncer(predictor);

  // Verify that "私の名前は中野です" is no longer predicted because it was
  // learned 63 days before.
  segments_proxy.Clear();
  const ConversionRequest convreq5 =
      SetUpInputForPrediction("わたしの", &composer_, &segments_proxy);
  results = predictor->Predict(convreq5);
  EXPECT_FALSE(results.empty());
  EXPECT_TRUE(FindCandidateByValue("私の名前は高橋です", results));
  EXPECT_FALSE(FindCandidateByValue("私の名前は中野です", results));

  // Verify also that on-memory data structure doesn't contain node for 中野.
  bool found_takahashi = false;
  for (const auto& elem : *predictor_peer.dic_()) {
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

  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  // Let the predictor learn "私の名前は中野です".
  SegmentsProxy segments_proxy;
  std::vector<Result> results;
  const ConversionRequest convreq1 = SetUpInputForConversion(
      "わたしのなまえはなかのです", &composer_, &segments_proxy);
  segments_proxy.AddCandidate(0, "私の名前は中野です");
  predictor->Finish(convreq1, segments_proxy.MakeLearningResults(), kRevertId);

  // Verify that "私の名前は中野です" is predicted.
  segments_proxy.Clear();
  const ConversionRequest convreq2 =
      SetUpInputForPrediction("わたしの", &composer_, &segments_proxy);
  results = predictor->Predict(convreq2);
  EXPECT_FALSE(results.empty());
  EXPECT_TRUE(FindCandidateByValue("私の名前は中野です", results));

  // Now, go back to the past.
  clock->SetTime(absl::FromUnixSeconds(1));

  // Verify that "私の名前は中野です" is predicted without crash.
  segments_proxy.Clear();
  const ConversionRequest convreq3 =
      SetUpInputForPrediction("わたしの", &composer_, &segments_proxy);
  results = predictor->Predict(convreq3);
  EXPECT_FALSE(results.empty());
  EXPECT_TRUE(FindCandidateByValue("私の名前は中野です", results));
}

TEST_F(UserHistoryPredictorTest, MaxPredictionCandidatesSize) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();
  SegmentsProxy segments_proxy;
  std::vector<Result> results;
  {
    const ConversionRequest convreq =
        SetUpInputForPrediction("てすと", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "てすと");
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
  }
  {
    const ConversionRequest convreq =
        SetUpInputForPrediction("てすと", &composer_, &segments_proxy);

    segments_proxy.AddCandidate(0, "テスト");
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
  }
  {
    const ConversionRequest convreq =
        SetUpInputForPrediction("てすと", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "Test");
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
  }
  {
    ConversionRequest::Options options1 = {
        .request_type = ConversionRequest::SUGGESTION,
        .max_user_history_prediction_candidates_size = 2,
    };
    const ConversionRequest convreq1 = CreateConversionRequestWithOptions(
        composer_, std::move(options1), segments_proxy);
    segments_proxy.MakeSegments("てすと");

    results = predictor->Predict(convreq1);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results.size(), 2);

    ConversionRequest::Options options2 = {
        .request_type = ConversionRequest::PREDICTION,
        .max_user_history_prediction_candidates_size = 2,
    };
    const ConversionRequest convreq2 = CreateConversionRequestWithOptions(
        composer_, std::move(options2), segments_proxy);
    segments_proxy.MakeSegments("てすと");
    results = predictor->Predict(convreq2);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results.size(), 2);
  }
  {
    SetUpInput("てすと", &composer_, &segments_proxy);
    ConversionRequest::Options options1 = {
        .request_type = ConversionRequest::SUGGESTION,
        .max_user_history_prediction_candidates_size = 3,
    };
    const ConversionRequest convreq1 = CreateConversionRequestWithOptions(
        composer_, std::move(options1), segments_proxy);
    results = predictor->Predict(convreq1);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results.size(), 3);

    SetUpInput("てすと", &composer_, &segments_proxy);
    ConversionRequest::Options options2 = {
        .request_type = ConversionRequest::PREDICTION,
        .max_user_history_prediction_candidates_size = 3,
    };
    const ConversionRequest convreq2 = CreateConversionRequestWithOptions(
        composer_, std::move(options2), segments_proxy);
    results = predictor->Predict(convreq2);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results.size(), 3);
  }

  {
    // Only 3 candidates in user history
    SetUpInput("てすと", &composer_, &segments_proxy);
    ConversionRequest::Options options1 = {
        .request_type = ConversionRequest::SUGGESTION,
        .max_user_history_prediction_candidates_size = 4,
    };
    const ConversionRequest convreq1 = CreateConversionRequestWithOptions(
        composer_, std::move(options1), segments_proxy);
    results = predictor->Predict(convreq1);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results.size(), 3);

    SetUpInput("てすと", &composer_, &segments_proxy);
    ConversionRequest::Options options2 = {
        .request_type = ConversionRequest::PREDICTION,
        .max_user_history_prediction_candidates_size = 4,
    };
    const ConversionRequest convreq2 = CreateConversionRequestWithOptions(
        composer_, std::move(options2), segments_proxy);
    results = predictor->Predict(convreq2);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results.size(), 3);
  }
}

TEST_F(UserHistoryPredictorTest, MaxPredictionCandidatesSizeForZeroQuery) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();
  request_test_util::FillMobileRequest(&request_);
  SegmentsProxy segments_proxy;
  std::vector<Result> results;

  const ConversionRequest convreq =
      SetUpInputForPrediction("てすと", &composer_, &segments_proxy);
  {
    segments_proxy.AddCandidate(0, "てすと");
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
    segments_proxy.SetHistoryType(0);
  }
  {
    segments_proxy.AddSegment("かお");
    segments_proxy.AddCandidate(1, "😀");
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
  }
  {
    segments_proxy.SetCandidate(1, 0, "😎");
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
  }
  {
    segments_proxy.SetCandidate(1, 0, "😂");
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
  }

  // normal prediction candidates size
  {
    SetUpInput("かお", &composer_, &segments_proxy);
    ConversionRequest::Options options1 = {
        .request_type = ConversionRequest::SUGGESTION,
        .max_user_history_prediction_candidates_size = 2,
        .max_user_history_prediction_candidates_size_for_zero_query = 3,
    };
    const ConversionRequest convreq1 = CreateConversionRequestWithOptions(
        composer_, std::move(options1), segments_proxy);
    results = predictor->Predict(convreq1);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results.size(), 2);

    SetUpInput("かお", &composer_, &segments_proxy);
    ConversionRequest::Options options2 = {
        .request_type = ConversionRequest::PREDICTION,
        .max_user_history_prediction_candidates_size = 2,
        .max_user_history_prediction_candidates_size_for_zero_query = 3,
    };
    const ConversionRequest convreq2 = CreateConversionRequestWithOptions(
        composer_, std::move(options2), segments_proxy);
    results = predictor->Predict(convreq2);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results.size(), 2);
  }

  // prediction candidates for zero query
  {
    SetUpInput("", &composer_, &segments_proxy);
    segments_proxy.PrependHistory("てすと", "てすと");
    ConversionRequest::Options options1 = {
        .request_type = ConversionRequest::SUGGESTION,
        .max_user_history_prediction_candidates_size = 2,
        .max_user_history_prediction_candidates_size_for_zero_query = 3,
    };
    const ConversionRequest convreq1 = CreateConversionRequestWithOptions(
        composer_, std::move(options1), segments_proxy);
    results = predictor->Predict(convreq1);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results.size(), 3);

    SetUpInput("", &composer_, &segments_proxy);
    segments_proxy.PrependHistory("てすと", "てすと");
    ConversionRequest::Options options2 = {
        .request_type = ConversionRequest::PREDICTION,
        .max_user_history_prediction_candidates_size = 2,
        .max_user_history_prediction_candidates_size_for_zero_query = 3,
    };
    const ConversionRequest convreq2 = CreateConversionRequestWithOptions(
        composer_, std::move(options2), segments_proxy);
    results = predictor->Predict(convreq2);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results.size(), 3);
  }
}

TEST_F(UserHistoryPredictorTest, TypingCorrection) {
  auto mock = std::make_unique<engine::MockSupplementalModel>();
  // TODO(taku): Avoid sharing the pointer of std::unique_ptr.
  engine::MockSupplementalModel* mock_ptr = mock.get();

  std::unique_ptr<engine::Modules> modules =
      engine::ModulesPresetBuilder()
          .PresetDictionary(std::make_unique<MockDictionary>())
          .PresetSupplementalModel(std::move(mock))
          .Build(std::make_unique<testing::MockDataManager>())
          .value();
  auto predictor = std::make_unique<UserHistoryPredictor>(*modules);
  UserHistoryPredictorTestPeer(*predictor).WaitForSyncer();

  ScopedClockMock clock(absl::FromUnixSeconds(1));

  SegmentsProxy segments_proxy;
  std::vector<Result> results;

  {
    clock->Advance(absl::Hours(1));
    const ConversionRequest convreq =
        SetUpInputForPrediction("がっこう", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "学校");
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
  }

  {
    clock->Advance(absl::Hours(1));
    const ConversionRequest convreq =
        SetUpInputForPrediction("がっこう", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "ガッコウ");
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
  }

  {
    clock->Advance(absl::Hours(1));
    const ConversionRequest convreq =
        SetUpInputForPrediction("かっこう", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "格好");
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
  }

  request_.mutable_decoder_experiment_params()
      ->set_typing_correction_apply_user_history_size(1);

  const ConversionRequest convreq1 =
      SetUpInputForSuggestion("がっこ", &composer_, &segments_proxy);
  results = predictor->Predict(convreq1);
  EXPECT_FALSE(results.empty());

  // No typing correction.
  const ConversionRequest convreq2 =
      SetUpInputForSuggestion("かつこ", &composer_, &segments_proxy);
  results = predictor->Predict(convreq2);
  EXPECT_TRUE(results.empty());

  std::vector<TypeCorrectedQuery> expected;
  auto add_expected = [&](const std::string& key) {
    expected.emplace_back(
        TypeCorrectedQuery{key, TypeCorrectedQuery::CORRECTION, 1.0});
  };

  // かつこ -> がっこ and かっこ
  add_expected("がっこ");
  add_expected("かっこ");
  EXPECT_CALL(*mock_ptr, CorrectComposition(_))
      .WillRepeatedly(Return(expected));

  // set_typing_correction_apply_user_history_size=0
  request_.mutable_decoder_experiment_params()
      ->set_typing_correction_apply_user_history_size(0);
  const ConversionRequest convreq3 =
      SetUpInputForSuggestion("かつこ", &composer_, &segments_proxy);
  results = predictor->Predict(convreq3);
  EXPECT_TRUE(results.empty());

  // set_typing_correction_apply_user_history_size=1
  request_.mutable_decoder_experiment_params()
      ->set_typing_correction_apply_user_history_size(1);
  const ConversionRequest convreq4 =
      SetUpInputForSuggestion("かつこ", &composer_, &segments_proxy);
  results = predictor->Predict(convreq4);
  EXPECT_FALSE(results.empty());
  ASSERT_EQ(results.size(), 2);
  EXPECT_EQ(results[0].value, "ガッコウ");
  EXPECT_EQ(results[1].value, "学校");

  // set_typing_correction_apply_user_history_size=2
  request_.mutable_decoder_experiment_params()
      ->set_typing_correction_apply_user_history_size(2);
  const ConversionRequest convreq5 =
      SetUpInputForSuggestion("かつこ", &composer_, &segments_proxy);
  results = predictor->Predict(convreq5);
  EXPECT_FALSE(results.empty());
  ASSERT_EQ(results.size(), 3);
  EXPECT_EQ(results[0].value, "格好");
  EXPECT_EQ(results[1].value, "ガッコウ");
  EXPECT_EQ(results[2].value, "学校");

  ::testing::Mock::VerifyAndClearExpectations(mock_ptr);
  const ConversionRequest convreq6 =
      SetUpInputForSuggestion("かつこ", &composer_, &segments_proxy);
  results = predictor->Predict(convreq6);
  EXPECT_TRUE(results.empty());
}

TEST_F(UserHistoryPredictorTest, MaxCharCoverage) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();
  SegmentsProxy segments_proxy;

  {
    const ConversionRequest convreq1 =
        SetUpInputForPrediction("てすと", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "てすと");
    predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                      kRevertId);
  }
  {
    const ConversionRequest convreq2 =
        SetUpInputForPrediction("てすと", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "テスト");
    predictor->Finish(convreq2, segments_proxy.MakeLearningResults(),
                      kRevertId);
  }
  {
    const ConversionRequest convreq3 =
        SetUpInputForPrediction("てすと", &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, "Test");
    predictor->Finish(convreq3, segments_proxy.MakeLearningResults(),
                      kRevertId);
  }

  // [max_char_coverage, expected_candidate_size]
  const std::vector<std::pair<int, int>> kTestCases = {
      {1, 1}, {2, 1}, {3, 1}, {4, 1},  {5, 1}, {6, 2},
      {7, 2}, {8, 2}, {9, 2}, {10, 3}, {11, 3}};

  for (const auto& [coverage, candidates_size] : kTestCases) {
    request_.mutable_decoder_experiment_params()
        ->set_user_history_prediction_max_char_coverage(coverage);
    segments_proxy.MakeSegments("てすと");
    ConversionRequest::Options options = {.request_type =
                                              ConversionRequest::SUGGESTION};
    const ConversionRequest convreq = CreateConversionRequestWithOptions(
        composer_, std::move(options), segments_proxy);

    const std::vector<Result> results = predictor->Predict(convreq);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results.size(), candidates_size);
  }
}

TEST_F(UserHistoryPredictorTest, RemoveRedundantCandidates) {
  // pass the input candidates and expected (filtered) candidates.
  auto run_test = [this](absl::Span<const absl::string_view> candidates,
                         absl::Span<const absl::string_view> expected) {
    ScopedClockMock clock(absl::FromUnixSeconds(1));
    UserHistoryPredictor* predictor =
        GetUserHistoryPredictorWithClearedHistory();
    SegmentsProxy segments_proxy;
    // Insert in reverse order to emulate LRU.
    for (auto it = candidates.rbegin(); it != candidates.rend(); ++it) {
      clock->Advance(absl::Hours(1));
      const ConversionRequest convreq =
          SetUpInputForPrediction("とうき", &composer_, &segments_proxy);
      segments_proxy.AddCandidate(0, *it);
      predictor->Finish(convreq, segments_proxy.MakeLearningResults(),
                        kRevertId);
    }
    segments_proxy.MakeSegments("とうき");
    ConversionRequest::Options options = {
        .request_type = ConversionRequest::SUGGESTION,
        .max_user_history_prediction_candidates_size = 10,
    };
    const ConversionRequest convreq = CreateConversionRequestWithOptions(
        composer_, std::move(options), segments_proxy);
    const std::vector<Result> results = predictor->Predict(convreq);
    EXPECT_FALSE(results.empty());
    ASSERT_EQ(expected.size(), results.size());
    for (int i = 0; i < expected.size(); ++i) {
      EXPECT_EQ(expected[i], results[i].value);
    }
  };

  // filter long and replace short entries.
  run_test({"東京は", "東京", "大阪", "大阪は"}, {"東京", "大阪"});
  run_test({"東京", "東京は", "大阪は", "大阪"}, {"東京", "大阪"});
  run_test({"東京駅", "東京", "大阪", "大阪駅"},
           {"東京駅", "東京", "大阪", "大阪駅"});
  run_test({"東京", "東京駅", "大阪駅", "大阪"},
           {"東京", "東京駅", "大阪駅", "大阪"});
  run_test({"東京は", "東京", "大阪", "大阪駅"}, {"東京", "大阪", "大阪駅"});
  run_test({"東京", "東京は", "大阪駅", "大阪"}, {"東京", "大阪駅", "大阪"});
}

TEST_F(UserHistoryPredictorTest, ContentValueZeroQuery) {
  UserHistoryPredictor* predictor = GetUserHistoryPredictorWithClearedHistory();

  // Remember 私の名前は中野です
  SegmentsProxy segments_proxy;
  {
    constexpr absl::string_view kKey = "わたしのなまえはなかのです";
    constexpr absl::string_view kValue = "私の名前は中野です";
    const ConversionRequest convreq =
        SetUpInputForPrediction(kKey, &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, kValue);
    // "わたしの, 私の", "わたし, 私"
    segments_proxy.PushBackInnerSegmentBoundary(0, 0, 12, 6, 9, 3);
    // "なまえは, 名前は", "なまえ, 名前"
    segments_proxy.PushBackInnerSegmentBoundary(0, 0, 12, 9, 9, 6);
    // "なかのです, 中野です", "なかの, 中野"
    segments_proxy.PushBackInnerSegmentBoundary(0, 0, 15, 12, 9, 6);
    predictor->Finish(convreq, segments_proxy.MakeLearningResults(), kRevertId);
  }

  // Zero query from content values. suffix is suggested.
  const std::vector<
      std::tuple<absl::string_view, absl::string_view, absl::string_view>>
      kZeroQueryTest = {{"わたし", "私", "の"},
                        {"なまえ", "名前", "は"},
                        {"なかの", "中野", "です"},
                        {"わたしの", "私の", "名前"},
                        {"なまえは", "名前は", "中野"}};
  for (const auto& [hist_key, hist_value, suggestion] : kZeroQueryTest) {
    segments_proxy.Clear();
    const ConversionRequest convreq1 =
        SetUpInputForConversion(hist_key, &composer_, &segments_proxy);
    segments_proxy.AddCandidate(0, hist_value);
    predictor->Finish(convreq1, segments_proxy.MakeLearningResults(),
                      kRevertId);
    segments_proxy.SetHistoryType(0);
    request_.set_zero_query_suggestion(true);
    const ConversionRequest convreq2 = SetUpInputForSuggestionWithHistory(
        "", hist_key, hist_value, &composer_, &segments_proxy);
    const std::vector<Result> results = predictor->Predict(convreq2);
    ASSERT_FALSE(results.empty());
  }

  // Bigram History.
  {
    segments_proxy.Clear();
    std::vector<Result> results;
    const ConversionRequest convreq1 =
        SetUpInputForSuggestion("", &composer_, &segments_proxy);
    segments_proxy.PrependHistory("の", "の");
    segments_proxy.PrependHistory("わたし", "私");
    request_.set_zero_query_suggestion(true);
    results = predictor->Predict(convreq1);
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, "名前");

    segments_proxy.Clear();
    const ConversionRequest convreq2 =
        SetUpInputForSuggestion("", &composer_, &segments_proxy);
    segments_proxy.PrependHistory("は", "は");
    segments_proxy.PrependHistory("なまえ", "名前");
    request_.set_zero_query_suggestion(true);
    results = predictor->Predict(convreq2);
    ASSERT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, "中野");
  }
}

TEST_F(UserHistoryPredictorTest, GuessRevertedValueOffset) {
  auto run_test = [](absl::string_view reverted_value,
                     absl::string_view left_context) {
    return UserHistoryPredictorTestPeer::GuessRevertedValueOffset(
        reverted_value, left_context);
  };

  // Invalid input.
  EXPECT_EQ(run_test("abc", ""), 0);
  EXPECT_EQ(run_test("", "abc"), 0);
  EXPECT_EQ(run_test("", ""), 0);

  EXPECT_EQ(run_test("xyz", ""), 0);
  EXPECT_EQ(run_test("xyz", "x"), 1);
  EXPECT_EQ(run_test("xyz", "xy"), 2);
  EXPECT_EQ(run_test("xyz", "xyz"), 3);
  EXPECT_EQ(run_test("xyz", "xyzX"), 0);

  // Real Japanese test set.
  EXPECT_EQ(run_test("渋谷駅", "停車駅は渋谷駅"), 9);
  EXPECT_EQ(run_test("渋谷駅", "停車駅は渋谷"), 6);
  EXPECT_EQ(run_test("渋谷駅", "停車駅は渋"), 3);
  EXPECT_EQ(run_test("渋谷駅", "停車駅は"), 0);
  EXPECT_EQ(run_test("渋谷駅", "停車駅"), 0);
}

TEST_F(UserHistoryPredictorTest, PartialRevert) {
  std::vector<Result> results;

  auto mock = std::make_unique<engine::MockSupplementalModel>();

  // Emulates reading aligner.
  EXPECT_CALL(*mock, GetReadingAlignment(_, _))
      .WillRepeatedly(
          Invoke([](absl::string_view surface, absl::string_view reading) {
            // splits 京都大学 -> 京都|大学
            const auto pos1 = surface.find("大学");
            const auto pos2 = reading.find("だいがく");
            std::vector<std::pair<absl::string_view, absl::string_view>> v;
            v.emplace_back(surface.substr(0, pos1), reading.substr(0, pos2));
            v.emplace_back(surface.substr(pos1), reading.substr(pos2));
            return v;
          }));

  std::unique_ptr<engine::Modules> modules =
      engine::ModulesPresetBuilder()
          .PresetDictionary(std::make_unique<MockDictionary>())
          .PresetSupplementalModel(std::move(mock))
          .Build(std::make_unique<testing::MockDataManager>())
          .value();
  auto predictor = std::make_unique<UserHistoryPredictor>(*modules);

  auto has_entry = [&](absl::string_view key, absl::string_view value) {
    UserHistoryPredictorTestPeer predictor_peer(*predictor);
    const UserHistoryPredictor::Entry* entry =
        predictor_peer.dic_()->LookupWithoutInsert(
            UserHistoryPredictor::Fingerprint(key, value));
    return entry && entry->suggestion_freq() > 0 &&
           entry->last_access_time() > 0;
  };

  auto init_predictor = [&]() {
    predictor->ClearAllHistory();
    UserHistoryPredictorTestPeer(*predictor).WaitForSyncer();

    Result result;

    auto add_segment = [&](absl::string_view key, absl::string_view value,
                           absl::string_view content_key,
                           absl::string_view content_value) {
      ASSERT_TRUE(absl::StartsWith(key, content_key));
      ASSERT_TRUE(absl::StartsWith(value, content_value));
      absl::StrAppend(&result.key, key);
      absl::StrAppend(&result.value, value);
      result.inner_segment_boundary.emplace_back(
          converter::EncodeLengths(key.size(), value.size(), content_key.size(),
                                   content_value.size())
              .value());
    };

    // 佐藤さんは京都大学を卒業した
    add_segment("さとうさんは", "佐藤さんは", "さとうさん", "佐藤さん");
    add_segment("きょうとだいがくを", "京都大学を", "きょうとだいがく",
                "京都大学");
    // value == content_value.
    add_segment("そつぎょうした", "卒業した", "そつぎょうした", "卒業した");

    // Make a placeholder request for Finish().
    SegmentsProxy segments_proxy;
    request_.set_mixed_conversion(true);
    const ConversionRequest convreq =
        SetUpInputForSuggestion("さとう", &composer_, &segments_proxy);

    predictor->Finish(convreq, {result}, kRevertId);

    // New entries are inserted.
    EXPECT_TRUE(has_entry("さとうさんはきょうとだいがくをそつぎょうした",
                          "佐藤さんは京都大学を卒業した"));
    EXPECT_TRUE(has_entry("さとうさんは", "佐藤さんは"));
    EXPECT_TRUE(has_entry("さとうさん", "佐藤さん"));
    EXPECT_TRUE(has_entry("きょうとだいがくを", "京都大学を"));
    EXPECT_TRUE(has_entry("きょうとだいがく", "京都大学"));
    EXPECT_TRUE(has_entry("そつぎょうした", "卒業した"));

    predictor->Revert(kRevertId);

    // All entries are removed with Revret.
    EXPECT_FALSE(has_entry("さとうさんはきょうとだいがくをそつぎょうした",
                           "佐藤さんは京都大学を卒業した"));
    EXPECT_FALSE(has_entry("さとうさんは", "佐藤さんは"));
    EXPECT_FALSE(has_entry("さとうさん", "佐藤さん"));
    EXPECT_FALSE(has_entry("きょうとだいがくを", "京都大学を"));
    EXPECT_FALSE(has_entry("きょうとだいがく", "京都大学"));
    EXPECT_FALSE(has_entry("そつぎょうした", "卒業した"));
  };

  // Continue to type with the key and left_context.
  auto suggest_with_context = [&](absl::string_view key,
                                  absl::string_view left_context) {
    context_.set_preceding_text(left_context);
    request_.mutable_decoder_experiment_params()
        ->set_user_history_partial_revert_mode(1);
    SegmentsProxy segments_proxy;
    const ConversionRequest convreq =
        SetUpInputForSuggestion(key, &composer_, &segments_proxy);
    return predictor->Predict(convreq);
  };

  {
    init_predictor();

    results = suggest_with_context("さとう", "佐藤さんは京都大学を");
    EXPECT_FALSE(results.empty());
    EXPECT_TRUE(absl::StartsWith(results[0].value, "佐藤さん"));

    EXPECT_TRUE(
        has_entry("さとうさんはきょうとだいがくを", "佐藤さんは京都大学を"));
    EXPECT_TRUE(has_entry("さとうさんは", "佐藤さんは"));
    EXPECT_TRUE(has_entry("さとうさん", "佐藤さん"));
    EXPECT_TRUE(has_entry("きょうとだいがくを", "京都大学を"));
    EXPECT_TRUE(has_entry("きょうとだいがく", "京都大学"));
    EXPECT_FALSE(has_entry("そつぎょうした", "卒業した"));
  }

  {
    init_predictor();

    results = suggest_with_context("さとう", "佐藤さんは京都大学");
    EXPECT_FALSE(results.empty());

    EXPECT_TRUE(
        has_entry("さとうさんはきょうとだいがく", "佐藤さんは京都大学"));
    EXPECT_TRUE(has_entry("さとうさんは", "佐藤さんは"));
    EXPECT_TRUE(has_entry("さとうさん", "佐藤さん"));
    EXPECT_FALSE(has_entry("きょうとだいがくを", "京都大学を"));
    EXPECT_TRUE(has_entry("きょうとだいがく", "京都大学"));
    EXPECT_FALSE(has_entry("そつぎょうした", "卒業した"));
  }

  {
    init_predictor();

    // middle of 京都大学 -> split by reading aligner.
    results = suggest_with_context("さとう", "佐藤さんは京都");
    EXPECT_FALSE(results.empty());

    EXPECT_TRUE(has_entry("さとうさんはきょうと", "佐藤さんは京都"));
    EXPECT_TRUE(has_entry("さとうさんは", "佐藤さんは"));
    EXPECT_TRUE(has_entry("さとうさん", "佐藤さん"));
    EXPECT_TRUE(has_entry("きょうと", "京都"));
    EXPECT_FALSE(has_entry("きょうとだいがくを", "京都大学を"));
    EXPECT_FALSE(has_entry("きょうとだいがく", "京都大学"));
    EXPECT_FALSE(has_entry("そつぎょうした", "卒業した"));
  }

  {
    init_predictor();

    // Middle of 卒業した -> safely remove "した" as it is HIRAGANA.
    results = suggest_with_context("さとう", "佐藤さんは京都大学を卒業");
    EXPECT_FALSE(results.empty());

    EXPECT_TRUE(has_entry("さとうさんはきょうとだいがくをそつぎょう",
                          "佐藤さんは京都大学を卒業"));
    EXPECT_TRUE(has_entry("さとうさんは", "佐藤さんは"));
    EXPECT_TRUE(has_entry("さとうさん", "佐藤さん"));
    EXPECT_TRUE(has_entry("きょうとだいがくを", "京都大学を"));
    EXPECT_TRUE(has_entry("きょうとだいがく", "京都大学"));
    EXPECT_FALSE(has_entry("そつぎょうした", "卒業した"));
    EXPECT_TRUE(has_entry("そつぎょう", "卒業"));  // Newly added.
  }

  {
    init_predictor();
    results = suggest_with_context("さとう", "佐藤さんは京都大学を卒業し");
    EXPECT_FALSE(results.empty());

    EXPECT_TRUE(has_entry("さとうさんはきょうとだいがくをそつぎょうし",
                          "佐藤さんは京都大学を卒業し"));
    EXPECT_TRUE(has_entry("さとうさんは", "佐藤さんは"));
    EXPECT_TRUE(has_entry("さとうさん", "佐藤さん"));
    EXPECT_TRUE(has_entry("きょうとだいがくを", "京都大学を"));
    EXPECT_TRUE(has_entry("きょうとだいがく", "京都大学"));
    EXPECT_FALSE(has_entry("そつぎょうした", "卒業した"));
    EXPECT_TRUE(has_entry("そつぎょうし", "卒業し"));  // Newly added.
  }

  {
    init_predictor();
    results = suggest_with_context("さとう", "佐藤さん");
    EXPECT_FALSE(results.empty());

    EXPECT_FALSE(has_entry("さとうさんは", "佐藤さんは"));
    EXPECT_TRUE(has_entry("さとうさん", "佐藤さん"));
    EXPECT_FALSE(has_entry("きょうとだいがくを", "京都大学を"));
    EXPECT_FALSE(has_entry("きょうとだいがく", "京都大学"));
    EXPECT_FALSE(has_entry("そつぎょうした", "卒業した"));
  }

  {
    init_predictor();
    results = suggest_with_context("さとう", "佐藤");
    EXPECT_FALSE(results.empty());

    EXPECT_TRUE(has_entry("さとう", "佐藤"));
    EXPECT_FALSE(has_entry("さとうさんは", "佐藤さんは"));
    EXPECT_FALSE(has_entry("さとうさん", "佐藤さん"));
    EXPECT_FALSE(has_entry("きょうとだいがくを", "京都大学を"));
    EXPECT_FALSE(has_entry("きょうとだいがく", "京都大学"));
    EXPECT_FALSE(has_entry("そつぎょうした", "卒業した"));
  }

  {
    init_predictor();
    results = suggest_with_context("さとう", "");
    EXPECT_TRUE(results.empty());

    EXPECT_FALSE(has_entry("さとうさんは", "佐藤さんは"));
    EXPECT_FALSE(has_entry("さとうさん", "佐藤さん"));
    EXPECT_FALSE(has_entry("きょうとだいがくを", "京都大学を"));
    EXPECT_FALSE(has_entry("きょうとだいがく", "京都大学"));
    EXPECT_FALSE(has_entry("そつぎょうした", "卒業した"));
  }

  {
    init_predictor();
    results = suggest_with_context("さとう", "関係ない文脈");
    EXPECT_TRUE(results.empty());

    EXPECT_FALSE(has_entry("さとうさんは", "佐藤さんは"));
    EXPECT_FALSE(has_entry("さとうさん", "佐藤さん"));
    EXPECT_FALSE(has_entry("きょうとだいがくを", "京都大学を"));
    EXPECT_FALSE(has_entry("きょうとだいがく", "京都大学"));
    EXPECT_FALSE(has_entry("そつぎょうした", "卒業した"));
  }

  // Adds extra characters directly w/o using IME.
  {
    init_predictor();

    results = suggest_with_context("さとう", "佐藤さんは京都大学を \n\r　");
    EXPECT_FALSE(results.empty());
    EXPECT_TRUE(absl::StartsWith(results[0].value, "佐藤さん"));

    EXPECT_TRUE(
        has_entry("さとうさんはきょうとだいがくを", "佐藤さんは京都大学を"));
    EXPECT_TRUE(has_entry("さとうさんは", "佐藤さんは"));
    EXPECT_TRUE(has_entry("さとうさん", "佐藤さん"));
    EXPECT_TRUE(has_entry("きょうとだいがくを", "京都大学を"));
    EXPECT_TRUE(has_entry("きょうとだいがく", "京都大学"));
    EXPECT_FALSE(has_entry("そつぎょうした", "卒業した"));
  }
}

}  // namespace mozc::prediction
