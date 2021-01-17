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

#include "rewriter/normalization_rewriter.h"

#include <string>

#include "base/system_util.h"
#include "converter/segments.h"
#include "request/conversion_request.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

void AddSegment(const std::string &key, const std::string &value,
                Segments *segments) {
  segments->Clear();
  Segment *seg = segments->push_back_segment();
  seg->set_key(key);
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->value = value;
  candidate->content_value = value;
}

class NormalizationRewriterTest : public ::testing::Test {
 protected:
  NormalizationRewriterTest() = default;
  ~NormalizationRewriterTest() override = default;

  void SetUp() override {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
  }
};

TEST_F(NormalizationRewriterTest, NormalizationTest) {
  NormalizationRewriter normalization_rewriter;
  Segments segments;
  const ConversionRequest request;

  segments.Clear();
  AddSegment("test", "test", &segments);
  EXPECT_FALSE(normalization_rewriter.Rewrite(request, &segments));
  EXPECT_EQ("test", segments.segment(0).candidate(0).value);

  segments.Clear();
  AddSegment("きょうと", "京都", &segments);
  EXPECT_FALSE(normalization_rewriter.Rewrite(request, &segments));
  EXPECT_EQ("京都", segments.segment(0).candidate(0).value);

  // Wave dash (U+301C)
  segments.Clear();
  AddSegment("なみ", "〜", &segments);
#ifdef OS_WIN
  EXPECT_TRUE(normalization_rewriter.Rewrite(request, &segments));
  // U+FF5E
  EXPECT_EQ("～", segments.segment(0).candidate(0).value);
#else
  EXPECT_FALSE(normalization_rewriter.Rewrite(request, &segments));
  // U+301C
  EXPECT_EQ("〜", segments.segment(0).candidate(0).value);
#endif

  // not normalized.
  segments.Clear();
  // U+301C
  AddSegment("なみ", "〜", &segments);
  segments.mutable_segment(0)->mutable_candidate(0)->attributes |=
      Segment::Candidate::USER_DICTIONARY;
  EXPECT_FALSE(normalization_rewriter.Rewrite(request, &segments));
  // U+301C
  EXPECT_EQ("〜", segments.segment(0).candidate(0).value);
}

}  // namespace
}  // namespace mozc
