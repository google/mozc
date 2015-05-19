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

#include "languages/pinyin/session.h"

#include <cctype>
#include <string>
#include <vector>

#include "base/base.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "languages/pinyin/keymap.h"
#include "languages/pinyin/pinyin_constant.h"
#include "languages/pinyin/session_config.h"
#include "languages/pinyin/session_converter.h"
#include "session/commands.pb.h"
#include "session/key_event_util.h"

// To implement AutoCommit correctly, we implemented following steps.
//
// 1. Calls SelectFocusedCandidate() and fills protocol buffers with current
//    context.
// 2. Discards a current context and creates a Punctuation context.
// 3. Inserts a character, which triggered AutoCommit.
// 4. Calls SelectFocusedCandidate() and appends new commit text.
// 5. Discards a Punctuation context and a create context.
//
// But it is not a natural implementation because filling a protocol buffer is a
// task of SessionConverter.  I think following implementation is better.
//
// 1. Stores all contexts on SessionConverter. These contexts are not discarded
//    until Session instance is destructed.
// 2. Implements a new method for AutoCommit on SessionConverter.
// 3. Session simply calls a new method. SessionConverter commits a current
//    context, inserts a character which triggered AutoCommit to Punctuation
//    context, commits Punctuation context, and merges a commit text of these
//    contexts.
//
// TODO(hsumita): Refactors Session and SessionConverter to store all contexts
// on SessionConverter. (But it might be hard to rewrite
// session_converter_test.cc)

namespace mozc {
namespace pinyin {

namespace {
// TODO(hsumita): Unify config updating mechanism like session_handler.
uint64 g_last_config_updated = 0;

size_t GetIndexFromKeyEvent(const commands::KeyEvent &key_event) {
  // This class only accepts number keys.
  DCHECK(key_event.has_key_code());
  const uint32 key_code = key_event.key_code();
  DCHECK(isdigit(key_code));
  return (key_code == '0') ? 9 : key_code - '1';
}
}  // namespace

Session::Session()
    : session_config_(new SessionConfig),
      converter_(new SessionConverter(*session_config_)),
      conversion_mode_(NONE),
      next_conversion_mode_(NONE),
      is_already_commited_(false),
      create_session_time_(Util::GetTime()),
      last_command_time_(0),
      last_config_updated_(0) {
  // Initializes session_config_
  const config::PinyinConfig &config = GET_CONFIG(pinyin_config);
  session_config_->full_width_word_mode = config.initial_mode_full_width_word();
  session_config_->full_width_punctuation_mode =
      config.initial_mode_full_width_punctuation();
  session_config_->simplified_chinese_mode =
      config.initial_mode_simplified_chinese();

  const ConversionMode conversion_mode =
      GET_CONFIG(pinyin_config).initial_mode_chinese() ? PINYIN : DIRECT;
  SwitchConversionMode(conversion_mode);
  ResetConfig();
}

Session::~Session() {
}

bool Session::SendKey(commands::Command *command) {
  DCHECK(command);

  last_command_time_ = Util::GetTime();
  is_already_commited_ = false;

  const bool consumed = ProcessKeyEvent(command);
  command->mutable_output()->set_consumed(consumed);

  if (!is_already_commited_) {
    converter_->PopOutput(command->mutable_output());
  }

  SwitchConversionMode(next_conversion_mode_);

  DLOG(INFO) << command->DebugString();

  return true;
}

bool Session::TestSendKey(commands::Command *command) {
  DCHECK(command);

  // TODO(hsumita): implement this.
  last_command_time_ = Util::GetTime();
  if (g_last_config_updated > last_config_updated_) {
    ResetConfig();
  }

  return true;
}

bool Session::SendCommand(commands::Command *command) {
  DCHECK(command);

  last_command_time_ = Util::GetTime();
  is_already_commited_ = false;
  if (g_last_config_updated > last_config_updated_) {
    ResetConfig();
  }

  bool consumed = ProcessCommand(command);
  command->mutable_output()->set_consumed(consumed);

  if (!is_already_commited_) {
    converter_->PopOutput(command->mutable_output());
  }

  SwitchConversionMode(next_conversion_mode_);

  DLOG(INFO) << command->DebugString();

  return true;
}

void Session::ReloadConfig() {
  last_command_time_ = Util::GetTime();
  ResetConfig();
}

#ifdef OS_CHROMEOS
void Session::UpdateConfig(const config::PinyinConfig &config) {
  config::Config mozc_config;
  mozc_config.mutable_pinyin_config()->MergeFrom(config);
  config::ConfigHandler::SetConfig(mozc_config);
  g_last_config_updated = Util::GetTime();
}
#endif  // OS_CHROMEOS

void Session::set_client_capability(const commands::Capability &capability) {
  // Does nothing.  Capability does not make sense with the current pinyin.
}

void Session::set_application_info(
    const commands::ApplicationInfo &application_info) {
  application_info_.CopyFrom(application_info);
}

const commands::ApplicationInfo &Session::application_info() const {
  return application_info_;
}

uint64 Session::create_session_time() const {
  return create_session_time_;
}

uint64 Session::last_command_time() const {
  return last_command_time_;
}

namespace {
// Returns true when key_command can change input mode to English. If this
// function returns true, it doesn't mean that always we change input mode.
// We should NOT change input mode when converter is active.
bool MaybePinyinModeCommandForKeyCommand(keymap::KeyCommand key_command) {
  switch (key_command) {
    case keymap::CLEAR:
    case keymap::COMMIT:
    case keymap::COMMIT_PREEDIT:
    case keymap::SELECT_CANDIDATE:
    case keymap::SELECT_FOCUSED_CANDIDATE:
    case keymap::SELECT_SECOND_CANDIDATE:
    case keymap::SELECT_THIRD_CANDIDATE:
    case keymap::REMOVE_CHAR_BEFORE:
    case keymap::REMOVE_CHAR_AFTER:
    case keymap::REMOVE_WORD_BEFORE:
    case keymap::REMOVE_WORD_AFTER:
      return true;
    default:
      return false;
  }
}

// Returns true when key_command can change input mode to English. If this
// function returns true, it doesn't mean that always we change input mode.
// We should NOT change input mode when converter is active.
bool MaybePinyinModeCommandForSessionCommand(
    const commands::SessionCommand &session_command) {
  switch (session_command.type()) {
    case commands::SessionCommand::SUBMIT:
    case commands::SessionCommand::SELECT_CANDIDATE:
      return true;
    default:
      return false;
  }
}
}  // namespace

bool Session::ProcessKeyEvent(commands::Command *command) {
  DCHECK(command);

  commands::KeyEvent key_event;
  {
    commands::KeyEvent temp_key_event;
    KeyEventUtil::RemoveModifiers(command->input().key(),
                                  commands::KeyEvent::CAPS,
                                  &temp_key_event);
    KeyEventUtil::NormalizeNumpadKey(temp_key_event, &key_event);
  }

  const keymap::ConverterState converter_state =
      (converter_->IsConverterActive())
      ? keymap::ACTIVE : keymap::INACTIVE;

  keymap::KeyCommand key_command;

  keymap_->GetCommand(key_event, converter_state, &key_command);
  VLOG(2) << "KeyCommand: " << key_command << "\n"
          << "Converter State: "
          << (converter_state == keymap::ACTIVE ? "ACTIVE" : "INACTIVE")
          << "\n"
          << "Conversion Mode: " << conversion_mode_;
  bool consumed = true;

  switch (key_command) {
    case keymap::INSERT:
      consumed = converter_->Insert(key_event);
      break;
    case keymap::INSERT_PUNCTUATION:
      {
        const ConversionMode comeback_conversion_mode = conversion_mode_;
        SwitchConversionMode(PUNCTUATION);
        consumed = converter_->Insert(key_event);
        next_conversion_mode_ = comeback_conversion_mode;
      }
      break;
    case keymap::COMMIT:
      converter_->Commit();
      break;
    case keymap::COMMIT_PREEDIT:
      converter_->CommitPreedit();
      break;
    case keymap::CLEAR:
      converter_->Clear();
      break;
    case keymap::AUTO_COMMIT:
      // TODO(hsumita): Moves these logics into SessionConverter.
      // Details are written on the top of this file.
      converter_->SelectFocusedCandidate();
      if (converter_->IsConverterActive()) {
        converter_->Commit();
      }
      converter_->PopOutput(command->mutable_output());

      {
        const ConversionMode comeback_conversion_mode = conversion_mode_;
        const string previous_commit_text = command->output().result().value();
        SwitchConversionMode(PUNCTUATION);
        converter_->Insert(key_event);
        converter_->PopOutput(command->mutable_output());
        const string punctuation_commit_text =
            command->output().result().value();
        command->mutable_output()->mutable_result()->set_value(
            previous_commit_text + punctuation_commit_text);

        is_already_commited_ = true;
        next_conversion_mode_ = comeback_conversion_mode;
      }

      break;
    case keymap::MOVE_CURSOR_RIGHT:
      converter_->MoveCursorRight();
      break;
    case keymap::MOVE_CURSOR_LEFT:
      converter_->MoveCursorLeft();
      break;
    case keymap::MOVE_CURSOR_RIGHT_BY_WORD:
      converter_->MoveCursorRightByWord();
      break;
    case keymap::MOVE_CURSOR_LEFT_BY_WORD:
      converter_->MoveCursorLeftByWord();
      break;
    case keymap::MOVE_CURSOR_TO_BEGINNING:
      converter_->MoveCursorToBeginning();
      break;
    case keymap::MOVE_CURSOR_TO_END:
      converter_->MoveCursorToEnd();
      break;

    case keymap::SELECT_CANDIDATE:
      converter_->SelectCandidateOnPage(GetIndexFromKeyEvent(key_event));
      break;
    case keymap::SELECT_FOCUSED_CANDIDATE:
      converter_->SelectFocusedCandidate();
      break;
    case keymap::SELECT_SECOND_CANDIDATE:
      converter_->SelectCandidateOnPage(1);
      break;
    case keymap::SELECT_THIRD_CANDIDATE:
      converter_->SelectCandidateOnPage(2);
      break;
    case keymap::FOCUS_CANDIDATE:
      converter_->FocusCandidateOnPage(GetIndexFromKeyEvent(key_event));
      break;
    case keymap::FOCUS_CANDIDATE_TOP:
      converter_->FocusCandidate(0);
      break;
    case keymap::FOCUS_CANDIDATE_PREV:
      converter_->FocusCandidatePrev();
      break;
    case keymap::FOCUS_CANDIDATE_NEXT:
      converter_->FocusCandidateNext();
      break;
    case keymap::FOCUS_CANDIDATE_PREV_PAGE:
      converter_->FocusCandidatePrevPage();
      break;
    case keymap::FOCUS_CANDIDATE_NEXT_PAGE:
      converter_->FocusCandidateNextPage();
      break;
    case keymap::CLEAR_CANDIDATE_FROM_HISTORY:
      converter_->ClearCandidateFromHistory(GetIndexFromKeyEvent(key_event));
      break;

    case keymap::REMOVE_CHAR_BEFORE:
      converter_->RemoveCharBefore();
      break;
    case keymap::REMOVE_CHAR_AFTER:
      converter_->RemoveCharAfter();
      break;
    case keymap::REMOVE_WORD_BEFORE:
      converter_->RemoveWordBefore();
      break;
    case keymap::REMOVE_WORD_AFTER:
      converter_->RemoveWordAfter();
      break;

    case keymap::TOGGLE_DIRECT_MODE:
      if (conversion_mode_ == DIRECT) {
        SwitchConversionMode(PINYIN);
      } else {
        SwitchConversionMode(DIRECT);
      }
      break;
    case keymap::TURN_ON_ENGLISH_MODE:
      SwitchConversionMode(ENGLISH);
      consumed = converter_->Insert(key_event);
      break;
    case keymap::TURN_ON_PUNCTUATION_MODE:
      SwitchConversionMode(PUNCTUATION);
      consumed = converter_->Insert(key_event);
      break;
    case keymap::TOGGLE_SIMPLIFIED_CHINESE_MODE:
      session_config_->simplified_chinese_mode =
          !session_config_->simplified_chinese_mode;
      ResetConfig();
      break;
    case keymap::DO_NOTHING_WITH_CONSUME:
      break;
    case keymap::DO_NOTHING_WITHOUT_CONSUME:
      consumed = false;
      break;
    default:
      LOG(ERROR) << "Should not reach here. command = " << key_command;
      consumed = false;
      break;
  }

  // Turn on Pinyin mode from English or Punctuation mode.
  if ((conversion_mode_ == ENGLISH || conversion_mode_ == PUNCTUATION) &&
      !converter_->IsConverterActive() &&
      MaybePinyinModeCommandForKeyCommand(key_command)) {
    next_conversion_mode_ = PINYIN;
  }

  return consumed;
}

bool Session::ProcessCommand(commands::Command *command) {
  DCHECK(command);

  if (!command->input().has_command()) {
    return false;
  }

  const commands::SessionCommand &session_command = command->input().command();

  bool consumed = true;

  switch (session_command.type()) {
    case commands::SessionCommand::REVERT:
    case commands::SessionCommand::RESET_CONTEXT:
      ResetContext();
      break;
    case commands::SessionCommand::SUBMIT:
      converter_->Commit();
      break;
    case commands::SessionCommand::SELECT_CANDIDATE:
      DCHECK(session_command.has_id());
      converter_->SelectCandidateOnPage(session_command.id());
      break;
    case commands::SessionCommand::SEND_LANGUAGE_BAR_COMMAND:
      HandleLanguageBarCommand(session_command);
      break;
    default:
      // Does nothing.
      DLOG(ERROR) << "Unexpected Session Command:" << command->DebugString();
      consumed = false;
      break;
  }

  // Turn on Pinyin mode from English or Punctuation mode.
  if ((conversion_mode_ == ENGLISH || conversion_mode_ == PUNCTUATION) &&
      !converter_->IsConverterActive() &&
      MaybePinyinModeCommandForSessionCommand(session_command)) {
    next_conversion_mode_ = PINYIN;
  }

  return consumed;
}

void Session::ResetContext() {
  converter_->Clear();
}

void Session::ResetConfig() {
  converter_->ReloadConfig();
  last_config_updated_ = Util::GetTime();
}

void Session::SwitchConversionMode(ConversionMode mode) {
  if (mode == conversion_mode_) {
    return;
  }

  conversion_mode_ = mode;
  next_conversion_mode_ = mode;

  switch (conversion_mode_) {
    case PINYIN:
      keymap_ = keymap::KeymapFactory::GetKeymap(keymap::PINYIN);
      break;
    case DIRECT:
      keymap_ = keymap::KeymapFactory::GetKeymap(keymap::DIRECT);
      break;
    case ENGLISH:
      keymap_ = keymap::KeymapFactory::GetKeymap(keymap::ENGLISH);
      break;
    case PUNCTUATION:
      keymap_ = keymap::KeymapFactory::GetKeymap(keymap::PUNCTUATION);
      break;
    default:
      LOG(ERROR) << "Should NOT reach here. Set a fallback context";
      conversion_mode_ = PINYIN;
      keymap_ = keymap::KeymapFactory::GetKeymap(keymap::PINYIN);
      break;
  }

  converter_->SwitchContext(conversion_mode_);
}

void Session::HandleLanguageBarCommand(
    const commands::SessionCommand &session_command) {
  DCHECK(session_command.has_language_bar_command_id());

  switch (session_command.language_bar_command_id()) {
    case commands::SessionCommand::TOGGLE_PINYIN_CHINESE_MODE:
      if (conversion_mode_ == DIRECT) {
        SwitchConversionMode(PINYIN);
      } else {
        SwitchConversionMode(DIRECT);
      }
      break;
    case commands::SessionCommand::TOGGLE_PINYIN_FULL_WIDTH_WORD_MODE:
      session_config_->full_width_word_mode =
          !session_config_->full_width_word_mode;
      break;
    case commands::SessionCommand::TOGGLE_PINYIN_FULL_WIDTH_PUNCTUATION_MODE:
      session_config_->full_width_punctuation_mode =
          !session_config_->full_width_punctuation_mode;
      break;
    case commands::SessionCommand::TOGGLE_PINYIN_SIMPLIFIED_CHINESE_MODE:
      session_config_->simplified_chinese_mode =
          !session_config_->simplified_chinese_mode;
      break;
    default:
      LOG(ERROR) << "Unknown session request. Should NOT reach here.";
      break;
  }

  ResetConfig();
}

}  // namespace pinyin
}  // namespace mozc
