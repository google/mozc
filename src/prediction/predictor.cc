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
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/util.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"
#include "prediction/predictor_interface.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"

namespace mozc::prediction {
namespace {

constexpr int kPredictionSize = 100;
// On Mobile mode PREDICTION (including PARTIAL_PREDICTION) behaves like as
// conversion so the limit is same as conversion's one.
constexpr int kMobilePredictionSize = 200;

size_t GetCandidatesSize(const Segments &segments) {
  if (segments.conversion_segments_size() <= 0) {
    LOG(ERROR) << "No conversion segments found";
    return 0;
  }
  return segments.conversion_segment(0).candidates_size();
}

// TODO(taku): Is it OK to check only |zero_query_suggestion| and
// |mixed_conversion|?
bool IsZeroQuery(const ConversionRequest &request) {
  return request.request().zero_query_suggestion();
}

size_t GetHistoryPredictionSizeFromRequest(const ConversionRequest &request) {
  if (!IsZeroQuery(request)) {
    return 2;
  }
  if (request.request().has_decoder_experiment_params() &&
      request.request()
          .decoder_experiment_params()
          .has_mobile_history_prediction_size()) {
    return request.request()
        .decoder_experiment_params()
        .mobile_history_prediction_size();
  }
  return 3;
}

}  // namespace

BasePredictor::BasePredictor(
    std::unique_ptr<PredictorInterface> dictionary_predictor,
    std::unique_ptr<PredictorInterface> user_history_predictor,
    const ConverterInterface &converter)
    : dictionary_predictor_(std::move(dictionary_predictor)),
      user_history_predictor_(std::move(user_history_predictor)),
      converter_(converter) {
  DCHECK(dictionary_predictor_);
  DCHECK(user_history_predictor_);
}

void BasePredictor::Finish(const ConversionRequest &request,
                           const Segments &segments) {
  user_history_predictor_->Finish(request, segments);
  dictionary_predictor_->Finish(request, segments);
}

// Since DictionaryPredictor is immutable, no need
// to call DictionaryPredictor::Revert/Clear*/Finish methods.
void BasePredictor::Revert(const Segments &segments) {
  user_history_predictor_->Revert(segments);
}

bool BasePredictor::ClearAllHistory() {
  return user_history_predictor_->ClearAllHistory();
}

bool BasePredictor::ClearUnusedHistory() {
  return user_history_predictor_->ClearUnusedHistory();
}

bool BasePredictor::ClearHistoryEntry(const absl::string_view key,
                                      const absl::string_view value) {
  return user_history_predictor_->ClearHistoryEntry(key, value);
}

bool BasePredictor::Wait() { return user_history_predictor_->Wait(); }

bool BasePredictor::Sync() { return user_history_predictor_->Sync(); }

bool BasePredictor::Reload() { return user_history_predictor_->Reload(); }

// static
std::unique_ptr<PredictorInterface> DesktopPredictor::CreateDesktopPredictor(
    std::unique_ptr<PredictorInterface> dictionary_predictor,
    std::unique_ptr<PredictorInterface> user_history_predictor,
    const ConverterInterface &converter) {
  return std::make_unique<DesktopPredictor>(std::move(dictionary_predictor),
                                            std::move(user_history_predictor),
                                            converter);
}

DesktopPredictor::DesktopPredictor(
    std::unique_ptr<PredictorInterface> dictionary_predictor,
    std::unique_ptr<PredictorInterface> user_history_predictor,
    const ConverterInterface &converter)
    : BasePredictor(std::move(dictionary_predictor),
                    std::move(user_history_predictor), converter),
      predictor_name_("DesktopPredictor") {}

DesktopPredictor::~DesktopPredictor() = default;

bool DesktopPredictor::PredictForRequest(const ConversionRequest &request,
                                         Segments *segments) const {
  DCHECK(request.request_type() == ConversionRequest::PREDICTION ||
         request.request_type() == ConversionRequest::SUGGESTION ||
         request.request_type() == ConversionRequest::PARTIAL_PREDICTION ||
         request.request_type() == ConversionRequest::PARTIAL_SUGGESTION);

  if (request.config().presentation_mode()) {
    return false;
  }

  int size = kPredictionSize;
  if (request.request_type() == ConversionRequest::SUGGESTION) {
    size = std::clamp<int>(request.config().suggestions_size(), 1, 9);
  }

  bool result = false;
  int remained_size = size;
  ConversionRequest::Options options = request.options();
  options.max_user_history_prediction_candidates_size = size;
  options.max_user_history_prediction_candidates_size_for_zero_query = size;
  ConversionRequest request_for_prediction =
      ConversionRequestBuilder()
          .SetConversionRequestView(request)
          .SetOptions(std::move(options))
          .SetHistorySegmentsView(*segments)
          .Build();
  result |= user_history_predictor_->PredictForRequest(request_for_prediction,
                                                       segments);
  remained_size = size - static_cast<size_t>(GetCandidatesSize(*segments));

  // Do not call dictionary_predictor if the size of candidates get
  // >= suggestions_size.
  if (remained_size <= 0) {
    return result;
  }

  ConversionRequest::Options options2 = request_for_prediction.options();
  options2.max_dictionary_prediction_candidates_size = remained_size;
  const ConversionRequest request_for_prediction2 =
      ConversionRequestBuilder()
          .SetConversionRequestView(request_for_prediction)
          .SetOptions(std::move(options2))
          .SetHistorySegmentsView(*segments)
          .Build();
  result |= dictionary_predictor_->PredictForRequest(request_for_prediction2,
                                                     segments);
  return result;
}

// static
std::unique_ptr<PredictorInterface> MobilePredictor::CreateMobilePredictor(
    std::unique_ptr<PredictorInterface> dictionary_predictor,
    std::unique_ptr<PredictorInterface> user_history_predictor,
    const ConverterInterface &converter) {
  return std::make_unique<MobilePredictor>(std::move(dictionary_predictor),
                                           std::move(user_history_predictor),
                                           converter);
}

MobilePredictor::MobilePredictor(
    std::unique_ptr<PredictorInterface> dictionary_predictor,
    std::unique_ptr<PredictorInterface> user_history_predictor,
    const ConverterInterface &converter)
    : BasePredictor(std::move(dictionary_predictor),
                    std::move(user_history_predictor), converter),
      predictor_name_("MobilePredictor") {}

MobilePredictor::~MobilePredictor() = default;

ConversionRequest MobilePredictor::GetRequestForPredict(
    const ConversionRequest &request, const Segments &segments) {
  ConversionRequest::Options options = request.options();
  size_t history_prediction_size = GetHistoryPredictionSizeFromRequest(request);
  switch (request.request_type()) {
    case ConversionRequest::SUGGESTION: {
      options.max_user_history_prediction_candidates_size =
          history_prediction_size;
      options.max_user_history_prediction_candidates_size_for_zero_query = 4;
      options.max_dictionary_prediction_candidates_size = 20;
      break;
    }
    case ConversionRequest::PARTIAL_SUGGESTION: {
      options.max_dictionary_prediction_candidates_size = 20;
      break;
    }
    case ConversionRequest::PARTIAL_PREDICTION: {
      options.max_dictionary_prediction_candidates_size = kMobilePredictionSize;
      break;
    }
    case ConversionRequest::PREDICTION: {
      options.max_user_history_prediction_candidates_size =
          history_prediction_size;
      options.max_user_history_prediction_candidates_size_for_zero_query = 4;
      options.max_dictionary_prediction_candidates_size = kMobilePredictionSize;
      break;
    }
    default:
      DLOG(ERROR) << "Unexpected request type: " << request.request_type();
  }
  return ConversionRequestBuilder()
      .SetConversionRequestView(request)
      .SetOptions(std::move(options))
      .SetHistorySegmentsView(segments)
      .Build();
}

namespace {
// Fills empty lid and rid of candidates with the candidates of the same value.
void MaybeFillFallbackPos(Segments *segments) {
  for (Segment &segment : segments->conversion_segments()) {
    absl::flat_hash_map<absl::string_view, Segment::Candidate *> posless_cands;
    for (size_t ci = 0; ci < segment.candidates_size(); ++ci) {
      Segment::Candidate *cand = segment.mutable_candidate(ci);
      // Candidates with empty POS come before candidates with filled POS.
      if (cand->lid == 0 || cand->rid == 0) {
        posless_cands[cand->value] = cand;
        continue;
      }
      if (!posless_cands.contains(cand->value)) {
        continue;
      }

      Segment::Candidate *posless_cand = posless_cands[cand->value];
      if (posless_cand->lid == 0) {
        posless_cand->lid = cand->lid;
      }
      if (posless_cand->rid == 0) {
        posless_cand->rid = cand->rid;
      }
      if (posless_cand->lid != 0 && posless_cand->rid != 0) {
        posless_cands.erase(cand->value);
      }
      MOZC_CANDIDATE_LOG(cand, "Fallback POSes were filled.");
    }
  }
}
}  // namespace

bool MobilePredictor::PredictForRequest(const ConversionRequest &request,
                                        Segments *segments) const {
  DCHECK(request.request_type() == ConversionRequest::PREDICTION ||
         request.request_type() == ConversionRequest::SUGGESTION ||
         request.request_type() == ConversionRequest::PARTIAL_PREDICTION ||
         request.request_type() == ConversionRequest::PARTIAL_SUGGESTION);

  if (request.config().presentation_mode()) {
    return false;
  }

  DCHECK(segments);

  bool result = false;
  const ConversionRequest request_for_predict =
      GetRequestForPredict(request, *segments);

  // TODO(taku,toshiyuki): Must rewrite the logic.
  switch (request.request_type()) {
    case ConversionRequest::SUGGESTION: {
      // Suggestion is triggered at every character insertion.
      // So here we should use slow predictors.
      result |= user_history_predictor_->PredictForRequest(request_for_predict,
                                                           segments);
      result |= dictionary_predictor_->PredictForRequest(request_for_predict,
                                                         segments);
      break;
    }
    case ConversionRequest::PARTIAL_SUGGESTION: {
      // PARTIAL SUGGESTION can be triggered in a similar manner to that of
      // SUGGESTION. We don't call slow predictors by the same reason.
      result |= dictionary_predictor_->PredictForRequest(request_for_predict,
                                                         segments);
      break;
    }
    case ConversionRequest::PARTIAL_PREDICTION: {
      result |= dictionary_predictor_->PredictForRequest(request_for_predict,
                                                         segments);
      break;
    }
    case ConversionRequest::PREDICTION: {
      result |= user_history_predictor_->PredictForRequest(request_for_predict,
                                                           segments);
      result |= dictionary_predictor_->PredictForRequest(request_for_predict,
                                                         segments);
      break;
    }
    default:
      DLOG(FATAL) << "Unexpected request type: " << request.request_type();
  }

  MaybeFillFallbackPos(segments);
  return result;
}

}  // namespace mozc::prediction
