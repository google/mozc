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

#ifndef MOZC_REWRITER_FOCUS_CANDIDATE_REWRITER_H_
#define MOZC_REWRITER_FOCUS_CANDIDATE_REWRITER_H_

#include <cstddef>
#include <cstdint>

#include "absl/strings/string_view.h"
#include "base/container/serialized_string_array.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

class DataManager;
struct CounterSuffixEntry;

class FocusCandidateRewriter : public RewriterInterface {
 public:
  explicit FocusCandidateRewriter(const DataManager &data_manager);
  FocusCandidateRewriter(const FocusCandidateRewriter &) = delete;
  FocusCandidateRewriter &operator=(const FocusCandidateRewriter &) = delete;
  ~FocusCandidateRewriter() override;

  // Changed the focus of "segment_index"-th segment to be "candidate_index".
  // The segments will be written according to pre-defined "actions".
  // Currently, FocusSegmentValue() finds bracket/parentheses matching, e.g.,
  // When user chooses "(" in some candidate, corresponding close bracket ")"
  // is automatically placed at the top.
  bool Focus(Segments *segments, size_t segment_index,
             int candidate_index) const override;

  bool Rewrite(const ConversionRequest &request,
               Segments *segments) const override {
    return false;
  }

 private:
  // Performs reranking of number candidates to make numbers consistent across
  // multiple segments.
  bool RerankNumberCandidates(Segments *segments, size_t segment_index,
                              int candidate_index) const;

  // Finds an index of candidate in |seg| that matches the given number script
  // type and suffix.  Returns -1 if there's no candidate matching the
  // condition.
  int FindMatchingCandidates(const Segment &seg, uint32_t ref_script_type,
                             absl::string_view ref_suffix) const;

  // Parses the value of a candidate into number and counter suffix.
  // Simultaneously checks the script type of number.  Here, number candidate is
  // defined to be the following pattern:
  //   * [数][助数詞][並立助詞]?  (e.g., 一階, 二回, ３階や, etc.)
  // Returns false if the value of candidate doesn't match the pattern.
  bool ParseNumberCandidate(const Segment::Candidate &cand,
                            absl::string_view *number,
                            absl::string_view *suffix,
                            uint32_t *script_type) const;

  SerializedStringArray suffix_array_;
  const dictionary::PosMatcher pos_matcher_;
};

}  // namespace mozc

#endif  // MOZC_REWRITER_FOCUS_CANDIDATE_REWRITER_H_
