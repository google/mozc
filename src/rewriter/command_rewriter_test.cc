// Copyright 2010-2012, Google Inc.
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

#include <string>

#include "base/util.h"
#include "converter/segments.h"
#include "rewriter/command_rewriter.h"
#include "testing/base/public/gunit.h"
#include "config/config_handler.h"
#include "config/config.pb.h"

DECLARE_string(test_tmpdir);

namespace mozc {

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

class CommandRewriterTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

  virtual void TearDown() {
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }
};

TEST_F(CommandRewriterTest, Rewrite) {
  CommandRewriter rewriter;
  Segments segments;

  Segment *seg = segments.push_back_segment();

  EXPECT_FALSE(rewriter.Rewrite(&segments));

  {
    Segment::Candidate *candidate = seg->add_candidate();
    // seg->set_key("こまんど");
    // candidate->value = "コマンド";
    seg->set_key("\xE3\x81\x93\xE3\x81\xBE\xE3\x82\x93\xE3\x81\xA9");
    candidate->value = "\xE3\x82\xB3\xE3\x83\x9E"
        "\xE3\x83\xB3\xE3\x83\x89";
    EXPECT_TRUE(rewriter.Rewrite(&segments));
    EXPECT_EQ(2, CommandCandidatesSize(*seg));
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    // seg->set_key("さじぇすと");
    // candidate->value = "サジェスト";
    seg->set_key("\xE3\x81\x95\xE3\x81\x98\xE3\x81\x87"
                 "\xE3\x81\x99\xE3\x81\xA8");
    candidate->value = "\xE3\x82\xB5\xE3\x82\xB8\xE3\x82\xA7"
        "\xE3\x82\xB9\xE3\x83\x88";
    EXPECT_TRUE(rewriter.Rewrite(&segments));
    EXPECT_EQ(1, CommandCandidatesSize(*seg));
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    // seg->set_key("ひみつ");
    // candidate->value = "秘密";
    seg->set_key("\xE3\x81\xB2\xE3\x81\xBF\xE3\x81\xA4");
    candidate->value = "\xE7\xA7\x98\xE5\xAF\x86";
    EXPECT_TRUE(rewriter.Rewrite(&segments));
    EXPECT_EQ(1, CommandCandidatesSize(*seg));
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    // seg->set_key("きょうと");
    // candidate->value = "京都";
    seg->set_key("\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x81\xA8");
    candidate->value = "\xE4\xBA\xAC\xE9\x83\xBD";
    EXPECT_FALSE(rewriter.Rewrite(&segments));
    EXPECT_EQ(0, CommandCandidatesSize(*seg));
    seg->clear_candidates();
  }

  {
    // don't trigger when multiple segments.
    Segment::Candidate *candidate = seg->add_candidate();
    // seg->set_key("こまんど");
    // candidate->value = "コマンド";
    seg->set_key("\xE3\x81\x93\xE3\x81\xBE\xE3\x82\x93\xE3\x81\xA9");
    candidate->value = "\xE3\x82\xB3\xE3\x83\x9E\xE3\x83\xB3\xE3\x83\x89";
    Segment *seg2 = segments.push_back_segment();
    Segment::Candidate *candidate2 = seg2->add_candidate();
    // seg2->set_key("です");
    // candidate2->value = "です";
    seg2->set_key("\xE3\x81\xA7\xE3\x81\x99");
    candidate2->value = "\xE3\x81\xA7\xE3\x81\x99";
    EXPECT_FALSE(rewriter.Rewrite(&segments));
    EXPECT_EQ(0, CommandCandidatesSize(*seg));
  }
}

TEST_F(CommandRewriterTest, ValueCheck) {
  CommandRewriter rewriter;
  Segments segments;
  config::Config config;

  Segment *seg = segments.push_back_segment();

  {
    Segment::Candidate *candidate = seg->add_candidate();
    // seg->set_key("さじぇすと");
    // candidate->value = "サジェスト";
    seg->set_key("\xE3\x81\x95\xE3\x81\x98\xE3\x81\x87"
                 "\xE3\x81\x99\xE3\x81\xA8");
    candidate->value = "\xE3\x82\xB5\xE3\x82\xB8\xE3\x82\xA7"
        "\xE3\x82\xB9\xE3\x83\x88";
    config.set_presentation_mode(false);
    config::ConfigHandler::SetConfig(config);
    EXPECT_TRUE(rewriter.Rewrite(&segments));
    // EXPECT_EQ("サジェスト機能の一時停止",
    // GetCommandCandidateValue(*seg));
    EXPECT_EQ("\xE3\x82\xB5\xE3\x82\xB8\xE3\x82\xA7"
              "\xE3\x82\xB9\xE3\x83\x88\xE6\xA9\x9F"
              "\xE8\x83\xBD\xE3\x81\xAE\xE4\xB8\x80"
              "\xE6\x99\x82\xE5\x81\x9C\xE6\xAD\xA2",
              GetCommandCandidateValue(*seg));
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    // seg->set_key("さじぇすと");
    // candidate->value = "サジェスト";
    seg->set_key("\xE3\x81\x95\xE3\x81\x98\xE3\x81\x87"
                 "\xE3\x81\x99\xE3\x81\xA8");
    candidate->value = "\xE3\x82\xB5\xE3\x82\xB8\xE3\x82\xA7"
        "\xE3\x82\xB9\xE3\x83\x88";
    config.set_presentation_mode(true);
    config::ConfigHandler::SetConfig(config);
    EXPECT_TRUE(rewriter.Rewrite(&segments));
    // EXPECT_EQ("サジェスト機能を元に戻す",
    /// GetCommandCandidateValue(*seg));
    EXPECT_EQ("\xE3\x82\xB5\xE3\x82\xB8\xE3\x82\xA7"
              "\xE3\x82\xB9\xE3\x83\x88\xE6\xA9\x9F"
              "\xE8\x83\xBD\xE3\x82\x92\xE5\x85\x83"
              "\xE3\x81\xAB\xE6\x88\xBB\xE3\x81\x99",
              GetCommandCandidateValue(*seg));
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    // seg->set_key("ひみつ");
    // candidate->value = "秘密";
    seg->set_key("\xE3\x81\xB2\xE3\x81\xBF\xE3\x81\xA4");
    candidate->value = "\xE7\xA7\x98\xE5\xAF\x86";
    config.set_incognito_mode(false);
    config::ConfigHandler::SetConfig(config);
    EXPECT_TRUE(rewriter.Rewrite(&segments));
    // EXPECT_EQ("シークレットモードをオン",
    // GetCommandCandidateValue(*seg));
    EXPECT_EQ("\xE3\x82\xB7\xE3\x83\xBC\xE3\x82\xAF"
              "\xE3\x83\xAC\xE3\x83\x83\xE3\x83\x88"
              "\xE3\x83\xA2\xE3\x83\xBC\xE3\x83\x89"
              "\xE3\x82\x92\xE3\x82\xAA\xE3\x83\xB3",
              GetCommandCandidateValue(*seg));
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    // seg->set_key("ひみつ");
    // candidate->value = "秘密";
    seg->set_key("\xE3\x81\xB2\xE3\x81\xBF\xE3\x81\xA4");
    candidate->value = "\xE7\xA7\x98\xE5\xAF\x86";
    config.set_incognito_mode(true);
    config::ConfigHandler::SetConfig(config);
    EXPECT_TRUE(rewriter.Rewrite(&segments));
    // EXPECT_EQ("シークレットモードをオフ",
    //               GetCommandCandidateValue(*seg));
    EXPECT_EQ("\xE3\x82\xB7\xE3\x83\xBC\xE3\x82\xAF"
              "\xE3\x83\xAC\xE3\x83\x83\xE3\x83\x88"
              "\xE3\x83\xA2\xE3\x83\xBC\xE3\x83\x89"
              "\xE3\x82\x92\xE3\x82\xAA\xE3\x83\x95",
              GetCommandCandidateValue(*seg));
    seg->clear_candidates();
  }
}
}  // namespace mozc
