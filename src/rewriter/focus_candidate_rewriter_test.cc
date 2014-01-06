// Copyright 2010-2014, Google Inc.
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

#include "rewriter/focus_candidate_rewriter.h"

#include <string>
#include <vector>

#include "base/number_util.h"
#include "base/scoped_ptr.h"
#include "base/system_util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "transliteration/transliteration.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {

void AddCandidate(Segment *segment, const string &value) {
  Segment::Candidate *c = segment->add_candidate();
  c->Init();
  c->value = value;
  c->content_value = value;
}

void AddCandidateWithContentValue(Segment *segment,
                                  const string &value,
                                  const string &content_value) {
  Segment::Candidate *c = segment->add_candidate();
  c->Init();
  c->value = value;
  c->content_value = content_value;
}

}  // namespace

class FocusCandidateRewriterTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::ConfigHandler::GetDefaultConfig(&default_config_);
    config::ConfigHandler::SetConfig(default_config_);

    rewriter_.reset(new FocusCandidateRewriter(&mock_data_manager_));
  }

  const RewriterInterface *GetRewriter() {
    return rewriter_.get();
  }

 private:
  scoped_ptr<FocusCandidateRewriter> rewriter_;
  config::Config default_config_;
  testing::MockDataManager mock_data_manager_;
};

TEST_F(FocusCandidateRewriterTest, FocusCandidateRewriterInvalidQuery) {
  Segments segments;

  Segment *seg1 = segments.add_segment();
  Segment *seg2 = segments.add_segment();
  Segment *seg3 = segments.add_segment();
  Segment *seg4 = segments.add_segment();

  // "｢"
  AddCandidate(seg1, "\xef\xbd\xa2");
  AddCandidate(seg1, "(");
  AddCandidate(seg1, "[");
  AddCandidate(seg1, "{");
  // "テスト"
  AddCandidate(seg2, "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88");
  // "てすと"
  AddCandidate(seg2, "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8");
  // "です"
  AddCandidate(seg3, "\xe3\x81\xa7\xe3\x81\x99");
  // "デス"
  AddCandidate(seg3, "\xe3\x83\x87\xe3\x82\xb9");
  // "｣"
  AddCandidate(seg4, "\xef\xbd\xa3");
  AddCandidate(seg4, ")");
  AddCandidate(seg4, "]");
  AddCandidate(seg4, "}");

  // invalid queries
  EXPECT_FALSE(GetRewriter()->Focus(&segments, 5, 0));
  EXPECT_FALSE(GetRewriter()->Focus(&segments, 0, 10));
  EXPECT_FALSE(GetRewriter()->Focus(&segments, 1, 0));
  EXPECT_FALSE(GetRewriter()->Focus(&segments, 2, 0));
}

TEST_F(FocusCandidateRewriterTest, FocusCandidateRewriterLeftToRight) {
  Segments segments;

  Segment *seg1 = segments.add_segment();
  Segment *seg2 = segments.add_segment();
  Segment *seg3 = segments.add_segment();
  Segment *seg4 = segments.add_segment();

  // "｢"
  AddCandidate(seg1, "\xef\xbd\xa2");
  AddCandidate(seg1, "(");
  AddCandidate(seg1, "[");
  AddCandidate(seg1, "{");
  // "テスト"
  AddCandidate(seg2, "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88");
  // "てすと"
  AddCandidate(seg2, "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8");
  // "です"
  AddCandidate(seg3, "\xe3\x81\xa7\xe3\x81\x99");
  // "デス"
  AddCandidate(seg3, "\xe3\x83\x87\xe3\x82\xb9");
  // "｣"
  AddCandidate(seg4, "\xef\xbd\xa3");
  AddCandidate(seg4, ")");
  AddCandidate(seg4, "]");
  AddCandidate(seg4, "}");

  EXPECT_TRUE(GetRewriter()->Focus(&segments, 0, 0));
  // "｣"
  EXPECT_EQ("\xef\xbd\xa3", seg4->candidate(0).content_value);

  EXPECT_TRUE(GetRewriter()->Focus(&segments, 0, 1));
  EXPECT_EQ(")", seg4->candidate(0).content_value);

  EXPECT_TRUE(GetRewriter()->Focus(&segments, 0, 2));
  EXPECT_EQ("]", seg4->candidate(0).content_value);

  EXPECT_TRUE(GetRewriter()->Focus(&segments, 0, 3));
  EXPECT_EQ("}", seg4->candidate(0).content_value);

  EXPECT_TRUE(GetRewriter()->Focus(&segments, 0, 0));
  // "｣"
  EXPECT_EQ("\xef\xbd\xa3", seg4->candidate(0).content_value);
}

TEST_F(FocusCandidateRewriterTest, FocusCandidateRewriterRightToLeft) {
  Segments segments;

  Segment *seg1 = segments.add_segment();
  Segment *seg2 = segments.add_segment();
  Segment *seg3 = segments.add_segment();
  Segment *seg4 = segments.add_segment();

  // "｢"
  AddCandidate(seg1, "\xef\xbd\xa2");
  AddCandidate(seg1, "(");
  AddCandidate(seg1, "[");
  AddCandidate(seg1, "{");
  // "テスト"
  AddCandidate(seg2, "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88");
  // "てすと"
  AddCandidate(seg2, "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8");
  // "です"
  AddCandidate(seg3, "\xe3\x81\xa7\xe3\x81\x99");
  // "デス"
  AddCandidate(seg3, "\xe3\x83\x87\xe3\x82\xb9");
  // "｣"
  AddCandidate(seg4, "\xef\xbd\xa3");
  AddCandidate(seg4, ")");
  AddCandidate(seg4, "]");
  AddCandidate(seg4, "}");

  // right to left
  EXPECT_TRUE(GetRewriter()->Focus(&segments, 3, 0));
  // "｢"
  EXPECT_EQ("\xef\xbd\xa2", seg1->candidate(0).content_value);

  EXPECT_TRUE(GetRewriter()->Focus(&segments, 3, 1));
  EXPECT_EQ("(", seg1->candidate(0).content_value);

  EXPECT_TRUE(GetRewriter()->Focus(&segments, 3, 2));
  EXPECT_EQ("[", seg1->candidate(0).content_value);

  EXPECT_TRUE(GetRewriter()->Focus(&segments, 3, 3));
  EXPECT_EQ("{", seg1->candidate(0).content_value);

  EXPECT_TRUE(GetRewriter()->Focus(&segments, 3, 0));
  // "｢"
  EXPECT_EQ("\xef\xbd\xa2", seg1->candidate(0).content_value);
}

TEST_F(FocusCandidateRewriterTest, FocusCandidateRewriterLeftToRightNest) {
  Segments segments;
  Segment *seg[7];
  for (int i = 0; i < arraysize(seg); ++i) {
    seg[i] = segments.add_segment();
  }

  // "｢"
  AddCandidate(seg[0], "\xef\xbd\xa2");
  AddCandidate(seg[0], "(");
  AddCandidate(seg[0], "[");
  AddCandidate(seg[0], "{");
  // "テスト1"
  AddCandidate(seg[1], "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88\x31");
  // "｢"
  AddCandidate(seg[2], "\xef\xbd\xa2");
  AddCandidate(seg[2], "(");
  AddCandidate(seg[2], "[");
  AddCandidate(seg[2], "{");
  // "テスト2"
  AddCandidate(seg[3], "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88\x32");
  // "｣"
  AddCandidate(seg[4], "\xef\xbd\xa3");
  AddCandidate(seg[4], ")");
  AddCandidate(seg[4], "]");
  AddCandidate(seg[4], "}");
  // "テスト3"
  AddCandidate(seg[5], "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88\x33");
  // "｣"
  AddCandidate(seg[6], "\xef\xbd\xa3");
  AddCandidate(seg[6], ")");
  AddCandidate(seg[6], "]");
  AddCandidate(seg[6], "}");


  EXPECT_TRUE(GetRewriter()->Focus(&segments, 0, 0));
  // "｣"
  EXPECT_EQ("\xef\xbd\xa3", seg[6]->candidate(0).content_value);
  // "｣"
  EXPECT_EQ("\xef\xbd\xa3", seg[4]->candidate(0).content_value);

  EXPECT_TRUE(GetRewriter()->Focus(&segments, 0, 1));
  EXPECT_EQ(")", seg[6]->candidate(0).content_value);
  // "｣"
  EXPECT_EQ("\xef\xbd\xa3", seg[4]->candidate(0).content_value);

  EXPECT_TRUE(GetRewriter()->Focus(&segments, 2, 0));
  EXPECT_EQ(")", seg[6]->candidate(0).content_value);
  // "｣"
  EXPECT_EQ("\xef\xbd\xa3", seg[4]->candidate(0).content_value);

  EXPECT_TRUE(GetRewriter()->Focus(&segments, 2, 1));
  EXPECT_EQ(")", seg[6]->candidate(0).content_value);
  EXPECT_EQ(")", seg[4]->candidate(0).content_value);
}

TEST_F(FocusCandidateRewriterTest, FocusCandidateRewriterRightToLeftNest) {
  Segments segments;
  Segment *seg[7];
  for (int i = 0; i < arraysize(seg); ++i) {
    seg[i] = segments.add_segment();
  }

  // "｢"
  AddCandidate(seg[0], "\xef\xbd\xa2");
  AddCandidate(seg[0], "(");
  AddCandidate(seg[0], "[");
  AddCandidate(seg[0], "{");
  // "テスト1"
  AddCandidate(seg[1], "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88\x31");
  // "｢"
  AddCandidate(seg[2], "\xef\xbd\xa2");
  AddCandidate(seg[2], "(");
  AddCandidate(seg[2], "[");
  AddCandidate(seg[2], "{");
  // "テスト2"
  AddCandidate(seg[3], "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88\x32");
  // "｣"
  AddCandidate(seg[4], "\xef\xbd\xa3");
  AddCandidate(seg[4], ")");
  AddCandidate(seg[4], "]");
  AddCandidate(seg[4], "}");
  // "テスト3"
  AddCandidate(seg[5], "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88\x33");
  // "｣"
  AddCandidate(seg[6], "\xef\xbd\xa3");
  AddCandidate(seg[6], ")");
  AddCandidate(seg[6], "]");
  AddCandidate(seg[6], "}");

  EXPECT_TRUE(GetRewriter()->Focus(&segments, 6, 0));
  // "｢"
  EXPECT_EQ("\xef\xbd\xa2", seg[0]->candidate(0).content_value);
  // "｢"
  EXPECT_EQ("\xef\xbd\xa2", seg[2]->candidate(0).content_value);

  EXPECT_TRUE(GetRewriter()->Focus(&segments, 6, 1));
  EXPECT_EQ("(", seg[0]->candidate(0).content_value);
  // "｢"
  EXPECT_EQ("\xef\xbd\xa2", seg[2]->candidate(0).content_value);

  EXPECT_TRUE(GetRewriter()->Focus(&segments, 4, 0));
  EXPECT_EQ("(", seg[0]->candidate(0).content_value);
  // "｢"
  EXPECT_EQ("\xef\xbd\xa2", seg[2]->candidate(0).content_value);

  EXPECT_TRUE(GetRewriter()->Focus(&segments, 4, 1));
  EXPECT_EQ("(", seg[0]->candidate(0).content_value);
  EXPECT_EQ("(", seg[2]->candidate(0).content_value);
}

TEST_F(FocusCandidateRewriterTest, FocusCandidateRewriterMetaCandidate) {
  Segments segments;
  Segment *seg[3];
  for (int i = 0; i < arraysize(seg); ++i) {
    seg[i] = segments.add_segment();
  }

  // set key
  // "「"
  seg[0]->set_key("\xe3\x80\x8c");
  // set meta candidates
  {
    EXPECT_EQ(0, seg[0]->meta_candidates_size());
    vector<Segment::Candidate> *meta_cands = seg[0]->mutable_meta_candidates();
    meta_cands->resize(transliteration::NUM_T13N_TYPES);
    for (size_t i = 0; i < transliteration::NUM_T13N_TYPES; ++i) {
      meta_cands->at(i).Init();
      // "「"
      meta_cands->at(i).value = "\xe3\x80\x8c";
      meta_cands->at(i).content_value = "\xe3\x80\x8c";
    }
    meta_cands->at(transliteration::HALF_KATAKANA).value = "\xef\xbd\xa2";
    meta_cands->at(transliteration::HALF_KATAKANA).content_value =
        "\xef\xbd\xa2";
    EXPECT_EQ(transliteration::NUM_T13N_TYPES, seg[0]->meta_candidates_size());
  }
  // "｢"
  EXPECT_EQ(
      "\xef\xbd\xa2",
      seg[0]->meta_candidate(transliteration::HALF_KATAKANA).content_value);
  // "｢"
  AddCandidate(seg[0], "\xef\xbd\xa2");
  AddCandidate(seg[0], "(");
  AddCandidate(seg[0], "[");
  AddCandidate(seg[0], "{");

  // "テスト1"
  AddCandidate(seg[1], "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88\x31");

  // set key
  // "」"
  seg[2]->set_key("\xe3\x80\x8d");
  // set meta candidates
  {
    EXPECT_EQ(0, seg[2]->meta_candidates_size());
    vector<Segment::Candidate> *meta_cands = seg[2]->mutable_meta_candidates();
    meta_cands->resize(transliteration::NUM_T13N_TYPES);
    for (size_t i = 0; i < transliteration::NUM_T13N_TYPES; ++i) {
      meta_cands->at(i).Init();
      // "」"
      meta_cands->at(i).value = "\xe3\x80\x8d";
      meta_cands->at(i).content_value = "\xe3\x80\x8d";
    }
    // "｣"
    meta_cands->at(transliteration::HALF_KATAKANA).value = "\xef\xbd\xa3";
    meta_cands->at(transliteration::HALF_KATAKANA).content_value =
        "\xef\xbd\xa3";
    EXPECT_EQ(transliteration::NUM_T13N_TYPES, seg[2]->meta_candidates_size());
  }
  // "｣"
  EXPECT_EQ(
      "\xef\xbd\xa3",
      seg[2]->meta_candidate(transliteration::HALF_KATAKANA).content_value);
  // "｣"
  AddCandidate(seg[2], "\xef\xbd\xa3");
  AddCandidate(seg[2], ")");
  AddCandidate(seg[2], "]");
  AddCandidate(seg[2], "}");

  const int half_index = -transliteration::HALF_KATAKANA - 1;
  EXPECT_TRUE(GetRewriter()->Focus(&segments, 0,
                                   half_index));
  // "｢"
  EXPECT_EQ("\xef\xbd\xa2", seg[0]->candidate(0).content_value);
  // "｣"
  EXPECT_EQ("\xef\xbd\xa3", seg[2]->candidate(0).content_value);

  const int valid_index = -(transliteration::NUM_T13N_TYPES - 1) - 1;
  EXPECT_TRUE(GetRewriter()->Focus(&segments, 0,
                                   valid_index));
  const int invalid_index = -transliteration::NUM_T13N_TYPES - 1;
  EXPECT_FALSE(GetRewriter()->Focus(&segments, 0,
                                    invalid_index));
}

TEST_F(FocusCandidateRewriterTest, FocusCandidateRewriterNumber) {
  Segments segments;
  Segment *seg[7];
  for (int i = 0; i < arraysize(seg); ++i) {
    seg[i] = segments.add_segment();
  }

  // set key
  seg[0]->set_key("2");
  AddCandidate(seg[0], "2");
  //  AddCandidate(seg[0], "２");
  //  AddCandidate(seg[0], "ニ");
  //  AddCandidate(seg[0], "弐");
  AddCandidate(seg[0], "\xEF\xBC\x92");
  AddCandidate(seg[0], "\xE3\x83\x8B");
  AddCandidate(seg[0], "\xE5\xBC\x90");


  seg[0]->mutable_candidate(2)->style = NumberUtil::NumberString::NUMBER_KANJI;
  seg[0]->mutable_candidate(3)->style =
      NumberUtil::NumberString::NUMBER_OLD_KANJI;

  // "テスト1"
  AddCandidate(seg[1], "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88\x31");

  // set key
  seg[2]->set_key("3");
  AddCandidate(seg[2], "3");
  //  AddCandidate(seg[2], "３");
  //  AddCandidate(seg[2], "三");
  //  AddCandidate(seg[2], "参");
  AddCandidate(seg[2], "\xEF\xBC\x93");
  AddCandidate(seg[2], "\xE4\xB8\x89");
  AddCandidate(seg[2], "\xE5\x8F\x82");

  seg[2]->mutable_candidate(2)->style = NumberUtil::NumberString::NUMBER_KANJI;
  seg[2]->mutable_candidate(3)->style =
      NumberUtil::NumberString::NUMBER_OLD_KANJI;

  seg[3]->set_key("4");
  AddCandidate(seg[3], "4");
  //  AddCandidate(seg[3], "４");
  //  AddCandidate(seg[3], "四");
  AddCandidate(seg[3], "\xEF\xBC\x94");
  AddCandidate(seg[3], "\xE5\x9B\x9B");
  seg[3]->mutable_candidate(2)->style = NumberUtil::NumberString::NUMBER_KANJI;

  // "テスト1"
  AddCandidate(seg[4], "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88\x31");
  AddCandidate(seg[5], "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88\x31");

  seg[6]->set_key("4");
  AddCandidate(seg[6], "4");
  //  AddCandidate(seg[6], "４");
  //  AddCandidate(seg[6], "四");
  AddCandidate(seg[6], "\xEF\xBC\x94");
  AddCandidate(seg[6], "\xE5\x9B\x9B");

  seg[6]->mutable_candidate(2)->style = NumberUtil::NumberString::NUMBER_KANJI;

  EXPECT_TRUE(GetRewriter()->Focus(&segments, 0, 0));
  EXPECT_EQ("2", seg[0]->candidate(0).content_value);
  EXPECT_EQ("3", seg[2]->candidate(0).content_value);
  EXPECT_EQ("4", seg[3]->candidate(0).content_value);

  EXPECT_TRUE(GetRewriter()->Focus(&segments, 0, 1));
  //  EXPECT_EQ("３", seg[2]->candidate(0).content_value);
  //  EXPECT_EQ("４", seg[3]->candidate(0).content_value);
  EXPECT_EQ("\xEF\xBC\x93", seg[2]->candidate(0).content_value);
  EXPECT_EQ("\xEF\xBC\x94", seg[3]->candidate(0).content_value);

  EXPECT_EQ("4",  seg[6]->candidate(0).content_value);  // far from


  EXPECT_TRUE(GetRewriter()->Focus(&segments, 0, 2));
  //  EXPECT_EQ("三", seg[2]->candidate(0).content_value);
  //  EXPECT_EQ("四", seg[3]->candidate(0).content_value);
  EXPECT_EQ("\xE4\xB8\x89", seg[2]->candidate(0).content_value);
  EXPECT_EQ("\xE5\x9B\x9B", seg[3]->candidate(0).content_value);

  EXPECT_EQ("4",  seg[6]->candidate(0).content_value);  // far from

  EXPECT_TRUE(GetRewriter()->Focus(&segments, 0, 3));
  //  EXPECT_EQ("参", seg[2]->candidate(0).content_value);
  EXPECT_EQ("\xE5\x8F\x82", seg[2]->candidate(0).content_value);
  EXPECT_EQ("4",  seg[6]->candidate(0).content_value);  // far from
}

// Bug #4596846: Non-number characters are changed to numbers
TEST_F(FocusCandidateRewriterTest, DontChangeNonNumberSegment) {
  Segments segments;
  Segment *seg[2];
  for (int i = 0; i < arraysize(seg); ++i) {
    seg[i] = segments.add_segment();
  }

  // set key
  seg[0]->set_key("1");
  AddCandidate(seg[0], "1");
  AddCandidate(seg[0], "\xEF\xBC\x91");  // "１"
  AddCandidate(seg[1], "\xE8\xAA\x9E");  // "語"
  AddCandidate(seg[1], "\xEF\xBC\x95");  // "５"

  // Should not change a segment that doesn't have a number as its first
  // candidate.
  EXPECT_FALSE(GetRewriter()->Focus(&segments, 0, 1));
  EXPECT_NE("\xEF\xBC\x95", seg[1]->candidate(0).content_value);
}

TEST_F(FocusCandidateRewriterTest, FocusCandidateRewriterSuffix) {
  {
    Segments segments;
    Segment *seg[6];
    for (int i = 0; i < arraysize(seg); ++i) {
      seg[i] = segments.add_segment();
    }

    seg[0]->set_key("2");
    AddCandidate(seg[0], "2");

    // "かい"
    seg[1]->set_key("\xE3\x81\x8B\xE3\x81\x84");
    AddCandidate(seg[1], "\xE5\x9B\x9E");  // "回"
    AddCandidate(seg[1], "\xE9\x9A\x8E");  // "解"

    seg[2]->set_key("3");
    AddCandidate(seg[2], "3");

    // "かい"
    seg[3]->set_key("\xE3\x81\x8B\xE3\x81\x84");
    AddCandidate(seg[3], "\xE5\x9B\x9E");  // "回"
    AddCandidate(seg[3], "\xE9\x9A\x8E");  // "解"

    seg[4]->set_key("4");
    AddCandidate(seg[4], "4");

    // "かい"
    seg[5]->set_key("\xE3\x81\x8B\xE3\x81\x84");
    AddCandidate(seg[5], "\xE5\x9B\x9E");  // "回"
    AddCandidate(seg[5], "\xE9\x9A\x8E");  // "解"


    EXPECT_TRUE(GetRewriter()->Focus(&segments, 1, 1));
    // "階"
    EXPECT_EQ("\xE9\x9A\x8E", seg[3]->candidate(0).content_value);
    EXPECT_EQ("\xE9\x9A\x8E", seg[5]->candidate(0).content_value);
  }

  // No Number
  {
    Segments segments;
    Segment *seg[3];
    for (int i = 0; i < arraysize(seg); ++i) {
      seg[i] = segments.add_segment();
    }

    // "かい"
    seg[0]->set_key("\xE3\x81\x8B\xE3\x81\x84");
    AddCandidate(seg[0], "\xE5\x9B\x9E");  // "回"
    AddCandidate(seg[0], "\xE9\x9A\x8E");  // "階"

    seg[1]->set_key("3");
    AddCandidate(seg[1], "3");

    // "かい"
    seg[2]->set_key("\xE3\x81\x8B\xE3\x81\x84");
    AddCandidate(seg[2], "\xE5\x9B\x9E");  // "回"
    AddCandidate(seg[2], "\xE9\x9A\x8E");  // "階"
    EXPECT_FALSE(GetRewriter()->Focus(&segments, 0, 1));
  }

  // No Number
  {
    Segments segments;
    Segment *seg[3];
    for (int i = 0; i < arraysize(seg); ++i) {
      seg[i] = segments.add_segment();
    }

    // "かい"
    seg[0]->set_key("\xE3\x81\x8B\xE3\x81\x84");
    AddCandidate(seg[0], "\xE5\x9B\x9E");  // "回"
    AddCandidate(seg[0], "\xE9\x9A\x8E");  // "階"

    seg[1]->set_key("3");
    AddCandidate(seg[1], "3");

    // "かい"
    seg[2]->set_key("\xE3\x81\x8B\xE3\x81\x84");
    AddCandidate(seg[2], "\xE5\x9B\x9E");  // "回"
    AddCandidate(seg[2], "\xE9\x9A\x8E");  // "階"
    EXPECT_FALSE(GetRewriter()->Focus(&segments, 0, 1));
  }

  // No number
  {
    Segments segments;
    Segment *seg[3];
    for (int i = 0; i < arraysize(seg); ++i) {
      seg[i] = segments.add_segment();
    }

    seg[0]->set_key("2");
    AddCandidate(seg[0], "2");

    // "かい"
    seg[1]->set_key("\xE3\x81\x8B\xE3\x81\x84");
    AddCandidate(seg[1], "\xE5\x9B\x9E");  // "回"
    AddCandidate(seg[1], "\xE9\x9A\x8E");  // "階"
    // "かい"
    seg[2]->set_key("\xE3\x81\x8B\xE3\x81\x84");
    AddCandidate(seg[2], "\xE5\x9B\x9E");  // "回"
    AddCandidate(seg[2], "\xE9\x9A\x8E");  // "解"

    EXPECT_FALSE(GetRewriter()->Focus(&segments, 1, 1));
  }
}

TEST_F(FocusCandidateRewriterTest, NumberAndSuffixCompound) {
  // Test for reranking of number compound pattern:
  {
    // Test scenario: Construct the following segments:
    //           Seg 0       Seg 1
    //        | "いっかい" | "にかい" |
    // cand 0 | "一階"    | "2階"   |
    // cand 1 | "一回"    | "二階"   |
    // cand 2 |          | "二回"   |
    //
    // Then, focusing on (Seg 0, cand 1) should move "二回" in Seg 1 to the top.
    Segments segments;
    Segment *seg[2];
    for (int i = 0; i < arraysize(seg); ++i) {
      seg[i] = segments.add_segment();
    }
    // "いっかい"
    seg[0]->set_key("\xE3\x81\x84\xE3\x81\xA3\xE3\x81\x8B\xE3\x81\x84");
    // value = "一階", content_value = "一階"
    AddCandidate(seg[0], "\xE4\xB8\x80\xE9\x9A\x8E");
    // value = "一回", content_value = "一回"
    AddCandidate(seg[0], "\xE4\xB8\x80\xE5\x9B\x9E");

    // "にかい"
    seg[1]->set_key("\xE3\x81\xAB\xE3\x81\x8B\xE3\x81\x84");
    AddCandidate(seg[1], "\x32\xE9\x9A\x8E");  // "2階"
    AddCandidate(seg[1], "\xE4\xBA\x8C\xE9\x9A\x8E");  // "二階"
    AddCandidate(seg[1], "\xE4\xBA\x8C\xE5\x9B\x9E");  // "二回"

    EXPECT_TRUE(GetRewriter()->Focus(&segments, 0, 1));
    EXPECT_EQ("\xE4\xBA\x8C\xE5\x9B\x9E", seg[1]->candidate(0).value);
  }
  // Test for reranking of number compound + parallel marker pattern:
  // http://mozcsuorg.appspot.com/#issue/49
  {
    // Test scenario: Similar to the above case, but construct the following
    // segments with parallel marker:
    //           Seg 0       Seg 1
    //        | "いっかいや" | "にかい" |
    // cand 0 | "一階や"    | "2階"    |
    // cand 1 | "一回や"    | "二階"   |
    // cand 2 |            | "二回"   |
    //
    // Then, focusing on (Seg 0, cand 1) should move "二回" in Seg 1 to the top.
    Segments segments;
    Segment *seg[2];
    for (int i = 0; i < arraysize(seg); ++i) {
      seg[i] = segments.add_segment();
    }

    const int kNounId = 1939;
    const int kParallelMarkerYa = 290;

    seg[0]->set_key(
        // "いっかいや"
        "\xE3\x81\x84\xE3\x81\xA3\xE3\x81\x8B\xE3\x81\x84\xE3\x82\x84");

    // value = "一階や", content_value = "一階"
    AddCandidateWithContentValue(seg[0],
                                 "\xE4\xB8\x80\xE9\x9A\x8E\xE3\x82\x84",
                                 "\xE4\xB8\x80\xE9\x9A\x8E");
    seg[0]->mutable_candidate(0)->lid = kNounId;
    seg[0]->mutable_candidate(0)->rid = kParallelMarkerYa;

    // value = "一回や", content_value = "一回"
    AddCandidateWithContentValue(seg[0],
                                 "\xE4\xB8\x80\xE5\x9B\x9E\xE3\x82\x84",
                                 "\xE4\xB8\x80\xE5\x9B\x9E");
    seg[0]->mutable_candidate(1)->lid = kNounId;
    seg[0]->mutable_candidate(1)->rid = kParallelMarkerYa;

    seg[1]->set_key(
        // "にかい"
        "\xE3\x81\xAB\xE3\x81\x8B\xE3\x81\x84");
    AddCandidate(seg[1], "\x32\xE9\x9A\x8E");  // "2階"
    seg[1]->mutable_candidate(0)->lid = kNounId;
    seg[1]->mutable_candidate(0)->rid = kNounId;
    AddCandidate(seg[1], "\xE4\xBA\x8C\xE9\x9A\x8E");  // "二階"
    seg[1]->mutable_candidate(1)->lid = kNounId;
    seg[1]->mutable_candidate(1)->rid = kNounId;
    AddCandidate(seg[1], "\xE4\xBA\x8C\xE5\x9B\x9E");  // "二回"
    seg[1]->mutable_candidate(2)->lid = kNounId;
    seg[1]->mutable_candidate(2)->rid = kNounId;

    EXPECT_TRUE(GetRewriter()->Focus(&segments, 0, 1));
    EXPECT_EQ("\xE4\xBA\x8C\xE5\x9B\x9E", seg[1]->candidate(0).value);
  }
  // Test for reranking of number compound + parallel marker pattern for 3
  // segments.
  {
    // Test scenario: Similar to the above case, but construct the following
    // segments with parallel marker:
    //           Seg 0       Seg 1      Seg 2
    //        | "いっかいや" | "にかいや" | "さんかい"
    // cand 0 | "一階や"    | "2階や"    | "参回"
    // cand 1 | "一回や"    | "二階や"   | "3階"
    // cand 2 |            | "二回や"   | "三回"
    //
    // Then, focusing on (Seg 0, cand 1) should move "二回や" in Seg 1 and "三回
    // " in Seg 2 to the top of each segment.
    Segments segments;
    Segment *seg[3];
    for (int i = 0; i < arraysize(seg); ++i) {
      seg[i] = segments.add_segment();
    }

    const int kNounId = 1939;
    const int kParallelMarkerYa = 290;

    seg[0]->set_key(
        // "いっかいや"
        "\xE3\x81\x84\xE3\x81\xA3\xE3\x81\x8B\xE3\x81\x84\xE3\x82\x84");

    // value = "一階や", content_value = "一階"
    AddCandidateWithContentValue(seg[0],
                                 "\xE4\xB8\x80\xE9\x9A\x8E\xE3\x82\x84",
                                 "\xE4\xB8\x80\xE9\x9A\x8E");
    seg[0]->mutable_candidate(0)->lid = kNounId;
    seg[0]->mutable_candidate(0)->rid = kParallelMarkerYa;

    // value = "一回や", content_value = "一回"
    AddCandidateWithContentValue(seg[0],
                                 "\xE4\xB8\x80\xE5\x9B\x9E\xE3\x82\x84",
                                 "\xE4\xB8\x80\xE5\x9B\x9E");
    seg[0]->mutable_candidate(1)->lid = kNounId;
    seg[0]->mutable_candidate(1)->rid = kParallelMarkerYa;

    seg[1]->set_key(
        // "にかいや"
        "\xE3\x81\xAB\xE3\x81\x8B\xE3\x81\x84\xE3\x82\x84");
    //value = "2階や", content_value = "2階"
    AddCandidateWithContentValue(seg[1],
                                 "\x32\xE9\x9A\x8E\xE3\x82\x84",
                                 "\x32\xE9\x9A\x8E");
    seg[1]->mutable_candidate(0)->lid = kNounId;
    seg[1]->mutable_candidate(0)->rid = kParallelMarkerYa;
    // value = "二階や", content_value = "二階"
    AddCandidateWithContentValue(seg[1],
                                 "\xE4\xBA\x8C\xE9\x9A\x8E\xE3\x82\x84",
                                 "\xE4\xBA\x8C\xE9\x9A\x8E");
    seg[1]->mutable_candidate(1)->lid = kNounId;
    seg[1]->mutable_candidate(1)->rid = kParallelMarkerYa;
    // value = "二回や", content_value = "二回"
    AddCandidateWithContentValue(seg[1],
                                 "\xE4\xBA\x8C\xE5\x9B\x9E\xE3\x82\x84",
                                 "\xE4\xBA\x8C\xE5\x9B\x9E");
    seg[1]->mutable_candidate(2)->lid = kNounId;
    seg[1]->mutable_candidate(2)->rid = kParallelMarkerYa;

    seg[2]->set_key(
        // "さんかいや"
        "\xE3\x81\x95\xE3\x82\x93\xE3\x81\x8B\xE3\x81\x84\xE3\x82\x84");
    AddCandidate(seg[2], "\xE5\x8F\x82\xE5\x9B\x9E");  // "参回"
    seg[2]->mutable_candidate(0)->lid = kNounId;
    seg[2]->mutable_candidate(0)->rid = kNounId;
    AddCandidate(seg[2], "\x33\xE9\x9A\x8E");  // "3階"
    seg[2]->mutable_candidate(1)->lid = kNounId;
    seg[2]->mutable_candidate(1)->rid = kNounId;
    AddCandidate(seg[2], "\xE4\xB8\x89\xE5\x9B\x9E");  // "三回"
    seg[2]->mutable_candidate(2)->lid = kNounId;
    seg[2]->mutable_candidate(2)->rid = kNounId;

    EXPECT_TRUE(GetRewriter()->Focus(&segments, 0, 1));
    // "二回や"
    EXPECT_EQ("\xE4\xBA\x8C\xE5\x9B\x9E\xE3\x82\x84",
              seg[1]->candidate(0).value);
    // "三回"
    EXPECT_EQ("\xE4\xB8\x89\xE5\x9B\x9E", seg[2]->candidate(0).value);
  }
  // Test case where two number segments are too far to be rewritten.
  {
    // Test scenario: Similar to the above case, but construct the following
    // segments with parallel marker:
    //           Seg 0       Seg 1 Seg 2   Seg 3 Seg 4
    //        | "いっかいや" | "あ" | "あ" | "あ" | "にかい"
    // cand 0 | "一階や"    | "あ" | "あ" | "あ" | "2階"
    // cand 1 | "一回や"    |      |     |      | "二回"
    //
    // Then, focusing on (Seg 0, cand 1) cannot move "二回" in Seg 3 to the top.
    Segments segments;
    Segment *seg[5];
    for (int i = 0; i < arraysize(seg); ++i) {
      seg[i] = segments.add_segment();
    }

    const int kNounId = 1939;
    const int kParallelMarkerYa = 290;

    seg[0]->set_key(
        // "いっかいや"
        "\xE3\x81\x84\xE3\x81\xA3\xE3\x81\x8B\xE3\x81\x84\xE3\x82\x84");

    // value = "一階や", content_value = "一階"
    AddCandidateWithContentValue(seg[0],
                                 "\xE4\xB8\x80\xE9\x9A\x8E\xE3\x82\x84",
                                 "\xE4\xB8\x80\xE9\x9A\x8E");
    seg[0]->mutable_candidate(0)->lid = kNounId;
    seg[0]->mutable_candidate(0)->rid = kParallelMarkerYa;

    // value = "一回や", content_value = "一回"
    AddCandidateWithContentValue(seg[0],
                                 "\xE4\xB8\x80\xE5\x9B\x9E\xE3\x82\x84",
                                 "\xE4\xB8\x80\xE5\x9B\x9E");
    seg[0]->mutable_candidate(1)->lid = kNounId;
    seg[0]->mutable_candidate(1)->rid = kParallelMarkerYa;

    for (size_t i = 1; i <= 3; ++i) {
      seg[i]->set_key("\xE3\x81\x82");  // "あ"
      AddCandidate(seg[i], "\xE3\x81\x82");  // "あ"
      seg[i]->mutable_candidate(0)->lid = kNounId;
      seg[i]->mutable_candidate(0)->rid = kNounId;
    }

    seg[4]->set_key(
        // "にかい"
        "\xE3\x81\xAB\xE3\x81\x8B\xE3\x81\x84");
    AddCandidate(seg[4], "\x32\xE9\x9A\x8E");  // "2階"
    seg[4]->mutable_candidate(0)->lid = kNounId;
    seg[4]->mutable_candidate(0)->rid = kNounId;
    AddCandidate(seg[4], "\xE4\xBA\x8C\xE5\x9B\x9E");  // "ニ回"
    seg[4]->mutable_candidate(0)->lid = kNounId;
    seg[4]->mutable_candidate(0)->rid = kNounId;

    EXPECT_FALSE(GetRewriter()->Focus(&segments, 0, 1));
  }
  // Test for the case where we shouldn't rewrite.
  {
    // Test scenario: Similar to the above cases, but construct the following
    // segments with particles へ and は:
    //           Seg 0       Seg 1        Seg 2
    //        | "いっかいへは" | "にかい" | "いった"
    // cand 0 | "一階へは"    | "二回"   | "行った"
    // cand 1 | "一回へは"    | "ニ階"   |
    // cand 2 |              | "2回"   |
    //
    // Here, "一階へは" has the following structure:
    // "一階(noun)" + "へ(格助詞)" + "は(係助詞)"
    // For this case, focusing on (Seg 0, cand 1) should not move "二階" in Seg
    // 1 to the top.
    Segments segments;
    Segment *seg[3];
    for (int i = 0; i < arraysize(seg); ++i) {
      seg[i] = segments.add_segment();
    }

    // "名詞,一般,*,*,*,*,*"
    const int kNounId = 1939;
    // "助詞,係助詞,*,*,*,*,は"
    const int kKakariJoshiHa = 299;
    // "動詞,非自立,*,*,五段・カ行促音便,連用タ接続,行く"
    const int kIkuTaSetsuzoku = 1501;
    // "助動詞,*,*,*,特殊・タ,基本形,た"
    const int kJodoushiTa = 161;

    seg[0]->set_key(
        // "いっかいへは"
        "\xE3\x81\x84\xE3\x81\xA3\xE3\x81\x8B\xE3\x81\x84"
        "\xE3\x81\xB8\xE3\x81\xAF");

    // value = "一階へは", content_value = "一階"
    AddCandidateWithContentValue(
        seg[0],
        "\xE4\xB8\x80\xE9\x9A\x8E\xE3\x81\xB8\xE3\x81\xAF",
        "\xE4\xB8\x80\xE9\x9A\x8E");
    seg[0]->mutable_candidate(0)->lid = kNounId;
    seg[0]->mutable_candidate(0)->rid = kKakariJoshiHa;

    // value = "一回へは", content_value = "一回"
    AddCandidateWithContentValue(
        seg[0],
        "\xE4\xB8\x80\xE5\x9B\x9E\xE3\x81\xB8\xE3\x81\xAF",
        "\xE4\xB8\x80\xE5\x9B\x9E");
    seg[0]->mutable_candidate(1)->lid = kNounId;
    seg[0]->mutable_candidate(1)->rid = kKakariJoshiHa;

    seg[1]->set_key(
        // "にかい"
        "\xE3\x81\xAB\xE3\x81\x8B\xE3\x81\x84");
    AddCandidate(seg[1], "\xE4\xBA\x8C\xE5\x9B\x9E");  // "二回"
    seg[1]->mutable_candidate(0)->lid = kNounId;
    seg[1]->mutable_candidate(0)->rid = kNounId;
    AddCandidate(seg[1], "\xE4\xBA\x8C\xE9\x9A\x8E");  // "二階"
    seg[1]->mutable_candidate(1)->lid = kNounId;
    seg[1]->mutable_candidate(1)->rid = kNounId;
    AddCandidate(seg[1], "\x32\xE9\x9A\x8E");  // "2階"
    seg[1]->mutable_candidate(2)->lid = kNounId;
    seg[1]->mutable_candidate(2)->rid = kNounId;

    seg[2]->set_key(
        // "いった"
        "\xE3\x81\x84\xE3\x81\xA3\xE3\x81\x9F");
    AddCandidate(seg[2], "\xE8\xA1\x8C\xE3\x81\xA3\xE3\x81\x9F");  // "行った"
    seg[2]->mutable_candidate(0)->lid = kIkuTaSetsuzoku;
    seg[2]->mutable_candidate(0)->rid = kJodoushiTa;

    EXPECT_FALSE(GetRewriter()->Focus(&segments, 0, 1));
  }
}

}  // namespace mozc
