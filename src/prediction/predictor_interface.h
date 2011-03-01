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

#ifndef MOZC_PREDICTION_PREDICTOR_INTERFACE_H_
#define MOZC_PREDICTION_PREDICTOR_INTERFACE_H_

namespace mozc {

class Segments;

class PredictorInterface {
 public:
  // return suggestions.
  // You may need to change the behavior according to the
  // Segments::request_type flag
  // SUGGESTION: automatic suggestions
  // PREDICTION: invoked only when user pushes "tab" key.
  // less aggressive than SUGGESTION mode.
  virtual bool Predict(Segments *segments) const = 0;

  // Hook(s) for all mutable operations
  virtual void Finish(Segments *segments) {}

  // Revert the last Finish operation
  virtual void Revert(Segments *segments) {}

  // clear all history data of UserHistoryPredictor
  virtual bool ClearAllHistory() { return true; }

  // clear unused history data of UserHistoryPredictor
  virtual bool ClearUnusedHistory() { return true; }

  // Sync user history
  virtual bool Sync() { return true; }

  // clear internal data
  virtual void Clear() {}

 protected:
  // don't allow users to call constructor and destructor
  PredictorInterface() {}
  virtual ~PredictorInterface() {}
};

// factory for making "default" predictors
class PredictorFactory {
 public:
  // return singleton object
  static PredictorInterface *GetPredictor();
  static PredictorInterface *GetUserHistoryPredictor();
  static PredictorInterface *GetDictionaryPredictor();
  static PredictorInterface *GetConversionPredictor();

  // dependency injection for unittesting
  static void SetPredictor(PredictorInterface *predictor);
  static void SetUserHistoryPredictor(PredictorInterface *predictor);
  static void SetDictionaryPredictor(PredictorInterface *predictor);
  static void SetConversionPredictor(PredictorInterface *predictor);

 private:
  PredictorFactory() {}
  ~PredictorFactory() {}
};
}  // namespace mozc

#endif  // MOZC_PREDICTION_PREDICTOR_INTERFACE_H_
