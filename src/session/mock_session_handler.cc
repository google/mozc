// Copyright 2010-2011, Google Inc.
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

// Mocked Session Handler of Mozc server.
#include "session/mock_session_handler.h"

#include "base/base.h"
#include "session/commands.pb.h"
#include "session/session.h"
#include "session/session_observer_handler.h"

namespace mozc {

MockSessionHandler::MockSessionHandler() {
  // everything is OK
  is_available_ = true;
}

MockSessionHandler::~MockSessionHandler() {}

bool MockSessionHandler::IsAvailable() const {
  return is_available_;
}

bool MockSessionHandler::StartWatchDog() {
  // ignore
  return false;
}

void MockSessionHandler::AddObserver(
    session::SessionObserverInterface *observer) {
  // ignore
  return;
}

bool MockSessionHandler::EvalCommand(commands::Command *command) {
  if (!is_available_) {
    return false;
  }

  bool eval_succeeded = false;

  switch (command->input().type()) {
    case commands::Input::CREATE_SESSION:
      eval_succeeded = CreateSession(command);
      break;
    case commands::Input::SEND_KEY:
      eval_succeeded = SendKey(command);
      break;
    case commands::Input::NO_OPERATION:
      eval_succeeded = NoOperation(command);
      break;
    default:
      eval_succeeded = false;
  }

  if (eval_succeeded) {
    command->mutable_output()->set_error_code(
        commands::Output::SESSION_SUCCESS);
  } else {
    command->mutable_output()->set_id(0);
    command->mutable_output()->set_error_code(
        commands::Output::SESSION_FAILURE);
  }

  return is_available_;
}

bool MockSessionHandler::CreateSession(commands::Command *command) {
  command->mutable_output()->set_id(1);
  return true;
}

bool MockSessionHandler::SendKey(commands::Command *command) {
  const SessionID id = command->input().id();
  command->mutable_output()->set_id(id);
  command->mutable_output()->set_consumed(true);
  command->mutable_output()->set_mode(commands::DIRECT);

  // always return "あ"
  FillConversionResult(
      " ", "\xe3\x81\x82", command->mutable_output()->mutable_result());
  return true;
}

// simpified version of the one on session_output.cc (no normalization)
void MockSessionHandler::FillConversionResult(const string &key,
                                              const string &result,
                                              commands::Result *result_proto) {
  result_proto->set_type(commands::Result::STRING);
  result_proto->set_key(key);
  result_proto->set_value(result);
}

bool MockSessionHandler::NoOperation(commands::Command *command) {
  const SessionID id = command->input().id();
  command->mutable_output()->set_id(id);
  return true;
}

}  // namespace mozc
