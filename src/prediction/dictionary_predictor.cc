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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/japanese_util.h"
#include "base/logging.h"
#include "base/util.h"
#include "composer/composer.h"
#include "converter/connector.h"
#include "converter/converter_interface.h"
#include "converter/immutable_converter_interface.h"
#include "converter/segmenter.h"
#include "converter/segments.h"
#include "data_manager/data_manager_interface.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_matcher.h"
#include "prediction/dictionary_prediction_aggregator.h"
#include "prediction/predictor_interface.h"
#include "prediction/result.h"
#include "prediction/suggestion_filter.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "transliteration/transliteration.h"
#include "usage_stats/usage_stats.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
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
namespace {
using ::mozc::commands::Request;
using ::mozc::dictionary::DictionaryInterface;
using ::mozc::dictionary::PosMatcher;
using ::mozc::prediction::PredictionType;
using ::mozc::prediction::ResultCostLess;
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

bool IsEnableNewSpatialScoring(const ConversionRequest &request) {
  return request.request()
      .decoder_experiment_params()
      .enable_new_spatial_scoring();
}

bool IsLatinInputMode(const ConversionRequest &request) {
  return (request.has_composer() &&
          (request.composer().GetInputMode() == transliteration::HALF_ASCII ||
           request.composer().GetInputMode() == transliteration::FULL_ASCII));
}

bool IsMixedConversionEnabled(const Request &request) {
  return request.mixed_conversion();
}

void GetCandidateKeyAndValue(const Result &result,
                             absl::string_view history_key,
                             absl::string_view history_value, std::string *key,
                             std::string *value) {
  if (result.types & PredictionType::BIGRAM) {
    // remove the prefix of history key and history value.
    *key = result.key.substr(history_key.size());
    *value = result.value.substr(history_value.size());
    return;
  }
  *key = result.key;
  *value = result.value;
}

}  // namespace

DictionaryPredictor::DictionaryPredictor(
    const DataManagerInterface &data_manager,
    const ConverterInterface *converter,
    const ImmutableConverterInterface *immutable_converter,
    const DictionaryInterface *dictionary,
    const DictionaryInterface *suffix_dictionary, const Connector *connector,
    const Segmenter *segmenter, const PosMatcher *pos_matcher,
    const SuggestionFilter *suggestion_filter)
    : aggregator_(std::make_unique<prediction::DictionaryPredictionAggregator>(
          data_manager, converter, immutable_converter, dictionary,
          suffix_dictionary, pos_matcher)),
      immutable_converter_(immutable_converter),
      connector_(connector),
      segmenter_(segmenter),
      suggestion_filter_(suggestion_filter),
      general_symbol_id_(pos_matcher->GetGeneralSymbolId()),
      predictor_name_("DictionaryPredictor") {}

DictionaryPredictor::DictionaryPredictor(
    std::unique_ptr<const prediction::PredictionAggregatorInterface> aggregator,
    const ImmutableConverterInterface *immutable_converter,
    const Connector *connector, const Segmenter *segmenter,
    const PosMatcher *pos_matcher, const SuggestionFilter *suggestion_filter)
    : aggregator_(std::move(aggregator)),
      immutable_converter_(immutable_converter),
      connector_(connector),
      segmenter_(segmenter),
      suggestion_filter_(suggestion_filter),
      general_symbol_id_(pos_matcher->GetGeneralSymbolId()),
      predictor_name_("DictionaryPredictorForTest") {}

DictionaryPredictor::~DictionaryPredictor() = default;

void DictionaryPredictor::Finish(const ConversionRequest &request,
                                 Segments *segments) {
  if (request.request_type() == ConversionRequest::REVERSE_CONVERSION) {
    // Do nothing for REVERSE_CONVERSION.
    return;
  }

  const Segment &segment = segments->conversion_segment(0);
  if (segment.candidates_size() < 1) {
    VLOG(2) << "candidates size < 1";
    return;
  }

  const Segment::Candidate &candidate = segment.candidate(0);
  if (segment.segment_type() != Segment::FIXED_VALUE) {
    VLOG(2) << "segment is not FIXED_VALUE" << candidate.value;
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
    VLOG(2) << "request type is CONVERSION";
    return false;
  }
  if (segments->conversion_segments_size() < 1) {
    VLOG(2) << "segment size < 1";
    return false;
  }

  std::vector<Result> results;
  // Mixed conversion is the feature that mixes prediction and
  // conversion, meaning that results may include the candidates whose
  // key is exactly the same as the composition.  This mode is used in mobile.
  const bool is_mixed_conversion = IsMixedConversionEnabled(request.request());
  aggregator_->AggregatePredictionForRequest(request, *segments, &results);
  if (results.empty()) {
    return false;
  }

  if (is_mixed_conversion) {
    SetPredictionCostForMixedConversion(request, *segments, &results);
    if (!IsEnableNewSpatialScoring(request)) {
      ApplyPenaltyForKeyExpansion(*segments, &results);
    }
    // Currently, we don't have spelling correction feature when in
    // the mixed conversion mode, so RemoveMissSpelledCandidates() is
    // not called.
    return AddPredictionToCandidates(request, segments, &results);
  }

  // Normal prediction.
  SetPredictionCost(request.request_type(), *segments, &results);
  if (!IsEnableNewSpatialScoring(request)) {
    ApplyPenaltyForKeyExpansion(*segments, &results);
  }
  const std::string &input_key = segments->conversion_segment(0).key();
  const size_t input_key_len = Util::CharsLen(input_key);
  RemoveMissSpelledCandidates(input_key_len, &results);
  return AddPredictionToCandidates(request, segments, &results);
}

bool DictionaryPredictor::AddPredictionToCandidates(
    const ConversionRequest &request, Segments *segments,
    std::vector<Result> *results) const {
  DCHECK(segments);
  DCHECK(results);

  std::string history_key, history_value;
  GetHistoryKeyAndValue(*segments, &history_key, &history_value);

  Segment *segment = segments->mutable_conversion_segment(0);
  DCHECK(segment);

  // Instead of sorting all the results, we construct a heap.
  // This is done in linear time and
  // we can pop as many results as we need efficiently.
  std::make_heap(results->begin(), results->end(), ResultCostLess());

  const size_t max_candidates_size = std::min(
      request.max_dictionary_prediction_candidates_size(), results->size());

  ResultFilter filter(request, *segments, suggestion_filter_);
  int added = 0;

  // TODO(taku): Sets more advanced debug info depending on the verbose_level.
  absl::flat_hash_map<std::string, int32_t> merged_types;
  if (IsDebug(request)) {
    for (const auto &result : *results) {
      if (!result.removed) {
        merged_types[result.value] |= result.types;
      }
    }
  }

#ifdef MOZC_DEBUG
  auto add_debug_candidate = [&](Result result, const absl::string_view log) {
    std::string key, value;
    GetCandidateKeyAndValue(result, history_key, history_value, &key, &value);
    absl::StrAppend(&result.log, log);
    Segment::Candidate candidate;
    FillCandidate(request, result, key, value, merged_types, &candidate);
    segment->removed_candidates_for_debug_.push_back(std::move(candidate));
  };
#define MOZC_ADD_DEBUG_CANDIDATE(result, log) \
  add_debug_candidate(result, MOZC_WORD_LOG_MESSAGE(log))

#else  // MOZC_DEBUG
#define MOZC_ADD_DEBUG_CANDIDATE(result, log) \
  {}

#endif  // MOZC_DEBUG

  for (size_t i = 0; i < results->size(); ++i) {
    // Pop a result from a heap. Please pay attention not to use results->at(i).
    std::pop_heap(results->begin(), results->end() - i, ResultCostLess());
    const Result &result = results->at(results->size() - i - 1);

    if (added >= max_candidates_size || result.cost >= kInfinity) {
      break;
    }

    std::string log_message;
    if (filter.ShouldRemove(result, added, &log_message)) {
      MOZC_ADD_DEBUG_CANDIDATE(result, log_message);
      continue;
    }

    Segment::Candidate *candidate = segment->push_back_candidate();

    std::string key, value;
    GetCandidateKeyAndValue(result, history_key, history_value, &key, &value);
    FillCandidate(request, result, key, value, merged_types, candidate);
    ++added;
  }

  return added > 0;
#undef MOZC_ADD_DEBUG_CANDIDATE
}

DictionaryPredictor::ResultFilter::ResultFilter(
    const ConversionRequest &request, const Segments &segments,
    const SuggestionFilter *suggestion_filter)
    : input_key_(segments.conversion_segment(0).key()),
      input_key_len_(Util::CharsLen(input_key_)),
      suggestion_filter_(suggestion_filter),
      is_mixed_conversion_(IsMixedConversionEnabled(request.request())),
      include_exact_key_(IsMixedConversionEnabled(request.request())) {
  GetHistoryKeyAndValue(segments, &history_key_, &history_value_);
  exact_bigram_key_ = history_key_ + input_key_;

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

  // When |include_exact_key| is true, we don't filter the results
  // which have the exactly same key as the input even if it's a bad
  // suggestion.
  if (!(include_exact_key_ && (result.key == input_key_)) &&
      suggestion_filter_->IsBadSuggestion(result.value)) {
    *log_message = "Bad suggestion";
    return true;
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

  std::string key, value;
  GetCandidateKeyAndValue(result, history_key_, history_value_, &key, &value);

  // User input: "おーすとり" (len = 5)
  // key/value:  "おーすとりら" "オーストラリア" (miss match pos = 4)
  if ((result.candidate_attributes & Segment::Candidate::SPELLING_CORRECTION) &&
      key != input_key_ &&
      input_key_len_ <= GetMissSpelledPosition(key, value) + 1) {
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
    return CheckDupAndReturn(value, log_message);
  }

  // Suppress long candidates to show more candidates in the candidate view.
  if (input_key_len_ > 0 &&  // Do not filter for zero query
      (input_key_len_ < Util::CharsLen(result.key)) &&
      (predictive_count_++ >= 3 || added_num >= 10)) {
    *log_message = absl::StrCat("Added predictive (",
                                GetPredictionTypeDebugString(result.types),
                                ") >= 3 || added >= 10");
    return true;
  }
  if ((result.types & PredictionType::REALTIME) &&
      // Do not remove one-segment realtime candidates
      // example:
      // "勝った" for the reading, "かった".
      result.inner_segment_boundary.size() != 1 &&
      (realtime_count_++ >= 3 || added_num >= 5)) {
    *log_message = "Added realtime >= 3 || added >= 5";
    return true;
  }
  if ((result.types & PredictionType::TYPING_CORRECTION) &&
      (tc_count_++ >= 3 || added_num >= 10)) {
    *log_message = "Added typing correction >= 3 || added >= 10";
    return true;
  }
  if ((result.types & PredictionType::PREFIX) &&
      (result.candidate_attributes & Segment::Candidate::TYPING_CORRECTION) &&
      (prefix_tc_count_++ >= 3 || added_num >= 10)) {
    *log_message = "Added prefix typing correction >= 3 || added >= 10";
    return true;
  }

  return CheckDupAndReturn(value, log_message);
}

bool DictionaryPredictor::ResultFilter::CheckDupAndReturn(
    const std::string &value, std::string *log_message) {
  if (!seen_.insert(value).second) {
    *log_message = "Duplicated";
    return true;
  }
  return false;
}

void DictionaryPredictor::FillCandidate(
    const ConversionRequest &request, const Result &result,
    const std::string &key, const std::string &value,
    const absl::flat_hash_map<std::string, int32_t> &merged_types,
    Segment::Candidate *candidate) const {
  DCHECK(candidate);

  const bool cursor_at_tail =
      request.has_composer() &&
      request.composer().GetCursor() == request.composer().GetLength();

  candidate->Init();
  candidate->content_key = key;
  candidate->content_value = value;
  candidate->key = key;
  candidate->value = value;
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

  SetDescription(result.types, &candidate->description);
  if (IsDebug(request)) {
    auto it = merged_types.find(result.value);
    SetDebugDescription(it == merged_types.end() ? 0 : it->second,
                        &candidate->description);
  }
#ifdef MOZC_DEBUG
  candidate->log += "\n" + result.log;
#endif  // MOZC_DEBUG
}

void DictionaryPredictor::SetDescription(PredictionTypes types,
                                         std::string *description) {
  if (types & PredictionType::TYPING_CORRECTION) {
    Util::AppendStringWithDelimiter(" ", "補正", description);
  }
}

void DictionaryPredictor::SetDebugDescription(PredictionTypes types,
                                              std::string *description) {
  std::string debug_desc = GetPredictionTypeDebugString(types);
  // Note that description for TYPING_CORRECTION is omitted
  // because it is appended by SetDescription.
  if (!debug_desc.empty()) {
    Util::AppendStringWithDelimiter(" ", debug_desc, description);
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
  return debug_desc;
}

// Returns cost for |result| when it's transitioned from |rid|.  Suffix penalty
// is also added for non-realtime results.
int DictionaryPredictor::GetLMCost(const Result &result, int rid) const {
  const int cost_with_context = connector_->GetTransitionCost(rid, result.lid);

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
        connector_->GetTransitionCost(0, result.lid);
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
  std::string history_key, history_value;
  GetHistoryKeyAndValue(segments, &history_key, &history_value);
  const std::string bigram_key = absl::StrCat(history_key, input_key);
  const bool is_suggestion = (request_type == ConversionRequest::SUGGESTION);

  // use the same scoring function for both unigram/bigram.
  // Bigram will be boosted because we pass the previous
  // key as a context information.
  const size_t bigram_key_len = Util::CharsLen(bigram_key);
  const size_t unigram_key_len = Util::CharsLen(input_key);

  // In the loop below, we track the minimum cost among those REALTIME
  // candidates that have the same key length as |input_key| so that we can set
  // a slightly smaller cost to REALTIME_TOP than these.
  int realtime_cost_min = kInfinity;
  Result *realtime_top_result = nullptr;

  for (size_t i = 0; i < results->size(); ++i) {
    const Result &result = results->at(i);

    // The cost of REALTIME_TOP is determined after the loop based on the
    // minimum cost for REALTIME. Just remember the pointer of result.
    if (result.types & PredictionType::REALTIME_TOP) {
      realtime_top_result = &results->at(i);
    }

    const int cost = GetLMCost(result, rid);
    const size_t query_len = (result.types & PredictionType::BIGRAM)
                                 ? bigram_key_len
                                 : unigram_key_len;
    const size_t key_len = Util::CharsLen(result.key);

    if (IsAggressiveSuggestion(query_len, key_len, cost, is_suggestion,
                               results->size())) {
      results->at(i).cost = kInfinity;
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
    // This behavior is baisically the same as the converter.
    //
    // TODO(team): want find the best parameter instead of kCostFactor.
    constexpr int kCostFactor = 500;
    results->at(i).cost =
        cost - kCostFactor * log(1.0 + std::max<int>(0, key_len - query_len));

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
  if (realtime_top_result != nullptr) {
    realtime_top_result->cost = std::max(0, realtime_cost_min - 10);
  }
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

  const std::string &input_key = segments.conversion_segment(0).key();

  absl::flat_hash_map<PrefixPenaltyKey, int32_t> prefix_penalty_cache;

  for (Result &result : *results) {
    int cost = GetLMCost(result, rid);
    MOZC_WORD_LOG(result, absl::StrCat("GetLMCost: ", cost));

    // Demote filtered word here, because they are not filtered for exact match.
    // Even for exact match, we don't want to show aggressive words with high
    // ranking.
    if (suggestion_filter_->IsBadSuggestion(result.value)) {
      // Cost penalty means for bad suggestion.
      // 3453 = 500 * log(1000)
      constexpr int kBadSuggestionPenalty = 3453;
      cost += kBadSuggestionPenalty;
      MOZC_WORD_LOG(result, absl::StrCat("BadSuggestionPenalty: ", cost));
    }

    // Make exact candidates to have higher ranking.
    // Because for mobile, suggestion is the main candidates and
    // users expect the candidates for the input key on the candidates.
    if (result.types &
        (PredictionType::UNIGRAM | PredictionType::TYPING_CORRECTION)) {
      const size_t input_key_len = Util::CharsLen(input_key);
      const size_t key_len = Util::CharsLen(result.key);
      if (key_len > input_key_len) {
        // Skip to add additional penalty for TYPING_CORRECTION because
        // query cost is already added for them.
        // (DictionaryPredictor::GetPredictiveResultsUsingTypingCorrection)
        //
        // Without this handling, a lot of TYPING_CORRECTION candidates
        // can be appended at the end of the candidate list.
        if (result.types & PredictionType::UNIGRAM) {
          const size_t predicted_key_len = key_len - input_key_len;
          // -500 * log(prob)
          // See also: mozc/converter/candidate_filter.cc
          const int predictive_penalty = 500 * log(50 * predicted_key_len);
          cost += predictive_penalty;
        }
        MOZC_WORD_LOG(result,
                      absl::StrCat("Unigram | Typing correction: ", cost));
      }
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
      MOZC_WORD_LOG(result, absl::StrCat("Bigram: ", cost));
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

    if (result.candidate_attributes &
        Segment::Candidate::PARTIALLY_KEY_CONSUMED) {
      cost +=
          CalculatePrefixPenalty(request, input_key, result,
                                 immutable_converter_, &prefix_penalty_cache);
      MOZC_WORD_LOG(result, absl::StrCat("Prefix: ", cost));
    }

    // Note that the cost is defined as -500 * log(prob).
    // Even after the ad hoc manipulations, cost must remain larger than 0.
    result.cost = std::max(1, cost);
    MOZC_WORD_LOG(result, absl::StrCat("SetLMCost: ", result.cost));
  }
}

// This method should be deprecated, as it is unintentionally adding extra
// spatial penalty to the candidate.

// static
void DictionaryPredictor::ApplyPenaltyForKeyExpansion(
    const Segments &segments, std::vector<Result> *results) {
  if (segments.conversion_segments_size() == 0) {
    return;
  }
  // Cost penalty 1151 means that expanded candidates are evaluated
  // 10 times smaller in frequency.
  // Note that the cost is calcurated by cost = -500 * log(prob)
  // 1151 = 500 * log(10)
  constexpr int kKeyExpansionPenalty = 1151;
  const std::string &conversion_key = segments.conversion_segment(0).key();
  for (size_t i = 0; i < results->size(); ++i) {
    Result &result = results->at(i);
    if (result.types & PredictionType::TYPING_CORRECTION) {
      continue;
    }
    if (!absl::StartsWith(result.key, conversion_key)) {
      result.cost += kKeyExpansionPenalty;
      MOZC_WORD_LOG(result, absl::StrCat("KeyExpansionPenalty: ", result.cost));
    }
  }
}

// static
size_t DictionaryPredictor::GetMissSpelledPosition(
    const absl::string_view key, const absl::string_view value) {
  std::string hiragana_value;
  japanese_util::KatakanaToHiragana(value, &hiragana_value);
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
      results->at(i).removed = true;
      MOZC_WORD_LOG(results->at(i), "Removed. same_(key|value)_index.");
      for (size_t k = 0; k < same_key_index.size(); ++k) {
        results->at(same_key_index[k]).removed = true;
        MOZC_WORD_LOG(results->at(i), "Removed. same_(key|value)_index.");
      }
    } else if (same_key_index.empty() && !same_value_index.empty()) {
      results->at(i).removed = true;
      MOZC_WORD_LOG(results->at(i), "Removed. same_value_index.");
    } else if (!same_key_index.empty() && same_value_index.empty()) {
      for (size_t k = 0; k < same_key_index.size(); ++k) {
        results->at(same_key_index[k]).removed = true;
        MOZC_WORD_LOG(results->at(i), "Removed. same_key_index.");
      }
      if (request_key_len <= GetMissSpelledPosition(result.key, result.value)) {
        results->at(i).removed = true;
        MOZC_WORD_LOG(results->at(i), "Removed. Invalid MissSpelledPosition.");
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
  Segment *segment = tmp_segments.add_segment();
  segment->set_key(Util::Utf8SubString(input_key, key_len));
  ConversionRequest req = request;
  req.set_max_conversion_candidates_size(1);
  if (immutable_converter->ConvertForRequest(req, &tmp_segments) &&
      segment->candidates_size() > 0) {
    const Segment::Candidate &top_candidate = segment->candidate(0);
    penalty = (connector_->GetTransitionCost(result_rid, top_candidate.lid) +
               top_candidate.cost);
  }
  (*cache)[cache_key] = penalty;
  return penalty;
}

// static
bool DictionaryPredictor::GetHistoryKeyAndValue(const Segments &segments,
                                                std::string *key,
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

}  // namespace mozc

#undef MOZC_WORD_LOG_MESSAGE
#undef MOZC_WORD_LOG
