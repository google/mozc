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

#include "base/util.h"
#include "config/config_handler.h"
#include "config/config.pb.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "rewriter/single_kanji_rewriter.h"
#include "session/commands.pb.h"
#include "session/request_test_util.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {

class SingleKanjiRewriterTest : public ::testing::Test {
 protected:
  SingleKanjiRewriterTest() {
    data_manager_.reset(new testing::MockDataManager);
    pos_matcher_ = data_manager_->GetPOSMatcher();
  }

  virtual ~SingleKanjiRewriterTest() {}

  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config default_config;
    config::ConfigHandler::GetDefaultConfig(&default_config);
    config::ConfigHandler::SetConfig(default_config);
  }

  SingleKanjiRewriter *CreateSingleKanjiRewriter() const {
    return new SingleKanjiRewriter(*pos_matcher_);
  }

  const POSMatcher &pos_matcher() {
    return *pos_matcher_;
  }

 private:
  scoped_ptr<testing::MockDataManager> data_manager_;
  const POSMatcher *pos_matcher_;
};

TEST_F(SingleKanjiRewriterTest, CapabilityTest) {
  scoped_ptr<SingleKanjiRewriter> rewriter(CreateSingleKanjiRewriter());

  commands::Request request;
  request.set_mixed_conversion(false);
  commands::ScopedRequestForUnittest scoped_request(request);
  EXPECT_EQ(RewriterInterface::CONVERSION, rewriter->capability());
}

TEST_F(SingleKanjiRewriterTest, SetKeyTest) {
  scoped_ptr<SingleKanjiRewriter> rewriter(CreateSingleKanjiRewriter());

  Segments segments;
  Segment *segment = segments.add_segment();
  // "あ"
  const string kKey = "\xe3\x81\x82";
  segment->set_key(kKey);
  Segment::Candidate *candidate = segment->add_candidate();
  // First candidate may be inserted by other rewriters.
  candidate->Init();
  candidate->key = "strange key";
  candidate->content_key = "starnge key";
  candidate->value = "starnge value";
  candidate->content_value = "strange value";

  EXPECT_EQ(1, segment->candidates_size());
  rewriter->Rewrite(ConversionRequest(), &segments);
  EXPECT_GT(segment->candidates_size(), 1);
  for (size_t i = 1; i < segment->candidates_size(); ++i) {
    EXPECT_EQ(kKey, segment->candidate(i).key);
  }
}

TEST_F(SingleKanjiRewriterTest, MobileEnvironmentTest) {
  commands::Request request;
  scoped_ptr<SingleKanjiRewriter> rewriter(CreateSingleKanjiRewriter());

  {
    request.set_mixed_conversion(true);
    commands::ScopedRequestForUnittest scoped_request(request);
    EXPECT_EQ(RewriterInterface::ALL, rewriter->capability());
  }

  {
    request.set_mixed_conversion(false);
    commands::ScopedRequestForUnittest scoped_request(request);
    EXPECT_EQ(RewriterInterface::CONVERSION, rewriter->capability());
  }
}

TEST_F(SingleKanjiRewriterTest, NounPrefixTest) {
  SingleKanjiRewriter rewriter(pos_matcher());
  Segments segments;
  Segment *segment1 = segments.add_segment();

  // "み"
  segment1->set_key("\xE3\x81\xBF");
  Segment::Candidate *candidate1 = segment1->add_candidate();

  candidate1->Init();
  // candidate1->key = "み";
  // candidate1->content_key = "見";
  // candidate1->value = "見";
  // candidate1->content_value = "見";
  candidate1->key = "\xE3\x81\xBF";
  candidate1->content_key = "\xE8\xA6\x8B";
  candidate1->value = "\xE8\xA6\x8B";
  candidate1->content_value = "\xE8\xA6\x8B";

  EXPECT_EQ(1, segment1->candidates_size());
  rewriter.Rewrite(ConversionRequest(), &segments);

  // "未"
  EXPECT_EQ("\xE6\x9C\xAA", segment1->candidate(0).value);

  Segment *segment2 = segments.add_segment();

  // segment2->set_key("こうたい");
  segment2->set_key("\xE3\x81\x93\xE3\x81\x86\xE3\x81\x9F\xE3\x81\x84");
  Segment::Candidate *candidate2 = segment2->add_candidate();

  candidate2->Init();
  // candidate2->key = "こうたい";
  // candidate2->content_key = "後退";
  // candidate2->value = "後退";
  candidate2->key = "\xE3\x81\x93\xE3\x81\x86\xE3\x81\x9F\xE3\x81\x84";
  candidate2->content_key = "\xE5\xBE\x8C\xE9\x80\x80";
  candidate2->value = "\xE5\xBE\x8C\xE9\x80\x80";

  candidate2->lid = pos_matcher().GetContentWordWithConjugationId();
  candidate2->rid = pos_matcher().GetContentWordWithConjugationId();

  candidate1 = segment1->mutable_candidate(0);
  candidate1->Init();
  // candidate1->key = "み";
  // candidate1->content_key = "見";
  // candidate1->value = "見";
  // candidate1->content_value = "見";
  candidate1->key = "\xE3\x81\xBF";
  candidate1->content_key = "\xE8\xA6\x8B";
  candidate1->value = "\xE8\xA6\x8B";
  candidate1->content_value = "\xE8\xA6\x8B";

  rewriter.Rewrite(ConversionRequest(), &segments);
  // "見"
  EXPECT_EQ("\xE8\xA6\x8B", segment1->candidate(0).value);

  // Only applied when right word's POS is noun.
  candidate2->lid = pos_matcher().GetContentNounId();
  candidate2->rid = pos_matcher().GetContentNounId();

  rewriter.Rewrite(ConversionRequest(), &segments);
  // "未"
  EXPECT_EQ("\xE6\x9C\xAA", segment1->candidate(0).value);

  EXPECT_EQ(pos_matcher().GetNounPrefixId(), segment1->candidate(0).lid);
  EXPECT_EQ(pos_matcher().GetNounPrefixId(), segment1->candidate(0).rid);
}

TEST_F(SingleKanjiRewriterTest, InsertionPositionTest) {
  SingleKanjiRewriter rewriter(pos_matcher());
  Segments segments;
  Segment *segment = segments.add_segment();

  // "あ"
  segment->set_key("\xe3\x81\x82");
  for (int i = 0; i < 10; ++i) {
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->Init();
    candidate->key = segment->key();
    candidate->content_key = segment->key();
    candidate->value = Util::StringPrintf("cand%d", i);
    candidate->content_value = candidate->value;
  }

  EXPECT_EQ(10, segment->candidates_size());
  EXPECT_TRUE(rewriter.Rewrite(ConversionRequest(), &segments));
  EXPECT_LT(10, segment->candidates_size());  // Some candidates were inserted.

  for (int i = 0; i < 10; ++i) {
    // First 10 candidates have not changed.
    const Segment::Candidate &candidate = segment->candidate(i);
    EXPECT_EQ(Util::StringPrintf("cand%d", i), candidate.value);
  }
}

TEST_F(SingleKanjiRewriterTest, AddDescriptionTest) {
  SingleKanjiRewriter rewriter(pos_matcher());
  Segments segments;
  Segment *segment = segments.add_segment();

  // "あ"
  segment->set_key("\xe3\x81\x82");
  {
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->Init();
    candidate->key = segment->key();
    candidate->content_key = segment->key();
    // "亞"
    candidate->value = "\xe4\xba\x9e";  // variant of "亜".
    candidate->content_value = candidate->value;
  }

  EXPECT_EQ(1, segment->candidates_size());
  EXPECT_TRUE(segment->candidate(0).description.empty());
  EXPECT_TRUE(rewriter.Rewrite(ConversionRequest(), &segments));
  EXPECT_LT(1, segment->candidates_size());  // Some candidates were inserted.
  // "亜の旧字体"
  EXPECT_EQ("\xe4\xba\x9c\xe3\x81\xae\xe6\x97\xa7\xe5\xad\x97\xe4\xbd\x93",
            segment->candidate(0).description);
}
}  // namespace mozc
