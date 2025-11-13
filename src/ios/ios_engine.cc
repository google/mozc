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

#include "ios/ios_engine.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/base/thread_annotations.h"
#include "absl/log/log.h"
#include "absl/synchronization/mutex.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "data_manager/data_manager.h"
#include "engine/engine.h"
#include "engine/engine_interface.h"
#include "protocol/commands.pb.h"
#include "protocol/user_dictionary_storage.pb.h"
#include "session/session_handler.h"

namespace mozc {
namespace ios {
namespace {

using ::mozc::user_dictionary::UserDictionaryCommand;
using ::mozc::user_dictionary::UserDictionaryCommandStatus;

const char *kIosSystemDictionaryName = "iOS_system_dictionary";

std::unique_ptr<EngineInterface> CreateMobileEngine(
    const std::string &data_file_path) {
  absl::StatusOr<std::unique_ptr<const DataManager>> data_manager =
      DataManager::CreateFromFile(data_file_path);
  if (!data_manager.ok()) {
    LOG(ERROR)
        << "Fallback to MinimalEngine due to data manager creation error: "
        << data_manager.status();
    return Engine::CreateEngine();
  }
  auto engine = Engine::CreateEngine(*std::move(data_manager));
  if (!engine.ok()) {
    LOG(ERROR) << "Failed to create an engine: " << engine.status()
               << ". Fallback to MinimalEngine";
    return Engine::CreateEngine();
  }
  return *std::move(engine);
}

std::unique_ptr<SessionHandler> CreateSessionHandler(
    const std::string &data_file_path) {
  std::unique_ptr<EngineInterface> engine = CreateMobileEngine(data_file_path);
  return std::make_unique<SessionHandler>(std::move(engine));
  // TODO(noriyukit): Add SessionUsageObserver by AddObserver().
}

void InitMobileRequest(commands::Request::SpecialRomanjiTable table_type,
                       commands::Request *request) {
  request->set_zero_query_suggestion(true);
  request->set_mixed_conversion(true);
  request->set_update_input_mode_from_surrounding_text(false);
  request->set_special_romanji_table(table_type);
  request->set_kana_modifier_insensitive_conversion(true);
  request->set_auto_partial_suggestion(true);
  request->set_language_aware_input(
      commands::Request::LANGUAGE_AWARE_SUGGESTION);
  request->set_space_on_alphanumeric(commands::Request::COMMIT);
}

bool UserDictCommandFailed(const commands::Command &command) {
  return command.output().user_dictionary_command_status().status() !=
         UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS;
}

uint64_t FindDictionaryId(const commands::Command &command,
                          const std::string &dictionary_name) {
  const auto &status = command.output().user_dictionary_command_status();
  for (const auto &dict : status.storage().dictionaries()) {
    if (dict.name() == dictionary_name) {
      return dict.id();
    }
  }
  return 0;
}

}  // namespace

// Returns the input config tuple that corresponds to the given keyboard layout.
IosEngine::InputConfigTuple IosEngine::GetInputConfigTupleFromLayoutName(
    const std::string &layout) {
  using commands::Request;
  if (layout == "12KEYS") {
    return {{Request::TOGGLE_FLICK_TO_HIRAGANA_INTUITIVE, commands::HIRAGANA},
            {Request::TOGGLE_FLICK_TO_HALFWIDTHASCII_IOS, commands::HALF_ASCII},
            {Request::TOGGLE_FLICK_TO_NUMBER, commands::HALF_ASCII}};
  }
  if (layout == "12KEYS_QWERTY") {
    return {{Request::TOGGLE_FLICK_TO_HIRAGANA_INTUITIVE, commands::HIRAGANA},
            {Request::QWERTY_MOBILE_TO_HALFWIDTHASCII, commands::HALF_ASCII},
            {Request::QWERTY_MOBILE_TO_HALFWIDTHASCII, commands::HALF_ASCII}};
  }
  if (layout == "12KEYS_FLICKONLY") {
    return {{Request::FLICK_TO_HIRAGANA_INTUITIVE, commands::HIRAGANA},
            {Request::FLICK_TO_HALFWIDTHASCII_IOS, commands::HALF_ASCII},
            {Request::FLICK_TO_NUMBER, commands::HALF_ASCII}};
  }
  if (layout == "12KEYS_FLICKONLY_QWERTY") {
    return {{Request::FLICK_TO_HIRAGANA_INTUITIVE, commands::HIRAGANA},
            {Request::QWERTY_MOBILE_TO_HALFWIDTHASCII, commands::HALF_ASCII},
            {Request::QWERTY_MOBILE_TO_HALFWIDTHASCII, commands::HALF_ASCII}};
  }
  if (layout == "QWERTY_JA") {
    return {{Request::QWERTY_MOBILE_TO_HIRAGANA, commands::HIRAGANA},
            {Request::QWERTY_MOBILE_TO_HALFWIDTHASCII, commands::HALF_ASCII},
            {Request::QWERTY_MOBILE_TO_HALFWIDTHASCII, commands::HALF_ASCII}};
  }
  if (layout == "GODAN") {
    return {{Request::GODAN_TO_HIRAGANA, commands::HIRAGANA},
            {Request::QWERTY_MOBILE_TO_HALFWIDTHASCII, commands::HALF_ASCII},
            {Request::QWERTY_MOBILE_TO_HALFWIDTHASCII, commands::HALF_ASCII}};
  }
  LOG(ERROR) << "Unexpected keyboard layout: " << layout
             << ". The same config as 12KEYS is used";
  return {{Request::TOGGLE_FLICK_TO_HIRAGANA_INTUITIVE, commands::HIRAGANA},
          {Request::TWELVE_KEYS_TO_HALFWIDTHASCII, commands::HALF_ASCII},
          {Request::QWERTY_MOBILE_TO_HALFWIDTHASCII, commands::HALF_ASCII}};
}

IosEngine::IosEngine(const std::string &data_file_path)
    : session_handler_(CreateSessionHandler(data_file_path)),
      session_id_(0),
      previous_command_(commands::SessionCommand::NONE) {
  current_input_config_ = &current_config_tuple_.hiragana_config;
}

IosEngine::~IosEngine() = default;

bool IosEngine::SetMobileRequest(const std::string &keyboard_layout,
                                 commands::Command *command) {
  command->Clear();
  auto *input = command->mutable_input();
  input->set_type(commands::Input::SET_REQUEST);
  current_config_tuple_ = GetInputConfigTupleFromLayoutName(keyboard_layout);
  InitMobileRequest(current_config_tuple_.hiragana_config.romaji_table,
                    input->mutable_request());
  current_request_ = input->request();
  current_input_config_ = &current_config_tuple_.hiragana_config;
  return EvalCommandLockGuarded(command);
}

void IosEngine::FillMobileConfig(config::Config *config) {
  *config = config::ConfigHandler::GetCopiedConfig();
  config->set_session_keymap(config::Config::MOBILE);
  config->set_use_kana_modifier_insensitive_conversion(true);
  config->set_space_character_form(config::Config::FUNDAMENTAL_HALF_WIDTH);
}

bool IosEngine::SetConfig(const config::Config &config,
                          commands::Command *command) {
  command->Clear();
  auto *input = command->mutable_input();
  input->set_type(commands::Input::SET_CONFIG);
  *(input->mutable_config()) = config;
  return EvalCommandLockGuarded(command);
}

bool IosEngine::CreateSession(commands::Command *command) {
  if (!DeleteSession(command)) {
    return false;
  }
  command->Clear();
  command->mutable_input()->set_type(commands::Input::CREATE_SESSION);
  if (!EvalCommandLockGuarded(command)) {
    return false;
  }
  session_id_ = command->output().id();
  return true;
}

bool IosEngine::DeleteSession(commands::Command *command) {
  if (session_id_ == 0) {
    return true;
  }
  command->Clear();
  commands::Input *input = command->mutable_input();
  input->set_type(commands::Input::DELETE_SESSION);
  input->set_id(session_id_);
  if (!EvalCommandLockGuarded(command)) {
    return false;
  }
  session_id_ = 0;
  return true;
}

bool IosEngine::ResetContext(commands::Command *command) {
  if (previous_command_ == commands::SessionCommand::RESET_CONTEXT) {
    return false;
  }

  command->Clear();
  commands::Input *input = command->mutable_input();
  input->set_id(session_id_);
  input->set_type(commands::Input::SEND_COMMAND);
  input->mutable_command()->set_type(commands::SessionCommand::RESET_CONTEXT);
  return EvalCommandLockGuarded(command);
}

bool IosEngine::SendSpecialKey(commands::KeyEvent::SpecialKey special_key,
                               commands::Command *command) {
  command->Clear();
  commands::Input *input = command->mutable_input();
  input->set_id(session_id_);
  input->set_type(commands::Input::SEND_KEY);
  input->mutable_key()->set_special_key(special_key);
  return EvalCommandLockGuarded(command);
}

bool IosEngine::SendKey(const std::string &character,
                        commands::Command *command) {
  command->Clear();
  commands::Input *input = command->mutable_input();
  input->set_id(session_id_);
  input->set_type(commands::Input::SEND_KEY);
  commands::KeyEvent *key_event = input->mutable_key();
  key_event->set_key_code(Util::Utf8ToCodepoint(character));
  key_event->set_mode(current_input_config_->composition_mode);
  constexpr uint32_t kNoModifiers = 0;
  key_event->set_modifiers(kNoModifiers);
  return EvalCommandLockGuarded(command);
}

bool IosEngine::MaybeCreateNewChunk(commands::Command *command)
    ABSL_NO_THREAD_SAFETY_ANALYSIS {
  switch (current_request_.special_romanji_table()) {
    case commands::Request::TOGGLE_FLICK_TO_HALFWIDTHASCII_IOS:
    case commands::Request::TOGGLE_FLICK_TO_HIRAGANA_INTUITIVE:
    case commands::Request::TOGGLE_FLICK_TO_NUMBER:
    case commands::Request::TWELVE_KEYS_TO_HALFWIDTHASCII:
      break;
    default:
      return false;
  }
  command->Clear();
  commands::Input *input = command->mutable_input();
  input->set_id(session_id_);
  input->set_type(commands::Input::SEND_COMMAND);
  commands::SessionCommand *session_command = input->mutable_command();
  session_command->set_type(commands::SessionCommand::STOP_KEY_TOGGLING);
  // TODO(b/438604511): Use try_lock when Abseil LTS supports it.
  if (!mutex_.try_lock()) {
    return false;
  }
  const bool ret = session_handler_->EvalCommand(command);
  // TODO(b/438604511): Use unlock when Abseil LTS supports it.
  mutex_.unlock();
  return ret;
}

bool IosEngine::SendSessionCommand(
    const commands::SessionCommand &session_command,
    commands::Command *command) {
  command->Clear();
  commands::Input *input = command->mutable_input();
  input->set_id(session_id_);
  input->set_type(commands::Input::SEND_COMMAND);
  *(input->mutable_command()) = session_command;
  return EvalCommandLockGuarded(command);
}

bool IosEngine::EvalCommandLockGuarded(commands::Command *command) {
  // TODO(b/438604511): Use mutex_ (w/o &) when Abseil LTS supports it.
  absl::MutexLock l(mutex_);
  if (command->input().has_command()) {
    previous_command_ = command->input().command().type();
  } else {
    previous_command_ = commands::SessionCommand::NONE;
  }
  return session_handler_->EvalCommand(command);
}

bool IosEngine::SwitchInputMode(InputMode mode) {
  InputConfig *target_config = nullptr;
  switch (mode) {
    case InputMode::HIRAGANA:
      target_config = &current_config_tuple_.hiragana_config;
      break;
    case InputMode::ALPHABET:
      target_config = &current_config_tuple_.alphabet_config;
      break;
    case InputMode::DIGIT:
      target_config = &current_config_tuple_.digit_config;
      break;
    default:
      ABSL_UNREACHABLE();
  }
  if (current_input_config_ == target_config) {
    return true;
  }
  if (!SetSpecialRomajiTable(target_config->romaji_table)) {
    return false;
  }
  current_input_config_ = target_config;
  return true;
}

bool IosEngine::SetSpecialRomajiTable(
    commands::Request::SpecialRomanjiTable table) {
  commands::Command command;
  auto *input = command.mutable_input();
  input->set_type(commands::Input::SET_REQUEST);
  *input->mutable_request() = current_request_;
  input->mutable_request()->set_special_romanji_table(table);
  if (!EvalCommandLockGuarded(&command)) {
    return false;
  }
  current_request_.Swap(input->mutable_request());
  return true;
}

bool IosEngine::ImportUserDictionary(const std::string &tsv_content,
                                     commands::Command *command) {
  {
    ScopedUserDictionarySession session(this);
    if (session.user_dict_session_id() == 0) {
      return false;
    }
    if (!LoadUserDictionaryIfExists(session.user_dict_session_id(), command)) {
      LOG(ERROR) << "Failed to load user dictionary: " << command;
      return false;
    }
    if (!DeleteUserDictionaryIfExists(session.user_dict_session_id(),
                                      kIosSystemDictionaryName, command)) {
      LOG(ERROR) << "Failed to delete a user dictionary ["
                 << kIosSystemDictionaryName << "]: " << command;
      return false;
    }
    // Import data only when tsv_content is nonempty.
    if (!tsv_content.empty() &&
        !ImportDataToNewUserDictionary(session.user_dict_session_id(),
                                       kIosSystemDictionaryName, tsv_content,
                                       command)) {
      LOG(ERROR) << "Failed to import data to a new user dictionary ["
                 << kIosSystemDictionaryName << "]: " << command;
      return false;
    }
    if (!SaveUserDictionary(session.user_dict_session_id(), command)) {
      LOG(ERROR) << "Failed to save user dictionary to storage: " << command;
      return false;
    }
  }

  if (!Reload(command)) {
    LOG(ERROR) << "Failed to reload engine";
    return false;
  }

  return true;
}

bool IosEngine::ClearUserHistory(commands::Command *command) {
  command->Clear();
  commands::Input *input = command->mutable_input();
  input->set_id(session_id_);
  input->set_type(commands::Input::CLEAR_USER_HISTORY);
  if (!EvalCommandLockGuarded(command)) {
    return false;
  }

  command->Clear();
  input = command->mutable_input();
  input->set_id(session_id_);
  input->set_type(commands::Input::CLEAR_USER_PREDICTION);
  if (!EvalCommandLockGuarded(command)) {
    return false;
  }

  // No need to call CLEAR_UNUSED_USER_PREDICTION.
  // The above CLEAR_USER_PREDICTION deletes unused prediction entries too.

  return true;
}

IosEngine::ScopedUserDictionarySession::ScopedUserDictionarySession(
    IosEngine *engine)
    : engine_(engine), user_dict_session_id_(0) {
  commands::Command command;
  commands::Input *input = command.mutable_input();
  input->set_id(engine->session_id_);
  input->set_type(commands::Input::SEND_USER_DICTIONARY_COMMAND);

  auto *user_dict_cmd = input->mutable_user_dictionary_command();
  user_dict_cmd->set_type(UserDictionaryCommand::CREATE_SESSION);
  if (!engine->EvalCommandLockGuarded(&command)) {
    LOG(ERROR) << "Failed to create a user dictionary session: " << command;
    return;
  }
  user_dict_session_id_ =
      command.output().user_dictionary_command_status().session_id();
}

IosEngine::ScopedUserDictionarySession::~ScopedUserDictionarySession() {
  if (user_dict_session_id_ == 0) {
    return;
  }
  commands::Command command;
  commands::Input *input = command.mutable_input();
  input->set_id(engine_->session_id_);
  input->set_type(commands::Input::SEND_USER_DICTIONARY_COMMAND);

  auto *user_dict_cmd = input->mutable_user_dictionary_command();
  user_dict_cmd->set_type(UserDictionaryCommand::DELETE_SESSION);
  user_dict_cmd->set_session_id(user_dict_session_id_);
  if (!engine_->EvalCommandLockGuarded(&command)) {
    LOG(ERROR) << "Failed to delete user dictionary session "
               << user_dict_session_id_
               << ".  This session may leak: " << command;
  }
}

bool IosEngine::LoadUserDictionaryIfExists(uint64_t user_dict_session_id,
                                           commands::Command *command) {
  command->Clear();
  commands::Input *input = command->mutable_input();
  input->set_id(session_id_);
  input->set_type(commands::Input::SEND_USER_DICTIONARY_COMMAND);

  auto *user_dict_cmd = input->mutable_user_dictionary_command();
  user_dict_cmd->set_session_id(user_dict_session_id);
  user_dict_cmd->set_type(UserDictionaryCommand::LOAD);
  if (!EvalCommandLockGuarded(command)) {
    return false;
  }
  const auto status =
      command->output().user_dictionary_command_status().status();
  switch (status) {
    case UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS:
    case UserDictionaryCommandStatus::FILE_NOT_FOUND:
      return true;
    default:
      return false;
  }
}

bool IosEngine::DeleteUserDictionaryIfExists(uint64_t user_dict_session_id,
                                             const std::string &dictionary_name,
                                             commands::Command *command) {
  command->Clear();
  commands::Input *input = command->mutable_input();
  input->set_id(session_id_);
  input->set_type(commands::Input::SEND_USER_DICTIONARY_COMMAND);
  auto *user_dict_cmd = input->mutable_user_dictionary_command();
  user_dict_cmd->set_session_id(user_dict_session_id);
  user_dict_cmd->set_type(UserDictionaryCommand::GET_USER_DICTIONARY_NAME_LIST);
  if (!EvalCommandLockGuarded(command) || UserDictCommandFailed(*command)) {
    return false;
  }

  const uint64_t dictionary_id = FindDictionaryId(*command, dictionary_name);
  if (dictionary_id == 0) {
    return true;
  }

  command->mutable_output()->Clear();
  user_dict_cmd->Clear();
  user_dict_cmd->set_session_id(user_dict_session_id);
  user_dict_cmd->set_type(UserDictionaryCommand::DELETE_DICTIONARY);
  user_dict_cmd->set_dictionary_id(dictionary_id);
  return EvalCommandLockGuarded(command) && !UserDictCommandFailed(*command);
}

bool IosEngine::ImportDataToNewUserDictionary(
    uint64_t user_dict_session_id, const std::string &dictionary_name,
    const std::string &tsv_content, commands::Command *command) {
  command->Clear();
  commands::Input *input = command->mutable_input();
  input->set_id(session_id_);
  input->set_type(commands::Input::SEND_USER_DICTIONARY_COMMAND);
  auto *user_dict_cmd = input->mutable_user_dictionary_command();
  user_dict_cmd->set_session_id(user_dict_session_id);
  user_dict_cmd->set_type(UserDictionaryCommand::IMPORT_DATA);
  user_dict_cmd->set_dictionary_name(dictionary_name);
  user_dict_cmd->set_data(tsv_content);
  user_dict_cmd->set_ignore_invalid_entries(true);
  return EvalCommandLockGuarded(command) && !UserDictCommandFailed(*command);
}

bool IosEngine::SaveUserDictionary(uint64_t user_dict_session_id,
                                   commands::Command *command) {
  command->Clear();
  commands::Input *input = command->mutable_input();
  input->set_id(session_id_);
  input->set_type(commands::Input::SEND_USER_DICTIONARY_COMMAND);
  auto *user_dict_cmd = input->mutable_user_dictionary_command();
  user_dict_cmd->set_session_id(user_dict_session_id);
  user_dict_cmd->set_type(UserDictionaryCommand::SAVE);
  return EvalCommandLockGuarded(command) && !UserDictCommandFailed(*command);
}

bool IosEngine::Reload(commands::Command *command) {
  command->Clear();
  commands::Input *input = command->mutable_input();
  input->set_id(session_id_);
  input->set_type(commands::Input::RELOAD);
  return EvalCommandLockGuarded(command);
}

}  // namespace ios
}  // namespace mozc
