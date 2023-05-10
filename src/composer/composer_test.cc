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

#include <iterator>
#include <memory>
#include <optional>
#include <set>
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
#include "testing/gmock.h"
#include "testing/googletest.h"
#include "testing/gunit.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace composer {
namespace {

using ::mozc::commands::CheckSpellingResponse;
using ::mozc::commands::KeyEvent;
using ::mozc::commands::Request;
using ::mozc::config::CharacterFormManager;
using ::mozc::config::Config;
using ::mozc::config::ConfigHandler;
using ::testing::_;
using ::testing::Return;

// ProbableKeyEvent is the innter-class member so needs to define as alias.
using ProbableKeyEvent = ::mozc::commands::KeyEvent::ProbableKeyEvent;

bool InsertKey(const absl::string_view key_string, Composer *composer) {
  commands::KeyEvent key;
  if (!KeyParser::ParseKey(key_string, &key)) {
    return false;
  }
  return composer->InsertCharacterKeyEvent(key);
}

bool InsertKeyWithMode(const absl::string_view key_string,
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
  ComposerTest(const ComposerTest &) = delete;
  ComposerTest &operator=(const ComposerTest &) = delete;
  ~ComposerTest() override = default;

  void SetUp() override {
    table_ = std::make_unique<Table>();
    config_ = std::make_unique<Config>();
    request_ = std::make_unique<Request>();
    composer_ =
        std::make_unique<Composer>(table_.get(), request_.get(), config_.get());
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
};

TEST_F(ComposerTest, Reset) {
  composer_->InsertCharacter("mozuku");

  composer_->SetInputMode(transliteration::HALF_ASCII);

  EXPECT_EQ(composer_->GetOutputMode(), transliteration::HIRAGANA);
  composer_->SetOutputMode(transliteration::HALF_ASCII);
  EXPECT_EQ(composer_->GetOutputMode(), transliteration::HALF_ASCII);

  composer_->SetInputFieldType(commands::Context::PASSWORD);
  composer_->Reset();

  EXPECT_TRUE(composer_->Empty());
  // The input mode ramains as the previous mode.
  EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);
  EXPECT_EQ(composer_->GetInputFieldType(), commands::Context::PASSWORD);
  // The output mode should be reset.
  EXPECT_EQ(composer_->GetOutputMode(), transliteration::HIRAGANA);
}

TEST_F(ComposerTest, ResetInputMode) {
  composer_->InsertCharacter("mozuku");

  composer_->SetInputMode(transliteration::FULL_KATAKANA);
  composer_->SetTemporaryInputMode(transliteration::HALF_ASCII);
  composer_->ResetInputMode();

  EXPECT_FALSE(composer_->Empty());
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);
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
  EXPECT_EQ(composer_->GetLength(), 5);

  EXPECT_TRUE(composer_->EnableInsert());
  composer_->InsertCharacter("u");
  EXPECT_EQ(composer_->GetLength(), 6);

  EXPECT_FALSE(composer_->EnableInsert());
  composer_->InsertCharacter("!");
  EXPECT_EQ(composer_->GetLength(), 6);

  std::string result;
  composer_->GetQueryForConversion(&result);
  EXPECT_EQ(result, "mozuku");

  composer_->Backspace();
  EXPECT_EQ(composer_->GetLength(), 5);
  EXPECT_TRUE(composer_->EnableInsert());
}

TEST_F(ComposerTest, BackSpace) {
  composer_->InsertCharacter("abc");

  composer_->Backspace();
  EXPECT_EQ(composer_->GetLength(), 2);
  EXPECT_EQ(composer_->GetCursor(), 2);
  std::string result;
  composer_->GetQueryForConversion(&result);
  EXPECT_EQ(result, "ab");

  result.clear();
  composer_->GetQueryForConversion(&result);
  EXPECT_EQ(result, "ab");

  composer_->MoveCursorToBeginning();
  EXPECT_EQ(composer_->GetCursor(), 0);

  composer_->Backspace();
  EXPECT_EQ(composer_->GetLength(), 2);
  EXPECT_EQ(composer_->GetCursor(), 0);
  result.clear();
  composer_->GetQueryForConversion(&result);
  EXPECT_EQ(result, "ab");

  composer_->Backspace();
  EXPECT_EQ(composer_->GetLength(), 2);
  EXPECT_EQ(composer_->GetCursor(), 0);
  result.clear();
  composer_->GetQueryForConversion(&result);
  EXPECT_EQ(result, "ab");
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
  EXPECT_EQ(output, "あいう");

  composer_->SetOutputMode(transliteration::FULL_ASCII);
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ(output, "ａｉｕ");

  composer_->InsertCharacter("a");
  composer_->InsertCharacter("i");
  composer_->InsertCharacter("u");
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ(output, "ａｉｕあいう");
}

TEST_F(ComposerTest, OutputMode2) {
  // This behaviour is based on Kotoeri

  table_->AddRule("a", "あ", "");
  table_->AddRule("i", "い", "");
  table_->AddRule("u", "う", "");

  composer_->InsertCharacter("a");
  composer_->InsertCharacter("i");
  composer_->InsertCharacter("u");

  std::string output;
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ(output, "あいう");

  composer_->MoveCursorLeft();
  composer_->SetOutputMode(transliteration::FULL_ASCII);
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ(output, "ａｉｕ");

  composer_->InsertCharacter("a");
  composer_->InsertCharacter("i");
  composer_->InsertCharacter("u");
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ(output, "ａｉｕあいう");
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
  EXPECT_EQ(transliterations.size(), transliteration::NUM_T13N_TYPES);
  EXPECT_EQ(transliterations[transliteration::HIRAGANA], "あ");
  EXPECT_EQ(transliterations[transliteration::FULL_KATAKANA], "ア");
  EXPECT_EQ(transliterations[transliteration::HALF_ASCII], "a");
  EXPECT_EQ(transliterations[transliteration::FULL_ASCII], "ａ");
  EXPECT_EQ(transliterations[transliteration::HALF_KATAKANA], "ｱ");

  composer_->Reset();
  ASSERT_TRUE(composer_->Empty());
  transliterations.clear();

  composer_->InsertCharacter("!");
  composer_->GetTransliterations(&transliterations);
  EXPECT_EQ(transliterations.size(), transliteration::NUM_T13N_TYPES);
  // NOTE(komatsu): The duplication will be handled by the session layer.
  EXPECT_EQ(transliterations[transliteration::HIRAGANA], "！");
  EXPECT_EQ(transliterations[transliteration::FULL_KATAKANA], "！");
  EXPECT_EQ(transliterations[transliteration::HALF_ASCII], "!");
  EXPECT_EQ(transliterations[transliteration::FULL_ASCII], "！");
  EXPECT_EQ(transliterations[transliteration::HALF_KATAKANA], "!");

  composer_->Reset();
  ASSERT_TRUE(composer_->Empty());
  transliterations.clear();

  composer_->InsertCharacter("aIu");
  composer_->GetTransliterations(&transliterations);
  EXPECT_EQ(transliterations.size(), transliteration::NUM_T13N_TYPES);
  EXPECT_EQ(transliterations[transliteration::HIRAGANA], "あいう");
  EXPECT_EQ(transliterations[transliteration::FULL_KATAKANA], "アイウ");
  EXPECT_EQ(transliterations[transliteration::HALF_ASCII], "aIu");
  EXPECT_EQ(transliterations[transliteration::HALF_ASCII_UPPER], "AIU");
  EXPECT_EQ(transliterations[transliteration::HALF_ASCII_LOWER], "aiu");
  EXPECT_EQ(transliterations[transliteration::HALF_ASCII_CAPITALIZED], "Aiu");
  EXPECT_EQ(transliterations[transliteration::FULL_ASCII], "ａＩｕ");
  EXPECT_EQ(transliterations[transliteration::FULL_ASCII_UPPER], "ＡＩＵ");
  EXPECT_EQ(transliterations[transliteration::FULL_ASCII_LOWER], "ａｉｕ");
  EXPECT_EQ(transliterations[transliteration::FULL_ASCII_CAPITALIZED],
            "Ａｉｕ");
  EXPECT_EQ(transliterations[transliteration::HALF_KATAKANA], "ｱｲｳ");

  // Transliterations for quote marks.  This is a test against
  // http://b/1581367
  composer_->Reset();
  ASSERT_TRUE(composer_->Empty());
  transliterations.clear();

  composer_->InsertCharacter("'\"`");
  composer_->GetTransliterations(&transliterations);
  EXPECT_EQ(transliterations[transliteration::HALF_ASCII], "'\"`");
  EXPECT_EQ(transliterations[transliteration::FULL_ASCII], "’”｀");
}

TEST_F(ComposerTest, GetSubTransliterations) {
  table_->AddRule("ka", "か", "");
  table_->AddRule("n", "ん", "");
  table_->AddRule("na", "な", "");
  table_->AddRule("da", "だ", "");

  composer_->InsertCharacter("kanna");

  transliteration::Transliterations transliterations;
  composer_->GetSubTransliterations(0, 2, &transliterations);
  EXPECT_EQ(transliterations[transliteration::HIRAGANA], "かん");
  EXPECT_EQ(transliterations[transliteration::FULL_KATAKANA], "カン");
  EXPECT_EQ(transliterations[transliteration::HALF_ASCII], "kan");
  EXPECT_EQ(transliterations[transliteration::FULL_ASCII], "ｋａｎ");
  EXPECT_EQ(transliterations[transliteration::HALF_KATAKANA], "ｶﾝ");

  transliterations.clear();
  composer_->GetSubTransliterations(1, 1, &transliterations);
  EXPECT_EQ(transliterations[transliteration::HIRAGANA], "ん");
  EXPECT_EQ(transliterations[transliteration::FULL_KATAKANA], "ン");
  EXPECT_EQ(transliterations[transliteration::HALF_ASCII], "n");
  EXPECT_EQ(transliterations[transliteration::FULL_ASCII], "ｎ");
  EXPECT_EQ(transliterations[transliteration::HALF_KATAKANA], "ﾝ");

  transliterations.clear();
  composer_->GetSubTransliterations(2, 1, &transliterations);
  EXPECT_EQ(transliterations[transliteration::HIRAGANA], "な");
  EXPECT_EQ(transliterations[transliteration::FULL_KATAKANA], "ナ");
  EXPECT_EQ(transliterations[transliteration::HALF_ASCII], "na");
  EXPECT_EQ(transliterations[transliteration::FULL_ASCII], "ｎａ");
  EXPECT_EQ(transliterations[transliteration::HALF_KATAKANA], "ﾅ");

  // Invalid position
  transliterations.clear();
  composer_->GetSubTransliterations(5, 3, &transliterations);
  EXPECT_EQ(transliterations[transliteration::HIRAGANA], "");
  EXPECT_EQ(transliterations[transliteration::FULL_KATAKANA], "");
  EXPECT_EQ(transliterations[transliteration::HALF_ASCII], "");
  EXPECT_EQ(transliterations[transliteration::FULL_ASCII], "");
  EXPECT_EQ(transliterations[transliteration::HALF_KATAKANA], "");

  // Invalid size
  transliterations.clear();
  composer_->GetSubTransliterations(0, 999, &transliterations);
  EXPECT_EQ(transliterations[transliteration::HIRAGANA], "かんな");
  EXPECT_EQ(transliterations[transliteration::FULL_KATAKANA], "カンナ");
  EXPECT_EQ(transliterations[transliteration::HALF_ASCII], "kanna");
  EXPECT_EQ(transliterations[transliteration::FULL_ASCII], "ｋａｎｎａ");
  EXPECT_EQ(transliterations[transliteration::HALF_KATAKANA], "ｶﾝﾅ");

  // Dakuon case
  transliterations.clear();
  composer_->EditErase();
  composer_->InsertCharacter("dankann");
  composer_->GetSubTransliterations(0, 3, &transliterations);
  EXPECT_EQ(transliterations[transliteration::HIRAGANA], "だんか");
  EXPECT_EQ(transliterations[transliteration::FULL_KATAKANA], "ダンカ");
  EXPECT_EQ(transliterations[transliteration::HALF_ASCII], "danka");
  EXPECT_EQ(transliterations[transliteration::FULL_ASCII], "ｄａｎｋａ");
  EXPECT_EQ(transliterations[transliteration::HALF_KATAKANA], "ﾀﾞﾝｶ");
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
  EXPECT_EQ(preedit, "！かｎ");

  std::string submission;
  composer_->GetStringForSubmission(&submission);
  EXPECT_EQ(submission, "！かｎ");

  std::string conversion;
  composer_->GetQueryForConversion(&conversion);
  EXPECT_EQ(conversion, "!かん");

  std::string prediction;
  composer_->GetQueryForPrediction(&prediction);
  EXPECT_EQ(prediction, "!か");

  // Query: "kas"
  composer_->EditErase();
  composer_->InsertCharacter("kas");

  preedit.clear();
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, "かｓ");

  submission.clear();
  composer_->GetStringForSubmission(&submission);
  EXPECT_EQ(submission, "かｓ");

  // Pending chars should remain.  This is a test against
  // http://b/1799399
  conversion.clear();
  composer_->GetQueryForConversion(&conversion);
  EXPECT_EQ(conversion, "かs");

  prediction.clear();
  composer_->GetQueryForPrediction(&prediction);
  EXPECT_EQ(prediction, "か");

  // Query: "s"
  composer_->EditErase();
  composer_->InsertCharacter("s");

  preedit.clear();
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, "ｓ");

  submission.clear();
  composer_->GetStringForSubmission(&submission);
  EXPECT_EQ(submission, "ｓ");

  conversion.clear();
  composer_->GetQueryForConversion(&conversion);
  EXPECT_EQ(conversion, "s");

  prediction.clear();
  composer_->GetQueryForPrediction(&prediction);
  EXPECT_EQ(prediction, "s");

  // Query: "sk"
  composer_->EditErase();
  composer_->InsertCharacter("sk");

  preedit.clear();
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, "ｓｋ");

  submission.clear();
  composer_->GetStringForSubmission(&submission);
  EXPECT_EQ(submission, "ｓｋ");

  conversion.clear();
  composer_->GetQueryForConversion(&conversion);
  EXPECT_EQ(conversion, "sk");

  prediction.clear();
  composer_->GetQueryForPrediction(&prediction);
  EXPECT_EQ(prediction, "sk");
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
    EXPECT_EQ(prediction, kInput);
  }
  prediction.clear();
  composer_->EditErase();
  {
    constexpr char kInput[] = "hello!";
    composer_->InsertCharacter(kInput);
    composer_->GetQueryForPrediction(&prediction);
    EXPECT_EQ(prediction, kInput);
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
    EXPECT_EQ(prediction, "hello");
  }
  prediction.clear();
  composer_->EditErase();
  {
    composer_->InsertCharacter("ｈｅｌｌｏ！");
    composer_->GetQueryForPrediction(&prediction);
    EXPECT_EQ(prediction, "hello!");
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
    EXPECT_EQ(base, "う");
    EXPECT_EQ(expanded.size(), 7);
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
    EXPECT_EQ(base, "い");
    EXPECT_EQ(expanded.size(), 2);
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
    EXPECT_EQ(base, "い");
    EXPECT_EQ(expanded.size(), 1);
    EXPECT_TRUE(expanded.end() != expanded.find("ど"));
  }
  {
    std::string base, preedit;
    std::set<std::string> expanded;
    composer_->EditErase();
    composer_->InsertCharacter("_x*");
    composer_->GetQueriesForPrediction(&base, &expanded);
    composer_->GetStringForPreedit(&preedit);
    EXPECT_EQ(base, "い");
    EXPECT_EQ(expanded.size(), 1);
    EXPECT_TRUE(expanded.end() != expanded.find("っ"));
  }
}

// TODO(yukiokamoto): This unit test currently fails due to a crash. (Please
// note that this crash could only occur in a debug build.)
// Once another issue in b/277163340 that an input "[][]" is not converted
// properly is fixed, uncomment the following test.
TEST_F(ComposerTest, Issue277163340) {
  // Test against http://b/277163340.
  // Before the fix, unexpected input was passed to
  // RemoveExpandedCharsForModifier() in the following test due to another
  // bug in composer/internal/char_chunk.cc and it
  // caused the process to crash.
  table_->AddRuleWithAttributes("[", "", "", NO_TRANSLITERATION);
  std::string base, asis, preedit;
  std::set<std::string> expanded;
  commands::KeyEvent key;
  key.set_key_string("[]");
  key.set_input_style(commands::KeyEvent::AS_IS);
  composer_->InsertCharacterKeyEvent(key);
  composer_->InsertCharacterKeyEvent(key);
  composer_->InsertCommandCharacter(composer::Composer::STOP_KEY_TOGGLING);
  composer_->GetQueriesForPrediction(&base, &expanded);

  // Never reached here due to the crash before the fix for b/277163340.
  EXPECT_EQ(base, "[][]");
}

// Emulates tapping "[]" key twice without enough internval.
TEST_F(ComposerTest, DoubleTapSquareBracketPairKey) {
  commands::KeyEvent key;
  table_->AddRuleWithAttributes("[", "", "",
                                TableAttribute::NO_TRANSLITERATION);

  std::string preedit;
  key.set_key_string("[]");
  key.set_input_style(commands::KeyEvent::AS_IS);
  composer_->InsertCharacterKeyEvent(key);
  composer_->InsertCharacterKeyEvent(key);
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, "[][]");
}

// Emulates tapping "[" key twice without enough internval.
TEST_F(ComposerTest, DoubleTapOpenSquareBracketKey) {
  commands::KeyEvent key;
  table_->AddRuleWithAttributes("[", "", "",
                                TableAttribute::NO_TRANSLITERATION);

  std::string preedit;
  key.set_key_string("[");
  key.set_input_style(commands::KeyEvent::AS_IS);
  composer_->InsertCharacterKeyEvent(key);
  composer_->InsertCharacterKeyEvent(key);
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, "[[");
}

// Emulates tapping "[]" key twice with enough internval.
TEST_F(ComposerTest, DoubleTapSquareBracketPairKeyWithInterval) {
  commands::KeyEvent key;
  table_->AddRuleWithAttributes("[", "", "",
                                TableAttribute::NO_TRANSLITERATION);

  std::string preedit;
  key.set_key_string("[]");
  key.set_input_style(commands::KeyEvent::AS_IS);
  composer_->InsertCharacterKeyEvent(key);
  composer_->InsertCommandCharacter(Composer::STOP_KEY_TOGGLING);
  composer_->InsertCharacterKeyEvent(key);
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, "[][]");
}

TEST_F(ComposerTest, GetTypeCorrectedQueriesForPredictionMobile) {
  config_->set_use_typing_correction(true);
  request_->set_special_romanji_table(
      commands::Request::TWELVE_KEYS_TO_HIRAGANA);
  table_->InitializeWithRequestAndConfig(*request_, *config_,
                                         mock_data_manager_);
  table_->SetTypingModelForTesting(TypingModel::CreateTypingModel(
      commands::Request::TWELVE_KEYS_TO_HIRAGANA, mock_data_manager_));

  struct ProbableKeyInfo {
    uint32_t key_code;
    double prob;
  };
  struct KeyInfo {
    uint32_t key_code;
    std::vector<ProbableKeyInfo> prob_keys;
  };

  auto insert_key_events = [](const std::vector<KeyInfo> &keys,
                              Composer *composer) {
    composer->EditErase();
    for (const auto &key : keys) {
      commands::KeyEvent key_event;
      key_event.set_key_code(key.key_code);
      if (!key.prob_keys.empty()) {
        ProbableKeyEvents *probable_key_events =
            key_event.mutable_probable_key_event();
        for (const ProbableKeyInfo &prob_key : key.prob_keys) {
          ProbableKeyEvent *event = probable_key_events->Add();
          event->set_key_code(prob_key.key_code);
          event->set_probability(prob_key.prob);
        }
      }
      composer->InsertCharacterKeyEvent(key_event);
    }
  };

  auto contains = [](std::set<std::string> str_set, std::string str) {
    return str_set.find(str) != str_set.end();
  };

  {
    insert_key_events({{'4', {}},                          // た
                       {'5', {{'5', 0.75}, {'2', 0.25}}},  // な
                       {'2', {}}},                         // か
                      composer_.get());
    std::vector<TypeCorrectedQuery> queries;
    composer_->GetTypeCorrectedQueriesForPrediction(&queries);
    ASSERT_EQ(queries.size(), 1);
    EXPECT_EQ(queries[0].base, "た");
    EXPECT_EQ(queries[0].asis, "たき");
    EXPECT_EQ(queries[0].expanded.size(), 2);
    EXPECT_TRUE(contains(queries[0].expanded, "き"));
    EXPECT_TRUE(contains(queries[0].expanded, "ぎ"));
  }

  {
    insert_key_events({{'4', {}},                          // た
                       {'5', {{'5', 0.75}, {'2', 0.25}}},  // な
                       {'2', {}},                          // か
                       {'*', {}}},                         // modifier key
                      composer_.get());
    std::vector<TypeCorrectedQuery> queries;
    composer_->GetTypeCorrectedQueriesForPrediction(&queries);
    ASSERT_EQ(queries.size(), 1);
    EXPECT_EQ(queries[0].base, "た");
    EXPECT_EQ(queries[0].asis, "たぎ");
    EXPECT_EQ(queries[0].expanded.size(), 1);
    EXPECT_TRUE(contains(queries[0].expanded, "ぎ"));
  }
}

TEST_F(ComposerTest, GetStringFunctionsForN) {
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
  EXPECT_EQ(preedit, "ｎｙ［ＮＹＡ］ｎ");

  std::string submission;
  composer_->GetStringForSubmission(&submission);
  EXPECT_EQ(submission, "ｎｙ［ＮＹＡ］ｎ");

  std::string conversion;
  composer_->GetQueryForConversion(&conversion);
  EXPECT_EQ(conversion, "ny[NYA][N]");

  std::string prediction;
  composer_->GetQueryForPrediction(&prediction);
  EXPECT_EQ(prediction, "ny[NYA]");

  composer_->InsertCharacter("ka");
  std::string conversion2;
  composer_->GetQueryForConversion(&conversion2);
  EXPECT_EQ(conversion2, "ny[NYA][N][KA]");

  std::string prediction2;
  composer_->GetQueryForPrediction(&prediction2);
  EXPECT_EQ(prediction2, "ny[NYA][N][KA]");
}

TEST_F(ComposerTest, GetStringFunctionsInputFieldType) {
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
        EXPECT_EQ(converted, key);
        composer_->GetStringForSubmission(&converted);
        EXPECT_EQ(converted, key);
      } else {
        // Expected result is FULL_WIDTH form.
        // Typically the result is a full-width form of the key,
        // but some characters are not.
        // So here we checks only the character form.
        composer_->GetStringForPreedit(&converted);
        EXPECT_EQ(Util::GetFormType(converted), Util::FULL_WIDTH);
        composer_->GetStringForSubmission(&converted);
        EXPECT_EQ(Util::GetFormType(converted), Util::FULL_WIDTH);
      }
    }
  }
}

TEST_F(ComposerTest, InsertCommandCharacter) {
  table_->AddRule("{<}", "REWIND", "");
  table_->AddRule("{!}", "STOP", "");

  composer_->SetInputMode(transliteration::HALF_ASCII);

  composer_->InsertCommandCharacter(Composer::REWIND);
  EXPECT_EQ(GetPreedit(composer_.get()), "REWIND");

  composer_->Reset();
  composer_->InsertCommandCharacter(Composer::STOP_KEY_TOGGLING);
  EXPECT_EQ(GetPreedit(composer_.get()), "STOP");
}

TEST_F(ComposerTest, InsertCharacterKeyEvent) {
  commands::KeyEvent key;
  table_->AddRule("a", "あ", "");

  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);

  std::string preedit;
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, "あ");

  // Half width "A" will be inserted.
  key.set_key_code('A');
  composer_->InsertCharacterKeyEvent(key);

  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, "あA");

  // Half width "a" will be inserted.
  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);

  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, "あAa");

  // Reset() should revert the previous input mode (Hiragana).
  composer_->Reset();

  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, "あ");

  // Typing "A" temporarily switch the input mode.  The input mode
  // should be reverted back after reset.
  composer_->SetInputMode(transliteration::FULL_KATAKANA);
  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, "あア");

  key.set_key_code('A');
  composer_->InsertCharacterKeyEvent(key);
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, "あアA");

  // Reset() should revert the previous input mode (Katakana).
  composer_->Reset();

  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, "ア");
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
  EXPECT_EQ(preedit, kYama);

  // Input "山" as key_code which is converted to "川".
  table_->AddRule(kYama, kKawa, "");
  composer_->Reset();
  composer_->InsertCharacterKeyEvent(key);
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, kKawa);

  // Input ("山", "空") as (key_code, key_string) which is treated as "空".
  key.set_key_string(kSora);
  composer_->Reset();
  composer_->InsertCharacterKeyEvent(key);
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, kSora);
}

TEST_F(ComposerTest, InsertCharacterKeyEventWithoutKeyCode) {
  commands::KeyEvent key;

  // Input "山" as key_string.  key_code is empty.
  key.set_key_string(kYama);
  composer_->InsertCharacterKeyEvent(key);
  EXPECT_FALSE(key.has_key_code());

  std::string preedit;
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, kYama);

  transliteration::Transliterations transliterations;
  composer_->GetTransliterations(&transliterations);
  EXPECT_EQ(transliterations[transliteration::HIRAGANA], kYama);
  EXPECT_EQ(transliterations[transliteration::HALF_ASCII], kYama);
}

TEST_F(ComposerTest, InsertCharacterKeyEventWithAsIs) {
  commands::KeyEvent key;
  table_->AddRule("a", "あ", "");
  table_->AddRule("-", "ー", "");

  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);

  std::string preedit;
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, "あ");

  // Full width "０" will be inserted.
  key.set_key_code('0');
  key.set_key_string("0");
  composer_->InsertCharacterKeyEvent(key);

  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, "あ０");

  // Half width "0" will be inserted.
  key.set_key_code('0');
  key.set_key_string("0");
  key.set_input_style(commands::KeyEvent::AS_IS);
  composer_->InsertCharacterKeyEvent(key);

  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, "あ０0");

  // Full width "0" will be inserted.
  key.set_key_code('0');
  key.set_key_string("0");
  key.set_input_style(commands::KeyEvent::FOLLOW_MODE);
  composer_->InsertCharacterKeyEvent(key);

  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, "あ０0０");

  // Half width "-" will be inserted.
  key.set_key_code('-');
  key.set_key_string("-");
  key.set_input_style(commands::KeyEvent::AS_IS);
  composer_->InsertCharacterKeyEvent(key);

  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, "あ０0０-");

  // Full width "−" (U+2212) will be inserted.
  key.set_key_code('-');
  key.set_key_string("−");
  key.set_input_style(commands::KeyEvent::FOLLOW_MODE);
  composer_->InsertCharacterKeyEvent(key);

  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, "あ０0０-−");  // The last hyphen is U+2212.
}

TEST_F(ComposerTest, InsertCharacterKeyEventWithInputMode) {
  table_->AddRule("a", "あ", "");
  table_->AddRule("i", "い", "");
  table_->AddRule("u", "う", "");

  {
    // "a" → "あ" (Hiragana)
    EXPECT_TRUE(InsertKeyWithMode("a", commands::HIRAGANA, composer_.get()));
    EXPECT_EQ(GetPreedit(composer_.get()), "あ");
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);

    // "aI" → "あI" (Alphanumeric)
    EXPECT_TRUE(InsertKeyWithMode("I", commands::HIRAGANA, composer_.get()));
    EXPECT_EQ(GetPreedit(composer_.get()), "あI");
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);

    // "u" → "あIu" (Alphanumeric)
    EXPECT_TRUE(InsertKeyWithMode("u", commands::HALF_ASCII, composer_.get()));
    EXPECT_EQ(GetPreedit(composer_.get()), "あIu");
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);

    // [shift] → "あIu" (Hiragana)
    EXPECT_TRUE(
        InsertKeyWithMode("Shift", commands::HALF_ASCII, composer_.get()));
    EXPECT_EQ(GetPreedit(composer_.get()), "あIu");
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);

    // "u" → "あIuう" (Hiragana)
    EXPECT_TRUE(InsertKeyWithMode("u", commands::HIRAGANA, composer_.get()));
    EXPECT_EQ(GetPreedit(composer_.get()), "あIuう");
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
  }

  composer_ =
      std::make_unique<Composer>(table_.get(), request_.get(), config_.get());

  {
    // "a" → "あ" (Hiragana)
    EXPECT_TRUE(InsertKeyWithMode("a", commands::HIRAGANA, composer_.get()));
    EXPECT_EQ(GetPreedit(composer_.get()), "あ");
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);

    // "i" (Katakana) → "あイ" (Katakana)
    EXPECT_TRUE(
        InsertKeyWithMode("i", commands::FULL_KATAKANA, composer_.get()));
    EXPECT_EQ(GetPreedit(composer_.get()), "あイ");
    EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

    // SetInputMode(Alphanumeric) → "あイ" (Alphanumeric)
    composer_->SetInputMode(transliteration::HALF_ASCII);
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);

    // [shift] → "あイ" (Alphanumeric) - Nothing happens.
    EXPECT_TRUE(
        InsertKeyWithMode("Shift", commands::HALF_ASCII, composer_.get()));
    EXPECT_EQ(GetPreedit(composer_.get()), "あイ");
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);

    // "U" → "あイ" (Alphanumeric)
    EXPECT_TRUE(InsertKeyWithMode("U", commands::HALF_ASCII, composer_.get()));
    EXPECT_EQ(GetPreedit(composer_.get()), "あイU");
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);

    // [shift] → "あイU" (Alphanumeric) - Nothing happens.
    EXPECT_TRUE(
        InsertKeyWithMode("Shift", commands::HALF_ASCII, composer_.get()));
    EXPECT_EQ(GetPreedit(composer_.get()), "あイU");
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);
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

TEST_F(ComposerTest, FullWidthCharRulesb31444698) {
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
  EXPECT_EQ(GetPreedit(composer_.get()), "あ");
  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "い");

  composer_->Reset();

  // b/31444698.  Test if "かか" is transliterated to "き"
  ASSERT_TRUE(InsertKeyWithMode("か", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "か");
  ASSERT_TRUE(InsertKeyWithMode("か", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "き");
}

TEST_F(ComposerTest, Copy) {
  table_->AddRule("a", "あ", "");
  table_->AddRule("n", "ん", "");
  table_->AddRule("na", "な", "");

  {
    SCOPED_TRACE("Precomposition");

    std::string src_composition;
    composer_->GetStringForSubmission(&src_composition);
    EXPECT_EQ(src_composition, "");

    Composer dest = *composer_;
    ExpectSameComposer(*composer_, dest);
  }

  {
    SCOPED_TRACE("Composition");

    composer_->InsertCharacter("a");
    composer_->InsertCharacter("n");
    std::string src_composition;
    composer_->GetStringForSubmission(&src_composition);
    EXPECT_EQ(src_composition, "あｎ");

    Composer dest = *composer_;
    ExpectSameComposer(*composer_, dest);
  }

  {
    SCOPED_TRACE("Conversion");

    std::string src_composition;
    composer_->GetQueryForConversion(&src_composition);
    EXPECT_EQ(src_composition, "あん");

    Composer dest = *composer_;
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
    EXPECT_EQ(src_composition, "AaAAあ");

    Composer dest = *composer_;
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
    EXPECT_EQ(src_composition, "M");

    Composer dest = *composer_;
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
    EXPECT_EQ(preedit, "あAaああ");
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
    EXPECT_EQ(preedit, "アAaアア");
    EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);
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
    EXPECT_EQ(preedit, "AAあAa");
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
    EXPECT_EQ(preedit, "D&D2a");
  }

  {  // Full-witdh alphanumeric
    composer_->SetInputMode(transliteration::FULL_ASCII);
    composer_->Reset();
    InsertKey("A", composer_.get());  // "Ａ"
    InsertKey("a", composer_.get());  // "Ａａ"

    std::string preedit;
    composer_->GetStringForPreedit(&preedit);
    EXPECT_EQ(preedit, "Ａａ");
  }

  {  // Half-witdh alphanumeric
    composer_->SetInputMode(transliteration::HALF_ASCII);
    composer_->Reset();
    InsertKey("A", composer_.get());  // "A"
    InsertKey("a", composer_.get());  // "Aa"

    std::string preedit;
    composer_->GetStringForPreedit(&preedit);
    EXPECT_EQ(preedit, "Aa");
  }
}

TEST_F(ComposerTest, ShiftKeyOperationForKatakana) {
  config_->set_shift_key_mode_switch(Config::KATAKANA_INPUT_MODE);
  table_->InitializeWithRequestAndConfig(*request_, *config_,
                                         mock_data_manager_);
  composer_->Reset();
  composer_->SetInputMode(transliteration::HIRAGANA);
  InsertKey("K", composer_.get());
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);
  InsertKey("A", composer_.get());
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);
  InsertKey("T", composer_.get());
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);
  InsertKey("a", composer_.get());
  // See the below comment.
  // EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);
  EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
  InsertKey("k", composer_.get());
  EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
  InsertKey("A", composer_.get());
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);
  InsertKey("n", composer_.get());
  EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
  InsertKey("a", composer_.get());
  EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);

  std::string preedit;
  composer_->GetStringForPreedit(&preedit);
  // NOTE(komatsu): "KATakAna" is converted to "カＴあｋアな" rather
  // than "カタカな".  This is a different behavior from Kotoeri due
  // to avoid complecated implementation.  Unless this is a problem
  // for users, this difference probably remains.
  //
  // EXPECT_EQ(preedit, "カタカな");

  EXPECT_EQ(preedit, "カＴあｋアな");
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
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
    InsertKey("p", composer_.get());

    std::string preedit;
    composer_->GetStringForPreedit(&preedit);
    EXPECT_EQ(preedit, "http");
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);

    composer_->Reset();
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
  }

  composer_ =
      std::make_unique<Composer>(table_.get(), request_.get(), config_.get());

  {  // google
    InsertKey("g", composer_.get());
    InsertKey("o", composer_.get());
    InsertKey("o", composer_.get());
    InsertKey("g", composer_.get());
    InsertKey("l", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
    InsertKey("e", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
    EXPECT_EQ(GetPreedit(composer_.get()), "google");

    InsertKey("a", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
    EXPECT_EQ(GetPreedit(composer_.get()), "googleあ");

    composer_->Reset();
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
  }

  {  // google in full-width alphanumeric mode.
    composer_->SetInputMode(transliteration::FULL_ASCII);
    InsertKey("g", composer_.get());
    InsertKey("o", composer_.get());
    InsertKey("o", composer_.get());
    InsertKey("g", composer_.get());
    InsertKey("l", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_ASCII);
    InsertKey("e", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_ASCII);

    EXPECT_EQ(GetPreedit(composer_.get()), "ｇｏｏｇｌｅ");

    InsertKey("a", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_ASCII);
    EXPECT_EQ(GetPreedit(composer_.get()), "ｇｏｏｇｌｅａ");

    composer_->Reset();
    EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_ASCII);
    // Reset to Hiragana mode
    composer_->SetInputMode(transliteration::HIRAGANA);
  }

  {  // Google
    InsertKey("G", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);
    InsertKey("o", composer_.get());
    InsertKey("o", composer_.get());
    InsertKey("g", composer_.get());
    InsertKey("l", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);
    InsertKey("e", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);
    EXPECT_EQ(GetPreedit(composer_.get()), "Google");

    InsertKey("a", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);
    EXPECT_EQ(GetPreedit(composer_.get()), "Googlea");

    composer_->Reset();
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
  }

  config_->set_shift_key_mode_switch(Config::OFF);
  composer_ =
      std::make_unique<Composer>(table_.get(), request_.get(), config_.get());

  {  // Google
    InsertKey("G", composer_.get());
    InsertKey("o", composer_.get());
    InsertKey("o", composer_.get());
    InsertKey("g", composer_.get());
    InsertKey("l", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
    InsertKey("e", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
    EXPECT_EQ(GetPreedit(composer_.get()), "Google");

    InsertKey("a", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
    EXPECT_EQ(GetPreedit(composer_.get()), "Googleあ");

    composer_->Reset();
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
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
  EXPECT_EQ(preedit, "ｈっｔｐ：・・");
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
  EXPECT_EQ(preedit, "ｈっｔｐ：・・");
}

TEST_F(ComposerTest, KanaPrediction) {
  composer_->InsertCharacterKeyAndPreedit("t", "か");
  {
    std::string preedit;
    composer_->GetQueryForPrediction(&preedit);
    EXPECT_EQ(preedit, "か");
  }
  composer_->InsertCharacterKeyAndPreedit("\\", "ー");
  {
    std::string preedit;
    composer_->GetQueryForPrediction(&preedit);
    EXPECT_EQ(preedit, "かー");
  }
  composer_->InsertCharacterKeyAndPreedit(",", "、");
  {
    std::string preedit;
    composer_->GetQueryForPrediction(&preedit);
    EXPECT_EQ(preedit, "かー、");
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
  EXPECT_EQ(preedit, "くいりりら");

  transliteration::Transliterations transliterations;
  composer_->GetTransliterations(&transliterations);
  EXPECT_EQ(transliterations.size(), transliteration::NUM_T13N_TYPES);
  EXPECT_EQ(transliterations[transliteration::HALF_ASCII], "hello");
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
  EXPECT_EQ(output, "もず");
  EXPECT_EQ(composer_->GetCursor(), 2);

  composer_->SetOutputMode(transliteration::HALF_ASCII);
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ(output, "mozu");
  EXPECT_EQ(composer_->GetCursor(), 4);

  composer_->SetOutputMode(transliteration::HALF_KATAKANA);
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ(output, "ﾓｽﾞ");
  EXPECT_EQ(composer_->GetCursor(), 3);
}

TEST_F(ComposerTest, UpdateInputMode) {
  table_->AddRule("a", "あ", "");
  table_->AddRule("i", "い", "");

  InsertKey("A", composer_.get());
  EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);

  InsertKey("I", composer_.get());
  EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);

  InsertKey("a", composer_.get());
  EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);

  InsertKey("i", composer_.get());
  EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);

  composer_->SetInputMode(transliteration::FULL_ASCII);
  InsertKey("a", composer_.get());
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_ASCII);

  InsertKey("i", composer_.get());
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_ASCII);

  std::string output;
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ(output, "AIあいａｉ");

  composer_->SetInputMode(transliteration::FULL_KATAKANA);

  // "|AIあいａｉ"
  composer_->MoveCursorToBeginning();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  // "A|Iあいａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);

  // "AI|あいａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  // "AIあ|いａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);

  // "AIあい|ａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  // "AIあいａ|ｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_ASCII);

  // "AIあいａｉ|"
  composer_->MoveCursorRight();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_ASCII);

  // "AIあいａ|ｉ"
  composer_->MoveCursorLeft();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_ASCII);

  // "|AIあいａｉ"
  composer_->MoveCursorToBeginning();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  // "A|Iあいａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);

  // "A|あいａｉ"
  composer_->Delete();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  // "Aあ|いａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);

  // "A|いａｉ"
  composer_->Backspace();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  // "Aいａｉ|"
  composer_->MoveCursorToEnd();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  // "Aいａ|ｉ"
  composer_->MoveCursorLeft();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_ASCII);

  // "Aいａｉ|"
  composer_->MoveCursorToEnd();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);
}

TEST_F(ComposerTest, DisabledUpdateInputMode) {
  // Set the flag disable.
  commands::Request request;
  request.set_update_input_mode_from_surrounding_text(false);
  composer_->SetRequest(&request);

  table_->AddRule("a", "あ", "");
  table_->AddRule("i", "い", "");

  InsertKey("A", composer_.get());
  EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);

  InsertKey("I", composer_.get());
  EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);

  InsertKey("a", composer_.get());
  EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);

  InsertKey("i", composer_.get());
  EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);

  composer_->SetInputMode(transliteration::FULL_ASCII);
  InsertKey("a", composer_.get());
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_ASCII);

  InsertKey("i", composer_.get());
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_ASCII);

  std::string output;
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ(output, "AIあいａｉ");

  composer_->SetInputMode(transliteration::FULL_KATAKANA);

  // Use same scenario as above test case, but the result of GetInputMode
  // should be always FULL_KATAKANA regardless of the surrounding text.

  // "|AIあいａｉ"
  composer_->MoveCursorToBeginning();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  // "A|Iあいａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  // "AI|あいａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  // "AIあ|いａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  // "AIあい|ａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  // "AIあいａ|ｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  // "AIあいａｉ|"
  composer_->MoveCursorRight();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  // "AIあいａ|ｉ"
  composer_->MoveCursorLeft();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  // "|AIあいａｉ"
  composer_->MoveCursorToBeginning();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  // "A|Iあいａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  // "A|あいａｉ"
  composer_->Delete();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  // "Aあ|いａｉ"
  composer_->MoveCursorRight();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  // "A|いａｉ"
  composer_->Backspace();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  // "Aいａｉ|"
  composer_->MoveCursorToEnd();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  // "Aいａ|ｉ"
  composer_->MoveCursorLeft();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  // "Aいａｉ|"
  composer_->MoveCursorToEnd();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);
}

TEST_F(ComposerTest, TransformCharactersForNumbers) {
  std::string query;

  query = "";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  query = "R2D2";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  query = "ー１";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ(query, "−１");  // The hyphen is U+2212.

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
  EXPECT_EQ(query, "１，０");

  query = "０。５";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ(query, "０．５");

  query = "ー１、０００。５";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ(query, "−１，０００．５");  // The hyphen is U+2212.

  query = "０３ー";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ(query, "０３−");  // The hyphen is U+2212.

  query = "０３ーーーーー";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ(query, "０３−−−−−");  // The hyphen is U+2212.

  query = "ｘー（ー１）＞ーｘ";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ(query, "ｘ−（−１）＞−ｘ");  // The hyphen is U+2212.

  query = "１＊ー２／ー３ーー４";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ(query, "１＊−２／−３−−４");  // The hyphen is U+2212.

  query = "ＡーＺ";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ(query, "Ａ−Ｚ");  // The hyphen is U+2212.

  query = "もずく、うぉーきんぐ。";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  query = "えー２、９８０円！月々たった、２、９８０円？";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ(query, "えー２，９８０円！月々たった、２，９８０円？");

  query = "およそ、３。１４１５９。";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ(query, "およそ、３．１４１５９．");

  query = "１００、";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ(query, "１００，");

  query = "１００。";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ(query, "１００．");
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
    EXPECT_EQ(result, "3.14");
  }

  {
    composer_->Reset();
    manager->SetDefaultRule();
    manager->AddPreeditRule("1", Config::FULL_WIDTH);
    manager->AddPreeditRule(".,", Config::HALF_WIDTH);
    composer_->InsertCharacter("3.14");
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ(result, "３.１４");
  }

  {
    composer_->Reset();
    manager->SetDefaultRule();
    manager->AddPreeditRule("1", Config::HALF_WIDTH);
    manager->AddPreeditRule(".,", Config::FULL_WIDTH);
    composer_->InsertCharacter("3.14");
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ(result, "3．14");
  }

  {
    composer_->Reset();
    manager->SetDefaultRule();
    manager->AddPreeditRule("1", Config::FULL_WIDTH);
    manager->AddPreeditRule(".,", Config::FULL_WIDTH);
    composer_->InsertCharacter("3.14");
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ(result, "３．１４");
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
    EXPECT_EQ(result, "−１，０００．５");  // The hyphen is U+2212.
  }
  {
    std::string result;
    composer_->GetStringForSubmission(&result);
    EXPECT_EQ(result, "−１，０００．５");  // The hyphen is U+2212.
  }
  {
    std::string result;
    composer_->GetQueryForConversion(&result);
    EXPECT_EQ(result, "-1,000.5");
  }
  {
    std::string result;
    composer_->GetQueryForPrediction(&result);
    EXPECT_EQ(result, "-1,000.5");
  }
  {
    std::string left, focused, right;
    // Right edge
    composer_->GetPreedit(&left, &focused, &right);
    EXPECT_EQ(left, "−１，０００．５");  // The hyphen is U+2212.
    EXPECT_TRUE(focused.empty());
    EXPECT_TRUE(right.empty());

    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    EXPECT_EQ(left, "−１，０００．");  // The hyphen is U+2212.
    EXPECT_EQ(focused, "５");
    EXPECT_TRUE(right.empty());

    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    EXPECT_EQ(left, "−１，０００");  // The hyphen is U+2212.
    EXPECT_EQ(focused, "．");
    EXPECT_EQ(right, "５");

    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    EXPECT_EQ(left, "−１，００");  // The hyphen is U+2212.
    EXPECT_EQ(focused, "０");
    EXPECT_EQ(right, "．５");

    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    EXPECT_EQ(left, "−１，０");  // The hyphen is U+2212.
    EXPECT_EQ(focused, "０");
    EXPECT_EQ(right, "０．５");

    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    EXPECT_EQ(left, "−１，");  // The hyphen is U+2212.
    EXPECT_EQ(focused, "０");
    EXPECT_EQ(right, "００．５");

    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    EXPECT_EQ(left, "−１");
    EXPECT_EQ(focused, "，");
    EXPECT_EQ(right, "０００．５");

    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    EXPECT_EQ(left, "−");  // U+2212.
    EXPECT_EQ(focused, "１");
    EXPECT_EQ(right, "，０００．５");

    // Left edge
    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    EXPECT_TRUE(left.empty());
    EXPECT_EQ(focused, "−");  // U+2212.
    EXPECT_EQ(right, "１，０００．５");
  }
}

TEST_F(ComposerTest, AlphanumericOfSSH) {
  // This is a unittest against http://b/3199626
  // 'ssh' (っｓｈ) + F10 should be 'ssh'.
  table_->AddRule("ss", "[X]", "s");
  table_->AddRule("sha", "[SHA]", "");
  composer_->InsertCharacter("ssh");
  EXPECT_EQ(GetPreedit(composer_.get()), "［Ｘ］ｓｈ");

  std::string query;
  composer_->GetQueryForConversion(&query);
  EXPECT_EQ(query, "[X]sh");

  transliteration::Transliterations t13ns;
  composer_->GetTransliterations(&t13ns);
  EXPECT_EQ(t13ns[transliteration::HALF_ASCII], "ssh");
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
  EXPECT_EQ(output, "a");

  // Insertion of a space and backspace it should not change the composition.
  composer_->InsertCharacter(" ");
  output.clear();
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ(output, "a ");

  composer_->Backspace();
  output.clear();
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ(output, "a");

  // Toggle the input mode to HIRAGANA, the preedit should not be changed.
  composer_->ToggleInputMode();
  output.clear();
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ(output, "a");

  // "a" should be converted to "ち" on Hiragana input mode.
  EXPECT_TRUE(composer_->InsertCharacterKeyEvent(key));
  output.clear();
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ(output, "aち");
}

TEST_F(ComposerTest, Issue1817410) {
  // This is a unittest against http://b/2190364
  table_->AddRule("ss", "っ", "s");

  InsertKey("s", composer_.get());
  InsertKey("s", composer_.get());

  std::string preedit;
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, "っｓ");

  std::string t13n;
  composer_->GetSubTransliteration(transliteration::HALF_ASCII, 0, 2, &t13n);
  EXPECT_EQ(t13n, "ss");

  t13n.clear();
  composer_->GetSubTransliteration(transliteration::HALF_ASCII, 0, 1, &t13n);
  EXPECT_EQ(t13n, "s");

  t13n.clear();
  composer_->GetSubTransliteration(transliteration::HALF_ASCII, 1, 1, &t13n);
  EXPECT_EQ(t13n, "s");
}

TEST_F(ComposerTest, Issue2272745) {
  // This is a unittest against http://b/2272745.
  // A temporary input mode remains when a composition is canceled.
  {
    InsertKey("G", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);

    composer_->Backspace();
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
  }
  composer_->Reset();
  {
    InsertKey("G", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);

    composer_->EditErase();
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
  }
}

TEST_F(ComposerTest, Isue2555503) {
  // This is a unittest against http://b/2555503.
  // Mode respects the previous character too much.
  InsertKey("a", composer_.get());
  composer_->SetInputMode(transliteration::FULL_KATAKANA);
  InsertKey("i", composer_.get());
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

  composer_->Backspace();
  EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);
}

TEST_F(ComposerTest, Issue2819580Case1) {
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
  EXPECT_EQ(result, "ny");
}

TEST_F(ComposerTest, Issue2819580Case2) {
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
  EXPECT_EQ(result, "ぽny");
}

TEST_F(ComposerTest, Issue2819580Case3) {
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
  EXPECT_EQ(result, "zny");
}

TEST_F(ComposerTest, Issue2797991Case1) {
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
  EXPECT_EQ(result, "C:\\Wi");
}

TEST_F(ComposerTest, Issue2797991Case2) {
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
  EXPECT_EQ(result, "C:Wi");
}

TEST_F(ComposerTest, Issue2797991Case3) {
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
  EXPECT_EQ(result, "C:\\Wiい");
}

TEST_F(ComposerTest, Issue2797991Case4) {
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
  EXPECT_EQ(result, "c:\\Wi");
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
    EXPECT_EQ(result, "いイいイ");
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
    EXPECT_EQ(result, "いIiI");
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
      EXPECT_EQ(result, "Ｉ");
    }

    {
      composer_->Reset();
      composer_->SetInputMode(transliteration::HALF_ASCII);
      InsertKey("I", composer_.get());
      std::string result;
      composer_->GetStringForPreedit(&result);
      EXPECT_EQ(result, "I");
    }

    {
      composer_->Reset();
      composer_->SetInputMode(transliteration::FULL_KATAKANA);
      InsertKey("I", composer_.get());
      std::string result;
      composer_->GetStringForPreedit(&result);
      EXPECT_EQ(result, "イ");
    }

    {
      composer_->Reset();
      composer_->SetInputMode(transliteration::HALF_KATAKANA);
      InsertKey("I", composer_.get());
      std::string result;
      composer_->GetStringForPreedit(&result);
      EXPECT_EQ(result, "ｲ");
    }

    {
      composer_->Reset();
      composer_->SetInputMode(transliteration::HIRAGANA);
      InsertKey("I", composer_.get());
      std::string result;
      composer_->GetStringForPreedit(&result);
      EXPECT_EQ(result, "イ");
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
    EXPECT_EQ(result, "いGoogle");
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
    EXPECT_EQ(result, "い");
  }

  InsertKey("i", composer_.get());

  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ(result, "いい");
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
    EXPECT_EQ(result, "い");
  }

  composer_->MoveCursorLeft();
  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ(result, "い");
  }

  InsertKey("G", composer_.get());
  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ(result, "Gい");
  }

  composer_->MoveCursorRight();
  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ(result, "Gい");
  }

  InsertKey("G", composer_.get());
  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ(result, "GいG");
  }

  composer_->MoveCursorLeft();
  InsertKey("i", composer_.get());
  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ(result, "GいいG");
  }

  composer_->MoveCursorRight();
  InsertKey("i", composer_.get());
  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ(result, "GいいGi");
  }

  InsertKey("G", composer_.get());
  InsertKey("i", composer_.get());
  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ(result, "GいいGiGi");
  }

  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  InsertKey("i", composer_.get());
  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ(result, "GいいGi");
  }

  InsertKey("G", composer_.get());
  InsertKey("G", composer_.get());
  composer_->MoveCursorRight();
  InsertKey("i", composer_.get());
  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ(result, "GいいGiGGi");
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
    EXPECT_EQ(result, "Gい");
  }

  composer_->Backspace();
  composer_->MoveCursorLeft();
  composer_->MoveCursorRight();
  InsertKey("i", composer_.get());
  {
    std::string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ(result, "Gi");
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
    TestData(const absl::string_view input_text,
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
      EXPECT_EQ(length_to_commit, test_data.expected_commit_length);
    } else {
      EXPECT_FALSE(result);
    }
  }
}

TEST_F(ComposerTest, CursorMovements) {
  composer_->InsertCharacter("mozuku");
  EXPECT_EQ(composer_->GetLength(), 6);
  EXPECT_EQ(composer_->GetCursor(), 6);

  composer_->MoveCursorRight();
  EXPECT_EQ(composer_->GetCursor(), 6);
  composer_->MoveCursorLeft();
  EXPECT_EQ(composer_->GetCursor(), 5);

  composer_->MoveCursorToBeginning();
  EXPECT_EQ(composer_->GetCursor(), 0);
  composer_->MoveCursorLeft();
  EXPECT_EQ(composer_->GetCursor(), 0);
  composer_->MoveCursorRight();
  EXPECT_EQ(composer_->GetCursor(), 1);

  composer_->MoveCursorTo(0);
  EXPECT_EQ(composer_->GetCursor(), 0);
  composer_->MoveCursorTo(6);
  EXPECT_EQ(composer_->GetCursor(), 6);
  composer_->MoveCursorTo(3);
  EXPECT_EQ(composer_->GetCursor(), 3);
  composer_->MoveCursorTo(10);
  EXPECT_EQ(composer_->GetCursor(), 3);
  composer_->MoveCursorTo(-1);
  EXPECT_EQ(composer_->GetCursor(), 3);
}

TEST_F(ComposerTest, SourceText) {
  composer_->SetInputMode(transliteration::HALF_ASCII);
  composer_->InsertCharacterPreedit("mozc");
  composer_->mutable_source_text()->assign("MOZC");
  EXPECT_FALSE(composer_->Empty());
  EXPECT_EQ(GetPreedit(composer_.get()), "mozc");
  EXPECT_EQ(composer_->source_text(), "MOZC");

  composer_->Backspace();
  composer_->Backspace();
  EXPECT_FALSE(composer_->Empty());
  EXPECT_EQ(GetPreedit(composer_.get()), "mo");
  EXPECT_EQ(composer_->source_text(), "MOZC");

  composer_->Reset();
  EXPECT_TRUE(composer_->Empty());
  EXPECT_TRUE(composer_->source_text().empty());
}

TEST_F(ComposerTest, DeleteAt) {
  table_->AddRule("mo", "も", "");
  table_->AddRule("zu", "ず", "");

  composer_->InsertCharacter("z");
  EXPECT_EQ(GetPreedit(composer_.get()), "ｚ");
  EXPECT_EQ(composer_->GetCursor(), 1);
  composer_->DeleteAt(0);
  EXPECT_EQ(GetPreedit(composer_.get()), "");
  EXPECT_EQ(composer_->GetCursor(), 0);

  composer_->InsertCharacter("mmoz");
  EXPECT_EQ(GetPreedit(composer_.get()), "ｍもｚ");
  EXPECT_EQ(composer_->GetCursor(), 3);
  composer_->DeleteAt(0);
  EXPECT_EQ(GetPreedit(composer_.get()), "もｚ");
  EXPECT_EQ(composer_->GetCursor(), 2);
  composer_->InsertCharacter("u");
  EXPECT_EQ(GetPreedit(composer_.get()), "もず");
  EXPECT_EQ(composer_->GetCursor(), 2);

  composer_->InsertCharacter("m");
  EXPECT_EQ(GetPreedit(composer_.get()), "もずｍ");
  EXPECT_EQ(composer_->GetCursor(), 3);
  composer_->DeleteAt(1);
  EXPECT_EQ(GetPreedit(composer_.get()), "もｍ");
  EXPECT_EQ(composer_->GetCursor(), 2);
  composer_->InsertCharacter("o");
  EXPECT_EQ(GetPreedit(composer_.get()), "もも");
  EXPECT_EQ(composer_->GetCursor(), 2);
}

TEST_F(ComposerTest, DeleteRange) {
  table_->AddRule("mo", "も", "");
  table_->AddRule("zu", "ず", "");

  composer_->InsertCharacter("z");
  EXPECT_EQ(GetPreedit(composer_.get()), "ｚ");
  EXPECT_EQ(composer_->GetCursor(), 1);

  composer_->DeleteRange(0, 1);
  EXPECT_EQ(GetPreedit(composer_.get()), "");
  EXPECT_EQ(composer_->GetCursor(), 0);

  composer_->InsertCharacter("mmozmoz");
  EXPECT_EQ(GetPreedit(composer_.get()), "ｍもｚもｚ");
  EXPECT_EQ(composer_->GetCursor(), 5);

  composer_->DeleteRange(0, 3);
  EXPECT_EQ(GetPreedit(composer_.get()), "もｚ");
  EXPECT_EQ(composer_->GetCursor(), 2);

  composer_->InsertCharacter("u");
  EXPECT_EQ(GetPreedit(composer_.get()), "もず");
  EXPECT_EQ(composer_->GetCursor(), 2);

  composer_->InsertCharacter("xyz");
  composer_->MoveCursorToBeginning();
  composer_->InsertCharacter("mom");
  EXPECT_EQ(GetPreedit(composer_.get()), "もｍもずｘｙｚ");
  EXPECT_EQ(composer_->GetCursor(), 2);

  composer_->DeleteRange(2, 3);
  // "もｍ|ｙｚ"
  EXPECT_EQ(GetPreedit(composer_.get()), "もｍｙｚ");
  EXPECT_EQ(composer_->GetCursor(), 2);

  composer_->InsertCharacter("o");
  // "もも|ｙｚ"
  EXPECT_EQ(GetPreedit(composer_.get()), "ももｙｚ");
  EXPECT_EQ(composer_->GetCursor(), 2);

  composer_->DeleteRange(2, 1000);
  // "もも|"
  EXPECT_EQ(GetPreedit(composer_.get()), "もも");
  EXPECT_EQ(composer_->GetCursor(), 2);
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
  EXPECT_EQ(GetPreedit(composer_.get()), "a");
  std::string result;
  composer_->GetQueryForConversion(&result);
  EXPECT_EQ(result, "a");
  result.clear();
  composer_->GetQueryForPrediction(&result);
  EXPECT_EQ(result, "a");
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

KeyEvent GetKeyEvent(const absl::string_view raw,
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
    config_ = std::make_unique<Config>();
    ConfigHandler::GetDefaultConfig(config_.get());
    config_->set_use_typing_correction(true);

    table_ = std::make_unique<Table>();
    table_->LoadFromFile("system://qwerty_mobile-hiragana.tsv");

    request_ = std::make_unique<Request>();
    request_->set_special_romanji_table(Request::QWERTY_MOBILE_TO_HIRAGANA);

    composer_ =
        std::make_unique<Composer>(table_.get(), request_.get(), config_.get());

    table_->typing_model_ = std::make_unique<MockTypingModel>();
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

TEST_F(TypingCorrectionTest, InvalidateAfterAsIsKeyEvent) {
  table_->AddRule("a", "あ", "");
  commands::KeyEvent key = GetKeyEvent("a", GetStubProbableKeyEvent('a', 0.9f));
  key.set_key_string("あ");
  composer_->InsertCharacterKeyEvent(key);
  EXPECT_FALSE(IsTypingCorrectorClearedOrInvalidated(*composer_));

  key.set_input_style(commands::KeyEvent::AS_IS);
  composer_->InsertCharacterKeyEvent(key);
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
  EXPECT_EQ(output, "さしみ");

  std::string raw_string;
  composer_->GetRawString(&raw_string);
  EXPECT_EQ(raw_string, "sashimi");

  std::string raw_sub_string;
  composer_->GetRawSubString(0, 2, &raw_sub_string);
  EXPECT_EQ(raw_sub_string, "sashi");

  composer_->GetRawSubString(1, 1, &raw_sub_string);
  EXPECT_EQ(raw_sub_string, "shi");
}

TEST_F(ComposerTest, SetPreeditTextForTestOnly) {
  std::string output;
  std::set<std::string> expanded;

  composer_->SetPreeditTextForTestOnly("も");

  EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);

  composer_->GetStringForPreedit(&output);
  EXPECT_EQ(output, "も");

  composer_->GetStringForSubmission(&output);
  EXPECT_EQ(output, "も");

  composer_->GetQueryForConversion(&output);
  EXPECT_EQ(output, "も");

  composer_->GetQueryForPrediction(&output);
  EXPECT_EQ(output, "も");

  composer_->GetQueriesForPrediction(&output, &expanded);
  EXPECT_EQ(output, "も");
  EXPECT_TRUE(expanded.empty());

  composer_->Reset();

  composer_->SetPreeditTextForTestOnly("mo");

  EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);

  composer_->GetStringForPreedit(&output);
  EXPECT_EQ(output, "mo");

  composer_->GetStringForSubmission(&output);
  EXPECT_EQ(output, "mo");

  composer_->GetQueryForConversion(&output);
  EXPECT_EQ(output, "mo");

  composer_->GetQueryForPrediction(&output);
  EXPECT_EQ(output, "mo");

  composer_->GetQueriesForPrediction(&output, &expanded);
  EXPECT_EQ(output, "mo");

  EXPECT_TRUE(expanded.empty());

  composer_->Reset();

  composer_->SetPreeditTextForTestOnly("ｍ");

  EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);

  composer_->GetStringForPreedit(&output);
  EXPECT_EQ(output, "ｍ");

  composer_->GetStringForSubmission(&output);
  EXPECT_EQ(output, "ｍ");

  composer_->GetQueryForConversion(&output);
  EXPECT_EQ(output, "m");

  composer_->GetQueryForPrediction(&output);
  EXPECT_EQ(output, "m");

  composer_->GetQueriesForPrediction(&output, &expanded);
  EXPECT_EQ(output, "m");

  EXPECT_TRUE(expanded.empty());

  composer_->Reset();

  composer_->SetPreeditTextForTestOnly("もｚ");

  EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);

  composer_->GetStringForPreedit(&output);
  EXPECT_EQ(output, "もｚ");

  composer_->GetStringForSubmission(&output);
  EXPECT_EQ(output, "もｚ");

  composer_->GetQueryForConversion(&output);
  EXPECT_EQ(output, "もz");

  composer_->GetQueryForPrediction(&output);
  EXPECT_EQ(output, "もz");

  composer_->GetQueriesForPrediction(&output, &expanded);
  EXPECT_EQ(output, "もz");

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
  EXPECT_EQ(GetPreedit(composer_.get()), "あ");
  EXPECT_TRUE(composer_->IsToggleable());

  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "い");
  EXPECT_TRUE(composer_->IsToggleable());

  composer_->InsertCommandCharacter(Composer::STOP_KEY_TOGGLING);
  EXPECT_EQ(GetPreedit(composer_.get()), "い");
  EXPECT_FALSE(composer_->IsToggleable());

  composer_->Reset();
  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "あ");
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

  EXPECT_EQ(composer_->timeout_threshold_msec(), 0);

  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "あ");

  clock->PutClockForward(3, 0);  // +3.0 sec

  // Because the threshold is not set, STOP_KEY_TOGGLING is not sent.
  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "い");

  // Set the threshold time.
  composer_->Reset();
  composer_->set_timeout_threshold_msec(1000);

  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "あ");

  clock->PutClockForward(3, 0);  // +3.0 sec
  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "ああ");

  clock->PutClockForward(0, 700'000);  // +0.7 sec
  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "あい");
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
  EXPECT_EQ(GetPreedit(composer_.get()), "あ");

  clock->PutClockForward(0, 100'000);  // +0.1 sec in the global clock
  timestamp_msec += 3000;              // +3.0 sec in proto.
  key_event.set_timestamp_msec(timestamp_msec);
  composer_->InsertCharacterKeyEvent(key_event);
  EXPECT_EQ(GetPreedit(composer_.get()), "ああ");

  clock->PutClockForward(0, 100'000);  // +0.1 sec in the global clock
  timestamp_msec += 700;               // +0.7 sec in proto.
  key_event.set_timestamp_msec(timestamp_msec);
  composer_->InsertCharacterKeyEvent(key_event);
  EXPECT_EQ(GetPreedit(composer_.get()), "あああ");

  clock->PutClockForward(3, 0);  // +3.0 sec in the global clock
  timestamp_msec += 100;         // +0.7 sec in proto.
  key_event.set_timestamp_msec(timestamp_msec);
  composer_->InsertCharacterKeyEvent(key_event);
  EXPECT_EQ(GetPreedit(composer_.get()), "ああい");
}

TEST_F(ComposerTest, SimultaneousInput) {
  table_->AddRule("k", "", "い");      // k → い
  table_->AddRule("い{!}", "い", "");  // k → い (timeout)
  table_->AddRule("d", "", "か");      // d → か
  table_->AddRule("か{!}", "か", "");  // d → か (timeout)
  table_->AddRule("かk", "れ", "");    // dk → れ
  table_->AddRule("いd", "れ", "");    // kd → れ
  table_->AddRule("zl", "→", "");      // normal conversion w/o timeout

  constexpr uint64_t kBaseSeconds = 86400;  // Epoch time + 1 day.
  mozc::ScopedClockMock clock(kBaseSeconds, 0);
  composer_->set_timeout_threshold_msec(50);

  ASSERT_TRUE(InsertKeyWithMode("k", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "い");

  clock->PutClockForward(0, 30'000);  // +30 msec (< 50)
  ASSERT_TRUE(InsertKeyWithMode("d", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "れ");

  clock->PutClockForward(0, 30'000);  // +30 msec (< 50)
  ASSERT_TRUE(InsertKeyWithMode("k", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "れい");

  clock->PutClockForward(0, 200'000);  // +200 msec (> 50)
  ASSERT_TRUE(InsertKeyWithMode("d", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "れいか");

  clock->PutClockForward(0, 200'000);  // +200 msec (> 50)
  ASSERT_TRUE(InsertKeyWithMode("z", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "れいかｚ");

  // Even after timeout, normal conversions work.
  clock->PutClockForward(0, 200'000);  // +200 msec (> 50)
  ASSERT_TRUE(InsertKeyWithMode("l", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "れいか→");
}

TEST_F(ComposerTest, SimultaneousInputWithSpecialKey1) {
  table_->AddRule("{henkan}", "あ", "");

  ASSERT_TRUE(InsertKeyWithMode("Henkan", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "あ");
}

TEST_F(ComposerTest, SimultaneousInputWithSpecialKey2) {
  table_->AddRule("{henkan}j", "お", "");

  ASSERT_TRUE(InsertKeyWithMode("Henkan", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "");

  ASSERT_TRUE(InsertKeyWithMode("j", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "お");

  ASSERT_TRUE(InsertKeyWithMode("Henkan", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "お");

  ASSERT_TRUE(InsertKeyWithMode("j", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "おお");
}

TEST_F(ComposerTest, SimultaneousInputWithSpecialKey3) {
  table_->AddRule("j", "", "と");
  table_->AddRule("と{henkan}", "お", "");
  table_->AddRule("{henkan}j", "お", "");

  ASSERT_TRUE(InsertKeyWithMode("Henkan", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "");

  ASSERT_TRUE(InsertKeyWithMode("j", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "お");
}

TEST_F(ComposerTest, SimultaneousInputWithSpecialKey4) {
  table_->AddRule("j", "", "と");
  table_->AddRule("と{henkan}", "お", "");
  table_->AddRule("{henkan}j", "お", "");

  ASSERT_TRUE(InsertKeyWithMode("j", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "と");

  ASSERT_TRUE(InsertKeyWithMode("Henkan", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "お");
}

TEST_F(ComposerTest, SimultaneousInputWithSpecialKey5) {
  table_->AddRule("r", "", "{*}");
  table_->AddRule("{*}j", "お", "");

  ASSERT_TRUE(InsertKeyWithMode("r", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "");

  ASSERT_TRUE(InsertKeyWithMode("j", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "お");
}

TEST_F(ComposerTest, SimultaneousInputWithSpecialKey6) {
  table_->AddRule("{!}", "", "");          // no-op
  table_->AddRule("{henkan}{!}", "", "");  // no-op
  table_->AddRule("j", "", "と");
  table_->AddRule("と{!}", "と", "");
  table_->AddRule("{henkan}j", "お", "");
  table_->AddRule("と{henkan}", "お", "");

  constexpr uint64_t kBaseSeconds = 86400;  // Epoch time + 1 day.
  mozc::ScopedClockMock clock(kBaseSeconds, 0);
  composer_->set_timeout_threshold_msec(50);

  // "j"
  ASSERT_TRUE(InsertKeyWithMode("j", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "と");

  // "j{henkan}"
  clock->PutClockForward(0, 30'000);  // +30 msec (< 50)
  ASSERT_TRUE(InsertKeyWithMode("Henkan", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "お");

  // "j{henkan}j"
  clock->PutClockForward(0, 30'000);  // +30 msec (< 50)
  ASSERT_TRUE(InsertKeyWithMode("j", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "おと");

  // "j{henkan}j{!}{henkan}"
  clock->PutClockForward(0, 200'000);  // +200 msec (> 50)
  ASSERT_TRUE(InsertKeyWithMode("Henkan", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "おと");

  // "j{henkan}j{!}{henkan}{!}j"
  clock->PutClockForward(0, 200'000);  // +200 msec (> 50)
  ASSERT_TRUE(InsertKeyWithMode("j", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "おとと");

  // "j{henkan}j{!}{henkan}{!}j{!}{henkan}"
  clock->PutClockForward(0, 200'000);  // +200 msec (> 50)
  ASSERT_TRUE(InsertKeyWithMode("Henkan", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "おとと");

  // "j{henkan}j{!}{henkan}{!}j{!}{henkan}j"
  clock->PutClockForward(0, 30'000);  // +30 msec (< 50)
  ASSERT_TRUE(InsertKeyWithMode("j", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(GetPreedit(composer_.get()), "おととお");
}

TEST_F(ComposerTest, SpellCheckerServiceTest) {
  table_->AddRule("a", "あ", "");
  table_->AddRule("i", "い", "");
  table_->AddRule("u", "う", "");
  composer_->InsertCharacter("aiu");

  const auto preedit = GetPreedit(composer_.get());
  composer_->SetSpellCheckerService(nullptr);
  EXPECT_EQ(composer_->GetTypeCorrectedQueries("context"), std::nullopt);

}
}  // namespace composer
}  // namespace mozc
