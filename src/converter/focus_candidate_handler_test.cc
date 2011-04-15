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
#include "base/base.h"
#include "base/util.h"
#include "transliteration/transliteration.h"
#include "converter/segments.h"
#include "converter/focus_candidate_handler.h"
#include "session/config.pb.h"
#include "session/config_handler.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/googletest.h"

namespace mozc {
namespace {
void AddCandidate(Segment *segment, const string &value) {
  Segment::Candidate *c = segment->add_candidate();
  c->Init();
  c->value = value;
  c->content_value = value;
}
}  // namespace

class FocusCandidateHandlerTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::ConfigHandler::GetDefaultConfig(&default_config_);
    config::ConfigHandler::SetConfig(default_config_);
  }

 private:
  config::Config default_config_;
};

TEST_F(FocusCandidateHandlerTest, FocusCandidateHandlerInvalidQuery) {
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
  EXPECT_FALSE(FocusCandidateHandler::FocusSegmentValue(&segments, 5, 0));
  EXPECT_FALSE(FocusCandidateHandler::FocusSegmentValue(&segments, 0, 10));
  EXPECT_FALSE(FocusCandidateHandler::FocusSegmentValue(&segments, 1, 0));
  EXPECT_FALSE(FocusCandidateHandler::FocusSegmentValue(&segments, 2, 0));
}

TEST_F(FocusCandidateHandlerTest, FocusCandidateHandlerLeftToRight) {
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

  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 0, 0));
  // "｣"
  EXPECT_EQ("\xef\xbd\xa3", seg4->candidate(0).content_value);

  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 0, 1));
  EXPECT_EQ(")", seg4->candidate(0).content_value);

  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 0, 2));
  EXPECT_EQ("]", seg4->candidate(0).content_value);

  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 0, 3));
  EXPECT_EQ("}", seg4->candidate(0).content_value);

  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 0, 0));
  // "｣"
  EXPECT_EQ("\xef\xbd\xa3", seg4->candidate(0).content_value);
}

TEST_F(FocusCandidateHandlerTest, FocusCandidateHandlerRightToLeft) {
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
  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 3, 0));
  // "｢"
  EXPECT_EQ("\xef\xbd\xa2", seg1->candidate(0).content_value);

  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 3, 1));
  EXPECT_EQ("(", seg1->candidate(0).content_value);

  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 3, 2));
  EXPECT_EQ("[", seg1->candidate(0).content_value);

  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 3, 3));
  EXPECT_EQ("{", seg1->candidate(0).content_value);

  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 3, 0));
  // "｢"
  EXPECT_EQ("\xef\xbd\xa2", seg1->candidate(0).content_value);
}

TEST_F(FocusCandidateHandlerTest, FocusCandidateHandlerLeftToRightNest) {
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


  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 0, 0));
  // "｣"
  EXPECT_EQ("\xef\xbd\xa3", seg[6]->candidate(0).content_value);
  // "｣"
  EXPECT_EQ("\xef\xbd\xa3", seg[4]->candidate(0).content_value);

  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 0, 1));
  EXPECT_EQ(")", seg[6]->candidate(0).content_value);
  // "｣"
  EXPECT_EQ("\xef\xbd\xa3", seg[4]->candidate(0).content_value);

  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 2, 0));
  EXPECT_EQ(")", seg[6]->candidate(0).content_value);
  // "｣"
  EXPECT_EQ("\xef\xbd\xa3", seg[4]->candidate(0).content_value);

  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 2, 1));
  EXPECT_EQ(")", seg[6]->candidate(0).content_value);
  EXPECT_EQ(")", seg[4]->candidate(0).content_value);
}

TEST_F(FocusCandidateHandlerTest, FocusCandidateHandlerRightToLeftNest) {
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

  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 6, 0));
  // "｢"
  EXPECT_EQ("\xef\xbd\xa2", seg[0]->candidate(0).content_value);
  // "｢"
  EXPECT_EQ("\xef\xbd\xa2", seg[2]->candidate(0).content_value);

  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 6, 1));
  EXPECT_EQ("(", seg[0]->candidate(0).content_value);
  // "｢"
  EXPECT_EQ("\xef\xbd\xa2", seg[2]->candidate(0).content_value);

  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 4, 0));
  EXPECT_EQ("(", seg[0]->candidate(0).content_value);
  // "｢"
  EXPECT_EQ("\xef\xbd\xa2", seg[2]->candidate(0).content_value);

  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 4, 1));
  EXPECT_EQ("(", seg[0]->candidate(0).content_value);
  EXPECT_EQ("(", seg[2]->candidate(0).content_value);
}

TEST_F(FocusCandidateHandlerTest, FocusCandidateHandlerMetaCandidate) {
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
  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 0,
                                                       half_index));
  // "｢"
  EXPECT_EQ("\xef\xbd\xa2", seg[0]->candidate(0).content_value);
  // "｣"
  EXPECT_EQ("\xef\xbd\xa3", seg[2]->candidate(0).content_value);

  const int valid_index = -(transliteration::NUM_T13N_TYPES - 1) - 1;
  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 0,
                                                       valid_index));
  const int invalid_index = -transliteration::NUM_T13N_TYPES - 1;
  EXPECT_FALSE(FocusCandidateHandler::FocusSegmentValue(&segments, 0,
                                                        invalid_index));
}

TEST_F(FocusCandidateHandlerTest, FocusCandidateHandlerNumber) {
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


  seg[0]->mutable_candidate(2)->style = Segment::Candidate::NUMBER_KANJI;
  seg[0]->mutable_candidate(3)->style = Segment::Candidate::NUMBER_OLD_KANJI;

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

  seg[2]->mutable_candidate(2)->style = Segment::Candidate::NUMBER_KANJI;
  seg[2]->mutable_candidate(3)->style = Segment::Candidate::NUMBER_OLD_KANJI;

  seg[3]->set_key("4");
  AddCandidate(seg[3], "4");
  //  AddCandidate(seg[3], "４");
  //  AddCandidate(seg[3], "四");
  AddCandidate(seg[3], "\xEF\xBC\x94");
  AddCandidate(seg[3], "\xE5\x9B\x9B");
  seg[3]->mutable_candidate(2)->style = Segment::Candidate::NUMBER_KANJI;

  // "テスト1"
  AddCandidate(seg[4], "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88\x31");
  AddCandidate(seg[5], "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88\x31");

  seg[6]->set_key("4");
  AddCandidate(seg[6], "4");
  //  AddCandidate(seg[6], "４");
  //  AddCandidate(seg[6], "四");
  AddCandidate(seg[6], "\xEF\xBC\x94");
  AddCandidate(seg[6], "\xE5\x9B\x9B");

  seg[6]->mutable_candidate(2)->style = Segment::Candidate::NUMBER_KANJI;

  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 0,
                                                       0));
  EXPECT_EQ("2", seg[0]->candidate(0).content_value);
  EXPECT_EQ("3", seg[2]->candidate(0).content_value);
  EXPECT_EQ("4", seg[3]->candidate(0).content_value);

  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 0,
                                                       1));
  //  EXPECT_EQ("３", seg[2]->candidate(0).content_value);
  //  EXPECT_EQ("４", seg[3]->candidate(0).content_value);
  EXPECT_EQ("\xEF\xBC\x93", seg[2]->candidate(0).content_value);
  EXPECT_EQ("\xEF\xBC\x94", seg[3]->candidate(0).content_value);

  EXPECT_EQ("4",  seg[6]->candidate(0).content_value);  // far from


  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 0,
                                                       2));
  //  EXPECT_EQ("三", seg[2]->candidate(0).content_value);
  //  EXPECT_EQ("四", seg[3]->candidate(0).content_value);
  EXPECT_EQ("\xE4\xB8\x89", seg[2]->candidate(0).content_value);
  EXPECT_EQ("\xE5\x9B\x9B", seg[3]->candidate(0).content_value);

  EXPECT_EQ("4",  seg[6]->candidate(0).content_value);  // far from

  EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 0,
                                                       3));
  //  EXPECT_EQ("参", seg[2]->candidate(0).content_value);
  EXPECT_EQ("\xE5\x8F\x82", seg[2]->candidate(0).content_value);
  EXPECT_EQ("4",  seg[6]->candidate(0).content_value);  // far from
}

TEST_F(FocusCandidateHandlerTest, FocusCandidateHandlerSuffix) {
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


    EXPECT_TRUE(FocusCandidateHandler::FocusSegmentValue(&segments, 1,
                                                         1));
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
    EXPECT_FALSE(FocusCandidateHandler::FocusSegmentValue(&segments, 0,
                                                          1));
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
    EXPECT_FALSE(FocusCandidateHandler::FocusSegmentValue(&segments, 0,
                                                          1));
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

    EXPECT_FALSE(FocusCandidateHandler::FocusSegmentValue(&segments, 1,
                                                          1));
  }
}
}  // namespace mozc
