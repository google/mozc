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

#ifndef MOZC_PREDICTION_DICTIONARY_PREDICTOR_H_
#define MOZC_PREDICTION_DICTIONARY_PREDICTOR_H_

#include <string>
#include <vector>
#include "base/base.h"
#include "prediction/predictor_interface.h"
// for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"

namespace mozc {

class ConnectorInterface;
class ConversionRequest;
class DictionaryInterface;
class ImmutableConverterInterface;
class NodeAllocatorInterface;
class Segments;
class SegmenterInterface;
struct Node;

// Dictioanry-based predictor
class DictionaryPredictor: public PredictorInterface {
 public:
  DictionaryPredictor();
  explicit DictionaryPredictor(SegmenterInterface *segmenter);
  virtual ~DictionaryPredictor();

  bool Predict(Segments *segments) const;
  bool PredictForRequest(const ConversionRequest &request,
                         Segments *segments) const;

 protected:
  // Protected members for unittesting
  // For use util method accessing private members, made them protected.
  enum PredictionType {
    // don't need to show any suggestions.
    NO_PREDICTION = 0,
    // suggests from current key user is now typing
    UNIGRAM = 1,
    // suggests from the previous history key user typed before.
    BIGRAM = 2,
    // suggests from immutable_converter
    REALTIME = 4,
    // add suffixes like "さん", "が" which matches to the pevious context.
    SUFFIX =  8,
  };

  struct Result {
    Result() : node(NULL), type(NO_PREDICTION), cost(0) {}
    Result(const Node *node_, PredictionType type_)
        : node(node_), type(type_), cost(0) {}
    const Node *node;
    PredictionType type;
    int cost;
  };

  // On MSVS2008/2010, TestableDictionaryPredictor::Result(node, type) causes
  // a compile error even if you change the access right of it to public.
  // You can use TestableDictionaryPredictor::MakeResult(node, type) instead.
  static Result MakeResult(const Node *node, PredictionType type) {
    return Result(node, type);
  }

  void AggregateRealtimeConversion(PredictionType type,
                                   Segments *segments,
                                   NodeAllocatorInterface *allocator,
                                   vector<Result> *results) const;

  void AggregateUnigramPrediction(PredictionType type,
                                  const ConversionRequest &request,
                                  Segments *segments,
                                  NodeAllocatorInterface *allocator,
                                  vector<Result> *results) const;

  void AggregateBigramPrediction(PredictionType type,
                                 const ConversionRequest &request,
                                 Segments *segments,
                                 NodeAllocatorInterface *allocator,
                                 vector<Result> *results) const;

  void AggregateSuffixPrediction(PredictionType type,
                                 const ConversionRequest &request,
                                 Segments *segments,
                                 NodeAllocatorInterface *allocator,
                                 vector<Result> *results) const;

  void ApplyPenaltyForKeyExpansion(const Segments &segments,
                                   vector<Result> *results) const;

 private:
  FRIEND_TEST(DictionaryPredictorTest, GetPredictionType);
  FRIEND_TEST(DictionaryPredictorTest,
              GetPredictionTypeTestWithZeroQuerySuggestion);
  FRIEND_TEST(DictionaryPredictorTest, IsZipCodeRequest);
  FRIEND_TEST(DictionaryPredictorTest, GetRealtimeCandidateMaxSize);
  FRIEND_TEST(DictionaryPredictorTest, GetRealtimeCandidateMaxSizeForMixed);
  FRIEND_TEST(DictionaryPredictorTest, AggregateRealtimeConversion);
  FRIEND_TEST(DictionaryPredictorTest, GetUnigramCandidateCutoffThreshold);
  FRIEND_TEST(DictionaryPredictorTest, AggregateUnigramPrediction);
  FRIEND_TEST(DictionaryPredictorTest, AggregateBigramPrediction);
  FRIEND_TEST(DictionaryPredictorTest, AggregateSuffixPrediction);
  FRIEND_TEST(DictionaryPredictorTest, ZeroQuerySuggestionAfterNumbers);
  FRIEND_TEST(DictionaryPredictorTest, GetHistoryKeyAndValue);
  FRIEND_TEST(DictionaryPredictorTest, RealtimeConversionStartingWithAlphabets);
  FRIEND_TEST(DictionaryPredictorTest, IsAggressiveSuggestion);
  FRIEND_TEST(DictionaryPredictorTest, LookupKeyValueFromDictionary);
  FRIEND_TEST(DictionaryPredictorTest,
              RealtimeConversionWithSpellingCorrection);
  FRIEND_TEST(DictionaryPredictorTest, GetMissSpelledPosition);
  FRIEND_TEST(DictionaryPredictorTest, RemoveMissSpelledCandidates);
  FRIEND_TEST(DictionaryPredictorTest, ConformCharacterWidthToPreference);
  FRIEND_TEST(DictionaryPredictorTest, SetLMCost);

  class ResultCompare {
   public:
    bool operator() (const Result &a, const Result &b) const {
      return a.cost > b.cost;
    }
  };

  // Return false if no results were aggregated.
  bool AggregatePrediction(const ConversionRequest &request,
                           Segments *segments,
                           NodeAllocatorInterface *allocator,
                           vector<Result> *results) const;

  void SetCost(const Segments &segments, vector<Result> *results) const;

  // Remove prediciton by setting NO_PREDICTION to result type if necessary.
  void RemovePrediction(const Segments &segments,
                        vector<Result> *results) const;

  bool AddPredictionToCandidates(Segments *segments,
                                 vector<Result> *results) const;

  const Node *GetPredictiveNodes(const DictionaryInterface *dictionary,
                                 const string &history_key,
                                 const ConversionRequest &request,
                                 const Segments &segments,
                                 NodeAllocatorInterface *allocator) const;

  // return the position of mis-spelled character position
  //
  // Example1:
  // key: "れみおめろん"
  // value: "レミオロメン"
  // returns 3
  //
  // Example3:
  // key: "ろっぽんぎ"5
  // value: "六本木"
  // returns 5 (charslen("六本木"))
  size_t GetMissSpelledPosition(const string &key,
                                const string &value) const;

  // return true if key/value is in dictionary.
  const Node *LookupKeyValueFromDictionary(
      const string &key,
      const string &value,
      NodeAllocatorInterface *allocator) const;

  // return language model cost of |node|
  // |rid| is the right id of previous word (node).
  // if |rid| is uknown, set 0 as a default value.
  int GetLMCost(PredictionType type, const Node &node, int rid) const;

  // Given the results aggregated by aggregates, remove
  // miss-spelled results from the |results|.
  // we don't directly remove miss-spelled result but set
  // result[i].type = NO_PREDICTION.
  //
  // Here's the basic step of removal:
  // Case1:
  // result1: "あぼがど" => "アボガド"
  // result2: "あぼがど" => "アボカド" (spelling correction)
  // result3: "あぼかど" => "アボカド"
  // In this case, we can remove result 1 and 2.
  // If there exists the same result2.key in result1,3 and
  // the same result2.value in result1,3, we can remove the
  // 1) spelling correction candidate 2) candidate having
  // the same key as the spelling correction candidate.
  //
  // Case2:
  // result1: "あぼかど" => "アボカド"
  // result2: "あぼがど" => "アボカド" (spelling correction)
  // In this case, remove result2.
  //
  // Case3:
  // result1: "あぼがど" => "アボガド"
  // result2: "あぼがど" => "アボカド" (spelling correction)
  // In this case,
  //   a) user input: あ,あぼ,あぼ => remove result1, result2
  //   b) user input: あぼが,あぼがど  => remove result1
  //
  // let |same_key_size| and |same_value_size| be the number of
  // non-spelling-correction-candidates who have the same key/value as
  // spelling-correction-candidate respectively.
  //
  // if (same_key_size > 0 && same_value_size > 0) {
  //   remove spelling correction and candidates having the
  //   same key as the spelling correction.
  // } else if (same_key_size == 0 && same_value_size > 0) {
  //   remove spelling correction
  // } else {
  //   do nothing.
  // }
  void RemoveMissSpelledCandidates(size_t request_key_len,
                                   vector<Result> *results) const;

  // Scoring function which takes prediction bounus into account.
  // It basically reranks the candidate by lang_prob * (1 + remain_len).
  void SetPredictionCost(const Segments &segments,
                         vector<Result> *results) const;

  // Language model-based scoring function.
  void SetLMCost(const Segments &segments,
                 vector<Result> *results) const;

  // return true if key consistes of '0'-'9' or '-'
  bool IsZipCodeRequest(const string &key) const;

  // return true if the suggestion is classified
  // as "aggressive".
  bool IsAggressiveSuggestion(
      size_t query_len, size_t key_len, int32 cost,
      bool is_suggestion, size_t total_candidates_size) const;

  // return history key/value.
  bool GetHistoryKeyAndValue(const Segments &segments,
                             string *key, string *value) const;

  // return PredictionType.
  // return value may be UNIGRAM | BIGRAM | REALTIME | SUFFIX.
  PredictionType GetPredictionType(const Segments &segments) const;

  // return max size of realtime candidates.
  size_t GetRealtimeCandidateMaxSize(const Segments &segments,
                                     bool mixed_conversion,
                                     size_t max_size) const;

  // return cutoff threshold of unigram candidates.
  // AggregateUnigramPrediction method does not return any candidates
  // if there are too many (>= cutoff threshold) eligible candidates.
  // This behavior prevents a user from seeing too many prefix-match
  // candidates.
  size_t GetUnigramCandidateCutoffThreshold(const Segments &segments,
                                            bool mixed_conversion) const;

  DictionaryInterface *dictionary_;
  DictionaryInterface *suffix_dictionary_;
  ConnectorInterface *connector_;
  const SegmenterInterface *segmenter_;
  ImmutableConverterInterface *immutable_converter_;
};
}  // namespace mozc

#endif  // MOZC_PREDICTION_DICTIONARY_PREDICTOR_H_
