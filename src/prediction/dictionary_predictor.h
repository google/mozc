// Copyright 2010-2021, Google Inc.
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

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "base/util.h"
#include "converter/connector.h"
#include "converter/converter_interface.h"
#include "converter/immutable_converter_interface.h"
#include "converter/segmenter.h"
#include "converter/segments.h"
#include "data_manager/data_manager_interface.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "prediction/predictor_interface.h"
#include "prediction/suggestion_filter.h"
#include "prediction/zero_query_dict.h"
#include "request/conversion_request.h"
// for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"

namespace mozc {

// Dictionary-based predictor
class DictionaryPredictor : public PredictorInterface {
 public:
  // Initializes a predictor with given references to submodules. Note that
  // pointers are not owned by the class and to be deleted by the caller.
  DictionaryPredictor(const DataManagerInterface &data_manager,
                      const ConverterInterface *converter,
                      const ImmutableConverterInterface *immutable_converter,
                      const dictionary::DictionaryInterface *dictionary,
                      const dictionary::DictionaryInterface *suffix_dictionary,
                      const Connector *connector, const Segmenter *segmenter,
                      const dictionary::PosMatcher *pos_matcher,
                      const SuggestionFilter *suggestion_filter);
  ~DictionaryPredictor() override;

  bool PredictForRequest(const ConversionRequest &request,
                         Segments *segments) const override;

  void Finish(const ConversionRequest &request, Segments *segments) override;

  const std::string &GetPredictorName() const override {
    return predictor_name_;
  }

 protected:
  // Protected members for unittesting
  // For use util method accessing private members, made them protected.
  // https://github.com/google/googletest/blob/master/googletest/docs/FAQ.md
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
    SUFFIX = 8,
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
  typedef int32_t PredictionTypes;

  struct Result {
    Result()
        : types(NO_PREDICTION),
          wcost(0),
          cost(0),
          lid(0),
          rid(0),
          candidate_attributes(0),
          source_info(0),
          consumed_key_size(0),
          removed(false) {}

    void InitializeByTokenAndTypes(const dictionary::Token &token,
                                   PredictionTypes types);
    void SetTypesAndTokenAttributes(
        PredictionTypes prediction_types,
        dictionary::Token::AttributesBitfield token_attr);
    void SetSourceInfoForZeroQuery(ZeroQueryType zero_query_type);
    bool IsUserDictionaryResult() const;
    bool IsEnglishEntryResult() const;

    std::string key;
    std::string value;
    // Indicating which PredictionType creates this instance.
    // UNIGRAM, BIGRAM, REALTIME, SUFFIX, ENGLISH or TYPING_CORRECTION
    // is set exclusively.
    PredictionTypes types;
    // Context "insensitive" candidate cost.
    int wcost;
    // Context "sensitive" candidate cost.
    int cost;
    int lid;
    int rid;
    // Boundary information for realtime conversion.
    // This will be set only for realtime conversion result candidates.
    // This contains inner segment size for key and value.
    // If the candidate key and value are
    // "わたしの|なまえは|なかのです", " 私の|名前は|中野です",
    // |inner_segment_boundary| have [(4,2), (4, 3), (5, 4)].
    std::vector<uint32_t> inner_segment_boundary;
    uint32_t candidate_attributes;
    // Segment::Candidate::SourceInfo.
    // Will be used for usage stats.
    uint32_t source_info;
    size_t consumed_key_size;
    // If removed is true, this result is not used for a candidate.
    bool removed;
#ifndef NDEBUG
    std::string log;
#endif  // NDEBUG
  };

  using AggregateUnigramFn = PredictionType (DictionaryPredictor::*)(
      const ConversionRequest &request, const Segments &segments,
      std::vector<Result> *results) const;

  struct UnigramConfig {
    AggregateUnigramFn unigram_fn;
    size_t min_key_len;
  };

  // On MSVS2008/2010, Constructors of TestableDictionaryPredictor::Result
  // causes a compile error even if you change the access right of it to public.
  // You can use TestableDictionaryPredictor::MakeEmptyResult() instead.
  static Result MakeEmptyResult() { return Result(); }

  class PredictiveLookupCallback;
  class PredictiveBigramLookupCallback;
  class ResultWCostLess;
  class ResultCostLess;

  // Aggregate* methods aggregate the candidates with different resources
  // and algorithms.
  void AggregateRealtimeConversion(const ConversionRequest &request,
                                   size_t realtime_candidates_size,
                                   Segments *segments,
                                   std::vector<Result> *results) const;

  void AggregateBigramPrediction(const ConversionRequest &request,
                                 const Segments &segments,
                                 Segment::Candidate::SourceInfo source_info,
                                 std::vector<Result> *results) const;

  void AggregateSuffixPrediction(const ConversionRequest &request,
                                 const Segments &segments,
                                 std::vector<Result> *results) const;

  void AggregateZeroQuerySuffixPrediction(const ConversionRequest &request,
                                          const Segments &segments,
                                          std::vector<Result> *results) const;

  void AggregateEnglishPrediction(const ConversionRequest &request,
                                  const Segments &segments,
                                  std::vector<Result> *results) const;

  // Note that this look up is done with raw input string rather than query
  // string from composer.  This is helpful to implement language aware input.
  void AggregateEnglishPredictionUsingRawInput(
      const ConversionRequest &request, const Segments &segments,
      std::vector<Result> *results) const;

  void AggregateTypeCorrectingPrediction(const ConversionRequest &request,
                                         const Segments &segments,
                                         std::vector<Result> *results) const;

  PredictionType AggregateUnigramCandidate(const ConversionRequest &request,
                                           const Segments &segments,
                                           std::vector<Result> *results) const;

  PredictionType AggregateUnigramCandidateForMixedConversion(
      const ConversionRequest &request, const Segments &segments,
      std::vector<Result> *results) const;

  PredictionType AggregateUnigramCandidateForLatinInput(
      const ConversionRequest &request, const Segments &segments,
      std::vector<Result> *results) const;

  static void AggregateUnigramCandidateForMixedConversion(
      const dictionary::DictionaryInterface &dictionary,
      const ConversionRequest &request, const Segments &segments,
      int unknown_id, std::vector<Result> *results);

  void ApplyPenaltyForKeyExpansion(const Segments &segments,
                                   std::vector<Result> *results) const;

  bool AddPredictionToCandidates(const ConversionRequest &request,
                                 bool include_exact_key, Segments *segments,
                                 std::vector<Result> *results) const;

 private:
  friend class DictionaryPredictorTest;
  FRIEND_TEST(DictionaryPredictorTest, IsZipCodeRequest);
  FRIEND_TEST(DictionaryPredictorTest, GetRealtimeCandidateMaxSize);
  FRIEND_TEST(DictionaryPredictorTest, GetRealtimeCandidateMaxSizeForMixed);
  FRIEND_TEST(DictionaryPredictorTest,
              GetRealtimeCandidateMaxSizeWithActualConverter);
  FRIEND_TEST(DictionaryPredictorTest, GetCandidateCutoffThreshold);
  FRIEND_TEST(DictionaryPredictorTest, AggregateUnigramCandidate);
  FRIEND_TEST(DictionaryPredictorTest, AggregateBigramPrediction);
  FRIEND_TEST(DictionaryPredictorTest, AggregateZeroQueryBigramPrediction);
  FRIEND_TEST(DictionaryPredictorTest, AggregateSuffixPrediction);
  FRIEND_TEST(DictionaryPredictorTest, AggregateZeroQuerySuffixPrediction);
  FRIEND_TEST(DictionaryPredictorTest,
              AggregateZeroQueryPrediction_LatinInputMode);
  FRIEND_TEST(DictionaryPredictorTest,
              AggregateUnigramCandidateForMixedConversion);
  FRIEND_TEST(DictionaryPredictorTest,
              AggregateUnigramCandidateForMixedConversion_EnglishWords);
  FRIEND_TEST(DictionaryPredictorTest, ZeroQuerySuggestionAfterNumbers);
  FRIEND_TEST(DictionaryPredictorTest, TriggerNumberZeroQuerySuggestion);
  FRIEND_TEST(DictionaryPredictorTest, TriggerZeroQuerySuggestion);
  FRIEND_TEST(DictionaryPredictorTest, GetHistoryKeyAndValue);
  FRIEND_TEST(DictionaryPredictorTest, RealtimeConversionStartingWithAlphabets);
  FRIEND_TEST(DictionaryPredictorTest, IsAggressiveSuggestion);
  FRIEND_TEST(DictionaryPredictorTest,
              RealtimeConversionWithSpellingCorrection);
  FRIEND_TEST(DictionaryPredictorTest, GetMissSpelledPosition);
  FRIEND_TEST(DictionaryPredictorTest, RemoveMissSpelledCandidates);
  FRIEND_TEST(DictionaryPredictorTest, ConformCharacterWidthToPreference);
  FRIEND_TEST(DictionaryPredictorTest, SetPredictionCostForMixedConversion);
  FRIEND_TEST(DictionaryPredictorTest, SetLMCostForUserDictionaryWord);
  FRIEND_TEST(DictionaryPredictorTest, SetDescription);
  FRIEND_TEST(DictionaryPredictorTest, SetDebugDescription);
  FRIEND_TEST(DictionaryPredictorTest, GetZeroQueryCandidates);
  FRIEND_TEST(DictionaryPredictorTest, TriggerConditions);
  FRIEND_TEST(DictionaryPredictorTest, TriggerConditions_Mobile);
  FRIEND_TEST(DictionaryPredictorTest, TriggerConditions_LatinInputMode);
  FRIEND_TEST(DictionaryPredictorTest, GetLMCost);
  FRIEND_TEST(TriggerConditionsTest, TriggerConditions);

  typedef std::pair<std::string, ZeroQueryType> ZeroQueryResult;

  // Looks up the given range and appends zero query candidate list for |key|
  // to |results|.
  // Returns false if there is no result for |key|.
  static bool GetZeroQueryCandidatesForKey(
      const ConversionRequest &request, const std::string &key,
      const ZeroQueryDict &dict, std::vector<ZeroQueryResult> *results);

  static void AppendZeroQueryToResults(
      const std::vector<ZeroQueryResult> &candidates, uint16_t lid,
      uint16_t rid, std::vector<Result> *results);

  // Returns the bitfield that indicates what prediction subroutines
  // were used.  NO_PREDICTION means that no prediction was made.
  PredictionTypes AggregatePredictionForRequest(
      const ConversionRequest &request, Segments *segments,
      std::vector<Result> *results) const;

  PredictionTypes AggregatePrediction(const ConversionRequest &request,
                                      size_t realtime_max_size,
                                      const UnigramConfig &unigram_config,
                                      Segments *segments,
                                      std::vector<Result> *results) const;

  PredictionTypes AggregatePredictionForZeroQuery(
      const ConversionRequest &request, Segments *segments,
      std::vector<Result> *results) const;

  bool AggregateNumberZeroQueryPrediction(const ConversionRequest &request,
                                          const Segments &segments,
                                          std::vector<Result> *results) const;

  bool AggregateZeroQueryPrediction(const ConversionRequest &request,
                                    const Segments &segments,
                                    std::vector<Result> *results) const;

  // Adds prediction results from history key and value.
  void AddBigramResultsFromHistory(const std::string &history_key,
                                   const std::string &history_value,
                                   const ConversionRequest &request,
                                   const Segments &segments,
                                   Segment::Candidate::SourceInfo source_info,
                                   std::vector<Result> *results) const;

  // Changes the prediction type for irrelevant bigram candidate.
  void CheckBigramResult(const dictionary::Token &history_token,
                         const Util::ScriptType history_ctype,
                         const Util::ScriptType last_history_ctype,
                         const ConversionRequest &request,
                         Result *result) const;

  static void GetPredictiveResults(
      const dictionary::DictionaryInterface &dictionary,
      const std::string &history_key, const ConversionRequest &request,
      const Segments &segments, PredictionTypes types, size_t lookup_limit,
      Segment::Candidate::SourceInfo source_info, int unknown_id,
      std::vector<Result> *results);

  void GetPredictiveResultsForBigram(
      const dictionary::DictionaryInterface &dictionary,
      const std::string &history_key, const std::string &history_value,
      const ConversionRequest &request, const Segments &segments,
      PredictionTypes types, size_t lookup_limit,
      Segment::Candidate::SourceInfo source_info, int unknown_id,
      std::vector<Result> *results) const;

  // Performs a custom look up for English words where case-conversion might be
  // applied to lookup key and/or output results.
  void GetPredictiveResultsForEnglishKey(
      const dictionary::DictionaryInterface &dictionary,
      const ConversionRequest &request, const std::string &input_key,
      PredictionTypes types, size_t lookup_limit,
      std::vector<Result> *results) const;

  // Performs look-ups using type-corrected queries from composer. Usually
  // involves multiple look-ups from dictionary.
  void GetPredictiveResultsUsingTypingCorrection(
      const dictionary::DictionaryInterface &dictionary,
      const std::string &history_key, const ConversionRequest &request,
      const Segments &segments, PredictionTypes types, size_t lookup_limit,
      std::vector<Result> *results) const;

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
  size_t GetMissSpelledPosition(const std::string &key,
                                const std::string &value) const;

  // Returns language model cost of |token| given prediciton type |type|.
  // |rid| is the right id of previous word (token).
  // If |rid| is unknown, set 0 as a default value.
  int GetLMCost(const Result &result, int rid) const;

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
                                   std::vector<Result> *results) const;

  // Scoring function which takes prediction bounus into account.
  // It basically reranks the candidate by lang_prob * (1 + remain_len).
  // This algorithm is mainly used for desktop.
  void SetPredictionCost(const Segments &segments,
                         std::vector<Result> *results) const;

  // Scoring function for mixed conversion.
  // In the mixed conversion we basically use the pure language model-based
  // scoring function. This algorithm is mainly used for mobile.
  void SetPredictionCostForMixedConversion(const Segments &segments,
                                           std::vector<Result> *results) const;

  // Returns true if the suggestion is classified
  // as "aggressive".
  bool IsAggressiveSuggestion(size_t query_len, size_t key_len, int cost,
                              bool is_suggestion,
                              size_t total_candidates_size) const;

  // Gets history key/value.
  // Returns false if history segments are
  // not found.
  bool GetHistoryKeyAndValue(const Segments &segments, std::string *key,
                             std::string *value) const;

  // Returns true if the realtime conversion should be used.
  // TODO(hidehiko): add Config and Request instances into the arguments
  //   to represent the dependency explicitly.
  static bool ShouldAggregateRealTimeConversionResults(
      const ConversionRequest &request, const Segments &segments);

  // Returns true if key consistes of '0'-'9' or '-'
  static bool IsZipCodeRequest(const std::string &key);

  // Returns max size of realtime candidates.
  size_t GetRealtimeCandidateMaxSize(const ConversionRequest &request,
                                     const Segments &segments,
                                     bool mixed_conversion) const;

  // Returns config to gather unigram candidates.
  UnigramConfig GetUnigramConfig(const ConversionRequest &request,
                                 const Segments &segments) const;

  // Returns cutoff threshold of unigram candidates.
  // AggregateUnigramPrediction method does not return any candidates
  // if there are too many (>= cutoff threshold) eligible candidates.
  // This behavior prevents a user from seeing too many prefix-match
  // candidates.
  size_t GetCandidateCutoffThreshold(const Segments &segments) const;

  // Generates a top conversion result from |converter_| and adds its result to
  // |results|.
  bool PushBackTopConversionResult(const ConversionRequest &request,
                                   const Segments &segments,
                                   std::vector<Result> *results) const;

  void MaybeRecordUsageStats(const Segment::Candidate &candidate) const;

  // Sets candidate description.
  static void SetDescription(PredictionTypes types, uint32_t attributes,
                             std::string *description);
  // Description for DEBUG mode.
  static void SetDebugDescription(PredictionTypes types,
                                  std::string *description);

  const ConverterInterface *converter_;
  const ImmutableConverterInterface *immutable_converter_;
  const dictionary::DictionaryInterface *dictionary_;
  const dictionary::DictionaryInterface *suffix_dictionary_;
  const Connector *connector_;
  const Segmenter *segmenter_;
  const SuggestionFilter *suggestion_filter_;
  const uint16_t counter_suffix_word_id_;
  const uint16_t general_symbol_id_;
  const uint16_t unknown_id_;
  const std::string predictor_name_;
  ZeroQueryDict zero_query_dict_;
  ZeroQueryDict zero_query_number_dict_;

  DISALLOW_COPY_AND_ASSIGN(DictionaryPredictor);
};

}  // namespace mozc

#endif  // MOZC_PREDICTION_DICTIONARY_PREDICTOR_H_
