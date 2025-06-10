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
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/optimization.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/strings/assign.h"
#include "base/util.h"
#include "base/vlog.h"
#include "composer/composer.h"
#include "converter/candidate.h"
#include "converter/history_reconstructor.h"
#include "converter/immutable_converter_interface.h"
#include "converter/reverse_converter.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "engine/modules.h"
#include "prediction/predictor_interface.h"
#include "prediction/result.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "transliteration/transliteration.h"

namespace mozc {
namespace {

using ::mozc::converter::Candidate;

constexpr size_t kErrorIndex = static_cast<size_t>(-1);

size_t GetSegmentIndex(const Segments *segments, size_t segment_index) {
  const size_t history_segments_size = segments->history_segments_size();
  const size_t result = history_segments_size + segment_index;
  if (result >= segments->segments_size()) {
    return kErrorIndex;
  }
  return result;
}

bool ShouldInitSegmentsForPrediction(absl::string_view key,
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

Converter::Converter(
    std::unique_ptr<engine::Modules> modules,
    const ImmutableConverterFactory &immutable_converter_factory,
    const PredictorFactory &predictor_factory,
    const RewriterFactory &rewriter_factory)
    : modules_(std::move(modules)),
      immutable_converter_(immutable_converter_factory(*modules_)),
      pos_matcher_(modules_->GetPosMatcher()),
      user_dictionary_(modules_->GetUserDictionary()),
      history_reconstructor_(modules_->GetPosMatcher()),
      reverse_converter_(*immutable_converter_),
      general_noun_id_(pos_matcher_.GetGeneralNounId()) {
  DCHECK(immutable_converter_);
  predictor_ = predictor_factory(*modules_, *this, *immutable_converter_);
  rewriter_ = rewriter_factory(*modules_);
  DCHECK(predictor_);
  DCHECK(rewriter_);
}

bool Converter::StartConversion(const ConversionRequest &request,
                                Segments *segments) const {
  DCHECK_EQ(request.request_type(), ConversionRequest::CONVERSION);

  absl::string_view key = request.key();
  if (key.empty()) {
    return false;
  }

  segments->InitForConvert(key);
  ApplyConversion(segments, request);
  return IsValidSegments(request, *segments);
}

bool Converter::StartReverseConversion(Segments *segments,
                                       const absl::string_view key) const {
  segments->Clear();
  if (key.empty()) {
    return false;
  }
  segments->InitForConvert(key);

  return reverse_converter_.ReverseConvert(key, segments);
}

// static
void Converter::MaybeSetConsumedKeySizeToCandidate(size_t consumed_key_size,
                                                   Candidate *candidate) {
  if (candidate->attributes & Candidate::PARTIALLY_KEY_CONSUMED) {
    // If PARTIALLY_KEY_CONSUMED is set already,
    // the candidate has set appropriate attribute and size by predictor.
    return;
  }
  candidate->attributes |= Candidate::PARTIALLY_KEY_CONSUMED;
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
  if (ShouldInitSegmentsForPrediction(key, *segments)) {
    segments->InitForConvert(key);
  }
  DCHECK_EQ(segments->conversion_segments_size(), 1);
  DCHECK_EQ(segments->conversion_segment(0).key(), key);

  if (!PredictForRequestWithSegments(request, segments)) {
    // Prediction can fail for keys like "12". Even in such cases, rewriters
    // (e.g., number and variant rewriters) can populate some candidates.
    // Therefore, this is not an error.
    MOZC_VLOG(1) << "PredictForRequest failed for key: "
                 << segments->segment(0).key();
  }
  ApplyPostProcessing(request, segments);
  return IsValidSegments(request, *segments);
}

bool Converter::StartPredictionWithPreviousSuggestion(
    const ConversionRequest &request, const Segment &previous_segment,
    Segments *segments) const {
  bool result = StartPrediction(request, segments);
  segments->PrependCandidates(previous_segment);
  if (!result) {
    return false;
  }

  ApplyPostProcessing(request, segments);
  return IsValidSegments(request, *segments);
}

void Converter::PrependCandidates(const ConversionRequest &request,
                                  const Segment &segment,
                                  Segments *segments) const {
  segments->PrependCandidates(segment);
  ApplyPostProcessing(request, segments);
}

void Converter::ApplyPostProcessing(const ConversionRequest &request,
                                    Segments *segments) const {
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
    MaybeSetConsumedKeySizeToSegment(Util::CharsLen(request.key()),
                                     segments->mutable_conversion_segment(0));
  }
}

void Converter::FinishConversion(const ConversionRequest &request,
                                 Segments *segments) const {
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

  PopulateReadingOfCommittedCandidateIfMissing(segments);

  // Sets unique revert id.
  // Clients store the last commit operations with this id.
  absl::BitGen bitgen;
  const uint64_t revert_id = absl::Uniform<uint64_t>(
      absl::IntervalClosed, bitgen, 1, std::numeric_limits<uint64_t>::max());
  segments->set_revert_id(revert_id);

  const ConversionRequest finish_req = ConversionRequestBuilder()
                                           .SetConversionRequestView(request)
                                           .SetHistorySegmentsView(*segments)
                                           .Build();
  DCHECK(finish_req.HasConverterHistorySegments());

  rewriter_->Finish(finish_req, *segments);
  predictor_->Finish(finish_req, MakeLearningResults(*segments),
                     segments->revert_id());

  if (request.request_type() != ConversionRequest::CONVERSION &&
      segments->conversion_segments_size() >= 1 &&
      segments->conversion_segment(0).candidates_size() >= 1) {
    Segment *segment = segments->mutable_conversion_segment(0);
    segment->set_key(segment->candidate(0).key);
  }

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
  if (segments->revert_id() == 0) {
    return;
  }
  rewriter_->Revert(*segments);
  predictor_->Revert(segments->revert_id());
  segments->set_revert_id(0);
}

bool Converter::DeleteCandidateFromHistory(const Segments &segments,
                                           size_t segment_index,
                                           int candidate_index) const {
  DCHECK_LT(segment_index, segments.segments_size());
  const Segment &segment = segments.segment(segment_index);
  DCHECK(segment.is_valid_index(candidate_index));
  const Candidate &candidate = segment.candidate(candidate_index);
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
    segment->mutable_candidate(0)->attributes |= Candidate::RERANKED;
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

  Segment *segment = segments->mutable_segment(raw_segment_index);
  DCHECK_LT(0, segment->candidates_size());
  segment->set_key(current_segment_key);

  Segment *new_segment = segments->insert_segment(raw_segment_index + 1);
  new_segment->set_key(new_segment_key);
  DCHECK_GT(segments->conversion_segments_size(), 0);

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
  for (size_t i = 0; i < candidate_index.size(); ++i) {
    // 2nd argument must always be 0 because on each iteration
    // 1st segment is submitted.
    // Using 0 means submitting 1st segment iteratively.
    if (!CommitSegmentValueInternal(segments, 0, candidate_index[i],
                                    Segment::SUBMITTED)) {
      return false;
    }
  }
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

  if (segment_index >= segments->conversion_segments_size()) {
    return false;
  }

  const size_t key_len = segments->conversion_segment(segment_index).key_len();
  if (key_len == 0) {
    return false;
  }

  const int new_size = key_len + offset_length;
  if (new_size <= 0 || new_size > std::numeric_limits<uint8_t>::max()) {
    return false;
  }
  const std::array<uint8_t, 1> new_size_array = {
      static_cast<uint8_t>(new_size)};
  return ResizeSegments(segments, request, segment_index, new_size_array);
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

  if (!segments->Resize(start_segment_index, new_size_array)) {
    return false;
  }

  ApplyConversion(segments, request);
  return true;
}

void Converter::ApplyConversion(Segments *segments,
                                const ConversionRequest &request) const {
  if (!immutable_converter_->ConvertForRequest(request, segments)) {
    // Conversion can fail for keys like "12". Even in such cases, rewriters
    // (e.g., number and variant rewriters) can populate some candidates.
    // Therefore, this is not an error.
    MOZC_VLOG(1) << "ConvertForRequest failed for key: "
                 << segments->segment(0).key();
  }

  ApplyPostProcessing(request, segments);
}

void Converter::CompletePosIds(Candidate *candidate) const {
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
    segments.InitForConvert(candidate->key);
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
    if (!immutable_converter_->ConvertForRequest(request, &segments)) {
      LOG(ERROR) << "ImmutableConverter::Convert() failed";
      return;
    }
    for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
      const Candidate &ref_candidate = segments.segment(0).candidate(i);
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
  // 1. Resize segments if needed.
  // Check if the segments need to be resized.
  if (std::optional<RewriterInterface::ResizeSegmentsRequest> resize_request =
          rewriter_->CheckResizeSegmentsRequest(request, *segments);
      resize_request.has_value()) {
    if (ResizeSegments(segments, request, resize_request->segment_index,
                       resize_request->segment_sizes)) {
      // If the segments are resized, ResizeSegments recursively executed
      // RewriteAndSuppressCandidates with resized segments. No need to execute
      // them again.
      // TODO(b/381537649): Stop using the recursive call of
      // RewriteAndSuppressCandidates.
      return;
    }
  }

  // 2. Rewrite candidates in each segment.
  if (!rewriter_->Rewrite(request, segments)) {
    return;
  }

  // 3. Suppress candidates in each segment.
  // Optimization for common use case: Since most of users don't use suppression
  // dictionary and we can skip the subsequent check.
  if (!user_dictionary_.HasSuppressedEntries()) {
    return;
  }
  // Although the suppression dictionary is applied at node-level in dictionary
  // layer, there's possibility that bad words are generated from multiple nodes
  // and by rewriters. Hence, we need to apply it again at the last stage of
  // converter.
  for (Segment &segment : segments->conversion_segments()) {
    for (size_t j = 0; j < segment.candidates_size();) {
      const Candidate &cand = segment.candidate(j);
      if (user_dictionary_.IsSuppressedEntry(cand.key, cand.value)) {
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

bool Converter::Reload() {
  modules().GetUserDictionary().Reload();
  return rewriter().Reload() && predictor().Reload();
}

bool Converter::Sync() { return rewriter().Sync() && predictor().Sync(); }

bool Converter::Wait() {
  modules().GetUserDictionary().WaitForReloader();
  return predictor().Wait();
}

std::optional<std::string> Converter::GetReading(absl::string_view text) const {
  Segments segments;
  if (!StartReverseConversion(&segments, text)) {
    LOG(ERROR) << "Reverse conversion failed to get the reading of " << text;
    return std::nullopt;
  }
  if (segments.conversion_segments_size() != 1 ||
      segments.conversion_segment(0).candidates_size() == 0) {
    LOG(ERROR) << "Reverse conversion returned an invalid result for " << text;
    return std::nullopt;
  }
  return std::move(
      segments.mutable_conversion_segment(0)->mutable_candidate(0)->value);
}

void Converter::PopulateReadingOfCommittedCandidateIfMissing(
    Segments *segments) const {
  if (segments->conversion_segments_size() == 0) return;

  Segment *segment = segments->mutable_conversion_segment(0);
  if (segment->candidates_size() == 0) return;

  converter::Candidate *cand = segment->mutable_candidate(0);
  if (!cand->key.empty() || cand->value.empty()) return;

  if (cand->content_value == cand->value) {
    if (std::optional<std::string> key = GetReading(cand->value);
        key.has_value()) {
      cand->key = *key;
      cand->content_key = *std::move(key);
    }
    return;
  }

  if (cand->content_value.empty()) {
    LOG(ERROR) << "Content value is empty: " << *cand;
    return;
  }

  const absl::string_view functional_value = cand->functional_value();
  if (Util::GetScriptType(functional_value) != Util::HIRAGANA) {
    LOG(ERROR) << "The functional value is not hiragana: " << *cand;
    return;
  }
  if (std::optional<std::string> content_key = GetReading(cand->content_value);
      content_key.has_value()) {
    cand->key = absl::StrCat(*content_key, functional_value);
    cand->content_key = *std::move(content_key);
  }
}

bool Converter::PredictForRequestWithSegments(const ConversionRequest &request,
                                              Segments *segments) const {
  DCHECK(segments);
  DCHECK(predictor_);

  const ConversionRequest conv_req = ConversionRequestBuilder()
                                         .SetConversionRequestView(request)
                                         .SetHistorySegmentsView(*segments)
                                         .Build();
  DCHECK(conv_req.HasConverterHistorySegments());

  const std::vector<prediction::Result> results = predictor_->Predict(conv_req);

  Segment *segment = segments->mutable_conversion_segment(0);
  DCHECK(segment);

  for (const prediction::Result &result : results) {
    converter::Candidate *candidate = segment->add_candidate();
    strings::Assign(candidate->key, result.key);
    strings::Assign(candidate->value, result.value);
    strings::Assign(candidate->content_key, result.key);
    strings::Assign(candidate->content_value, result.value);
    strings::Assign(candidate->description, result.description);
    candidate->lid = result.lid;
    candidate->rid = result.rid;
    candidate->wcost = result.wcost;
    candidate->cost = result.cost;
    candidate->attributes = result.candidate_attributes;
    candidate->consumed_key_size = result.consumed_key_size;
    candidate->inner_segment_boundary = result.inner_segment_boundary;

    // When inner_segment_boundary is available, generate
    // content_key and content_value from the boundary info.
    if (!result.inner_segment_boundary.empty()) {
      // Gets the last key/value and content_key/content_value.
      const auto [key_len, value_len, content_key_len, content_value_len] =
          converter::Candidate::DecodeLengths(
              result.inner_segment_boundary.back());
      const int function_key_len = key_len - content_key_len;
      const int function_value_len = value_len - content_value_len;
      if (function_key_len > 0 &&
          function_key_len <= candidate->content_key.size()) {
        candidate->content_key.erase(content_key_len, function_key_len);
      }
      if (function_value_len > 0 &&
          function_value_len <= candidate->content_value.size()) {
        candidate->content_value.erase(content_value_len, function_value_len);
      }
    }
#ifndef NDEBUG
    absl::StrAppend(&candidate->log, "\n", result.log);
#endif  // NDEBUG
  }

  return !results.empty();
}

// static
std::vector<prediction::Result> Converter::MakeLearningResults(
    const Segments &segments) {
  std::vector<prediction::Result> results;

  if (segments.conversion_segments_size() == 0) {
    return results;
  }

  // - segments_size = 1: Populates the nbest candidates to result.
  if (segments.conversion_segments_size() == 1) {
    // Populates only top 5 results.
    // See UserHistoryPredictor::MaybeRemoveUnselectedHistory
    constexpr int kMaxHistorySize = 5;
    for (const auto &candidate : segments.conversion_segment(0).candidates()) {
      prediction::Result result;
      strings::Assign(result.key, candidate->key);
      strings::Assign(result.value, candidate->value);
      strings::Assign(result.description, candidate->description);
      result.lid = candidate->lid;
      result.rid = candidate->rid;
      result.wcost = candidate->wcost;
      result.cost = candidate->cost;
      result.candidate_attributes = candidate->attributes;
      result.consumed_key_size = candidate->consumed_key_size;
      result.inner_segment_boundary = candidate->inner_segment_boundary;
      // Force to set inner_segment_boundary from key/content_key.
      uint32_t encoded = 0;
      if (result.inner_segment_boundary.empty() &&
          converter::Candidate::EncodeLengths(
              candidate->key.size(), candidate->value.size(),
              candidate->content_key.size(), candidate->content_value.size(),
              &encoded)) {
        result.inner_segment_boundary.emplace_back(encoded);
      }
      results.emplace_back(std::move(result));
      if (results.size() >= kMaxHistorySize) break;
    }

    return results;
  }

  // segments_size > 1: Populates the top candidate to result by
  //                    concatenating the segments.
  {
    bool inner_segment_boundary_failed = false;
    prediction::Result result;
    for (const auto &segment : segments.conversion_segments()) {
      if (segment.candidates_size() == 0) return {};
      const converter::Candidate &candidate = segment.candidate(0);
      absl::StrAppend(&result.key, candidate.key);
      absl::StrAppend(&result.value, candidate.value);
      result.candidate_attributes |= candidate.attributes;
      result.wcost += candidate.wcost;
      result.cost += candidate.cost;
      uint32_t encoded = 0;
      if (!converter::Candidate::EncodeLengths(
              candidate.key.size(), candidate.value.size(),
              candidate.content_key.size(), candidate.content_value.size(),
              &encoded)) {
        inner_segment_boundary_failed = true;
      }
      result.inner_segment_boundary.emplace_back(encoded);
    }

    if (inner_segment_boundary_failed) result.inner_segment_boundary.clear();

    const int size = segments.conversion_segments_size();
    result.lid = segments.conversion_segment(0).candidate(0).lid;
    result.rid = segments.conversion_segment(size - 1).candidate(0).rid;

    results.emplace_back(std::move(result));
  }

  return results;
}
}  // namespace mozc
