// Copyright 2010-2014, Google Inc.
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

#ifndef MOZC_PREDICTION_PREDICTOR_H_
#define MOZC_PREDICTION_PREDICTOR_H_

#include <string>

#include "base/scoped_ptr.h"
#include "converter/conversion_request.h"
#include "prediction/predictor_interface.h"

namespace mozc {

class BasePredictor : public PredictorInterface {
 public:
  // Initializes the composite of predictor with given sub-predictors. All the
  // predictors are owned by this class and deleted on destruction of this
  // instance.
  BasePredictor(PredictorInterface *dictionary_predictor,
                PredictorInterface *user_history_predictor);
  virtual ~BasePredictor();

  // Overwrite predictor
  virtual bool PredictForRequest(const ConversionRequest &request,
                                 Segments *segments) const = 0;

  // Hook(s) for all mutable operations.
  virtual void Finish(Segments *segments);

  // Reverts the last Finish operation.
  virtual void Revert(Segments *segments);

  // Clears all history data of UserHistoryPredictor.
  virtual bool ClearAllHistory();

  // Clears unused history data of UserHistoryPredictor.
  virtual bool ClearUnusedHistory();

  // Clears a specific user history data of UserHistoryPredictor.
  virtual bool ClearHistoryEntry(const string &key, const string &value);

  // Syncs user history.
  virtual bool Sync();

  // Reloads usre history.
  virtual bool Reload();

  // Waits for syncer to complete.
  virtual bool WaitForSyncerForTest();

  virtual const string &GetPredictorName() const = 0;

 protected:
  scoped_ptr<PredictorInterface> dictionary_predictor_;
  scoped_ptr<PredictorInterface> user_history_predictor_;
};

// TODO(team): The name should be DesktopPredictor
class DefaultPredictor : public BasePredictor {
 public:
  static PredictorInterface *CreateDefaultPredictor(
      PredictorInterface *dictionary_predictor,
      PredictorInterface *user_history_predictor);

  DefaultPredictor(PredictorInterface *dictionary_predictor,
                   PredictorInterface *user_history_predictor);
  virtual ~DefaultPredictor();

  virtual bool PredictForRequest(const ConversionRequest &request,
                                 Segments *segments) const;

  virtual const string &GetPredictorName() const { return predictor_name_; }

 private:
  const ConversionRequest empty_request_;
  const string predictor_name_;
};

class MobilePredictor : public BasePredictor {
 public:
  static PredictorInterface *CreateMobilePredictor(
      PredictorInterface *dictionary_predictor,
      PredictorInterface *user_history_predictor);

  MobilePredictor(PredictorInterface *dictionary_predictor,
                  PredictorInterface *user_history_predictor);
  virtual ~MobilePredictor();

  virtual bool PredictForRequest(const ConversionRequest &request,
                                 Segments *segments) const;

  virtual const string &GetPredictorName() const { return predictor_name_; }

 private:
  const ConversionRequest empty_request_;
  const string predictor_name_;
};

}  // namespace mozc

#endif  // MOZC_PREDICTION_INTERFACE_H_
