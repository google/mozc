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
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/util.h"
#include "base/vlog.h"
#include "composer/composer.h"
#include "converter/attribute.h"
#include "converter/connector.h"
#include "converter/segmenter.h"
#include "dictionary/pos_matcher.h"
#include "engine/modules.h"
#include "engine/supplemental_model_interface.h"
#include "prediction/dictionary_prediction_aggregator.h"
#include "prediction/realtime_decoder.h"
#include "prediction/result.h"
#include "prediction/result_filter.h"
#include "prediction/suggestion_filter.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "request/request_util.h"
#include "transliteration/transliteration.h"

namespace mozc::prediction {
namespace {

bool IsDebug(const ConversionRequest &request) {
#ifndef NDEBUG
  return true;
#else   // NDEBUG
  return request.config().verbose_level() >= 1;
#endif  // NDEBUG
}

bool IsLatinInputMode(const ConversionRequest &request) {
  return request.composer().GetInputMode() == transliteration::HALF_ASCII ||
         request.composer().GetInputMode() == transliteration::FULL_ASCII;
}

bool IsMixedConversionEnabled(const ConversionRequest &request) {
  return request.request().mixed_conversion();
}

bool IsTypingCorrectionEnabled(const ConversionRequest &request) {
  return request.config().use_typing_correction();
}

template <typename... Args>
void AppendDescription(Result &result, Args &&...args) {
  absl::StrAppend(&result.description, result.description.empty() ? "" : " ",
                  std::forward<Args>(args)...);
}

void MaybeFixRealtimeTopCost(const ConversionRequest &request,
                             absl::Span<Result> results) {
  // Remember the minimum cost among those REALTIME
  // candidates that have the same key length as |request_key| so that we can
  // set a slightly smaller cost to REALTIME_TOP than these.
  int realtime_cost_min = Result::kInvalidCost;
  Result *realtime_top_result = nullptr;
  for (Result &result : results) {
    if (result.types & PredictionType::REALTIME_TOP) {
      realtime_top_result = &result;
    }

    // Update the minimum cost for REALTIME candidates that have the same key
    // length as request_key.
    if (result.types & PredictionType::REALTIME &&
        result.cost < realtime_cost_min &&
        result.key.size() == request.key().size()) {
      realtime_cost_min = result.cost;
    }
  }

  // Ensure that the REALTIME_TOP candidate has relatively smaller cost than
  // those of REALTIME candidates.
  if (realtime_top_result != nullptr &&
      realtime_cost_min != Result::kInvalidCost) {
    realtime_top_result->cost = std::max(0, realtime_cost_min - 10);
  }
}

}  // namespace

DictionaryPredictor::DictionaryPredictor(
    const engine::Modules &modules,
    std::unique_ptr<const RealtimeDecoder> decoder)
    : DictionaryPredictor(modules, nullptr, std::move(decoder)) {
  // Explicitly allocate aggregator_ as decoder is unique_ptr and cannot be
  // passed to the constructor of DictionaryPredictor and
  // DictionaryPredictionAggregator at the same time.
  aggregator_ = std::make_unique<prediction::DictionaryPredictionAggregator>(
      modules, *decoder_);
}

DictionaryPredictor::DictionaryPredictor(
    const engine::Modules &modules,
    std::unique_ptr<const DictionaryPredictionAggregatorInterface> aggregator,
    std::unique_ptr<const RealtimeDecoder> decoder)
    : aggregator_(std::move(aggregator)),
      decoder_(std::move(decoder)),
      connector_(modules.GetConnector()),
      segmenter_(modules.GetSegmenter()),
      suggestion_filter_(modules.GetSuggestionFilter()),
      pos_matcher_(modules.GetPosMatcher()),
      general_symbol_id_(pos_matcher_.GetGeneralSymbolId()),
      modules_(modules) {}

std::vector<Result> DictionaryPredictor::Predict(
    const ConversionRequest &request) const {
  if (request.request_type() == ConversionRequest::CONVERSION) {
    MOZC_VLOG(2) << "request type is CONVERSION";
    return {};
  }

  std::vector<Result> results;

  // TODO(taku): Separate DesktopPredictor and MixedDecodingPredictor.
  if (IsMixedConversionEnabled(request)) {
    std::vector<Result> literal_results =
        aggregator_->AggregateResultsForMixedConversion(request);
    std::vector<Result> tc_results =
        AggregateTypingCorrectedResultsForMixedConversion(request);
    absl::c_move(literal_results, std::back_inserter(results));
    absl::c_move(tc_results, std::back_inserter(results));
  } else {
    results = aggregator_->AggregateResultsForDesktop(request);
  }

  RewriteResultsForPrediction(request, absl::MakeSpan(results));

  MaybeRescoreResults(request, absl::MakeSpan(results));

  // `results` are no longer used.
  return RerankAndFilterResults(request, std::move(results));
}

void DictionaryPredictor::RewriteResultsForPrediction(
    const ConversionRequest &request, absl::Span<Result> results) const {
  // Mixed conversion is the feature that mixes prediction and
  // conversion, meaning that results may include the candidates whose
  // key is exactly the same as the composition.  This mode is used in mobile.
  const bool is_mixed_conversion = IsMixedConversionEnabled(request);

  if (is_mixed_conversion) {
    SetPredictionCostForMixedConversion(request, results);
  } else {
    SetPredictionCost(request, results);
  }

  MaybeFixRealtimeTopCost(request, results);

  if (!is_mixed_conversion) {
    RemoveMissSpelledCandidates(request, results);
  }
}

std::vector<Result>
DictionaryPredictor::AggregateTypingCorrectedResultsForMixedConversion(
    const ConversionRequest &request) const {
  constexpr int kMinTypingCorrectionKeyLen = 3;
  if (!IsTypingCorrectionEnabled(request) ||
      Util::CharsLen(request.key()) < kMinTypingCorrectionKeyLen) {
    return {};
  }

  return aggregator_->AggregateTypingCorrectedResultsForMixedConversion(
      request);
}

std::vector<Result> DictionaryPredictor::RerankAndFilterResults(
    const ConversionRequest &request, std::vector<Result> results) const {
  const bool cursor_at_tail =
      request.composer().GetCursor() == request.composer().GetLength();

  // Instead of sorting all the results, we construct a heap.
  // This is done in linear time and
  // we can pop as many results as we need efficiently.
  std::make_heap(results.begin(), results.end(),
                 [](const Result &lhs, const Result &rhs) {
                   return ResultCostLess()(rhs, lhs);
                 });

  const size_t max_candidates_size = std::min(
      request.max_dictionary_prediction_candidates_size(), results.size());

  filter::ResultFilter filter(request, pos_matcher_, connector_,
                              suggestion_filter_);

  absl::flat_hash_map<std::string, int32_t> merged_types;
  if (IsDebug(request)) {
    for (const auto &result : results) {
      if (!result.removed) {
        merged_types[result.value] |= result.types;
      }
    }
  }

  std::shared_ptr<Result> prev_top_result;
  std::vector<Result> final_results;

  for (size_t i = 0; i < results.size(); ++i) {
    std::pop_heap(results.begin(), results.end() - i,
                  [](const Result &lhs, const Result &rhs) {
                    return ResultCostLess()(rhs, lhs);
                  });
    Result &result = results[results.size() - i - 1];
    if (final_results.size() >= max_candidates_size ||
        result.cost >= Result::kInvalidCost) {
      break;
    }

    if (i == 0 && (prev_top_result =
                       MaybeGetPreviousTopResult(result, request)) != nullptr) {
      final_results.emplace_back(*prev_top_result);
    }

    if (filter.ShouldRemove(result, final_results.size())) {
      continue;
    }

    if ((result.candidate_attributes &
         converter::Attribute::PARTIALLY_KEY_CONSUMED) &&
        cursor_at_tail) {
      result.candidate_attributes |=
          converter::Attribute::AUTO_PARTIAL_SUGGESTION;
    }

    if ((!(result.candidate_attributes &
           converter::Attribute::SPELLING_CORRECTION) &&
         IsLatinInputMode(request)) ||
        (result.types & PredictionType::SUFFIX)) {
      result.candidate_attributes |=
          converter::Attribute::NO_VARIANTS_EXPANSION;
      result.candidate_attributes |= converter::Attribute::NO_EXTRA_DESCRIPTION;
    }

    if (result.types & PredictionType::TYPING_CORRECTION) {
      result.candidate_attributes |= converter::Attribute::TYPING_CORRECTION;
    }

    final_results.emplace_back(std::move(result));
  }

  MaybeApplyPostCorrection(request, final_results);

  if (IsDebug(request)) {
    for (auto &result : final_results) {
      const auto it = merged_types.find(result.value);
      const int32_t types = it == merged_types.end() ? 0 : it->second;
      AppendDescription(result, GetPredictionTypeDebugString(types));
    }
  }

  return final_results;
}

void DictionaryPredictor::MaybeApplyPostCorrection(
    const ConversionRequest &request, std::vector<Result> &results) const {
  // b/363902660:
  // Stop applying post correction when handwriting mode.
  if (request_util::IsHandwriting(request)) {
    return;
  }
  modules_.GetSupplementalModel().PostCorrect(request, results);
}

int DictionaryPredictor::CalculateSingleKanjiCostOffset(
    const ConversionRequest &request, uint16_t rid,
    absl::Span<const Result> results,
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

    if (result.value == request.key()) {
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
        converter::Attribute::PARTIALLY_KEY_CONSUMED) {
      lm_cost += CalculatePrefixPenalty(request, result, cache);
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
    lm_cost += segmenter_.GetSuffixPenalty(result.rid);
  }

  return lm_cost;
}

void DictionaryPredictor::SetPredictionCost(const ConversionRequest &request,
                                            absl::Span<Result> results) const {
  const int history_rid = request.converter_history_rid();

  const bool is_suggestion =
      (request.request_type() == ConversionRequest::SUGGESTION);

  // use the same scoring function for both unigram/bigram.
  // Bigram will be boosted because we pass the previous
  // key as a context information.
  const size_t history_key_len =
      Util::CharsLen(request.converter_history_key(1));
  const size_t request_key_len = Util::CharsLen(request.key());

  for (Result &result : results) {
    const int cost = GetLMCost(result, history_rid);
    size_t query_len = request_key_len;
    size_t key_len = Util::CharsLen(result.key);
    if (result.types & PredictionType::BIGRAM) {
      query_len += history_key_len;
      key_len += history_key_len;
    }

    if (IsAggressiveSuggestion(query_len, key_len, cost, is_suggestion,
                               results.size())) {
      result.cost = Result::kInvalidCost;
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
    result.cost =
        cost - kCostFactor * log(1.0 + std::max<int>(0, key_len - query_len));
  }
}

void DictionaryPredictor::SetPredictionCostForMixedConversion(
    const ConversionRequest &request, absl::Span<Result> results) const {
  // ranking for mobile
  const int history_rid =
      request.converter_history_rid();  // 0 (BOS) is default

  // cost of the last history candidate.
  int prev_cost = request.converter_history_cost();
  if (prev_cost == 0) {
    // if prev_cost is set to be 0 for some reason, use default cost.
    prev_cost = 5000;
  }

  absl::flat_hash_map<PrefixPenaltyKey, int32_t> prefix_penalty_cache;
  const int single_kanji_offset = CalculateSingleKanjiCostOffset(
      request, history_rid, results, &prefix_penalty_cache);

  for (Result &result : results) {
    int cost = GetLMCost(result, history_rid);
    MOZC_WORD_LOG(result, "GetLMCost: ", cost);
    if (result.lid == result.rid && !pos_matcher_.IsSuffixWord(result.rid) &&
        !pos_matcher_.IsFunctional(result.rid) &&
        !pos_matcher_.IsWeakCompoundVerbSuffix(result.rid)) {
      // TODO(b/242683049): It's better to define a new POS matcher rule once
      // we decide to make this behavior default.
      // Implementation note:
      // Suffix penalty is added in the above GetLMCost(), which is also used
      // for calculating non-mobile prediction cost. So we modify the cost
      // calculation here for now.
      cost -= segmenter_.GetSuffixPenalty(result.rid);
      MOZC_WORD_LOG(result, "Cancel Suffix Penalty: ", cost);
    }

    // Demote filtered word here, because they are not filtered for exact match.
    // Even for exact match, we don't want to show aggressive words with high
    // ranking.
    if (suggestion_filter_.IsBadSuggestion(result.value)) {
      // Cost penalty means for bad suggestion.
      // 3453 = 500 * log(1000)
      constexpr int kBadSuggestionPenalty = 3453;
      cost += kBadSuggestionPenalty;
      MOZC_WORD_LOG(result, "BadSuggestionPenalty: ", cost);
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
      MOZC_WORD_LOG(result, "Bigram: ", cost);
    }

    if (result.types & PredictionType::SINGLE_KANJI) {
      cost += single_kanji_offset;
      if (cost <= 0) {
        // single_kanji_offset can be negative.
        // cost should be larger than 0.
        cost = result.wcost + 1;
      }
      MOZC_WORD_LOG(result, "SingleKanji: ", cost);
    }

    if (result.candidate_attributes & converter::Attribute::USER_DICTIONARY &&
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
      MOZC_WORD_LOG(result, "User dictionary: ", cost);
    }

    // Demote predictive results for making exact candidates to have higher
    // ranking.
    // - Users expect the candidates for the input key on the candidates.
    // - We want to show candidates as many as possible in the limited
    //   candidates area.
    const size_t candidate_lookup_key_len = Util::CharsLen(request.key());
    const size_t candidate_key_len = Util::CharsLen(result.key);
    if (!(result.types & PredictionType::SUFFIX) &&
        candidate_key_len > candidate_lookup_key_len) {
      const size_t predicted_key_len =
          candidate_key_len - candidate_lookup_key_len;
      // -500 * log(prob)
      // See also: mozc/converter/candidate_filter.cc
      const int predictive_penalty = 500 * log(50 * predicted_key_len);
      cost += predictive_penalty;
      MOZC_WORD_LOG(result, "Predictive: ", cost);
    }
    // Penalty for prefix results.
    if (result.candidate_attributes &
        converter::Attribute::PARTIALLY_KEY_CONSUMED) {
      const int prefix_penalty =
          CalculatePrefixPenalty(request, result, &prefix_penalty_cache);
      result.penalty += prefix_penalty;
      cost += prefix_penalty;
      MOZC_WORD_LOG(result, "Prefix: ", cost,
                    "; prefix penalty: ", prefix_penalty);
    }

    // Note that the cost is defined as -500 * log(prob).
    // Even after the ad hoc manipulations, cost must remain larger than 0.
    result.cost = std::max(1, cost);
    MOZC_WORD_LOG(result, "SetPredictionCost: ", result.cost);
  }
}

// static
void DictionaryPredictor::RemoveMissSpelledCandidates(
    const ConversionRequest &request, absl::Span<Result> results) {
  const size_t request_key_len = Util::CharsLen(request.key());

  if (results.size() <= 1) {
    return;
  }

  int spelling_correction_size = 5;
  for (size_t i = 0; i < results.size(); ++i) {
    const Result &result = results[i];
    if (!(result.candidate_attributes &
          converter::Attribute::SPELLING_CORRECTION)) {
      continue;
    }

    // Only checks at most 5 spelling corrections to avoid the case
    // like all candidates have SPELLING_CORRECTION.
    if (--spelling_correction_size == 0) {
      return;
    }

    std::vector<size_t> same_key_index, same_value_index;
    for (size_t j = 0; j < results.size(); ++j) {
      if (i == j) {
        continue;
      }
      const Result &target_result = results[j];
      if (target_result.candidate_attributes &
          converter::Attribute::SPELLING_CORRECTION) {
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
      results[i].removed = true;
      MOZC_WORD_LOG(results[i], "Removed. same_(key|value)_index.");
      for (const size_t k : same_key_index) {
        results[k].removed = true;
        MOZC_WORD_LOG(results[k], "Removed. same_(key|value)_index.");
      }
    } else if (same_key_index.empty() && !same_value_index.empty()) {
      results[i].removed = true;
      MOZC_WORD_LOG(results[i], "Removed. same_value_index.");
    } else if (!same_key_index.empty() && same_value_index.empty()) {
      for (const size_t k : same_key_index) {
        results[k].removed = true;
        MOZC_WORD_LOG(results[k], "Removed. same_key_index.");
      }
      if (request_key_len <=
          filter::GetMissSpelledPosition(result.key, result.value)) {
        results[i].removed = true;
        MOZC_WORD_LOG(results[i], "Removed. Invalid MissSpelledPosition.");
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
    const ConversionRequest &request, const Result &result,
    absl::flat_hash_map<PrefixPenaltyKey, int> *cache) const {
  if (request.key() == result.key) {
    LOG(WARNING) << "Invalid prefix key: " << result.key;
    return 0;
  }

  const uint16_t result_rid = result.rid;
  const size_t key_len = Util::CharsLen(result.key);
  const PrefixPenaltyKey cache_key = std::make_pair(result_rid, key_len);
  if (const auto it = cache->find(cache_key); it != cache->end()) {
    return it->second;
  }

  // Use the conversion result's cost for the remaining request.key
  // as the penalty of the prefix candidate.
  // For example, if the input key is "きょうの" and the target prefix candidate
  // is "木:き", the penalty will be the cost of the conversion result for
  // "ょうの".
  int penalty = 0;

  absl::string_view remain_key = Util::Utf8SubString(request.key(), key_len);

  ConversionRequest::Options options = request.options();
  options.max_conversion_candidates_size = 1;
  // Explicitly request conversion result for the entire key.
  options.create_partial_candidates = false;
  options.kana_modifier_insensitive_conversion = false;
  // for efficiency, disable converter.
  options.use_actual_converter_for_realtime_conversion = false;

  // Do not use the current segments but use empty segments as
  // we want to calculate the suffix penalty only.
  const ConversionRequest req = ConversionRequestBuilder()
                                    .SetConversionRequestView(request)
                                    .SetOptions(std::move(options))
                                    .SetEmptyHistoryResult()
                                    .SetKey(remain_key)
                                    .Build();

  if (const std::vector<Result> results = decoder_->Decode(req);
      !results.empty()) {
    const Result &top_result = results.front();
    penalty = connector_.GetTransitionCost(result_rid, top_result.lid) +
              top_result.cost;
  }

  // Convert() can return placeholder candidate with cost 0 when it
  // failed to generate candidates.
  if (penalty <= 0) {
    penalty = Result::kInvalidCost;
  }
  constexpr int kPrefixCandidateCostOffset = 1151;  // 500 * log(10)
  // TODO(toshiyuki): Optimize the cost offset.
  penalty += kPrefixCandidateCostOffset;
  (*cache)[cache_key] = penalty;
  return penalty;
}

void DictionaryPredictor::MaybeRescoreResults(
    const ConversionRequest &request, absl::Span<Result> results) const {
  if (request_util::IsHandwriting(request)) {
    // We want to fix the first candidate for handwriting request.
    return;
  }

  if (IsDebug(request)) {
    for (Result &r : results) r.cost_before_rescoring = r.cost;
  }

  modules_.GetSupplementalModel().RescoreResults(request, results);
}

// static
void DictionaryPredictor::AddRescoringDebugDescription(
    absl::Span<Result> results) {
  // Calculate the ranking by the original costs. Note: this can be slightly
  // different from the actual ranking because, when the candidates were
  // generated, `filter.ShouldRemove()` was applied to the results ordered by
  // the rescored costs. To get the true original ranking, we need to apply
  // `filter.ShouldRemove()` to the results ordered by the original cost.
  // This is just for debugging, so such difference won't matter.
  std::vector<const Result *> cands;
  cands.reserve(results.size());
  int diff = 0;
  for (const auto &result : results) {
    diff += std::abs(result.cost - result.cost_before_rescoring);
    cands.emplace_back(&result);
  }
  // No rescoring happened.
  if (diff == 0) {
    return;
  }
  std::sort(cands.begin(), cands.end(), [](const auto *lhs, const auto *rhs) {
    return lhs->cost_before_rescoring < rhs->cost_before_rescoring;
  });
  absl::flat_hash_map<const Result *, size_t> orig_rank;
  for (size_t i = 0; i < cands.size(); ++i) {
    orig_rank[cands[i]] = i + 1;
  }
  // Populate the debug description.
  for (size_t i = 0; i < results.size(); ++i) {
    const size_t rank = i + 1;
    AppendDescription(results[i], orig_rank[&results[i]], "→", rank);
  }
}

std::shared_ptr<Result> DictionaryPredictor::MaybeGetPreviousTopResult(
    const Result &current_top_result, const ConversionRequest &request) const {
  const int32_t max_diff = request.request()
                               .decoder_experiment_params()
                               .candidate_consistency_cost_max_diff();
  if (max_diff == 0) {
    return nullptr;
  }

  std::shared_ptr<Result> prev_top_result = prev_top_result_.load();

  // Updates the key length.
  const int cur_top_key_length = request.key().size();
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
      prev_top_result->key.starts_with(current_top_result.key)) {
    // Do not need to remember the previous key as `prev_top_result` is still
    // top result.
    return prev_top_result;
  }

  // Remembers the top result.
  prev_top_result_.store(std::make_shared<Result>(current_top_result));

  return nullptr;
}

}  // namespace mozc::prediction
