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
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "absl/flags/flag.h"
#include "absl/memory/memory.h"

namespace mozc {
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
    std::unique_ptr<PredictorInterface> user_history_predictor)
    : dictionary_predictor_(std::move(dictionary_predictor)),
      user_history_predictor_(std::move(user_history_predictor)) {
  DCHECK(dictionary_predictor_);
  DCHECK(user_history_predictor_);
}

BasePredictor::~BasePredictor() {}

void BasePredictor::Finish(const ConversionRequest &request,
                           Segments *segments) {
  user_history_predictor_->Finish(request, segments);
  dictionary_predictor_->Finish(request, segments);

  if (segments->conversion_segments_size() < 1 ||
      segments->request_type() == Segments::CONVERSION) {
    return;
  }
  Segment *segment = segments->mutable_conversion_segment(0);
  if (segment->candidates_size() < 1) {
    return;
  }
  // update the key as the original key only contains
  // the 'prefix'.
  // note that candidate key may be different from request key (=segment key)
  // due to suggestion/prediction.
  segment->set_key(segment->candidate(0).key);
}

// Since DictionaryPredictor is immutable, no need
// to call DictionaryPredictor::Revert/Clear*/Finish methods.
void BasePredictor::Revert(Segments *segments) {
  user_history_predictor_->Revert(segments);
}

bool BasePredictor::ClearAllHistory() {
  return user_history_predictor_->ClearAllHistory();
}

bool BasePredictor::ClearUnusedHistory() {
  return user_history_predictor_->ClearUnusedHistory();
}

bool BasePredictor::ClearHistoryEntry(const std::string &key,
                                      const std::string &value) {
  return user_history_predictor_->ClearHistoryEntry(key, value);
}

bool BasePredictor::Wait() { return user_history_predictor_->Wait(); }

bool BasePredictor::Sync() { return user_history_predictor_->Sync(); }

bool BasePredictor::Reload() { return user_history_predictor_->Reload(); }

// static
std::unique_ptr<PredictorInterface> DefaultPredictor::CreateDefaultPredictor(
    std::unique_ptr<PredictorInterface> dictionary_predictor,
    std::unique_ptr<PredictorInterface> user_history_predictor) {
  return absl::make_unique<DefaultPredictor>(std::move(dictionary_predictor),
                                             std::move(user_history_predictor));
}

DefaultPredictor::DefaultPredictor(
    std::unique_ptr<PredictorInterface> dictionary_predictor,
    std::unique_ptr<PredictorInterface> user_history_predictor)
    : BasePredictor(std::move(dictionary_predictor),
                    std::move(user_history_predictor)),
      empty_request_(),
      predictor_name_("DefaultPredictor") {}

DefaultPredictor::~DefaultPredictor() = default;

bool DefaultPredictor::PredictForRequest(const ConversionRequest &request,
                                         Segments *segments) const {
  DCHECK(segments->request_type() == Segments::PREDICTION ||
         segments->request_type() == Segments::SUGGESTION ||
         segments->request_type() == Segments::PARTIAL_PREDICTION ||
         segments->request_type() == Segments::PARTIAL_SUGGESTION);

  if (request.config().presentation_mode()) {
    return false;
  }

  int size = kPredictionSize;
  if (segments->request_type() == Segments::SUGGESTION) {
    size = std::min(
        9, std::max(1, static_cast<int>(request.config().suggestions_size())));
  }

  bool result = false;
  int remained_size = size;
  segments->set_max_prediction_candidates_size(static_cast<size_t>(size));
  result |= user_history_predictor_->PredictForRequest(request, segments);
  remained_size = size - static_cast<size_t>(GetCandidatesSize(*segments));

  // Do not call dictionary_predictor if the size of candidates get
  // >= suggestions_size.
  if (remained_size <= 0) {
    return result;
  }

  segments->set_max_prediction_candidates_size(remained_size);
  result |= dictionary_predictor_->PredictForRequest(request, segments);
  return result;
}

// static
std::unique_ptr<PredictorInterface> MobilePredictor::CreateMobilePredictor(
    std::unique_ptr<PredictorInterface> dictionary_predictor,
    std::unique_ptr<PredictorInterface> user_history_predictor) {
  return absl::make_unique<MobilePredictor>(std::move(dictionary_predictor),
                                            std::move(user_history_predictor));
}

MobilePredictor::MobilePredictor(
    std::unique_ptr<PredictorInterface> dictionary_predictor,
    std::unique_ptr<PredictorInterface> user_history_predictor)
    : BasePredictor(std::move(dictionary_predictor),
                    std::move(user_history_predictor)),
      empty_request_(),
      predictor_name_("MobilePredictor") {}

MobilePredictor::~MobilePredictor() {}

bool MobilePredictor::PredictForRequest(const ConversionRequest &request,
                                        Segments *segments) const {
  DCHECK(segments->request_type() == Segments::PREDICTION ||
         segments->request_type() == Segments::SUGGESTION ||
         segments->request_type() == Segments::PARTIAL_PREDICTION ||
         segments->request_type() == Segments::PARTIAL_SUGGESTION);

  if (request.config().presentation_mode()) {
    return false;
  }

  DCHECK(segments);

  bool result = false;
  size_t size = 0;
  size_t history_prediction_size = GetHistoryPredictionSizeFromRequest(request);

  // TODO(taku,toshiyuki): Must rewrite the logic.
  switch (segments->request_type()) {
    case Segments::SUGGESTION: {
      // Suggestion is triggered at every character insertion.
      // So here we should use slow predictors.
      size = GetCandidatesSize(*segments) + history_prediction_size;
      segments->set_max_prediction_candidates_size(size);
      result |= user_history_predictor_->PredictForRequest(request, segments);

      size = GetCandidatesSize(*segments) + 20;
      segments->set_max_prediction_candidates_size(size);
      result |= dictionary_predictor_->PredictForRequest(request, segments);
      break;
    }
    case Segments::PARTIAL_SUGGESTION: {
      // PARTIAL SUGGESTION can be triggered in a similar manner to that of
      // SUGGESTION. We don't call slow predictors by the same reason.
      size = GetCandidatesSize(*segments) + 20;
      segments->set_max_prediction_candidates_size(size);
      result |= dictionary_predictor_->PredictForRequest(request, segments);
      break;
    }
    case Segments::PARTIAL_PREDICTION: {
      segments->set_max_prediction_candidates_size(kMobilePredictionSize);
      result |= dictionary_predictor_->PredictForRequest(request, segments);
      break;
    }
    case Segments::PREDICTION: {
      size = GetCandidatesSize(*segments) + history_prediction_size;
      segments->set_max_prediction_candidates_size(size);
      result |= user_history_predictor_->PredictForRequest(request, segments);

      segments->set_max_prediction_candidates_size(kMobilePredictionSize);
      result |= dictionary_predictor_->PredictForRequest(request, segments);
      break;
    }
    default: {
    }  // Never reach here
  }

  return result;
}

}  // namespace mozc
