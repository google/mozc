// Copyright 2010-2013, Google Inc.
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

#include "languages/pinyin/session.h"

#include <PyZy/InputContext.h>
#include <string>

#include "base/scoped_ptr.h"
#include "base/system_util.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "languages/pinyin/pinyin_context_mock.h"
#include "languages/pinyin/session_config.h"
#include "languages/pinyin/session_converter.h"
#include "session/commands.pb.h"
#include "session/key_parser.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace pinyin {
namespace {

// Wrapper function. It takes only half width ASCII characters.
string ToFullWidthAscii(const string &half_width) {
  string full_width;
  Util::HalfWidthAsciiToFullWidthAscii(half_width, &full_width);
  return full_width;
}

// This code is based on session_conveter_test::CheckConversion.
void CheckConversion(const commands::Command &command,
                     const string &selected_text,
                     const string &conversion_text,
                     const string &rest_text) {
  const commands::Output &output = command.output();

  // There is no preedit.
  if (selected_text.empty() && conversion_text.empty() && rest_text.empty()) {
    EXPECT_FALSE(output.has_preedit());
    return;
  }

  ASSERT_TRUE(output.has_preedit());

  const commands::Preedit &conversion = output.preedit();
  const size_t segments_size = (selected_text.empty() ? 0 : 1)
      + (conversion_text.empty() ? 0 : 1)
      + (rest_text.empty() ? 0 : 1);
  ASSERT_EQ(segments_size, conversion.segment_size());

  size_t index = 0;

  if (!selected_text.empty()) {
    EXPECT_EQ(selected_text,
              conversion.segment(index).value());
    EXPECT_EQ(commands::Preedit::Segment::UNDERLINE,
              conversion.segment(index).annotation());
    EXPECT_EQ(Util::CharsLen(selected_text),
              conversion.segment(index).value_length());
    ++index;
  }

  if (!conversion_text.empty()) {
    EXPECT_EQ(conversion_text,
              conversion.segment(index).value());
    EXPECT_EQ(commands::Preedit::Segment::HIGHLIGHT,
              conversion.segment(index).annotation());
    EXPECT_EQ(Util::CharsLen(conversion_text),
              conversion.segment(index).value_length());
    EXPECT_EQ(Util::CharsLen(selected_text), conversion.highlighted_position());
    ++index;
  }

  if (!rest_text.empty()) {
    EXPECT_EQ(rest_text,
              conversion.segment(index).value());
    EXPECT_EQ(commands::Preedit::Segment::UNDERLINE,
              conversion.segment(index).annotation());
    EXPECT_EQ(Util::CharsLen(rest_text),
              conversion.segment(index).value_length());
    ++index;
  }

  EXPECT_EQ(Util::CharsLen(selected_text), conversion.cursor());
}

void CheckCandidates(const commands::Command &command,
                     const string &base_text, size_t focused_index) {
  // This is simple check.
  // Deep comparison test is written on session_converter_test.cc.

  const commands::Output &output = command.output();

  // There are no candidates.
  if (base_text.empty()) {
    ASSERT_FALSE(output.has_candidates());
    return;
  }

  ASSERT_TRUE(output.has_candidates());
  ASSERT_TRUE(output.has_preedit());

  // Converts "abc" to ("ＡＢＣ", "ＡＢ", "Ａ").

  const commands::Candidates &candidates = output.candidates();
  // Okay to check only the candidates.size is 0 or not, because this value
  // does not represent actual one due to performance issue
  if (base_text.empty()) {
    EXPECT_EQ(0, candidates.size());
  } else {
    EXPECT_NE(0, candidates.size());
  }
  EXPECT_EQ(focused_index, candidates.focused_index());

  string focused_text = base_text.substr(0, base_text.size() - focused_index);
  Util::UpperString(&focused_text);
  focused_text.assign(ToFullWidthAscii(focused_text));
  const size_t kCandidatesPerPage = 5;
  const size_t focused_index_in_current_page =
      focused_index % kCandidatesPerPage;
  EXPECT_EQ(focused_text,
            candidates.candidate(focused_index_in_current_page).value());
}

// This code is based on session_conveter_test::CheckResult.
void CheckResult(const commands::Command &command, const string &result_text) {
  const commands::Output &output = command.output();

  if (result_text.empty()) {
    EXPECT_FALSE(output.has_result());
    return;
  }

  ASSERT_TRUE(output.has_result());
  EXPECT_EQ(result_text, output.result().value());
}
}  // namespace

class PinyinSessionTest : public testing::Test {
 protected:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);

    PyZy::InputContext::init(FLAGS_test_tmpdir, FLAGS_test_tmpdir);

    ResetSession();
  }

  virtual void TearDown() {
    PyZy::InputContext::finalize();

    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

  bool SendKey(const string &key,
               commands::Command *command) {
    command->Clear();
    command->mutable_input()->set_type(commands::Input::SEND_KEY);
    if (!KeyParser::ParseKey(key, command->mutable_input()->mutable_key())) {
      return false;
    }
    return session_->SendKey(command);
  }

  bool SendCommand(const commands::SessionCommand &session_command,
                   commands::Command *command) {
    command->Clear();
    command->mutable_input()->set_type(commands::Input::SEND_COMMAND);
    command->mutable_input()->mutable_command()->CopyFrom(session_command);
    return session_->SendCommand(command);
  }

  bool InsertCharacterChars(const string &chars, commands::Command *command) {
    const uint32 kNoModifiers = 0;
    for (int i = 0; i < chars.size(); ++i) {
      command->Clear();
      commands::KeyEvent *key_event = command->mutable_input()->mutable_key();
      key_event->set_key_code(chars[i]);
      key_event->set_modifiers(kNoModifiers);
      if (!session_->SendKey(command)) {
        return false;
      }
    }
    return true;
  }

  void ResetSession() {
    session_.reset(new pinyin::Session);

    SessionConverter *converter =
        new SessionConverter(*session_->session_config_);
    PinyinContextMock *mock = new PinyinContextMock;
    converter->pinyin_context_.reset(mock);
    converter->context_ = mock;

    session_->converter_.reset(converter);

    config::ConfigHandler::Reload();
  }

  void ResetContext() {
    session_->ResetContext();
  }

  ConversionMode GetConversionMode() const {
    return session_->conversion_mode_;
  }

  ConversionMode GetConversionModeWithSessionInstance(
      const Session &session) const {
    return session.conversion_mode_;
  }

  const SessionConfig *GetSessionConfigWithSessionInstance(
      const Session &session) const {
    return session.session_config_.get();
  }

#ifdef OS_CHROMEOS
  void SessionUpdateConfig(const config::PinyinConfig &pinyin_config) {
    session_->UpdateConfig(pinyin_config);
  }
#endif  // OS_CHROMEOS

  scoped_ptr<Session> session_;
};

class SessionConfigTest : public PinyinSessionTest,
                          public testing::WithParamInterface<
  tr1::tuple<bool, bool, bool> > {
 protected:
  void SetUp() {
    PinyinSessionTest::SetUp();

    const tr1::tuple<bool, bool, bool> &param = GetParam();
    full_width_word_mode_ = tr1::get<0>(param);
    full_width_punctuation_mode_ = tr1::get<1>(param);
    simplified_chinese_mode_ = tr1::get<2>(param);
  }

  bool full_width_word_mode_;
  bool full_width_punctuation_mode_;
  bool simplified_chinese_mode_;
};

TEST_P(SessionConfigTest, Constructor) {
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  config::PinyinConfig *pinyin_config = config.mutable_pinyin_config();

  pinyin_config->set_initial_mode_full_width_word(
      full_width_word_mode_);
  pinyin_config->set_initial_mode_full_width_punctuation(
      full_width_punctuation_mode_);
  pinyin_config->set_initial_mode_simplified_chinese(
      simplified_chinese_mode_);

  config::ConfigHandler::SetConfig(config);

  Session session;
  const SessionConfig *session_config =
      GetSessionConfigWithSessionInstance(session);

  EXPECT_EQ(full_width_word_mode_,
            session_config->full_width_word_mode);
  EXPECT_EQ(full_width_punctuation_mode_,
            session_config->full_width_punctuation_mode);
  EXPECT_EQ(simplified_chinese_mode_,
            session_config->simplified_chinese_mode);
}

INSTANTIATE_TEST_CASE_P(SessionConfigTest, SessionConfigTest, testing::Combine(
    testing::Bool(), testing::Bool(), testing::Bool()));

TEST_F(PinyinSessionTest, Insert) {
  commands::Command command;

  {
    SCOPED_TRACE("Insert CAPS A (Converter is NOT active)");
    command.Clear();
    SendKey("CAPS A", &command);
    CheckConversion(command, "", "", "");
    CheckCandidates(command, "", 0);
    CheckResult(command, "");
  }

  {
    SCOPED_TRACE("Insert CAPS a");
    command.Clear();
    SendKey("CAPS a", &command);
    CheckConversion(command, "", ToFullWidthAscii("A"), "");
    CheckCandidates(command, "A", 0);
    CheckResult(command, "");
  }

  {
    SCOPED_TRACE("Insert a");
    command.Clear();
    SendKey("a", &command);
    CheckConversion(command, "", ToFullWidthAscii("AA"), "");
    CheckCandidates(command, "AA", 0);
    CheckResult(command, "");
  }

  {
    SCOPED_TRACE("Insert CAPS A (Converter is active)");
    command.Clear();
    SendKey("CAPS A", &command);
    CheckConversion(command, "", ToFullWidthAscii("AA"), "");
    CheckCandidates(command, "AA", 0);
    CheckResult(command, "");
  }
}

TEST_F(PinyinSessionTest, ResetContext) {
  commands::Command command;

  InsertCharacterChars("abc", &command);
  {
    SCOPED_TRACE("Resets context.");
    commands::SessionCommand session_command;
    session_command.set_type(commands::SessionCommand::RESET_CONTEXT);
    SendCommand(session_command, &command);
    CheckConversion(command, "", "", "");
    CheckCandidates(command, "", 0);
    CheckResult(command, "");
  }

  InsertCharacterChars("abc", &command);
  {
    SCOPED_TRACE("Reverts context.");
    commands::SessionCommand session_command;
    session_command.set_type(commands::SessionCommand::REVERT);
    SendCommand(session_command, &command);
    CheckConversion(command, "", "", "");
    CheckCandidates(command, "", 0);
    CheckResult(command, "");
  }
}

TEST_F(PinyinSessionTest, Result) {
  commands::Command command;
  InsertCharacterChars("abc", &command);

  {
    SCOPED_TRACE("Commit");
    command.Clear();
    SendKey("Enter", &command);
    CheckConversion(command, "", "", "");
    CheckCandidates(command, "", 0);
    CheckResult(command, "abc");
  }

  {
    SCOPED_TRACE("Insert characters and check that result is cleared.");
    command.Clear();
    InsertCharacterChars("def", &command);
    CheckConversion(command, "", ToFullWidthAscii("DEF"), "");
    CheckCandidates(command, "def", 0);
    CheckResult(command, "");
  }
}

TEST_F(PinyinSessionTest, SelectCandidate) {
  commands::Command command;

  {
    SCOPED_TRACE("Selects first candidate by space key and commit.");
    command.Clear();
    InsertCharacterChars("abc", &command);

    command.Clear();
    SendKey("SPACE", &command);
    CheckConversion(command, "", "", "");
    CheckCandidates(command, "", 0);
    CheckResult(command, ToFullWidthAscii("ABC"));
  }

  ResetContext();
  command.Clear();
  InsertCharacterChars("abcd", &command);

  {
    SCOPED_TRACE("Selects a 4th candidate by number.");
    command.Clear();
    SendKey("4", &command);
    CheckConversion(
        command, ToFullWidthAscii("A"), ToFullWidthAscii("BCD"), "");
    CheckCandidates(command, "BCD", 0);
    CheckResult(command, "");
  }

  {
    SCOPED_TRACE("Selects a 3rd candidate by numpad.");
    command.Clear();
    SendKey("NUMPAD3", &command);
    CheckConversion(
        command, ToFullWidthAscii("AB"), ToFullWidthAscii("CD"), "");
    CheckCandidates(command, "CD", 0);
    CheckResult(command, "");
  }

  {
    SCOPED_TRACE("Selects a 2nd candidate with SessionCommand");
    command.Clear();
    commands::SessionCommand session_command;
    session_command.set_type(commands::SessionCommand::SELECT_CANDIDATE);
    session_command.set_id(1);
    SendCommand(session_command, &command);
    CheckConversion(
        command, ToFullWidthAscii("ABC"), ToFullWidthAscii("D"), "");
    CheckCandidates(command, "D", 0);
    CheckResult(command, "");
  }
}

TEST_F(PinyinSessionTest, Commit) {
  commands::Command command;
  InsertCharacterChars("abc", &command);

  {
    SCOPED_TRACE("Selects a second candidate.");
    command.Clear();
    SendKey("2", &command);
    CheckConversion(command, ToFullWidthAscii("AB"), ToFullWidthAscii("C"), "");
    CheckCandidates(command, "C", 0);
    CheckResult(command, "");
  }

  {
    SCOPED_TRACE("Commits.");
    command.Clear();
    SendKey("ENTER", &command);
    CheckConversion(command, "", "", "");
    CheckCandidates(command, "", 0);
    CheckResult(command, ToFullWidthAscii("AB") + "c");
  }

  ResetContext();
  command.Clear();
  InsertCharacterChars("abc", &command);

  {
    SCOPED_TRACE("Commits with Numpad");
    command.Clear();
    SendKey("SEPARATOR", &command);
    CheckConversion(command, "", "", "");
    CheckCandidates(command, "", 0);
    CheckResult(command, "abc");
  }

  ResetContext();
  command.Clear();
  InsertCharacterChars("abc", &command);

  {
    SCOPED_TRACE("Commits with SessionCommand");
    command.Clear();
    commands::SessionCommand session_command;
    session_command.set_type(commands::SessionCommand::SUBMIT);
    SendCommand(session_command, &command);
    CheckConversion(command, "", "", "");
    CheckCandidates(command, "", 0);
    CheckResult(command, "abc");
  }
}

TEST_F(PinyinSessionTest, FocusCandidate) {
  commands::Command command;
  InsertCharacterChars("abcdef", &command);

  {
    SCOPED_TRACE("Focuses candidate next");
    command.Clear();
    SendKey("DOWN", &command);
    CheckConversion(command, "", ToFullWidthAscii("ABCDE"), "f");
    CheckCandidates(command, "ABCDEF", 1);
    CheckResult(command, "");
  }

  {
    SCOPED_TRACE("Focuses candidate prev");
    command.Clear();
    SendKey("UP", &command);
    CheckConversion(command, "", ToFullWidthAscii("ABCDEF"), "");
    CheckCandidates(command, "ABCDEF", 0);
    CheckResult(command, "");
  }

  {
    SCOPED_TRACE("Focuses candidate next page");
    command.Clear();
    SendKey("PAGEDOWN", &command);
    CheckConversion(command, "", ToFullWidthAscii("A"), "bcdef");
    CheckCandidates(command, "ABCDEF", 5);
    CheckResult(command, "");
  }

  {
    SCOPED_TRACE("Focuses candidate prev page");
    command.Clear();
    SendKey("PAGEUP", &command);
    CheckConversion(command, "", ToFullWidthAscii("ABCDEF"), "");
    CheckCandidates(command, "ABCDEF", 0);
    CheckResult(command, "");
  }

  {
    SCOPED_TRACE("Focuses candidate next");
    command.Clear();
    SendKey("DOWN", &command);
    CheckConversion(command, "", ToFullWidthAscii("ABCDE"), "f");
    CheckCandidates(command, "ABCDEF", 1);
    CheckResult(command, "");
  }

  {
    SCOPED_TRACE("Commits focused candidate.");
    command.Clear();
    SendKey("SPACE", &command);
    CheckConversion(
        command, ToFullWidthAscii("ABCDE"), ToFullWidthAscii("F"), "");
    CheckCandidates(command, "F", 0);
    CheckResult(command, "");
  }
}

TEST_F(PinyinSessionTest, MoveCursor) {
  commands::Command command;
  InsertCharacterChars("abcdefghijkl", &command);

  {
    SCOPED_TRACE("Moves cursor left. abcdefghijk|l");
    command.Clear();
    SendKey("LEFT", &command);
    CheckConversion(command, "", "abcdefghijk", "l");
    CheckCandidates(command, "ABCDEFGHIJK", 0);
    CheckResult(command, "");
  }

  {
    SCOPED_TRACE("Moves cursor right. abcdefghijkl|");
    command.Clear();
    SendKey("RIGHT", &command);
    CheckConversion(command, "", ToFullWidthAscii("ABCDEFGHIJKL"), "");
    CheckCandidates(command, "ABCDEFGHIJKL", 0);
    CheckResult(command, "");
  }

  {
    SCOPED_TRACE("Moves cursor to beginning. |abcdefghijkl");
    command.Clear();
    SendKey("HOME", &command);
    CheckConversion(command, "", "", "abcdefghijkl");
    CheckCandidates(command, "", 0);
    CheckResult(command, "");
  }

  {
    SCOPED_TRACE("Moves cursor right by word. abc|defghijkl");
    command.Clear();
    SendKey("CTRL RIGHT", &command);
    CheckConversion(command, "", "abc", "defghijkl");
    CheckCandidates(command, "ABC", 0);
    CheckResult(command, "");
  }

  {
    SCOPED_TRACE("Moves cursor left by word. |abcdefghijkl");
    command.Clear();
    SendKey("CTRL LEFT", &command);
    CheckConversion(command, "", "", "abcdefghijkl");
    CheckCandidates(command, "", 0);
    CheckResult(command, "");
  }

  {
    SCOPED_TRACE("Moves cursor to end. abcdefghijkl|");
    command.Clear();
    SendKey("END", &command);
    CheckConversion(command, "", ToFullWidthAscii("ABCDEFGHIJKL"), "");
    CheckCandidates(command, "ABCDEFGHIJKL", 0);
    CheckResult(command, "");
  }
}

TEST_F(PinyinSessionTest, ClearCandidateFromHistory) {
  commands::Command command;
  InsertCharacterChars("abc", &command);

  {
    SCOPED_TRACE("Clear 1st candidate");
    SendKey("CTRL 1", &command);
    CheckConversion(command, "", ToFullWidthAscii("AB"), "c");
    CheckCandidates(command, "AB", 0);
    CheckResult(command, "");
  }
}

TEST_F(PinyinSessionTest, BackSpaceAndDelete) {
  commands::Command command;
  InsertCharacterChars("abcdefghijkl", &command);

  {
    SCOPED_TRACE("BackSpace");
    command.Clear();
    SendKey("BACKSPACE", &command);
    CheckConversion(command, "", ToFullWidthAscii("ABCDEFGHIJK"), "");
    CheckCandidates(command, "ABCDEFGHIJK", 0);
    CheckResult(command, "");
  }

  {
    SCOPED_TRACE("Ctrl BackSpace");
    command.Clear();
    SendKey("CTRL BACKSPACE", &command);
    CheckConversion(command, "", ToFullWidthAscii("ABCDEFGHI"), "");
    CheckCandidates(command, "ABCDEFGHI", 0);
    CheckResult(command, "");
  }

  command.Clear();
  SendKey("HOME", &command);

  {
    SCOPED_TRACE("Delete");
    command.Clear();
    SendKey("DEL", &command);
    CheckConversion(command, "", "", "bcdefghi");
    CheckCandidates(command, "", 0);
    CheckResult(command, "");
  }

  {
    SCOPED_TRACE("Ctrl Delete");
    command.Clear();
    SendKey("CTRL DEL", &command);
    CheckConversion(command, "", "", "efghi");
    CheckCandidates(command, "", 0);
    CheckResult(command, "");
  }
}

#ifdef OS_CHROMEOS
TEST_F(PinyinSessionTest, UpdateConfig) {
  config::Config config;
  config::ConfigHandler::GetConfig(&config);
  const bool original_value = config.pinyin_config().correct_pinyin();

  config.mutable_pinyin_config()->set_correct_pinyin(!original_value);
  Session::UpdateConfig(config.pinyin_config());

  config::ConfigHandler::GetConfig(&config);
  EXPECT_NE(original_value, config.pinyin_config().correct_pinyin());
}
#endif  // OS_CHROMEOS

TEST_F(PinyinSessionTest, SelectWithShift) {
  config::Config config;
  config::ConfigHandler::GetConfig(&config);
  config.mutable_pinyin_config()->set_select_with_shift(true);
  config::ConfigHandler::SetConfig(config);

  commands::Command command;
  InsertCharacterChars("abc", &command);

  {
    SCOPED_TRACE("Selects third candidate with right shift");
    command.Clear();
    SendKey("RightShift", &command);
    CheckConversion(command,
                    ToFullWidthAscii("A"), ToFullWidthAscii("BC"), "");
    CheckCandidates(command, "bc", 0);
    CheckResult(command, "");
  }

  {
    SCOPED_TRACE("Selects second candidate with left shift");
    command.Clear();
    SendKey("LeftShift", &command);
    CheckConversion(command,
                    ToFullWidthAscii("AB"), ToFullWidthAscii("C"), "");
    CheckCandidates(command, "c", 0);
    CheckResult(command, "");
  }

  {
    SCOPED_TRACE("Selects non-exisntent candidate with left shift and fail");
    command.Clear();
    SendKey("LeftShift", &command);
    CheckConversion(command,
                    ToFullWidthAscii("AB"), ToFullWidthAscii("C"), "");
    CheckCandidates(command, "c", 0);
    CheckResult(command, "");
  }

  ResetSession();
  command.Clear();
  InsertCharacterChars("abcdefg", &command);
  command.Clear();
  SendKey("PageDown", &command);

  {
    SCOPED_TRACE("Selects second candidate with left shift on 2nd page");
    command.Clear();
    SendKey("LeftShift", &command);
    CheckConversion(command,
                    ToFullWidthAscii("A"), ToFullWidthAscii("BCDEFG"), "");
    CheckCandidates(command, "bcdefg", 0);
    CheckResult(command, "");
  }
}

TEST_F(PinyinSessionTest, AutoCommit) {
  config::Config config;
  config::ConfigHandler::GetConfig(&config);
  config.mutable_pinyin_config()->set_auto_commit(true);
  config::ConfigHandler::SetConfig(config);

  commands::Command command;
  {
    SCOPED_TRACE("Inserts abc! and does auto commit");
    command.Clear();
    InsertCharacterChars("abc", &command);

    command.Clear();
    SendKey("!", &command);
    EXPECT_TRUE(command.output().consumed());

    CheckConversion(command, "", "", "");
    CheckCandidates(command, "", 0);
    CheckResult(command, ToFullWidthAscii("ABC!"));
  }

  ResetSession();

  {
    SCOPED_TRACE("Inserts abc, moves cursor, and does auto commit");
    command.Clear();
    InsertCharacterChars("abc", &command);
    SendKey("LEFT", &command);

    command.Clear();
    SendKey("!", &command);
    EXPECT_TRUE(command.output().consumed());

    CheckConversion(command, "", "", "");
    CheckCandidates(command, "", 0);
    CheckResult(command, ToFullWidthAscii("AB") + "c" + ToFullWidthAscii("!"));
  }
}

TEST_F(PinyinSessionTest, SwitchConversionMode) {
  // Real pinyin context will be used in this test because context is reset by
  // switching conversion mode.

  commands::Command command;

  {
    SCOPED_TRACE("English mode");
    ASSERT_EQ(PINYIN, GetConversionMode());

    command.Clear();
    InsertCharacterChars("vt", &command);
    EXPECT_EQ(ENGLISH, GetConversionMode());

    command.Clear();
    SendKey("Enter", &command);
    EXPECT_EQ(PINYIN, GetConversionMode());
    CheckConversion(command, "", "", "");
    CheckCandidates(command, "", 0);
    CheckResult(command, "t");

    // Makes converter active.
    command.Clear();
    SendKey("n", &command);
    EXPECT_EQ(PINYIN, GetConversionMode());

    // Doesn't turn on English mode because converter is active.
    command.Clear();
    SendKey("v", &command);
    EXPECT_EQ(PINYIN, GetConversionMode());
  }

  ResetSession();

  {
    SCOPED_TRACE("Direct mode");
    ASSERT_EQ(PINYIN, GetConversionMode());

    command.Clear();
    SendKey("Shift", &command);
    EXPECT_EQ(DIRECT, GetConversionMode());

    SendKey("a", &command);
    CheckConversion(command, "", "", "");
    CheckCandidates(command, "", 0);
    CheckResult(command, "a");
    EXPECT_EQ(DIRECT, GetConversionMode());

    command.Clear();
    SendKey("Shift", &command);
    EXPECT_EQ(PINYIN, GetConversionMode());

    command.Clear();
    SendKey("LeftShift", &command);
    EXPECT_EQ(DIRECT, GetConversionMode());

    command.Clear();
    SendKey("RightShift", &command);
    EXPECT_EQ(PINYIN, GetConversionMode());
  }

  ResetSession();

  {
    SCOPED_TRACE("Punctuation mode");
    ASSERT_EQ(PINYIN, GetConversionMode());

    command.Clear();
    SendKey("`", &command);
    EXPECT_EQ(PUNCTUATION, GetConversionMode());

    command.Clear();
    SendKey("Enter", &command);
    EXPECT_EQ(PINYIN, GetConversionMode());

    // Makes converter active.
    command.Clear();
    SendKey("n", &command);
    EXPECT_EQ(PINYIN, GetConversionMode());

    // Doesn't turn on Punctuation mode because converter is active.
    command.Clear();
    SendKey("`", &command);
    EXPECT_EQ(PINYIN, GetConversionMode());
  }

  ResetSession();

  {
    SCOPED_TRACE("English mode to Pinyin mode with SessionCommand");
    ASSERT_EQ(PINYIN, GetConversionMode());

    command.Clear();
    InsertCharacterChars("vt", &command);
    EXPECT_EQ(ENGLISH, GetConversionMode());

    command.Clear();
    command.mutable_input()->mutable_command()->set_type(
        commands::SessionCommand::SUBMIT);
    session_->SendCommand(&command);
    EXPECT_EQ(PINYIN, GetConversionMode());
    CheckConversion(command, "", "", "");
    CheckCandidates(command, "", 0);
    CheckResult(command, "t");
  }

  {
    SCOPED_TRACE("Punctuation mode to Pinyin mode with SessionCommand");
    ASSERT_EQ(PINYIN, GetConversionMode());

    command.Clear();
    InsertCharacterChars("`", &command);
    EXPECT_EQ(PUNCTUATION, GetConversionMode());

    command.Clear();
    command.mutable_input()->mutable_command()->set_type(
        commands::SessionCommand::SUBMIT);
    session_->SendCommand(&command);
    EXPECT_EQ(PINYIN, GetConversionMode());
    CheckConversion(command, "", "", "");
    CheckCandidates(command, "", 0);
    CheckResult(command, "\xC2\xB7");  // "·"
  }
}

TEST_F(PinyinSessionTest, InitialConversionMode) {
  // Real pinyin context will be used in this test.

  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  config.mutable_pinyin_config()->set_initial_mode_chinese(true);
  config::ConfigHandler::SetConfig(config);

  {
    Session session;
    EXPECT_EQ(PINYIN, GetConversionModeWithSessionInstance(session));
  }

  config.mutable_pinyin_config()->set_initial_mode_chinese(false);
  config::ConfigHandler::SetConfig(config);

  {
    Session session;
    EXPECT_EQ(DIRECT, GetConversionModeWithSessionInstance(session));
  }
}

TEST_F(PinyinSessionTest, HandleNumpadOnPunctuationMode_Issue6055961) {
  commands::Command command;

  SendKey("`", &command);
  ASSERT_EQ(PUNCTUATION, GetConversionMode());

  command.Clear();
  SendKey("MULTIPLY", &command);

  ASSERT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
}

TEST_F(PinyinSessionTest, ToggleSimplifiedChineseMode) {
  const SessionConfig *session_config =
      GetSessionConfigWithSessionInstance(*session_);
  ASSERT_TRUE(session_config->simplified_chinese_mode);

  commands::Command command;
  SendKey("Ctrl Shift f", &command);
  EXPECT_FALSE(session_config->simplified_chinese_mode);

  command.Clear();
  SendKey("Ctrl Shift F", &command);
  EXPECT_TRUE(session_config->simplified_chinese_mode);
}

TEST_F(PinyinSessionTest, SetSessionRequest) {
  const SessionConfig *session_config =
      GetSessionConfigWithSessionInstance(*session_);
  commands::Command command;
  commands::SessionCommand session_command;
  session_command.set_type(commands::SessionCommand::SEND_LANGUAGE_BAR_COMMAND);

  // Chinese mode
  ASSERT_EQ(PINYIN, GetConversionMode());
  session_command.set_language_bar_command_id(
      commands::SessionCommand::TOGGLE_PINYIN_CHINESE_MODE);
  command.Clear();
  EXPECT_TRUE(SendCommand(session_command, &command));
  EXPECT_EQ(DIRECT, GetConversionMode());
  command.Clear();
  EXPECT_TRUE(SendCommand(session_command, &command));
  EXPECT_EQ(PINYIN, GetConversionMode());

  // Full width word mode
  ASSERT_FALSE(session_config->full_width_word_mode);
  session_command.set_language_bar_command_id(
      commands::SessionCommand::TOGGLE_PINYIN_FULL_WIDTH_WORD_MODE);
  command.Clear();
  EXPECT_TRUE(SendCommand(session_command, &command));
  EXPECT_TRUE(session_config->full_width_word_mode);

  // Full width punctuation mode
      GetSessionConfigWithSessionInstance(*session_);
  ASSERT_TRUE(session_config->full_width_punctuation_mode);
  session_command.set_language_bar_command_id(
      commands::SessionCommand::TOGGLE_PINYIN_FULL_WIDTH_PUNCTUATION_MODE);
  command.Clear();
  EXPECT_TRUE(SendCommand(session_command, &command));
  EXPECT_FALSE(session_config->full_width_punctuation_mode);

  // Simplified chinese mode
      GetSessionConfigWithSessionInstance(*session_);
  ASSERT_TRUE(session_config->simplified_chinese_mode);
  session_command.set_language_bar_command_id(
      commands::SessionCommand::TOGGLE_PINYIN_SIMPLIFIED_CHINESE_MODE);
  command.Clear();
  EXPECT_TRUE(SendCommand(session_command, &command));
  EXPECT_FALSE(session_config->simplified_chinese_mode);
}

// TODO(hsumita): Implements this test.
// TEST_F(PinyinSessionTest, ResetConfig) {
// }

}  // namespace pinyin
}  // namespace mozc
