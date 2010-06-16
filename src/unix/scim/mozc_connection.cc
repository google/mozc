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

#include "unix/scim/mozc_connection.h"

#include <string>

#include "base/logging.h"
#include "base/util.h"
#include "client/session.h"
#include "ipc/ipc.h"
#include "session/commands.pb.h"
#include "session/ime_switch_util.h"
#include "unix/scim/scim_key_translator.h"

namespace mozc_unix_scim {

MozcConnectionInterface::~MozcConnectionInterface() {
}

MozcConnection::MozcConnection(
    mozc::client::ServerLauncherInterface *server_launcher,
    mozc::IPCClientFactoryInterface *client_factory)
    : translator_(new ScimKeyTranslator) {
  mozc::client::Session *session = new mozc::client::Session;
  session->SetServerLauncher(server_launcher);
  session->SetIPCClientFactory(client_factory);
  session_.reset(session);
}

MozcConnection::~MozcConnection() {
  session_->Shutdown();
}

bool MozcConnection::TrySendKeyEvent(
    const scim::KeyEvent &key,
    mozc::commands::CompositionMode composition_mode,
    mozc::commands::Output *out,
    string *out_error) const {
  DCHECK(out);
  DCHECK(out_error);

  mozc::config::Config config;
  if (!session_->EnsureConnection()) {
    *out_error = "IPC error";
    VLOG(1) << "TrySendKeyEvent: EnsureConnection failed";
    return false;
  }

  // TODO(yusukes): GetConfig() call could be very heavy. It would be better to
  // remove the call. See ibus/mozc_engine.cc for details.
  if (!session_->GetConfig(&config)) {
    *out_error = "IPC error";
    VLOG(1) << "TrySendKeyEvent: GetConfig failed";
    return false;
  }

  mozc::commands::KeyEvent event;
  const mozc::config::Config::PreeditMethod method =
      config.has_preedit_method() ?
      config.preedit_method() : mozc::config::Config::ROMAN;
  translator_->Translate(key, method, &event);

  if ((composition_mode == mozc::commands::DIRECT) &&
      !mozc::config::ImeSwitchUtil::IsTurnOnInDirectMode(event)) {
    VLOG(1) << "In DIRECT mode. Not consumed.";
    return false;  // not consumed.
  }

  VLOG(1) << "TrySendInternal: --->" << endl << event.DebugString();
  if (!session_->SendKey(event, out)) {
    *out_error = "IPC error";
    VLOG(1) << "TrySendInternal: <--- ERROR";
    return false;
  }
  VLOG(1) << "TrySendInternal: <---" << endl << out->DebugString();
  return true;
}

bool MozcConnection::TrySendClick(int32 unique_id,
                                  mozc::commands::Output *out,
                                  string *out_error) const {
  DCHECK(out);
  DCHECK(out_error);

  mozc::commands::SessionCommand command;
  translator_->TranslateClick(unique_id, &command);
  VLOG(1) << "TrySendInternal: --->" << endl << command.DebugString();
  if (!session_->SendCommand(command, out)) {
    *out_error = "IPC error";
    VLOG(1) << "TrySendInternal: <--- ERROR";
    return false;
  }
  VLOG(1) << "TrySendInternal: <---" << endl << out->DebugString();
  return true;
}

bool MozcConnection::TrySendCompositionMode(
    mozc::commands::CompositionMode mode,
    mozc::commands::Output *out,
    string *out_error) const {
  DCHECK(out);
  DCHECK(out_error);

  mozc::commands::SessionCommand command;
  command.set_type(mozc::commands::SessionCommand::SWITCH_INPUT_MODE);
  command.set_composition_mode(mode);

  VLOG(1) << "TrySendInternal: --->" << endl << command.DebugString();
  if (!session_->SendCommand(command, out)) {
    *out_error = "IPC error";
    VLOG(1) << "TrySendInternal: <--- ERROR";
    return false;
  }
  VLOG(1) << "TrySendInternal: <---" << endl << out->DebugString();
  return true;
}

bool MozcConnection::TrySendSubmit(mozc::commands::Output *out,
                                   string *out_error) const {
  DCHECK(out);
  DCHECK(out_error);

  mozc::commands::SessionCommand command;
  command.set_type(mozc::commands::SessionCommand::SUBMIT);

  VLOG(1) << "TrySendInternal: --->" << endl << command.DebugString();
  if (!session_->SendCommand(command, out)) {
    *out_error = "IPC error";
    VLOG(1) << "TrySendInternal: <--- ERROR";
    return false;
  }
  VLOG(1) << "TrySendInternal: <---" << endl << out->DebugString();
  return true;
}

bool MozcConnection::CanSend(const scim::KeyEvent &key) const {
  return translator_->CanConvert(key);
}

MozcConnection *MozcConnection::CreateMozcConnection() {
  mozc::client::ServerLauncher *server_launcher
      = new mozc::client::ServerLauncher;

  return new MozcConnection(server_launcher, new mozc::IPCClientFactory);
}

}  // namespace mozc_unix_scim
