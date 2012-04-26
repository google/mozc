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

#include "rewriter/usage_rewriter.h"

#include <string>
#include "base/base.h"
#include "base/singleton.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
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

  virtual void TearDown() {
    // just in case, reset the config in test_tmpdir
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

  UsageRewriter *CreateUsageRewriter() const {
    return new UsageRewriter(Singleton<POSMatcher>::get());
  }
};

TEST_F(UsageRewriterTest, CapabilityTest) {
  scoped_ptr<UsageRewriter> rewriter(CreateUsageRewriter());
  EXPECT_EQ(RewriterInterface::CONVERSION |
            RewriterInterface::PREDICTION,
            rewriter->capability());
}

TEST_F(UsageRewriterTest, ConjugationTest) {
  Segments segments;
  scoped_ptr<UsageRewriter> rewriter(CreateUsageRewriter());
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
  EXPECT_TRUE(rewriter->Rewrite(ConversionRequest(), &segments));
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
  scoped_ptr<UsageRewriter> rewriter(CreateUsageRewriter());
  Segment *seg;
  const ConversionRequest request;

  segments.Clear();
  seg = segments.push_back_segment();
  // "あおい"
  seg->set_key("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84");
  // "あおい", "青い", "あおい", "青い"
  AddCandidate("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
               "\xE9\x9D\x92\xE3\x81\x84",
               "\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
               "\xE9\x9D\x92\xE3\x81\x84", seg);
  EXPECT_TRUE(rewriter->Rewrite(request, &segments));
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
  EXPECT_FALSE(rewriter->Rewrite(request, &segments));
  EXPECT_EQ("", segments.conversion_segment(0).candidate(0).usage_title);
  EXPECT_EQ("", segments.conversion_segment(0).candidate(0).usage_description);
}

TEST_F(UsageRewriterTest, ConfigTest) {
  Segments segments;
  scoped_ptr<UsageRewriter> rewriter(CreateUsageRewriter());
  Segment *seg;
  const ConversionRequest request;

  // Default setting
  {
    segments.Clear();
    seg = segments.push_back_segment();
    // "あおい"
    seg->set_key("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84");
    // "あおい", "青い", "あおい", "青い"
    AddCandidate("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
                 "\xE9\x9D\x92\xE3\x81\x84",
                 "\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
                 "\xE9\x9D\x92\xE3\x81\x84", seg);
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
  }

  {
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config.mutable_information_list_config()->
        set_use_local_usage_dictionary(false);
    config::ConfigHandler::SetConfig(config);

    segments.Clear();
    seg = segments.push_back_segment();
    // "あおい"
    seg->set_key("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84");
    // "あおい", "青い", "あおい", "青い"
    AddCandidate("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
                 "\xE9\x9D\x92\xE3\x81\x84",
                 "\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
                 "\xE9\x9D\x92\xE3\x81\x84", seg);
    EXPECT_FALSE(rewriter->Rewrite(request, &segments));
  }

  {
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config.mutable_information_list_config()->
        set_use_local_usage_dictionary(true);
    config::ConfigHandler::SetConfig(config);

    segments.Clear();
    seg = segments.push_back_segment();
    // "あおい"
    seg->set_key("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84");
    // "あおい", "青い", "あおい", "青い"
    AddCandidate("\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
                 "\xE9\x9D\x92\xE3\x81\x84",
                 "\xE3\x81\x82\xE3\x81\x8A\xE3\x81\x84",
                 "\xE9\x9D\x92\xE3\x81\x84", seg);
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
  }
}

TEST_F(UsageRewriterTest, SingleSegmentMultiCandidatesTest) {
  Segments segments;
  scoped_ptr<UsageRewriter> rewriter(CreateUsageRewriter());
  Segment *seg;
  const ConversionRequest request;

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
  EXPECT_TRUE(rewriter->Rewrite(request, &segments));
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
  EXPECT_TRUE(rewriter->Rewrite(request, &segments));
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
  EXPECT_TRUE(rewriter->Rewrite(request, &segments));
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
  EXPECT_FALSE(rewriter->Rewrite(request, &segments));
  EXPECT_EQ("", segments.conversion_segment(0).candidate(0).usage_title);
  EXPECT_EQ("", segments.conversion_segment(0).candidate(0).usage_description);
  EXPECT_EQ("", segments.conversion_segment(0).candidate(1).usage_title);
  EXPECT_EQ("", segments.conversion_segment(0).candidate(1).usage_description);
}

TEST_F(UsageRewriterTest, MultiSegmentsTest) {
  Segments segments;
  scoped_ptr<UsageRewriter> rewriter(CreateUsageRewriter());
  Segment *seg;
  const ConversionRequest request;

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
  EXPECT_TRUE(rewriter->Rewrite(request, &segments));
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
  scoped_ptr<UsageRewriter> rewriter(CreateUsageRewriter());
  Segment *seg;
  const ConversionRequest request;

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
  EXPECT_TRUE(rewriter->Rewrite(request, &segments));
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

TEST_F(UsageRewriterTest, GetKanjiPrefixAndOneHiragana) {
  //  EXPECT_EQ("合わ",
  // UsageRewriter::GetKanjiPrefixAndOneHiragana("合わせる"));
  EXPECT_EQ("\xE5\x90\x88\xE3\x82\x8F",
            UsageRewriter::GetKanjiPrefixAndOneHiragana(
                "\xE5\x90\x88\xE3\x82\x8F\xE3\x81\x9B\xE3\x82\x8B"));

  // EXPECT_EQ("合う",
  // UsageRewriter::GetKanjiPrefixAndOneHiragana("合う"));
  EXPECT_EQ("\xE5\x90\x88\xE3\x81\x86",
            UsageRewriter::GetKanjiPrefixAndOneHiragana(
                "\xE5\x90\x88\xE3\x81\x86"));

  // EXPECT_EQ("合合わ",
  // UsageRewriter::GetKanjiPrefixAndOneHiragana("合合わせる"));
  EXPECT_EQ("\xE5\x90\x88\xE5\x90\x88\xE3\x82\x8F",
            UsageRewriter::GetKanjiPrefixAndOneHiragana(
                "\xE5\x90\x88\xE5\x90\x88\xE3\x82\x8F\xE3\x81\x9B"
                "\xE3\x82\x8B"));

  // EXPECT_EQ("", UsageRewriter::GetKanjiPrefixAndOneHiragana("合"));
  EXPECT_EQ("", UsageRewriter::GetKanjiPrefixAndOneHiragana("\xE5\x90\x88"));

  // EXPECT_EQ("", UsageRewriter::GetKanjiPrefixAndOneHiragana("京都"));
  EXPECT_EQ("", UsageRewriter::GetKanjiPrefixAndOneHiragana(
      "\xE4\xBA\xAC\xE9\x83\xBD"));

  // EXPECT_EQ("",
  // UsageRewriter::GetKanjiPrefixAndOneHiragana("合合合わせる"));
  EXPECT_EQ("", UsageRewriter::GetKanjiPrefixAndOneHiragana(
      "\xE5\x90\x88\xE5\x90\x88\xE5\x90\x88\xE3\x82\x8F"
      "\xE3\x81\x9B\xE3\x82\x8B"));

  // EXPECT_EQ("",
  // UsageRewriter::GetKanjiPrefixAndOneHiragana("カタカナ"));
  EXPECT_EQ("", UsageRewriter::GetKanjiPrefixAndOneHiragana(
      "\xE3\x82\xAB\xE3\x82\xBF\xE3\x82\xAB\xE3\x83\x8A"));

  EXPECT_EQ("", UsageRewriter::GetKanjiPrefixAndOneHiragana("abc"));

  // EXPECT_EQ("",
  // UsageRewriter::GetKanjiPrefixAndOneHiragana("あ合わせる"));
  EXPECT_EQ("", UsageRewriter::GetKanjiPrefixAndOneHiragana(
      "\xE3\x81\x82\xE5\x90\x88\xE3\x82\x8F\xE3\x81\x9B\xE3\x82\x8B"));
}
}  // namespace mozc
