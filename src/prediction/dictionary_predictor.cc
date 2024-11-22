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

#include "prediction/dictionary_predictor.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/strings/assign.h"
#include "base/strings/japanese.h"
#include "base/util.h"
#include "base/vlog.h"
#include "composer/composer.h"
#include "converter/connector.h"
#include "converter/converter_interface.h"
#include "converter/immutable_converter_interface.h"
#include "converter/segmenter.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/single_kanji_dictionary.h"
#include "engine/modules.h"
#include "engine/supplemental_model_interface.h"
#include "prediction/dictionary_prediction_aggregator.h"
#include "prediction/prediction_aggregator_interface.h"
#include "prediction/result.h"
#include "prediction/suggestion_filter.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "request/request_util.h"
#include "transliteration/transliteration.h"
#include "usage_stats/usage_stats.h"

#ifndef NDEBUG
#define MOZC_DEBUG
#endif  // NDEBUG

namespace mozc::prediction {
namespace {

using ::mozc::commands::Request;
using ::mozc::prediction::dictionary_predictor_internal::KeyValueView;
using ::mozc::usage_stats::UsageStats;

// Used to emulate positive infinity for cost. This value is set for those
// candidates that are thought to be aggressive; thus we can eliminate such
// candidates from suggestion or prediction. Note that for this purpose we
// don't want to use INT_MAX because someone might further add penalty after
// cost is set to INT_MAX, which leads to overflow and consequently aggressive
// candidates would appear in the top results.
constexpr int kInfinity = (2 << 20);

bool IsDebug(const ConversionRequest &request) {
#ifndef NDEBUG
  return true;
#else   // NDEBUG
  return request.config().verbose_level() >= 1;
#endif  // NDEBUG
}

bool IsLatinInputMode(const ConversionRequest &request) {
  return (request.has_composer() &&
          (request.composer().GetInputMode() == transliteration::HALF_ASCII ||
           request.composer().GetInputMode() == transliteration::FULL_ASCII));
}

bool IsMixedConversionEnabled(const Request &request) {
  return request.mixed_conversion();
}

bool IsTypingCorrectionEnabled(const ConversionRequest &request) {
  return request.config().use_typing_correction();
}

KeyValueView GetCandidateKeyAndValue(const Result &result
                                         ABSL_ATTRIBUTE_LIFETIME_BOUND,
                                     const KeyValueView history) {
  if (result.types & PredictionType::BIGRAM) {
    // remove the prefix of history key and history value.
    return {absl::string_view(result.key).substr(history.key.size()),
            absl::string_view(result.value).substr(history.value.size())};
  }
  return {result.key, result.value};
}

// Returns the non-expanded lookup key for the result
absl::string_view GetCandidateOriginalLookupKey(
    absl::string_view input_key ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const Result &result ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::string_view history_key) {
  if (result.non_expanded_original_key.empty()) {
    return input_key;
  }

  absl::string_view lookup_key = result.non_expanded_original_key;
  if (result.types & PredictionType::BIGRAM) {
    lookup_key.remove_prefix(history_key.size());
  }
  return lookup_key;
}

absl::string_view GetCandidateKey(const Result &result
                                      ABSL_ATTRIBUTE_LIFETIME_BOUND,
                                  absl::string_view history_key) {
  absl::string_view candidate_key = result.key;
  if (result.types & PredictionType::BIGRAM) {
    candidate_key.remove_prefix(history_key.size());
  }
  return candidate_key;
}

// Gets history key/value.
// Returns empty strings if there's no history segment in the segments.
KeyValueView GetHistoryKeyAndValue(
    const Segments &segments ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  if (segments.history_segments_size() == 0) {
    return {};
  }

  const Segment &history_segment =
      segments.history_segment(segments.history_segments_size() - 1);
  if (history_segment.candidates_size() == 0) {
    return {};
  }

  return {history_segment.candidate(0).key, history_segment.candidate(0).value};
}

template <typename... Args>
void AppendDescription(Segment::Candidate &candidate, Args &&...args) {
  absl::StrAppend(&candidate.description,
                  candidate.description.empty() ? "" : " ",
                  std::forward<Args>(args)...);
}

void MaybeFixRealtimeTopCost(absl::string_view input_key,
                             std::vector<Result> &results) {
  // Remember the minimum cost among those REALTIME
  // candidates that have the same key length as |input_key| so that we can set
  // a slightly smaller cost to REALTIME_TOP than these.
  int realtime_cost_min = kInfinity;
  Result *realtime_top_result = nullptr;
  for (size_t i = 0; i < results.size(); ++i) {
    const Result &result = results[i];
    if (result.types & PredictionType::REALTIME_TOP) {
      realtime_top_result = &results[i];
    }

    // Update the minimum cost for REALTIME candidates that have the same key
    // length as input_key.
    if (result.types & PredictionType::REALTIME &&
        result.cost < realtime_cost_min &&
        result.key.size() == input_key.size()) {
      realtime_cost_min = result.cost;
    }
  }

  // Ensure that the REALTIME_TOP candidate has relatively smaller cost than
  // those of REALTIME candidates.
  if (realtime_top_result != nullptr && realtime_cost_min != kInfinity) {
    realtime_top_result->cost = std::max(0, realtime_cost_min - 10);
  }
}

}  // namespace

DictionaryPredictor::DictionaryPredictor(
    const engine::Modules &modules, const ConverterInterface *converter,
    const ImmutableConverterInterface *immutable_converter)
    : DictionaryPredictor(
          "DictionaryPredictor", modules,
          std::make_unique<prediction::DictionaryPredictionAggregator>(
              modules, converter, immutable_converter),
          immutable_converter) {}

DictionaryPredictor::DictionaryPredictor(
    std::string predictor_name, const engine::Modules &modules,
    std::unique_ptr<const prediction::PredictionAggregatorInterface> aggregator,
    const ImmutableConverterInterface *immutable_converter)
    : aggregator_(std::move(aggregator)),
      immutable_converter_(immutable_converter),
      connector_(modules.GetConnector()),
      segmenter_(modules.GetSegmenter()),
      suggestion_filter_(modules.GetSuggestionFilter()),
      single_kanji_dictionary_(
          std::make_unique<dictionary::SingleKanjiDictionary>(
              modules.GetDataManager())),
      pos_matcher_(*modules.GetPosMatcher()),
      general_symbol_id_(pos_matcher_.GetGeneralSymbolId()),
      predictor_name_(std::move(predictor_name)),
      modules_(modules) {}

void DictionaryPredictor::Finish(const ConversionRequest &request,
                                 Segments *segments) {
  if (request.request_type() == ConversionRequest::REVERSE_CONVERSION) {
    // Do nothing for REVERSE_CONVERSION.
    return;
  }

  const Segment &segment = segments->conversion_segment(0);
  if (segment.candidates_size() < 1) {
    MOZC_VLOG(2) << "candidates size < 1";
    return;
  }

  const Segment::Candidate &candidate = segment.candidate(0);
  if (segment.segment_type() != Segment::FIXED_VALUE) {
    MOZC_VLOG(2) << "segment is not FIXED_VALUE" << candidate.value;
    return;
  }

  MaybeRecordUsageStats(candidate);
}

void DictionaryPredictor::MaybeRecordUsageStats(
    const Segment::Candidate &candidate) const {
  if (candidate.source_info &
      Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_NONE) {
    UsageStats::IncrementCount("CommitDictionaryPredictorZeroQueryTypeNone");
  }

  if (candidate.source_info &
      Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_NUMBER_SUFFIX) {
    UsageStats::IncrementCount(
        "CommitDictionaryPredictorZeroQueryTypeNumberSuffix");
  }

  if (candidate.source_info &
      Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_EMOTICON) {
    UsageStats::IncrementCount(
        "CommitDictionaryPredictorZeroQueryTypeEmoticon");
  }

  if (candidate.source_info &
      Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_EMOJI) {
    UsageStats::IncrementCount("CommitDictionaryPredictorZeroQueryTypeEmoji");
  }

  if (candidate.source_info &
      Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_BIGRAM) {
    UsageStats::IncrementCount("CommitDictionaryPredictorZeroQueryTypeBigram");
  }

  if (candidate.source_info &
      Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX) {
    UsageStats::IncrementCount("CommitDictionaryPredictorZeroQueryTypeSuffix");
  }
}

bool DictionaryPredictor::PredictForRequest(const ConversionRequest &request,
                                            Segments *segments) const {
  if (segments == nullptr) {
    return false;
  }
  if (request.request_type() == ConversionRequest::CONVERSION) {
    MOZC_VLOG(2) << "request type is CONVERSION";
    return false;
  }
  if (segments->conversion_segments_size() < 1) {
    MOZC_VLOG(2) << "segment size < 1";
    return false;
  }

  std::vector<Result> results =
      aggregator_->AggregateResults(request, *segments);
  RewriteResultsForPrediction(request, *segments, &results);

  // Explicitly populate the typing corrected results.
  MaybePopulateTypingCorrectedResults(request, *segments, &results);

  MaybeRescoreResults(request, *segments, absl::MakeSpan(results));

  return AddPredictionToCandidates(request, segments, absl::MakeSpan(results));
}

void DictionaryPredictor::RewriteResultsForPrediction(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  if (results->empty()) {
    return;
  }

  // Mixed conversion is the feature that mixes prediction and
  // conversion, meaning that results may include the candidates whose
  // key is exactly the same as the composition.  This mode is used in mobile.
  const bool is_mixed_conversion = IsMixedConversionEnabled(request.request());

  if (is_mixed_conversion) {
    SetPredictionCostForMixedConversion(request, segments, results);
  } else {
    SetPredictionCost(request.request_type(), segments, results);
  }

  if (!is_mixed_conversion) {
    const size_t input_key_len =
        Util::CharsLen(segments.conversion_segment(0).key());
    RemoveMissSpelledCandidates(input_key_len, results);
  }
}

void DictionaryPredictor::MaybePopulateTypingCorrectedResults(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  if (!IsTypingCorrectionEnabled(request) || results->empty()) {
    return;
  }

  const size_t key_len = Util::CharsLen(segments.conversion_segment(0).key());
  constexpr int kMinTypingCorrectionKeyLen = 3;
  if (key_len < kMinTypingCorrectionKeyLen) {
    return;
  }

  std::vector<Result> typing_corrected_results =
      aggregator_->AggregateTypingCorrectedResults(request, segments);
  RewriteResultsForPrediction(request, segments, &typing_corrected_results);

  for (auto &result : typing_corrected_results) {
    results->emplace_back(std::move(result));
  }
}

bool DictionaryPredictor::AddPredictionToCandidates(
    const ConversionRequest &request, Segments *segments,
    absl::Span<Result> results) const {
  DCHECK(segments);

  const KeyValueView history = GetHistoryKeyAndValue(*segments);

  Segment *segment = segments->mutable_conversion_segment(0);
  DCHECK(segment);

  // This pointer array is used to perform heap operations efficiently.
  std::vector<const Result *> result_ptrs;
  result_ptrs.reserve(results.size());
  for (const auto &r : results) result_ptrs.push_back(&r);

  // Instead of sorting all the results, we construct a heap.
  // This is done in linear time and
  // we can pop as many results as we need efficiently.
  auto min_heap_cmp = [](const Result *lhs, const Result *rhs) {
    // `rhs < lhs` instead of `lhs < rhs`, since `make_heap()` creates max heap
    // by default.
    return ResultCostLess()(*rhs, *lhs);
  };
  std::make_heap(result_ptrs.begin(), result_ptrs.end(), min_heap_cmp);

  const size_t max_candidates_size = std::min(
      request.max_dictionary_prediction_candidates_size(), results.size());

  ResultFilter filter(request, *segments, pos_matcher_, suggestion_filter_);

  // TODO(taku): Sets more advanced debug info depending on the verbose_level.
  absl::flat_hash_map<std::string, int32_t> merged_types;
  if (IsDebug(request)) {
    for (const auto &result : results) {
      if (!result.removed) {
        merged_types[result.value] |= result.types;
      }
    }
  }

#ifdef MOZC_DEBUG
  auto add_debug_candidate = [&](Result result, const absl::string_view log) {
    absl::StrAppend(&result.log, log);
    Segment::Candidate candidate;
    FillCandidate(request, result, GetCandidateKeyAndValue(result, history),
                  merged_types, &candidate);
    segment->removed_candidates_for_debug_.push_back(std::move(candidate));
  };
#define MOZC_ADD_DEBUG_CANDIDATE(result, log) \
  add_debug_candidate(result, MOZC_WORD_LOG_MESSAGE(log))

#else  // MOZC_DEBUG
#define MOZC_ADD_DEBUG_CANDIDATE(result, log) \
  {                                           \
  }

#endif  // MOZC_DEBUG

  std::vector<absl::Nonnull<const Result *>> final_results_ptrs;
  final_results_ptrs.reserve(result_ptrs.size());
  std::shared_ptr<Result> prev_top_result;

  for (size_t i = 0; i < result_ptrs.size(); ++i) {
    std::pop_heap(result_ptrs.begin(), result_ptrs.end() - i, min_heap_cmp);
    const Result &result = *result_ptrs[result_ptrs.size() - i - 1];

    if (final_results_ptrs.size() >= max_candidates_size ||
        result.cost >= kInfinity) {
      break;
    }

    if (i == 0 && (prev_top_result = MaybeGetPreviousTopResult(
                       result, request, *segments)) != nullptr) {
      final_results_ptrs.emplace_back(prev_top_result.get());
    }

    std::string log_message;
    if (filter.ShouldRemove(result, final_results_ptrs.size(), &log_message)) {
      MOZC_ADD_DEBUG_CANDIDATE(result, log_message);
      continue;
    }

    final_results_ptrs.emplace_back(&result);
  }

  MaybeRerankAggressiveTypingCorrection(request, *segments,
                                        &final_results_ptrs);

  // Fill segments from final_results_ptrs.
  for (const Result *result : final_results_ptrs) {
    FillCandidate(request, *result, GetCandidateKeyAndValue(*result, history),
                  merged_types, segment->push_back_candidate());
  }

  // TODO(b/320221782): Add unit tests for MaybeApplyPostCorrection.
  MaybeApplyPostCorrection(request, modules_, segments);

  if (IsDebug(request) && modules_.GetSupplementalModel()) {
    AddRescoringDebugDescription(segments);
  }

  return !final_results_ptrs.empty();
#undef MOZC_ADD_DEBUG_CANDIDATE
}

void DictionaryPredictor::MaybeRerankAggressiveTypingCorrection(
    const ConversionRequest &request, const Segments &segments,
    std::vector<absl::Nonnull<const Result *>> *results) const {
  if (!IsTypingCorrectionEnabled(request) || results->empty()) {
    return;
  }
  const engine::SupplementalModelInterface *supplemental_model =
      modules_.GetSupplementalModel();
  if (supplemental_model == nullptr) return;
  supplemental_model->RerankTypingCorrection(request, segments, results);
}

// static
void DictionaryPredictor::MaybeApplyPostCorrection(
    const ConversionRequest &request, const engine::Modules &modules,
    Segments *segments) {
  // b/363902660:
  // Stop applying post correction when typing correction is disabled.
  // We may want to use other conditions if we want to enable post correction
  // separately.
  if (!IsTypingCorrectionEnabled(request)) {
    return;
  }
  const engine::SupplementalModelInterface *supplemental_model =
      modules.GetSupplementalModel();
  if (supplemental_model == nullptr) {
    return;
  }
  supplemental_model->PostCorrect(request, segments);
}

int DictionaryPredictor::CalculateSingleKanjiCostOffset(
    const ConversionRequest &request, uint16_t rid,
    const absl::string_view input_key, absl::Span<const Result> results,
    absl::flat_hash_map<PrefixPenaltyKey, int> *cache) const {
  // Make a map from reference value to min-cost result.
  // Reference entry:
  //  - single-char REALTIME or UNIGRAM entry
  //  - PREFIX or NUMBER entry
  // cost is calculated with LM cost (with prefix penalty)
  // When the reference is not found, Hiragana (value == key) entry will be used
  // as the fallback.
  absl::flat_hash_map<absl::string_view, int> min_cost_map;
  int fallback_cost = -1;
  for (const auto &result : results) {
    if (result.removed) {
      continue;
    }
    if (!(result.types & (prediction::REALTIME | prediction::UNIGRAM |
                          prediction::PREFIX | prediction::NUMBER))) {
      continue;
    }

    if (result.value == input_key) {
      const int cost = GetLMCost(result, rid);
      if (fallback_cost == -1 || fallback_cost > cost) {
        fallback_cost = cost;
      }
    }

    if (((result.types & (prediction::REALTIME | prediction::UNIGRAM)) &&
         Util::CharsLen(result.value) != 1)) {
      continue;
    }
    int lm_cost = GetLMCost(result, rid);
    if (result.candidate_attributes &
        Segment::Candidate::PARTIALLY_KEY_CONSUMED) {
      lm_cost += CalculatePrefixPenalty(request, input_key, result,
                                        immutable_converter_, cache);
    }
    const auto it = min_cost_map.find(result.value);
    if (it == min_cost_map.end()) {
      min_cost_map[result.value] = lm_cost;
      continue;
    }
    min_cost_map[result.value] = std::min(it->second, lm_cost);
  }

  // Use the wcost of the highest cost to calculate the single kanji cost
  // offset.
  int single_kanji_max_cost = 0;
  for (const auto &it : min_cost_map) {
    single_kanji_max_cost = std::max(single_kanji_max_cost, it.second);
  }
  single_kanji_max_cost = std::max(single_kanji_max_cost, fallback_cost);

  const int single_kanji_transition_cost =
      std::min(connector_.GetTransitionCost(rid, general_symbol_id_),
               connector_.GetTransitionCost(0, general_symbol_id_));
  const int wcost_diff =
      std::max(0, single_kanji_max_cost - single_kanji_transition_cost);
  const int kSingleKanjiPredictionCostOffset = 800;  // ~= 500*ln(5)
  return wcost_diff + kSingleKanjiPredictionCostOffset;
}

DictionaryPredictor::ResultFilter::ResultFilter(
    const ConversionRequest &request, const Segments &segments,
    dictionary::PosMatcher pos_matcher,
    const SuggestionFilter &suggestion_filter)
    : input_key_(segments.conversion_segment(0).key()),
      input_key_len_(Util::CharsLen(input_key_)),
      pos_matcher_(pos_matcher),
      suggestion_filter_(suggestion_filter),
      is_mixed_conversion_(IsMixedConversionEnabled(request.request())),
      auto_partial_suggestion_(
          request_util::IsAutoPartialSuggestionEnabled(request)),
      include_exact_key_(IsMixedConversionEnabled(request.request())),
      is_handwriting_(request_util::IsHandwriting(request)) {
  const KeyValueView history = GetHistoryKeyAndValue(segments);
  strings::Assign(history_key_, history.key);
  strings::Assign(history_value_, history.value);
  exact_bigram_key_ = absl::StrCat(history.key, input_key_);

  suffix_count_ = 0;
  predictive_count_ = 0;
  realtime_count_ = 0;
  prefix_tc_count_ = 0;
  tc_count_ = 0;
}

bool DictionaryPredictor::ResultFilter::ShouldRemove(const Result &result,
                                                     int added_num,
                                                     std::string *log_message) {
  if (result.removed) {
    *log_message = "Removed flag is on";
    return true;
  }

  if (result.cost >= kInfinity) {
    *log_message = "Too large cost";
    return true;
  }

  if (!auto_partial_suggestion_ &&
      (result.candidate_attributes &
       Segment::Candidate::PARTIALLY_KEY_CONSUMED)) {
    *log_message = "Auto partial suggestion disabled";
    return true;
  }

  // When |include_exact_key| is true, we don't filter the results
  // which have the exactly same key as the input even if it's a bad
  // suggestion.
  if (!(include_exact_key_ && (result.key == input_key_)) &&
      suggestion_filter_.IsBadSuggestion(result.value)) {
    *log_message = "Bad suggestion";
    return true;
  }

  if (is_handwriting_) {
    // Only unigram results are appended for handwriting and we do not need to
    // apply filtering.
    return false;
  }

  // Don't suggest exactly the same candidate as key.
  // if |include_exact_key| is true, that's not the case.
  if (!include_exact_key_ && !(result.types & PredictionType::REALTIME) &&
      (((result.types & PredictionType::BIGRAM) &&
        exact_bigram_key_ == result.value) ||
       (!(result.types & PredictionType::BIGRAM) &&
        input_key_ == result.value))) {
    *log_message = "Key == candidate";
    return true;
  }

  const KeyValueView candidate =
      GetCandidateKeyAndValue(result, {history_key_, history_value_});

  if (seen_.contains(candidate.value)) {
    *log_message = "Duplicated";
    return true;
  }

  // User input: "おーすとり" (len = 5)
  // key/value:  "おーすとりら" "オーストラリア" (miss match pos = 4)
  if ((result.candidate_attributes & Segment::Candidate::SPELLING_CORRECTION) &&
      candidate.key != input_key_ &&
      input_key_len_ <=
          GetMissSpelledPosition(candidate.key, candidate.value) + 1) {
    *log_message = "Spelling correction";
    return true;
  }

  if ((result.types & PredictionType::SUFFIX) && suffix_count_++ >= 20) {
    // TODO(toshiyuki): Need refactoring for controlling suffix
    // prediction number after we will fix the appropriate number.
    *log_message = "Added suffix >= 20";
    return true;
  }

  if (!is_mixed_conversion_) {
    return CheckDupAndReturn(candidate.value, result, log_message);
  }

  // Suppress long candidates to show more candidates in the candidate view.
  const size_t lookup_key_len = Util::CharsLen(
      GetCandidateOriginalLookupKey(input_key_, result, history_key_));
  const size_t candidate_key_len = Util::CharsLen(candidate.key);
  if (lookup_key_len > 0 &&  // Do not filter for zero query
      lookup_key_len < candidate_key_len &&
      (predictive_count_++ >= 3 || added_num >= 10)) {
    *log_message = absl::StrCat("Added predictive (",
                                GetPredictionTypeDebugString(result.types),
                                ") >= 3 || added >= 10");
    return true;
  }
  if ((result.types & PredictionType::REALTIME) &&
      // Do not remove one-segment / on-char realtime candidates
      // example:
      // - "勝った" for the reading, "かった".
      // - "勝" for the reading, "かつ".
      result.inner_segment_boundary.size() >= 2 &&
      Util::CharsLen(result.value) != 1 &&
      (realtime_count_++ >= 3 || added_num >= 5)) {
    *log_message = "Added realtime >= 3 || added >= 5";
    return true;
  }

  constexpr int kTcMaxCount = 3;
  constexpr int kTcMaxRank = 10;

  if ((result.types & PredictionType::TYPING_CORRECTION) &&
      (tc_count_++ >= kTcMaxCount || added_num >= kTcMaxRank)) {
    *log_message = absl::StrCat("Added typing correction >= ", kTcMaxCount,
                                " || added >= ", kTcMaxRank);
    return true;
  }
  if ((result.types & PredictionType::PREFIX) &&
      (result.candidate_attributes & Segment::Candidate::TYPING_CORRECTION) &&
      (prefix_tc_count_++ >= 3 || added_num >= 10)) {
    *log_message = "Added prefix typing correction >= 3 || added >= 10";
    return true;
  }

  return CheckDupAndReturn(candidate.value, result, log_message);
}

bool DictionaryPredictor::ResultFilter::CheckDupAndReturn(
    const absl::string_view value, const Result &result,
    std::string *log_message) {
  if (seen_.contains(value)) {
    *log_message = "Duplicated";
    return true;
  }
  seen_.emplace(value);
  return false;
}

void DictionaryPredictor::FillCandidate(
    const ConversionRequest &request, const Result &result,
    const KeyValueView key_value,
    const absl::flat_hash_map<std::string, int32_t> &merged_types,
    Segment::Candidate *candidate) const {
  DCHECK(candidate);

  const bool cursor_at_tail =
      request.has_composer() &&
      request.composer().GetCursor() == request.composer().GetLength();

  strings::Assign(candidate->content_key, key_value.key);
  strings::Assign(candidate->content_value, key_value.value);
  strings::Assign(candidate->key, key_value.key);
  strings::Assign(candidate->value, key_value.value);
  candidate->lid = result.lid;
  candidate->rid = result.rid;
  candidate->wcost = result.wcost;
  candidate->cost = result.cost;
  candidate->attributes = result.candidate_attributes;
  if ((!(candidate->attributes & Segment::Candidate::SPELLING_CORRECTION) &&
       IsLatinInputMode(request)) ||
      (result.types & PredictionType::SUFFIX)) {
    candidate->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
    candidate->attributes |= Segment::Candidate::NO_EXTRA_DESCRIPTION;
  }
  if (candidate->attributes & Segment::Candidate::PARTIALLY_KEY_CONSUMED) {
    candidate->consumed_key_size = result.consumed_key_size;
    // There are two scenarios to reach here.
    // 1. Auto partial suggestion.
    //    e.g. composition わたしのなまえ| -> candidate 私の
    // 2. Partial suggestion.
    //    e.g. composition わたしの|なまえ -> candidate 私の
    // To distinguish auto partial suggestion from (non-auto) partial
    // suggestion, see the cursor position. If the cursor is at the tail
    // of the composition, this is auto partial suggestion.
    if (cursor_at_tail) {
      candidate->attributes |= Segment::Candidate::AUTO_PARTIAL_SUGGESTION;
    }
  }
  candidate->source_info = result.source_info;
  if (result.types & PredictionType::REALTIME) {
    candidate->inner_segment_boundary = result.inner_segment_boundary;
  }
  if (result.types & PredictionType::TYPING_CORRECTION) {
    candidate->attributes |= Segment::Candidate::TYPING_CORRECTION;
  }
  SetDescription(result.types, candidate);
  if (IsDebug(request)) {
    auto it = merged_types.find(result.value);
    SetDebugDescription(it == merged_types.end() ? 0 : it->second, candidate);
    candidate->cost_before_rescoring = result.cost_before_rescoring;
  }
#ifdef MOZC_DEBUG
  absl::StrAppend(&candidate->log, "\n", result.log);
#endif  // MOZC_DEBUG
}

void DictionaryPredictor::SetDescription(PredictionTypes types,
                                         Segment::Candidate *candidate) const {
  if (candidate->description.empty()) {
    single_kanji_dictionary_->GenerateDescription(candidate->value,
                                                  &candidate->description);
  }
}

void DictionaryPredictor::SetDebugDescription(PredictionTypes types,
                                              Segment::Candidate *candidate) {
  std::string debug_desc = GetPredictionTypeDebugString(types);
  // Note that description for TYPING_CORRECTION is omitted
  // because it is appended by SetDescription.
  if (!debug_desc.empty()) {
    AppendDescription(*candidate, debug_desc);
  }
}

std::string DictionaryPredictor::GetPredictionTypeDebugString(
    PredictionTypes types) {
  std::string debug_desc;
  if (types & PredictionType::UNIGRAM) {
    debug_desc.append(1, 'U');
  }
  if (types & PredictionType::BIGRAM) {
    debug_desc.append(1, 'B');
  }
  if (types & PredictionType::REALTIME_TOP) {
    debug_desc.append("R1");
  } else if (types & PredictionType::REALTIME) {
    debug_desc.append(1, 'R');
  }
  if (types & PredictionType::SUFFIX) {
    debug_desc.append(1, 'S');
  }
  if (types & PredictionType::ENGLISH) {
    debug_desc.append(1, 'E');
  }
  if (types & PredictionType::TYPING_CORRECTION) {
    debug_desc.append(1, 'T');
  }
  if (types & PredictionType::TYPING_COMPLETION) {
    debug_desc.append(1, 'C');
  }
  if (types & PredictionType::SUPPLEMENTAL_MODEL) {
    debug_desc.append(1, 'X');
  }
  if (types & PredictionType::KEY_EXPANDED_IN_DICTIONARY) {
    debug_desc.append(1, 'K');
  }
  return debug_desc;
}

// Returns cost for |result| when it's transitioned from |rid|.  Suffix penalty
// is also added for non-realtime results.
int DictionaryPredictor::GetLMCost(const Result &result, int rid) const {
  const int cost_with_context = connector_.GetTransitionCost(rid, result.lid);

  int lm_cost = 0;

  if (result.types & PredictionType::SUFFIX) {
    // We always respect the previous context to calculate the cost of SUFFIX.
    // Otherwise, the suffix that doesn't match the context will be promoted.
    lm_cost = cost_with_context + result.wcost;
  } else {
    // Sometimes transition cost is too high and causes a bug like b/18112966.
    // For example, "接続詞 が" -> "始まる 動詞,五段活用,基本形" has very large
    // cost and "始まる" is demoted.  To prevent such cases, ImmutableConverter
    // computes transition from BOS/EOS too; see
    // ImmutableConverterImpl::MakeLatticeNodesForHistorySegments().
    // Here, taking the minimum of |cost1| and |cost2| has a similar effect.
    const int cost_without_context =
        connector_.GetTransitionCost(0, result.lid);
    lm_cost = std::min(cost_with_context, cost_without_context) + result.wcost;
  }

  if (!(result.types & PredictionType::REALTIME)) {
    // Realtime conversion already adds prefix/suffix penalties to the result.
    // Note that we don't add prefix penalty the role of "bunsetsu" is
    // ambiguous on zero-query suggestion.
    lm_cost += segmenter_->GetSuffixPenalty(result.rid);
  }

  return lm_cost;
}

void DictionaryPredictor::SetPredictionCost(
    ConversionRequest::RequestType request_type, const Segments &segments,
    std::vector<Result> *results) const {
  DCHECK(results);
  int rid = 0;  // 0 (BOS) is default
  if (segments.history_segments_size() > 0) {
    const Segment &history_segment =
        segments.history_segment(segments.history_segments_size() - 1);
    if (history_segment.candidates_size() > 0) {
      rid = history_segment.candidate(0).rid;  // use history segment's id
    }
  }

  const std::string &input_key = segments.conversion_segment(0).key();
  const KeyValueView history = GetHistoryKeyAndValue(segments);
  const std::string bigram_key = absl::StrCat(history.key, history.key);
  const bool is_suggestion = (request_type == ConversionRequest::SUGGESTION);

  // use the same scoring function for both unigram/bigram.
  // Bigram will be boosted because we pass the previous
  // key as a context information.
  const size_t bigram_key_len = Util::CharsLen(bigram_key);
  const size_t unigram_key_len = Util::CharsLen(input_key);

  for (size_t i = 0; i < results->size(); ++i) {
    const Result &result = (*results)[i];
    const int cost = GetLMCost(result, rid);
    const size_t query_len = (result.types & PredictionType::BIGRAM)
                                 ? bigram_key_len
                                 : unigram_key_len;
    const size_t key_len = Util::CharsLen(result.key);

    if (IsAggressiveSuggestion(query_len, key_len, cost, is_suggestion,
                               results->size())) {
      (*results)[i].cost = kInfinity;
      continue;
    }

    // cost = -500 * log(lang_prob(w) * (1 + remain_length))    -- (1)
    // where lang_prob(w) is a language model probability of the word "w", and
    // remain_length the length of key user must type to input "w".
    //
    // Example:
    // key/value = "とうきょう/東京"
    // user_input = "とう"
    // remain_length = len("とうきょう") - len("とう") = 3
    //
    // By taking the log of (1),
    // cost  = -500 [log(lang_prob(w)) + log(1 + ramain_length)]
    //       = -500 * log(lang_prob(w)) + 500 * log(1 + remain_length)
    //       = cost - 500 * log(1 + remain_length)
    // Because 500 * log(lang_prob(w)) = -cost.
    //
    // lang_prob(w) * (1 + remain_length) represents how user can reduce
    // the total types by choosing this candidate.
    // Before this simple algorithm, we have been using an SVM-base scoring,
    // but we stop usign it with the following reasons.
    // 1) Hard to maintain the ranking.
    // 2) Hard to control the final results of SVM.
    // 3) Hard to debug.
    // 4) Since we used the log(remain_length) as a feature,
    //    the new ranking algorithm and SVM algorithm was essentially
    //    the same.
    // 5) Since we used the length of value as a feature, we find
    //    inconsistencies between the conversion and the prediction
    //    -- the results of top prediction and the top conversion
    //    (the candidate shown after the space key) may differ.
    //
    // The new function brings consistent results. If two candidate
    // have the same reading (key), they should have the same cost bonus
    // from the length part. This implies that the result is reranked by
    // the language model probability as long as the key part is the same.
    // This behavior is basically the same as the converter.
    //
    // TODO(team): want find the best parameter instead of kCostFactor.
    constexpr int kCostFactor = 500;
    (*results)[i].cost =
        cost - kCostFactor * log(1.0 + std::max<int>(0, key_len - query_len));
  }

  MaybeFixRealtimeTopCost(input_key, *results);
}

void DictionaryPredictor::SetPredictionCostForMixedConversion(
    const ConversionRequest &request, const Segments &segments,
    std::vector<Result> *results) const {
  DCHECK(results);

  // ranking for mobile
  int rid = 0;        // 0 (BOS) is default
  int prev_cost = 0;  // cost of the last history candidate.

  if (segments.history_segments_size() > 0) {
    const Segment &history_segment =
        segments.history_segment(segments.history_segments_size() - 1);
    if (history_segment.candidates_size() > 0) {
      rid = history_segment.candidate(0).rid;  // use history segment's id
      prev_cost = history_segment.candidate(0).cost;
      if (prev_cost == 0) {
        // if prev_cost is set to be 0 for some reason, use default cost.
        prev_cost = 5000;
      }
    }
  }

  absl::flat_hash_map<PrefixPenaltyKey, int32_t> prefix_penalty_cache;
  const std::string &input_key = segments.conversion_segment(0).key();
  const int single_kanji_offset = CalculateSingleKanjiCostOffset(
      request, rid, input_key, *results, &prefix_penalty_cache);

  const KeyValueView history = GetHistoryKeyAndValue(segments);

  for (Result &result : *results) {
    int cost = GetLMCost(result, rid);
    MOZC_WORD_LOG(result, absl::StrCat("GetLMCost: ", cost));
    if (result.lid == result.rid && !pos_matcher_.IsSuffixWord(result.rid) &&
        !pos_matcher_.IsFunctional(result.rid) &&
        !pos_matcher_.IsWeakCompoundVerbSuffix(result.rid)) {
      // TODO(b/242683049): It's better to define a new POS matcher rule once
      // we decide to make this behavior default.
      // Implementation note:
      // Suffix penalty is added in the above GetLMCost(), which is also used
      // for calculating non-mobile prediction cost. So we modify the cost
      // calculation here for now.
      cost -= segmenter_->GetSuffixPenalty(result.rid);
      MOZC_WORD_LOG(result, absl::StrCat("Cancel Suffix Penalty: ", cost));
    }

    // Demote filtered word here, because they are not filtered for exact match.
    // Even for exact match, we don't want to show aggressive words with high
    // ranking.
    if (suggestion_filter_.IsBadSuggestion(result.value)) {
      // Cost penalty means for bad suggestion.
      // 3453 = 500 * log(1000)
      constexpr int kBadSuggestionPenalty = 3453;
      cost += kBadSuggestionPenalty;
      MOZC_WORD_LOG(result, absl::StrCat("BadSuggestionPenalty: ", cost));
    }

    if (result.types & PredictionType::BIGRAM) {
      // When user inputs "六本木" and there is an entry
      // "六本木ヒルズ" in the dictionary, we can suggest
      // "ヒルズ" as a ZeroQuery suggestion. In this case,
      // We can't calcurate the transition cost between "六本木"
      // and "ヒルズ". If we ignore the transition cost,
      // bigram-based suggestion will be overestimated.
      // Here we use kDefaultTransitionCost as an
      // transition cost between "六本木" and "ヒルズ". Currently,
      // the cost is basically the same as the cost between
      // "名詞,一般" and "名詞,一般".
      // TODO(taku): Adjust these parameters.
      // Seems the bigram is overestimated.
      constexpr int kDefaultTransitionCost = 1347;
      // Promoting bigram candidates.
      constexpr int kBigramBonus = 800;  // ~= 500*ln(5)
      cost += (kDefaultTransitionCost - kBigramBonus - prev_cost);
      // The bonus can make the cost negative and promotes bigram results
      // too much.
      // Align the cost here before applying other cost modifications.
      cost = std::max(1, cost);
      MOZC_WORD_LOG(result, absl::StrCat("Bigram: ", cost));
    }

    if (result.types & PredictionType::SINGLE_KANJI) {
      cost += single_kanji_offset;
      if (cost <= 0) {
        // single_kanji_offset can be negative.
        // cost should be larger than 0.
        cost = result.wcost + 1;
      }
      MOZC_WORD_LOG(result, absl::StrCat("SingleKanji: ", cost));
    }

    if (result.candidate_attributes & Segment::Candidate::USER_DICTIONARY &&
        result.lid != general_symbol_id_) {
      // Decrease cost for words from user dictionary in order to promote them,
      // provided that it is not a general symbol (Note: emoticons are mapped to
      // general symbol).  Currently user dictionary words are evaluated 5 times
      // bigger in frequency, being capped by 1000 (this number is adhoc, so
      // feel free to adjust).
      constexpr int kUserDictionaryPromotionFactor = 804;  // 804 = 500 * log(5)
      constexpr int kUserDictionaryCostUpperLimit = 1000;
      cost = std::min(cost - kUserDictionaryPromotionFactor,
                      kUserDictionaryCostUpperLimit);
      MOZC_WORD_LOG(result, absl::StrCat("User dictionary: ", cost));
    }

    // Demote predictive results for making exact candidates to have higher
    // ranking.
    // - Users expect the candidates for the input key on the candidates.
    // - We want to show candidates as many as possible in the limited
    //   candidates area.
    const size_t candidate_lookup_key_len = Util::CharsLen(
        GetCandidateOriginalLookupKey(input_key, result, history.key));
    const size_t candidate_key_len =
        Util::CharsLen(GetCandidateKey(result, history.key));
    if (!(result.types & PredictionType::SUFFIX) &&
        candidate_key_len > candidate_lookup_key_len) {
      const size_t predicted_key_len =
          candidate_key_len - candidate_lookup_key_len;
      // -500 * log(prob)
      // See also: mozc/converter/candidate_filter.cc
      const int predictive_penalty = 500 * log(50 * predicted_key_len);
      cost += predictive_penalty;
      MOZC_WORD_LOG(result, absl::StrCat("Predictive: ", cost));
    }
    // Penalty for prefix results.
    if (result.candidate_attributes &
        Segment::Candidate::PARTIALLY_KEY_CONSUMED) {
      const int prefix_penalty =
          CalculatePrefixPenalty(request, input_key, result,
                                 immutable_converter_, &prefix_penalty_cache);
      result.penalty += prefix_penalty;
      cost += prefix_penalty;
      MOZC_WORD_LOG(result, absl::StrCat("Prefix: ", cost,
                                         "; prefix penalty: ", prefix_penalty));
    }

    // Note that the cost is defined as -500 * log(prob).
    // Even after the ad hoc manipulations, cost must remain larger than 0.
    result.cost = std::max(1, cost);
    MOZC_WORD_LOG(result, absl::StrCat("SetPredictionCost: ", result.cost));
  }

  MaybeFixRealtimeTopCost(input_key, *results);
}

// static
size_t DictionaryPredictor::GetMissSpelledPosition(
    const absl::string_view key, const absl::string_view value) {
  const std::string hiragana_value = japanese::KatakanaToHiragana(value);
  // value is mixed type. return true if key == request_key.
  if (Util::GetScriptType(hiragana_value) != Util::HIRAGANA) {
    return Util::CharsLen(key);
  }

  // Find the first position of character where miss spell occurs.
  int position = 0;
  ConstChar32Iterator key_iter(key);
  for (ConstChar32Iterator hiragana_iter(hiragana_value);
       !hiragana_iter.Done() && !key_iter.Done();
       hiragana_iter.Next(), key_iter.Next(), ++position) {
    if (hiragana_iter.Get() != key_iter.Get()) {
      return position;
    }
  }

  // not find. return the length of key.
  while (!key_iter.Done()) {
    ++position;
    key_iter.Next();
  }

  return position;
}

// static
void DictionaryPredictor::RemoveMissSpelledCandidates(
    size_t request_key_len, std::vector<Result> *results) {
  DCHECK(results);

  if (results->size() <= 1) {
    return;
  }

  int spelling_correction_size = 5;
  for (size_t i = 0; i < results->size(); ++i) {
    const Result &result = (*results)[i];
    if (!(result.candidate_attributes &
          Segment::Candidate::SPELLING_CORRECTION)) {
      continue;
    }

    // Only checks at most 5 spelling corrections to avoid the case
    // like all candidates have SPELLING_CORRECTION.
    if (--spelling_correction_size == 0) {
      return;
    }

    std::vector<size_t> same_key_index, same_value_index;
    for (size_t j = 0; j < results->size(); ++j) {
      if (i == j) {
        continue;
      }
      const Result &target_result = (*results)[j];
      if (target_result.candidate_attributes &
          Segment::Candidate::SPELLING_CORRECTION) {
        continue;
      }
      if (target_result.key == result.key) {
        same_key_index.push_back(j);
      }
      if (target_result.value == result.value) {
        same_value_index.push_back(j);
      }
    }

    // delete same_key_index and same_value_index
    if (!same_key_index.empty() && !same_value_index.empty()) {
      (*results)[i].removed = true;
      MOZC_WORD_LOG((*results)[i], "Removed. same_(key|value)_index.");
      for (const size_t k : same_key_index) {
        (*results)[k].removed = true;
        MOZC_WORD_LOG((*results)[k], "Removed. same_(key|value)_index.");
      }
    } else if (same_key_index.empty() && !same_value_index.empty()) {
      (*results)[i].removed = true;
      MOZC_WORD_LOG((*results)[i], "Removed. same_value_index.");
    } else if (!same_key_index.empty() && same_value_index.empty()) {
      for (const size_t k : same_key_index) {
        (*results)[k].removed = true;
        MOZC_WORD_LOG((*results)[k], "Removed. same_key_index.");
      }
      if (request_key_len <= GetMissSpelledPosition(result.key, result.value)) {
        (*results)[i].removed = true;
        MOZC_WORD_LOG((*results)[i], "Removed. Invalid MissSpelledPosition.");
      }
    }
  }
}

// static
bool DictionaryPredictor::IsAggressiveSuggestion(size_t query_len,
                                                 size_t key_len, int cost,
                                                 bool is_suggestion,
                                                 size_t total_candidates_size) {
  // Temporal workaround for fixing the problem where longer sentence-like
  // suggestions are shown when user input is very short.
  // "ただしい" => "ただしいけめんにかぎる"
  // "それでもぼ" => "それでもぼくはやっていない".
  // If total_candidates_size is small enough, we don't perform
  // special filtering. e.g., "せんとち" has only two candidates, so
  // showing "千と千尋の神隠し" is OK.
  // Also, if the cost is too small (< 5000), we allow to display
  // long phrases. Examples include "よろしくおねがいします".
  if (is_suggestion && total_candidates_size >= 10 && key_len >= 8 &&
      cost >= 5000 && query_len <= static_cast<size_t>(0.4 * key_len)) {
    return true;
  }

  return false;
}

int DictionaryPredictor::CalculatePrefixPenalty(
    const ConversionRequest &request, const absl::string_view input_key,
    const Result &result,
    const ImmutableConverterInterface *immutable_converter,
    absl::flat_hash_map<PrefixPenaltyKey, int> *cache) const {
  if (input_key == result.key) {
    LOG(WARNING) << "Invalid prefix key: " << result.key;
    return 0;
  }
  const std::string &candidate_key = result.key;
  const uint16_t result_rid = result.rid;
  const size_t key_len = Util::CharsLen(candidate_key);
  const PrefixPenaltyKey cache_key = std::make_pair(result_rid, key_len);
  if (const auto it = cache->find(cache_key); it != cache->end()) {
    return it->second;
  }

  // Use the conversion result's cost for the remaining input key
  // as the penalty of the prefix candidate.
  // For example, if the input key is "きょうの" and the target prefix candidate
  // is "木:き", the penalty will be the cost of the conversion result for
  // "ょうの".
  int penalty = 0;
  Segments tmp_segments;
  tmp_segments.add_segment()->set_key(Util::Utf8SubString(input_key, key_len));

  ConversionRequest::Options options = request.options();
  options.max_conversion_candidates_size = 1;
  // Explicitly request conversion result for the entire key.
  options.create_partial_candidates = false;
  options.kana_modifier_insensitive_conversion = false;
  const ConversionRequest req = ConversionRequestBuilder()
                                    .SetConversionRequest(request)
                                    .SetOptions(std::move(options))
                                    .Build();

  if (immutable_converter->ConvertForRequest(req, &tmp_segments) &&
      tmp_segments.segment(0).candidates_size() > 0) {
    const Segment::Candidate &top_candidate =
        tmp_segments.segment(0).candidate(0);
    penalty = (connector_.GetTransitionCost(result_rid, top_candidate.lid) +
               top_candidate.cost);
  }
  // ConvertForRequest() can return placeholder candidate with cost 0 when it
  // failed to generate candidates.
  if (penalty <= 0) {
    penalty = kInfinity;
  }
  constexpr int kPrefixCandidateCostOffset = 1151;  // 500 * log(10)
  // TODO(toshiyuki): Optimize the cost offset.
  penalty += kPrefixCandidateCostOffset;
  (*cache)[cache_key] = penalty;
  return penalty;
}

void DictionaryPredictor::MaybeRescoreResults(
    const ConversionRequest &request, const Segments &segments,
    absl::Span<Result> results) const {
  if (request_util::IsHandwriting(request)) {
    // We want to fix the first candidate for handwriting request.
    return;
  }

  if (IsDebug(request)) {
    for (Result &r : results) r.cost_before_rescoring = r.cost;
  }

  if (const engine::SupplementalModelInterface *const supplemental_model =
          modules_.GetSupplementalModel();
      supplemental_model != nullptr) {
    supplemental_model->RescoreResults(request, segments, results);
  }
}

void DictionaryPredictor::AddRescoringDebugDescription(Segments *segments) {
  if (segments->conversion_segments_size() == 0) {
    return;
  }
  Segment *seg = segments->mutable_conversion_segment(0);
  if (seg->candidates_size() == 0) {
    return;
  }
  // Calculate the ranking by the original costs. Note: this can be slightly
  // different from the actual ranking because, when the candidates were
  // generated, `filter.ShouldRemove()` was applied to the results ordered by
  // the rescored costs. To get the true original ranking, we need to apply
  // `filter.ShouldRemove()` to the results ordered by the original cost.
  // This is just for debugging, so such difference won't matter.
  std::vector<const Segment::Candidate *> cands;
  cands.reserve(seg->candidates_size());
  for (int i = 0; i < seg->candidates_size(); ++i) {
    cands.push_back(&seg->candidate(i));
  }
  std::sort(cands.begin(), cands.end(),
            [](const Segment::Candidate *l, const Segment::Candidate *r) {
              return l->cost_before_rescoring < r->cost_before_rescoring;
            });
  absl::flat_hash_map<const Segment::Candidate *, size_t> orig_rank;
  for (size_t i = 0; i < cands.size(); ++i) orig_rank[cands[i]] = i + 1;

  // Populate the debug description.
  for (size_t i = 0; i < seg->candidates_size(); ++i) {
    Segment::Candidate *c = seg->mutable_candidate(i);
    const size_t rank = i + 1;
    AppendDescription(*c, orig_rank[c], "→", rank);
  }
}

std::shared_ptr<Result> DictionaryPredictor::MaybeGetPreviousTopResult(
    const Result &current_top_result, const ConversionRequest &request,
    const Segments &segments) const {
  const int32_t max_diff = request.request()
                               .decoder_experiment_params()
                               .candidate_consistency_cost_max_diff();
  if (max_diff == 0 || segments.conversion_segments_size() <= 0) {
    return nullptr;
  }

  auto prev_top_result = std::atomic_load(&prev_top_result_);

  // Updates the key length.
  const int cur_top_key_length = segments.conversion_segment(0).key().size();
  const int prev_top_key_length = prev_top_key_length_.exchange(
      cur_top_key_length);  // returns the old value.

  // Insert condition.
  // 1. prev key length < current key length (incrementally character is added)
  // 2. cost diff is less than max_diff.
  // 3. current key is shorter than previous key.
  // 4. current key is the prefix of previous key.
  // 5. current result is not a partial suggestion.
  if (prev_top_result && cur_top_key_length >= prev_top_key_length &&
      std::abs(current_top_result.cost - prev_top_result->cost) < max_diff &&
      current_top_result.key.size() < prev_top_result->key.size() &&
      !(current_top_result.types & PREFIX) &&
      absl::StartsWith(prev_top_result->key, current_top_result.key)) {
    // Do not need to remember the previous key as `prev_top_result` is still
    // top result.
    return prev_top_result;
  }

  // Remembers the top result.
  std::atomic_store(&prev_top_result_,
                    std::make_shared<Result>(current_top_result));

  return nullptr;
}

}  // namespace mozc::prediction

#undef MOZC_WORD_LOG_MESSAGE
#undef MOZC_WORD_LOG
