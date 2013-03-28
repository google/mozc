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

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "languages/hangul/session.h"
#include "session/key_event_util.h"
#include "session/key_parser.h"
#include "testing/base/public/gunit.h"

using mozc::config::HangulConfig;

namespace mozc {
namespace hangul {
// TODO(nona): Make test enable on general Linux.
// As the reason why following tests cannot run on Linux, UpdateConfig is only
// for ChromeOS and there are no way to update configure on Linux. Is it better
// way to unify with Linux updating configuration using SessionHandler?
#ifdef OS_CHROMEOS
namespace {
bool SendKey(const string &key,
             session::SessionInterface *session,
             commands::Command *command) {
  command->Clear();
  command->mutable_input()->set_type(commands::Input::SEND_KEY);
  if (!KeyParser::ParseKey(key, command->mutable_input()->mutable_key())) {
    return false;
  }
  return session->SendKey(command);
}

bool SendSpecialKey(const commands::KeyEvent::SpecialKey special_key,
                    session::SessionInterface *session,
                    commands::Command *command) {
  command->Clear();
  command->mutable_input()->mutable_key()->set_special_key(special_key);
  return session->SendKey(command);
}

bool SendCommand(const commands::SessionCommand &session_command,
                 session::SessionInterface *session,
                 commands::Command *command) {
  command->Clear();
  command->mutable_input()->set_type(commands::Input::SEND_COMMAND);
  command->mutable_input()->mutable_command()->CopyFrom(session_command);
  return session->SendCommand(command);
}

string GetNthCandidate(const commands::Command &command, int n) {
  if (!command.has_output()) {
    return "";
  }

  if (!command.output().has_candidates()) {
    return "";
  }

  if (command.output().candidates().candidate_size() <= n) {
    return "";
  }

  return command.output().candidates().candidate(n).value();
}

size_t GetCandidateCount(const commands::Command &command) {
  if (!command.has_output()) {
    return 0;
  }

  if (!command.output().has_candidates()) {
    return 0;
  }

  return command.output().candidates().candidate_size();
}

bool ExpectPreedit(const string &preedit, const int length,
                   commands::Command &command) {
  EXPECT_TRUE(command.has_output());
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  const commands::Preedit_Segment &segment
      = command.output().preedit().segment(0);
  EXPECT_EQ(preedit, segment.value());
  EXPECT_EQ(length, segment.value_length());
  return true;
}

bool ExpectResult(const string &result, commands::Command &command) {
  EXPECT_TRUE(command.has_output());
  EXPECT_TRUE(command.output().has_result());
  EXPECT_TRUE(command.output().result().has_value());
  EXPECT_EQ(result, command.output().result().value());
  return true;
}
}  // namespace

class HangulSessionTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    session_ = new hangul::Session();
    config::ConfigHandler::SetConfigFileName("memory://hangul_config.1.db");
    config::ConfigHandler::Reload();
  }

  void SetUpKeyboard(config::HangulConfig_KeyboardTypes keyboard) {
    ResetSession();
    config::HangulConfig hangul_config;
    hangul_config.set_keyboard_type(keyboard);
    SessionUpdateConfig(hangul_config);
    SessionResetConfig();
  }

  bool SetUpCompositionMode(commands::CompositionMode mode,
                            commands::Command *command) {
    commands::SessionCommand session_command;
    session_command.set_type(commands::SessionCommand::SWITCH_INPUT_MODE);
    session_command.set_composition_mode(mode);
    return SendCommand(session_command, session_, command);
  }

  bool SessionSendSessionCommand(
      commands::SessionCommand_CommandType command_type,
      commands::Command *command) {
    commands::SessionCommand session_command;
    session_command.set_type(command_type);
    return SendCommand(session_command, session_, command);
  }

  virtual void TearDown() {
    delete session_;
  }

  void ResetSession() {
    delete session_;
    session_ = new hangul::Session();
    config::ConfigHandler::Reload();
  }

  bool SessionHasReproduciblePreedit() {
    return session_->HasReproduciblePreedit();
  }

  bool SessionIsHanjaSelectionMode() {
    return session_->IsHanjaSelectionMode();
  }

  Session::InputMode SessionGetCurrentMode() {
    return session_->current_mode_;
  }

  const deque<char32> &SessionGetHanjaLockPreedit() {
    return session_->hanja_lock_preedit_;
  }

  const set<KeyInformation> &SessionGetHanjaKeySet() {
    return session_->hanja_key_set_;
  }

  const HanjaList *SessionGetHanjaList() {
    return session_->hanja_list_;
  }

  void SessionRenewContext() {
    session_->RenewContext();
  }

  void SessionResetConfig() {
    session_->ResetConfig();
  }

  void SessionUpdateConfig(const HangulConfig &hangul_config) {
    session_->UpdateConfig(hangul_config);
  }

  void SessionCancelContext(commands::Output *output) {
    session_->CancelContext(output);
  }

  bool ReloadSymbolDictionary(const string& file_name) {
    return session_->ReloadSymbolDictionary(file_name);
  }

  Session *session_;
};

TEST_F(HangulSessionTest, SebeolsikSenarioTest) {
  commands::Command command;
  SetUpKeyboard(config::HangulConfig::KEYBOARD_Sebeolsik390);

  // Input "대한민국" by "rogksalsrnr"
  EXPECT_TRUE(SendKey("u", session_, &command));  // key of "ㄷ"
  ExpectPreedit("\xE3\x84\xB7", 1, command);  // "ㄷ"
  EXPECT_TRUE(SendKey("r", session_, &command));  // key of "ㅐ"
  ExpectPreedit("\xEB\x8C\x80", 1, command);  // "대"
  EXPECT_TRUE(SendKey("m", session_, &command));  // key of "ㅎ"
  ExpectPreedit("\xE3\x85\x8E", 1, command);  // "ㅎ"
  ExpectResult("\xEB\x8C\x80", command);  // "대"
  EXPECT_TRUE(SendKey("f", session_, &command));  // key of "ㅏ"
  ExpectPreedit("\xED\x95\x98", 1, command);  // "하"
  EXPECT_TRUE(SendKey("s", session_, &command));  // key of "ㄴ"
  ExpectPreedit("\xED\x95\x9C", 1, command);  // "한"
  EXPECT_TRUE(SendKey("i", session_, &command));  // key of "ㅁ"
  ExpectPreedit("\xE3\x85\x81", 1, command);  // "ㅁ"
  ExpectResult("\xED\x95\x9C", command);  // "한"
  EXPECT_TRUE(SendKey("d", session_, &command));  // key of "ㅣ"
  ExpectPreedit("\xEB\xAF\xB8", 1, command);  // "미"
  EXPECT_TRUE(SendKey("s", session_, &command));  // key of "ㄴ"
  ExpectPreedit("\xEB\xAF\xBC", 1, command);  // "민"
  EXPECT_TRUE(SendKey("k", session_, &command));  // key of "ㄱ"
  ExpectPreedit("\xE3\x84\xB1", 1, command);  // "ㄱ"
  ExpectResult("\xEB\xAF\xBC", command);  // "민"
  EXPECT_TRUE(SendKey("b", session_, &command));  // key of "ㅜ"
  ExpectPreedit("\xEA\xB5\xAC", 1, command);  // "구"
  EXPECT_TRUE(SendKey("x", session_, &command));  // key of "ㄱ"
  ExpectPreedit("\xEA\xB5\xAD", 1, command);  // "국"
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::ENTER, session_, &command));
  ExpectResult("\xEA\xB5\xAD", command);  // "국"
}

TEST_F(HangulSessionTest, BackspaceSenarioTest) {
  commands::Command command;
  SetUpKeyboard(config::HangulConfig::KEYBOARD_Dubeolsik);

  // Input "대한민국" by "rogksalsrnr"
  EXPECT_TRUE(SendKey("e", session_, &command));  // key of "ㄷ"
  ExpectPreedit("\xE3\x84\xB7", 1, command);  // "ㄷ"
  EXPECT_TRUE(SendKey("o", session_, &command));  // key of "ㅐ"
  ExpectPreedit("\xEB\x8C\x80", 1, command);  // "대"
  EXPECT_TRUE(SendKey("g", session_, &command));  // key of "ㅎ"
  ExpectPreedit("\xEB\x8C\x9B", 1, command);  // "댛"

  // Reproducible backspace.
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::BACKSPACE,
                             session_, &command));
  ExpectPreedit("\xEB\x8C\x80", 1, command);  // "대"
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::BACKSPACE,
                             session_, &command));
  ExpectPreedit("\xE3\x84\xB7", 1, command);  // "ㄷ"

  EXPECT_TRUE(SendKey("o", session_, &command));  // key of "ㅐ"
  EXPECT_TRUE(SendKey("g", session_, &command));  // key of "ㅎ"
  ExpectPreedit("\xEB\x8C\x9B", 1, command);  // "댛"

  EXPECT_TRUE(SendKey("k", session_, &command));  // key of "ㅏ"
  ExpectPreedit("\xED\x95\x98", 1, command);  // "하"
  ExpectResult("\xEB\x8C\x80", command);  // "대"

  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::BACKSPACE,
                             session_, &command));
  ExpectPreedit("\xE3\x85\x8E", 1, command);  // "ㅎ"
  EXPECT_TRUE(SendKey("k", session_, &command));  // key of "ㅏ"
  ExpectPreedit("\xED\x95\x98", 1, command);  // "하"

  EXPECT_TRUE(SendKey("s", session_, &command));  // key of "ㄴ"
  ExpectPreedit("\xED\x95\x9C", 1, command);  // "한"
  EXPECT_TRUE(SendKey("a", session_, &command));  // key of "ㅁ"
  ExpectPreedit("\xE3\x85\x81", 1, command);  // "ㅁ"
  ExpectResult("\xED\x95\x9C", command);  // "한"
  EXPECT_TRUE(SendKey("l", session_, &command));  // key of "ㅣ"
  ExpectPreedit("\xEB\xAF\xB8", 1, command);  // "미"
  EXPECT_TRUE(SendKey("s", session_, &command));  // key of "ㄴ"
  ExpectPreedit("\xEB\xAF\xBC", 1, command);  // "민"
  EXPECT_TRUE(SendKey("r", session_, &command));  // key of "ㄱ"
  ExpectPreedit("\xE3\x84\xB1", 1, command);  // "ㄱ"
  ExpectResult("\xEB\xAF\xBC", command);  // "민"
  EXPECT_TRUE(SendKey("n", session_, &command));  // key of "ㅜ"
  ExpectPreedit("\xEA\xB5\xAC", 1, command);  // "구"
  EXPECT_TRUE(SendKey("r", session_, &command));  // key of "ㄱ"
  ExpectPreedit("\xEA\xB5\xAD", 1, command);  // "국"
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::ENTER, session_, &command));
  ExpectResult("\xEA\xB5\xAD", command);  // "국"

  // Backspace senario with hanja conversion.
  // Following steps are issued as crosbug.com/18419
  EXPECT_TRUE(SendKey("d", session_, &command));
  EXPECT_TRUE(SendKey("k", session_, &command));
  EXPECT_TRUE(SendKey("s", session_, &command));
  ExpectPreedit("\xEC\x95\x88", 1, command);  // "안"
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::HANJA, session_, &command));
  EXPECT_EQ("\xE5\xAE\x89", GetNthCandidate(command, 0));  // "安"
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::BACKSPACE,
                             session_, &command));
  ExpectPreedit("\xEC\x95\x88", 1, command);  // "안"
  EXPECT_EQ("\xE5\xAE\x89", GetNthCandidate(command, 0));  // "安"

  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::ENTER, session_, &command));
  ExpectResult("\xE5\xAE\x89", command);  // "安"

  EXPECT_TRUE(SendKey("e", session_, &command));
  ExpectPreedit("\xE3\x84\xB7", 1, command);  // "ㄷ"
  EXPECT_TRUE(SendKey("o", session_, &command));
  ExpectPreedit("\xEB\x8C\x80", 1, command);  // "대"
}

TEST_F(HangulSessionTest, DubeolsikSenarioTest) {
  commands::Command command;
  SetUpKeyboard(config::HangulConfig::KEYBOARD_Dubeolsik);

  // Input "대한민국" by "rogksalsrnr"
  EXPECT_TRUE(SendKey("e", session_, &command));  // key of "ㄷ"
  ExpectPreedit("\xE3\x84\xB7", 1, command);  // "ㄷ"
  EXPECT_TRUE(SendKey("o", session_, &command));  // key of "ㅐ"
  ExpectPreedit("\xEB\x8C\x80", 1, command);  // "대"
  EXPECT_TRUE(SendKey("g", session_, &command));  // key of "ㅎ"
  ExpectPreedit("\xEB\x8C\x9B", 1, command);  // "댛"
  EXPECT_TRUE(SendKey("k", session_, &command));  // key of "ㅏ"
  ExpectPreedit("\xED\x95\x98", 1, command);  // "하"
  ExpectResult("\xEB\x8C\x80", command);  // "대"
  EXPECT_TRUE(SendKey("s", session_, &command));  // key of "ㄴ"
  ExpectPreedit("\xED\x95\x9C", 1, command);  // "한"
  EXPECT_TRUE(SendKey("a", session_, &command));  // key of "ㅁ"
  ExpectPreedit("\xE3\x85\x81", 1, command);  // "ㅁ"
  ExpectResult("\xED\x95\x9C", command);  // "한"
  EXPECT_TRUE(SendKey("l", session_, &command));  // key of "ㅣ"
  ExpectPreedit("\xEB\xAF\xB8", 1, command);  // "미"
  EXPECT_TRUE(SendKey("s", session_, &command));  // key of "ㄴ"
  ExpectPreedit("\xEB\xAF\xBC", 1, command);  // "민"
  EXPECT_TRUE(SendKey("r", session_, &command));  // key of "ㄱ"
  ExpectPreedit("\xE3\x84\xB1", 1, command);  // "ㄱ"
  ExpectResult("\xEB\xAF\xBC", command);  // "민"
  EXPECT_TRUE(SendKey("n", session_, &command));  // key of "ㅜ"
  ExpectPreedit("\xEA\xB5\xAC", 1, command);  // "구"
  EXPECT_TRUE(SendKey("r", session_, &command));  // key of "ㄱ"
  ExpectPreedit("\xEA\xB5\xAD", 1, command);  // "국"
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::ENTER, session_, &command));
  ExpectResult("\xEA\xB5\xAD", command);  // "국"
}

TEST_F(HangulSessionTest, CandidateTest) {
  commands::Command command;
  SetUpKeyboard(config::HangulConfig::KEYBOARD_Dubeolsik);

  // Candidates for "다" by "ek"
  const char *kCandidates[] = {
    "\xE5\xA4\x9A",  // "多"
    "\xE8\x8C\xB6",  // "茶"
    "\xE7\x88\xB9",  // "爹"
    "\xE5\x97\xB2",  // "嗲"
    "\xE5\xA4\x9B",  // "夛"
    "\xE8\x8C\xA4",  // "茤"
    "\xE8\xA7\xB0",  // "觰"
    "\xE8\xB7\xA2",  // "跢"
    "\xE9\xAF\xBA",  // "鯺"
    "\xE4\xAB\x82",  // "䫂"
    "\xE4\xAF\xAC"  // "䯬"
  };

  EXPECT_TRUE(SendKey("e", session_, &command));
  EXPECT_TRUE(SendKey("k", session_, &command));
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::HANJA, session_, &command));
  ExpectPreedit("\xEB\x8B\xA4", 1, command);  // "다"

  const int kCandidatesPerPage = 10;  // TODO(nona): load from ibus

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kCandidates); ++i) {
    EXPECT_EQ(kCandidates[i], GetNthCandidate(command, i % kCandidatesPerPage));
    EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::RIGHT, session_, &command));
  }

  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::ENTER, session_, &command));
  ExpectResult(kCandidates[0], command);
  EXPECT_FALSE(SessionIsHanjaSelectionMode());
  EXPECT_FALSE(SessionHasReproduciblePreedit());

  for (size_t i = 0; i < kCandidatesPerPage; ++i) {
    EXPECT_TRUE(SendKey("e", session_, &command));
    EXPECT_TRUE(SendKey("k", session_, &command));
    EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::HANJA, session_, &command));
    ExpectPreedit("\xEB\x8B\xA4", 1, command);  // "다"

    string indicate_key = Util::StringPrintf("%d", (i + 1) % 10);
    EXPECT_TRUE(SendKey(indicate_key, session_, &command));
    ExpectResult(kCandidates[i], command);
    EXPECT_FALSE(SessionIsHanjaSelectionMode());
    EXPECT_FALSE(SessionHasReproduciblePreedit());
  }
}

TEST_F(HangulSessionTest, ToggleModeTest) {
  commands::Command command;
  ResetSession();

  commands::SessionCommand session_command;
  session_command.set_type(commands::SessionCommand::SWITCH_INPUT_MODE);

  session_command.set_composition_mode(commands::HIRAGANA);
  EXPECT_TRUE(SendCommand(session_command, session_, &command));
  EXPECT_EQ(Session::kHangulMode, SessionGetCurrentMode());
  EXPECT_TRUE(SessionGetHanjaLockPreedit().empty());

  session_command.set_composition_mode(commands::FULL_ASCII);
  EXPECT_TRUE(SendCommand(session_command, session_, &command));
  EXPECT_EQ(Session::kHanjaLockMode, SessionGetCurrentMode());
  EXPECT_TRUE(SessionGetHanjaLockPreedit().empty());
}

TEST_F(HangulSessionTest, IgnoreCapsLockState) {
  commands::Command command;
  SetUpKeyboard(config::HangulConfig::KEYBOARD_Dubeolsik);

  // Input with Caps lock being off.
  EXPECT_TRUE(SendKey("q", session_, &command));  // key of "ㅂ"
  ExpectPreedit("\xE3\x85\x82", 1, command);  // "ㅂ"
  EXPECT_TRUE(SendKey("Q", session_, &command));  // key of "ㅃ"
  ExpectPreedit("\xE3\x85\x83", 1, command);  // "ㅃ"
  SessionCancelContext(command.mutable_output());
  EXPECT_TRUE(SendKey("1", session_, &command));  // key of "1"
  ExpectResult("1", command);  // "1"
  EXPECT_TRUE(SendKey("!", session_, &command));  // key of "!"
  ExpectResult("!", command);  // "!"

  // Input with Caps lock being on.
  EXPECT_TRUE(SendKey("caps Q", session_, &command));
  ExpectPreedit("\xE3\x85\x82", 1, command);  // "ㅂ"
  EXPECT_TRUE(SendKey("caps q", session_, &command));
  ExpectPreedit("\xE3\x85\x83", 1, command);  // "ㅃ"
  SessionCancelContext(command.mutable_output());
  EXPECT_TRUE(SendKey("caps 1", session_, &command));
  ExpectResult("1", command);  // "1"
  EXPECT_TRUE(SendKey("caps !", session_, &command));
  ExpectResult("!", command);  // "!"

  // With Sebeolsik keyboard.
  SetUpKeyboard(config::HangulConfig::KEYBOARD_Sebeolsik390);

  // Input with Caps lock being off.
  EXPECT_TRUE(SendKey("q", session_, &command));  // key of "ㅅ"
  ExpectPreedit("\xE3\x85\x85", 1, command);  // "ㅅ"
  EXPECT_TRUE(SendKey("Q", session_, &command));  // key of "ㅍ"
  ExpectPreedit("\xE3\x85\x8D", 1, command);  // "ㅍ"
  EXPECT_TRUE(SendKey("1", session_, &command));  // key of "ㅎ"
  ExpectPreedit("\xE3\x85\x8E", 1, command);  // "ㅎ"
  EXPECT_TRUE(SendKey("!", session_, &command));  // key of "ㅈ"
  ExpectPreedit("\xE3\x85\x88", 1, command);  // "ㅈ"

  // Input with Caps lock being on.
  EXPECT_TRUE(SendKey("caps Q", session_, &command));
  ExpectPreedit("\xE3\x85\x85", 1, command);  // "ㅅ"
  EXPECT_TRUE(SendKey("caps q", session_, &command));
  ExpectPreedit("\xE3\x85\x8D", 1, command);  // "ㅍ"
  EXPECT_TRUE(SendKey("caps 1", session_, &command));
  ExpectPreedit("\xE3\x85\x8E", 1, command);  // "ㅎ"
  EXPECT_TRUE(SendKey("caps !", session_, &command));
  ExpectPreedit("\xE3\x85\x88", 1, command);  // "ㅈ"
}

TEST_F(HangulSessionTest, RenewContext) {
  commands::Command command;
  ResetSession();

  EXPECT_TRUE(SendKey("e", session_, &command));
  EXPECT_TRUE(SendKey("k", session_, &command));
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::HANJA, session_, &command));

  config::HangulConfig hangul_config;
  hangul_config.set_keyboard_type(config::HangulConfig::KEYBOARD_Sebeolsik390);

  SessionUpdateConfig(hangul_config);
  SessionRenewContext();

  // RenewContext expects discarding current candidates.
  EXPECT_EQ(NULL, SessionGetHanjaList());
  // RenewContext expects remaining keyboard_type and input mode.
  EXPECT_EQ(config::HangulConfig::KEYBOARD_Sebeolsik390,
            GET_CONFIG(hangul_config).keyboard_type());
  EXPECT_EQ(Session::kHangulMode, SessionGetCurrentMode());
}

TEST_F(HangulSessionTest, UpdateConfig) {
  config::HangulConfig hangul_config;
  hangul_config.set_keyboard_type(config::HangulConfig::KEYBOARD_Sebeolsik390);

  SessionUpdateConfig(hangul_config);

  EXPECT_EQ(config::HangulConfig::KEYBOARD_Sebeolsik390,
            GET_CONFIG(hangul_config).keyboard_type());
}

TEST_F(HangulSessionTest, CursorPosition) {
  commands::Command command;
  ResetSession();

  EXPECT_TRUE(SendKey("e", session_, &command));
  EXPECT_EQ(1, command.output().preedit().cursor());
  EXPECT_TRUE(SendKey("k", session_, &command));
  EXPECT_EQ(1, command.output().preedit().cursor());
}

TEST_F(HangulSessionTest, HanjaLockSenarioTest) {
  commands::Command command;
  SetUpKeyboard(config::HangulConfig::KEYBOARD_Dubeolsik);

  // Set input mode as "Hanja-Lock".
  commands::SessionCommand session_command;
  session_command.set_type(commands::SessionCommand::SWITCH_INPUT_MODE);
  session_command.set_composition_mode(commands::FULL_ASCII);
  EXPECT_TRUE(SendCommand(session_command, session_, &command));
  EXPECT_EQ(Session::kHanjaLockMode, SessionGetCurrentMode());
  EXPECT_TRUE(SessionGetHanjaLockPreedit().empty());

  // To control candidate numbers, we do not use additional symbols.
  EXPECT_FALSE(ReloadSymbolDictionary("/tmp/invalid_filename_"));

  EXPECT_TRUE(SendKey("t", session_, &command));
  // "ㅅ"
  ExpectPreedit("\xE3\x85\x85", 1, command);
  EXPECT_EQ(0, GetCandidateCount(command));

  EXPECT_TRUE(SendKey("h", session_, &command));
  // "소"
  ExpectPreedit("\xEC\x86\x8C", 1, command);
  // "小"
  EXPECT_EQ("\xE5\xB0\x8F", GetNthCandidate(command, 0));

  EXPECT_TRUE(SendKey("w", session_, &command));
  // "솢"
  ExpectPreedit("\xEC\x86\xA2", 1, command);
  EXPECT_EQ(0, GetCandidateCount(command));

  EXPECT_TRUE(SendKey("n", session_, &command));
  // "소주"
  ExpectPreedit("\xEC\x86\x8C\xEC\xA3\xBC", 2, command);
  // "燒酒"
  EXPECT_EQ("\xE7\x87\x92\xE9\x85\x92", GetNthCandidate(command, 0));

  EXPECT_TRUE(SendKey("s", session_, &command));
  // "소준"
  ExpectPreedit("\xEC\x86\x8C\xEC\xA4\x80", 2, command);
  // "燒準"
  EXPECT_EQ("\xE7\x87\x92\xE6\xBA\x96", GetNthCandidate(command, 0));

  EXPECT_TRUE(SendKey("h", session_, &command));
  // "소주노"
  ExpectPreedit("\xEC\x86\x8C\xEC\xA3\xBC\xEB\x85\xB8", 3, command);
  // "燒酒"
  EXPECT_EQ("\xE7\x87\x92\xE9\x85\x92", GetNthCandidate(command, 0));

  EXPECT_TRUE(SendKey("r", session_, &command));
  // "소주녹"
  ExpectPreedit("\xEC\x86\x8C\xEC\xA3\xBC\xEB\x85\xB9", 3, command);
  // "燒酒"
  EXPECT_EQ("\xE7\x87\x92\xE9\x85\x92", GetNthCandidate(command, 0));

  EXPECT_TRUE(SendKey("c", session_, &command));
  // "소주녹ㅊ"
  ExpectPreedit("\xEC\x86\x8C\xEC\xA3\xBC\xEB\x85\xB9\xE3\x85\x8A", 4, command);
  // "燒酒"
  EXPECT_EQ("\xE7\x87\x92\xE9\x85\x92", GetNthCandidate(command, 0));

  EXPECT_TRUE(SendKey("k", session_, &command));
  // "소주녹차"
  ExpectPreedit("\xEC\x86\x8C\xEC\xA3\xBC\xEB\x85\xB9\xEC\xB0\xA8", 4, command);
  // "燒酒"
  EXPECT_EQ("\xE7\x87\x92\xE9\x85\x92", GetNthCandidate(command, 0));

  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::ENTER, session_, &command));
  // "燒酒"
  ExpectResult("\xE7\x87\x92\xE9\x85\x92", command);
  // "녹차"
  ExpectPreedit("\xEB\x85\xB9\xEC\xB0\xA8", 2, command);
  // "綠茶"
  EXPECT_EQ("\xE7\xB6\xA0\xE8\x8C\xB6", GetNthCandidate(command, 0));
  EXPECT_TRUE(SessionIsHanjaSelectionMode());
  EXPECT_TRUE(SessionHasReproduciblePreedit());

  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::ENTER, session_, &command));
  // "綠茶"
  ExpectResult("\xE7\xB6\xA0\xE8\x8C\xB6", command);
  EXPECT_EQ(0, GetCandidateCount(command));
  EXPECT_FALSE(SessionIsHanjaSelectionMode());
  EXPECT_FALSE(SessionHasReproduciblePreedit());
}

TEST_F(HangulSessionTest, HanjaLockBackspaceSenarioTest) {
  commands::Command command;
  SetUpKeyboard(config::HangulConfig::KEYBOARD_Dubeolsik);

  // Set input mode as "Hanja-Lock".
  commands::SessionCommand session_command;
  session_command.set_type(commands::SessionCommand::SWITCH_INPUT_MODE);
  session_command.set_composition_mode(commands::FULL_ASCII);
  EXPECT_TRUE(SendCommand(session_command, session_, &command));
  EXPECT_EQ(Session::kHanjaLockMode, SessionGetCurrentMode());
  EXPECT_TRUE(SessionGetHanjaLockPreedit().empty());

  // To control candidate numbers, we do not use additional symbols.
  EXPECT_FALSE(ReloadSymbolDictionary("/tmp/invalid_filename_"));

  EXPECT_TRUE(SendKey("t", session_, &command));
  // "ㅅ"
  ExpectPreedit("\xE3\x85\x85", 1, command);
  EXPECT_EQ(0, GetCandidateCount(command));

  EXPECT_TRUE(SendKey("h", session_, &command));
  // "소"
  ExpectPreedit("\xEC\x86\x8C", 1, command);
  // "小"
  EXPECT_EQ("\xE5\xB0\x8F", GetNthCandidate(command, 0));

  EXPECT_TRUE(SendKey("w", session_, &command));
  // "솢"
  ExpectPreedit("\xEC\x86\xA2", 1, command);
  EXPECT_EQ(0, GetCandidateCount(command));

  EXPECT_TRUE(SendKey("n", session_, &command));
  // "소주"
  ExpectPreedit("\xEC\x86\x8C\xEC\xA3\xBC", 2, command);
  // "燒酒"
  EXPECT_EQ("\xE7\x87\x92\xE9\x85\x92", GetNthCandidate(command, 0));

  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::BACKSPACE,
                             session_, &command));
  // "소ㅈ"
  ExpectPreedit("\xEC\x86\x8C\xE3\x85\x88", 2, command);
  // "小"
  EXPECT_EQ("\xE5\xB0\x8F", GetNthCandidate(command, 0));
  EXPECT_TRUE(SessionIsHanjaSelectionMode());
  EXPECT_TRUE(SessionHasReproduciblePreedit());

  // Following expectation comes from cloning with ibus-hangul.
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::BACKSPACE,
                             session_, &command));
  // "소"
  ExpectResult("\xEC\x86\x8C", command);
  EXPECT_FALSE(SessionIsHanjaSelectionMode());
  EXPECT_FALSE(SessionHasReproduciblePreedit());
}

TEST_F(HangulSessionTest, HanjaCandidateToggleTest) {
  commands::Command command;
  SetUpKeyboard(config::HangulConfig::KEYBOARD_Dubeolsik);

  EXPECT_TRUE(SendKey("e", session_, &command));
  EXPECT_TRUE(SendKey("k", session_, &command));

  // Hanja conversion start.
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::HANJA, session_, &command));
  ExpectPreedit("\xEB\x8B\xA4", 1, command);  // "다"
  EXPECT_EQ("\xE5\xA4\x9A", GetNthCandidate(command, 0));

  // Cancel conversion, yet still remain preedit.
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::HANJA, session_, &command));
  ExpectPreedit("\xEB\x8B\xA4", 1, command);  // "다"
  EXPECT_EQ(0, GetCandidateCount(command));

  // Again start hanja conversion.
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::HANJA, session_, &command));
  ExpectPreedit("\xEB\x8B\xA4", 1, command);  // "다"
  EXPECT_EQ("\xE5\xA4\x9A", GetNthCandidate(command, 0));

  // Escape key can also cancel conversion.
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::ESCAPE, session_, &command));
  ExpectPreedit("\xEB\x8B\xA4", 1, command);  // "다"
  EXPECT_EQ(0, GetCandidateCount(command));

  // Again start hanja conversion.
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::HANJA, session_, &command));
  ExpectPreedit("\xEB\x8B\xA4", 1, command);  // "다"
  EXPECT_EQ("\xE5\xA4\x9A", GetNthCandidate(command, 0));

  // If escape key press twice, commit preedit to result.
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::ESCAPE, session_, &command));
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::ESCAPE, session_, &command));
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_EQ(0, GetCandidateCount(command));
}

TEST_F(HangulSessionTest, SelectCandidateCommandTest) {
  commands::Command command;
  commands::SessionCommand session_command;
  SetUpKeyboard(config::HangulConfig::KEYBOARD_Dubeolsik);

  // Set input mode as "Hangul".
  EXPECT_TRUE(SetUpCompositionMode(commands::HIRAGANA, &command));

  // Candidates for "다" by "ek"
  const char *kCandidates[] = {
    "\xE5\xA4\x9A",  // "多"
    "\xE8\x8C\xB6",  // "茶"
    "\xE7\x88\xB9",  // "爹"
    "\xE5\x97\xB2",  // "嗲"
    "\xE5\xA4\x9B",  // "夛"
    "\xE8\x8C\xA4",  // "茤"
    "\xE8\xA7\xB0",  // "觰"
    "\xE8\xB7\xA2",  // "跢"
    "\xE9\xAF\xBA",  // "鯺"
    "\xE4\xAB\x82",  // "䫂"
    "\xE4\xAF\xAC"  // "䯬"
  };

  const int kCandidatesPerPage = 10;  // TODO(nona): load from ibus

  for (size_t i = 0; i < kCandidatesPerPage; ++i) {
    EXPECT_TRUE(SendKey("e", session_, &command));
    EXPECT_TRUE(SendKey("k", session_, &command));
    EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::HANJA, session_, &command));

    session_command.Clear();
    session_command.set_type(commands::SessionCommand::SELECT_CANDIDATE);
    session_command.set_id(i);
    EXPECT_TRUE(SendCommand(session_command, session_, &command));

    ExpectResult(kCandidates[i], command);
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_EQ(0, GetCandidateCount(command));
  }
}

TEST_F(HangulSessionTest, ShowCommentTest) {
  commands::Command command;
  SetUpKeyboard(config::HangulConfig::KEYBOARD_Dubeolsik);

  // Set input mode as "Hangul".
  EXPECT_TRUE(SetUpCompositionMode(commands::HIRAGANA, &command));

  EXPECT_TRUE(SendKey("t", session_, &command));
  EXPECT_TRUE(SendKey("h", session_, &command));
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::HANJA, session_, &command));

  EXPECT_TRUE(command.output().candidates().has_footer());
  // "작을 소, 좁을 소, 적을 소, 어릴 소, 적게 여길 소"
  EXPECT_EQ("\xEC\x9E\x91\xEC\x9D\x84\x20\xEC\x86\x8C\x2C\x20\xEC\xA2\x81\xEC"
            "\x9D\x84\x20\xEC\x86\x8C\x2C\x20\xEC\xA0\x81\xEC\x9D\x84\x20\xEC"
            "\x86\x8C\x2C\x20\xEC\x96\xB4\xEB\xA6\xB4\x20\xEC\x86\x8C\x2C\x20"
            "\xEC\xA0\x81\xEA\xB2\x8C\x20\xEC\x97\xAC\xEA\xB8\xB8\x20\xEC\x86"
            "\x8C", command.output().candidates().footer().label());

  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::RIGHT, session_, &command));

  // "적을 소, 조금 소, 멸시할 소, 잠깐 소, 젊을 소"
  EXPECT_EQ("\xEC\xA0\x81\xEC\x9D\x84\x20\xEC\x86\x8C\x2C\x20\xEC\xA1\xB0\xEA"
            "\xB8\x88\x20\xEC\x86\x8C\x2C\x20\xEB\xA9\xB8\xEC\x8B\x9C\xED\x95"
            "\xA0\x20\xEC\x86\x8C\x2C\x20\xEC\x9E\xA0\xEA\xB9\x90\x20\xEC\x86"
            "\x8C\x2C\x20\xEC\xA0\x8A\xEC\x9D\x84\x20\xEC\x86\x8C",
            command.output().candidates().footer().label());
}

TEST_F(HangulSessionTest, HanjaKeyMapTest) {
  commands::Command command;
  ResetSession();
  config::HangulConfig hangul_config;
  hangul_config.set_keyboard_type(config::HangulConfig::KEYBOARD_Dubeolsik);
  hangul_config.add_hanja_keys("Ctrl 9");
  hangul_config.add_hanja_keys("F9");
  hangul_config.add_hanja_keys("F10");
  SessionUpdateConfig(hangul_config);
  SessionResetConfig();

  KeyInformation key;
  commands::KeyEvent key_event;

  KeyParser::ParseKey("Ctrl 9", &key_event);
  KeyEventUtil::GetKeyInformation(key_event, &key);
  EXPECT_NE(SessionGetHanjaKeySet().end(), SessionGetHanjaKeySet().find(key));

  KeyParser::ParseKey("F9", &key_event);
  KeyEventUtil::GetKeyInformation(key_event, &key);
  EXPECT_NE(SessionGetHanjaKeySet().end(), SessionGetHanjaKeySet().find(key));

  KeyParser::ParseKey("F10", &key_event);
  KeyEventUtil::GetKeyInformation(key_event, &key);
  EXPECT_NE(SessionGetHanjaKeySet().end(), SessionGetHanjaKeySet().find(key));

  KeyParser::ParseKey("F7", &key_event);
  KeyEventUtil::GetKeyInformation(key_event, &key);
  EXPECT_EQ(SessionGetHanjaKeySet().end(), SessionGetHanjaKeySet().find(key));

  EXPECT_TRUE(SendKey("e", session_, &command));
  EXPECT_TRUE(SendKey("k", session_, &command));
  EXPECT_TRUE(SendKey("Ctrl 9", session_, &command));
  ExpectPreedit("\xEB\x8B\xA4", 1, command);  // "다"

  EXPECT_EQ("\xE5\xA4\x9A", GetNthCandidate(command, 0));  // "多"
}

TEST_F(HangulSessionTest, HanjaLockNumberSelectionTest) {
  // This test reproduce bug issued as crosbug/18387.
  commands::Command command;
  SetUpKeyboard(config::HangulConfig::KEYBOARD_Dubeolsik);

  // Set input mode as "Hanja-Lock".
  EXPECT_TRUE(SetUpCompositionMode(commands::FULL_ASCII, &command));

  // Candidates for "소주" by "thwn"
  const char *kCandidates[] = {
    "\xE7\x87\x92\xE9\x85\x92",  // "燒酒"
    "\xE5\xB0\x8F\xE8\x88\x9F",  // "小舟"
    "\xE5\xB0\x8F\xE8\xA8\xBB",  // "小註"
    "\xE5\xB0\x91\xE4\xB8\xBB",  // "少主"
    "\xE8\x98\x87\xE5\xB7\x9E",  // "蘇州"
    "\xE9\x9F\xB6\xE5\xB7\x9E",  // "韶州"
    "\xE5\xB0\x8F\xE6\xA0\xAA",  // "小株"
    "\xE7\x96\x8F\xE6\xB3\xA8",  // "疏注"
    "\xE7\x96\x8F\xE8\xA8\xBB"   // "疏註"
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kCandidates); ++i) {
    EXPECT_TRUE(SendKey("t", session_, &command));
    EXPECT_TRUE(SendKey("h", session_, &command));
    EXPECT_TRUE(SendKey("w", session_, &command));
    EXPECT_TRUE(SendKey("n", session_, &command));
    ExpectPreedit("\xEC\x86\x8C\xEC\xA3\xBC", 2, command);  // "소주"

    string indicate_key = Util::StringPrintf("%d", i + 1);
    EXPECT_TRUE(SendKey(indicate_key, session_, &command));
    ExpectResult(kCandidates[i], command);
    EXPECT_FALSE(SessionIsHanjaSelectionMode());
    EXPECT_FALSE(SessionHasReproduciblePreedit());
  }
}

TEST_F(HangulSessionTest, HangulSpaceKeyTest) {
  // Following steps are reported as crosbug.com/18454.
  // This test reproduce bug issued as crosbug/18387.
  commands::Command command;
  SetUpKeyboard(config::HangulConfig::KEYBOARD_Dubeolsik);

  // Set input mode as "Hangul".
  EXPECT_TRUE(SetUpCompositionMode(commands::HIRAGANA, &command));

  EXPECT_TRUE(SendKey("t", session_, &command));
  EXPECT_TRUE(SendKey("h", session_, &command));
  EXPECT_TRUE(SendKey("w", session_, &command));
  EXPECT_TRUE(SendKey("n", session_, &command));
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::SPACE, session_, &command));

  // "주"
  ExpectResult("\xEC\xA3\xBC", command);
  EXPECT_TRUE(!command.output().consumed());
}

TEST_F(HangulSessionTest, HanjaLockSpaceKeyTest) {
  // Following steps are reported as crosbug.com/18454.
  // This test reproduce bug issued as crosbug/18387.
  commands::Command command;
  SetUpKeyboard(config::HangulConfig::KEYBOARD_Dubeolsik);

  // Set input mode as "Hanja-Lock".
  EXPECT_TRUE(SetUpCompositionMode(commands::FULL_ASCII, &command));

  EXPECT_TRUE(SendKey("d", session_, &command));
  EXPECT_TRUE(SendKey("k", session_, &command));
  EXPECT_TRUE(SendKey("s", session_, &command));
  EXPECT_TRUE(SendKey("s", session_, &command));
  EXPECT_TRUE(SendKey("u", session_, &command));
  EXPECT_TRUE(SendKey("d", session_, &command));
  EXPECT_TRUE(SendKey("d", session_, &command));
  EXPECT_TRUE(SendKey("k", session_, &command));
  EXPECT_TRUE(SendKey("s", session_, &command));

  // "안녕안"
  ExpectPreedit("\xEC\x95\x88\xEB\x85\x95\xEC\x95\x88", 3, command);

  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::SPACE, session_, &command));

  // "안녕안"
  ExpectResult("\xEC\x95\x88\xEB\x85\x95\xEC\x95\x88", command);

  EXPECT_TRUE(!command.output().consumed());
}

TEST_F(HangulSessionTest, PreeditSubmitionWhenCompositionModeChenged) {
  // This test is part of crosbug/18507.
  commands::Command command;
  SetUpKeyboard(config::HangulConfig::KEYBOARD_Dubeolsik);

  // Set input mode as "Hangul".
  EXPECT_TRUE(SetUpCompositionMode(commands::HIRAGANA, &command));
  EXPECT_TRUE(SendKey("t", session_, &command));
  EXPECT_TRUE(SendKey("h", session_, &command));
  ExpectPreedit("\xEC\x86\x8C", 1, command);  // "소"

  // Set input mode as "HanjaLock".
  EXPECT_TRUE(SetUpCompositionMode(commands::FULL_ASCII, &command));
  ExpectResult("\xEC\x86\x8C", command);  // "소"

  EXPECT_TRUE(SendKey("t", session_, &command));
  EXPECT_TRUE(SendKey("h", session_, &command));
  EXPECT_TRUE(SendKey("w", session_, &command));
  EXPECT_TRUE(SendKey("n", session_, &command));
  ExpectPreedit("\xEC\x86\x8C\xEC\xA3\xBC", 2, command);  // "소주"

  // Set input mode as "Hangul".
  EXPECT_TRUE(SetUpCompositionMode(commands::HIRAGANA, &command));
  ExpectResult("\xEC\x86\x8C\xEC\xA3\xBC", command);  // "소주"
}

TEST_F(HangulSessionTest, RevertSubmitSessionTest) {
  commands::Command command;
  SetUpKeyboard(config::HangulConfig::KEYBOARD_Dubeolsik);

  EXPECT_TRUE(SetUpCompositionMode(commands::HIRAGANA, &command));

  EXPECT_TRUE(SendKey("t", session_, &command));
  EXPECT_TRUE(SendKey("h", session_, &command));
  ExpectPreedit("\xEC\x86\x8C", 1, command);  // "소"

  SessionSendSessionCommand(commands::SessionCommand::REVERT, &command);
  EXPECT_TRUE(SessionGetHanjaLockPreedit().empty());

  EXPECT_TRUE(SendKey("t", session_, &command));
  EXPECT_TRUE(SendKey("h", session_, &command));
  ExpectPreedit("\xEC\x86\x8C", 1, command);  // "소"

  SessionSendSessionCommand(commands::SessionCommand::SUBMIT, &command);
  EXPECT_TRUE(SessionGetHanjaLockPreedit().empty());
  ExpectResult("\xEC\x86\x8C", command);  // "소"
}

TEST_F(HangulSessionTest, HanjaLockModeHanjaSelectionDoesNotAcceptModifiedKey) {
  commands::Command command;
  ResetSession();
  config::HangulConfig hangul_config;
  hangul_config.set_keyboard_type(config::HangulConfig::KEYBOARD_Dubeolsik);
  hangul_config.add_hanja_keys("Ctrl 9");
  SessionUpdateConfig(hangul_config);
  SessionResetConfig();

  // Set input mode as "HanjaLock".
  EXPECT_TRUE(SetUpCompositionMode(commands::FULL_ASCII, &command));

  // Following steps are reported as crosbug.com/19074
  EXPECT_TRUE(SendKey("d", session_, &command));
  EXPECT_TRUE(SendKey("k", session_, &command));
  EXPECT_TRUE(SendKey("s", session_, &command));
  ExpectPreedit("\xEC\x95\x88", 1, command);  // "안"

  EXPECT_TRUE(SendKey("Ctrl s", session_, &command));
  ExpectPreedit("\xEC\x95\x88", 1, command);  // "안"

  // Additional to reporting, after release Ctrl key, client send Ctrl key.
  // Even if such case, expected behavior is still keeping preedit string.
  EXPECT_TRUE(SendKey("Ctrl", session_, &command));
  ExpectPreedit("\xEC\x95\x88", 1,  command);  // "안"
}

// This feature request is reported as crosbug.com/15947.
TEST_F(HangulSessionTest, WonKeySenarioTest) {
  commands::Command command;
  ResetSession();
  config::HangulConfig hangul_config;
  hangul_config.set_keyboard_type(config::HangulConfig::KEYBOARD_Dubeolsik);
  hangul_config.add_hanja_keys("Ctrl 9");
  SessionUpdateConfig(hangul_config);
  SessionResetConfig();

  // Set input mode as "Hangul".
  EXPECT_TRUE(SetUpCompositionMode(commands::HIRAGANA, &command));

  // In Hangul mode, the won key commits preedit string and appends won sign
  // into result string.
  EXPECT_TRUE(SendKey("d", session_, &command));
  EXPECT_TRUE(SendKey("k", session_, &command));
  EXPECT_TRUE(SendKey("s", session_, &command));
  ExpectPreedit("\xEC\x95\x88", 1, command);  // "안"
  EXPECT_TRUE(SendKey("\\", session_, &command));
  ExpectResult("\xEC\x95\x88\xE2\x82\xA9", command);  // "한₩"

  // In Hangul mode and look-up table is shown, the won key is just ignored.
  EXPECT_TRUE(SendKey("d", session_, &command));
  EXPECT_TRUE(SendKey("k", session_, &command));
  EXPECT_TRUE(SendKey("s", session_, &command));
  ExpectPreedit("\xEC\x95\x88", 1, command);  // "안"
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::HANJA, session_, &command));
  EXPECT_TRUE(SendKey("\\", session_, &command));
  EXPECT_FALSE(command.output().has_result());

  // Set input mode as "HanjaLock".
  EXPECT_TRUE(SetUpCompositionMode(commands::FULL_ASCII, &command));

  // In HanjaLock mode, the won key committs preedit string and appends won sign
  // into result string.
  EXPECT_TRUE(SendKey("d", session_, &command));
  EXPECT_TRUE(SendKey("k", session_, &command));
  EXPECT_TRUE(SendKey("s", session_, &command));
  ExpectPreedit("\xEC\x95\x88", 1, command);  // "안"
  EXPECT_TRUE(SendKey("\\", session_, &command));
  ExpectResult("\xEC\x95\x88\xE2\x82\xA9", command);  // "한₩"
}

TEST_F(HangulSessionTest, NumpadSelectionTest) {
  commands::Command command;
  SetUpKeyboard(config::HangulConfig::KEYBOARD_Dubeolsik);

  // Candidates for "다" by "ek"
  const char *kCandidates[] = {
    "\xE5\xA4\x9A",  // "多"
    "\xE8\x8C\xB6",  // "茶"
    "\xE7\x88\xB9",  // "爹"
    "\xE5\x97\xB2",  // "嗲"
    "\xE5\xA4\x9B",  // "夛"
    "\xE8\x8C\xA4",  // "茤"
    "\xE8\xA7\xB0",  // "觰"
    "\xE8\xB7\xA2",  // "跢"
    "\xE9\xAF\xBA",  // "鯺"
    "\xE4\xAB\x82",  // "䫂"
    "\xE4\xAF\xAC"  // "䯬"
  };

  const commands::KeyEvent::SpecialKey kNumPadKeys[] = {
    commands::KeyEvent::NUMPAD0,
    commands::KeyEvent::NUMPAD1,
    commands::KeyEvent::NUMPAD2,
    commands::KeyEvent::NUMPAD3,
    commands::KeyEvent::NUMPAD4,
    commands::KeyEvent::NUMPAD5,
    commands::KeyEvent::NUMPAD6,
    commands::KeyEvent::NUMPAD7,
    commands::KeyEvent::NUMPAD8,
    commands::KeyEvent::NUMPAD9,
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kNumPadKeys); ++i) {
    EXPECT_TRUE(SendKey("e", session_, &command));
    EXPECT_TRUE(SendKey("k", session_, &command));
    EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::HANJA, session_, &command));
    ExpectPreedit("\xEB\x8B\xA4", 1, command);  // "다"

    EXPECT_TRUE(SendSpecialKey(kNumPadKeys[(i + 1) % 10], session_, &command));
    ExpectResult(kCandidates[i], command);
    EXPECT_FALSE(SessionIsHanjaSelectionMode());
    EXPECT_FALSE(SessionHasReproduciblePreedit());
  }
}

TEST_F(HangulSessionTest, SymbolDictionaryTest) {
  commands::Command command;
  const char kTestDictName[] = "/tmp/test_dict.txt";
  const char kTestDict[] =
      "\xE3\x85\x81:\xEF\xBC\x83:\n"  // "ㅁ:＃:"
      "\xE3\x85\x81:\xEF\xBC\x86:\n"  // "ㅁ:＆:"
      "\xE3\x85\x81:\xEF\xBC\x8A:\n"  // "ㅁ:＊:"
      "\xE3\x85\x8E:\xCE\x91:\n"  // "ㅎ:Α:"
      "\xE3\x85\x8E:\xCE\x92:\n"  // "ㅎ:Β:"
      "\xE3\x85\x8E:\xCE\x93:\n";  // "ㅎ:Γ:"

  OutputFileStream ofs(kTestDictName, ios::binary);
  ofs.write(kTestDict, strlen(kTestDict));
  ofs.close();

  // Set input mode as "Hangul".
  EXPECT_TRUE(SetUpCompositionMode(commands::HIRAGANA, &command));

  SetUpKeyboard(config::HangulConfig::KEYBOARD_Dubeolsik);
  EXPECT_TRUE(ReloadSymbolDictionary(kTestDictName));

  EXPECT_TRUE(SendKey("a", session_, &command));
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::HANJA, session_, &command));
  ExpectPreedit("\xE3\x85\x81", 1, command);  // "ㅁ"

  EXPECT_EQ(3, GetCandidateCount(command));
  EXPECT_EQ("\xEF\xBC\x83", GetNthCandidate(command, 0));  // "＃"
  EXPECT_EQ("\xEF\xBC\x86", GetNthCandidate(command, 1));  // "＆"
  EXPECT_EQ("\xEF\xBC\x8A", GetNthCandidate(command, 2));  // "＊"
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::ENTER, session_, &command));

  EXPECT_TRUE(SendKey("g", session_, &command));
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::HANJA, session_, &command));
  ExpectPreedit("\xE3\x85\x8E", 1, command);  // "ㅎ"
  EXPECT_EQ(3, GetCandidateCount(command));
  EXPECT_EQ("\xCE\x91", GetNthCandidate(command, 0));  // "Α"
  EXPECT_EQ("\xCE\x92", GetNthCandidate(command, 1));  // "B"
  EXPECT_EQ("\xCE\x93", GetNthCandidate(command, 2));  // "Γ"
  EXPECT_TRUE(SendSpecialKey(commands::KeyEvent::ENTER, session_, &command));
  FileUtil::Unlink(kTestDictName);
}

#endif  // OS_CHROMEOS

}  // namespace hangul
}  // namespace mozc
