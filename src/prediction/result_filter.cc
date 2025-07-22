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

#include "prediction/result_filter.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "base/strings/japanese.h"
#include "base/util.h"
#include "converter/candidate.h"
#include "converter/connector.h"
#include "dictionary/pos_matcher.h"
#include "prediction/result.h"
#include "prediction/suggestion_filter.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "request/request_util.h"

namespace mozc::prediction::filter {

namespace {

// Returns true if the |target| may be redundant result.
bool MaybeRedundant(const Result &reference_result,
                    const Result &target_result) {
  const absl::string_view reference = reference_result.value;
  const absl::string_view target = target_result.value;

  // Same value means the result is redundant.
  if (reference == target) {
    return true;
  }

  // If the key is the same, the target is not redundant as value is different.
  if (reference_result.key == target_result.key) {
    return false;
  }

  // target is not an appended value of the reference.
  if (!target.starts_with(reference)) {
    return false;
  }

  // If the suffix is Emoji or unknown script, the result is not redundant.
  // For example, if the reference is "東京", "東京🗼" is not redundant, but
  // "東京タワー" is redundant.
  const absl::string_view suffix = target.substr(reference.size());
  const Util::ScriptType script_type = Util::GetScriptType(suffix);
  return (script_type != Util::EMOJI && script_type != Util::UNKNOWN_SCRIPT);
}

}  // namespace

ResultFilter::ResultFilter(const ConversionRequest &request,
                           dictionary::PosMatcher pos_matcher,
                           const Connector &connector,
                           const SuggestionFilter &suggestion_filter)
    : request_key_(request.key()),
      history_key_(request.converter_history_key(1)),
      history_value_(request.converter_history_value(1)),
      request_key_len_(Util::CharsLen(request_key_)),
      pos_matcher_(pos_matcher),
      connector_(connector),
      suggestion_filter_(suggestion_filter),
      is_mixed_conversion_(request.request().mixed_conversion()),
      auto_partial_suggestion_(
          request_util::IsAutoPartialSuggestionEnabled(request)),
      include_exact_key_(request.request().mixed_conversion()),
      is_handwriting_(request_util::IsHandwriting(request)),
      suffix_nwp_transition_cost_threshold_(
          request.request()
              .decoder_experiment_params()
              .suffix_nwp_transition_cost_threshold()),
      history_rid_(request.converter_history_rid()) {}

bool ResultFilter::ShouldRemove(const Result &result, int added_num) {
  if (result.removed) {
    return true;
  }

  if (result.cost >= Result::kInvalidCost) {
    return true;
  }

  if (!auto_partial_suggestion_ &&
      (result.candidate_attributes &
       converter::Candidate::PARTIALLY_KEY_CONSUMED)) {
    return true;
  }

  // When |include_exact_key| is true, we don't filter the results
  // which have the exactly same key as the input even if it's a bad
  // suggestion.
  if (!(include_exact_key_ && result.key == request_key_) &&
      suggestion_filter_.IsBadSuggestion(result.value)) {
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
      request_key_ == result.value) {
    return true;
  }

  if (seen_.contains(result.value)) {
    return true;
  }

  // User input: "おーすとり" (len = 5)
  // key/value:  "おーすとりら" "オーストラリア" (miss match pos = 4)
  if ((result.candidate_attributes &
       converter::Candidate::SPELLING_CORRECTION) &&
      result.key != request_key_ &&
      request_key_len_ <=
          GetMissSpelledPosition(result.key, result.value) + 1) {
    return true;
  }

  const size_t lookup_key_len = Util::CharsLen(request_key_);

  if (suffix_nwp_transition_cost_threshold_ > 0 && lookup_key_len == 0 &&
      result.types & PredictionType::SUFFIX &&
      connector_.GetTransitionCost(history_rid_, result.lid) >
          suffix_nwp_transition_cost_threshold_) {
    return true;
  }

  if ((result.types & PredictionType::SUFFIX) && suffix_count_++ >= 20) {
    // TODO(toshiyuki): Need refactoring for controlling suffix
    // prediction number after we will fix the appropriate number.
    return true;
  }

  auto check_dup_and_return = [this](absl::string_view value) {
    return !seen_.emplace(value).second;
  };

  if (!is_mixed_conversion_) {
    return check_dup_and_return(result.value);
  }

  // Suppress long candidates to show more candidates in the candidate view.
  const size_t candidate_key_len = Util::CharsLen(result.key);
  if (lookup_key_len > 0 &&  // Do not filter for zero query
      lookup_key_len < candidate_key_len &&
      (predictive_count_++ >= 3 || added_num >= 10)) {
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
    return true;
  }

  constexpr int kTcMaxCount = 3;
  constexpr int kTcMaxRank = 10;

  if ((result.types & PredictionType::TYPING_CORRECTION) &&
      (tc_count_++ >= kTcMaxCount || added_num >= kTcMaxRank)) {
    return true;
  }

  if ((result.types & PredictionType::PREFIX) &&
      (result.candidate_attributes & converter::Candidate::TYPING_CORRECTION) &&
      (prefix_tc_count_++ >= 3 || added_num >= 10)) {
    return true;
  }

  return check_dup_and_return(result.value);
}

size_t GetMissSpelledPosition(const absl::string_view key,
                              const absl::string_view value) {
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

void RemoveRedundantResults(std::vector<Result> *results) {
  constexpr size_t kDeleteTrialNum = 5;
  DCHECK(results);

  // min_iter is the beginning of the remaining results (inclusive), and
  // max_iter is the end of the remaining results (exclusive).
  auto min_iter = results->begin();
  auto max_iter = results->end();
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
    for (auto iter = min_iter; iter != max_iter;) {
      // We do not filter user dictionary word.
      if (iter->candidate_attributes & converter::Candidate::USER_DICTIONARY) {
        ++iter;
        continue;
      }
      // If the result is redundant, swap it out.
      if (MaybeRedundant(reference_result, *iter)) {
        --max_iter;
        std::iter_swap(iter, max_iter);
        continue;
      }
      ++iter;
    }
  }

  // Then the |results| contains;
  // [begin, min_iter): reference results in the above loop.
  // [max_iter, end): (maybe) redundant results.
  // [min_iter, max_iter): remaining results.
  // Here, we revive the redundant results up to five in the result cost order.
  constexpr size_t kDoNotDeleteNum = 5;
  if (std::distance(max_iter, results->end()) >= kDoNotDeleteNum) {
    std::partial_sort(max_iter, max_iter + kDoNotDeleteNum, results->end(),
                      ResultWCostLess());
    max_iter += kDoNotDeleteNum;
  } else {
    max_iter = results->end();
  }

  results->erase(max_iter, results->end());
}

}  // namespace mozc::prediction::filter
