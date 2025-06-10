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

#ifndef MOZC_CONVERTER_CONVERTER_MOCK_H_
#define MOZC_CONVERTER_CONVERTER_MOCK_H_

#include <cstddef>
#include <cstdint>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"
#include "request/conversion_request.h"
#include "testing/gmock.h"

namespace mozc {

class StrictMockConverter : public ConverterInterface {
 public:
  StrictMockConverter() = default;
  ~StrictMockConverter() override = default;

  MOCK_METHOD(bool, StartConversion,
              (const ConversionRequest &request, Segments *segments),
              (const, override));
  MOCK_METHOD(bool, StartReverseConversion,
              (Segments * segments, absl::string_view key), (const, override));
  MOCK_METHOD(bool, StartPrediction,
              (const ConversionRequest &request, Segments *segments),
              (const, override));
  MOCK_METHOD(bool, StartPredictionWithPreviousSuggestion,
              (const ConversionRequest &request,
               const Segment &previous_segment, Segments *segments),
              (const, override));
  MOCK_METHOD(void, PrependCandidates,
              (const ConversionRequest &request, const Segment &segment,
               Segments *segments),
              (const, override));
  MOCK_METHOD(void, FinishConversion,
              (const ConversionRequest &request, Segments *segments),
              (const, override));
  MOCK_METHOD(void, CancelConversion, (Segments * segments), (const, override));
  MOCK_METHOD(void, ResetConversion, (Segments * segments), (const, override));
  MOCK_METHOD(void, RevertConversion, (Segments * segments), (const, override));
  MOCK_METHOD(bool, DeleteCandidateFromHistory, (const Segments &, size_t, int),
              (const));
  MOCK_METHOD(bool, ReconstructHistory,
              (Segments * segments, absl::string_view preceding_text),
              (const, override));
  MOCK_METHOD(bool, CommitSegmentValue,
              (Segments * segments, size_t segment_index, int candidate_index),
              (const, override));
  MOCK_METHOD(bool, CommitPartialSuggestionSegmentValue,
              (Segments * segments, size_t segment_index, int candidate_index,
               absl::string_view current_segment_key,
               absl::string_view new_segment_key),
              (const, override));
  MOCK_METHOD(bool, FocusSegmentValue,
              (Segments * segments, size_t segment_index, int candidate_index),
              (const, override));
  MOCK_METHOD(bool, CommitSegments,
              (Segments * segments, absl::Span<const size_t> candidate_index),
              (const, override));
  MOCK_METHOD(bool, ResizeSegment,
              (Segments * segments, const ConversionRequest &request,
               size_t segment_index, int offset_length),
              (const, override));
  MOCK_METHOD(bool, ResizeSegments,
              (Segments * segments, const ConversionRequest &request,
               size_t start_segment_index,
               absl::Span<const uint8_t> new_size_array),
              (const, override));
};

typedef ::testing::NiceMock<StrictMockConverter> MockConverter;

}  // namespace mozc

#endif  // MOZC_CONVERTER_CONVERTER_MOCK_H_
