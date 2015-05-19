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

#include "converter/converter_mock.h"

#include <string>
#include "converter/segments.h"
#include "converter/user_data_manager_mock.h"

namespace mozc {

ConverterMock::ConverterMock()
    : user_data_manager_(new UserDataManagerMock) {
  LOG(INFO) << "ConverterMock is created";
}

ConverterMock::~ConverterMock() {}

void ConverterMock::SetStartConversionForRequest(Segments *segments,
                                                 bool result) {
  startconversionwithrequest_output_.initialized = true;
  startconversionwithrequest_output_.segments.CopyFrom(*segments);
  startconversionwithrequest_output_.return_value = result;
}

void ConverterMock::SetStartConversion(Segments *segments, bool result) {
  startconversion_output_.initialized = true;
  startconversion_output_.segments.CopyFrom(*segments);
  startconversion_output_.return_value = result;
}

void ConverterMock::SetStartReverseConversion(Segments *segments, bool result) {
  startreverseconversion_output_.initialized = true;
  startreverseconversion_output_.segments.CopyFrom(*segments);
  startreverseconversion_output_.return_value = result;
}

void ConverterMock::SetStartPredictionForRequest(Segments *segments,
                                                 bool result) {
  startpredictionwithrequest_output_.initialized = true;
  startpredictionwithrequest_output_.segments.CopyFrom(*segments);
  startpredictionwithrequest_output_.return_value = result;
}

void ConverterMock::SetStartPrediction(Segments *segments, bool result) {
  startprediction_output_.initialized = true;
  startprediction_output_.segments.CopyFrom(*segments);
  startprediction_output_.return_value = result;
}

void ConverterMock::SetStartSuggestionForRequest(Segments *segments,
                                                 bool result) {
  startsuggestionforrequest_output_.initialized = true;
  startsuggestionforrequest_output_.segments.CopyFrom(*segments);
  startsuggestionforrequest_output_.return_value = result;
}

void ConverterMock::SetStartSuggestion(Segments *segments, bool result) {
  startsuggestion_output_.initialized = true;
  startsuggestion_output_.segments.CopyFrom(*segments);
  startsuggestion_output_.return_value = result;
}

void ConverterMock::SetStartPartialPredictionForRequest(Segments *segments,
                                                        bool result) {
  startpartialpredictionforrequest_output_.initialized = true;
  startpartialpredictionforrequest_output_.segments.CopyFrom(*segments);
  startpartialpredictionforrequest_output_.return_value = result;
}

void ConverterMock::SetStartPartialPrediction(Segments *segments, bool result) {
  startpartialprediction_output_.initialized = true;
  startpartialprediction_output_.segments.CopyFrom(*segments);
  startpartialprediction_output_.return_value = result;
}

void ConverterMock::SetStartPartialSuggestionForRequest(Segments *segments,
                                                        bool result) {
  startpartialsuggestionforrequest_output_.initialized = true;
  startpartialsuggestionforrequest_output_.segments.CopyFrom(*segments);
  startpartialsuggestionforrequest_output_.return_value = result;
}

void ConverterMock::SetStartPartialSuggestion(Segments *segments, bool result) {
  startpartialsuggestion_output_.initialized = true;
  startpartialsuggestion_output_.segments.CopyFrom(*segments);
  startpartialsuggestion_output_.return_value = result;
}

void ConverterMock::SetFinishConversion(Segments *segments, bool result) {
  finishconversion_output_.initialized = true;
  finishconversion_output_.segments.CopyFrom(*segments);
  finishconversion_output_.return_value = result;
}

void ConverterMock::SetCancelConversion(Segments *segments, bool result) {
  cancelconversion_output_.initialized = true;
  cancelconversion_output_.segments.CopyFrom(*segments);
  cancelconversion_output_.return_value = result;
}

void ConverterMock::SetResetConversion(Segments *segments, bool result) {
  resetconversion_output_.initialized = true;
  resetconversion_output_.segments.CopyFrom(*segments);
  resetconversion_output_.return_value = result;
}

void ConverterMock::SetRevertConversion(Segments *segments, bool result) {
  revertconversion_output_.initialized = true;
  revertconversion_output_.segments.CopyFrom(*segments);
  revertconversion_output_.return_value = result;
}

void ConverterMock::SetCommitSegmentValue(Segments *segments, bool result) {
  commitsegmentvalue_output_.initialized = true;
  commitsegmentvalue_output_.segments.CopyFrom(*segments);
  commitsegmentvalue_output_.return_value = result;
}

void ConverterMock::SetCommitPartialSuggestionSegmentValue(
    Segments *segments, bool result) {
  commitpartialsuggestionsegmentvalue_output_.initialized = true;
  commitpartialsuggestionsegmentvalue_output_.segments.CopyFrom(*segments);
  commitpartialsuggestionsegmentvalue_output_.return_value = result;
}

void ConverterMock::SetFocusSegmentValue(Segments *segments, bool result) {
  focussegmentvalue_output_.initialized = true;
  focussegmentvalue_output_.segments.CopyFrom(*segments);
  focussegmentvalue_output_.return_value = result;
}

void ConverterMock::SetFreeSegmentValue(Segments *segments, bool result) {
  freesegmentvalue_output_.initialized = true;
  freesegmentvalue_output_.segments.CopyFrom(*segments);
  freesegmentvalue_output_.return_value = result;
}

void ConverterMock::SetSubmitFirstSegment(Segments *segments, bool result) {
  submitfirstsegment_output_.initialized = true;
  submitfirstsegment_output_.segments.CopyFrom(*segments);
  submitfirstsegment_output_.return_value = result;
}

void ConverterMock::SetResizeSegment1(Segments *segments, bool result) {
  resizesegment1_output_.initialized = true;
  resizesegment1_output_.segments.CopyFrom(*segments);
  resizesegment1_output_.return_value = result;
}

void ConverterMock::SetResizeSegment2(Segments *segments, bool result) {
  resizesegment2_output_.initialized = true;
  resizesegment2_output_.segments.CopyFrom(*segments);
  resizesegment2_output_.return_value = result;
}

void ConverterMock::GetStartConversionForRequest(
    Segments *segments, ConversionRequest *request) {
  segments->CopyFrom(startconversionwithrequest_input_.segments);
  *request = startconversionwithrequest_input_.request;
}

void ConverterMock::GetStartConversion(Segments *segments, string *key) {
  segments->CopyFrom(startconversion_input_.segments);
  *key = startconversion_input_.key;
}

void ConverterMock::GetStartReverseConversion(Segments *segments,
                                              string *key) {
  segments->CopyFrom(startreverseconversion_input_.segments);
  *key = startreverseconversion_input_.key;
}

void ConverterMock::GetStartPredictionForRequest(
    Segments *segments, ConversionRequest *request) {
  segments->CopyFrom(startpredictionwithrequest_input_.segments);
  *request = startpredictionwithrequest_input_.request;
}

void ConverterMock::GetStartPrediction(Segments *segments, string *key) {
  segments->CopyFrom(startprediction_input_.segments);
  *key = startprediction_input_.key;
}

void ConverterMock::GetStartSuggestionForRequest(
    Segments *segments, ConversionRequest *request) {
  segments->CopyFrom(startsuggestionforrequest_input_.segments);
  *request = startsuggestionforrequest_input_.request;
}

void ConverterMock::GetStartSuggestion(Segments *segments, string *key) {
  segments->CopyFrom(startsuggestion_input_.segments);
  *key = startsuggestion_input_.key;
}

void ConverterMock::GetStartPartialPredictionForRequest(
    Segments *segments, ConversionRequest *request) {
  segments->CopyFrom(startpartialpredictionforrequest_input_.segments);
  *request = startpartialpredictionforrequest_input_.request;
}

void ConverterMock::GetStartPartialPrediction(Segments *segments, string *key) {
  segments->CopyFrom(startpartialprediction_input_.segments);
  *key = startpartialprediction_input_.key;
}

void ConverterMock::GetStartPartialSuggestion(Segments *segments, string *key) {
  segments->CopyFrom(startpartialsuggestion_input_.segments);
  *key = startpartialsuggestion_input_.key;
}

void ConverterMock::GetFinishConversion(Segments *segments) {
  segments->CopyFrom(finishconversion_input_.segments);
}

void ConverterMock::GetCancelConversion(Segments *segments) {
  segments->CopyFrom(cancelconversion_input_.segments);
}

void ConverterMock::GetResetConversion(Segments *segments) {
  segments->CopyFrom(resetconversion_input_.segments);
}

void ConverterMock::GetRevertConversion(Segments *segments) {
  segments->CopyFrom(revertconversion_input_.segments);
}

void ConverterMock::GetCommitSegmentValue(Segments *segments,
                                           size_t *segment_index,
                                           int *candidate_index) {
  segments->CopyFrom(commitsegmentvalue_input_.segments);
  *segment_index = commitsegmentvalue_input_.segment_index;
  *candidate_index = commitsegmentvalue_input_.candidate_index;
}

void ConverterMock::GetCommitPartialSuggestionSegmentValue(
    Segments *segments, size_t *segment_index, int *candidate_index,
    string *current_segment_key, string *new_segment_key) {
  segments->CopyFrom(commitpartialsuggestionsegmentvalue_input_.segments);
  *segment_index = commitpartialsuggestionsegmentvalue_input_.segment_index;
  *candidate_index = commitpartialsuggestionsegmentvalue_input_.candidate_index;
  *current_segment_key =
      commitpartialsuggestionsegmentvalue_input_.current_segment_key;
  *new_segment_key = commitpartialsuggestionsegmentvalue_input_.new_segment_key;
}

void ConverterMock::GetFocusSegmentValue(Segments *segments,
                                          size_t *segment_index,
                                          int *candidate_index) {
  segments->CopyFrom(focussegmentvalue_input_.segments);
  *segment_index = focussegmentvalue_input_.segment_index;
  *candidate_index = focussegmentvalue_input_.candidate_index;
}

void ConverterMock::GetFreeSegmentValue(Segments *segments,
                                         size_t *segment_index) {
  segments->CopyFrom(freesegmentvalue_input_.segments);
  *segment_index = freesegmentvalue_input_.segment_index;
}

void ConverterMock::GetSubmitFirstSegment(Segments *segments,
                                          size_t *candidate_index) {
  segments->CopyFrom(submitfirstsegment_input_.segments);
  *candidate_index = submitfirstsegment_input_.candidate_index;
}

void ConverterMock::GetResizeSegment1(Segments *segments, size_t *segment_index,
                                     int *offset_length) {
  segments->CopyFrom(resizesegment1_input_.segments);
  *segment_index = resizesegment1_input_.segment_index;
  *offset_length = resizesegment1_input_.offset_length;
}

void ConverterMock::GetResizeSegment2(Segments *segments,
                                     size_t *start_segment_index,
                                     size_t *segments_size,
                                     uint8 **new_size_array,
                                     size_t *array_size) {
  segments->CopyFrom(resizesegment2_input_.segments);
  *start_segment_index = resizesegment2_input_.start_segment_index;
  *segments_size = resizesegment2_input_.segments_size;
  *new_size_array = &resizesegment2_input_.new_size_array[0];
  *array_size = resizesegment2_input_.new_size_array.size();
}

bool ConverterMock::StartConversionForRequest(const ConversionRequest &request,
                                              Segments *segments) const {
  VLOG(2) << "mock function: StartConversion with ConversionRequest";
  startconversionwithrequest_input_.segments.CopyFrom(*segments);
  startconversionwithrequest_input_.request = request;

  if (!startconversionwithrequest_output_.initialized) {
    return false;
  } else {
    segments->CopyFrom(startconversionwithrequest_output_.segments);
    return startconversionwithrequest_output_.return_value;
  }
}

bool ConverterMock::StartConversion(Segments *segments,
                                    const string &key) const {
  VLOG(2) << "mock function: StartConversion";
  startconversion_input_.segments.CopyFrom(*segments);
  startconversion_input_.key = key;

  if (!startconversion_output_.initialized) {
    return false;
  } else {
    segments->CopyFrom(startconversion_output_.segments);
    return startconversion_output_.return_value;
  }
}

bool ConverterMock::StartReverseConversion(Segments *segments,
                                           const string &key) const {
  VLOG(2) << "mock function: StartReverseConversion";
  startreverseconversion_input_.segments.CopyFrom(*segments);
  startreverseconversion_input_.key = key;

  if (!startreverseconversion_output_.initialized) {
    return false;
  } else {
    segments->CopyFrom(startreverseconversion_output_.segments);
    return startreverseconversion_output_.return_value;
  }
  return false;
}

bool ConverterMock::StartPredictionForRequest(const ConversionRequest &request,
                                              Segments *segments) const {
  VLOG(2) << "mock function: StartPredictionForRequest";
  startpredictionwithrequest_input_.segments.CopyFrom(*segments);
  startpredictionwithrequest_input_.request = request;

  if (!startpredictionwithrequest_output_.initialized) {
    return false;
  } else {
    segments->CopyFrom(startpredictionwithrequest_output_.segments);
    return startpredictionwithrequest_output_.return_value;
  }
}

bool ConverterMock::StartPrediction(Segments *segments,
                                    const string &key) const {
  VLOG(2) << "mock function: StartPrediction";
  startprediction_input_.segments.CopyFrom(*segments);
  startprediction_input_.key = key;

  if (!startprediction_output_.initialized) {
    return false;
  } else {
    segments->CopyFrom(startprediction_output_.segments);
    return startprediction_output_.return_value;
  }
}

bool ConverterMock::StartSuggestionForRequest(const ConversionRequest &request,
                                              Segments *segments) const {
  VLOG(2) << "mock function: StartSuggestionForRequest";
  startsuggestionforrequest_input_.segments.CopyFrom(*segments);
  startsuggestionforrequest_input_.request = request;

  if (!startsuggestionforrequest_output_.initialized) {
    return false;
  } else {
    segments->CopyFrom(startsuggestionforrequest_output_.segments);
    return startsuggestionforrequest_output_.return_value;
  }
}

bool ConverterMock::StartSuggestion(Segments *segments,
                                    const string &key) const {
  VLOG(2) << "mock function: StartSuggestion";
  startsuggestion_input_.segments.CopyFrom(*segments);
  startsuggestion_input_.key = key;

  if (!startsuggestion_output_.initialized) {
    return false;
  } else {
    segments->CopyFrom(startsuggestion_output_.segments);
    return startsuggestion_output_.return_value;
  }
}

bool ConverterMock::StartPartialPredictionForRequest(
    const ConversionRequest &request, Segments *segments) const {
  VLOG(2) << "mock function: StartPartialPredictionForRequest";
  startpartialpredictionforrequest_input_.segments.CopyFrom(*segments);
  startpartialpredictionforrequest_input_.request = request;

  if (!startpartialpredictionforrequest_output_.initialized) {
    return false;
  } else {
    segments->CopyFrom(startpartialpredictionforrequest_output_.segments);
    return startpartialpredictionforrequest_output_.return_value;
  }
}

bool ConverterMock::StartPartialPrediction(Segments *segments,
                                           const string &key) const {
  VLOG(2) << "mock function: StartParialPrediction";
  startpartialprediction_input_.segments.CopyFrom(*segments);
  startpartialprediction_input_.key = key;

  if (!startpartialprediction_output_.initialized) {
    return false;
  } else {
    segments->CopyFrom(startpartialprediction_output_.segments);
    return startpartialprediction_output_.return_value;
  }
}

bool ConverterMock::StartPartialSuggestionForRequest(
    const ConversionRequest &request, Segments *segments) const {
  VLOG(2) << "mock function: StartPartialSuggestionForRequest";
  startpartialsuggestionforrequest_input_.segments.CopyFrom(*segments);
  startpartialsuggestionforrequest_input_.request = request;

  if (!startpartialsuggestionforrequest_output_.initialized) {
    return false;
  } else {
    segments->CopyFrom(startpartialsuggestionforrequest_output_.segments);
    return startpartialsuggestionforrequest_output_.return_value;
  }
}

bool ConverterMock::StartPartialSuggestion(Segments *segments,
                                           const string &key) const {
  VLOG(2) << "mock function: StartParialSuggestion";
  startpartialsuggestion_input_.segments.CopyFrom(*segments);
  startpartialsuggestion_input_.key = key;

  if (!startpartialsuggestion_output_.initialized) {
    return false;
  } else {
    segments->CopyFrom(startpartialsuggestion_output_.segments);
    return startpartialsuggestion_output_.return_value;
  }
}

bool ConverterMock::FinishConversion(Segments *segments) const {
  VLOG(2) << "mock function: FinishConversion";
  finishconversion_input_.segments.CopyFrom(*segments);

  if (!finishconversion_output_.initialized) {
    return false;
  } else {
    segments->CopyFrom(finishconversion_output_.segments);
    return finishconversion_output_.return_value;
  }
}

bool ConverterMock::CancelConversion(Segments *segments) const {
  VLOG(2) << "mock function: CancelConversion";
  cancelconversion_input_.segments.CopyFrom(*segments);

  if (!cancelconversion_output_.initialized) {
    return false;
  } else {
    segments->CopyFrom(cancelconversion_output_.segments);
    return cancelconversion_output_.return_value;
  }
}

bool ConverterMock::ResetConversion(Segments *segments) const {
  VLOG(2) << "mock function: ResetConversion";
  resetconversion_input_.segments.CopyFrom(*segments);

  if (!resetconversion_output_.initialized) {
    return false;
  } else {
    segments->CopyFrom(resetconversion_output_.segments);
    return resetconversion_output_.return_value;
  }
}

bool ConverterMock::RevertConversion(Segments *segments) const {
  VLOG(2) << "mock function: RevertConversion";
  revertconversion_input_.segments.CopyFrom(*segments);

  if (!revertconversion_output_.initialized) {
    return false;
  } else {
    segments->CopyFrom(revertconversion_output_.segments);
    return revertconversion_output_.return_value;
  }
}

bool ConverterMock::CommitSegmentValue(Segments *segments,
                                       size_t segment_index,
                                       int candidate_index) const {
  VLOG(2) << "mock function: CommitSegmentValue";
  commitsegmentvalue_input_.segments.CopyFrom(*segments);
  commitsegmentvalue_input_.segment_index = segment_index;
  commitsegmentvalue_input_.candidate_index = candidate_index;

  if (!commitsegmentvalue_output_.initialized) {
    return false;
  } else {
    segments->CopyFrom(commitsegmentvalue_output_.segments);
    return commitsegmentvalue_output_.return_value;
  }
}

bool ConverterMock::CommitPartialSuggestionSegmentValue(
    Segments *segments,
    size_t segment_index,
    int candidate_index,
    const string &current_segment_key,
    const string &new_segment_key) const {
  VLOG(2) << "mock function: CommitPartialSuggestionSegmentValue";
  commitpartialsuggestionsegmentvalue_input_.segments.CopyFrom(*segments);
  commitpartialsuggestionsegmentvalue_input_.segment_index = segment_index;
  commitpartialsuggestionsegmentvalue_input_.candidate_index = candidate_index;
  commitpartialsuggestionsegmentvalue_input_.current_segment_key =
      current_segment_key;
  commitpartialsuggestionsegmentvalue_input_.new_segment_key = new_segment_key;

  if (!commitpartialsuggestionsegmentvalue_output_.initialized) {
    return false;
  } else {
    segments->CopyFrom(commitpartialsuggestionsegmentvalue_output_.segments);
    return commitpartialsuggestionsegmentvalue_output_.return_value;
  }
}

bool ConverterMock::FocusSegmentValue(Segments *segments,
                                      size_t segment_index,
                                      int    candidate_index) const {
  VLOG(2) << "mock function: FocusSegmentValue";
  focussegmentvalue_input_.segments.CopyFrom(*segments);
  focussegmentvalue_input_.segment_index = segment_index;
  focussegmentvalue_input_.candidate_index = candidate_index;

  if (!focussegmentvalue_output_.initialized) {
    return false;
  } else {
    segments->CopyFrom(focussegmentvalue_output_.segments);
    return focussegmentvalue_output_.return_value;
  }
}


bool ConverterMock::FreeSegmentValue(Segments *segments,
                                     size_t segment_index) const {
  VLOG(2) << "mock function: FreeSegmentValue";
  freesegmentvalue_input_.segments.CopyFrom(*segments);
  freesegmentvalue_input_.segment_index = segment_index;

  if (!freesegmentvalue_output_.initialized) {
    return false;
  } else {
    segments->CopyFrom(freesegmentvalue_output_.segments);
    return freesegmentvalue_output_.return_value;
  }
}

bool ConverterMock::SubmitFirstSegment(Segments *segments,
                                       size_t candidate_index) const {
  VLOG(2) << "mock function: SubmitFirstSegment";
  submitfirstsegment_input_.segments.CopyFrom(*segments);
  submitfirstsegment_input_.candidate_index = candidate_index;

  if (!submitfirstsegment_output_.initialized) {
    return false;
  } else {
    segments->CopyFrom(submitfirstsegment_output_.segments);
    return submitfirstsegment_output_.return_value;
  }
}

bool ConverterMock::ResizeSegment(Segments *segments,
                                  const ConversionRequest &request,
                                  size_t segment_index,
                                  int offset_length) const {
  VLOG(2) << "mock function: ResizeSegment";
  resizesegment1_input_.segments.CopyFrom(*segments);
  resizesegment1_input_.segment_index = segment_index;
  resizesegment1_input_.offset_length = offset_length;

  if (!resizesegment1_output_.initialized) {
    return false;
  } else {
    segments->CopyFrom(resizesegment1_output_.segments);
    return resizesegment1_output_.return_value;
  }
}

bool ConverterMock::ResizeSegment(Segments *segments,
                                  const ConversionRequest &request,
                                  size_t start_segment_index,
                                  size_t segments_size,
                                  const uint8 *new_size_array,
                                  size_t array_size) const {
  VLOG(2) << "mock function: ResizeSegmnet";
  resizesegment2_input_.segments.CopyFrom(*segments);
  resizesegment2_input_.start_segment_index = start_segment_index;
  resizesegment2_input_.segments_size = segments_size;
  vector<uint8> size_array(new_size_array,
                           new_size_array + array_size);
  resizesegment2_input_.new_size_array = size_array;

  if (!resizesegment2_output_.initialized) {
    return false;
  } else {
    segments->CopyFrom(resizesegment2_output_.segments);
    return resizesegment2_output_.return_value;
  }
}

UserDataManagerInterface *ConverterMock::GetUserDataManager() {
  return user_data_manager_.get();
}

void ConverterMock::SetUserDataManager(
    UserDataManagerInterface *user_data_manager) {
  user_data_manager_.reset(user_data_manager);
}
}  // namespace mozc
