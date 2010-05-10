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

// An abstract interface for the session class.

#ifndef MOZC_CLIENT_SESSION_INTERFACE_H_
#define MOZC_CLIENT_SESSION_INTERFACE_H_

namespace mozc {
namespace config {
class Config;
}
namespace commands {
class KeyEvent;
class Output;
class SessionCommand;
}

namespace client {

class SessionInterface;

class StartServerHandlerInterface {
 public:
  enum ServerErrorType {
    SERVER_TIMEOUT,
    SERVER_BROKEN_MESSAGE,
    SERVER_VERSION_MISMATCH,
    SERVER_SHUTDOWN,
    SERVER_FATAL,
  };

  // implement StartServer.
  // return true if server can launched sucesfully.
  virtual bool StartServer(SessionInterface *session) = 0;

  // terminate the server.
  // You should not call this method unless protocol version mismatch happens.
  virtual bool ForceTerminateServer(const string &name) = 0;

  // Wait server until it terminates
  virtual bool WaitServer(uint32 pid) = 0;

  // called when fatal error occured.
  virtual void OnFatal(ServerErrorType type) = 0;

  // return the full path of server program
  // This is used for making IPC connection.
  virtual const string &server_program() const = 0;

  // launch with restricted mode
  virtual void set_restricted(bool restricted) = 0;

  StartServerHandlerInterface() {}
  virtual ~StartServerHandlerInterface() {}
};

class SessionInterface {
 public:
  virtual ~SessionInterface() {}

  virtual bool EnsureSession() = 0;
  virtual bool EnsureConnection() = 0;

  virtual bool CheckVersionOrRestartServer() = 0;

  virtual bool SendKey(const commands::KeyEvent &key,
                       commands::Output *output) = 0;
  virtual bool TestSendKey(const commands::KeyEvent &key,
                           commands::Output *output) = 0;
  virtual bool SendCommand(const commands::SessionCommand &command,
                           commands::Output *output) = 0;
  virtual bool GetConfig(config::Config *config) = 0;
  virtual bool SetConfig(const config::Config &config) = 0;
  virtual bool ClearUserHistory() = 0;
  virtual bool ClearUserPrediction() = 0;
  virtual bool ClearUnusedUserPrediction() = 0;
  virtual bool Shutdown() = 0;
  virtual bool SyncData() = 0;
  virtual bool Reload() = 0;
  virtual bool Cleanup() = 0;
  virtual bool NoOperation() = 0;
  virtual bool PingServer() const = 0;
  virtual void Reset() = 0;
  virtual void EnableCascadingWindow(bool enable) = 0;
  virtual void set_timeout(int timeout) = 0;
};

class SendCommandInterface {
 public:
  virtual ~SendCommandInterface() {}
  virtual bool SendCommand(const commands::SessionCommand &command,
                           commands::Output *output) = 0;
};

}  // namespace client
}  // namespace mozc
#endif  // MOZC_CLIENT_SESSION_INTERFACE_H_
