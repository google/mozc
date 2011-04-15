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

#include "prediction/dictionary_predictor.h"

#include <limits.h>   // INT_MIN
#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>
#include "base/base.h"
#include "base/init.h"
#include "base/util.h"
#include "converter/connector_interface.h"
#include "converter/converter_interface.h"
#include "converter/immutable_converter_interface.h"
#include "converter/node.h"
#include "converter/segments.h"
#include "dictionary/dictionary_interface.h"
#include "prediction/svm_model.h"
#include "prediction/suggestion_filter.h"
#include "prediction/predictor_interface.h"
#include "rewriter/variants_rewriter.h"
#include "session/commands.pb.h"
#include "session/config_handler.h"
#include "session/config.pb.h"


namespace mozc {

namespace {

// Note that PREDICTION mode is much slower than SUGGESTION.
// Number of prediction calls should be minimized.
const size_t kSuggestionMaxNodesSize = 256;
const size_t kPredictionMaxNodesSize = 100000;

// define kLidGroup[]
#include "prediction/suggestion_feature_pos_group.h"
}  // namespace

DictionaryPredictor::DictionaryPredictor()
    : dictionary_(DictionaryFactory::GetDictionary()),
      immutable_converter_(
          ImmutableConverterFactory::GetImmutableConverter()) {}

DictionaryPredictor::~DictionaryPredictor() {}

bool DictionaryPredictor::Predict(Segments *segments) const {
  if (segments == NULL) {
    return false;
  }

  const PredictionType prediction_type = GetPredictionType(*segments);
  if (prediction_type == NO_PREDICTION) {
    return false;
  }

  vector<Result> results;
  AggregateRealtimeConversion(prediction_type, segments, &results);
  AggregateUnigramPrediction(prediction_type, segments, &results);
  AggregateBigramPrediction(prediction_type, segments, &results);

  if (results.empty()) {
    VLOG(2) << "|result| is empty";
    return false;
  }

  bool mixed_conversion = false;

  if (mixed_conversion) {
    SetLMScore(*segments, &results);
  } else {
    SetSVMScore(*segments, &results);
  }

  const size_t size = min(segments->max_prediction_candidates_size(),
                          results.size());

  // Collect values of non-spelling correction predictions.
  // This is to stop showing inadequate <did you mean?> notations.
  //
  // ex. When the key is "あぼ", the following candidates are contained:
  // - "アボカド"
  // - "アボカド" <did you mean?>
  // but we should not show ["アボカド" <did you mean?>].
  //
  // So we don't allow spelling correction predictions that
  // there are non-spelling correction predictions with the same values.
  //
  // TODO(takiba): this may cause bad performance
  set<string> non_spelling_correction_values;
  for (size_t i = 0; i < results.size(); ++i) {
    const Node *node = results[i].node;
    if (!(node->attributes & Node::SPELLING_CORRECTION)) {
      non_spelling_correction_values.insert(node->value);
    }
  }

  // Instead of sorting all the results, we construct a heap.
  // This is done in linear time and
  // we can pop as many results as we need effectively.
  make_heap(results.begin(), results.end(), ResultCompare());

  Segment *segment = segments->mutable_conversion_segment(0);
  DCHECK(segment);

  int added = 0;
  set<string> seen;
  const string &input_key = segments->conversion_segment(0).key();
  const size_t key_length = Util::CharsLen(input_key);

  string history_key, history_value;
  GetHistoryKeyAndValue(*segments, &history_key, &history_value);
  const string bigram_key = history_key + input_key;

  for (size_t i = 0; i < results.size(); ++i) {
    if (added >= size || results[i].score == INT_MIN) {
      break;
    }

    pop_heap(results.begin(), results.end() - i, ResultCompare());
    const Result &result = results[results.size() - i - 1];
    const Node *node = result.node;
    DCHECK(node);

    string lower_value = node->value;
    Util::LowerString(&lower_value);
    // TODO(taku): do we have to use BadSuggestion for mixed conversion?
    if (SuggestionFilter::IsBadSuggestion(lower_value)) {
      continue;
    }

    // don't suggest exactly the same candidate as key.
    // if |mixed_conversion| is true, that's not the case.
    if (!mixed_conversion &&
        !(result.type & REALTIME) &&
        (((result.type & BIGRAM) &&
          bigram_key == node->value) ||
         (!(result.type & BIGRAM) &&
          input_key == node->value))) {
      continue;
    }

    // The below conditions should never happen
    if ((result.type & DictionaryPredictor::BIGRAM) &&
        (node->key.size() <= history_key.size() ||
         (node->value.size() <= history_value.size()))) {
      LOG(ERROR) << "Invalid bigram key/value";
      continue;
    }

    // filter bad spelling correction
    if (node->attributes & Node::SPELLING_CORRECTION) {
      if (non_spelling_correction_values.count(node->value)) {
        continue;
      }
      // TODO(takiba): 0.75 is just heurestics.
      if (key_length < Util::CharsLen(node->key) * 0.75) {
        continue;
      }
    }

    // TODO(taku): call CharacterFormManager for these values
    string key, value;
    if (result.type & BIGRAM) {
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

    Segment::Candidate *candidate = segment->push_back_candidate();
    DCHECK(candidate);

    candidate->Init();
    VLOG(2) << "DictionarySuggest: " << node->wcost << " " << value;

    candidate->content_key = key;
    candidate->content_value = value;
    candidate->key = key;
    candidate->value = value;
    candidate->lid = node->lid;
    candidate->rid = node->rid;
    candidate->wcost = node->wcost;
    candidate->cost = node->wcost;  // This cost is not correct.
    candidate->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
    if (node->attributes & Node::SPELLING_CORRECTION) {
      candidate->attributes |= Segment::Candidate::SPELLING_CORRECTION;
    }

    // Don't provide any descriptions for dictionary suggests
#ifdef _DEBUG
    const char kDescription[] = "Dictionary Suggest";
    candidate->description = kDescription;
#endif

    VariantsRewriter::SetDescriptionForPrediction(candidate);

    ++added;
  }

  return added > 0;
}

bool DictionaryPredictor::GetHistoryKeyAndValue(
    const Segments &segments,
    string *key, string *value) const {
  DCHECK(key);
  DCHECK(value);
  if (segments.history_segments_size() > 0) {
    const Segment &history_segment =
        segments.history_segment(segments.history_segments_size() - 1);
    if (history_segment.candidates_size() > 0) {
      key->assign(history_segment.candidate(0).key);
      value->assign(history_segment.candidate(0).value);
      return true;
    }
  }
  return false;
}

void DictionaryPredictor::SetLMScore(const Segments &segments,
                                     vector<Result> *results) const {
  DCHECK(results);

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
    DCHECK(node);
    int cost = node->wcost;
    if ((*results)[i].type & BIGRAM) {
      cost -= prev_cost;
    } else {
      cost += ConnectorFactory::GetConnector()->GetTransitionCost(rid,
                                                                  node->lid);
    }
    (*results)[i].score = -cost;
  }
}

void DictionaryPredictor::SetSVMScore(const Segments &segments,
                                      vector<Result> *results) const {
  DCHECK(results);
  const string &input_key = segments.conversion_segment(0).key();
  string history_key, history_value;
  GetHistoryKeyAndValue(segments, &history_key, &history_value);
  const string bigram_key = history_key + input_key;

  const bool is_zip_code = DictionaryPredictor::IsZipCodeRequest(input_key);
  const bool is_suggestion = (segments.request_type() ==
                              Segments::SUGGESTION);
  vector<pair<int, double> > feature;
  for (size_t i = 0; i < results->size(); ++i) {
    // use the same scoring function for both unigram/bigram.
    // Bigram will be boosted because we pass the previous
    // key as a context information.
    const Node *node = (*results)[i].node;
    DCHECK(node);
    (*results)[i].score = GetSVMScore(
        ((*results)[i].type & BIGRAM) ? bigram_key : input_key,
        node->key, node->value, node->wcost, node->lid,
        is_zip_code, is_suggestion, results->size(), &feature);
  }
}

void DictionaryPredictor::AggregateRealtimeConversion(
    PredictionType type,
    Segments *segments,
    vector<Result> *results) const {
  if (!(type & REALTIME)) {
    return;
  }

  DCHECK(immutable_converter_);
  DCHECK(segments);
  DCHECK(results);

  NodeAllocatorInterface *allocator = segments->node_allocator();
  DCHECK(allocator);

  Segment *segment = segments->mutable_conversion_segment(0);
  DCHECK(segment);

  // preserve the previous max_prediction_candidates_size,
  // and candidates_size.
  const size_t prev_candidates_size = segment->candidates_size();
  const size_t prev_max_prediction_candidates_size =
      segments->max_prediction_candidates_size();

  // set how many candidates we want to obtain with
  // immutable converter.
  size_t realtime_candidates_size = 1;
  bool mixed_conversion = false;

  if (mixed_conversion ||
      segments->request_type() == Segments::PREDICTION) {
    // obtain 10 results from realtime conversion when
    // mixed_conversion or PREDICTION mode.
    // TODO(taku): want to change the size more adaptivly.
    // For example, if key_len is long, we don't need to
    // obtain 10 results.
    realtime_candidates_size = 10;
  }

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

void DictionaryPredictor::AggregateUnigramPrediction(
    PredictionType type,
    Segments *segments,
    vector<Result> *results) const {
  if (!(type & UNIGRAM)) {
    return;
  }

  DCHECK(segments);
  DCHECK(results);
  DCHECK(dictionary_);

  const string &input_key = segments->conversion_segment(0).key();
  NodeAllocatorInterface *allocator = segments->node_allocator();
  DCHECK(allocator);

  const size_t max_nodes_size =
      (segments->request_type() == Segments::PREDICTION) ?
      kPredictionMaxNodesSize : kSuggestionMaxNodesSize;
  allocator->set_max_nodes_size(max_nodes_size);

  const size_t prev_results_size = results->size();
  const Node *unigram_node = dictionary_->LookupPredictive(input_key.c_str(),
                                                           input_key.size(),
                                                           allocator);
  size_t unigram_results_size = 0;
  for (; unigram_node != NULL; unigram_node = unigram_node->bnext) {
    results->push_back(Result(unigram_node, UNIGRAM));
    ++unigram_results_size;
  }

  // if size reaches max_nodes_size.
  // we don't show the candidates, since disambiguation from
  // 256 candidates is hard. (It may exceed max_nodes_size, because this is
  // just a limit for each backend, so total number may be larger)
  if (unigram_results_size >= allocator->max_nodes_size()) {
    results->resize(prev_results_size);
  }
}

void DictionaryPredictor::AggregateBigramPrediction(
    PredictionType type,
    Segments *segments,
    vector<Result> *results) const {
  if (!(type & BIGRAM)) {
    return;
  }

  DCHECK(segments);
  DCHECK(results);
  DCHECK(dictionary_);

  const string &input_key = segments->conversion_segment(0).key();
  NodeAllocatorInterface *allocator = segments->node_allocator();
  DCHECK(allocator);

  const size_t max_nodes_size =
      (segments->request_type() == Segments::PREDICTION) ?
      kPredictionMaxNodesSize : kSuggestionMaxNodesSize;
  allocator->set_max_nodes_size(max_nodes_size);

  string history_key, history_value;
  GetHistoryKeyAndValue(*segments, &history_key, &history_value);
  const string bigram_key = history_key + input_key;

  const size_t prev_results_size = results->size();

  const Node *bigram_node = dictionary_->LookupPredictive(bigram_key.c_str(),
                                                          bigram_key.size(),
                                                          allocator);
  size_t bigram_results_size  = 0;
  for (; bigram_node != NULL; bigram_node = bigram_node->bnext) {
    // filter out the output (value)'s prefix doesn't match to
    // the history value.
    if (Util::StartsWith(bigram_node->value, history_value)) {
      results->push_back(Result(bigram_node, BIGRAM));
      ++bigram_results_size;
    }
  }

  // if size reaches max_nodes_size.
  // we don't show the candidates, since disambiguation from
  // 256 candidates is hard. (It may exceed max_nodes_size, because this is
  // just a limit for each backend, so total number may be larger)
  if (bigram_results_size >= allocator->max_nodes_size()) {
    results->resize(prev_results_size);
  }
}

namespace {
enum {
  BIAS           = 1,
  QUERY_LEN      = 2,
  KEY_LEN        = 3,
  KEY_LEN1       = 4,
  REMAIN_LEN0    = 5,
  VALUE_LEN      = 6,
  COST           = 7,
  CONTAINS_ALPHABET = 8,
  POS_BASE       = 10,
  MAX_FEATURE_ID = 2000,
};
}   // namespace

#define ADD_FEATURE(i, v) \
  { feature->push_back(pair<int, double>(i, v)); } while (0)

int DictionaryPredictor::GetSVMScore(
    const string &query,
    const string &key,
    const string &value,
    uint16 cost,
    uint16 lid,
    bool is_zip_code,
    bool is_suggestion,
    size_t total_candidates_size,
    vector<pair<int, double> > *feature) const {
  feature->clear();

  // We use the cost for ranking if query looks like zip code
  // TODO(taku): find out better and more sophisicated way instead of this
  // huristics.
  if (is_zip_code) {
    ADD_FEATURE(COST, cost / 500.0);
  } else {
    const size_t query_len = Util::CharsLen(query);
    const size_t key_len = Util::CharsLen(key);

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
      return INT_MIN;
    }

    const bool contains_alphabet =
        Util::ContainsScriptType(value, Util::ALPHABET);
    ADD_FEATURE(BIAS, 1.0);
    ADD_FEATURE(QUERY_LEN, log(1.0 + query_len));
    ADD_FEATURE(KEY_LEN, log(1.0 + key_len));
    ADD_FEATURE(KEY_LEN1, key_len == 1 ? 1 : 0);
    ADD_FEATURE(REMAIN_LEN0, query_len == key_len ? 1 : 0);
    ADD_FEATURE(VALUE_LEN, log(1.0 + Util::CharsLen(value)));
    ADD_FEATURE(COST, cost / 500.0);
    ADD_FEATURE(CONTAINS_ALPHABET, contains_alphabet ? 1 : 0);
    ADD_FEATURE(POS_BASE + kLidGroup[lid], 1.0);
  }

  return static_cast<int>(1000 * SVMClassify(*feature));
}
#undef ADD_FEATURE

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

DictionaryPredictor::PredictionType
DictionaryPredictor::GetPredictionType(const Segments &segments) const {
  if (segments.request_type() == Segments::CONVERSION) {
    VLOG(2) << "request type is CONVERSION";
    return NO_PREDICTION;
  }

  if (segments.node_allocator() == NULL) {
    LOG(WARNING) << "NodeAllocator is NULL";
    return NO_PREDICTION;
  }

  if (segments.conversion_segments_size() < 1) {
    VLOG(2) << "segment size < 1";
    return NO_PREDICTION;
  }

  const string &key = segments.conversion_segment(0).key();

  // default setting
  int result = NO_PREDICTION;

  // support realtime conversion.
  const size_t kMaxKeySize = 300;   // 300 bytes in UTF8

  bool mixed_conversion = false;

  if ((GET_CONFIG(use_realtime_conversion) || mixed_conversion) &&
      key.size() > 0 && key.size() < kMaxKeySize) {
    result |= REALTIME;
  }

  if (!GET_CONFIG(use_dictionary_suggest) &&
      segments.request_type() == Segments::SUGGESTION) {
    VLOG(2) << "no_dictionary_suggest";
    return static_cast<PredictionType>(result);
  }

  bool zero_query_suggestion = false;

  const size_t key_len = Util::CharsLen(key);
  if (key_len == 0 && !zero_query_suggestion) {
    return static_cast<PredictionType>(result);
  }

  // Never trigger prediction if key looks like zip code.
  const bool is_zip_code = DictionaryPredictor::IsZipCodeRequest(key);

  if (segments.request_type() == Segments::SUGGESTION &&
      is_zip_code && key_len < 6) {
    return static_cast<PredictionType>(result);
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
    const int kMinBigramKeyLen = zero_query_suggestion ? 2 : 3;
    // even in PREDICTION mode, bigram-based suggestion requires that
    // the length of previous key is >= kMinBigramKeyLen.
    // It also implies that bigram-based suggestion will be triggered,
    // even if the current key length is short enough.
    // TOOD(taku): this setting might be aggressive if the current key
    // looks like Japanese particle like "が|で|は"
    // If the current key looks like particle, we can make the behavior
    // less aggressive.
    if (history_segment.candidates_size() > 0 &&
        Util::CharsLen(history_segment.candidate(0).key) >= kMinBigramKeyLen) {
      result |= BIGRAM;
    }
  }

  return static_cast<PredictionType>(result);
}
}  // namespace mozc
