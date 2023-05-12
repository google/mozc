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

#include "prediction/dictionary_prediction_aggregator.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/japanese_util.h"
#include "base/logging.h"
#include "base/number_util.h"
#include "base/util.h"
#include "composer/composer.h"
#include "converter/converter_interface.h"
#include "converter/immutable_converter_interface.h"
#include "converter/node_list_builder.h"
#include "converter/segments.h"
#include "data_manager/data_manager_interface.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "prediction/number_decoder.h"
#include "prediction/single_kanji_prediction_aggregator.h"
#include "prediction/zero_query_dict.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "transliteration/transliteration.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

#ifndef NDEBUG
#define MOZC_DEBUG
#define MOZC_WORD_LOG_MESSAGE(message) \
  absl::StrCat(__FILE__, ":", __LINE__, " ", message, "\n")
#define MOZC_WORD_LOG(result, message) \
  (result).log.append(MOZC_WORD_LOG_MESSAGE(message))

#else  // NDEBUG
#define MOZC_WORD_LOG(result, message) \
  {}

#endif  // NDEBUG

namespace mozc {
namespace prediction {

namespace {

using ::mozc::commands::Request;
using ::mozc::dictionary::DictionaryInterface;
using ::mozc::dictionary::Token;

// Note that PREDICTION mode is much slower than SUGGESTION.
// Number of prediction calls should be minimized.
constexpr size_t kSuggestionMaxResultsSize = 256;
constexpr size_t kPredictionMaxResultsSize = 100000;

bool IsEnableSingleKanjiPrediction(const ConversionRequest &request) {
  return request.request()
      .decoder_experiment_params()
      .enable_single_kanji_prediction();
}

// Returns true if the |target| may be reduncant result.
bool MaybeRedundant(const absl::string_view reference,
                    const absl::string_view target) {
  return absl::StartsWith(target, reference);
}

bool IsLatinInputMode(const ConversionRequest &request) {
  return (request.has_composer() &&
          (request.composer().GetInputMode() == transliteration::HALF_ASCII ||
           request.composer().GetInputMode() == transliteration::FULL_ASCII));
}

bool IsQwertyMobileTable(const ConversionRequest &request) {
  const auto table = request.request().special_romanji_table();
  return (table == Request::QWERTY_MOBILE_TO_HIRAGANA ||
          table == Request::QWERTY_MOBILE_TO_HALFWIDTHASCII);
}

bool IsLanguageAwareInputEnabled(const ConversionRequest &request) {
  const auto lang_aware = request.request().language_aware_input();
  return lang_aware == Request::LANGUAGE_AWARE_SUGGESTION;
}

// Returns true if |segments| contains number history.
// Normalized number will be set to |number_key|
// Note:
//  Now this function supports arabic number candidates only and
//  we don't support kanji number candidates for now.
//  This is because We have several kanji number styles, for example,
//  "一二", "十二", "壱拾弐", etc for 12.
// TODO(toshiyuki): Define the spec and support Kanji.
bool GetNumberHistory(const Segments &segments, std::string *number_key) {
  DCHECK(number_key);
  const size_t history_size = segments.history_segments_size();
  if (history_size <= 0) {
    return false;
  }

  const Segment &last_segment = segments.history_segment(history_size - 1);
  DCHECK_GT(last_segment.candidates_size(), 0);
  const std::string &history_value = last_segment.candidate(0).value;
  if (!NumberUtil::IsArabicNumber(history_value)) {
    return false;
  }

  japanese_util::FullWidthToHalfWidth(history_value, number_key);
  return true;
}

bool IsMixedConversionEnabled(const Request &request) {
  return request.mixed_conversion();
}

bool IsTypingCorrectionEnabled(const ConversionRequest &request) {
  return request.config().use_typing_correction();
}

bool HasHistoryKeyLongerThanOrEqualTo(const Segments &segments,
                                      size_t utf8_len) {
  const size_t history_segments_size = segments.history_segments_size();
  if (history_segments_size == 0) {
    return false;
  }
  const Segment &history_segment =
      segments.history_segment(history_segments_size - 1);
  if (history_segment.candidates_size() == 0) {
    return false;
  }
  return Util::CharsLen(history_segment.candidate(0).key) >= utf8_len;
}

bool IsLongKeyForRealtimeCandidates(const Segments &segments) {
  constexpr int kFewResultThreshold = 8;
  return (segments.segments_size() > 0 &&
          Util::CharsLen(segments.segment(0).key()) >= kFewResultThreshold);
}

size_t GetMaxSizeForRealtimeCandidates(const ConversionRequest &request,
                                       const Segments &segments,
                                       bool is_long_key) {
  const auto &segment = segments.conversion_segment(0);
  const size_t size = (request.max_dictionary_prediction_candidates_size() -
                       segment.candidates_size());
  return is_long_key ? std::min<size_t>(size, 8) : size;
}

size_t GetDefaultSizeForRealtimeCandidates(bool is_long_key) {
  return is_long_key ? 5 : 10;
}

ConversionRequest GetConversionRequestForRealtimeCandidates(
    const ConversionRequest &request, size_t realtime_candidates_size) {
  ConversionRequest ret = request;
  ret.set_max_conversion_candidates_size(realtime_candidates_size);
  return ret;
}

Segments GetSegmentsForRealtimeCandidatesGeneration(
    const Segments &original_segments) {
  Segments segments = original_segments;
  // Other predictors (i.e. user_history_predictor) can add candidates
  // before this predictor.
  segments.mutable_conversion_segment(0)->clear_candidates();
  return segments;
}

bool GetHistoryKeyAndValue(const Segments &segments, std::string *key,
                           std::string *value) {
  DCHECK(key);
  DCHECK(value);
  if (segments.history_segments_size() == 0) {
    return false;
  }

  const Segment &history_segment =
      segments.history_segment(segments.history_segments_size() - 1);
  if (history_segment.candidates_size() == 0) {
    return false;
  }

  key->assign(history_segment.candidate(0).key);
  value->assign(history_segment.candidate(0).value);
  return true;
}

}  // namespace

class DictionaryPredictionAggregator::PredictiveLookupCallback
    : public DictionaryInterface::Callback {
 public:
  PredictiveLookupCallback(PredictionTypes types, size_t limit,
                           size_t original_key_len,
                           const std::set<std::string> *subsequent_chars,
                           Segment::Candidate::SourceInfo source_info,
                           int zip_code_id, int unknown_id,
                           absl::string_view non_expanded_original_key,
                           const SpatialCostParams &spatial_cost_params,
                           std::vector<Result> *results)
      : penalty_(0),
        types_(types),
        limit_(limit),
        original_key_len_(original_key_len),
        subsequent_chars_(subsequent_chars),
        source_info_(source_info),
        zip_code_id_(zip_code_id),
        unknown_id_(unknown_id),
        non_expanded_original_key_(non_expanded_original_key),
        spatial_cost_params_(spatial_cost_params),
        results_(results) {}

  PredictiveLookupCallback(const PredictiveLookupCallback &) = delete;
  PredictiveLookupCallback &operator=(const PredictiveLookupCallback &) =
      delete;

  ResultType OnKey(absl::string_view key) override {
    if (subsequent_chars_ == nullptr) {
      return TRAVERSE_CONTINUE;
    }
    // If |subsequent_chars_| was provided, check if the substring of |key|
    // obtained by removing the original lookup key starts with a string in the
    // set.  For example, if original key is "he" and "hello" was found,
    // continue traversing only when one of "l", "ll", or "llo" is in
    // |subsequent_chars_|.
    // Implementation note: Although absl::StartsWith is called at most N times
    // where N = subsequent_chars_.size(), N is very small in practice, less
    // than 10.  Thus, this linear order algorithm is fast enough.
    // Theoretically, we can construct a trie of strings in |subsequent_chars_|
    // to get more performance but it's overkill here.
    // TODO(noriyukit): std::vector<string> would be better than set<string>.
    // To this end, we need to fix Comopser as well.
    const absl::string_view rest = absl::ClippedSubstr(key, original_key_len_);
    for (const std::string &chr : *subsequent_chars_) {
      if (absl::StartsWith(rest, chr)) {
        return TRAVERSE_CONTINUE;
      }
    }
    return TRAVERSE_NEXT_KEY;
  }

  ResultType OnActualKey(absl::string_view key, absl::string_view actual_key,
                         int num_expanded) override {
    penalty_ = 0;
    if (num_expanded > 0 ||
        (spatial_cost_params_.enable_new_spatial_scoring &&
         !non_expanded_original_key_.empty() &&
         !absl::StartsWith(actual_key, non_expanded_original_key_))) {
      penalty_ = spatial_cost_params_.GetPenalty(key);
    }
    return TRAVERSE_CONTINUE;
  }

  ResultType OnToken(absl::string_view key, absl::string_view actual_key,
                     const Token &token) override {
    // If the token is from user dictionary and its POS is unknown, it is
    // suggest-only words.  Such words are looked up only when their keys
    // exactly match |key|.  Otherwise, unigram suggestion can be annoying.  For
    // example, suppose a user registers their email address as める.  Then,
    // we don't want to show the email address from め but exactly from める.
    //
    // We also want to show ZIP_CODE entries only for the exact input key.
    if (((token.attributes & Token::USER_DICTIONARY) != 0 &&
         token.lid == unknown_id_) ||
        token.lid == zip_code_id_) {
      const auto orig_key = absl::ClippedSubstr(key, 0, original_key_len_);
      if (token.key != orig_key) {
        return TRAVERSE_CONTINUE;
      }
    }
    if (IsNoisyNumberToken(key, token)) {
      return TRAVERSE_CONTINUE;
    }

    results_->push_back(Result());
    results_->back().InitializeByTokenAndTypes(token, types_);
    results_->back().wcost += penalty_;
    results_->back().source_info |= source_info_;
    results_->back().non_expanded_original_key =
        std::string(non_expanded_original_key_);
    return (results_->size() < limit_) ? TRAVERSE_CONTINUE : TRAVERSE_DONE;
  }

 protected:
  int32_t penalty_;
  const PredictionTypes types_;
  const size_t limit_;
  const size_t original_key_len_;
  const std::set<std::string> *subsequent_chars_;
  const Segment::Candidate::SourceInfo source_info_;
  const int zip_code_id_;
  const int unknown_id_;
  absl::string_view non_expanded_original_key_;
  const SpatialCostParams spatial_cost_params_;
  std::vector<Result> *results_;

 private:
  // When the key is number, number token will be noisy if
  // - the key predicts number ("十月[10がつ]" for the key, "1")
  // - the value predicts number ("12時" for the key, "1")
  // - the value contains long suffix ("101匹わんちゃん" for the key, "101")
  bool IsNoisyNumberToken(absl::string_view key, const Token &token) const {
    const auto orig_key = absl::ClippedSubstr(key, 0, original_key_len_);
    if (!NumberUtil::IsArabicNumber(orig_key)) {
      return false;
    }
    const absl::string_view key_suffix(token.key.data() + orig_key.size(),
                                       token.key.size() - orig_key.size());
    if (key_suffix.empty()) {
      return false;
    }
    if (Util::GetFirstScriptType(key_suffix) == Util::NUMBER) {
      // If the key is "1", the token "10がつ" is noisy because the suffix
      // starts from a number. i.e. "0がつ".
      return true;
    }

    if (!absl::StartsWith(token.value, orig_key)) {
      return false;
    }

    const absl::string_view value_suffix(token.value.data() + orig_key.size(),
                                         token.value.size() - orig_key.size());
    if (value_suffix.empty()) {
      return false;
    }
    if (Util::GetFirstScriptType(value_suffix) == Util::NUMBER) {
      return true;
    }
    return Util::CharsLen(value_suffix) >= 3;
  }
};

class DictionaryPredictionAggregator::PredictiveBigramLookupCallback
    : public PredictiveLookupCallback {
 public:
  PredictiveBigramLookupCallback(PredictionTypes types, size_t limit,
                                 size_t original_key_len,
                                 const std::set<std::string> *subsequent_chars,
                                 absl::string_view history_value,
                                 Segment::Candidate::SourceInfo source_info,
                                 int zip_code_id, int unknown_id,
                                 absl::string_view non_expanded_original_key,
                                 const SpatialCostParams spatial_cost_params,
                                 std::vector<Result> *results)
      : PredictiveLookupCallback(types, limit, original_key_len,
                                 subsequent_chars, source_info, zip_code_id,
                                 unknown_id, non_expanded_original_key,
                                 spatial_cost_params, results),
        history_value_(history_value) {}

  PredictiveBigramLookupCallback(const PredictiveBigramLookupCallback &) =
      delete;
  PredictiveBigramLookupCallback &operator=(
      const PredictiveBigramLookupCallback &) = delete;

  ResultType OnToken(absl::string_view key, absl::string_view expanded_key,
                     const Token &token) override {
    // Skip the token if its value doesn't start with the previous user input,
    // |history_value_|.
    if (!absl::StartsWith(token.value, history_value_) ||
        token.value.size() <= history_value_.size()) {
      return TRAVERSE_CONTINUE;
    }
    ResultType result_type =
        PredictiveLookupCallback::OnToken(key, expanded_key, token);
    return result_type;
  }

 private:
  absl::string_view history_value_;
};

class DictionaryPredictionAggregator::PrefixLookupCallback
    : public DictionaryInterface::Callback {
 public:
  PrefixLookupCallback(size_t limit, int kanji_number_id, int unknown_id,
                       int min_value_chars_len, std::vector<Result> *results)
      : limit_(limit),
        kanji_number_id_(kanji_number_id),
        unknown_id_(unknown_id),
        min_value_chars_len_(min_value_chars_len),
        results_(results) {}

  PrefixLookupCallback(const PrefixLookupCallback &) = delete;
  PrefixLookupCallback &operator=(const PrefixLookupCallback &) = delete;

  ResultType OnToken(absl::string_view key, absl::string_view actual_key,
                     const Token &token) override {
    if ((token.attributes & Token::USER_DICTIONARY) != 0 &&
        token.lid == unknown_id_) {
      // No suggest-only words as prefix candidates
      return TRAVERSE_CONTINUE;
    }
    // Avoid noisy script type nodes.
    if (token.lid == kanji_number_id_ && token.rid == kanji_number_id_) {
      // Kanji number entry can be looked up with the special reading and will
      // be expanded for the number variants, so we want to suppress them here.
      // For example, for the input "ろっぽんぎ", "六" can be looked up for
      // the prefix reading "ろ" or "ろっ", and then be expanded with "6", "Ⅵ",
      // etc.
      return TRAVERSE_CONTINUE;
    }
    const Util::ScriptType script_type = Util::GetScriptType(token.value);
    if (script_type == Util::NUMBER || script_type == Util::ALPHABET ||
        script_type == Util::EMOJI) {
      return TRAVERSE_CONTINUE;
    }
    if (Util::CharsLen(token.value) < min_value_chars_len_) {
      return TRAVERSE_CONTINUE;
    }
    Result result;
    result.InitializeByTokenAndTypes(token, PREFIX);
    if (key != actual_key) {
      result.candidate_attributes |= Segment::Candidate::TYPING_CORRECTION;
    }
    result.consumed_key_size = Util::CharsLen(key);
    results_->emplace_back(result);
    return (results_->size() < limit_) ? TRAVERSE_CONTINUE : TRAVERSE_DONE;
  }

 private:
  const size_t limit_;
  const int kanji_number_id_;
  const int unknown_id_;
  const int min_value_chars_len_;
  std::vector<Result> *results_;
};

DictionaryPredictionAggregator::DictionaryPredictionAggregator(
    const DataManagerInterface &data_manager,
    const ConverterInterface *converter,
    const ImmutableConverterInterface *immutable_converter,
    const dictionary::DictionaryInterface *dictionary,
    const dictionary::DictionaryInterface *suffix_dictionary,
    const dictionary::PosMatcher *pos_matcher)
    : DictionaryPredictionAggregator(
          data_manager, converter, immutable_converter, dictionary,
          suffix_dictionary, pos_matcher,
          std::make_unique<SingleKanjiPredictionAggregator>(data_manager)) {}

DictionaryPredictionAggregator::DictionaryPredictionAggregator(
    const DataManagerInterface &data_manager,
    const ConverterInterface *converter,
    const ImmutableConverterInterface *immutable_converter,
    const dictionary::DictionaryInterface *dictionary,
    const dictionary::DictionaryInterface *suffix_dictionary,
    const dictionary::PosMatcher *pos_matcher,
    std::unique_ptr<PredictionAggregatorInterface>
        single_kanji_prediction_aggregator)
    : converter_(converter),
      immutable_converter_(immutable_converter),
      dictionary_(dictionary),
      suffix_dictionary_(suffix_dictionary),
      counter_suffix_word_id_(pos_matcher->GetCounterSuffixWordId()),
      kanji_number_id_(pos_matcher->GetKanjiNumberId()),
      zip_code_id_(pos_matcher->GetZipcodeId()),
      number_id_(pos_matcher->GetNumberId()),
      unknown_id_(pos_matcher->GetUnknownId()),
      single_kanji_prediction_aggregator_(
          std::move(single_kanji_prediction_aggregator)) {
  absl::string_view zero_query_token_array_data;
  absl::string_view zero_query_string_array_data;
  absl::string_view zero_query_number_token_array_data;
  absl::string_view zero_query_number_string_array_data;
  data_manager.GetZeroQueryData(&zero_query_token_array_data,
                                &zero_query_string_array_data,
                                &zero_query_number_token_array_data,
                                &zero_query_number_string_array_data);
  zero_query_dict_.Init(zero_query_token_array_data,
                        zero_query_string_array_data);
  zero_query_number_dict_.Init(zero_query_number_token_array_data,
                               zero_query_number_string_array_data);
}

std::vector<Result> DictionaryPredictionAggregator::AggregateResults(
    const ConversionRequest &request, const Segments &segments) const {
  std::vector<Result> results;
  AggregatePredictionForTesting(request, segments, &results);
  return results;
}

PredictionTypes DictionaryPredictionAggregator::AggregatePredictionForTesting(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  const bool is_mixed_conversion = IsMixedConversionEnabled(request.request());
  // In mixed conversion mode, the number of real time candidates is increased.
  const size_t realtime_max_size =
      GetRealtimeCandidateMaxSize(request, segments, is_mixed_conversion);
  const auto &unigram_config = GetUnigramConfig(request);

  return AggregatePrediction(request, realtime_max_size, unigram_config,
                             segments, results);
}

DictionaryPredictionAggregator::UnigramConfig
DictionaryPredictionAggregator::GetUnigramConfig(
    const ConversionRequest &request) const {
  const bool is_mixed_conversion = IsMixedConversionEnabled(request.request());
  if (IsLatinInputMode(request)) {
    // For SUGGESTION request in Desktop, We don't look up English words when
    // key length is one.
    const size_t min_key_len_for_latin_input =
        (is_mixed_conversion ||
         request.request_type() == ConversionRequest::PREDICTION)
            ? 1
            : 2;
    return {
        &DictionaryPredictionAggregator::AggregateUnigramCandidateForLatinInput,
        min_key_len_for_latin_input};
  }

  if (is_mixed_conversion) {
    // In mixed conversion mode, we want to show unigram candidates even for
    // short keys to emulate PREDICTION mode.
    constexpr size_t kMinUnigramKeyLen = 1;
    return {&DictionaryPredictionAggregator::
                AggregateUnigramCandidateForMixedConversion,
            kMinUnigramKeyLen};
  }

  // Normal prediction.
  const size_t min_unigram_key_len =
      (request.request_type() == ConversionRequest::PREDICTION) ? 1 : 3;
  return {&DictionaryPredictionAggregator::AggregateUnigramCandidate,
          min_unigram_key_len};
}

PredictionTypes DictionaryPredictionAggregator::AggregatePrediction(
    const ConversionRequest &request, size_t realtime_max_size,
    const UnigramConfig &unigram_config, const Segments &segments,
    std::vector<Result> *results) const {
  DCHECK(results);

  // Zero query prediction.
  if (segments.conversion_segment(0).key().empty()) {
    return AggregatePredictionForZeroQuery(request, segments, results);
  }

  const std::string &key = segments.conversion_segment(0).key();
  const size_t key_len = Util::CharsLen(key);

  // TODO(toshiyuki): Check if we can remove this SUGGESTION check.
  // i.e. can we return NO_PREDICTION here for both of SUGGESTION and
  // PREDICTION?
  if (request.request_type() == ConversionRequest::SUGGESTION) {
    if (!request.config().use_dictionary_suggest()) {
      VLOG(2) << "no_dictionary_suggest";
      return NO_PREDICTION;
    }
    // Never trigger prediction if the key looks like zip code.
    if (DictionaryPredictionAggregator::IsZipCodeRequest(key) && key_len < 6) {
      return NO_PREDICTION;
    }
  }
  PredictionTypes selected_types = NO_PREDICTION;
  if (ShouldAggregateRealTimeConversionResults(request, segments)) {
    AggregateRealtimeConversion(request, realtime_max_size, segments, results);
    selected_types |= REALTIME;
  }
  // In partial suggestion or prediction, only realtime candidates are used.
  if (request.request_type() == ConversionRequest::PARTIAL_SUGGESTION ||
      request.request_type() == ConversionRequest::PARTIAL_PREDICTION) {
    return selected_types;
  }

  // Add unigram candidates.
  const size_t min_unigram_key_len = unigram_config.min_key_len;
  if (key_len >= min_unigram_key_len) {
    const auto &unigram_fn = unigram_config.unigram_fn;
    PredictionType type = (this->*unigram_fn)(request, segments, results);
    selected_types |= type;
  }

  if (IsMixedConversionEnabled(request.request()) && key_len > 0 &&
      AggregateNumberCandidates(request, segments, results)) {
    selected_types |= NUMBER;
  }

  // Add bigram candidates.
  constexpr int kMinHistoryKeyLen = 3;
  if (HasHistoryKeyLongerThanOrEqualTo(segments, kMinHistoryKeyLen)) {
    AggregateBigramPrediction(request, segments,
                              Segment::Candidate::SOURCE_INFO_NONE, results);
    selected_types |= BIGRAM;
  }

  // Add english candidates.
  if (IsLanguageAwareInputEnabled(request) && IsQwertyMobileTable(request) &&
      key_len >= min_unigram_key_len) {
    AggregateEnglishPredictionUsingRawInput(request, segments, results);
    selected_types |= ENGLISH;
  }

  // Add typing correction candidates.
  constexpr int kMinTypingCorrectionKeyLen = 3;
  if (IsTypingCorrectionEnabled(request) &&
      key_len >= kMinTypingCorrectionKeyLen) {
    AggregateTypeCorrectingPrediction(request, segments, selected_types,
                                      results);
    selected_types |= TYPING_CORRECTION;
  }

  if (IsMixedConversionEnabled(request.request())) {
    AggregatePrefixCandidates(request, segments, results);
    selected_types |= PREFIX;
  }

  if (IsMixedConversionEnabled(request.request()) &&
      IsEnableSingleKanjiPrediction(request)) {
    // We do not want to add single kanji results for non mixed conversion
    // (i.e., Desktop, or Hardware Keyboard in Mobile), since they contain
    // partial results.
    const std::vector<Result> single_kanji_results =
        single_kanji_prediction_aggregator_->AggregateResults(request,
                                                              segments);
    if (!single_kanji_results.empty()) {
      results->insert(results->end(), single_kanji_results.begin(),
                      single_kanji_results.end());
      selected_types |= SINGLE_KANJI;
    }
  }

  return selected_types;
}

PredictionTypes DictionaryPredictionAggregator::AggregatePredictionForZeroQuery(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  DCHECK(results);

  if (!request.request().zero_query_suggestion()) {
    // Zero query is disabled by request.
    return NO_PREDICTION;
  }

  PredictionTypes selected_types = NO_PREDICTION;
  constexpr int kMinHistoryKeyLenForZeroQuery = 2;
  if (HasHistoryKeyLongerThanOrEqualTo(segments,
                                       kMinHistoryKeyLenForZeroQuery)) {
    AggregateBigramPrediction(
        request, segments,
        Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_BIGRAM, results);
    selected_types |= BIGRAM;
  }
  if (segments.history_segments_size() > 0) {
    AggregateZeroQuerySuffixPrediction(request, segments, results);
    selected_types |= SUFFIX;
  }
  return selected_types;
}

PredictionType
DictionaryPredictionAggregator::AggregateUnigramCandidateForLatinInput(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  AggregateEnglishPrediction(request, segments, results);
  return ENGLISH;
}

namespace {

class FindValueCallback : public DictionaryInterface::Callback {
 public:
  FindValueCallback(const FindValueCallback &) = delete;
  FindValueCallback &operator=(const FindValueCallback &) = delete;
  explicit FindValueCallback(absl::string_view target_value)
      : target_value_(target_value), found_(false) {}

  ResultType OnToken(absl::string_view,  // key
                     absl::string_view,  // actual_key
                     const Token &token) override {
    if (token.value != target_value_) {
      return TRAVERSE_CONTINUE;
    }
    found_ = true;
    token_ = token;
    return TRAVERSE_DONE;
  }

  bool found() const { return found_; }

  const Token &token() const { return token_; }

 private:
  absl::string_view target_value_;
  bool found_;
  Token token_;
};

}  // namespace

size_t DictionaryPredictionAggregator::GetRealtimeCandidateMaxSize(
    const ConversionRequest &request, const Segments &segments,
    bool mixed_conversion) const {
  const ConversionRequest::RequestType request_type = request.request_type();
  DCHECK(request_type == ConversionRequest::PREDICTION ||
         request_type == ConversionRequest::SUGGESTION ||
         request_type == ConversionRequest::PARTIAL_PREDICTION ||
         request_type == ConversionRequest::PARTIAL_SUGGESTION);
  if (segments.conversion_segments_size() == 0) {
    return 0;
  }
  const bool is_long_key = IsLongKeyForRealtimeCandidates(segments);
  const size_t max_size =
      GetMaxSizeForRealtimeCandidates(request, segments, is_long_key);
  const size_t default_size = GetDefaultSizeForRealtimeCandidates(is_long_key);
  size_t size = 0;
  switch (request_type) {
    case ConversionRequest::PREDICTION:
      size = mixed_conversion ? max_size : default_size;
      break;
    case ConversionRequest::SUGGESTION:
      // Fewer candidatats are needed basically.
      // But on mixed_conversion mode we should behave like as conversion mode.
      size = mixed_conversion ? default_size : 1;
      break;
    case ConversionRequest::PARTIAL_PREDICTION:
      // This is kind of prediction so richer result than PARTIAL_SUGGESTION
      // is needed.
      size = max_size;
      break;
    case ConversionRequest::PARTIAL_SUGGESTION:
      // PARTIAL_SUGGESTION works like as conversion mode so returning
      // some candidates is needed.
      size = default_size;
      break;
    default:
      size = 0;  // Never reach here
  }

  return std::min(max_size, size);
}

bool DictionaryPredictionAggregator::PushBackTopConversionResult(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  DCHECK_EQ(1, segments.conversion_segments_size());

  Segments tmp_segments = GetSegmentsForRealtimeCandidatesGeneration(segments);
  ConversionRequest tmp_request = request;
  tmp_request.set_max_conversion_candidates_size(20);
  tmp_request.set_composer_key_selection(ConversionRequest::PREDICTION_KEY);
  // Some rewriters cause significant performance loss. So we skip them.
  tmp_request.set_skip_slow_rewriters(true);
  // This method emulates usual converter's behavior so here disable
  // partial candidates.
  tmp_request.set_create_partial_candidates(false);
  tmp_request.set_request_type(ConversionRequest::CONVERSION);
  if (!converter_->StartConversionForRequest(tmp_request, &tmp_segments)) {
    return false;
  }

  results->push_back(Result());
  Result *result = &results->back();
  result->key = segments.conversion_segment(0).key();
  result->lid = tmp_segments.conversion_segment(0).candidate(0).lid;
  result->rid =
      tmp_segments
          .conversion_segment(tmp_segments.conversion_segments_size() - 1)
          .candidate(0)
          .rid;
  result->SetTypesAndTokenAttributes(REALTIME | REALTIME_TOP, Token::NONE);
  result->candidate_attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;

  // Concatenate the top candidates.
  // Note that since StartConversionForRequest() runs in conversion mode, the
  // resulting |tmp_segments| doesn't have inner_segment_boundary. We need to
  // construct it manually here.
  // TODO(noriyukit): This is code duplicate in converter/nbest_generator.cc and
  // we should refactor code after finding more good design.
  bool inner_segment_boundary_success = true;
  for (size_t i = 0; i < tmp_segments.conversion_segments_size(); ++i) {
    const Segment &segment = tmp_segments.conversion_segment(i);
    const Segment::Candidate &candidate = segment.candidate(0);
    result->value.append(candidate.value);
    result->wcost += candidate.wcost;

    uint32_t encoded_lengths;
    if (inner_segment_boundary_success &&
        Segment::Candidate::EncodeLengths(
            candidate.key.size(), candidate.value.size(),
            candidate.content_key.size(), candidate.content_value.size(),
            &encoded_lengths)) {
      result->inner_segment_boundary.push_back(encoded_lengths);
    } else {
      inner_segment_boundary_success = false;
    }
  }
  if (!inner_segment_boundary_success) {
    LOG(WARNING) << "Failed to construct inner segment boundary";
    result->inner_segment_boundary.clear();
  }
  return true;
}

void DictionaryPredictionAggregator::AggregateRealtimeConversion(
    const ConversionRequest &request, size_t realtime_candidates_size,
    const Segments &segments, std::vector<Result> *results) const {
  DCHECK(converter_);
  DCHECK(immutable_converter_);
  DCHECK(results);
  // First insert a top conversion result.
  if (request.use_actual_converter_for_realtime_conversion()) {
    if (!PushBackTopConversionResult(request, segments, results)) {
      LOG(WARNING) << "Realtime conversion with converter failed";
    }
  }

  if (realtime_candidates_size == 0) {
    return;
  }

  const ConversionRequest request_for_realtime =
      GetConversionRequestForRealtimeCandidates(request,
                                                realtime_candidates_size);
  Segments tmp_segments = GetSegmentsForRealtimeCandidatesGeneration(segments);
  if (!immutable_converter_->ConvertForRequest(request_for_realtime,
                                               &tmp_segments) ||
      tmp_segments.conversion_segments_size() == 0 ||
      tmp_segments.conversion_segment(0).candidates_size() == 0) {
    LOG(WARNING) << "Convert failed";
    return;
  }

  // Copy candidates into the array of Results.
  const Segment segment = tmp_segments.conversion_segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    const Segment::Candidate &candidate = segment.candidate(i);
    results->push_back(Result());
    Result *result = &results->back();
    result->key = candidate.key;
    result->value = candidate.value;
    // TODO(toshiyuki): Fix the cost.
    // This should be |candidate.wcost + candidate.structure_cost|.
    // |wcost| does not include transition cost between internal nodes.
    result->wcost = candidate.wcost;
    result->lid = candidate.lid;
    result->rid = candidate.rid;
    result->inner_segment_boundary = candidate.inner_segment_boundary;
    result->SetTypesAndTokenAttributes(REALTIME, Token::NONE);
    result->candidate_attributes |= candidate.attributes;
    result->consumed_key_size = candidate.consumed_key_size;
  }
}

size_t DictionaryPredictionAggregator::GetCandidateCutoffThreshold(
    ConversionRequest::RequestType request_type) const {
  DCHECK(request_type == ConversionRequest::PREDICTION ||
         request_type == ConversionRequest::SUGGESTION);
  if (request_type == ConversionRequest::PREDICTION) {
    // If PREDICTION, many candidates are needed than SUGGESTION.
    return kPredictionMaxResultsSize;
  }
  return kSuggestionMaxResultsSize;
}

PredictionType DictionaryPredictionAggregator::AggregateUnigramCandidate(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  DCHECK(results);
  DCHECK(dictionary_);
  DCHECK(request.request_type() == ConversionRequest::PREDICTION ||
         request.request_type() == ConversionRequest::SUGGESTION);

  const size_t cutoff_threshold =
      GetCandidateCutoffThreshold(request.request_type());
  const size_t prev_results_size = results->size();
  GetPredictiveResults(*dictionary_, "", request, segments, UNIGRAM,
                       cutoff_threshold, Segment::Candidate::SOURCE_INFO_NONE,
                       zip_code_id_, unknown_id_, results);
  const size_t unigram_results_size = results->size() - prev_results_size;

  // If size reaches max_results_size (== cutoff_threshold).
  // we don't show the candidates, since disambiguation from
  // 256 candidates is hard. (It may exceed max_results_size, because this is
  // just a limit for each backend, so total number may be larger)
  if (unigram_results_size >= cutoff_threshold) {
    results->resize(prev_results_size);
  }
  return UNIGRAM;
}

PredictionType
DictionaryPredictionAggregator::AggregateUnigramCandidateForMixedConversion(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  DCHECK(request.request_type() == ConversionRequest::PREDICTION ||
         request.request_type() == ConversionRequest::SUGGESTION);
  LookupUnigramCandidateForMixedConversion(*dictionary_, request, segments,
                                           zip_code_id_, unknown_id_, results);
  return UNIGRAM;
}

void DictionaryPredictionAggregator::LookupUnigramCandidateForMixedConversion(
    const dictionary::DictionaryInterface &dictionary,
    const ConversionRequest &request, const Segments &segments, int zip_code_id,
    int unknown_id, std::vector<Result> *results) {
  const size_t cutoff_threshold = kPredictionMaxResultsSize;

  std::vector<Result> raw_result;
  // No history key
  GetPredictiveResults(dictionary, "", request, segments, UNIGRAM,
                       cutoff_threshold, Segment::Candidate::SOURCE_INFO_NONE,
                       zip_code_id, unknown_id, &raw_result);

  // Hereafter, we split "Needed Results" and "(maybe) Unneeded Results."
  // The algorithm is:
  // 1) Take the Result with minimum cost.
  // 2) Remove results which is "redundant" (defined by MaybeRedundant),
  //    from remaining results.
  // 3) Repeat 1) and 2) five times.
  // Note: to reduce the number of memory allocation, we swap out the
  //   "redundant" results to the end of the |results| vector.
  constexpr size_t kDeleteTrialNum = 5;

  // min_iter is the beginning of the remaining results (inclusive), and
  // max_iter is the end of the remaining results (exclusive).
  typedef std::vector<Result>::iterator Iter;
  Iter min_iter = raw_result.begin();
  Iter max_iter = raw_result.end();
  for (size_t i = 0; i < kDeleteTrialNum; ++i) {
    if (min_iter == max_iter) {
      break;
    }

    // Find the Result with minimum cost. Swap it with the beginning element.
    std::iter_swap(min_iter,
                   std::min_element(min_iter, max_iter, ResultWCostLess()));

    const Result &reference_result = *min_iter;

    // Preserve the reference result.
    ++min_iter;

    // Traverse all remaining elements and check if each result is redundant.
    for (Iter iter = min_iter; iter != max_iter;) {
      // - We do not filter user dictionary word.
      const bool should_check_redundant = !iter->IsUserDictionaryResult();
      if (should_check_redundant &&
          MaybeRedundant(reference_result.value, iter->value)) {
        // Swap out the redundant result.
        --max_iter;
        std::iter_swap(iter, max_iter);
      } else {
        ++iter;
      }
    }
  }

  // Then the |raw_result| contains;
  // [begin, min_iter): reference results in the above loop.
  // [max_iter, end): (maybe) redundant results.
  // [min_iter, max_iter): remaining results.
  // Here, we revive the redundant results up to five in the result cost order.
  constexpr size_t kDoNotDeleteNum = 5;
  if (std::distance(max_iter, raw_result.end()) >= kDoNotDeleteNum) {
    std::partial_sort(max_iter, max_iter + kDoNotDeleteNum, raw_result.end(),
                      ResultWCostLess());
    max_iter += kDoNotDeleteNum;
  } else {
    max_iter = raw_result.end();
  }

  // Finally output the result.
  results->insert(results->end(), raw_result.begin(), max_iter);
}

void DictionaryPredictionAggregator::AggregateBigramPrediction(
    const ConversionRequest &request, const Segments &segments,
    Segment::Candidate::SourceInfo source_info,
    std::vector<Result> *results) const {
  DCHECK(results);
  DCHECK(dictionary_);

  // TODO(toshiyuki): Support suggestion from the last 2 histories.
  //  ex) "六本木"+"ヒルズ"->"レジデンス"
  std::string history_key, history_value;
  if (!GetHistoryKeyAndValue(segments, &history_key, &history_value)) {
    return;
  }
  AddBigramResultsFromHistory(history_key, history_value, request, segments,
                              source_info, results);
}

void DictionaryPredictionAggregator::AddBigramResultsFromHistory(
    const absl::string_view history_key, const absl::string_view history_value,
    const ConversionRequest &request, const Segments &segments,
    Segment::Candidate::SourceInfo source_info,
    std::vector<Result> *results) const {
  // Check that history_key/history_value are in the dictionary.
  FindValueCallback find_history_callback(history_value);
  dictionary_->LookupPrefix(history_key, request, &find_history_callback);

  // History value is not found in the dictionary.
  // User may create this the history candidate from T13N or segment
  // expand/shrinkg operations.
  if (!find_history_callback.found()) {
    return;
  }

  const size_t cutoff_threshold =
      GetCandidateCutoffThreshold(request.request_type());
  const size_t prev_results_size = results->size();
  GetPredictiveResultsForBigram(*dictionary_, history_key, history_value,
                                request, segments, BIGRAM, cutoff_threshold,
                                source_info, unknown_id_, results);
  const size_t bigram_results_size = results->size() - prev_results_size;

  // if size reaches max_results_size,
  // we don't show the candidates, since disambiguation from
  // 256 candidates is hard. (It may exceed max_results_size, because this is
  // just a limit for each backend, so total number may be larger)
  if (bigram_results_size >= cutoff_threshold) {
    results->resize(prev_results_size);
    return;
  }

  // Obtain the character type of the last history value.
  const size_t history_value_size = Util::CharsLen(history_value);
  if (history_value_size == 0) {
    return;
  }

  const Util::ScriptType history_ctype = Util::GetScriptType(history_value);
  const Util::ScriptType last_history_ctype = Util::GetScriptType(
      Util::Utf8SubString(history_value, history_value_size - 1, 1));
  for (size_t i = prev_results_size; i < results->size(); ++i) {
    CheckBigramResult(find_history_callback.token(), history_ctype,
                      last_history_ctype, request, &(*results)[i]);
  }
}

// Filter out irrelevant bigrams. For example, we don't want to
// suggest "リカ" from the history "アメ".
void DictionaryPredictionAggregator::CheckBigramResult(
    const Token &history_token, const Util::ScriptType history_ctype,
    const Util::ScriptType last_history_ctype, const ConversionRequest &request,
    Result *result) const {
  DCHECK(result);

  const std::string &history_key = history_token.key;
  const std::string &history_value = history_token.value;
  const std::string key(result->key, history_key.size(),
                        result->key.size() - history_key.size());
  const std::string value(result->value, history_value.size(),
                          result->value.size() - history_value.size());

  // Don't suggest 0-length key/value.
  if (key.empty() || value.empty()) {
    result->removed = true;
    MOZC_WORD_LOG(*result, "Removed. key, value or both are empty.");
    return;
  }

  const Util::ScriptType ctype =
      Util::GetScriptType(Util::Utf8SubString(value, 0, 1));

  if (history_ctype == Util::KANJI && ctype == Util::KATAKANA) {
    // Do not filter "六本木ヒルズ"
    MOZC_WORD_LOG(*result, "Valid bigram. Kanji + Katakana pattern.");
    return;
  }

  // If freq("アメ") < freq("アメリカ"), we don't
  // need to suggest it. As "アメリカ" should already be
  // suggested when user type "アメ".
  // Note that wcost = -500 * log(prob).
  if (ctype != Util::KANJI && history_token.cost > result->wcost) {
    result->removed = true;
    MOZC_WORD_LOG(*result,
                  "Removed. The prefix's score is lower than the whole.");
    return;
  }

  // If character type doesn't change, this boundary might NOT
  // be a word boundary. Only use iif the entire key is reasonably long.
  const size_t key_len = Util::CharsLen(result->key);
  if (ctype == last_history_ctype &&
      ((ctype == Util::HIRAGANA && key_len <= 9) ||
       (ctype == Util::KATAKANA && key_len <= 5))) {
    result->removed = true;
    MOZC_WORD_LOG(*result, "Removed. Short Hiragana (<= 9) or Katakana (<= 5)");
    return;
  }

  // The suggested key/value pair must exist in the dictionary.
  // For example, we don't want to suggest "ターネット" from
  // the history "イン".
  // If character type is Kanji and the suggestion is not a
  // zero_query_suggestion, we relax this condition, as there are
  // many Kanji-compounds which may not in the dictionary. For example,
  // we want to suggest "霊長類研究所" from the history "京都大学".
  if (ctype == Util::KANJI && Util::CharsLen(value) >= 2) {
    // Do not filter this.
    // TODO(toshiyuki): one-length kanji prediction may be annoying other than
    // some exceptions, "駅", "口", etc
    MOZC_WORD_LOG(*result, "Valid bigram. Kanji suffix (>= 2).");
    return;
  }

  // Check if the word is in the dictionary or not.
  // For Hiragana words, check if that word is in a key of values.
  // This is for a situation that
  // ありがとうございました is not in the dictionary, but
  // ありがとう御座いました is in the dictionary.
  if (ctype == Util::HIRAGANA) {
    if (!dictionary_->HasKey(key)) {
      result->removed = true;
      MOZC_WORD_LOG(*result, "Removed. No keys are found.");
      return;
    }
  } else {
    FindValueCallback callback(value);
    dictionary_->LookupPrefix(key, request, &callback);
    if (!callback.found()) {
      result->removed = true;
      MOZC_WORD_LOG(*result, "Removed. No prefix found.");
      return;
    }
  }

  MOZC_WORD_LOG(*result, "Valid bigram.");
}

void DictionaryPredictionAggregator::GetPredictiveResults(
    const DictionaryInterface &dictionary, const absl::string_view history_key,
    const ConversionRequest &request, const Segments &segments,
    PredictionTypes types, size_t lookup_limit,
    Segment::Candidate::SourceInfo source_info, int zip_code_id, int unknown_id,
    std::vector<Result> *results) {
  if (!request.has_composer()) {
    std::string input_key(history_key);
    input_key.append(segments.conversion_segment(0).key());
    PredictiveLookupCallback callback(
        types, lookup_limit, input_key.size(), nullptr, source_info,
        zip_code_id, unknown_id, "", GetSpatialCostParams(request), results);
    dictionary.LookupPredictive(input_key, request, &callback);
    return;
  }

  // If we have ambiguity for the input, get expanded key.
  // Example1 roman input: for "あk", we will get |base|, "あ" and |expanded|,
  // "か", "き", etc
  // Example2 kana input: for "あか", we will get |base|, "あ" and |expanded|,
  // "か", and "が".
  std::string base;
  std::set<std::string> expanded;
  request.composer().GetQueriesForPrediction(&base, &expanded);
  std::string input_key;
  if (expanded.empty()) {
    input_key = absl::StrCat(history_key, base);
    PredictiveLookupCallback callback(
        types, lookup_limit, input_key.size(), nullptr, source_info,
        zip_code_id, unknown_id, "", GetSpatialCostParams(request), results);
    dictionary.LookupPredictive(input_key, request, &callback);
    return;
  }

  // `non_expanded_original_key` keeps the original key request before
  // key expansions. This key is passed to the callback so that it can
  // identify whether the key is actually expanded or not.
  const std::string non_expanded_original_key =
      absl::StrCat(history_key, segments.conversion_segment(0).key());

  // |expanded| is a very small set, so calling LookupPredictive multiple
  // times is not so expensive.  Also, the number of lookup results is limited
  // by |lookup_limit|.
  for (const std::string &expanded_char : expanded) {
    input_key = absl::StrCat(history_key, base, expanded_char);
    PredictiveLookupCallback callback(types, lookup_limit, input_key.size(),
                                      nullptr, source_info, zip_code_id,
                                      unknown_id, non_expanded_original_key,
                                      GetSpatialCostParams(request), results);
    dictionary.LookupPredictive(input_key, request, &callback);
  }
}

void DictionaryPredictionAggregator::GetPredictiveResultsForBigram(
    const DictionaryInterface &dictionary, const absl::string_view history_key,
    const absl::string_view history_value, const ConversionRequest &request,
    const Segments &segments, PredictionTypes types, size_t lookup_limit,
    Segment::Candidate::SourceInfo source_info, int unknown_id_,
    std::vector<Result> *results) const {
  if (!request.has_composer()) {
    std::string input_key(history_key);
    input_key.append(segments.conversion_segment(0).key());
    PredictiveBigramLookupCallback callback(
        types, lookup_limit, input_key.size(), nullptr, history_value,
        source_info, zip_code_id_, unknown_id_, "",
        GetSpatialCostParams(request), results);
    dictionary.LookupPredictive(input_key, request, &callback);
    return;
  }

  // If we have ambiguity for the input, get expanded key.
  // Example1 roman input: for "あk", we will get |base|, "あ" and |expanded|,
  // "か", "き", etc
  // Example2 kana input: for "あか", we will get |base|, "あ" and |expanded|,
  // "か", and "が".
  std::string base;
  std::set<std::string> expanded;
  request.composer().GetQueriesForPrediction(&base, &expanded);
  const std::string input_key = absl::StrCat(history_key, base);
  const std::string non_expanded_original_key =
      absl::StrCat(history_key, segments.conversion_segment(0).key());

  PredictiveBigramLookupCallback callback(
      types, lookup_limit, input_key.size(),
      expanded.empty() ? nullptr : &expanded, history_value, source_info,
      zip_code_id_, unknown_id_, non_expanded_original_key,
      GetSpatialCostParams(request), results);
  dictionary.LookupPredictive(input_key, request, &callback);
}

void DictionaryPredictionAggregator::GetPredictiveResultsForEnglishKey(
    const DictionaryInterface &dictionary, const ConversionRequest &request,
    const absl::string_view input_key, PredictionTypes types,
    size_t lookup_limit, std::vector<Result> *results) const {
  const size_t prev_results_size = results->size();
  if (Util::IsUpperAscii(input_key)) {
    // For upper case key, look up its lower case version and then transform
    // the results to upper case.
    std::string key(input_key);
    Util::LowerString(&key);
    PredictiveLookupCallback callback(types, lookup_limit, key.size(), nullptr,
                                      Segment::Candidate::SOURCE_INFO_NONE,
                                      zip_code_id_, unknown_id_, "",
                                      GetSpatialCostParams(request), results);
    dictionary.LookupPredictive(key, request, &callback);
    for (size_t i = prev_results_size; i < results->size(); ++i) {
      Util::UpperString(&(*results)[i].value);
    }
  } else if (Util::IsCapitalizedAscii(input_key)) {
    // For capitalized key, look up its lower case version and then transform
    // the results to capital.
    std::string key(input_key);
    Util::LowerString(&key);
    PredictiveLookupCallback callback(types, lookup_limit, key.size(), nullptr,
                                      Segment::Candidate::SOURCE_INFO_NONE,
                                      zip_code_id_, unknown_id_, "",
                                      GetSpatialCostParams(request), results);
    dictionary.LookupPredictive(key, request, &callback);
    for (size_t i = prev_results_size; i < results->size(); ++i) {
      Util::CapitalizeString(&(*results)[i].value);
    }
  } else {
    // For other cases (lower and as-is), just look up directly.
    PredictiveLookupCallback callback(
        types, lookup_limit, input_key.size(), nullptr,
        Segment::Candidate::SOURCE_INFO_NONE, zip_code_id_, unknown_id_, "",
        GetSpatialCostParams(request), results);
    dictionary.LookupPredictive(input_key, request, &callback);
  }
  // If input mode is FULL_ASCII, then convert the results to full-width.
  if (request.has_composer() &&
      request.composer().GetInputMode() == transliteration::FULL_ASCII) {
    std::string tmp;
    for (size_t i = prev_results_size; i < results->size(); ++i) {
      tmp.assign((*results)[i].value);
      japanese_util::HalfWidthAsciiToFullWidthAscii(tmp, &(*results)[i].value);
    }
  }
}

void DictionaryPredictionAggregator::
    GetPredictiveResultsUsingExtendedTypingCorrection(
        absl::Span<const composer::TypeCorrectedQuery> queries,
        const ConversionRequest &request, const Segments &segments,
        PredictionTypes base_selected_types,
        std::vector<Result> *results) const {
  if (queries.empty()) {
    return;
  }

  // Only use the 1-best correction now.
  // We here intentionally use 'asis' string since the composition
  // spellchecker is designed to predict the word from incomplete composition
  // string, e.g. おk. -> おか
  // TODO(taku): Revisits this design once QWERTY model gets ready.
  absl::string_view key = queries.front().asis;
  const bool is_kana_modifier_insensitive_only =
      queries.front().is_kana_modifier_insensitive_only;
  const size_t key_len = Util::CharsLen(key);

  // Makes dummy segments with corrected query.
  Segments corrected_segments = segments;
  corrected_segments.mutable_conversion_segment(0)->set_key(key);

  // Make ConversionRequest that has no composer to avoid the original key from
  // being used during the candidate aggregation.
  // Kana modifier insensitive dictionary lookup is also disabled as
  // composition spellchecker has already fixed them.
  ConversionRequest corrected_request = request;
  corrected_request.set_composer(nullptr);
  corrected_request.set_kana_modifier_insensitive_conversion(false);

  std::vector<Result> corrected_results;

  const UnigramConfig &unigram_config = GetUnigramConfig(request);
  if (key_len >= unigram_config.min_key_len) {
    const AggregateUnigramFn &unigram_fn = unigram_config.unigram_fn;
    (this->*unigram_fn)(corrected_request, corrected_segments,
                        &corrected_results);
  }

  if (base_selected_types & REALTIME) {
    constexpr int kRealtimeSize = 2;
    AggregateRealtimeConversion(corrected_request, kRealtimeSize,
                                corrected_segments, &corrected_results);
  }

  if (base_selected_types & BIGRAM) {
    AggregateBigramPrediction(corrected_request, corrected_segments,
                              Segment::Candidate::SOURCE_INFO_NONE,
                              &corrected_results);
  }

  // Appends the result with TYPING_CORRECTION attribute.
  for (Result &result : corrected_results) {
    // If the correction is a pure kana modifier insensitive correction,
    // typing correction annotation is not necessary.
    if (!is_kana_modifier_insensitive_only) {
      result.types |= TYPING_CORRECTION;
    }
    results->emplace_back(std::move(result));
  }
}

void DictionaryPredictionAggregator::GetPredictiveResultsUsingTypingCorrection(
    absl::Span<const composer::TypeCorrectedQuery> queries,
    const DictionaryInterface &dictionary, const ConversionRequest &request,
    const Segments &segments, int lookup_limit,
    std::vector<Result> *results) const {
  for (size_t query_index = 0; query_index < queries.size(); ++query_index) {
    const composer::TypeCorrectedQuery &query = queries[query_index];
    const std::string &input_key = query.base;
    const size_t previous_results_size = results->size();
    PredictiveLookupCallback callback(
        TYPING_CORRECTION, lookup_limit, input_key.size(),
        query.expanded.empty() ? nullptr : &query.expanded,
        Segment::Candidate::SOURCE_INFO_NONE, zip_code_id_, unknown_id_,
        query.asis, GetSpatialCostParams(request), results);
    dictionary.LookupPredictive(input_key, request, &callback);

    for (size_t i = previous_results_size; i < results->size(); ++i) {
      // Query cost can be negative in 'diff cost' due to typing model.
      // We do not want to strongly promote TC candidates even if the query cost
      // is negative.
      (*results)[i].wcost += std::max(0, query.cost);
    }
    lookup_limit -= results->size() - previous_results_size;
    if (lookup_limit <= 0) {
      break;
    }
  }
}

// static
bool DictionaryPredictionAggregator::GetZeroQueryCandidatesForKey(
    const ConversionRequest &request, const absl::string_view key,
    const ZeroQueryDict &dict, std::vector<ZeroQueryResult> *results) {
  DCHECK(results);
  results->clear();

  auto range = dict.equal_range(key);
  if (range.first == range.second) {
    return false;
  }

  const bool is_key_one_char_and_not_kanji =
      Util::CharsLen(key) == 1 && !Util::ContainsScriptType(key, Util::KANJI);
  for (; range.first != range.second; ++range.first) {
    const auto &entry = range.first;
    if (entry.type() != ZERO_QUERY_EMOJI) {
      results->push_back(
          std::make_pair(std::string(entry.value()), entry.type()));
      continue;
    }

    // Emoji should not be suggested for single Hiragana / Katakana input,
    // because they tend to be too much aggressive.
    if (is_key_one_char_and_not_kanji) {
      continue;
    }

    results->push_back(
        std::make_pair(std::string(entry.value()), entry.type()));
  }
  return !results->empty();
}

// static
void DictionaryPredictionAggregator::AppendZeroQueryToResults(
    const std::vector<ZeroQueryResult> &candidates, uint16_t lid, uint16_t rid,
    std::vector<Result> *results) {
  int cost = 0;

  for (size_t i = 0; i < candidates.size(); ++i) {
    // Increment cost to show the candidates in order.
    constexpr int kSuffixPenalty = 10;

    results->push_back(Result());
    Result *result = &results->back();
    result->SetTypesAndTokenAttributes(SUFFIX, Token::NONE);
    result->SetSourceInfoForZeroQuery(candidates[i].second);
    result->key = candidates[i].first;
    result->value = candidates[i].first;
    result->wcost = cost;
    result->lid = lid;
    result->rid = rid;

    cost += kSuffixPenalty;
  }
}

// Returns true if we add zero query result.
bool DictionaryPredictionAggregator::AggregateNumberZeroQueryPrediction(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  std::string number_key;
  if (!GetNumberHistory(segments, &number_key)) {
    return false;
  }

  std::vector<ZeroQueryResult> candidates_for_number_key;
  GetZeroQueryCandidatesForKey(request, number_key, zero_query_number_dict_,
                               &candidates_for_number_key);

  std::vector<ZeroQueryResult> default_candidates_for_number;
  GetZeroQueryCandidatesForKey(request, "default", zero_query_number_dict_,
                               &default_candidates_for_number);
  DCHECK(!default_candidates_for_number.empty());

  AppendZeroQueryToResults(candidates_for_number_key, counter_suffix_word_id_,
                           counter_suffix_word_id_, results);
  AppendZeroQueryToResults(default_candidates_for_number,
                           counter_suffix_word_id_, counter_suffix_word_id_,
                           results);
  return true;
}

// Returns true if we add zero query result.
bool DictionaryPredictionAggregator::AggregateZeroQueryPrediction(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  const size_t history_size = segments.history_segments_size();
  if (history_size <= 0) {
    return false;
  }

  const Segment &last_segment = segments.history_segment(history_size - 1);
  DCHECK_GT(last_segment.candidates_size(), 0);
  const std::string &history_value = last_segment.candidate(0).value;

  std::vector<ZeroQueryResult> candidates;
  if (!GetZeroQueryCandidatesForKey(request, history_value, zero_query_dict_,
                                    &candidates)) {
    return false;
  }

  const uint16_t kId = 0;  // EOS
  AppendZeroQueryToResults(candidates, kId, kId, results);
  return true;
}

// TODO(toshiyuki): Confirm whether this produces useful candidates.
// Now this is not called from anywhere.
void DictionaryPredictionAggregator::AggregateSuffixPrediction(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  DCHECK_GT(segments.conversion_segments_size(), 0);
  DCHECK(!segments.conversion_segment(0).key().empty());  // Not zero query
  // Uses larger cutoff (kPredictionMaxResultsSize) in order to consider
  // all suffix entries.
  const size_t cutoff_threshold = kPredictionMaxResultsSize;
  const std::string kEmptyHistoryKey = "";
  GetPredictiveResults(*suffix_dictionary_, kEmptyHistoryKey, request, segments,
                       SUFFIX, cutoff_threshold,
                       Segment::Candidate::SOURCE_INFO_NONE, zip_code_id_,
                       unknown_id_, results);
}

void DictionaryPredictionAggregator::AggregateZeroQuerySuffixPrediction(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  DCHECK_GT(segments.conversion_segments_size(), 0);
  DCHECK(segments.conversion_segment(0).key().empty());

  if (AggregateNumberZeroQueryPrediction(request, segments, results)) {
    return;
  }
  AggregateZeroQueryPrediction(request, segments, results);

  if (IsLatinInputMode(request)) {
    // We do not want zero query results from suffix dictionary for Latin
    // input mode. For example, we do not need "です", "。" just after "when".
    return;
  }
  // Uses larger cutoff (kPredictionMaxResultsSize) in order to consider
  // all suffix entries.
  const size_t cutoff_threshold = kPredictionMaxResultsSize;
  const std::string kEmptyHistoryKey = "";
  GetPredictiveResults(
      *suffix_dictionary_, kEmptyHistoryKey, request, segments, SUFFIX,
      cutoff_threshold,
      Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX, zip_code_id_,
      unknown_id_, results);
}

void DictionaryPredictionAggregator::AggregateEnglishPrediction(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  DCHECK(results);
  DCHECK(dictionary_);
  const size_t cutoff_threshold =
      GetCandidateCutoffThreshold(request.request_type());
  const size_t prev_results_size = results->size();
  const std::string &input_key = segments.conversion_segment(0).key();
  GetPredictiveResultsForEnglishKey(*dictionary_, request, input_key, ENGLISH,
                                    cutoff_threshold, results);

  size_t unigram_results_size = results->size() - prev_results_size;
  if (unigram_results_size >= cutoff_threshold) {
    results->resize(prev_results_size);
    return;
  }
}

void DictionaryPredictionAggregator::AggregateEnglishPredictionUsingRawInput(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  DCHECK(results);
  DCHECK(dictionary_);

  if (!request.has_composer()) {
    return;
  }

  const size_t cutoff_threshold =
      GetCandidateCutoffThreshold(request.request_type());
  const size_t prev_results_size = results->size();

  std::string input_key;
  request.composer().GetRawString(&input_key);
  GetPredictiveResultsForEnglishKey(*dictionary_, request, input_key, ENGLISH,
                                    cutoff_threshold, results);

  size_t unigram_results_size = results->size() - prev_results_size;
  if (unigram_results_size >= cutoff_threshold) {
    results->resize(prev_results_size);
    return;
  }
}

void DictionaryPredictionAggregator::AggregateTypeCorrectingPrediction(
    const ConversionRequest &request, const Segments &segments,
    PredictionTypes base_selected_types, std::vector<Result> *results) const {
  DCHECK(results);
  DCHECK(dictionary_);

  if (!request.has_composer()) {
    return;
  }

  const size_t prev_results_size = results->size();
  if (prev_results_size > 10000) {
    return;
  }

  const int lookup_limit = GetCandidateCutoffThreshold(request.request_type());

  std::string history_key, history_value;
  GetHistoryKeyAndValue(segments, &history_key, &history_value);

  const std::optional<std::vector<composer::TypeCorrectedQuery>> corrected =
      request.composer().GetTypeCorrectedQueries(history_key);

  if (corrected) {
    // Use Extended composition spell checker.
    GetPredictiveResultsUsingExtendedTypingCorrection(
        corrected.value(), request, segments, base_selected_types, results);
  } else {
    // Runs the default fallback spell checker.
    std::vector<composer::TypeCorrectedQuery> queries;
    request.composer().GetTypeCorrectedQueriesForPrediction(&queries);
    GetPredictiveResultsUsingTypingCorrection(queries, *dictionary_, request,
                                              segments, lookup_limit, results);
  }

  if (results->size() - prev_results_size >= lookup_limit) {
    results->resize(prev_results_size);
    return;
  }
}

bool DictionaryPredictionAggregator::AggregateNumberCandidates(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  DCHECK(results);

  if (!IsMixedConversionEnabled(request.request())) {
    return false;
  }

  const size_t prev_results_size = results->size();
  const size_t cutoff_threshold =
      GetCandidateCutoffThreshold(request.request_type());
  if (prev_results_size > cutoff_threshold) {
    return false;
  }

  std::string input_key;
  if (request.has_composer()) {
    request.composer().GetQueryForPrediction(&input_key);
  } else {
    input_key = segments.conversion_segment(0).key();
  }

  // NumberDecoder decoder;
  std::vector<NumberDecoder::Result> decode_results;
  if (!number_decoder_.Decode(input_key, &decode_results)) {
    return false;
  }

  for (const auto &r : decode_results) {
    Result result;
    const bool is_arabic = Util::GetScriptType(r.candidate) == Util::NUMBER;
    result.types = PredictionType::NUMBER;
    result.key = input_key.substr(0, r.consumed_key_byte_len);
    result.value = r.candidate;
    // Heuristic small cost: 1000 ~= 500 * log(10)
    result.wcost = 1000;
    result.lid = is_arabic ? number_id_ : kanji_number_id_;
    result.rid = is_arabic ? number_id_ : kanji_number_id_;
    if (r.consumed_key_byte_len < input_key.size()) {
      result.candidate_attributes |= Segment::Candidate::PARTIALLY_KEY_CONSUMED;
      result.consumed_key_size = Util::CharsLen(result.key);
    }
    results->emplace_back(result);
  }
  return true;
}

void DictionaryPredictionAggregator::AggregatePrefixCandidates(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  DCHECK(results);
  DCHECK(dictionary_);
  const size_t prev_results_size = results->size();
  const size_t cutoff_threshold =
      GetCandidateCutoffThreshold(request.request_type());
  if (prev_results_size > cutoff_threshold) {
    return;
  }

  std::string input_key;
  if (request.has_composer()) {
    request.composer().GetQueryForPrediction(&input_key);
  } else {
    input_key = segments.conversion_segment(0).key();
  }

  const size_t input_key_len = Util::CharsLen(input_key);
  if (input_key_len <= 1) {
    return;
  }
  // Excludes exact match nodes.
  Util::Utf8SubString(input_key, 0, input_key_len - 1, &input_key);

  const int min_value_chars_len =
      IsEnableSingleKanjiPrediction(request) ? 2 : 1;
  PrefixLookupCallback callback(cutoff_threshold, kanji_number_id_, unknown_id_,
                                min_value_chars_len, results);
  dictionary_->LookupPrefix(input_key, request, &callback);
  const size_t prefix_results_size = results->size() - prev_results_size;
  if (prefix_results_size >= cutoff_threshold) {
    results->resize(prev_results_size);
  }
}

bool DictionaryPredictionAggregator::ShouldAggregateRealTimeConversionResults(
    const ConversionRequest &request, const Segments &segments) {
  constexpr size_t kMaxRealtimeKeySize = 300;  // 300 bytes in UTF8
  const std::string &key = segments.conversion_segment(0).key();
  if (key.empty() || key.size() >= kMaxRealtimeKeySize) {
    // 1) If key is empty, realtime conversion doesn't work.
    // 2) If the key is too long, we'll hit a performance issue.
    return false;
  }

  return (request.request_type() == ConversionRequest::PARTIAL_SUGGESTION ||
          request.config().use_realtime_conversion() ||
          IsMixedConversionEnabled(request.request()));
}

bool DictionaryPredictionAggregator::IsZipCodeRequest(
    const absl::string_view key) {
  if (key.empty()) {
    return false;
  }

  for (ConstChar32Iterator iter(key); !iter.Done(); iter.Next()) {
    const char32_t c = iter.Get();
    if (!('0' <= c && c <= '9') && (c != '-')) {
      return false;
    }
  }
  return true;
}
}  // namespace prediction
}  // namespace mozc

#undef MOZC_WORD_LOG_MESSAGE
#undef MOZC_WORD_LOG
