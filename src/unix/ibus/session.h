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

#ifndef MOZC_UNIX_IBUS_SESSION_H_
#define MOZC_UNIX_IBUS_SESSION_H_

#include <string>
#include <vector>
#include "base/port.h"
#include "base/scoped_ptr.h"
#include "session/commands.pb.h"
#include "session/session_factory.h"
#include "client/session_interface.h"

namespace mozc {

namespace config {
class Config;
}  // namespace config

namespace ibus {

class StandaloneSessionHandler;

// Implements SessionInterface for ibus-mozc.
// NOTE: ibus-mozc is directlly linked to the Mozc server comoponents so it is
// not ncessary to implement SessionInterface.
// However we will implment this interface so that we can change the
// implementation easily to another implementation with some IPC layer.
// Since some functions (such as EnsureConnection, CheckVersionOrRestartServer
// and PingServer) assume that this module is separated from the server
// components, thier implementations are empty.
class Session : public client::SessionInterface {
 public:
  Session();
  virtual ~Session();

  // Dose nothing and always returns true.
  virtual bool IsValidRunLevel() const;

  // Does nothing.
  virtual bool EnsureConnection();

  // Returns true if session id is valid.
  // If session id is invalid, re-issue a valid sssion id.
  virtual bool EnsureSession();

  // Does nothing.
  virtual bool CheckVersionOrRestartServer();

  // SendKey/TestSendKey/SendCommand automatically issue a session id if a valid
  // session id is not found.
  virtual bool SendKey(const commands::KeyEvent &key,
                       commands::Output *output);
  virtual bool TestSendKey(const commands::KeyEvent &key,
                           commands::Output *output);
  virtual bool SendCommand(const commands::SessionCommand &command,
                           commands::Output *output);

  virtual bool GetConfig(config::Config *config);
  virtual bool SetConfig(const config::Config &config);

  virtual bool ClearUserHistory();
  virtual bool ClearUserPrediction();
  virtual bool ClearUnusedUserPrediction();
  virtual bool Shutdown();
  virtual bool SyncData();
  virtual bool Reload();
  virtual bool Cleanup();
  virtual void Reset();

  // Does nothing.
  virtual bool PingServer() const;

  virtual bool NoOperation();

  // Enables or disables using cascading window.
  virtual void EnableCascadingWindow(bool enable);

  // Does nothing.
  virtual void set_timeout(int timeout);

  // Does nothing.
  virtual void set_restricted(bool restricted);

  // Does nothing.
  virtual void set_server_program(const string &program_path);

  // Remember the client capability information.
  virtual void set_client_capability(const commands::Capability &capability);

  // Does nothing.
  virtual bool LaunchTool(const string &mode,
                          const string &extra_arg);

  // Does nothing.
  virtual bool OpenBrowser(const string &url);

  static void SetSessionFactory(session::SessionFactoryInterface *new_factory);

 private:
  bool CreateSession();
  bool DeleteSession();
  bool CallCommand(commands::Input::CommandType type);

  // This method automatically re-issue session id if it is not available.
  bool EnsureCallCommand(commands::Input *input,
                         commands::Output *output);

  // The most primitive Call method.
  bool Call(const commands::Input &input,
            commands::Output *output);

  uint64 id_;

  StandaloneSessionHandler *handler_;
  commands::Capability client_capability_;
};

}  // namespace ibus
}  // namespace mozc

#endif  // MOZC_UNIX_IBUS_SESSION_H_
