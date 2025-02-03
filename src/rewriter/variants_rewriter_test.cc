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

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/japanese_util.h"
#include "base/number_util.h"
#include "base/util.h"
#include "config/character_form_manager.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

using ::mozc::commands::Request;
using ::mozc::config::CharacterFormManager;
using ::mozc::config::Config;
using ::mozc::dictionary::PosMatcher;

class VariantsRewriterTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    Reset();
    pos_matcher_.Set(mock_data_manager_.GetPosMatcherData());
  }

  void TearDown() override { Reset(); }

  void Reset() {
    CharacterFormManager::GetCharacterFormManager()->SetDefaultRule();
    CharacterFormManager::GetCharacterFormManager()->ClearHistory();
  }

  static void InitSegmentsForAlphabetRewrite(const absl::string_view value,
                                             Segments *segments) {
    Segment *segment = segments->push_back_segment();
    CHECK(segment);
    segment->set_key(value);
    Segment::Candidate *candidate = segment->add_candidate();
    CHECK(candidate);
    candidate->key = std::string(value);
    candidate->content_key = std::string(value);
    candidate->value = std::string(value);
    candidate->content_value = std::string(value);
  }

  VariantsRewriter *CreateVariantsRewriter() const {
    return new VariantsRewriter(pos_matcher_);
  }

  PosMatcher pos_matcher_;

 private:
  const testing::MockDataManager mock_data_manager_;
};

TEST_F(VariantsRewriterTest, RewriteTest) {
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  Segments segments;
  const ConversionRequest request;

  Segment *seg = segments.push_back_segment();

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->value = "あいう";
    candidate->content_value = "あいう";
    EXPECT_FALSE(rewriter->Rewrite(request, &segments));
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->value = "012";
    candidate->content_value = "012";
    CharacterFormManager::GetCharacterFormManager()->SetCharacterForm(
        "012", Config::FULL_WIDTH);

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 2);
    EXPECT_EQ(seg->candidate(0).value, "０１２");
    EXPECT_EQ(seg->candidate(0).content_value, "０１２");
    EXPECT_EQ(seg->candidate(1).value, "012");
    EXPECT_EQ(seg->candidate(1).content_value, "012");
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->value = "012";
    candidate->content_value = "012";
    candidate->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
    CharacterFormManager::GetCharacterFormManager()->SetCharacterForm(
        "012", Config::FULL_WIDTH);

    EXPECT_FALSE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 1);
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->value = "Google";
    candidate->content_value = "Google";
    CharacterFormManager::GetCharacterFormManager()->SetCharacterForm(
        "abc", Config::FULL_WIDTH);

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 2);
    EXPECT_EQ(seg->candidate(0).value, "Ｇｏｏｇｌｅ");
    EXPECT_EQ(seg->candidate(0).content_value, "Ｇｏｏｇｌｅ");
    EXPECT_EQ(seg->candidate(1).value, "Google");
    EXPECT_EQ(seg->candidate(1).content_value, "Google");
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->value = "@";
    candidate->content_value = "@";
    CharacterFormManager::GetCharacterFormManager()->SetCharacterForm(
        "@", Config::FULL_WIDTH);

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 2);
    EXPECT_EQ(seg->candidate(0).value, "＠");
    EXPECT_EQ(seg->candidate(0).content_value, "＠");
    EXPECT_EQ(seg->candidate(1).value, "@");
    EXPECT_EQ(seg->candidate(1).content_value, "@");
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->value = "グーグル";
    candidate->content_value = "グーグル";
    CharacterFormManager::GetCharacterFormManager()->SetCharacterForm(
        "アイウ", Config::FULL_WIDTH);

    EXPECT_FALSE(rewriter->Rewrite(request, &segments));
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->value = "グーグル";
    candidate->content_value = "グーグル";
    CharacterFormManager::GetCharacterFormManager()->AddConversionRule(
        "アイウ", Config::HALF_WIDTH);

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 2);
    EXPECT_EQ(seg->candidate(0).value, "ｸﾞｰｸﾞﾙ");
    EXPECT_EQ(seg->candidate(0).content_value, "ｸﾞｰｸﾞﾙ");
    EXPECT_EQ(seg->candidate(1).value, "グーグル");
    EXPECT_EQ(seg->candidate(1).content_value, "グーグル");
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
      candidate1->value = std::to_string(i);
      candidate1->content_value = std::to_string(i);
      Segment::Candidate *candidate2 = seg->add_candidate();
      candidate2->content_key = "ぐーぐる";
      candidate2->key = "ぐーぐる";
      candidate2->value = "ぐーぐる";
      candidate2->content_value = "ぐーぐる";
    }

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 30);

    for (int i = 0; i < 10; ++i) {
      EXPECT_EQ(seg->candidate(3 * i + 1).value, std::to_string(i));
      EXPECT_EQ(seg->candidate(3 * i + 1).content_value, std::to_string(i));
      std::string full_width =
          japanese_util::HalfWidthToFullWidth(seg->candidate(3 * i + 1).value);
      EXPECT_EQ(seg->candidate(3 * i).value, full_width);
      EXPECT_EQ(seg->candidate(3 * i).content_value, full_width);
      EXPECT_EQ(seg->candidate(3 * i + 2).value, "ぐーぐる");
      EXPECT_EQ(seg->candidate(3 * i + 2).content_value, "ぐーぐる");
    }
  }

  {
    seg->Clear();

    for (int i = 0; i < 10; ++i) {
      Segment::Candidate *candidate1 = seg->add_candidate();
      candidate1->content_key = "ぐーぐる";
      candidate1->key = "ぐーぐる";
      candidate1->value = "ぐーぐる";
      candidate1->content_value = "ぐーぐる";
      Segment::Candidate *candidate2 = seg->add_candidate();
      candidate2->value = std::to_string(i);
      candidate2->content_value = std::to_string(i);
    }

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 30);

    for (int i = 0; i < 10; ++i) {
      EXPECT_EQ(seg->candidate(3 * i + 2).value, std::to_string(i));
      EXPECT_EQ(seg->candidate(3 * i + 2).content_value, std::to_string(i));
      std::string full_width =
          japanese_util::HalfWidthToFullWidth(seg->candidate(3 * i + 2).value);
      EXPECT_EQ(seg->candidate(3 * i + 1).value, full_width);
      EXPECT_EQ(seg->candidate(3 * i + 1).content_value, full_width);
      EXPECT_EQ(seg->candidate(3 * i).value, "ぐーぐる");
      EXPECT_EQ(seg->candidate(3 * i).content_value, "ぐーぐる");
    }
  }
}

TEST_F(VariantsRewriterTest, SetDescriptionForCandidate) {
  {
    Segment::Candidate candidate;
    candidate.value = "HalfASCII";
    candidate.content_value = candidate.value;
    candidate.content_key = "halfascii";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "アルファベット"
    EXPECT_EQ(candidate.description, VariantsRewriter::kAlphabet);
  }
  {
    Segment::Candidate candidate;
    candidate.value = "ＦｕｌｌＡＳＣＩＩ";
    candidate.content_value = candidate.value;
    candidate.content_key = "fullascii";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "[全] アルファベット"
    EXPECT_EQ(absl::StrCat(VariantsRewriter::kFullWidth, " ",
                           VariantsRewriter::kAlphabet),
              candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.value = "コギトエルゴスム";
    candidate.content_value = candidate.value;
    candidate.content_key = "こぎとえるごすむ";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "カタカナ"
    EXPECT_EQ(candidate.description, VariantsRewriter::kKatakana);
  }
  {
    Segment::Candidate candidate;
    candidate.value = "ｺｷﾞﾄｴﾙｺﾞｽﾑ";
    candidate.content_value = candidate.value;
    candidate.content_key = "こぎとえるごすむ";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "[半] カタカナ"
    EXPECT_EQ(absl::StrCat(VariantsRewriter::kHalfWidth, " ",
                           VariantsRewriter::kKatakana),
              candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.value = "123";
    candidate.content_value = candidate.value;
    candidate.content_key = "123";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "数字"
    EXPECT_EQ(candidate.description, VariantsRewriter::kNumber);
  }
  {
    Segment::Candidate candidate;
    candidate.value = "１２３";
    candidate.content_value = candidate.value;
    candidate.content_key = "123";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "[全] 数字"
    EXPECT_EQ(absl::StrCat(VariantsRewriter::kFullWidth, " ",
                           VariantsRewriter::kNumber),
              candidate.description);
  }
  // containing symbols
  {
    Segment::Candidate candidate;
    candidate.value = "Half ASCII";
    candidate.content_value = candidate.value;
    candidate.content_key = "half ascii";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "アルファベット"
    EXPECT_EQ(candidate.description, VariantsRewriter::kAlphabet);
  }
  {
    Segment::Candidate candidate;
    candidate.value = "Half!ASCII!";
    candidate.content_value = candidate.value;
    candidate.content_key = "half!ascii!";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "アルファベット"
    EXPECT_EQ(candidate.description, VariantsRewriter::kAlphabet);
  }
  {
    Segment::Candidate candidate;
    candidate.value = "CD-ROM";
    candidate.content_value = candidate.value;
    candidate.content_key = "しーでぃーろむ";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "アルファベット"
    EXPECT_EQ(candidate.description, VariantsRewriter::kAlphabet);
  }
  {
    Segment::Candidate candidate;
    candidate.value = "コギト・エルゴ・スム";
    candidate.content_value = candidate.value;
    candidate.content_key = "こぎとえるごすむ";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "カタカナ"
    EXPECT_EQ(candidate.description, VariantsRewriter::kKatakana);
  }
  {
    Segment::Candidate candidate;
    candidate.value = "!@#";
    candidate.content_value = candidate.value;
    candidate.content_key = "!@#";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    EXPECT_EQ(candidate.description, VariantsRewriter::kHalfWidth);
  }
  {
    Segment::Candidate candidate;
    candidate.value = "「ＡＢＣ」";
    candidate.content_value = candidate.value;
    candidate.content_key = "[ABC]";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "[全] アルファベット"
    EXPECT_EQ(absl::StrCat(VariantsRewriter::kFullWidth, " ",
                           VariantsRewriter::kAlphabet),
              candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.value = "\\";
    candidate.content_value = candidate.value;
    candidate.content_key = "えん";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    constexpr absl::string_view kExpected = "[半] バックスラッシュ";
    EXPECT_EQ(candidate.description, kExpected);
  }
  {
    Segment::Candidate candidate;
    candidate.value = "＼";  // Full-width backslash
    candidate.content_value = candidate.value;
    candidate.content_key = "ばっくすらっしゅ";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    constexpr absl::string_view kExpected = "[全] バックスラッシュ";
    EXPECT_EQ(candidate.description, kExpected);
  }
  {
    Segment::Candidate candidate;
    candidate.value = "¥";  // Half-width yen-symbol
    candidate.content_value = candidate.value;
    candidate.content_key = "えん";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    std::string expected = "[半] 円記号";
    EXPECT_EQ(candidate.description, expected);
  }
  {
    Segment::Candidate candidate;
    candidate.value = "￥";  // Full-width yen-symbol
    candidate.content_value = candidate.value;
    candidate.content_key = "えん";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    constexpr absl::string_view kExpected = "[全] 円記号";
    EXPECT_EQ(candidate.description, kExpected);
  }
  {
    Segment::Candidate candidate;
    candidate.value = "~";  // Tilde
    candidate.content_value = candidate.value;
    candidate.content_key = "~";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    std::string expected = "[半] チルダ";
    EXPECT_EQ(candidate.description, expected);
  }
  {
    Segment::Candidate candidate;
    // An emoji character of mouse face.
    candidate.value = "🐭";
    candidate.content_value = candidate.value;
    candidate.content_key = "ねずみ";
    candidate.description = "絵文字";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    std::string expected = "絵文字";
    EXPECT_EQ(candidate.description, expected);
  }
  {
    Segment::Candidate candidate;
    // A symbol representing "パーセント".
    candidate.value = "㌫";
    candidate.content_value = candidate.value;
    candidate.content_key = "ぱーせんと";
    candidate.description = "単位";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    std::string expected = "単位";
    EXPECT_EQ(candidate.description, expected);
  }
  {
    Segment::Candidate candidate;
    // Minus sign.
    candidate.value = "−";
    candidate.content_value = candidate.value;
    candidate.content_key = "まいなす";
    candidate.description = "マイナス";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    std::string expected = "[全] マイナス";
    EXPECT_EQ(candidate.description, expected);
  }
}

TEST_F(VariantsRewriterTest, SetDescriptionForTransliteration) {
  {
    Segment::Candidate candidate;
    candidate.value = "HalfASCII";
    candidate.content_value = candidate.value;
    candidate.content_key = "halfascii";
    VariantsRewriter::SetDescriptionForTransliteration(pos_matcher_,
                                                       &candidate);
    // "アルファベット"
    EXPECT_EQ(candidate.description, VariantsRewriter::kAlphabet);
  }
  {
    Segment::Candidate candidate;
    candidate.value = "!@#";
    candidate.content_value = candidate.value;
    candidate.content_key = "!@#";
    VariantsRewriter::SetDescriptionForTransliteration(pos_matcher_,
                                                       &candidate);
    // "[半]"
    EXPECT_EQ(candidate.description, VariantsRewriter::kHalfWidth);
  }
  {
    Segment::Candidate candidate;
    candidate.value = "「ＡＢＣ」";
    candidate.content_value = candidate.value;
    candidate.content_key = "[ABC]";
    VariantsRewriter::SetDescriptionForTransliteration(pos_matcher_,
                                                       &candidate);
    // "[全] アルファベット"
    EXPECT_EQ(absl::StrCat(VariantsRewriter::kFullWidth, " ",
                           VariantsRewriter::kAlphabet),
              candidate.description);
  }
  {
    Segment::Candidate candidate;
    // A symbol representing "パーセント".
    candidate.value = "㌫";
    candidate.content_value = candidate.value;
    candidate.content_key = "ぱーせんと";
    candidate.description = "単位";
    VariantsRewriter::SetDescriptionForTransliteration(pos_matcher_,
                                                       &candidate);
    std::string expected = "単位";
    EXPECT_EQ(candidate.description, expected);
  }
  {
    Segment::Candidate candidate;
    // Minus sign.
    candidate.value = "−";
    candidate.content_value = candidate.value;
    candidate.content_key = "まいなす";
    candidate.description = "マイナス";
    VariantsRewriter::SetDescriptionForTransliteration(pos_matcher_,
                                                       &candidate);
    std::string expected = "[全] マイナス";
    EXPECT_EQ(candidate.description, expected);
  }
}

TEST_F(VariantsRewriterTest, SetDescriptionForPrediction) {
  {
    Segment::Candidate candidate;
    candidate.value = "HalfASCII";
    candidate.content_value = candidate.value;
    candidate.content_key = "halfascii";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ(candidate.description, "");
  }
  // containing symbols
  {
    Segment::Candidate candidate;
    candidate.value = "Half ASCII";
    candidate.content_value = candidate.value;
    candidate.content_key = "half ascii";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ(candidate.description, "");
  }
  {
    Segment::Candidate candidate;
    candidate.value = "Half!ASCII!";
    candidate.content_value = candidate.value;
    candidate.content_key = "half!ascii!";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ(candidate.description, "");
  }
  {
    Segment::Candidate candidate;
    candidate.value = "CD-ROM";
    candidate.content_value = candidate.value;
    candidate.content_key = "しーでぃーろむ";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ(candidate.description, "");
  }
  {
    Segment::Candidate candidate;
    candidate.value = "!@#";
    candidate.content_value = candidate.value;
    candidate.content_key = "!@#";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ(candidate.description, "");
  }
  {
    Segment::Candidate candidate;
    candidate.value = "「ＡＢＣ」";
    candidate.content_value = candidate.value;
    candidate.content_key = "[ABC]";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ(candidate.description, "");
  }
  {
    Segment::Candidate candidate;
    // A symbol representing "パーセント".
    candidate.value = "㌫";
    candidate.content_value = candidate.value;
    candidate.content_key = "ぱーせんと";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    std::string expected = "";
    EXPECT_EQ(candidate.description, expected);
  }
  {
    Segment::Candidate candidate;
    // Minus sign.
    candidate.value = "−";
    candidate.content_value = candidate.value;
    candidate.content_key = "まいなす";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    std::string expected = "";
    EXPECT_EQ(candidate.description, expected);
  }
}

TEST_F(VariantsRewriterTest, GetFormTypesFromStringPair) {
  constexpr std::pair<Util::FormType, Util::FormType> kUnknownForm = {
      Util::UNKNOWN_FORM, Util::UNKNOWN_FORM};
  EXPECT_EQ(VariantsRewriter::GetFormTypesFromStringPair("", ""), kUnknownForm);

  EXPECT_EQ(VariantsRewriter::GetFormTypesFromStringPair("abc", "ab"),
            kUnknownForm);

  EXPECT_EQ(VariantsRewriter::GetFormTypesFromStringPair("abc", "abc"),
            kUnknownForm);

  EXPECT_EQ(VariantsRewriter::GetFormTypesFromStringPair("12", "12"),
            kUnknownForm);

  EXPECT_EQ(VariantsRewriter::GetFormTypesFromStringPair("あいう", "あいう"),
            kUnknownForm);

  EXPECT_EQ(VariantsRewriter::GetFormTypesFromStringPair("アイウ", "アイウ"),
            kUnknownForm);

  EXPECT_EQ(VariantsRewriter::GetFormTypesFromStringPair("愛", "恋"),
            kUnknownForm);

  constexpr std::pair<Util::FormType, Util::FormType> kHalfFullPair = {
      Util::HALF_WIDTH, Util::FULL_WIDTH};
  constexpr std::pair<Util::FormType, Util::FormType> kFullHalfPair = {
      Util::FULL_WIDTH, Util::HALF_WIDTH};

  EXPECT_EQ(VariantsRewriter::GetFormTypesFromStringPair("ABC", "ＡＢＣ"),
            kHalfFullPair);

  EXPECT_EQ(VariantsRewriter::GetFormTypesFromStringPair("ａｂｃ", "abc"),
            kFullHalfPair);

  EXPECT_EQ(VariantsRewriter::GetFormTypesFromStringPair("おばQ", "おばＱ"),
            kHalfFullPair);

  EXPECT_EQ(VariantsRewriter::GetFormTypesFromStringPair("よろしくヨロシク",
                                                         "よろしくﾖﾛｼｸ"),
            kFullHalfPair);

  EXPECT_EQ(VariantsRewriter::GetFormTypesFromStringPair("よろしくグーグル",
                                                         "よろしくｸﾞｰｸﾞﾙ"),
            kFullHalfPair);

  // semi voice sound mark
  EXPECT_EQ(VariantsRewriter::GetFormTypesFromStringPair(
                "カッパよろしくグーグル", "ｶｯﾊﾟよろしくｸﾞｰｸﾞﾙ"),
            kFullHalfPair);

  EXPECT_EQ(VariantsRewriter::GetFormTypesFromStringPair("ヨロシクＱ", "ﾖﾛｼｸQ"),
            kFullHalfPair);

  // // mixed
  EXPECT_EQ(VariantsRewriter::GetFormTypesFromStringPair("ヨロシクQ", "ﾖﾛｼｸＱ"),
            kUnknownForm);

  EXPECT_EQ(VariantsRewriter::GetFormTypesFromStringPair("京都Qぐーぐる",
                                                         "京都Ｑぐーぐる"),
            kHalfFullPair);
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
      candidate->key = "abc";
      candidate->content_key = "abc";
      candidate->value = "abc";
      candidate->content_value = "abc";
    }
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    EXPECT_EQ(segments.segment(0).candidates_size(), 2);

    EXPECT_EQ(Config::FULL_WIDTH,
              character_form_manager->GetConversionCharacterForm("abc"));

    EXPECT_EQ(segments.segment(0).candidate(0).value, "ａｂｃ");
    EXPECT_EQ(segments.segment(0).candidate(1).value, "abc");
  }
  {
    character_form_manager->SetCharacterForm("abc", Config::HALF_WIDTH);
    Segments segments;
    {
      Segment *segment = segments.push_back_segment();
      segment->set_key("abc");
      Segment::Candidate *candidate = segment->add_candidate();
      candidate->key = "abc";
      candidate->content_key = "abc";
      candidate->value = "abc";
      candidate->content_value = "abc";
    }
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    EXPECT_EQ(segments.segment(0).candidates_size(), 2);

    EXPECT_EQ(Config::HALF_WIDTH,
              character_form_manager->GetConversionCharacterForm("abc"));

    EXPECT_EQ(segments.segment(0).candidate(0).value, "abc");
    EXPECT_EQ(segments.segment(0).candidate(1).value, "ａｂｃ");
  }
  {
    Segments segments;
    {
      Segment *segment = segments.push_back_segment();
      segment->set_key("~");
      Segment::Candidate *candidate = segment->add_candidate();
      candidate->key = "~";
      candidate->content_key = "~";
      candidate->value = "〜";
      candidate->content_value = "〜";
      candidate->description = "波ダッシュ";
    }
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    EXPECT_EQ(segments.segment(0).candidates_size(), 2);

    EXPECT_EQ(Config::FULL_WIDTH,
              character_form_manager->GetConversionCharacterForm("~"));

    EXPECT_EQ(segments.segment(0).candidate(0).value, "〜");
    EXPECT_EQ(segments.segment(0).candidate(0).description, "[全] 波ダッシュ");
    EXPECT_EQ(segments.segment(0).candidate(1).value, "~");
    EXPECT_EQ(segments.segment(0).candidate(1).description, "[半] チルダ");
  }
}

TEST_F(VariantsRewriterTest, RewriteForPrediction) {
  CharacterFormManager *character_form_manager =
      CharacterFormManager::GetCharacterFormManager();
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  const ConversionRequest request =
      ConversionRequestBuilder()
          .SetRequestType(ConversionRequest::PREDICTION)
          .Build();
  {
    Segments segments;
    InitSegmentsForAlphabetRewrite("abc", &segments);
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    EXPECT_EQ(segments.segment(0).candidates_size(), 2);

    EXPECT_EQ(Config::FULL_WIDTH,
              character_form_manager->GetConversionCharacterForm("abc"));

    EXPECT_EQ(segments.segment(0).candidate(0).value, "ａｂｃ");
    EXPECT_EQ(segments.segment(0).candidate(1).value, "abc");
  }
  {
    character_form_manager->SetCharacterForm("abc", Config::HALF_WIDTH);
    Segments segments;
    InitSegmentsForAlphabetRewrite("abc", &segments);
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    EXPECT_EQ(segments.segment(0).candidates_size(), 2);

    EXPECT_EQ(Config::HALF_WIDTH,
              character_form_manager->GetConversionCharacterForm("abc"));

    EXPECT_EQ(segments.segment(0).candidate(0).value, "abc");
    EXPECT_EQ(segments.segment(0).candidate(1).value, "ａｂｃ");
  }
}

TEST_F(VariantsRewriterTest, RewriteForMixedConversion) {
  CharacterFormManager *character_form_manager =
      CharacterFormManager::GetCharacterFormManager();
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  Request request;
  request.set_mixed_conversion(true);  // Request mixed conversion.
  const ConversionRequest conv_request =
      ConversionRequestBuilder()
          .SetRequest(request)
          .SetRequestType(ConversionRequest::SUGGESTION)
          .Build();
  {
    Segments segments;
    InitSegmentsForAlphabetRewrite("abc", &segments);
    EXPECT_TRUE(rewriter->Rewrite(conv_request, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    EXPECT_EQ(segments.segment(0).candidates_size(), 2);
    EXPECT_EQ(Config::FULL_WIDTH,
              character_form_manager->GetConversionCharacterForm("abc"));
    EXPECT_EQ(segments.segment(0).candidate(0).value, "ａｂｃ");
    EXPECT_EQ(segments.segment(0).candidate(1).value, "abc");
  }
  {
    character_form_manager->SetCharacterForm("abc", Config::HALF_WIDTH);
    Segments segments;
    InitSegmentsForAlphabetRewrite("abc", &segments);
    EXPECT_TRUE(rewriter->Rewrite(conv_request, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    EXPECT_EQ(segments.segment(0).candidates_size(), 2);
    EXPECT_EQ(Config::HALF_WIDTH,
              character_form_manager->GetConversionCharacterForm("abc"));
    EXPECT_EQ(segments.segment(0).candidate(0).value, "abc");
    EXPECT_EQ(segments.segment(0).candidate(1).value, "ａｂｃ");
  }
  {
    // Test for candidate with inner segment boundary.
    // The test case is based on b/116826494.
    character_form_manager->SetCharacterForm("0", Config::HALF_WIDTH);

    Segments segments;

    Segment *segment = segments.push_back_segment();
    segment->set_key("さんえん");

    Segment::Candidate *candidate = segment->add_candidate();
    candidate->key = "さんえん";
    candidate->content_key = candidate->key;
    candidate->value = "３円";  // Full-width three.
    candidate->content_value = candidate->value;
    candidate->inner_segment_boundary = {
        Segment::Candidate::EncodeLengths(6, 3, 6, 3),
        Segment::Candidate::EncodeLengths(6, 3, 6, 3),
    };

    EXPECT_TRUE(rewriter->Rewrite(conv_request, &segments));

    ASSERT_EQ(segments.segments_size(), 1);
    ASSERT_EQ(segments.segment(0).candidates_size(), 2);

    // Since the character form preference is set to Config::HALF_WIDTH, the
    // half-width variant comes first.
    const Segment::Candidate &half = segments.segment(0).candidate(0);
    EXPECT_EQ(half.key, "さんえん");
    EXPECT_EQ(half.value, "3円");
    ASSERT_EQ(half.inner_segment_boundary.size(), 2);
    EXPECT_EQ(half.inner_segment_boundary[0],
              Segment::Candidate::EncodeLengths(6, 1, 6, 1));
    EXPECT_EQ(half.inner_segment_boundary[1],
              Segment::Candidate::EncodeLengths(6, 3, 6, 3));

    const Segment::Candidate &full = segments.segment(0).candidate(1);
    EXPECT_EQ(full.key, "さんえん");
    EXPECT_EQ(full.value, "３円");
    ASSERT_EQ(full.inner_segment_boundary.size(), 2);
    EXPECT_EQ(full.inner_segment_boundary[0],
              Segment::Candidate::EncodeLengths(6, 3, 6, 3));
    EXPECT_EQ(full.inner_segment_boundary[1],
              Segment::Candidate::EncodeLengths(6, 3, 6, 3));
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
  const ConversionRequest conv_request =
      ConversionRequestBuilder()
          .SetRequest(request)
          .SetRequestType(ConversionRequest::SUGGESTION)
          .Build();
  {
    Segments segments;

    Segment *segment = segments.push_back_segment();
    segment->set_key("3えん");

    Segment::Candidate *candidate = segment->add_candidate();
    candidate->key = "3";
    candidate->content_key = candidate->key;
    candidate->value = "３";  // Full-width three.
    candidate->content_value = candidate->value;
    candidate->consumed_key_size = 1;
    candidate->attributes |= Segment::Candidate::PARTIALLY_KEY_CONSUMED;
    candidate->attributes |= Segment::Candidate::AUTO_PARTIAL_SUGGESTION;

    EXPECT_TRUE(rewriter->Rewrite(conv_request, &segments));

    EXPECT_EQ(segments.segments_size(), 1);
    EXPECT_EQ(segments.segment(0).candidates_size(), 2);

    for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
      const Segment::Candidate &cand = segments.segment(0).candidate(i);
      EXPECT_EQ(cand.consumed_key_size, 1);
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
  const ConversionRequest request =
      ConversionRequestBuilder()
          .SetRequestType(ConversionRequest::SUGGESTION)
          .Build();
  {
    Segments segments;
    InitSegmentsForAlphabetRewrite("abc", &segments);
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    EXPECT_EQ(segments.segment(0).candidates_size(), 1);

    EXPECT_EQ(Config::FULL_WIDTH,
              character_form_manager->GetConversionCharacterForm("abc"));

    EXPECT_EQ(segments.segment(0).candidate(0).value, "ａｂｃ");
  }
  {
    character_form_manager->SetCharacterForm("abc", Config::HALF_WIDTH);
    Segments segments;
    InitSegmentsForAlphabetRewrite("abc", &segments);
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    EXPECT_EQ(segments.segment(0).candidates_size(), 1);

    EXPECT_EQ(character_form_manager->GetConversionCharacterForm("abc"),
              Config::HALF_WIDTH);

    EXPECT_EQ(segments.segment(0).candidate(0).value, "abc");
  }
  {
    Segments segments;
    Segment *segment = segments.push_back_segment();
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = "1,000";
    candidate->content_value = "1,000";
    candidate->style =
        NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH;
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    ASSERT_EQ(segments.segments_size(), 1);
    ASSERT_EQ(segments.segment(0).candidates_size(), 1);
    const Segment::Candidate &rewritten_candidate =
        segments.segment(0).candidate(0);
    EXPECT_EQ(rewritten_candidate.value, "１，０００");
    EXPECT_EQ(rewritten_candidate.style,
              NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH);
  }
  {
    // Test for candidate with inner segment boundary.
    Segments segments;

    Segment *segment = segments.push_back_segment();
    segment->set_key("まじ!");

    Segment::Candidate *candidate = segment->add_candidate();
    candidate->key = "まじ!";
    candidate->content_key = candidate->key;
    candidate->value = "マジ!";
    candidate->content_value = candidate->value;
    candidate->inner_segment_boundary.push_back(
        Segment::Candidate::EncodeLengths(6, 6, 6, 6));  // 6 bytes for "まじ"
    candidate->inner_segment_boundary.push_back(
        Segment::Candidate::EncodeLengths(1, 1, 1, 1));  // 1 byte for "!"

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    ASSERT_EQ(segments.segments_size(), 1);
    ASSERT_EQ(segments.segment(0).candidates_size(), 1);

    const Segment::Candidate &rewritten_candidate =
        segments.segment(0).candidate(0);
    EXPECT_EQ(rewritten_candidate.value, "マジ！");  // "マジ！" (full-width)
    EXPECT_EQ(rewritten_candidate.content_value,
              "マジ！");  // "マジ！" (full-width)
    ASSERT_EQ(rewritten_candidate.inner_segment_boundary.size(), 2);

    // Boundary information for
    // key="まじ", value="マジ", ckey="まじ", cvalue="マジ"
    EXPECT_EQ(rewritten_candidate.inner_segment_boundary[0],
              Segment::Candidate::EncodeLengths(6, 6, 6, 6));
    // Boundary information for
    // key="!", value="！", ckey="!", cvalue="！".
    // Values are converted to full-width.
    EXPECT_EQ(rewritten_candidate.inner_segment_boundary[1],
              Segment::Candidate::EncodeLengths(1, 3, 1, 3));
  }
}

TEST_F(VariantsRewriterTest, Capability) {
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  const ConversionRequest request;
  EXPECT_EQ(rewriter->capability(request), RewriterInterface::ALL);
}

TEST_F(VariantsRewriterTest, LearningLevel) {
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  CharacterFormManager *manager =
      CharacterFormManager::GetCharacterFormManager();
  config::Config config;
  config.set_history_learning_level(Config::NO_HISTORY);
  const ConversionRequest request =
      ConversionRequestBuilder().SetConfig(config).Build();

  Segments segments;

  Segment *segment = segments.push_back_segment();
  segment->set_key("いちにさん");
  segment->set_segment_type(Segment::FIXED_VALUE);

  Segment::Candidate *cand = segment->add_candidate();
  cand->key = "いちにさん";
  cand->content_key = cand->key;

  // Half-width number with style.
  cand->value = "123";
  cand->content_value = cand->value;
  cand->style = NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_HALFWIDTH;
  EXPECT_NE(manager->GetConversionCharacterForm("0"), Config::HALF_WIDTH);
  rewriter->Finish(request, &segments);
  EXPECT_NE(manager->GetConversionCharacterForm("0"), Config::HALF_WIDTH);
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
  cand->key = "いちにさん";
  cand->content_key = cand->key;

  // Half-width number with style.
  cand->value = "123";
  cand->content_value = cand->value;
  cand->style = NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_HALFWIDTH;
  rewriter->Finish(request, &segments);
  EXPECT_EQ(manager->GetConversionCharacterForm("0"), Config::HALF_WIDTH);

  // Full-width number with style.
  cand->value = "１２３";
  cand->content_value = cand->value;
  cand->style = NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH;
  rewriter->Finish(request, &segments);
  EXPECT_EQ(manager->GetConversionCharacterForm("0"), Config::FULL_WIDTH);

  // Half-width number expression with description.
  cand->value = "3時";
  cand->content_value = cand->value;
  cand->style = NumberUtil::NumberString::DEFAULT_STYLE;
  cand->description = std::string(VariantsRewriter::kHalfWidth);
  rewriter->Finish(request, &segments);
  EXPECT_EQ(manager->GetConversionCharacterForm("0"), Config::HALF_WIDTH);

  // Full-width number expression with description.
  cand->value = "３時";
  cand->content_value = cand->value;
  cand->style = NumberUtil::NumberString::DEFAULT_STYLE;
  cand->description = std::string(VariantsRewriter::kFullWidth);
  rewriter->Finish(request, &segments);
  EXPECT_EQ(manager->GetConversionCharacterForm("0"), Config::FULL_WIDTH);
}

}  // namespace
}  // namespace mozc
