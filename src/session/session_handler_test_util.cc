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

#include "session/session_handler_test_util.h"

#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "engine/engine_factory.h"
#include "engine/engine_interface.h"
#include "session/commands.pb.h"
#include "session/session_factory_manager.h"
#include "session/session_handler.h"
#include "session/session_handler_interface.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace session {
namespace testing {

using commands::Command;
using config::ConfigHandler;

bool CreateSession(SessionHandlerInterface *handler, uint64 *id) {
  Command command;
  command.mutable_input()->set_type(commands::Input::CREATE_SESSION);
  handler->EvalCommand(&command);
  if (id != NULL) {
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

bool CleanUp(SessionHandlerInterface *handler) {
  Command command;
  command.mutable_input()->set_type(commands::Input::CLEANUP);
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

JapaneseSessionHandlerTestBase::JapaneseSessionHandlerTestBase() {
}
JapaneseSessionHandlerTestBase::~JapaneseSessionHandlerTestBase() {
}

void JapaneseSessionHandlerTestBase::SetUp() {
  user_profile_directory_backup_ = Util::GetUserProfileDirectory();
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

  ConfigHandler::GetConfig(&config_backup_);
  config::Config config;
  ConfigHandler::GetDefaultConfig(&config);
  ConfigHandler::SetConfig(config);

  // Store the origianl to restore it in TearDown.
  session_factory_backup_ = SessionFactoryManager::GetSessionFactory();

  ResetEngine(EngineFactory::Create());
}

void JapaneseSessionHandlerTestBase::TearDown() {
  SessionFactoryManager::SetSessionFactory(session_factory_backup_);
  ConfigHandler::SetConfig(config_backup_);
  Util::SetUserProfileDirectory(user_profile_directory_backup_);
}

void JapaneseSessionHandlerTestBase::ResetEngine(EngineInterface *engine) {
  engine_.reset(engine);
  session_factory_.reset(new JapaneseSessionFactory(engine_.get()));
  SessionFactoryManager::SetSessionFactory(session_factory_.get());
}

TestSessionClient::TestSessionClient() : id_(0), handler_(new SessionHandler) {
}
TestSessionClient::~TestSessionClient() {
}

bool TestSessionClient::CreateSession() {
  return ::mozc::session::testing::CreateSession(handler_.get(), &id_);
}

bool TestSessionClient::DeleteSession() {
  return ::mozc::session::testing::DeleteSession(handler_.get(), id_);
}

bool TestSessionClient::CleanUp() {
  return ::mozc::session::testing::CleanUp(handler_.get());
}

bool TestSessionClient::SendKey(const commands::KeyEvent &key,
                                commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::SEND_KEY);
  input.mutable_key()->CopyFrom(key);
  return EvalCommand(&input, output);
}

bool TestSessionClient::TestSendKey(const commands::KeyEvent &key,
                                    commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::TEST_SEND_KEY);
  input.mutable_key()->CopyFrom(key);
  return EvalCommand(&input, output);
}

bool TestSessionClient::ResetContext() {
  commands::Input input;
  input.set_type(commands::Input::SEND_COMMAND);
  input.mutable_command()->set_type(commands::SessionCommand::RESET_CONTEXT);
  return EvalCommand(&input, NULL);
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
  return EvalCommand(&input, NULL);
}

bool TestSessionClient::SetRequest(const commands::Request &request,
                                   commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::SET_REQUEST);
  input.mutable_request()->CopyFrom(request);
  return EvalCommand(&input, output);
}

bool TestSessionClient::EvalCommand(commands::Input *input,
                                    commands::Output *output) {
  input->set_id(id_);
  commands::Command command;
  command.mutable_input()->CopyFrom(*input);
  bool result = handler_->EvalCommand(&command);
  if (result && output != NULL) {
    output->CopyFrom(command.output());
  }
  return result;
}

}  // namespace testing
}  // namespace session
}  // namespace mozc
