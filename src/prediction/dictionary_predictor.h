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
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "converter/connector.h"
#include "converter/converter_interface.h"
#include "converter/immutable_converter_interface.h"
#include "converter/segmenter.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/single_kanji_dictionary.h"
#include "engine/modules.h"
#include "prediction/prediction_aggregator_interface.h"
#include "prediction/predictor_interface.h"
#include "prediction/result.h"
#include "prediction/suggestion_filter.h"
#include "request/conversion_request.h"

namespace mozc::prediction {
namespace dictionary_predictor_internal {

// Views for a key and a value. Pass by value.
struct KeyValueView {
  absl::string_view key, value;
};

}  // namespace dictionary_predictor_internal

// Dictionary-based predictor
class DictionaryPredictor : public PredictorInterface {
 public:
  // Cost penalty 1151 means that expanded candidates are evaluated
  // 10 times smaller in frequency.
  // Note that the cost is calcurated by cost = -500 * log(prob)
  // 1151 = 500 * log(10)
  static constexpr int kKeyExpansionPenalty = 1151;

  // Initializes a predictor with given references to submodules. Note that
  // pointers are not owned by the class and to be deleted by the caller.
  DictionaryPredictor(const engine::Modules &modules,
                      const ConverterInterface &converter,
                      const ImmutableConverterInterface &immutable_converter);

  DictionaryPredictor(const DictionaryPredictor &) = delete;
  DictionaryPredictor &operator=(const DictionaryPredictor &) = delete;

  bool PredictForRequest(const ConversionRequest &request,
                         Segments *segments) const override;

  void Finish(const ConversionRequest &request, Segments *segments) override;

  const std::string &GetPredictorName() const override {
    return predictor_name_;
  }

 private:
  class ResultFilter {
   public:
    ResultFilter(const ConversionRequest &request, const Segments &segments,
                 dictionary::PosMatcher pos_matcher,
                 const SuggestionFilter &suggestion_filter
                     ABSL_ATTRIBUTE_LIFETIME_BOUND);
    bool ShouldRemove(const Result &result, int added_num,
                      std::string *log_message);

   private:
    bool CheckDupAndReturn(absl::string_view value, const Result &result,
                           std::string *log_message);

    const std::string input_key_;
    const size_t input_key_len_;
    const dictionary::PosMatcher pos_matcher_;
    const SuggestionFilter &suggestion_filter_;
    const bool is_mixed_conversion_;
    const bool auto_partial_suggestion_;
    const bool include_exact_key_;
    const bool is_handwriting_;

    std::string history_key_;
    std::string history_value_;
    std::string exact_bigram_key_;

    int suffix_count_;
    int predictive_count_;
    int realtime_count_;
    int prefix_tc_count_;
    int tc_count_;

    // Seen set for dup value check.
    absl::flat_hash_set<std::string> seen_;
  };

  // pair: <rid, key_length>
  using PrefixPenaltyKey = std::pair<uint16_t, int16_t>;

  // Constructor for testing
  DictionaryPredictor(
      std::string predictor_name, const engine::Modules &modules,
      std::unique_ptr<const prediction::PredictionAggregatorInterface>
          aggregator,
      const ImmutableConverterInterface &immutable_converter);

  // It is better to pass the rvalue of `results` if the
  // caller doesn't use the results after calling this method.
  bool AddPredictionToCandidates(const ConversionRequest &request,
                                 Segments *segments,
                                 std::vector<Result> results) const;

  void FillCandidate(
      const ConversionRequest &request, const Result &result,
      dictionary_predictor_internal::KeyValueView key_value,
      const absl::flat_hash_map<std::string, int32_t> &merged_types,
      Segment::Candidate *candidate) const;

  // Returns the position of misspelled character position.
  //
  // Example:
  // key: "れみおめろん"
  // value: "レミオロメン"
  // returns 3
  //
  // Example:
  // key: "ろっぽんぎ"
  // value: "六本木"
  // returns 5 (charslen("ろっぽんぎ"))
  static size_t GetMissSpelledPosition(absl::string_view key,
                                       absl::string_view value);

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
  static void RemoveMissSpelledCandidates(size_t request_key_len,
                                          std::vector<Result> *results);

  // Populate conversion costs to `results`.
  void RewriteResultsForPrediction(const ConversionRequest &request,
                                   const Segments &segments,
                                   std::vector<Result> *results) const;

  // Scoring function which takes prediction bounus into account.
  // It basically reranks the candidate by lang_prob * (1 + remain_len).
  // This algorithm is mainly used for desktop.
  void SetPredictionCost(ConversionRequest::RequestType request_type,
                         const Segments &segments,
                         std::vector<Result> *results) const;

  // Scoring function for mixed conversion.
  // In the mixed conversion we basically use the pure language model-based
  // scoring function. This algorithm is mainly used for mobile.
  void SetPredictionCostForMixedConversion(const ConversionRequest &request,
                                           const Segments &segments,
                                           std::vector<Result> *results) const;

  // Returns the cost offset for SINGLE_KANJI results.
  // Aggregated SINGLE_KANJI results does not have LM based wcost(word cost),
  // so we want to add the offset based on the other entries.
  int CalculateSingleKanjiCostOffset(
      const ConversionRequest &request, uint16_t rid,
      absl::string_view input_key, absl::Span<const Result> results,
      absl::flat_hash_map<PrefixPenaltyKey, int> *cache) const;

  // Returns true if the suggestion is classified
  // as "aggressive".
  static bool IsAggressiveSuggestion(size_t query_len, size_t key_len, int cost,
                                     bool is_suggestion,
                                     size_t total_candidates_size);

  // Sets candidate description.
  void SetDescription(PredictionTypes types,
                      Segment::Candidate *candidate) const;
  // Description for DEBUG mode.
  static void SetDebugDescription(PredictionTypes types,
                                  Segment::Candidate *candidate);

  static std::string GetPredictionTypeDebugString(PredictionTypes types);

  int CalculatePrefixPenalty(
      const ConversionRequest &request, absl::string_view input_key,
      const Result &result,
      const ImmutableConverterInterface &immutable_converter,
      absl::flat_hash_map<PrefixPenaltyKey, int> *cache) const;

  // Populates typing corrected results to `results`.
  void MaybePopulateTypingCorrectedResults(const ConversionRequest &request,
                                           const Segments &segments,
                                           std::vector<Result> *results) const;

  void MaybeApplyPostCorrection(const ConversionRequest &request,
                                const Segments &segments,
                                std::vector<Result> &results) const;

  void MaybeRescoreResults(const ConversionRequest &request,
                           const Segments &segments,
                           absl::Span<Result> results) const;
  static void AddRescoringDebugDescription(Segments *segments);

  std::shared_ptr<Result> MaybeGetPreviousTopResult(
      const Result &current_top_result, const ConversionRequest &request,
      const Segments &segments) const;

  // Test peer to access private methods
  friend class DictionaryPredictorTestPeer;

  std::unique_ptr<const prediction::PredictionAggregatorInterface> aggregator_;

  // Previous top result and request key length. (not result length).
  // When the previous and current result are consistent, we still keep showing
  // the previous result to prevent flickering.
  //
  // We can still keep the purely functional decoder design as
  // result = Decode("ABCD") = Decode(Decode("ABC"), "D") =
  //          Decode(Decode(Decode("AB"), "C"), "D")) ...
  // These variables work as a cache of previous results to prevent recursive
  // and expensive functional calls.
  mutable std::shared_ptr<Result> prev_top_result_;
  mutable std::atomic<int32_t> prev_top_key_length_ = 0;

  const ImmutableConverterInterface &immutable_converter_;
  const Connector &connector_;
  const Segmenter *segmenter_;
  const SuggestionFilter &suggestion_filter_;
  std::unique_ptr<const dictionary::SingleKanjiDictionary>
      single_kanji_dictionary_;
  const dictionary::PosMatcher pos_matcher_;
  const uint16_t general_symbol_id_;
  const std::string predictor_name_;
  const engine::Modules &modules_;
};

}  // namespace mozc::prediction

#endif  // MOZC_PREDICTION_DICTIONARY_PREDICTOR_H_
