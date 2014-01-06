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

#ifndef MOZC_PREDICTION_DICTIONARY_PREDICTOR_H_
#define MOZC_PREDICTION_DICTIONARY_PREDICTOR_H_

#include <functional>
#include <string>
#include <vector>
#include "base/util.h"
#include "converter/node.h"
#include "prediction/predictor_interface.h"
// for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"

namespace mozc {

class ConnectorInterface;
class ConversionRequest;
class ConverterInterface;
class DictionaryInterface;
class ImmutableConverterInterface;
class NodeAllocatorInterface;
class POSMatcher;
class SegmenterInterface;
class Segments;
class SuggestionFilter;

// Dictionary-based predictor
class DictionaryPredictor : public PredictorInterface {
 public:
  // Initializes a predictor with given references to submodules. Note that
  // pointers are not owned by the class and to be deleted by the caller.
  DictionaryPredictor(const ConverterInterface *converter,
                      const ImmutableConverterInterface *immutable_converter,
                      const DictionaryInterface *dictionary,
                      const DictionaryInterface *suffix_dictionary,
                      const ConnectorInterface *connector,
                      const SegmenterInterface *segmenter,
                      const POSMatcher *pos_matcher,
                      const SuggestionFilter *suggestion_filter);
  virtual ~DictionaryPredictor();

  virtual bool PredictForRequest(const ConversionRequest &request,
                                 Segments *segments) const;

  virtual const string &GetPredictorName() const { return predictor_name_; }

 protected:
  // Protected members for unittesting
  // For use util method accessing private members, made them protected.
  // http://code.google.com/p/googletest/wiki/FAQ
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
    // add English words.
    ENGLISH = 16,
    // add prediciton to type corrected keys
    TYPING_CORRECTION = 32,

    // Suggests from |converter_|. The difference from REALTIME is that it uses
    // the full converter with rewriter, history, etc.
    // TODO(noriyukit): This label should be integrated with REALTIME. This is
    // why 65536 is used to indicate that it is a temporary assignment.
    REALTIME_TOP = 65536,
  };
  // Bitfield to store a set of PredictionType.
  typedef int32 PredictionTypes;

  struct Result {
    Result() : node(NULL), types(NO_PREDICTION), cost(0) {}
    Result(const Node *node_, PredictionTypes types_)
        : node(node_), types(types_), cost(0) {}
    Result(const Node *node_, PredictionTypes types_,
           const vector<pair<int, int> > &inner_segment_boundary_)
        : node(node_), types(types_), cost(0),
          inner_segment_boundary(inner_segment_boundary_) {}
    const Node *node;
    // Indicating which PredictionType creates this instance.
    // UNIGRAM, BIGRAM, REALTIME, SUFFIX, ENGLISH or TYPING_CORRECTION
    // is set exclusively.
    // TODO(matsuzakit): Using PredictionTypes both as input and output
    //                   makes the code complex. Let's split them.
    PredictionTypes types;
    int cost;
    // Boundary information for realtime conversion.
    // This will be set only for realtime conversion result candidates.
    // This contains inner segment size for key and value.
    // If the candidate key and value are
    // "わたしの|なまえは|なかのです", " 私の|名前は|中野です",
    // |inner_segment_boundary| have [(4,2), (4, 3), (5, 4)].
    vector<pair<int, int> > inner_segment_boundary;
  };

  // On MSVS2008/2010, TestableDictionaryPredictor::Result(node, types) causes a
  // compile error even if you change the access right of it to public.  You can
  // use TestableDictionaryPredictor::MakeResult(node, types) instead.
  static Result MakeResult(const Node *node, PredictionTypes types) {
    return Result(node, types);
  }

  void AggregateRealtimeConversion(PredictionTypes types,
                                   const ConversionRequest &request,
                                   Segments *segments,
                                   NodeAllocatorInterface *allocator,
                                   vector<Result> *results) const;

  void AggregateUnigramPrediction(PredictionTypes types,
                                  const ConversionRequest &request,
                                  Segments *segments,
                                  NodeAllocatorInterface *allocator,
                                  vector<Result> *results) const;

  void AggregateBigramPrediction(PredictionTypes types,
                                 const ConversionRequest &request,
                                 Segments *segments,
                                 NodeAllocatorInterface *allocator,
                                 vector<Result> *results) const;

  void AggregateSuffixPrediction(PredictionTypes types,
                                 const ConversionRequest &request,
                                 Segments *segments,
                                 NodeAllocatorInterface *allocator,
                                 vector<Result> *results) const;

  void AggregateEnglishPrediction(PredictionTypes types,
                                  const ConversionRequest &request,
                                  Segments *segments,
                                  NodeAllocatorInterface *allocator,
                                  vector<Result> *results) const;
  void AggregateTypeCorrectingPrediction(PredictionTypes types,
                                         const ConversionRequest &request,
                                         Segments *segments,
                                         NodeAllocatorInterface *allocator,
                                         vector<Result> *results) const;

  void ApplyPenaltyForKeyExpansion(const Segments &segments,
                                   vector<Result> *results) const;

  bool AddPredictionToCandidates(const ConversionRequest &request,
                                 Segments *segments,
                                 vector<Result> *results) const;

 private:
  FRIEND_TEST(DictionaryPredictorTest, GetPredictionTypes);
  FRIEND_TEST(DictionaryPredictorTest,
              GetPredictionTypesTestWithTypingCorrection);
  FRIEND_TEST(DictionaryPredictorTest,
              GetPredictionTypesTestWithZeroQuerySuggestion);
  FRIEND_TEST(DictionaryPredictorTest, IsZipCodeRequest);
  FRIEND_TEST(DictionaryPredictorTest, GetRealtimeCandidateMaxSize);
  FRIEND_TEST(DictionaryPredictorTest, GetRealtimeCandidateMaxSizeForMixed);
  FRIEND_TEST(DictionaryPredictorTest,
              GetRealtimeCandidateMaxSizeWithActualConverter);
  FRIEND_TEST(DictionaryPredictorTest, GetUnigramCandidateCutoffThreshold);
  FRIEND_TEST(DictionaryPredictorTest, AggregateUnigramPrediction);
  FRIEND_TEST(DictionaryPredictorTest, AggregateBigramPrediction);
  FRIEND_TEST(DictionaryPredictorTest, AggregateSuffixPrediction);
  FRIEND_TEST(DictionaryPredictorTest, AddCostToNodesWcost);
  FRIEND_TEST(DictionaryPredictorTest, ZeroQuerySuggestionAfterNumbers);
  FRIEND_TEST(DictionaryPredictorTest, TriggerNumberZeroQuerySuggestion);
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
  FRIEND_TEST(DictionaryPredictorTest, SetDescription);
  FRIEND_TEST(DictionaryPredictorTest, SetDebugDescription);

  struct ResultCompare : public binary_function<Result, Result, bool> {
    bool operator() (const Result &lhs, const Result &rhs) const {
      return lhs.cost > rhs.cost;
    }
  };

  // Returns false if no results were aggregated.
  bool AggregatePrediction(const ConversionRequest &request,
                           Segments *segments,
                           NodeAllocatorInterface *allocator,
                           vector<Result> *results) const;

  void SetCost(const ConversionRequest &request,
               const Segments &segments, vector<Result> *results) const;

  // Removes prediciton by setting NO_PREDICTION to result type if necessary.
  void RemovePrediction(const ConversionRequest &request,
                        const Segments &segments,
                        vector<Result> *results) const;

  // Adds prediction results from history key and value.
  void AddBigramResultsFromHistory(const string &history_key,
                                   const string &history_value,
                                   const ConversionRequest &request,
                                   Segments *segments,
                                   NodeAllocatorInterface *allocator,
                                   vector<Result> *results) const;

  // Changes the prediction type for irrelevant bigram candidate.
  void CheckBigramResult(const Node *history_node,
                         const Util::ScriptType history_ctype,
                         const Util::ScriptType last_history_ctype,
                         NodeAllocatorInterface *allocator,
                         Result *result) const;

  const Node *GetPredictiveNodes(const DictionaryInterface *dictionary,
                                 const string &history_key,
                                 const ConversionRequest &request,
                                 const Segments &segments,
                                 NodeAllocatorInterface *allocator) const;

  // Performs a custom look up for English words where case-conversion might be
  // applied to lookup key and/or output results.
  const Node *GetPredictiveNodesForEnglish(
      const DictionaryInterface *dictionary,
      const string &history_key,
      const ConversionRequest &request,
      const Segments &segments,
      NodeAllocatorInterface *allocator) const;

  // Adds |cost| to given all the nodes' wcost and
  // returns the tail of |node| list.
  // |node| must be non-null.
  static Node *AddCostToNodesWcost(int32 cost, Node *node);

  // Performs look-ups using type-corrected queries from composer. Usually
  // involves multiple look-ups from dictionary.
  const Node *GetPredictiveNodesUsingTypingCorrection(
      const DictionaryInterface *dictionary,
      const string &history_key,
      const ConversionRequest &request,
      const Segments &segments,
      NodeAllocatorInterface *allocator) const;

  // Returns the position of misspelled character position.
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

  // Returns true if key/value is in dictionary.
  const Node *LookupKeyValueFromDictionary(
      const string &key,
      const string &value,
      NodeAllocatorInterface *allocator) const;

  // Returns language model cost of |node| given prediciton type |type|.
  // |rid| is the right id of previous word (node).
  // If |rid| is uknown, set 0 as a default value.
  int GetLMCost(PredictionTypes type, const Node &node, int rid) const;

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
  // This algorithm is mainly used for desktop.
  void SetPredictionCost(const Segments &segments,
                         vector<Result> *results) const;

  // Language model-based scoring function.
  // This algorithm is mainly used for mobile.
  void SetLMCost(const Segments &segments,
                 vector<Result> *results) const;

  // Returns true if the suggestion is classified
  // as "aggressive".
  bool IsAggressiveSuggestion(
      size_t query_len, size_t key_len, int cost,
      bool is_suggestion, size_t total_candidates_size) const;

  // Gets history key/value.
  // Returns false if history segments are
  // not found.
  bool GetHistoryKeyAndValue(const Segments &segments,
                             string *key, string *value) const;

  // Returns a bitfield of PredictionType.
  static PredictionTypes GetPredictionTypes(const ConversionRequest &request,
                                            const Segments &segments);

  // Returns true if the realtime conversion should be used.
  // TODO(hidehiko): add Config and Request instances into the arguments
  //   to represent the dependency explicitly.
  static bool ShouldRealTimeConversionEnabled(const ConversionRequest &request,
                                              const Segments &segments);

  // Returns true if key consistes of '0'-'9' or '-'
  static bool IsZipCodeRequest(const string &key);

  // Returns max size of realtime candidates.
  size_t GetRealtimeCandidateMaxSize(const Segments &segments,
                                     bool mixed_conversion,
                                     size_t max_size) const;

  // Aggregates unigram candidate for mixed conversion.
  // This reduces redundant candidates.
  void AggregateUnigramCandidateForMixedConversion(
      const ConversionRequest &request,
      Segments *segments,
      NodeAllocatorInterface *allocator,
      vector<Result> *results) const;

  // Aggregates unigram candidate for non mixed conversion.
  void AggregateUnigramCandidate(const ConversionRequest &request,
                                 Segments *segments,
                                 NodeAllocatorInterface *allocator,
                                 vector<Result> *results) const;

  // Returns cutoff threshold of unigram candidates.
  // AggregateUnigramPrediction method does not return any candidates
  // if there are too many (>= cutoff threshold) eligible candidates.
  // This behavior prevents a user from seeing too many prefix-match
  // candidates.
  size_t GetUnigramCandidateCutoffThreshold(const Segments &segments) const;

  // Generates a top conversion result from |converter_| and adds its result to
  // |results|.
  bool PushBackTopConversionResult(const ConversionRequest &request,
                                   Segments *segments,
                                   NodeAllocatorInterface *allocator,
                                   vector<Result> *results) const;

  // Sets candidate description.
  static void SetDescription(PredictionTypes types,
                             uint32 attributes,
                             string *description);
  // Description for DEBUG mode.
  static void SetDebugDescription(PredictionTypes types,
                                  string *description);

  const ConverterInterface *converter_;
  const ImmutableConverterInterface *immutable_converter_;
  const DictionaryInterface *dictionary_;
  const DictionaryInterface *suffix_dictionary_;
  const ConnectorInterface *connector_;
  const SegmenterInterface *segmenter_;
  const SuggestionFilter *suggestion_filter_;
  const uint16 counter_suffix_word_id_;
  const string predictor_name_;
};
}  // namespace mozc

#endif  // MOZC_PREDICTION_DICTIONARY_PREDICTOR_H_
