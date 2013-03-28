// Copyright 2010-2013, Google Inc.
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

// This class is used to filter out generated candidate by its
// cost, structure and relation with previously added candidates.

#include "converter/candidate_filter.h"

#include <algorithm>
#include <climits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/util.h"
#include "converter/node.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "prediction/suggestion_filter.h"

namespace mozc {
namespace converter {
namespace {

const size_t kMaxCandidatesSize = 200;   // how many candidates we expand

// Currently, the cost (logprob) is calcurated as cost = -500 * log(prob).
// Suppose having two candidates A and B and prob(A) = C * prob(B), where
// C = 1000 (some constant variable). The word "A" appears 1000 times more
// frequently than B.
// In this case,
// cost(B) - cost(A) = -500 * [log(prob(B)) - log (C * prob(B)) ]
//                   = -500 * [-log(C) + log(prob(B)) - log(prob(B))]
//                   = 500 * log(C)
// This implies that it is more reasonable to filter candiates
// by an absolute difference of costs between cost(B) and cost(A).
//
// Here's "C" and cost-diff relation:
// C       cost diff: 500 * log(C)
// 10      1151.29
// 100     2302.58
// 1000    3453.87
// 10000   4605.17
// 100000  5756.46
// 1000000 6907.75
const int   kMinCost                 = 100;
const int   kCostOffset              = 6907;
const int   kStructureCostOffset     = 3453;
const int   kMinStructureCostOffset  = 1151;
const int32 kStopEnmerationCacheSize = 15;

}  // anonymous namespace

CandidateFilter::CandidateFilter(
    const SuppressionDictionary *suppression_dictionary,
    const POSMatcher *pos_matcher,
    const SuggestionFilter *suggestion_filter)
    : suppression_dictionary_(suppression_dictionary),
      pos_matcher_(pos_matcher),
      suggestion_filter_(suggestion_filter),
      top_candidate_(NULL) {
  CHECK(suppression_dictionary_);
  CHECK(pos_matcher_);
  CHECK(suggestion_filter_);
}

CandidateFilter::~CandidateFilter() {}

void CandidateFilter::Reset() {
  seen_.clear();
  top_candidate_ = NULL;
}

CandidateFilter::ResultType CandidateFilter::FilterCandidateInternal(
    const string &original_key,
    const Segment::Candidate *candidate,
    const vector<const Node *> &nodes,
    Segments::RequestType request_type) {
  DCHECK(candidate);

  // Filtering by the suggestion filter, which is applied only for the
  // PREDICTION and SUGGESTION modes.
  switch (request_type) {
    case Segments::PREDICTION:
      // In the PREDICTION mode, the suggestion filter is not applied and the
      // same filtering rule as the CONVERSION mode is used because the
      // PREDICTION is triggered by user action (hitting tab keys), i.e.,
      // prediction candidates are not automatically shown to users. On the
      // contrary, since a user hit tab keys to run prediction, even unfavorable
      // words might be what the user wants to type.  Therefore, filtering rule
      // is relaxed for the PREDICTION mode: we don't apply the suggestion
      // filter if the user input key is exactly the same as candidate's.
      if (original_key == candidate->key) {
        break;
      }
      FALLTHROUGH_INTENDED;
    case Segments::SUGGESTION:
      // In contrast to the PREDICTION mode, the SUGGESTION is triggered without
      // any user actions, i.e., suggestion candidates are automatically
      // displayed to users.  Therefore, it's better to filter unfavorable words
      // in this mode.
      CHECK(suggestion_filter_);
      if (suggestion_filter_->IsBadSuggestion(candidate->value)) {
        return BAD_CANDIDATE;
      }
      // TODO(noriyukit): In the implementation below, the possibility remains
      // that multiple nodes constitute bad candidates. For stronger filtering,
      // we may want to check all the possibilities.
      for (size_t i = 0; i < nodes.size(); ++i) {
        if (suggestion_filter_->IsBadSuggestion(nodes[i]->value)) {
          return BAD_CANDIDATE;
        }
      }
      break;
    default:
      break;
  }

  // In general, the cost of constrained node tends to be overestimated.
  // If the top candidate has constrained node, we skip the main body
  // of CandidateFilter, meaning that the node is not treated as the top
  // node for CandidateFilter.
  if (candidate->attributes & Segment::Candidate::CONTEXT_SENSITIVE) {
    return CandidateFilter::GOOD_CANDIDATE;
  }

  const size_t candidate_size = seen_.size();
  if (top_candidate_ == NULL || candidate_size == 0) {
    top_candidate_ = candidate;
  }

  CHECK(top_candidate_);

  // Don't remove duplications if USER_DICTIONARY.
  if (candidate->attributes & Segment::Candidate::USER_DICTIONARY) {
    return CandidateFilter::GOOD_CANDIDATE;
  }

  // too many candidates size
  if (candidate_size + 1 >= kMaxCandidatesSize) {
    return CandidateFilter::STOP_ENUMERATION;
  }

  // The candidate is already seen.
  if (seen_.find(candidate->value) != seen_.end()) {
    return CandidateFilter::BAD_CANDIDATE;
  }

  CHECK(!nodes.empty());

  // "短縮よみ" must only have 1 node.
  if (pos_matcher_->IsIsolatedWord(nodes[0]->lid) &&
      (nodes.size() > 1 ||
       nodes[0]->prev == NULL ||
       nodes[0]->prev->node_type == Node::NOR_NODE ||
       nodes[0]->prev->node_type == Node::CON_NODE ||
       nodes[0]->next == NULL ||
       nodes[0]->next->node_type == Node::NOR_NODE ||
       nodes[0]->next->node_type == Node::CON_NODE)) {
    return CandidateFilter::BAD_CANDIDATE;
  }

  // Remove "抑制単語" just in case.
  if (suppression_dictionary_->SuppressEntry(candidate->key,
                                             candidate->value) ||
      (candidate->key != candidate->content_key &&
       candidate->value != candidate->content_value &&
       suppression_dictionary_->SuppressEntry(candidate->content_key,
                                              candidate->content_value))) {
    return CandidateFilter::BAD_CANDIDATE;
  }

  // The candidate consists of only one token
  if (nodes.size() == 1) {
    VLOG(1) << "don't filter single segment";
    return CandidateFilter::GOOD_CANDIDATE;
  }

  // don't drop single character
  if (Util::CharsLen(candidate->value) == 1) {
    VLOG(1) << "don't filter single character";
    return CandidateFilter::GOOD_CANDIDATE;
  }

  // don't drop lid/rid are the same as those
  // of top candidate.
  // http://b/issue?id=4285213
  if (top_candidate_->structure_cost == 0 &&
      candidate->lid == top_candidate_->lid &&
      candidate->rid == top_candidate_->rid) {
    VLOG(1) << "don't filter lid/rid are the same";
    return CandidateFilter::GOOD_CANDIDATE;
  }

  // "好かっ|たり" vs  "良かっ|たり" have same non_content_value.
  // "良かっ|たり" is also a good candidate but it is not the top candidate.
  if (top_candidate_ != candidate &&
      top_candidate_->content_value != top_candidate_->value &&
      (top_candidate_->value.compare(
          top_candidate_->content_value.size(),
          top_candidate_->value.size() - top_candidate_->content_value.size(),
          candidate->value,
          candidate->content_value.size(),
          candidate->value.size() - candidate->content_value.size()) == 0)) {
    VLOG(1) << "don't filter if non-content value are the same";
    return CandidateFilter::GOOD_CANDIDATE;
  }

  // Check Katakana transliterations
  // Skip this check when the conversion mode is real-time;
  // otherwise this ruins the whole sentence
  // that starts with alphabets.
  if (!(candidate->attributes & Segment::Candidate::REALTIME_CONVERSION)) {
    const bool is_top_english_t13n =
        Util::IsEnglishTransliteration(nodes[0]->value);
    for (size_t i = 1; i < nodes.size(); ++i) {
      // EnglishT13N must be the prefix of the candidate.
      if (Util::IsEnglishTransliteration(nodes[i]->value)) {
        return CandidateFilter::BAD_CANDIDATE;
      }
      // nodes[1..] are non-functional candidates.
      // In other words, the node just after KatakanaT13n candidate should
      // be a functional word.
      if (is_top_english_t13n && !pos_matcher_->IsFunctional(nodes[i]->lid)) {
        return CandidateFilter::BAD_CANDIDATE;
      }
    }
  }

  const int top_cost = max(kMinCost, top_candidate_->cost);
  const int top_structure_cost = max(kMinCost, top_candidate_->structure_cost);

  // If candidate size < 3, don't filter candidate aggressively
  // TOOD(taku): This is a tentative workaround for the case where
  // TOP candidate is compound and the structure cost for it is "0"
  // If 2nd or 3rd candidates are regular candidate but not having
  // non-zero cost, they might be removed. This hack removes such case.
  if (candidate_size < 3 &&
      candidate->cost < top_cost + 2302 &&
      candidate->structure_cost < 6907) {
     return CandidateFilter::GOOD_CANDIDATE;
  }

  // if candidate starts with prefix "お", we'd like to demote
  // the candidate if the rank of the candidate is below 1 (0-origin).
  // This is a temporal workaround for fixing "おそう" => "御|総"
  // TODO(taku): remove it after intorducing a word clustering for noun.
  if (candidate_size >= 1 && nodes.size() > 1 &&
      nodes[0]->lid == nodes[0]->rid &&
      pos_matcher_->IsWeakCompoundPrefix(nodes[0]->lid)) {
    VLOG(2) << "removing noisy prefix pattern";
    return CandidateFilter::BAD_CANDIDATE;
  }

  // don't drop personal names aggressivly.
  // We have to show personal names even if they are minor enough.
  // We basically ignores the cost threadshould. Filter candidate
  // only with StructureCost
  int cost_offset = kCostOffset;
  if (candidate->lid == pos_matcher_->GetLastNameId() ||
      candidate->lid == pos_matcher_->GetFirstNameId()) {
    cost_offset = INT_MAX;
  }

  // Filters out candidates with higher cost.
  if (top_cost + cost_offset < candidate->cost &&
      top_structure_cost + kMinStructureCostOffset
      < candidate->structure_cost) {
    // Stops candidates enumeration when we see sufficiently high cost
    // candidate.
    VLOG(2) << "cost is invalid: "
            << "top_cost=" << top_cost
            << " cost_offset=" << cost_offset
            << " value=" << candidate->value
            << " cost=" << candidate->cost
            << " top_structure_cost=" << top_structure_cost
            << " structure_cost=" << candidate->structure_cost
            << " lid=" << candidate->lid
            << " rid=" << candidate->rid;
    if (candidate_size < kStopEnmerationCacheSize) {
      // Even when the current candidate is classified as bad candidate,
      // we don't return STOP_ENUMERATION here.
      // When the current candidate is removed only with the "structure_cost",
      // there might exist valid candidates just after the current candidate.
      // We don't want to miss them.
      return CandidateFilter::BAD_CANDIDATE;
    } else {
      return CandidateFilter::STOP_ENUMERATION;
    }
  }

  // Filters out candidates with higher cost structure.
  if (max(top_structure_cost, kMinStructureCostOffset) + kStructureCostOffset <
      candidate->structure_cost) {
    // We don't stop enumeration here. Just drops high cost structure
    // looks enough.
    // |top_structure_cost| can be so small especially for compound or
    // web dictionary entries.
    // For avoiding over filtering, we use kMinStructureCostOffset if
    // |top_structure_cost| is small.
    VLOG(2) << "structure cost is invalid:  "
              << candidate->value << " " << candidate->content_value << " "
              << candidate->structure_cost
            << " " << candidate->cost;
    return CandidateFilter::BAD_CANDIDATE;
  }

  return CandidateFilter::GOOD_CANDIDATE;
}

CandidateFilter::ResultType CandidateFilter::FilterCandidate(
    const string &original_key,
    const Segment::Candidate *candidate,
    const vector<const Node *> &nodes,
    Segments::RequestType request_type) {
  const ResultType result =
      FilterCandidateInternal(original_key, candidate, nodes, request_type);
  if (result != GOOD_CANDIDATE) {
    return result;
  }
  seen_.insert(candidate->value);
  return result;
}

}  // namespace converter
}  // namespace mozc
