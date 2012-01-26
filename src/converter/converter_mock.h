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

#ifndef MOZC_CONVERTER_CONVERTER_MOCK_H_
#define MOZC_CONVERTER_CONVERTER_MOCK_H_

#include "converter/converter_interface.h"

#include <string>
#include <vector>
#include "converter/segments.h"
#include "converter/user_data_manager_interface.h"

namespace mozc {
class ConverterMock : public ConverterInterface {
 public:
  struct ConverterOutput {
    Segments segments;
    bool return_value;
    bool initialized;
    ConverterOutput() : return_value(false), initialized(false) {}
  };

  // TODO(toshiyuki): define more appropriate struct.
  struct ConverterInput {
    Segments segments;
    string key;
    const composer::Composer *composer;
    size_t segment_index;
    size_t candidate_size;
    int candidate_index;
    int offset_length;
    size_t start_segment_index;
    size_t segments_size;
    vector<uint8> new_size_array;
    string current_segment_key;
    string new_segment_key;
  };

  ConverterMock();
  virtual ~ConverterMock();

  // set next output of respective functions
  void SetStartConversion(Segments *segments, bool result);
  void SetStartConversionWithComposer(Segments *segments, bool result);
  void SetStartReverseConversion(Segments *segments, bool result);
  void SetStartPrediction(Segments *segments, bool result);
  void SetStartPredictionWithComposer(Segments *segments, bool result);
  void SetStartSuggestion(Segments *segments, bool result);
  void SetStartSuggestionWithComposer(Segments *segments, bool result);
  void SetStartPartialPrediction(Segments *segments, bool result);
  void SetStartPartialPredictionWithComposer(Segments *segments, bool result);
  void SetStartPartialSuggestion(Segments *segments, bool result);
  void SetStartPartialSuggestionWithComposer(Segments *segments, bool result);
  void SetFinishConversion(Segments *segments, bool result);
  void SetCancelConversion(Segments *segments, bool result);
  void SetResetConversion(Segments *segments, bool result);
  void SetRevertConversion(Segments *segments, bool result);
  void SetCommitSegmentValue(Segments *segments, bool result);
  void SetCommitPartialSuggestionSegmentValue(Segments *segments, bool result);
  void SetFocusSegmentValue(Segments *segments, bool result);
  void SetFreeSegmentValue(Segments *segments, bool result);
  void SetSubmitFirstSegment(Segments *segments, bool result);
  void SetResizeSegment1(Segments *segments, bool result);
  void SetResizeSegment2(Segments *segments, bool result);

  // get last input of respective functions
  void GetStartConversion(Segments *segments, string *key);
  void GetStartConversionWithComposer(Segments *segments,
                                      const composer::Composer **composer);
  void GetStartReverseConversion(Segments *segments, string *key);
  void GetStartPrediction(Segments *segments, string *key);
  void GetStartPredictionWithComposer(Segments *segments,
                                      const composer::Composer **composer);
  void GetStartSuggestion(Segments *segments, string *key);
  void GetStartSuggestionWithComposer(Segments *segments,
                                      const composer::Composer **composer);
  void GetStartPartialPrediction(Segments *segments, string *key);
  void GetStartPartialPredictionWithComposer(
      Segments *segments,
      const composer::Composer **composer);
  void GetStartPartialSuggestion(Segments *segments, string *key);
  void GetStartPartialSuggestionWithComposer(
      Segments *segments,
      const composer::Composer **composer);
  void GetFinishConversion(Segments *segments);
  void GetCancelConversion(Segments *segments);
  void GetResetConversion(Segments *segments);
  void GetRevertConversion(Segments *segments);
  void GetCommitSegmentValue(Segments *segments, size_t *segment_index,
                             int *candidate_index);
  void GetCommitPartialSuggestionSegmentValue(Segments *segments,
                                              size_t *segment_index,
                                              int *candidate_index,
                                              string *current_segment_key,
                                              string *new_segment_key);
  void GetFocusSegmentValue(Segments *segments, size_t *segment_index,
                            int *candidate_index);
  void GetFreeSegmentValue(Segments *segments, size_t *segment_index);
  void GetSubmitFirstSegment(Segments *segments, size_t *candidate_index);
  void GetResizeSegment1(Segments *segments, size_t *segment_index,
                        int *offset_length);
  void GetResizeSegment2(Segments *segments, size_t *start_segment_index,
                        size_t *segments_size, uint8 **new_size_array,
                        size_t *array_size);

  // ConverterInterface
  bool StartConversion(Segments *segments,
                       const string &key) const;
  bool StartConversionWithComposer(
      Segments *segments, const composer::Composer *composer) const;
  bool StartReverseConversion(Segments *segments,
                              const string &key) const;
  bool StartPrediction(Segments *segments,
                       const string &key) const;
  bool StartPredictionWithComposer(
      Segments *segments, const composer::Composer *composer) const;
  bool StartSuggestion(Segments *segments,
                       const string &key) const;
  bool StartSuggestionWithComposer(
      Segments *segments, const composer::Composer *composer) const;
  bool StartPartialPrediction(Segments *segments,
                              const string &key) const;
  bool StartPartialPredictionWithComposer(
      Segments *segments, const composer::Composer *composer) const;
  bool StartPartialSuggestion(Segments *segments,
                              const string &key) const;
  bool StartPartialSuggestionWithComposer(
      Segments *segments, const composer::Composer *composer) const;
  bool FinishConversion(Segments *segments) const;
  bool CancelConversion(Segments *segments) const;
  bool ResetConversion(Segments *segments) const;
  bool RevertConversion(Segments *segments) const;
  bool CommitSegmentValue(Segments *segments,
                          size_t segment_index,
                          int    candidate_index) const;
  bool CommitPartialSuggestionSegmentValue(
      Segments *segments,
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
  void SetUserDataManager(UserDataManagerInterface *user_data_manager);

 private:
  // mutable for recode input in const functions
  mutable ConverterInput startconversion_input_;
  mutable ConverterInput startconversionwithcomposer_input_;
  mutable ConverterInput startreverseconversion_input_;
  mutable ConverterInput startprediction_input_;
  mutable ConverterInput startpredictionwithcomposer_input_;
  mutable ConverterInput startsuggestion_input_;
  mutable ConverterInput startsuggestionwithcomposer_input_;
  mutable ConverterInput startpartialprediction_input_;
  mutable ConverterInput startpartialpredictionwithcomposer_input_;
  mutable ConverterInput startpartialsuggestion_input_;
  mutable ConverterInput startpartialsuggestionwithcomposer_input_;
  mutable ConverterInput finishconversion_input_;
  mutable ConverterInput cancelconversion_input_;
  mutable ConverterInput resetconversion_input_;
  mutable ConverterInput revertconversion_input_;
  mutable ConverterInput commitsegmentvalue_input_;
  mutable ConverterInput commitpartialsuggestionsegmentvalue_input_;
  mutable ConverterInput focussegmentvalue_input_;
  mutable ConverterInput freesegmentvalue_input_;
  mutable ConverterInput submitfirstsegment_input_;
  mutable ConverterInput resizesegment1_input_;
  mutable ConverterInput resizesegment2_input_;

  ConverterOutput startconversion_output_;
  ConverterOutput startconversionwithcomposer_output_;
  ConverterOutput startreverseconversion_output_;
  ConverterOutput startprediction_output_;
  ConverterOutput startpredictionwithcomposer_output_;
  ConverterOutput startsuggestion_output_;
  ConverterOutput startsuggestionwithcomposer_output_;
  ConverterOutput startpartialprediction_output_;
  ConverterOutput startpartialpredictionwithcomposer_output_;
  ConverterOutput startpartialsuggestion_output_;
  ConverterOutput startpartialsuggestionwithcomposer_output_;
  ConverterOutput finishconversion_output_;
  ConverterOutput cancelconversion_output_;
  ConverterOutput resetconversion_output_;
  ConverterOutput revertconversion_output_;
  ConverterOutput commitsegmentvalue_output_;
  ConverterOutput commitpartialsuggestionsegmentvalue_output_;
  ConverterOutput focussegmentvalue_output_;
  ConverterOutput freesegmentvalue_output_;
  ConverterOutput submitfirstsegment_output_;
  ConverterOutput resizesegment1_output_;
  ConverterOutput resizesegment2_output_;

  scoped_ptr<UserDataManagerInterface> user_data_manager_;
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_CONVERTER_MOCK_H_
