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
#include "converter/segments.h"
#include "session/config_handler.h"
#include "session/config.pb.h"
#include "prediction/predictor.h"
#include "prediction/predictor_interface.h"
#include "prediction/dictionary_predictor.h"
#include "prediction/user_history_predictor.h"

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
}

Predictor::Predictor() : user_history_predictor_(NULL) {
  user_history_predictor_ = new UserHistoryPredictor;
  predictors_.push_back(user_history_predictor_);
  predictors_.push_back(new DictionaryPredictor);
  DCHECK(user_history_predictor_);
}

Predictor::~Predictor() {
  for (size_t i = 0; i < predictors_.size(); ++i) {
    delete predictors_[i];
  }
  predictors_.clear();
}

void Predictor::Finish(Segments *segments) {
  for (size_t i = 0; i < predictors_.size(); ++i) {
    predictors_[i]->Finish(segments);
  }
}

void Predictor::Revert(Segments *segments) {
  for (size_t i = 0; i < predictors_.size(); ++i) {
    predictors_[i]->Revert(segments);
  }
}

bool Predictor::Suggest(Segments *segments) const {
  bool result = false;
  int size = min(9, max(1, static_cast<int>(GET_CONFIG(suggestions_size))));

  for (size_t i = 0; i < predictors_.size(); ++i) {
    if (size <= 0) {
      break;
    }
    segments->set_max_prediction_candidates_size(static_cast<size_t>(size));
    result |= predictors_[i]->Suggest(segments);
    size -= static_cast<int>(GetCandidatesSize(*segments));
  }
  return result;
}

bool Predictor::Predict(Segments *segments) const {
  bool result = false;
  int size = kPredictionSize;
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

bool Predictor::ClearAllHistory() {
  return user_history_predictor_->ClearAllHistory();
}

bool Predictor::ClearUnusedHistory() {
  return user_history_predictor_->ClearUnusedHistory();
}

bool Predictor::Sync() {
  return user_history_predictor_->Sync();
}
}  // namespace mozc
