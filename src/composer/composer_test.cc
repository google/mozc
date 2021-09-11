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

#include "composer/composer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/clock_mock.h"
#include "base/logging.h"
#include "base/system_util.h"
#include "base/util.h"
#include "composer/internal/typing_model.h"
#include "composer/key_parser.h"
#include "composer/table.h"
#include "config/character_form_manager.h"
#include "config/config_handler.h"
#include "data_manager/testing/mock_data_manager.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "testing/base/public/gunit.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace composer {
namespace {

using CharacterFormManager = ::mozc::config::CharacterFormManager;
using Config = ::mozc::config::Config;
using ConfigHandler = ::mozc::config::ConfigHandler;
using KeyEvent = ::mozc::commands::KeyEvent;
using ProbableKeyEvent = ::mozc::commands::KeyEvent::ProbableKeyEvent;
using ProbableKeyEvents = ::mozc::protobuf::RepeatedPtrField<ProbableKeyEvent>;
using Request = ::mozc::commands::Request;

bool InsertKey(const std::string &key_string, Composer *composer) {
  commands::KeyEvent key;
  if (!KeyParser::ParseKey(key_string, &key)) {
    return false;
  }
  return composer->InsertCharacterKeyEvent(key);
}

bool InsertKeyWithMode(const std::string &key_string,
                       const commands::CompositionMode mode,
                       Composer *composer) {
  commands::KeyEvent key;
  if (!KeyParser::ParseKey(key_string, &key)) {
    return false;
  }
  key.set_mode(mode);
  return composer->InsertCharacterKeyEvent(key);
}

std::string GetPreedit(const Composer *composer) {
  std::string preedit;
  composer->GetStringForPreedit(&preedit);
  return preedit;
}

void ExpectSameComposer(const Composer &lhs, const Composer &rhs) {
  EXPECT_EQ(lhs.GetCursor(), rhs.GetCursor());
  EXPECT_EQ(lhs.is_new_input(), rhs.is_new_input());
  EXPECT_EQ(lhs.GetInputMode(), rhs.GetInputMode());
  EXPECT_EQ(lhs.GetOutputMode(), rhs.GetOutputMode());
  EXPECT_EQ(lhs.GetComebackInputMode(), rhs.GetComebackInputMode());
  EXPECT_EQ(lhs.shifted_sequence_count(), rhs.shifted_sequence_count());
  EXPECT_EQ(lhs.source_text(), rhs.source_text());
  EXPECT_EQ(lhs.max_length(), rhs.max_length());
  EXPECT_EQ(lhs.GetInputFieldType(), rhs.GetInputFieldType());

  {
    std::string left_text, right_text;
    lhs.GetStringForPreedit(&left_text);
    rhs.GetStringForPreedit(&right_text);
    EXPECT_EQ(left_text, right_text);
  }
  {
    std::string left_text, right_text;
    lhs.GetStringForSubmission(&left_text);
    rhs.GetStringForSubmission(&right_text);
    EXPECT_EQ(left_text, right_text);
  }
  {
    std::string left_text, right_text;
    lhs.GetQueryForConversion(&left_text);
    rhs.GetQueryForConversion(&right_text);
    EXPECT_EQ(left_text, right_text);
  }
  {
    std::string left_text, right_text;
    lhs.GetQueryForPrediction(&left_text);
    rhs.GetQueryForPrediction(&right_text);
    EXPECT_EQ(left_text, right_text);
  }
}

}  // namespace

class ComposerTest : public ::testing::Test {
 protected:
  ComposerTest() = default;
  ~ComposerTest() override = default;

  void SetUp() override {
    table_ = absl::make_unique<Table>();
    config_ = absl::make_unique<Config>();
    request_ = absl::make_unique<Request>();
    composer_ = absl::make_unique<Composer>(table_.get(), request_.get(),
                                            config_.get());
    CharacterFormManager::GetCharacterFormManager()->SetDefaultRule();
  }

  void TearDown() override {
    CharacterFormManager::GetCharacterFormManager()->SetDefaultRule();
    composer_.reset();
    request_.reset();
    config_.reset();
    table_.reset();
  }

  const testing::MockDataManager mock_data_manager_;
  std::unique_ptr<Composer> composer_;
  std::unique_ptr<Table> table_;
  std::unique_ptr<Request> request_;
  std::unique_ptr<Config> config_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ComposerTest);
};

TEST_F(ComposerTest, Reset) {
  composer_->InsertCharacter("mozuku");

  composer_->SetInputMode(transliteration::HALF_ASCII);

  EXPECT_EQ(transliteration::HIRAGANA, composer_->GetOutputMode());
  composer_->SetOutputMode(transliteration::HALF_ASCII);
  EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetOutputMode());

  composer_->SetInputFieldType(commands::Context::PASSWORD);
  composer_->Reset();

  EXPECT_TRUE(composer_->Empty());
  // The input mode ramains as the previous mode.
  EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());
  EXPECT_EQ(commands::Context::PASSWORD, composer_->GetInputFieldType());
  // The output mode should be reset.
  EXPECT_EQ(transliteration::HIRAGANA, composer_->GetOutputMode());
}

TEST_F(ComposerTest, ResetInputMode) {
  composer_->InsertCharacter("mozuku");

  composer_->SetInputMode(transliteration::FULL_KATAKANA);
  composer_->SetTemporaryInputMode(transliteration::HALF_ASCII);
  composer_->ResetInputMode();

  EXPECT_FALSE(composer_->Empty());
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());
}

TEST_F(ComposerTest, Empty) {
  composer_->InsertCharacter("mozuku");
  EXPECT_FALSE(composer_->Empty());

  composer_->EditErase();
  EXPECT_TRUE(composer_->Empty());
}

TEST_F(ComposerTest, EnableInsert) {
  composer_->set_max_length(6);

  composer_->InsertCharacter("mozuk");
  EXPECT_EQ(5, composer_->GetLength());

  EXPECT_TRUE(composer_->EnableInsert());
  composer_->InsertCharacter("u");
  EXPECT_EQ(6, composer_->GetLength());

  EXPECT_FALSE(composer_->EnableInsert());
  composer_->InsertCharacter("!");
  EXPECT_EQ(6, composer_->GetLength());

  std::string result;
  composer_->GetQueryForConversion(&result);
  EXPECT_EQ("mozuku", result);

  composer_->Backspace();
  EXPECT_EQ(5, composer_->GetLength());
  EXPECT_TRUE(composer_->EnableInsert());
}

TEST_F(ComposerTest, BackSpace) {
  composer_->InsertCharacter("abc");

  composer_->Backspace();
  EXPECT_EQ(2, composer_->GetLength());
  EXPECT_EQ(2, composer_->GetCursor());
  std::string result;
  composer_->GetQueryForConversion(&result);
  EXPECT_EQ("ab", result);

  result.clear();
  composer_->GetQueryForConversion(&result);
  EXPECT_EQ("ab", result);

  composer_->MoveCursorToBeginning();
  EXPECT_EQ(0, composer_->GetCursor());

  composer_->Backspace();
  EXPECT_EQ(2, composer_->GetLength());
  EXPECT_EQ(0, composer_->GetCursor());
  result.clear();
  composer_->GetQueryForConversion(&result);
  EXPECT_EQ("ab", result);

  composer_->Backspace();
  EXPECT_EQ(2, composer_->GetLength());
  EXPECT_EQ(0, composer_->GetCursor());
  result.clear();
  composer_->GetQueryForConversion(&result);
  EXPECT_EQ("ab", result);
}

TEST_F(ComposerTest, OutputMode) {
  // This behaviour is based on Kotoeri

  table_->AddRule("a", "あ", "");
  table_->AddRule("i", "い", "");
  table_->AddRule("u", "う", "");

  composer_->SetOutputMode(transliteration::HIRAGANA);

  composer_->InsertCharacter("a");
  composer_->InsertCharacter("i");
  composer_->InsertCharacter("u");

  std::string output;
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ("あいう", output);

  composer_->SetOutputMode(transliteration::FULL_ASCII);
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ("ａｉｕ", output);

  composer_->InsertCharacter("a");
  composer_->InsertCharacter("i");
  composer_->InsertCharacter("u");
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ("ａｉｕあいう", output);
}

TEST_F(ComposerTest, OutputMode_2) {
  // This behaviour is based on Kotoeri

  table_->AddRule("a", "あ", "");
  table_->AddRule("i", "い", "");
  table_->AddRule("u", "う", "");

  composer_->InsertCharacter("a");
  composer_->InsertCharacter("i");
  composer_->InsertCharacter("u");

  std::string output;
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ("あいう", output);

  composer_->MoveCursorLeft();
  composer_->SetOutputMode(transliteration::FULL_ASCII);
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ("ａｉｕ", output);

  composer_->InsertCharacter("a");
  composer_->InsertCharacter("i");
  composer_->InsertCharacter("u");
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ("ａｉｕあいう", output);
}

TEST_F(ComposerTest, GetTransliterations) {
  table_->AddRule("a", "あ", "");
  table_->AddRule("i", "い", "");
  table_->AddRule("u", "う", "");
  table_->AddRule("A", "あ", "");
  table_->AddRule("I", "い", "");
  table_->AddRule("U", "う", "");
  composer_->InsertCharacter("a");

  transliteration::Transliterations transliterations;
  composer_->GetTransliterations(&transliterations);
  EXPECT_EQ(transliteration::NUM_T13N_TYPES, transliterations.size());
  EXPECT_EQ("あ", transliterations[transliteration::HIRAGANA]);
  EXPECT_EQ("ア", transliterations[transliteration::FULL_KATAKANA]);
  EXPECT_EQ("a", transliterations[transliteration::HALF_ASCII]);
  EXPECT_EQ("ａ", transliterations[transliteration::FULL_ASCII]);
  EXPECT_EQ("ｱ", transliterations[transliteration::HALF_KATAKANA]);

  composer_->Reset();
  ASSERT_TRUE(composer_->Empty());
  transliterations.clear();

  composer_->InsertCharacter("!");
  composer_->GetTransliterations(&transliterations);
  EXPECT_EQ(transliteration::NUM_T13N_TYPES, transliterations.size());
  // NOTE(komatsu): The duplication will be handled by the session layer.
  EXPECT_EQ("！", transliterations[transliteration::HIRAGANA]);
  EXPECT_EQ("！", transliterations[transliteration::FULL_KATAKANA]);
  EXPECT_EQ("!", transliterations[transliteration::HALF_ASCII]);
  EXPECT_EQ("！", transliterations[transliteration::FULL_ASCII]);
  EXPECT_EQ("!", transliterations[transliteration::HALF_KATAKANA]);

  composer_->Reset();
  ASSERT_TRUE(composer_->Empty());
  transliterations.clear();

  composer_->InsertCharacter("aIu");
  composer_->GetTransliterations(&transliterations);
  EXPECT_EQ(transliteration::NUM_T13N_TYPES, transliterations.size());
  EXPECT_EQ("あいう", transliterations[transliteration::HIRAGANA]);
  EXPECT_EQ("アイウ", transliterations[transliteration::FULL_KATAKANA]);
  EXPECT_EQ("aIu", transliterations[transliteration::HALF_ASCII]);
  EXPECT_EQ("AIU", transliterations[transliteration::HALF_ASCII_UPPER]);
  EXPECT_EQ("aiu", transliterations[transliteration::HALF_ASCII_LOWER]);
  EXPECT_EQ("Aiu", transliterations[transliteration::HALF_ASCII_CAPITALIZED]);
  EXPECT_EQ("ａＩｕ", transliterations[transliteration::FULL_ASCII]);
  EXPECT_EQ("ＡＩＵ", transliterations[transliteration::FULL_ASCII_UPPER]);
  EXPECT_EQ("ａｉｕ", transliterations[transliteration::FULL_ASCII_LOWER]);
  EXPECT_EQ("Ａｉｕ",
            transliterations[transliteration::FULL_ASCII_CAPITALIZED]);
  EXPECT_EQ("ｱｲｳ", transliterations[transliteration::HALF_KATAKANA]);

  // Transliterations for quote marks.  This is a test against
  // http://b/1581367
  composer_->Reset();
  ASSERT_TRUE(composer_->Empty());
  transliterations.clear();

  composer_->InsertCharacter("'\"`");
  composer_->GetTransliterations(&transliterations);
  EXPECT_EQ("'\"`", transliterations[transliteration::HALF_ASCII]);
  EXPECT_EQ("’”｀", transliterations[transliteration::FULL_ASCII]);
}

TEST_F(ComposerTest, GetSubTransliterations) {
  table_->AddRule("ka", "か", "");
  table_->AddRule("n", "ん", "");
  table_->AddRule("na", "な", "");
  table_->AddRule("da", "だ", "");

  composer_->InsertCharacter("kanna");

  transliteration::Transliterations transliterations;
  composer_->GetSubTransliterations(0, 2, &transliterations);
  EXPECT_EQ("かん", transliterations[transliteration::HIRAGANA]);
  EXPECT_EQ("カン", transliterations[transliteration::FULL_KATAKANA]);
  EXPECT_EQ("kan", transliterations[transliteration::HALF_ASCII]);
  EXPECT_EQ("ｋａｎ", transliterations[transliteration::FULL_ASCII]);
  EXPECT_EQ("ｶﾝ", transliterations[transliteration::HALF_KATAKANA]);

  transliterations.clear();
  composer_->GetSubTransliterations(1, 1, &transliterations);
  EXPECT_EQ("ん", transliterations[transliteration::HIRAGANA]);
  EXPECT_EQ("ン", transliterations[transliteration::FULL_KATAKANA]);
  EXPECT_EQ("n", transliterations[transliteration::HALF_ASCII]);
  EXPECT_EQ("ｎ", transliterations[transliteration::FULL_ASCII]);
  EXPECT_EQ("ﾝ", transliterations[transliteration::HALF_KATAKANA]);

  transliterations.clear();
  composer_->GetSubTransliterations(2, 1, &transliterations);
  EXPECT_EQ("な", transliterations[transliteration::HIRAGANA]);
  EXPECT_EQ("ナ", transliterations[transliteration::FULL_KATAKANA]);
  EXPECT_EQ("na", transliterations[transliteration::HALF_ASCII]);
  EXPECT_EQ("ｎａ", transliterations[transliteration::FULL_ASCII]);
  EXPECT_EQ("ﾅ", transliterations[transliteration::HALF_KATAKANA]);

  // Invalid position
  transliterations.clear();
  composer_->GetSubTransliterations(5, 3, &transliterations);
  EXPECT_EQ("", transliterations[transliteration::HIRAGANA]);
  EXPECT_EQ("", transliterations[transliteration::FULL_KATAKANA]);
  EXPECT_EQ("", transliterations[transliteration::HALF_ASCII]);
  EXPECT_EQ("", transliterations[transliteration::FULL_ASCII]);
  EXPECT_EQ("", transliterations[transliteration::HALF_KATAKANA]);

  // Invalid size
  transliterations.clear();
  composer_->GetSubTransliterations(0, 999, &transliterations);
  EXPECT_EQ("かんな", transliterations[transliteration::HIRAGANA]);
  EXPECT_EQ("カンナ", transliterations[transliteration::FULL_KATAKANA]);
  EXPECT_EQ("kanna", transliterations[transliteration::HALF_ASCII]);
  EXPECT_EQ("ｋａｎｎａ", transliterations[transliteration::FULL_ASCII]);
  EXPECT_EQ("ｶﾝﾅ", transliterations[transliteration::HALF_KATAKANA]);

  // Dakuon case
  transliterations.clear();
  composer_->EditErase();
  composer_->InsertCharacter("dankann");
  composer_->GetSubTransliterations(0, 3, &transliterations);
  EXPECT_EQ("だんか", transliterations[transliteration::HIRAGANA]);
  EXPECT_EQ("ダンカ", transliterations[transliteration::FULL_KATAKANA]);
  EXPECT_EQ("danka", transliterations[transliteration::HALF_ASCII]);
  EXPECT_EQ("ｄａｎｋａ", transliterations[transliteration::FULL_ASCII]);
  EXPECT_EQ("ﾀﾞﾝｶ", transliterations[transliteration::HALF_KATAKANA]);
}

TEST_F(ComposerTest, GetStringFunctions) {
  table_->AddRule("ka", "か", "");
  table_->AddRule("n", "ん", "");
  table_->AddRule("na", "な", "");
  table_->AddRule("sa", "さ", "");

  // Query: "!kan"
  composer_->InsertCharacter("!kan");
  std::string preedit;
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("！かｎ", preedit);

  std::string submission;
  composer_->GetStringForSubmission(&submission);
  EXPECT_EQ("！かｎ", submission);

  std::string conversion;
  composer_->GetQueryForConversion(&conversion);
  EXPECT_EQ("!かん", conversion);

  std::string prediction;
  composer_->GetQueryForPrediction(&prediction);
  EXPECT_EQ("!か", prediction);

  // Query: "kas"
  composer_->EditErase();
  composer_->InsertCharacter("kas");

  preedit.clear();
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("かｓ", preedit);

  submission.clear();
  composer_->GetStringForSubmission(&submission);
  EXPECT_EQ("かｓ", submission);

  // Pending chars should remain.  This is a test against
  // http://b/1799399
  conversion.clear();
  composer_->GetQueryForConversion(&conversion);
  EXPECT_EQ("かs", conversion);

  prediction.clear();
  composer_->GetQueryForPrediction(&prediction);
  EXPECT_EQ("か", prediction);

  // Query: "s"
  composer_->EditErase();
  composer_->InsertCharacter("s");

  preedit.clear();
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("ｓ", preedit);

  submission.clear();
  composer_->GetStringForSubmission(&submission);
  EXPECT_EQ("ｓ", submission);

  conversion.clear();
  composer_->GetQueryForConversion(&conversion);
  EXPECT_EQ("s", conversion);

  prediction.clear();
  composer_->GetQueryForPrediction(&prediction);
  EXPECT_EQ("s", prediction);

  // Query: "sk"
  composer_->EditErase();
  composer_->InsertCharacter("sk");

  preedit.clear();
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("ｓｋ", preedit);

  submission.clear();
  composer_->GetStringForSubmission(&submission);
  EXPECT_EQ("ｓｋ", submission);

  conversion.clear();
  composer_->GetQueryForConversion(&conversion);
  EXPECT_EQ("sk", conversion);

  prediction.clear();
  composer_->GetQueryForPrediction(&prediction);
  EXPECT_EQ("sk", prediction);
}

TEST_F(ComposerTest, GetQueryForPredictionHalfAscii) {
  // Dummy setup of romanji table.
  table_->AddRule("he", "へ", "");
  table_->AddRule("ll", "っｌ", "");
  table_->AddRule("lo", "ろ", "");

  // Switch to Half-Latin input mode.
  composer_->SetInputMode(transliteration::HALF_ASCII);

  std::string prediction;
  {
    constexpr char kInput[] = "hello";
    composer_->InsertCharacter(kInput);
    composer_->GetQueryForPrediction(&prediction);
    EXPECT_EQ(kInput, prediction);
  }
  prediction.clear();
  composer_->EditErase();
  {
    constexpr char kInput[] = "hello!";
    composer_->InsertCharacter(kInput);
    composer_->GetQueryForPrediction(&prediction);
    EXPECT_EQ(kInput, prediction);
  }
}

TEST_F(ComposerTest, GetQueryForPredictionFullAscii) {
  // Dummy setup of romanji table.
  table_->AddRule("he", "へ", "");
  table_->AddRule("ll", "っｌ", "");
  table_->AddRule("lo", "ろ", "");

  // Switch to Full-Latin input mode.
  composer_->SetInputMode(transliteration::FULL_ASCII);

  std::string prediction;
  {
    composer_->InsertCharacter("ｈｅｌｌｏ");
    composer_->GetQueryForPrediction(&prediction);
    EXPECT_EQ("hello", prediction);
  }
  prediction.clear();
  composer_->EditErase();
  {
    composer_->InsertCharacter("ｈｅｌｌｏ！");
    composer_->GetQueryForPrediction(&prediction);
    EXPECT_EQ("hello!", prediction);
  }
}

TEST_F(ComposerTest, GetQueriesForPredictionRoman) {
  table_->AddRule("u", "う", "");
  table_->AddRule("ss", "っ", "s");
  table_->AddRule("sa", "さ", "");
  table_->AddRule("si", "し", "");
  table_->AddRule("su", "す", "");
  table_->AddRule("se", "せ", "");
  table_->AddRule("so", "そ", "");

  {
    std::string base, preedit;
    std::set<std::string> expanded;
    composer_->EditErase();
    composer_->InsertCharacter("us");
    composer_->GetQueriesForPrediction(&base, &expanded);
    composer_->GetStringForPreedit(&preedit);
    EXPECT_EQ("う", base);
    EXPECT_EQ(7, expanded.size());
    // We can't use EXPECT_NE for iterator
    EXPECT_TRUE(expanded.end() != expanded.find("s"));
    EXPECT_TRUE(expanded.end() != expanded.find("っ"));
    EXPECT_TRUE(expanded.end() != expanded.find("さ"));
    EXPECT_TRUE(expanded.end() != expanded.find("し"));
    EXPECT_TRUE(expanded.end() != expanded.find("す"));
    EXPECT_TRUE(expanded.end() != expanded.find("せ"));
    EXPECT_TRUE(expanded.end() != expanded.find("そ"));
  }
}

TEST_F(ComposerTest, GetQueriesForPredictionMobile) {
  table_->AddRule("_", "", "い");
  table_->AddRule("い*", "", "ぃ");
  table_->AddRule("ぃ*", "", "い");
  table_->AddRule("$", "", "と");
  table_->AddRule("と*", "", "ど");
  table_->AddRule("ど*", "", "と");
  table_->AddRule("x", "", "つ");
  table_->AddRule("つ*", "", "っ");
  table_->AddRule("っ*", "", "づ");
  table_->AddRule("づ*", "", "つ");

  {
    std::string base, preedit;
    std::set<std::string> expanded;
    composer_->EditErase();
    composer_->InsertCharacter("_$");
    composer_->GetQueriesForPrediction(&base, &expanded);
    composer_->GetStringForPreedit(&preedit);
    EXPECT_EQ("い", base);
    EXPECT_EQ(2, expanded.size());
    EXPECT_TRUE(expanded.end() != expanded.find("と"));
    EXPECT_TRUE(expanded.end() != expanded.find("ど"));
  }
  {
    std::string base, preedit;
    std::set<std::string> expanded;
    composer_->EditErase();
    composer_->InsertCharacter("_$*");
    composer_->GetQueriesForPrediction(&base, &expanded);
    composer_->GetStringForPreedit(&preedit);
    EXPECT_EQ("い", base);
    EXPECT_EQ(1, expanded.size());
    EXPECT_TRUE(expanded.end() != expanded.find("ど"));
  }
  {
    std::string base, preedit;
    std::set<std::string> expanded;
    composer_->EditErase();
    composer_->InsertCharacter("_x*");
    composer_->GetQueriesForPrediction(&base, &expanded);
    composer_->GetStringForPreedit(&preedit);
    EXPECT_EQ("い", base);
    EXPECT_EQ(1, expanded.size());
    EXPECT_TRUE(expanded.end() != expanded.find("っ"));
  }
}

TEST_F(ComposerTest, GetStringFunctions_ForN) {
  table_->AddRule("a", "[A]", "");
  table_->AddRule("n", "[N]", "");
  table_->AddRule("nn", "[N]", "");
  table_->AddRule("na", "[NA]", "");
  table_->AddRule("nya", "[NYA]", "");
  table_->AddRule("ya", "[YA]", "");
  table_->AddRule("ka", "[KA]", "");

  composer_->InsertCharacter("nynyan");
  std::string preedit;
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("ｎｙ［ＮＹＡ］ｎ", preedit);

  std::string submission;
  composer_->GetStringForSubmission(&submission);
  EXPECT_EQ("ｎｙ［ＮＹＡ］ｎ", submission);

  std::string conversion;
  composer_->GetQueryForConversion(&conversion);
  EXPECT_EQ("ny[NYA][N]", conversion);

  std::string prediction;
  composer_->GetQueryForPrediction(&prediction);
  EXPECT_EQ("ny[NYA]", prediction);

  composer_->InsertCharacter("ka");
  std::string conversion2;
  composer_->GetQueryForConversion(&conversion2);
  EXPECT_EQ("ny[NYA][N][KA]", conversion2);

  std::string prediction2;
  composer_->GetQueryForPrediction(&prediction2);
  EXPECT_EQ("ny[NYA][N][KA]", prediction2);
}

TEST_F(ComposerTest, GetStringFunctions_InputFieldType) {
  const struct TestData {
    const commands::Context::InputFieldType field_type;
    const bool ascii_expected;
  } test_data_list[] = {
      {commands::Context::NORMAL, false},
      {commands::Context::NUMBER, true},
      {commands::Context::PASSWORD, true},
      {commands::Context::TEL, true},
  };

  composer_->SetInputMode(transliteration::HIRAGANA);
  for (size_t test_data_index = 0; test_data_index < std::size(test_data_list);
       ++test_data_index) {
    const TestData &test_data = test_data_list[test_data_index];
    composer_->SetInputFieldType(test_data.field_type);
    std::string key, converted;
    for (char c = 0x20; c <= 0x7E; ++c) {
      key.assign(1, c);
      composer_->EditErase();
      composer_->InsertCharacter(key);
      if (test_data.ascii_expected) {
        composer_->GetStringForPreedit(&converted);
        EXPECT_EQ(key, converted);
        composer_->GetStringForSubmission(&converted);
        EXPECT_EQ(key, converted);
      } else {
        // Expected result is FULL_WIDTH form.
        // Typically the result is a full-width form of the key,
        // but some characters are not.
        // So here we checks only the character form.
        composer_->GetStringForPreedit(&converted);
        EXPECT_EQ(Util::FULL_WIDTH, Util::GetFormType(converted));
        composer_->GetStringForSubmission(&converted);
        EXPECT_EQ(Util::FULL_WIDTH, Util::GetFormType(converted));
      }
    }
  }
}

TEST_F(ComposerTest, InsertCommandCharacter) {
  composer_->SetInputMode(transliteration::HALF_ASCII);

  composer_->InsertCommandCharacter(Composer::REWIND);
  EXPECT_EQ("\x0F<\x0E", GetPreedit(composer_.get()));

  composer_->Reset();
  composer_->InsertCommandCharacter(Composer::STOP_KEY_TOGGLING);
  EXPECT_EQ("\x0F!\x0E", GetPreedit(composer_.get()));
}

TEST_F(ComposerTest, InsertCharacterKeyEvent) {
  commands::KeyEvent key;
  table_->AddRule("a", "あ", "");

  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);

  std::string preedit;
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("あ", preedit);

  // Half width "A" will be inserted.
  key.set_key_code('A');
  composer_->InsertCharacterKeyEvent(key);

  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("あA", preedit);

  // Half width "a" will be inserted.
  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);

  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("あAa", preedit);

  // Reset() should revert the previous input mode (Hiragana).
  composer_->Reset();

  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("あ", preedit);

  // Typing "A" temporarily switch the input mode.  The input mode
  // should be reverted back after reset.
  composer_->SetInputMode(transliteration::FULL_KATAKANA);
  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("あア", preedit);

  key.set_key_code('A');
  composer_->InsertCharacterKeyEvent(key);
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("あアA", preedit);

  // Reset() should revert the previous input mode (Katakana).
  composer_->Reset();

  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("ア", preedit);
}

namespace {
constexpr char kYama[] = "山";
constexpr char kKawa[] = "川";
constexpr char kSora[] = "空";
}  // namespace

TEST_F(ComposerTest, InsertCharacterKeyEventWithUcs4KeyCode) {
  commands::KeyEvent key;

  // Input "山" as key_code.
  key.set_key_code(0x5C71);  // U+5C71 = "山"
  composer_->InsertCharacterKeyEvent(key);

  std::string preedit;
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(kYama, preedit);

  // Input "山" as key_code which is converted to "川".
  table_->AddRule(kYama, kKawa, "");
  composer_->Reset();
  composer_->InsertCharacterKeyEvent(key);
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(kKawa, preedit);

  // Input ("山", "空") as (key_code, key_string) which is treated as "空".
  key.set_key_string(kSora);
  composer_->Reset();
  composer_->InsertCharacterKeyEvent(key);
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(kSora, preedit);
}

TEST_F(ComposerTest, InsertCharacterKeyEventWithoutKeyCode) {
  commands::KeyEvent key;

  // Input "山" as key_string.  key_code is empty.
  key.set_key_string(kYama);
  composer_->InsertCharacterKeyEvent(key);
  EXPECT_FALSE(key.has_key_code());

  std::string preedit;
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(kYama, preedit);

  transliteration::Transliterations transliterations;
  composer_->GetTransliterations(&transliterations);
  EXPECT_EQ(kYama, transliterations[transliteration::HIRAGANA]);
  EXPECT_EQ(kYama, transliterations[transliteration::HALF_ASCII]);
}

TEST_F(ComposerTest, InsertCharacterKeyEventWithAsIs) {
  commands::KeyEvent key;
  table_->AddRule("a", "あ", "");
  table_->AddRule("-", "ー", "");

  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);

  std::string preedit;
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("あ", preedit);

  // Full width "０" will be inserted.
  key.set_key_code('0');
  key.set_key_string("0");
  composer_->InsertCharacterKeyEvent(key);

  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("あ０", preedit);

  // Half width "0" will be inserted.
  key.set_key_code('0');
  key.set_key_string("0");
  key.set_input_style(commands::KeyEvent::AS_IS);
  composer_->InsertCharacterKeyEvent(key);

  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("あ０0", preedit);

  // Full width "0" will be inserted.
  key.set_key_code('0');
  key.set_key_string("0");
  key.set_input_style(commands::KeyEvent::FOLLOW_MODE);
  composer_->InsertCharacterKeyEvent(key);

  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("あ０0０", preedit);

  // Half width "-" will be inserted.
  key.set_key_code('-');
  key.set_key_string("-");
  key.set_input_style(commands::KeyEvent::AS_IS);
  composer_->InsertCharacterKeyEvent(key);

  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("あ０0０-", preedit);

  // Full width "−" (U+2212) will be inserted.
  key.set_key_code('-');
  key.set_key_string("−");
  key.set_input_style(commands::KeyEvent::FOLLOW_MODE);
  composer_->InsertCharacterKeyEvent(key);

  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("あ０0０-−", preedit);  // The last hyphen is U+2212.
}

TEST_F(ComposerTest, InsertCharacterKeyEventWithInputMode) {
  table_->AddRule("a", "あ", "");
  table_->AddRule("i", "い", "");
  table_->AddRule("u", "う", "");

  {
    // "a" → "あ" (Hiragana)
    EXPECT_TRUE(InsertKeyWithMode("a", commands::HIRAGANA, composer_.get()));
    EXPECT_EQ("あ", GetPreedit(composer_.get()));
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());

    // "aI" → "あI" (Alphanumeric)
    EXPECT_TRUE(InsertKeyWithMode("I", commands::HIRAGANA, composer_.get()));
    EXPECT_EQ("あI", GetPreedit(composer_.get()));
    EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());

    // "u" → "あIu" (Alphanumeric)
    EXPECT_TRUE(InsertKeyWithMode("u", commands::HALF_ASCII, composer_.get()));
    EXPECT_EQ("あIu", GetPreedit(composer_.get()));
    EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());

    // [shift] → "あIu" (Hiragana)
    EXPECT_TRUE(
        InsertKeyWithMode("Shift", commands::HALF_ASCII, composer_.get()));
    EXPECT_EQ("あIu", GetPreedit(composer_.get()));
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());

    // "u" → "あIuう" (Hiragana)
    EXPECT_TRUE(InsertKeyWithMode("u", commands::HIRAGANA, composer_.get()));
    EXPECT_EQ("あIuう", GetPreedit(composer_.get()));
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());
  }

  composer_ =
      absl::make_unique<Composer>(table_.get(), request_.get(), config_.get());

  {
    // "a" → "あ" (Hiragana)
    EXPECT_TRUE(InsertKeyWithMode("a", commands::HIRAGANA, composer_.get()));
    EXPECT_EQ("あ", GetPreedit(composer_.get()));
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());

    // "i" (Katakana) → "あイ" (Katakana)
    EXPECT_TRUE(
        InsertKeyWithMode("i", commands::FULL_KATAKANA, composer_.get()));
    EXPECT_EQ("あイ", GetPreedit(composer_.get()));
    EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

    // SetInputMode(Alphanumeric) → "あイ" (Alphanumeric)
    composer_->SetInputMode(transliteration::HALF_ASCII);
    EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());

    // [shift] → "あイ" (Alphanumeric) - Nothing happens.
    EXPECT_TRUE(
        InsertKeyWithMode("Shift", commands::HALF_ASCII, composer_.get()));
    EXPECT_EQ("あイ", GetPreedit(composer_.get()));
    EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());

    // "U" → "あイ" (Alphanumeric)
    EXPECT_TRUE(InsertKeyWithMode("U", commands::HALF_ASCII, composer_.get()));
    EXPECT_EQ("あイU", GetPreedit(composer_.get()));
    EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());

    // [shift] → "あイU" (Alphanumeric) - Nothing happens.
    EXPECT_TRUE(
        InsertKeyWithMode("Shift", commands::HALF_ASCII, composer_.get()));
    EXPECT_EQ("あイU", GetPreedit(composer_.get()));
    EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());
  }
}

TEST_F(ComposerTest, ApplyTemporaryInputMode) {
  constexpr bool kCapsLocked = true;
  constexpr bool kCapsUnlocked = false;

  table_->AddRule("a", "あ", "");
  composer_->SetInputMode(transliteration::HIRAGANA);

  // Since handlings of continuous shifted input differ,
  // test cases differ between ASCII_INPUT_MODE and KATAKANA_INPUT_MODE

  {  // ASCII_INPUT_MODE (w/o CapsLock)
    config_->set_shift_key_mode_switch(Config::ASCII_INPUT_MODE);

    // pair<input, use_temporary_input_mode>
    std::pair<std::string, bool> kTestDataAscii[] = {
        std::make_pair("a", false),  std::make_pair("A", true),
        std::make_pair("a", true),   std::make_pair("a", true),
        std::make_pair("A", true),   std::make_pair("A", true),
        std::make_pair("a", false),  std::make_pair("A", true),
        std::make_pair("A", true),   std::make_pair("A", true),
        std::make_pair("a", false),  std::make_pair("A", true),
        std::make_pair(".", true),   std::make_pair("a", true),
        std::make_pair("A", true),   std::make_pair("A", true),
        std::make_pair(".", true),   std::make_pair("a", true),
        std::make_pair("あ", false), std::make_pair("a", false),
    };

    for (int i = 0; i < std::size(kTestDataAscii); ++i) {
      composer_->ApplyTemporaryInputMode(kTestDataAscii[i].first,
                                         kCapsUnlocked);

      const transliteration::TransliterationType expected =
          kTestDataAscii[i].second ? transliteration::HALF_ASCII
                                   : transliteration::HIRAGANA;

      EXPECT_EQ(expected, composer_->GetInputMode()) << "index=" << i;
      EXPECT_EQ(transliteration::HIRAGANA, composer_->GetComebackInputMode())
          << "index=" << i;
    }
  }

  {  // ASCII_INPUT_MODE (w/ CapsLock)
    config_->set_shift_key_mode_switch(Config::ASCII_INPUT_MODE);

    // pair<input, use_temporary_input_mode>
    std::pair<std::string, bool> kTestDataAscii[] = {
        std::make_pair("A", false),  std::make_pair("a", true),
        std::make_pair("A", true),   std::make_pair("A", true),
        std::make_pair("a", true),   std::make_pair("a", true),
        std::make_pair("A", false),  std::make_pair("a", true),
        std::make_pair("a", true),   std::make_pair("a", true),
        std::make_pair("A", false),  std::make_pair("a", true),
        std::make_pair(".", true),   std::make_pair("A", true),
        std::make_pair("a", true),   std::make_pair("a", true),
        std::make_pair(".", true),   std::make_pair("A", true),
        std::make_pair("あ", false), std::make_pair("A", false),
    };

    for (int i = 0; i < std::size(kTestDataAscii); ++i) {
      composer_->ApplyTemporaryInputMode(kTestDataAscii[i].first, kCapsLocked);

      const transliteration::TransliterationType expected =
          kTestDataAscii[i].second ? transliteration::HALF_ASCII
                                   : transliteration::HIRAGANA;

      EXPECT_EQ(expected, composer_->GetInputMode()) << "index=" << i;
      EXPECT_EQ(transliteration::HIRAGANA, composer_->GetComebackInputMode())
          << "index=" << i;
    }
  }

  {  // KATAKANA_INPUT_MODE (w/o CapsLock)
    config_->set_shift_key_mode_switch(Config::KATAKANA_INPUT_MODE);

    // pair<input, use_temporary_input_mode>
    std::pair<std::string, bool> kTestDataKatakana[] = {
        std::make_pair("a", false),  std::make_pair("A", true),
        std::make_pair("a", false),  std::make_pair("a", false),
        std::make_pair("A", true),   std::make_pair("A", true),
        std::make_pair("a", false),  std::make_pair("A", true),
        std::make_pair("A", true),   std::make_pair("A", true),
        std::make_pair("a", false),  std::make_pair("A", true),
        std::make_pair(".", true),   std::make_pair("a", false),
        std::make_pair("A", true),   std::make_pair("A", true),
        std::make_pair(".", true),   std::make_pair("a", false),
        std::make_pair("あ", false), std::make_pair("a", false),
    };

    for (int i = 0; i < std::size(kTestDataKatakana); ++i) {
      composer_->ApplyTemporaryInputMode(kTestDataKatakana[i].first,
                                         kCapsUnlocked);

      const transliteration::TransliterationType expected =
          kTestDataKatakana[i].second ? transliteration::FULL_KATAKANA
                                      : transliteration::HIRAGANA;

      EXPECT_EQ(expected, composer_->GetInputMode()) << "index=" << i;
      EXPECT_EQ(transliteration::HIRAGANA, composer_->GetComebackInputMode())
          << "index=" << i;
    }
  }

  {  // KATAKANA_INPUT_MODE (w/ CapsLock)
    config_->set_shift_key_mode_switch(Config::KATAKANA_INPUT_MODE);

    // pair<input, use_temporary_input_mode>
    std::pair<std::string, bool> kTestDataKatakana[] = {
        std::make_pair("A", false),  std::make_pair("a", true),
        std::make_pair("A", false),  std::make_pair("A", false),
        std::make_pair("a", true),   std::make_pair("a", true),
        std::make_pair("A", false),  std::make_pair("a", true),
        std::make_pair("a", true),   std::make_pair("a", true),
        std::make_pair("A", false),  std::make_pair("a", true),
        std::make_pair(".", true),   std::make_pair("A", false),
        std::make_pair("a", true),   std::make_pair("a", true),
        std::make_pair(".", true),   std::make_pair("A", false),
        std::make_pair("あ", false), std::make_pair("A", false),
    };

    for (int i = 0; i < std::size(kTestDataKatakana); ++i) {
      composer_->ApplyTemporaryInputMode(kTestDataKatakana[i].first,
                                         kCapsLocked);

      const transliteration::TransliterationType expected =
          kTestDataKatakana[i].second ? transliteration::FULL_KATAKANA
                                      : transliteration::HIRAGANA;

      EXPECT_EQ(expected, composer_->GetInputMode()) << "index=" << i;
      EXPECT_EQ(transliteration::HIRAGANA, composer_->GetComebackInputMode())
          << "index=" << i;
    }
  }
}

TEST_F(ComposerTest, FullWidthCharRules_b31444698) {
  // Construct the following romaji table:
  //
  // 1<tab><tab>{?}あ<tab>NewChunk NoTransliteration
  // {?}あ1<tab><tab>{?}い<tab>
  // か<tab><tab>{?}か<tab>NewChunk NoTransliteration
  // {?}かか<tab><tab>{?}き<tab>
  constexpr int kAttrs =
      TableAttribute::NEW_CHUNK | TableAttribute::NO_TRANSLITERATION;
  table_->AddRuleWithAttributes("1", "", "{?}あ", kAttrs);
  table_->AddRule("{?}あ1", "", "{?}い");
  table_->AddRuleWithAttributes("か", "", "{?}か", kAttrs);
  table_->AddRule("{?}かか", "", "{?}き");

  // Test if "11" is transliterated to "い"
  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ("あ", GetPreedit(composer_.get()));
  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ("い", GetPreedit(composer_.get()));

  composer_->Reset();

  // b/31444698.  Test if "かか" is transliterated to "き"
  ASSERT_TRUE(InsertKeyWithMode("か", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ("か", GetPreedit(composer_.get()));
  ASSERT_TRUE(InsertKeyWithMode("か", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ("き", GetPreedit(composer_.get()));
}

TEST_F(ComposerTest, CopyFrom) {
  table_->AddRule("a", "あ", "");
  table_->AddRule("n", "ん", "");
  table_->AddRule("na", "な", "");

  {
    SCOPED_TRACE("Precomposition");

    std::string src_composition;
    composer_->GetStringForSubmission(&src_composition);
    EXPECT_EQ("", src_composition);

    Composer dest(nullptr, request_.get(), config_.get());
    dest.CopyFrom(*composer_);

    ExpectSameComposer(*composer_, dest);
  }

  {
    SCOPED_TRACE("Composition");

    composer_->InsertCharacter("a");
    composer_->InsertCharacter("n");
    std::string src_composition;
    composer_->GetStringForSubmission(&src_composition);
    EXPECT_EQ("あｎ", src_composition);

    Composer dest(nullptr, request_.get(), config_.get());
    dest.CopyFrom(*composer_);

    ExpectSameComposer(*composer_, dest);
  }

  {
    SCOPED_TRACE("Conversion");

    std::string src_composition;
    composer_->GetQueryForConversion(&src_composition);
    EXPECT_EQ("あん", src_composition);

    Composer dest(nullptr, request_.get(), config_.get());
    dest.CopyFrom(*composer_);

    ExpectSameComposer(*composer_, dest);
  }

  {
    SCOPED_TRACE("Composition with temporary input mode");

    composer_->Reset();
    InsertKey("A", composer_.get());
    InsertKey("a", composer_.get());
    InsertKey("A", composer_.get());
    InsertKey("A", composer_.get());
    InsertKey("a", composer_.get());
    std::string src_composition;
    composer_->GetStringForSubmission(&src_composition);
    EXPECT_EQ("AaAAあ", src_composition);

    Composer dest(nullptr, request_.get(), config_.get());
    dest.CopyFrom(*composer_);

    ExpectSameComposer(*composer_, dest);
  }

  {
    SCOPED_TRACE("Composition with password mode");

    composer_->Reset();
    composer_->SetInputFieldType(commands::Context::PASSWORD);
    composer_->SetInputMode(transliteration::HALF_ASCII);
    composer_->SetOutputMode(transliteration::HALF_ASCII);
    composer_->InsertCharacter("M");
    std::string src_composition;
    composer_->GetStringForSubmission(&src_composition);
    EXPECT_EQ("M", src_composition);

    Composer dest(nullptr, request_.get(), config_.get());
    dest.CopyFrom(*composer_);

    ExpectSameComposer(*composer_, dest);
  }
}

TEST_F(ComposerTest, ShiftKeyOperation) {
  commands::KeyEvent key;
  table_->AddRule("a", "あ", "");

  {  // Basic feature.
    composer_->Reset();
    InsertKey("a", composer_.get());  // "あ"
    InsertKey("A", composer_.get());  // "あA"
    InsertKey("a", composer_.get());  // "あAa"
    // Shift reverts the input mode to Hiragana.
    InsertKey("Shift", composer_.get());
    InsertKey("a", composer_.get());  // "あAaあ"
    // Shift does nothing because the input mode has already been reverted.
    InsertKey("Shift", composer_.get());
    InsertKey("a", composer_.get());  // "あAaああ"

    std::string preedit;
    composer_->GetStringForPreedit(&preedit);
    EXPECT_EQ("あAaああ", preedit);
  }

  {  // Revert back to the previous input mode.
    composer_->SetInputMode(transliteration::FULL_KATAKANA);
    composer_->Reset();
    InsertKey("a", composer_.get());  // "ア"
    InsertKey("A", composer_.get());  // "アA"
    InsertKey("a", composer_.get());  // "アAa"
    // Shift reverts the input mode to Hiragana.
    InsertKey("Shift", composer_.get());
    InsertKey("a", composer_.get());  // "アAaア"
    // Shift does nothing because the input mode has already been reverted.
    InsertKey("Shift", composer_.get());
    InsertKey("a", composer_.get());  // "アAaアア"

    std::string preedit;
    composer_->GetStringForPreedit(&preedit);
    EXPECT_EQ("アAaアア", preedit);
    EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());
  }

  {  // Multiple shifted characters
    composer_->SetInputMode(transliteration::HIRAGANA);
    composer_->Reset();
    // Sequential shfited keys change the behavior of the next
    // non-shifted key.  "AAaa" Should become "AAああ", "Aaa" should
    // become "Aaa".
    InsertKey("A", composer_.get());  // "A"
    InsertKey("A", composer_.get());  // "AA"
    InsertKey("a", composer_.get());  // "AAあ"
    InsertKey("A", composer_.get());  // "AAあA"
    InsertKey("a", composer_.get());  // "AAあAa"

    std::string preedit;
    composer_->GetStringForPreedit(&preedit);
    EXPECT_EQ("AAあAa", preedit);
  }

  {  // Multiple shifted characters #2
    composer_->SetInputMode(transliteration::HIRAGANA);
    composer_->Reset();
    InsertKey("D", composer_.get());  // "D"
    InsertKey("&", composer_.get());  // "D&"
    InsertKey("D", composer_.get());  // "D&D"
    InsertKey("2", composer_.get());  // "D&D2"
    InsertKey("a", composer_.get());  // "D&D2a"

    std::string preedit;
    composer_->GetStringForPreedit(&preedit);
    EXPECT_EQ("D&D2a", preedit);
  }

  {  // Full-witdh alphanumeric
    composer_->SetInputMode(transliteration::FULL_ASCII);
    composer_->Reset();
    InsertKey("A", composer_.get());  // "Ａ"
    InsertKey("a", composer_.get());  // "Ａａ"

    std::string preedit;
    composer_->GetStringForPreedit(&preedit);
    EXPECT_EQ("Ａａ", preedit);
  }

  {  // Half-witdh alphanumeric
    composer_->SetInputMode(transliteration::HALF_ASCII);
    composer_->Reset();
    InsertKey("A", composer_.get());  // "A"
    InsertKey("a", composer_.get());  // "Aa"

    std::string preedit;
    composer_->GetStringForPreedit(&preedit);
    EXPECT_EQ("Aa", preedit);
  }
}

TEST_F(ComposerTest, ShiftKeyOperationForKatakana) {
  config_->set_shift_key_mode_switch(Config::KATAKANA_INPUT_MODE);
  table_->InitializeWithRequestAndConfig(*request_, *config_,
                                         mock_data_manager_);
  composer_->Reset();
  composer_->SetInputMode(transliteration::HIRAGANA);
  InsertKey("K", composer_.get());
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());
  InsertKey("A", composer_.get());
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());
  InsertKey("T", composer_.get());
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());
  InsertKey("a", composer_.get());
  // See the below comment.
  // EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());
  EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());
  InsertKey("k", composer_.get());
  EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());
  InsertKey("A", composer_.get());
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());
  InsertKey("n", composer_.get());
  EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());
  InsertKey("a", composer_.get());
  EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());

  std::string preedit;
  composer_->GetStringForPreedit(&preedit);
  // NOTE(komatsu): "KATakAna" is converted to "カＴあｋアな" rather
  // than "カタカな".  This is a different behavior from Kotoeri due
  // to avoid complecated implementation.  Unless this is a problem
  // for users, this difference probably remains.
  //
  // EXPECT_EQ("カタカな", preedit);

  EXPECT_EQ("カＴあｋアな", preedit);
}

TEST_F(ComposerTest, AutoIMETurnOffEnabled) {
  config_->set_preedit_method(Config::ROMAN);
  config_->set_use_auto_ime_turn_off(true);

  table_->InitializeWithRequestAndConfig(*request_, *config_,
                                         mock_data_manager_);

  commands::KeyEvent key;

  {  // http
    InsertKey("h", composer_.get());
    InsertKey("t", composer_.get());
    InsertKey("t", composer_.get());
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());
    InsertKey("p", composer_.get());

    std::string preedit;
    composer_->GetStringForPreedit(&preedit);
    EXPECT_EQ("http", preedit);
    EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());

    composer_->Reset();
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());
  }

  composer_ =
      absl::make_unique<Composer>(table_.get(), request_.get(), config_.get());

  {  // google
    InsertKey("g", composer_.get());
    InsertKey("o", composer_.get());
    InsertKey("o", composer_.get());
    InsertKey("g", composer_.get());
    InsertKey("l", composer_.get());
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());
    InsertKey("e", composer_.get());
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());
    EXPECT_EQ("google", GetPreedit(composer_.get()));

    InsertKey("a", composer_.get());
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());
    EXPECT_EQ("googleあ", GetPreedit(composer_.get()));

    composer_->Reset();
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());
  }

  {  // google in full-width alphanumeric mode.
    composer_->SetInputMode(transliteration::FULL_ASCII);
    InsertKey("g", composer_.get());
    InsertKey("o", composer_.get());
    InsertKey("o", composer_.get());
    InsertKey("g", composer_.get());
    InsertKey("l", composer_.get());
    EXPECT_EQ(transliteration::FULL_ASCII, composer_->GetInputMode());
    InsertKey("e", composer_.get());
    EXPECT_EQ(transliteration::FULL_ASCII, composer_->GetInputMode());

    EXPECT_EQ("ｇｏｏｇｌｅ", GetPreedit(composer_.get()));

    InsertKey("a", composer_.get());
    EXPECT_EQ(transliteration::FULL_ASCII, composer_->GetInputMode());
    EXPECT_EQ("ｇｏｏｇｌｅａ", GetPreedit(composer_.get()));

    composer_->Reset();
    EXPECT_EQ(transliteration::FULL_ASCII, composer_->GetInputMode());
    // Reset to Hiragana mode
    composer_->SetInputMode(transliteration::HIRAGANA);
  }

  {  // Google
    InsertKey("G", composer_.get());
    EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());
    InsertKey("o", composer_.get());
    InsertKey("o", composer_.get());
    InsertKey("g", composer_.get());
    InsertKey("l", composer_.get());
    EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());
    InsertKey("e", composer_.get());
    EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());
    EXPECT_EQ("Google", GetPreedit(composer_.get()));

    InsertKey("a", composer_.get());
    EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());
    EXPECT_EQ("Googlea", GetPreedit(composer_.get()));

    composer_->Reset();
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());
  }

  config_->set_shift_key_mode_switch(Config::OFF);
  composer_ =
      absl::make_unique<Composer>(table_.get(), request_.get(), config_.get());

  {  // Google
    InsertKey("G", composer_.get());
    InsertKey("o", composer_.get());
    InsertKey("o", composer_.get());
    InsertKey("g", composer_.get());
    InsertKey("l", composer_.get());
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());
    InsertKey("e", composer_.get());
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());
    EXPECT_EQ("Google", GetPreedit(composer_.get()));

    InsertKey("a", composer_.get());
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());
    EXPECT_EQ("Googleあ", GetPreedit(composer_.get()));

    composer_->Reset();
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());
  }
}

TEST_F(ComposerTest, AutoIMETurnOffDisabled) {
  config_->set_preedit_method(Config::ROMAN);
  config_->set_use_auto_ime_turn_off(false);

  table_->InitializeWithRequestAndConfig(*request_, *config_,
                                         mock_data_manager_);

  commands::KeyEvent key;

  // Roman
  key.set_key_code('h');
  composer_->InsertCharacterKeyEvent(key);

  key.set_key_code('t');
  composer_->InsertCharacterKeyEvent(key);

  key.set_key_code('t');
  composer_->InsertCharacterKeyEvent(key);

  key.set_key_code('p');
  composer_->InsertCharacterKeyEvent(key);

  key.set_key_code(':');
  composer_->InsertCharacterKeyEvent(key);

  key.set_key_code('/');
  composer_->InsertCharacterKeyEvent(key);

  key.set_key_code('/');
  composer_->InsertCharacterKeyEvent(key);

  std::string preedit;
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("ｈっｔｐ：・・", preedit);
}

TEST_F(ComposerTest, AutoIMETurnOffKana) {
  config_->set_preedit_method(Config::KANA);
  config_->set_use_auto_ime_turn_off(true);

  table_->InitializeWithRequestAndConfig(*request_, *config_,
                                         mock_data_manager_);

  commands::KeyEvent key;

  // Kana
  key.set_key_code('h');
  composer_->InsertCharacterKeyEvent(key);

  key.set_key_code('t');
  composer_->InsertCharacterKeyEvent(key);

  key.set_key_code('t');
  composer_->InsertCharacterKeyEvent(key);

  key.set_key_code('p');
  composer_->InsertCharacterKeyEvent(key);

  key.set_key_code(':');
  composer_->InsertCharacterKeyEvent(key);

  key.set_key_code('/');
  composer_->InsertCharacterKeyEvent(key);

  key.set_key_code('/');
  composer_->InsertCharacterKeyEvent(key);

  std::string preedit;
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("ｈっｔｐ：・・", preedit);
}

TEST_F(ComposerTest, KanaPrediction) {
  composer_->InsertCharacterKeyAndPreedit("t", "か");
  {
    std::string preedit;
    composer_->GetQueryForPrediction(&preedit);
    EXPECT_EQ("か", preedit);
  }
  composer_->InsertCharacterKeyAndPreedit("\\", "ー");
  {
    std::string preedit;
    composer_->GetQueryForPrediction(&preedit);
    EXPECT_EQ("かー", preedit);
  }
  composer_->InsertCharacterKeyAndPreedit(",", "、");
  {
    std::string preedit;
    composer_->GetQueryForPrediction(&preedit);
    EXPECT_EQ("かー、", preedit);
  }
}

TEST_F(ComposerTest, KanaTransliteration) {
  table_->AddRule("く゛", "ぐ", "");
  composer_->InsertCharacterKeyAndPreedit("h", "く");
  composer_->InsertCharacterKeyAndPreedit("e", "い");
  composer_->InsertCharacterKeyAndPreedit("l", "り");
  composer_->InsertCharacterKeyAndPreedit("l", "り");
  composer_->InsertCharacterKeyAndPreedit("o", "ら");

  std::string preedit;
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("くいりりら", preedit);

  transliteration::Transliterations transliterations;
  composer_->GetTransliterations(&transliterations);
  EXPECT_EQ(transliteration::NUM_T13N_TYPES, transliterations.size());
  EXPECT_EQ("hello", transliterations[transliteration::HALF_ASCII]);
}

TEST_F(ComposerTest, SetOutputMode) {
  table_->AddRule("mo", "も", "");
  table_->AddRule("zu", "ず", "");

  composer_->InsertCharacter("m");
  composer_->InsertCharacter("o");
  composer_->InsertCharacter("z");
  composer_->InsertCharacter("u");

  std::string output;
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ("もず", output);
  EXPECT_EQ(2, composer_->GetCursor());

  composer_->SetOutputMode(transliteration::HALF_ASCII);
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ("mozu", output);
  EXPECT_EQ(4, composer_->GetCursor());

  composer_->SetOutputMode(transliteration::HALF_KATAKANA);
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ("ﾓｽﾞ", output);
  EXPECT_EQ(3, composer_->GetCursor());
}

TEST_F(ComposerTest, UpdateInputMode) {
  table_->AddRule("a", "あ", "");
  table_->AddRule("i", "い", "");

  InsertKey("A", composer_.get());
  EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());

  InsertKey("I", composer_.get());
  EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());

  InsertKey("a", composer_.get());
  EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());

  InsertKey("i", composer_.get());
  EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());

  composer_->SetInputMode(transliteration::FULL_ASCII);
  InsertKey("a", composer_.get());
  EXPECT_EQ(transliteration::FULL_ASCII, composer_->GetInputMode());

  InsertKey("i", composer_.get());
  EXPECT_EQ(transliteration::FULL_ASCII, composer_->GetInputMode());

  std::string output;
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ("AIあいａｉ", output);

  composer_->SetInputMode(transliteration::FULL_KATAKANA);

  // "|AIあいａｉ"
  composer_->MoveCursorToBeginning();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  // "A|Iあいａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());

  // "AI|あいａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  // "AIあ|いａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());

  // "AIあい|ａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  // "AIあいａ|ｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(transliteration::FULL_ASCII, composer_->GetInputMode());

  // "AIあいａｉ|"
  composer_->MoveCursorRight();
  EXPECT_EQ(transliteration::FULL_ASCII, composer_->GetInputMode());

  // "AIあいａ|ｉ"
  composer_->MoveCursorLeft();
  EXPECT_EQ(transliteration::FULL_ASCII, composer_->GetInputMode());

  // "|AIあいａｉ"
  composer_->MoveCursorToBeginning();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  // "A|Iあいａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());

  // "A|あいａｉ"
  composer_->Delete();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  // "Aあ|いａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());

  // "A|いａｉ"
  composer_->Backspace();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  // "Aいａｉ|"
  composer_->MoveCursorToEnd();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  // "Aいａ|ｉ"
  composer_->MoveCursorLeft();
  EXPECT_EQ(transliteration::FULL_ASCII, composer_->GetInputMode());

  // "Aいａｉ|"
  composer_->MoveCursorToEnd();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());
}

TEST_F(ComposerTest, DisabledUpdateInputMode) {
  // Set the flag disable.
  commands::Request request;
  request.set_update_input_mode_from_surrounding_text(false);
  composer_->SetRequest(&request);

  table_->AddRule("a", "あ", "");
  table_->AddRule("i", "い", "");

  InsertKey("A", composer_.get());
  EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());

  InsertKey("I", composer_.get());
  EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());

  InsertKey("a", composer_.get());
  EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());

  InsertKey("i", composer_.get());
  EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());

  composer_->SetInputMode(transliteration::FULL_ASCII);
  InsertKey("a", composer_.get());
  EXPECT_EQ(transliteration::FULL_ASCII, composer_->GetInputMode());

  InsertKey("i", composer_.get());
  EXPECT_EQ(transliteration::FULL_ASCII, composer_->GetInputMode());

  std::string output;
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ("AIあいａｉ", output);

  composer_->SetInputMode(transliteration::FULL_KATAKANA);

  // Use same scenario as above test case, but the result of GetInputMode
  // should be always FULL_KATAKANA regardless of the surrounding text.

  // "|AIあいａｉ"
  composer_->MoveCursorToBeginning();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  // "A|Iあいａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  // "AI|あいａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  // "AIあ|いａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  // "AIあい|ａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  // "AIあいａ|ｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  // "AIあいａｉ|"
  composer_->MoveCursorRight();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  // "AIあいａ|ｉ"
  composer_->MoveCursorLeft();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  // "|AIあいａｉ"
  composer_->MoveCursorToBeginning();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  // "A|Iあいａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  // "A|あいａｉ"
  composer_->Delete();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  // "Aあ|いａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  // "A|いａｉ"
  composer_->Backspace();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  // "Aいａｉ|"
  composer_->MoveCursorToEnd();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  // "Aいａ|ｉ"
  composer_->MoveCursorLeft();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  // "Aいａｉ|"
  composer_->MoveCursorToEnd();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());
}

TEST_F(ComposerTest, TransformCharactersForNumbers) {
  std::string query;

  query = "";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  query = "R2D2";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  query = "ー１";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ("−１", query);  // The hyphen is U+2212.

  query = "ーー１";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  query = "ー";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  query = "ーー";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  query = "ーーーーー";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  query = "ｗ";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  query = "ーｗ";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  query = "ーーｗ";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  query = "@";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  query = "ー@";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  query = "ーー@";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  query = "＠";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  query = "ー＠";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  query = "ーー＠";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  query = "まじかー１";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  query = "まじかーｗ";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  query = "１、０";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ("１，０", query);

  query = "０。５";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ("０．５", query);

  query = "ー１、０００。５";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ("−１，０００．５", query);  // The hyphen is U+2212.

  query = "０３ー";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ("０３−", query);  // The hyphen is U+2212.

  query = "０３ーーーーー";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ("０３−−−−−", query);  // The hyphen is U+2212.

  query = "ｘー（ー１）＞ーｘ";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ("ｘ−（−１）＞−ｘ", query);  // The hyphen is U+2212.

  query = "１＊ー２／ー３ーー４";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ("１＊−２／−３−−４", query);  // The hyphen is U+2212.

  query = "ＡーＺ";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ("Ａ−Ｚ", query);  // The hyphen is U+2212.

  query = "もずく、うぉーきんぐ。";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  query = "えー２、９８０円！月々たった、２、９８０円？";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ("えー２，９８０円！月々たった、２，９８０円？", query);

  query = "およそ、３。１４１５９。";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ("およそ、３．１４１５９．", query);

  query = "１００、";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ("１００，", query);

  query = "１００。";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ("１００．", query);
}

TEST_F(ComposerTest, PreeditFormAfterCharacterTransform) {
  CharacterFormManager *manager =
      CharacterFormManager::GetCharacterFormManager();
  table_->AddRule("0", "０", "");
  table_->AddRule("1", "１", "");
  table_->AddRule("2", "２", "");
  table_->AddRule("3", "３", "");
  table_->AddRule("4", "４", "");
  table_->AddRule("5", "５", "");
  table_->AddRule("6", "６", "");
  table_->AddRule("7", "７", "");
  table_->AddRule("8", "８", "");
  table_->AddRule("9", "９", "");
  table_->AddRule("-", "ー", "");
  table_->AddRule(",", "、", "");
  table_->AddRule(".", "。", "");

  {
    composer_->Reset();
    manager->SetDefaultRule();
    manager->AddPreeditRule("1", Config::HALF_WIDTH);
    manager->AddPreeditRule(".,", Config::HALF_WIDTH);
    composer_->InsertCharacter("3.14");
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("3.14", result);
  }

  {
    composer_->Reset();
    manager->SetDefaultRule();
    manager->AddPreeditRule("1", Config::FULL_WIDTH);
    manager->AddPreeditRule(".,", Config::HALF_WIDTH);
    composer_->InsertCharacter("3.14");
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("３.１４", result);
  }

  {
    composer_->Reset();
    manager->SetDefaultRule();
    manager->AddPreeditRule("1", Config::HALF_WIDTH);
    manager->AddPreeditRule(".,", Config::FULL_WIDTH);
    composer_->InsertCharacter("3.14");
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("3．14", result);
  }

  {
    composer_->Reset();
    manager->SetDefaultRule();
    manager->AddPreeditRule("1", Config::FULL_WIDTH);
    manager->AddPreeditRule(".,", Config::FULL_WIDTH);
    composer_->InsertCharacter("3.14");
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("３．１４", result);
  }
}

TEST_F(ComposerTest, ComposingWithcharactertransform) {
  table_->AddRule("0", "０", "");
  table_->AddRule("1", "１", "");
  table_->AddRule("2", "２", "");
  table_->AddRule("3", "３", "");
  table_->AddRule("4", "４", "");
  table_->AddRule("5", "５", "");
  table_->AddRule("6", "６", "");
  table_->AddRule("7", "７", "");
  table_->AddRule("8", "８", "");
  table_->AddRule("9", "９", "");
  table_->AddRule("-", "ー", "");
  table_->AddRule(",", "、", "");
  table_->AddRule(".", "。", "");
  composer_->InsertCharacter("-1,000.5");

  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("−１，０００．５", result);  // The hyphen is U+2212.
  }
  {
    std::string result;
    composer_->GetStringForSubmission(&result);
    EXPECT_EQ("−１，０００．５", result);  // The hyphen is U+2212.
  }
  {
    std::string result;
    composer_->GetQueryForConversion(&result);
    EXPECT_EQ("-1,000.5", result);
  }
  {
    std::string result;
    composer_->GetQueryForPrediction(&result);
    EXPECT_EQ("-1,000.5", result);
  }
  {
    std::string left, focused, right;
    // Right edge
    composer_->GetPreedit(&left, &focused, &right);
    EXPECT_EQ("−１，０００．５", left);  // The hyphen is U+2212.
    EXPECT_TRUE(focused.empty());
    EXPECT_TRUE(right.empty());

    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    EXPECT_EQ("−１，０００．", left);  // The hyphen is U+2212.
    EXPECT_EQ("５", focused);
    EXPECT_TRUE(right.empty());

    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    EXPECT_EQ("−１，０００", left);  // The hyphen is U+2212.
    EXPECT_EQ("．", focused);
    EXPECT_EQ("５", right);

    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    EXPECT_EQ("−１，００", left);  // The hyphen is U+2212.
    EXPECT_EQ("０", focused);
    EXPECT_EQ("．５", right);

    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    EXPECT_EQ("−１，０", left);  // The hyphen is U+2212.
    EXPECT_EQ("０", focused);
    EXPECT_EQ("０．５", right);

    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    EXPECT_EQ("−１，", left);  // The hyphen is U+2212.
    EXPECT_EQ("０", focused);
    EXPECT_EQ("００．５", right);

    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    EXPECT_EQ("−１", left);
    EXPECT_EQ("，", focused);
    EXPECT_EQ("０００．５", right);

    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    EXPECT_EQ("−", left);  // U+2212.
    EXPECT_EQ("１", focused);
    EXPECT_EQ("，０００．５", right);

    // Left edge
    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    EXPECT_TRUE(left.empty());
    EXPECT_EQ("−", focused);  // U+2212.
    EXPECT_EQ("１，０００．５", right);
  }
}

TEST_F(ComposerTest, AlphanumericOfSSH) {
  // This is a unittest against http://b/3199626
  // 'ssh' (っｓｈ) + F10 should be 'ssh'.
  table_->AddRule("ss", "[X]", "s");
  table_->AddRule("sha", "[SHA]", "");
  composer_->InsertCharacter("ssh");
  EXPECT_EQ("［Ｘ］ｓｈ", GetPreedit(composer_.get()));

  std::string query;
  composer_->GetQueryForConversion(&query);
  EXPECT_EQ("[X]sh", query);

  transliteration::Transliterations t13ns;
  composer_->GetTransliterations(&t13ns);
  EXPECT_EQ("ssh", t13ns[transliteration::HALF_ASCII]);
}

TEST_F(ComposerTest, Issue2190364) {
  // This is a unittest against http://b/2190364
  commands::KeyEvent key;
  key.set_key_code('a');
  key.set_key_string("ち");

  // Toggle the input mode to HALF_ASCII
  composer_->ToggleInputMode();
  EXPECT_TRUE(composer_->InsertCharacterKeyEvent(key));
  std::string output;
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ("a", output);

  // Insertion of a space and backspace it should not change the composition.
  composer_->InsertCharacter(" ");
  output.clear();
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ("a ", output);

  composer_->Backspace();
  output.clear();
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ("a", output);

  // Toggle the input mode to HIRAGANA, the preedit should not be changed.
  composer_->ToggleInputMode();
  output.clear();
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ("a", output);

  // "a" should be converted to "ち" on Hiragana input mode.
  EXPECT_TRUE(composer_->InsertCharacterKeyEvent(key));
  output.clear();
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ("aち", output);
}

TEST_F(ComposerTest, Issue1817410) {
  // This is a unittest against http://b/2190364
  table_->AddRule("ss", "っ", "s");

  InsertKey("s", composer_.get());
  InsertKey("s", composer_.get());

  std::string preedit;
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ("っｓ", preedit);

  std::string t13n;
  composer_->GetSubTransliteration(transliteration::HALF_ASCII, 0, 2, &t13n);
  EXPECT_EQ("ss", t13n);

  t13n.clear();
  composer_->GetSubTransliteration(transliteration::HALF_ASCII, 0, 1, &t13n);
  EXPECT_EQ("s", t13n);

  t13n.clear();
  composer_->GetSubTransliteration(transliteration::HALF_ASCII, 1, 1, &t13n);
  EXPECT_EQ("s", t13n);
}

TEST_F(ComposerTest, Issue2272745) {
  // This is a unittest against http://b/2272745.
  // A temporary input mode remains when a composition is canceled.
  {
    InsertKey("G", composer_.get());
    EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());

    composer_->Backspace();
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());
  }
  composer_->Reset();
  {
    InsertKey("G", composer_.get());
    EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());

    composer_->EditErase();
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());
  }
}

TEST_F(ComposerTest, Isue2555503) {
  // This is a unittest against http://b/2555503.
  // Mode respects the previous character too much.
  InsertKey("a", composer_.get());
  composer_->SetInputMode(transliteration::FULL_KATAKANA);
  InsertKey("i", composer_.get());
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

  composer_->Backspace();
  EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());
}

TEST_F(ComposerTest, Issue2819580_1) {
  // This is a unittest against http://b/2819580.
  // 'y' after 'n' disappears.
  table_->AddRule("n", "ん", "");
  table_->AddRule("na", "な", "");
  table_->AddRule("ya", "や", "");
  table_->AddRule("nya", "にゃ", "");

  InsertKey("n", composer_.get());
  InsertKey("y", composer_.get());

  std::string result;
  composer_->GetQueryForConversion(&result);
  EXPECT_EQ("んy", result);
}

TEST_F(ComposerTest, Issue2819580_2) {
  // This is a unittest against http://b/2819580.
  // 'y' after 'n' disappears.
  table_->AddRule("po", "ぽ", "");
  table_->AddRule("n", "ん", "");
  table_->AddRule("na", "な", "");
  table_->AddRule("ya", "や", "");
  table_->AddRule("nya", "にゃ", "");

  InsertKey("p", composer_.get());
  InsertKey("o", composer_.get());
  InsertKey("n", composer_.get());
  InsertKey("y", composer_.get());

  std::string result;
  composer_->GetQueryForConversion(&result);
  EXPECT_EQ("ぽんy", result);
}

TEST_F(ComposerTest, Issue2819580_3) {
  // This is a unittest against http://b/2819580.
  // 'y' after 'n' disappears.
  table_->AddRule("n", "ん", "");
  table_->AddRule("na", "な", "");
  table_->AddRule("ya", "や", "");
  table_->AddRule("nya", "にゃ", "");

  InsertKey("z", composer_.get());
  InsertKey("n", composer_.get());
  InsertKey("y", composer_.get());

  std::string result;
  composer_->GetQueryForConversion(&result);
  EXPECT_EQ("zんy", result);
}

TEST_F(ComposerTest, Issue2797991_1) {
  // This is a unittest against http://b/2797991.
  // Half-width alphanumeric mode quits after [CAPITAL LETTER]:[CAPITAL LETTER]
  // e.g. C:\Wi -> C:\Wい

  table_->AddRule("i", "い", "");

  InsertKey("C", composer_.get());
  InsertKey(":", composer_.get());
  InsertKey("\\", composer_.get());
  InsertKey("W", composer_.get());
  InsertKey("i", composer_.get());

  std::string result;
  composer_->GetStringForPreedit(&result);
  EXPECT_EQ("C:\\Wi", result);
}

TEST_F(ComposerTest, Issue2797991_2) {
  // This is a unittest against http://b/2797991.
  // Half-width alphanumeric mode quits after [CAPITAL LETTER]:[CAPITAL LETTER]
  // e.g. C:\Wi -> C:\Wい

  table_->AddRule("i", "い", "");

  InsertKey("C", composer_.get());
  InsertKey(":", composer_.get());
  InsertKey("W", composer_.get());
  InsertKey("i", composer_.get());

  std::string result;
  composer_->GetStringForPreedit(&result);
  EXPECT_EQ("C:Wi", result);
}

TEST_F(ComposerTest, Issue2797991_3) {
  // This is a unittest against http://b/2797991.
  // Half-width alphanumeric mode quits after [CAPITAL LETTER]:[CAPITAL LETTER]
  // e.g. C:\Wi -> C:\Wい

  table_->AddRule("i", "い", "");

  InsertKey("C", composer_.get());
  InsertKey(":", composer_.get());
  InsertKey("\\", composer_.get());
  InsertKey("W", composer_.get());
  InsertKey("i", composer_.get());
  InsertKeyWithMode("i", commands::HIRAGANA, composer_.get());
  std::string result;
  composer_->GetStringForPreedit(&result);
  EXPECT_EQ("C:\\Wiい", result);
}

TEST_F(ComposerTest, Issue2797991_4) {
  // This is a unittest against http://b/2797991.
  // Half-width alphanumeric mode quits after [CAPITAL LETTER]:[CAPITAL LETTER]
  // e.g. C:\Wi -> C:\Wい

  table_->AddRule("i", "い", "");

  InsertKey("c", composer_.get());
  InsertKey(":", composer_.get());
  InsertKey("\\", composer_.get());
  InsertKey("W", composer_.get());
  InsertKey("i", composer_.get());

  std::string result;
  composer_->GetStringForPreedit(&result);
  EXPECT_EQ("c:\\Wi", result);
}

TEST_F(ComposerTest, CaseSensitiveByConfiguration) {
  {
    config_->set_shift_key_mode_switch(Config::OFF);
    table_->InitializeWithRequestAndConfig(*request_, *config_,
                                           mock_data_manager_);

    table_->AddRule("i", "い", "");
    table_->AddRule("I", "イ", "");

    InsertKey("i", composer_.get());
    InsertKey("I", composer_.get());
    InsertKey("i", composer_.get());
    InsertKey("I", composer_.get());
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("いイいイ", result);
  }
  composer_->Reset();
  {
    config_->set_shift_key_mode_switch(Config::ASCII_INPUT_MODE);
    table_->InitializeWithRequestAndConfig(*request_, *config_,
                                           mock_data_manager_);

    table_->AddRule("i", "い", "");
    table_->AddRule("I", "イ", "");

    InsertKey("i", composer_.get());
    InsertKey("I", composer_.get());
    InsertKey("i", composer_.get());
    InsertKey("I", composer_.get());
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("いIiI", result);
  }
}

TEST_F(ComposerTest,
       InputUppercaseInAlphanumericModeWithShiftKeyModeSwitchIsKatakana) {
  {
    config_->set_shift_key_mode_switch(Config::KATAKANA_INPUT_MODE);
    table_->InitializeWithRequestAndConfig(*request_, *config_,
                                           mock_data_manager_);

    table_->AddRule("i", "い", "");
    table_->AddRule("I", "イ", "");

    {
      composer_->Reset();
      composer_->SetInputMode(transliteration::FULL_ASCII);
      InsertKey("I", composer_.get());
      std::string result;
      composer_->GetStringForPreedit(&result);
      EXPECT_EQ("Ｉ", result);
    }

    {
      composer_->Reset();
      composer_->SetInputMode(transliteration::HALF_ASCII);
      InsertKey("I", composer_.get());
      std::string result;
      composer_->GetStringForPreedit(&result);
      EXPECT_EQ("I", result);
    }

    {
      composer_->Reset();
      composer_->SetInputMode(transliteration::FULL_KATAKANA);
      InsertKey("I", composer_.get());
      std::string result;
      composer_->GetStringForPreedit(&result);
      EXPECT_EQ("イ", result);
    }

    {
      composer_->Reset();
      composer_->SetInputMode(transliteration::HALF_KATAKANA);
      InsertKey("I", composer_.get());
      std::string result;
      composer_->GetStringForPreedit(&result);
      EXPECT_EQ("ｲ", result);
    }

    {
      composer_->Reset();
      composer_->SetInputMode(transliteration::HIRAGANA);
      InsertKey("I", composer_.get());
      std::string result;
      composer_->GetStringForPreedit(&result);
      EXPECT_EQ("イ", result);
    }
  }
}

TEST_F(ComposerTest, DeletingAlphanumericPartShouldQuitToggleAlphanumericMode) {
  // http://b/2206560
  // 1. Type "iGoogle" (preedit text turns to be "いGoogle")
  // 2. Type Back-space 6 times ("い")
  // 3. Type "i" (should be "いい")

  table_->InitializeWithRequestAndConfig(*request_, *config_,
                                         mock_data_manager_);

  table_->AddRule("i", "い", "");

  InsertKey("i", composer_.get());
  InsertKey("G", composer_.get());
  InsertKey("o", composer_.get());
  InsertKey("o", composer_.get());
  InsertKey("g", composer_.get());
  InsertKey("l", composer_.get());
  InsertKey("e", composer_.get());

  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("いGoogle", result);
  }

  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();

  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("い", result);
  }

  InsertKey("i", composer_.get());

  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("いい", result);
  }
}

TEST_F(ComposerTest, InputModesChangeWhenCursorMoves) {
  // The expectation of this test is the same as MS-IME's

  table_->InitializeWithRequestAndConfig(*request_, *config_,
                                         mock_data_manager_);

  table_->AddRule("i", "い", "");
  table_->AddRule("gi", "ぎ", "");

  InsertKey("i", composer_.get());
  composer_->MoveCursorRight();
  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("い", result);
  }

  composer_->MoveCursorLeft();
  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("い", result);
  }

  InsertKey("G", composer_.get());
  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("Gい", result);
  }

  composer_->MoveCursorRight();
  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("Gい", result);
  }

  InsertKey("G", composer_.get());
  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("GいG", result);
  }

  composer_->MoveCursorLeft();
  InsertKey("i", composer_.get());
  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("GいいG", result);
  }

  composer_->MoveCursorRight();
  InsertKey("i", composer_.get());
  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("GいいGi", result);
  }

  InsertKey("G", composer_.get());
  InsertKey("i", composer_.get());
  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("GいいGiGi", result);
  }

  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  InsertKey("i", composer_.get());
  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("GいいGi", result);
  }

  InsertKey("G", composer_.get());
  InsertKey("G", composer_.get());
  composer_->MoveCursorRight();
  InsertKey("i", composer_.get());
  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("GいいGiGGi", result);
  }

  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  InsertKey("i", composer_.get());
  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("Gい", result);
  }

  composer_->Backspace();
  composer_->MoveCursorLeft();
  composer_->MoveCursorRight();
  InsertKey("i", composer_.get());
  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("Gi", result);
  }
}

TEST_F(ComposerTest, ShouldCommit) {
  table_->AddRuleWithAttributes("ka", "[KA]", "", DIRECT_INPUT);
  table_->AddRuleWithAttributes("tt", "[X]", "t", DIRECT_INPUT);
  table_->AddRuleWithAttributes("ta", "[TA]", "", NO_TABLE_ATTRIBUTE);

  composer_->InsertCharacter("k");
  EXPECT_FALSE(composer_->ShouldCommit());

  composer_->InsertCharacter("a");
  EXPECT_TRUE(composer_->ShouldCommit());

  composer_->InsertCharacter("t");
  EXPECT_FALSE(composer_->ShouldCommit());

  composer_->InsertCharacter("t");
  EXPECT_FALSE(composer_->ShouldCommit());

  composer_->InsertCharacter("a");
  EXPECT_TRUE(composer_->ShouldCommit());

  composer_->InsertCharacter("t");
  EXPECT_FALSE(composer_->ShouldCommit());

  composer_->InsertCharacter("a");
  EXPECT_FALSE(composer_->ShouldCommit());
}

TEST_F(ComposerTest, ShouldCommitHead) {
  struct TestData {
    const std::string input_text;
    const commands::Context::InputFieldType field_type;
    const bool expected_return;
    const size_t expected_commit_length;
    TestData(const std::string &input_text,
             commands::Context::InputFieldType field_type, bool expected_return,
             size_t expected_commit_length)
        : input_text(input_text),
          field_type(field_type),
          expected_return(expected_return),
          expected_commit_length(expected_commit_length) {}
  };
  const TestData test_data_list[] = {
      // On NORMAL, never commit the head.
      TestData("", commands::Context::NORMAL, false, 0),
      TestData("A", commands::Context::NORMAL, false, 0),
      TestData("AB", commands::Context::NORMAL, false, 0),
      TestData("", commands::Context::PASSWORD, false, 0),
      // On PASSWORD, commit (length - 1) characters.
      TestData("A", commands::Context::PASSWORD, false, 0),
      TestData("AB", commands::Context::PASSWORD, true, 1),
      TestData("ABCDEFGHI", commands::Context::PASSWORD, true, 8),
      // On NUMBER and TEL, commit (length) characters.
      TestData("", commands::Context::NUMBER, false, 0),
      TestData("A", commands::Context::NUMBER, true, 1),
      TestData("AB", commands::Context::NUMBER, true, 2),
      TestData("ABCDEFGHI", commands::Context::NUMBER, true, 9),
      TestData("", commands::Context::TEL, false, 0),
      TestData("A", commands::Context::TEL, true, 1),
      TestData("AB", commands::Context::TEL, true, 2),
      TestData("ABCDEFGHI", commands::Context::TEL, true, 9),
  };

  for (size_t i = 0; i < std::size(test_data_list); ++i) {
    const TestData &test_data = test_data_list[i];
    SCOPED_TRACE(test_data.input_text);
    SCOPED_TRACE(test_data.field_type);
    SCOPED_TRACE(test_data.expected_return);
    SCOPED_TRACE(test_data.expected_commit_length);
    composer_->Reset();
    composer_->SetInputFieldType(test_data.field_type);
    composer_->InsertCharacter(test_data.input_text);
    size_t length_to_commit;
    const bool result = composer_->ShouldCommitHead(&length_to_commit);
    if (test_data.expected_return) {
      EXPECT_TRUE(result);
      EXPECT_EQ(test_data.expected_commit_length, length_to_commit);
    } else {
      EXPECT_FALSE(result);
    }
  }
}

TEST_F(ComposerTest, CursorMovements) {
  composer_->InsertCharacter("mozuku");
  EXPECT_EQ(6, composer_->GetLength());
  EXPECT_EQ(6, composer_->GetCursor());

  composer_->MoveCursorRight();
  EXPECT_EQ(6, composer_->GetCursor());
  composer_->MoveCursorLeft();
  EXPECT_EQ(5, composer_->GetCursor());

  composer_->MoveCursorToBeginning();
  EXPECT_EQ(0, composer_->GetCursor());
  composer_->MoveCursorLeft();
  EXPECT_EQ(0, composer_->GetCursor());
  composer_->MoveCursorRight();
  EXPECT_EQ(1, composer_->GetCursor());

  composer_->MoveCursorTo(0);
  EXPECT_EQ(0, composer_->GetCursor());
  composer_->MoveCursorTo(6);
  EXPECT_EQ(6, composer_->GetCursor());
  composer_->MoveCursorTo(3);
  EXPECT_EQ(3, composer_->GetCursor());
  composer_->MoveCursorTo(10);
  EXPECT_EQ(3, composer_->GetCursor());
  composer_->MoveCursorTo(-1);
  EXPECT_EQ(3, composer_->GetCursor());
}

TEST_F(ComposerTest, SourceText) {
  composer_->SetInputMode(transliteration::HALF_ASCII);
  composer_->InsertCharacterPreedit("mozc");
  composer_->mutable_source_text()->assign("MOZC");
  EXPECT_FALSE(composer_->Empty());
  EXPECT_EQ("mozc", GetPreedit(composer_.get()));
  EXPECT_EQ("MOZC", composer_->source_text());

  composer_->Backspace();
  composer_->Backspace();
  EXPECT_FALSE(composer_->Empty());
  EXPECT_EQ("mo", GetPreedit(composer_.get()));
  EXPECT_EQ("MOZC", composer_->source_text());

  composer_->Reset();
  EXPECT_TRUE(composer_->Empty());
  EXPECT_TRUE(composer_->source_text().empty());
}

TEST_F(ComposerTest, DeleteAt) {
  table_->AddRule("mo", "も", "");
  table_->AddRule("zu", "ず", "");

  composer_->InsertCharacter("z");
  EXPECT_EQ("ｚ", GetPreedit(composer_.get()));
  EXPECT_EQ(1, composer_->GetCursor());
  composer_->DeleteAt(0);
  EXPECT_EQ("", GetPreedit(composer_.get()));
  EXPECT_EQ(0, composer_->GetCursor());

  composer_->InsertCharacter("mmoz");
  EXPECT_EQ("ｍもｚ", GetPreedit(composer_.get()));
  EXPECT_EQ(3, composer_->GetCursor());
  composer_->DeleteAt(0);
  EXPECT_EQ("もｚ", GetPreedit(composer_.get()));
  EXPECT_EQ(2, composer_->GetCursor());
  composer_->InsertCharacter("u");
  EXPECT_EQ("もず", GetPreedit(composer_.get()));
  EXPECT_EQ(2, composer_->GetCursor());

  composer_->InsertCharacter("m");
  EXPECT_EQ("もずｍ", GetPreedit(composer_.get()));
  EXPECT_EQ(3, composer_->GetCursor());
  composer_->DeleteAt(1);
  EXPECT_EQ("もｍ", GetPreedit(composer_.get()));
  EXPECT_EQ(2, composer_->GetCursor());
  composer_->InsertCharacter("o");
  EXPECT_EQ("もも", GetPreedit(composer_.get()));
  EXPECT_EQ(2, composer_->GetCursor());
}

TEST_F(ComposerTest, DeleteRange) {
  table_->AddRule("mo", "も", "");
  table_->AddRule("zu", "ず", "");

  composer_->InsertCharacter("z");
  EXPECT_EQ("ｚ", GetPreedit(composer_.get()));
  EXPECT_EQ(1, composer_->GetCursor());

  composer_->DeleteRange(0, 1);
  EXPECT_EQ("", GetPreedit(composer_.get()));
  EXPECT_EQ(0, composer_->GetCursor());

  composer_->InsertCharacter("mmozmoz");
  EXPECT_EQ("ｍもｚもｚ", GetPreedit(composer_.get()));
  EXPECT_EQ(5, composer_->GetCursor());

  composer_->DeleteRange(0, 3);
  EXPECT_EQ("もｚ", GetPreedit(composer_.get()));
  EXPECT_EQ(2, composer_->GetCursor());

  composer_->InsertCharacter("u");
  EXPECT_EQ("もず", GetPreedit(composer_.get()));
  EXPECT_EQ(2, composer_->GetCursor());

  composer_->InsertCharacter("xyz");
  composer_->MoveCursorToBeginning();
  composer_->InsertCharacter("mom");
  EXPECT_EQ("もｍもずｘｙｚ", GetPreedit(composer_.get()));
  EXPECT_EQ(2, composer_->GetCursor());

  composer_->DeleteRange(2, 3);
  // "もｍ|ｙｚ"
  EXPECT_EQ("もｍｙｚ", GetPreedit(composer_.get()));
  EXPECT_EQ(2, composer_->GetCursor());

  composer_->InsertCharacter("o");
  // "もも|ｙｚ"
  EXPECT_EQ("ももｙｚ", GetPreedit(composer_.get()));
  EXPECT_EQ(2, composer_->GetCursor());

  composer_->DeleteRange(2, 1000);
  // "もも|"
  EXPECT_EQ("もも", GetPreedit(composer_.get()));
  EXPECT_EQ(2, composer_->GetCursor());
}

TEST_F(ComposerTest, 12KeysAsciiGetQueryForPrediction) {
  // http://b/5509480
  commands::Request request;
  request.set_zero_query_suggestion(true);
  request.set_mixed_conversion(true);
  request.set_special_romanji_table(
      commands::Request::TWELVE_KEYS_TO_HALFWIDTHASCII);
  composer_->SetRequest(&request);
  table_->InitializeWithRequestAndConfig(
      request, config::ConfigHandler::DefaultConfig(), mock_data_manager_);
  composer_->InsertCharacter("2");
  EXPECT_EQ("a", GetPreedit(composer_.get()));
  std::string result;
  composer_->GetQueryForConversion(&result);
  EXPECT_EQ("a", result);
  result.clear();
  composer_->GetQueryForPrediction(&result);
  EXPECT_EQ("a", result);
}

TEST_F(ComposerTest, InsertCharacterPreedit) {
  constexpr char kTestStr[] = "ああaｋka。";

  {
    std::string preedit;
    std::string conversion_query;
    std::string prediction_query;
    std::string base;
    std::set<std::string> expanded;
    composer_->InsertCharacterPreedit(kTestStr);
    composer_->GetStringForPreedit(&preedit);
    composer_->GetQueryForConversion(&conversion_query);
    composer_->GetQueryForPrediction(&prediction_query);
    composer_->GetQueriesForPrediction(&base, &expanded);
    EXPECT_FALSE(preedit.empty());
    EXPECT_FALSE(conversion_query.empty());
    EXPECT_FALSE(prediction_query.empty());
    EXPECT_FALSE(base.empty());
  }
  composer_->Reset();
  {
    std::string preedit;
    std::string conversion_query;
    std::string prediction_query;
    std::string base;
    std::set<std::string> expanded;
    std::vector<std::string> chars;
    Util::SplitStringToUtf8Chars(kTestStr, &chars);
    for (size_t i = 0; i < chars.size(); ++i) {
      composer_->InsertCharacterPreedit(chars[i]);
    }
    composer_->GetStringForPreedit(&preedit);
    composer_->GetQueryForConversion(&conversion_query);
    composer_->GetQueryForPrediction(&prediction_query);
    composer_->GetQueriesForPrediction(&base, &expanded);
    EXPECT_FALSE(preedit.empty());
    EXPECT_FALSE(conversion_query.empty());
    EXPECT_FALSE(prediction_query.empty());
    EXPECT_FALSE(base.empty());
  }
}

namespace {
ProbableKeyEvents GetStubProbableKeyEvent(int key_code, double probability) {
  ProbableKeyEvents result;
  ProbableKeyEvent *event;
  event = result.Add();
  event->set_key_code(key_code);
  event->set_probability(probability);
  event = result.Add();
  event->set_key_code('z');
  event->set_probability(1.0f - probability);
  return result;
}

KeyEvent GetKeyEvent(const std::string &raw,
                     ProbableKeyEvents probable_key_events) {
  KeyEvent key_event;
  key_event.set_key_code(Util::Utf8ToUcs4(raw));
  *(key_event.mutable_probable_key_event()) = probable_key_events;
  return key_event;
}

}  // namespace

class MockTypingModel : public TypingModel {
 public:
  MockTypingModel() : TypingModel(nullptr, 0, nullptr, 0, nullptr) {}
  ~MockTypingModel() override = default;
  int GetCost(absl::string_view key) const override { return 10; }
};

// Test fixture for setting up mobile qwerty romaji table to test typing
// corrector inside composer.
class TypingCorrectionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_ = absl::make_unique<Config>();
    ConfigHandler::GetDefaultConfig(config_.get());
    config_->set_use_typing_correction(true);

    table_ = absl::make_unique<Table>();
    table_->LoadFromFile("system://qwerty_mobile-hiragana.tsv");

    request_ = absl::make_unique<Request>();
    request_->set_special_romanji_table(Request::QWERTY_MOBILE_TO_HIRAGANA);

    composer_ = absl::make_unique<Composer>(table_.get(), request_.get(),
                                            config_.get());

    table_->typing_model_ = absl::make_unique<MockTypingModel>();
  }

  static bool IsTypingCorrectorClearedOrInvalidated(const Composer &composer) {
    std::vector<TypeCorrectedQuery> queries;
    composer.GetTypeCorrectedQueriesForPrediction(&queries);
    return queries.empty();
  }

  std::unique_ptr<Config> config_;
  std::unique_ptr<Request> request_;
  std::unique_ptr<Composer> composer_;
  std::unique_ptr<Table> table_;
};

TEST_F(TypingCorrectionTest, ResetAfterComposerReset) {
  composer_->InsertCharacterKeyEvent(
      GetKeyEvent("a", GetStubProbableKeyEvent('a', 0.9f)));
  EXPECT_FALSE(IsTypingCorrectorClearedOrInvalidated(*composer_));
  composer_->Reset();
  EXPECT_TRUE(IsTypingCorrectorClearedOrInvalidated(*composer_));
  composer_->InsertCharacterKeyEvent(
      GetKeyEvent("a", GetStubProbableKeyEvent('a', 0.9f)));
  EXPECT_FALSE(IsTypingCorrectorClearedOrInvalidated(*composer_));
}

TEST_F(TypingCorrectionTest, InvalidateAfterDeleteAt) {
  composer_->InsertCharacterKeyEvent(
      GetKeyEvent("a", GetStubProbableKeyEvent('a', 0.9f)));
  EXPECT_FALSE(IsTypingCorrectorClearedOrInvalidated(*composer_));
  composer_->DeleteAt(0);
  EXPECT_TRUE(IsTypingCorrectorClearedOrInvalidated(*composer_));
}

TEST_F(TypingCorrectionTest, InvalidateAfterDelete) {
  composer_->InsertCharacterKeyEvent(
      GetKeyEvent("a", GetStubProbableKeyEvent('a', 0.9f)));
  EXPECT_FALSE(IsTypingCorrectorClearedOrInvalidated(*composer_));
  composer_->Delete();
  EXPECT_TRUE(IsTypingCorrectorClearedOrInvalidated(*composer_));
}

TEST_F(TypingCorrectionTest, InvalidateAfterDeleteRange) {
  composer_->InsertCharacterKeyEvent(
      GetKeyEvent("a", GetStubProbableKeyEvent('a', 0.9f)));
  EXPECT_FALSE(IsTypingCorrectorClearedOrInvalidated(*composer_));
  composer_->DeleteRange(0, 1);
  EXPECT_TRUE(IsTypingCorrectorClearedOrInvalidated(*composer_));
}

TEST_F(TypingCorrectionTest, ResetAfterEditErase) {
  composer_->InsertCharacterKeyEvent(
      GetKeyEvent("a", GetStubProbableKeyEvent('a', 0.9f)));
  EXPECT_FALSE(IsTypingCorrectorClearedOrInvalidated(*composer_));
  composer_->EditErase();
  EXPECT_TRUE(IsTypingCorrectorClearedOrInvalidated(*composer_));
  composer_->InsertCharacterKeyEvent(
      GetKeyEvent("a", GetStubProbableKeyEvent('a', 0.9f)));
  EXPECT_FALSE(IsTypingCorrectorClearedOrInvalidated(*composer_));
}

TEST_F(TypingCorrectionTest, InvalidateAfterBackspace) {
  composer_->InsertCharacterKeyEvent(
      GetKeyEvent("a", GetStubProbableKeyEvent('a', 0.9f)));
  EXPECT_FALSE(IsTypingCorrectorClearedOrInvalidated(*composer_));
  composer_->Backspace();
  EXPECT_TRUE(IsTypingCorrectorClearedOrInvalidated(*composer_));
}

TEST_F(TypingCorrectionTest, InvalidateAfterMoveCursorLeft) {
  composer_->InsertCharacterKeyEvent(
      GetKeyEvent("a", GetStubProbableKeyEvent('a', 0.9f)));
  EXPECT_FALSE(IsTypingCorrectorClearedOrInvalidated(*composer_));
  composer_->MoveCursorLeft();
  EXPECT_TRUE(IsTypingCorrectorClearedOrInvalidated(*composer_));
}

TEST_F(TypingCorrectionTest, InvalidateAfterMoveCursorRight) {
  composer_->InsertCharacterKeyEvent(
      GetKeyEvent("a", GetStubProbableKeyEvent('a', 0.9f)));
  EXPECT_FALSE(IsTypingCorrectorClearedOrInvalidated(*composer_));
  composer_->MoveCursorRight();
  EXPECT_TRUE(IsTypingCorrectorClearedOrInvalidated(*composer_));
}

TEST_F(TypingCorrectionTest, InvalidateAfterMoveCursorToBeginning) {
  composer_->InsertCharacterKeyEvent(
      GetKeyEvent("a", GetStubProbableKeyEvent('a', 0.9f)));
  EXPECT_FALSE(IsTypingCorrectorClearedOrInvalidated(*composer_));
  composer_->MoveCursorToBeginning();
  EXPECT_TRUE(IsTypingCorrectorClearedOrInvalidated(*composer_));
}

TEST_F(TypingCorrectionTest, InvalidateAfterMoveCursorToEnd) {
  composer_->InsertCharacterKeyEvent(
      GetKeyEvent("a", GetStubProbableKeyEvent('a', 0.9f)));
  EXPECT_FALSE(IsTypingCorrectorClearedOrInvalidated(*composer_));
  composer_->MoveCursorToEnd();
  EXPECT_TRUE(IsTypingCorrectorClearedOrInvalidated(*composer_));
}

TEST_F(TypingCorrectionTest, InvalidateAfterMoveCursorTo) {
  composer_->InsertCharacterKeyEvent(
      GetKeyEvent("a", GetStubProbableKeyEvent('a', 0.9f)));
  composer_->InsertCharacterKeyEvent(
      GetKeyEvent("b", GetStubProbableKeyEvent('a', 0.9f)));
  EXPECT_FALSE(IsTypingCorrectorClearedOrInvalidated(*composer_));
  composer_->MoveCursorTo(0);
  EXPECT_TRUE(IsTypingCorrectorClearedOrInvalidated(*composer_));
}

TEST_F(TypingCorrectionTest, GetTypeCorrectedQueriesForPrediction) {
  // This test only checks if typing correction candidates are nonempty after
  // each key insertion. The quality of typing correction depends on data model
  // and is tested in composer/internal/typing_corrector_test.cc.
  const char *kKeys[] = {"m", "o", "z", "u", "k", "u"};
  for (size_t i = 0; i < std::size(kKeys); ++i) {
    composer_->InsertCharacterKeyEvent(
        GetKeyEvent(kKeys[i], GetStubProbableKeyEvent(kKeys[i][0], 0.9f)));
    EXPECT_FALSE(IsTypingCorrectorClearedOrInvalidated(*composer_));
  }
  composer_->Backspace();
  for (size_t i = 0; i < std::size(kKeys); ++i) {
    composer_->InsertCharacterKeyEvent(
        GetKeyEvent(kKeys[i], ProbableKeyEvents()));
    EXPECT_TRUE(IsTypingCorrectorClearedOrInvalidated(*composer_));
  }
}

TEST_F(ComposerTest, GetRawString) {
  table_->AddRule("sa", "さ", "");
  table_->AddRule("shi", "し", "");
  table_->AddRule("mi", "み", "");

  composer_->SetOutputMode(transliteration::HIRAGANA);

  composer_->InsertCharacter("sashimi");

  std::string output;
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ("さしみ", output);

  std::string raw_string;
  composer_->GetRawString(&raw_string);
  EXPECT_EQ("sashimi", raw_string);

  std::string raw_sub_string;
  composer_->GetRawSubString(0, 2, &raw_sub_string);
  EXPECT_EQ("sashi", raw_sub_string);

  composer_->GetRawSubString(1, 1, &raw_sub_string);
  EXPECT_EQ("shi", raw_sub_string);
}

TEST_F(ComposerTest, SetPreeditTextForTestOnly) {
  std::string output;
  std::set<std::string> expanded;

  composer_->SetPreeditTextForTestOnly("も");

  EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());

  composer_->GetStringForPreedit(&output);
  EXPECT_EQ("も", output);

  composer_->GetStringForSubmission(&output);
  EXPECT_EQ("も", output);

  composer_->GetQueryForConversion(&output);
  EXPECT_EQ("も", output);

  composer_->GetQueryForPrediction(&output);
  EXPECT_EQ("も", output);

  composer_->GetQueriesForPrediction(&output, &expanded);
  EXPECT_EQ("も", output);
  EXPECT_TRUE(expanded.empty());

  composer_->Reset();

  composer_->SetPreeditTextForTestOnly("mo");

  EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());

  composer_->GetStringForPreedit(&output);
  EXPECT_EQ("mo", output);

  composer_->GetStringForSubmission(&output);
  EXPECT_EQ("mo", output);

  composer_->GetQueryForConversion(&output);
  EXPECT_EQ("mo", output);

  composer_->GetQueryForPrediction(&output);
  EXPECT_EQ("mo", output);

  composer_->GetQueriesForPrediction(&output, &expanded);
  EXPECT_EQ("mo", output);

  EXPECT_TRUE(expanded.empty());

  composer_->Reset();

  composer_->SetPreeditTextForTestOnly("ｍ");

  EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());

  composer_->GetStringForPreedit(&output);
  EXPECT_EQ("ｍ", output);

  composer_->GetStringForSubmission(&output);
  EXPECT_EQ("ｍ", output);

  composer_->GetQueryForConversion(&output);
  EXPECT_EQ("m", output);

  composer_->GetQueryForPrediction(&output);
  EXPECT_EQ("m", output);

  composer_->GetQueriesForPrediction(&output, &expanded);
  EXPECT_EQ("m", output);

  EXPECT_TRUE(expanded.empty());

  composer_->Reset();

  composer_->SetPreeditTextForTestOnly("もｚ");

  EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());

  composer_->GetStringForPreedit(&output);
  EXPECT_EQ("もｚ", output);

  composer_->GetStringForSubmission(&output);
  EXPECT_EQ("もｚ", output);

  composer_->GetQueryForConversion(&output);
  EXPECT_EQ("もz", output);

  composer_->GetQueryForPrediction(&output);
  EXPECT_EQ("もz", output);

  composer_->GetQueriesForPrediction(&output, &expanded);
  EXPECT_EQ("もz", output);

  EXPECT_TRUE(expanded.empty());
}

TEST_F(ComposerTest, IsToggleable) {
  constexpr int kAttrs =
      TableAttribute::NEW_CHUNK | TableAttribute::NO_TRANSLITERATION;
  table_->AddRuleWithAttributes("1", "", "{?}あ", kAttrs);
  table_->AddRule("{?}あ1", "", "{?}い");
  table_->AddRule("{?}い{!}", "", "{*}い");

  EXPECT_FALSE(composer_->IsToggleable());

  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ("あ", GetPreedit(composer_.get()));
  EXPECT_TRUE(composer_->IsToggleable());

  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ("い", GetPreedit(composer_.get()));
  EXPECT_TRUE(composer_->IsToggleable());

  composer_->InsertCommandCharacter(Composer::STOP_KEY_TOGGLING);
  EXPECT_EQ("い", GetPreedit(composer_.get()));
  EXPECT_FALSE(composer_->IsToggleable());

  composer_->Reset();
  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ("あ", GetPreedit(composer_.get()));
  composer_->SetNewInput();
  EXPECT_FALSE(composer_->IsToggleable());
}

TEST_F(ComposerTest, CheckTimeout) {
  table_->AddRule("1", "", "あ");
  table_->AddRule("あ{!}", "あ", "");
  table_->AddRule("あ1", "", "い");
  table_->AddRule("い{!}", "い", "");
  table_->AddRule("い1", "", "う");

  constexpr uint64_t kBaseSeconds = 86400;  // Epoch time + 1 day.
  mozc::ScopedClockMock clock(kBaseSeconds, 0);

  EXPECT_EQ(0, composer_->timeout_threshold_msec());

  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ("あ", GetPreedit(composer_.get()));

  clock->PutClockForward(3, 0);  // +3.0 sec

  // Because the threshold is not set, STOP_KEY_TOGGLING is not sent.
  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ("い", GetPreedit(composer_.get()));

  // Set the threshold time.
  composer_->Reset();
  composer_->set_timeout_threshold_msec(1000);

  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ("あ", GetPreedit(composer_.get()));

  clock->PutClockForward(3, 0);  // +3.0 sec
  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ("ああ", GetPreedit(composer_.get()));

  clock->PutClockForward(0, 700'000);  // +0.7 sec
  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ("あい", GetPreedit(composer_.get()));
}


TEST_F(ComposerTest, CheckTimeoutWithProtobuf) {
  table_->AddRule("1", "", "あ");
  table_->AddRule("あ{!}", "あ", "");
  table_->AddRule("あ1", "", "い");
  table_->AddRule("い{!}", "い", "");
  table_->AddRule("い1", "", "う");

  constexpr uint64_t kBaseSeconds = 86400;  // Epoch time + 1 day.
  mozc::ScopedClockMock clock(kBaseSeconds, 0);

  config_->set_composing_timeout_threshold_msec(500);
  composer_->Reset();  // The threshold should be updated to 500msec.

  uint64_t timestamp_msec = kBaseSeconds * 1000;

  KeyEvent key_event;
  key_event.set_key_code('1');
  key_event.set_timestamp_msec(timestamp_msec);
  composer_->InsertCharacterKeyEvent(key_event);
  EXPECT_EQ("あ", GetPreedit(composer_.get()));

  clock->PutClockForward(0, 100'000);  // +0.1 sec in the global clock
  timestamp_msec += 3000;  // +3.0 sec in proto.
  key_event.set_timestamp_msec(timestamp_msec);
  composer_->InsertCharacterKeyEvent(key_event);
  EXPECT_EQ("ああ", GetPreedit(composer_.get()));

  clock->PutClockForward(0, 100'000);  // +0.1 sec in the global clock
  timestamp_msec += 700;  // +0.7 sec in proto.
  key_event.set_timestamp_msec(timestamp_msec);
  composer_->InsertCharacterKeyEvent(key_event);
  EXPECT_EQ("あああ", GetPreedit(composer_.get()));

  clock->PutClockForward(3, 0);  // +3.0 sec in the global clock
  timestamp_msec += 100;  // +0.7 sec in proto.
  key_event.set_timestamp_msec(timestamp_msec);
  composer_->InsertCharacterKeyEvent(key_event);
  EXPECT_EQ("ああい", GetPreedit(composer_.get()));
}


TEST_F(ComposerTest, SimultaneousInput) {
  table_->AddRule("k", "", "い");      // k → い
  table_->AddRule("い{!}", "い", "");  // k → い (timeout)
  table_->AddRule("d", "", "か");      // d → か
  table_->AddRule("か{!}", "か", "");  // d → か (timeout)
  table_->AddRule("かk", "れ", "");    // dk → れ
  table_->AddRule("いd", "れ", "");    // kd → れ

  constexpr uint64_t kBaseSeconds = 86400;  // Epoch time + 1 day.
  mozc::ScopedClockMock clock(kBaseSeconds, 0);
  composer_->set_timeout_threshold_msec(50);

  ASSERT_TRUE(InsertKeyWithMode("k", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ("い", GetPreedit(composer_.get()));

  clock->PutClockForward(0, 30'000);  // +30 msec (< 50)
  ASSERT_TRUE(InsertKeyWithMode("d", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ("れ", GetPreedit(composer_.get()));

  clock->PutClockForward(0, 30'000);  // +30 msec (< 50)
  ASSERT_TRUE(InsertKeyWithMode("k", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ("れい", GetPreedit(composer_.get()));

  clock->PutClockForward(0, 200'000);  // +200 msec (> 50)
  ASSERT_TRUE(InsertKeyWithMode("d", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ("れいか", GetPreedit(composer_.get()));
}
}  // namespace composer
}  // namespace mozc
