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

#include "rewriter/single_hentaigana_rewriter.h"

#include <memory>
#include <string>
#include <vector>

#include "base/system_util.h"
#include "converter/segments.h"
#include "engine/mock_data_engine_factory.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

void AddSegment(const std::string &key, const std::vector<std::string> &values,
                Segments *segments) {
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  for (const std::string &value : values) {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->content_key = key;
    candidate->value = value;
    candidate->content_value = value;
  }
}

bool ContainCandidate(const Segments &segments, const std::string &value,
                      const std::string &description) {
  const Segment &segment = segments.segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (value == segment.candidate(i).value &&
        description == segment.candidate(i).description) {
      return true;
    }
  }
  return false;
}

}  // namespace

class SingleHentaiganaRewriterTest : public ::testing::Test {
 protected:
  SingleHentaiganaRewriterTest() {}
  ~SingleHentaiganaRewriterTest() override {}
  std::unique_ptr<EngineInterface> engine_;

  void SetUp() override {
    SystemUtil::SetUserProfileDirectory(absl::GetFlag(FLAGS_test_tmpdir));
    engine_ = MockDataEngineFactory::Create().value();
  }
};

TEST_F(SingleHentaiganaRewriterTest, RewriteTest) {
  SingleHentaiganaRewriter rewriter(engine_->GetConverter());
  rewriter.SetEnabled(true);
  commands::Request request;
  ConversionRequest default_request;
  default_request.set_request(&request);
  {
    Segments segments;
    AddSegment("あ", {"あ"}, &segments);
    EXPECT_TRUE(rewriter.Rewrite(default_request, &segments));
    EXPECT_TRUE(ContainCandidate(segments, "\U0001B002", "安の変体仮名"));
    EXPECT_TRUE(ContainCandidate(segments, "\U0001B003", "愛の変体仮名"));
    EXPECT_FALSE(ContainCandidate(segments, "\U0001B007", "伊の変体仮名"));
  }
  {
    Segments segments;
    AddSegment("いぇ", {"いぇ"}, &segments);
    EXPECT_TRUE(rewriter.Rewrite(default_request, &segments));
    EXPECT_TRUE(ContainCandidate(segments, "\U0001B001", "江の変体仮名"));
    EXPECT_TRUE(ContainCandidate(segments, "\U0001B121", "変体仮名"));
  }
}

TEST_F(SingleHentaiganaRewriterTest, NoRewriteTest) {
  SingleHentaiganaRewriter rewriter(engine_->GetConverter());
  rewriter.SetEnabled(false);

  commands::Request request;
  ConversionRequest default_request;
  default_request.set_request(&request);
  {
    Segments segments;
    AddSegment("あ", {"あ"}, &segments);
    EXPECT_FALSE(rewriter.Rewrite(default_request, &segments));
    EXPECT_EQ(1, segments.conversion_segment(0).candidates_size());
    EXPECT_FALSE(ContainCandidate(segments, "\U0001B002", "安の変体仮名"));
    EXPECT_FALSE(ContainCandidate(segments, "\U0001B003", "愛の変体仮名"));
  }
  {
    Segments segments;
    AddSegment("いぇ", {"いぇ"}, &segments);
    EXPECT_FALSE(rewriter.Rewrite(default_request, &segments));
    EXPECT_EQ(1, segments.conversion_segment(0).candidates_size());
    EXPECT_FALSE(ContainCandidate(segments, "\U0001B001", "江の変体仮名"));
    EXPECT_FALSE(ContainCandidate(segments, "\U0001B121", "変体仮名"));
  }
}

}  // namespace mozc
