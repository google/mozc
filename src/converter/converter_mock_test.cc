// Copyright 2010, Google Inc.
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
#include "dictionary/dictionary_interface.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {
void SetSegments(Segments *segments, const string &cand_value) {
  Segment *segment = segments->add_segment();
  // "Testてすと"
  segment->set_key("\x54\x65\x73\x74\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8");
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = cand_value;
}
}

TEST(ConverterMockTest, SetConverter) {
  ConverterMock mock;
  ConverterInterface *converter = ConverterFactory::GetConverter();
  EXPECT_NE(&mock, converter);

  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter_mock = ConverterFactory::GetConverter();
  EXPECT_EQ(&mock, converter_mock);
}

TEST(ConverterMockTest, SetStartConvert) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "StartConvert");
  mock.SetStartConversion(&expect, true);
  EXPECT_TRUE(converter->StartConversion(&output, "dummy"));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST(ConverterMockTest, SetStartPrediction) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "StartPrediction");
  mock.SetStartPrediction(&expect, true);
  EXPECT_TRUE(converter->StartPrediction(&output, "dummy"));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST(ConverterMockTest, SetStartSuggestion) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "StartSuggestion");
  mock.SetStartSuggestion(&expect, true);
  EXPECT_TRUE(converter->StartSuggestion(&output, "dummy"));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST(ConverterMockTest, SetFinishConversion) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "FinishConversion");
  mock.SetFinishConversion(&expect, true);
  EXPECT_TRUE(converter->FinishConversion(&output));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST(ConverterMockTest, SetCancelConversion) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "CancelConversion");
  mock.SetCancelConversion(&expect, true);
  EXPECT_TRUE(converter->CancelConversion(&output));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST(ConverterMockTest, SetResetConversion) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "ResetConversion");
  mock.SetResetConversion(&expect, true);
  EXPECT_TRUE(converter->ResetConversion(&output));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST(ConverterMockTest, SetGetCandidates) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "GetCandidates");
  mock.SetGetCandidates(&expect, true);
  EXPECT_TRUE(converter->GetCandidates(&output, 10, 100));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST(ConverterMockTest, SetCommitSegmentValue) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "CommitSegmentValue");
  mock.SetCommitSegmentValue(&expect, true);
  EXPECT_TRUE(converter->CommitSegmentValue(&output, 1, 10));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST(ConverterMockTest, SetFocusSegmentValue) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "FocusSegmentValue");
  mock.SetFocusSegmentValue(&expect, true);
  EXPECT_TRUE(converter->FocusSegmentValue(&output, 1, 10));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST(ConverterMockTest, SetFreeSegmentValue) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "FreeSegmentValue");
  mock.SetFreeSegmentValue(&expect, true);
  EXPECT_TRUE(converter->FreeSegmentValue(&output, 1));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST(ConverterMockTest, SetSubmitFirstSegment) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "SubmitFirstSegment");
  mock.SetSubmitFirstSegment(&expect, true);
  EXPECT_TRUE(converter->SubmitFirstSegment(&output, 1));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST(ConverterMockTest, SetResizeSegment1) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "ResizeSegment1");
  mock.SetResizeSegment1(&expect, true);
  EXPECT_TRUE(converter->ResizeSegment(&output, 1, 5));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST(ConverterMockTest, SetResizeSegment2) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "ResizeSegment2");
  mock.SetResizeSegment2(&expect, true);
  uint8 size_array[] = {1, 2, 3};
  EXPECT_TRUE(converter->ResizeSegment(&output, 1, 5,
                                       size_array, arraysize(size_array)));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST(ConverterMockTest, SetSync) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  mock.SetSync(true);
  EXPECT_TRUE(converter->Sync());

  mock.SetSync(false);
  EXPECT_FALSE(converter->Sync());
}

TEST(ConverterMockTest, SetClearUserHistory) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  mock.SetClearUserHistory(true);
  EXPECT_TRUE(converter->ClearUserHistory());

  mock.SetClearUserHistory(false);
  EXPECT_FALSE(converter->ClearUserHistory());
}

TEST(ConverterMockTest, SetClearUserPrediction) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  mock.SetClearUserPrediction(true);
  EXPECT_TRUE(converter->ClearUserPrediction());

  mock.SetClearUserPrediction(false);
  EXPECT_FALSE(converter->ClearUserPrediction());
}

TEST(ConverterMockTest, GetStartConversion) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments input;
  string input_str;
  const string input_key = "Key";
  SetSegments(&input, "StartConversion");
  input.DebugString(&input_str);
  converter->StartConversion(&input, input_key);

  Segments last_segment;
  string last_segment_str;
  string last_key;
  mock.GetStartConversion(&last_segment, &last_key);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_key, last_key);
}

TEST(ConverterMockTest, GetStartPrediction) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments input;
  string input_str;
  const string input_key = "Key";
  SetSegments(&input, "StartPrediction");
  input.DebugString(&input_str);
  converter->StartPrediction(&input, input_key);

  Segments last_segment;
  string last_segment_str;
  string last_key;
  mock.GetStartPrediction(&last_segment, &last_key);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_key, last_key);
}

TEST(ConverterMockTest, GetStartSuggestion) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments input;
  string input_str;
  const string input_key = "Key";
  SetSegments(&input, "StartSuggestion");
  input.DebugString(&input_str);
  converter->StartSuggestion(&input, input_key);

  Segments last_segment;
  string last_segment_str;
  string last_key;
  mock.GetStartSuggestion(&last_segment, &last_key);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_key, last_key);
}

TEST(ConverterMockTest, GetFinishConversion) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments input;
  string input_str;
  SetSegments(&input, "FinishConversion");
  input.DebugString(&input_str);
  converter->FinishConversion(&input);

  Segments last_segment;
  string last_segment_str;
  mock.GetFinishConversion(&last_segment);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
}

TEST(ConverterMockTest, GetCancelConversion) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments input;
  string input_str;
  SetSegments(&input, "CancelConversion");
  input.DebugString(&input_str);
  converter->CancelConversion(&input);

  Segments last_segment;
  string last_segment_str;
  mock.GetCancelConversion(&last_segment);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
}

TEST(ConverterMockTest, GetResetConversion) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments input;
  string input_str;
  SetSegments(&input, "ResetConversion");
  input.DebugString(&input_str);
  converter->ResetConversion(&input);

  Segments last_segment;
  string last_segment_str;
  mock.GetResetConversion(&last_segment);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
}

TEST(ConverterMockTest, GetGetCandidates) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments input;
  string input_str;
  size_t input_idx = 2, input_size = 10;
  SetSegments(&input, "GetCandidates");
  input.DebugString(&input_str);
  converter->GetCandidates(&input, input_idx, input_size);

  Segments last_segment;
  string last_segment_str;
  size_t last_idx, last_size;
  mock.GetGetCandidates(&last_segment, &last_idx, &last_size);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_idx, last_idx);
  EXPECT_EQ(input_size, last_size);
}

TEST(ConverterMockTest, GetCommitSegmentValue) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments input;
  string input_str;
  size_t input_idx = 1;
  int input_cidx = 5;
  SetSegments(&input, "CommitSegmentValue");
  input.DebugString(&input_str);
  converter->CommitSegmentValue(&input, input_idx, input_cidx);

  Segments last_segment;
  string last_segment_str;
  size_t last_idx;
  int last_cidx;
  mock.GetCommitSegmentValue(&last_segment, &last_idx, &last_cidx);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_idx, last_idx);
  EXPECT_EQ(input_cidx, last_cidx);
}

TEST(ConverterMockTest, GetFocusSegmentValue) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments input;
  string input_str;
  size_t input_idx = 1;
  int input_cidx = 5;
  SetSegments(&input, "FocueSegmentValue");
  input.DebugString(&input_str);
  converter->FocusSegmentValue(&input, input_idx, input_cidx);

  Segments last_segment;
  string last_segment_str;
  size_t last_idx;
  int last_cidx;
  mock.GetFocusSegmentValue(&last_segment, &last_idx, &last_cidx);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_idx, last_idx);
  EXPECT_EQ(input_cidx, last_cidx);
}

TEST(ConverterMockTest, GetFreeSegmentValue) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments input;
  string input_str;
  size_t input_idx = 1;
  SetSegments(&input, "FreeSegmentValue");
  input.DebugString(&input_str);
  converter->FreeSegmentValue(&input, input_idx);

  Segments last_segment;
  string last_segment_str;
  size_t last_idx;
  mock.GetFreeSegmentValue(&last_segment, &last_idx);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_idx, last_idx);
}

TEST(ConverterMockTest, GetSubmitFirstSegment) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments input;
  string input_str;
  size_t input_idx = 1;
  SetSegments(&input, "SubmitFirstSegment");
  input.DebugString(&input_str);
  converter->SubmitFirstSegment(&input, input_idx);

  Segments last_segment;
  string last_segment_str;
  size_t last_idx;
  mock.GetSubmitFirstSegment(&last_segment, &last_idx);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_idx, last_idx);
}

TEST(ConverterMockTest, GetResizeSegment1) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments input;
  string input_str;
  size_t input_idx = 1;
  int input_offset = 3;
  SetSegments(&input, "ResizeSegment1");
  input.DebugString(&input_str);
  converter->ResizeSegment(&input, input_idx, input_offset);

  Segments last_segment;
  string last_segment_str;
  size_t last_idx;
  int last_offset;
  mock.GetResizeSegment1(&last_segment, &last_idx, &last_offset);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_idx, last_idx);
  EXPECT_EQ(input_offset, last_offset);
}

TEST(ConverterMockTest, GetResizeSegment2) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments input;
  string input_str;
  size_t input_idx = 1, input_size = 3;
  uint8 input_array[] = {1, 2, 3};
  SetSegments(&input, "ResizeSegment2");
  input.DebugString(&input_str);
  converter->ResizeSegment(&input, input_idx, input_size,
                           input_array, arraysize(input_array));

  Segments last_segment;
  string last_segment_str;
  size_t last_idx, last_size;
  uint8 *last_array;
  size_t last_array_size;

  mock.GetResizeSegment2(&last_segment, &last_idx, &last_size,
                         &last_array, &last_array_size);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_idx, last_idx);
  EXPECT_EQ(input_size, last_size);
  EXPECT_EQ(arraysize(input_array), last_array_size);
  for (int i = 0; i < arraysize(input_array); ++i) {
    EXPECT_EQ(input_array[i], *(last_array + i));
  }
}

TEST(ConverterMockTest, DefaultBehavior) {
  ConverterMock mock;
  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments input;
  const string input_key = "Key";
  SetSegments(&input, "StartConversion");
  string input_str;
  input.DebugString(&input_str);
  EXPECT_FALSE(converter->StartConversion(&input, input_key));

  string last_str;
  input.DebugString(&last_str);
  EXPECT_EQ(input_str, last_str);
}

}  // namespace mozc
