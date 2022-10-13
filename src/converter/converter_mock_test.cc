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
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "converter/segments.h"
#include "request/conversion_request.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {

void SetSegments(Segments *segments, absl::string_view cand_value) {
  Segment *segment = segments->add_segment();
  segment->set_key("Testてすと");
  Segment::Candidate *candidate = segment->add_candidate();
#ifdef ABSL_USES_STD_STRING_VIEW
  candidate->value = cand_value;
#else   // ABSL_USES_STD_STRING_VIEW
  candidate->value = std::string(cand_value);
#endif  // ABSL_USES_STD_STRING_VIEW

  // Add meta candidates
  Segment::Candidate *meta_cand = segment->add_meta_candidate();
  meta_cand->Init();
  meta_cand->value = "TestT13N";
}

class ConverterMockTest : public ::testing::Test {
 protected:
  void SetUp() override { mock_ = std::make_unique<ConverterMock>(); }

  ConverterMock *GetMock() { return mock_.get(); }

 private:
  std::unique_ptr<ConverterMock> mock_;
};

TEST_F(ConverterMockTest, CopySegment) {
  ConverterInterface *converter = GetMock();

  Segments output, expect;
  SetSegments(&expect, "StartConvert");
  GetMock()->SetStartConversion(&expect, true);
  EXPECT_TRUE(converter->StartConversion(&output, "dummy"));
  EXPECT_EQ(expect.DebugString(), output.DebugString());
  EXPECT_EQ(1, output.segments_size());
  const Segment &seg = output.segment(0);
  EXPECT_EQ("Testてすと", seg.key());
  EXPECT_EQ(1, seg.candidates_size());
  EXPECT_EQ("StartConvert", seg.candidate(0).value);
  EXPECT_EQ(1, seg.meta_candidates_size());
  EXPECT_EQ("TestT13N", seg.meta_candidate(0).value);
}

TEST_F(ConverterMockTest, SetStartConversion) {
  ConverterInterface *converter = GetMock();

  Segments output, expect;
  SetSegments(&expect, "StartConversion");
  GetMock()->SetStartConversion(&expect, true);
  EXPECT_TRUE(converter->StartConversion(&output, "dummy"));
  EXPECT_EQ(expect.DebugString(), output.DebugString());
}

TEST_F(ConverterMockTest, SetStartReverseConvert) {
  ConverterInterface *converter = GetMock();

  Segments output, expect;
  SetSegments(&expect, "StartReverseConvert");
  GetMock()->SetStartReverseConversion(&expect, true);
  EXPECT_TRUE(converter->StartReverseConversion(&output, "dummy"));
  EXPECT_EQ(expect.DebugString(), output.DebugString());
}

TEST_F(ConverterMockTest, SetStartPrediction) {
  ConverterInterface *converter = GetMock();

  Segments output, expect;
  SetSegments(&expect, "StartPrediction");
  GetMock()->SetStartPrediction(&expect, true);
  EXPECT_TRUE(converter->StartPrediction(&output, "dummy"));
  EXPECT_EQ(expect.DebugString(), output.DebugString());
}

TEST_F(ConverterMockTest, SetStartSuggestion) {
  ConverterInterface *converter = GetMock();

  Segments output, expect;
  SetSegments(&expect, "StartSuggestion");
  GetMock()->SetStartSuggestion(&expect, true);
  EXPECT_TRUE(converter->StartSuggestion(&output, "dummy"));
  EXPECT_EQ(expect.DebugString(), output.DebugString());
}

TEST_F(ConverterMockTest, SetStartPartialPrediction) {
  ConverterInterface *converter = GetMock();

  Segments output, expect;
  SetSegments(&expect, "StartPartialPrediction");
  GetMock()->SetStartPartialPrediction(&expect, true);
  EXPECT_TRUE(converter->StartPartialPrediction(&output, "dummy"));
  EXPECT_EQ(expect.DebugString(), output.DebugString());
}

TEST_F(ConverterMockTest, SetStartPartialSuggestion) {
  ConverterInterface *converter = GetMock();

  Segments output, expect;
  SetSegments(&expect, "StartPartialSuggestion");
  GetMock()->SetStartPartialSuggestion(&expect, true);
  EXPECT_TRUE(converter->StartPartialSuggestion(&output, "dummy"));
  EXPECT_EQ(expect.DebugString(), output.DebugString());
}

TEST_F(ConverterMockTest, SetFinishConversion) {
  ConverterInterface *converter = GetMock();

  Segments output, expect;
  SetSegments(&expect, "FinishConversion");
  GetMock()->SetFinishConversion(&expect, true);
  const ConversionRequest default_request;
  EXPECT_TRUE(converter->FinishConversion(default_request, &output));
  EXPECT_EQ(expect.DebugString(), output.DebugString());
}

TEST_F(ConverterMockTest, SetCancelConversion) {
  ConverterInterface *converter = GetMock();

  Segments output, expect;
  SetSegments(&expect, "CancelConversion");
  GetMock()->SetCancelConversion(&expect, true);
  EXPECT_TRUE(converter->CancelConversion(&output));
  EXPECT_EQ(expect.DebugString(), output.DebugString());
}

TEST_F(ConverterMockTest, SetResetConversion) {
  ConverterInterface *converter = GetMock();

  Segments output, expect;
  SetSegments(&expect, "ResetConversion");
  GetMock()->SetResetConversion(&expect, true);
  EXPECT_TRUE(converter->ResetConversion(&output));
  EXPECT_EQ(expect.DebugString(), output.DebugString());
}

TEST_F(ConverterMockTest, SetCommitSegmentValue) {
  ConverterInterface *converter = GetMock();

  Segments output, expect;
  SetSegments(&expect, "CommitSegmentValue");
  GetMock()->SetCommitSegmentValue(&expect, true);
  EXPECT_TRUE(converter->CommitSegmentValue(&output, 1, 10));
  EXPECT_EQ(expect.DebugString(), output.DebugString());
}

TEST_F(ConverterMockTest, SetFocusSegmentValue) {
  ConverterInterface *converter = GetMock();

  Segments output, expect;
  SetSegments(&expect, "FocusSegmentValue");
  GetMock()->SetFocusSegmentValue(&expect, true);
  EXPECT_TRUE(converter->FocusSegmentValue(&output, 1, 10));
  EXPECT_EQ(expect.DebugString(), output.DebugString());
}

TEST_F(ConverterMockTest, SetCommitSegments) {
  ConverterInterface *converter = GetMock();

  Segments output, expect;
  SetSegments(&expect, "CommitSegments");
  GetMock()->SetCommitSegments(&expect, true);
  std::vector<size_t> singleton_vector;
  singleton_vector.push_back(1);
  EXPECT_TRUE(converter->CommitSegments(&output, singleton_vector));
  EXPECT_EQ(expect.DebugString(), output.DebugString());
}

TEST_F(ConverterMockTest, SetResizeSegment1) {
  ConverterInterface *converter = GetMock();

  Segments output, expect;
  SetSegments(&expect, "ResizeSegment1");
  GetMock()->SetResizeSegment1(&expect, true);
  const ConversionRequest default_request;
  EXPECT_TRUE(converter->ResizeSegment(&output, default_request, 1, 5));
  EXPECT_EQ(expect.DebugString(), output.DebugString());
}

TEST_F(ConverterMockTest, SetResizeSegment2) {
  ConverterInterface *converter = GetMock();

  Segments output, expect;
  SetSegments(&expect, "ResizeSegment2");
  GetMock()->SetResizeSegment2(&expect, true);
  uint8_t size_array[] = {1, 2, 3};
  const ConversionRequest default_request;
  EXPECT_TRUE(
      converter->ResizeSegment(&output, default_request, 1, 5, size_array));
  EXPECT_EQ(expect.DebugString(), output.DebugString());
}
TEST_F(ConverterMockTest, GetStartConversion) {
  ConverterInterface *converter = GetMock();

  Segments input;
  const std::string input_key = "Key";
  SetSegments(&input, "StartConversion");
  const std::string input_str = input.DebugString();
  converter->StartConversion(&input, input_key);

  Segments last_segment;
  std::string last_key;
  GetMock()->GetStartConversion(&last_segment, &last_key);
  const std::string last_segment_str = last_segment.DebugString();

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_key, last_key);
}

TEST_F(ConverterMockTest, GetStartReverseConversion) {
  ConverterInterface *converter = GetMock();

  Segments input;
  const std::string input_key = "Key";
  SetSegments(&input, "StartReverseConversion");
  const std::string input_str = input.DebugString();
  converter->StartReverseConversion(&input, input_key);

  Segments last_segment;
  std::string last_key;
  GetMock()->GetStartReverseConversion(&last_segment, &last_key);
  const std::string last_segment_str = last_segment.DebugString();

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_key, last_key);
}

TEST_F(ConverterMockTest, GetStartPrediction) {
  ConverterInterface *converter = GetMock();

  Segments input;
  const std::string input_key = "Key";
  SetSegments(&input, "StartPrediction");
  const std::string input_str = input.DebugString();
  converter->StartPrediction(&input, input_key);

  Segments last_segment;
  std::string last_key;
  GetMock()->GetStartPrediction(&last_segment, &last_key);
  const std::string last_segment_str = last_segment.DebugString();

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_key, last_key);
}

TEST_F(ConverterMockTest, GetStartSuggestion) {
  ConverterInterface *converter = GetMock();

  Segments input;
  const std::string input_key = "Key";
  SetSegments(&input, "StartSuggestion");
  const std::string input_str = input.DebugString();
  converter->StartSuggestion(&input, input_key);

  Segments last_segment;
  std::string last_key;
  GetMock()->GetStartSuggestion(&last_segment, &last_key);
  const std::string last_segment_str = last_segment.DebugString();

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_key, last_key);
}

TEST_F(ConverterMockTest, GetStartPartialPrediction) {
  ConverterInterface *converter = GetMock();

  Segments input;
  const std::string input_key = "Key";
  SetSegments(&input, "StartPartialPrediction");
  const std::string input_str = input.DebugString();
  converter->StartPartialPrediction(&input, input_key);

  Segments last_segment;
  std::string last_key;
  GetMock()->GetStartPartialPrediction(&last_segment, &last_key);
  const std::string last_segment_str = last_segment.DebugString();

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_key, last_key);
}

TEST_F(ConverterMockTest, GetStartPartialSuggestion) {
  ConverterInterface *converter = GetMock();

  Segments input;
  const std::string input_key = "Key";
  SetSegments(&input, "StartPartialSuggestion");
  const std::string input_str = input.DebugString();
  converter->StartPartialSuggestion(&input, input_key);

  Segments last_segment;
  std::string last_key;
  GetMock()->GetStartPartialSuggestion(&last_segment, &last_key);
  const std::string last_segment_str = last_segment.DebugString();

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_key, last_key);
}

TEST_F(ConverterMockTest, GetFinishConversion) {
  ConverterInterface *converter = GetMock();

  Segments input;
  SetSegments(&input, "FinishConversion");
  const std::string input_str = input.DebugString();
  ConversionRequest default_request;
  converter->FinishConversion(default_request, &input);

  Segments last_segment;
  GetMock()->GetFinishConversion(&last_segment);
  const std::string last_segment_str = last_segment.DebugString();

  EXPECT_EQ(input_str, last_segment_str);
}

TEST_F(ConverterMockTest, GetCancelConversion) {
  ConverterInterface *converter = GetMock();

  Segments input;
  SetSegments(&input, "CancelConversion");
  const std::string input_str = input.DebugString();
  converter->CancelConversion(&input);

  Segments last_segment;
  GetMock()->GetCancelConversion(&last_segment);
  const std::string last_segment_str = last_segment.DebugString();

  EXPECT_EQ(input_str, last_segment_str);
}

TEST_F(ConverterMockTest, GetResetConversion) {
  ConverterInterface *converter = GetMock();

  Segments input;
  SetSegments(&input, "ResetConversion");
  const std::string input_str = input.DebugString();
  converter->ResetConversion(&input);

  Segments last_segment;
  GetMock()->GetResetConversion(&last_segment);
  const std::string last_segment_str = last_segment.DebugString();

  EXPECT_EQ(input_str, last_segment_str);
}

TEST_F(ConverterMockTest, GetCommitSegmentValue) {
  ConverterInterface *converter = GetMock();

  Segments input;
  size_t input_idx = 1;
  int input_cidx = 5;
  SetSegments(&input, "CommitSegmentValue");
  const std::string input_str = input.DebugString();
  converter->CommitSegmentValue(&input, input_idx, input_cidx);

  Segments last_segment;
  size_t last_idx;
  int last_cidx;
  GetMock()->GetCommitSegmentValue(&last_segment, &last_idx, &last_cidx);
  const std::string last_segment_str = last_segment.DebugString();

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_idx, last_idx);
  EXPECT_EQ(input_cidx, last_cidx);
}

TEST_F(ConverterMockTest, GetFocusSegmentValue) {
  ConverterInterface *converter = GetMock();

  Segments input;
  size_t input_idx = 1;
  int input_cidx = 5;
  SetSegments(&input, "FocueSegmentValue");
  const std::string input_str = input.DebugString();
  converter->FocusSegmentValue(&input, input_idx, input_cidx);

  Segments last_segment;
  size_t last_idx;
  int last_cidx;
  GetMock()->GetFocusSegmentValue(&last_segment, &last_idx, &last_cidx);
  const std::string last_segment_str = last_segment.DebugString();

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_idx, last_idx);
  EXPECT_EQ(input_cidx, last_cidx);
}

TEST_F(ConverterMockTest, GetCommitSegments) {
  ConverterInterface *converter = GetMock();

  Segments input;
  size_t input_idx1 = 1;
  size_t input_idx2 = 2;
  SetSegments(&input, "CommitSegments");
  const std::string input_str = input.DebugString();
  std::vector<size_t> index_list;
  index_list.push_back(input_idx1);
  index_list.push_back(input_idx2);
  converter->CommitSegments(&input, index_list);

  Segments last_segment;
  std::vector<size_t> last_idx;
  GetMock()->GetCommitSegments(&last_segment, &last_idx);
  const std::string last_segment_str = last_segment.DebugString();

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_idx1, last_idx[0]);
  EXPECT_EQ(input_idx2, last_idx[1]);
}

TEST_F(ConverterMockTest, GetResizeSegment1) {
  ConverterInterface *converter = GetMock();

  Segments input;
  size_t input_idx = 1;
  int input_offset = 3;
  SetSegments(&input, "ResizeSegment1");
  const std::string input_str = input.DebugString();
  const ConversionRequest default_request;
  // TODO(noriyukit): Since converter->SetResizeSegment1 is not called, this
  // method returns false. Verify if this is the correct expectation.
  ASSERT_FALSE(converter->ResizeSegment(&input, default_request, input_idx,
                                        input_offset));

  Segments last_segment;
  size_t last_idx;
  int last_offset;
  GetMock()->GetResizeSegment1(&last_segment, &last_idx, &last_offset);
  const std::string last_segment_str = last_segment.DebugString();

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_idx, last_idx);
  EXPECT_EQ(input_offset, last_offset);
}

TEST_F(ConverterMockTest, GetResizeSegment2) {
  ConverterInterface *converter = GetMock();

  Segments input;
  size_t input_idx = 1, input_size = 3;
  uint8_t input_array[] = {1, 2, 3};
  SetSegments(&input, "ResizeSegment2");
  const std::string input_str = input.DebugString();
  const ConversionRequest default_request;
  // TODO(noriyukit): Since converter->SetResizeSegment1 is not called, this
  // method returns false. Verify if this is the correct expectation.
  ASSERT_FALSE(converter->ResizeSegment(&input, default_request, input_idx,
                                        input_size, input_array));

  Segments last_segment;
  size_t last_idx, last_size;
  uint8_t *last_array;
  size_t last_array_size;

  GetMock()->GetResizeSegment2(&last_segment, &last_idx, &last_size,
                               &last_array, &last_array_size);
  const std::string last_segment_str = last_segment.DebugString();

  EXPECT_EQ(input_str, last_segment_str);
  EXPECT_EQ(input_idx, last_idx);
  EXPECT_EQ(input_size, last_size);
  EXPECT_EQ(std::size(input_array), last_array_size);
  for (int i = 0; i < std::size(input_array); ++i) {
    EXPECT_EQ(input_array[i], *(last_array + i));
  }
}

TEST_F(ConverterMockTest, DefaultBehavior) {
  ConverterInterface *converter = GetMock();

  Segments input;
  const std::string input_key = "Key";
  SetSegments(&input, "StartConversion");
  const std::string input_str = input.DebugString();
  EXPECT_FALSE(converter->StartConversion(&input, input_key));

  const std::string last_str = input.DebugString();
  EXPECT_EQ(input_str, last_str);
}

}  // namespace
}  // namespace mozc
