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
#include "session/config_handler.h"
#include "session/config.pb.h"
#include "prediction/predictor_interface.h"
#include "prediction/dictionary_predictor.h"
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

 public:
  UserHistoryPredictor *user_history_predictor_;
  DictionaryPredictor  *dictionary_predictor_;
  vector<PredictorInterface *> predictors_;
};

size_t GetCandidatesSize(const Segments &segments) {
  if (segments.conversion_segments_size() <= 0) {
    LOG(ERROR) << "No conversion segments found";
    return 0;
  }
  return segments.conversion_segment(0).candidates_size();
}

PredictorImpl::PredictorImpl()
    : user_history_predictor_(new UserHistoryPredictor),
      dictionary_predictor_(new DictionaryPredictor) {
  predictors_.push_back(user_history_predictor_);
  predictors_.push_back(dictionary_predictor_);
  DCHECK(user_history_predictor_);
  DCHECK(dictionary_predictor_);
}

PredictorImpl::~PredictorImpl() {
  delete user_history_predictor_;
  delete dictionary_predictor_;
  predictors_.clear();
}

void PredictorImpl::Finish(Segments *segments) {
  for (size_t i = 0; i < predictors_.size(); ++i) {
    predictors_[i]->Finish(segments);
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
  for (size_t i = 0; i < predictors_.size(); ++i) {
    predictors_[i]->Revert(segments);
  }
}

bool PredictorImpl::Predict(Segments *segments) const {
  bool result = false;

  int size = kPredictionSize;
  if (segments->request_type() == Segments::SUGGESTION) {
    size = min(9, max(1, static_cast<int>(GET_CONFIG(suggestions_size))));
  }

  for (size_t i = 0; i < predictors_.size(); ++i) {
    if (size <= 0) {
      break;
    }
    segments->set_max_prediction_candidates_size(static_cast<size_t>(size));
    result |= predictors_[i]->Predict(segments);
    size -= static_cast<int>(GetCandidatesSize(*segments));
  }

  return result;
}

bool PredictorImpl::ClearAllHistory() {
  return user_history_predictor_->ClearAllHistory();
}

bool PredictorImpl::ClearUnusedHistory() {
  return user_history_predictor_->ClearUnusedHistory();
}

bool PredictorImpl::Sync() {
  return user_history_predictor_->Sync();
}

PredictorInterface *g_predictor = NULL;
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
}  // namespace mozc
