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

#include "converter/converter_mock.h"

#include <cstdint>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/port.h"
#include "converter/segments.h"
#include "request/conversion_request.h"

namespace mozc {

ConverterMock::ConverterMock() { LOG(INFO) << "ConverterMock is created"; }

ConverterMock::~ConverterMock() {}

void ConverterMock::SetStartConversionForRequest(Segments *segments,
                                                 bool result) {
  startconversionwithrequest_output_.initialized = true;
  startconversionwithrequest_output_.segments = *segments;
  startconversionwithrequest_output_.return_value = result;
}

void ConverterMock::SetStartConversion(Segments *segments, bool result) {
  startconversion_output_.initialized = true;
  startconversion_output_.segments = *segments;
  startconversion_output_.return_value = result;
}

void ConverterMock::SetStartReverseConversion(Segments *segments, bool result) {
  startreverseconversion_output_.initialized = true;
  startreverseconversion_output_.segments = *segments;
  startreverseconversion_output_.return_value = result;
}

void ConverterMock::SetStartPredictionForRequest(Segments *segments,
                                                 bool result) {
  startpredictionwithrequest_output_.initialized = true;
  startpredictionwithrequest_output_.segments = *segments;
  startpredictionwithrequest_output_.return_value = result;
}

void ConverterMock::SetStartPrediction(Segments *segments, bool result) {
  startprediction_output_.initialized = true;
  startprediction_output_.segments = *segments;
  startprediction_output_.return_value = result;
}

void ConverterMock::SetStartSuggestionForRequest(Segments *segments,
                                                 bool result) {
  startsuggestionforrequest_output_.initialized = true;
  startsuggestionforrequest_output_.segments = *segments;
  startsuggestionforrequest_output_.return_value = result;
}

void ConverterMock::SetStartSuggestion(Segments *segments, bool result) {
  startsuggestion_output_.initialized = true;
  startsuggestion_output_.segments = *segments;
  startsuggestion_output_.return_value = result;
}

void ConverterMock::SetStartPartialPredictionForRequest(Segments *segments,
                                                        bool result) {
  startpartialpredictionforrequest_output_.initialized = true;
  startpartialpredictionforrequest_output_.segments = *segments;
  startpartialpredictionforrequest_output_.return_value = result;
}

void ConverterMock::SetStartPartialPrediction(Segments *segments, bool result) {
  startpartialprediction_output_.initialized = true;
  startpartialprediction_output_.segments = *segments;
  startpartialprediction_output_.return_value = result;
}

void ConverterMock::SetStartPartialSuggestionForRequest(Segments *segments,
                                                        bool result) {
  startpartialsuggestionforrequest_output_.initialized = true;
  startpartialsuggestionforrequest_output_.segments = *segments;
  startpartialsuggestionforrequest_output_.return_value = result;
}

void ConverterMock::SetStartPartialSuggestion(Segments *segments, bool result) {
  startpartialsuggestion_output_.initialized = true;
  startpartialsuggestion_output_.segments = *segments;
  startpartialsuggestion_output_.return_value = result;
}

void ConverterMock::SetFinishConversion(Segments *segments, bool result) {
  finishconversion_output_.initialized = true;
  finishconversion_output_.segments = *segments;
  finishconversion_output_.return_value = result;
}

void ConverterMock::SetCancelConversion(Segments *segments, bool result) {
  cancelconversion_output_.initialized = true;
  cancelconversion_output_.segments = *segments;
  cancelconversion_output_.return_value = result;
}

void ConverterMock::SetResetConversion(Segments *segments, bool result) {
  resetconversion_output_.initialized = true;
  resetconversion_output_.segments = *segments;
  resetconversion_output_.return_value = result;
}

void ConverterMock::SetRevertConversion(Segments *segments, bool result) {
  revertconversion_output_.initialized = true;
  revertconversion_output_.segments = *segments;
  revertconversion_output_.return_value = result;
}

void ConverterMock::SetReconstructHistory(Segments *segments, bool result) {
  reconstructhistory_output_.initialized = true;
  reconstructhistory_output_.segments = *segments;
  reconstructhistory_output_.return_value = result;
}

void ConverterMock::SetCommitSegmentValue(Segments *segments, bool result) {
  commitsegmentvalue_output_.initialized = true;
  commitsegmentvalue_output_.segments = *segments;
  commitsegmentvalue_output_.return_value = result;
}

void ConverterMock::SetCommitPartialSuggestionSegmentValue(Segments *segments,
                                                           bool result) {
  commitpartialsuggestionsegmentvalue_output_.initialized = true;
  commitpartialsuggestionsegmentvalue_output_.segments = *segments;
  commitpartialsuggestionsegmentvalue_output_.return_value = result;
}

void ConverterMock::SetFocusSegmentValue(Segments *segments, bool result) {
  focussegmentvalue_output_.initialized = true;
  focussegmentvalue_output_.segments = *segments;
  focussegmentvalue_output_.return_value = result;
}

void ConverterMock::SetCommitSegments(Segments *segments, bool result) {
  submitsegments_output_.initialized = true;
  submitsegments_output_.segments = *segments;
  submitsegments_output_.return_value = result;
}

void ConverterMock::SetResizeSegment1(Segments *segments, bool result) {
  resizesegment1_output_.initialized = true;
  resizesegment1_output_.segments = *segments;
  resizesegment1_output_.return_value = result;
}

void ConverterMock::SetResizeSegment2(Segments *segments, bool result) {
  resizesegment2_output_.initialized = true;
  resizesegment2_output_.segments = *segments;
  resizesegment2_output_.return_value = result;
}

void ConverterMock::GetStartConversionForRequest(Segments *segments,
                                                 ConversionRequest *request) {
  *segments = startconversionwithrequest_input_.segments;
  *request = startconversionwithrequest_input_.request;
}

void ConverterMock::GetStartConversion(Segments *segments, std::string *key) {
  *segments = startconversion_input_.segments;
  *key = startconversion_input_.key;
}

void ConverterMock::GetStartReverseConversion(Segments *segments,
                                              std::string *key) {
  *segments = startreverseconversion_input_.segments;
  *key = startreverseconversion_input_.key;
}

void ConverterMock::GetStartPredictionForRequest(Segments *segments,
                                                 ConversionRequest *request) {
  *segments = startpredictionwithrequest_input_.segments;
  *request = startpredictionwithrequest_input_.request;
}

void ConverterMock::GetStartPrediction(Segments *segments, std::string *key) {
  *segments = startprediction_input_.segments;
  *key = startprediction_input_.key;
}

void ConverterMock::GetStartSuggestionForRequest(Segments *segments,
                                                 ConversionRequest *request) {
  *segments = startsuggestionforrequest_input_.segments;
  *request = startsuggestionforrequest_input_.request;
}

void ConverterMock::GetStartSuggestion(Segments *segments, std::string *key) {
  *segments = startsuggestion_input_.segments;
  *key = startsuggestion_input_.key;
}

void ConverterMock::GetStartPartialPredictionForRequest(
    Segments *segments, ConversionRequest *request) {
  *segments = startpartialpredictionforrequest_input_.segments;
  *request = startpartialpredictionforrequest_input_.request;
}

void ConverterMock::GetStartPartialPrediction(Segments *segments,
                                              std::string *key) {
  *segments = startpartialprediction_input_.segments;
  *key = startpartialprediction_input_.key;
}

void ConverterMock::GetStartPartialSuggestion(Segments *segments,
                                              std::string *key) {
  *segments = startpartialsuggestion_input_.segments;
  *key = startpartialsuggestion_input_.key;
}

void ConverterMock::GetFinishConversion(Segments *segments) {
  *segments = finishconversion_input_.segments;
}

void ConverterMock::GetCancelConversion(Segments *segments) {
  *segments = cancelconversion_input_.segments;
}

void ConverterMock::GetResetConversion(Segments *segments) {
  *segments = resetconversion_input_.segments;
}

void ConverterMock::GetRevertConversion(Segments *segments) {
  *segments = revertconversion_input_.segments;
}

void ConverterMock::GetReconstructHistory(Segments *segments) {
  *segments = reconstructhistory_input_.segments;
}

void ConverterMock::GetCommitSegmentValue(Segments *segments,
                                          size_t *segment_index,
                                          int *candidate_index) {
  *segments = commitsegmentvalue_input_.segments;
  *segment_index = commitsegmentvalue_input_.segment_index;
  *candidate_index = commitsegmentvalue_input_.candidate_index;
}

void ConverterMock::GetCommitPartialSuggestionSegmentValue(
    Segments *segments, size_t *segment_index, int *candidate_index,
    std::string *current_segment_key, std::string *new_segment_key) {
  *segments = commitpartialsuggestionsegmentvalue_input_.segments;
  *segment_index = commitpartialsuggestionsegmentvalue_input_.segment_index;
  *candidate_index = commitpartialsuggestionsegmentvalue_input_.candidate_index;
  *current_segment_key =
      commitpartialsuggestionsegmentvalue_input_.current_segment_key;
  *new_segment_key = commitpartialsuggestionsegmentvalue_input_.new_segment_key;
}

void ConverterMock::GetFocusSegmentValue(Segments *segments,
                                         size_t *segment_index,
                                         int *candidate_index) {
  *segments = focussegmentvalue_input_.segments;
  *segment_index = focussegmentvalue_input_.segment_index;
  *candidate_index = focussegmentvalue_input_.candidate_index;
}

void ConverterMock::GetCommitSegments(Segments *segments,
                                      std::vector<size_t> *candidate_index) {
  *segments = submitsegments_input_.segments;
  *candidate_index = submitsegments_input_.candidate_index_list;
}

void ConverterMock::GetResizeSegment1(Segments *segments, size_t *segment_index,
                                      int *offset_length) {
  *segments = resizesegment1_input_.segments;
  *segment_index = resizesegment1_input_.segment_index;
  *offset_length = resizesegment1_input_.offset_length;
}

void ConverterMock::GetResizeSegment2(Segments *segments,
                                      size_t *start_segment_index,
                                      size_t *segments_size,
                                      uint8_t **new_size_array,
                                      size_t *array_size) {
  *segments = resizesegment2_input_.segments;
  *start_segment_index = resizesegment2_input_.start_segment_index;
  *segments_size = resizesegment2_input_.segments_size;
  *new_size_array = &resizesegment2_input_.new_size_array[0];
  *array_size = resizesegment2_input_.new_size_array.size();
}

bool ConverterMock::StartConversionForRequest(const ConversionRequest &request,
                                              Segments *segments) const {
  VLOG(2) << "mock function: StartConversion with ConversionRequest";
  startconversionwithrequest_input_.segments = *segments;
  startconversionwithrequest_input_.request = request;

  if (!startconversionwithrequest_output_.initialized) {
    return false;
  } else {
    *segments = startconversionwithrequest_output_.segments;
    return startconversionwithrequest_output_.return_value;
  }
}

bool ConverterMock::StartConversion(Segments *segments,
                                    const std::string &key) const {
  VLOG(2) << "mock function: StartConversion";
  startconversion_input_.segments = *segments;
  startconversion_input_.key = key;

  if (!startconversion_output_.initialized) {
    return false;
  } else {
    *segments = startconversion_output_.segments;
    return startconversion_output_.return_value;
  }
}

bool ConverterMock::StartReverseConversion(Segments *segments,
                                           const std::string &key) const {
  VLOG(2) << "mock function: StartReverseConversion";
  startreverseconversion_input_.segments = *segments;
  startreverseconversion_input_.key = key;

  if (!startreverseconversion_output_.initialized) {
    return false;
  } else {
    *segments = startreverseconversion_output_.segments;
    return startreverseconversion_output_.return_value;
  }
  return false;
}

bool ConverterMock::StartPredictionForRequest(const ConversionRequest &request,
                                              Segments *segments) const {
  VLOG(2) << "mock function: StartPredictionForRequest";
  startpredictionwithrequest_input_.segments = *segments;
  startpredictionwithrequest_input_.request = request;

  if (!startpredictionwithrequest_output_.initialized) {
    return false;
  } else {
    *segments = startpredictionwithrequest_output_.segments;
    return startpredictionwithrequest_output_.return_value;
  }
}

bool ConverterMock::StartPrediction(Segments *segments,
                                    const std::string &key) const {
  VLOG(2) << "mock function: StartPrediction";
  startprediction_input_.segments = *segments;
  startprediction_input_.key = key;

  if (!startprediction_output_.initialized) {
    return false;
  } else {
    *segments = startprediction_output_.segments;
    return startprediction_output_.return_value;
  }
}

bool ConverterMock::StartSuggestionForRequest(const ConversionRequest &request,
                                              Segments *segments) const {
  VLOG(2) << "mock function: StartSuggestionForRequest";
  startsuggestionforrequest_input_.segments = *segments;
  startsuggestionforrequest_input_.request = request;

  if (!startsuggestionforrequest_output_.initialized) {
    return false;
  } else {
    *segments = startsuggestionforrequest_output_.segments;
    return startsuggestionforrequest_output_.return_value;
  }
}

bool ConverterMock::StartSuggestion(Segments *segments,
                                    const std::string &key) const {
  VLOG(2) << "mock function: StartSuggestion";
  startsuggestion_input_.segments = *segments;
  startsuggestion_input_.key = key;

  if (!startsuggestion_output_.initialized) {
    return false;
  } else {
    *segments = startsuggestion_output_.segments;
    return startsuggestion_output_.return_value;
  }
}

bool ConverterMock::StartPartialPredictionForRequest(
    const ConversionRequest &request, Segments *segments) const {
  VLOG(2) << "mock function: StartPartialPredictionForRequest";
  startpartialpredictionforrequest_input_.segments = *segments;
  startpartialpredictionforrequest_input_.request = request;

  if (!startpartialpredictionforrequest_output_.initialized) {
    return false;
  } else {
    *segments = startpartialpredictionforrequest_output_.segments;
    return startpartialpredictionforrequest_output_.return_value;
  }
}

bool ConverterMock::StartPartialPrediction(Segments *segments,
                                           const std::string &key) const {
  VLOG(2) << "mock function: StartParialPrediction";
  startpartialprediction_input_.segments = *segments;
  startpartialprediction_input_.key = key;

  if (!startpartialprediction_output_.initialized) {
    return false;
  } else {
    *segments = startpartialprediction_output_.segments;
    return startpartialprediction_output_.return_value;
  }
}

bool ConverterMock::StartPartialSuggestionForRequest(
    const ConversionRequest &request, Segments *segments) const {
  VLOG(2) << "mock function: StartPartialSuggestionForRequest";
  startpartialsuggestionforrequest_input_.segments = *segments;
  startpartialsuggestionforrequest_input_.request = request;

  if (!startpartialsuggestionforrequest_output_.initialized) {
    return false;
  } else {
    *segments = startpartialsuggestionforrequest_output_.segments;
    return startpartialsuggestionforrequest_output_.return_value;
  }
}

bool ConverterMock::StartPartialSuggestion(Segments *segments,
                                           const std::string &key) const {
  VLOG(2) << "mock function: StartParialSuggestion";
  startpartialsuggestion_input_.segments = *segments;
  startpartialsuggestion_input_.key = key;

  if (!startpartialsuggestion_output_.initialized) {
    return false;
  } else {
    *segments = startpartialsuggestion_output_.segments;
    return startpartialsuggestion_output_.return_value;
  }
}

bool ConverterMock::FinishConversion(const ConversionRequest &request,
                                     Segments *segments) const {
  VLOG(2) << "mock function: FinishConversion";
  finishconversion_input_.segments = *segments;

  if (!finishconversion_output_.initialized) {
    return false;
  } else {
    *segments = finishconversion_output_.segments;
    return finishconversion_output_.return_value;
  }
}

bool ConverterMock::CancelConversion(Segments *segments) const {
  VLOG(2) << "mock function: CancelConversion";
  cancelconversion_input_.segments = *segments;

  if (!cancelconversion_output_.initialized) {
    return false;
  } else {
    *segments = cancelconversion_output_.segments;
    return cancelconversion_output_.return_value;
  }
}

bool ConverterMock::ResetConversion(Segments *segments) const {
  VLOG(2) << "mock function: ResetConversion";
  resetconversion_input_.segments = *segments;

  if (!resetconversion_output_.initialized) {
    return false;
  } else {
    *segments = resetconversion_output_.segments;
    return resetconversion_output_.return_value;
  }
}

bool ConverterMock::RevertConversion(Segments *segments) const {
  VLOG(2) << "mock function: RevertConversion";
  revertconversion_input_.segments = *segments;

  if (!revertconversion_output_.initialized) {
    return false;
  } else {
    *segments = revertconversion_output_.segments;
    return revertconversion_output_.return_value;
  }
}

bool ConverterMock::ReconstructHistory(
    Segments *segments, const std::string &preceding_text) const {
  VLOG(2) << "mock function: ReconstructHistory";
  reconstructhistory_input_.segments = *segments;

  if (!reconstructhistory_output_.initialized) {
    return false;
  } else {
    *segments = reconstructhistory_output_.segments;
    return reconstructhistory_output_.return_value;
  }
}

bool ConverterMock::CommitSegmentValue(Segments *segments, size_t segment_index,
                                       int candidate_index) const {
  VLOG(2) << "mock function: CommitSegmentValue";
  commitsegmentvalue_input_.segments = *segments;
  commitsegmentvalue_input_.segment_index = segment_index;
  commitsegmentvalue_input_.candidate_index = candidate_index;

  if (!commitsegmentvalue_output_.initialized) {
    return false;
  } else {
    *segments = commitsegmentvalue_output_.segments;
    return commitsegmentvalue_output_.return_value;
  }
}

bool ConverterMock::CommitPartialSuggestionSegmentValue(
    Segments *segments, size_t segment_index, int candidate_index,
    absl::string_view current_segment_key,
    absl::string_view new_segment_key) const {
  VLOG(2) << "mock function: CommitPartialSuggestionSegmentValue";
  commitpartialsuggestionsegmentvalue_input_.segments = *segments;
  commitpartialsuggestionsegmentvalue_input_.segment_index = segment_index;
  commitpartialsuggestionsegmentvalue_input_.candidate_index = candidate_index;
  commitpartialsuggestionsegmentvalue_input_.current_segment_key =
      std::string(current_segment_key);
  commitpartialsuggestionsegmentvalue_input_.new_segment_key =
      std::string(new_segment_key);

  if (!commitpartialsuggestionsegmentvalue_output_.initialized) {
    return false;
  } else {
    *segments = commitpartialsuggestionsegmentvalue_output_.segments;
    return commitpartialsuggestionsegmentvalue_output_.return_value;
  }
}

bool ConverterMock::FocusSegmentValue(Segments *segments, size_t segment_index,
                                      int candidate_index) const {
  VLOG(2) << "mock function: FocusSegmentValue";
  focussegmentvalue_input_.segments = *segments;
  focussegmentvalue_input_.segment_index = segment_index;
  focussegmentvalue_input_.candidate_index = candidate_index;

  if (!focussegmentvalue_output_.initialized) {
    return false;
  } else {
    *segments = focussegmentvalue_output_.segments;
    return focussegmentvalue_output_.return_value;
  }
}

bool ConverterMock::CommitSegments(
    Segments *segments, const std::vector<size_t> &candidate_index) const {
  VLOG(2) << "mock function: CommitSegments";
  submitsegments_input_.segments = *segments;
  submitsegments_input_.candidate_index_list = candidate_index;

  if (!submitsegments_output_.initialized) {
    return false;
  } else {
    *segments = submitsegments_output_.segments;
    return submitsegments_output_.return_value;
  }
}

bool ConverterMock::ResizeSegment(Segments *segments,
                                  const ConversionRequest &request,
                                  size_t segment_index,
                                  int offset_length) const {
  VLOG(2) << "mock function: ResizeSegment";
  resizesegment1_input_.segments = *segments;
  resizesegment1_input_.segment_index = segment_index;
  resizesegment1_input_.offset_length = offset_length;

  if (!resizesegment1_output_.initialized) {
    return false;
  } else {
    *segments = resizesegment1_output_.segments;
    return resizesegment1_output_.return_value;
  }
}

bool ConverterMock::ResizeSegment(
    Segments *segments, const ConversionRequest &request,
    size_t start_segment_index, size_t segments_size,
    absl::Span<const uint8_t> new_size_array) const {
  VLOG(2) << "mock function: ResizeSegmnet";
  resizesegment2_input_.segments = *segments;
  resizesegment2_input_.start_segment_index = start_segment_index;
  resizesegment2_input_.segments_size = segments_size;
  resizesegment2_input_.new_size_array.assign(new_size_array.begin(),
                                              new_size_array.end());

  if (!resizesegment2_output_.initialized) {
    return false;
  } else {
    *segments = resizesegment2_output_.segments;
    return resizesegment2_output_.return_value;
  }
}

}  // namespace mozc
