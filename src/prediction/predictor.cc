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

#include <string>
#include <vector>
#include "base/base.h"
#include "base/singleton.h"
#include "config/config_handler.h"
#include "config/config.pb.h"
#include "converter/segments.h"
#include "session/commands.pb.h"
#include "prediction/dictionary_predictor.h"
#include "prediction/predictor.h"
#include "prediction/user_history_predictor.h"

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

PredictorInterface *g_predictor = NULL;
PredictorInterface *g_user_history_predictor = NULL;
PredictorInterface *g_dictionary_predictor = NULL;


}  // namespace

BasePredictor::BasePredictor() {
  FLAGS_enable_expansion_for_dictionary_predictor =
      FLAGS_enable_ambiguity_expansion;
  FLAGS_enable_expansion_for_user_history_predictor =
      FLAGS_enable_ambiguity_expansion;
}

BasePredictor::~BasePredictor() {}

void BasePredictor::Finish(Segments *segments) {
  PredictorFactory::GetUserHistoryPredictor()->Finish(segments);
  PredictorFactory::GetDictionaryPredictor()->Finish(segments);

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
  PredictorInterface *user_history_predictor =
      PredictorFactory::GetUserHistoryPredictor();
  DCHECK(user_history_predictor != NULL);
  user_history_predictor->Revert(segments);
}

bool BasePredictor::ClearAllHistory() {
  PredictorInterface *user_history_predictor =
      PredictorFactory::GetUserHistoryPredictor();
  DCHECK(user_history_predictor != NULL);
  return user_history_predictor->ClearAllHistory();
}

bool BasePredictor::ClearUnusedHistory() {
  PredictorInterface *user_history_predictor =
      PredictorFactory::GetUserHistoryPredictor();
  DCHECK(user_history_predictor);
  return user_history_predictor->ClearUnusedHistory();
}

bool BasePredictor::Sync() {
  PredictorInterface *user_history_predictor =
      PredictorFactory::GetUserHistoryPredictor();
  DCHECK(user_history_predictor);
  return user_history_predictor->Sync();
}

bool BasePredictor::Reload() {
  PredictorInterface *user_history_predictor =
      PredictorFactory::GetUserHistoryPredictor();
  DCHECK(user_history_predictor);
  return user_history_predictor->Reload();
}

DefaultPredictor::DefaultPredictor() : empty_request_(ConversionRequest()) {}

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

  PredictorInterface *user_history_predictor =
      PredictorFactory::GetUserHistoryPredictor();
  PredictorInterface *dictionary_predictor =
      PredictorFactory::GetDictionaryPredictor();
  DCHECK(user_history_predictor);
  DCHECK(dictionary_predictor);

  int remained_size = size;
  segments->set_max_prediction_candidates_size(static_cast<size_t>(size));
  result |= user_history_predictor->PredictForRequest(request, segments);
  remained_size = size - static_cast<size_t>(GetCandidatesSize(*segments));

  // Do not call dictionary_predictor if the size of candidates get
  // >= suggestions_size.
  if (remained_size <= 0) {
    return result;
  }

  segments->set_max_prediction_candidates_size(remained_size);
  result |= dictionary_predictor->PredictForRequest(request, segments);
  remained_size = size - static_cast<size_t>(GetCandidatesSize(*segments));


  return result;
}


#define GET_PREDICTOR(PredictorClass, predictor_instance) do { \
  if (predictor_instance == NULL) { \
    return Singleton<PredictorClass>::get(); \
  } else { \
    return predictor_instance; \
  } \
} while (0)

PredictorInterface *PredictorFactory::GetPredictor() {
    GET_PREDICTOR(DefaultPredictor, g_predictor);
}

void PredictorFactory::SetPredictor(PredictorInterface *predictor) {
  g_predictor = predictor;
}

PredictorInterface *PredictorFactory::GetUserHistoryPredictor() {
  GET_PREDICTOR(UserHistoryPredictor, g_user_history_predictor);
}

void PredictorFactory::SetUserHistoryPredictor(PredictorInterface *predictor) {
  g_user_history_predictor = predictor;
}

PredictorInterface *PredictorFactory::GetDictionaryPredictor() {
  GET_PREDICTOR(DictionaryPredictor, g_dictionary_predictor);
}

void PredictorFactory::SetDictionaryPredictor(PredictorInterface *predictor) {
  g_dictionary_predictor = predictor;
}


#undef GET_PREDICTOR
}  // namespace mozc
