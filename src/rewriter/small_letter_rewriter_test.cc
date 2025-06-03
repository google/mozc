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

#include "rewriter/small_letter_rewriter.h"

#include <array>
#include <cstddef>
#include <memory>
#include <optional>

#include "absl/strings/string_view.h"
#include "base/strings/assign.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "engine/engine.h"
#include "engine/mock_data_engine_factory.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

void AddSegment(absl::string_view key, absl::string_view value,
                Segments *segments) {
  Segment *seg = segments->add_segment();
  converter::Candidate *candidate = seg->add_candidate();
  seg->set_key(key);
  strings::Assign(candidate->content_key, key);
  strings::Assign(candidate->value, value);
  strings::Assign(candidate->content_value, value);
}

void InitSegments(absl::string_view key, absl::string_view value,
                  Segments *segments) {
  segments->Clear();
  AddSegment(key, value, segments);
}

bool ContainCandidate(const Segments &segments,
                      const absl::string_view candidate) {
  const Segment &segment = segments.segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (candidate == segment.candidate(i).value) {
      return true;
    }
  }
  return false;
}

class SmallLetterRewriterTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override { engine_ = MockDataEngineFactory::Create().value(); }

  std::unique_ptr<Engine> engine_;
  const commands::Request &default_request() const { return default_request_; }
  const config::Config &default_config() const { return default_config_; }

 private:
  const commands::Request default_request_;
  const config::Config default_config_;
};

TEST_F(SmallLetterRewriterTest, ScriptConversionTest) {
  Segments segments;
  SmallLetterRewriter rewriter;

  struct InputOutputData {
    absl::string_view input;
    absl::string_view output;
  };

  constexpr InputOutputData kInputOutputData[] = {
      // Superscript
      {"^123", "¹²³"},
      {"^4", "⁴"},
      {"^56789", "⁵⁶⁷⁸⁹"},
      {"^2^+^(^3^-^1^)^=", "²⁺⁽³⁻¹⁾⁼"},

      // Subscript
      {"_123", "₁₂₃"},
      {"_4", "₄"},
      {"_56789", "₅₆₇₈₉"},
      {"_2_+_(_3_-_1_)_=", "₂₊₍₃₋₁₎₌"},

      // Math Formula
      {"x^2+y^2=z^2", "x²+y²=z²"},

      // Chemical Formula
      {"Na_2CO_3", "Na₂CO₃"},
      {"C_6H_12O_6", "C₆H₁₂O₆"},
      {"(NH_4)_2CO_3", "(NH₄)₂CO₃"},
      {"2Na_2CO_3", "2Na₂CO₃"},
      {"2H_2O", "2H₂O"},
      {"O^2^-", "O²⁻"},

      // Others
      {"O^2-", "O²-"},
      {"O^X_2", "O^X₂"},
      {"_2O^", "₂O^"},
      {"あ^2", "あ²"},
  };

  constexpr absl::string_view kMozcUnsupportedInput[] = {
      // Roman alphabet superscript
      "^n",
      "^x",
      "^a",
      // Roman alphabet subscript
      "_m",
      "_y",
      "_b",

      // Multibyte characters
      "_あ",
      "_⏰",

      // Formula without explicit prefix is not supported
      "H2O",
      "Na+",
      "NH4+",
      "C2O42-",
      "AKB48",

      // Others
      "あ^あ",
      "x^^x",
  };

  // Test behavior for each test cases in kInputOutputData.
  for (const InputOutputData &item : kInputOutputData) {
    InitSegments(item.input, item.input, &segments);
    const ConversionRequest request =
        ConversionRequestBuilder().SetKey(item.input).Build();
    std::optional<RewriterInterface::ResizeSegmentsRequest> resize_request =
        rewriter.CheckResizeSegmentsRequest(request, segments);
    EXPECT_FALSE(resize_request.has_value());
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_TRUE(ContainCandidate(segments, item.output));
  }

  // Mozc does not accept some superscript/subscript supported in Unicode
  for (const absl::string_view &item : kMozcUnsupportedInput) {
    InitSegments(item, item, &segments);
    const ConversionRequest request =
        ConversionRequestBuilder().SetKey(item).Build();
    std::optional<RewriterInterface::ResizeSegmentsRequest> resize_request =
        rewriter.CheckResizeSegmentsRequest(request, segments);
    EXPECT_FALSE(resize_request.has_value());
    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
  }

  constexpr std::array<absl::string_view, 4> kInvalidInput = {"^", "_", "12345",
                                                              "^^12345"};
  for (absl::string_view invalid_input : kInvalidInput) {
    InitSegments("^", "^", &segments);
    const ConversionRequest request =
        ConversionRequestBuilder().SetKey(invalid_input).Build();
    std::optional<RewriterInterface::ResizeSegmentsRequest> resize_request =
        rewriter.CheckResizeSegmentsRequest(request, segments);
    EXPECT_FALSE(resize_request.has_value());
    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
  }
}

TEST_F(SmallLetterRewriterTest, MultipleSegment) {
  Segments segments;
  SmallLetterRewriter rewriter;
  const ConversionRequest request;

  {
    // Multiple segments are combined.
    InitSegments("^123", "^123", &segments);
    AddSegment("45", "45", &segments);
    AddSegment("6", "6", &segments);
    const ConversionRequest request =
        ConversionRequestBuilder().SetKey("^123456").Build();
    std::optional<RewriterInterface::ResizeSegmentsRequest> resize_request =
        rewriter.CheckResizeSegmentsRequest(request, segments);
    ASSERT_TRUE(resize_request.has_value());
    EXPECT_EQ(resize_request->segment_index, 0);
    EXPECT_EQ(resize_request->segment_sizes[0], 7);
    EXPECT_EQ(resize_request->segment_sizes[1], 0);
  }
  {
    // If the segments is already resized, returns false.
    InitSegments("^123", "^123", &segments);
    AddSegment("^123", "^123", &segments);
    segments.set_resized(true);
    const ConversionRequest request =
        ConversionRequestBuilder().SetKey("^123").Build();
    std::optional<RewriterInterface::ResizeSegmentsRequest> resize_request =
        rewriter.CheckResizeSegmentsRequest(request, segments);
    EXPECT_FALSE(resize_request.has_value());
  }
  {
    // History segment has to be ignored.
    // In this case 1st segment is HISTORY
    // so this rewriting returns true.
    InitSegments("^123", "^123", &segments);
    AddSegment("^123", "^123", &segments);
    segments.set_resized(true);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
    const ConversionRequest request =
        ConversionRequestBuilder().SetKey("^123").Build();
    std::optional<RewriterInterface::ResizeSegmentsRequest> resize_request =
        rewriter.CheckResizeSegmentsRequest(request, segments);
    EXPECT_FALSE(resize_request.has_value());
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidate(1).value, "¹²³");
  }
}

}  // namespace
}  // namespace mozc
