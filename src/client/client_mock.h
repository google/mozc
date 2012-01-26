// Copyright 2010-2012, Google Inc.
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

#ifndef IME_MOZC_CLIENT_CLIENT_MOCK_H_
#define IME_MOZC_CLIENT_CLIENT_MOCK_H_

#include <map>
#include <string>
#include "client/client_interface.h"
#include "session/commands.pb.h"

namespace mozc {
namespace client {

class ClientMock : public client::ClientInterface {
 public:
  void SetIPCClientFactory(IPCClientFactoryInterface *client_factory);
  void SetServerLauncher(ServerLauncherInterface *server_launcher);
  bool IsValidRunLevel() const;
  bool EnsureConnection();
  bool EnsureSession();
  bool CheckVersionOrRestartServer();
  bool SendKey(const commands::KeyEvent &key,
               commands::Output *output);
  bool TestSendKey(const commands::KeyEvent &key,
                   commands::Output *output);
  bool SendCommand(const commands::SessionCommand &command,
                   commands::Output *output);
  bool GetConfig(config::Config *config);
  bool SetConfig(const config::Config &config);
  bool ClearUserHistory();
  bool ClearUserPrediction();
  bool ClearUnusedUserPrediction();
  bool Shutdown();
  bool SyncData();
  bool Reload();
  virtual bool Cleanup();
  virtual void Reset();
  bool PingServer() const;
  bool NoOperation();
  virtual void EnableCascadingWindow(bool enable);
  virtual void set_timeout(int timeout);
  virtual void set_restricted(bool restricted);
  virtual void set_server_program(const string &program_path);
  virtual void set_client_capability(const commands::Capability &capability);
  bool LaunchTool(const string &mode, const string &extra_arg);
  bool LaunchToolWithProtoBuf(const commands::Output &output);
  bool OpenBrowser(const string &url);

  void ClearFunctionCounter();
  void SetBoolFunctionReturn(string func_name, bool value);
  int GetFunctionCallCount(string key);

#define TEST_METHODS(method_name, arg_type)                             \
 private:                                                               \
  arg_type called_##method_name##_;                                     \
 public:                                                                \
  arg_type called_##method_name() const { return called_##method_name##_; } \
  void set_output_##method_name(const commands::Output &output) {       \
    outputs_[#method_name].CopyFrom(output);                            \
  }
  TEST_METHODS(SendKey, commands::KeyEvent);
  TEST_METHODS(TestSendKey, commands::KeyEvent);
  TEST_METHODS(SendCommand, commands::SessionCommand);
#undef TEST_METHODS

 private:
  // Counter increments each time the function called.  This method is
  // marked as 'mutable' because it has to accumulate the counter even
  // with const methods.
  mutable map<string, int> function_counter_;

  // Stores return values when corresponding function is called.
  map<string, bool> return_bool_values_;

  map<string, commands::Output> outputs_;

  config::Config called_config_;

};
}  // namespace ibus
}  // namespace mozc
#endif  // IME_MOZC_CLIENT_CLIENT_MOCK_H_
