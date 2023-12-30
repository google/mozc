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
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "base/util.h"
#include "converter/segments.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

namespace {

class CandidateGroup {
 public:
  CandidateGroup() = default;
  explicit CandidateGroup(Segment::Candidate::Category category)
      : category_(category) {}
  ~CandidateGroup() = default;

  const std::deque<Segment::Candidate> &candidates() const {
    return candidates_;
  }

  void AppendToSegment(Segment &segment) const {
    for (const Segment::Candidate &c : candidates_) {
      Segment::Candidate *candidate = segment.add_candidate();
      *candidate = c;
    }
  }

  std::deque<Segment::Candidate> *mutable_candidates() { return &candidates_; }
  size_t size() const { return candidates_.size(); }

  void AddCandidate(const Segment::Candidate &candidate) {
    if (const auto [_, inserted] =
            added_.emplace(candidate.key, candidate.value);
        inserted) {
      candidates_.push_back(candidate);
      if (category_.has_value()) {
        candidates_.back().category = *category_;
      }
    }
  }

  void AddHiragnaCandidates() {
    // (Key, Base candidate)
    absl::flat_hash_map<std::string, const Segment::Candidate *> keys;
    for (const Segment::Candidate &c : candidates_) {
      keys.insert({c.key, &c});
    }
    for (const auto &itr : keys) {
      Segment::Candidate c = *itr.second;
      c.value = c.key;
      c.content_key = c.key;
      c.content_value = c.key;
      c.description.clear();
      c.inner_segment_boundary.clear();

      candidates_.push_front(std::move(c));
    }
  }

  void SortCandidates() {
    // key length -> value length
    const auto cmp = [](const Segment::Candidate &lhs,
                        const Segment::Candidate &rhs) {
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
  const std::optional<Segment::Candidate::Category> category_ = std::nullopt;
  std::deque<Segment::Candidate> candidates_;
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
  if (!request.request()
           .decoder_experiment_params()
           .enable_findability_oriented_order()) {
    return false;
  }
  if (segments->conversion_segments_size() != 1) {
    return false;
  }

  Segment *segment = segments->mutable_conversion_segment(0);

  // Candidates in the same category will be deduped.
  CandidateGroup top, normal, partial, t13n;
  CandidateGroup single_kanji(Segment::Candidate::OTHER),
      single_kanji_partial(Segment::Candidate::OTHER),
      symbol(Segment::Candidate::OTHER), other(Segment::Candidate::OTHER);

  const int top_candidates_size = request.request()
                                      .decoder_experiment_params()
                                      .findability_oriented_order_top_size();

  for (size_t i = 0; i < segment->candidates_size(); ++i) {
    const Segment::Candidate &candidate = segment->candidate(i);
    if (top.size() < top_candidates_size &&
        candidate.category != Segment::Candidate::OTHER) {
      top.AddCandidate(candidate);
      continue;
    }
    if (candidate.category == Segment::Candidate::SYMBOL) {
      symbol.AddCandidate(candidate);
    } else if (candidate.category == Segment::Candidate::OTHER) {
      other.AddCandidate(candidate);
    } else if (candidate.category == Segment::Candidate::DEFAULT_CATEGORY) {
      // TODO(toshiyuki): Use better way to check single kanji entries.
      // - There are single characters with multiple code points
      // (e.g. SVS, IVS). Grapheme treats those multiple code points as a single
      // character, but CharsLen() treats them as multiple characters.
      // - Or, we may be able to set candidate category when we generate single
      // kanji candidates.
      const bool is_single_kanji =
          Util::CharsLen(candidate.value) == 1 &&
          Util::IsScriptType(candidate.value, Util::KANJI);
      const bool is_partial =
          candidate.attributes & Segment::Candidate::PARTIALLY_KEY_CONSUMED;
      const bool is_t13n = Util::IsScriptType(candidate.key, Util::HIRAGANA) &&
                           Util::IsScriptType(candidate.value, Util::ALPHABET);
      if (is_partial && is_single_kanji) {
        single_kanji_partial.AddCandidate(candidate);
      } else if (is_partial) {
        partial.AddCandidate(candidate);
      } else if (is_single_kanji) {
        single_kanji.AddCandidate(candidate);
      } else if (is_t13n) {
        t13n.AddCandidate(candidate);
      } else {
        normal.AddCandidate(candidate);
      }
    }
  }
  for (size_t i = 0; i < segment->meta_candidates_size(); ++i) {
    const Segment::Candidate &c = segment->meta_candidate(i);
    t13n.AddCandidate(c);
  }

  single_kanji.AddHiragnaCandidates();
  single_kanji_partial.AddHiragnaCandidates();

  // The following candidates are originally sorted in LM based order.
  // Reorder these candidates based on the length of key and value so that
  // users can find the expected candidate easily.
  normal.SortCandidates();
  partial.SortCandidates();
  single_kanji.SortCandidates();
  single_kanji_partial.SortCandidates();

  segment->clear_candidates();
  segment->clear_meta_candidates();

  top.AppendToSegment(*segment);
  normal.AppendToSegment(*segment);
  t13n.AppendToSegment(*segment);
  other.AppendToSegment(*segment);
  single_kanji.AppendToSegment(*segment);
  symbol.AppendToSegment(*segment);
  partial.AppendToSegment(*segment);
  single_kanji_partial.AppendToSegment(*segment);

  return true;
}

}  // namespace mozc
