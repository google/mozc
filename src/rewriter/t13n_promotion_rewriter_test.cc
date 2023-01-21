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

#include "rewriter/t13n_promotion_rewriter.h"

#include <memory>
#include <string>

#include "composer/composer.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "rewriter/transliteration_rewriter.h"
#include "session/request_test_util.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

void AddCandidateWithValue(const std::string &value, Segment *segment) {
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->Init();
  candidate->key = segment->key();
  candidate->content_key = segment->key();
  candidate->value = value;
  candidate->content_value = value;
}

// Returns -1 if not found.
int GetCandidateIndexByValue(const std::string &value, const Segment &segment) {
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (segment.candidate(i).value == value) {
      return i;
    }
  }
  return -1;
}

class T13nPromotionRewriterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    t13n_rewriter_ = std::make_unique<TransliterationRewriter>(
        dictionary::PosMatcher(mock_data_manager_.GetPosMatcherData()));

    mobile_conv_request_ = ConversionRequest();
    composer_ = composer::Composer();
    mobile_request_ = commands::Request();

    commands::RequestForUnitTest::FillMobileRequest(&mobile_request_);
    composer_.SetRequest(&mobile_request_);
    mobile_conv_request_.set_request(&mobile_request_);
    mobile_conv_request_.set_composer(&composer_);
  }

  std::unique_ptr<TransliterationRewriter> t13n_rewriter_;
  ConversionRequest mobile_conv_request_;
  composer::Composer composer_;
  commands::Request mobile_request_;

 private:
  const testing::ScopedTmpUserProfileDirectory scoped_profile_dir_;
  const testing::MockDataManager mock_data_manager_;
};

TEST_F(T13nPromotionRewriterTest, Capability) {
  T13nPromotionRewriter rewriter;

  // Mobile
  EXPECT_EQ(RewriterInterface::ALL, rewriter.capability(mobile_conv_request_));

  // Desktop
  commands::Request default_request;
  ConversionRequest default_conv_request;
  default_conv_request.set_request(&default_request);
  EXPECT_EQ(RewriterInterface::NOT_AVAILABLE,
            rewriter.capability(default_conv_request));
}

TEST_F(T13nPromotionRewriterTest, PromoteKatakanaFromT13N) {
  T13nPromotionRewriter rewriter;

  Segments segments;
  composer_.SetInputMode(transliteration::HIRAGANA);
  composer_.SetPreeditTextForTestOnly("きょう");
  Segment *segment = segments.push_back_segment();
  segment->set_key("きょう");
  AddCandidateWithValue("今日", segment);
  AddCandidateWithValue("きょう", segment);
  AddCandidateWithValue("強", segment);
  AddCandidateWithValue("教", segment);
  AddCandidateWithValue("凶", segment);
  AddCandidateWithValue("卿", segment);

  EXPECT_EQ(-1, GetCandidateIndexByValue("キョウ", *segment));

  EXPECT_TRUE(t13n_rewriter_->Rewrite(mobile_conv_request_, &segments));
  EXPECT_EQ(-1, GetCandidateIndexByValue("キョウ", *segment));

  EXPECT_TRUE(rewriter.Rewrite(mobile_conv_request_, &segments));
  EXPECT_EQ(5, GetCandidateIndexByValue("キョウ", *segment));
}

TEST_F(T13nPromotionRewriterTest, PromoteKatakanaFromT13NForFewCandidates) {
  T13nPromotionRewriter rewriter;

  Segments segments;
  composer_.SetInputMode(transliteration::HIRAGANA);
  composer_.SetPreeditTextForTestOnly("きょう");
  Segment *segment = segments.push_back_segment();
  segment->set_key("きょう");
  AddCandidateWithValue("今日", segment);
  AddCandidateWithValue("きょう", segment);
  AddCandidateWithValue("強", segment);

  EXPECT_EQ(-1, GetCandidateIndexByValue("キョウ", *segment));

  EXPECT_TRUE(t13n_rewriter_->Rewrite(mobile_conv_request_, &segments));
  EXPECT_EQ(-1, GetCandidateIndexByValue("キョウ", *segment));

  EXPECT_TRUE(rewriter.Rewrite(mobile_conv_request_, &segments));
  EXPECT_EQ(3, GetCandidateIndexByValue("キョウ", *segment));
}

TEST_F(T13nPromotionRewriterTest, PromoteKatakana) {
  T13nPromotionRewriter rewriter;

  Segments segments;
  composer_.SetInputMode(transliteration::HIRAGANA);
  composer_.SetPreeditTextForTestOnly("きょう");
  Segment *segment = segments.push_back_segment();
  segment->set_key("きょう");
  AddCandidateWithValue("今日", segment);
  AddCandidateWithValue("きょう", segment);
  AddCandidateWithValue("強", segment);
  AddCandidateWithValue("教", segment);
  AddCandidateWithValue("凶", segment);
  AddCandidateWithValue("卿", segment);
  AddCandidateWithValue("京", segment);
  AddCandidateWithValue("キョウ", segment);

  const int katakana_index = GetCandidateIndexByValue("キョウ", *segment);
  EXPECT_EQ(katakana_index, 7);

  Segment::Candidate *katakana_candidate =
      segment->mutable_candidate(katakana_index);
  katakana_candidate->lid = 1;
  katakana_candidate->rid = 1;

  EXPECT_TRUE(t13n_rewriter_->Rewrite(mobile_conv_request_, &segments));

  EXPECT_TRUE(rewriter.Rewrite(mobile_conv_request_, &segments));

  const int promoted_index = GetCandidateIndexByValue("キョウ", *segment);
  // Make sure that the existing candidate was promoted.
  EXPECT_EQ(5, promoted_index);
  EXPECT_EQ(1, segment->candidate(promoted_index).lid);
  EXPECT_EQ(1, segment->candidate(promoted_index).rid);
}

TEST_F(T13nPromotionRewriterTest, KatakanaIsAlreadyRankedHigh) {
  T13nPromotionRewriter rewriter;

  Segments segments;
  composer_.SetInputMode(transliteration::HIRAGANA);
  composer_.SetPreeditTextForTestOnly("きょう");
  Segment *segment = segments.push_back_segment();
  segment->set_key("きょう");
  AddCandidateWithValue("今日", segment);
  AddCandidateWithValue("きょう", segment);
  AddCandidateWithValue("キョウ", segment);
  AddCandidateWithValue("強", segment);
  AddCandidateWithValue("教", segment);
  AddCandidateWithValue("凶", segment);
  AddCandidateWithValue("卿", segment);

  EXPECT_EQ(2, GetCandidateIndexByValue("キョウ", *segment));

  EXPECT_TRUE(t13n_rewriter_->Rewrite(mobile_conv_request_, &segments));

  EXPECT_FALSE(rewriter.Rewrite(mobile_conv_request_, &segments));
  EXPECT_EQ(2, GetCandidateIndexByValue("キョウ", *segment));
}

TEST_F(T13nPromotionRewriterTest, PromoteKatakanaForMultiSegments) {
  T13nPromotionRewriter rewriter;

  Segments segments;
  composer_.SetInputMode(transliteration::HIRAGANA);
  composer_.SetPreeditTextForTestOnly("きょうははれ");
  Segment *segment = segments.push_back_segment();
  segment->set_key("きょうは");
  AddCandidateWithValue("今日は", segment);
  AddCandidateWithValue("きょうは", segment);
  AddCandidateWithValue("強は", segment);
  AddCandidateWithValue("教は", segment);
  AddCandidateWithValue("凶は", segment);
  AddCandidateWithValue("卿は", segment);

  segment = segments.push_back_segment();
  segment->set_key("はれ");
  AddCandidateWithValue("晴れ", segment);
  AddCandidateWithValue("腫れ", segment);
  AddCandidateWithValue("晴", segment);
  AddCandidateWithValue("貼れ", segment);
  AddCandidateWithValue("張れ", segment);
  AddCandidateWithValue("脹れ", segment);

  EXPECT_TRUE(t13n_rewriter_->Rewrite(mobile_conv_request_, &segments));

  EXPECT_EQ(
      -1, GetCandidateIndexByValue("キョウハ", segments.conversion_segment(0)));
  EXPECT_EQ(-1,
            GetCandidateIndexByValue("ハレ", segments.conversion_segment(1)));

  EXPECT_TRUE(rewriter.Rewrite(mobile_conv_request_, &segments));
  EXPECT_EQ(
      5, GetCandidateIndexByValue("キョウハ", segments.conversion_segment(0)));
  EXPECT_EQ(5,
            GetCandidateIndexByValue("ハレ", segments.conversion_segment(1)));
}

TEST_F(T13nPromotionRewriterTest, PromoteLatinT13n) {
  T13nPromotionRewriter rewriter;

  Segments segments;
  Segment *segment = segments.push_back_segment();
  segment->set_key("go");
  composer_.SetInputMode(transliteration::HALF_ASCII);
  composer_.SetPreeditTextForTestOnly("go");
  AddCandidateWithValue("google", segment);
  AddCandidateWithValue("golden", segment);
  AddCandidateWithValue("goodness", segment);
  AddCandidateWithValue("governor", segment);
  AddCandidateWithValue("goalkeeper", segment);
  AddCandidateWithValue("gorgeous", segment);

  EXPECT_TRUE(t13n_rewriter_->Rewrite(mobile_conv_request_, &segments));

  EXPECT_EQ(-1, GetCandidateIndexByValue("go", segments.conversion_segment(0)));

  EXPECT_TRUE(rewriter.Rewrite(mobile_conv_request_, &segments));
  EXPECT_LE(GetCandidateIndexByValue("go", segments.conversion_segment(0)), 4);
  EXPECT_LE(GetCandidateIndexByValue("ｇｏ", segments.conversion_segment(0)),
            4);
}

TEST_F(T13nPromotionRewriterTest, PromoteLatinT13nSkipExisting) {
  T13nPromotionRewriter rewriter;

  Segments segments;
  Segment *segment = segments.push_back_segment();
  segment->set_key("go");
  composer_.SetInputMode(transliteration::HALF_ASCII);
  composer_.SetPreeditTextForTestOnly("go");
  AddCandidateWithValue("go", segment);
  AddCandidateWithValue("ｇｏ", segment);
  AddCandidateWithValue("google", segment);
  AddCandidateWithValue("golden", segment);
  AddCandidateWithValue("goodness", segment);
  AddCandidateWithValue("governor", segment);
  AddCandidateWithValue("goalkeeper", segment);
  AddCandidateWithValue("gorgeous", segment);

  EXPECT_TRUE(t13n_rewriter_->Rewrite(mobile_conv_request_, &segments));
  EXPECT_TRUE(rewriter.Rewrite(mobile_conv_request_, &segments));
  EXPECT_EQ(segments.conversion_segment(0).candidate(1).value, "ｇｏ");
  for (size_t i = 2; i < segments.conversion_segment(0).candidates_size();
       ++i) {
    // No dup T13N candidates
    EXPECT_NE(segments.conversion_segment(0).candidate(i).value, "ｇｏ");
  }
}

TEST_F(T13nPromotionRewriterTest, PromoteNumberT13n) {
  T13nPromotionRewriter rewriter;

  Segments segments;
  Segment *segment = segments.push_back_segment();
  segment->set_key("12");
  composer_.SetInputMode(transliteration::HALF_ASCII);
  composer_.SetPreeditTextForTestOnly("12");
  AddCandidateWithValue("12日", segment);
  AddCandidateWithValue("12月", segment);
  AddCandidateWithValue("1/2", segment);
  AddCandidateWithValue("12個", segment);

  EXPECT_TRUE(t13n_rewriter_->Rewrite(mobile_conv_request_, &segments));

  EXPECT_EQ(-1,
            GetCandidateIndexByValue("１２", segments.conversion_segment(0)));

  EXPECT_TRUE(rewriter.Rewrite(mobile_conv_request_, &segments));
  EXPECT_LE(GetCandidateIndexByValue("１２", segments.conversion_segment(0)),
            4);
}

}  // namespace
}  // namespace mozc
