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

#include <string>

#include "base/util.h"
#include "converter/segments.h"
#include "rewriter/emoticon_rewriter.h"
#include "testing/base/public/gunit.h"
#include "session/config_handler.h"
#include "session/config.pb.h"

DECLARE_string(test_tmpdir);

namespace mozc {

namespace {
void AddSegment(const string &key, const string &value,
                Segments *segments) {
  segments->Clear();
  Segment *seg = segments->push_back_segment();
  seg->set_key(key);
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->value = key;
  candidate->content_key = key;
  candidate->content_value = value;
}

bool HasEmoticon(const Segments &segments) {
  CHECK_EQ(segments.segments_size(), 1);
  for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
    const Segment::Candidate &candidate = segments.segment(0).candidate(i);
    // "顔文字"
    if ("\xE9\xA1\x94\xE6\x96\x87\xE5\xAD\x97" == candidate.description) {
      return true;
    }
  }
  return false;
}
}  // namespace

class EmoticonRewriterTest : public testing::Test {
 protected:
  EmoticonRewriterTest() {}
  ~EmoticonRewriterTest() {}

  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  }

  virtual void TearDown() {}
};

TEST_F(EmoticonRewriterTest, BasicTest) {
  EmoticonRewriter emoticon_rewriter;

  {
    config::Config input;
    config::ConfigHandler::GetConfig(&input);
    input.set_use_emoticon_conversion(true);
    config::ConfigHandler::SetConfig(input);

    Segments segments;
    AddSegment("test", "test", &segments);
    emoticon_rewriter.Rewrite(&segments);
    EXPECT_FALSE(HasEmoticon(segments));

    // "かお"
    AddSegment("\xE3\x81\x8B\xE3\x81\x8A", "test", &segments);
    emoticon_rewriter.Rewrite(&segments);
    EXPECT_TRUE(HasEmoticon(segments));

    // "かおもじ"
    AddSegment("\xE3\x81\x8B\xE3\x81\x8A\xE3\x82\x82\xE3\x81\x98",
               "test", &segments);
    emoticon_rewriter.Rewrite(&segments);
    EXPECT_TRUE(HasEmoticon(segments));

    // "にこにこ"
    AddSegment("\xE3\x81\xAB\xE3\x81\x93\xE3\x81\xAB\xE3\x81\x93",
               "test", &segments);
    emoticon_rewriter.Rewrite(&segments);
    EXPECT_TRUE(HasEmoticon(segments));

    // "ふくわらい"
    AddSegment("\xE3\x81\xB5\xE3\x81\x8F\xE3\x82\x8F\xE3\x82\x89\xE3\x81\x84",
               "test", &segments);
    emoticon_rewriter.Rewrite(&segments);
    EXPECT_TRUE(HasEmoticon(segments));
  }

  {
    config::Config input;
    config::ConfigHandler::GetConfig(&input);
    input.set_use_emoticon_conversion(false);
    config::ConfigHandler::SetConfig(input);

    Segments segments;
    AddSegment("test", "test", &segments);
    emoticon_rewriter.Rewrite(&segments);
    EXPECT_FALSE(HasEmoticon(segments));

    // "かお"
    AddSegment("\xE3\x81\x8B\xE3\x81\x8A", "test", &segments);
    emoticon_rewriter.Rewrite(&segments);
    EXPECT_FALSE(HasEmoticon(segments));

    // "かおもじ"
    AddSegment("\xE3\x81\x8B\xE3\x81\x8A\xE3\x82\x82\xE3\x81\x98",
               "test", &segments);
    emoticon_rewriter.Rewrite(&segments);
    EXPECT_FALSE(HasEmoticon(segments));

    // "にこにこ"
    AddSegment("\xE3\x81\xAB\xE3\x81\x93\xE3\x81\xAB\xE3\x81\x93",
               "test", &segments);
    emoticon_rewriter.Rewrite(&segments);
    EXPECT_FALSE(HasEmoticon(segments));

    // "ふくわらい"
    AddSegment("\xE3\x81\xB5\xE3\x81\x8F\xE3\x82\x8F\xE3\x82\x89\xE3\x81\x84",
               "test", &segments);
    emoticon_rewriter.Rewrite(&segments);
    EXPECT_FALSE(HasEmoticon(segments));
  }
}
}  // namespace mozc
