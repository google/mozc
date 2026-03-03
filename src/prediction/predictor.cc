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

#include "prediction/predictor.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/util.h"
#include "converter/attribute.h"
#include "converter/converter_interface.h"
#include "converter/immutable_converter_interface.h"
#include "engine/modules.h"
#include "prediction/dictionary_predictor.h"
#include "prediction/predictor_interface.h"
#include "prediction/realtime_decoder.h"
#include "prediction/result.h"
#include "prediction/user_history_predictor.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"

namespace mozc::prediction {
namespace {

constexpr int kPredictionSizeForDesktop = 100;

// On mixed conversion mode PREDICTION (including PARTIAL_PREDICTION) behaves
// like as conversion so the limit is same as conversion's one.
constexpr int kPredictionSizeForMixedConersion = 200;

bool IsMixedConversionEnabled(const ConversionRequest& request) {
  return request.request().mixed_conversion();
}

// Fills empty lid and rid of candidates with the candidates of the same value.
void MaybeFillFallbackPos(absl::Span<Result> results) {
  absl::flat_hash_map<absl::string_view, Result*> posless_results;
  for (Result& result : results) {
    // Candidates with empty POS come before candidates with filled POS.
    if (result.lid == 0 || result.rid == 0) {
      posless_results[result.value] = &result;
      continue;
    }
    if (!posless_results.contains(result.value)) {
      continue;
    }
    Result* posless_result = posless_results[result.value];
    if (posless_result->lid == 0) {
      posless_result->lid = result.lid;
    }
    if (posless_result->rid == 0) {
      posless_result->rid = result.rid;
    }
    if (posless_result->lid != 0 && posless_result->rid != 0) {
      posless_results.erase(result.value);
    }
  }
}

}  // namespace

Predictor::Predictor(const engine::Modules& modules,
                     const ConverterInterface& converter,
                     const ImmutableConverterInterface& immutable_converter)
    : realtime_decoder_(
          std::make_unique<RealtimeDecoder>(immutable_converter, converter)),
      dictionary_predictor_(
          std::make_unique<DictionaryPredictor>(modules, *realtime_decoder_)),
      user_history_predictor_(std::make_unique<UserHistoryPredictor>(modules)),
      pos_matcher_(modules.GetPosMatcher()) {
  DCHECK(dictionary_predictor_);
  DCHECK(user_history_predictor_);
}

Predictor::Predictor(const engine::Modules& modules,
                     std::unique_ptr<PredictorInterface> dictionary_predictor,
                     std::unique_ptr<PredictorInterface> user_history_predictor)
    : dictionary_predictor_(std::move(dictionary_predictor)),
      user_history_predictor_(std::move(user_history_predictor)),
      pos_matcher_(modules.GetPosMatcher()) {
  DCHECK(dictionary_predictor_);
  DCHECK(user_history_predictor_);
}

std::vector<Result> Predictor::Predict(const ConversionRequest& request) const {
  DCHECK(request.request_type() == ConversionRequest::PREDICTION ||
         request.request_type() == ConversionRequest::SUGGESTION ||
         request.request_type() == ConversionRequest::PARTIAL_PREDICTION ||
         request.request_type() == ConversionRequest::PARTIAL_SUGGESTION);
  if (request.request_type() == ConversionRequest::CONVERSION) {
    return {};
  }

  if (request.config().presentation_mode()) {
    return {};
  }

  // TODO(taku): Introduces independent sub predictors for desktop and mixed
  // conversion.
  return IsMixedConversionEnabled(request) ? PredictForMixedConversion(request)
                                           : PredictForDesktop(request);
}

void Predictor::Finish(const ConversionRequest& request,
                       absl::Span<const Result> results, uint32_t revert_id) {
  user_history_predictor_->Finish(request, results, revert_id);
}

void Predictor::CommitContext(const ConversionRequest& request) const {
  user_history_predictor_->CommitContext(request);
}

// Since DictionaryPredictor is immutable, no need
// to call DictionaryPredictor::Revert/Clear*/Finish methods.
void Predictor::Revert(uint32_t revert_id) {
  user_history_predictor_->Revert(revert_id);
}

bool Predictor::ClearAllHistory() {
  return user_history_predictor_->ClearAllHistory();
}

bool Predictor::ClearUnusedHistory() {
  return user_history_predictor_->ClearUnusedHistory();
}

bool Predictor::ClearHistoryEntry(absl::string_view key,
                                  absl::string_view value) {
  return user_history_predictor_->ClearHistoryEntry(key, value);
}

bool Predictor::AddHistoryEntry(absl::string_view key,
                                absl::string_view value) {
  return user_history_predictor_->AddHistoryEntry(key, value);
}

bool Predictor::Wait() { return user_history_predictor_->Wait(); }

bool Predictor::Sync() { return user_history_predictor_->Sync(); }

bool Predictor::Reload() { return user_history_predictor_->Reload(); }

std::vector<Result> Predictor::PredictForDesktop(
    const ConversionRequest& request) const {
  DCHECK(!IsMixedConversionEnabled(request));

  int prediction_size = kPredictionSizeForDesktop;
  if (request.request_type() == ConversionRequest::SUGGESTION) {
    prediction_size =
        std::clamp<int>(request.config().suggestions_size(), 1, 9);
  }

  std::vector<Result> user_history_results, dictionary_results;

  ConversionRequest::Options options = request.options();
  options.max_user_history_prediction_candidates_size = prediction_size;
  options.max_user_history_prediction_candidates_size_for_zero_query =
      prediction_size;
  ConversionRequest request_for_prediction =
      ConversionRequestBuilder()
          .SetConversionRequestView(request)
          .SetOptions(std::move(options))
          .Build();

  user_history_results =
      user_history_predictor_->Predict(request_for_prediction);

  // Do not call dictionary_predictor if the size of candidates get
  // >= suggestions_size.
  if (user_history_results.size() < prediction_size) {
    ConversionRequest::Options options2 = request_for_prediction.options();
    options2.max_dictionary_prediction_candidates_size =
        prediction_size - user_history_results.size();
    const ConversionRequest request_for_prediction2 =
        ConversionRequestBuilder()
            .SetConversionRequestView(request_for_prediction)
            .SetOptions(std::move(options2))
            .Build();
    dictionary_results =
        dictionary_predictor_->Predict(request_for_prediction2);
  }

  std::vector<Result> results = MixCandidates(
      request, std::move(user_history_results), std::move(dictionary_results));

  return results;
}

std::vector<Result> Predictor::PredictForMixedConversion(
    const ConversionRequest& request) const {
  DCHECK(IsMixedConversionEnabled(request));

  // No distinction between SUGGESTION and PREDICTION in mixed conversion mode.
  // Always PREDICTION mode is used.
  ConversionRequest::Options options = request.options();
  options.max_user_history_prediction_candidates_size = 3;
  options.max_user_history_prediction_candidates_size_for_zero_query = 4;
  options.max_dictionary_prediction_candidates_size =
      kPredictionSizeForMixedConersion;

  const ConversionRequest request_for_predict =
      ConversionRequestBuilder()
          .SetConversionRequestView(request)
          .SetOptions(std::move(options))
          .Build();

  std::vector<Result> user_history_results, dictionary_results;

  switch (request.request_type()) {
    case ConversionRequest::SUGGESTION:
    case ConversionRequest::PREDICTION:
      // Suggestion is triggered at every character insertion.
      // So here we should use slow predictors.
      user_history_results =
          user_history_predictor_->Predict(request_for_predict);
      dictionary_results = dictionary_predictor_->Predict(request_for_predict);
      break;
    case ConversionRequest::PARTIAL_SUGGESTION:
    case ConversionRequest::PARTIAL_PREDICTION:
      // PARTIAL SUGGESTION can be triggered in a similar manner to that of
      // SUGGESTION. We don't call slow predictors by the same reason.
      dictionary_results = dictionary_predictor_->Predict(request_for_predict);
      break;
    default:
      break;
  }

  std::vector<Result> results = MixCandidates(
      request, std::move(user_history_results), std::move(dictionary_results));

  MaybeFillFallbackPos(absl::MakeSpan(results));

  return results;
}

bool Predictor::PromoteTopDictionaryResult(
    const ConversionRequest& request,
    absl::Span<const Result> user_history_results,
    absl::Span<const Result> dictionary_results) const {
  const commands::DecoderExperimentParams& params =
      request.request().decoder_experiment_params();

  if (!IsMixedConversionEnabled(request) ||
      params.candidate_mixing_mode() == 0) {
    return false;
  }

  if (user_history_results.empty() || dictionary_results.empty()) {
    return false;
  }

  const Result& user_history_result = user_history_results.front();
  const Result& dictionary_result = dictionary_results.front();

  if (user_history_result.key != dictionary_result.key ||
      user_history_result.value == dictionary_result.value) {
    return false;
  }

  // - Prioritize history for bigram candidates, as they are context-dependent.
  // - The top dictionary candidate is not post corrected.
  // - If there is a possibility of multiple segments, prioritize history to
  //   favor fixed expression.
  if (user_history_result.types & prediction::BIGRAM ||
      !(dictionary_result.types & prediction::POST_CORRECTION) ||
      dictionary_result.inner_segments().size() >= 2) {
    return false;
  }

  // If the candidate is a proper noun, it is better to prioritize history.
  // Heuristics are used to determine whether a candidate is a proper noun.

  // Katakana, Number or Alphabet candidate  would be a proper noun.
  const Util::ScriptType stype = Util::GetScriptType(user_history_result.value);
  if (stype == Util::KATAKANA || stype == Util::NUMBER ||
      stype == Util::ALPHABET) {
    return false;
  }

  {
    // Finds the top history result from the dictionary results to
    // identify the POS. The user history candidate has no POS information.
    const auto it =
        absl::c_find_if(dictionary_results, [&](const auto& result) {
          return result.value == user_history_result.value;
        });
    // The candidate not generated by the dictionary decoder.
    if (it == dictionary_results.end()) {
      return false;
    }

    // The top history candidate is a proper noun.
    if (pos_matcher_.IsUniqueNoun(it->lid) ||
        pos_matcher_.IsUniqueNoun(it->rid) ||
        it->types & prediction::SINGLE_KANJI ||
        it->types & prediction::NUMBER) {
      return false;
    }
  }

  return dictionary_result.post_correction_prob >
         params.candidate_mixing_min_post_correction_prob();
}

std::vector<Result> Predictor::MixCandidates(
    const ConversionRequest& request, std::vector<Result> user_history_results,
    std::vector<Result> dictionary_results) const {
  // Add NO_DELETABLE annotation when the top user history result is the same
  // as the top dictionary result.
  // when user_history_results.size() > 1, the decoder can still show the second
  // candidate, so user can be aware of the change after the deleteion.
  if (user_history_results.size() == 1 && !dictionary_results.empty() &&
      user_history_results.front().key == dictionary_results.front().key &&
      user_history_results.front().value == dictionary_results.front().value) {
    Result& result = user_history_results.front();
    result.candidate_attributes |= converter::Attribute::NO_DELETABLE;
  }

  std::vector<Result> results;
  auto insert_results = [&results](std::vector<Result>& source,
                                   int start_offset = 0) {
    results.insert(results.end(),
                   std::make_move_iterator(source.begin() + start_offset),
                   std::make_move_iterator(source.end()));
  };

  if (PromoteTopDictionaryResult(request, user_history_results,
                                 dictionary_results)) {
    DCHECK(!dictionary_results.empty());
    results.emplace_back(std::move(dictionary_results.front()));
    insert_results(user_history_results);
    insert_results(dictionary_results, 1);
  } else {
    insert_results(user_history_results);
    insert_results(dictionary_results);
  }

  return results;
}

}  // namespace mozc::prediction
