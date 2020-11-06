// Copyright 2010-2020, Google Inc.
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

#include <set>
#include <string>
#include <vector>

#include "base/port.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"

namespace mozc {

struct Node;
class SuggestionFilter;

namespace converter {

class CandidateFilter {
 public:
  CandidateFilter(
      const dictionary::SuppressionDictionary *suppression_dictionary,
      const dictionary::POSMatcher *pos_matcher,
      const SuggestionFilter *suggestion_filter,
      bool apply_suggestion_filter_for_exact_match);
  ~CandidateFilter();

  enum ResultType {
    GOOD_CANDIDATE,    // Can insert the candidate into the list
    BAD_CANDIDATE,     // Do not insert the candidate
    STOP_ENUMERATION,  // Do not insert and stop enumurations
  };

  // Checks if the candidate should be filtered out.
  ResultType FilterCandidate(const std::string &original_key,
                             const Segment::Candidate *candidate,
                             const std::vector<const Node *> &nodes,
                             Segments::RequestType request_type);

  // Resets the internal state.
  void Reset();

 private:
  ResultType FilterCandidateInternal(const std::string &original_key,
                                     const Segment::Candidate *candidate,
                                     const std::vector<const Node *> &nodes,
                                     Segments::RequestType request_type);

  const dictionary::SuppressionDictionary *suppression_dictionary_;
  const dictionary::POSMatcher *pos_matcher_;
  const SuggestionFilter *suggestion_filter_;

  std::set<std::string> seen_;
  const Segment::Candidate *top_candidate_;
  bool apply_suggestion_filter_for_exact_match_;

  DISALLOW_COPY_AND_ASSIGN(CandidateFilter);
};

}  // namespace converter
}  // namespace mozc

#endif  // MOZC_CONVERTER_CANDIDATE_FILTER_H_
