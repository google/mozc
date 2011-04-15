// Copyright 2010-2011, Google Inc.
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

namespace mozc {
namespace {
void CopySegment(const Segment &src, Segment *dest) {
  dest->set_segment_type(src.segment_type());
  dest->set_key(src.key());
  for (size_t i = 0; i < src.candidates_size(); ++i) {
    Segment::Candidate *candidate = dest->add_candidate();
    *candidate = src.candidate(i);
  }
  // Copy T13N
  {
    vector<Segment::Candidate> *meta_cands = dest->mutable_meta_candidates();
    *meta_cands = src.meta_candidates();
  }
}

void CopySegments(const Segments &src, Segments *dest) {
  if (dest == NULL) {
    return;
  }
  dest->Clear();
  dest->set_request_type(src.request_type());
  for (size_t i = 0; i < src.segments_size(); ++i) {
    Segment *seg = dest->add_segment();
    const Segment &src_seg = src.segment(i);
    CopySegment(src_seg, seg);
  }
  dest->set_max_history_segments_size(src.max_history_segments_size());
}
}  // namespace

ConverterMock::ConverterMock() {
  LOG(INFO) << "ConverterMock is created";
}

ConverterMock::~ConverterMock() {}

void ConverterMock::SetStartConversion(Segments *segments, bool result) {
  startconversion_output_.initialized = true;
  CopySegments(*segments, &startconversion_output_.segments);
  startconversion_output_.return_value = result;
}

void ConverterMock::SetStartConversionWithComposer(Segments *segments,
                                                   bool result) {
  startconversionwithcomposer_output_.initialized = true;
  CopySegments(*segments, &startconversionwithcomposer_output_.segments);
  startconversionwithcomposer_output_.return_value = result;
}

void ConverterMock::SetStartReverseConversion(Segments *segments, bool result) {
  startreverseconversion_output_.initialized = true;
  CopySegments(*segments, &startreverseconversion_output_.segments);
  startreverseconversion_output_.return_value = result;
}

void ConverterMock::SetStartPrediction(Segments *segments, bool result) {
  startprediction_output_.initialized = true;
  CopySegments(*segments, &startprediction_output_.segments);
  startprediction_output_.return_value = result;
}

void ConverterMock::SetStartSuggestion(Segments *segments, bool result) {
  startsuggestion_output_.initialized = true;
  CopySegments(*segments, &startsuggestion_output_.segments);
  startsuggestion_output_.return_value = result;
}

void ConverterMock::SetFinishConversion(Segments *segments, bool result) {
  finishconversion_output_.initialized = true;
  CopySegments(*segments, &finishconversion_output_.segments);
  finishconversion_output_.return_value = result;
}

void ConverterMock::SetCancelConversion(Segments *segments, bool result) {
  cancelconversion_output_.initialized = true;
  CopySegments(*segments, &cancelconversion_output_.segments);
  cancelconversion_output_.return_value = result;
}

void ConverterMock::SetResetConversion(Segments *segments, bool result) {
  resetconversion_output_.initialized = true;
  CopySegments(*segments, &resetconversion_output_.segments);
  resetconversion_output_.return_value = result;
}

void ConverterMock::SetRevertConversion(Segments *segments, bool result) {
  revertconversion_output_.initialized = true;
  CopySegments(*segments, &revertconversion_output_.segments);
  revertconversion_output_.return_value = result;
}

void ConverterMock::SetCommitSegmentValue(Segments *segments, bool result) {
  commitsegmentvalue_output_.initialized = true;
  CopySegments(*segments, &commitsegmentvalue_output_.segments);
  commitsegmentvalue_output_.return_value = result;
}

void ConverterMock::SetFocusSegmentValue(Segments *segments, bool result) {
  focussegmentvalue_output_.initialized = true;
  CopySegments(*segments, &focussegmentvalue_output_.segments);
  focussegmentvalue_output_.return_value = result;
}

void ConverterMock::SetFreeSegmentValue(Segments *segments, bool result) {
  freesegmentvalue_output_.initialized = true;
  CopySegments(*segments, &freesegmentvalue_output_.segments);
  freesegmentvalue_output_.return_value = result;
}

void ConverterMock::SetSubmitFirstSegment(Segments *segments, bool result) {
  submitfirstsegment_output_.initialized = true;
  CopySegments(*segments, &submitfirstsegment_output_.segments);
  submitfirstsegment_output_.return_value = result;
}

void ConverterMock::SetResizeSegment1(Segments *segments, bool result) {
  resizesegment1_output_.initialized = true;
  CopySegments(*segments, &resizesegment1_output_.segments);
  resizesegment1_output_.return_value = result;
}

void ConverterMock::SetResizeSegment2(Segments *segments, bool result) {
  resizesegment2_output_.initialized = true;
  CopySegments(*segments, &resizesegment2_output_.segments);
  resizesegment2_output_.return_value = result;
}

void ConverterMock::SetSync(bool result) {
  sync_output_ = result;
}

void ConverterMock::SetReload(bool result) {
  reload_output_ = result;
}

void ConverterMock::SetClearUserHistory(bool result) {
  clearuserhistory_output_ = result;
}

void ConverterMock::SetClearUserPrediction(bool result) {
  clearuserprediction_output_ = result;
}

void ConverterMock::SetClearUnusedUserPrediction(bool result) {
  clearunuseduserprediction_output_ = result;
}


void ConverterMock::GetStartConversion(Segments *segments, string *key) {
  CopySegments(startconversion_input_.segments, segments);
  *key = startconversion_input_.key;
}

void ConverterMock::GetStartConversionWithComposer(
    Segments *segments, const composer::Composer **composer) {
  CopySegments(startconversionwithcomposer_input_.segments, segments);
  *composer = startconversionwithcomposer_input_.composer;
}

void ConverterMock::GetStartReverseConversion(Segments *segments,
                                              string *key) {
  CopySegments(startreverseconversion_input_.segments, segments);
  *key = startreverseconversion_input_.key;
}

void ConverterMock::GetStartPrediction(Segments *segments, string *key) {
  CopySegments(startprediction_input_.segments, segments);
  *key = startprediction_input_.key;
}

void ConverterMock::GetStartSuggestion(Segments *segments, string *key) {
  CopySegments(startsuggestion_input_.segments, segments);
  *key = startsuggestion_input_.key;
}

void ConverterMock::GetFinishConversion(Segments *segments) {
  CopySegments(finishconversion_input_.segments, segments);
}

void ConverterMock::GetCancelConversion(Segments *segments) {
  CopySegments(cancelconversion_input_.segments, segments);
}

void ConverterMock::GetResetConversion(Segments *segments) {
  CopySegments(resetconversion_input_.segments, segments);
}

void ConverterMock::GetRevertConversion(Segments *segments) {
  CopySegments(revertconversion_input_.segments, segments);
}

void ConverterMock::GetCommitSegmentValue(Segments *segments,
                                           size_t *segment_index,
                                           int *candidate_index) {
  CopySegments(commitsegmentvalue_input_.segments, segments);
  *segment_index = commitsegmentvalue_input_.segment_index;
  *candidate_index = commitsegmentvalue_input_.candidate_index;
}

void ConverterMock::GetFocusSegmentValue(Segments *segments,
                                          size_t *segment_index,
                                          int *candidate_index) {
  CopySegments(focussegmentvalue_input_.segments, segments);
  *segment_index = focussegmentvalue_input_.segment_index;
  *candidate_index = focussegmentvalue_input_.candidate_index;
}

void ConverterMock::GetFreeSegmentValue(Segments *segments,
                                         size_t *segment_index) {
  CopySegments(freesegmentvalue_input_.segments, segments);
  *segment_index = freesegmentvalue_input_.segment_index;
}

void ConverterMock::GetSubmitFirstSegment(Segments *segments,
                                          size_t *candidate_index) {
  CopySegments(submitfirstsegment_input_.segments, segments);
  *candidate_index = submitfirstsegment_input_.candidate_index;
}

void ConverterMock::GetResizeSegment1(Segments *segments, size_t *segment_index,
                                     int *offset_length) {
  CopySegments(resizesegment1_input_.segments, segments);
  *segment_index = resizesegment1_input_.segment_index;
  *offset_length = resizesegment1_input_.offset_length;
}

void ConverterMock::GetResizeSegment2(Segments *segments,
                                     size_t *start_segment_index,
                                     size_t *segments_size,
                                     uint8 **new_size_array,
                                     size_t *array_size) {
  CopySegments(resizesegment2_input_.segments, segments);
  *start_segment_index = resizesegment2_input_.start_segment_index;
  *segments_size = resizesegment2_input_.segments_size;
  *new_size_array = &resizesegment2_input_.new_size_array[0];
  *array_size = resizesegment2_input_.new_size_array.size();
}

bool ConverterMock::StartConversion(Segments *segments,
                                    const string &key) const {
  VLOG(2) << "mock function: StartConversion";
  CopySegments(*segments, &startconversion_input_.segments);
  startconversion_input_.key = key;

  if (!startconversion_output_.initialized) {
    return false;
  } else {
    CopySegments(startconversion_output_.segments, segments);
    return startconversion_output_.return_value;
  }
}

bool ConverterMock::StartConversionWithComposer(
    Segments *segments, const composer::Composer *composer) const {
  VLOG(2) << "mock function: StartConversionWithComposer";
  CopySegments(*segments, &startconversionwithcomposer_input_.segments);
  startconversionwithcomposer_input_.composer = composer;

  if (!startconversionwithcomposer_output_.initialized) {
    return false;
  } else {
    CopySegments(startconversionwithcomposer_output_.segments, segments);
    return startconversionwithcomposer_output_.return_value;
  }
}

bool ConverterMock::StartReverseConversion(Segments *segments,
                                           const string &key) const {
  VLOG(2) << "mock function: StartReverseConversion";
  CopySegments(*segments, &startreverseconversion_input_.segments);
  startreverseconversion_input_.key = key;

  if (!startreverseconversion_output_.initialized) {
    return false;
  } else {
    CopySegments(startreverseconversion_output_.segments, segments);
    return startreverseconversion_output_.return_value;
  }
  return false;
}

bool ConverterMock::StartPrediction(Segments *segments,
                                    const string &key) const {
  VLOG(2) << "mock function: StartPrediction";
  CopySegments(*segments, &startprediction_input_.segments);
  startprediction_input_.key = key;

  if (!startprediction_output_.initialized) {
    return false;
  } else {
    CopySegments(startprediction_output_.segments, segments);
    return startprediction_output_.return_value;
  }
}

bool ConverterMock::StartSuggestion(Segments *segments,
                                    const string &key) const {
  VLOG(2) << "mock function: StartSuggestion";
  CopySegments(*segments, &startsuggestion_input_.segments);
  startsuggestion_input_.key = key;

  if (!startsuggestion_output_.initialized) {
    return false;
  } else {
    CopySegments(startsuggestion_output_.segments, segments);
    return startsuggestion_output_.return_value;
  }
}

bool ConverterMock::FinishConversion(Segments *segments) const {
  VLOG(2) << "mock function: FinishConversion";
  CopySegments(*segments, &finishconversion_input_.segments);

  if (!finishconversion_output_.initialized) {
    return false;
  } else {
    CopySegments(finishconversion_output_.segments, segments);
    return finishconversion_output_.return_value;
  }
}

bool ConverterMock::CancelConversion(Segments *segments) const {
  VLOG(2) << "mock function: CancelConversion";
  CopySegments(*segments, &cancelconversion_input_.segments);

  if (!cancelconversion_output_.initialized) {
    return false;
  } else {
    CopySegments(cancelconversion_output_.segments, segments);
    return cancelconversion_output_.return_value;
  }
}

bool ConverterMock::ResetConversion(Segments *segments) const {
  VLOG(2) << "mock function: ResetConversion";
  CopySegments(*segments, &resetconversion_input_.segments);

  if (!resetconversion_output_.initialized) {
    return false;
  } else {
    CopySegments(resetconversion_output_.segments, segments);
    return resetconversion_output_.return_value;
  }
}

bool ConverterMock::RevertConversion(Segments *segments) const {
  VLOG(2) << "mock function: RevertConversion";
  CopySegments(*segments, &revertconversion_input_.segments);

  if (!revertconversion_output_.initialized) {
    return false;
  } else {
    CopySegments(revertconversion_output_.segments, segments);
    return revertconversion_output_.return_value;
  }
}

bool ConverterMock::CommitSegmentValue(Segments *segments,
                                       size_t segment_index,
                                       int candidate_index) const {
  VLOG(2) << "mock function: CommitSegmentValue";
  CopySegments(*segments, &commitsegmentvalue_input_.segments);
  commitsegmentvalue_input_.segment_index = segment_index;
  commitsegmentvalue_input_.candidate_index = candidate_index;

  if (!commitsegmentvalue_output_.initialized) {
    return false;
  } else {
    CopySegments(commitsegmentvalue_output_.segments, segments);
    return commitsegmentvalue_output_.return_value;
  }
}

bool ConverterMock::FocusSegmentValue(Segments *segments,
                                      size_t segment_index,
                                      int    candidate_index) const {
  VLOG(2) << "mock function: FocusSegmentValue";
  CopySegments(*segments, &focussegmentvalue_input_.segments);
  focussegmentvalue_input_.segment_index = segment_index;
  focussegmentvalue_input_.candidate_index = candidate_index;

  if (!focussegmentvalue_output_.initialized) {
    return false;
  } else {
    CopySegments(focussegmentvalue_output_.segments, segments);
    return focussegmentvalue_output_.return_value;
  }
}


bool ConverterMock::FreeSegmentValue(Segments *segments,
                                     size_t segment_index) const {
  VLOG(2) << "mock function: FreeSegmentValue";
  CopySegments(*segments, &freesegmentvalue_input_.segments);
  freesegmentvalue_input_.segment_index = segment_index;

  if (!freesegmentvalue_output_.initialized) {
    return false;
  } else {
    CopySegments(freesegmentvalue_output_.segments, segments);
    return freesegmentvalue_output_.return_value;
  }
}

bool ConverterMock::SubmitFirstSegment(Segments *segments,
                                       size_t candidate_index) const {
  VLOG(2) << "mock function: SubmitFirstSegment";
  CopySegments(*segments, &submitfirstsegment_input_.segments);
  submitfirstsegment_input_.candidate_index = candidate_index;

  if (!submitfirstsegment_output_.initialized) {
    return false;
  } else {
    CopySegments(submitfirstsegment_output_.segments, segments);
    return submitfirstsegment_output_.return_value;
  }
}

bool ConverterMock::ResizeSegment(Segments *segments,
                                  size_t segment_index,
                                  int offset_length) const {
  VLOG(2) << "mock function: ResizeSegment";
  CopySegments(*segments, &resizesegment1_input_.segments);
  resizesegment1_input_.segment_index = segment_index;
  resizesegment1_input_.offset_length = offset_length;

  if (!resizesegment1_output_.initialized) {
    return false;
  } else {
    CopySegments(resizesegment1_output_.segments, segments);
    return resizesegment1_output_.return_value;
  }
}

bool ConverterMock::ResizeSegment(Segments *segments,
                                  size_t start_segment_index,
                                  size_t segments_size,
                                  const uint8 *new_size_array,
                                  size_t array_size) const {
  VLOG(2) << "mock function: ResizeSegmnet";
  CopySegments(*segments, &resizesegment2_input_.segments);
  resizesegment2_input_.start_segment_index = start_segment_index;
  resizesegment2_input_.segments_size = segments_size;
  vector<uint8> size_array(new_size_array,
                           new_size_array + array_size);
  resizesegment2_input_.new_size_array = size_array;

  if (!resizesegment2_output_.initialized) {
    return false;
  } else {
    CopySegments(resizesegment2_output_.segments, segments);
    return resizesegment2_output_.return_value;
  }
}

bool ConverterMock::Sync() const {
  VLOG(2) << "mock function: Sync";
  return sync_output_;
}

bool ConverterMock::Reload() const {
  VLOG(2) << "mock function: Reload";
  return reload_output_;
}

bool ConverterMock::ClearUserHistory() const {
  VLOG(2) << "mock function: ClearUserHistory";
  return clearuserhistory_output_;
}

bool ConverterMock::ClearUserPrediction() const {
  VLOG(2) << "mock function: ClearUserPrediction";
  return clearuserprediction_output_;
}

bool ConverterMock::ClearUnusedUserPrediction() const {
  VLOG(2) << "mock function: ClearUserPrediction";
  return clearunuseduserprediction_output_;
}
}  // namespace mozc
