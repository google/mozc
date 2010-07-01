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

#ifndef MOZC_PREDICTION_DICTIONARY_PREDICTOR_H_
#define MOZC_PREDICTION_DICTIONARY_PREDICTOR_H_

#include <string>
#include <vector>
#include "base/base.h"
#include "prediction/predictor_interface.h"

namespace mozc {

class Segments;
class DictionaryInterface;

// Dictioanry-based predictor
class DictionaryPredictor: public PredictorInterface {
 public:
  DictionaryPredictor();
  virtual ~DictionaryPredictor();

  bool Predict(Segments *segments) const;

  static void MakeFeature(const string &query,
                          const string &key,
                          const string &value,
                          uint16 cost,
                          uint16 lid,
                          bool is_zip_code,
                          vector<pair<int, double> > *feature);

  // return true if key consistes of '0'-'9' or '-'
  static bool IsZipCodeRequest(const string &key);

 private:
  DictionaryInterface *dictionary_;
};
}  // namespace mozc

#endif  // MOZC_PREDICTION_DICTIONARY_PREDICTOR_H_
