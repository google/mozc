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
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "engine/modules.h"
#include "prediction/dictionary_decoder.h"
#include "prediction/english_decoder.h"
#include "prediction/handwriting_decoder.h"
#include "prediction/realtime_decoder.h"
#include "prediction/result.h"
#include "prediction/zero_query_decoder.h"
#include "request/conversion_request.h"

namespace mozc {
namespace prediction {

// TODO(taku): We want to make this class only provide the primitive
// candidate aggregate methods. The downstream client will decide which
// aggregators are used and combined.

// Interface class for mock.
class DictionaryPredictionAggregatorInterface {
 public:
  virtual ~DictionaryPredictionAggregatorInterface() = default;

  // These methods will be moved to DesktopPredictor and MixedDecodingPredictor.
  virtual std::vector<Result> AggregateResultsForMixedConversion(
      const ConversionRequest& request) const = 0;

  virtual std::vector<Result> AggregateResultsForDesktop(
      const ConversionRequest& request) const = 0;

  virtual std::vector<Result> AggregateTypingCorrectedResultsForMixedConversion(
      const ConversionRequest& request) const = 0;
};

class DictionaryPredictionAggregator
    : public DictionaryPredictionAggregatorInterface {
 public:
  DictionaryPredictionAggregator() = delete;
  DictionaryPredictionAggregator(const DictionaryPredictionAggregator&) =
      delete;

  DictionaryPredictionAggregator& operator=(
      const DictionaryPredictionAggregator&) = delete;
  virtual ~DictionaryPredictionAggregator() = default;

  DictionaryPredictionAggregator(const engine::Modules& modules,
                                 const RealtimeDecoder& decoder);

  // Calls AggregateResultsForMixedConversion or AggregateResultsForDesktop
  // depending on the request.
  std::vector<Result> AggregateResultsForTesting(
      const ConversionRequest& request) const;

  // These methods will be moved to DesktopPredictor and MixedDecodingPredictor.
  std::vector<Result> AggregateResultsForMixedConversion(
      const ConversionRequest& request) const override;

  std::vector<Result> AggregateResultsForDesktop(
      const ConversionRequest& request) const override;

  std::vector<Result> AggregateTypingCorrectedResultsForMixedConversion(
      const ConversionRequest& request) const override;

 private:
  //////////////////////////////////////////////////////////////////////////
  // Top level basic aggregators.
  // Do not implement preconditions for calling the actual operation within
  // these methods. Example includes length, config, or request-based
  // pre-conditions Aggregate unigram candidates from dictionary.

  // Aggregates basic unigram candidates from dictionary.
  // Depending on the request and condition, processing is passed to
  // the following AggregateUnigramForXXX methods.
  void AggregateUnigram(const ConversionRequest& request,
                        std::vector<Result>* results,
                        int* min_unigram_key_len) const;

  // Aggregate bigram candidates from dictionary.
  // This aggregator uses the history (context).
  void AggregateBigram(const ConversionRequest& request,
                       std::vector<Result>* results) const;

  // Aggregate results from the converter.
  void AggregateRealtime(const ConversionRequest& request,
                         size_t realtime_candidates_size,
                         bool insert_realtime_top_from_actual_converter,
                         std::vector<Result>* results) const;

  // Aggregate zero query candidates. Current key must be empty.
  void AggregateZeroQuery(const ConversionRequest& request,
                          std::vector<Result>* results) const;

  // Aggregate English candidates from English dictionary.
  void AggregateEnglish(const ConversionRequest& request,
                        std::vector<Result>* results) const;

  // Note that this look up is done with raw input string rather than query
  // string from composer.  This is helpful to implement language aware input.
  void AggregateEnglishUsingRawInput(const ConversionRequest& request,
                                     std::vector<Result>* results) const;

  // Aggregate numbers using number decoder.
  void AggregateNumber(const ConversionRequest& request,
                       std::vector<Result>* results) const;

  // Aggregate partial suffix candidates.
  void AggregatePrefix(const ConversionRequest& request,
                       std::vector<Result>* results) const;

  // Aggregate single kanji.
  void AggregateSingleKanji(const ConversionRequest& request,
                            std::vector<Result>* results) const;

  //////////////////////////////////////////////////////////////////////////
  // Sub aggregators called inside the top Aggregators.
  void AggregateUnigramForHandwriting(const ConversionRequest& request,
                                      std::vector<Result>* results) const;

  //////////////////////////////////////////////////////////////////////////
  // Misc functions

  void MaybePopulateTypingCorrectionPenalty(const ConversionRequest& request,
                                            std::vector<Result>* results) const;

  // Returns true if the realtime conversion should be used.
  // TODO(hidehiko): add Config and Request instances into the arguments
  //   to represent the dependency explicitly.
  static bool ShouldAggregateRealTimeConversionResults(
      const ConversionRequest& request);

  // Returns true if key consists of '0'-'9' or '-'
  static bool IsZipCodeRequest(absl::string_view key);

  // Returns max size of realtime candidates.
  static size_t GetRealtimeCandidateMaxSize(const ConversionRequest& request);

  // Returns cutoff threshold of unigram candidates.
  // AggregateUnigramPrediction method does not return any candidates
  // if there are too many (>= cutoff threshold) eligible candidates.
  // This behavior prevents a user from seeing too many prefix-match
  // candidates.
  static size_t GetCandidateCutoffThreshold(
      ConversionRequest::RequestType request_type);

  static bool IsNotExceedingCutoffThreshold(const ConversionRequest& request,
                                            absl::Span<const Result> results) {
    return results.size() <=
           GetCandidateCutoffThreshold(request.request_type());
  }

  // Test peer to access private methods
  friend class DictionaryPredictionAggregatorTestPeer;

  const engine::Modules& modules_;
  const RealtimeDecoder& decoder_;
  const DictionaryDecoder dictionary_decoder_;
  const HandwritingDecoder handwriting_decoder_;
  const ZeroQueryDecoder zero_query_decoder_;
  const EnglishDecoder english_decoder_;
};

}  // namespace prediction
}  // namespace mozc

#endif  // MOZC_PREDICTION_DICTIONARY_PREDICTION_AGGREGATOR_H_
