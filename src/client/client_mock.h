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

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "client/client_interface.h"
#include "ipc/ipc.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "testing/gmock.h"

namespace mozc {
namespace client {

class ClientMock : public client::ClientInterface {
 public:
  MOCK_METHOD(void, SetIPCClientFactory,
              (IPCClientFactoryInterface * client_factory), (override));
  MOCK_METHOD(void, SetServerLauncher,
              (std::unique_ptr<ServerLauncherInterface> server_launcher),
              (override));
  MOCK_METHOD(bool, IsValidRunLevel, (), (const, override));
  MOCK_METHOD(bool, EnsureConnection, (), (override));
  MOCK_METHOD(bool, EnsureSession, (), (override));
  MOCK_METHOD(bool, CheckVersionOrRestartServer, (), (override));
  MOCK_METHOD(bool, SendKeyWithContext,
              (const commands::KeyEvent &argument,
               const commands::Context &context, commands::Output *output),
              (override));
  MOCK_METHOD(bool, TestSendKeyWithContext,
              (const commands::KeyEvent &argument,
               const commands::Context &context, commands::Output *output),
              (override));
  MOCK_METHOD(bool, SendCommandWithContext,
              (const commands::SessionCommand &argument,
               const commands::Context &context, commands::Output *output),
              (override));

  MOCK_METHOD(bool, IsDirectModeCommand, (const commands::KeyEvent &key),
              (const, override));
  MOCK_METHOD(bool, GetConfig, (config::Config * config), (override));
  MOCK_METHOD(bool, SetConfig, (const config::Config &config), (override));
  MOCK_METHOD(bool, ClearUserHistory, (), (override));
  MOCK_METHOD(bool, ClearUserPrediction, (), (override));
  MOCK_METHOD(bool, ClearUnusedUserPrediction, (), (override));
  MOCK_METHOD(bool, Shutdown, (), (override));
  MOCK_METHOD(bool, SyncData, (), (override));
  MOCK_METHOD(bool, Reload, (), (override));
  MOCK_METHOD(bool, Cleanup, (), (override));
  MOCK_METHOD(void, Reset, (), (override));
  MOCK_METHOD(bool, PingServer, (), (const, override));
  MOCK_METHOD(bool, NoOperation, (), (override));
  MOCK_METHOD(void, EnableCascadingWindow, (bool enable), (override));
  MOCK_METHOD(void, set_timeout, (absl::Duration timeout), (override));
  MOCK_METHOD(void, set_server_program, (absl::string_view program_path),
              (override));
  MOCK_METHOD(void, set_suppress_error_dialog, (bool suppress), (override));
  MOCK_METHOD(void, set_client_capability,
              (const commands::Capability &capability), (override));
  MOCK_METHOD(bool, LaunchTool,
              (const std::string &mode, absl::string_view extra_arg),
              (override));
  MOCK_METHOD(bool, LaunchToolWithProtoBuf, (const commands::Output &output),
              (override));
  MOCK_METHOD(bool, OpenBrowser, (const std::string &url), (override));
};
}  // namespace client
}  // namespace mozc
#endif  // MOZC_CLIENT_CLIENT_MOCK_H_
