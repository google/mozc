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

#ifndef MOZC_CONVERTER_CONVERTER_UTIL_H_
#define MOZC_CONVERTER_CONVERTER_UTIL_H_

#include <optional>
#include <vector>

#include "converter/candidate.h"
#include "converter/segments.h"
#include "prediction/result.h"
#include "request/conversion_request.h"

namespace mozc::converter {

// Converts the top candidate (candidate 0) of each segment in the given
// conversion segments to a single composite Result.
//
// Boundary handling:
// Aggregates the length information (key, value, content_key, content_value)
// of candidate(0) across all segments into `result.inner_segment_boundary`
// via InnerSegmentBoundaryBuilder.
//
// Returns std::nullopt if segments is empty or any segment has no candidates.
std::optional<prediction::Result> ConversionSegmentsToResult(
    Segments::const_range segments);

// Converts history segments to a single composite Result.
//
// Boundary handling:
// Aggregates the length information of candidate(0) from each HISTORY segment
// into `result.inner_segment_boundary` via InnerSegmentBoundaryBuilder, so that
// the history boundary information can be inspected via
// Result::inner_segments().
//
// Returns an empty Result if history_segments is empty.
prediction::Result HistorySegmentsToResult(
    Segments::const_range history_segments);
inline prediction::Result HistorySegmentsToResult(const Segments& segments) {
  return HistorySegmentsToResult(segments.history_segments());
}

// Converts Segments to learning results for Predictor::Finish().
// - Single conversion segment: Returns up to top 5 candidates as individual
//   Results via CandidateToResult.
// - Multiple conversion segments: Returns a single composite Result via
//   ConversionSegmentsToResult.
//
// Boundary handling:
// In the single-segment case, each candidate's inner_segment_boundary is
// preserved (or synthesized via CandidateToResult). In the multi-segment case,
// segment boundaries are merged into a composite inner_segment_boundary.
std::vector<prediction::Result> MakeLearningResultsFromSegments(
    const Segments& segments);

// Converts a single Candidate to a prediction::Result.
//
// Boundary handling:
// If candidate.inner_segment_boundary is already populated, it is copied
// as-is. If candidate.inner_segment_boundary is empty, a single-segment
// fallback boundary is automatically synthesized from the candidate's key,
// value, content_key, and content_value lengths.
prediction::Result CandidateToResult(const Candidate& candidate);

// Populates Candidate fields from a prediction::Result.
//
// Boundary handling:
// - Copies result.inner_segment_boundary directly to
//   candidate->inner_segment_boundary.
// - Automatically updates candidate->content_key and candidate->content_value
//   from result.inner_segments().GetMergedContentKeyAndValue().
//
// Note: This function only updates fields present in Result (key, value,
// content_key/value, cost, wcost, lid, rid, attributes, consumed_key_size,
// inner_segment_boundary, description, display_value). Pre-existing Candidate
// metadata (such as category, command, usage_id, prefix, suffix) is preserved.
void PopulateCandidateFromResult(const prediction::Result& result,
                                 Candidate* candidate);

// Prepares Segments containing HISTORY segments and a new conversion segment
// for request.key().
//
// Boundary handling:
// Iterates through request.history_result().inner_segments() to unpack each
// inner segment into an independent HISTORY Segment with its respective key,
// value, content_key, and content_value.
Segments PrepareSegmentsFromRequest(const ConversionRequest& request);

// Merges user history prediction results and post-correction (supplemental
// model) results into a single list with deduplication.
//
// Ordering rules:
// - Default: Prioritizes user history results over post-correction results.
// - Weak history: If the top user history candidate is weak (has
//   Attribute::WEAK_USER_HISTORY_PREDICTION), the top post-correction/default
//   result is prioritized at position 0 to prevent low-confidence history from
//   overriding Viterbi/PostCorrect, and history candidates are demoted to
//   subsequent positions (consistent with Predictor::DemoteWeakUserHistory).
// - Deduplication: Candidates with duplicate values (spellings) are skipped.
std::vector<prediction::Result> MergePredictionResults(
    std::vector<prediction::Result> user_history_results,
    std::vector<prediction::Result> pc_results);

}  // namespace mozc::converter

#endif  // MOZC_CONVERTER_CONVERTER_UTIL_H_
