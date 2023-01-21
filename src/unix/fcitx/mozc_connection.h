// Copyright 2010-2012, Google Inc.
// Copyright 2012~2013, Weng Xuetian <wengxt@gmail.com>
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

#ifndef MOZC_UNIX_FCITX_MOZC_CONNECTION_H_
#define MOZC_UNIX_FCITX_MOZC_CONNECTION_H_

#include <string>
#include <memory>

#include <fcitx-config/hotkey.h>
#include <fcitx/instance.h>

#include "base/port.h"
#include "protocol/commands.pb.h"
#include "unix/fcitx/fcitx_key_event_handler.h"

namespace mozc {

class IPCClientInterface;
class IPCClientFactoryInterface;

namespace client {
class ClientInterface;
class ServerLauncherInterface;
}  // namespace client

}  // namespace mozc

namespace mozc {
    
namespace fcitx {

class KeyTranslator;

// This class is for mozc_response_parser_test.cc.
class MozcConnectionInterface {
 public:
  virtual ~MozcConnectionInterface();

  virtual bool TrySendKeyEvent(FcitxInstance* instance,
                               FcitxKeySym sym, uint32 keycode, uint32 state,
                               mozc::commands::CompositionMode composition_mode,
                               bool layout_is_jp,
                               bool is_key_up,
                               mozc::commands::Output *out,
                               std::string *out_error) const = 0;
  virtual bool TrySendClick(int32 unique_id,
                            mozc::commands::Output *out,
                            std::string *out_error) const = 0;
  virtual bool TrySendCompositionMode(mozc::commands::CompositionMode mode,
                                      mozc::commands::CompositionMode old_mode,
                                      mozc::commands::Output *out,
                                      std::string *out_error) const = 0;
  virtual bool TrySendCommand(mozc::commands::SessionCommand::CommandType type,
                              mozc::commands::Output *out,
                              std::string *out_error) const = 0;

  virtual bool TrySendRawCommand(const mozc::commands::SessionCommand& command,
                                 mozc::commands::Output *out,
                                 std::string *out_error) const = 0;
  virtual mozc::client::ClientInterface* GetClient() = 0;
  virtual void UpdatePreeditMethod() = 0;
};

class MozcConnection : public MozcConnectionInterface {
 public:
  MozcConnection(const MozcConnection &) = delete;
  static const int kNoSession;

  static MozcConnection *CreateMozcConnection();
  virtual ~MozcConnection();

  // Sends key event to the server. If the IPC succeeds, returns true and the
  // response is stored on 'out' (and 'out_error' is not modified). If the IPC
  // fails, returns false and the error message is stored on 'out_error'. In
  // this case, 'out' is not modified.
  virtual bool TrySendKeyEvent(FcitxInstance* instance,
                               FcitxKeySym sym, uint32 keycode, uint32 state,
                               mozc::commands::CompositionMode composition_mode,
                               bool layout_is_jp,
                               bool is_key_up,
                               mozc::commands::Output *out,
                               std::string *out_error) const;

  // Sends 'mouse click on the candidate window' event to the server.
  virtual bool TrySendClick(int32 unique_id,
                            mozc::commands::Output *out,
                            std::string *out_error) const;

  // Sends composition mode to the server.
  virtual bool TrySendCompositionMode(mozc::commands::CompositionMode mode,
                                      mozc::commands::CompositionMode old_mode,
                                      mozc::commands::Output *out,
                                      std::string *out_error) const;

  // Sends a command to the server.
  virtual bool TrySendCommand(mozc::commands::SessionCommand::CommandType type,
                              mozc::commands::Output *out,
                              std::string *out_error) const;

  virtual bool TrySendRawCommand(const mozc::commands::SessionCommand& command,
                                 mozc::commands::Output *out,
                                 std::string *out_error) const;

  virtual mozc::client::ClientInterface* GetClient();

  virtual void UpdatePreeditMethod();

 private:
  friend class MozcConnectionTest;
  MozcConnection(mozc::client::ServerLauncherInterface *server_launcher,
                 mozc::IPCClientFactoryInterface *client_factory);

  const std::unique_ptr<KeyEventHandler> handler_;
  mozc::config::Config::PreeditMethod preedit_method_;
  // Keep definition order of client_factory_ and client_.
  // We should delete client_ before deleting client_factory_.
  std::unique_ptr<mozc::IPCClientFactoryInterface> client_factory_;
  std::unique_ptr<mozc::client::ClientInterface> client_;
};

}  // namespace fcitx

}  // namespace mozc

#endif  // MOZC_UNIX_SCIM_MOZC_CONNECTION_H_
