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

#include "rewriter/variants_rewriter.h"

#include <memory>
#include <string>

#include "base/japanese_util.h"
#include "base/logging.h"
#include "config/character_form_manager.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/mozctest.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {

using ::mozc::commands::Request;
using ::mozc::config::CharacterFormManager;
using ::mozc::config::Config;
using ::mozc::dictionary::PosMatcher;

std::string AppendString(absl::string_view lhs, absl::string_view rhs) {
  auto result = std::string(lhs);
  if (!rhs.empty()) {
    result.append(1, ' ').append(rhs.data(), rhs.size());
  }
  return result;
}

}  // namespace

class VariantsRewriterTest : public ::testing::Test {
 protected:
  // Explicitly define constructor to prevent Visual C++ from
  // considering this class as POD.
  VariantsRewriterTest() {}

  void SetUp() override {
    Reset();
    pos_matcher_.Set(mock_data_manager_.GetPosMatcherData());
  }

  void TearDown() override { Reset(); }

  void Reset() {
    CharacterFormManager::GetCharacterFormManager()->SetDefaultRule();
    CharacterFormManager::GetCharacterFormManager()->ClearHistory();
  }

  static void InitSegmentsForAlphabetRewrite(const std::string &value,
                                             Segments *segments) {
    Segment *segment = segments->push_back_segment();
    CHECK(segment);
    segment->set_key(value);
    Segment::Candidate *candidate = segment->add_candidate();
    CHECK(candidate);
    candidate->Init();
    candidate->key = value;
    candidate->content_key = value;
    candidate->value = value;
    candidate->content_value = value;
  }

  VariantsRewriter *CreateVariantsRewriter() const {
    return new VariantsRewriter(pos_matcher_);
  }

  PosMatcher pos_matcher_;

 private:
  const testing::ScopedTmpUserProfileDirectory tmp_profile_dir_;
  const testing::MockDataManager mock_data_manager_;
};

TEST_F(VariantsRewriterTest, RewriteTest) {
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  Segments segments;
  const ConversionRequest request;

  Segment *seg = segments.push_back_segment();

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "あいう";
    candidate->content_value = "あいう";
    EXPECT_FALSE(rewriter->Rewrite(request, &segments));
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "012";
    candidate->content_value = "012";
    CharacterFormManager::GetCharacterFormManager()->SetCharacterForm(
        "012", Config::FULL_WIDTH);

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(2, seg->candidates_size());
    EXPECT_EQ("０１２", seg->candidate(0).value);
    EXPECT_EQ("０１２", seg->candidate(0).content_value);
    EXPECT_EQ("012", seg->candidate(1).value);
    EXPECT_EQ("012", seg->candidate(1).content_value);
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "012";
    candidate->content_value = "012";
    candidate->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
    CharacterFormManager::GetCharacterFormManager()->SetCharacterForm(
        "012", Config::FULL_WIDTH);

    EXPECT_FALSE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(1, seg->candidates_size());
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "Google";
    candidate->content_value = "Google";
    CharacterFormManager::GetCharacterFormManager()->SetCharacterForm(
        "abc", Config::FULL_WIDTH);

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(2, seg->candidates_size());
    EXPECT_EQ("Ｇｏｏｇｌｅ", seg->candidate(0).value);
    EXPECT_EQ("Ｇｏｏｇｌｅ", seg->candidate(0).content_value);
    EXPECT_EQ("Google", seg->candidate(1).value);
    EXPECT_EQ("Google", seg->candidate(1).content_value);
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "@";
    candidate->content_value = "@";
    CharacterFormManager::GetCharacterFormManager()->SetCharacterForm(
        "@", Config::FULL_WIDTH);

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(2, seg->candidates_size());
    EXPECT_EQ("＠", seg->candidate(0).value);
    EXPECT_EQ("＠", seg->candidate(0).content_value);
    EXPECT_EQ("@", seg->candidate(1).value);
    EXPECT_EQ("@", seg->candidate(1).content_value);
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "グーグル";
    candidate->content_value = "グーグル";
    CharacterFormManager::GetCharacterFormManager()->SetCharacterForm(
        "アイウ", Config::FULL_WIDTH);

    EXPECT_FALSE(rewriter->Rewrite(request, &segments));
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "グーグル";
    candidate->content_value = "グーグル";
    CharacterFormManager::GetCharacterFormManager()->AddConversionRule(
        "アイウ", Config::HALF_WIDTH);

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(2, seg->candidates_size());
    EXPECT_EQ("ｸﾞｰｸﾞﾙ", seg->candidate(0).value);
    EXPECT_EQ("ｸﾞｰｸﾞﾙ", seg->candidate(0).content_value);
    EXPECT_EQ("グーグル", seg->candidate(1).value);
    EXPECT_EQ("グーグル", seg->candidate(1).content_value);
    seg->clear_candidates();
  }
}

TEST_F(VariantsRewriterTest, RewriteTestManyCandidates) {
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  Segments segments;
  const ConversionRequest request;
  Segment *seg = segments.push_back_segment();

  {
    for (int i = 0; i < 10; ++i) {
      Segment::Candidate *candidate1 = seg->add_candidate();
      candidate1->Init();
      candidate1->value = std::to_string(i);
      candidate1->content_value = std::to_string(i);
      Segment::Candidate *candidate2 = seg->add_candidate();
      candidate2->Init();
      candidate2->content_key = "ぐーぐる";
      candidate2->key = "ぐーぐる";
      candidate2->value = "ぐーぐる";
      candidate2->content_value = "ぐーぐる";
    }

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(30, seg->candidates_size());

    for (int i = 0; i < 10; ++i) {
      EXPECT_EQ(std::to_string(i), seg->candidate(3 * i + 1).value);
      EXPECT_EQ(std::to_string(i), seg->candidate(3 * i + 1).content_value);
      std::string full_width;
      japanese_util::HalfWidthToFullWidth(seg->candidate(3 * i + 1).value,
                                          &full_width);
      EXPECT_EQ(full_width, seg->candidate(3 * i).value);
      EXPECT_EQ(full_width, seg->candidate(3 * i).content_value);
      EXPECT_EQ("ぐーぐる", seg->candidate(3 * i + 2).value);
      EXPECT_EQ("ぐーぐる", seg->candidate(3 * i + 2).content_value);
    }
  }

  {
    seg->Clear();

    for (int i = 0; i < 10; ++i) {
      Segment::Candidate *candidate1 = seg->add_candidate();
      candidate1->Init();
      candidate1->content_key = "ぐーぐる";
      candidate1->key = "ぐーぐる";
      candidate1->value = "ぐーぐる";
      candidate1->content_value = "ぐーぐる";
      Segment::Candidate *candidate2 = seg->add_candidate();
      candidate2->Init();
      candidate2->value = std::to_string(i);
      candidate2->content_value = std::to_string(i);
    }

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(30, seg->candidates_size());

    for (int i = 0; i < 10; ++i) {
      EXPECT_EQ(std::to_string(i), seg->candidate(3 * i + 2).value);
      EXPECT_EQ(std::to_string(i), seg->candidate(3 * i + 2).content_value);
      std::string full_width;
      japanese_util::HalfWidthToFullWidth(seg->candidate(3 * i + 2).value,
                                          &full_width);
      EXPECT_EQ(full_width, seg->candidate(3 * i + 1).value);
      EXPECT_EQ(full_width, seg->candidate(3 * i + 1).content_value);
      EXPECT_EQ("ぐーぐる", seg->candidate(3 * i).value);
      EXPECT_EQ("ぐーぐる", seg->candidate(3 * i).content_value);
    }
  }
}

TEST_F(VariantsRewriterTest, SetDescriptionForCandidate) {
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "HalfASCII";
    candidate.content_value = candidate.value;
    candidate.content_key = "halfascii";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "アルファベット"
    EXPECT_EQ(VariantsRewriter::kAlphabet, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "ＦｕｌｌＡＳＣＩＩ";
    candidate.content_value = candidate.value;
    candidate.content_key = "fullascii";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "[全] アルファベット"
    EXPECT_EQ(
        AppendString(VariantsRewriter::kFullWidth, VariantsRewriter::kAlphabet),
        candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "コギトエルゴスム";
    candidate.content_value = candidate.value;
    candidate.content_key = "こぎとえるごすむ";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "カタカナ"
    EXPECT_EQ(VariantsRewriter::kKatakana, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "ｺｷﾞﾄｴﾙｺﾞｽﾑ";
    candidate.content_value = candidate.value;
    candidate.content_key = "こぎとえるごすむ";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "[半] カタカナ"
    EXPECT_EQ(
        AppendString(VariantsRewriter::kHalfWidth, VariantsRewriter::kKatakana),
        candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "123";
    candidate.content_value = candidate.value;
    candidate.content_key = "123";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "数字"
    EXPECT_EQ(VariantsRewriter::kNumber, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "１２３";
    candidate.content_value = candidate.value;
    candidate.content_key = "123";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "[全] 数字"
    EXPECT_EQ(
        AppendString(VariantsRewriter::kFullWidth, VariantsRewriter::kNumber),
        candidate.description);
  }
  // containing symbols
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "Half ASCII";
    candidate.content_value = candidate.value;
    candidate.content_key = "half ascii";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "アルファベット"
    EXPECT_EQ(VariantsRewriter::kAlphabet, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "Half!ASCII!";
    candidate.content_value = candidate.value;
    candidate.content_key = "half!ascii!";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "アルファベット"
    EXPECT_EQ(VariantsRewriter::kAlphabet, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "CD-ROM";
    candidate.content_value = candidate.value;
    candidate.content_key = "しーでぃーろむ";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "アルファベット"
    EXPECT_EQ(VariantsRewriter::kAlphabet, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "コギト・エルゴ・スム";
    candidate.content_value = candidate.value;
    candidate.content_key = "こぎとえるごすむ";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "カタカナ"
    EXPECT_EQ(VariantsRewriter::kKatakana, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "!@#";
    candidate.content_value = candidate.value;
    candidate.content_key = "!@#";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    EXPECT_EQ(VariantsRewriter::kHalfWidth, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "「ＡＢＣ」";
    candidate.content_value = candidate.value;
    candidate.content_key = "[ABC]";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "[全] アルファベット"
    EXPECT_EQ(
        AppendString(VariantsRewriter::kFullWidth, VariantsRewriter::kAlphabet),
        candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "\\";
    candidate.content_value = candidate.value;
    candidate.content_key = "えん";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    const char *expected = "[半] バックスラッシュ";
    EXPECT_EQ(expected, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "＼";  // Full-width backslash
    candidate.content_value = candidate.value;
    candidate.content_key = "ばっくすらっしゅ";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    const char *expected = "[全] バックスラッシュ";
    EXPECT_EQ(expected, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "¥";  // Half-width yen-symbol
    candidate.content_value = candidate.value;
    candidate.content_key = "えん";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    std::string expected = "[半] 円記号";
    EXPECT_EQ(expected, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "￥";  // Full-width yen-symbol
    candidate.content_value = candidate.value;
    candidate.content_key = "えん";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    const char *expected = "[全] 円記号";
    EXPECT_EQ(expected, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "~";  // Tilde
    candidate.content_value = candidate.value;
    candidate.content_key = "~";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    std::string expected = "[半] チルダ";
    EXPECT_EQ(expected, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    // An emoji character of mouse face.
    candidate.value = "🐭";
    candidate.content_value = candidate.value;
    candidate.content_key = "ねずみ";
    candidate.description = "絵文字";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    std::string expected = "絵文字";
    EXPECT_EQ(expected, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    // A symbol representing "パーセント".
    candidate.value = "㌫";
    candidate.content_value = candidate.value;
    candidate.content_key = "ぱーせんと";
    candidate.description = "単位";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    std::string expected = "単位";
    EXPECT_EQ(expected, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    // Minus sign.
    candidate.value = "−";
    candidate.content_value = candidate.value;
    candidate.content_key = "まいなす";
    candidate.description = "マイナス";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    std::string expected = "[全] マイナス";
    EXPECT_EQ(expected, candidate.description);
  }
}

TEST_F(VariantsRewriterTest, SetDescriptionForTransliteration) {
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "HalfASCII";
    candidate.content_value = candidate.value;
    candidate.content_key = "halfascii";
    VariantsRewriter::SetDescriptionForTransliteration(pos_matcher_,
                                                       &candidate);
    // "アルファベット"
    EXPECT_EQ(VariantsRewriter::kAlphabet, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "!@#";
    candidate.content_value = candidate.value;
    candidate.content_key = "!@#";
    VariantsRewriter::SetDescriptionForTransliteration(pos_matcher_,
                                                       &candidate);
    // "[半]"
    EXPECT_EQ(VariantsRewriter::kHalfWidth, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "「ＡＢＣ」";
    candidate.content_value = candidate.value;
    candidate.content_key = "[ABC]";
    VariantsRewriter::SetDescriptionForTransliteration(pos_matcher_,
                                                       &candidate);
    // "[全] アルファベット"
    EXPECT_EQ(
        AppendString(VariantsRewriter::kFullWidth, VariantsRewriter::kAlphabet),
        candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    // A symbol representing "パーセント".
    candidate.value = "㌫";
    candidate.content_value = candidate.value;
    candidate.content_key = "ぱーせんと";
    candidate.description = "単位";
    VariantsRewriter::SetDescriptionForTransliteration(pos_matcher_,
                                                       &candidate);
    std::string expected = "単位";
    EXPECT_EQ(expected, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    // Minus sign.
    candidate.value = "−";
    candidate.content_value = candidate.value;
    candidate.content_key = "まいなす";
    candidate.description = "マイナス";
    VariantsRewriter::SetDescriptionForTransliteration(pos_matcher_,
                                                       &candidate);
    std::string expected = "[全] マイナス";
    EXPECT_EQ(expected, candidate.description);
  }
}

TEST_F(VariantsRewriterTest, SetDescriptionForPrediction) {
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "HalfASCII";
    candidate.content_value = candidate.value;
    candidate.content_key = "halfascii";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ("", candidate.description);
  }
  // containing symbols
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "Half ASCII";
    candidate.content_value = candidate.value;
    candidate.content_key = "half ascii";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ("", candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "Half!ASCII!";
    candidate.content_value = candidate.value;
    candidate.content_key = "half!ascii!";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ("", candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "CD-ROM";
    candidate.content_value = candidate.value;
    candidate.content_key = "しーでぃーろむ";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ("", candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "!@#";
    candidate.content_value = candidate.value;
    candidate.content_key = "!@#";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ("", candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "「ＡＢＣ」";
    candidate.content_value = candidate.value;
    candidate.content_key = "[ABC]";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ("", candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    // A symbol representing "パーセント".
    candidate.value = "㌫";
    candidate.content_value = candidate.value;
    candidate.content_key = "ぱーせんと";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    std::string expected = "";
    EXPECT_EQ(expected, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    // Minus sign.
    candidate.value = "−";
    candidate.content_value = candidate.value;
    candidate.content_key = "まいなす";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    std::string expected = "";
    EXPECT_EQ(expected, candidate.description);
  }
}

TEST_F(VariantsRewriterTest, RewriteForConversion) {
  CharacterFormManager *character_form_manager =
      CharacterFormManager::GetCharacterFormManager();
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  ConversionRequest request;
  {
    Segments segments;
    {
      Segment *segment = segments.push_back_segment();
      segment->set_key("abc");
      Segment::Candidate *candidate = segment->add_candidate();
      candidate->Init();
      candidate->key = "abc";
      candidate->content_key = "abc";
      candidate->value = "abc";
      candidate->content_value = "abc";
    }
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(1, segments.segments_size());
    EXPECT_EQ(2, segments.segment(0).candidates_size());

    EXPECT_EQ(Config::FULL_WIDTH,
              character_form_manager->GetConversionCharacterForm("abc"));

    EXPECT_EQ("ａｂｃ", segments.segment(0).candidate(0).value);
    EXPECT_EQ("abc", segments.segment(0).candidate(1).value);
  }
  {
    character_form_manager->SetCharacterForm("abc", Config::HALF_WIDTH);
    Segments segments;
    {
      Segment *segment = segments.push_back_segment();
      segment->set_key("abc");
      Segment::Candidate *candidate = segment->add_candidate();
      candidate->Init();
      candidate->key = "abc";
      candidate->content_key = "abc";
      candidate->value = "abc";
      candidate->content_value = "abc";
    }
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(1, segments.segments_size());
    EXPECT_EQ(2, segments.segment(0).candidates_size());

    EXPECT_EQ(Config::HALF_WIDTH,
              character_form_manager->GetConversionCharacterForm("abc"));

    EXPECT_EQ("abc", segments.segment(0).candidate(0).value);
    EXPECT_EQ("ａｂｃ", segments.segment(0).candidate(1).value);
  }
  {
    Segments segments;
    {
      Segment *segment = segments.push_back_segment();
      segment->set_key("~");
      Segment::Candidate *candidate = segment->add_candidate();
      candidate->Init();
      candidate->key = "~";
      candidate->content_key = "~";
      candidate->value = "〜";
      candidate->content_value = "〜";
      candidate->description = "波ダッシュ";
    }
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(1, segments.segments_size());
    EXPECT_EQ(2, segments.segment(0).candidates_size());

    EXPECT_EQ(Config::FULL_WIDTH,
              character_form_manager->GetConversionCharacterForm("~"));

    EXPECT_EQ("〜", segments.segment(0).candidate(0).value);
    EXPECT_EQ("[全] 波ダッシュ", segments.segment(0).candidate(0).description);
    EXPECT_EQ("~", segments.segment(0).candidate(1).value);
    EXPECT_EQ("[半] チルダ", segments.segment(0).candidate(1).description);
  }
}

TEST_F(VariantsRewriterTest, RewriteForPrediction) {
  CharacterFormManager *character_form_manager =
      CharacterFormManager::GetCharacterFormManager();
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  ConversionRequest request;
  request.set_request_type(ConversionRequest::PREDICTION);
  {
    Segments segments;
    InitSegmentsForAlphabetRewrite("abc", &segments);
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(1, segments.segments_size());
    EXPECT_EQ(2, segments.segment(0).candidates_size());

    EXPECT_EQ(Config::FULL_WIDTH,
              character_form_manager->GetConversionCharacterForm("abc"));

    EXPECT_EQ("ａｂｃ", segments.segment(0).candidate(0).value);
    EXPECT_EQ("abc", segments.segment(0).candidate(1).value);
  }
  {
    character_form_manager->SetCharacterForm("abc", Config::HALF_WIDTH);
    Segments segments;
    InitSegmentsForAlphabetRewrite("abc", &segments);
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(1, segments.segments_size());
    EXPECT_EQ(2, segments.segment(0).candidates_size());

    EXPECT_EQ(Config::HALF_WIDTH,
              character_form_manager->GetConversionCharacterForm("abc"));

    EXPECT_EQ("abc", segments.segment(0).candidate(0).value);
    EXPECT_EQ("ａｂｃ", segments.segment(0).candidate(1).value);
  }
}

TEST_F(VariantsRewriterTest, RewriteForMixedConversion) {
  CharacterFormManager *character_form_manager =
      CharacterFormManager::GetCharacterFormManager();
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  Request request;
  request.set_mixed_conversion(true);  // Request mixed conversion.
  ConversionRequest conv_request;
  conv_request.set_request(&request);
  conv_request.set_request_type(ConversionRequest::SUGGESTION);
  {
    Segments segments;
    InitSegmentsForAlphabetRewrite("abc", &segments);
    EXPECT_TRUE(rewriter->Rewrite(conv_request, &segments));
    EXPECT_EQ(1, segments.segments_size());
    EXPECT_EQ(2, segments.segment(0).candidates_size());
    EXPECT_EQ(Config::FULL_WIDTH,
              character_form_manager->GetConversionCharacterForm("abc"));
    EXPECT_EQ("ａｂｃ", segments.segment(0).candidate(0).value);
    EXPECT_EQ("abc", segments.segment(0).candidate(1).value);
  }
  {
    character_form_manager->SetCharacterForm("abc", Config::HALF_WIDTH);
    Segments segments;
    InitSegmentsForAlphabetRewrite("abc", &segments);
    EXPECT_TRUE(rewriter->Rewrite(conv_request, &segments));
    EXPECT_EQ(1, segments.segments_size());
    EXPECT_EQ(2, segments.segment(0).candidates_size());
    EXPECT_EQ(Config::HALF_WIDTH,
              character_form_manager->GetConversionCharacterForm("abc"));
    EXPECT_EQ("abc", segments.segment(0).candidate(0).value);
    EXPECT_EQ("ａｂｃ", segments.segment(0).candidate(1).value);
  }
  {
    // Test for candidate with inner segment boundary.
    // The test case is based on b/116826494.
    character_form_manager->SetCharacterForm("0", Config::HALF_WIDTH);

    Segments segments;

    Segment *segment = segments.push_back_segment();
    segment->set_key("さんえん");

    Segment::Candidate *candidate = segment->add_candidate();
    candidate->Init();
    candidate->key = "さんえん";
    candidate->content_key = candidate->key;
    candidate->value = "３円";  // Full-width three.
    candidate->content_value = candidate->value;
    candidate->inner_segment_boundary = {
        Segment::Candidate::EncodeLengths(6, 3, 6, 3),
        Segment::Candidate::EncodeLengths(6, 3, 6, 3),
    };

    EXPECT_TRUE(rewriter->Rewrite(conv_request, &segments));

    ASSERT_EQ(1, segments.segments_size());
    ASSERT_EQ(2, segments.segment(0).candidates_size());

    // Since the character form preference is set to Config::HALF_WIDTH, the
    // half-width variant comes first.
    const Segment::Candidate &half = segments.segment(0).candidate(0);
    EXPECT_EQ("さんえん", half.key);
    EXPECT_EQ("3円", half.value);
    ASSERT_EQ(2, half.inner_segment_boundary.size());
    EXPECT_EQ(Segment::Candidate::EncodeLengths(6, 1, 6, 1),
              half.inner_segment_boundary[0]);
    EXPECT_EQ(Segment::Candidate::EncodeLengths(6, 3, 6, 3),
              half.inner_segment_boundary[1]);

    const Segment::Candidate &full = segments.segment(0).candidate(1);
    EXPECT_EQ("さんえん", full.key);
    EXPECT_EQ("３円", full.value);
    ASSERT_EQ(2, full.inner_segment_boundary.size());
    EXPECT_EQ(Segment::Candidate::EncodeLengths(6, 3, 6, 3),
              full.inner_segment_boundary[0]);
    EXPECT_EQ(Segment::Candidate::EncodeLengths(6, 3, 6, 3),
              full.inner_segment_boundary[1]);
  }
}

TEST_F(VariantsRewriterTest, RewriteForPartialSuggestion) {
  CharacterFormManager *character_form_manager =
      CharacterFormManager::GetCharacterFormManager();
  EXPECT_EQ(Config::FULL_WIDTH,
            character_form_manager->GetConversionCharacterForm("0"));
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  Request request;
  request.set_mixed_conversion(true);  // Request mixed conversion.
  ConversionRequest conv_request;
  conv_request.set_request(&request);
  conv_request.set_request_type(ConversionRequest::SUGGESTION);
  {
    Segments segments;

    Segment *segment = segments.push_back_segment();
    segment->set_key("3えん");

    Segment::Candidate *candidate = segment->add_candidate();
    candidate->Init();
    candidate->key = "3";
    candidate->content_key = candidate->key;
    candidate->value = "３";  // Full-width three.
    candidate->content_value = candidate->value;
    candidate->consumed_key_size = 1;
    candidate->attributes |= Segment::Candidate::PARTIALLY_KEY_CONSUMED;
    candidate->attributes |= Segment::Candidate::AUTO_PARTIAL_SUGGESTION;

    EXPECT_TRUE(rewriter->Rewrite(conv_request, &segments));

    EXPECT_EQ(1, segments.segments_size());
    EXPECT_EQ(2, segments.segment(0).candidates_size());

    for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
      const Segment::Candidate &cand = segments.segment(0).candidate(i);
      EXPECT_EQ(1, cand.consumed_key_size);
      EXPECT_TRUE(cand.attributes & Segment::Candidate::PARTIALLY_KEY_CONSUMED);
      EXPECT_TRUE(cand.attributes &
                  Segment::Candidate::AUTO_PARTIAL_SUGGESTION);
    }
  }
}

TEST_F(VariantsRewriterTest, RewriteForSuggestion) {
  CharacterFormManager *character_form_manager =
      CharacterFormManager::GetCharacterFormManager();
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  ConversionRequest request;
  request.set_request_type(ConversionRequest::SUGGESTION);
  {
    Segments segments;
    InitSegmentsForAlphabetRewrite("abc", &segments);
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(1, segments.segments_size());
    EXPECT_EQ(1, segments.segment(0).candidates_size());

    EXPECT_EQ(Config::FULL_WIDTH,
              character_form_manager->GetConversionCharacterForm("abc"));

    EXPECT_EQ("ａｂｃ", segments.segment(0).candidate(0).value);
  }
  {
    character_form_manager->SetCharacterForm("abc", Config::HALF_WIDTH);
    Segments segments;
    InitSegmentsForAlphabetRewrite("abc", &segments);
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(1, segments.segments_size());
    EXPECT_EQ(1, segments.segment(0).candidates_size());

    EXPECT_EQ(Config::HALF_WIDTH,
              character_form_manager->GetConversionCharacterForm("abc"));

    EXPECT_EQ("abc", segments.segment(0).candidate(0).value);
  }
  {
    // Test for candidate with inner segment boundary.
    Segments segments;

    Segment *segment = segments.push_back_segment();
    segment->set_key("まじ!");

    Segment::Candidate *candidate = segment->add_candidate();
    candidate->Init();
    candidate->key = "まじ!";
    candidate->content_key = candidate->key;
    candidate->value = "マジ!";
    candidate->content_value = candidate->value;
    candidate->inner_segment_boundary.push_back(
        Segment::Candidate::EncodeLengths(6, 6, 6, 6));  // 6 bytes for "まじ"
    candidate->inner_segment_boundary.push_back(
        Segment::Candidate::EncodeLengths(1, 1, 1, 1));  // 1 byte for "!"

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    ASSERT_EQ(1, segments.segments_size());
    ASSERT_EQ(1, segments.segment(0).candidates_size());

    const Segment::Candidate &rewritten_candidate =
        segments.segment(0).candidate(0);
    EXPECT_EQ("マジ！",  // "マジ！" (full-width)
              rewritten_candidate.value);
    EXPECT_EQ("マジ！",  // "マジ！" (full-width)
              rewritten_candidate.content_value);
    ASSERT_EQ(2, rewritten_candidate.inner_segment_boundary.size());

    // Boundary information for
    // key="まじ", value="マジ", ckey="まじ", cvalue="マジ"
    EXPECT_EQ(Segment::Candidate::EncodeLengths(6, 6, 6, 6),
              rewritten_candidate.inner_segment_boundary[0]);
    // Boundary information for
    // key="!", value="！", ckey="!", cvalue="！".
    // Values are converted to full-width.
    EXPECT_EQ(Segment::Candidate::EncodeLengths(1, 3, 1, 3),
              rewritten_candidate.inner_segment_boundary[1]);
  }
}

TEST_F(VariantsRewriterTest, Capability) {
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  const ConversionRequest request;
  EXPECT_EQ(RewriterInterface::ALL, rewriter->capability(request));
}

TEST_F(VariantsRewriterTest, Finish) {
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  auto *manager = CharacterFormManager::GetCharacterFormManager();
  const ConversionRequest request;

  Segments segments;

  Segment *segment = segments.push_back_segment();
  segment->set_key("いちにさん");
  segment->set_segment_type(Segment::FIXED_VALUE);

  Segment::Candidate *cand = segment->add_candidate();
  cand->Init();
  cand->key = "いちにさん";
  cand->content_key = cand->key;

  // Half-width number with style.
  cand->value = "123";
  cand->content_value = cand->value;
  cand->style = NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_HALFWIDTH;
  rewriter->Finish(request, &segments);
  EXPECT_EQ(Config::HALF_WIDTH, manager->GetConversionCharacterForm("0"));

  // Full-width number with style.
  cand->value = "１２３";
  cand->content_value = cand->value;
  cand->style = NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH;
  rewriter->Finish(request, &segments);
  EXPECT_EQ(Config::FULL_WIDTH, manager->GetConversionCharacterForm("0"));

  // Half-width number expression with description.
  cand->value = "3時";
  cand->content_value = cand->value;
  cand->style = NumberUtil::NumberString::DEFAULT_STYLE;
  cand->description = std::string(VariantsRewriter::kHalfWidth);
  rewriter->Finish(request, &segments);
  EXPECT_EQ(Config::HALF_WIDTH, manager->GetConversionCharacterForm("0"));

  // Full-width number expression with description.
  cand->value = "３時";
  cand->content_value = cand->value;
  cand->style = NumberUtil::NumberString::DEFAULT_STYLE;
  cand->description = std::string(VariantsRewriter::kFullWidth);
  rewriter->Finish(request, &segments);
  EXPECT_EQ(Config::FULL_WIDTH, manager->GetConversionCharacterForm("0"));
}

}  // namespace mozc
