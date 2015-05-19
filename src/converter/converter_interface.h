// Copyright 2010-2012, Google Inc.
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

#include <string>
#include "base/base.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"

namespace mozc {
class UserDataManagerInterface;

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
  // key is a request writtein in Hiragana sequence
  virtual bool StartConversion(Segments *segments,
                               const string &key) const = 0;

  // Start reverse conversion with key.
  virtual bool StartReverseConversion(Segments *segments,
                                      const string &key) const = 0;

  // Starts prediction for given request.
  virtual bool StartPredictionForRequest(const ConversionRequest &request,
                                         Segments *segments) const = 0;

  // Start prediction with key (request_type = PREDICTION)
  virtual bool StartPrediction(Segments *segments,
                               const string &key) const = 0;

  // DEPRECATED. Start prediction with composer.
  // TODO(noriyukit): Remove this method.
  virtual bool StartPredictionWithComposer(
      Segments *segments, const composer::Composer *composer) const {
    DCHECK(composer);
    DLOG(WARNING) << ("StartPredictionWithComposer() is deprecated. "
                      "Use StartPredictionForRequest() instead.");
    return StartPredictionForRequest(ConversionRequest(composer), segments);
  }

  // Starts suggestion for given request.
  virtual bool StartSuggestionForRequest(const ConversionRequest &request,
                                         Segments *segments) const = 0;

  // Start suggestion with key (request_type = SUGGESTION)
  virtual bool StartSuggestion(Segments *segments,
                               const string &key) const = 0;

  // DEPRECATED. Start suggestion with composer.
  // TODO(noriyukit): Remove this method.
  virtual bool StartSuggestionWithComposer(
      Segments *segments, const composer::Composer *composer) const {
    DCHECK(composer);
    DLOG(WARNING) << ("StartSuggestionWithComposer() is deprecated. "
                      "Use StartSuggestionForRequest() instead.");
    return StartSuggestionForRequest(ConversionRequest(composer), segments);
  }

  // Starts partial prediction for given request.
  virtual bool StartPartialPredictionForRequest(
      const ConversionRequest &request, Segments *segments) const = 0;

  // Start prediction with key (request_type = PARTIAL_PREDICTION)
  virtual bool StartPartialPrediction(Segments *segments,
                                      const string &key) const = 0;

  // DEPRECATED.
  // Start prediction with composer (request_type = PARTIAL_PREDICTION)
  // TODO(noriyukit): Remove this method.
  virtual bool StartPartialPredictionWithComposer(
      Segments *segments, const composer::Composer *composer) const {
    DCHECK(composer);
    DLOG(WARNING) << ("StartPartialPredictionWithComposer() is deprecated. "
                      "Use StartPartialPredictionForRequest() instead.");
    return StartPartialPredictionForRequest(ConversionRequest(composer),
                                            segments);
  }

  // Starts partial suggestion for given request.
  virtual bool StartPartialSuggestionForRequest(
      const ConversionRequest &request, Segments *segments) const = 0;

  // Start suggestion with key (request_type = PARTIAL_SUGGESTION)
  virtual bool StartPartialSuggestion(Segments *segments,
                                      const string &key) const = 0;

  // DEPRECATED.
  // Start suggestion with composer (request_type = PARTIAL_SUGGESTION)
  // TODO(noriyukit): Remove this method.
  virtual bool StartPartialSuggestionWithComposer(
      Segments *segments, const composer::Composer *composer) const {
    DCHECK(composer);
    DLOG(WARNING) << ("StartPartialSuggestionWithComposer() is deprecated. "
                      "Use StartPartialSuggestionForRequest() instead.");
    return StartPartialSuggestionForRequest(ConversionRequest(composer),
                                            segments);
  }

  // Finish conversion.
  // Segments are cleared. Context is not clreared
  virtual bool FinishConversion(Segments *segments) const = 0;

  // Clear segments and keep the context
  virtual bool CancelConversion(Segments *segments) const = 0;

  // Reset segments and context
  virtual bool ResetConversion(Segments *segments) const = 0;

  // Revert last Finish operation
  virtual bool RevertConversion(Segments *segments) const = 0;

  // Expand the bunsetsu-segment at "segment_index" by candidate_size
  // DEPRECATED: This method doesn't take any effect.
  // TODO(taku): remove this method.
  virtual bool GetCandidates(Segments *segments,
                             size_t segment_index,
                             size_t candidate_size) const {
    return true;
  }

  // Commit candidate
  virtual bool CommitSegmentValue(Segments *segments,
                                  size_t segment_index,
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
      Segments *segments,
      size_t segment_index,
      int candidate_index,
      const string &current_segment_key,
      const string &new_segment_key) const = 0;
  // Focus the candidate.
  // This method is mainly called when user puts SPACE key
  // and changes the focused candidate.
  // In this method, Converter will find bracketing matching.
  // e.g., when user selects "「",  corresponding closing bracket "」"
  // is chosen in the preedit.
  virtual bool FocusSegmentValue(Segments *segments,
                                 size_t segment_index,
                                 int candidate_index) const = 0;

  // Revert the operation of CommitSegment
  virtual bool FreeSegmentValue(Segments *segments,
                                size_t segment_index) const = 0;

  // Commit the first segment and move the candidate
  // into history segment temporally.
  // Session can use this method for PartialSubmit.
  virtual bool SubmitFirstSegment(Segments *segments,
                                  size_t candidate_index) const = 0;

  // Resize segment_index-th segment by offset_length.
  // offset_lenth can be negative.
  virtual bool ResizeSegment(Segments *segments,
                             const ConversionRequest &request,
                             size_t segment_index,
                             int offset_length) const = 0;

  // Resize [start_segment_index, start_segment_index + segment_size]
  // segments with the new size in new_size_array.
  // size of new_size_array is specified in 'array_size'
  virtual bool ResizeSegment(Segments *segments,
                             const ConversionRequest &request,
                             size_t start_segment_index,
                             size_t segments_size,
                             const uint8 *new_size_array,
                             size_t array_size) const = 0;

  virtual UserDataManagerInterface *GetUserDataManager() = 0;

 protected:
  ConverterInterface() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ConverterInterface);
};

class ConverterFactory {
 public:
  // Create singleton Converter insntance.
  // If failed, return NULL
  // TODO(taku): move them to ConverterFactory
  static ConverterInterface *GetConverter();

  // Inject a dependency for unittesting
  static void SetConverter(ConverterInterface *converter);

 private:
  ConverterFactory() {}
  ~ConverterFactory() {}
};

// static converter utilities
class ConverterUtil {
 public:
  // Make a segment having one candidate. The value of candidate is the
  // same as the preedit.  This function can be used for error handling.
  // When the converter fails, we can call this function to make a
  // virtual segment.
  static void InitSegmentsFromString(const string &key,
                                     const string &preedit,
                                     Segments *segments);

 private:
  ConverterUtil() {}
  ~ConverterUtil() {}
};
}  // namespace mozc
#endif  // MOZC_CONVERTER_CONVERTER_INTERFACE_H_
