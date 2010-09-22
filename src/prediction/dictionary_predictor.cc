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

#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include "base/base.h"
#include "base/init.h"
#include "base/util.h"
#include "converter/node.h"
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
}

DictionaryPredictor::DictionaryPredictor()
    : dictionary_(DictionaryFactory::GetDictionary()) {}

DictionaryPredictor::~DictionaryPredictor() {}

bool DictionaryPredictor::Predict(Segments *segments) const {
  if (!GET_CONFIG(use_dictionary_suggest) &&
      segments->request_type() == Segments::SUGGESTION) {
    VLOG(2) << "no_dictionary_suggest";
    return false;
  }

  if (segments->request_type() == Segments::CONVERSION) {
    VLOG(2) << "request type is CONVERSION";
    return false;
  }

  if (segments->conversion_segments_size() < 1) {
    VLOG(2) << "segment size < 1";
    return false;
  }

  const string &key = segments->conversion_segment(0).key();
  const size_t key_len = Util::CharsLen(key);
  if (key_len == 0) {
    VLOG(2) << "key length is 0";
    return false;
  }

  if (segments->request_type() == Segments::SUGGESTION &&
      key_len < 3) {  // too short
    VLOG(2) << "key length is short";
    return false;
  }

  const bool is_zip_code = IsZipCodeRequest(key);
  if (segments->request_type() == Segments::SUGGESTION &&
      is_zip_code && key_len < 6) {
    VLOG(2) << "key looks like a zip code request";
    return false;
  }

  Segment *segment = segments->mutable_conversion_segment(0);
  if (segment == NULL) {
    LOG(ERROR) << "conversion segment is NULL";
    return false;
  }

  NodeAllocatorInterface *allocator = segments->node_allocator();
  if (allocator == NULL) {
    LOG(WARNING) << "NodeAllocator is NULL";
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

  allocator->set_max_nodes_size(max_nodes_size);
  const Node *node = dictionary_->LookupPredictive(key.c_str(),
                                                   key.size(),
                                                   allocator);
  if (node == NULL) {
    return false;
  }

  vector<pair<int, const Node *> > results;
  results.reserve(max_nodes_size);
  for (; node != NULL; node = node->bnext) {
    results.push_back(make_pair(0, node));
  }

  // if size reaches max_nodes_size.
  // we don't show the candidates, since disambiguation from
  // 256 candidates is hard. (It may exceed max_nodes_size, because this is
  // just a limit for each backend, so total number may be larger)
  if (results.size() >= max_nodes_size) {
    return false;
  }

  // ranking
  vector<pair<int, double> > feature;
  for (size_t i = 0; i < results.size(); ++i) {
    const Node *node = results[i].second;
    MakeFeature(key,
                node->key, node->value, node->wcost, node->lid,
                is_zip_code,
                &feature);
    results[i].first = static_cast<int>(-1000 * SVMClassify(feature));
  }

  const size_t size = min(segments->max_prediction_candidates_size(),
                          results.size());

  partial_sort(results.begin(), results.begin() + size,
               results.end());

  bool added = false;
  for (size_t i = 0; i < size; ++i) {
    const Node *node = results[i].second;
    string value = node->value;
    Util::LowerString(&value);
    if (SuggestionFilter::IsBadSuggestion(value)) {
      continue;
    }

    // don't suggest exactly the same candidate as key
    if (key == node->value) {
      continue;
    }

    Segment::Candidate *candidate = segment->push_back_candidate();
    if (candidate == NULL) {
      LOG(ERROR) << "candidate is NULL";
      break;
    }

    candidate->Init();
    VLOG(2) << "DictionarySuggest: " << node->wcost << " "
            << node->value;
    candidate->content_key = node->key;
    // TODO(taku): call CharacterFormManager for these values
    candidate->value = node->value;
    candidate->content_value = node->value;
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
    added = true;
  }

  return added;
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

void DictionaryPredictor::MakeFeature(const string &query,
                                      const string &key,
                                      const string &value,
                                      uint16 cost,
                                      uint16 lid,
                                      bool is_zip_code,
                                      vector<pair<int, double> > *feature) {
  feature->clear();

  // We use the cost for ranking if query looks like zip code
  // TODO(taku): find out better and more sophisicated way instead of this
  // huristics.
  if (is_zip_code) {
    ADD_FEATURE(COST, cost / 500.0);
    return;
  }

  const size_t query_len = Util::CharsLen(query);
  const size_t key_len = Util::CharsLen(key);
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
}  // namespace mozc
