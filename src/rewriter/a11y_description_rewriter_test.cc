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

#include "rewriter/a11y_description_rewriter.h"

#include <string>

#include "absl/strings/string_view.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

void AddCandidateWithValue(const absl::string_view value, Segment *segment) {
  converter::Candidate *candidate = segment->add_candidate();
  candidate->key = segment->key();
  candidate->content_key = segment->key();
  candidate->value = std::string(value);
  candidate->content_value = std::string(value);
}

// Mock data manager that returns empty a11y description data.
class NoDataMockDataManager : public testing::MockDataManager {
 public:
  void GetA11yDescriptionRewriterData(
      absl::string_view *token_array_data,
      absl::string_view *string_array_data) const override {
    *token_array_data = "";
    *string_array_data = "";
  }
};

class A11yDescriptionRewriterTest : public ::testing::Test {
 protected:
  A11yDescriptionRewriterTest()
      : rewriter_(mock_data_manager_),
        rewriter_without_data_(no_data_mock_data_manager_) {}

  const RewriterInterface *GetRewriter() { return &rewriter_; }
  const RewriterInterface *GetRewriterWithoutData() {
    return &rewriter_without_data_;
  }

 private:
  testing::MockDataManager mock_data_manager_;
  NoDataMockDataManager no_data_mock_data_manager_;
  A11yDescriptionRewriter rewriter_;
  A11yDescriptionRewriter rewriter_without_data_;
};

TEST_F(A11yDescriptionRewriterTest, WithoutData) {
  commands::Request a11y_request;
  a11y_request.set_enable_a11y_description(true);

  const ConversionRequest a11y_conv_request =
      ConversionRequestBuilder().SetRequest(a11y_request).Build();

  EXPECT_EQ(GetRewriterWithoutData()->capability(a11y_conv_request),
            RewriterInterface::NOT_AVAILABLE);
}

TEST_F(A11yDescriptionRewriterTest, FeatureDisabled) {
  ConversionRequest non_a11y_conv_request;
  commands::Request a11y_request;

  a11y_request.set_enable_a11y_description(true);
  const ConversionRequest a11y_conv_request =
      ConversionRequestBuilder().SetRequest(a11y_request).Build();

  EXPECT_EQ(GetRewriter()->capability(a11y_conv_request),
            RewriterInterface::ALL);
  EXPECT_EQ(GetRewriter()->capability(non_a11y_conv_request),
            RewriterInterface::NOT_AVAILABLE);
}

TEST_F(A11yDescriptionRewriterTest, AddA11yDescriptionForSingleCharacter) {
  const ConversionRequest request;

  Segments segments;
  Segment *segment = segments.push_back_segment();
  AddCandidateWithValue("あ", segment);
  AddCandidateWithValue("イ", segment);
  AddCandidateWithValue("ｱ", segment);
  AddCandidateWithValue("亜", segment);
  AddCandidateWithValue("ぁ", segment);
  AddCandidateWithValue("ｧ", segment);
  AddCandidateWithValue("ー", segment);
  AddCandidateWithValue("a", segment);
  AddCandidateWithValue("B", segment);
  AddCandidateWithValue("ｃ", segment);
  AddCandidateWithValue("Ｄ", segment);

  EXPECT_TRUE(GetRewriter()->Rewrite(request, &segments));
  EXPECT_EQ(segment->candidate(0).a11y_description, "あ。ヒラガナ あ");
  EXPECT_EQ(segment->candidate(1).a11y_description, "イ。カタカナ イ");
  EXPECT_EQ(segment->candidate(2).a11y_description, "ｱ。ハンカクカタカナ ｱ");
  EXPECT_EQ(segment->candidate(3).a11y_description, "亜。アネッタイ ノ ア");
  EXPECT_EQ(segment->candidate(4).a11y_description, "ぁ。ヒラガナコモジ あ");
  EXPECT_EQ(segment->candidate(5).a11y_description,
            "ｧ。ハンカクカタカナコモジ ｱ");
  EXPECT_EQ(segment->candidate(6).a11y_description, "ー。チョウオン ー");
  EXPECT_EQ(segment->candidate(7).a11y_description, "a。コモジ a");
  EXPECT_EQ(segment->candidate(8).a11y_description, "B。オオモジ B");
  EXPECT_EQ(segment->candidate(9).a11y_description, "ｃ。ゼンカクコモジ ｃ");
  EXPECT_EQ(segment->candidate(10).a11y_description, "Ｄ。ゼンカクオオモジ Ｄ");
}

TEST_F(A11yDescriptionRewriterTest, AddA11yDescriptionForMultiCharacters) {
  const ConversionRequest request;

  Segments segments;
  Segment *segment = segments.push_back_segment();
  AddCandidateWithValue("あ亜", segment);
  AddCandidateWithValue("ぁたし", segment);
  AddCandidateWithValue("ぁぃ", segment);
  AddCandidateWithValue("ｧｨ", segment);
  AddCandidateWithValue("あぃ", segment);

  EXPECT_TRUE(GetRewriter()->Rewrite(request, &segments));
  EXPECT_EQ(segment->candidate(0).a11y_description,
            "あ亜。ヒラガナ あ。アネッタイ ノ ア");
  EXPECT_EQ(segment->candidate(1).a11y_description,
            "ぁたし。ヒラガナコモジ あ。ヒラガナ たし");
  EXPECT_EQ(segment->candidate(2).a11y_description,
            "ぁぃ。ヒラガナコモジ あ。ヒラガナコモジ い");
  EXPECT_EQ(segment->candidate(3).a11y_description,
            "ｧｨ。ハンカクカタカナコモジ ｱ。ハンカクカタカナコモジ ｲ");
  EXPECT_EQ(segment->candidate(4).a11y_description, "あぃ。ヒラガナ あぃ");
}

TEST_F(A11yDescriptionRewriterTest, AddA11yDescriptionForHiraganaCharacters) {
  const ConversionRequest request;

  Segments segments;
  Segment *segment = segments.push_back_segment();
  AddCandidateWithValue("あい", segment);

  EXPECT_TRUE(GetRewriter()->Rewrite(request, &segments));
  EXPECT_EQ(segment->candidate(0).a11y_description, "あい。ヒラガナ あい");
}

TEST_F(A11yDescriptionRewriterTest, AddA11yDescriptionForUnsupportedCharacter) {
  const ConversionRequest request;

  Segments segments;
  Segment *segment = segments.push_back_segment();
  AddCandidateWithValue("☺", segment);

  EXPECT_TRUE(GetRewriter()->Rewrite(request, &segments));
  EXPECT_EQ(segment->candidate(0).a11y_description, "☺");
}

TEST_F(A11yDescriptionRewriterTest, AddA11yDescriptionForLongSound) {
  const ConversionRequest request;

  Segments segments;
  Segment *segment = segments.push_back_segment();
  AddCandidateWithValue("あー", segment);
  AddCandidateWithValue("亜ー", segment);
  AddCandidateWithValue("ーー", segment);
  AddCandidateWithValue("しーずー", segment);
  AddCandidateWithValue("シーズー", segment);
  AddCandidateWithValue("ｼｰｽﾞｰ", segment);
  AddCandidateWithValue("亜ー胃", segment);

  EXPECT_TRUE(GetRewriter()->Rewrite(request, &segments));
  EXPECT_EQ(segment->candidate(0).a11y_description, "あー。ヒラガナ あー");
  EXPECT_EQ(segment->candidate(1).a11y_description,
            "亜ー。アネッタイ ノ ア。チョウオン ー");
  EXPECT_EQ(segment->candidate(2).a11y_description, "ーー。チョウオン ーー");
  EXPECT_EQ(segment->candidate(3).a11y_description,
            "しーずー。ヒラガナ しーずー");
  EXPECT_EQ(segment->candidate(4).a11y_description,
            "シーズー。カタカナ シーズー");
  EXPECT_EQ(segment->candidate(5).a11y_description,
            "ｼｰｽﾞｰ。ハンカクカタカナ ｼｰｽﾞｰ");
  EXPECT_EQ(segment->candidate(6).a11y_description,
            "亜ー胃。アネッタイ ノ ア。チョウオン ー。イブクロ ノ イ");
}

TEST_F(A11yDescriptionRewriterTest, AddA11yDescriptionForAlphabetCharacters) {
  const ConversionRequest request;

  Segments segments;
  Segment *segment = segments.push_back_segment();
  AddCandidateWithValue("abc", segment);
  AddCandidateWithValue("Google", segment);
  AddCandidateWithValue("ｘｙｚ", segment);
  AddCandidateWithValue("Ｇｏｏｇｌｅ", segment);

  EXPECT_TRUE(GetRewriter()->Rewrite(request, &segments));
  EXPECT_EQ(segment->candidate(0).a11y_description, "abc。コモジ abc");
  EXPECT_EQ(segment->candidate(1).a11y_description,
            "Google。オオモジ G。コモジ oogle");
  EXPECT_EQ(segment->candidate(2).a11y_description,
            "ｘｙｚ。ゼンカクコモジ ｘｙｚ");
  EXPECT_EQ(segment->candidate(3).a11y_description,
            "Ｇｏｏｇｌｅ。ゼンカクオオモジ Ｇ。ゼンカクコモジ ｏｏｇｌｅ");
}

TEST_F(A11yDescriptionRewriterTest, CandidateValue) {
  // Confirm the `value` is used rather than `content_value`.
  Segments segments;
  Segment *segment = segments.push_back_segment();
  segment->set_key("あを");
  converter::Candidate *candidate = segment->add_candidate();
  candidate->key = "あを";
  candidate->content_key = "あ";
  candidate->value = "亜を";
  candidate->content_value = "亜";

  const ConversionRequest request;
  EXPECT_TRUE(GetRewriter()->Rewrite(request, &segments));
  EXPECT_EQ(segment->candidate(0).a11y_description,
            "亜を。アネッタイ ノ ア。ヒラガナ を");
}

}  // namespace
}  // namespace mozc
