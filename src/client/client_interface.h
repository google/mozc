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

// An abstract interface for the client class.

#ifndef MOZC_CLIENT_CLIENT_INTERFACE_H_
#define MOZC_CLIENT_CLIENT_INTERFACE_H_

#include <cstdint>
#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "base/strings/zstring_view.h"
#include "ipc/ipc.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"

namespace mozc {

namespace client {
class ClientInterface;

class ServerLauncherInterface {
 public:
  enum ServerErrorType {
    SERVER_TIMEOUT,
    SERVER_BROKEN_MESSAGE,
    SERVER_VERSION_MISMATCH,
    SERVER_SHUTDOWN,
    SERVER_FATAL,
  };

  virtual ~ServerLauncherInterface() = default;

  virtual bool StartServer(ClientInterface *client) = 0;

  // terminate the server.
  // You should not call this method unless protocol version mismatch happens.
  virtual bool ForceTerminateServer(absl::string_view name) = 0;

  // Wait server until it terminates
  virtual bool WaitServer(uint32_t pid) = 0;

  // called when fatal error occurred.
  virtual void OnFatal(ServerErrorType type) = 0;

  // set the full path of server program.
  virtual void set_server_program(absl::string_view server_program) = 0;

  // return the full path of server program
  // This is used for making IPC connection.
  virtual zstring_view server_program() const = 0;

  // launch with restricted mode
  virtual void set_restricted(bool restricted) = 0;

  // Sets the flag of error dialog suppression.
  virtual void set_suppress_error_dialog(bool suppress) = 0;
};

class ClientInterface {
 public:
  virtual ~ClientInterface() = default;

  // NOTE: Client class does NOT take the ownership of client_factory
  virtual void SetIPCClientFactory(
      IPCClientFactoryInterface *client_factory) = 0;

  // set ServerLauncher.
  // ServerLauncher is used as default.
  // NOTE: Client class takes the ownership of start_server_handler.
  virtual void SetServerLauncher(
      std::unique_ptr<ServerLauncherInterface> server_launcher) = 0;

  // return true if the current thread is running
  // inside valid run level.
  virtual bool IsValidRunLevel() const = 0;

  // Returns true if connection is alive.
  // If connection is not available,  re-launch mozc_server internally.
  virtual bool EnsureConnection() = 0;

  // Returns true if session id is valid.
  // if session id is invalid, re-issue a valid sssion id.
  virtual bool EnsureSession() = 0;

  // Checks protocol/product version.
  // If a  new version is available, restart the server.
  // return true the server is available.
  // return false some error happened during the server restart.
  // This method calls EnsureConnection automatically.
  virtual bool CheckVersionOrRestartServer() = 0;

  // SendKey/TestSendKey/SendCommand automatically
  // make a connection and issue an session id
  // if valid session id is not found.
  bool SendKey(const commands::KeyEvent &key, commands::Output *output) {
    return SendKeyWithContext(key, commands::Context::default_instance(),
                              output);
  }

  bool TestSendKey(const commands::KeyEvent &key, commands::Output *output) {
    return TestSendKeyWithContext(key, commands::Context::default_instance(),
                                  output);
  }

  bool SendCommand(const commands::SessionCommand &command,
                   commands::Output *output) {
    return SendCommandWithContext(
        command, commands::Context::default_instance(), output);
  }

  virtual bool SendKeyWithContext(const commands::KeyEvent &key,
                                  const commands::Context &context,
                                  commands::Output *output) = 0;
  virtual bool TestSendKeyWithContext(const commands::KeyEvent &key,
                                      const commands::Context &context,
                                      commands::Output *output) = 0;
  virtual bool SendCommandWithContext(const commands::SessionCommand &command,
                                      const commands::Context &context,
                                      commands::Output *output) = 0;

  // The methods below don't call
  // StartServer even if server is not available. This treatment
  // avoids unexceptional and continuous server restart trials.
  // If you really want to ensure the connection,
  // call EnsureConnection() in advance.

  // Return true if the key is consumed in the direct mode.
  virtual bool IsDirectModeCommand(const commands::KeyEvent &key) const = 0;

  // Get/Set config data
  virtual bool GetConfig(config::Config *config) = 0;
  virtual bool SetConfig(const config::Config &config) = 0;

  // Clear history data
  virtual bool ClearUserHistory() = 0;
  virtual bool ClearUserPrediction() = 0;
  virtual bool ClearUnusedUserPrediction() = 0;

  // Shutdonw server
  virtual bool Shutdown() = 0;

  // Sync server data, e.g., (prediction data) into disk
  virtual bool SyncData() = 0;

  // Reload server data, e.g., (user dictionary, prediction data)
  virtual bool Reload() = 0;

  // Cleanup un-used sessions
  virtual bool Cleanup() = 0;

  // Resets internal state (changes the state to be SERVER_UNKNOWN)
  virtual void Reset() = 0;

  // Returns true if server is alive.
  // This method will never change the internal state.
  virtual bool PingServer() const = 0;

  // This method is similar to PingServer(), but the internal
  // state may change. In almost all cases, you don't need to
  // call this method.
  virtual bool NoOperation() = 0;

  // Enables or disables using cascading window.
  virtual void EnableCascadingWindow(bool enable) = 0;

  // Sets the time out in milli second used for the IPC connection.
  virtual void set_timeout(absl::Duration timeout) = 0;

  // Sets restricted mode.
  // server is launched inside restricted environment.
  virtual void set_restricted(bool restricted) = 0;

  // Sets server program path.
  // mainly for unittesting.
  virtual void set_server_program(absl::string_view program_path) = 0;

  // Sets the flag of error dialog suppression.
  virtual void set_suppress_error_dialog(bool suppress) = 0;

  // Sets client capability.
  virtual void set_client_capability(
      const commands::Capability &capability) = 0;

  // Launches mozc tool. |mode| is the mode of MozcTool,
  // e,g,. "config_dialog", "dictionary_tool".
  virtual bool LaunchTool(const std::string &mode,
                          absl::string_view extra_arg) = 0;
  // Launches mozc_tool with output message.
  // If launch_tool_mode has no value or is set as NO_TOOL, this function will
  // do nothing and return false.
  virtual bool LaunchToolWithProtoBuf(const commands::Output &output) = 0;

  // Launches browser and pass |url|
  virtual bool OpenBrowser(const std::string &url) = 0;
};

class ClientFactoryInterface {
 public:
  virtual ~ClientFactoryInterface() = default;
  virtual std::unique_ptr<ClientInterface> NewClient() = 0;
};

class SendCommandInterface {
 public:
  virtual ~SendCommandInterface() = default;
  virtual bool SendCommand(const commands::SessionCommand &command,
                           commands::Output *output) = 0;
};
}  // namespace client
}  // namespace mozc
#endif  // MOZC_CLIENT_CLIENT_INTERFACE_H_
