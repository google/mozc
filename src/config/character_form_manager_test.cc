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

#include "config/character_form_manager.h"

#include <optional>
#include <string>

#include "base/number_util.h"
#include "prediction/user_history_predictor.pb.h"
#include "protocol/config.pb.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace config {
namespace {

class CharacterFormManagerTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    CharacterFormManager* manager =
        CharacterFormManager::GetCharacterFormManager();
    manager->SetDefaultRule();
  }

  void TearDown() override {
    CharacterFormManager* manager =
        CharacterFormManager::GetCharacterFormManager();
    manager->SetDefaultRule();
  }
};

TEST_F(CharacterFormManagerTest, DefaultTest) {
  CharacterFormManager* manager =
      CharacterFormManager::GetCharacterFormManager();

  manager->ClearHistory();

  EXPECT_EQ(manager->GetPreeditCharacterForm("カタカナ"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetPreeditCharacterForm("012"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetPreeditCharacterForm("["), config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetPreeditCharacterForm("/"), config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetPreeditCharacterForm("・"), config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetPreeditCharacterForm("。"), config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetPreeditCharacterForm("、"), config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetPreeditCharacterForm("\\"), config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("ABC012ほげ"),
            config::Config::NO_CONVERSION);

  EXPECT_EQ(manager->GetConversionCharacterForm("カタカナ"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("012"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("["),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("/"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("・"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("。"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("、"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("\\"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("ABC012ほげ"),
            config::Config::NO_CONVERSION);

  std::string output;
  manager->ConvertPreeditString("京都東京ABCインターネット", &output);
  EXPECT_EQ(output, "京都東京ＡＢＣインターネット");

  manager->ConvertPreeditString("ｲﾝﾀｰﾈｯﾄ", &output);
  EXPECT_EQ(output, "インターネット");

  manager->ConvertPreeditString("[]・。、", &output);
  EXPECT_EQ(output, "［］・。、");

  manager->ConvertPreeditString(".!@#$%^&", &output);
  EXPECT_EQ(output, "．！＠＃＄％＾＆");

  manager->ConvertPreeditString("京都東京ABCｲﾝﾀｰﾈｯﾄ012", &output);
  EXPECT_EQ(output, "京都東京ＡＢＣインターネット０１２");

  manager->ConvertPreeditString("グーグルABCｲﾝﾀｰﾈｯﾄ012あいう", &output);
  EXPECT_EQ(output, "グーグルＡＢＣインターネット０１２あいう");

  manager->ConvertPreeditString("京都東京ABCインターネット", &output);
  EXPECT_EQ(output, "京都東京ＡＢＣインターネット");

  manager->ConvertPreeditString("[京都]{東京}ABC!インターネット", &output);
  EXPECT_EQ(output, "［京都］｛東京｝ＡＢＣ！インターネット");

  manager->ConvertConversionString("ｲﾝﾀｰﾈｯﾄ", &output);
  EXPECT_EQ(output, "インターネット");

  manager->ConvertConversionString("[]・。、", &output);
  EXPECT_EQ(output, "［］・。、");

  manager->ConvertConversionString(".!@#$%^&", &output);
  EXPECT_EQ(output, "．！＠＃＄％＾＆");

  manager->ConvertConversionString("京都東京ABCｲﾝﾀｰﾈｯﾄ012", &output);
  EXPECT_EQ(output, "京都東京ＡＢＣインターネット０１２");

  manager->ConvertConversionString("グーグルABCｲﾝﾀｰﾈｯﾄ012あいう", &output);
  EXPECT_EQ(output, "グーグルＡＢＣインターネット０１２あいう");

  manager->ConvertConversionString("[京都]{東京}ABC!インターネット", &output);
  EXPECT_EQ(output, "［京都］｛東京｝ＡＢＣ！インターネット");

  // Set
  manager->SetCharacterForm("カタカナ", config::Config::HALF_WIDTH);
  manager->SetCharacterForm("012", config::Config::HALF_WIDTH);
  manager->SetCharacterForm("[", config::Config::HALF_WIDTH);
  manager->SetCharacterForm("/", config::Config::HALF_WIDTH);
  manager->SetCharacterForm("・", config::Config::HALF_WIDTH);
  manager->SetCharacterForm("。", config::Config::HALF_WIDTH);
  manager->SetCharacterForm("、", config::Config::HALF_WIDTH);
  manager->SetCharacterForm("\\", config::Config::HALF_WIDTH);

  // retry
  EXPECT_EQ(manager->GetPreeditCharacterForm("カタカナ"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetPreeditCharacterForm("012"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetPreeditCharacterForm("["), config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetPreeditCharacterForm("/"), config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetPreeditCharacterForm("・"), config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetPreeditCharacterForm("。"), config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetPreeditCharacterForm("、"), config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetPreeditCharacterForm("\\"), config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("ABC012ほげ"),
            config::Config::NO_CONVERSION);

  EXPECT_EQ(manager->GetConversionCharacterForm("カタカナ"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("012"),
            config::Config::HALF_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("["),
            config::Config::HALF_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("/"),
            config::Config::HALF_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("・"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("。"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("、"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("\\"),
            config::Config::HALF_WIDTH);

  manager->ConvertPreeditString("京都東京ABCインターネット", &output);
  EXPECT_EQ(output, "京都東京ＡＢＣインターネット");

  manager->ConvertPreeditString("ｲﾝﾀｰﾈｯﾄ", &output);
  EXPECT_EQ(output, "インターネット");

  manager->ConvertPreeditString("[]・。、", &output);
  EXPECT_EQ(output, "［］・。、");

  manager->ConvertPreeditString(".!@#$%^&", &output);
  EXPECT_EQ(output, "．！＠＃＄％＾＆");

  manager->ConvertPreeditString("京都東京ABCｲﾝﾀｰﾈｯﾄ", &output);
  EXPECT_EQ(output, "京都東京ＡＢＣインターネット");

  manager->ConvertPreeditString("グーグルABCｲﾝﾀｰﾈｯﾄあいう", &output);
  EXPECT_EQ(output, "グーグルＡＢＣインターネットあいう");

  manager->ConvertPreeditString("京都東京ABCインターネット", &output);
  EXPECT_EQ(output, "京都東京ＡＢＣインターネット");

  manager->ConvertPreeditString("[京都]{東京}ABC!インターネット", &output);
  EXPECT_EQ(output, "［京都］｛東京｝ＡＢＣ！インターネット");

  manager->ConvertConversionString("ｲﾝﾀｰﾈｯﾄ", &output);
  EXPECT_EQ(output, "インターネット");

  manager->ConvertConversionString("[]・。、", &output);
  EXPECT_EQ(output, "[]・。、");

  // require_consistent_conversion_ suppresses this conversion because
  // ".!@#$%^&" would become "．！@#$%^&" by preference, which mixes
  // full-width ("．！") and half-width ("@#$%^&") forms within a contiguous
  // run of variable-width symbols.
  manager->ConvertConversionString(".!@#$%^&", &output);
  EXPECT_EQ(output, ".!@#$%^&");

  // Converting chunks separately confirms that each chunk matches its
  // individual preference.
  manager->ConvertConversionString(".!", &output);
  EXPECT_EQ(output, "．！");
  manager->ConvertConversionString("@#$%^&", &output);
  EXPECT_EQ(output, "@#$%^&");

  manager->ConvertConversionString("京都東京ABCｲﾝﾀｰﾈｯﾄ", &output);
  EXPECT_EQ(output, "京都東京ＡＢＣインターネット");

  manager->ConvertConversionString("グーグルABCｲﾝﾀｰﾈｯﾄあいう", &output);
  EXPECT_EQ(output, "グーグルＡＢＣインターネットあいう");

  // "[京都]{東京}ABC!インターネット" would be
  // "[京都]{東京}ＡＢＣ！インターネット" by preference, but "}ABC!" is not
  // consistent within that contiguous variable-width run, so it is kept as-is.
  manager->ConvertConversionString("[京都]{東京}ABC!インターネット", &output);
  EXPECT_EQ(output, "[京都]{東京}ABC!インターネット");

  manager->ConvertConversionString("[京都]{東京}", &output);
  EXPECT_EQ(output, "[京都]{東京}");

  manager->ConvertConversionString("ABC!インターネット", &output);
  EXPECT_EQ(output, "ＡＢＣ！インターネット");

  // reset
  manager->SetCharacterForm("カタカナ", config::Config::FULL_WIDTH);
  manager->SetCharacterForm("012", config::Config::FULL_WIDTH);
  manager->SetCharacterForm("[", config::Config::FULL_WIDTH);
  manager->SetCharacterForm("/", config::Config::FULL_WIDTH);
  manager->SetCharacterForm("・", config::Config::FULL_WIDTH);
  manager->SetCharacterForm("。", config::Config::FULL_WIDTH);
  manager->SetCharacterForm("、", config::Config::FULL_WIDTH);
  manager->SetCharacterForm("\\", config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetPreeditCharacterForm("カタカナ"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetPreeditCharacterForm("012"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetPreeditCharacterForm("["), config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetPreeditCharacterForm("/"), config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetPreeditCharacterForm("・"), config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetPreeditCharacterForm("。"), config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetPreeditCharacterForm("、"), config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetPreeditCharacterForm("\\"), config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("ABC012ほげ"),
            config::Config::NO_CONVERSION);

  EXPECT_EQ(manager->GetConversionCharacterForm("カタカナ"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("012"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("["),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("/"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("・"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("。"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("、"),
            config::Config::FULL_WIDTH);

  EXPECT_EQ(manager->GetConversionCharacterForm("ABC012ほげ"),
            config::Config::NO_CONVERSION);

  manager->ConvertPreeditString("京都東京ABCインターネット", &output);
  EXPECT_EQ(output, "京都東京ＡＢＣインターネット");

  manager->ConvertPreeditString("ｲﾝﾀｰﾈｯﾄ", &output);
  EXPECT_EQ(output, "インターネット");

  manager->ConvertPreeditString("[]・。、", &output);
  EXPECT_EQ(output, "［］・。、");

  manager->ConvertPreeditString(".!@#$%^&", &output);
  EXPECT_EQ(output, "．！＠＃＄％＾＆");

  manager->ConvertPreeditString("京都東京ABCｲﾝﾀｰﾈｯﾄ012", &output);
  EXPECT_EQ(output, "京都東京ＡＢＣインターネット０１２");

  manager->ConvertPreeditString("グーグルABCｲﾝﾀｰﾈｯﾄ012あいう", &output);
  EXPECT_EQ(output, "グーグルＡＢＣインターネット０１２あいう");

  manager->ConvertPreeditString("京都東京ABCインターネット", &output);
  EXPECT_EQ(output, "京都東京ＡＢＣインターネット");

  manager->ConvertPreeditString("[京都]{東京}ABC!インターネット", &output);
  EXPECT_EQ(output, "［京都］｛東京｝ＡＢＣ！インターネット");

  manager->ConvertConversionString("ｲﾝﾀｰﾈｯﾄ", &output);
  EXPECT_EQ(output, "インターネット");

  manager->ConvertConversionString("[]・。、", &output);
  EXPECT_EQ(output, "［］・。、");

  manager->ConvertConversionString(".!@#$%^&", &output);
  EXPECT_EQ(output, "．！＠＃＄％＾＆");

  manager->ConvertConversionString("京都東京ABCｲﾝﾀｰﾈｯﾄ012", &output);
  EXPECT_EQ(output, "京都東京ＡＢＣインターネット０１２");

  manager->ConvertConversionString("グーグルABCｲﾝﾀｰﾈｯﾄ012あいう", &output);
  EXPECT_EQ(output, "グーグルＡＢＣインターネット０１２あいう");

  manager->ConvertConversionString("[京都]{東京}ABC!インターネット", &output);
  EXPECT_EQ(output, "［京都］｛東京｝ＡＢＣ！インターネット");
}

TEST_F(CharacterFormManagerTest, MixedFormTest) {
  CharacterFormManager* manager =
      CharacterFormManager::GetCharacterFormManager();

  manager->AddConversionRule("0", config::Config::FULL_WIDTH);
  manager->AddConversionRule(".,", config::Config::HALF_WIDTH);
  manager->AddPreeditRule("0", config::Config::FULL_WIDTH);
  manager->AddPreeditRule(".,", config::Config::HALF_WIDTH);

  std::string output;
  // A period surrounded by numbers is treated as part of the number run ("0"
  // rule).
  manager->ConvertConversionString("1.23", &output);
  EXPECT_EQ(output, "１．２３");

  manager->ConvertPreeditString("1.23", &output);
  EXPECT_EQ(output, "１．２３");

  // When not surrounded by numbers, the period follows the ".," rule.
  // In conversion, require_consistent_conversion_ prevents mixed-width
  // conversion of "ABC.DEF".
  manager->ConvertConversionString("ABC.DEF", &output);
  EXPECT_EQ(output, "ABC.DEF");

  // In preedit, where require_consistent_conversion_ is false, chunks are
  // converted according to their respective preferences.
  manager->ConvertPreeditString("ABC.DEF", &output);
  EXPECT_EQ(output, "ＡＢＣ.ＤＥＦ");
}

TEST_F(CharacterFormManagerTest, ChunkNormalizationAndAlternativeTest) {
  CharacterFormManager* manager =
      CharacterFormManager::GetCharacterFormManager();

  // Test 1: "Tシャツ" / "Ｔシャツ"
  {
    manager->SetDefaultRule();
    manager->SetCharacterForm("A", config::Config::FULL_WIDTH);
    manager->SetCharacterForm("ア", config::Config::FULL_WIDTH);

    std::string primary, secondary;
    EXPECT_TRUE(manager->ConvertConversionStringWithAlternative(
        "Tシャツ", &primary, &secondary));
    EXPECT_EQ(primary, "Ｔシャツ");
    EXPECT_EQ(secondary, "Tシャツ");

    manager->SetCharacterForm("A", config::Config::HALF_WIDTH);
    EXPECT_TRUE(manager->ConvertConversionStringWithAlternative(
        "Ｔシャツ", &primary, &secondary));
    EXPECT_EQ(primary, "Tシャツ");
    EXPECT_EQ(secondary, "Ｔシャツ");
  }

  // Test 2: "3時" / "３時"
  {
    manager->SetDefaultRule();
    manager->SetCharacterForm("0", config::Config::FULL_WIDTH);

    std::string primary, secondary;
    EXPECT_TRUE(manager->ConvertConversionStringWithAlternative("3時", &primary,
                                                                &secondary));
    EXPECT_EQ(primary, "３時");
    EXPECT_EQ(secondary, "3時");

    manager->SetCharacterForm("0", config::Config::HALF_WIDTH);
    EXPECT_TRUE(manager->ConvertConversionStringWithAlternative(
        "３時", &primary, &secondary));
    EXPECT_EQ(primary, "3時");
    EXPECT_EQ(secondary, "３時");
  }

  // Test 3: "3.14" / "３．１４" and "1,234" / "１，２３４"
  {
    manager->SetDefaultRule();
    manager->SetCharacterForm("0", config::Config::FULL_WIDTH);

    std::string primary, secondary;
    EXPECT_TRUE(manager->ConvertConversionStringWithAlternative(
        "3.14", &primary, &secondary));
    EXPECT_EQ(primary, "３．１４");
    EXPECT_EQ(secondary, "3.14");

    EXPECT_TRUE(manager->ConvertConversionStringWithAlternative(
        "1,234", &primary, &secondary));
    EXPECT_EQ(primary, "１，２３４");
    EXPECT_EQ(secondary, "1,234");

    manager->SetCharacterForm("0", config::Config::HALF_WIDTH);
    EXPECT_TRUE(manager->ConvertConversionStringWithAlternative(
        "３．１４", &primary, &secondary));
    EXPECT_EQ(primary, "3.14");
    EXPECT_EQ(secondary, "３．１４");

    EXPECT_TRUE(manager->ConvertConversionStringWithAlternative(
        "１，２３４", &primary, &secondary));
    EXPECT_EQ(primary, "1,234");
    EXPECT_EQ(secondary, "１，２３４");
  }

  // Test 4: "2日です。"
  {
    manager->SetDefaultRule();
    manager->SetCharacterForm("0", config::Config::HALF_WIDTH);

    std::string primary, secondary;
    EXPECT_TRUE(manager->ConvertConversionStringWithAlternative(
        "２日です。", &primary, &secondary));
    EXPECT_EQ(primary, "2日です。");
    EXPECT_EQ(secondary, "２日です。");
  }

  // Test 5: Regression tests for mixed alphanumeric/symbol candidates
  // ("Wi-Fi", "C++", "Yahoo!", "12:30").
  // When alphabet/number is learned as half-width, but symbols have not been
  // committed (defaulting to full-width in storage fallback), consistency
  // within a variable-width run must be preserved, preventing inconsistent
  // forms like "Wi－Fi", "C＋＋", "Yahoo！", or "12：30".
  {
    manager->SetDefaultRule();
    manager->ClearHistory();
    manager->SetCharacterForm("A", config::Config::HALF_WIDTH);
    manager->SetCharacterForm("0", config::Config::HALF_WIDTH);

    std::string primary, secondary;

    // "Wi-Fi" keeps primary as "Wi-Fi" because "-" is uncommitted (fallback
    // full-width) and adjacent to alphabet "Wi" and "Fi" in the same
    // variable-width run, but still generates full-width alternative
    // "Ｗｉ−Ｆｉ".
    EXPECT_TRUE(manager->ConvertConversionStringWithAlternative(
        "Wi-Fi", &primary, &secondary));
    EXPECT_EQ(primary, "Wi-Fi");
    EXPECT_EQ(secondary, "Ｗｉ−Ｆｉ");

    // "C++" keeps primary "C++", generates "Ｃ＋＋".
    EXPECT_TRUE(manager->ConvertConversionStringWithAlternative("C++", &primary,
                                                                &secondary));
    EXPECT_EQ(primary, "C++");
    EXPECT_EQ(secondary, "Ｃ＋＋");

    // "Yahoo!" keeps primary "Yahoo!", generates "Ｙａｈｏｏ！".
    EXPECT_TRUE(manager->ConvertConversionStringWithAlternative(
        "Yahoo!", &primary, &secondary));
    EXPECT_EQ(primary, "Yahoo!");
    EXPECT_EQ(secondary, "Ｙａｈｏｏ！");

    // "12:30" keeps primary "12:30", generates "１２：３０".
    EXPECT_TRUE(manager->ConvertConversionStringWithAlternative(
        "12:30", &primary, &secondary));
    EXPECT_EQ(primary, "12:30");
    EXPECT_EQ(secondary, "１２：３０");

    // If the symbol is explicitly set/learned as HALF_WIDTH, conversion
    // from full-width input succeeds consistently to half-width.
    manager->SetCharacterForm("-", config::Config::HALF_WIDTH);
    EXPECT_TRUE(manager->ConvertConversionStringWithAlternative(
        "Ｗｉ−Ｆｉ", &primary, &secondary));
    EXPECT_EQ(primary, "Wi-Fi");
    EXPECT_EQ(secondary, "Ｗｉ−Ｆｉ");
  }

  // Test 6: Japanese brackets/punctuation and katakana as run delimiters.
  {
    manager->SetDefaultRule();
    manager->SetCharacterForm("A", config::Config::HALF_WIDTH);
    manager->SetCharacterForm("0", config::Config::HALF_WIDTH);
    manager->SetCharacterForm("ア", config::Config::FULL_WIDTH);

    std::string primary, secondary;

    // "「Ｔシャツ」" converts to "「Tシャツ」" because "「" is a fixed-width
    // delimiter.
    EXPECT_TRUE(manager->ConvertConversionStringWithAlternative(
        "「Ｔシャツ」", &primary, &secondary));
    EXPECT_EQ(primary, "「Tシャツ」");
    EXPECT_EQ(secondary, "「Ｔシャツ」");

    // "「３時」" converts to "「3時」".
    EXPECT_TRUE(manager->ConvertConversionStringWithAlternative(
        "「３時」", &primary, &secondary));
    EXPECT_EQ(primary, "「3時」");
    EXPECT_EQ(secondary, "「３時」");

    // "１００。" converts to "100。" because "。" is a fixed-width delimiter.
    EXPECT_TRUE(manager->ConvertConversionStringWithAlternative(
        "１００。", &primary, &secondary));
    EXPECT_EQ(primary, "100。");
    EXPECT_EQ(secondary, "１００。");

    // "ABCｲﾝﾀｰﾈｯﾄ" converts to "ABCインターネット" because katakana acts as a
    // delimiter regardless of whether input katakana is full- or half-width.
    std::string output;
    manager->ConvertConversionString("ABCｲﾝﾀｰﾈｯﾄ", &output);
    EXPECT_EQ(output, "ABCインターネット");
  }
}

TEST_F(CharacterFormManagerTest, GroupTest) {
  CharacterFormManager* manager =
      CharacterFormManager::GetCharacterFormManager();

  {
    manager->ClearHistory();
    manager->Clear();
    manager->AddConversionRule("ア", config::Config::FULL_WIDTH);
    manager->AddPreeditRule("ア", config::Config::HALF_WIDTH);
    manager->AddConversionRule("[]", config::Config::HALF_WIDTH);
    manager->AddPreeditRule("[]", config::Config::FULL_WIDTH);
    manager->AddConversionRule("!@#$%^&*()-=", config::Config::FULL_WIDTH);
    manager->AddConversionRule("!@#$%^&*()-=", config::Config::HALF_WIDTH);

    EXPECT_EQ(manager->GetConversionCharacterForm("["),
              config::Config::HALF_WIDTH);

    EXPECT_EQ(manager->GetPreeditCharacterForm("["),
              config::Config::FULL_WIDTH);

    EXPECT_EQ(manager->GetPreeditCharacterForm("ア"),
              config::Config::HALF_WIDTH);

    manager->SetCharacterForm("[", config::Config::FULL_WIDTH);
    manager->SetCharacterForm("ア", config::Config::FULL_WIDTH);
    manager->SetCharacterForm("@", config::Config::FULL_WIDTH);

    EXPECT_EQ(manager->GetConversionCharacterForm("["),
              config::Config::HALF_WIDTH);

    EXPECT_EQ(manager->GetPreeditCharacterForm("["),
              config::Config::FULL_WIDTH);

    EXPECT_EQ(manager->GetPreeditCharacterForm("ア"),
              config::Config::HALF_WIDTH);
  }

  {
    manager->ClearHistory();
    manager->Clear();
    manager->AddConversionRule("ア", config::Config::FULL_WIDTH);
    manager->AddConversionRule("[]", config::Config::LAST_FORM);
    manager->AddConversionRule("!@#$%^&*()-=", config::Config::FULL_WIDTH);

    EXPECT_EQ(manager->GetConversionCharacterForm("["),
              config::Config::FULL_WIDTH);  // default

    // same group
    manager->SetCharacterForm("]", config::Config::HALF_WIDTH);

    EXPECT_EQ(manager->GetConversionCharacterForm("["),
              config::Config::HALF_WIDTH);
  }

  {
    manager->ClearHistory();
    manager->Clear();
    manager->AddConversionRule("ア", config::Config::FULL_WIDTH);
    manager->AddConversionRule("[](){}", config::Config::LAST_FORM);
    manager->AddConversionRule("!@#$%^&*-=", config::Config::FULL_WIDTH);

    EXPECT_EQ(manager->GetConversionCharacterForm("{"),
              config::Config::FULL_WIDTH);
    EXPECT_EQ(manager->GetConversionCharacterForm("}"),
              config::Config::FULL_WIDTH);
    EXPECT_EQ(manager->GetConversionCharacterForm("("),
              config::Config::FULL_WIDTH);
    EXPECT_EQ(manager->GetConversionCharacterForm(")"),
              config::Config::FULL_WIDTH);

    // same group
    manager->SetCharacterForm(")", config::Config::HALF_WIDTH);

    EXPECT_EQ(manager->GetConversionCharacterForm("{"),
              config::Config::HALF_WIDTH);
    EXPECT_EQ(manager->GetConversionCharacterForm("}"),
              config::Config::HALF_WIDTH);
    EXPECT_EQ(manager->GetConversionCharacterForm("("),
              config::Config::HALF_WIDTH);
    EXPECT_EQ(manager->GetConversionCharacterForm(")"),
              config::Config::HALF_WIDTH);
  }

  {
    manager->ClearHistory();
    manager->Clear();
    manager->AddConversionRule("ア", config::Config::FULL_WIDTH);
    manager->AddConversionRule("[](){}", config::Config::LAST_FORM);
    manager->AddPreeditRule("[](){}", config::Config::FULL_WIDTH);
    manager->AddConversionRule("!@#$%^&*-=", config::Config::FULL_WIDTH);

    EXPECT_EQ(manager->GetConversionCharacterForm("{"),
              config::Config::FULL_WIDTH);
    EXPECT_EQ(manager->GetConversionCharacterForm("}"),
              config::Config::FULL_WIDTH);
    EXPECT_EQ(manager->GetConversionCharacterForm("("),
              config::Config::FULL_WIDTH);
    EXPECT_EQ(manager->GetConversionCharacterForm(")"),
              config::Config::FULL_WIDTH);

    EXPECT_EQ(manager->GetPreeditCharacterForm("{"),
              config::Config::FULL_WIDTH);
    EXPECT_EQ(manager->GetPreeditCharacterForm("}"),
              config::Config::FULL_WIDTH);
    EXPECT_EQ(manager->GetPreeditCharacterForm("("),
              config::Config::FULL_WIDTH);
    EXPECT_EQ(manager->GetPreeditCharacterForm(")"),
              config::Config::FULL_WIDTH);

    // same group
    manager->SetCharacterForm(")", config::Config::HALF_WIDTH);

    EXPECT_EQ(manager->GetConversionCharacterForm("{"),
              config::Config::HALF_WIDTH);
    EXPECT_EQ(manager->GetConversionCharacterForm("}"),
              config::Config::HALF_WIDTH);
    EXPECT_EQ(manager->GetConversionCharacterForm("("),
              config::Config::HALF_WIDTH);
    EXPECT_EQ(manager->GetConversionCharacterForm(")"),
              config::Config::HALF_WIDTH);

    EXPECT_EQ(manager->GetPreeditCharacterForm("{"),
              config::Config::FULL_WIDTH);
    EXPECT_EQ(manager->GetPreeditCharacterForm("}"),
              config::Config::FULL_WIDTH);
    EXPECT_EQ(manager->GetPreeditCharacterForm("("),
              config::Config::FULL_WIDTH);
    EXPECT_EQ(manager->GetPreeditCharacterForm(")"),
              config::Config::FULL_WIDTH);
  }
}

TEST_F(CharacterFormManagerTest, InvalidStringTest) {
  CharacterFormManager* manager =
      CharacterFormManager::GetCharacterFormManager();

  std::string output;
  // "る<invalid>る"
  manager->ConvertConversionString("\xE3\x82\x8B\x88\xE3\x82\x8B", &output);
  EXPECT_EQ(output, "るる");
}

TEST_F(CharacterFormManagerTest, NumberStyle) {
  CharacterFormManager* manager =
      CharacterFormManager::GetCharacterFormManager();
  {
    std::optional<const CharacterFormManager::NumberFormStyle> stored_entry =
        manager->GetLastNumberStyle();
    EXPECT_EQ(stored_entry, std::nullopt);
  }

  CharacterFormManager::NumberFormStyle entry = {
      config::Config::FULL_WIDTH, NumberUtil::NumberString::DEFAULT_STYLE};
  manager->SetLastNumberStyle(entry);

  {
    std::optional<const CharacterFormManager::NumberFormStyle> stored_entry =
        manager->GetLastNumberStyle();
    ASSERT_NE(stored_entry, std::nullopt);
    EXPECT_EQ(stored_entry->form, entry.form);
    EXPECT_EQ(stored_entry->style, entry.style);
  }
}

TEST_F(CharacterFormManagerTest, GuessAndSetCharacterForm) {
  CharacterFormManager* manager =
      CharacterFormManager::GetCharacterFormManager();
  manager->ClearHistory();

  EXPECT_EQ(manager->GetConversionCharacterForm("0"),
            config::Config::FULL_WIDTH);

  manager->GuessAndSetCharacterForm("123");
  EXPECT_EQ(manager->GetConversionCharacterForm("0"),
            config::Config::HALF_WIDTH);

  manager->GuessAndSetCharacterForm("１２３");
  EXPECT_EQ(manager->GetConversionCharacterForm("0"),
            config::Config::FULL_WIDTH);

  // Mixed-script number + Kanji counter.
  manager->GuessAndSetCharacterForm("2日");
  EXPECT_EQ(manager->GetConversionCharacterForm("0"),
            config::Config::HALF_WIDTH);

  manager->GuessAndSetCharacterForm("２日");
  EXPECT_EQ(manager->GetConversionCharacterForm("0"),
            config::Config::FULL_WIDTH);

  // Hiragana + number + Hiragana.
  manager->GuessAndSetCharacterForm("あと3つ");
  EXPECT_EQ(manager->GetConversionCharacterForm("0"),
            config::Config::HALF_WIDTH);

  // Decimal and comma-separated numbers.
  manager->GuessAndSetCharacterForm("３．１４");
  EXPECT_EQ(manager->GetConversionCharacterForm("0"),
            config::Config::FULL_WIDTH);

  manager->GuessAndSetCharacterForm("3.14");
  EXPECT_EQ(manager->GetConversionCharacterForm("0"),
            config::Config::HALF_WIDTH);

  // Full-width number with half-width period (e.g., when number and period
  // rules have different widths).
  manager->GuessAndSetCharacterForm("３.１４");
  EXPECT_EQ(manager->GetConversionCharacterForm("0"),
            config::Config::FULL_WIDTH);

  manager->GuessAndSetCharacterForm("1.項目");
  EXPECT_EQ(manager->GetConversionCharacterForm("0"),
            config::Config::HALF_WIDTH);

  manager->GuessAndSetCharacterForm("１.項目");
  EXPECT_EQ(manager->GetConversionCharacterForm("0"),
            config::Config::FULL_WIDTH);

  manager->GuessAndSetCharacterForm("１，０００");
  EXPECT_EQ(manager->GetConversionCharacterForm("0"),
            config::Config::FULL_WIDTH);

  manager->GuessAndSetCharacterForm("1,000");
  EXPECT_EQ(manager->GetConversionCharacterForm("0"),
            config::Config::HALF_WIDTH);

  // Katakana with prolonged sound mark.
  manager->AddConversionRule("カタカナ", config::Config::LAST_FORM);
  manager->GuessAndSetCharacterForm("ｲﾝﾀｰﾈｯﾄ");
  EXPECT_EQ(manager->GetConversionCharacterForm("カタカナ"),
            config::Config::HALF_WIDTH);

  manager->GuessAndSetCharacterForm("インターネット");
  EXPECT_EQ(manager->GetConversionCharacterForm("カタカナ"),
            config::Config::FULL_WIDTH);

  // Embedded full-width middle dot should not overwrite symbol rule or prevent
  // learning half-width katakana.
  manager->AddConversionRule("・「」", config::Config::LAST_FORM);
  manager->SetCharacterForm("「", config::Config::HALF_WIDTH);
  manager->GuessAndSetCharacterForm("東京・大阪");
  EXPECT_EQ(manager->GetConversionCharacterForm("「"),
            config::Config::HALF_WIDTH);

  manager->GuessAndSetCharacterForm("ｱ・ｲ");
  EXPECT_EQ(manager->GetConversionCharacterForm("カタカナ"),
            config::Config::HALF_WIDTH);
}

TEST_F(CharacterFormManagerTest, StorageSerializationTest) {
  CharacterFormManager* manager =
      CharacterFormManager::GetCharacterFormManager();
  manager->ClearHistory();
  EXPECT_FALSE(manager->IsStorageDirty());

  manager->SetCharacterForm("012", config::Config::HALF_WIDTH);
  manager->SetCharacterForm("[", config::Config::HALF_WIDTH);
  const CharacterFormManager::NumberFormStyle number_style = {
      config::Config::HALF_WIDTH, NumberUtil::NumberString::NUMBER_KANJI};
  manager->SetLastNumberStyle(number_style);
  EXPECT_TRUE(manager->IsStorageDirty());

  user_history_predictor::UserHistory history;
  manager->SaveStorage(&history);
  EXPECT_FALSE(manager->IsStorageDirty());

  manager->ClearHistory();
  EXPECT_EQ(manager->GetConversionCharacterForm("012"),
            config::Config::FULL_WIDTH);
  EXPECT_EQ(manager->GetConversionCharacterForm("["),
            config::Config::FULL_WIDTH);
  EXPECT_EQ(manager->GetLastNumberStyle(), std::nullopt);

  manager->LoadStorage(history);
  EXPECT_FALSE(manager->IsStorageDirty());
  EXPECT_EQ(manager->GetConversionCharacterForm("012"),
            config::Config::HALF_WIDTH);
  EXPECT_EQ(manager->GetConversionCharacterForm("["),
            config::Config::HALF_WIDTH);
  EXPECT_EQ(manager->GetConversionCharacterForm("]"),
            config::Config::HALF_WIDTH);
  const std::optional<const CharacterFormManager::NumberFormStyle>
      loaded_style = manager->GetLastNumberStyle();
  ASSERT_NE(loaded_style, std::nullopt);
  EXPECT_EQ(loaded_style->form, number_style.form);
  EXPECT_EQ(loaded_style->style, number_style.style);
}

}  // namespace
}  // namespace config
}  // namespace mozc
