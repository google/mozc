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

#include "converter/converter_util.h"

#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/strings/assign.h"
#include "converter/candidate.h"
#include "converter/inner_segment.h"
#include "converter/segments.h"
#include "prediction/result.h"
#include "request/conversion_request.h"

namespace mozc::converter {

std::optional<prediction::Result> ConversionSegmentsToResult(
    Segments::const_range segments) {
  if (segments.empty()) {
    return std::nullopt;
  }

  prediction::Result result;
  InnerSegmentBoundaryBuilder builder;
  for (const Segment& segment : segments) {
    if (segment.candidates_size() == 0) {
      return std::nullopt;
    }
    const Candidate& candidate = segment.candidate(0);
    absl::StrAppend(&result.key, candidate.key);
    absl::StrAppend(&result.value, candidate.value);
    result.attributes |= candidate.attributes;
    result.wcost += candidate.wcost;
    result.cost += candidate.cost;
    builder.Add(candidate.key.size(), candidate.value.size(),
                candidate.content_key.size(), candidate.content_value.size());
  }

  result.inner_segment_boundary = builder.Build(result.key, result.value);
  result.lid = segments.front().candidate(0).lid;
  result.rid = segments.back().candidate(0).rid;

  return result;
}

prediction::Result HistorySegmentsToResult(
    Segments::const_range history_segments) {
  if (history_segments.empty()) {
    return {};
  }

  prediction::Result result;
  InnerSegmentBoundaryBuilder builder;
  for (const Segment& segment : history_segments) {
    if (segment.candidates_size() == 0) {
      return prediction::Result::DefaultResult();
    }
    const Candidate& candidate = segment.candidate(0);
    absl::StrAppend(&result.key, candidate.key);
    absl::StrAppend(&result.value, candidate.value);
    result.attributes |= candidate.attributes;
    builder.Add(candidate.key.size(), candidate.value.size(),
                candidate.content_key.size(), candidate.content_value.size());
  }

  result.inner_segment_boundary = builder.Build(result.key, result.value);
  result.lid = history_segments.front().candidate(0).lid;
  result.rid = history_segments.back().candidate(0).rid;
  result.cost = history_segments.back().candidate(0).cost;

  return result;
}

std::vector<prediction::Result> MakeLearningResultsFromSegments(
    const Segments& segments) {
  if (segments.conversion_segments_size() == 0) {
    return {};
  }

  // - segments_size = 1: Populates the nbest candidates to result.
  if (segments.conversion_segments_size() == 1) {
    // Populates only top 5 results.
    // See UserHistoryPredictor::MaybeRemoveUnselectedHistory
    constexpr int kMaxHistorySize = 5;
    std::vector<prediction::Result> results;
    for (const auto& candidate : segments.conversion_segment(0).candidates()) {
      results.push_back(CandidateToResult(*candidate));
      if (results.size() >= kMaxHistorySize) break;
    }
    return results;
  }

  // segments_size > 1: Populates the top candidate to result by
  //                    concatenating the segments.
  std::optional<prediction::Result> result =
      ConversionSegmentsToResult(segments.conversion_segments());
  if (!result.has_value()) {
    return {};
  }
  return {*std::move(result)};
}

prediction::Result CandidateToResult(const Candidate& candidate) {
  prediction::Result result;
  strings::Assign(result.key, candidate.key);
  strings::Assign(result.value, candidate.value);
  strings::Assign(result.description, candidate.description);
  strings::Assign(result.display_value, candidate.display_value);
  result.lid = candidate.lid;
  result.rid = candidate.rid;
  result.wcost = candidate.wcost;
  result.cost = candidate.cost;
  result.attributes = candidate.attributes;
  result.consumed_key_size = candidate.consumed_key_size;
  result.inner_segment_boundary = candidate.inner_segment_boundary;
  if (result.inner_segment_boundary.empty()) {
    result.inner_segment_boundary = BuildInnerSegmentBoundary(
        {{candidate.key.size(), candidate.value.size(),
          candidate.content_key.size(), candidate.content_value.size()}},
        result.key, result.value);
  }
  return result;
}

void PopulateCandidateFromResult(const prediction::Result& result,
                                 Candidate* candidate) {
  DCHECK(candidate);
  strings::Assign(candidate->key, result.key);
  strings::Assign(candidate->value, result.value);
  strings::Assign(candidate->description, result.description);
  strings::Assign(candidate->display_value, result.display_value);
  candidate->lid = result.lid;
  candidate->rid = result.rid;
  candidate->wcost = result.wcost;
  candidate->cost = result.cost;
  candidate->attributes = result.attributes;
  candidate->consumed_key_size = result.consumed_key_size;
  candidate->inner_segment_boundary = result.inner_segment_boundary;
  std::tie(candidate->content_key, candidate->content_value) =
      result.inner_segments().GetMergedContentKeyAndValue();
#ifndef NDEBUG
  absl::StrAppend(&candidate->log, "\n", result.log);
#endif  // NDEBUG
}

Segments PrepareSegmentsFromRequest(const ConversionRequest& request) {
  Segments segments;
  const prediction::Result& result = request.history_result();

  auto add_history_segment = [&](absl::string_view key, absl::string_view value,
                                 absl::string_view content_key,
                                 absl::string_view content_value) {
    Segment* seg = segments.add_segment();
    seg->set_key(key);
    seg->set_segment_type(Segment::HISTORY);
    Candidate* candidate = seg->add_candidate();
    strings::Assign(candidate->key, key);
    strings::Assign(candidate->value, value);
    strings::Assign(candidate->content_key, content_key);
    strings::Assign(candidate->content_value, content_value);
  };

  for (const auto& iter : result.inner_segments()) {
    add_history_segment(iter.GetKey(), iter.GetValue(), iter.GetContentKey(),
                        iter.GetContentValue());
  }

  const int history_size = segments.history_segments_size();
  if (history_size > 0) {
    Candidate* candidate = segments.mutable_history_segment(history_size - 1)
                               ->mutable_candidate(0);
    candidate->cost = result.cost;
    candidate->rid = result.rid;
  }

  segments.add_segment()->set_key(request.key());

  return segments;
}

}  // namespace mozc::converter
