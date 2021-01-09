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

#ifndef MOZC_CLIENT_CLIENT_MOCK_H_
#define MOZC_CLIENT_CLIENT_MOCK_H_

#include <map>
#include <string>
#include "base/mutex.h"
#include "client/client_interface.h"
#include "protocol/commands.pb.h"

namespace mozc {
namespace client {

class ClientMock : public client::ClientInterface {
 public:
  void SetIPCClientFactory(IPCClientFactoryInterface *client_factory) override;
  void SetServerLauncher(ServerLauncherInterface *server_launcher) override;
  bool IsValidRunLevel() const override;
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
  bool GetConfig(config::Config *config) override;
  bool SetConfig(const config::Config &config) override;
  bool ClearUserHistory() override;
  bool ClearUserPrediction() override;
  bool ClearUnusedUserPrediction() override;
  bool Shutdown() override;
  bool SyncData() override;
  bool Reload() override;
  bool Cleanup() override;
  void Reset() override;
  bool PingServer() const override;
  bool NoOperation() override;
  void EnableCascadingWindow(bool enable) override;
  void set_timeout(int timeout) override;
  void set_restricted(bool restricted) override;
  void set_server_program(const std::string &program_path) override;
  void set_suppress_error_dialog(bool suppress) override;
  void set_client_capability(const commands::Capability &capability) override;
  bool LaunchTool(const std::string &mode,
                  const std::string &extra_arg) override;
  bool LaunchToolWithProtoBuf(const commands::Output &output) override;
  bool OpenBrowser(const std::string &url) override;

  void ClearFunctionCounter();
  void SetBoolFunctionReturn(std::string func_name, bool value);
  int GetFunctionCallCount(std::string key);

#define TEST_METHODS(method_name, arg_type)                                 \
 private:                                                                   \
  arg_type called_##method_name##_;                                         \
                                                                            \
 public:                                                                    \
  arg_type called_##method_name() const { return called_##method_name##_; } \
  void set_output_##method_name(const commands::Output &output) {           \
    outputs_[#method_name].CopyFrom(output);                                \
  }
  TEST_METHODS(SendKeyWithContext, commands::KeyEvent);
  TEST_METHODS(TestSendKeyWithContext, commands::KeyEvent);
  TEST_METHODS(SendCommandWithContext, commands::SessionCommand);
#undef TEST_METHODS

 private:
  // Counter increments each time the function called.  This method is
  // marked as 'mutable' because it has to accumulate the counter even
  // with const methods.
  mutable std::map<std::string, int> function_counter_;

  // Stores return values when corresponding function is called.
  std::map<std::string, bool> return_bool_values_;

  std::map<std::string, commands::Output> outputs_;

  config::Config called_config_;

  // ClientMock is called from a thread in SessionWatchDog, and
  // SessionWatchDogTest. So a mutex lock is required.
  mutable Mutex mutex_;
};
}  // namespace client
}  // namespace mozc
#endif  // MOZC_CLIENT_CLIENT_MOCK_H_
