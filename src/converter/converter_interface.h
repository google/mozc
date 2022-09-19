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

#include <cstdint>
#include <string>
#include <vector>

#include "base/port.h"
#include "converter/segments.h"
#include "request/conversion_request.h"
#include "absl/strings/string_view.h"

namespace mozc {

namespace composer {
class Composer;
}  // namespace composer

class ConverterInterface {
 public:
  // Allow deletion through the interface.
  virtual ~ConverterInterface() {}

  // Starts conversion for given request.
  virtual bool StartConversionForRequest(const ConversionRequest &request,
                                         Segments *segments) const = 0;

  // Start conversion with key.
  // key is a request written in Hiragana sequence
  virtual bool StartConversion(Segments *segments,
                               const std::string &key) const = 0;

  // Start reverse conversion with key.
  virtual bool StartReverseConversion(Segments *segments,
                                      const std::string &key) const = 0;

  // Starts prediction for given request.
  virtual bool StartPredictionForRequest(const ConversionRequest &request,
                                         Segments *segments) const = 0;

  // Start prediction with key (request_type = PREDICTION)
  virtual bool StartPrediction(Segments *segments,
                               const std::string &key) const = 0;

  // Starts suggestion for given request.
  virtual bool StartSuggestionForRequest(const ConversionRequest &request,
                                         Segments *segments) const = 0;

  // Start suggestion with key (request_type = SUGGESTION)
  virtual bool StartSuggestion(Segments *segments,
                               const std::string &key) const = 0;

  // Starts partial prediction for given request.
  virtual bool StartPartialPredictionForRequest(
      const ConversionRequest &request, Segments *segments) const = 0;

  // Start prediction with key (request_type = PARTIAL_PREDICTION)
  virtual bool StartPartialPrediction(Segments *segments,
                                      const std::string &key) const = 0;

  // Starts partial suggestion for given request.
  virtual bool StartPartialSuggestionForRequest(
      const ConversionRequest &request, Segments *segments) const = 0;

  // Start suggestion with key (request_type = PARTIAL_SUGGESTION)
  virtual bool StartPartialSuggestion(Segments *segments,
                                      const std::string &key) const = 0;

  // Finish conversion.
  // Segments are cleared. Context is not cleared
  virtual bool FinishConversion(const ConversionRequest &request,
                                Segments *segments) const = 0;

  // Clear segments and keep the context
  virtual bool CancelConversion(Segments *segments) const = 0;

  // Reset segments and context
  virtual bool ResetConversion(Segments *segments) const = 0;

  // Revert last Finish operation
  virtual bool RevertConversion(Segments *segments) const = 0;

  // Reconstruct history segments from given preceding text.
  virtual bool ReconstructHistory(Segments *segments,
                                  const std::string &preceding_text) const = 0;

  // Expand the bunsetsu-segment at "segment_index" by candidate_size
  // DEPRECATED: This method doesn't take any effect.
  // TODO(taku): remove this method.
  virtual bool GetCandidates(Segments *segments, size_t segment_index,
                             size_t candidate_size) const {
    return true;
  }

  // Commit candidate
  virtual bool CommitSegmentValue(Segments *segments, size_t segment_index,
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
  virtual bool CommitPartialSuggestionSegmentValue(
      Segments *segments, size_t segment_index, int candidate_index,
      absl::string_view current_segment_key,
      absl::string_view new_segment_key) const = 0;
  // Focus the candidate.
  // This method is mainly called when user puts SPACE key
  // and changes the focused candidate.
  // In this method, Converter will find bracketing matching.
  // e.g., when user selects "「",  corresponding closing bracket "」"
  // is chosen in the preedit.
  virtual bool FocusSegmentValue(Segments *segments, size_t segment_index,
                                 int candidate_index) const = 0;

  // Commit segments of which the range is [0, candidate_index.size()]
  // and move the candidates into history segment temporally.
  // Session can use this method for PartialCommit.
  // |candidate_index| is a vector containing candidate index.
  // candidate_index[0] corresponds to the index of the candidate of
  // 1st segment.
  virtual bool CommitSegments(
      Segments *segments, const std::vector<size_t> &candidate_index) const = 0;

  // Resize segment_index-th segment by offset_length.
  // offset_length can be negative.
  virtual bool ResizeSegment(Segments *segments,
                             const ConversionRequest &request,
                             size_t segment_index, int offset_length) const = 0;

  // Resize [start_segment_index, start_segment_index + segment_size]
  // segments with the new size in new_size_array.
  // size of new_size_array is specified in 'array_size'
  virtual bool ResizeSegment(Segments *segments,
                             const ConversionRequest &request,
                             size_t start_segment_index, size_t segments_size,
                             const uint8_t *new_size_array,
                             size_t array_size) const = 0;

 protected:
  ConverterInterface() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ConverterInterface);
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_CONVERTER_INTERFACE_H_
