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

#include "prediction/dictionary_predictor.h"

#include <algorithm>
#include <cctype>
#include <climits>   // INT_MAX
#include <cmath>
#include <list>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base.h"
#include "base/logging.h"
#include "base/number_util.h"
#include "base/trie.h"
#include "base/util.h"
#include "composer/composer.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/connector_interface.h"
#include "converter/conversion_request.h"
#include "converter/immutable_converter_interface.h"
#include "converter/node.h"
#include "converter/node_allocator.h"
#include "converter/segmenter_interface.h"
#include "converter/segments.h"
#include "data_manager/data_manager_interface.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_matcher.h"
#include "prediction/predictor_interface.h"
#include "prediction/suggestion_filter.h"
#include "prediction/zero_query_number_data.h"
#include "session/commands.pb.h"
#include "session/request_handler.h"

// This flag is set by predictor.cc
// We can remove this after the ambiguity expansion feature get stable.
DEFINE_bool(enable_expansion_for_dictionary_predictor,
            false,
            "enable ambiguity expansion for dictionary_predictor");

namespace mozc {
namespace {

// Note that PREDICTION mode is much slower than SUGGESTION.
// Number of prediction calls should be minimized.
const size_t kSuggestionMaxNodesSize = 256;
const size_t kPredictionMaxNodesSize = 100000;

void GetNumberSuffixArray(const string &history_input,
                          vector<string> *suffixes) {
  DCHECK(suffixes);
  const char kDefault[] = "default";
  const string default_str(kDefault);

  int default_num = -1;
  int suffix_num = -1;

  for (int i = 0; ZeroQueryNum[i]; ++i) {
    if (default_str == ZeroQueryNum[i][0]) {
      default_num = i;
    } else if (history_input == ZeroQueryNum[i][0]) {
      suffix_num = i;
    }
  }
  DCHECK_GE(default_num, 0);

  if (suffix_num != -1) {
    for (int j = 1; ZeroQueryNum[suffix_num][j]; ++j) {
      suffixes->push_back(ZeroQueryNum[suffix_num][j]);
    }
  }
  for (int j = 1; ZeroQueryNum[default_num][j]; ++j) {
    suffixes->push_back(ZeroQueryNum[default_num][j]);
  }
}

// Returns true if the |target| may be reduncant node.
bool MaybeRedundant(const Node *reference, const Node *target) {
  return Util::StartsWith(target->value, reference->value);
}

// Comparator for sorting prediction candidates.
// If we have words A and AB, for example "六本木" and "六本木ヒルズ",
// assume that cost(A) < cost(AB).
class NodeLess {
 public:
  bool operator() (const Node *lhs, const Node *rhs) {
    return lhs->wcost < rhs->wcost;
  }
};

bool IsLatinInputMode(const ConversionRequest &request) {
  return (request.has_composer() &&
          (request.composer().GetInputMode() == transliteration::HALF_ASCII ||
           request.composer().GetInputMode() == transliteration::FULL_ASCII));
}

}  // namespace

DictionaryPredictor::DictionaryPredictor(
    const ImmutableConverterInterface *immutable_converter,
    const DictionaryInterface *dictionary,
    const DictionaryInterface *suffix_dictionary,
    const DataManagerInterface *data_manager)
    : immutable_converter_(immutable_converter),
      dictionary_(dictionary),
      suffix_dictionary_(suffix_dictionary),
      connector_(data_manager->GetConnector()),
      segmenter_(data_manager->GetSegmenter()),
      counter_suffix_word_id_(
          data_manager->GetPOSMatcher()->GetCounterSuffixWordId()),
      predictor_name_("DictionaryPredictor") {
  // Create suggestion filter.
  const char *data = NULL;
  size_t size = 0;
  data_manager->GetSuggestionFilterData(&data, &size);
  CHECK(data);
  suggestion_filter_.reset(new SuggestionFilter(data, size));
}

DictionaryPredictor::~DictionaryPredictor() {}

bool DictionaryPredictor::Predict(Segments *segments) const {
  ConversionRequest default_request;
  return PredictForRequest(default_request, segments);
}

bool DictionaryPredictor::PredictForRequest(const ConversionRequest &request,
                                            Segments *segments) const {
  if (segments == NULL) {
    return false;
  }

  vector<Result> results;
  scoped_ptr<NodeAllocatorInterface> allocator(new NodeAllocator);

  if (!AggregatePrediction(request, segments, allocator.get(), &results)) {
    return false;
  }

  SetCost(*segments, &results);
  RemovePrediction(*segments, &results);
  return AddPredictionToCandidates(request, segments, &results);
}

bool DictionaryPredictor::AggregatePrediction(
    const ConversionRequest &request,
    Segments *segments, NodeAllocatorInterface *allocator,
    vector<Result> *results) const {
  DCHECK(segments);
  DCHECK(results);

  const PredictionTypes prediction_types =
      GetPredictionTypes(request, *segments);
  if (prediction_types == NO_PREDICTION) {
    return false;
  }

  if (segments->request_type() == Segments::PARTIAL_SUGGESTION ||
      segments->request_type() == Segments::PARTIAL_PREDICTION) {
    // This request type is used to get conversion before cursor during
    // composition mode. Thus it should return only the candidates whose key
    // exactly matches the query.
    // Therefore, we use only the realtime conversion result.
    AggregateRealtimeConversion(prediction_types, segments, allocator, results);
  } else {
    AggregateRealtimeConversion(prediction_types, segments, allocator, results);
    AggregateUnigramPrediction(prediction_types, request, segments,
                               allocator, results);
    AggregateBigramPrediction(prediction_types, request, segments,
                              allocator, results);
    AggregateSuffixPrediction(prediction_types, request, segments,
                              allocator, results);
    AggregateEnglishPrediction(prediction_types, request, segments,
                               allocator, results);
  }

  if (results->empty()) {
    VLOG(2) << "|result| is empty";
    return false;
  } else {
    return true;
  }
}

void DictionaryPredictor::SetCost(const Segments &segments,
                                  vector<Result> *results) const {
  DCHECK(results);

  if (GET_REQUEST(mixed_conversion)) {
    SetLMCost(segments, results);
  } else {
    SetPredictionCost(segments, results);
  }

  ApplyPenaltyForKeyExpansion(segments, results);
}

void DictionaryPredictor::RemovePrediction(const Segments &segments,
                                           vector<Result> *results) const {
  DCHECK(results);

  if (!GET_REQUEST(mixed_conversion)) {
    // Currently, we don't have spelling correction feature on mobile,
    // so we don't run RemoveMissSpelledCandidates.
    const string &input_key = segments.conversion_segment(0).key();
    const size_t input_key_len = Util::CharsLen(input_key);
    RemoveMissSpelledCandidates(input_key_len, results);
  }
}

bool DictionaryPredictor::AddPredictionToCandidates(
    const ConversionRequest &request,
    Segments *segments, vector<Result> *results) const {
  DCHECK(segments);
  DCHECK(results);
  const bool mixed_conversion = GET_REQUEST(mixed_conversion);
  const string &input_key = segments->conversion_segment(0).key();
  const size_t input_key_len = Util::CharsLen(input_key);

  string history_key, history_value;
  GetHistoryKeyAndValue(*segments, &history_key, &history_value);

  // exact_bigram_key does not contain ambiguity expansion, because
  // this is used for exact matching for the key.
  const string exact_bigram_key = history_key + input_key;

  Segment *segment = segments->mutable_conversion_segment(0);
  DCHECK(segment);

  // Instead of sorting all the results, we construct a heap.
  // This is done in linear time and
  // we can pop as many results as we need efficiently.
  make_heap(results->begin(), results->end(), ResultCompare());

  const size_t size = min(segments->max_prediction_candidates_size(),
                          results->size());

  int added = 0;
  set<string> seen;

  int added_suffix = 0;

  for (size_t i = 0; i < results->size(); ++i) {
    if (added >= size || results->at(i).cost == INT_MAX) {
      break;
    }

    pop_heap(results->begin(), results->end() - i, ResultCompare());
    const Result &result = results->at(results->size() - i - 1);
    const Node *node = result.node;
    DCHECK(node);

    if (result.types == NO_PREDICTION) {
      continue;
    }

    // We don't filter the results from realtime conversion if mixed_conversion
    // is true.
    // TODO(manabe): Add a unit test. For that, we'll need a mock class for
    //               SuppressionDictionary.
    if (suggestion_filter_->IsBadSuggestion(node->value) &&
        !(mixed_conversion && result.types & REALTIME)) {
      continue;
    }

    // don't suggest exactly the same candidate as key.
    // if |mixed_conversion| is true, that's not the case.
    if (!mixed_conversion &&
        !(result.types & REALTIME) &&
        (((result.types & BIGRAM) &&
          exact_bigram_key == node->value) ||
         (!(result.types & BIGRAM) &&
          input_key == node->value))) {
      continue;
    }

    string key, value;
    if (result.types & BIGRAM) {
      // remove the prefix of history key and history value.
      key = node->key.substr(history_key.size(),
                             node->key.size() - history_key.size());
      value = node->value.substr(history_value.size(),
                                 node->value.size() - history_value.size());
    } else {
      key = node->key;
      value = node->value;
    }

    if (!seen.insert(value).second) {
      continue;
    }

    // User input: "おーすとり" (len = 5)
    // key/value:  "おーすとりら" "オーストラリア" (miss match pos = 4)
    if ((node->attributes & Node::SPELLING_CORRECTION) &&
        key != input_key &&
        input_key_len <= GetMissSpelledPosition(key, value) + 1) {
      continue;
    }

    if (result.types == SUFFIX && added_suffix++ >= 20) {
      // TODO(toshiyuki): Need refactoring for controlling suffix
      // prediction number after we will fix the appropriate number.
      continue;
    }

    Segment::Candidate *candidate = segment->push_back_candidate();
    DCHECK(candidate);

    candidate->Init();
    candidate->content_key = key;
    candidate->content_value = value;
    candidate->key = key;
    candidate->value = value;
    candidate->lid = node->lid;
    candidate->rid = node->rid;
    candidate->wcost = node->wcost;
    candidate->cost = results->at(i).cost;
    if (node->attributes & Node::SPELLING_CORRECTION) {
      candidate->attributes |= Segment::Candidate::SPELLING_CORRECTION;
    } else if (IsLatinInputMode(request)) {
      candidate->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
      candidate->attributes |= Segment::Candidate::NO_EXTRA_DESCRIPTION;
    }
    if (node->attributes & Node::NO_VARIANTS_EXPANSION) {
      candidate->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
    }
    if (node->attributes & Node::USER_DICTIONARY) {
      candidate->attributes |= Segment::Candidate::USER_DICTIONARY;
    }

#ifdef DEBUG
    SetDebugDescription(result.types, &candidate->description);
#endif  // DEBUG

    ++added;
  }

  return added > 0;
}

#ifdef DEBUG
void DictionaryPredictor::SetDebugDescription(PredictionTypes types,
                                              string *description) {
  vector<string> descriptions;
  if (!description->empty()) {
    descriptions.push_back(*description);
  }
  if (types & UNIGRAM) {
    descriptions.push_back("Unigram");
  }
  if (types & BIGRAM) {
    descriptions.push_back("Bigram");
  }
  if (types & REALTIME) {
    descriptions.push_back("Realtime");
  }
  if (types & SUFFIX) {
    descriptions.push_back("Suffix");
  }
  if (types & ENGLISH) {
    descriptions.push_back("English");
  }
  Util::JoinStrings(descriptions, " ", description);
}
#endif  // DEBUG

// return transition_cost[rid][node.lid] + node.wcost (+ penalties).
int DictionaryPredictor::GetLMCost(PredictionTypes types,
                                   const Node &node,
                                   int rid) const {
  int lm_cost = connector_->GetTransitionCost(rid, node.lid) + node.wcost;
  if (!(types & REALTIME)) {
    // Relatime conversion already adds perfix/suffix penalties to the nodes.
    // Note that we don't add prefix penalty the role of "bunsetsu" is
    // ambigous on zero-query suggestion.
    lm_cost += segmenter_->GetSuffixPenalty(node.rid);
  }

  return lm_cost;
}

// return dictionary node whose value/key are |key| and |value|.
// return NULL no words are found in the dictionary.
const Node *DictionaryPredictor::LookupKeyValueFromDictionary(
    const string &key,
    const string &value,
    NodeAllocatorInterface *allocator) const {
  DCHECK(allocator);
  const Node *node = dictionary_->LookupPrefix(key.data(), key.size(),
                                               allocator);
  for (; node != NULL; node = node->bnext) {
    if (value == node->value) {
      return node;
    }
  }
  return NULL;
}

bool DictionaryPredictor::GetHistoryKeyAndValue(
    const Segments &segments, string *key, string *value) const {
  DCHECK(key);
  DCHECK(value);
  if (segments.history_segments_size() == 0) {
    return false;
  }

  const Segment &history_segment =
      segments.history_segment(segments.history_segments_size() - 1);
  if (history_segment.candidates_size() == 0) {
    return false;
  }

  key->assign(history_segment.candidate(0).key);
  value->assign(history_segment.candidate(0).value);
  return true;
}

void DictionaryPredictor::SetPredictionCost(const Segments &segments,
                                            vector<Result> *results) const {
  int rid = 0;  // 0 (BOS) is default
  if (segments.history_segments_size() > 0) {
    const Segment &history_segment =
        segments.history_segment(segments.history_segments_size() - 1);
    if (history_segment.candidates_size() > 0) {
      rid = history_segment.candidate(0).rid;  // use history segment's id
    }
  }

  DCHECK(results);
  const string &input_key = segments.conversion_segment(0).key();
  string history_key, history_value;
  GetHistoryKeyAndValue(segments, &history_key, &history_value);
  const string bigram_key = history_key + input_key;
  const bool is_suggestion = (segments.request_type() ==
                              Segments::SUGGESTION);

  // use the same scoring function for both unigram/bigram.
  // Bigram will be boosted because we pass the previous
  // key as a context information.
  const size_t bigram_key_len = Util::CharsLen(bigram_key);
  const size_t unigram_key_len = Util::CharsLen(input_key);

  for (size_t i = 0; i < results->size(); ++i) {
    const Node *node = (*results)[i].node;
    const PredictionTypes types = (*results)[i].types;
    const int cost = GetLMCost(types, *node, rid);
    DCHECK(node);

    const size_t query_len =
        (types & BIGRAM) ? bigram_key_len : unigram_key_len;
    const size_t key_len = Util::CharsLen(node->key);

    if (IsAggressiveSuggestion(query_len, key_len, cost,
                               is_suggestion, results->size())) {
      (*results)[i].cost = INT_MAX;
      continue;
    }

    // cost = -500 * log(lang_prob(w) * (1 + remain_length))    -- (1)
    // where lang_prob(w) is a language model probability of the word "w", and
    // remain_length the length of key user must type to input "w".
    //
    // Example:
    // key/value = "とうきょう/東京"
    // user_input = "とう"
    // remain_length = len("とうきょう") - len("とう") = 3
    //
    // By taking the log of (1),
    // cost  = -500 [log(lang_prob(w)) + log(1 + ramain_length)]
    //       = -500 * log(lang_prob(w)) + 500 * log(1 + remain_length)
    //       = cost - 500 * log(1 + remain_length)
    // Because 500 * log(lang_prob(w)) = -cost.
    //
    // lang_prob(w) * (1 + remain_length) represents how user can reduce
    // the total types by choosing this candidate.
    // Before this simple algorithm, we have been using an SVM-base scoring,
    // but we stop usign it with the following reasons.
    // 1) Hard to maintain the ranking.
    // 2) Hard to control the final results of SVM.
    // 3) Hard to debug.
    // 4) Since we used the log(remain_length) as a feature,
    //    the new ranking algorithm and SVM algorithm was essentially
    //    the same.
    // 5) Since we used the length of value as a feature, we find
    //    inconsistencies between the conversion and the prediction
    //    -- the results of top prediction and the top conversion
    //    (the candidate shown after the space key) may differ.
    //
    // The new function brings consistent results. If two candidate
    // have the same reading (key), they should have the same cost bonus
    // from the length part. This implies that the result is reranked by
    // the language model probability as long as the key part is the same.
    // This behavior is baisically the same as the converter.
    //
    // TODO(team): want find the best parameter instread of kCostFactor.
    const int kCostFactor = 500;
    (*results)[i].cost = cost -
        kCostFactor * log(1.0 + max(0, static_cast<int>(key_len - query_len)));
  }
}

void DictionaryPredictor::SetLMCost(const Segments &segments,
                                    vector<Result> *results) const {
  DCHECK(results);

  // ranking for mobile
  int rid = 0;  // 0 (BOS) is default
  int prev_cost = 0;
  if (segments.history_segments_size() > 0) {
    const Segment &history_segment =
        segments.history_segment(segments.history_segments_size() - 1);
    if (history_segment.candidates_size() > 0) {
      rid = history_segment.candidate(0).rid;  // use history segment's id
      prev_cost = history_segment.candidate(0).cost;
      if (prev_cost == 0) {
        // if prev_cost is set to be 0 for some reason, use default cost.
        prev_cost = 5000;
      }
    }
  }

  for (size_t i = 0; i < results->size(); ++i) {
    const Node *node = (*results)[i].node;
    const PredictionTypes types = (*results)[i].types;
    DCHECK(node);
    int cost = GetLMCost(types, *node, rid);
    // Make exact candidates to have higher ranking.
    // Because for mobile, suggestion is the main candidates and
    // users expect the candidates for the input key on the candidates.
    if (types & UNIGRAM) {
      const size_t input_key_len = Util::CharsLen(
          segments.conversion_segment(0).key());
      const size_t key_len = Util::CharsLen(node->key);
      if (key_len > input_key_len) {
        // Cost penalty means that exact candiates are evaluated
        // 50 times bigger in frequency.
        // Note that the cost is calculated by cost = -500 * log(prob)
        // 1956 = 500 * log(50)
        const int kNotExactPenalty = 1956;
        cost += kNotExactPenalty;
      }
    }
    if (types & BIGRAM) {
      // When user inputs "六本木" and there is an entry
      // "六本木ヒルズ" in the dictionary, we can suggest
      // "ヒルズ" as a ZeroQuery suggestion. In this case,
      // We can't calcurate the transition cost between "六本木"
      // and "ヒルズ". If we ignore the transition cost,
      // bigram-based suggestion will be overestimated.
      // Here we use |default_transition_cost| as an
      // transition cost between "六本木" and "ヒルズ". Currently,
      // the cost is basically the same as the cost between
      // "名詞,一般" and "名詞,一般".
      const int kDefaultTransitionCost = 1347;
      // Promoting bigram candidates.
      const int kBigramBonus = 800;  // ~= 500*ln(5)
      cost += (kDefaultTransitionCost - kBigramBonus - prev_cost);
    }
    (*results)[i].cost = cost;
  }
}

void DictionaryPredictor::ApplyPenaltyForKeyExpansion(
    const Segments &segments, vector<Result> *results) const {
  if (segments.conversion_segments_size() == 0) {
    return;
  }
  // Cost penalty 1151 means that expanded candiates are evaluated
  // 10 times smaller in frequency.
  // Note that the cost is calcurated by cost = -500 * log(prob)
  // 1151 = 500 * log(10)
  const int kKeyExpansionPenalty = 1151;
  const string &conversion_key = segments.conversion_segment(0).key();
  for (size_t i = 0; i < results->size(); ++i) {
    const Node *node = (*results)[i].node;
    if (!Util::StartsWith(node->key, conversion_key)) {
      (*results)[i].cost += kKeyExpansionPenalty;
    }
  }
}

size_t DictionaryPredictor::GetMissSpelledPosition(
    const string &key, const string &value) const {
  string hiragana_value;
  Util::KatakanaToHiragana(value, &hiragana_value);
  // value is mixed type. return true if key == request_key.
  if (Util::GetScriptType(hiragana_value) != Util::HIRAGANA) {
    return Util::CharsLen(key);
  }

  // Find the first position of character where miss spell occurs.
  int position = 0;
  ConstChar32Iterator key_iter(key);
  for (ConstChar32Iterator hiragana_iter(hiragana_value);
       !hiragana_iter.Done() && !key_iter.Done();
       hiragana_iter.Next(), key_iter.Next(), ++position) {
    if (hiragana_iter.Get() != key_iter.Get()) {
      return position;
    }
  }

  // not find. return the length of key.
  while (!key_iter.Done()) {
    ++position;
    key_iter.Next();
  }

  return position;
}

void DictionaryPredictor::RemoveMissSpelledCandidates(
    size_t request_key_len,
    vector<Result> *results) const {
  DCHECK(results);

  if (results->size() <= 1) {
    return;
  }

  int spelling_correction_size = 5;
  for (size_t i = 0; i < results->size(); ++i) {
    const Result &result = (*results)[i];
    DCHECK(result.node);
    if (!(result.node->attributes & Node::SPELLING_CORRECTION)) {
      continue;
    }

    // Only checks at most 5 spelling corrections to avoid the case
    // like all candidates have SPELLING_CORRECTION.
    if (--spelling_correction_size == 0) {
      return;
    }

    vector<size_t> same_key_index, same_value_index;
    for (size_t j = 0; j < results->size(); ++j) {
      if (i == j) {
        continue;
      }
      const Result &target_result = (*results)[j];
      if (target_result.node->attributes & Node::SPELLING_CORRECTION) {
        continue;
      }
      if (target_result.node->key == result.node->key) {
        same_key_index.push_back(j);
      }
      if (target_result.node->value == result.node->value) {
        same_value_index.push_back(j);
      }
    }

    // delete same_key_index and same_value_index
    if (!same_key_index.empty() && !same_value_index.empty()) {
      (*results)[i].types = NO_PREDICTION;
      for (size_t k = 0; k < same_key_index.size(); ++k) {
        (*results)[same_key_index[k]].types = NO_PREDICTION;
      }
    } else if (same_key_index.empty() && !same_value_index.empty()) {
      (*results)[i].types = NO_PREDICTION;
    } else if (!same_key_index.empty() && same_value_index.empty()) {
      for (size_t k = 0; k < same_key_index.size(); ++k) {
        (*results)[same_key_index[k]].types = NO_PREDICTION;
      }
      if (request_key_len <=
          GetMissSpelledPosition(result.node->key, result.node->value)) {
        (*results)[i].types = NO_PREDICTION;
      }
    }
  }
}

bool DictionaryPredictor::IsAggressiveSuggestion(
    size_t query_len, size_t key_len, int cost,
    bool is_suggestion, size_t total_candidates_size) const {
  // Temporal workaround for fixing the problem where longer sentence-like
  // suggestions are shown when user input is very short.
  // "ただしい" => "ただしいけめんにかぎる"
  // "それでもぼ" => "それでもぼくはやっていない".
  // If total_candidates_size is small enough, we don't perform
  // special filtering. e.g., "せんとち" has only two candidates, so
  // showing "千と千尋の神隠し" is OK.
  // Also, if the cost is too small (< 5000), we allow to display
  // long phrases. Examples include "よろしくおねがいします".
  if (is_suggestion && total_candidates_size >= 10 && key_len >= 8 &&
      cost >= 5000 && query_len <= static_cast<size_t>(0.4 * key_len)) {
    return true;
  }

  return false;
}

size_t DictionaryPredictor::GetRealtimeCandidateMaxSize(
    const Segments &segments, bool mixed_conversion, size_t max_size) const {
  const Segments::RequestType request_type = segments.request_type();
  DCHECK(request_type == Segments::PREDICTION ||
         request_type == Segments::SUGGESTION ||
         request_type == Segments::PARTIAL_PREDICTION ||
         request_type == Segments::PARTIAL_SUGGESTION);
  const int kFewResultThreshold = 8;
  size_t default_size = 6;
  if (segments.segments_size() > 0 &&
      Util::CharsLen(segments.segment(0).key()) >= kFewResultThreshold) {
    // We don't make so many realtime conversion prediction
    // even if we have enough margin, as it's expected less useful.
    max_size = min(max_size, static_cast<size_t>(8));
    default_size = 3;
  }
  size_t size = 0;
  switch (request_type) {
    case Segments::PREDICTION:
      size = mixed_conversion ? max_size - default_size : default_size;
      break;
    case Segments::SUGGESTION:
      // Fewer candidatats are needed basically.
      // But on mixed_conversion mode we should behave like as conversion mode.
      size = mixed_conversion ? default_size : 1;
      break;
    case Segments::PARTIAL_PREDICTION:
      // This is kind of prediction so richer result than PARTIAL_SUGGESTION
      // is needed.
      size = max_size;
      break;
    case Segments::PARTIAL_SUGGESTION:
      // PARTIAL_SUGGESTION works like as conversion mode so returning
      // some candidates is needed.
      size = default_size;
      break;
    default:
      size = 0;  // Never reach here
  }
  return min(max_size, size);
}

void DictionaryPredictor::AggregateRealtimeConversion(
    PredictionTypes types,
    Segments *segments,
    NodeAllocatorInterface *allocator,
    vector<Result> *results) const {
  if (!(types & REALTIME)) {
    return;
  }

  DCHECK(immutable_converter_);
  DCHECK(segments);
  DCHECK(results);
  DCHECK(allocator);

  Segment *segment = segments->mutable_conversion_segment(0);
  DCHECK(segment);
  DCHECK(!segment->key().empty());

  // preserve the previous max_prediction_candidates_size,
  // and candidates_size.
  const size_t prev_candidates_size = segment->candidates_size();
  const size_t prev_max_prediction_candidates_size =
      segments->max_prediction_candidates_size();

  // set how many candidates we want to obtain with
  // immutable converter.
  const bool mixed_conversion = GET_REQUEST(mixed_conversion);
  const size_t realtime_candidates_size = GetRealtimeCandidateMaxSize(
      *segments,
      mixed_conversion,
      prev_max_prediction_candidates_size - prev_candidates_size);

  segments->set_max_prediction_candidates_size(prev_candidates_size +
                                               realtime_candidates_size);

  if (immutable_converter_->Convert(segments) &&
      prev_candidates_size < segment->candidates_size()) {
    // A little tricky treatment:
    // Since ImmutableConverter::Converter creates a set of new candidates,
    // copy them into the array of Results.
    for (size_t i = prev_candidates_size;
         i < segment->candidates_size(); ++i) {
      const Segment::Candidate &candidate = segment->candidate(i);
      Node *node= allocator->NewNode();
      DCHECK(node);
      node->Init();
      node->lid = candidate.lid;
      node->rid = candidate.rid;
      node->wcost = candidate.wcost;
      node->key = candidate.key;
      node->value = candidate.value;
      if (candidate.attributes & Segment::Candidate::SPELLING_CORRECTION) {
        node->attributes |= Node::SPELLING_CORRECTION;
      }
      if (candidate.attributes & Segment::Candidate::NO_VARIANTS_EXPANSION) {
        node->attributes |= Node::NO_VARIANTS_EXPANSION;
      }
      if (candidate.attributes & Segment::Candidate::USER_DICTIONARY) {
        node->attributes |= Node::USER_DICTIONARY;
      }
      results->push_back(Result(node, REALTIME));
    }
    // remove candidates created by ImmutableConverter.
    segment->erase_candidates(prev_candidates_size,
                              segment->candidates_size() -
                              prev_candidates_size);
    // restore the max_prediction_candidates_size.
    segments->set_max_prediction_candidates_size(
        prev_max_prediction_candidates_size);
  } else {
    LOG(WARNING) << "Convert failed";
  }
}

size_t DictionaryPredictor::GetUnigramCandidateCutoffThreshold(
    const Segments &segments) const {
  DCHECK(segments.request_type() == Segments::PREDICTION ||
         segments.request_type() == Segments::SUGGESTION);
  if (segments.request_type() == Segments::PREDICTION) {
    // If PREDICTION, many candidates are needed than SUGGESTION.
    return kPredictionMaxNodesSize;
  }
  return kSuggestionMaxNodesSize;
}

void DictionaryPredictor::AggregateUnigramPrediction(
    PredictionTypes types,
    const ConversionRequest &request,
    Segments *segments,
    NodeAllocatorInterface *allocator,
    vector<Result> *results) const {
  if (!(types & UNIGRAM)) {
    return;
  }

  DCHECK(segments);
  DCHECK(allocator);
  DCHECK(results);
  DCHECK(segments->request_type() == Segments::PREDICTION ||
         segments->request_type() == Segments::SUGGESTION);

  const bool mixed_conversion = GET_REQUEST(mixed_conversion);
  if (!mixed_conversion) {
    AggregateUnigramCandidate(request, segments, allocator, results);
  } else {
    AggregateUnigramCandidateForMixedConversion(
        request, segments, allocator, results);
  }
}

void DictionaryPredictor::AggregateUnigramCandidate(
    const ConversionRequest &request,
    Segments *segments,
    NodeAllocatorInterface *allocator,
    vector<Result> *results) const {
  DCHECK(segments);
  DCHECK(allocator);
  DCHECK(results);
  DCHECK(dictionary_);

  const size_t cutoff_threshold = GetUnigramCandidateCutoffThreshold(*segments);
  allocator->set_max_nodes_size(cutoff_threshold);
  // no history key
  const Node *unigram_node = GetPredictiveNodes(
      dictionary_, "", request, *segments, allocator);

  const size_t prev_results_size = results->size();
  size_t unigram_results_size = 0;
  for (; unigram_node != NULL; unigram_node = unigram_node->bnext) {
    results->push_back(Result(unigram_node, UNIGRAM));
    ++unigram_results_size;
  }

  // if size reaches max_nodes_size (== cutoff_threshold).
  // we don't show the candidates, since disambiguation from
  // 256 candidates is hard. (It may exceed max_nodes_size, because this is
  // just a limit for each backend, so total number may be larger)
  if (unigram_results_size >= allocator->max_nodes_size()) {
    results->resize(prev_results_size);
  }
}

void DictionaryPredictor::AggregateUnigramCandidateForMixedConversion(
    const ConversionRequest &request,
    Segments *segments,
    NodeAllocatorInterface *allocator,
    vector<Result> *results) const {
  const size_t cutoff_threshold = kPredictionMaxNodesSize;
  allocator->set_max_nodes_size(cutoff_threshold);

  // No history key
  const Node *unigram_node = GetPredictiveNodes(
      dictionary_, "", request, *segments, allocator);

  // Move pointers to a vector.
  // TODO(hidehiko): reserve the nodes' size, when we make an interface
  //   to return the number of returned nodes by dictionary look-up.
  vector<const Node*> nodes;
  for (; unigram_node != NULL; unigram_node = unigram_node->bnext) {
    nodes.push_back(unigram_node);
  }

  // Hereafter, we split "Needed Nodes" and "(maybe) Unneeded Nodes."
  // The algorithm is:
  // 1) Take the Node with minimum cost.
  // 2) Remove nodes which is "redundant" (defined by MaybeRedundant),
  //    from remaining nodes.
  // 3) Repeat 1) and 2) five times.
  // Note: to reduce the number of memory allocation, we swap out the
  //   "redundant" nodes to the end of the |nodes| vector.
  const size_t kDeleteTrialNum = 5;
  typedef vector<const Node *>::iterator Iter;

  // min_iter is the beginning of the remaining nodes (inclusive), and
  // max_iter is the end of the remaining nodes (exclusive).
  Iter min_iter = nodes.begin();
  Iter max_iter = nodes.end();
  for (size_t i = 0; i < kDeleteTrialNum; ++i) {
    if (min_iter == max_iter) {
      break;
    }

    // Find the Node with minimum cost. Swap it with the beginning element.
    iter_swap(min_iter, min_element(min_iter, max_iter, NodeLess()));
    const Node *reference_node = *min_iter;

    // Preserve the reference node.
    ++min_iter;

    // Traverse all remaining elements and check if each node is redundant.
    for (Iter iter = min_iter; iter != max_iter; ) {
      if (MaybeRedundant(reference_node, *iter)) {
        // Swap out the redundant node.
        --max_iter;
        iter_swap(iter, max_iter);
      } else {
        ++iter;
      }
    }
  }

  // Then the |nodes| contains;
  // [begin, min_iter): reference nodes in the above loop.
  // [max_iter, end): (maybe) redundant nodes.
  // [min_iter, max_iter): remaining nodes.
  // Here, we revive the redundant nodes up to five in the node cost order.
  const size_t kDoNotDeleteNum = 5;
  if (distance(max_iter, nodes.end()) >= kDoNotDeleteNum) {
    partial_sort(max_iter, max_iter + kDoNotDeleteNum, nodes.end());
    max_iter += kDoNotDeleteNum;
  } else {
    max_iter = nodes.end();
  }

  // Finally output the result.
  results->reserve(results->size() + distance(nodes.begin(), max_iter));
  for (Iter iter = nodes.begin(); iter != max_iter; ++iter) {
    results->push_back(Result(*iter, UNIGRAM));
  }
}

void DictionaryPredictor::AggregateBigramPrediction(
    PredictionTypes types,
    const ConversionRequest &request,
    Segments *segments,
    NodeAllocatorInterface *allocator,
    vector<Result> *results) const {
  if (!(types & BIGRAM)) {
    return;
  }

  DCHECK(segments);
  DCHECK(results);
  DCHECK(dictionary_);
  DCHECK(allocator);

  // TODO(toshiyuki): Support suggestion from the last 2 histories.
  //  ex) 六本木+ヒルズ->レジデンス
  string history_key, history_value;
  if (!GetHistoryKeyAndValue(*segments, &history_key, &history_value)) {
    return;
  }
  AddBigramResultsFromHistory(
      history_key, history_value, request, segments, allocator, results);
}

void DictionaryPredictor::AddBigramResultsFromHistory(
    const string &history_key,
    const string &history_value,
    const ConversionRequest &request,
    Segments *segments,
    NodeAllocatorInterface *allocator,
    vector<Result> *results) const {
  // Check that history_key/history_value are in the dictionary.
  const Node *history_node = LookupKeyValueFromDictionary(
      history_key, history_value, allocator);

  // History value is not found in the dictionary.
  // User may create this the history candidate from T13N or segment
  // expand/shrinkg operations.
  if (history_node == NULL) {
    return;
  }

  const size_t max_nodes_size =
      (segments->request_type() == Segments::PREDICTION) ?
      kPredictionMaxNodesSize : kSuggestionMaxNodesSize;
  allocator->set_max_nodes_size(max_nodes_size);

  const size_t prev_results_size = results->size();

  const Node *bigram_node = GetPredictiveNodes(
      dictionary_, history_key, request, *segments, allocator);
  size_t bigram_results_size = 0;
  for (; bigram_node != NULL; bigram_node = bigram_node->bnext) {
    // filter out the output (value)'s prefix doesn't match to
    // the history value.
    if (Util::StartsWith(bigram_node->value, history_value) &&
        (bigram_node->value.size() > history_value.size())) {
      results->push_back(Result(bigram_node, BIGRAM));
      ++bigram_results_size;
    }
  }

  // if size reaches max_nodes_size,
  // we don't show the candidates, since disambiguation from
  // 256 candidates is hard. (It may exceed max_nodes_size, because this is
  // just a limit for each backend, so total number may be larger)
  if (bigram_results_size >= allocator->max_nodes_size()) {
    results->resize(prev_results_size);
    return;
  }

  // Obtain the character type of the last history value.
  const size_t history_value_size = Util::CharsLen(history_value);
  if (history_value_size == 0) {
    return;
  }

  const Util::ScriptType history_ctype = Util::GetScriptType(history_value);
  const Util::ScriptType last_history_ctype =
      Util::GetScriptType(Util::SubString(history_value,
                                          history_value_size - 1, 1));
  for (size_t i = prev_results_size; i < results->size(); ++i) {
    CheckBigramResult(history_node, history_ctype, last_history_ctype,
                      allocator, &(*results)[i]);
  }
}

// Filter out irrelevant bigrams. For example, we don't want to
// suggest "リカ" from the history "アメ".
void DictionaryPredictor::CheckBigramResult(
    const Node *history_node,
    const Util::ScriptType history_ctype,
    const Util::ScriptType last_history_ctype,
    NodeAllocatorInterface *allocator,
    Result *result) const {
  DCHECK(history_node);
  DCHECK(result);

  const Node *node = result->node;
  DCHECK(node);
  const string &history_key = history_node->key;
  const string &history_value = history_node->value;
  const string key(node->key, history_key.size(),
                   node->key.size() - history_key.size());
  const string value(node->value, history_value.size(),
                     node->value.size() - history_value.size());

  // Don't suggest 0-length key/value.
  if (key.empty() || value.empty()) {
    result->types = NO_PREDICTION;
    return;
  }

  const Util::ScriptType ctype =
      Util::GetScriptType(Util::SubString(value, 0, 1));

  if (history_ctype == Util::KANJI &&
      ctype == Util::KATAKANA) {
    // Do not filter "六本木ヒルズ"
    return;
  }

  // If freq("アメ") < freq("アメリカ"), we don't
  // need to suggest it. As "アメリカ" should already be
  // suggested when user type "アメ".
  // Note that wcost = -500 * log(prob).
  if (ctype != Util::KANJI &&
      history_node->wcost > node->wcost) {
    result->types = NO_PREDICTION;
    return;
  }

  // If character type doesn't change, this boundary might NOT
  // be a word boundary. If character type is HIRAGANA,
  // we don't trust it. If Katakana, only trust iif the
  // entire key is reasonably long.
  if (ctype == last_history_ctype &&
      (ctype == Util::HIRAGANA ||
       (ctype == Util::KATAKANA && Util::CharsLen(node->key) <= 5))) {
    result->types = NO_PREDICTION;
    return;
  }

  // The suggested key/value pair must exist in the dictionary.
  // For example, we don't want to suggest "ターネット" from
  // the history "イン".
  // If character type is Kanji and the suggestion is not a
  // zero_query_suggestion, we relax this condition, as there are
  // many Kanji-compounds which may not in the dictionary. For example,
  // we want to suggest "霊長類研究所" from the history "京都大学".
  if (ctype == Util::KANJI && Util::CharsLen(value) >= 2) {
    // Do not filter this.
    // TODO(toshiyuki): one-length kanji prediciton may be annoying other than
    // some exceptions, "駅", "口", etc
    return;
  }

  if (NULL == LookupKeyValueFromDictionary(key, value, allocator)) {
    result->types = NO_PREDICTION;
    return;
  }
}

const Node *DictionaryPredictor::GetPredictiveNodes(
    const DictionaryInterface *dictionary,
    const string &history_key,
    const ConversionRequest &request,
    const Segments &segments,
    NodeAllocatorInterface *allocator) const {
  if (!request.has_composer() ||
      !FLAGS_enable_expansion_for_dictionary_predictor) {
    const string input_key = history_key + segments.conversion_segment(0).key();
    return dictionary->LookupPredictive(input_key.c_str(),
                                        input_key.size(),
                                        allocator);
  } else {
    // If we have ambiguity for the input, get expanded key.
    // Example1 roman input: for "あk", we will get |base|, "あ" and |expanded|,
    // "か", "き", etc
    // Example2 kana input: for "あか", we will get |base|, "あ" and |expanded|,
    // "か", and "が".
    string base;
    set<string> expanded;
    request.composer().GetQueriesForPrediction(&base, &expanded);
    const string input_key = history_key + base;
    DictionaryInterface::Limit limit;
    scoped_ptr<Trie<string> > trie(NULL);
    if (expanded.size() > 0) {
      trie.reset(new Trie<string>);
      for (set<string>::const_iterator itr = expanded.begin();
           itr != expanded.end(); ++itr) {
        trie->AddEntry(*itr, "");
      }
      limit.begin_with_trie = trie.get();
    }
    return dictionary->LookupPredictiveWithLimit(input_key.c_str(),
                                                 input_key.size(),
                                                 limit,
                                                 allocator);
  }
}

const Node *DictionaryPredictor::GetPredictiveNodesForEnglish(
    const DictionaryInterface *dictionary,
    const string &history_key,
    const ConversionRequest &request,
    const Segments &segments,
    NodeAllocatorInterface *allocator) const {
  if (!request.has_composer()) {
    const string input_key = history_key + segments.conversion_segment(0).key();
    return dictionary->LookupPredictive(input_key.c_str(),
                                        input_key.size(),
                                        allocator);
  }

  string input_key;
  request.composer().GetQueryForPrediction(&input_key);
  // We don't look up English words when key length is one.
  if (input_key.size() < 2) {
    return NULL;
  }
  Node *head = NULL;
  if (Util::IsUpperAscii(input_key)) {
    // For upper case key, look up its lower case version and then transform the
    // results to upper case.
    string key(input_key);
    Util::LowerString(&key);
    head = dictionary->LookupPredictive(key.c_str(), key.size(), allocator);
    for (Node *node = head; node != NULL; node = node->bnext) {
      Util::UpperString(&node->value);
    }
  } else if (Util::IsCapitalizedAscii(input_key)) {
    // For capitalized key, look up its lower case version and then transform
    // the results to capital.
    string key(input_key);
    Util::LowerString(&key);
    head = dictionary->LookupPredictive(key.c_str(), key.size(), allocator);
    for (Node *node = head; node != NULL; node = node->bnext) {
      Util::CapitalizeString(&node->value);
    }
  } else {
    // For other cases (lower and as-is), just look up directly.
    head = dictionary->LookupPredictive(input_key.c_str(),
                                        input_key.size(),
                                        allocator);
  }
  // If input mode is FULL_ASCII, then convert the results to full-width.
  if (request.composer().GetInputMode() == transliteration::FULL_ASCII) {
    string tmp;
    for (Node *node = head; node != NULL; node = node->bnext) {
      tmp.assign(node->value);
      Util::HalfWidthAsciiToFullWidthAscii(tmp, &node->value);
    }
  }
  return head;
}

void DictionaryPredictor::AggregateSuffixPrediction(
    PredictionTypes types,
    const ConversionRequest &request,
    Segments *segments,
    NodeAllocatorInterface *allocator,
    vector<Result> *results) const {
  if (!(types & SUFFIX)) {
    return;
  }

  DCHECK(allocator);

  size_t history_size = segments->history_segments_size();
  bool has_number_history = false;

  if (history_size) {
    const string &history_key =
        segments->history_segment(history_size - 1).key();
    has_number_history = NumberUtil::IsDecimalInteger(history_key);
  }

  if (has_number_history && segments->conversion_segment(0).key().size() == 0) {
    const string &history_key =
        segments->history_segment(history_size - 1).key();
    vector<string> suffixes;
    GetNumberSuffixArray(history_key, &suffixes);
    DCHECK_GT(suffixes.size(), 0);
    Node *result = NULL;
    int cost = 0;

    for (vector<string>::const_iterator it = suffixes.begin();
         it != suffixes.end(); ++it) {
      // Increment cost to show the candidates in order.
      const int kSuffixPenalty = 10;

      Node *node = allocator->NewNode();
      DCHECK(node);
      node->Init();
      node->wcost = cost;
      node->key = *it;  // Filler; same as the value
      node->value = *it;
      node->lid = counter_suffix_word_id_;
      node->rid = counter_suffix_word_id_;
      node->bnext = result;
      result = node;
      results->push_back(Result(node, SUFFIX));
      cost += kSuffixPenalty;
    }
  } else {
    const string kEmptyHistoryKey = "";
    const Node *node = GetPredictiveNodes(
        suffix_dictionary_, kEmptyHistoryKey, request, *segments, allocator);
    for (; node != NULL; node = node->bnext) {
      if (Util::CharsLen(node->value) <= 1) {
        // Do not suggest one char suffix.
        // TODO(toshiyuki): If this works well, fix from the dictionary
        // generation process.
        continue;
      }
      results->push_back(Result(node, SUFFIX));
    }
  }
}

void DictionaryPredictor::AggregateEnglishPrediction(
    PredictionTypes types,
    const ConversionRequest &request,
    Segments *segments,
    NodeAllocatorInterface *allocator,
    vector<Result> *results) const {
  if (!(types & ENGLISH)) {
    return;
  }
  DCHECK(segments);
  DCHECK(allocator);
  DCHECK(results);
  DCHECK(dictionary_);

  const size_t cutoff_threshold = GetUnigramCandidateCutoffThreshold(*segments);
  allocator->set_max_nodes_size(cutoff_threshold);

  // Currently, history key is never utilized.
  // TODO(noriyukit): Come up with a way of utilizing it.
  const string kEmptyHistoryKey = "";
  const Node *unigram_node = GetPredictiveNodesForEnglish(
      dictionary_, kEmptyHistoryKey, request, *segments, allocator);

  const size_t prev_results_size = results->size();
  size_t unigram_results_size = 0;
  for (; unigram_node != NULL; unigram_node = unigram_node->bnext) {
    results->push_back(Result(unigram_node, ENGLISH));
    // If size reaches max_nodes_size (== cutoff_threshold).  we don't show the
    // candidates, since disambiguation from 256 candidates is hard. (It may
    // exceed max_nodes_size, because this is just a limit for each backend, so
    // total number may be larger.)
    // TODO(team): If DictionaryInterface provides the information of size of
    // returned node list, we can avoid unnecessary push_backs and resize here.
    if (++unigram_results_size >= allocator->max_nodes_size()) {
      results->resize(prev_results_size);
      return;
    }
  }
}

bool DictionaryPredictor::IsZipCodeRequest(const string &key) const {
  if (key.empty()) {
    return false;
  }
  const char *begin = key.data();
  const char *end = key.data() + key.size();
  size_t mblen = 0;
  while (begin < end) {
    Util::UTF8ToUCS2(begin, end, &mblen);
    if (mblen == 1 &&
        ((*begin >= '0' && *begin <= '9') || *begin == '-')) {
      // do nothing
    } else {
      return false;
    }

    begin += mblen;
  }

  return true;
}

DictionaryPredictor::PredictionTypes DictionaryPredictor::GetPredictionTypes(
    const ConversionRequest &request, const Segments &segments) const {
  if (segments.request_type() == Segments::CONVERSION) {
    VLOG(2) << "request type is CONVERSION";
    return NO_PREDICTION;
  }

  if (segments.conversion_segments_size() < 1) {
    VLOG(2) << "segment size < 1";
    return NO_PREDICTION;
  }

  if (IsLatinInputMode(request)) {
    return ENGLISH | REALTIME;
  }

  const string &key = segments.conversion_segment(0).key();

  // default setting
  PredictionTypes result = NO_PREDICTION;

  // support realtime conversion.
  const size_t kMaxKeySize = 300;   // 300 bytes in UTF8

  const bool mixed_conversion = GET_REQUEST(mixed_conversion);

  if (segments.request_type() == Segments::PARTIAL_SUGGESTION) {
    result |= REALTIME;
  } else if ((GET_CONFIG(use_realtime_conversion) || mixed_conversion) &&
      key.size() > 0 && key.size() < kMaxKeySize) {
    result |= REALTIME;
  }

  if (!GET_CONFIG(use_dictionary_suggest) &&
      segments.request_type() == Segments::SUGGESTION) {
    VLOG(2) << "no_dictionary_suggest";
    return result;
  }

  const bool zero_query_suggestion = GET_REQUEST(zero_query_suggestion);

  const size_t key_len = Util::CharsLen(key);
  if (key_len == 0 && !zero_query_suggestion) {
    return result;
  }

  // Never trigger prediction if key looks like zip code.
  const bool is_zip_code = DictionaryPredictor::IsZipCodeRequest(key);

  if (segments.request_type() == Segments::SUGGESTION &&
      is_zip_code && key_len < 6) {
    return result;
  }

  const int kMinUnigramKeyLen = zero_query_suggestion ? 1 : 3;

  // unigram based suggestion requires key_len >= kMinUnigramKeyLen.
  // Providing suggestions from very short user input key is annoying.
  if ((segments.request_type() == Segments::PREDICTION && key_len >= 1) ||
      key_len >= kMinUnigramKeyLen) {
    result |= UNIGRAM;
  }

  const size_t history_segments_size = segments.history_segments_size();
  if (history_segments_size > 0) {
    const Segment &history_segment =
        segments.history_segment(history_segments_size - 1);
    const int kMinHistoryKeyLen = zero_query_suggestion ? 2 : 3;
    // even in PREDICTION mode, bigram-based suggestion requires that
    // the length of previous key is >= kMinBigramKeyLen.
    // It also implies that bigram-based suggestion will be triggered,
    // even if the current key length is short enough.
    // TOOD(taku): this setting might be aggressive if the current key
    // looks like Japanese particle like "が|で|は"
    // If the current key looks like particle, we can make the behavior
    // less aggressive.
    if (history_segment.candidates_size() > 0 &&
        Util::CharsLen(history_segment.candidate(0).key) >= kMinHistoryKeyLen) {
      result |= BIGRAM;
    }
  }

  if (history_segments_size > 0 && zero_query_suggestion) {
    result |= SUFFIX;
  }

  return result;
}
}  // namespace mozc
