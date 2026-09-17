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

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/strings/assign.h"
#include "converter/attribute.h"
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

std::vector<prediction::Result> ConversionSegmentsToResults(
    const Segments& segments, size_t max_results) {
  if (segments.conversion_segments_size() == 0) {
    return {};
  }

  // - segments_size = 1: Populates the nbest candidates to result.
  if (segments.conversion_segments_size() == 1) {
    std::vector<prediction::Result> results;
    for (const auto& candidate : segments.conversion_segment(0).candidates()) {
      results.push_back(CandidateToResult(*candidate));
      if (results.size() >= max_results) break;
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

void PopulateCandidateFromResult(prediction::Result result,
                                 Candidate* candidate) {
  DCHECK(candidate);
  // Note: Compute content_key and content_value from inner_segments before
  // moving result.key, result.value, and result.inner_segment_boundary.
  std::tie(candidate->content_key, candidate->content_value) =
      result.inner_segments().GetMergedContentKeyAndValue();
  candidate->key = std::move(result.key);
  candidate->value = std::move(result.value);
  candidate->description = std::move(result.description);
  candidate->display_value = std::move(result.display_value);
  candidate->lid = result.lid;
  candidate->rid = result.rid;
  candidate->wcost = result.wcost;
  candidate->cost = result.cost;
  candidate->attributes = result.attributes;
  candidate->consumed_key_size = result.consumed_key_size;
  candidate->inner_segment_boundary = std::move(result.inner_segment_boundary);
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

void ApplyResultToSegmentsMultiSegment(const prediction::Result& result,
                                       size_t target_pos, Segments& segments) {
  if (result.key.empty() || result.value.empty()) {
    return;
  }

  // ---------------------------------------------------------------------------
  // Step 1: Collect inner segment views from the prediction result.
  // Converts the inner segment iterator into a random-access vector.
  // ---------------------------------------------------------------------------
  struct InnerSegView {
    absl::string_view key;
    absl::string_view value;
    size_t functional_key_size;
    size_t functional_value_size;
  };

  std::vector<InnerSegView> inner_segs;
  for (const auto& iter : result.inner_segments()) {
    inner_segs.push_back({
        .key = iter.GetKey(),
        .value = iter.GetValue(),
        .functional_key_size = iter.GetFunctionalKey().size(),
        .functional_value_size = iter.GetFunctionalValue().size(),
    });
  }

  size_t seg_idx = 0;
  size_t inner_idx = 0;
  const size_t num_conversion_segments = segments.conversion_segments_size();
  const size_t num_inner_segments = inner_segs.size();

  // ---------------------------------------------------------------------------
  // Step 2: Two-pointer boundary synchronization walk.
  // Walk through conversion segments and inner segments simultaneously from
  // front to back, finding intervals where cumulative key byte lengths match.
  // ---------------------------------------------------------------------------
  while (seg_idx < num_conversion_segments && inner_idx < num_inner_segments) {
    const size_t start_seg = seg_idx;
    const size_t start_inner = inner_idx;

    // Start with the first segment and inner segment in the current interval.
    size_t seg_span_bytes = segments.conversion_segment(seg_idx).key().size();
    size_t inner_span_bytes = inner_segs[inner_idx].key.size();

    ++seg_idx;
    ++inner_idx;

    // Advance whichever pointer has accumulated fewer key bytes until both
    // spans cover the exact same key byte length.
    while (seg_span_bytes != inner_span_bytes) {
      if (seg_span_bytes < inner_span_bytes) {
        if (seg_idx >= num_conversion_segments) break;
        seg_span_bytes += segments.conversion_segment(seg_idx).key().size();
        ++seg_idx;
      } else {
        if (inner_idx >= num_inner_segments) break;
        inner_span_bytes += inner_segs[inner_idx].key.size();
        ++inner_idx;
      }
    }

    if (seg_span_bytes != inner_span_bytes) {
      LOG(WARNING) << "Key lengths mismatch between segments and result";
      break;
    }

    // -------------------------------------------------------------------------
    // Step 3: Combine inner segments covered by the synchronized interval.
    // `num_segs` is the number of conversion segments spanned by this interval.
    // Since all inner segments are contiguous slices of `result.key` and
    // `result.value`, the combined key and value are contiguous string_views.
    // -------------------------------------------------------------------------
    const size_t num_segs = seg_idx - start_seg;
    const InnerSegView& first_inner = inner_segs[start_inner];
    const InnerSegView& last_inner = inner_segs[inner_idx - 1];

    const char* key_start = first_inner.key.data();
    const char* key_end = last_inner.key.data() + last_inner.key.size();
    const absl::string_view combined_key(key_start, key_end - key_start);

    const char* value_start = first_inner.value.data();
    const char* value_end = last_inner.value.data() + last_inner.value.size();
    const absl::string_view combined_value(value_start,
                                           value_end - value_start);

    absl::string_view combined_content_key = combined_key;
    combined_content_key.remove_suffix(last_inner.functional_key_size);
    absl::string_view combined_content_value = combined_value;
    combined_content_value.remove_suffix(last_inner.functional_value_size);

    // Target the first conversion segment in the interval.
    Segment* target_segment = segments.mutable_conversion_segment(start_seg);

    // -------------------------------------------------------------------------
    // Step 4: Search for an existing candidate in the target segment.
    // Match by value and segment count (single vs multi-segment).
    // -------------------------------------------------------------------------
    int existing_index = -1;
    for (int i = 0; i < target_segment->candidates_size(); ++i) {
      const Candidate& cand = target_segment->candidate(i);
      if (cand.value == combined_value &&
          std::max<size_t>(1, cand.converted_segment_count) == num_segs) {
        existing_index = i;
        break;
      }
    }

    // -------------------------------------------------------------------------
    // Step 5: Update / Promote existing candidate or insert new candidate.
    // -------------------------------------------------------------------------
    if (existing_index >= 0) {
      // Case A: Existing candidate found.
      // Move candidate forward if it is currently positioned after target_pos.
      // Do NOT demote if it is already positioned ahead of target_pos (e.g.
      // placed at pos 0 by a higher-ranked prediction result).
      if (existing_index > target_pos) {
        const size_t pos =
            std::min(target_pos, target_segment->candidates_size() - 1);
        target_segment->move_candidate(existing_index, pos);
        existing_index = pos;
      }
      Candidate* cand = target_segment->mutable_candidate(existing_index);
      cand->cost = std::min(cand->cost, result.cost);
      cand->wcost = std::min(cand->wcost, result.wcost);
      cand->attributes |= result.attributes;
      if (result.inner_segment_boundary.size() >= inner_idx) {
        cand->inner_segment_boundary.assign(
            result.inner_segment_boundary.begin() + start_inner,
            result.inner_segment_boundary.begin() + inner_idx);
      }
    } else {
      // Case B: Candidate not present. Insert a new candidate.
      // - If num_segs > 1: inserted as a multi-segment candidate.
      // - If num_segs == 1: inserted as a standard single-segment candidate.
      const size_t pos =
          std::min(target_pos, target_segment->candidates_size());
      Candidate* cand = (pos == 0 && target_segment->candidates_size() > 0)
                            ? target_segment->push_front_candidate()
                            : target_segment->insert_candidate(pos);
      strings::Assign(cand->key, combined_key);
      strings::Assign(cand->value, combined_value);
      strings::Assign(cand->content_key, combined_content_key);
      strings::Assign(cand->content_value, combined_content_value);
      cand->converted_segment_count = num_segs;
      cand->lid = result.lid;
      cand->rid = result.rid;
      cand->wcost = result.wcost;
      cand->cost = result.cost;
      cand->attributes = result.attributes;
      cand->consumed_key_size = result.consumed_key_size;
      if (result.inner_segment_boundary.size() >= inner_idx) {
        cand->inner_segment_boundary.assign(
            result.inner_segment_boundary.begin() + start_inner,
            result.inner_segment_boundary.begin() + inner_idx);
      }
    }
  }
}

std::vector<prediction::Result> MergePredictionResults(
    std::vector<prediction::Result> user_history_results,
    std::vector<prediction::Result> pc_results) {
  std::vector<prediction::Result> results;
  const bool is_weak_history = !user_history_results.empty() &&
                               (user_history_results.front().attributes &
                                Attribute::WEAK_USER_HISTORY_PREDICTION);

  auto append_unique = [](prediction::Result res,
                          std::vector<prediction::Result>* dst) {
    if (std::none_of(dst->begin(), dst->end(),
                     [&res](const prediction::Result& r) {
                       return r.value == res.value;
                     })) {
      dst->push_back(std::move(res));
    }
  };

  if (is_weak_history && !pc_results.empty()) {
    // When the top history candidate is weak, prevent it from becoming the top
    // candidate by prioritizing pc_results.front() (default conversion or
    // post-corrected result) at position 0 and demoting history candidates
    // to subsequent positions (consistent with
    // Predictor::DemoteWeakUserHistory).
    results.push_back(pc_results.front());
    for (prediction::Result& res : user_history_results) {
      append_unique(std::move(res), &results);
    }
    for (size_t i = 1; i < pc_results.size(); ++i) {
      append_unique(std::move(pc_results[i]), &results);
    }
  } else {
    // Normal history: user history results followed by post-correction results.
    results = std::move(user_history_results);
    for (prediction::Result& res : pc_results) {
      append_unique(std::move(res), &results);
    }
  }
  return results;
}

}  // namespace mozc::converter
