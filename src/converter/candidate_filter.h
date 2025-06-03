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

#ifndef MOZC_CONVERTER_CANDIDATE_FILTER_H_
#define MOZC_CONVERTER_CANDIDATE_FILTER_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "converter/candidate.h"
#include "converter/node.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_matcher.h"
#include "prediction/suggestion_filter.h"
#include "request/conversion_request.h"

namespace mozc {
namespace converter {

namespace candidate_filter_internal {
// ID of the candidate for filtering.
struct CandidateId {
  explicit CandidateId(const converter::Candidate &candidate)
      : value(candidate.value), lid(candidate.lid), rid(candidate.rid) {}

  template <typename H>
  friend H AbslHashValue(H h, const CandidateId &c) {
    return H::combine(std::move(h), c.value, c.lid, c.rid);
  }

  friend bool operator==(const CandidateId &lhs, const CandidateId &rhs) {
    // Comparing the int values first for performance.
    return lhs.lid == rhs.lid && lhs.rid == rhs.rid && lhs.value == rhs.value;
  }
  friend bool operator==(const CandidateId &lhs,
                         const converter::Candidate &rhs) {
    return lhs.lid == rhs.lid && lhs.rid == rhs.rid && lhs.value == rhs.value;
  }

  std::string value;
  uint16_t lid;
  uint16_t rid;
};

struct CandidateHasher {
  using is_transparent = void;

  size_t operator()(const CandidateId &c) const { return absl::HashOf(c); }
  size_t operator()(const converter::Candidate &c) const {
    return absl::HashOf(c.value, c.lid, c.rid);
  }
};

}  // namespace candidate_filter_internal

class CandidateFilter {
 public:
  CandidateFilter(const dictionary::UserDictionaryInterface &user_dictionary,
                  const dictionary::PosMatcher &pos_matcher,
                  const SuggestionFilter &suggestion_filter);
  CandidateFilter(const CandidateFilter &) = delete;
  CandidateFilter &operator=(const CandidateFilter &) = delete;

  enum ResultType {
    GOOD_CANDIDATE,    // Can insert the candidate into the list
    BAD_CANDIDATE,     // Do not insert the candidate
    STOP_ENUMERATION,  // Do not insert and stop enumurations
  };

  // Checks if the candidate should be filtered out.
  //
  // top_nodes: Node vector for the top candidate for the segment.
  // nodes: Node vector for the target candidate
  ResultType FilterCandidate(const ConversionRequest &request,
                             absl::string_view original_key,
                             const converter::Candidate *candidate,
                             absl::Span<const Node *const> top_nodes,
                             absl::Span<const Node *const> nodes);

  // Resets the internal state.
  void Reset();

 private:
  ResultType CheckRequestType(const ConversionRequest &request,
                              absl::string_view original_key,
                              const converter::Candidate &candidate,
                              absl::Span<const Node *const> nodes) const;
  ResultType FilterCandidateInternal(const ConversionRequest &request,
                                     absl::string_view original_key,
                                     const converter::Candidate *candidate,
                                     absl::Span<const Node *const> top_nodes,
                                     absl::Span<const Node *const> nodes);

  const dictionary::UserDictionaryInterface &user_dictionary_;
  const dictionary::PosMatcher &pos_matcher_;
  const SuggestionFilter &suggestion_filter_;

  absl::flat_hash_set<candidate_filter_internal::CandidateId,
                      candidate_filter_internal::CandidateHasher,
                      std::equal_to<>>
      seen_;
  const converter::Candidate *top_candidate_;
};

}  // namespace converter
}  // namespace mozc

#endif  // MOZC_CONVERTER_CANDIDATE_FILTER_H_
