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

class ConverterMockTest : public testing::Test {
 public:
  void SetUp() {
    mock_.reset(new ConverterMock);
    ConverterFactory::SetConverter(mock_.get());
  }

  void TearDown() {
    ConverterFactory::SetConverter(NULL);
  }

  ConverterMock *GetMock() {
    return mock_.get();
  }

 private:
  scoped_ptr<ConverterMock> mock_;
};

// This is not a test of ConverterMock but a test of ConverterFactory.
// TODO(toshiyuki): move this test case to converter_test.cc
TEST_F(ConverterMockTest, SetConverter) {
  ConverterMock mock;
  ConverterInterface *converter = ConverterFactory::GetConverter();
  EXPECT_NE(&mock, converter);

  ConverterFactory::SetConverter(&mock);
  ConverterInterface *converter_mock = ConverterFactory::GetConverter();
  EXPECT_EQ(&mock, converter_mock);
}

TEST_F(ConverterMockTest, SetStartConvert) {
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "StartConvert");
  GetMock()->SetStartConversion(&expect, true);
  EXPECT_TRUE(converter->StartConversion(&output, "dummy"));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST_F(ConverterMockTest, SetStartPrediction) {
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "StartPrediction");
  GetMock()->SetStartPrediction(&expect, true);
  EXPECT_TRUE(converter->StartPrediction(&output, "dummy"));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST_F(ConverterMockTest, SetStartSuggestion) {
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "StartSuggestion");
  GetMock()->SetStartSuggestion(&expect, true);
  EXPECT_TRUE(converter->StartSuggestion(&output, "dummy"));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST_F(ConverterMockTest, SetFinishConversion) {
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "FinishConversion");
  GetMock()->SetFinishConversion(&expect, true);
  EXPECT_TRUE(converter->FinishConversion(&output));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST_F(ConverterMockTest, SetCancelConversion) {
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "CancelConversion");
  GetMock()->SetCancelConversion(&expect, true);
  EXPECT_TRUE(converter->CancelConversion(&output));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST_F(ConverterMockTest, SetResetConversion) {
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "ResetConversion");
  GetMock()->SetResetConversion(&expect, true);
  EXPECT_TRUE(converter->ResetConversion(&output));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST_F(ConverterMockTest, SetGetCandidates) {
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "GetCandidates");
  GetMock()->SetGetCandidates(&expect, true);
  EXPECT_TRUE(converter->GetCandidates(&output, 10, 100));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST_F(ConverterMockTest, SetCommitSegmentValue) {
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "CommitSegmentValue");
  GetMock()->SetCommitSegmentValue(&expect, true);
  EXPECT_TRUE(converter->CommitSegmentValue(&output, 1, 10));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST_F(ConverterMockTest, SetFocusSegmentValue) {
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "FocusSegmentValue");
  GetMock()->SetFocusSegmentValue(&expect, true);
  EXPECT_TRUE(converter->FocusSegmentValue(&output, 1, 10));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST_F(ConverterMockTest, SetFreeSegmentValue) {
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "FreeSegmentValue");
  GetMock()->SetFreeSegmentValue(&expect, true);
  EXPECT_TRUE(converter->FreeSegmentValue(&output, 1));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST_F(ConverterMockTest, SetSubmitFirstSegment) {
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "SubmitFirstSegment");
  GetMock()->SetSubmitFirstSegment(&expect, true);
  EXPECT_TRUE(converter->SubmitFirstSegment(&output, 1));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST_F(ConverterMockTest, SetResizeSegment1) {
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "ResizeSegment1");
  GetMock()->SetResizeSegment1(&expect, true);
  EXPECT_TRUE(converter->ResizeSegment(&output, 1, 5));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST_F(ConverterMockTest, SetResizeSegment2) {
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments output, expect;
  string output_str, expect_str;
  SetSegments(&expect, "ResizeSegment2");
  GetMock()->SetResizeSegment2(&expect, true);
  uint8 size_array[] = {1, 2, 3};
  EXPECT_TRUE(converter->ResizeSegment(&output, 1, 5,
                                       size_array, arraysize(size_array)));
  output.DebugString(&output_str);
  expect.DebugString(&expect_str);
  EXPECT_EQ(expect_str, output_str);
}

TEST_F(ConverterMockTest, SetSync) {
  ConverterInterface *converter = ConverterFactory::GetConverter();

  GetMock()->SetSync(true);
  EXPECT_TRUE(converter->Sync());

  GetMock()->SetSync(false);
  EXPECT_FALSE(converter->Sync());
}

TEST_F(ConverterMockTest, SetClearUserHistory) {
  ConverterInterface *converter = ConverterFactory::GetConverter();

  GetMock()->SetClearUserHistory(true);
  EXPECT_TRUE(converter->ClearUserHistory());

  GetMock()->SetClearUserHistory(false);
  EXPECT_FALSE(converter->ClearUserHistory());
}

TEST_F(ConverterMockTest, SetClearUserPrediction) {
  ConverterInterface *converter = ConverterFactory::GetConverter();

  GetMock()->SetClearUserPrediction(true);
  EXPECT_TRUE(converter->ClearUserPrediction());

  GetMock()->SetClearUserPrediction(false);
  EXPECT_FALSE(converter->ClearUserPrediction());
}

TEST_F(ConverterMockTest, GetStartConversion) {
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
  GetMock()->GetStartConversion(&last_segment, &last_key);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_key, last_key);
}

TEST_F(ConverterMockTest, GetStartPrediction) {
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
  GetMock()->GetStartPrediction(&last_segment, &last_key);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_key, last_key);
}

TEST_F(ConverterMockTest, GetStartSuggestion) {
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
  GetMock()->GetStartSuggestion(&last_segment, &last_key);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_key, last_key);
}

TEST_F(ConverterMockTest, GetFinishConversion) {
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments input;
  string input_str;
  SetSegments(&input, "FinishConversion");
  input.DebugString(&input_str);
  converter->FinishConversion(&input);

  Segments last_segment;
  string last_segment_str;
  GetMock()->GetFinishConversion(&last_segment);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
}

TEST_F(ConverterMockTest, GetCancelConversion) {
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments input;
  string input_str;
  SetSegments(&input, "CancelConversion");
  input.DebugString(&input_str);
  converter->CancelConversion(&input);

  Segments last_segment;
  string last_segment_str;
  GetMock()->GetCancelConversion(&last_segment);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
}

TEST_F(ConverterMockTest, GetResetConversion) {
  ConverterInterface *converter = ConverterFactory::GetConverter();

  Segments input;
  string input_str;
  SetSegments(&input, "ResetConversion");
  input.DebugString(&input_str);
  converter->ResetConversion(&input);

  Segments last_segment;
  string last_segment_str;
  GetMock()->GetResetConversion(&last_segment);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
}

TEST_F(ConverterMockTest, GetGetCandidates) {
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
  GetMock()->GetGetCandidates(&last_segment, &last_idx, &last_size);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_idx, last_idx);
  EXPECT_EQ(input_size, last_size);
}

TEST_F(ConverterMockTest, GetCommitSegmentValue) {
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
  GetMock()->GetCommitSegmentValue(&last_segment, &last_idx, &last_cidx);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_idx, last_idx);
  EXPECT_EQ(input_cidx, last_cidx);
}

TEST_F(ConverterMockTest, GetFocusSegmentValue) {
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
  GetMock()->GetFocusSegmentValue(&last_segment, &last_idx, &last_cidx);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_idx, last_idx);
  EXPECT_EQ(input_cidx, last_cidx);
}

TEST_F(ConverterMockTest, GetFreeSegmentValue) {
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
  GetMock()->GetFreeSegmentValue(&last_segment, &last_idx);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_idx, last_idx);
}

TEST_F(ConverterMockTest, GetSubmitFirstSegment) {
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
  GetMock()->GetSubmitFirstSegment(&last_segment, &last_idx);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_idx, last_idx);
}

TEST_F(ConverterMockTest, GetResizeSegment1) {
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
  GetMock()->GetResizeSegment1(&last_segment, &last_idx, &last_offset);
  last_segment.DebugString(&last_segment_str);

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_idx, last_idx);
  EXPECT_EQ(input_offset, last_offset);
}

TEST_F(ConverterMockTest, GetResizeSegment2) {
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

  GetMock()->GetResizeSegment2(&last_segment, &last_idx, &last_size,
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

TEST_F(ConverterMockTest, DefaultBehavior) {
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
