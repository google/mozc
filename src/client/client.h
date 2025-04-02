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

// A class handling the IPC connection for the session b/w server and clients.

#ifndef MOZC_CLIENT_CLIENT_H_
#define MOZC_CLIENT_CLIENT_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "base/run_level.h"
#include "base/strings/assign.h"
#include "base/strings/zstring_view.h"
#include "client/client_interface.h"
#include "composer/key_event_util.h"
#include "ipc/ipc.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "testing/friend_test.h"

namespace mozc {
namespace client {

// default ServerLauncher implementation.
// This class uses fork&exec (linux/mac) and CreateProcess() (Windows)
// to launch server process
class ServerLauncher : public ServerLauncherInterface {
 public:
  ServerLauncher();
  ~ServerLauncher() override;

  bool StartServer(ClientInterface *client) override;

  bool ForceTerminateServer(absl::string_view name) override;

  bool WaitServer(uint32_t pid) override;

  void OnFatal(ServerLauncherInterface::ServerErrorType type) override;

  // specify server program. On Mac, we need to specify the server path
  // using this method.
  void set_server_program(const absl::string_view server_program) override {
    strings::Assign(server_program_, server_program);
  }

  // return server program
  zstring_view server_program() const override { return server_program_; }

  void set_restricted(bool restricted) override { restricted_ = restricted; }

  // Sets the flag of error dialog suppression.
  void set_suppress_error_dialog(bool suppress) override {
    suppress_error_dialog_ = suppress;
  }

 private:
  std::string server_program_;
  bool restricted_;
  bool suppress_error_dialog_;
};

class Client : public ClientInterface {
 public:
  Client();
  ~Client() override;

  // Initializes `request_` with the flag.
  // This function should be called before EnsureSession.
  void InitRequestForSvsJapanese(bool use_svs);

  void SetIPCClientFactory(IPCClientFactoryInterface *client_factory) override {
    client_factory_ = client_factory;
  }

  // set ServerLauncher.
  // ServerLauncher is used as default
  // NOTE: Client class takes the owership of start_server_handler.
  void SetServerLauncher(
      std::unique_ptr<ServerLauncherInterface> server_launcher) override {
    server_launcher_ = std::move(server_launcher);
  }

  bool IsValidRunLevel() const override {
    return RunLevel::IsValidClientRunLevel();
  }

  bool EnsureConnection() override;

  bool EnsureSession() override;

  bool CheckVersionOrRestartServer() override;

  bool SendKeyWithContext(const commands::KeyEvent &key,
                          const commands::Context &context,
                          commands::Output *output) override;
  bool TestSendKeyWithContext(const commands::KeyEvent &key,
                              const commands::Context &context,
                              commands::Output *output) override;
  bool SendCommandWithContext(const commands::SessionCommand &command,
                              const commands::Context &context,
                              commands::Output *output) override;

  bool IsDirectModeCommand(const commands::KeyEvent &key) const override;

  bool GetConfig(config::Config *config) override;
  bool SetConfig(const config::Config &config) override;

  bool ClearUserHistory() override;
  bool ClearUserPrediction() override;
  bool ClearUnusedUserPrediction() override;
  bool Shutdown() override;
  bool SyncData() override;
  bool Reload() override;
  bool Cleanup() override;

  bool NoOperation() override;
  bool PingServer() const override;

  void Reset() override;

  void EnableCascadingWindow(bool enable) override;

  void set_timeout(absl::Duration timeout) override;
  void set_restricted(bool restricted) override;
  void set_server_program(absl::string_view program_path) override;
  void set_suppress_error_dialog(bool suppress) override;
  void set_client_capability(const commands::Capability &capability) override;

  bool LaunchTool(const std::string &mode, absl::string_view arg) override;
  bool LaunchToolWithProtoBuf(const commands::Output &output) override;
  // Converts Output message from server to corresponding mozc_tool arguments
  // If launch_tool_mode is not set or NO_TOOL is set or an invalid value is
  // set, this function will return false and do nothing.
  static bool TranslateProtoBufToMozcToolArg(const commands::Output &output,
                                             std::string *mode);

  bool OpenBrowser(const std::string &url) override;

 private:
  FRIEND_TEST(SessionPlaybackTest, PushAndResetHistoryWithNoModeTest);
  FRIEND_TEST(SessionPlaybackTest, PushAndResetHistoryWithModeTest);
  FRIEND_TEST(SessionPlaybackTest, PushAndResetHistoryWithDirectTest);
  FRIEND_TEST(SessionPlaybackTest, PlaybackHistoryTest);
  FRIEND_TEST(SessionPlaybackTest, SetModeInitializerTest);
  FRIEND_TEST(SessionPlaybackTest, ConsumedTest);

  enum ServerStatus {
    SERVER_UNKNOWN,           // initial status
    SERVER_SHUTDOWN,          // server is currently not working
    SERVER_INVALID_SESSION,   // current session is not available
    SERVER_OK,                // both server and session are health
    SERVER_TIMEOUT,           // server is blocked
    SERVER_VERSION_MISMATCH,  // server version is different
    SERVER_BROKEN_MESSAGE,    // server's message is broken
    SERVER_FATAL              // cannot start server (binary is broken/missing)
  };

  // Dump the recent user inputs to specified file with label
  // This is used for debugging
  void DumpHistorySnapshot(absl::string_view filename,
                           absl::string_view label) const;

  // Start server:
  // * Return true if server is launched successfully or server is already
  //   running.
  // * Return false if server cannot be launched.
  // If server_program is empty, which is default setting, the path to
  // GoogleJapaneseInputConverter is determined automatically.
  // Windows: "C:\Program Files\Google\Google Japanese Input\"
  // Linux/Mac: searching from default path
  bool StartServer();

  // Displays a message box to notify the user of fatal error.
  void OnFatal(ServerLauncherInterface::ServerErrorType type);

  // Initialize input filling id and preferences.
  void InitInput(commands::Input *input) const;

  bool CreateSession();
  bool DeleteSession();
  bool CallCommand(commands::Input::CommandType type);

  // This method automatically re-launch mozc_server and
  // re-issue session id if it is not available.
  bool EnsureCallCommand(commands::Input *input, commands::Output *output);

  // The most primitive Call method
  // This method won't change the server_status_ even
  // when version mismatch happens. In this case,
  // just return false.
  bool Call(const commands::Input &input, commands::Output *output);

  // first invoke Call() command and check the
  // protocol_version. When protocol version mismatch,
  // client goes to FATAL state
  bool CallAndCheckVersion(const commands::Input &input,
                           commands::Output *output);

  // Making a journal inputs to restore
  // the current state even when mozc_server crashes
  void PlaybackHistory();
  void PushHistory(const commands::Input &input,
                   const commands::Output &output);
  void ResetHistory();

  // The alias of
  // DumpHistorySnapshot("query_of_death.log", "QUERY OF DEATH");
  // and history_inputs_.clear();
  void DumpQueryOfDeath();

  // Execute |input| and check the version by seeing the
  // initial response. If a new version is available, automatically
  // restart the server and execute the same input command again.
  // If any errors happen inside the version up, shows an error dialog
  // and returns false.
  bool CheckVersionOrRestartServerInternal(const commands::Input &input,
                                           commands::Output *output);

  // for unittest
  // copy the history inputs to |result|.
  void GetHistoryInputs(std::vector<commands::Input> *result) const;

  uint64_t id_;
  IPCClientFactoryInterface *client_factory_;
  std::unique_ptr<ServerLauncherInterface> server_launcher_;
  std::unique_ptr<config::Config> preferences_;
  std::unique_ptr<commands::Request> request_;
  std::string response_;
  absl::Duration timeout_;
  ServerStatus server_status_;
  uint32_t server_protocol_version_;
  uint32_t server_process_id_;
  std::string server_product_version_;
  std::vector<commands::Input> history_inputs_;
  // List of key combinations used in the direct input mode.
  std::vector<KeyInformation> direct_mode_keys_;
  // Remember the composition mode of input session for playback.
  commands::CompositionMode last_mode_;
  commands::Capability client_capability_;
};

class ClientFactory {
 public:
  ClientFactory() = delete;
  ~ClientFactory() = delete;

  // Return a new client.
  static std::unique_ptr<ClientInterface> NewClient();

  // Set a ClientFactoryInterface for unit testing.
  static void SetClientFactory(ClientFactoryInterface *client_factory);
};

}  // namespace client
}  // namespace mozc
#endif  // MOZC_CLIENT_CLIENT_H_
