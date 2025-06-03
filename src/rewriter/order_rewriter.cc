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

#include "rewriter/order_rewriter.h"

#include <cstddef>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "base/util.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "request/conversion_request.h"
#include "request/request_util.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

namespace {

class CandidateGroup {
 public:
  CandidateGroup() = default;
  ~CandidateGroup() = default;

  const std::deque<converter::Candidate> &candidates() const {
    return candidates_;
  }

  void AppendToSegment(Segment &segment) const {
    for (const converter::Candidate &c : candidates_) {
      converter::Candidate *candidate = segment.add_candidate();
      *candidate = c;
    }
  }

  std::deque<converter::Candidate> *mutable_candidates() {
    return &candidates_;
  }
  size_t size() const { return candidates_.size(); }

  void AddCandidate(const converter::Candidate &candidate) {
    if (const auto [_, inserted] =
            added_.emplace(candidate.key, candidate.value);
        inserted) {
      candidates_.push_back(candidate);
    }
  }

  void AddHiraganaCandidates() {
    // (Key, Base candidate)
    absl::flat_hash_map<std::string, const converter::Candidate *> keys;
    for (const converter::Candidate &c : candidates_) {
      keys.insert({c.key, &c});
    }
    for (const auto &[key, candidate] : keys) {
      if (added_.contains({key, key})) {
        // Hiragana candidate is already included.
        continue;
      }
      converter::Candidate c = *candidate;
      c.value = c.key;
      c.content_key = c.key;
      c.content_value = c.key;
      c.description.clear();
      c.inner_segment_boundary.clear();

      candidates_.push_front(std::move(c));
    }
  }

  void SortWithKeyLength() {
    const auto cmp = [](const converter::Candidate &lhs,
                        const converter::Candidate &rhs) {
      return lhs.key.size() > rhs.key.size();
    };
    absl::c_stable_sort(candidates_, cmp);
  }

  void SortWithKeyValueLength() {
    // key length -> value length
    const auto cmp = [](const converter::Candidate &lhs,
                        const converter::Candidate &rhs) {
      if (lhs.key.size() != rhs.key.size()) {
        if (lhs.content_key.size() != rhs.content_key.size()) {
          return lhs.content_key.size() > rhs.content_key.size();
        }
        return lhs.key.size() > rhs.key.size();
      }
      if (lhs.key != rhs.key) {
        return lhs.key < rhs.key;
      }
      const size_t lhs_len = Util::CharsLen(lhs.value);
      const size_t rhs_len = Util::CharsLen(rhs.value);
      return lhs_len > rhs_len;
    };
    absl::c_stable_sort(candidates_, cmp);
  }

 private:
  std::deque<converter::Candidate> candidates_;
  absl::flat_hash_set<std::pair<std::string, std::string>> added_;
};

}  // namespace

int OrderRewriter::capability(const ConversionRequest &request) const {
  if (request.request().mixed_conversion()) {  // For mobile
    return RewriterInterface::PREDICTION | RewriterInterface::SUGGESTION;
  } else {
    return RewriterInterface::NOT_AVAILABLE;
  }
}

bool OrderRewriter::Rewrite(const ConversionRequest &request,
                            Segments *segments) const {
  if (!request_util::IsFindabilityOrientedOrderEnabled(request)) {
    return false;
  }
  if (segments->conversion_segments_size() != 1) {
    return false;
  }
  // Do not change the order for zero query prediction.
  if (segments->conversion_segment(0).key().empty()) {
    return false;
  }

  const int top_candidates_size = request.request()
                                      .decoder_experiment_params()
                                      .findability_oriented_order_top_size();

  Segment *segment = segments->mutable_conversion_segment(0);

  CandidateGroup top;     // Top candidates to keep the current order.
  CandidateGroup normal;  // Converted words, etc
  CandidateGroup t13n;
  CandidateGroup other;  // OTHER category candidates from rewriters.
  CandidateGroup symbol;
  CandidateGroup partial;  // For prefix match.

  for (size_t i = 0; i < segment->meta_candidates_size(); ++i) {
    const converter::Candidate &c = segment->meta_candidate(i);
    t13n.AddCandidate(c);
  }

  for (size_t i = 0; i < segment->candidates_size(); ++i) {
    const converter::Candidate &candidate = segment->candidate(i);
    if (top.size() < top_candidates_size) {
      top.AddCandidate(candidate);
      continue;
    }
    if (candidate.category == converter::Candidate::SYMBOL) {
      symbol.AddCandidate(candidate);
    } else if (candidate.category == converter::Candidate::OTHER) {
      other.AddCandidate(candidate);
    } else {
      // DEFAULT_CATEGORY
      const bool is_partial =
          candidate.attributes & converter::Candidate::PARTIALLY_KEY_CONSUMED;
      if (is_partial) {
        partial.AddCandidate(candidate);
      } else {
        normal.AddCandidate(candidate);
      }
    }
  }

  partial.AddHiraganaCandidates();

  // The following candidates are originally sorted in LM based order.
  // Reorder these candidates based on the length of key and value so that
  // users can find the expected candidate easily.
  normal.SortWithKeyLength();
  partial.SortWithKeyValueLength();

  // Update segment
  segment->clear_candidates();
  segment->clear_meta_candidates();

  top.AppendToSegment(*segment);
  normal.AppendToSegment(*segment);
  t13n.AppendToSegment(*segment);
  other.AppendToSegment(*segment);
  symbol.AppendToSegment(*segment);
  partial.AppendToSegment(*segment);

  return true;
}

}  // namespace mozc
