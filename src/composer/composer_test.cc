// Copyright 2010-2012, Google Inc.
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

#include <string>
#include <utility>

#include "base/logging.h"
#include "composer/table.h"
#include "config/character_form_manager.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "session/commands.pb.h"
#include "session/key_parser.h"
#include "session/request_test_util.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace composer {

using ::mozc::config::CharacterFormManager;
using ::mozc::config::Config;
using ::mozc::config::ConfigHandler;
using ::mozc::commands::Request;

namespace {
bool InsertKey(const string &key_string, Composer *composer) {
  commands::KeyEvent key;
  if (!KeyParser::ParseKey(key_string, &key)) {
    return false;
  }
  return composer->InsertCharacterKeyEvent(key);
}

bool InsertKeyWithMode(const string &key_string,
                       const commands::CompositionMode mode,
                       Composer *composer) {
  commands::KeyEvent key;
  if (!KeyParser::ParseKey(key_string, &key)) {
    return false;
  }
  key.set_mode(mode);
  return composer->InsertCharacterKeyEvent(key);
}

string GetPreedit(const Composer *composer) {
  string preedit;
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
    string left_text, right_text;
    lhs.GetStringForPreedit(&left_text);
    rhs.GetStringForPreedit(&right_text);
    EXPECT_EQ(left_text, right_text);
  }
  {
    string left_text, right_text;
    lhs.GetStringForSubmission(&left_text);
    rhs.GetStringForSubmission(&right_text);
    EXPECT_EQ(left_text, right_text);
  }
  {
    string left_text, right_text;
    lhs.GetQueryForConversion(&left_text);
    rhs.GetQueryForConversion(&right_text);
    EXPECT_EQ(left_text, right_text);
  }
  {
    string left_text, right_text;
    lhs.GetQueryForPrediction(&left_text);
    rhs.GetQueryForPrediction(&right_text);
    EXPECT_EQ(left_text, right_text);
  }
}

}  // namespace

class ComposerTest : public testing::Test {
 protected:
  ComposerTest() {}

  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    ConfigHandler::SetConfig(config);
    table_.reset(new Table);
    default_request_.CopyFrom(Request::default_instance());
    composer_.reset(new Composer(table_.get(), default_request_));
    CharacterFormManager::GetCharacterFormManager()->SetDefaultRule();
  }

  virtual void TearDown() {
    table_.reset();
    composer_.reset();
    // just in case, reset config in test_tmpdir
    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    ConfigHandler::SetConfig(config);
  }

  scoped_ptr<Composer> composer_;
  scoped_ptr<Table> table_;
  Request default_request_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ComposerTest);
};


TEST_F(ComposerTest, Reset) {
  composer_->InsertCharacter("mozuku");

  composer_->SetInputMode(transliteration::HALF_ASCII);
  composer_->SetInputFieldType(commands::SessionCommand::PASSWORD);
  composer_->Reset();

  EXPECT_TRUE(composer_->Empty());
  // The input mode ramains the previous mode.
  EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());
  EXPECT_EQ(commands::SessionCommand::PASSWORD,
            composer_->GetInputFieldType());
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

  string result;
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
  string result;
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

TEST_F(ComposerTest, InsertCharacterPreeditAt) {
  // "あ"
  table_->AddRule("a", "\xe3\x81\x82", "");
  // "い"
  table_->AddRule("i", "\xe3\x81\x84", "");
  // "う"
  table_->AddRule("u", "\xe3\x81\x86", "");
  // "く"
  table_->AddRule("ku", "\xE3\x81\x8F", "");

  composer_->InsertCharacter("au");
  string output;

  const transliteration::TransliterationType input_mode =
      composer_->GetInputMode();

  composer_->InsertCharacterPreeditAt(0, "a");
  EXPECT_EQ(3, composer_->GetLength());
  EXPECT_EQ(3, composer_->GetCursor());
  composer_->GetStringForPreedit(&output);
  // "aあう"
  EXPECT_EQ("a\xE3\x81\x82\xE3\x81\x86", output);
  // Input mode of composer must not be changed
  EXPECT_EQ(input_mode, composer_->GetInputMode());

  composer_->MoveCursorLeft();
  // "い"
  composer_->InsertCharacterPreeditAt(2, "\xE3\x81\x84");
  EXPECT_EQ(4, composer_->GetLength());
  EXPECT_EQ(3, composer_->GetCursor());
  composer_->GetStringForPreedit(&output);
  // "aあいう"
  EXPECT_EQ("a\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86", output);
  EXPECT_EQ(input_mode, composer_->GetInputMode());

  composer_->MoveCursorLeft();
  composer_->InsertCharacterPreeditAt(3, "ku");
  EXPECT_EQ(6, composer_->GetLength());
  EXPECT_EQ(2, composer_->GetCursor());
  composer_->GetStringForPreedit(&output);
  // "aあいkuう"
  EXPECT_EQ("a\xE3\x81\x82\xE3\x81\x84ku\xE3\x81\x86", output);
  EXPECT_EQ(input_mode, composer_->GetInputMode());

  composer_->InsertCharacterPreeditAt(0, "1");
  composer_->GetStringForPreedit(&output);
  // "1aあいkuう"
  EXPECT_EQ("1a\xE3\x81\x82\xE3\x81\x84ku\xE3\x81\x86", output);
  EXPECT_EQ(input_mode, composer_->GetInputMode());

  // Check the actual input mode of composition
  composer_->InsertCharacter("a");
  composer_->GetStringForPreedit(&output);
  // "1aああいkuう"
  EXPECT_EQ("1a\xE3\x81\x82\xE3\x81\x82\xE3\x81\x84ku\xE3\x81\x86", output);
}

TEST_F(ComposerTest, OutputMode) {
  // This behaviour is based on Kotoeri

  // "あ"
  table_->AddRule("a", "\xe3\x81\x82", "");
  // "い"
  table_->AddRule("i", "\xe3\x81\x84", "");
  // "う"
  table_->AddRule("u", "\xe3\x81\x86", "");

  composer_->SetOutputMode(transliteration::HIRAGANA);

  composer_->InsertCharacter("a");
  composer_->InsertCharacter("i");
  composer_->InsertCharacter("u");

  string output;
  composer_->GetStringForPreedit(&output);
  // "あいう"
  EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86", output);

  composer_->SetOutputMode(transliteration::FULL_ASCII);
  composer_->GetStringForPreedit(&output);
  // "ａｉｕ"
  EXPECT_EQ("\xEF\xBD\x81\xEF\xBD\x89\xEF\xBD\x95", output);

  composer_->InsertCharacter("a");
  composer_->InsertCharacter("i");
  composer_->InsertCharacter("u");
  composer_->GetStringForPreedit(&output);
  // "ａｉｕあいう"
  EXPECT_EQ("\xEF\xBD\x81\xEF\xBD\x89\xEF\xBD\x95"
            "\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86",
            output);
}

TEST_F(ComposerTest, OutputMode_2) {
  // This behaviour is based on Kotoeri

  // "あ"
  table_->AddRule("a", "\xe3\x81\x82", "");
  // "い"
  table_->AddRule("i", "\xe3\x81\x84", "");
  // "う"
  table_->AddRule("u", "\xe3\x81\x86", "");

  composer_->InsertCharacter("a");
  composer_->InsertCharacter("i");
  composer_->InsertCharacter("u");

  string output;
  composer_->GetStringForPreedit(&output);
  // "あいう"
  EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86", output);

  composer_->MoveCursorLeft();
  composer_->SetOutputMode(transliteration::FULL_ASCII);
  composer_->GetStringForPreedit(&output);
  // "ａｉｕ"
  EXPECT_EQ("\xEF\xBD\x81\xEF\xBD\x89\xEF\xBD\x95", output);

  composer_->InsertCharacter("a");
  composer_->InsertCharacter("i");
  composer_->InsertCharacter("u");
  composer_->GetStringForPreedit(&output);
  // "ａｉｕあいう"
  EXPECT_EQ("\xEF\xBD\x81\xEF\xBD\x89\xEF\xBD\x95"
            "\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86",
            output);
}

TEST_F(ComposerTest, GetTransliterations) {
  // "あ"
  table_->AddRule("a", "\xe3\x81\x82", "");
  // "い"
  table_->AddRule("i", "\xe3\x81\x84", "");
  // "う"
  table_->AddRule("u", "\xe3\x81\x86", "");
  // "あ"
  table_->AddRule("A", "\xe3\x81\x82", "");
  // "い"
  table_->AddRule("I", "\xe3\x81\x84", "");
  // "う"
  table_->AddRule("U", "\xe3\x81\x86", "");
  composer_->InsertCharacter("a");

  transliteration::Transliterations transliterations;
  composer_->GetTransliterations(&transliterations);
  EXPECT_EQ(transliteration::NUM_T13N_TYPES, transliterations.size());
  // "あ"
  EXPECT_EQ("\xe3\x81\x82", transliterations[transliteration::HIRAGANA]);
  // "ア"
  EXPECT_EQ("\xe3\x82\xa2", transliterations[transliteration::FULL_KATAKANA]);
  EXPECT_EQ("a", transliterations[transliteration::HALF_ASCII]);
  // "ａ"
  EXPECT_EQ("\xef\xbd\x81", transliterations[transliteration::FULL_ASCII]);
  // "ｱ"
  EXPECT_EQ("\xef\xbd\xb1", transliterations[transliteration::HALF_KATAKANA]);

  composer_->Reset();
  ASSERT_TRUE(composer_->Empty());
  transliterations.clear();

  composer_->InsertCharacter("!");
  composer_->GetTransliterations(&transliterations);
  EXPECT_EQ(transliteration::NUM_T13N_TYPES, transliterations.size());
  // NOTE(komatsu): The duplication will be handled by the session layer.
  // "！"
  EXPECT_EQ("\xef\xbc\x81", transliterations[transliteration::HIRAGANA]);
  // "！"
  EXPECT_EQ("\xef\xbc\x81", transliterations[transliteration::FULL_KATAKANA]);
  EXPECT_EQ("!", transliterations[transliteration::HALF_ASCII]);
  // "！"
  EXPECT_EQ("\xef\xbc\x81", transliterations[transliteration::FULL_ASCII]);
  EXPECT_EQ("!", transliterations[transliteration::HALF_KATAKANA]);

  composer_->Reset();
  ASSERT_TRUE(composer_->Empty());
  transliterations.clear();

  composer_->InsertCharacter("aIu");
  composer_->GetTransliterations(&transliterations);
  EXPECT_EQ(transliteration::NUM_T13N_TYPES, transliterations.size());
  // "あいう"
  EXPECT_EQ("\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86",
            transliterations[transliteration::HIRAGANA]);
  // "アイウ"
  EXPECT_EQ("\xe3\x82\xa2\xe3\x82\xa4\xe3\x82\xa6",
            transliterations[transliteration::FULL_KATAKANA]);
  EXPECT_EQ("aIu", transliterations[transliteration::HALF_ASCII]);
  EXPECT_EQ("AIU", transliterations[transliteration::HALF_ASCII_UPPER]);
  EXPECT_EQ("aiu", transliterations[transliteration::HALF_ASCII_LOWER]);
  EXPECT_EQ("Aiu", transliterations[transliteration::HALF_ASCII_CAPITALIZED]);
  // "ａＩｕ"
  EXPECT_EQ("\xef\xbd\x81\xef\xbc\xa9\xef\xbd\x95",
            transliterations[transliteration::FULL_ASCII]);
  // "ＡＩＵ"
  EXPECT_EQ("\xef\xbc\xa1\xef\xbc\xa9\xef\xbc\xb5",
            transliterations[transliteration::FULL_ASCII_UPPER]);
  // "ａｉｕ"
  EXPECT_EQ("\xef\xbd\x81\xef\xbd\x89\xef\xbd\x95",
            transliterations[transliteration::FULL_ASCII_LOWER]);
  // "Ａｉｕ"
  EXPECT_EQ("\xef\xbc\xa1\xef\xbd\x89\xef\xbd\x95",
            transliterations[transliteration::FULL_ASCII_CAPITALIZED]);
  // "ｱｲｳ"
  EXPECT_EQ("\xef\xbd\xb1\xef\xbd\xb2\xef\xbd\xb3",
            transliterations[transliteration::HALF_KATAKANA]);

  // Transliterations for quote marks.  This is a test against
  // http://b/1581367
  composer_->Reset();
  ASSERT_TRUE(composer_->Empty());
  transliterations.clear();

  composer_->InsertCharacter("'\"`");
  composer_->GetTransliterations(&transliterations);
  // "'" -> \u2019
  // """ -> \u201d
  // "`" -> \uff40
  EXPECT_EQ("'\"`", transliterations[transliteration::HALF_ASCII]);
  // "’”｀"
  EXPECT_EQ("\xe2\x80\x99\xe2\x80\x9d\xef\xbd\x80",
            transliterations[transliteration::FULL_ASCII]);
}

TEST_F(ComposerTest, GetSubTransliterations) {
  // "か"
  table_->AddRule("ka", "\xe3\x81\x8b", "");
  // "ん"
  table_->AddRule("n", "\xe3\x82\x93", "");
  // "な"
  table_->AddRule("na", "\xe3\x81\xaa", "");
  // "だ"
  table_->AddRule("da", "\xe3\x81\xa0", "");

  composer_->InsertCharacter("kanna");

  transliteration::Transliterations transliterations;
  composer_->GetSubTransliterations(0, 2, &transliterations);
  // "かん"
  EXPECT_EQ("\xe3\x81\x8b\xe3\x82\x93",
            transliterations[transliteration::HIRAGANA]);
  // "カン"
  EXPECT_EQ("\xe3\x82\xab\xe3\x83\xb3",
            transliterations[transliteration::FULL_KATAKANA]);
  EXPECT_EQ("kan", transliterations[transliteration::HALF_ASCII]);
  // "ｋａｎ"
  EXPECT_EQ("\xef\xbd\x8b\xef\xbd\x81\xef\xbd\x8e",
            transliterations[transliteration::FULL_ASCII]);
  // "ｶﾝ"
  EXPECT_EQ("\xef\xbd\xb6\xef\xbe\x9d",
            transliterations[transliteration::HALF_KATAKANA]);

  transliterations.clear();
  composer_->GetSubTransliterations(1, 1, &transliterations);
  // "ん"
  EXPECT_EQ("\xe3\x82\x93", transliterations[transliteration::HIRAGANA]);
  // "ン"
  EXPECT_EQ("\xe3\x83\xb3", transliterations[transliteration::FULL_KATAKANA]);
  EXPECT_EQ("n", transliterations[transliteration::HALF_ASCII]);
  // "ｎ"
  EXPECT_EQ("\xef\xbd\x8e", transliterations[transliteration::FULL_ASCII]);
  // "ﾝ"
  EXPECT_EQ("\xef\xbe\x9d", transliterations[transliteration::HALF_KATAKANA]);

  transliterations.clear();
  composer_->GetSubTransliterations(2, 1, &transliterations);
  // "な"
  EXPECT_EQ("\xe3\x81\xaa", transliterations[transliteration::HIRAGANA]);
  // "ナ"
  EXPECT_EQ("\xe3\x83\x8a", transliterations[transliteration::FULL_KATAKANA]);
  EXPECT_EQ("na", transliterations[transliteration::HALF_ASCII]);
  // "ｎａ"
  EXPECT_EQ("\xef\xbd\x8e\xef\xbd\x81",
            transliterations[transliteration::FULL_ASCII]);
  // "ﾅ"
  EXPECT_EQ("\xef\xbe\x85", transliterations[transliteration::HALF_KATAKANA]);

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
  // "かんな"
  EXPECT_EQ("\xe3\x81\x8b\xe3\x82\x93\xe3\x81\xaa",
            transliterations[transliteration::HIRAGANA]);
  // "カンナ"
  EXPECT_EQ("\xe3\x82\xab\xe3\x83\xb3\xe3\x83\x8a",
            transliterations[transliteration::FULL_KATAKANA]);
  EXPECT_EQ("kanna", transliterations[transliteration::HALF_ASCII]);
  // "ｋａｎｎａ"
  EXPECT_EQ("\xef\xbd\x8b\xef\xbd\x81\xef\xbd\x8e\xef\xbd\x8e\xef\xbd\x81",
            transliterations[transliteration::FULL_ASCII]);
  // "ｶﾝﾅ"
  EXPECT_EQ("\xef\xbd\xb6\xef\xbe\x9d\xef\xbe\x85",
            transliterations[transliteration::HALF_KATAKANA]);

  // Dakuon case
  transliterations.clear();
  composer_->EditErase();
  composer_->InsertCharacter("dankann");
  composer_->GetSubTransliterations(0, 3, &transliterations);
  // "だんか"
  EXPECT_EQ("\xe3\x81\xa0\xe3\x82\x93\xe3\x81\x8b",
            transliterations[transliteration::HIRAGANA]);
  // "ダンカ"
  EXPECT_EQ("\xe3\x83\x80\xe3\x83\xb3\xe3\x82\xab",
            transliterations[transliteration::FULL_KATAKANA]);
  EXPECT_EQ("danka", transliterations[transliteration::HALF_ASCII]);
  // "ｄａｎｋａ"
  EXPECT_EQ("\xef\xbd\x84\xef\xbd\x81\xef\xbd\x8e\xef\xbd\x8b\xef\xbd\x81",
            transliterations[transliteration::FULL_ASCII]);
  // "ﾀﾞﾝｶ"
  EXPECT_EQ("\xef\xbe\x80\xef\xbe\x9e\xef\xbe\x9d\xef\xbd\xb6",
            transliterations[transliteration::HALF_KATAKANA]);
}

TEST_F(ComposerTest, GetStringFunctions) {
  // "か"
  table_->AddRule("ka", "\xe3\x81\x8b", "");
  // "ん"
  table_->AddRule("n", "\xe3\x82\x93", "");
  // "な"
  table_->AddRule("na", "\xe3\x81\xaa", "");
  // "さ"
  table_->AddRule("sa", "\xe3\x81\x95", "");

  // Query: "!kan"
  composer_->InsertCharacter("!kan");
  string preedit;
  composer_->GetStringForPreedit(&preedit);
  // "！かｎ"
  EXPECT_EQ("\xef\xbc\x81\xe3\x81\x8b\xef\xbd\x8e", preedit);

  string submission;
  composer_->GetStringForSubmission(&submission);
  // "！かｎ"
  EXPECT_EQ("\xef\xbc\x81\xe3\x81\x8b\xef\xbd\x8e", submission);

  string conversion;
  composer_->GetQueryForConversion(&conversion);
  // "!かん"
  EXPECT_EQ("\x21\xe3\x81\x8b\xe3\x82\x93", conversion);

  string prediction;
  composer_->GetQueryForPrediction(&prediction);
  // "!か"
  EXPECT_EQ("\x21\xe3\x81\x8b", prediction);

  // Query: "kas"
  composer_->EditErase();
  composer_->InsertCharacter("kas");

  preedit.clear();
  composer_->GetStringForPreedit(&preedit);
  // "かｓ"
  EXPECT_EQ("\xe3\x81\x8b\xef\xbd\x93", preedit);

  submission.clear();
  composer_->GetStringForSubmission(&submission);
  // "かｓ"
  EXPECT_EQ("\xe3\x81\x8b\xef\xbd\x93", submission);

  // Pending chars should remain.  This is a test against
  // http://b/1799399
  conversion.clear();
  composer_->GetQueryForConversion(&conversion);
  // "かs"
  EXPECT_EQ("\xe3\x81\x8b\x73", conversion);

  prediction.clear();
  composer_->GetQueryForPrediction(&prediction);
  // "か"
  EXPECT_EQ("\xe3\x81\x8b", prediction);

  // Query: "s"
  composer_->EditErase();
  composer_->InsertCharacter("s");

  preedit.clear();
  composer_->GetStringForPreedit(&preedit);
  // "ｓ"
  EXPECT_EQ("\xef\xbd\x93", preedit);

  submission.clear();
  composer_->GetStringForSubmission(&submission);
  // "ｓ"
  EXPECT_EQ("\xef\xbd\x93", submission);

  conversion.clear();
  composer_->GetQueryForConversion(&conversion);
  // "s"
  EXPECT_EQ("s", conversion);

  prediction.clear();
  composer_->GetQueryForPrediction(&prediction);
  EXPECT_EQ("s", prediction);

  // Query: "sk"
  composer_->EditErase();
  composer_->InsertCharacter("sk");

  preedit.clear();
  composer_->GetStringForPreedit(&preedit);
  // "ｓｋ"
  EXPECT_EQ("\xef\xbd\x93\xEF\xBD\x8B", preedit);

  submission.clear();
  composer_->GetStringForSubmission(&submission);
  // "ｓｋ"
  EXPECT_EQ("\xef\xbd\x93\xEF\xBD\x8B", submission);

  conversion.clear();
  composer_->GetQueryForConversion(&conversion);
  // "sk"
  EXPECT_EQ("sk", conversion);

  prediction.clear();
  composer_->GetQueryForPrediction(&prediction);
  // "sk"
  EXPECT_EQ("sk", prediction);
}

TEST_F(ComposerTest, GetQueryForPredictionHalfAscii) {
  // Dummy setup of romanji table.
  table_->AddRule("he", "\xe3\x81\xb8", "");  // "へ"
  table_->AddRule("ll", "\xe3\x81\xa3\xef\xbd\x8c", "");  // "っｌ"
  table_->AddRule("lo", "\xe3\x82\x8d", "");  // "ろ"

  // Switch to Half-Latin input mode.
  composer_->SetInputMode(transliteration::HALF_ASCII);

  string prediction;
  {
    const char kInput[] = "hello";
    composer_->InsertCharacter(kInput);
    composer_->GetQueryForPrediction(&prediction);
    EXPECT_EQ(kInput, prediction);
  }
  prediction.clear();
  composer_->EditErase();
  {
    const char kInput[] = "hello!";
    composer_->InsertCharacter(kInput);
    composer_->GetQueryForPrediction(&prediction);
    EXPECT_EQ(kInput, prediction);
  }
}

TEST_F(ComposerTest, GetQueryForPredictionFullAscii) {
  // Dummy setup of romanji table.
  table_->AddRule("he", "\xe3\x81\xb8", "");  // "へ"
  table_->AddRule("ll", "\xe3\x81\xa3\xef\xbd\x8c", "");  // "っｌ"
  table_->AddRule("lo", "\xe3\x82\x8d", "");  // "ろ"

  // Switch to Full-Latin input mode.
  composer_->SetInputMode(transliteration::FULL_ASCII);

  string prediction;
  {
    // "ｈｅｌｌｏ"
    composer_->InsertCharacter(
        "\xef\xbd\x88\xef\xbd\x85\xef\xbd\x8c\xef\xbd\x8c\xef\xbd\x8f");
    composer_->GetQueryForPrediction(&prediction);
    EXPECT_EQ("hello", prediction);
  }
  prediction.clear();
  composer_->EditErase();
  {
    // "ｈｅｌｌｏ！"
    composer_->InsertCharacter(
        "\xef\xbd\x88\xef\xbd\x85\xef\xbd\x8c\xef\xbd\x8c"
        "\xef\xbd\x8f\xef\xbc\x81");
    composer_->GetQueryForPrediction(&prediction);
    EXPECT_EQ("hello!", prediction);
  }
}

TEST_F(ComposerTest, GetQueriesForPredictionRoman) {
  // "う"
  table_->AddRule("u", "\xe3\x81\x86", "");
  // "っ"
  table_->AddRule("ss", "\xe3\x81\xa3", "s");
  // "さ"
  table_->AddRule("sa", "\xe3\x81\x95", "");
  // "し"
  table_->AddRule("si", "\xe3\x81\x97", "");
  // "す"
  table_->AddRule("su", "\xe3\x81\x99", "");
  // "せ"
  table_->AddRule("se", "\xe3\x81\x9b", "");
  // "そ"
  table_->AddRule("so", "\xe3\x81\x9d", "");

  {
    string base, preedit;
    set<string> expanded;
    composer_->EditErase();
    composer_->InsertCharacter("us");
    composer_->GetQueriesForPrediction(&base, &expanded);
    composer_->GetStringForPreedit(&preedit);
    // "う"
    EXPECT_EQ("\xe3\x81\x86", base);
    for (set<string>::const_iterator itr = expanded.begin();
         itr != expanded.end(); ++itr) {
      LOG(INFO) << *itr;
    }
    EXPECT_EQ(7, expanded.size());
    // We can't use EXPECT_NE for iterator
    EXPECT_TRUE(expanded.end() != expanded.find("s"));
    // "っ"
    EXPECT_TRUE(expanded.end() != expanded.find("\xe3\x81\xa3"));
    // "さ"
    EXPECT_TRUE(expanded.end() != expanded.find("\xe3\x81\x95"));
    // "し"
    EXPECT_TRUE(expanded.end() != expanded.find("\xe3\x81\x97"));
    // "す"
    EXPECT_TRUE(expanded.end() != expanded.find("\xe3\x81\x99"));
    // "せ"
    EXPECT_TRUE(expanded.end() != expanded.find("\xe3\x81\x9b"));
    // "そ"
    EXPECT_TRUE(expanded.end() != expanded.find("\xe3\x81\x9d"));
  }
}

TEST_F(ComposerTest, GetQueriesForPredictionMobile) {
  // "い"
  table_->AddRule("_", "", "\xe3\x81\x84");
  // "い*", "ぃ"
  table_->AddRule("\xe3\x81\x84\x2a", "", "\xe3\x81\x83");
  // "ぃ*", "い"
  table_->AddRule("\xe3\x81\x83\x2a", "", "\xe3\x81\x84");
  // "と"
  table_->AddRule("$", "", "\xe3\x81\xa8");
  // "と*", "ど"
  table_->AddRule("\xe3\x81\xa8\x2a", "", "\xe3\x81\xa9");
  // "ど*", "と"
  table_->AddRule("\xe3\x81\xa9\x2a", "", "\xe3\x81\xa8");

  {
    string base, preedit;
    set<string> expanded;
    composer_->EditErase();
    composer_->InsertCharacter("_$");
    composer_->GetQueriesForPrediction(&base, &expanded);
    composer_->GetStringForPreedit(&preedit);
    // "い"
    EXPECT_EQ("\xe3\x81\x84", base);
    EXPECT_EQ(2, expanded.size());
    // "と"
    EXPECT_TRUE(expanded.end() != expanded.find("\xe3\x81\xa8"));
    // "ど"
    EXPECT_TRUE(expanded.end() != expanded.find("\xe3\x81\xa9"));
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
  string preedit;
  composer_->GetStringForPreedit(&preedit);
  // "ｎｙ［ＮＹＡ］ｎ"
  EXPECT_EQ("\xEF\xBD\x8E\xEF\xBD\x99\xEF\xBC\xBB\xEF\xBC\xAE\xEF\xBC\xB9"
            "\xEF\xBC\xA1\xEF\xBC\xBD\xEF\xBD\x8E", preedit);

  string submission;
  composer_->GetStringForSubmission(&submission);
  // "ｎｙ［ＮＹＡ］ｎ"
  EXPECT_EQ("\xEF\xBD\x8E\xEF\xBD\x99\xEF\xBC\xBB\xEF\xBC\xAE\xEF\xBC\xB9"
            "\xEF\xBC\xA1\xEF\xBC\xBD\xEF\xBD\x8E", submission);

  string conversion;
  composer_->GetQueryForConversion(&conversion);
  EXPECT_EQ("ny[NYA][N]", conversion);

  string prediction;
  composer_->GetQueryForPrediction(&prediction);
  EXPECT_EQ("ny[NYA]", prediction);

  composer_->InsertCharacter("ka");
  string conversion2;
  composer_->GetQueryForConversion(&conversion2);
  EXPECT_EQ("ny[NYA][N][KA]", conversion2);

  string prediction2;
  composer_->GetQueryForPrediction(&prediction2);
  EXPECT_EQ("ny[NYA][N][KA]", prediction2);
}

TEST_F(ComposerTest, GetStringFunctions_InputFieldType) {
  const struct TestData {
    const commands::SessionCommand::InputFieldType field_type_;
    const bool ascii_expected_;
  } test_data_list[] = {
      {commands::SessionCommand::NORMAL, false},
      {commands::SessionCommand::NUMBER, true},
      {commands::SessionCommand::PASSWORD, true},
      {commands::SessionCommand::TEL, true},
  };

  composer_->SetInputMode(transliteration::HIRAGANA);
  for (size_t test_data_index = 0;
       test_data_index < ARRAYSIZE_UNSAFE(test_data_list);
       ++test_data_index) {
    const TestData &test_data = test_data_list[test_data_index];
    composer_->SetInputFieldType(test_data.field_type_);
    string key, converted;
    for (char c = 0x20; c <= 0x7E; ++c) {
      key.assign(1, c);
      composer_->EditErase();
      composer_->InsertCharacter(key);
      if (test_data.ascii_expected_) {
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
}

TEST_F(ComposerTest, InsertCharacterKeyEvent) {
  commands::KeyEvent key;
  // "あ"
  table_->AddRule("a", "\xe3\x81\x82", "");

  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);

  string preedit;
  composer_->GetStringForPreedit(&preedit);
  // "あ"
  EXPECT_EQ("\xe3\x81\x82", preedit);

  // Half width "A" will be inserted.
  key.set_key_code('A');
  composer_->InsertCharacterKeyEvent(key);

  composer_->GetStringForPreedit(&preedit);
  // "あA"
  EXPECT_EQ("\xe3\x81\x82\x41", preedit);

  // Half width "a" will be inserted.
  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);

  composer_->GetStringForPreedit(&preedit);
  // "あAa"
  EXPECT_EQ("\xe3\x81\x82\x41\x61", preedit);

  // Reset() should revert the previous input mode (Hiragana).
  composer_->Reset();

  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);
  composer_->GetStringForPreedit(&preedit);
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", preedit);

  // Typing "A" temporarily switch the input mode.  The input mode
  // should be reverted back after reset.
  composer_->SetInputMode(transliteration::FULL_KATAKANA);
  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);
  composer_->GetStringForPreedit(&preedit);
  // "あア"
  EXPECT_EQ("\xE3\x81\x82\xE3\x82\xA2", preedit);

  key.set_key_code('A');
  composer_->InsertCharacterKeyEvent(key);
  composer_->GetStringForPreedit(&preedit);
  // "あアA"
  EXPECT_EQ("\xE3\x81\x82\xE3\x82\xA2\x41", preedit);

  // Reset() should revert the previous input mode (Katakana).
  composer_->Reset();

  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);
  composer_->GetStringForPreedit(&preedit);
  // "ア"
  EXPECT_EQ("\xE3\x82\xA2", preedit);
}

namespace {
// "山"
const char kYama[] = "\xE5\xB1\xB1";
// "川"
const char kKawa[] = "\xE5\xB7\x9D";
// "空"
const char kSora[] = "\xE7\xA9\xBA";
}  // namespace

TEST_F(ComposerTest, InsertCharacterKeyEventWithUcs4KeyCode) {
  commands::KeyEvent key;

  // Input "山" as key_code.
  key.set_key_code(0x5C71);  // U+5C71 = "山"
  composer_->InsertCharacterKeyEvent(key);

  string preedit;
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

  string preedit;
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(kYama, preedit);

  transliteration::Transliterations transliterations;
  composer_->GetTransliterations(&transliterations);
  EXPECT_EQ(kYama, transliterations[transliteration::HIRAGANA]);
  EXPECT_EQ(kYama, transliterations[transliteration::HALF_ASCII]);
}

TEST_F(ComposerTest, InsertCharacterKeyEventWithAsIs) {
  commands::KeyEvent key;
  // "あ"
  table_->AddRule("a", "\xe3\x81\x82", "");
  // "ー"
  table_->AddRule("-", "\xE3\x83\xBC", "");

  key.set_key_code('a');
  composer_->InsertCharacterKeyEvent(key);

  string preedit;
  composer_->GetStringForPreedit(&preedit);
  // "あ"
  EXPECT_EQ("\xe3\x81\x82", preedit);

  // Full width "０" will be inserted.
  key.set_key_code('0');
  key.set_key_string("0");
  composer_->InsertCharacterKeyEvent(key);

  composer_->GetStringForPreedit(&preedit);
  // "あ０"
  EXPECT_EQ("\xE3\x81\x82\xEF\xBC\x90", preedit);

  // Half width "0" will be inserted.
  key.set_key_code('0');
  key.set_key_string("0");
  key.set_input_style(commands::KeyEvent::AS_IS);
  composer_->InsertCharacterKeyEvent(key);

  composer_->GetStringForPreedit(&preedit);
  // "あ０0"
  EXPECT_EQ("\xE3\x81\x82\xEF\xBC\x90\x30", preedit);

  // Full width "0" will be inserted.
  key.set_key_code('0');
  key.set_key_string("0");
  key.set_input_style(commands::KeyEvent::FOLLOW_MODE);
  composer_->InsertCharacterKeyEvent(key);

  composer_->GetStringForPreedit(&preedit);
  // "あ０0０"
  EXPECT_EQ("\xE3\x81\x82\xEF\xBC\x90\x30\xEF\xBC\x90", preedit);

  // Half width "-" will be inserted.
  key.set_key_code('-');
  key.set_key_string("-");
  key.set_input_style(commands::KeyEvent::AS_IS);
  composer_->InsertCharacterKeyEvent(key);

  composer_->GetStringForPreedit(&preedit);
  // "あ０0０-"
  EXPECT_EQ("\xE3\x81\x82\xEF\xBC\x90\x30\xEF\xBC\x90\x2D", preedit);

  // Full width "−" will be inserted.
  key.set_key_code('-');
  // "−"
  key.set_key_string("\xE2\x88\x92");
  key.set_input_style(commands::KeyEvent::FOLLOW_MODE);
  composer_->InsertCharacterKeyEvent(key);

  composer_->GetStringForPreedit(&preedit);
  // "あ０0０-−"
  EXPECT_EQ("\xE3\x81\x82\xEF\xBC\x90\x30\xEF\xBC\x90\x2D\xE2\x88\x92",
            preedit);
}

TEST_F(ComposerTest, InsertCharacterKeyEventWithInputMode) {
  // "あ"
  table_->AddRule("a", "\xE3\x81\x82", "");
  // "い"
  table_->AddRule("i", "\xE3\x81\x84", "");
  // "う"
  table_->AddRule("u", "\xE3\x81\x86", "");

  {
    // "a" → "あ" (Hiragana)
    EXPECT_TRUE(InsertKeyWithMode("a", commands::HIRAGANA, composer_.get()));
    // "あ"
    EXPECT_EQ("\xE3\x81\x82", GetPreedit(composer_.get()));
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());

    // "aI" → "あI" (Alphanumeric)
    EXPECT_TRUE(InsertKeyWithMode("I", commands::HIRAGANA, composer_.get()));
    // "あI"
    EXPECT_EQ("\xE3\x81\x82\x49", GetPreedit(composer_.get()));
    EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());

    // "u" → "あIu" (Alphanumeric)
    EXPECT_TRUE(InsertKeyWithMode("u", commands::HALF_ASCII, composer_.get()));
    // "あIu"
    EXPECT_EQ("\xE3\x81\x82\x49\x75", GetPreedit(composer_.get()));
    EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());

    // [shift] → "あIu" (Hiragana)
    EXPECT_TRUE(InsertKeyWithMode("Shift", commands::HALF_ASCII,
                                  composer_.get()));
    // "あIu"
    EXPECT_EQ("\xE3\x81\x82\x49\x75", GetPreedit(composer_.get()));
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());

    // "u" → "あIuう" (Hiragana)
    EXPECT_TRUE(InsertKeyWithMode("u", commands::HIRAGANA, composer_.get()));
    // "あIuう"
    EXPECT_EQ("\xE3\x81\x82\x49\x75\xE3\x81\x86", GetPreedit(composer_.get()));
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());
  }

  composer_.reset(new Composer(table_.get(), default_request_));

  {
    // "a" → "あ" (Hiragana)
    EXPECT_TRUE(InsertKeyWithMode("a", commands::HIRAGANA, composer_.get()));
    // "あ"
    EXPECT_EQ("\xE3\x81\x82", GetPreedit(composer_.get()));
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());

    // "i" (Katakana) → "あイ" (Katakana)
    EXPECT_TRUE(InsertKeyWithMode("i", commands::FULL_KATAKANA,
                                  composer_.get()));
    // "あイ"
    EXPECT_EQ("\xE3\x81\x82\xE3\x82\xA4", GetPreedit(composer_.get()));
    EXPECT_EQ(transliteration::FULL_KATAKANA, composer_->GetInputMode());

    // SetInputMode(Alphanumeric) → "あイ" (Alphanumeric)
    composer_->SetInputMode(transliteration::HALF_ASCII);
    EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());

    // [shift] → "あイ" (Alphanumeric) - Nothing happens.
    EXPECT_TRUE(InsertKeyWithMode("Shift", commands::HALF_ASCII,
                                  composer_.get()));
    // "あイ"
    EXPECT_EQ("\xE3\x81\x82\xE3\x82\xA4", GetPreedit(composer_.get()));
    EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());

    // "U" → "あイ" (Alphanumeric)
    EXPECT_TRUE(InsertKeyWithMode("U", commands::HALF_ASCII,
                                  composer_.get()));
    // "あイU"
    EXPECT_EQ("\xE3\x81\x82\xE3\x82\xA4\x55", GetPreedit(composer_.get()));
    EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());

    // [shift] → "あイU" (Alphanumeric) - Nothing happens.
    EXPECT_TRUE(InsertKeyWithMode("Shift", commands::HALF_ASCII,
                                  composer_.get()));
    // "あイU"
    EXPECT_EQ("\xE3\x81\x82\xE3\x82\xA4\x55", GetPreedit(composer_.get()));
    EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());
  }
}

TEST_F(ComposerTest, ApplyTemporaryInputMode) {
  const bool kCapsLocked = true;
  const bool kCapsUnlocked = false;

  // "あ"
  table_->AddRule("a", "\xE3\x81\x82", "");
  composer_->SetInputMode(transliteration::HIRAGANA);

  // Since handlings of continuous shifted input differ,
  // test cases differ between ASCII_INPUT_MODE and KATAKANA_INPUT_MODE

  {  // ASCII_INPUT_MODE (w/o CapsLock)
    Config config;
    ConfigHandler::GetConfig(&config);
    config.set_shift_key_mode_switch(Config::ASCII_INPUT_MODE);
    ConfigHandler::SetConfig(config);

    // pair<input, use_temporary_input_mode>
    pair<string, bool> kTestDataAscii[] = {
      make_pair("a", false),
      make_pair("A", true),
      make_pair("a", true),
      make_pair("a", true),
      make_pair("A", true),
      make_pair("A", true),
      make_pair("a", false),
      make_pair("A", true),
      make_pair("A", true),
      make_pair("A", true),
      make_pair("a", false),
      make_pair("A", true),
      make_pair(".", true),
      make_pair("a", true),
      make_pair("A", true),
      make_pair("A", true),
      make_pair(".", true),
      make_pair("a", true),
      // "あ"
      make_pair("\xE3\x81\x82", false),
      make_pair("a", false),
    };

    for (int i = 0; i < arraysize(kTestDataAscii); ++i) {
      composer_->ApplyTemporaryInputMode(kTestDataAscii[i].first,
                                         kCapsUnlocked);

      const transliteration::TransliterationType expected =
          kTestDataAscii[i].second
          ? transliteration::HALF_ASCII
          : transliteration::HIRAGANA;

      EXPECT_EQ(expected, composer_->GetInputMode()) << "index=" << i;
      EXPECT_EQ(transliteration::HIRAGANA, composer_->GetComebackInputMode())
          << "index=" << i;
    }
  }

  {  // ASCII_INPUT_MODE (w/ CapsLock)
    Config config;
    ConfigHandler::GetConfig(&config);
    config.set_shift_key_mode_switch(Config::ASCII_INPUT_MODE);
    ConfigHandler::SetConfig(config);

    // pair<input, use_temporary_input_mode>
    pair<string, bool> kTestDataAscii[] = {
      make_pair("A", false),
      make_pair("a", true),
      make_pair("A", true),
      make_pair("A", true),
      make_pair("a", true),
      make_pair("a", true),
      make_pair("A", false),
      make_pair("a", true),
      make_pair("a", true),
      make_pair("a", true),
      make_pair("A", false),
      make_pair("a", true),
      make_pair(".", true),
      make_pair("A", true),
      make_pair("a", true),
      make_pair("a", true),
      make_pair(".", true),
      make_pair("A", true),
      // "あ"
      make_pair("\xE3\x81\x82", false),
      make_pair("A", false),
    };

    for (int i = 0; i < arraysize(kTestDataAscii); ++i) {
      composer_->ApplyTemporaryInputMode(kTestDataAscii[i].first,
                                         kCapsLocked);

      const transliteration::TransliterationType expected =
          kTestDataAscii[i].second
          ? transliteration::HALF_ASCII
          : transliteration::HIRAGANA;

      EXPECT_EQ(expected, composer_->GetInputMode()) << "index=" << i;
      EXPECT_EQ(transliteration::HIRAGANA, composer_->GetComebackInputMode())
          << "index=" << i;
    }
  }

  {  // KATAKANA_INPUT_MODE (w/o CapsLock)
    Config config;
    ConfigHandler::GetConfig(&config);
    config.set_shift_key_mode_switch(Config::KATAKANA_INPUT_MODE);
    ConfigHandler::SetConfig(config);

    // pair<input, use_temporary_input_mode>
    pair<string, bool> kTestDataKatakana[] = {
      make_pair("a", false),
      make_pair("A", true),
      make_pair("a", false),
      make_pair("a", false),
      make_pair("A", true),
      make_pair("A", true),
      make_pair("a", false),
      make_pair("A", true),
      make_pair("A", true),
      make_pair("A", true),
      make_pair("a", false),
      make_pair("A", true),
      make_pair(".", true),
      make_pair("a", false),
      make_pair("A", true),
      make_pair("A", true),
      make_pair(".", true),
      make_pair("a", false),
      // "あ"
      make_pair("\xE3\x81\x82", false),
      make_pair("a", false),
    };

    for (int i = 0; i < arraysize(kTestDataKatakana); ++i) {
      composer_->ApplyTemporaryInputMode(kTestDataKatakana[i].first,
                                         kCapsUnlocked);

      const transliteration::TransliterationType expected =
          kTestDataKatakana[i].second
          ? transliteration::FULL_KATAKANA
          : transliteration::HIRAGANA;

      EXPECT_EQ(expected, composer_->GetInputMode()) << "index=" << i;
      EXPECT_EQ(transliteration::HIRAGANA, composer_->GetComebackInputMode())
          << "index=" << i;
    }
  }

  {  // KATAKANA_INPUT_MODE (w/ CapsLock)
    Config config;
    ConfigHandler::GetConfig(&config);
    config.set_shift_key_mode_switch(Config::KATAKANA_INPUT_MODE);
    ConfigHandler::SetConfig(config);

    // pair<input, use_temporary_input_mode>
    pair<string, bool> kTestDataKatakana[] = {
      make_pair("A", false),
      make_pair("a", true),
      make_pair("A", false),
      make_pair("A", false),
      make_pair("a", true),
      make_pair("a", true),
      make_pair("A", false),
      make_pair("a", true),
      make_pair("a", true),
      make_pair("a", true),
      make_pair("A", false),
      make_pair("a", true),
      make_pair(".", true),
      make_pair("A", false),
      make_pair("a", true),
      make_pair("a", true),
      make_pair(".", true),
      make_pair("A", false),
      // "あ"
      make_pair("\xE3\x81\x82", false),
      make_pair("A", false),
    };

    for (int i = 0; i < arraysize(kTestDataKatakana); ++i) {
      composer_->ApplyTemporaryInputMode(kTestDataKatakana[i].first,
                                         kCapsLocked);

      const transliteration::TransliterationType expected =
          kTestDataKatakana[i].second
          ? transliteration::FULL_KATAKANA
          : transliteration::HIRAGANA;

      EXPECT_EQ(expected, composer_->GetInputMode()) << "index=" << i;
      EXPECT_EQ(transliteration::HIRAGANA, composer_->GetComebackInputMode())
          << "index=" << i;
    }
  }
}

TEST_F(ComposerTest, CopyFrom) {
  // "あ"
  table_->AddRule("a", "\xE3\x81\x82", "");
  // "ん"
  table_->AddRule("n", "\xE3\x82\x93", "");
  // "な"
  table_->AddRule("na", "\xE3\x81\xAA", "");

  {
    SCOPED_TRACE("Precomposition");

    string src_composition;
    composer_->GetStringForSubmission(&src_composition);
    EXPECT_EQ("", src_composition);

    Composer dest(NULL, default_request_);
    dest.CopyFrom(*composer_);

    ExpectSameComposer(*composer_, dest);
  }

  {
    SCOPED_TRACE("Composition");

    composer_->InsertCharacter("a");
    composer_->InsertCharacter("n");
    string src_composition;
    composer_->GetStringForSubmission(&src_composition);
    // "あｎ"
    EXPECT_EQ("\xE3\x81\x82\xEF\xBD\x8E", src_composition);

    Composer dest(NULL, default_request_);
    dest.CopyFrom(*composer_);

    ExpectSameComposer(*composer_, dest);
  }

  {
    SCOPED_TRACE("Conversion");

    string src_composition;
    composer_->GetQueryForConversion(&src_composition);
    // "あん"
    EXPECT_EQ("\xE3\x81\x82\xE3\x82\x93", src_composition);

    Composer dest(NULL, default_request_);
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
    string src_composition;
    composer_->GetStringForSubmission(&src_composition);
    // "AaAAあ"
    EXPECT_EQ("AaAA\xE3\x81\x82", src_composition);

    Composer dest(NULL, default_request_);
    dest.CopyFrom(*composer_);

    ExpectSameComposer(*composer_, dest);
  }

  {
    SCOPED_TRACE("Composition with password mode");

    composer_->Reset();
    composer_->SetInputFieldType(commands::SessionCommand::PASSWORD);
    composer_->SetInputMode(transliteration::HALF_ASCII);
    composer_->SetOutputMode(transliteration::HALF_ASCII);
    composer_->InsertCharacter("M");
    string src_composition;
    composer_->GetStringForSubmission(&src_composition);
    EXPECT_EQ("M", src_composition);

    Composer dest(NULL, default_request_);
    dest.CopyFrom(*composer_);

    ExpectSameComposer(*composer_, dest);
  }
}

TEST_F(ComposerTest, ShiftKeyOperation) {
  commands::KeyEvent key;
  // "あ"
  table_->AddRule("a", "\xe3\x81\x82", "");

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

    string preedit;
    composer_->GetStringForPreedit(&preedit);
    // "あAaああ"
    EXPECT_EQ("\xE3\x81\x82\x41\x61\xE3\x81\x82\xE3\x81\x82", preedit);
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

    string preedit;
    composer_->GetStringForPreedit(&preedit);
    // "アAaアア"
    EXPECT_EQ("\xE3\x82\xA2\x41\x61\xE3\x82\xA2\xE3\x82\xA2", preedit);
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

    string preedit;
    composer_->GetStringForPreedit(&preedit);
    // "AAあAa"
    EXPECT_EQ("\x41\x41\xE3\x81\x82\x41\x61", preedit);
  }

  {  // Multiple shifted characters #2
    composer_->SetInputMode(transliteration::HIRAGANA);
    composer_->Reset();
    InsertKey("D", composer_.get());  // "D"
    InsertKey("&", composer_.get());  // "D&"
    InsertKey("D", composer_.get());  // "D&D"
    InsertKey("2", composer_.get());  // "D&D2"
    InsertKey("a", composer_.get());  // "D&D2a"

    string preedit;
    composer_->GetStringForPreedit(&preedit);
    // "D&D2a"
    EXPECT_EQ("\x44\x26\x44\x32\x61", preedit);
  }

  {  // Full-witdh alphanumeric
    composer_->SetInputMode(transliteration::FULL_ASCII);
    composer_->Reset();
    InsertKey("A", composer_.get());  // "Ａ"
    InsertKey("a", composer_.get());  // "Ａａ"

    string preedit;
    composer_->GetStringForPreedit(&preedit);
    // "Ａａ"
    EXPECT_EQ("\xEF\xBC\xA1\xEF\xBD\x81", preedit);
  }

  {  // Half-witdh alphanumeric
    composer_->SetInputMode(transliteration::HALF_ASCII);
    composer_->Reset();
    InsertKey("A", composer_.get());  // "A"
    InsertKey("a", composer_.get());  // "Aa"

    string preedit;
    composer_->GetStringForPreedit(&preedit);
    EXPECT_EQ("Aa", preedit);
  }
}

TEST_F(ComposerTest, ShiftKeyOperationForKatakana) {
  Config config;
  ConfigHandler::GetConfig(&config);
  config.set_shift_key_mode_switch(Config::KATAKANA_INPUT_MODE);
  ConfigHandler::SetConfig(config);
  table_->Initialize();
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

  string preedit;
  composer_->GetStringForPreedit(&preedit);
  // NOTE(komatsu): "KATakAna" is converted to "カＴあｋアな" rather
  // than "カタカな".  This is a different behavior from Kotoeri due
  // to avoid complecated implementation.  Unless this is a problem
  // for users, this difference probably remains.
  //
  // "カタカな"
  // EXPECT_EQ("\xE3\x82\xAB\xE3\x82\xBF\xE3\x82\xAB\xE3\x81\xAA", preedit);

  // "カＴあｋアな"
  EXPECT_EQ("\xE3\x82\xAB\xEF\xBC\xB4\xE3\x81\x82"
            "\xEF\xBD\x8B\xE3\x82\xA2\xE3\x81\xAA",
            preedit);

  config.set_shift_key_mode_switch(Config::ASCII_INPUT_MODE);
  ConfigHandler::SetConfig(config);
}

TEST_F(ComposerTest, AutoIMETurnOffEnabled) {
  Config config;
  ConfigHandler::GetConfig(&config);
  config.set_preedit_method(Config::ROMAN);
  config.set_use_auto_ime_turn_off(true);
  ConfigHandler::SetConfig(config);

  table_->Initialize();

  commands::KeyEvent key;

  {  // http
    InsertKey("h", composer_.get());
    InsertKey("t", composer_.get());
    InsertKey("t", composer_.get());
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());
    InsertKey("p", composer_.get());

    string preedit;
    composer_->GetStringForPreedit(&preedit);
    EXPECT_EQ("http", preedit);
    EXPECT_EQ(transliteration::HALF_ASCII, composer_->GetInputMode());

    composer_->Reset();
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());
  }

  composer_.reset(new Composer(table_.get(), default_request_));

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
    // "googleあ"
    EXPECT_EQ("google\xE3\x81\x82", GetPreedit(composer_.get()));

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

    // "ｇｏｏｇｌｅ"
    EXPECT_EQ("\xEF\xBD\x87\xEF\xBD\x8F\xEF\xBD\x8F"
              "\xEF\xBD\x87\xEF\xBD\x8C\xEF\xBD\x85",
              GetPreedit(composer_.get()));

    InsertKey("a", composer_.get());
    EXPECT_EQ(transliteration::FULL_ASCII, composer_->GetInputMode());
    // "ｇｏｏｇｌｅａ"
    EXPECT_EQ("\xEF\xBD\x87\xEF\xBD\x8F\xEF\xBD\x8F"
              "\xEF\xBD\x87\xEF\xBD\x8C\xEF\xBD\x85\xEF\xBD\x81",
              GetPreedit(composer_.get()));

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

  config.set_shift_key_mode_switch(Config::OFF);
  ConfigHandler::SetConfig(config);
  composer_.reset(new Composer(table_.get(), default_request_));

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
    // "Googleあ"
    EXPECT_EQ("Google\xE3\x81\x82", GetPreedit(composer_.get()));

    composer_->Reset();
    EXPECT_EQ(transliteration::HIRAGANA, composer_->GetInputMode());
  }
}

TEST_F(ComposerTest, AutoIMETurnOffDisabled) {
  Config config;
  ConfigHandler::GetConfig(&config);

  config.set_preedit_method(Config::ROMAN);
  config.set_use_auto_ime_turn_off(false);
  ConfigHandler::SetConfig(config);

  table_->Initialize();

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

  string preedit;
  composer_->GetStringForPreedit(&preedit);
  // "ｈっｔｐ：・・"
  EXPECT_EQ("\xEF\xBD\x88\xE3\x81\xA3\xEF\xBD\x94"
            "\xEF\xBD\x90\xEF\xBC\x9A\xE3\x83\xBB\xE3\x83\xBB", preedit);
}

TEST_F(ComposerTest, AutoIMETurnOffKana) {
  Config config;
  ConfigHandler::GetConfig(&config);

  config.set_preedit_method(Config::KANA);
  config.set_use_auto_ime_turn_off(true);
  ConfigHandler::SetConfig(config);

  table_->Initialize();

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

  string preedit;
  composer_->GetStringForPreedit(&preedit);
  // "ｈっｔｐ：・・"
  EXPECT_EQ("\xEF\xBD\x88\xE3\x81\xA3\xEF\xBD\x94\xEF\xBD\x90\xEF\xBC\x9A"
            "\xE3\x83\xBB\xE3\x83\xBB", preedit);
}

TEST_F(ComposerTest, KanaPrediction) {
  // "か゛", "か"
  //  table_->AddRule("\xE3\x81\x8B", "\xE3\x81\x8B", "");
  // "か゛", "が"
  //  table_->AddRule("\xE3\x81\x8B\xE3\x82\x9B", "\xE3\x81\x8C", "");
  // "ー", "ー"
  //  table_->AddRule("\xE3\x83\xBC", "\xE3\x83\xBC", "");

  // "か"
  composer_->InsertCharacterKeyAndPreedit("t", "\xE3\x81\x8B");
  {
    string preedit;
    composer_->GetQueryForPrediction(&preedit);
    // "か"
    EXPECT_EQ("\xE3\x81\x8B", preedit);
  }
  // "ー"
  composer_->InsertCharacterKeyAndPreedit("\\", "\xE3\x83\xBC");
  {
    string preedit;
    composer_->GetQueryForPrediction(&preedit);
    // "かー"
    EXPECT_EQ("\xE3\x81\x8B\xE3\x83\xBC", preedit);
  }
  // "、"
  composer_->InsertCharacterKeyAndPreedit(",", "\xE3\x80\x81");
  {
    string preedit;
    composer_->GetQueryForPrediction(&preedit);
    // "かー、"
    EXPECT_EQ("\xE3\x81\x8B\xE3\x83\xBC\xE3\x80\x81", preedit);
  }
}

TEST_F(ComposerTest, KanaTransliteration) {
  // "く゛", "ぐ"
  table_->AddRule("\xE3\x81\x8F\xE3\x82\x9B", "\xE3\x81\x90", "");
  // "く"
  composer_->InsertCharacterKeyAndPreedit("h", "\xE3\x81\x8F");
  // "い"
  composer_->InsertCharacterKeyAndPreedit("e", "\xE3\x81\x84");
  // "り"
  composer_->InsertCharacterKeyAndPreedit("l", "\xE3\x82\x8A");
  // "り"
  composer_->InsertCharacterKeyAndPreedit("l", "\xE3\x82\x8A");
  // "ら"
  composer_->InsertCharacterKeyAndPreedit("o", "\xE3\x82\x89");

  string preedit;
  composer_->GetStringForPreedit(&preedit);
  // "くいりりら"
  EXPECT_EQ("\xE3\x81\x8F\xE3\x81\x84\xE3\x82\x8A\xE3\x82\x8A\xE3\x82\x89",
            preedit);

  transliteration::Transliterations transliterations;
  composer_->GetTransliterations(&transliterations);
  EXPECT_EQ(transliteration::NUM_T13N_TYPES, transliterations.size());
  EXPECT_EQ("hello", transliterations[transliteration::HALF_ASCII]);
}

TEST_F(ComposerTest, SetOutputMode) {
  // "も"
  table_->AddRule("mo", "\xE3\x82\x82", "");
  // "ず"
  table_->AddRule("zu", "\xE3\x81\x9A", "");

  composer_->InsertCharacter("m");
  composer_->InsertCharacter("o");
  composer_->InsertCharacter("z");
  composer_->InsertCharacter("u");

  string output;
  composer_->GetStringForPreedit(&output);
  // "もず"
  EXPECT_EQ("\xE3\x82\x82\xE3\x81\x9A", output);
  EXPECT_EQ(2, composer_->GetCursor());

  composer_->SetOutputMode(transliteration::HALF_ASCII);
  composer_->GetStringForPreedit(&output);
  EXPECT_EQ("mozu", output);
  EXPECT_EQ(4, composer_->GetCursor());

  composer_->SetOutputMode(transliteration::HALF_KATAKANA);
  composer_->GetStringForPreedit(&output);
  // "ﾓｽﾞ"
  EXPECT_EQ("\xEF\xBE\x93\xEF\xBD\xBD\xEF\xBE\x9E", output);
  EXPECT_EQ(3, composer_->GetCursor());
}

TEST_F(ComposerTest, UpdateInputMode) {
  // "あ"
  table_->AddRule("a", "\xE3\x81\x82", "");
  // "い"
  table_->AddRule("i", "\xE3\x81\x84", "");

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

  string output;
  composer_->GetStringForPreedit(&output);
  // "AIあいａｉ"
  EXPECT_EQ("\x41\x49\xE3\x81\x82\xE3\x81\x84\xEF\xBD\x81\xEF\xBD\x89", output);

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
  composer_->SetRequest(request);

  // "あ"
  table_->AddRule("a", "\xE3\x81\x82", "");
  // "い"
  table_->AddRule("i", "\xE3\x81\x84", "");

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

  string output;
  composer_->GetStringForPreedit(&output);
  // "AIあいａｉ"
  EXPECT_EQ("\x41\x49\xE3\x81\x82\xE3\x81\x84\xEF\xBD\x81\xEF\xBD\x89", output);

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
  string query;

  query = "";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  query = "R2D2";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  // "ー１"
  query = "\xE3\x83\xBC\xEF\xBC\x91";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  // "−１"
  EXPECT_EQ("\xE2\x88\x92\xEF\xBC\x91", query);

  // "ーー１"
  query = "\xE3\x83\xBC\xE3\x83\xBC\xEF\xBC\x91";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  // "ー"
  query = "\xE3\x83\xBC";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  // "ーー"
  query = "\xE3\x83\xBC\xE3\x83\xBC";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  // "ーーーーー"
  query = "\xE3\x83\xBC\xE3\x83\xBC\xE3\x83\xBC\xE3\x83\xBC\xE3\x83\xBC";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  // "ｗ"
  query = "\xEF\xBD\x97";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  // "ーｗ"
  query = "\xE3\x83\xBC\xEF\xBD\x97";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  // "ーーｗ"
  query = "\xE3\x83\xBC\xE3\x83\xBC\xEF\xBD\x97";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  // "@" (half-width @)
  query = "@";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  // "ー@" (half-width @)
  query = "\xE3\x83\xBC@";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  // "ーー@" (half-width @)
  query = "\xE3\x83\xBC\xE3\x83\xBC@";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  // "＠" (full-width @)
  query = "\xEF\xBC\xA0";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  // "ー＠" (full-width @)
  query = "\xE3\x83\xBC\xEF\xBC\xA0";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  // "ーー＠" (full-width @)
  query = "\xE3\x83\xBC\xE3\x83\xBC\xEF\xBC\xA0";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  // "まじかー１"
  query = "\xE3\x81\xBE\xE3\x81\x98\xE3\x81\x8B\xE3\x83\xBC\xEF\xBC\x91";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  // "まじかーｗ"
  query = "\xE3\x81\xBE\xE3\x81\x98\xE3\x81\x8B\xE3\x83\xBC\xEF\xBD\x97";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  // "１、０"
  query = "\xEF\xBC\x91\xE3\x80\x81\xEF\xBC\x90";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  // "１，０"
  EXPECT_EQ("\xEF\xBC\x91\xEF\xBC\x8C\xEF\xBC\x90", query);

  // "０。５"
  query = "\xEF\xBC\x90\xE3\x80\x82\xEF\xBC\x95";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  // "０．５"
  EXPECT_EQ("\xEF\xBC\x90\xEF\xBC\x8E\xEF\xBC\x95", query);

  // "ー１、０００。５"
  query = "\xE3\x83\xBC\xEF\xBC\x91\xE3\x80\x81\xEF\xBC\x90\xEF\xBC\x90"
          "\xEF\xBC\x90\xE3\x80\x82\xEF\xBC\x95";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  // "−１，０００．５"
  EXPECT_EQ("\xE2\x88\x92\xEF\xBC\x91\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90"
            "\xEF\xBC\x90\xEF\xBC\x8E\xEF\xBC\x95",
            query);

  // "０３ー"
  query = "\xEF\xBC\x90\xEF\xBC\x93\xE3\x83\xBC";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  // "０３−"
  EXPECT_EQ("\xEF\xBC\x90\xEF\xBC\x93\xE2\x88\x92", query);

  // "０３ーーーーー"
  query = "\xEF\xBC\x90\xEF\xBC\x93"
      "\xE3\x83\xBC\xE3\x83\xBC\xE3\x83\xBC\xE3\x83\xBC\xE3\x83\xBC";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  // "０３−−−−−"
  EXPECT_EQ("\xEF\xBC\x90\xEF\xBC\x93"
            "\xE2\x88\x92\xE2\x88\x92\xE2\x88\x92\xE2\x88\x92\xE2\x88\x92",
            query);

  // "ｘー（ー１）＞ーｘ"
  query = "\xEF\xBD\x98\xE3\x83\xBC\xEF\xBC\x88\xE3\x83\xBC\xEF\xBC\x91"
          "\xEF\xBC\x89\xEF\xBC\x9E\xE3\x83\xBC\xEF\xBD\x98";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  // "ｘ−（−１）＞−ｘ"
  EXPECT_EQ("\xEF\xBD\x98\xE2\x88\x92\xEF\xBC\x88\xE2\x88\x92\xEF\xBC\x91"
            "\xEF\xBC\x89\xEF\xBC\x9E\xE2\x88\x92\xEF\xBD\x98",
            query);

  // "１＊ー２／ー３ーー４"
  query = "\xEF\xBC\x91\xEF\xBC\x8A\xE3\x83\xBC\xEF\xBC\x92\xEF\xBC\x8F"
          "\xE3\x83\xBC\xEF\xBC\x93\xE3\x83\xBC\xE3\x83\xBC\xEF\xBC\x94";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  // "１＊−２／−３−−４"
  EXPECT_EQ("\xEF\xBC\x91\xEF\xBC\x8A\xE2\x88\x92\xEF\xBC\x92\xEF\xBC\x8F"
            "\xE2\x88\x92\xEF\xBC\x93\xE2\x88\x92\xE2\x88\x92\xEF\xBC\x94",
            query);

  // "ＡーＺ"
  query = "\xEF\xBC\xA1\xE3\x83\xBC\xEF\xBC\xBA";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  // "Ａ−Ｚ"
  EXPECT_EQ("\xEF\xBC\xA1\xE2\x88\x92\xEF\xBC\xBA", query);

  // "もずく、うぉーきんぐ。"
  query = "\xE3\x82\x82\xE3\x81\x9A\xE3\x81\x8F\xE3\x80\x81\xE3\x81\x86"
          "\xE3\x81\x89\xE3\x83\xBC\xE3\x81\x8D\xE3\x82\x93\xE3\x81\x90"
          "\xE3\x80\x82";
  EXPECT_FALSE(Composer::TransformCharactersForNumbers(&query));

  // "えー２、９８０円！月々たった、２、９８０円？"
  query = "\xE3\x81\x88\xE3\x83\xBC\xEF\xBC\x92\xE3\x80\x81\xEF\xBC\x99"
          "\xEF\xBC\x98\xEF\xBC\x90\xE5\x86\x86\xEF\xBC\x81\xE6\x9C\x88"
          "\xE3\x80\x85\xE3\x81\x9F\xE3\x81\xA3\xE3\x81\x9F\xE3\x80\x81"
          "\xEF\xBC\x92\xE3\x80\x81\xEF\xBC\x99\xEF\xBC\x98\xEF\xBC\x90"
          "\xE5\x86\x86\xEF\xBC\x9F";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  // "えー２，９８０円！月々たった、２，９８０円？"
  EXPECT_EQ("\xE3\x81\x88\xE3\x83\xBC\xEF\xBC\x92\xEF\xBC\x8C\xEF\xBC\x99"
            "\xEF\xBC\x98\xEF\xBC\x90\xE5\x86\x86\xEF\xBC\x81\xE6\x9C\x88"
            "\xE3\x80\x85\xE3\x81\x9F\xE3\x81\xA3\xE3\x81\x9F\xE3\x80\x81"
            "\xEF\xBC\x92\xEF\xBC\x8C\xEF\xBC\x99\xEF\xBC\x98\xEF\xBC\x90"
            "\xE5\x86\x86\xEF\xBC\x9F",
            query);

  // "およそ、３。１４１５９。"
  query = "\xE3\x81\x8A\xE3\x82\x88\xE3\x81\x9D\xE3\x80\x81\xEF\xBC\x93"
          "\xE3\x80\x82\xEF\xBC\x91\xEF\xBC\x94\xEF\xBC\x91\xEF\xBC\x95"
          "\xEF\xBC\x99\xE3\x80\x82";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  // "およそ、３．１４１５９．"
  EXPECT_EQ("\xE3\x81\x8A\xE3\x82\x88\xE3\x81\x9D\xE3\x80\x81\xEF\xBC\x93"
            "\xEF\xBC\x8E\xEF\xBC\x91\xEF\xBC\x94\xEF\xBC\x91\xEF\xBC\x95"
            "\xEF\xBC\x99\xEF\xBC\x8E",
            query);

  // "１００、" => "１００，"
  query = "\xEF\xBC\x91\xEF\xBC\x90\xEF\xBC\x90\xE3\x80\x81";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ("\xEF\xBC\x91\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x8C",
            query);

  // "１００。" => "１００．"
  query = "\xEF\xBC\x91\xEF\xBC\x90\xEF\xBC\x90\xE3\x80\x82";
  EXPECT_TRUE(Composer::TransformCharactersForNumbers(&query));
  EXPECT_EQ("\xEF\xBC\x91\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x8E",
            query);
}

TEST_F(ComposerTest, PreeditFormAfterCharacterTransform) {
  CharacterFormManager *manager =
      CharacterFormManager::GetCharacterFormManager();
  table_->AddRule("0", "\xEF\xBC\x90", "");  // "０"
  table_->AddRule("1", "\xEF\xBC\x91", "");  // "１"
  table_->AddRule("2", "\xEF\xBC\x92", "");  // "２"
  table_->AddRule("3", "\xEF\xBC\x93", "");  // "３"
  table_->AddRule("4", "\xEF\xBC\x94", "");  // "４"
  table_->AddRule("5", "\xEF\xBC\x95", "");  // "５"
  table_->AddRule("6", "\xEF\xBC\x96", "");  // "６"
  table_->AddRule("7", "\xEF\xBC\x97", "");  // "７"
  table_->AddRule("8", "\xEF\xBC\x98", "");  // "８"
  table_->AddRule("9", "\xEF\xBC\x99", "");  // "９"
  table_->AddRule("-", "\xE3\x83\xBC", "");  // "ー"
  table_->AddRule(",", "\xE3\x80\x81", "");  // "、"
  table_->AddRule(".", "\xE3\x80\x82", "");  // "。"


  {
    composer_->Reset();
    manager->SetDefaultRule();
    manager->AddPreeditRule("1", Config::HALF_WIDTH);
    manager->AddPreeditRule(".,", Config::HALF_WIDTH);
    composer_->InsertCharacter("3.14");
    string result;
    composer_->GetStringForPreedit(&result);
    EXPECT_EQ("3.14",
              result);
  }

  {
    composer_->Reset();
    manager->SetDefaultRule();
    manager->AddPreeditRule("1", Config::FULL_WIDTH);
    manager->AddPreeditRule(".,", Config::HALF_WIDTH);
    composer_->InsertCharacter("3.14");
    string result;
    composer_->GetStringForPreedit(&result);
    // "３.１４"
    EXPECT_EQ("\xef\xbc\x93\x2e\xef\xbc\x91\xef\xbc\x94",
              result);
  }

  {
    composer_->Reset();
    manager->SetDefaultRule();
    manager->AddPreeditRule("1", Config::HALF_WIDTH);
    manager->AddPreeditRule(".,", Config::FULL_WIDTH);
    composer_->InsertCharacter("3.14");
    string result;
    composer_->GetStringForPreedit(&result);
    // "3．14"
    EXPECT_EQ("\x33\xef\xbc\x8e\x31\x34",
              result);
  }

  {
    composer_->Reset();
    manager->SetDefaultRule();
    manager->AddPreeditRule("1", Config::FULL_WIDTH);
    manager->AddPreeditRule(".,", Config::FULL_WIDTH);
    composer_->InsertCharacter("3.14");
    string result;
    composer_->GetStringForPreedit(&result);
    // "３．１４"
    EXPECT_EQ("\xef\xbc\x93\xef\xbc\x8e\xef\xbc\x91\xef\xbc\x94",
              result);
  }
}

TEST_F(ComposerTest, ComposingWithcharactertransform) {
  table_->AddRule("0", "\xEF\xBC\x90", "");  // "０"
  table_->AddRule("1", "\xEF\xBC\x91", "");  // "１"
  table_->AddRule("2", "\xEF\xBC\x92", "");  // "２"
  table_->AddRule("3", "\xEF\xBC\x93", "");  // "３"
  table_->AddRule("4", "\xEF\xBC\x94", "");  // "４"
  table_->AddRule("5", "\xEF\xBC\x95", "");  // "５"
  table_->AddRule("6", "\xEF\xBC\x96", "");  // "６"
  table_->AddRule("7", "\xEF\xBC\x97", "");  // "７"
  table_->AddRule("8", "\xEF\xBC\x98", "");  // "８"
  table_->AddRule("9", "\xEF\xBC\x99", "");  // "９"
  table_->AddRule("-", "\xE3\x83\xBC", "");  // "ー"
  table_->AddRule(",", "\xE3\x80\x81", "");  // "、"
  table_->AddRule(".", "\xE3\x80\x82", "");  // "。"
  composer_->InsertCharacter("-1,000.5");

  {
    string result;
    composer_->GetStringForPreedit(&result);
    // "−１，０００．５"
    EXPECT_EQ("\xE2\x88\x92\xEF\xBC\x91\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90"
              "\xEF\xBC\x90\xEF\xBC\x8E\xEF\xBC\x95",
              result);
  }
  {
    string result;
    composer_->GetStringForSubmission(&result);
    // "−１，０００．５"
    EXPECT_EQ("\xE2\x88\x92\xEF\xBC\x91\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90"
              "\xEF\xBC\x90\xEF\xBC\x8E\xEF\xBC\x95",
              result);
  }
  {
    string result;
    composer_->GetQueryForConversion(&result);
    EXPECT_EQ("-1,000.5", result);
  }
  {
    string result;
    composer_->GetQueryForPrediction(&result);
    EXPECT_EQ("-1,000.5", result);
  }
  {
    string left, focused, right;
    // Right edge
    composer_->GetPreedit(&left, &focused, &right);
    // "−１，０００．５"
    EXPECT_EQ("\xE2\x88\x92\xEF\xBC\x91\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90"
              "\xEF\xBC\x90\xEF\xBC\x8E\xEF\xBC\x95",
              left);
    EXPECT_TRUE(focused.empty());
    EXPECT_TRUE(right.empty());

    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    // "−１，０００．"
    EXPECT_EQ("\xE2\x88\x92\xEF\xBC\x91\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90"
              "\xEF\xBC\x90\xEF\xBC\x8E",
              left);
    // "５"
    EXPECT_EQ("\xEF\xBC\x95", focused);
    EXPECT_TRUE(right.empty());

    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    // "−１，０００"
    EXPECT_EQ("\xE2\x88\x92\xEF\xBC\x91\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90"
              "\xEF\xBC\x90",
              left);
    // "．"
    EXPECT_EQ("\xEF\xBC\x8E", focused);
    // "５"
    EXPECT_EQ("\xEF\xBC\x95", right);

    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    // "−１，００"
    EXPECT_EQ("\xE2\x88\x92\xEF\xBC\x91\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90",
              left);
    // "０"
    EXPECT_EQ("\xEF\xBC\x90", focused);
    // "．５"
    EXPECT_EQ("\xEF\xBC\x8E\xEF\xBC\x95", right);

    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    // "−１，０"
    EXPECT_EQ("\xE2\x88\x92\xEF\xBC\x91\xEF\xBC\x8C\xEF\xBC\x90", left);
    // "０"
    EXPECT_EQ("\xEF\xBC\x90", focused);
    // "０．５"
    EXPECT_EQ("\xEF\xBC\x90\xEF\xBC\x8E\xEF\xBC\x95", right);

    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    // "−１，"
    EXPECT_EQ("\xE2\x88\x92\xEF\xBC\x91\xEF\xBC\x8C", left);
    // "０"
    EXPECT_EQ("\xEF\xBC\x90", focused);
    // "００．５"
    EXPECT_EQ("\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x8E\xEF\xBC\x95", right);

    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    // "−１"
    EXPECT_EQ("\xE2\x88\x92\xEF\xBC\x91", left);
    // "，"
    EXPECT_EQ("\xEF\xBC\x8C", focused);
    // "０００．５"
    EXPECT_EQ("\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x8E\xEF\xBC\x95",
              right);

    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    // "−"
    EXPECT_EQ("\xE2\x88\x92", left);
    // "１"
    EXPECT_EQ("\xEF\xBC\x91", focused);
    // "，０００．５"
    EXPECT_EQ("\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x8E"
              "\xEF\xBC\x95",
              right);

    // Left edge
    composer_->MoveCursorLeft();
    composer_->GetPreedit(&left, &focused, &right);
    EXPECT_TRUE(left.empty());
    // "−"
    EXPECT_EQ("\xE2\x88\x92", focused);
    // "１，０００．５"
    EXPECT_EQ("\xEF\xBC\x91\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90"
              "\xEF\xBC\x8E\xEF\xBC\x95",
              right);
  }
}

TEST_F(ComposerTest, AlphanumericOfSSH) {
  // This is a unittest against http://b/3199626
  // 'ssh' (っｓｈ) + F10 should be 'ssh'.
  table_->AddRule("ss", "[X]", "s");
  table_->AddRule("sha", "[SHA]", "");
  composer_->InsertCharacter("ssh");
  // "［Ｘ］ｓｈ"
  EXPECT_EQ("\xEF\xBC\xBB\xEF\xBC\xB8\xEF\xBC\xBD\xEF\xBD\x93\xEF\xBD\x88",
            GetPreedit(composer_.get()));

  string query;
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
  // "ち"
  key.set_key_string("\xE3\x81\xA1");

  // Toggle the input mode to HALF_ASCII
  composer_->ToggleInputMode();
  EXPECT_TRUE(composer_->InsertCharacterKeyEvent(key));
  string output;
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
  // "aち"
  EXPECT_EQ("a\xE3\x81\xA1", output);
}

TEST_F(ComposerTest, Issue1817410) {
  // This is a unittest against http://b/2190364
  // "っ"
  table_->AddRule("ss", "\xE3\x81\xA3", "s");

  InsertKey("s", composer_.get());
  InsertKey("s", composer_.get());

  string preedit;
  composer_->GetStringForPreedit(&preedit);
  // "っｓ"
  EXPECT_EQ("\xE3\x81\xA3\xEF\xBD\x93", preedit);

  string t13n;
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
  // "ん"
  table_->AddRule("n", "\xe3\x82\x93", "");
  // "な"
  table_->AddRule("na", "\xe3\x81\xaa", "");
  // "や"
  table_->AddRule("ya", "\xe3\x82\x84", "");
  // "にゃ"
  table_->AddRule("nya", "\xe3\x81\xab\xe3\x82\x83", "");

  InsertKey("n", composer_.get());
  InsertKey("y", composer_.get());

  string result;
  composer_->GetQueryForConversion(&result);
  // "んy"
  EXPECT_EQ("\xe3\x82\x93y", result);
}

TEST_F(ComposerTest, Issue2819580_2) {
  // This is a unittest against http://b/2819580.
  // 'y' after 'n' disappears.
  // "ぽ"
  table_->AddRule("po", "\xe3\x81\xbd", "");
  // "ん"
  table_->AddRule("n", "\xe3\x82\x93", "");
  // "な"
  table_->AddRule("na", "\xe3\x81\xaa", "");
  // "や"
  table_->AddRule("ya", "\xe3\x82\x84", "");
  // "にゃ"
  table_->AddRule("nya", "\xe3\x81\xab\xe3\x82\x83", "");

  InsertKey("p", composer_.get());
  InsertKey("o", composer_.get());
  InsertKey("n", composer_.get());
  InsertKey("y", composer_.get());

  string result;
  composer_->GetQueryForConversion(&result);
  // "ぽんy"
  EXPECT_EQ("\xe3\x81\xbd\xe3\x82\x93y", result);
}

TEST_F(ComposerTest, Issue2819580_3) {
  // This is a unittest against http://b/2819580.
  // 'y' after 'n' disappears.
  // "ん"
  table_->AddRule("n", "\xe3\x82\x93", "");
  // "な"
  table_->AddRule("na", "\xe3\x81\xaa", "");
  // "や"
  table_->AddRule("ya", "\xe3\x82\x84", "");
  // "にゃ"
  table_->AddRule("nya", "\xe3\x81\xab\xe3\x82\x83", "");

  InsertKey("z", composer_.get());
  InsertKey("n", composer_.get());
  InsertKey("y", composer_.get());

  string result;
  composer_->GetQueryForConversion(&result);
  // "zんy"
  EXPECT_EQ("z\xe3\x82\x93y", result);
}

TEST_F(ComposerTest, Issue2797991_1) {
  // This is a unittest against http://b/2797991.
  // Half-width alphanumeric mode quits after [CAPITAL LETTER]:[CAPITAL LETTER]
  // e.g. C:\Wi -> C:\Wい

  // "い"
  table_->AddRule("i", "\xe3\x81\x84", "");

  InsertKey("C", composer_.get());
  InsertKey(":", composer_.get());
  InsertKey("\\", composer_.get());
  InsertKey("W", composer_.get());
  InsertKey("i", composer_.get());

  string result;
  composer_->GetStringForPreedit(&result);
  EXPECT_EQ("C:\\Wi", result);
}

TEST_F(ComposerTest, Issue2797991_2) {
  // This is a unittest against http://b/2797991.
  // Half-width alphanumeric mode quits after [CAPITAL LETTER]:[CAPITAL LETTER]
  // e.g. C:\Wi -> C:\Wい

  // "い"
  table_->AddRule("i", "\xe3\x81\x84", "");

  InsertKey("C", composer_.get());
  InsertKey(":", composer_.get());
  InsertKey("W", composer_.get());
  InsertKey("i", composer_.get());

  string result;
  composer_->GetStringForPreedit(&result);
  EXPECT_EQ("C:Wi", result);
}

TEST_F(ComposerTest, Issue2797991_3) {
  // This is a unittest against http://b/2797991.
  // Half-width alphanumeric mode quits after [CAPITAL LETTER]:[CAPITAL LETTER]
  // e.g. C:\Wi -> C:\Wい

  // "い"
  table_->AddRule("i", "\xe3\x81\x84", "");

  InsertKey("C", composer_.get());
  InsertKey(":", composer_.get());
  InsertKey("\\", composer_.get());
  InsertKey("W", composer_.get());
  InsertKey("i", composer_.get());
  InsertKeyWithMode("i", commands::HIRAGANA, composer_.get());
  string result;
  composer_->GetStringForPreedit(&result);
  // "C:\Wiい"
  EXPECT_EQ("C:\\Wi\xe3\x81\x84", result);
}

TEST_F(ComposerTest, Issue2797991_4) {
  // This is a unittest against http://b/2797991.
  // Half-width alphanumeric mode quits after [CAPITAL LETTER]:[CAPITAL LETTER]
  // e.g. C:\Wi -> C:\Wい

  // "い"
  table_->AddRule("i", "\xe3\x81\x84", "");

  InsertKey("c", composer_.get());
  InsertKey(":", composer_.get());
  InsertKey("\\", composer_.get());
  InsertKey("W", composer_.get());
  InsertKey("i", composer_.get());

  string result;
  composer_->GetStringForPreedit(&result);
  EXPECT_EQ("c:\\Wi", result);
}

TEST_F(ComposerTest, CaseSensitiveByConfiguration) {
  {
    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    config.set_shift_key_mode_switch(Config::OFF);
    ConfigHandler::SetConfig(config);
    EXPECT_TRUE(ConfigHandler::SetConfig(config));
    table_->Initialize();

    // i -> "い"
    table_->AddRule("i", "\xe3\x81\x84", "");
    // I -> "イ"
    table_->AddRule("I", "\xe3\x82\xa4", "");

    InsertKey("i", composer_.get());
    InsertKey("I", composer_.get());
    InsertKey("i", composer_.get());
    InsertKey("I", composer_.get());
    string result;
    composer_->GetStringForPreedit(&result);
    // "いイいイ"
    EXPECT_EQ("\xe3\x81\x84\xe3\x82\xa4\xe3\x81\x84\xe3\x82\xa4", result);
  }
  composer_->Reset();
  {
    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    config.set_shift_key_mode_switch(Config::ASCII_INPUT_MODE);
    ConfigHandler::SetConfig(config);
    EXPECT_TRUE(ConfigHandler::SetConfig(config));
    table_->Initialize();

    // i -> "い"
    table_->AddRule("i", "\xe3\x81\x84", "");
    // I -> "イ"
    table_->AddRule("I", "\xe3\x82\xa4", "");

    InsertKey("i", composer_.get());
    InsertKey("I", composer_.get());
    InsertKey("i", composer_.get());
    InsertKey("I", composer_.get());
    string result;
    composer_->GetStringForPreedit(&result);
    // "いIiI"
    EXPECT_EQ("\xe3\x81\x84IiI", result);
  }
}

TEST_F(ComposerTest,
       InputUppercaseInAlphanumericModeWithShiftKeyModeSwitchIsKatakana) {
  {
    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    config.set_shift_key_mode_switch(Config::KATAKANA_INPUT_MODE);
    ConfigHandler::SetConfig(config);
    EXPECT_TRUE(ConfigHandler::SetConfig(config));
    table_->Initialize();

    // i -> "い"
    table_->AddRule("i", "\xe3\x81\x84", "");
    // I -> "イ"
    table_->AddRule("I", "\xe3\x82\xa4", "");

    {
      composer_->Reset();
      composer_->SetInputMode(transliteration::FULL_ASCII);
      InsertKey("I", composer_.get());
      string result;
      composer_->GetStringForPreedit(&result);
      // "Ｉ"
      EXPECT_EQ("\xef\xbc\xa9", result);
    }

    {
      composer_->Reset();
      composer_->SetInputMode(transliteration::HALF_ASCII);
      InsertKey("I", composer_.get());
      string result;
      composer_->GetStringForPreedit(&result);
      EXPECT_EQ("I", result);
    }

    {
      composer_->Reset();
      composer_->SetInputMode(transliteration::FULL_KATAKANA);
      InsertKey("I", composer_.get());
      string result;
      composer_->GetStringForPreedit(&result);
      // "イ"
      EXPECT_EQ("\xe3\x82\xa4", result);
    }

    {
      composer_->Reset();
      composer_->SetInputMode(transliteration::HALF_KATAKANA);
      InsertKey("I", composer_.get());
      string result;
      composer_->GetStringForPreedit(&result);
      // "ｲ"
      EXPECT_EQ("\xEF\xBD\xB2", result);
    }

    {
      composer_->Reset();
      composer_->SetInputMode(transliteration::HIRAGANA);
      InsertKey("I", composer_.get());
      string result;
      composer_->GetStringForPreedit(&result);
      // "イ"
      EXPECT_EQ("\xe3\x82\xa4", result);
    }
  }
}

TEST_F(ComposerTest,
       DeletingAlphanumericPartShouldQuitToggleAlphanumericMode) {
  // http://b/2206560
  // 1. Type "iGoogle" (preedit text turns to be "いGoogle")
  // 2. Type Back-space 6 times ("い")
  // 3. Type "i" (should be "いい")

  table_->Initialize();

  // i -> "い"
  table_->AddRule("i", "\xe3\x81\x84", "");

  InsertKey("i", composer_.get());
  InsertKey("G", composer_.get());
  InsertKey("o", composer_.get());
  InsertKey("o", composer_.get());
  InsertKey("g", composer_.get());
  InsertKey("l", composer_.get());
  InsertKey("e", composer_.get());

  {
    string result;
    composer_->GetStringForPreedit(&result);
    // "いGoogle"
    EXPECT_EQ("\xe3\x81\x84Google", result);
  }

  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();

  {
    string result;
    composer_->GetStringForPreedit(&result);
    // "い"
    EXPECT_EQ("\xe3\x81\x84", result);
  }

  InsertKey("i", composer_.get());

  {
    string result;
    composer_->GetStringForPreedit(&result);
    // "いい"
    EXPECT_EQ("\xe3\x81\x84\xe3\x81\x84", result);
  }
}

TEST_F(ComposerTest, InputModesChangeWhenCursorMoves) {
  // The expectation of this test is the same as MS-IME's

  table_->Initialize();

  // i -> "い"
  table_->AddRule("i", "\xe3\x81\x84", "");
  // gi -> "ぎ"
  table_->AddRule("gi", "\xe3\x81\x8e", "");

  InsertKey("i", composer_.get());
  composer_->MoveCursorRight();
  {
    string result;
    composer_->GetStringForPreedit(&result);
    // "い"
    EXPECT_EQ("\xe3\x81\x84", result);
  }

  composer_->MoveCursorLeft();
  {
    string result;
    composer_->GetStringForPreedit(&result);
    // "い"
    EXPECT_EQ("\xe3\x81\x84", result);
  }

  InsertKey("G", composer_.get());
  {
    string result;
    composer_->GetStringForPreedit(&result);
    // "Gい"
    EXPECT_EQ("G\xe3\x81\x84", result);
  }

  composer_->MoveCursorRight();
  {
    string result;
    composer_->GetStringForPreedit(&result);
    // "Gい"
    EXPECT_EQ("G\xe3\x81\x84", result);
  }

  InsertKey("G", composer_.get());
  {
    string result;
    composer_->GetStringForPreedit(&result);
    // "GいG"
    EXPECT_EQ("G\xe3\x81\x84G", result);
  }

  composer_->MoveCursorLeft();
  InsertKey("i", composer_.get());
  {
    string result;
    composer_->GetStringForPreedit(&result);
    // "GいいG"
    EXPECT_EQ("G\xe3\x81\x84\xe3\x81\x84G", result);
  }

  composer_->MoveCursorRight();
  InsertKey("i", composer_.get());
  {
    string result;
    composer_->GetStringForPreedit(&result);
    // "GいいGi"
    EXPECT_EQ("G\xe3\x81\x84\xe3\x81\x84Gi", result);
  }

  InsertKey("G", composer_.get());
  InsertKey("i", composer_.get());
  {
    string result;
    composer_->GetStringForPreedit(&result);
    // "GいいGiGi"
    EXPECT_EQ("G\xe3\x81\x84\xe3\x81\x84GiGi", result);
  }

  composer_->Backspace();
  composer_->Backspace();
  composer_->Backspace();
  InsertKey("i", composer_.get());
  {
    string result;
    composer_->GetStringForPreedit(&result);
    // "GいいGi"
    EXPECT_EQ("G\xe3\x81\x84\xe3\x81\x84Gi", result);
  }

  InsertKey("G", composer_.get());
  InsertKey("G", composer_.get());
  composer_->MoveCursorRight();
  InsertKey("i", composer_.get());
  {
    string result;
    composer_->GetStringForPreedit(&result);
    // "GいいGiGGi"
    EXPECT_EQ("G\xe3\x81\x84\xe3\x81\x84GiGGi", result);
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
    string result;
    composer_->GetStringForPreedit(&result);
    // "Gい"
    EXPECT_EQ("G\xe3\x81\x84", result);
  }

  composer_->Backspace();
  composer_->MoveCursorLeft();
  composer_->MoveCursorRight();
  InsertKey("i", composer_.get());
  {
    string result;
    composer_->GetStringForPreedit(&result);
    // "Gi"
    EXPECT_EQ("Gi", result);
  }
}

TEST_F(ComposerTest, ShuoldCommit) {
  table_->AddRuleWithAttributes("ka", "[KA]", "", DIRECT_INPUT);
  table_->AddRuleWithAttributes("tt", "[X]", "t", DIRECT_INPUT);
  table_->AddRuleWithAttributes("ta", "[TA]", "", NO_TABLE_ATTRIBUTE);

  // k
  composer_->InsertCharacter("k");
  EXPECT_FALSE(composer_->ShouldCommit());

  // k + a
  composer_->InsertCharacter("a");
  EXPECT_TRUE(composer_->ShouldCommit());

  // ka + t
  composer_->InsertCharacter("t");
  EXPECT_FALSE(composer_->ShouldCommit());

  // kat + t
  composer_->InsertCharacter("t");
  EXPECT_FALSE(composer_->ShouldCommit());

  // katt + a
  composer_->InsertCharacter("a");
  EXPECT_TRUE(composer_->ShouldCommit());

  // katta + t
  composer_->InsertCharacter("t");
  EXPECT_FALSE(composer_->ShouldCommit());

  // kattat + a
  composer_->InsertCharacter("a");
  EXPECT_FALSE(composer_->ShouldCommit());
}

TEST_F(ComposerTest, ShouldCommitHead) {
  struct TestData {
    const string input_text;
    const commands::SessionCommand::InputFieldType field_type;
    const bool expected_return;
    const size_t expected_commit_length;
    TestData(const string &input_text,
             commands::SessionCommand::InputFieldType field_type,
             bool expected_return,
             size_t expected_commit_length)
        : input_text(input_text),
          field_type(field_type),
          expected_return(expected_return),
          expected_commit_length(expected_commit_length) {
    }
  };
  const TestData test_data_list[] = {
      // On NORMAL, never commit the head.
      TestData("", commands::SessionCommand::NORMAL, false, 0),
      TestData("A", commands::SessionCommand::NORMAL, false, 0),
      TestData("AB", commands::SessionCommand::NORMAL, false, 0),
      TestData("", commands::SessionCommand::PASSWORD, false, 0),
      // On PASSWORD, commit (length - 1) characters.
      TestData("A", commands::SessionCommand::PASSWORD, false, 0),
      TestData("AB", commands::SessionCommand::PASSWORD, true, 1),
      TestData("ABCDEFGHI", commands::SessionCommand::PASSWORD, true, 8),
      // On NUMBER and TEL, commit (length) characters.
      TestData("", commands::SessionCommand::NUMBER, false, 0),
      TestData("A", commands::SessionCommand::NUMBER, true, 1),
      TestData("AB", commands::SessionCommand::NUMBER, true, 2),
      TestData("ABCDEFGHI", commands::SessionCommand::NUMBER, true, 9),
      TestData("", commands::SessionCommand::TEL, false, 0),
      TestData("A", commands::SessionCommand::TEL, true, 1),
      TestData("AB", commands::SessionCommand::TEL, true, 2),
      TestData("ABCDEFGHI", commands::SessionCommand::TEL, true, 9),
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_data_list); ++i) {
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
  // "も"
  table_->AddRule("mo", "\xE3\x82\x82", "");
  // "ず"
  table_->AddRule("zu", "\xE3\x81\x9a", "");

  composer_->InsertCharacter("z");
  // "ｚ"
  EXPECT_EQ("\xef\xbd\x9a", GetPreedit(composer_.get()));
  EXPECT_EQ(1, composer_->GetCursor());
  composer_->DeleteAt(0);
  EXPECT_EQ("", GetPreedit(composer_.get()));
  EXPECT_EQ(0, composer_->GetCursor());

  composer_->InsertCharacter("mmoz");
  // "ｍもｚ"
  EXPECT_EQ("\xef\xbd\x8d\xe3\x82\x82\xef\xbd\x9a",
            GetPreedit(composer_.get()));
  EXPECT_EQ(3, composer_->GetCursor());
  composer_->DeleteAt(0);
  // "もｚ"
  EXPECT_EQ("\xe3\x82\x82\xef\xbd\x9a", GetPreedit(composer_.get()));
  EXPECT_EQ(2, composer_->GetCursor());
  composer_->InsertCharacter("u");
  // "もず"
  EXPECT_EQ("\xe3\x82\x82\xe3\x81\x9a", GetPreedit(composer_.get()));
  EXPECT_EQ(2, composer_->GetCursor());

  composer_->InsertCharacter("m");
  // "もずｍ"
  EXPECT_EQ("\xe3\x82\x82\xe3\x81\x9a\xef\xbd\x8d",
            GetPreedit(composer_.get()));
  EXPECT_EQ(3, composer_->GetCursor());
  composer_->DeleteAt(1);
  // "もｍ"
  EXPECT_EQ("\xe3\x82\x82\xef\xbd\x8d", GetPreedit(composer_.get()));
  EXPECT_EQ(2, composer_->GetCursor());
  composer_->InsertCharacter("o");
  // "もも"
  EXPECT_EQ("\xe3\x82\x82\xe3\x82\x82", GetPreedit(composer_.get()));
  EXPECT_EQ(2, composer_->GetCursor());
}

TEST_F(ComposerTest, DeleteRange) {
  // "も"
  table_->AddRule("mo", "\xE3\x82\x82", "");
  // "ず"
  table_->AddRule("zu", "\xE3\x81\x9a", "");

  composer_->InsertCharacter("z");
  // "ｚ"
  EXPECT_EQ("\xef\xbd\x9a", GetPreedit(composer_.get()));
  EXPECT_EQ(1, composer_->GetCursor());

  composer_->DeleteRange(0, 1);
  EXPECT_EQ("", GetPreedit(composer_.get()));
  EXPECT_EQ(0, composer_->GetCursor());

  composer_->InsertCharacter("mmozmoz");
  // "ｍもｚもｚ"
  EXPECT_EQ("\xef\xbd\x8d\xe3\x82\x82\xef\xbd\x9a\xe3\x82\x82\xef\xbd\x9a",
            GetPreedit(composer_.get()));
  EXPECT_EQ(5, composer_->GetCursor());

  composer_->DeleteRange(0, 3);
  // "もｚ"
  EXPECT_EQ("\xe3\x82\x82\xef\xbd\x9a", GetPreedit(composer_.get()));
  EXPECT_EQ(2, composer_->GetCursor());

  composer_->InsertCharacter("u");
  // "もず"
  EXPECT_EQ("\xe3\x82\x82\xe3\x81\x9a", GetPreedit(composer_.get()));
  EXPECT_EQ(2, composer_->GetCursor());

  composer_->InsertCharacter("xyz");
  composer_->MoveCursorToBeginning();
  composer_->InsertCharacter("mom");
  // "もｍ|もずｘｙｚ"
  EXPECT_EQ("\xe3\x82\x82\xef\xbd\x8d\xe3\x82\x82\xe3\x81\x9a"
            "\xef\xbd\x98\xef\xbd\x99\xef\xbd\x9a",
            GetPreedit(composer_.get()));
  EXPECT_EQ(2, composer_->GetCursor());

  composer_->DeleteRange(2, 3);
  // "もｍ|ｙｚ"
  EXPECT_EQ("\xe3\x82\x82\xef\xbd\x8d\xef\xbd\x99\xef\xbd\x9a",
            GetPreedit(composer_.get()));
  EXPECT_EQ(2, composer_->GetCursor());

  composer_->InsertCharacter("o");
  // "もも|ｙｚ"
  EXPECT_EQ("\xe3\x82\x82\xe3\x82\x82\xef\xbd\x99\xef\xbd\x9a",
            GetPreedit(composer_.get()));
  EXPECT_EQ(2, composer_->GetCursor());
}

TEST_F(ComposerTest, 12KeysAsciiGetQueryForPrediction) {
  // http://b/5509480
  commands::Request request;
  request.set_zero_query_suggestion(true);
  request.set_mixed_conversion(true);
  request.set_combine_all_segments(true);
  request.set_special_romanji_table(
      commands::Request::TWELVE_KEYS_TO_HALFWIDTHASCII);
  composer_->SetRequest(request);
  table_->InitializeWithRequestAndConfig(request,
                                         config::ConfigHandler::GetConfig());
  composer_->InsertCharacter("2");
  EXPECT_EQ("a", GetPreedit(composer_.get()));
  string result;
  composer_->GetQueryForConversion(&result);
  EXPECT_EQ("a", result);
  result.clear();
  composer_->GetQueryForPrediction(&result);
  EXPECT_EQ("a", result);
}

}  // namespace composer
}  // namespace mozc
