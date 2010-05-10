// Copyright 2010, Google Inc.
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

#include "unix/ibus/session.h"

#include "base/base.h"
#include "session/commands.pb.h"
#include "session/session_handler.h"

namespace mozc {
namespace ibus {

Session::Session()
    : id_(0),
      handler_(new SessionHandler) {
}

Session::~Session() {
  DeleteSession();
}

bool Session::EnsureSession() {
  if (id_ != 0) {
    VLOG(1) << "session has been already created.";
    return true;
  }
  return CreateSession();
}

bool Session::EnsureConnection() {
  return true;
}

bool Session::SendKey(const commands::KeyEvent &key,
                      commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::SEND_KEY);
  input.mutable_key()->CopyFrom(key);
  return EnsureCallCommand(&input, output);
}

bool Session::TestSendKey(const commands::KeyEvent &key,
                          commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::TEST_SEND_KEY);
  input.mutable_key()->CopyFrom(key);
  return EnsureCallCommand(&input, output);
}

bool Session::SendCommand(const commands::SessionCommand &command,
                          commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::SEND_COMMAND);
  input.mutable_command()->CopyFrom(command);
  return EnsureCallCommand(&input, output);
}

bool Session::CheckVersionOrRestartServer() {
  return true;
}

bool Session::EnsureCallCommand(commands::Input *input,
                                commands::Output *output) {
  if (!EnsureSession()) {
    LOG(ERROR) << "EnsureSession failed";
    return false;
  }

  input->set_id(id_);
  output->set_id(0);
  return Call(*input, output);
}

void Session::EnableCascadingWindow(bool enable) {
  // TODO(mazda): implement this
}

void Session::set_timeout(int timeout) {
}

bool Session::CreateSession() {
  if (id_ != 0) {
    VLOG(1) << "session has been already created.";
    return true;
  }

  commands::Command command;
  command.mutable_input()->set_type(commands::Input::CREATE_SESSION);
  if (!handler_->EvalCommand(&command)) {
    LOG(ERROR) << "EvalCommand failed";
    return false;
  }
  id_ = command.output().id();
  return true;
}

bool Session::DeleteSession() {
  // No need to delete session
  if (id_ == 0) {
    VLOG(1) << "session has been already deleted.";
    return true;
  }

  commands::Command command;
  command.mutable_input()->set_id(id_);
  command.mutable_input()->set_type(commands::Input::DELETE_SESSION);
  bool result = handler_->EvalCommand(&command);
  id_ = 0;
  return result;
}

bool Session::GetConfig(config::Config *config) {
  // TODO(mazda): Implement this
  return true;
}

bool Session::SetConfig(const config::Config &config) {
  // TODO(mazda): Implement this
  return true;
}

bool Session::ClearUserHistory() {
  return CallCommand(commands::Input::CLEAR_USER_HISTORY);
}

bool Session::ClearUserPrediction() {
  return CallCommand(commands::Input::CLEAR_USER_PREDICTION);
}

bool Session::ClearUnusedUserPrediction() {
  return CallCommand(commands::Input::CLEAR_UNUSED_USER_PREDICTION);
}

bool Session::Shutdown() {
  CallCommand(commands::Input::SHUTDOWN);
  return true;
}

bool Session::SyncData() {
  return CallCommand(commands::Input::SYNC_DATA);
}

bool Session::Reload() {
  return CallCommand(commands::Input::RELOAD);
}

bool Session::Cleanup() {
  return CallCommand(commands::Input::CLEANUP);
}

bool Session::NoOperation() {
  return CallCommand(commands::Input::NO_OPERATION);
}

bool Session::PingServer() const {
  return true;
}

bool Session::CallCommand(commands::Input::CommandType type) {
  commands::Input input;
  input.set_id(id_);
  input.set_type(type);
  commands::Output output;
  return Call(input, &output);
}

bool Session::Call(const commands::Input &input,
                   commands::Output *output) {
  DCHECK(output) << "output is null";

  commands::Command command;
  command.mutable_input()->CopyFrom(input);
  if (!handler_->EvalCommand(&command)) {
    LOG(ERROR) << "EvalCommand failed.";
    return false;
  }
  output->CopyFrom(command.output());
  return true;
}

void Session::Reset() {
  // TODO(mazda): Implement this
}

}  // namespace ibus
}  // namespace mozc
