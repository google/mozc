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

#ifndef MOZC_CONVERTER_CONVERTER_INTERFACE_H_
#define MOZC_CONVERTER_CONVERTER_INTERFACE_H_

#include <cstddef>
#include <cstdint>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "converter/segments.h"
#include "request/conversion_request.h"

namespace mozc {

namespace composer {
class Composer;
}  // namespace composer

class ConverterInterface {
 public:
  ConverterInterface(const ConverterInterface&) = delete;
  ConverterInterface& operator=(const ConverterInterface&) = delete;

  // Allow deletion through the interface.
  virtual ~ConverterInterface() = default;

  // Starts conversion for given request.
  [[nodiscard]]
  virtual bool StartConversion(const ConversionRequest& request,
                               Segments* segments) const = 0;

  // Start reverse conversion with key.
  [[nodiscard]]
  virtual bool StartReverseConversion(Segments* segments,
                                      absl::string_view key) const = 0;

  // Starts prediction for given request.
  [[nodiscard]]
  virtual bool StartPrediction(const ConversionRequest& request,
                               Segments* segments) const = 0;

  // Starts prediction with previous suggestion.
  // This method is used for expanding the candidates while keeping the
  // previous suggestion.
  [[nodiscard]]
  virtual bool StartPredictionWithPreviousSuggestion(
      const ConversionRequest& request, const Segment& previous_segment,
      Segments* segments) const = 0;

  // Builds segments from the given segment.
  // This method also applies the converter's post processing such as rewriters.
  virtual void PrependCandidates(const ConversionRequest& request,
                                 const Segment& segment,
                                 Segments* segments) const = 0;

  // Finish conversion.
  // Segments are cleared. Context is not cleared
  virtual void FinishConversion(const ConversionRequest& request,
                                Segments* segments) const = 0;

  // Clear segments and keep the context
  virtual void CancelConversion(Segments* segments) const = 0;

  // Reset segments and context
  virtual void ResetConversion(Segments* segments) const = 0;

  // Revert last Finish operation
  virtual void RevertConversion(Segments* segments) const = 0;

  // Delete candidate from user input history.
  // Returns false if the candidate was not found or deletion failed.
  // Note: |segment_index| is the index for all segments, not the index of
  // conversion_segments.
  [[nodiscard]]
  virtual bool DeleteCandidateFromHistory(const Segments& segments,
                                          size_t segment_index,
                                          int candidate_index) const = 0;

  // Reconstruct history segments from given preceding text.
  [[nodiscard]]
  virtual bool ReconstructHistory(Segments* segments,
                                  absl::string_view preceding_text) const = 0;

  // Commit candidate
  [[nodiscard]]
  virtual bool CommitSegmentValue(Segments* segments, size_t segment_index,
                                  int candidate_index) const = 0;
  // Commit candidate for partial suggestion.
  // current_segment_key : key for submitted segment.
  // new_segment_key : key for newly inserted segment.
  // Example:
  //   If the preedit is "いれた|てのおちゃ",
  //   |current_segment_key| is "いれた" and
  //   |new_segment_key| is "てのおちゃ".
  //   After calling this method, the segments will contain following segments.
  //   - {key_ : "いれた",  segment_type_ : SUBMITTED}
  //   - {key_ : "てのおちゃ", segment_type_ : FREE}
  [[nodiscard]]
  virtual bool CommitPartialSuggestionSegmentValue(
      Segments* segments, size_t segment_index, int candidate_index,
      absl::string_view current_segment_key,
      absl::string_view new_segment_key) const = 0;

  // Focus the candidate.
  // This method is mainly called when user puts SPACE key
  // and changes the focused candidate.
  // In this method, Converter will find bracketing matching.
  // e.g., when user selects "「",  corresponding closing bracket "」"
  // is chosen in the preedit.
  [[nodiscard]]
  virtual bool FocusSegmentValue(Segments* segments, size_t segment_index,
                                 int candidate_index) const = 0;

  // Commit segments of which the range is [0, candidate_index.size()]
  // and move the candidates into history segment temporally.
  // Session can use this method for PartialCommit.
  // |candidate_index| is a vector containing candidate index.
  // candidate_index[0] corresponds to the index of the candidate of
  // 1st segment.
  [[nodiscard]]
  virtual bool CommitSegments(
      Segments* segments, absl::Span<const size_t> candidate_index) const = 0;

  // Resize segment_index-th segment by offset_length.
  // offset_length can be negative.
  [[nodiscard]] virtual bool ResizeSegment(Segments* segments,
                                           const ConversionRequest& request,
                                           size_t segment_index,
                                           int offset_length) const = 0;

  // Resize [start_segment_index, start_segment_index + segment_size]
  // segments with the new size in new_size_array.
  [[nodiscard]] virtual bool ResizeSegments(
      Segments* segments, const ConversionRequest& request,
      size_t start_segment_index,
      absl::Span<const uint8_t> new_size_array) const = 0;

 protected:
  ConverterInterface() = default;
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_CONVERTER_INTERFACE_H_
