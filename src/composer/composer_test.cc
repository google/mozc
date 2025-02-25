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

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "base/clock_mock.h"
#include "base/util.h"
#include "composer/key_parser.h"
#include "composer/table.h"
#include "config/character_form_manager.h"
#include "config/config_handler.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "testing/gunit.h"
#include "transliteration/transliteration.h"

namespace mozc {
namespace composer {
namespace {

using ::mozc::commands::KeyEvent;
using ::mozc::commands::Request;
using ::mozc::config::CharacterFormManager;
using ::mozc::config::Config;

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

  EXPECT_EQ(lhs.GetStringForPreedit(), rhs.GetStringForPreedit());
  EXPECT_EQ(lhs.GetStringForSubmission(), rhs.GetStringForSubmission());
  EXPECT_EQ(lhs.GetQueryForConversion(), rhs.GetQueryForConversion());
  EXPECT_EQ(lhs.GetQueryForPrediction(), rhs.GetQueryForPrediction());
}

}  // namespace

class ComposerTest : public ::testing::Test {
 protected:
  ComposerTest() = default;
  ComposerTest(const ComposerTest &) = delete;
  ComposerTest &operator=(const ComposerTest &) = delete;
  ~ComposerTest() override = default;

  void SetUp() override {
    table_ = std::make_shared<Table>();
    config_ = std::make_shared<Config>();
    request_ = std::make_shared<Request>();
    composer_ = std::make_unique<Composer>(table_, request_, config_);
    CharacterFormManager::GetCharacterFormManager()->SetDefaultRule();
  }

  void TearDown() override {
    CharacterFormManager::GetCharacterFormManager()->SetDefaultRule();
  }

  std::unique_ptr<Composer> composer_;
  std::shared_ptr<Table> table_;
  std::shared_ptr<Request> request_;
  std::shared_ptr<Config> config_;
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
  // The input mode remains as the previous mode.
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

  EXPECT_EQ(composer_->GetQueryForConversion(), "mozuku");

  composer_->Backspace();
  EXPECT_EQ(composer_->GetLength(), 5);
  EXPECT_TRUE(composer_->EnableInsert());
}

TEST_F(ComposerTest, BackSpace) {
  composer_->InsertCharacter("abc");

  composer_->Backspace();
  EXPECT_EQ(composer_->GetLength(), 2);
  EXPECT_EQ(composer_->GetCursor(), 2);
  EXPECT_EQ(composer_->GetQueryForConversion(), "ab");

  EXPECT_EQ(composer_->GetQueryForConversion(), "ab");

  composer_->MoveCursorToBeginning();
  EXPECT_EQ(composer_->GetCursor(), 0);

  composer_->Backspace();
  EXPECT_EQ(composer_->GetLength(), 2);
  EXPECT_EQ(composer_->GetCursor(), 0);
  EXPECT_EQ(composer_->GetQueryForConversion(), "ab");

  composer_->Backspace();
  EXPECT_EQ(composer_->GetLength(), 2);
  EXPECT_EQ(composer_->GetCursor(), 0);
  EXPECT_EQ(composer_->GetQueryForConversion(), "ab");
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

  EXPECT_EQ(composer_->GetStringForPreedit(), "あいう");

  composer_->SetOutputMode(transliteration::FULL_ASCII);
  EXPECT_EQ(composer_->GetStringForPreedit(), "ａｉｕ");

  composer_->InsertCharacter("a");
  composer_->InsertCharacter("i");
  composer_->InsertCharacter("u");
  EXPECT_EQ(composer_->GetStringForPreedit(), "ａｉｕあいう");
}

TEST_F(ComposerTest, OutputMode2) {
  // This behaviour is based on Kotoeri

  table_->AddRule("a", "あ", "");
  table_->AddRule("i", "い", "");
  table_->AddRule("u", "う", "");

  composer_->InsertCharacter("a");
  composer_->InsertCharacter("i");
  composer_->InsertCharacter("u");

  EXPECT_EQ(composer_->GetStringForPreedit(), "あいう");

  composer_->MoveCursorLeft();
  composer_->SetOutputMode(transliteration::FULL_ASCII);
  EXPECT_EQ(composer_->GetStringForPreedit(), "ａｉｕ");

  composer_->InsertCharacter("a");
  composer_->InsertCharacter("i");
  composer_->InsertCharacter("u");
  EXPECT_EQ(composer_->GetStringForPreedit(), "ａｉｕあいう");
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
  EXPECT_EQ(composer_->GetStringForPreedit(), "！かｎ");
  EXPECT_EQ(composer_->GetStringForSubmission(), "！かｎ");
  EXPECT_EQ(composer_->GetQueryForConversion(), "!かん");
  EXPECT_EQ(composer_->GetQueryForPrediction(), "!か");

  // Query: "kas"
  composer_->EditErase();
  composer_->InsertCharacter("kas");

  EXPECT_EQ(composer_->GetStringForPreedit(), "かｓ");
  EXPECT_EQ(composer_->GetStringForSubmission(), "かｓ");
  // Pending chars should remain.  This is a test against
  // http://b/1799399
  EXPECT_EQ(composer_->GetQueryForConversion(), "かs");
  EXPECT_EQ(composer_->GetQueryForPrediction(), "か");

  // Query: "s"
  composer_->EditErase();
  composer_->InsertCharacter("s");

  EXPECT_EQ(composer_->GetStringForPreedit(), "ｓ");
  EXPECT_EQ(composer_->GetStringForSubmission(), "ｓ");
  EXPECT_EQ(composer_->GetQueryForConversion(), "s");
  EXPECT_EQ(composer_->GetQueryForPrediction(), "s");

  // Query: "sk"
  composer_->EditErase();
  composer_->InsertCharacter("sk");

  EXPECT_EQ(composer_->GetStringForPreedit(), "ｓｋ");
  EXPECT_EQ(composer_->GetStringForSubmission(), "ｓｋ");
  EXPECT_EQ(composer_->GetQueryForConversion(), "sk");
  EXPECT_EQ(composer_->GetQueryForPrediction(), "sk");
}

TEST_F(ComposerTest, GetQueryForPredictionHalfAscii) {
  // Dummy setup of romanji table.
  table_->AddRule("he", "へ", "");
  table_->AddRule("ll", "っｌ", "");
  table_->AddRule("lo", "ろ", "");

  // Switch to Half-Latin input mode.
  composer_->SetInputMode(transliteration::HALF_ASCII);

  {
    constexpr char kInput[] = "hello";
    composer_->InsertCharacter(kInput);
    EXPECT_EQ(composer_->GetQueryForPrediction(), kInput);
  }
  composer_->EditErase();
  {
    constexpr char kInput[] = "hello!";
    composer_->InsertCharacter(kInput);
    EXPECT_EQ(composer_->GetQueryForPrediction(), kInput);
  }
}

TEST_F(ComposerTest, GetQueryForPredictionFullAscii) {
  // Dummy setup of romanji table.
  table_->AddRule("he", "へ", "");
  table_->AddRule("ll", "っｌ", "");
  table_->AddRule("lo", "ろ", "");

  // Switch to Full-Latin input mode.
  composer_->SetInputMode(transliteration::FULL_ASCII);

  {
    composer_->InsertCharacter("ｈｅｌｌｏ");
    EXPECT_EQ(composer_->GetQueryForPrediction(), "hello");
  }
  composer_->EditErase();
  {
    composer_->InsertCharacter("ｈｅｌｌｏ！");
    EXPECT_EQ(composer_->GetQueryForPrediction(), "hello!");
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
    composer_->EditErase();
    composer_->InsertCharacter("us");
    // auto = std::pair<std::string, absl::btree_set<std::string>>
    const auto [base, expanded] = composer_->GetQueriesForPrediction();
    composer_->GetStringForPreedit();
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
    composer_->EditErase();
    composer_->InsertCharacter("_$");
    // auto = std::pair<std::string, absl::btree_set<std::string>>
    const auto [base, expanded] = composer_->GetQueriesForPrediction();
    composer_->GetStringForPreedit();
    EXPECT_EQ(base, "い");
    EXPECT_EQ(expanded.size(), 2);
    EXPECT_TRUE(expanded.end() != expanded.find("と"));
    EXPECT_TRUE(expanded.end() != expanded.find("ど"));
  }
  {
    composer_->EditErase();
    composer_->InsertCharacter("_$*");
    // auto = std::pair<std::string, absl::btree_set<std::string>>
    const auto [base, expanded] = composer_->GetQueriesForPrediction();
    composer_->GetStringForPreedit();
    EXPECT_EQ(base, "い");
    EXPECT_EQ(expanded.size(), 1);
    EXPECT_TRUE(expanded.end() != expanded.find("ど"));
  }
  {
    composer_->EditErase();
    composer_->InsertCharacter("_x*");
    // auto = std::pair<std::string, absl::btree_set<std::string>>
    const auto [base, expanded] = composer_->GetQueriesForPrediction();
    composer_->GetStringForPreedit();
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
  commands::KeyEvent key;
  key.set_key_string("[]");
  key.set_input_style(commands::KeyEvent::AS_IS);
  composer_->InsertCharacterKeyEvent(key);
  composer_->InsertCharacterKeyEvent(key);
  composer_->InsertCommandCharacter(composer::Composer::STOP_KEY_TOGGLING);
  // auto = std::pair<std::string, absl::btree_set<std::string>>
  const auto [base, expanded] = composer_->GetQueriesForPrediction();

  // Never reached here due to the crash before the fix for b/277163340.
  EXPECT_EQ(base, "[][]");
}

// Emulates tapping "[]" key twice without enough internval.
TEST_F(ComposerTest, DoubleTapSquareBracketPairKey) {
  commands::KeyEvent key;
  table_->AddRuleWithAttributes("[", "", "",
                                TableAttribute::NO_TRANSLITERATION);

  key.set_key_string("[]");
  key.set_input_style(commands::KeyEvent::AS_IS);
  composer_->InsertCharacterKeyEvent(key);
  composer_->InsertCharacterKeyEvent(key);
  EXPECT_EQ(composer_->GetStringForPreedit(), "[][]");
}

// Emulates tapping "[" key twice without enough internval.
TEST_F(ComposerTest, DoubleTapOpenSquareBracketKey) {
  commands::KeyEvent key;
  table_->AddRuleWithAttributes("[", "", "",
                                TableAttribute::NO_TRANSLITERATION);

  key.set_key_string("[");
  key.set_input_style(commands::KeyEvent::AS_IS);
  composer_->InsertCharacterKeyEvent(key);
  composer_->InsertCharacterKeyEvent(key);
  EXPECT_EQ(composer_->GetStringForPreedit(), "[[");
}

// Emulates tapping "[]" key twice with enough internval.
TEST_F(ComposerTest, DoubleTapSquareBracketPairKeyWithInterval) {
  commands::KeyEvent key;
  table_->AddRuleWithAttributes("[", "", "",
                                TableAttribute::NO_TRANSLITERATION);

  key.set_key_string("[]");
  key.set_input_style(commands::KeyEvent::AS_IS);
  composer_->InsertCharacterKeyEvent(key);
  composer_->InsertCommandCharacter(Composer::STOP_KEY_TOGGLING);
  composer_->InsertCharacterKeyEvent(key);
  EXPECT_EQ(composer_->GetStringForPreedit(), "[][]");
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
  EXPECT_EQ(composer_->GetStringForPreedit(), "ｎｙ［ＮＹＡ］ｎ");
  EXPECT_EQ(composer_->GetStringForSubmission(), "ｎｙ［ＮＹＡ］ｎ");
  EXPECT_EQ(composer_->GetQueryForConversion(), "ny[NYA][N]");
  EXPECT_EQ(composer_->GetQueryForPrediction(), "ny[NYA]");

  composer_->InsertCharacter("ka");
  EXPECT_EQ(composer_->GetQueryForConversion(), "ny[NYA][N][KA]");
  EXPECT_EQ(composer_->GetQueryForPrediction(), "ny[NYA][N][KA]");
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
    std::string key;
    for (char c = 0x20; c <= 0x7E; ++c) {
      key.assign(1, c);
      composer_->EditErase();
      composer_->InsertCharacter(key);
      if (test_data.ascii_expected) {
        EXPECT_EQ(composer_->GetStringForPreedit(), key);
        EXPECT_EQ(composer_->GetStringForSubmission(), key);
      } else {
        // Expected result is FULL_WIDTH form.
        // Typically the result is a full-width form of the key,
        // but some characters are not.
        // So here we checks only the character form.
        EXPECT_EQ(Util::GetFormType(composer_->GetStringForPreedit()),
                  Util::FULL_WIDTH);
        EXPECT_EQ(Util::GetFormType(composer_->GetStringForSubmission()),
                  Util::FULL_WIDTH);
      }
    }
  }
}

TEST_F(ComposerTest, InsertCommandCharacter) {
  table_->AddRule("{<}", "REWIND", "");
  table_->AddRule("{!}", "STOP", "");

  composer_->SetInputMode(transliteration::HALF_ASCII);

  composer_->InsertCommandCharacter(Composer::REWIND);
  EXPECT_EQ(composer_->GetStringForPreedit(), "REWIND");

  composer_->Reset();
  composer_->InsertCommandCharacter(Composer::STOP_KEY_TOGGLING);
  EXPECT_EQ(composer_->GetStringForPreedit(), "STOP");
}

TEST_F(ComposerTest, InsertCharacterKeyEvent) {
  commands::KeyEvent key;
  table_->AddRule("a", "あ", "");

  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);

  EXPECT_EQ(composer_->GetStringForPreedit(), "あ");

  // Half width "A" will be inserted.
  key.set_key_code('A');
  composer_->InsertCharacterKeyEvent(key);

  EXPECT_EQ(composer_->GetStringForPreedit(), "あA");

  // Half width "a" will be inserted.
  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);

  EXPECT_EQ(composer_->GetStringForPreedit(), "あAa");

  // Reset() should revert the previous input mode (Hiragana).
  composer_->Reset();

  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);
  EXPECT_EQ(composer_->GetStringForPreedit(), "あ");

  // Typing "A" temporarily switch the input mode.  The input mode
  // should be reverted back after reset.
  composer_->SetInputMode(transliteration::FULL_KATAKANA);
  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);
  EXPECT_EQ(composer_->GetStringForPreedit(), "あア");

  key.set_key_code('A');
  composer_->InsertCharacterKeyEvent(key);
  EXPECT_EQ(composer_->GetStringForPreedit(), "あアA");

  // Reset() should revert the previous input mode (Katakana).
  composer_->Reset();

  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);
  EXPECT_EQ(composer_->GetStringForPreedit(), "ア");
}

namespace {
constexpr char kYama[] = "山";
constexpr char kKawa[] = "川";
constexpr char kSora[] = "空";
}  // namespace

TEST_F(ComposerTest, InsertCharacterKeyEventWithCodepointKeyCode) {
  commands::KeyEvent key;

  // Input "山" as key_code.
  key.set_key_code(0x5C71);  // U+5C71 = "山"
  composer_->InsertCharacterKeyEvent(key);

  EXPECT_EQ(composer_->GetStringForPreedit(), kYama);

  // Input "山" as key_code which is converted to "川".
  table_->AddRule(kYama, kKawa, "");
  composer_->Reset();
  composer_->InsertCharacterKeyEvent(key);
  EXPECT_EQ(composer_->GetStringForPreedit(), kKawa);

  // Input ("山", "空") as (key_code, key_string) which is treated as "空".
  key.set_key_string(kSora);
  composer_->Reset();
  composer_->InsertCharacterKeyEvent(key);
  EXPECT_EQ(composer_->GetStringForPreedit(), kSora);
}

TEST_F(ComposerTest, InsertCharacterKeyEventWithoutKeyCode) {
  commands::KeyEvent key;

  // Input "山" as key_string.  key_code is empty.
  key.set_key_string(kYama);
  composer_->InsertCharacterKeyEvent(key);
  EXPECT_FALSE(key.has_key_code());

  EXPECT_EQ(composer_->GetStringForPreedit(), kYama);

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

  EXPECT_EQ(composer_->GetStringForPreedit(), "あ");

  // Full width "０" will be inserted.
  key.set_key_code('0');
  key.set_key_string("0");
  composer_->InsertCharacterKeyEvent(key);

  EXPECT_EQ(composer_->GetStringForPreedit(), "あ０");

  // Half width "0" will be inserted.
  key.set_key_code('0');
  key.set_key_string("0");
  key.set_input_style(commands::KeyEvent::AS_IS);
  composer_->InsertCharacterKeyEvent(key);

  EXPECT_EQ(composer_->GetStringForPreedit(), "あ０0");

  // Full width "0" will be inserted.
  key.set_key_code('0');
  key.set_key_string("0");
  key.set_input_style(commands::KeyEvent::FOLLOW_MODE);
  composer_->InsertCharacterKeyEvent(key);

  EXPECT_EQ(composer_->GetStringForPreedit(), "あ０0０");

  // Half width "-" will be inserted.
  key.set_key_code('-');
  key.set_key_string("-");
  key.set_input_style(commands::KeyEvent::AS_IS);
  composer_->InsertCharacterKeyEvent(key);

  EXPECT_EQ(composer_->GetStringForPreedit(), "あ０0０-");

  // Full width "−" (U+2212) will be inserted.
  key.set_key_code('-');
  key.set_key_string("−");
  key.set_input_style(commands::KeyEvent::FOLLOW_MODE);
  composer_->InsertCharacterKeyEvent(key);

  EXPECT_EQ(composer_->GetStringForPreedit(),
            "あ０0０-−");  // The last hyphen is U+2212.
}

TEST_F(ComposerTest, InsertCharacterKeyEventWithInputMode) {
  table_->AddRule("a", "あ", "");
  table_->AddRule("i", "い", "");
  table_->AddRule("u", "う", "");

  {
    // "a" → "あ" (Hiragana)
    EXPECT_TRUE(InsertKeyWithMode("a", commands::HIRAGANA, composer_.get()));
    EXPECT_EQ(composer_->GetStringForPreedit(), "あ");
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);

    // "aI" → "あI" (Alphanumeric)
    EXPECT_TRUE(InsertKeyWithMode("I", commands::HIRAGANA, composer_.get()));
    EXPECT_EQ(composer_->GetStringForPreedit(), "あI");
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);

    // "u" → "あIu" (Alphanumeric)
    EXPECT_TRUE(InsertKeyWithMode("u", commands::HALF_ASCII, composer_.get()));
    EXPECT_EQ(composer_->GetStringForPreedit(), "あIu");
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);

    // [shift] → "あIu" (Hiragana)
    EXPECT_TRUE(
        InsertKeyWithMode("Shift", commands::HALF_ASCII, composer_.get()));
    EXPECT_EQ(composer_->GetStringForPreedit(), "あIu");
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);

    // "u" → "あIuう" (Hiragana)
    EXPECT_TRUE(InsertKeyWithMode("u", commands::HIRAGANA, composer_.get()));
    EXPECT_EQ(composer_->GetStringForPreedit(), "あIuう");
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
  }

  composer_ = std::make_unique<Composer>(table_, request_, config_);

  {
    // "a" → "あ" (Hiragana)
    EXPECT_TRUE(InsertKeyWithMode("a", commands::HIRAGANA, composer_.get()));
    EXPECT_EQ(composer_->GetStringForPreedit(), "あ");
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);

    // "i" (Katakana) → "あイ" (Katakana)
    EXPECT_TRUE(
        InsertKeyWithMode("i", commands::FULL_KATAKANA, composer_.get()));
    EXPECT_EQ(composer_->GetStringForPreedit(), "あイ");
    EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_KATAKANA);

    // SetInputMode(Alphanumeric) → "あイ" (Alphanumeric)
    composer_->SetInputMode(transliteration::HALF_ASCII);
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);

    // [shift] → "あイ" (Alphanumeric) - Nothing happens.
    EXPECT_TRUE(
        InsertKeyWithMode("Shift", commands::HALF_ASCII, composer_.get()));
    EXPECT_EQ(composer_->GetStringForPreedit(), "あイ");
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);

    // "U" → "あイ" (Alphanumeric)
    EXPECT_TRUE(InsertKeyWithMode("U", commands::HALF_ASCII, composer_.get()));
    EXPECT_EQ(composer_->GetStringForPreedit(), "あイU");
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);

    // [shift] → "あイU" (Alphanumeric) - Nothing happens.
    EXPECT_TRUE(
        InsertKeyWithMode("Shift", commands::HALF_ASCII, composer_.get()));
    EXPECT_EQ(composer_->GetStringForPreedit(), "あイU");
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
  EXPECT_EQ(composer_->GetStringForPreedit(), "あ");
  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "い");

  composer_->Reset();

  // b/31444698.  Test if "かか" is transliterated to "き"
  ASSERT_TRUE(InsertKeyWithMode("か", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "か");
  ASSERT_TRUE(InsertKeyWithMode("か", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "き");
}

TEST_F(ComposerTest, Copy) {
  table_->AddRule("a", "あ", "");
  table_->AddRule("n", "ん", "");
  table_->AddRule("na", "な", "");

  {
    SCOPED_TRACE("Precomposition");

    EXPECT_EQ(composer_->GetStringForSubmission(), "");

    Composer dest = *composer_;
    ExpectSameComposer(*composer_, dest);
  }

  {
    SCOPED_TRACE("Composition");

    composer_->InsertCharacter("a");
    composer_->InsertCharacter("n");
    EXPECT_EQ(composer_->GetStringForSubmission(), "あｎ");

    Composer dest = *composer_;
    ExpectSameComposer(*composer_, dest);
  }

  {
    SCOPED_TRACE("Conversion");

    EXPECT_EQ(composer_->GetQueryForConversion(), "あん");

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
    EXPECT_EQ(composer_->GetStringForSubmission(), "AaAAあ");

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
    EXPECT_EQ(composer_->GetStringForSubmission(), "M");

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

    EXPECT_EQ(composer_->GetStringForPreedit(), "あAaああ");
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

    EXPECT_EQ(composer_->GetStringForPreedit(), "アAaアア");
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

    EXPECT_EQ(composer_->GetStringForPreedit(), "AAあAa");
  }

  {  // Multiple shifted characters #2
    composer_->SetInputMode(transliteration::HIRAGANA);
    composer_->Reset();
    InsertKey("D", composer_.get());  // "D"
    InsertKey("&", composer_.get());  // "D&"
    InsertKey("D", composer_.get());  // "D&D"
    InsertKey("2", composer_.get());  // "D&D2"
    InsertKey("a", composer_.get());  // "D&D2a"

    EXPECT_EQ(composer_->GetStringForPreedit(), "D&D2a");
  }

  {  // Full-witdh alphanumeric
    composer_->SetInputMode(transliteration::FULL_ASCII);
    composer_->Reset();
    InsertKey("A", composer_.get());  // "Ａ"
    InsertKey("a", composer_.get());  // "Ａａ"

    EXPECT_EQ(composer_->GetStringForPreedit(), "Ａａ");
  }

  {  // Half-witdh alphanumeric
    composer_->SetInputMode(transliteration::HALF_ASCII);
    composer_->Reset();
    InsertKey("A", composer_.get());  // "A"
    InsertKey("a", composer_.get());  // "Aa"

    EXPECT_EQ(composer_->GetStringForPreedit(), "Aa");
  }
}

TEST_F(ComposerTest, ShiftKeyOperationForKatakana) {
  config_->set_shift_key_mode_switch(Config::KATAKANA_INPUT_MODE);
  table_->InitializeWithRequestAndConfig(*request_, *config_);
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

  // NOTE(komatsu): "KATakAna" is converted to "カＴあｋアな" rather
  // than "カタカな".  This is a different behavior from Kotoeri due
  // to avoid complecated implementation.  Unless this is a problem
  // for users, this difference probably remains.
  //
  // EXPECT_EQ(composer_->GetStringForPreedit(), "カタカな");

  EXPECT_EQ(composer_->GetStringForPreedit(), "カＴあｋアな");
}

TEST_F(ComposerTest, AutoIMETurnOffEnabled) {
  config_->set_preedit_method(Config::ROMAN);
  config_->set_use_auto_ime_turn_off(true);

  table_->InitializeWithRequestAndConfig(*request_, *config_);

  commands::KeyEvent key;

  {  // http
    InsertKey("h", composer_.get());
    InsertKey("t", composer_.get());
    InsertKey("t", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
    InsertKey("p", composer_.get());

    EXPECT_EQ(composer_->GetStringForPreedit(), "http");
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);

    composer_->Reset();
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
  }

  composer_ = std::make_unique<Composer>(table_, request_, config_);

  {  // google
    InsertKey("g", composer_.get());
    InsertKey("o", composer_.get());
    InsertKey("o", composer_.get());
    InsertKey("g", composer_.get());
    InsertKey("l", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
    InsertKey("e", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
    EXPECT_EQ(composer_->GetStringForPreedit(), "google");

    InsertKey("a", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
    EXPECT_EQ(composer_->GetStringForPreedit(), "googleあ");

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

    EXPECT_EQ(composer_->GetStringForPreedit(), "ｇｏｏｇｌｅ");

    InsertKey("a", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::FULL_ASCII);
    EXPECT_EQ(composer_->GetStringForPreedit(), "ｇｏｏｇｌｅａ");

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
    EXPECT_EQ(composer_->GetStringForPreedit(), "Google");

    InsertKey("a", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);
    EXPECT_EQ(composer_->GetStringForPreedit(), "Googlea");

    composer_->Reset();
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
  }

  config_->set_shift_key_mode_switch(Config::OFF);
  composer_ = std::make_unique<Composer>(table_, request_, config_);

  {  // Google
    InsertKey("G", composer_.get());
    InsertKey("o", composer_.get());
    InsertKey("o", composer_.get());
    InsertKey("g", composer_.get());
    InsertKey("l", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
    InsertKey("e", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
    EXPECT_EQ(composer_->GetStringForPreedit(), "Google");

    InsertKey("a", composer_.get());
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
    EXPECT_EQ(composer_->GetStringForPreedit(), "Googleあ");

    composer_->Reset();
    EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
  }
}

TEST_F(ComposerTest, AutoIMETurnOffDisabled) {
  config_->set_preedit_method(Config::ROMAN);
  config_->set_use_auto_ime_turn_off(false);

  table_->InitializeWithRequestAndConfig(*request_, *config_);

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

  EXPECT_EQ(composer_->GetStringForPreedit(), "ｈっｔｐ：・・");
}

TEST_F(ComposerTest, AutoIMETurnOffKana) {
  config_->set_preedit_method(Config::KANA);
  config_->set_use_auto_ime_turn_off(true);

  table_->InitializeWithRequestAndConfig(*request_, *config_);

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

  EXPECT_EQ(composer_->GetStringForPreedit(), "ｈっｔｐ：・・");
}

TEST_F(ComposerTest, KanaPrediction) {
  composer_->InsertCharacterKeyAndPreedit("t", "か");
  EXPECT_EQ(composer_->GetQueryForPrediction(), "か");

  composer_->InsertCharacterKeyAndPreedit("\\", "ー");
  EXPECT_EQ(composer_->GetQueryForPrediction(), "かー");

  composer_->InsertCharacterKeyAndPreedit(",", "、");
  EXPECT_EQ(composer_->GetQueryForPrediction(), "かー、");
}

TEST_F(ComposerTest, KanaTransliteration) {
  table_->AddRule("く゛", "ぐ", "");
  composer_->InsertCharacterKeyAndPreedit("h", "く");
  composer_->InsertCharacterKeyAndPreedit("e", "い");
  composer_->InsertCharacterKeyAndPreedit("l", "り");
  composer_->InsertCharacterKeyAndPreedit("l", "り");
  composer_->InsertCharacterKeyAndPreedit("o", "ら");

  EXPECT_EQ(composer_->GetStringForPreedit(), "くいりりら");

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

  EXPECT_EQ(composer_->GetStringForPreedit(), "もず");
  EXPECT_EQ(composer_->GetCursor(), 2);

  composer_->SetOutputMode(transliteration::HALF_ASCII);
  EXPECT_EQ(composer_->GetStringForPreedit(), "mozu");
  EXPECT_EQ(composer_->GetCursor(), 4);

  composer_->SetOutputMode(transliteration::HALF_KATAKANA);
  EXPECT_EQ(composer_->GetStringForPreedit(), "ﾓｽﾞ");
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

  EXPECT_EQ(composer_->GetStringForPreedit(), "AIあいａｉ");

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
  auto request = std::make_shared<commands::Request>();
  request->set_update_input_mode_from_surrounding_text(false);
  composer_->SetRequest(request);

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

  EXPECT_EQ(composer_->GetStringForPreedit(), "AIあいａｉ");

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
    EXPECT_EQ(composer_->GetStringForPreedit(), "3.14");
  }

  {
    composer_->Reset();
    manager->SetDefaultRule();
    manager->AddPreeditRule("1", Config::FULL_WIDTH);
    manager->AddPreeditRule(".,", Config::HALF_WIDTH);
    composer_->InsertCharacter("3.14");
    EXPECT_EQ(composer_->GetStringForPreedit(), "３.１４");
  }

  {
    composer_->Reset();
    manager->SetDefaultRule();
    manager->AddPreeditRule("1", Config::HALF_WIDTH);
    manager->AddPreeditRule(".,", Config::FULL_WIDTH);
    composer_->InsertCharacter("3.14");
    EXPECT_EQ(composer_->GetStringForPreedit(), "3．14");
  }

  {
    composer_->Reset();
    manager->SetDefaultRule();
    manager->AddPreeditRule("1", Config::FULL_WIDTH);
    manager->AddPreeditRule(".,", Config::FULL_WIDTH);
    composer_->InsertCharacter("3.14");
    EXPECT_EQ(composer_->GetStringForPreedit(), "３．１４");
  }

  {
    composer_->Reset();
    manager->SetDefaultRule();
    composer_->InsertCharacter("-123");
    EXPECT_EQ(composer_->GetStringForPreedit(), "−１２３");
    EXPECT_EQ(composer_->GetRawString(), "-123");
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

  EXPECT_EQ(composer_->GetStringForPreedit(),
            "−１，０００．５");  // The hyphen is U+2212.
  EXPECT_EQ(composer_->GetStringForSubmission(),
            "−１，０００．５");  // The hyphen is U+2212.
  EXPECT_EQ(composer_->GetQueryForConversion(), "-1,000.5");
  EXPECT_EQ(composer_->GetQueryForPrediction(), "-1,000.5");
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
  EXPECT_EQ(composer_->GetStringForPreedit(), "［Ｘ］ｓｈ");

  EXPECT_EQ(composer_->GetQueryForConversion(), "[X]sh");

  transliteration::Transliterations t13ns;
  composer_->GetTransliterations(&t13ns);
  EXPECT_EQ(t13ns[transliteration::HALF_ASCII], "ssh");
}

TEST_F(ComposerTest, TransliterationsOfNegativeNumber) {
  table_->AddRule("-", "ー", "");  // U+30FC (prolonged sound mark)
  table_->AddRule("1", "１", "");
  composer_->InsertCharacter("-1");
  EXPECT_EQ(composer_->GetStringForPreedit(), "−１");  // U+2212 (minus)

  transliteration::Transliterations t13ns;
  composer_->GetTransliterations(&t13ns);
  // U+002D (hyphen-minus)
  EXPECT_EQ(t13ns[transliteration::HALF_ASCII], "-1");
  // U+2212 (minus)
  EXPECT_EQ(t13ns[transliteration::FULL_ASCII], "−１");
  // U+30FC (prolonged sound mark, 長音)
  EXPECT_EQ(t13ns[transliteration::HIRAGANA], "ー１");
}

TEST_F(ComposerTest, Issue2190364) {
  // This is a unittest against http://b/2190364
  commands::KeyEvent key;
  key.set_key_code('a');
  key.set_key_string("ち");

  // Toggle the input mode to HALF_ASCII
  composer_->ToggleInputMode();
  EXPECT_TRUE(composer_->InsertCharacterKeyEvent(key));
  EXPECT_EQ(composer_->GetStringForPreedit(), "a");

  // Insertion of a space and backspace it should not change the composition.
  composer_->InsertCharacter(" ");
  EXPECT_EQ(composer_->GetStringForPreedit(), "a ");

  composer_->Backspace();
  EXPECT_EQ(composer_->GetStringForPreedit(), "a");

  // Toggle the input mode to HIRAGANA, the preedit should not be changed.
  composer_->ToggleInputMode();
  EXPECT_EQ(composer_->GetStringForPreedit(), "a");

  // "a" should be converted to "ち" on Hiragana input mode.
  EXPECT_TRUE(composer_->InsertCharacterKeyEvent(key));
  EXPECT_EQ(composer_->GetStringForPreedit(), "aち");
}

TEST_F(ComposerTest, Issue1817410) {
  // This is a unittest against http://b/2190364
  table_->AddRule("ss", "っ", "s");

  InsertKey("s", composer_.get());
  InsertKey("s", composer_.get());

  EXPECT_EQ(composer_->GetStringForPreedit(), "っｓ");

  std::string t13n =
      composer_->GetSubTransliteration(transliteration::HALF_ASCII, 0, 2);
  EXPECT_EQ(t13n, "ss");

  t13n = composer_->GetSubTransliteration(transliteration::HALF_ASCII, 0, 1);
  EXPECT_EQ(t13n, "s");

  t13n = composer_->GetSubTransliteration(transliteration::HALF_ASCII, 1, 1);
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

  EXPECT_EQ(composer_->GetQueryForConversion(), "ny");
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

  EXPECT_EQ(composer_->GetQueryForConversion(), "ぽny");
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

  EXPECT_EQ(composer_->GetQueryForConversion(), "zny");
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

  EXPECT_EQ(composer_->GetStringForPreedit(), "C:\\Wi");
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

  EXPECT_EQ(composer_->GetStringForPreedit(), "C:Wi");
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
  EXPECT_EQ(composer_->GetStringForPreedit(), "C:\\Wiい");
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

  EXPECT_EQ(composer_->GetStringForPreedit(), "c:\\Wi");
}

TEST_F(ComposerTest, CaseSensitiveByConfiguration) {
  {
    config_->set_shift_key_mode_switch(Config::OFF);
    table_->InitializeWithRequestAndConfig(*request_, *config_);

    table_->AddRule("i", "い", "");
    table_->AddRule("I", "イ", "");

    InsertKey("i", composer_.get());
    InsertKey("I", composer_.get());
    InsertKey("i", composer_.get());
    InsertKey("I", composer_.get());
    EXPECT_EQ(composer_->GetStringForPreedit(), "いイいイ");
  }
  composer_->Reset();
  {
    config_->set_shift_key_mode_switch(Config::ASCII_INPUT_MODE);
    table_->InitializeWithRequestAndConfig(*request_, *config_);

    table_->AddRule("i", "い", "");
    table_->AddRule("I", "イ", "");

    InsertKey("i", composer_.get());
    InsertKey("I", composer_.get());
    InsertKey("i", composer_.get());
    InsertKey("I", composer_.get());
    EXPECT_EQ(composer_->GetStringForPreedit(), "いIiI");
  }
}

TEST_F(ComposerTest,
       InputUppercaseInAlphanumericModeWithShiftKeyModeSwitchIsKatakana) {
  {
    config_->set_shift_key_mode_switch(Config::KATAKANA_INPUT_MODE);
    table_->InitializeWithRequestAndConfig(*request_, *config_);

    table_->AddRule("i", "い", "");
    table_->AddRule("I", "イ", "");

    {
      composer_->Reset();
      composer_->SetInputMode(transliteration::FULL_ASCII);
      InsertKey("I", composer_.get());
      EXPECT_EQ(composer_->GetStringForPreedit(), "Ｉ");
    }

    {
      composer_->Reset();
      composer_->SetInputMode(transliteration::HALF_ASCII);
      InsertKey("I", composer_.get());
      EXPECT_EQ(composer_->GetStringForPreedit(), "I");
    }

    {
      composer_->Reset();
      composer_->SetInputMode(transliteration::FULL_KATAKANA);
      InsertKey("I", composer_.get());
      EXPECT_EQ(composer_->GetStringForPreedit(), "イ");
    }

    {
      composer_->Reset();
      composer_->SetInputMode(transliteration::HALF_KATAKANA);
      InsertKey("I", composer_.get());
      EXPECT_EQ(composer_->GetStringForPreedit(), "ｲ");
    }

    {
      composer_->Reset();
      composer_->SetInputMode(transliteration::HIRAGANA);
      InsertKey("I", composer_.get());
      EXPECT_EQ(composer_->GetStringForPreedit(), "イ");
    }
  }
}

TEST_F(ComposerTest, DeletingAlphanumericPartShouldQuitToggleAlphanumericMode) {
  // http://b/2206560
  // 1. Type "iGoogle" (preedit text turns to be "いGoogle")
  // 2. Type Back-space 6 times ("い")
  // 3. Type "i" (should be "いい")

  table_->InitializeWithRequestAndConfig(*request_, *config_);

  table_->AddRule("i", "い", "");

  InsertKey("i", composer_.get());
  InsertKey("G", composer_.get());
  InsertKey("o", composer_.get());
  InsertKey("o", composer_.get());
  InsertKey("g", composer_.get());
  InsertKey("l", composer_.get());
  InsertKey("e", composer_.get());

  EXPECT_EQ(composer_->GetStringForPreedit(), "いGoogle");

  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();

  EXPECT_EQ(composer_->GetStringForPreedit(), "い");

  InsertKey("i", composer_.get());

  EXPECT_EQ(composer_->GetStringForPreedit(), "いい");
}

TEST_F(ComposerTest, InputModesChangeWhenCursorMoves) {
  // The expectation of this test is the same as MS-IME's

  table_->InitializeWithRequestAndConfig(*request_, *config_);

  table_->AddRule("i", "い", "");
  table_->AddRule("gi", "ぎ", "");

  InsertKey("i", composer_.get());
  composer_->MoveCursorRight();
  EXPECT_EQ(composer_->GetStringForPreedit(), "い");

  composer_->MoveCursorLeft();
  EXPECT_EQ(composer_->GetStringForPreedit(), "い");

  InsertKey("G", composer_.get());
  EXPECT_EQ(composer_->GetStringForPreedit(), "Gい");

  composer_->MoveCursorRight();
  EXPECT_EQ(composer_->GetStringForPreedit(), "Gい");

  InsertKey("G", composer_.get());
  EXPECT_EQ(composer_->GetStringForPreedit(), "GいG");

  composer_->MoveCursorLeft();
  InsertKey("i", composer_.get());
  EXPECT_EQ(composer_->GetStringForPreedit(), "GいいG");

  composer_->MoveCursorRight();
  InsertKey("i", composer_.get());
  EXPECT_EQ(composer_->GetStringForPreedit(), "GいいGi");

  InsertKey("G", composer_.get());
  InsertKey("i", composer_.get());
  EXPECT_EQ(composer_->GetStringForPreedit(), "GいいGiGi");

  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  InsertKey("i", composer_.get());
  EXPECT_EQ(composer_->GetStringForPreedit(), "GいいGi");

  InsertKey("G", composer_.get());
  InsertKey("G", composer_.get());
  composer_->MoveCursorRight();
  InsertKey("i", composer_.get());
  EXPECT_EQ(composer_->GetStringForPreedit(), "GいいGiGGi");

  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  InsertKey("i", composer_.get());
  EXPECT_EQ(composer_->GetStringForPreedit(), "Gい");

  composer_->Backspace();
  composer_->MoveCursorLeft();
  composer_->MoveCursorRight();
  InsertKey("i", composer_.get());
  EXPECT_EQ(composer_->GetStringForPreedit(), "Gi");
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
  EXPECT_EQ(composer_->GetStringForPreedit(), "mozc");
  EXPECT_EQ(composer_->source_text(), "MOZC");

  composer_->Backspace();
  composer_->Backspace();
  EXPECT_FALSE(composer_->Empty());
  EXPECT_EQ(composer_->GetStringForPreedit(), "mo");
  EXPECT_EQ(composer_->source_text(), "MOZC");

  composer_->Reset();
  EXPECT_TRUE(composer_->Empty());
  EXPECT_TRUE(composer_->source_text().empty());
}

TEST_F(ComposerTest, DeleteAt) {
  table_->AddRule("mo", "も", "");
  table_->AddRule("zu", "ず", "");

  composer_->InsertCharacter("z");
  EXPECT_EQ(composer_->GetStringForPreedit(), "ｚ");
  EXPECT_EQ(composer_->GetCursor(), 1);
  composer_->DeleteAt(0);
  EXPECT_EQ(composer_->GetStringForPreedit(), "");
  EXPECT_EQ(composer_->GetCursor(), 0);

  composer_->InsertCharacter("mmoz");
  EXPECT_EQ(composer_->GetStringForPreedit(), "ｍもｚ");
  EXPECT_EQ(composer_->GetCursor(), 3);
  composer_->DeleteAt(0);
  EXPECT_EQ(composer_->GetStringForPreedit(), "もｚ");
  EXPECT_EQ(composer_->GetCursor(), 2);
  composer_->InsertCharacter("u");
  EXPECT_EQ(composer_->GetStringForPreedit(), "もず");
  EXPECT_EQ(composer_->GetCursor(), 2);

  composer_->InsertCharacter("m");
  EXPECT_EQ(composer_->GetStringForPreedit(), "もずｍ");
  EXPECT_EQ(composer_->GetCursor(), 3);
  composer_->DeleteAt(1);
  EXPECT_EQ(composer_->GetStringForPreedit(), "もｍ");
  EXPECT_EQ(composer_->GetCursor(), 2);
  composer_->InsertCharacter("o");
  EXPECT_EQ(composer_->GetStringForPreedit(), "もも");
  EXPECT_EQ(composer_->GetCursor(), 2);
}

TEST_F(ComposerTest, DeleteRange) {
  table_->AddRule("mo", "も", "");
  table_->AddRule("zu", "ず", "");

  composer_->InsertCharacter("z");
  EXPECT_EQ(composer_->GetStringForPreedit(), "ｚ");
  EXPECT_EQ(composer_->GetCursor(), 1);

  composer_->DeleteRange(0, 1);
  EXPECT_EQ(composer_->GetStringForPreedit(), "");
  EXPECT_EQ(composer_->GetCursor(), 0);

  composer_->InsertCharacter("mmozmoz");
  EXPECT_EQ(composer_->GetStringForPreedit(), "ｍもｚもｚ");
  EXPECT_EQ(composer_->GetCursor(), 5);

  composer_->DeleteRange(0, 3);
  EXPECT_EQ(composer_->GetStringForPreedit(), "もｚ");
  EXPECT_EQ(composer_->GetCursor(), 2);

  composer_->InsertCharacter("u");
  EXPECT_EQ(composer_->GetStringForPreedit(), "もず");
  EXPECT_EQ(composer_->GetCursor(), 2);

  composer_->InsertCharacter("xyz");
  composer_->MoveCursorToBeginning();
  composer_->InsertCharacter("mom");
  EXPECT_EQ(composer_->GetStringForPreedit(), "もｍもずｘｙｚ");
  EXPECT_EQ(composer_->GetCursor(), 2);

  composer_->DeleteRange(2, 3);
  // "もｍ|ｙｚ"
  EXPECT_EQ(composer_->GetStringForPreedit(), "もｍｙｚ");
  EXPECT_EQ(composer_->GetCursor(), 2);

  composer_->InsertCharacter("o");
  // "もも|ｙｚ"
  EXPECT_EQ(composer_->GetStringForPreedit(), "ももｙｚ");
  EXPECT_EQ(composer_->GetCursor(), 2);

  composer_->DeleteRange(2, 1000);
  // "もも|"
  EXPECT_EQ(composer_->GetStringForPreedit(), "もも");
  EXPECT_EQ(composer_->GetCursor(), 2);
}

TEST_F(ComposerTest, 12KeysAsciiGetQueryForPrediction) {
  // http://b/5509480
  auto request = std::make_shared<commands::Request>();
  request->set_zero_query_suggestion(true);
  request->set_mixed_conversion(true);
  request->set_special_romanji_table(
      commands::Request::TWELVE_KEYS_TO_HALFWIDTHASCII);
  composer_->SetRequest(request);
  table_->InitializeWithRequestAndConfig(
      *request, config::ConfigHandler::DefaultConfig());
  composer_->InsertCharacter("2");
  EXPECT_EQ(composer_->GetStringForPreedit(), "a");
  EXPECT_EQ(composer_->GetQueryForConversion(), "a");
  EXPECT_EQ(composer_->GetQueryForPrediction(), "a");
}

TEST_F(ComposerTest, InsertCharacterPreedit) {
  constexpr char kTestStr[] = "ああaｋka。";

  {
    composer_->InsertCharacterPreedit(kTestStr);
    const std::string preedit = composer_->GetStringForPreedit();
    const std::string conversion_query = composer_->GetQueryForConversion();
    const std::string prediction_query = composer_->GetQueryForPrediction();
    // auto = std::pair<std::string, absl::btree_set<std::string>>
    const auto [base, expanded] = composer_->GetQueriesForPrediction();
    EXPECT_FALSE(preedit.empty());
    EXPECT_FALSE(conversion_query.empty());
    EXPECT_FALSE(prediction_query.empty());
    EXPECT_FALSE(base.empty());
  }
  composer_->Reset();
  {
    for (const std::string &c : Util::SplitStringToUtf8Chars(kTestStr)) {
      composer_->InsertCharacterPreedit(c);
    }
    const std::string preedit = composer_->GetStringForPreedit();
    const std::string conversion_query = composer_->GetQueryForConversion();
    const std::string prediction_query = composer_->GetQueryForPrediction();
    // auto = std::pair<std::string, absl::btree_set<std::string>>
    const auto [base, expanded] = composer_->GetQueriesForPrediction();
    EXPECT_FALSE(preedit.empty());
    EXPECT_FALSE(conversion_query.empty());
    EXPECT_FALSE(prediction_query.empty());
    EXPECT_FALSE(base.empty());
  }
}

TEST_F(ComposerTest, GetRawString) {
  table_->AddRule("sa", "さ", "");
  table_->AddRule("shi", "し", "");
  table_->AddRule("mi", "み", "");

  composer_->SetOutputMode(transliteration::HIRAGANA);

  composer_->InsertCharacter("sashimi");

  EXPECT_EQ(composer_->GetStringForPreedit(), "さしみ");

  std::string raw_string = composer_->GetRawString();
  EXPECT_EQ(raw_string, "sashimi");

  std::string raw_sub_string = composer_->GetRawSubString(0, 2);
  EXPECT_EQ(raw_sub_string, "sashi");

  raw_sub_string = composer_->GetRawSubString(1, 1);
  EXPECT_EQ(raw_sub_string, "shi");
}

TEST_F(ComposerTest, SetPreeditTextForTestOnly) {
  composer_->SetPreeditTextForTestOnly("も");

  EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
  EXPECT_EQ(composer_->GetStringForPreedit(), "も");
  EXPECT_EQ(composer_->GetStringForSubmission(), "も");
  EXPECT_EQ(composer_->GetQueryForConversion(), "も");
  EXPECT_EQ(composer_->GetQueryForPrediction(), "も");

  // auto = std::pair<std::string, absl::btree_set<std::string>>
  const auto [output1, expanded1] = composer_->GetQueriesForPrediction();
  EXPECT_EQ(output1, "も");
  EXPECT_TRUE(expanded1.empty());

  composer_->Reset();

  composer_->SetPreeditTextForTestOnly("mo");

  EXPECT_EQ(composer_->GetInputMode(), transliteration::HALF_ASCII);
  EXPECT_EQ(composer_->GetStringForPreedit(), "mo");
  EXPECT_EQ(composer_->GetStringForSubmission(), "mo");
  EXPECT_EQ(composer_->GetQueryForConversion(), "mo");
  EXPECT_EQ(composer_->GetQueryForPrediction(), "mo");

  // auto = std::pair<std::string, absl::btree_set<std::string>>
  const auto [output2, expanded2] = composer_->GetQueriesForPrediction();
  EXPECT_EQ(output2, "mo");

  EXPECT_TRUE(expanded2.empty());

  composer_->Reset();

  composer_->SetPreeditTextForTestOnly("ｍ");

  EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);
  EXPECT_EQ(composer_->GetStringForPreedit(), "ｍ");
  EXPECT_EQ(composer_->GetStringForSubmission(), "ｍ");
  EXPECT_EQ(composer_->GetQueryForConversion(), "m");
  EXPECT_EQ(composer_->GetQueryForPrediction(), "m");

  // auto = std::pair<std::string, absl::btree_set<std::string>>
  const auto [output3, expanded3] = composer_->GetQueriesForPrediction();
  EXPECT_EQ(output3, "m");

  EXPECT_TRUE(expanded3.empty());

  composer_->Reset();

  composer_->SetPreeditTextForTestOnly("もｚ");

  EXPECT_EQ(composer_->GetInputMode(), transliteration::HIRAGANA);

  EXPECT_EQ(composer_->GetStringForPreedit(), "もｚ");
  EXPECT_EQ(composer_->GetStringForSubmission(), "もｚ");
  EXPECT_EQ(composer_->GetQueryForConversion(), "もz");
  EXPECT_EQ(composer_->GetQueryForPrediction(), "もz");

  // auto = std::pair<std::string, absl::btree_set<std::string>>
  const auto [output4, expanded4] = composer_->GetQueriesForPrediction();
  EXPECT_EQ(output4, "もz");

  EXPECT_TRUE(expanded4.empty());
}

TEST_F(ComposerTest, IsToggleable) {
  constexpr int kAttrs =
      TableAttribute::NEW_CHUNK | TableAttribute::NO_TRANSLITERATION;
  table_->AddRuleWithAttributes("1", "", "{?}あ", kAttrs);
  table_->AddRule("{?}あ1", "", "{?}い");
  table_->AddRule("{?}い{!}", "", "{*}い");

  EXPECT_FALSE(composer_->IsToggleable());

  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "あ");
  EXPECT_TRUE(composer_->IsToggleable());

  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "い");
  EXPECT_TRUE(composer_->IsToggleable());

  composer_->InsertCommandCharacter(Composer::STOP_KEY_TOGGLING);
  EXPECT_EQ(composer_->GetStringForPreedit(), "い");
  EXPECT_FALSE(composer_->IsToggleable());

  composer_->Reset();
  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "あ");
  composer_->SetNewInput();
  EXPECT_FALSE(composer_->IsToggleable());
}

TEST_F(ComposerTest, CheckTimeout) {
  table_->AddRule("1", "", "あ");
  table_->AddRule("あ{!}", "あ", "");
  table_->AddRule("あ1", "", "い");
  table_->AddRule("い{!}", "い", "");
  table_->AddRule("い1", "", "う");

  mozc::ScopedClockMock clock(absl::UnixEpoch() + absl::Hours(24));

  EXPECT_EQ(composer_->timeout_threshold_msec(), 0);

  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "あ");

  clock->Advance(absl::Seconds(3));

  // Because the threshold is not set, STOP_KEY_TOGGLING is not sent.
  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "い");

  // Set the threshold time.
  composer_->Reset();
  composer_->set_timeout_threshold_msec(1000);

  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "あ");

  clock->Advance(absl::Seconds(3));

  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "ああ");

  clock->Advance(absl::Milliseconds(700));

  ASSERT_TRUE(InsertKeyWithMode("1", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "あい");
}

TEST_F(ComposerTest, CheckTimeoutWithProtobuf) {
  table_->AddRule("1", "", "あ");
  table_->AddRule("あ{!}", "あ", "");
  table_->AddRule("あ1", "", "い");
  table_->AddRule("い{!}", "い", "");
  table_->AddRule("い1", "", "う");

  mozc::ScopedClockMock clock(absl::UnixEpoch() + absl::Hours(24));

  config_->set_composing_timeout_threshold_msec(500);
  composer_->Reset();  // The threshold should be updated to 500msec.

  int64_t timestamp_msec =
      absl::ToUnixMillis(absl::UnixEpoch() + absl::Hours(24));

  KeyEvent key_event;
  key_event.set_key_code('1');
  key_event.set_timestamp_msec(timestamp_msec);
  composer_->InsertCharacterKeyEvent(key_event);
  EXPECT_EQ(composer_->GetStringForPreedit(), "あ");

  clock->Advance(absl::Milliseconds(100));
  timestamp_msec += 3000;  // +3.0 sec in proto.
  key_event.set_timestamp_msec(timestamp_msec);
  composer_->InsertCharacterKeyEvent(key_event);
  EXPECT_EQ(composer_->GetStringForPreedit(), "ああ");

  clock->Advance(absl::Milliseconds(100));
  timestamp_msec += 700;  // +0.7 sec in proto.
  key_event.set_timestamp_msec(timestamp_msec);
  composer_->InsertCharacterKeyEvent(key_event);
  EXPECT_EQ(composer_->GetStringForPreedit(), "あああ");

  clock->Advance(absl::Milliseconds(300));
  timestamp_msec += 100;  // +0.7 sec in proto.
  key_event.set_timestamp_msec(timestamp_msec);
  composer_->InsertCharacterKeyEvent(key_event);
  EXPECT_EQ(composer_->GetStringForPreedit(), "ああい");
}

TEST_F(ComposerTest, SimultaneousInput) {
  table_->AddRule("k", "", "い");      // k → い
  table_->AddRule("い{!}", "い", "");  // k → い (timeout)
  table_->AddRule("d", "", "か");      // d → か
  table_->AddRule("か{!}", "か", "");  // d → か (timeout)
  table_->AddRule("かk", "れ", "");    // dk → れ
  table_->AddRule("いd", "れ", "");    // kd → れ
  table_->AddRule("zl", "→", "");      // normal conversion w/o timeout

  mozc::ScopedClockMock clock(absl::UnixEpoch() + absl::Hours(24));
  composer_->set_timeout_threshold_msec(50);

  ASSERT_TRUE(InsertKeyWithMode("k", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "い");

  clock->Advance(absl::Milliseconds(30));  // < 50 msec
  ASSERT_TRUE(InsertKeyWithMode("d", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "れ");

  clock->Advance(absl::Milliseconds(30));  // < 50 msec
  ASSERT_TRUE(InsertKeyWithMode("k", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "れい");

  clock->Advance(absl::Milliseconds(200));  // > 50 msec
  ASSERT_TRUE(InsertKeyWithMode("d", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "れいか");

  clock->Advance(absl::Milliseconds(200));  // > 50 msec

  ASSERT_TRUE(InsertKeyWithMode("z", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "れいかｚ");

  // Even after timeout, normal conversions work.
  clock->Advance(absl::Milliseconds(200));  // > 50 msec
  ASSERT_TRUE(InsertKeyWithMode("l", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "れいか→");
}

TEST_F(ComposerTest, SimultaneousInputWithSpecialKey1) {
  table_->AddRule("{henkan}", "あ", "");

  ASSERT_TRUE(InsertKeyWithMode("Henkan", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "あ");
}

TEST_F(ComposerTest, SimultaneousInputWithSpecialKey2) {
  table_->AddRule("{henkan}j", "お", "");

  ASSERT_TRUE(InsertKeyWithMode("Henkan", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "");

  ASSERT_TRUE(InsertKeyWithMode("j", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "お");

  ASSERT_TRUE(InsertKeyWithMode("Henkan", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "お");

  ASSERT_TRUE(InsertKeyWithMode("j", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "おお");
}

TEST_F(ComposerTest, SimultaneousInputWithSpecialKey3) {
  table_->AddRule("j", "", "と");
  table_->AddRule("と{henkan}", "お", "");
  table_->AddRule("{henkan}j", "お", "");

  ASSERT_TRUE(InsertKeyWithMode("Henkan", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "");

  ASSERT_TRUE(InsertKeyWithMode("j", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "お");
}

TEST_F(ComposerTest, SimultaneousInputWithSpecialKey4) {
  table_->AddRule("j", "", "と");
  table_->AddRule("と{henkan}", "お", "");
  table_->AddRule("{henkan}j", "お", "");

  ASSERT_TRUE(InsertKeyWithMode("j", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "と");

  ASSERT_TRUE(InsertKeyWithMode("Henkan", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "お");
}

TEST_F(ComposerTest, SimultaneousInputWithSpecialKey5) {
  table_->AddRule("r", "", "{*}");
  table_->AddRule("{*}j", "お", "");

  ASSERT_TRUE(InsertKeyWithMode("r", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "");

  ASSERT_TRUE(InsertKeyWithMode("j", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "お");
}

TEST_F(ComposerTest, SimultaneousInputWithSpecialKey6) {
  table_->AddRule("{!}", "", "");          // no-op
  table_->AddRule("{henkan}{!}", "", "");  // no-op
  table_->AddRule("j", "", "と");
  table_->AddRule("と{!}", "と", "");
  table_->AddRule("{henkan}j", "お", "");
  table_->AddRule("と{henkan}", "お", "");

  mozc::ScopedClockMock clock(absl::UnixEpoch() + absl::Hours(24));
  composer_->set_timeout_threshold_msec(50);

  // "j"
  ASSERT_TRUE(InsertKeyWithMode("j", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "と");

  // "j{henkan}"
  clock->Advance(absl::Milliseconds(30));  // < 50 msec
  ASSERT_TRUE(InsertKeyWithMode("Henkan", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "お");

  // "j{henkan}j"
  clock->Advance(absl::Milliseconds(30));  // < 50 msec
  ASSERT_TRUE(InsertKeyWithMode("j", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "おと");

  // "j{henkan}j{!}{henkan}"
  clock->Advance(absl::Milliseconds(200));  // > 50 msec
  ASSERT_TRUE(InsertKeyWithMode("Henkan", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "おと");

  // "j{henkan}j{!}{henkan}{!}j"
  clock->Advance(absl::Milliseconds(200));  // > 50 msec
  ASSERT_TRUE(InsertKeyWithMode("j", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "おとと");

  // "j{henkan}j{!}{henkan}{!}j{!}{henkan}"
  clock->Advance(absl::Milliseconds(200));  // > 50 msec
  ASSERT_TRUE(InsertKeyWithMode("Henkan", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "おとと");

  // "j{henkan}j{!}{henkan}{!}j{!}{henkan}j"
  clock->Advance(absl::Milliseconds(30));  // < 50 msec
  ASSERT_TRUE(InsertKeyWithMode("j", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "おととお");
}

// Confirms empty entries (e.g. the 大⇔小 key to は) are skipped. b/289217346
TEST_F(ComposerTest, SkipEmptyEntries) {
  // Rules are extracted from data/preedit/toggle_flick-hiragana.tsv
  // specified from Request::TOGGLE_FLICK_TO_HIRAGANA.
  table_->AddRuleWithAttributes("6", "", "{?}は",
                                NEW_CHUNK | NO_TRANSLITERATION);
  // "*" is the raw input for the 大⇔小 key.
  table_->AddRule("{?}は*", "", "{*}ば");
  // "`" is the raw input for the sliding up of the 大⇔小 key.
  table_->AddRuleWithAttributes("`", "", "", NO_TRANSLITERATION);
  table_->AddRuleWithAttributes("*", "", "", NO_TRANSLITERATION);

  // Type は from 6
  ASSERT_TRUE(InsertKeyWithMode("6", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "は");

  // Slide up the 大⇔小 key to try to make は smaller, but it's invalid.
  ASSERT_TRUE(InsertKeyWithMode("`", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "は");

  // Tap the 大⇔小 key to make は to ば. It should work regardless the above
  // step.
  ASSERT_TRUE(InsertKeyWithMode("*", commands::HIRAGANA, composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "ば");
}

// Confirm that N in the middle of composing text is not removed where the
// character form is fullwidth. (b/241724792)
TEST_F(ComposerTest, NBforeN_WithFullWidth) {
  table_->AddRule("na", "な", "");
  table_->AddRule("n", "ん", "");
  table_->AddRule("nn", "ん", "");
  table_->AddRule("a", "あ", "");

  CharacterFormManager *manager =
      CharacterFormManager::GetCharacterFormManager();
  manager->SetDefaultRule();
  manager->AddPreeditRule("a", Config::FULL_WIDTH);

  ASSERT_TRUE(InsertKey("a", composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "あ");

  ASSERT_TRUE(InsertKey("n", composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "あｎ");

  composer_->MoveCursorLeft();
  ASSERT_TRUE(InsertKey("n", composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "あｎｎ");

  std::string left, focused, right;
  composer_->GetPreedit(&left, &focused, &right);
  EXPECT_EQ(left, "あｎ");
  EXPECT_EQ(focused, "ｎ");
  EXPECT_EQ(right, "");

  // auto = std::pair<std::string, absl::btree_set<std::string>>
  const auto [queries_base, queries_expanded] =
      composer_->GetQueriesForPrediction();
  EXPECT_EQ(queries_base, "あn");

  EXPECT_EQ(composer_->GetQueryForPrediction(), "あnn");

  ASSERT_TRUE(InsertKey("a", composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "あなｎ");

  composer_->GetPreedit(&left, &focused, &right);
  EXPECT_EQ(left, "あな");
  EXPECT_EQ(focused, "ｎ");
  EXPECT_EQ(right, "");

  EXPECT_EQ(composer_->GetQueryForPrediction(), "あな");
}

// Confirm that N in the middle of composing text is not removed where the
// character form is halfwidth. (b/241724792)
TEST_F(ComposerTest, NBforeN_WithHalfWidth) {
  table_->AddRule("na", "な", "");
  table_->AddRule("n", "ん", "");
  table_->AddRule("nn", "ん", "");
  table_->AddRule("a", "あ", "");

  CharacterFormManager *manager =
      CharacterFormManager::GetCharacterFormManager();
  manager->SetDefaultRule();
  manager->AddPreeditRule("a", Config::HALF_WIDTH);

  ASSERT_TRUE(InsertKey("a", composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "あ");

  ASSERT_TRUE(InsertKey("n", composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "あn");

  composer_->MoveCursorLeft();
  ASSERT_TRUE(InsertKey("n", composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "あnn");

  std::string left, focused, right;
  composer_->GetPreedit(&left, &focused, &right);
  EXPECT_EQ(left, "あn");
  EXPECT_EQ(focused, "n");
  EXPECT_EQ(right, "");

  // auto = std::pair<std::string, absl::btree_set<std::string>>
  const auto [queries_base, queries_expanded] =
      composer_->GetQueriesForPrediction();
  EXPECT_EQ(queries_base, "あn");

  EXPECT_EQ(composer_->GetQueryForPrediction(), "あnn");

  ASSERT_TRUE(InsertKey("a", composer_.get()));
  EXPECT_EQ(composer_->GetStringForPreedit(), "あなn");
  composer_->GetPreedit(&left, &focused, &right);
  EXPECT_EQ(left, "あな");
  EXPECT_EQ(focused, "n");
  EXPECT_EQ(right, "");

  EXPECT_EQ(composer_->GetQueryForPrediction(), "あな");
}

TEST_F(ComposerTest, GetStringForTypeCorrectionTest) {
  table_->AddRule("a", "あ", "");
  table_->AddRule("i", "い", "");
  table_->AddRule("u", "う", "");
  composer_->InsertCharacter("aiu");

  EXPECT_EQ(composer_->GetStringForTypeCorrection(), "あいう");
}

TEST_F(ComposerTest, UpdateComposition) {
  {
    commands::SessionCommand command;
    commands::SessionCommand::CompositionEvent *composition_event =
        command.add_composition_events();
    composition_event->set_composition_string("かん字");
    composition_event->set_probability(1.0);
    composer_->SetCompositionsForHandwriting(command.composition_events());
  }

  EXPECT_EQ(composer_->GetQueryForPrediction(), "かん字");
  EXPECT_EQ(composer_->GetHandwritingCompositions().size(), 1);

  {
    commands::SessionCommand command;
    commands::SessionCommand::CompositionEvent *composition_event =
        command.add_composition_events();
    composition_event->set_composition_string("ねこ");
    composition_event->set_probability(0.9);
    composition_event = command.add_composition_events();
    composition_event->set_composition_string("ね二");
    composition_event->set_probability(0.1);
    composer_->SetCompositionsForHandwriting(command.composition_events());
  }

  EXPECT_EQ(composer_->GetQueryForPrediction(), "ねこ");
  EXPECT_EQ(composer_->GetHandwritingCompositions().size(), 2);
}

TEST_F(ComposerTest, CreateComposerData) {
  table_->AddRule("a", "あ", "");
  table_->AddRule("ka", "か", "");
  table_->AddRule("ki", "き", "");

  InsertKey("a", composer_.get());
  InsertKeyWithMode("a", commands::FULL_KATAKANA, composer_.get());
  InsertKey("k", composer_.get());

  ComposerData data(composer_->CreateComposerData());
  EXPECT_EQ(data.GetInputMode(), composer_->GetInputMode());
  EXPECT_EQ(data.GetStringForPreedit(), "あアｋ");
  EXPECT_EQ(data.GetStringForPreedit(), composer_->GetStringForPreedit());
  EXPECT_EQ(data.GetQueryForConversion(), composer_->GetQueryForConversion());
  EXPECT_EQ(data.GetQueryForPrediction(), "あア");
  EXPECT_EQ(data.GetQueryForPrediction(), composer_->GetQueryForPrediction());
  EXPECT_EQ(data.GetStringForTypeCorrection(),
            composer_->GetStringForTypeCorrection());
  EXPECT_EQ(data.GetLength(), composer_->GetLength());
  EXPECT_EQ(data.GetCursor(), composer_->GetCursor());
  EXPECT_EQ(data.GetRawString(), composer_->GetRawString());
  EXPECT_EQ(data.GetRawSubString(0, 2), composer_->GetRawSubString(0, 2));
  EXPECT_EQ(data.GetRawSubString(1, 1), composer_->GetRawSubString(1, 1));
  EXPECT_EQ(data.source_text(), composer_->source_text());

  EXPECT_EQ(data.GetHandwritingCompositions().size(), 0);
  EXPECT_EQ(composer_->GetHandwritingCompositions().size(), 0);

  {  // Queries for prediction
    // auto = std::pair<std::string, absl::btree_set<std::string>>
    const auto [data_base, data_expanded] = data.GetQueriesForPrediction();
    EXPECT_EQ(data_base, "あア");
    EXPECT_EQ(data_expanded.size(), 3);

    // auto = std::pair<std::string, absl::btree_set<std::string>>
    const auto [composer_base, composer_expanded] =
        data.GetQueriesForPrediction();
    EXPECT_EQ(data_base, composer_base);
    EXPECT_EQ(data_expanded.size(), composer_expanded.size());

    const absl::btree_set<std::string> expected_expanded = {"k", "か", "き"};
    EXPECT_EQ(data_expanded, composer_expanded);
    EXPECT_EQ(data_expanded, expected_expanded);
  }

  {  // Transliterations
    transliteration::Transliterations data_t13ns;
    data.GetTransliterations(&data_t13ns);
    transliteration::Transliterations composer_t13ns;
    const transliteration::Transliterations expected_t13ns = {
        "ああｋ", "アアｋ", "aak",    "AAK",    "aak", "Aak",
        "ａａｋ", "ＡＡＫ", "ａａｋ", "Ａａｋ", "ｱｱk"};
    composer_->GetTransliterations(&composer_t13ns);
    EXPECT_EQ(data_t13ns.size(), 11);
    EXPECT_EQ(data_t13ns.size(), composer_t13ns.size());
    EXPECT_EQ(data_t13ns, expected_t13ns);
    EXPECT_EQ(data_t13ns, composer_t13ns);
  }

  {  // SubTransliterations
    transliteration::Transliterations data_t13ns;
    data.GetSubTransliterations(2, 1, &data_t13ns);
    transliteration::Transliterations composer_t13ns;
    const transliteration::Transliterations expected_t13ns = {
        "ｋ", "ｋ", "k", "K", "k", "K", "ｋ", "Ｋ", "ｋ", "Ｋ", "k"};
    composer_->GetSubTransliterations(2, 1, &composer_t13ns);
    EXPECT_EQ(data_t13ns.size(), 11);
    EXPECT_EQ(data_t13ns.size(), composer_t13ns.size());
    EXPECT_EQ(data_t13ns, expected_t13ns);
    EXPECT_EQ(data_t13ns, composer_t13ns);
  }
}

TEST_F(ComposerTest, CreateComposerDataForHandwriting) {
  commands::SessionCommand command;
  commands::SessionCommand::CompositionEvent *composition_event =
      command.add_composition_events();
  composition_event->set_composition_string("ねこ");
  composition_event->set_probability(0.9);
  composition_event = command.add_composition_events();
  composition_event->set_composition_string("ね二");
  composition_event->set_probability(0.1);
  composer_->SetCompositionsForHandwriting(command.composition_events());

  ComposerData data(composer_->CreateComposerData());
  EXPECT_EQ(data.GetInputMode(), composer_->GetInputMode());
  EXPECT_EQ(data.GetStringForPreedit(), composer_->GetStringForPreedit());
  EXPECT_EQ(data.GetQueryForConversion(), composer_->GetQueryForConversion());
  EXPECT_EQ(data.GetQueryForPrediction(), "ねこ");
  EXPECT_EQ(data.GetQueryForPrediction(), composer_->GetQueryForPrediction());
  EXPECT_EQ(data.GetStringForTypeCorrection(),
            composer_->GetStringForTypeCorrection());
  EXPECT_EQ(data.GetLength(), composer_->GetLength());
  EXPECT_EQ(data.GetCursor(), composer_->GetCursor());
  EXPECT_EQ(data.GetRawString(), composer_->GetRawString());
  EXPECT_EQ(data.GetRawSubString(0, 2), composer_->GetRawSubString(0, 2));
  EXPECT_EQ(data.GetRawSubString(1, 1), composer_->GetRawSubString(1, 1));
  EXPECT_EQ(data.source_text(), composer_->source_text());

  {  // HandwritingCompositions
    const absl::Span<const commands::SessionCommand::CompositionEvent>
        data_events = data.GetHandwritingCompositions();
    const absl::Span<const commands::SessionCommand::CompositionEvent>
        composer_events = composer_->GetHandwritingCompositions();
    EXPECT_EQ(data_events.size(), 2);
    EXPECT_EQ(data_events.size(), composer_events.size());
    for (int i = 0; i < data_events.size(); ++i) {
      EXPECT_EQ(data_events[i].composition_string(),
                composer_events[i].composition_string());
      EXPECT_EQ(data_events[i].probability(), composer_events[i].probability());
    }
  }

  {  // Queries for prediction
    // auto = std::pair<std::string, absl::btree_set<std::string>>
    const auto [data_base, data_expanded] = data.GetQueriesForPrediction();

    EXPECT_EQ(data_base, "ねこ");
    EXPECT_EQ(data_expanded.size(), 0);

    // auto = std::pair<std::string, absl::btree_set<std::string>>
    const auto [composer_base, composer_expanded] =
        data.GetQueriesForPrediction();
    EXPECT_EQ(data_base, composer_base);
    EXPECT_EQ(data_expanded.size(), composer_expanded.size());
    EXPECT_EQ(data_expanded, composer_expanded);  // Empty
  }
}

TEST_F(ComposerTest, CreateComposerDataForSourceText) {
  absl::string_view source_text = "再変換用";
  absl::string_view preedit_text = "さいへんかんよう";
  composer_->set_source_text(source_text);
  composer_->SetPreeditTextForTestOnly(preedit_text);
  ComposerData data(composer_->CreateComposerData());
  EXPECT_EQ(data.GetInputMode(), composer_->GetInputMode());
  EXPECT_EQ(data.GetStringForPreedit(), composer_->GetStringForPreedit());
  EXPECT_EQ(data.GetQueryForConversion(), composer_->GetQueryForConversion());
  EXPECT_EQ(data.GetQueryForPrediction(), preedit_text);
  EXPECT_EQ(data.GetQueryForPrediction(), composer_->GetQueryForPrediction());
  EXPECT_EQ(data.GetStringForTypeCorrection(),
            composer_->GetStringForTypeCorrection());
  EXPECT_EQ(data.GetLength(), composer_->GetLength());
  EXPECT_EQ(data.GetCursor(), composer_->GetCursor());
  EXPECT_EQ(data.GetRawString(), composer_->GetRawString());
  EXPECT_EQ(data.GetRawSubString(0, 2), composer_->GetRawSubString(0, 2));
  EXPECT_EQ(data.GetRawSubString(1, 1), composer_->GetRawSubString(1, 1));
  EXPECT_EQ(data.source_text(), source_text);
  EXPECT_EQ(data.source_text(), composer_->source_text());
}

TEST_F(ComposerTest, CreateComposerOperators) {
  table_->AddRule("a", "あ", "");
  table_->AddRule("ka", "か", "");
  table_->AddRule("ki", "き", "");

  InsertKey("a", composer_.get());
  InsertKeyWithMode("a", commands::FULL_KATAKANA, composer_.get());
  InsertKey("k", composer_.get());

  const ComposerData data(composer_->CreateComposerData());
  const ComposerData copied = data;
  const ComposerData moved = std::move(data);

  EXPECT_EQ(copied.GetInputMode(), moved.GetInputMode());
  EXPECT_EQ(copied.GetStringForPreedit(), moved.GetStringForPreedit());
  EXPECT_EQ(copied.GetQueryForConversion(), moved.GetQueryForConversion());
  EXPECT_EQ(copied.GetQueryForPrediction(), moved.GetQueryForPrediction());
  EXPECT_EQ(copied.GetStringForTypeCorrection(),
            moved.GetStringForTypeCorrection());
  EXPECT_EQ(copied.GetLength(), moved.GetLength());
  EXPECT_EQ(copied.GetCursor(), moved.GetCursor());
  EXPECT_EQ(copied.GetRawString(), moved.GetRawString());
  EXPECT_EQ(copied.GetRawSubString(0, 2), moved.GetRawSubString(0, 2));
  EXPECT_EQ(copied.GetRawSubString(1, 1), moved.GetRawSubString(1, 1));
  EXPECT_EQ(copied.source_text(), moved.source_text());
}

TEST_F(ComposerTest, CreateEmptyComposerData) {
  const ComposerData data = Composer::CreateEmptyComposerData();
  EXPECT_EQ(data.GetInputMode(), transliteration::HIRAGANA);
  EXPECT_EQ(data.GetStringForPreedit(), "");
  EXPECT_EQ(data.GetQueryForConversion(), "");
  EXPECT_EQ(data.GetQueryForPrediction(), "");
  EXPECT_EQ(data.GetStringForTypeCorrection(), "");
  EXPECT_EQ(data.GetLength(), 0);
  EXPECT_EQ(data.GetCursor(), 0);
  EXPECT_EQ(data.GetRawString(), "");
  EXPECT_EQ(data.GetRawSubString(0, 2), "");
  EXPECT_EQ(data.GetRawSubString(1, 1), "");
  EXPECT_EQ(data.source_text(), "");
}

}  // namespace composer
}  // namespace mozc
