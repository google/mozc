// Copyright 2010-2011, Google Inc.
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
// for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"

namespace mozc {

class Segments;
class DictionaryInterface;
class ImmutableConverterInterface;
struct Node;

// Dictioanry-based predictor
class DictionaryPredictor: public PredictorInterface {
 public:
  DictionaryPredictor();
  virtual ~DictionaryPredictor();

  bool Predict(Segments *segments) const;

 private:
  FRIEND_TEST(DictionaryPredictorTest, GetPredictionType);
  FRIEND_TEST(DictionaryPredictorTest,
              GetPredictionTypeTestWithZeroQuerySuggestion);
  FRIEND_TEST(DictionaryPredictorTest, IsZipCodeRequest);
  FRIEND_TEST(DictionaryPredictorTest, GetSVMScore);
  FRIEND_TEST(DictionaryPredictorTest, AggregateRealtimeConversion);
  FRIEND_TEST(DictionaryPredictorTest, AggregateUnigramPrediction);
  FRIEND_TEST(DictionaryPredictorTest, AggregateBigramPrediction);
  FRIEND_TEST(DictionaryPredictorTest, GetHistoryKeyAndValue);


  enum PredictionType {
    // don't need to show any suggestions.
    NO_PREDICTION = 0,
    // suggests from current key user is now typing
    UNIGRAM = 1,
    // suggests from the previous history key user typed before.
    BIGRAM = 2,
    // suggests from immutable_converter
    REALTIME = 4,
  };

  struct Result {
    Result() : node(NULL), type(NO_PREDICTION), score(0) {}
    Result(const Node *node_, PredictionType type_)
        : node(node_), type(type_), score(0) {}
    const Node *node;
    PredictionType type;
    int score;
  };

  class ResultCompare {
   public:
    bool operator() (const Result &a, const Result &b) const {
      return a.score < b.score;
    }
  };

  void AggregateRealtimeConversion(PredictionType type,
                                   Segments *segments,
                                   vector<Result> *results) const;
  void AggregateUnigramPrediction(PredictionType type,
                                  Segments *segments,
                                  vector<Result> *results) const;
  void AggregateBigramPrediction(PredictionType type,
                                 Segments *segments,
                                 vector<Result> *results) const;

  // SVM-based scoring function.
  void SetSVMScore(const Segments &segments,
                   vector<Result> *results) const;

  // Language model-based scoring function.
  void SetLMScore(const Segments &segments,
                  vector<Result> *results) const;

  // return true if key consistes of '0'-'9' or '-'
  bool IsZipCodeRequest(const string &key) const;

  // return history key/value.
  bool GetHistoryKeyAndValue(const Segments &segments,
                             string *key, string *value) const;

  // return PredictionType.
  // return value may be UNIGRAM | BIGRAM.
  PredictionType GetPredictionType(const Segments &segments) const;

  // return SVM score from feature.
  // |feature| is used as an internal buffer for calculating SVM score.
  int GetSVMScore(const string &query,
                  const string &key,
                  const string &value,
                  uint16 cost,
                  uint16 lid,
                  bool is_zip_code,
                  bool is_suggestion,
                  size_t total_candidates_size,
                  vector<pair<int, double> > *feature) const;

  DictionaryInterface *dictionary_;
  ImmutableConverterInterface *immutable_converter_;
};
}  // namespace mozc

#endif  // MOZC_PREDICTION_DICTIONARY_PREDICTOR_H_
