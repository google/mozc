// Copyright 2010-2012, Google Inc.
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

#include <string>
#include <vector>
#include "base/base.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "session/commands.pb.h"

// TODO(team): Implement ambiguity expansion for rewriters.
DEFINE_bool(enable_ambiguity_expansion, true,
            "Enable ambiguity trigger expansion for predictions");
DECLARE_bool(enable_expansion_for_dictionary_predictor);
DECLARE_bool(enable_expansion_for_user_history_predictor);

namespace mozc {
namespace {
const int kPredictionSize = 100;


size_t GetCandidatesSize(const Segments &segments) {
  if (segments.conversion_segments_size() <= 0) {
    LOG(ERROR) << "No conversion segments found";
    return 0;
  }
  return segments.conversion_segment(0).candidates_size();
}


}  // namespace

BasePredictor::BasePredictor(PredictorInterface *dictionary_predictor,
                             PredictorInterface *user_history_predictor,
                             PredictorInterface *extra_predictor)
    : dictionary_predictor_(dictionary_predictor),
      user_history_predictor_(user_history_predictor),
      extra_predictor_(extra_predictor) {
  DCHECK(dictionary_predictor_.get());
  DCHECK(user_history_predictor_.get());
  FLAGS_enable_expansion_for_dictionary_predictor =
      FLAGS_enable_ambiguity_expansion;
  FLAGS_enable_expansion_for_user_history_predictor =
      FLAGS_enable_ambiguity_expansion;
}

BasePredictor::~BasePredictor() {}

void BasePredictor::Finish(Segments *segments) {
  user_history_predictor_->Finish(segments);
  dictionary_predictor_->Finish(segments);
  if (extra_predictor_.get()) {
    extra_predictor_->Finish(segments);
  }

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

bool BasePredictor::Sync() {
  return user_history_predictor_->Sync();
}

bool BasePredictor::Reload() {
  return user_history_predictor_->Reload();
}

DefaultPredictor::DefaultPredictor(PredictorInterface *dictionary_predictor,
                                   PredictorInterface *user_history_predictor,
                                   PredictorInterface *extra_predictor)
    : BasePredictor(dictionary_predictor,
                    user_history_predictor,
                    extra_predictor),
      empty_request_(ConversionRequest()) {}

DefaultPredictor::~DefaultPredictor() {}

bool DefaultPredictor::Predict(Segments *segments) const {
  return PredictForRequest(empty_request_, segments);
}

bool DefaultPredictor::PredictForRequest(const ConversionRequest &request,
                                         Segments *segments) const {
  DCHECK(segments->request_type() == Segments::PREDICTION ||
         segments->request_type() == Segments::SUGGESTION ||
         segments->request_type() == Segments::PARTIAL_PREDICTION ||
         segments->request_type() == Segments::PARTIAL_SUGGESTION);

  if (GET_CONFIG(presentation_mode)) {
    return false;
  }

  int size = kPredictionSize;
  if (segments->request_type() == Segments::SUGGESTION) {
    size = min(9, max(1, static_cast<int>(GET_CONFIG(suggestions_size))));
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
  remained_size = size - static_cast<size_t>(GetCandidatesSize(*segments));


  return result;
}

}  // namespace mozc
