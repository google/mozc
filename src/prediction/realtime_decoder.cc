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

#include "prediction/realtime_decoder.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/strings/assign.h"
#include "base/util.h"
#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/converter_interface.h"
#include "converter/immutable_converter_interface.h"
#include "converter/inner_segment.h"
#include "converter/segments.h"
#include "dictionary/dictionary_token.h"
#include "prediction/result.h"
#include "request/conversion_request.h"

namespace mozc::prediction {
namespace {

// TODO(taku): Defines this function as a common utility function.
Segments MakeSegments(const ConversionRequest& request) {
  converter::Segments segments;
  const prediction::Result& result = request.history_result();

  auto add_history_segment = [&](absl::string_view key, absl::string_view value,
                                 absl::string_view content_key,
                                 absl::string_view content_value) {
    converter::Segment* seg = segments.add_segment();
    seg->set_key(key);
    seg->set_segment_type(converter::Segment::HISTORY);
    converter::Candidate* candidate = seg->add_candidate();
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
    converter::Candidate* candidate =
        segments.mutable_history_segment(history_size - 1)
            ->mutable_candidate(0);
    candidate->cost = result.cost;
    candidate->rid = result.rid;
  }

  segments.add_segment()->set_key(request.key());

  return segments;
}

// TODO(taku): Defines this function as a common utility function.
std::optional<Result> ConversionSegmentsToResult(const Segments& segments) {
  Result result;

  converter::InnerSegmentBoundaryBuilder builder;
  for (const Segment& segment : segments.conversion_segments()) {
    if (segment.candidates_size() == 0) return std::nullopt;
    const converter::Candidate& candidate = segment.candidate(0);
    absl::StrAppend(&result.value, candidate.value);
    absl::StrAppend(&result.key, candidate.key);
    result.wcost += candidate.wcost;
    result.cost += candidate.cost;
    result.candidate_attributes |= candidate.attributes;
    result.candidate_attributes |=
        (candidate.attributes &
         converter::Attribute::USER_SEGMENT_HISTORY_REWRITER);
    builder.Add(candidate.key.size(), candidate.value.size(),
                candidate.content_key.size(), candidate.content_value.size());
  }

  result.inner_segment_boundary = builder.Build(result.key, result.value);

  result.lid = segments.conversion_segments().front().candidate(0).lid;
  result.rid = segments.conversion_segments().back().candidate(0).rid;

  return result;
}
}  // namespace

bool RealtimeDecoder::PushBackTopConversionResult(
    const ConversionRequest& request, std::vector<Result>* results) const {
  ConversionRequest::Options options;
  options.max_conversion_candidates_size = 20;
  options.composer_key_selection = ConversionRequest::PREDICTION_KEY;
  // Some rewriters cause significant performance loss. So we skip them.
  options.skip_slow_rewriters = true;
  // This method emulates usual converter's behavior so here disable
  // partial candidates.
  options.create_partial_candidates = false;
  options.request_type = ConversionRequest::CONVERSION;
  const ConversionRequest tmp_request = ConversionRequestBuilder()
                                            .SetConversionRequestView(request)
                                            .SetOptions(std::move(options))
                                            .Build();

  Segments tmp_segments = MakeSegments(request);

  DCHECK_EQ(tmp_segments.conversion_segments_size(), 1);
  DCHECK_EQ(tmp_segments.conversion_segment(0).key(), tmp_request.key());

  if (!converter().StartConversion(tmp_request, &tmp_segments)) {
    return false;
  }

  std::optional<Result> result_opt = ConversionSegmentsToResult(tmp_segments);
  if (!result_opt.has_value()) {
    return false;
  }

  Result& result = result_opt.value();
  result.SetTypesAndTokenAttributes(REALTIME | REALTIME_TOP,
                                    dictionary::Token::NONE);
  result.candidate_attributes |= converter::Attribute::NO_VARIANTS_EXPANSION;

  results->emplace_back(std::move(result));

  return true;
}

std::vector<Result> RealtimeDecoder::Decode(
    const ConversionRequest& request) const {
  std::vector<Result> results;
  if (request.options().max_conversion_candidates_size == 0) {
    return results;
  }

  // Accepts only single-segment request.
  DCHECK_NE(request.request_type(), ConversionRequest::CONVERSION);
  if (request.request_type() == ConversionRequest::CONVERSION) {
    return results;
  }

  const ConversionRequest request_for_realtime =
      ConversionRequestBuilder().SetConversionRequestView(request).Build();

  Segments tmp_segments = MakeSegments(request);
  DCHECK_EQ(tmp_segments.conversion_segments_size(), 1);
  DCHECK_EQ(tmp_segments.conversion_segment(0).key(),
            request_for_realtime.key());

  // First insert a top conversion result.
  // Note: Do not call actual converter for partial suggestion /
  // prediction. Converter::StartConversion() resets conversion key from
  // composer rather than using the key in segments.
  if (request.options().use_actual_converter_for_realtime_conversion &&
      request.request_type() != ConversionRequest::PARTIAL_SUGGESTION &&
      request.request_type() != ConversionRequest::PARTIAL_PREDICTION) {
    if (!PushBackTopConversionResult(request_for_realtime, &results)) {
      LOG(WARNING) << "Realtime conversion with converter failed";
    }
  }

  // non-CONVERSION request returns concatenated single segment.
  if (!immutable_converter().Convert(request_for_realtime, &tmp_segments) ||
      tmp_segments.conversion_segments_size() != 1 ||
      tmp_segments.conversion_segment(0).candidates_size() == 0) {
    LOG(WARNING) << "Convert failed";
    return results;
  }

  // Copy candidates into the array of Results.
  const Segment& segment = tmp_segments.conversion_segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    const converter::Candidate& candidate = segment.candidate(i);

    Result result;
    result.key = candidate.key;
    result.value = candidate.value;
    result.cost = candidate.cost;
    result.wcost = candidate.wcost;
    result.lid = candidate.lid;
    result.rid = candidate.rid;
    result.inner_segment_boundary = candidate.inner_segment_boundary;
    result.SetTypesAndTokenAttributes(REALTIME, dictionary::Token::NONE);
    result.candidate_attributes |= converter::Attribute::NO_VARIANTS_EXPANSION;
    if (candidate.key.size() < segment.key().size()) {
      result.candidate_attributes |=
          converter::Attribute::PARTIALLY_KEY_CONSUMED;
      result.consumed_key_size = Util::CharsLen(candidate.key);
    }
    // Kana expansion happens inside the decoder.
    if (candidate.attributes &
        converter::Attribute::KEY_EXPANDED_IN_DICTIONARY) {
      result.types |= prediction::KEY_EXPANDED_IN_DICTIONARY;
    }
    result.candidate_attributes |= candidate.attributes;
    results.emplace_back(std::move(result));
  }

  return results;
}

std::vector<Result> RealtimeDecoder::ReverseDecode(
    const ConversionRequest& request) const {
  Segments tmp_segments = MakeSegments(request);

  const ConversionRequest request_for_reverse =
      ConversionRequestBuilder()
          .SetConversionRequestView(request)
          .SetRequestType(ConversionRequest::REVERSE_CONVERSION)
          .Build();

  if (!immutable_converter().Convert(request_for_reverse, &tmp_segments) ||
      tmp_segments.conversion_segments_size() == 0) {
    LOG(WARNING) << "Reverse conversion failed";
    return {};
  }

  if (std::optional<Result> result_opt =
          ConversionSegmentsToResult(tmp_segments);
      result_opt.has_value()) {
    return {result_opt.value()};
  }

  return {};
}

}  // namespace mozc::prediction
