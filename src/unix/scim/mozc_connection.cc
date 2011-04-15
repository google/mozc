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
    : translator_(new ScimKeyTranslator),
      preedit_method_(mozc::config::Config::ROMAN) {
  VLOG(1) << "MozcConnection is created";
  mozc::client::Session *session = new mozc::client::Session;
  session->SetServerLauncher(server_launcher);
  session->SetIPCClientFactory(client_factory);
  session_.reset(session);

  mozc::config::Config config;
  if (session_->EnsureConnection() &&
      session_->GetConfig(&config) && config.has_preedit_method()) {
    preedit_method_ = config.preedit_method();
  }
  VLOG(1)
      << "Current preedit method is "
      << (preedit_method_ == mozc::config::Config::ROMAN ? "Roman" : "Kana");
}

MozcConnection::~MozcConnection() {
  session_->SyncData();
  VLOG(1) << "MozcConnection is destroyed";
}

bool MozcConnection::TrySendKeyEvent(
    const scim::KeyEvent &key,
    mozc::commands::CompositionMode composition_mode,
    mozc::commands::Output *out,
    string *out_error) const {
  DCHECK(out);
  DCHECK(out_error);

  // Call EnsureConnection just in case MozcConnection::MozcConnection() fails
  // to establish the server connection.
  if (!session_->EnsureConnection()) {
    *out_error = "EnsureConnection failed";
    VLOG(1) << "EnsureConnection failed";
    return false;
  }

  mozc::commands::KeyEvent event;
  translator_->Translate(key, preedit_method_, &event);

  if ((composition_mode == mozc::commands::DIRECT) &&
      !mozc::config::ImeSwitchUtil::IsTurnOnInDirectMode(event)) {
    VLOG(1) << "In DIRECT mode. Not consumed.";
    return false;  // not consumed.
  }

  VLOG(1) << "TrySendKeyEvent: " << endl << event.DebugString();
  if (!session_->SendKey(event, out)) {
    *out_error = "SendKey failed";
    VLOG(1) << "ERROR";
    return false;
  }
  VLOG(1) << "OK: " << endl << out->DebugString();
  return true;
}

bool MozcConnection::TrySendClick(int32 unique_id,
                                  mozc::commands::Output *out,
                                  string *out_error) const {
  DCHECK(out);
  DCHECK(out_error);

  mozc::commands::SessionCommand command;
  translator_->TranslateClick(unique_id, &command);
  return TrySendCommandInternal(command, out, out_error);
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
  return TrySendCommandInternal(command, out, out_error);
}

bool MozcConnection::TrySendCommand(
    mozc::commands::SessionCommand::CommandType type,
    mozc::commands::Output *out,
    string *out_error) const {
  DCHECK(out);
  DCHECK(out_error);

  mozc::commands::SessionCommand command;
  command.set_type(type);
  return TrySendCommandInternal(command, out, out_error);
}

bool MozcConnection::TrySendCommandInternal(
    const mozc::commands::SessionCommand& command,
    mozc::commands::Output *out,
    string *out_error) const {
  VLOG(1) << "TrySendCommandInternal: " << endl << command.DebugString();
  if (!session_->SendCommand(command, out)) {
    *out_error = "SendCommand failed";
    VLOG(1) << "ERROR";
    return false;
  }
  VLOG(1) << "OK: " << endl << out->DebugString();
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
