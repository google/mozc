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

#ifndef MOZC_PREDICTION_DICTIONARY_PREDICTION_AGGREGATOR_H_
#define MOZC_PREDICTION_DICTIONARY_PREDICTION_AGGREGATOR_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/util.h"
#include "converter/converter_interface.h"
#include "converter/immutable_converter_interface.h"
#include "converter/segments.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "engine/modules.h"
#include "prediction/number_decoder.h"
#include "prediction/prediction_aggregator_interface.h"
#include "prediction/result.h"
#include "prediction/zero_query_dict.h"
#include "request/conversion_request.h"

namespace mozc {
namespace prediction {

class DictionaryPredictionAggregator : public PredictionAggregatorInterface {
 public:
  DictionaryPredictionAggregator(const DictionaryPredictionAggregator &) =
      delete;
  DictionaryPredictionAggregator &operator=(
      const DictionaryPredictionAggregator &) = delete;
  ~DictionaryPredictionAggregator() override = default;

  DictionaryPredictionAggregator(
      const engine::Modules &modules, const ConverterInterface &converter,
      const ImmutableConverterInterface &immutable_converter);

  std::vector<Result> AggregateResults(const ConversionRequest &request,
                                       const Segments &segments) const override;

  std::vector<Result> AggregateTypingCorrectedResults(
      const ConversionRequest &request,
      const Segments &segments) const override;

 private:
  class PredictiveLookupCallback;
  class PrefixLookupCallback;
  class PredictiveBigramLookupCallback;
  class HandwritingLookupCallback;

  using AggregateUnigramFn = PredictionType (DictionaryPredictionAggregator::*)(
      const ConversionRequest &request, const Segments &segments,
      std::vector<Result> *results) const;

  struct UnigramConfig {
    AggregateUnigramFn unigram_fn;
    size_t min_key_len;
  };

  struct HandwritingQueryInfo {
    // Hiragana key for dictionary look up.
    // ex. "かんじじてん" for "かん字じ典"
    std::string query;
    // The list of non-Hiragana strings. They should be appeared in the result
    // token value in order.
    // ex. {"字", "典"} for "かん字じ典"
    std::vector<std::string> constraints;
  };

  // Returns the bitfield that indicates what prediction subroutines
  // were used.  NO_PREDICTION means that no prediction was made.
  PredictionTypes AggregatePredictionForTesting(
      const ConversionRequest &request, const Segments &segments,
      std::vector<Result> *results) const;

  PredictionTypes AggregatePrediction(const ConversionRequest &request,
                                      size_t realtime_max_size,
                                      const UnigramConfig &unigram_config,
                                      const Segments &segments,
                                      std::vector<Result> *results) const;

  // Looks up the given range and appends zero query candidate list for |key|
  // to |results|.
  // Returns false if there is no result for |key|.
  static bool GetZeroQueryCandidatesForKey(
      const ConversionRequest &request, absl::string_view key,
      const ZeroQueryDict &dict, std::vector<ZeroQueryResult> *results);

  static void AppendZeroQueryToResults(
      absl::Span<const ZeroQueryResult> candidates, uint16_t lid, uint16_t rid,
      std::vector<Result> *results);

  PredictionTypes AggregatePredictionForZeroQuery(
      const ConversionRequest &request, const Segments &segments,
      std::vector<Result> *results) const;

  bool AggregateNumberZeroQueryPrediction(const ConversionRequest &request,
                                          const Segments &segments,
                                          std::vector<Result> *results) const;

  bool AggregateZeroQueryPrediction(const ConversionRequest &request,
                                    const Segments &segments,
                                    std::vector<Result> *results) const;

  // Adds prediction results from history key and value.
  void AddBigramResultsFromHistory(absl::string_view history_key,
                                   absl::string_view history_value,
                                   const ConversionRequest &request,
                                   const Segments &segments,
                                   Segment::Candidate::SourceInfo source_info,
                                   std::vector<Result> *results) const;

  // Changes the prediction type for irrelevant bigram candidate.
  void CheckBigramResult(const dictionary::Token &history_token,
                         Util::ScriptType history_ctype,
                         Util::ScriptType last_history_ctype,
                         const ConversionRequest &request,
                         Result *result) const;

  static void GetPredictiveResults(
      const dictionary::DictionaryInterface &dictionary,
      absl::string_view history_key, const ConversionRequest &request,
      const Segments &segments, PredictionTypes types, size_t lookup_limit,
      Segment::Candidate::SourceInfo source_info, int zip_code_id,
      int unknown_id, std::vector<Result> *results);

  void GetPredictiveResultsForBigram(
      const dictionary::DictionaryInterface &dictionary,
      absl::string_view history_key, absl::string_view history_value,
      const ConversionRequest &request, const Segments &segments,
      PredictionTypes types, size_t lookup_limit,
      Segment::Candidate::SourceInfo source_info, int unknown_id,
      std::vector<Result> *results) const;

  // Performs a custom look up for English words where case-conversion might be
  // applied to lookup key and/or output results.
  void GetPredictiveResultsForEnglishKey(
      const dictionary::DictionaryInterface &dictionary,
      const ConversionRequest &request, absl::string_view input_key,
      PredictionTypes types, size_t lookup_limit,
      std::vector<Result> *results) const;

  // Returns true if the realtime conversion should be used.
  // TODO(hidehiko): add Config and Request instances into the arguments
  //   to represent the dependency explicitly.
  static bool ShouldAggregateRealTimeConversionResults(
      const ConversionRequest &request, const Segments &segments);

  // Returns true if key consistes of '0'-'9' or '-'
  static bool IsZipCodeRequest(absl::string_view key);

  // Returns max size of realtime candidates.
  size_t GetRealtimeCandidateMaxSize(const ConversionRequest &request,
                                     const Segments &segments,
                                     bool mixed_conversion) const;

  // Returns config to gather unigram candidates.
  UnigramConfig GetUnigramConfig(const ConversionRequest &request) const;

  // Returns cutoff threshold of unigram candidates.
  // AggregateUnigramPrediction method does not return any candidates
  // if there are too many (>= cutoff threshold) eligible candidates.
  // This behavior prevents a user from seeing too many prefix-match
  // candidates.
  size_t GetCandidateCutoffThreshold(
      ConversionRequest::RequestType request_type) const;

  // Generates a top conversion result from |converter_| and adds its result to
  // |results|.
  bool PushBackTopConversionResult(const ConversionRequest &request,
                                   const Segments &segments,
                                   std::vector<Result> *results) const;

  // Aggregate* methods aggregate the candidates with different resources
  // and algorithms.
  void AggregateRealtimeConversion(
      const ConversionRequest &request, size_t realtime_candidates_size,
      bool insert_realtime_top_from_actual_converter, const Segments &segments,
      std::vector<Result> *results) const;

  void AggregateBigramPrediction(const ConversionRequest &request,
                                 const Segments &segments,
                                 Segment::Candidate::SourceInfo source_info,
                                 std::vector<Result> *results) const;

  void AggregateSuffixPrediction(const ConversionRequest &request,
                                 const Segments &segments,
                                 std::vector<Result> *results) const;

  void AggregateZeroQuerySuffixPrediction(const ConversionRequest &request,
                                          const Segments &segments,
                                          std::vector<Result> *results) const;

  void AggregateEnglishPrediction(const ConversionRequest &request,
                                  const Segments &segments,
                                  std::vector<Result> *results) const;

  void AggregatePrefixCandidates(const ConversionRequest &request,
                                 const Segments &segments,
                                 std::vector<Result> *results) const;

  bool AggregateNumberCandidates(const ConversionRequest &request,
                                 const Segments &segments,
                                 std::vector<Result> *results) const;

  bool AggregateNumberCandidates(absl::string_view input_key,
                                 std::vector<Result> *results) const;

  // Note that this look up is done with raw input string rather than query
  // string from composer.  This is helpful to implement language aware input.
  void AggregateEnglishPredictionUsingRawInput(
      const ConversionRequest &request, const Segments &segments,
      std::vector<Result> *results) const;

  void AggregateTypingCorrectedPrediction(const ConversionRequest &request,
                                          const Segments &segments,
                                          PredictionTypes base_selected_types,
                                          std::vector<Result> *results) const;

  PredictionType AggregateUnigramCandidate(const ConversionRequest &request,
                                           const Segments &segments,
                                           std::vector<Result> *results) const;

  PredictionType AggregateUnigramCandidateForMixedConversion(
      const ConversionRequest &request, const Segments &segments,
      std::vector<Result> *results) const;

  PredictionType AggregateUnigramCandidateForLatinInput(
      const ConversionRequest &request, const Segments &segments,
      std::vector<Result> *results) const;

  // Generates `HandwritingQueryInfo` for the given composition event.
  std::optional<HandwritingQueryInfo> GenerateQueryForHandwriting(
      const ConversionRequest &request,
      const commands::SessionCommand::CompositionEvent &composition_event)
      const;

  // Generates prediction candidates using composition events in composer and
  // appends to `results`.
  PredictionType AggregateUnigramCandidateForHandwriting(
      const ConversionRequest &request, const Segments &segments,
      std::vector<Result> *results) const;

  static void LookupUnigramCandidateForMixedConversion(
      const dictionary::DictionaryInterface &dictionary,
      const ConversionRequest &request, const Segments &segments,
      int zip_code_id, int unknown_id, std::vector<Result> *results);

  void MaybePopulateTypingCorrectionPenalty(const ConversionRequest &request,
                                            const Segments &segments,
                                            std::vector<Result> *results) const;

  // Test peer to access private methods
  friend class DictionaryPredictionAggregatorTestPeer;

  const engine::Modules &modules_;
  const ConverterInterface &converter_;
  const ImmutableConverterInterface &immutable_converter_;
  const dictionary::DictionaryInterface *dictionary_;
  const dictionary::DictionaryInterface *suffix_dictionary_;
  const uint16_t counter_suffix_word_id_;
  const uint16_t kanji_number_id_;
  const uint16_t zip_code_id_;
  const uint16_t number_id_;
  const uint16_t unknown_id_;
  const ZeroQueryDict &zero_query_dict_;
  const ZeroQueryDict &zero_query_number_dict_;
  NumberDecoder number_decoder_;
  std::unique_ptr<PredictionAggregatorInterface>
      single_kanji_prediction_aggregator_;
};

}  // namespace prediction
}  // namespace mozc

#endif  // MOZC_PREDICTION_DICTIONARY_PREDICTION_AGGREGATOR_H_
