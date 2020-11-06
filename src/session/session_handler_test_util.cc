// Copyright 2010-2020, Google Inc.
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

#include "session/session_handler_test_util.h"

#include <utility>

#include "base/config_file_stream.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/system_util.h"
#include "config/character_form_manager.h"
#include "config/config_handler.h"
#include "converter/converter_interface.h"
#include "engine/engine_interface.h"
#include "prediction/user_history_predictor.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/session_handler.h"
#include "session/session_handler_interface.h"
#include "session/session_usage_observer.h"
#include "storage/registry.h"
#include "testing/base/public/googletest.h"

DECLARE_int32(max_session_size);
DECLARE_int32(create_session_min_interval);
DECLARE_int32(watch_dog_interval);
DECLARE_int32(last_command_timeout);
DECLARE_int32(last_create_session_timeout);
DECLARE_bool(restricted);

namespace mozc {
namespace session {
namespace testing {

using commands::Command;
using config::CharacterFormManager;
using config::ConfigHandler;

bool CreateSession(SessionHandlerInterface *handler, uint64 *id) {
  Command command;
  command.mutable_input()->set_type(commands::Input::CREATE_SESSION);
  command.mutable_input()->mutable_capability()->set_text_deletion(
      commands::Capability::DELETE_PRECEDING_TEXT);
  handler->EvalCommand(&command);
  if (id != nullptr) {
    *id = command.has_output() ? command.output().id() : 0;
  }
  return (command.output().error_code() == commands::Output::SESSION_SUCCESS);
}

bool DeleteSession(SessionHandlerInterface *handler, uint64 id) {
  Command command;
  command.mutable_input()->set_id(id);
  command.mutable_input()->set_type(commands::Input::DELETE_SESSION);
  return handler->EvalCommand(&command);
}

bool CleanUp(SessionHandlerInterface *handler, uint64 id) {
  Command command;
  command.mutable_input()->set_id(id);
  command.mutable_input()->set_type(commands::Input::CLEANUP);
  return handler->EvalCommand(&command);
}

bool ClearUserPrediction(SessionHandlerInterface *handler, uint64 id) {
  Command command;
  command.mutable_input()->set_id(id);
  command.mutable_input()->set_type(commands::Input::CLEAR_USER_PREDICTION);
  return handler->EvalCommand(&command);
}

bool IsGoodSession(SessionHandlerInterface *handler, uint64 id) {
  Command command;
  command.mutable_input()->set_id(id);
  command.mutable_input()->set_type(commands::Input::SEND_KEY);
  command.mutable_input()->mutable_key()->set_special_key(
      commands::KeyEvent::SPACE);
  handler->EvalCommand(&command);
  return (command.output().error_code() == commands::Output::SESSION_SUCCESS);
}

SessionHandlerTestBase::SessionHandlerTestBase() {}
SessionHandlerTestBase::~SessionHandlerTestBase() {}

void SessionHandlerTestBase::SetUp() {
  flags_max_session_size_backup_ = FLAGS_max_session_size;
  flags_create_session_min_interval_backup_ = FLAGS_create_session_min_interval;
  flags_watch_dog_interval_backup_ = FLAGS_watch_dog_interval;
  flags_last_command_timeout_backup_ = FLAGS_last_command_timeout;
  flags_last_create_session_timeout_backup_ = FLAGS_last_create_session_timeout;
  flags_restricted_backup_ = FLAGS_restricted;

  user_profile_directory_backup_ = SystemUtil::GetUserProfileDirectory();
  SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
  ConfigHandler::GetConfig(&config_backup_);
  ClearState();
}

void SessionHandlerTestBase::TearDown() {
  ClearState();
  ConfigHandler::SetConfig(config_backup_);
  SystemUtil::SetUserProfileDirectory(user_profile_directory_backup_);

  FLAGS_max_session_size = flags_max_session_size_backup_;
  FLAGS_create_session_min_interval = flags_create_session_min_interval_backup_;
  FLAGS_watch_dog_interval = flags_watch_dog_interval_backup_;
  FLAGS_last_command_timeout = flags_last_command_timeout_backup_;
  FLAGS_last_create_session_timeout = flags_last_create_session_timeout_backup_;
  FLAGS_restricted = flags_restricted_backup_;
}

void SessionHandlerTestBase::ClearState() {
  config::Config config;
  ConfigHandler::GetDefaultConfig(&config);
  ConfigHandler::SetConfig(config);

  // CharacterFormManager is not automatically updated when the config is
  // updated.
  CharacterFormManager::GetCharacterFormManager()->ReloadConfig(config);

  // Some destructors may save the state on storages. To clear the state, we
  // explicitly call destructors before clearing storages.
  storage::Registry::Clear();
  FileUtil::Unlink(ConfigFileStream::GetFileName("user://boundary.db"));
  FileUtil::Unlink(ConfigFileStream::GetFileName("user://segment.db"));
  FileUtil::Unlink(UserHistoryPredictor::GetUserHistoryFileName());
}

TestSessionClient::TestSessionClient(std::unique_ptr<EngineInterface> engine)
    : id_(0),
      usage_observer_(new SessionUsageObserver),
      handler_(new SessionHandler(std::move(engine))) {
  handler_->AddObserver(usage_observer_.get());
}

TestSessionClient::~TestSessionClient() {}

bool TestSessionClient::CreateSession() {
  return ::mozc::session::testing::CreateSession(handler_.get(), &id_);
}

bool TestSessionClient::DeleteSession() {
  return ::mozc::session::testing::DeleteSession(handler_.get(), id_);
}

bool TestSessionClient::CleanUp() {
  return ::mozc::session::testing::CleanUp(handler_.get(), id_);
}

bool TestSessionClient::ClearUserPrediction() {
  return ::mozc::session::testing::ClearUserPrediction(handler_.get(), id_);
}

bool TestSessionClient::SendKeyWithOption(const commands::KeyEvent &key,
                                          const commands::Input &option,
                                          commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::SEND_KEY);
  input.mutable_key()->CopyFrom(key);
  input.MergeFrom(option);
  return EvalCommand(&input, output);
}

bool TestSessionClient::TestSendKeyWithOption(const commands::KeyEvent &key,
                                              const commands::Input &option,
                                              commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::TEST_SEND_KEY);
  input.mutable_key()->CopyFrom(key);
  input.MergeFrom(option);
  return EvalCommand(&input, output);
}

bool TestSessionClient::SelectCandidate(uint32 id, commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::SEND_COMMAND);
  input.mutable_command()->set_type(commands::SessionCommand::SELECT_CANDIDATE);
  input.mutable_command()->set_id(id);
  return EvalCommand(&input, output);
}

bool TestSessionClient::SubmitCandidate(uint32 id, commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::SEND_COMMAND);
  input.mutable_command()->set_type(commands::SessionCommand::SUBMIT_CANDIDATE);
  input.mutable_command()->set_id(id);
  return EvalCommand(&input, output);
}

bool TestSessionClient::Reload() {
  commands::Input input;
  input.set_type(commands::Input::RELOAD);
  return EvalCommand(&input, nullptr);
}

bool TestSessionClient::ResetContext() {
  commands::Input input;
  input.set_type(commands::Input::SEND_COMMAND);
  input.mutable_command()->set_type(commands::SessionCommand::RESET_CONTEXT);
  return EvalCommand(&input, nullptr);
}

bool TestSessionClient::UndoOrRewind(commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::SEND_COMMAND);
  input.mutable_command()->set_type(commands::SessionCommand::UNDO_OR_REWIND);
  return EvalCommand(&input, output);
}

bool TestSessionClient::SwitchInputMode(
    commands::CompositionMode composition_mode) {
  commands::Input input;
  input.set_type(commands::Input::SEND_COMMAND);
  input.mutable_command()->set_type(
      commands::SessionCommand::SWITCH_INPUT_MODE);
  input.mutable_command()->set_composition_mode(composition_mode);
  return EvalCommand(&input, nullptr);
}

bool TestSessionClient::SetRequest(const commands::Request &request,
                                   commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::SET_REQUEST);
  input.mutable_request()->CopyFrom(request);
  return EvalCommand(&input, output);
}

bool TestSessionClient::SetConfig(const config::Config &config,
                                  commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::SET_CONFIG);
  *input.mutable_config() = config;
  return EvalCommand(&input, output);
}

void TestSessionClient::SetCallbackText(const std::string &text) {
  callback_text_ = text;
}

bool TestSessionClient::EvalCommandInternal(commands::Input *input,
                                            commands::Output *output,
                                            bool allow_callback) {
  input->set_id(id_);
  commands::Command command;
  command.mutable_input()->CopyFrom(*input);
  bool result = handler_->EvalCommand(&command);
  if (result && output != nullptr) {
    output->CopyFrom(command.output());
  }

  // If callback is allowed and the callback field exists, evaluate the callback
  // command.
  if (result && allow_callback && command.output().has_callback() &&
      command.output().callback().has_session_command()) {
    commands::Input input2;
    input2.set_type(commands::Input::SEND_COMMAND);
    input2.mutable_command()->CopyFrom(
        command.output().callback().session_command());
    input2.mutable_command()->set_text(callback_text_);
    // Disallow further recursion.
    result = EvalCommandInternal(&input2, output, false);
  }
  callback_text_.clear();
  return result;
}

bool TestSessionClient::EvalCommand(commands::Input *input,
                                    commands::Output *output) {
  return EvalCommandInternal(input, output, true);
}

}  // namespace testing
}  // namespace session
}  // namespace mozc
