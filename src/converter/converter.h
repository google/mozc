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
//  for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"

namespace mozc {
class ConversionRequest;
class ImmutableConverterInterface;
class PredictorInterface;
class POSMatcher;
class PosGroup;
class RewriterInterface;
class Segments;
class UserDatamanagerInterface;

class ConverterImpl : public ConverterInterface {
 public:
  ConverterImpl();
  ConverterImpl(PredictorInterface *predictor, RewriterInterface *rewriter);
  virtual ~ConverterImpl();

  // Lazily initializes the internal predictor and rewriter.
  void Init(PredictorInterface *predictor, RewriterInterface *rewriter);

  bool Predict(const ConversionRequest &request,
               const string &key,
               const Segments::RequestType request_type,
               Segments *segments) const;

  bool StartConversionForRequest(const ConversionRequest &request,
                                 Segments *segments) const;
  bool StartConversion(Segments *segments,
                       const string &key) const;
  bool StartReverseConversion(Segments *segments,
                              const string &key) const;
  bool StartPredictionForRequest(const ConversionRequest &request,
                                 Segments *segments) const;
  bool StartPrediction(Segments *segments,
                       const string &key) const;
  bool StartSuggestionForRequest(const ConversionRequest &request,
                                 Segments *segments) const;
  bool StartSuggestion(Segments *segments,
                       const string &key) const;
  bool StartPartialPredictionForRequest(const ConversionRequest &request,
                                        Segments *segments) const;
  bool StartPartialPrediction(Segments *segments,
                              const string &key) const;
  bool StartPartialSuggestionForRequest(const ConversionRequest &request,
                                        Segments *segments) const;
  bool StartPartialSuggestion(Segments *segments,
                              const string &key) const;

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
                     const ConversionRequest &requset,
                     size_t segment_index,
                     int offset_length) const;
  bool ResizeSegment(Segments *segments,
                     const ConversionRequest &requset,
                     size_t start_segment_index,
                     size_t segments_size,
                     const uint8 *new_size_array,
                     size_t array_size) const;
  UserDataManagerInterface *GetUserDataManager();

 private:
  FRIEND_TEST(ConverterTest, CompletePOSIds);
  FRIEND_TEST(ConverterTest, SetupHistorySegmentsFromPrecedingText);
  FRIEND_TEST(ConverterTest, DefaultPredictor);

  // Complete Left id/Right id if they are not defined.
  // Some users don't push conversion button but directly
  // input hiragana sequence only with composition mode. Converter
  // cannot know which POS ids should be used for these directly-
  // input strings. This function estimates IDs from value heuristically.
  void CompletePOSIds(Segment::Candidate *candidate) const;

  bool CommitSegmentValueInternal(Segments *segments,
                                  size_t segment_index,
                                  int candidate_index,
                                  Segment::SegmentType segment_type) const;

  // Reconstructs history segments from preceding text to emulate user input
  // from preceding (surrounding) text.
  bool SetupHistorySegmentsFromPrecedingText(const string &preceding_text,
                                             Segments *segments) const;

  const POSMatcher *pos_matcher_;
  const PosGroup *pos_group_;
  scoped_ptr<PredictorInterface> predictor_;
  scoped_ptr<RewriterInterface> rewriter_;
  scoped_ptr<UserDataManagerInterface> user_data_manager_;
  const ImmutableConverterInterface *immutable_converter_;
  const uint16 general_noun_id_;
};
}  // namespace mozc

#endif  // MOZC_CONVERTER_CONVERTER_H_
