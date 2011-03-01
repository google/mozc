// Copyright 2010, Google Inc.
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
#include "converter/segments.h"
#include "session/commands.pb.h"
#include "session/config_handler.h"
#include "session/config.pb.h"
#include "prediction/conversion_predictor.h"
#include "prediction/dictionary_predictor.h"
#include "prediction/predictor_interface.h"
#include "prediction/user_history_predictor.h"

namespace mozc {
namespace {
const int kPredictionSize = 100;

class PredictorImpl : public PredictorInterface {
 public:
  PredictorImpl();
  virtual ~PredictorImpl();

  virtual bool Predict(Segments *segments) const;

  // Hook(s) for all mutable operations
  virtual void Finish(Segments *segments);

  // Revert the last Finish operation
  virtual void Revert(Segments *segments);

  // clear all history data of UserHistoryPredictor
  virtual bool ClearAllHistory();

  // clear unused history data of UserHistoryPredictor
  virtual bool ClearUnusedHistory();

  // Sync user history
  virtual bool Sync();

 private:
  // Load predictors
  void LoadPredictors(vector<PredictorInterface *> *predictors) const;
};

size_t GetCandidatesSize(const Segments &segments) {
  if (segments.conversion_segments_size() <= 0) {
    LOG(ERROR) << "No conversion segments found";
    return 0;
  }
  return segments.conversion_segment(0).candidates_size();
}

PredictorImpl::PredictorImpl() {}

PredictorImpl::~PredictorImpl() {}

void PredictorImpl::LoadPredictors(
    vector<PredictorInterface *> *predictors) const {
  predictors->clear();
  predictors->push_back(PredictorFactory::GetUserHistoryPredictor());
  predictors->push_back(PredictorFactory::GetDictionaryPredictor());
  predictors->push_back(PredictorFactory::GetConversionPredictor());
  for (size_t i = 0; i < predictors->size(); ++i) {
    DCHECK(predictors->at(i) != NULL);
  }
}

void PredictorImpl::Finish(Segments *segments) {
  vector<PredictorInterface *> predictors;
  LoadPredictors(&predictors);
  for (size_t i = 0; i < predictors.size(); ++i) {
    predictors[i]->Finish(segments);
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
  // TODO(taku): Ideally, we don't need to set Segment::key(), since
  // Segment::key() stores the request key.
  // Here we keep the original code not to break backward compatibility.
  segment->set_key(segment->candidate(0).key);
}

void PredictorImpl::Revert(Segments *segments) {
  vector<PredictorInterface *> predictors;
  LoadPredictors(&predictors);
  for (size_t i = 0; i < predictors.size(); ++i) {
    predictors[i]->Revert(segments);
  }
}

bool PredictorImpl::Predict(Segments *segments) const {
  vector<PredictorInterface *> predictors;
  LoadPredictors(&predictors);
  bool result = false;

  int size = kPredictionSize;
  if (segments->request_type() == Segments::SUGGESTION) {
    size = min(9, max(1, static_cast<int>(GET_CONFIG(suggestions_size))));
  }

  bool suppress_conversion_prediction = true;
  for (size_t i = 0; i < predictors.size(); ++i) {
    if (size <= 0) {
      break;
    }
    if (predictors[i] == PredictorFactory::GetConversionPredictor() &&
        suppress_conversion_prediction) {
      // Only requires one result from conversion predictor.
      // Showing multiple conversion results would not be useful.
      segments->set_max_prediction_candidates_size(1);
    } else {
      segments->set_max_prediction_candidates_size(static_cast<size_t>(size));
    }
    result |= predictors[i]->Predict(segments);
    size -= static_cast<int>(GetCandidatesSize(*segments));
  }

  // TODO(toshiyuki): It's nice if we have the system to rewrite
  // the ranking of candidates from predictors

  return result;
}

bool PredictorImpl::ClearAllHistory() {
  PredictorInterface *user_history_predictor =
      PredictorFactory::GetUserHistoryPredictor();
  DCHECK(user_history_predictor != NULL);
  return user_history_predictor->ClearAllHistory();
}

bool PredictorImpl::ClearUnusedHistory() {
  PredictorInterface *user_history_predictor =
      PredictorFactory::GetUserHistoryPredictor();
  DCHECK(user_history_predictor);
  return user_history_predictor->ClearUnusedHistory();
}

bool PredictorImpl::Sync() {
  PredictorInterface *user_history_predictor =
      PredictorFactory::GetUserHistoryPredictor();
  DCHECK(user_history_predictor);
  return user_history_predictor->Sync();
}

PredictorInterface *g_predictor = NULL;

PredictorInterface *g_user_history_predictor = NULL;
PredictorInterface *g_dictionary_predictor = NULL;
PredictorInterface *g_conversion_predictor = NULL;
}  // namespace

PredictorInterface *PredictorFactory::GetPredictor() {
  if (g_predictor == NULL) {
    return Singleton<PredictorImpl>::get();
  } else {
    return g_predictor;
  }
}

void PredictorFactory::SetPredictor(PredictorInterface *predictor) {
  g_predictor = predictor;
}

PredictorInterface *PredictorFactory::GetUserHistoryPredictor() {
  if (g_user_history_predictor == NULL) {
    return Singleton<UserHistoryPredictor>::get();
  } else {
    return g_user_history_predictor;
  }
}

void PredictorFactory::SetUserHistoryPredictor(PredictorInterface *predictor) {
  g_user_history_predictor = predictor;
}

PredictorInterface *PredictorFactory::GetDictionaryPredictor() {
  if (g_dictionary_predictor == NULL) {
    return Singleton<DictionaryPredictor>::get();
  } else {
    return g_dictionary_predictor;
  }
}

void PredictorFactory::SetDictionaryPredictor(PredictorInterface *predictor) {
  g_dictionary_predictor = predictor;
}

PredictorInterface *PredictorFactory::GetConversionPredictor() {
  if (g_conversion_predictor == NULL) {
    return Singleton<ConversionPredictor>::get();
  } else {
    return g_conversion_predictor;
  }
}

void PredictorFactory::SetConversionPredictor(PredictorInterface *predictor) {
  g_conversion_predictor = predictor;
}
}  // namespace mozc
