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

#include "converter/converter.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/optimization.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/util.h"
#include "base/vlog.h"
#include "composer/composer.h"
#include "converter/history_reconstructor.h"
#include "converter/immutable_converter_interface.h"
#include "converter/reverse_converter.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "engine/modules.h"
#include "prediction/predictor_interface.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "transliteration/transliteration.h"
#include "usage_stats/usage_stats.h"

namespace mozc {
namespace {

using ::mozc::prediction::PredictorInterface;
using ::mozc::usage_stats::UsageStats;

constexpr size_t kErrorIndex = static_cast<size_t>(-1);

size_t GetSegmentIndex(const Segments *segments, size_t segment_index) {
  const size_t history_segments_size = segments->history_segments_size();
  const size_t result = history_segments_size + segment_index;
  if (result >= segments->segments_size()) {
    return kErrorIndex;
  }
  return result;
}

void SetKey(Segments *segments, const absl::string_view key) {
  segments->set_max_history_segments_size(4);
  segments->clear_conversion_segments();

  mozc::Segment *seg = segments->add_segment();
  DCHECK(seg);

  seg->set_key(key);
  seg->set_segment_type(mozc::Segment::FREE);

  MOZC_VLOG(2) << segments->DebugString();
}

bool ShouldSetKeyForPrediction(const absl::string_view key,
                               const Segments &segments) {
  // (1) If the segment size is 0, invoke SetKey because the segments is not
  //   correctly prepared.
  //   If the key of the segments differs from the input key,
  //   invoke SetKey because current segments should be completely reset.
  // (2) Otherwise keep current key and candidates.
  //
  // This SetKey omitting is for mobile predictor.
  // On normal inputting, we are showing suggestion results. When users
  // push expansion button, we will add prediction results just after the
  // suggestion results. For this, we don't reset segments for prediction.
  // However, we don't have to do so for suggestion. Here, we are deciding
  // whether the input key is changed or not by using segment key. This is not
  // perfect because for roman input, conversion key is not updated by
  // incomplete input, for example, conversion key is "あ" for the input "a",
  // and will still be "あ" for the input "ak". For avoiding mis-reset of
  // the results, we will reset always for suggestion request type.
  return segments.conversion_segments_size() == 0 ||
         segments.conversion_segment(0).key() != key;
}

bool IsValidSegments(const ConversionRequest &request,
                     const Segments &segments) {
  const bool is_mobile = request.request().zero_query_suggestion() &&
                         request.request().mixed_conversion();

  // All segments should have candidate
  for (const Segment &segment : segments) {
    if (segment.candidates_size() != 0) {
      continue;
    }
    // On mobile, we don't distinguish candidates and meta candidates
    // So it's ok if we have meta candidates even if we don't have candidates
    // TODO(team): we may remove mobile check if other platforms accept
    // meta candidate only segment
    if (is_mobile && segment.meta_candidates_size() != 0) {
      continue;
    }
    return false;
  }
  return true;
}

}  // namespace

Converter::Converter(const engine::Modules &modules,
                     const ImmutableConverterInterface &immutable_converter)
    : modules_(modules),
      immutable_converter_(immutable_converter),
      pos_matcher_(*modules.GetPosMatcher()),
      suppression_dictionary_(*modules.GetSuppressionDictionary()),
      history_reconstructor_(*modules.GetPosMatcher()),
      reverse_converter_(immutable_converter),
      general_noun_id_(pos_matcher_.GetGeneralNounId()) {}

void Converter::Init(std::unique_ptr<PredictorInterface> predictor,
                     std::unique_ptr<RewriterInterface> rewriter) {
  // Initializes in order of declaration.
  predictor_ = std::move(predictor);
  rewriter_ = std::move(rewriter);
}

bool Converter::StartConversion(const ConversionRequest &request,
                                Segments *segments) const {
  DCHECK_EQ(request.request_type(), ConversionRequest::CONVERSION);

  absl::string_view key = request.key();
  if (key.empty()) {
    return false;
  }

  SetKey(segments, key);
  ApplyConversion(segments, request);
  return IsValidSegments(request, *segments);
}

bool Converter::StartReverseConversion(Segments *segments,
                                       const absl::string_view key) const {
  segments->Clear();
  if (key.empty()) {
    return false;
  }
  SetKey(segments, key);

  return reverse_converter_.ReverseConvert(key, segments);
}

// static
void Converter::MaybeSetConsumedKeySizeToCandidate(
    size_t consumed_key_size, Segment::Candidate *candidate) {
  if (candidate->attributes & Segment::Candidate::PARTIALLY_KEY_CONSUMED) {
    // If PARTIALLY_KEY_CONSUMED is set already,
    // the candidate has set appropriate attribute and size by predictor.
    return;
  }
  candidate->attributes |= Segment::Candidate::PARTIALLY_KEY_CONSUMED;
  candidate->consumed_key_size = consumed_key_size;
}

// static
void Converter::MaybeSetConsumedKeySizeToSegment(size_t consumed_key_size,
                                                 Segment *segment) {
  for (size_t i = 0; i < segment->candidates_size(); ++i) {
    MaybeSetConsumedKeySizeToCandidate(consumed_key_size,
                                       segment->mutable_candidate(i));
  }
  for (size_t i = 0; i < segment->meta_candidates_size(); ++i) {
    MaybeSetConsumedKeySizeToCandidate(consumed_key_size,
                                       segment->mutable_meta_candidate(i));
  }
}

namespace {
bool ValidateConversionRequestForPrediction(const ConversionRequest &request) {
  switch (request.request_type()) {
    case ConversionRequest::CONVERSION:
      // Conversion request is not for prediction.
      return false;
    case ConversionRequest::PREDICTION:
    case ConversionRequest::SUGGESTION:
      // Typical use case.
      return true;
    case ConversionRequest::PARTIAL_PREDICTION:
    case ConversionRequest::PARTIAL_SUGGESTION: {
      // Partial prediction/suggestion request is applicable only if the
      // cursor is in the middle of the composer.
      const size_t cursor = request.composer().GetCursor();
      return cursor != 0 || cursor != request.composer().GetLength();
    }
    default:
      ABSL_UNREACHABLE();
  }
}
}  // namespace

bool Converter::StartPrediction(const ConversionRequest &request,
                                Segments *segments) const {
  DCHECK(ValidateConversionRequestForPrediction(request));

  absl::string_view key = request.key();
  if (ShouldSetKeyForPrediction(key, *segments)) {
    SetKey(segments, key);
  }
  DCHECK_EQ(1, segments->conversion_segments_size());
  DCHECK_EQ(key, segments->conversion_segment(0).key());

  if (!predictor_->PredictForRequest(request, segments)) {
    // Prediction can fail for keys like "12". Even in such cases, rewriters
    // (e.g., number and variant rewriters) can populate some candidates.
    // Therefore, this is not an error.
    MOZC_VLOG(1) << "PredictForRequest failed for key: "
                 << segments->segment(0).key();
  }
  RewriteAndSuppressCandidates(request, segments);
  TrimCandidates(request, segments);
  if (request.request_type() == ConversionRequest::PARTIAL_SUGGESTION ||
      request.request_type() == ConversionRequest::PARTIAL_PREDICTION) {
    // Here 1st segment's key is the query string of
    // the partial prediction/suggestion.
    // e.g. If the composition is "わた|しは", the key is "わた".
    // If partial prediction/suggestion candidate is submitted,
    // all the characters which are located from the head to the cursor
    // should be submitted (in above case "わた" should be submitted).
    // To do this, PARTIALLY_KEY_CONSUMED and consumed_key_size should be set.
    // Note that this process should be done in a predictor because
    // we have to do this on the candidates created by rewriters.
    MaybeSetConsumedKeySizeToSegment(Util::CharsLen(key),
                                     segments->mutable_conversion_segment(0));
  }
  return IsValidSegments(request, *segments);
}

void Converter::FinishConversion(const ConversionRequest &request,
                                 Segments *segments) const {
  CommitUsageStats(segments, segments->history_segments_size(),
                   segments->conversion_segments_size());

  for (Segment &segment : *segments) {
    // revert SUBMITTED segments to FIXED_VALUE
    // SUBMITTED segments are created by "submit first segment" operation
    // (ctrl+N for ATOK keymap).
    // To learn the conversion result, we should change the segment types
    // to FIXED_VALUE.
    if (segment.segment_type() == Segment::SUBMITTED) {
      segment.set_segment_type(Segment::FIXED_VALUE);
    }
    if (segment.candidates_size() > 0) {
      CompletePosIds(segment.mutable_candidate(0));
    }
  }

  segments->clear_revert_entries();
  rewriter_->Finish(request, segments);
  predictor_->Finish(request, segments);

  // Remove the front segments except for some segments which will be
  // used as history segments.
  const int start_index = std::max<int>(
      0, segments->segments_size() - segments->max_history_segments_size());
  for (int i = 0; i < start_index; ++i) {
    segments->pop_front_segment();
  }

  // Remaining segments are used as history segments.
  for (Segment &segment : *segments) {
    segment.set_segment_type(Segment::HISTORY);
  }
}

void Converter::CancelConversion(Segments *segments) const {
  segments->clear_conversion_segments();
}

void Converter::ResetConversion(Segments *segments) const { segments->Clear(); }

void Converter::RevertConversion(Segments *segments) const {
  if (segments->revert_entries_size() == 0) {
    return;
  }
  rewriter_->Revert(segments);
  predictor_->Revert(segments);
  segments->clear_revert_entries();
}

bool Converter::DeleteCandidateFromHistory(const Segments &segments,
                                           size_t segment_index,
                                           int candidate_index) const {
  DCHECK_LT(segment_index, segments.segments_size());
  const Segment &segment = segments.segment(segment_index);
  DCHECK(segment.is_valid_index(candidate_index));
  const Segment::Candidate &candidate = segment.candidate(candidate_index);
  bool result = false;
  result |=
      rewriter_->ClearHistoryEntry(segments, segment_index, candidate_index);
  result |= predictor_->ClearHistoryEntry(candidate.key, candidate.value);

  return result;
}

bool Converter::ReconstructHistory(
    Segments *segments, const absl::string_view preceding_text) const {
  segments->Clear();
  return history_reconstructor_.ReconstructHistory(preceding_text, segments);
}

bool Converter::CommitSegmentValueInternal(
    Segments *segments, size_t segment_index, int candidate_index,
    Segment::SegmentType segment_type) const {
  segment_index = GetSegmentIndex(segments, segment_index);
  if (segment_index == kErrorIndex) {
    return false;
  }

  Segment *segment = segments->mutable_segment(segment_index);
  const int values_size = static_cast<int>(segment->candidates_size());
  if (candidate_index < -transliteration::NUM_T13N_TYPES ||
      candidate_index >= values_size) {
    return false;
  }

  segment->set_segment_type(segment_type);
  segment->move_candidate(candidate_index, 0);

  if (candidate_index != 0) {
    segment->mutable_candidate(0)->attributes |= Segment::Candidate::RERANKED;
  }

  return true;
}

bool Converter::CommitSegmentValue(Segments *segments, size_t segment_index,
                                   int candidate_index) const {
  return CommitSegmentValueInternal(segments, segment_index, candidate_index,
                                    Segment::FIXED_VALUE);
}

bool Converter::CommitPartialSuggestionSegmentValue(
    Segments *segments, size_t segment_index, int candidate_index,
    const absl::string_view current_segment_key,
    const absl::string_view new_segment_key) const {
  DCHECK_GT(segments->conversion_segments_size(), 0);

  const size_t raw_segment_index = GetSegmentIndex(segments, segment_index);
  if (!CommitSegmentValueInternal(segments, segment_index, candidate_index,
                                  Segment::SUBMITTED)) {
    return false;
  }
  CommitUsageStats(segments, raw_segment_index, 1);

  Segment *segment = segments->mutable_segment(raw_segment_index);
  DCHECK_LT(0, segment->candidates_size());
  const Segment::Candidate &submitted_candidate = segment->candidate(0);
  const bool auto_partial_suggestion =
      Util::CharsLen(submitted_candidate.key) != Util::CharsLen(segment->key());
  segment->set_key(current_segment_key);

  Segment *new_segment = segments->insert_segment(raw_segment_index + 1);
  new_segment->set_key(new_segment_key);
  DCHECK_GT(segments->conversion_segments_size(), 0);

  if (auto_partial_suggestion) {
    UsageStats::IncrementCount("CommitAutoPartialSuggestion");
  } else {
    UsageStats::IncrementCount("CommitPartialSuggestion");
  }

  return true;
}

bool Converter::FocusSegmentValue(Segments *segments, size_t segment_index,
                                  int candidate_index) const {
  segment_index = GetSegmentIndex(segments, segment_index);
  if (segment_index == kErrorIndex) {
    return false;
  }

  return rewriter_->Focus(segments, segment_index, candidate_index);
}

bool Converter::CommitSegments(Segments *segments,
                               absl::Span<const size_t> candidate_index) const {
  const size_t conversion_segment_index = segments->history_segments_size();
  for (size_t i = 0; i < candidate_index.size(); ++i) {
    // 2nd argument must always be 0 because on each iteration
    // 1st segment is submitted.
    // Using 0 means submitting 1st segment iteratively.
    if (!CommitSegmentValueInternal(segments, 0, candidate_index[i],
                                    Segment::SUBMITTED)) {
      return false;
    }
  }
  CommitUsageStats(segments, conversion_segment_index, candidate_index.size());
  return true;
}

bool Converter::ResizeSegment(Segments *segments,
                              const ConversionRequest &request,
                              size_t segment_index, int offset_length) const {
  if (request.request_type() != ConversionRequest::CONVERSION) {
    return false;
  }

  // invalid request
  if (offset_length == 0) {
    return false;
  }

  segment_index = GetSegmentIndex(segments, segment_index);
  if (segment_index == kErrorIndex) {
    return false;
  }

  // the last segments cannot become longer
  if (offset_length > 0 && segment_index == segments->segments_size() - 1) {
    return false;
  }

  const Segment &cur_segment = segments->segment(segment_index);
  const size_t cur_length = Util::CharsLen(cur_segment.key());

  // length cannot become 0
  if (cur_length + offset_length == 0) {
    return false;
  }

  const std::string cur_segment_key = cur_segment.key();

  if (offset_length > 0) {
    int length = offset_length;
    std::string last_key;
    size_t last_clen = 0;
    {
      std::string new_key = cur_segment_key;
      while (segment_index + 1 < segments->segments_size()) {
        last_key = segments->segment(segment_index + 1).key();
        segments->erase_segment(segment_index + 1);
        last_clen = Util::CharsLen(last_key);
        length -= static_cast<int>(last_clen);
        if (length <= 0) {
          std::string tmp;
          Util::Utf8SubString(last_key, 0, length + last_clen, &tmp);
          new_key += tmp;
          break;
        }
        new_key += last_key;
      }

      Segment *segment = segments->mutable_segment(segment_index);
      segment->Clear();
      segment->set_segment_type(Segment::FIXED_BOUNDARY);
      segment->set_key(std::move(new_key));
    }  // scope out |segment|, |new_key|

    if (length < 0) {  // remaining part
      Segment *segment = segments->insert_segment(segment_index + 1);
      segment->set_segment_type(Segment::FREE);
      segment->set_key(
          Util::Utf8SubString(last_key, static_cast<size_t>(length + last_clen),
                              static_cast<size_t>(-length)));
    }
  } else if (offset_length < 0) {
    if (cur_length + offset_length > 0) {
      Segment *segment1 = segments->mutable_segment(segment_index);
      segment1->Clear();
      segment1->set_segment_type(Segment::FIXED_BOUNDARY);
      segment1->set_key(
          Util::Utf8SubString(cur_segment_key, 0, cur_length + offset_length));
    }

    if (segment_index + 1 < segments->segments_size()) {
      Segment *segment2 = segments->mutable_segment(segment_index + 1);
      segment2->set_segment_type(Segment::FREE);
      std::string tmp;
      Util::Utf8SubString(cur_segment_key,
                          std::max<size_t>(0, cur_length + offset_length),
                          cur_length, &tmp);
      tmp += segment2->key();
      segment2->set_key(std::move(tmp));
    } else {
      Segment *segment2 = segments->add_segment();
      segment2->set_segment_type(Segment::FREE);
      segment2->set_key(Util::Utf8SubString(
          cur_segment_key, std::max<size_t>(0, cur_length + offset_length),
          cur_length));
    }
  }

  segments->set_resized(true);

  ApplyConversion(segments, request);
  return true;
}

bool Converter::ResizeSegments(Segments *segments,
                               const ConversionRequest &request,
                               size_t start_segment_index,
                               absl::Span<const uint8_t> new_size_array) const {
  if (request.request_type() != ConversionRequest::CONVERSION) {
    return false;
  }

  start_segment_index = GetSegmentIndex(segments, start_segment_index);
  if (start_segment_index == kErrorIndex) {
    return false;
  }

  const size_t total_size =
      std::accumulate(new_size_array.begin(), new_size_array.end(), 0);
  if (total_size == 0) {
    return false;
  }

  std::string key;
  size_t segments_size = 0;
  for (const Segment &segment : segments->all().drop(start_segment_index)) {
    absl::StrAppend(&key, segment.key());
    ++segments_size;
    if (Util::CharsLen(key) >= total_size) {
      break;
    }
  }

  if (key.empty()) {
    return false;
  }

  size_t consumed = 0;
  const size_t key_len = Util::CharsLen(key);
  std::vector<std::string> new_keys;
  new_keys.reserve(new_size_array.size() + 1);

  for (size_t new_size : new_size_array) {
    if (new_size != 0 && consumed < key_len) {
      new_keys.emplace_back(Util::Utf8SubString(key, consumed, new_size));
      consumed += new_size;
    }
  }
  if (consumed < key_len) {
    new_keys.emplace_back(
        Util::Utf8SubString(key, consumed, key_len - consumed));
  }

  segments->erase_segments(start_segment_index, segments_size);

  for (size_t i = 0; i < new_keys.size(); ++i) {
    Segment *seg = segments->insert_segment(start_segment_index + i);
    seg->set_segment_type(Segment::FIXED_BOUNDARY);
    seg->set_key(std::move(new_keys[i]));
  }

  segments->set_resized(true);

  ApplyConversion(segments, request);
  return true;
}

void Converter::ApplyConversion(Segments *segments,
                                const ConversionRequest &request) const {
  if (!immutable_converter_.ConvertForRequest(request, segments)) {
    // Conversion can fail for keys like "12". Even in such cases, rewriters
    // (e.g., number and variant rewriters) can populate some candidates.
    // Therefore, this is not an error.
    MOZC_VLOG(1) << "ConvertForRequest failed for key: "
                 << segments->segment(0).key();
  }
  RewriteAndSuppressCandidates(request, segments);
  TrimCandidates(request, segments);
}

void Converter::CompletePosIds(Segment::Candidate *candidate) const {
  DCHECK(candidate);
  if (candidate->value.empty() || candidate->key.empty()) {
    return;
  }

  if (candidate->lid != 0 && candidate->rid != 0) {
    return;
  }

  // Use general noun,  unknown word ("サ変") tend to produce
  // "する" "して", which are not always acceptable for non-sahen words.
  candidate->lid = general_noun_id_;
  candidate->rid = general_noun_id_;
  constexpr size_t kExpandSizeStart = 5;
  constexpr size_t kExpandSizeDiff = 50;
  constexpr size_t kExpandSizeMax = 80;
  // In almost all cases, user chooses the top candidate.
  // In order to reduce the latency, first, expand 5 candidates.
  // If no valid candidates are found within 5 candidates, expand
  // candidates step-by-step.
  for (size_t size = kExpandSizeStart; size < kExpandSizeMax;
       size += kExpandSizeDiff) {
    Segments segments;
    SetKey(&segments, candidate->key);
    // use PREDICTION mode, as the size of segments after
    // PREDICTION mode is always 1, thanks to real time conversion.
    // However, PREDICTION mode produces "predictions", meaning
    // that keys of result candidate are not always the same as
    // query key. It would be nice to have PREDICTION_REALTIME_CONVERSION_ONLY.
    const ConversionRequest request =
        ConversionRequestBuilder()
            .SetOptions({
                .request_type = ConversionRequest::PREDICTION,
                .max_conversion_candidates_size = static_cast<int>(size),
            })
            .Build();
    // In order to complete PosIds, call ImmutableConverter again.
    if (!immutable_converter_.ConvertForRequest(request, &segments)) {
      LOG(ERROR) << "ImmutableConverter::Convert() failed";
      return;
    }
    for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
      const Segment::Candidate &ref_candidate =
          segments.segment(0).candidate(i);
      if (ref_candidate.value == candidate->value) {
        candidate->lid = ref_candidate.lid;
        candidate->rid = ref_candidate.rid;
        candidate->cost = ref_candidate.cost;
        candidate->wcost = ref_candidate.wcost;
        candidate->structure_cost = ref_candidate.structure_cost;
        MOZC_VLOG(1) << "Set LID: " << candidate->lid;
        MOZC_VLOG(1) << "Set RID: " << candidate->rid;
        return;
      }
    }
  }
  MOZC_DVLOG(2) << "Cannot set lid/rid. use default value. "
                << "key: " << candidate->key << ", "
                << "value: " << candidate->value << ", "
                << "lid: " << candidate->lid << ", "
                << "rid: " << candidate->rid;
}

void Converter::RewriteAndSuppressCandidates(const ConversionRequest &request,
                                             Segments *segments) const {
  if (!rewriter_->Rewrite(request, segments)) {
    return;
  }
  // Optimization for common use case: Since most of users don't use suppression
  // dictionary and we can skip the subsequent check.
  if (suppression_dictionary_.IsEmpty()) {
    return;
  }
  // Although the suppression dictionary is applied at node-level in dictionary
  // layer, there's possibility that bad words are generated from multiple nodes
  // and by rewriters. Hence, we need to apply it again at the last stage of
  // converter.
  for (Segment &segment : segments->conversion_segments()) {
    for (size_t j = 0; j < segment.candidates_size();) {
      const Segment::Candidate &cand = segment.candidate(j);
      if (suppression_dictionary_.SuppressEntry(cand.key, cand.value)) {
        segment.erase_candidate(j);
      } else {
        ++j;
      }
    }
  }
}

void Converter::TrimCandidates(const ConversionRequest &request,
                               Segments *segments) const {
  const mozc::commands::Request &request_proto = request.request();
  if (!request_proto.has_candidates_size_limit()) {
    return;
  }

  const int limit = request_proto.candidates_size_limit();
  for (Segment &segment : segments->conversion_segments()) {
    const int candidates_size = segment.candidates_size();
    // A segment should have at least one candidate.
    const int candidates_limit =
        std::max<int>(1, limit - segment.meta_candidates_size());
    if (candidates_size < candidates_limit) {
      continue;
    }
    segment.erase_candidates(candidates_limit,
                             candidates_size - candidates_limit);
  }
}

void Converter::CommitUsageStats(const Segments *segments,
                                 size_t begin_segment_index,
                                 size_t segment_length) const {
  if (segment_length == 0) {
    return;
  }
  if (begin_segment_index + segment_length > segments->segments_size()) {
    LOG(ERROR) << "Invalid state. segments size: " << segments->segments_size()
               << " required size: " << begin_segment_index + segment_length;
    return;
  }

  // Timing stats are scaled by 1,000 to improve the accuracy of average values.

  uint64_t submitted_total_length = 0;
  for (const Segment &segment :
       segments->all().subrange(begin_segment_index, segment_length)) {
    const uint32_t submitted_length =
        Util::CharsLen(segment.candidate(0).value);
    UsageStats::UpdateTiming("SubmittedSegmentLengthx1000",
                             submitted_length * 1000);
    submitted_total_length += submitted_length;
  }

  UsageStats::UpdateTiming("SubmittedLengthx1000",
                           submitted_total_length * 1000);
  UsageStats::UpdateTiming("SubmittedSegmentNumberx1000",
                           segment_length * 1000);
  UsageStats::IncrementCountBy("SubmittedTotalLength", submitted_total_length);
}
}  // namespace mozc
