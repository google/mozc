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

#ifndef MOZC_CONVERTER_CONVERTER_H_
#define MOZC_CONVERTER_CONVERTER_H_

#include "converter/converter_interface.h"

namespace mozc {
class Segments;
class UserDatamanagerInterface;
class ImmutableConverterInterface;

class ConverterImpl : public ConverterInterface {
 public:
  ConverterImpl();
  virtual ~ConverterImpl();

  bool Predict(Segments *segments,
               const string &key,
               const Segments::RequestType request_type) const;

  bool StartConversion(Segments *segments,
                       const string &key) const;
  bool StartConversionWithComposer(Segments *segments,
                                   const composer::Composer *composer) const;
  bool StartReverseConversion(Segments *segments,
                              const string &key) const;
  bool StartPrediction(Segments *segments,
                       const string &key) const;
  bool StartPredictionWithComposer(Segments *segments,
                                   const composer::Composer *composer) const;
  bool StartSuggestion(Segments *segments,
                       const string &key) const;
  bool StartSuggestionWithComposer(Segments *segments,
                                   const composer::Composer *composer) const;
  bool StartPartialPrediction(Segments *segments,
                              const string &key) const;
  bool StartPartialPredictionWithComposer(
      Segments *segments,
      const composer::Composer *composer) const;
  bool StartPartialSuggestion(Segments *segments,
                              const string &key) const;
  bool StartPartialSuggestionWithComposer(
      Segments *segments,
      const composer::Composer *composer) const;

  bool FinishConversion(Segments *segments) const;
  bool CancelConversion(Segments *segments) const;
  bool ResetConversion(Segments *segments) const;
  bool RevertConversion(Segments *segments) const;
  bool CommitSegmentValue(Segments *segments,
                          size_t segment_index,
                          int candidate_index) const;
  bool CommitPartialSuggestionSegmentValue(Segments *segments,
                                         size_t segment_index,
                                         int candidate_index,
                                         const string &current_segment_key,
                                         const string &new_segment_key) const;
  bool FocusSegmentValue(Segments *segments,
                         size_t segment_index,
                         int candidate_index) const;
  bool FreeSegmentValue(Segments *segments,
                        size_t segment_index) const;
  bool SubmitFirstSegment(Segments *segments,
                          size_t candidate_index) const;
  bool ResizeSegment(Segments *segments,
                     size_t segment_index,
                     int offset_length) const;
  bool ResizeSegment(Segments *segments,
                     size_t start_segment_index,
                     size_t segments_size,
                     const uint8 *new_size_array,
                     size_t array_size) const;
  UserDataManagerInterface *GetUserDataManager();

 private:
  bool CommitSegmentValueInternal(Segments *segments,
                                  size_t segment_index,
                                  int candidate_index,
                                  Segment::SegmentType segment_type) const;

  scoped_ptr<UserDataManagerInterface> user_data_manager_;
  ImmutableConverterInterface *immutable_converter_;
};
}  // namespace mozc

#endif  // MOZC_CONVERTER_CONVERTER_H_
