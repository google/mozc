// Copyright 2010-2011, Google Inc.
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
#include "config/config_handler.h"
#include "config/config.pb.h"
#include "converter/segments.h"
#include "rewriter/usage_rewriter.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {
void AddCandidate(const string &key, const string &value,
                  const string &content_key, const string &content_value,
                  Segment *segment) {
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->Init();
  candidate->key = key;
  candidate->value = value;
  candidate->content_key = content_key;
  candidate->content_value = content_value;
}
}  // namespace

class UsageRewriterTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }
};

TEST_F(UsageRewriterTest, ConjugationTest) {
  Segments segments;
  UsageRewriter rewriter;
  Segment *seg;

  segments.Clear();
  seg = segments.push_back_segment();
  // "うたえば"
  seg->set_key("\xE3\x81\x86\xE3\x81\x9F\xE3\x81\x88\xE3\x81\xB0\x22");
  // "うたえば", "歌えば", "うたえ", "歌え",
  AddCandidate("\xE3\x81\x86\xE3\x81\x9F\xE3\x81\x88\xE3\x81\xB0\x22",
               "\xE6\xAD\x8C\xE3\x81\x88\xE3\x81\xB0",
               "\xE3\x81\x86\xE3\x81\x9F\xE3\x81\x88",
               "\xE6\xAD\x8C\xE3\x81\x88", seg);
  // "うたえば", "唱えば", "うたえ", "唄え"
  AddCandidate("\xE3\x81\x86\xE3\x81\x9F\xE3\x81\x88\xE3\x81\xB0",
               "\xE5\x94\xB1\xE3\x81\x88\xE3\x81\xB0",
               "\xE3\x81\x86\xE3\x81\x9F\xE3\x81\x88",
               "\xE5\x94\x84\xE3\x81\x88", seg);
  EXPECT_TRUE(rewriter.Rewrite(&segments));
  // "歌う"
  EXPECT_EQ("\xE6\xAD\x8C\xE3\x81\x86",
            segments.conversion_segment(0).candidate(0).usage_title);
  EXPECT_NE("", segments.conversion_segment(0).candidate(0).usage_description);
  // "唄う"
  EXPECT_EQ("\xE5\x94\x84\xE3\x81\x86",
            segments.conversion_segment(0).candidate(1).usage_title);
  EXPECT_NE("", segments.conversion_segment(0).candidate(1).usage_description);
}

TEST_F(UsageRewriterTest, SingleSegmentSingleCandidateTest) {
  Segments segments;
  UsageRewriter rewriter;
  Segment *seg;

  segments.Clear();
  seg = segments.push_back_segment();
  // "あおい"
  seg->set_key("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84");
  // "あおい", "青い", "あおい", "青い"
  AddCandidate("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
               "\xE9\x9D\x92\xE3\x81\x84",
               "\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
               "\xE9\x9D\x92\xE3\x81\x84", seg);
  EXPECT_TRUE(rewriter.Rewrite(&segments));
  // "青い"
  EXPECT_EQ("\xE9\x9D\x92\xE3\x81\x84",
            segments.conversion_segment(0).candidate(0).usage_title);
  EXPECT_NE("", segments.conversion_segment(0).candidate(0).usage_description);

  segments.Clear();
  seg = segments.push_back_segment();
  // "あおい"
  seg->set_key("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84");
  // "あおい", "あああ", "あおい", "あああ"
  AddCandidate("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
               "\xE3\x81\x82\xE3\x81\x82\xE3\x81\x82",
               "\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
               "\xE3\x81\x82\xE3\x81\x82\xE3\x81\x82", seg);
  EXPECT_FALSE(rewriter.Rewrite(&segments));
  EXPECT_EQ("", segments.conversion_segment(0).candidate(0).usage_title);
  EXPECT_EQ("", segments.conversion_segment(0).candidate(0).usage_description);
}

TEST_F(UsageRewriterTest, SingleSegmentMultiCandidatesTest) {
  Segments segments;
  UsageRewriter rewriter;
  Segment *seg;

  segments.Clear();
  seg = segments.push_back_segment();
  // "あおい"
  seg->set_key("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84");
  // "あおい", "青い", "あおい", "青い"
  AddCandidate("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
               "\xE9\x9D\x92\xE3\x81\x84",
               "\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
               "\xE9\x9D\x92\xE3\x81\x84", seg);
  // "あおい", "蒼い", "あおい", "蒼い"
  AddCandidate("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
               "\xE8\x92\xBC\xE3\x81\x84",
               "\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
               "\xE8\x92\xBC\xE3\x81\x84", seg);
  EXPECT_TRUE(rewriter.Rewrite(&segments));
  // "青い"
  EXPECT_EQ("\xE9\x9D\x92\xE3\x81\x84",
            segments.conversion_segment(0).candidate(0).usage_title);
  EXPECT_NE("", segments.conversion_segment(0).candidate(0).usage_description);
  // "蒼い"
  EXPECT_EQ("\xE8\x92\xBC\xE3\x81\x84",
            segments.conversion_segment(0).candidate(1).usage_title);
  EXPECT_NE("", segments.conversion_segment(0).candidate(1).usage_description);

  segments.Clear();
  seg = segments.push_back_segment();
  // "あおい"
  seg->set_key("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84");
  // "あおい", "青い", "あおい", "青い"
  AddCandidate("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
               "\xE9\x9D\x92\xE3\x81\x84",
               "\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
               "\xE9\x9D\x92\xE3\x81\x84", seg);
  // "あおい", "あああ", "あおい", "あああ"
  AddCandidate("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
               "\xE3\x81\x82\xE3\x81\x82\xE3\x81\x82",
               "\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
               "\xE3\x81\x82\xE3\x81\x82\xE3\x81\x82", seg);
  EXPECT_TRUE(rewriter.Rewrite(&segments));
  // "青い"
  EXPECT_EQ("\xE9\x9D\x92\xE3\x81\x84",
            segments.conversion_segment(0).candidate(0).usage_title);
  EXPECT_NE("", segments.conversion_segment(0).candidate(0).usage_description);
  EXPECT_EQ("", segments.conversion_segment(0).candidate(1).usage_title);
  EXPECT_EQ("", segments.conversion_segment(0).candidate(1).usage_description);

  segments.Clear();
  seg = segments.push_back_segment();
  // "あおい"
  seg->set_key("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84");
  // "あおい", "あああ", "あおい", "あああ"
  AddCandidate("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
               "\xE3\x81\x82\xE3\x81\x82\xE3\x81\x82",
               "\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
               "\xE3\x81\x82\xE3\x81\x82\xE3\x81\x82", seg);
  // "あおい", "青い", "あおい", "青い"
  AddCandidate("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
               "\xE9\x9D\x92\xE3\x81\x84",
               "\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
               "\xE9\x9D\x92\xE3\x81\x84", seg);
  EXPECT_TRUE(rewriter.Rewrite(&segments));
  EXPECT_EQ("", segments.conversion_segment(0).candidate(0).usage_title);
  EXPECT_EQ("", segments.conversion_segment(0).candidate(0).usage_description);
  // "青い"
  EXPECT_EQ("\xE9\x9D\x92\xE3\x81\x84",
            segments.conversion_segment(0).candidate(1).usage_title);
  EXPECT_NE("", segments.conversion_segment(0).candidate(1).usage_description);

  segments.Clear();
  seg = segments.push_back_segment();
  // "あおい"
  seg->set_key("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84");
  // "あおい", "あああ", "あおい", "あああ"
  AddCandidate("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
               "\xE3\x81\x82\xE3\x81\x82\xE3\x81\x82",
               "\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
               "\xE3\x81\x82\xE3\x81\x82\xE3\x81\x82", seg);
  // "あおい", "いいい", "あおい", "いいい"
  AddCandidate("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
               "\xE3\x81\x84\xE3\x81\x84\xE3\x81\x84",
               "\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
               "\xE3\x81\x84\xE3\x81\x84\xE3\x81\x84", seg);
  EXPECT_FALSE(rewriter.Rewrite(&segments));
  EXPECT_EQ("", segments.conversion_segment(0).candidate(0).usage_title);
  EXPECT_EQ("", segments.conversion_segment(0).candidate(0).usage_description);
  EXPECT_EQ("", segments.conversion_segment(0).candidate(1).usage_title);
  EXPECT_EQ("", segments.conversion_segment(0).candidate(1).usage_description);
}

TEST_F(UsageRewriterTest, MultiSegmentsTest) {
  Segments segments;
  UsageRewriter rewriter;
  Segment *seg;

  segments.Clear();
  seg = segments.push_back_segment();
  // "あおく"
  seg->set_key("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x8F");
  // "あおく", "青く", "あおく", "青く"
  AddCandidate("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x8F",
               "\xE9\x9D\x92\xE3\x81\x8F",
               "\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x8F",
               "\xE9\x9D\x92\xE3\x81\x8F", seg);
  // "あおく", "蒼く", "あおく", "蒼く"
  AddCandidate("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x8F",
               "\xE8\x92\xBC\xE3\x81\x8F",
               "\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x8F",
               "\xE8\x92\xBC\xE3\x81\x8F", seg);
  // "あおく", "アオク", "あおく", "アオク"
  AddCandidate("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x8F",
               "\xE3\x82\xA2\xE3\x82\xAA\xE3\x82\xAF",
               "\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x8F",
               "\xE3\x82\xA2\xE3\x82\xAA\xE3\x82\xAF", seg);
  seg = segments.push_back_segment();
  // "うたえば"
  seg->set_key("\xE3\x81\x86\xE3\x81\x9F\xE3\x81\x88\xE3\x81\xB0\x22");
  // "うたえば", "歌えば", "うたえ", "歌え",
  AddCandidate("\xE3\x81\x86\xE3\x81\x9F\xE3\x81\x88\xE3\x81\xB0\x22",
               "\xE6\xAD\x8C\xE3\x81\x88\xE3\x81\xB0",
               "\xE3\x81\x86\xE3\x81\x9F\xE3\x81\x88",
               "\xE6\xAD\x8C\xE3\x81\x88", seg);
  // "うたえば", "唱えば", "うたえ", "唄え"
  AddCandidate("\xE3\x81\x86\xE3\x81\x9F\xE3\x81\x88\xE3\x81\xB0",
               "\xE5\x94\xB1\xE3\x81\x88\xE3\x81\xB0",
               "\xE3\x81\x86\xE3\x81\x9F\xE3\x81\x88",
               "\xE5\x94\x84\xE3\x81\x88", seg);
  EXPECT_TRUE(rewriter.Rewrite(&segments));
  // "青い"
  EXPECT_EQ("\xE9\x9D\x92\xE3\x81\x84",
            segments.conversion_segment(0).candidate(0).usage_title);
  EXPECT_NE("", segments.conversion_segment(0).candidate(0).usage_description);
  // "蒼い"
  EXPECT_EQ("\xE8\x92\xBC\xE3\x81\x84",
            segments.conversion_segment(0).candidate(1).usage_title);
  EXPECT_NE("", segments.conversion_segment(0).candidate(1).usage_description);
  EXPECT_EQ("", segments.conversion_segment(0).candidate(2).usage_title);
  EXPECT_EQ("", segments.conversion_segment(0).candidate(2).usage_description);
  // "歌う"
  EXPECT_EQ("\xE6\xAD\x8C\xE3\x81\x86",
            segments.conversion_segment(1).candidate(0).usage_title);
  EXPECT_NE("", segments.conversion_segment(1).candidate(0).usage_description);
  // "唄う"
  EXPECT_EQ("\xE5\x94\x84\xE3\x81\x86",
            segments.conversion_segment(1).candidate(1).usage_title);
  EXPECT_NE("", segments.conversion_segment(1).candidate(1).usage_description);
}

TEST_F(UsageRewriterTest, SameUsageTest) {
  Segments segments;
  UsageRewriter rewriter;
  Segment *seg;
  seg = segments.push_back_segment();
  // "うたえば"
  seg->set_key("\xE3\x81\x86\xE3\x81\x9F\xE3\x81\x88\xE3\x81\xB0\x22");
  // "うたえば", "歌えば", "うたえ", "歌え",
  AddCandidate("\xE3\x81\x86\xE3\x81\x9F\xE3\x81\x88\xE3\x81\xB0\x22",
               "\xE6\xAD\x8C\xE3\x81\x88\xE3\x81\xB0",
               "\xE3\x81\x86\xE3\x81\x9F\xE3\x81\x88",
               "\xE6\xAD\x8C\xE3\x81\x88", seg);
  // "うたえば", "唱えば", "うたえ", "唄え"
  AddCandidate("\xE3\x81\x86\xE3\x81\x9F\xE3\x81\x88\xE3\x81\xB0",
               "\xE5\x94\xB1\xE3\x81\x88\xE3\x81\xB0",
               "\xE3\x81\x86\xE3\x81\x9F\xE3\x81\x88",
               "\xE5\x94\x84\xE3\x81\x88", seg);
  // "うたえば", "唱エバ", "うたえ", "唄え"
  AddCandidate("\xE3\x81\x86\xE3\x81\x9F\xE3\x81\x88\xE3\x81\xB0",
               "\xE5\x94\xB1\xE3\x82\xA8\xE3\x83\x90",
               "\xE3\x81\x86\xE3\x81\x9F\xE3\x81\x88",
               "\xE5\x94\x84\xE3\x81\x88", seg);
  EXPECT_TRUE(rewriter.Rewrite(&segments));
  // "歌う"
  EXPECT_EQ("\xE6\xAD\x8C\xE3\x81\x86",
            segments.conversion_segment(0).candidate(0).usage_title);
  EXPECT_NE("", segments.conversion_segment(0).candidate(0).usage_description);
  // "唄う"
  EXPECT_EQ("\xE5\x94\x84\xE3\x81\x86",
            segments.conversion_segment(0).candidate(1).usage_title);
  EXPECT_NE("", segments.conversion_segment(0).candidate(1).usage_description);
  // "唄う"
  EXPECT_EQ("\xE5\x94\x84\xE3\x81\x86",
            segments.conversion_segment(0).candidate(2).usage_title);
  EXPECT_NE("", segments.conversion_segment(0).candidate(2).usage_description);
  EXPECT_NE(segments.conversion_segment(0).candidate(0).usage_id,
            segments.conversion_segment(0).candidate(1).usage_id);
  EXPECT_EQ(segments.conversion_segment(0).candidate(1).usage_id,
            segments.conversion_segment(0).candidate(2).usage_id);
}
}  // mozc namespace
