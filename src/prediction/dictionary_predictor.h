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

#ifndef MOZC_PREDICTION_DICTIONARY_PREDICTOR_H_
#define MOZC_PREDICTION_DICTIONARY_PREDICTOR_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/thread.h"
#include "converter/connector.h"
#include "converter/segmenter.h"
#include "dictionary/pos_matcher.h"
#include "engine/modules.h"
#include "prediction/dictionary_prediction_aggregator.h"
#include "prediction/predictor_interface.h"
#include "prediction/result.h"
#include "prediction/suggestion_filter.h"
#include "request/conversion_request.h"

namespace mozc::prediction {

// Dictionary-based predictor
class DictionaryPredictor : public PredictorInterface {
 public:
  // Initializes a predictor with given references to submodules. Note that
  // pointers are not owned by the class and to be deleted by the caller.
  DictionaryPredictor(const engine::Modules &modules,
                      std::unique_ptr<const RealtimeDecoder> decoder);

  DictionaryPredictor(const DictionaryPredictor &) = delete;
  DictionaryPredictor &operator=(const DictionaryPredictor &) = delete;

  std::vector<Result> Predict(const ConversionRequest &request) const override;

  absl::string_view GetPredictorName() const override {
    return "DictionaryPredictor";
  }

 private:
  // Test peer to access private methods
  friend class DictionaryPredictorTestPeer;
  friend class MockDataAndPredictor;

  // pair: <rid, key_length>
  using PrefixPenaltyKey = std::pair<uint16_t, int16_t>;

  // Constructor for testing
  DictionaryPredictor(
      const engine::Modules &modules,
      std::unique_ptr<const DictionaryPredictionAggregatorInterface> aggregator,
      std::unique_ptr<const RealtimeDecoder> decoder);

  std::vector<Result> RerankAndFilterResults(const ConversionRequest &request,
                                             std::vector<Result> result) const;

  // Returns language model cost of |token| given prediction type |type|.
  // |rid| is the right id of previous word (token).
  // If |rid| is unknown, set 0 as a default value.
  int GetLMCost(const Result &result, int rid) const;

  // Given the results aggregated by aggregates, remove
  // miss-spelled results from the |results|.
  // we don't directly remove miss-spelled result but set
  // result[i].type = NO_PREDICTION.
  //
  // Here's the basic step of removal:
  // Case1:
  // result1: "あぼがど" => "アボガド"
  // result2: "あぼがど" => "アボカド" (spelling correction)
  // result3: "あぼかど" => "アボカド"
  // In this case, we can remove result 1 and 2.
  // If there exists the same result2.key in result1,3 and
  // the same result2.value in result1,3, we can remove the
  // 1) spelling correction candidate 2) candidate having
  // the same key as the spelling correction candidate.
  //
  // Case2:
  // result1: "あぼかど" => "アボカド"
  // result2: "あぼがど" => "アボカド" (spelling correction)
  // In this case, remove result2.
  //
  // Case3:
  // result1: "あぼがど" => "アボガド"
  // result2: "あぼがど" => "アボカド" (spelling correction)
  // In this case,
  //   a) user input: あ,あぼ,あぼ => remove result1, result2
  //   b) user input: あぼが,あぼがど  => remove result1
  //
  // let |same_key_size| and |same_value_size| be the number of
  // non-spelling-correction-candidates who have the same key/value as
  // spelling-correction-candidate respectively.
  //
  // if (same_key_size > 0 && same_value_size > 0) {
  //   remove spelling correction and candidates having the
  //   same key as the spelling correction.
  // } else if (same_key_size == 0 && same_value_size > 0) {
  //   remove spelling correction
  // } else {
  //   do nothing.
  // }
  static void RemoveMissSpelledCandidates(const ConversionRequest &request,
                                          absl::Span<Result> results);

  // Populate conversion costs to `results`.
  void RewriteResultsForPrediction(const ConversionRequest &request,
                                   absl::Span<Result> results) const;

  // Scoring function which takes prediction bounus into account.
  // It basically reranks the candidate by lang_prob * (1 + remain_len).
  // This algorithm is mainly used for desktop.
  void SetPredictionCost(const ConversionRequest &request,
                         absl::Span<Result> results) const;

  // Scoring function for mixed conversion.
  // In the mixed conversion we basically use the pure language model-based
  // scoring function. This algorithm is mainly used for mobile.
  void SetPredictionCostForMixedConversion(const ConversionRequest &request,
                                           absl::Span<Result> results) const;

  // Returns the cost offset for SINGLE_KANJI results.
  // Aggregated SINGLE_KANJI results does not have LM based wcost(word cost),
  // so we want to add the offset based on the other entries.
  int CalculateSingleKanjiCostOffset(
      const ConversionRequest &request, uint16_t rid,
      absl::Span<const Result> results,
      absl::flat_hash_map<PrefixPenaltyKey, int> *cache) const;

  // Returns true if the suggestion is classified
  // as "aggressive".
  static bool IsAggressiveSuggestion(size_t query_len, size_t key_len, int cost,
                                     bool is_suggestion,
                                     size_t total_candidates_size);

  int CalculatePrefixPenalty(
      const ConversionRequest &request, const Result &result,
      absl::flat_hash_map<PrefixPenaltyKey, int> *cache) const;

  std::vector<Result> AggregateTypingCorrectedResultsForMixedConversion(
      const ConversionRequest &request) const;

  void MaybeApplyPostCorrection(const ConversionRequest &request,
                                std::vector<Result> &results) const;

  void MaybeRescoreResults(const ConversionRequest &request,
                           absl::Span<Result> results) const;

  static void AddRescoringDebugDescription(absl::Span<Result> results);

  std::shared_ptr<Result> MaybeGetPreviousTopResult(
      const Result &current_top_result, const ConversionRequest &request) const;

  std::unique_ptr<const DictionaryPredictionAggregatorInterface> aggregator_;

  // Previous top result and request key length. (not result length).
  // When the previous and current result are consistent, we still keep showing
  // the previous result to prevent flickering.
  //
  // We can still keep the purely functional decoder design as
  // result = Decode("ABCD") = Decode(Decode("ABC"), "D") =
  //          Decode(Decode(Decode("AB"), "C"), "D")) ...
  // These variables work as a cache of previous results to prevent recursive
  // and expensive functional calls.
  mutable AtomicSharedPtr<Result> prev_top_result_;
  mutable std::atomic<int32_t> prev_top_key_length_ = 0;

  std::unique_ptr<const RealtimeDecoder> decoder_;
  const Connector &connector_;
  const Segmenter &segmenter_;
  const SuggestionFilter &suggestion_filter_;
  const dictionary::PosMatcher pos_matcher_;
  const uint16_t general_symbol_id_;
  const engine::Modules &modules_;
};

}  // namespace mozc::prediction

#endif  // MOZC_PREDICTION_DICTIONARY_PREDICTOR_H_
