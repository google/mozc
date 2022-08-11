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

#include <iterator>
#include <memory>
#include <string>

#include "base/system_util.h"
#include "engine/mock_data_engine_factory.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

void AddSegment(const std::string &key, const std::string &value,
                Segments *segments) {
  Segment *seg = segments->add_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  seg->set_key(key);
  candidate->content_key = key;
  candidate->value = value;
  candidate->content_value = value;
}

void InitSegments(const std::string &key, const std::string &value,
                  Segments *segments) {
  segments->Clear();
  AddSegment(key, value, segments);
}

bool ContainCandidate(const Segments &segments, const std::string &candidate) {
  const Segment &segment = segments.segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (candidate == segment.candidate(i).value) {
      return true;
    }
  }
  return false;
}

}  // namespace

class SmallLetterRewriterTest : public ::testing::Test {
 protected:
  // Workaround for C2512 error (no default appropriate constructor) on MSVS.
  SmallLetterRewriterTest() {}
  ~SmallLetterRewriterTest() override {}

  void SetUp() override {
    SystemUtil::SetUserProfileDirectory(absl::GetFlag(FLAGS_test_tmpdir));
    engine_ = MockDataEngineFactory::Create().value();
  }

  std::unique_ptr<EngineInterface> engine_;
  const commands::Request &default_request() const { return default_request_; }
  const config::Config &default_config() const { return default_config_; }

 private:
  const commands::Request default_request_;
  const config::Config default_config_;
};

TEST_F(SmallLetterRewriterTest, ScriptConversionTest) {
  Segments segments;
  SmallLetterRewriter rewriter(engine_->GetConverter());
  const ConversionRequest request;

  struct InputOutputData {
    const char *input;
    const char *output;
  };

  const InputOutputData kInputOutputData[] = {
      // Superscript
      {"^123", "¹²³"},
      {"^4", "⁴"},
      {"^56789", "⁵⁶⁷⁸⁹"},

      // Subscript
      {"_123", "₁₂₃"},
      {"_4", "₄"},
      {"_56789", "₅₆₇₈₉"},
  };

  const char *kMozcUnsupportedInput[] = {
      // Roman alhapet superscript/subscript
      "^n",
      "^x",
      "^a",
      "^2x",

      "_m",
      "_y",
      "_b",
      "_2y",

      // Symbol superscript/subscript
      "^+",
      "_-",
  };

  // Allowed cases
  for (size_t i = 0; i < std::size(kInputOutputData); ++i) {
    InitSegments(kInputOutputData[i].input, kInputOutputData[i].input,
                 &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_TRUE(ContainCandidate(segments, kInputOutputData[i].output));
  }

  // Mozc does not accept some superscript/subscript supported in Unicode
  for (size_t i = 0; i < std::size(kMozcUnsupportedInput); ++i) {
    InitSegments(kMozcUnsupportedInput[i], kMozcUnsupportedInput[i], &segments);
    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
  }

  // Invalid style input
  InitSegments("^", "^", &segments);
  EXPECT_FALSE(rewriter.Rewrite(request, &segments));

  InitSegments("_", "_", &segments);
  EXPECT_FALSE(rewriter.Rewrite(request, &segments));

  InitSegments("12345", "12345", &segments);
  EXPECT_FALSE(rewriter.Rewrite(request, &segments));

  InitSegments("^^12345", "^^12345", &segments);
  EXPECT_FALSE(rewriter.Rewrite(request, &segments));
}

TEST_F(SmallLetterRewriterTest, MultipleSegment) {
  Segments segments;
  SmallLetterRewriter rewriter(engine_->GetConverter());
  const ConversionRequest request;

  // Multiple segments are combined.
  InitSegments("^123", "^123", &segments);
  AddSegment("45", "45", &segments);
  AddSegment("6", "6", &segments);
  EXPECT_TRUE(rewriter.Rewrite(request, &segments));
  EXPECT_EQ(1, segments.conversion_segments_size());
  EXPECT_EQ("¹²³⁴⁵⁶", segments.conversion_segment(0).candidate(0).value);

  // If the segments is already resized, returns false.
  InitSegments("^123", "^123", &segments);
  AddSegment("^123", "^123", &segments);
  segments.set_resized(true);
  EXPECT_FALSE(rewriter.Rewrite(request, &segments));

  // History segment has to be ignored.
  // In this case 1st segment is HISTORY
  // so this rewriting returns true.
  InitSegments("^123", "^123", &segments);
  AddSegment("^123", "^123", &segments);
  segments.set_resized(true);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
  EXPECT_TRUE(rewriter.Rewrite(request, &segments));
  EXPECT_EQ("¹²³", segments.conversion_segment(0).candidate(0).value);
}

}  // namespace mozc
