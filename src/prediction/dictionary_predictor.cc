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
#include "converter/converter_interface.h"
#include "converter/node.h"
#include "converter/segments.h"
#include "dictionary/dictionary_interface.h"
#include "prediction/svm_model.h"
#include "prediction/suggestion_filter.h"
#include "prediction/predictor_interface.h"
#include "session/config_handler.h"
#include "session/config.pb.h"

namespace mozc {

namespace {
// define kLidGroup[]
#include "prediction/suggestion_feature_pos_group.h"

class Result {
 public:
  Result() : node_(NULL), score_(0), is_bigram_(false) {}
  Result(const Node *node, int score, bool is_bigram)
      : node_(node), score_(score), is_bigram_(is_bigram) {}
  ~Result() {}

  const Node *node() const {
    return node_;
  }

  void set_score(int score) {
    score_ = score;
  }

  int score() const {
    return score_;
  }

  bool is_bigram() const {
    return is_bigram_;
  }

 private:
  const Node *node_;
  int score_;
  bool is_bigram_;
};

class ResultCompare {
 public:
  bool operator() (const Result &a, const Result &b) const {
    return a.score() < b.score();
  }
};
}  // namespace

DictionaryPredictor::DictionaryPredictor()
    : dictionary_(DictionaryFactory::GetDictionary()) {}

DictionaryPredictor::~DictionaryPredictor() {}

bool DictionaryPredictor::Predict(Segments *segments) const {
  if (segments == NULL) {
    return false;
  }

  const PredictionType prediction_type = GetPredictionType(*segments);
  if (prediction_type == NO_PREDICTION) {
    return false;
  }

  // TODO(taku): 256 are just heuristics.
  size_t max_nodes_size = 256;

  // set max_nodes_size to be 100000.
  // Note that PREDICTION mode is much slower than SUGGESTION.
  // Number of prediction calls should be minimized.
  if (segments->request_type() == Segments::PREDICTION) {
    max_nodes_size = 100000;
  }

  if (max_nodes_size == 0) {  // should never happen
    LOG(WARNING) << "max_nodes_size is 0";
    return false;
  }

  NodeAllocatorInterface *allocator = segments->node_allocator();
  allocator->set_max_nodes_size(max_nodes_size);

  const string &key = segments->conversion_segment(0).key();

  vector<Result> results;
  if (prediction_type & UNIGRAM) {
    const size_t prev_results_size = results.size();
    const Node *unigram_node = dictionary_->LookupPredictive(key.c_str(),
                                                             key.size(),
                                                             allocator);
    size_t unigram_results_size = 0;
    for (; unigram_node != NULL; unigram_node = unigram_node->bnext) {
      results.push_back(Result(unigram_node, 0, false));
      ++unigram_results_size;
    }

    // if size reaches max_nodes_size.
    // we don't show the candidates, since disambiguation from
    // 256 candidates is hard. (It may exceed max_nodes_size, because this is
    // just a limit for each backend, so total number may be larger)
    if (unigram_results_size >= max_nodes_size) {
      results.resize(prev_results_size);
    }
  }

  string bigram_key, bigram_prefix_key, bigram_prefix_value;
  if (prediction_type & BIGRAM) {
    const Segment &history_segment =
        segments->history_segment(segments->history_segments_size() - 1);
    bigram_prefix_key = history_segment.candidate(0).key;
    bigram_prefix_value = history_segment.candidate(0).value;
    bigram_key = bigram_prefix_key + key;
    const Node *bigram_node = dictionary_->LookupPredictive(bigram_key.c_str(),
                                                            bigram_key.size(),
                                                            allocator);
    const size_t prev_results_size = results.size();
    size_t bigram_results_size  = 0;
    for (; bigram_node != NULL; bigram_node = bigram_node->bnext) {
      // filter out the output (value)'s prefix doesn't match to
      // the history value.
      if (Util::StartsWith(bigram_node->value, bigram_prefix_value)) {
        results.push_back(Result(bigram_node, 0, true));
        ++bigram_results_size;
      }
    }

    // if size reaches max_nodes_size.
    // we don't show the candidates, since disambiguation from
    // 256 candidates is hard. (It may exceed max_nodes_size, because this is
    // just a limit for each backend, so total number may be larger)
    if (bigram_results_size >= max_nodes_size) {
      results.resize(prev_results_size);
    }
  }

  if (results.empty()) {
    VLOG(2) << "|result| is empty";
    return false;
  }

  // ranking
  const bool is_zip_code = DictionaryPredictor::IsZipCodeRequest(key);
  const bool is_suggestion = (segments->request_type() ==
                              Segments::SUGGESTION);

  vector<pair<int, double> > feature;
  for (size_t i = 0; i < results.size(); ++i) {
    // use the same scoring function for both unigram/bigram.
    // Bigram will be boosted because we pass the previous
    // key as a context information.
    const Node *node = results[i].node();
    results[i].set_score(
        GetSVMScore(
            results[i].is_bigram() ? bigram_key : key,
            node->key, node->value, node->wcost, node->lid,
            is_zip_code, is_suggestion, results.size(), &feature));
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
    const Node *node = results[i].node();
    if (!node->is_spelling_correction) {
      non_spelling_correction_values.insert(node->value);
    }
  }

  // Instead of sorting all the results, we construct a heap.
  // This is done in linear time and
  // we can pop as many results as we need effectively.
  make_heap(results.begin(), results.end(), ResultCompare());

  Segment *segment = segments->mutable_conversion_segment(0);
  if (segment == NULL) {
    LOG(ERROR) << "conversion segment is NULL";
    return false;
  }

  int added = 0;
  const size_t key_length = Util::CharsLen(key);
  for (size_t i = 0; i < results.size(); ++i) {
    if (added >= size || results[i].score() == INT_MIN) {
      break;
    }

    pop_heap(results.begin(), results.end() - i, ResultCompare());
    const Result &result = results[results.size() - i - 1];
    const Node *node = result.node();

    string value = node->value;
    Util::LowerString(&value);
    if (SuggestionFilter::IsBadSuggestion(value)) {
      continue;
    }

    // don't suggest exactly the same candidate as key
    if ((result.is_bigram() && bigram_key == node->value) ||
        (!result.is_bigram() && key == node->value)) {
      continue;
    }

    // The below conditions should never happen
    if (result.is_bigram() &&
        (node->key.size() <= bigram_prefix_key.size() ||
         (node->value.size() <= bigram_prefix_value.size()))) {
      LOG(ERROR) << "Invalid bigram key/value";
      continue;
    }

    // filter bad spelling correction
    if (node->is_spelling_correction) {
      if (non_spelling_correction_values.count(node->value)) {
        continue;
      }
      // TODO(takiba): 0.75 is just heurestics.
      if (key_length < Util::CharsLen(node->key) * 0.75) {
        continue;
      }
    }

    Segment::Candidate *candidate = segment->push_back_candidate();
    if (candidate == NULL) {
      LOG(ERROR) << "candidate is NULL";
      break;
    }

    candidate->Init();
    VLOG(2) << "DictionarySuggest: " << node->wcost << " "
            << node->value;

    // TODO(taku): call CharacterFormManager for these values
    if (result.is_bigram()) {
      // remove the prefix of history key and history value.
      candidate->key =
          node->key.substr(bigram_prefix_key.size(),
                           node->key.size() - bigram_prefix_key.size());
      candidate->value =
          node->value.substr(bigram_prefix_value.size(),
                             node->value.size() - bigram_prefix_value.size());
    } else {
      candidate->key = node->key;
      candidate->value = node->value;
    }

    candidate->content_key = candidate->key;
    candidate->content_value = candidate->value;
    candidate->lid = node->lid;
    candidate->rid = node->rid;
    candidate->cost = node->wcost;
    candidate->is_spelling_correction = node->is_spelling_correction;

    // Don't provide any descriptions for dictionary suggests
#ifdef _DEBUG
    const char kDescription[] = "Dictionary Suggest";
#else
    const char kDescription[] = "";
#endif

    candidate->SetDescription(Segment::Candidate::PLATFORM_DEPENDENT_CHARACTER |
                              Segment::Candidate::ZIPCODE,
                              kDescription);
    ++added;
  }

  return added > 0;
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
}

#define ADD_FEATURE(i, v) \
   { feature->push_back(make_pair<int, double>(i, v)); } while (0)

int DictionaryPredictor::GetSVMScore(const string &query,
                                     const string &key,
                                     const string &value,
                                     uint16 cost,
                                     uint16 lid,
                                     bool is_zip_code,
                                     bool is_suggestion,
                                     size_t total_candidates_size,
                                     vector<pair<int, double> > *feature) {
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

bool DictionaryPredictor::IsZipCodeRequest(const string &key) {
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
DictionaryPredictor::GetPredictionType(const Segments &segments) {
  if (!GET_CONFIG(use_dictionary_suggest) &&
      segments.request_type() == Segments::SUGGESTION) {
    VLOG(2) << "no_dictionary_suggest";
    return NO_PREDICTION;
  }

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
  const size_t key_len = Util::CharsLen(key);
  if (key_len == 0) {
    return NO_PREDICTION;
  }

  const bool is_zip_code = DictionaryPredictor::IsZipCodeRequest(key);

  // Never trigger prediction if key looks like zip code.
  if (segments.request_type() == Segments::SUGGESTION &&
      is_zip_code && key_len < 6) {
    return NO_PREDICTION;
  }

  int result = NO_PREDICTION;

  // unigram based suggestion requires key_len >= 3.
  // Providing suggestions from very short user input key is annoying.
  if (segments.request_type() != Segments::SUGGESTION || key_len >= 3) {
    result |= UNIGRAM;
  }

  const size_t history_segments_size = segments.history_segments_size();
  if (history_segments_size > 0) {
    const Segment &history_segment =
        segments.history_segment(history_segments_size - 1);
    // even in PREDICTION mode, bigram-based suggestion requires that
    // the length of previous key is >= 3.
    // It also implies that bigram-based suggestion will be triggered,
    // even if the current key length is short enough.
    // TOOD(taku): this setting might be aggressive if the current key
    // looks like Japanese particle like "が|で|は"
    // If the current key looks like particle, we can make the behavior
    // less aggressive.
    if (history_segment.candidates_size() > 0 &&
        Util::CharsLen(history_segment.candidate(0).key) >= 3) {
      result |= BIGRAM;
    }
  }

  return static_cast<PredictionType>(result);
}
}  // namespace mozc
