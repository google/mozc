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
#include <cstring>
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
#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/inner_segment.h"
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
  VariantsRewriterTest()
      : pos_matcher_(mock_data_manager_.GetPosMatcherData()) {}

  void SetUp() override { Reset(); }

  void TearDown() override { Reset(); }

  void Reset() {
    CharacterFormManager::GetCharacterFormManager()->SetDefaultRule();
    CharacterFormManager::GetCharacterFormManager()->ClearHistory();
  }

  static void InitSegmentsForAlphabetRewrite(const absl::string_view value,
                                             Segments* segments) {
    Segment* segment = segments->push_back_segment();
    CHECK(segment);
    segment->set_key(value);
    converter::Candidate* candidate = segment->add_candidate();
    CHECK(candidate);
    candidate->key = std::string(value);
    candidate->content_key = std::string(value);
    candidate->value = std::string(value);
    candidate->content_value = std::string(value);
  }

  VariantsRewriter* CreateVariantsRewriter() const {
    return new VariantsRewriter(pos_matcher_);
  }

 private:
  const testing::MockDataManager mock_data_manager_;

 protected:
  const PosMatcher pos_matcher_;
};

TEST_F(VariantsRewriterTest, RewriteTest) {
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  Segments segments;
  const ConversionRequest request;

  Segment* seg = segments.push_back_segment();

  {
    converter::Candidate* candidate = seg->add_candidate();
    candidate->value = "あいう";
    candidate->content_value = "あいう";
    EXPECT_FALSE(rewriter->Rewrite(request, &segments));
    seg->clear_candidates();
  }

  {
    converter::Candidate* candidate = seg->add_candidate();
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
    converter::Candidate* candidate = seg->add_candidate();
    candidate->value = "012";
    candidate->content_value = "012";
    candidate->attributes |= converter::Attribute::NO_VARIANTS_EXPANSION;
    CharacterFormManager::GetCharacterFormManager()->SetCharacterForm(
        "012", Config::FULL_WIDTH);

    EXPECT_FALSE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 1);
    seg->clear_candidates();
  }

  {
    converter::Candidate* candidate = seg->add_candidate();
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
    converter::Candidate* candidate = seg->add_candidate();
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
    converter::Candidate* candidate = seg->add_candidate();
    candidate->value = "グーグル";
    candidate->content_value = "グーグル";
    CharacterFormManager::GetCharacterFormManager()->SetCharacterForm(
        "アイウ", Config::FULL_WIDTH);

    EXPECT_FALSE(rewriter->Rewrite(request, &segments));
    seg->clear_candidates();
  }

  {
    converter::Candidate* candidate = seg->add_candidate();
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
  Segment* seg = segments.push_back_segment();

  {
    for (int i = 0; i < 10; ++i) {
      converter::Candidate* candidate1 = seg->add_candidate();
      candidate1->value = std::to_string(i);
      candidate1->content_value = std::to_string(i);
      converter::Candidate* candidate2 = seg->add_candidate();
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
      converter::Candidate* candidate1 = seg->add_candidate();
      candidate1->content_key = "ぐーぐる";
      candidate1->key = "ぐーぐる";
      candidate1->value = "ぐーぐる";
      candidate1->content_value = "ぐーぐる";
      converter::Candidate* candidate2 = seg->add_candidate();
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
    converter::Candidate candidate;
    candidate.value = "HalfASCII";
    candidate.content_value = candidate.value;
    candidate.content_key = "halfascii";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "アルファベット"
    EXPECT_EQ(candidate.description, VariantsRewriter::kAlphabet);
  }
  {
    converter::Candidate candidate;
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
    converter::Candidate candidate;
    candidate.value = "コギトエルゴスム";
    candidate.content_value = candidate.value;
    candidate.content_key = "こぎとえるごすむ";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "カタカナ"
    EXPECT_EQ(candidate.description, VariantsRewriter::kKatakana);
  }
  {
    converter::Candidate candidate;
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
    converter::Candidate candidate;
    candidate.value = "123";
    candidate.content_value = candidate.value;
    candidate.content_key = "123";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "数字"
    EXPECT_EQ(candidate.description, VariantsRewriter::kNumber);
  }
  {
    converter::Candidate candidate;
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
    converter::Candidate candidate;
    candidate.value = "Half ASCII";
    candidate.content_value = candidate.value;
    candidate.content_key = "half ascii";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "アルファベット"
    EXPECT_EQ(candidate.description, VariantsRewriter::kAlphabet);
  }
  {
    converter::Candidate candidate;
    candidate.value = "Half!ASCII!";
    candidate.content_value = candidate.value;
    candidate.content_key = "half!ascii!";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "アルファベット"
    EXPECT_EQ(candidate.description, VariantsRewriter::kAlphabet);
  }
  {
    converter::Candidate candidate;
    candidate.value = "CD-ROM";
    candidate.content_value = candidate.value;
    candidate.content_key = "しーでぃーろむ";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "アルファベット"
    EXPECT_EQ(candidate.description, VariantsRewriter::kAlphabet);
  }
  {
    converter::Candidate candidate;
    candidate.value = "コギト・エルゴ・スム";
    candidate.content_value = candidate.value;
    candidate.content_key = "こぎとえるごすむ";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "カタカナ"
    EXPECT_EQ(candidate.description, VariantsRewriter::kKatakana);
  }
  {
    converter::Candidate candidate;
    candidate.value = "!@#";
    candidate.content_value = candidate.value;
    candidate.content_key = "!@#";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    EXPECT_EQ(candidate.description, VariantsRewriter::kHalfWidth);
  }
  {
    converter::Candidate candidate;
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
    converter::Candidate candidate;
    candidate.value = "\\";
    candidate.content_value = candidate.value;
    candidate.content_key = "えん";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    constexpr absl::string_view kExpected = "[半] バックスラッシュ";
    EXPECT_EQ(candidate.description, kExpected);
  }
  {
    converter::Candidate candidate;
    candidate.value = "＼";  // Full-width backslash
    candidate.content_value = candidate.value;
    candidate.content_key = "ばっくすらっしゅ";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    constexpr absl::string_view kExpected = "[全] バックスラッシュ";
    EXPECT_EQ(candidate.description, kExpected);
  }
  {
    converter::Candidate candidate;
    candidate.value = "¥";  // Half-width yen-symbol
    candidate.content_value = candidate.value;
    candidate.content_key = "えん";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    std::string expected = "[半] 円記号";
    EXPECT_EQ(candidate.description, expected);
  }
  {
    converter::Candidate candidate;
    candidate.value = "￥";  // Full-width yen-symbol
    candidate.content_value = candidate.value;
    candidate.content_key = "えん";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    constexpr absl::string_view kExpected = "[全] 円記号";
    EXPECT_EQ(candidate.description, kExpected);
  }
  {
    converter::Candidate candidate;
    candidate.value = "~";  // Tilde
    candidate.content_value = candidate.value;
    candidate.content_key = "~";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    std::string expected = "[半] チルダ";
    EXPECT_EQ(candidate.description, expected);
  }
  {
    converter::Candidate candidate;
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
    converter::Candidate candidate;
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
    converter::Candidate candidate;
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
    converter::Candidate candidate;
    candidate.value = "HalfASCII";
    candidate.content_value = candidate.value;
    candidate.content_key = "halfascii";
    VariantsRewriter::SetDescriptionForTransliteration(pos_matcher_,
                                                       &candidate);
    // "アルファベット"
    EXPECT_EQ(candidate.description, VariantsRewriter::kAlphabet);
  }
  {
    converter::Candidate candidate;
    candidate.value = "!@#";
    candidate.content_value = candidate.value;
    candidate.content_key = "!@#";
    VariantsRewriter::SetDescriptionForTransliteration(pos_matcher_,
                                                       &candidate);
    // "[半]"
    EXPECT_EQ(candidate.description, VariantsRewriter::kHalfWidth);
  }
  {
    converter::Candidate candidate;
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
    converter::Candidate candidate;
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
    converter::Candidate candidate;
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
    converter::Candidate candidate;
    candidate.value = "HalfASCII";
    candidate.content_value = candidate.value;
    candidate.content_key = "halfascii";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ(candidate.description, "");
  }
  // containing symbols
  {
    converter::Candidate candidate;
    candidate.value = "Half ASCII";
    candidate.content_value = candidate.value;
    candidate.content_key = "half ascii";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ(candidate.description, "");
  }
  {
    converter::Candidate candidate;
    candidate.value = "Half!ASCII!";
    candidate.content_value = candidate.value;
    candidate.content_key = "half!ascii!";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ(candidate.description, "");
  }
  {
    converter::Candidate candidate;
    candidate.value = "CD-ROM";
    candidate.content_value = candidate.value;
    candidate.content_key = "しーでぃーろむ";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ(candidate.description, "");
  }
  {
    converter::Candidate candidate;
    candidate.value = "!@#";
    candidate.content_value = candidate.value;
    candidate.content_key = "!@#";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ(candidate.description, "");
  }
  {
    converter::Candidate candidate;
    candidate.value = "「ＡＢＣ」";
    candidate.content_value = candidate.value;
    candidate.content_key = "[ABC]";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ(candidate.description, "");
  }
  {
    converter::Candidate candidate;
    // A symbol representing "パーセント".
    candidate.value = "㌫";
    candidate.content_value = candidate.value;
    candidate.content_key = "ぱーせんと";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    std::string expected = "";
    EXPECT_EQ(candidate.description, expected);
  }
  {
    converter::Candidate candidate;
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
  CharacterFormManager* character_form_manager =
      CharacterFormManager::GetCharacterFormManager();
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  ConversionRequest request;
  {
    Segments segments;
    {
      Segment* segment = segments.push_back_segment();
      segment->set_key("abc");
      converter::Candidate* candidate = segment->add_candidate();
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
      Segment* segment = segments.push_back_segment();
      segment->set_key("abc");
      converter::Candidate* candidate = segment->add_candidate();
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
      Segment* segment = segments.push_back_segment();
      segment->set_key("~");
      converter::Candidate* candidate = segment->add_candidate();
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
  CharacterFormManager* character_form_manager =
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
  CharacterFormManager* character_form_manager =
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

    Segment* segment = segments.push_back_segment();
    segment->set_key("さんえん");

    converter::Candidate* candidate = segment->add_candidate();
    candidate->key = "さんえん";
    candidate->content_key = candidate->key;
    candidate->value = "３円";  // Full-width three.
    candidate->content_value = candidate->value;
    candidate->inner_segment_boundary = converter::BuildInnerSegmentBoundary(
        {{6, 3, 6, 3}, {6, 3, 6, 3}}, candidate->key, candidate->value);

    EXPECT_TRUE(rewriter->Rewrite(conv_request, &segments));

    ASSERT_EQ(segments.segments_size(), 1);
    ASSERT_EQ(segments.segment(0).candidates_size(), 2);

    // Since the character form preference is set to Config::HALF_WIDTH, the
    // half-width variant comes first.
    const converter::Candidate& half = segments.segment(0).candidate(0);
    EXPECT_EQ(half.key, "さんえん");
    EXPECT_EQ(half.value, "3円");
    ASSERT_EQ(half.inner_segment_boundary.size(), 2);
    EXPECT_EQ(half.inner_segment_boundary,
              converter::BuildInnerSegmentBoundary({{6, 1, 6, 1}, {6, 3, 6, 3}},
                                                   half.key, half.value));

    const converter::Candidate& full = segments.segment(0).candidate(1);
    EXPECT_EQ(full.key, "さんえん");
    EXPECT_EQ(full.value, "３円");
    ASSERT_EQ(full.inner_segment_boundary.size(), 2);
    EXPECT_EQ(full.inner_segment_boundary,
              converter::BuildInnerSegmentBoundary({{6, 3, 6, 3}, {6, 3, 6, 3}},
                                                   full.key, full.value));
  }
}

TEST_F(VariantsRewriterTest, RewriteForPartialSuggestion) {
  CharacterFormManager* character_form_manager =
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

    Segment* segment = segments.push_back_segment();
    segment->set_key("3えん");

    converter::Candidate* candidate = segment->add_candidate();
    candidate->key = "3";
    candidate->content_key = candidate->key;
    candidate->value = "３";  // Full-width three.
    candidate->content_value = candidate->value;
    candidate->consumed_key_size = 1;
    candidate->attributes |= converter::Attribute::PARTIALLY_KEY_CONSUMED;
    candidate->attributes |= converter::Attribute::AUTO_PARTIAL_SUGGESTION;

    EXPECT_TRUE(rewriter->Rewrite(conv_request, &segments));

    EXPECT_EQ(segments.segments_size(), 1);
    EXPECT_EQ(segments.segment(0).candidates_size(), 2);

    for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
      const converter::Candidate& cand = segments.segment(0).candidate(i);
      EXPECT_EQ(cand.consumed_key_size, 1);
      EXPECT_TRUE(cand.attributes &
                  converter::Attribute::PARTIALLY_KEY_CONSUMED);
      EXPECT_TRUE(cand.attributes &
                  converter::Attribute::AUTO_PARTIAL_SUGGESTION);
    }
  }
}

TEST_F(VariantsRewriterTest, RewriteForSuggestion) {
  CharacterFormManager* character_form_manager =
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
    Segment* segment = segments.push_back_segment();
    converter::Candidate* candidate = segment->add_candidate();
    candidate->value = "1,000";
    candidate->content_value = "1,000";
    candidate->style =
        NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH;
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    ASSERT_EQ(segments.segments_size(), 1);
    ASSERT_EQ(segments.segment(0).candidates_size(), 1);
    const converter::Candidate& rewritten_candidate =
        segments.segment(0).candidate(0);
    EXPECT_EQ(rewritten_candidate.value, "１，０００");
    EXPECT_EQ(rewritten_candidate.style,
              NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH);
  }
  {
    // Test for candidate with inner segment boundary.
    Segments segments;

    Segment* segment = segments.push_back_segment();
    segment->set_key("まじ!");

    converter::Candidate* candidate = segment->add_candidate();
    candidate->key = "まじ!";
    candidate->content_key = candidate->key;
    candidate->value = "マジ!";
    candidate->content_value = candidate->value;
    candidate->inner_segment_boundary = converter::BuildInnerSegmentBoundary(
        {
            {6, 6, 6, 6},  // 6 bytes for "まじ"
            {1, 1, 1, 1}   // 1 byte for "!"
        },
        candidate->key, candidate->value);

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    ASSERT_EQ(segments.segments_size(), 1);
    ASSERT_EQ(segments.segment(0).candidates_size(), 1);

    const converter::Candidate& rewritten_candidate =
        segments.segment(0).candidate(0);
    EXPECT_EQ(rewritten_candidate.value, "マジ！");  // "マジ！" (full-width)
    EXPECT_EQ(rewritten_candidate.content_value,
              "マジ！");  // "マジ！" (full-width)
    EXPECT_EQ(rewritten_candidate.inner_segment_boundary,
              converter::BuildInnerSegmentBoundary(
                  {
                      // key="まじ", value="マジ", ckey="まじ", cvalue="マジ"
                      {6, 6, 6, 6},
                      // key="!", value="！", ckey="!", cvalue="！".
                      // Values are converted to full-width.
                      {1, 3, 1, 3}  // 1 byte for "!"
                  },
                  rewritten_candidate.key, rewritten_candidate.value));
  }
}

TEST_F(VariantsRewriterTest, Capability) {
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  const ConversionRequest request;
  EXPECT_EQ(rewriter->capability(request), RewriterInterface::ALL);
}

TEST_F(VariantsRewriterTest, LearningLevel) {
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  CharacterFormManager* manager =
      CharacterFormManager::GetCharacterFormManager();
  config::Config config;
  config.set_history_learning_level(Config::NO_HISTORY);
  const ConversionRequest request =
      ConversionRequestBuilder().SetConfig(config).Build();

  Segments segments;

  Segment* segment = segments.push_back_segment();
  segment->set_key("いちにさん");
  segment->set_segment_type(Segment::FIXED_VALUE);

  converter::Candidate* cand = segment->add_candidate();
  cand->key = "いちにさん";
  cand->content_key = cand->key;

  // Half-width number with style.
  cand->value = "123";
  cand->content_value = cand->value;
  cand->style = NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_HALFWIDTH;
  EXPECT_NE(manager->GetConversionCharacterForm("0"), Config::HALF_WIDTH);
  rewriter->Finish(request, segments);
  EXPECT_NE(manager->GetConversionCharacterForm("0"), Config::HALF_WIDTH);
}

TEST_F(VariantsRewriterTest, Finish) {
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  auto* manager = CharacterFormManager::GetCharacterFormManager();
  const ConversionRequest request;

  Segments segments;

  Segment* segment = segments.push_back_segment();
  segment->set_key("いちにさん");
  segment->set_segment_type(Segment::FIXED_VALUE);

  converter::Candidate* cand = segment->add_candidate();
  cand->key = "いちにさん";
  cand->content_key = cand->key;

  // Half-width number with style.
  cand->value = "123";
  cand->content_value = cand->value;
  cand->style = NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_HALFWIDTH;
  rewriter->Finish(request, segments);
  EXPECT_EQ(manager->GetConversionCharacterForm("0"), Config::HALF_WIDTH);

  // Full-width number with style.
  cand->value = "１２３";
  cand->content_value = cand->value;
  cand->style = NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH;
  rewriter->Finish(request, segments);
  EXPECT_EQ(manager->GetConversionCharacterForm("0"), Config::FULL_WIDTH);

  // Half-width number expression with description.
  cand->value = "3時";
  cand->content_value = cand->value;
  cand->style = NumberUtil::NumberString::DEFAULT_STYLE;
  cand->description = std::string(VariantsRewriter::kHalfWidth);
  rewriter->Finish(request, segments);
  EXPECT_EQ(manager->GetConversionCharacterForm("0"), Config::HALF_WIDTH);

  // Full-width number expression with description.
  cand->value = "３時";
  cand->content_value = cand->value;
  cand->style = NumberUtil::NumberString::DEFAULT_STYLE;
  cand->description = std::string(VariantsRewriter::kFullWidth);
  rewriter->Finish(request, segments);
  EXPECT_EQ(manager->GetConversionCharacterForm("0"), Config::FULL_WIDTH);
}

TEST_F(VariantsRewriterTest, RewriteTopCandidateForMixedConversionTest) {
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  CharacterFormManager* manager =
      CharacterFormManager::GetCharacterFormManager();

  commands::Request client_request;
  client_request.set_mixed_conversion(true);
  client_request.mutable_decoder_experiment_params()
      ->set_suppress_realtime_conversion_with_converter(true);
  const ConversionRequest request =
      ConversionRequestBuilder().SetRequest(client_request).Build();

  Segments segments;
  Segment* seg = segments.push_back_segment();

  // Case 1: Top candidate has REALTIME_CONVERSION and NO_VARIANTS_EXPANSION.
  // With CharacterFormManager set to FULL_WIDTH, primary candidate "１２３" is
  // inserted at index 0 and secondary candidate "123" is preserved at index 1.
  {
    manager->SetCharacterForm("0", Config::FULL_WIDTH);

    converter::Candidate* top = seg->add_candidate();
    top->value = "123";
    top->content_value = "123";
    top->attributes |= (converter::Attribute::REALTIME_CONVERSION |
                        converter::Attribute::NO_VARIANTS_EXPANSION);

    converter::Candidate* second = seg->add_candidate();
    second->value = "456";
    second->content_value = "456";
    second->attributes |= converter::Attribute::NO_VARIANTS_EXPANSION;

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 3);
    EXPECT_EQ(seg->candidate(0).value, "１２３");
    EXPECT_EQ(seg->candidate(0).content_value, "１２３");
    EXPECT_EQ(seg->candidate(0).description, "");
    EXPECT_TRUE(seg->candidate(0).attributes &
                converter::Attribute::REALTIME_CONVERSION);
    EXPECT_TRUE(seg->candidate(0).attributes &
                converter::Attribute::NO_EXTRA_DESCRIPTION);
    EXPECT_EQ(seg->candidate(1).value, "123");
    EXPECT_EQ(seg->candidate(1).content_value, "123");
    EXPECT_EQ(seg->candidate(2).value, "456");

    seg->clear_candidates();
  }

  // Case 2: Already matches primary form -> no rewrite.
  {
    manager->SetCharacterForm("0", Config::HALF_WIDTH);

    converter::Candidate* top = seg->add_candidate();
    top->value = "123";
    top->content_value = "123";
    top->attributes |= (converter::Attribute::REALTIME_CONVERSION |
                        converter::Attribute::NO_VARIANTS_EXPANSION);

    EXPECT_FALSE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 1);
    EXPECT_EQ(seg->candidate(0).value, "123");

    seg->clear_candidates();
  }

  // Case 3: Flag is OFF -> no rewrite even if form differs.
  {
    manager->SetCharacterForm("0", Config::FULL_WIDTH);

    commands::Request request_flag_off;
    request_flag_off.set_mixed_conversion(true);
    request_flag_off.mutable_decoder_experiment_params()
        ->set_suppress_realtime_conversion_with_converter(false);
    const ConversionRequest req_off =
        ConversionRequestBuilder().SetRequest(request_flag_off).Build();

    converter::Candidate* top = seg->add_candidate();
    top->value = "123";
    top->content_value = "123";
    top->attributes |= (converter::Attribute::REALTIME_CONVERSION |
                        converter::Attribute::NO_VARIANTS_EXPANSION);

    EXPECT_FALSE(rewriter->Rewrite(req_off, &segments));
    EXPECT_EQ(seg->candidates_size(), 1);
    EXPECT_EQ(seg->candidate(0).value, "123");

    seg->clear_candidates();
  }

  // Case 4: Compound with inner segment boundary (e.g. "あと2つ").
  {
    manager->SetCharacterForm("0", Config::FULL_WIDTH);

    converter::Candidate* top = seg->add_candidate();
    top->key = "あとふたつ";
    top->value = "あと2つ";
    top->content_key = top->key;
    top->content_value = top->value;
    top->attributes |= (converter::Attribute::REALTIME_CONVERSION |
                        converter::Attribute::NO_VARIANTS_EXPANSION);
    // Inner segments: "あと" (6B), "2" (1B), "つ" (3B)
    converter::InnerSegmentBoundaryBuilder builder;
    builder.Add(6, 6, 6, 6);
    builder.Add(6, 1, 6, 1);
    builder.Add(3, 3, 3, 3);
    top->inner_segment_boundary = builder.Build(top->key, top->value);

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 2);
    EXPECT_EQ(seg->candidate(0).value, "あと２つ");
    EXPECT_EQ(seg->candidate(1).value, "あと2つ");
    std::vector<std::string> inner_values;
    for (const auto& inner : seg->candidate(0).inner_segments()) {
      inner_values.emplace_back(inner.GetValue());
    }
    EXPECT_THAT(inner_values, ::testing::ElementsAre("あと", "２", "つ"));

    seg->clear_candidates();
  }

  // Case 5: Desktop suggestion (mixed_conversion = false).
  {
    manager->SetCharacterForm("0", Config::FULL_WIDTH);

    commands::Request desktop_request;
    desktop_request.set_mixed_conversion(false);
    desktop_request.mutable_decoder_experiment_params()
        ->set_suppress_realtime_conversion_with_converter(true);
    const ConversionRequest req =
        ConversionRequestBuilder()
            .SetRequest(desktop_request)
            .SetRequestType(ConversionRequest::SUGGESTION)
            .Build();

    converter::Candidate* top = seg->add_candidate();
    top->value = "123";
    top->content_value = "123";
    top->attributes |= (converter::Attribute::REALTIME_CONVERSION |
                        converter::Attribute::NO_VARIANTS_EXPANSION);

    EXPECT_TRUE(rewriter->Rewrite(req, &segments));
    EXPECT_EQ(seg->candidates_size(), 2);
    EXPECT_EQ(seg->candidate(0).value, "１２３");
    EXPECT_EQ(seg->candidate(1).value, "123");

    seg->clear_candidates();
  }

  // Case 6: Empty segment (candidates_size() == 0) -> returns false.
  {
    EXPECT_FALSE(rewriter->Rewrite(request, &segments));
  }

  // Case 7: Candidate without REALTIME_CONVERSION attribute -> not rewritten.
  {
    manager->SetCharacterForm("0", Config::FULL_WIDTH);

    converter::Candidate* top = seg->add_candidate();
    top->value = "123";
    top->content_value = "123";
    top->attributes |= converter::Attribute::NO_VARIANTS_EXPANSION;

    EXPECT_FALSE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(seg->candidate(0).value, "123");

    seg->clear_candidates();
  }

  // Case 8: Candidate with no character form alternatives (e.g. hiragana) ->
  // GenerateAlternatives returns false.
  {
    converter::Candidate* top = seg->add_candidate();
    top->key = "あいう";
    top->value = "あいう";
    top->content_key = "あいう";
    top->content_value = "あいう";
    top->attributes |= (converter::Attribute::REALTIME_CONVERSION |
                        converter::Attribute::NO_VARIANTS_EXPANSION);

    EXPECT_FALSE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(seg->candidate(0).value, "あいう");

    seg->clear_candidates();
  }

  // Case 9: PARTIAL_SUGGESTION and PARTIAL_PREDICTION are excluded from
  // RewriteTopCandidateForSuggestion.
  {
    manager->SetCharacterForm("0", Config::FULL_WIDTH);

    for (const ConversionRequest::RequestType req_type :
         {ConversionRequest::PARTIAL_SUGGESTION,
          ConversionRequest::PARTIAL_PREDICTION}) {
      const ConversionRequest partial_req = ConversionRequestBuilder()
                                                .SetRequest(client_request)
                                                .SetRequestType(req_type)
                                                .Build();

      converter::Candidate* top = seg->add_candidate();
      top->value = "123";
      top->content_value = "123";
      top->attributes |= (converter::Attribute::REALTIME_CONVERSION |
                          converter::Attribute::NO_VARIANTS_EXPANSION);

      EXPECT_FALSE(rewriter->Rewrite(partial_req, &segments));
      EXPECT_EQ(seg->candidate(0).value, "123");

      seg->clear_candidates();
    }
  }

  // Case 10: Candidate at index 0 already rewritten by NumberRewriter (marked
  // with NO_EXTRA_DESCRIPTION).
  // - If index 0 has no character form variants ("石の上にも三年"), no third
  //   candidate ("石の上にも３年") is inserted, keeping 2 candidates.
  // - If index 0 has alphabet variants ("wikipediaを三年使う"), index 0 is
  //   updated in-place to "ｗｉｋｉｐｅｄｉａを三年使う", keeping 2 candidates.
  {
    manager->SetCharacterForm("0", Config::FULL_WIDTH);
    manager->SetCharacterForm("A", Config::FULL_WIDTH);

    converter::Candidate* kanji_cand = seg->add_candidate();
    kanji_cand->key = "いしのうえにもさんねん";
    kanji_cand->value = "石の上にも三年";
    kanji_cand->content_key = kanji_cand->key;
    kanji_cand->content_value = kanji_cand->value;
    kanji_cand->attributes |= (converter::Attribute::REALTIME_CONVERSION |
                               converter::Attribute::NO_VARIANTS_EXPANSION |
                               converter::Attribute::NO_EXTRA_DESCRIPTION);

    converter::Candidate* arabic_cand = seg->add_candidate();
    arabic_cand->key = "いしのうえにもさんねん";
    arabic_cand->value = "石の上にも3年";
    arabic_cand->content_key = arabic_cand->key;
    arabic_cand->content_value = arabic_cand->value;
    arabic_cand->attributes |= (converter::Attribute::REALTIME_CONVERSION |
                                converter::Attribute::NO_VARIANTS_EXPANSION);

    EXPECT_FALSE(rewriter->Rewrite(request, &segments));
    ASSERT_EQ(seg->candidates_size(), 2);
    EXPECT_EQ(seg->candidate(0).value, "石の上にも三年");
    EXPECT_EQ(seg->candidate(1).value, "石の上にも3年");

    seg->clear_candidates();

    // Mixed alphabet + Kanji number ("wikipediaを三年使う" at [0],
    // "wikipediaを3年使う" at [1]):
    converter::Candidate* mixed_top = seg->add_candidate();
    mixed_top->key = "wikipediaを3ねんつかう";
    mixed_top->value = "wikipediaを三年使う";
    mixed_top->content_key = mixed_top->key;
    mixed_top->content_value = mixed_top->value;
    mixed_top->attributes |= (converter::Attribute::REALTIME_CONVERSION |
                              converter::Attribute::NO_VARIANTS_EXPANSION |
                              converter::Attribute::NO_EXTRA_DESCRIPTION);
    converter::InnerSegmentBoundaryBuilder builder;
    builder.Add(strlen("wikipediaを"), strlen("wikipediaを"),
                strlen("wikipedia"), strlen("wikipedia"));
    builder.Add(strlen("3ねん"), strlen("三年"), strlen("3ねん"),
                strlen("三年"));
    builder.Add(strlen("つかう"), strlen("使う"), strlen("つかう"),
                strlen("使う"));
    mixed_top->inner_segment_boundary =
        builder.Build(mixed_top->key, mixed_top->value);

    converter::Candidate* mixed_fallback = seg->add_candidate();
    mixed_fallback->key = "wikipediaを3ねんつかう";
    mixed_fallback->value = "wikipediaを3年使う";
    mixed_fallback->content_key = mixed_fallback->key;
    mixed_fallback->content_value = mixed_fallback->value;
    mixed_fallback->attributes |= (converter::Attribute::REALTIME_CONVERSION |
                                   converter::Attribute::NO_VARIANTS_EXPANSION);

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    ASSERT_EQ(seg->candidates_size(), 2);
    EXPECT_EQ(seg->candidate(0).value, "ｗｉｋｉｐｅｄｉａを三年使う");
    EXPECT_EQ(seg->candidate(1).value, "wikipediaを3年使う");

    seg->clear_candidates();
  }
}

}  // namespace
}  // namespace mozc
