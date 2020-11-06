// Copyright 2010-2020, Google Inc.
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

#include "rewriter/command_rewriter.h"

#include <string>

#include "base/system_util.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

size_t CommandCandidatesSize(const Segment &segment) {
  size_t result = 0;
  for (int i = 0; i < segment.candidates_size(); ++i) {
    if (segment.candidate(i).attributes &
        Segment::Candidate::COMMAND_CANDIDATE) {
      result++;
    }
  }
  return result;
}

string GetCommandCandidateValue(const Segment &segment) {
  for (int i = 0; i < segment.candidates_size(); ++i) {
    if (segment.candidate(i).attributes &
        Segment::Candidate::COMMAND_CANDIDATE) {
      return segment.candidate(i).value;
    }
  }
  return "";
}

class CommandRewriterTest : public ::testing::Test {
 protected:
  CommandRewriterTest() {
    convreq_.set_request(&request_);
    convreq_.set_config(&config_);
  }

  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::ConfigHandler::GetDefaultConfig(&config_);
    request_.Clear();
  }

  virtual void TearDown() {
    config::ConfigHandler::GetDefaultConfig(&config_);
    request_.Clear();
  }

  ConversionRequest convreq_;
  commands::Request request_;
  config::Config config_;
};

TEST_F(CommandRewriterTest, Rewrite) {
  CommandRewriter rewriter;
  Segments segments;
  Segment *seg = segments.push_back_segment();

  EXPECT_FALSE(rewriter.Rewrite(convreq_, &segments));

  {
    Segment::Candidate *candidate = seg->add_candidate();
    seg->set_key("こまんど");
    candidate->value = "コマンド";
    EXPECT_TRUE(rewriter.Rewrite(convreq_, &segments));
    EXPECT_EQ(2, CommandCandidatesSize(*seg));
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    seg->set_key("さじぇすと");
    candidate->value = "サジェスト";
    EXPECT_TRUE(rewriter.Rewrite(convreq_, &segments));
    EXPECT_EQ(1, CommandCandidatesSize(*seg));
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    seg->set_key("ひみつ");
    candidate->value = "秘密";
    EXPECT_TRUE(rewriter.Rewrite(convreq_, &segments));
    EXPECT_EQ(1, CommandCandidatesSize(*seg));
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    seg->set_key("きょうと");
    candidate->value = "京都";
    EXPECT_FALSE(rewriter.Rewrite(convreq_, &segments));
    EXPECT_EQ(0, CommandCandidatesSize(*seg));
    seg->clear_candidates();
  }

  {
    // don't trigger when multiple segments.
    Segment::Candidate *candidate = seg->add_candidate();
    seg->set_key("こまんど");
    candidate->value = "コマンド";
    Segment *seg2 = segments.push_back_segment();
    Segment::Candidate *candidate2 = seg2->add_candidate();
    seg2->set_key("です");
    candidate2->value = "です";
    EXPECT_FALSE(rewriter.Rewrite(convreq_, &segments));
    EXPECT_EQ(0, CommandCandidatesSize(*seg));
  }
}

TEST_F(CommandRewriterTest, ValueCheck) {
  CommandRewriter rewriter;
  Segments segments;
  Segment *seg = segments.push_back_segment();

  {
    Segment::Candidate *candidate = seg->add_candidate();
    seg->set_key("さじぇすと");
    candidate->value = "サジェスト";
    config_.set_presentation_mode(false);
    EXPECT_TRUE(rewriter.Rewrite(convreq_, &segments));
    EXPECT_EQ("サジェスト機能の一時停止", GetCommandCandidateValue(*seg));
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    seg->set_key("さじぇすと");
    candidate->value = "サジェスト";
    config_.set_presentation_mode(true);
    EXPECT_TRUE(rewriter.Rewrite(convreq_, &segments));
    EXPECT_EQ("サジェスト機能を元に戻す", GetCommandCandidateValue(*seg));
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    seg->set_key("ひみつ");
    candidate->value = "秘密";
    config_.set_incognito_mode(false);
    EXPECT_TRUE(rewriter.Rewrite(convreq_, &segments));
    EXPECT_EQ("シークレットモードをオン", GetCommandCandidateValue(*seg));
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    seg->set_key("ひみつ");
    candidate->value = "秘密";
    config_.set_incognito_mode(true);
    EXPECT_TRUE(rewriter.Rewrite(convreq_, &segments));
    EXPECT_EQ("シークレットモードをオフ", GetCommandCandidateValue(*seg));
    seg->clear_candidates();
  }
}

}  // namespace
}  // namespace mozc
