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

#include "renderer/renderer_client.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/time/time.h"
#include "base/number_util.h"
#include "base/strings/zstring_view.h"
#include "base/version.h"
#include "ipc/ipc.h"
#include "protocol/commands.pb.h"
#include "protocol/renderer_command.pb.h"
#include "testing/gunit.h"

namespace mozc {
namespace renderer {
namespace {

std::string UpdateVersion(int diff) {
  std::vector<std::string> tokens =
      absl::StrSplit(Version::GetMozcVersion(), '.', absl::SkipEmpty());
  EXPECT_EQ(tokens.size(), 4);
  tokens[3] = absl::StrCat(NumberUtil::SimpleAtoi(tokens[3]) + diff);
  return absl::StrJoin(tokens, ".");
}

struct IPCClientParams {
  int counter = 0;
  bool connected = false;
  uint32_t server_protocol_version = IPC_PROTOCOL_VERSION;
  std::string server_product_version = Version::GetMozcVersion();
};

class TestIPCClient : public IPCClientInterface {
 public:
  explicit TestIPCClient(IPCClientParams& params) : params_(params) {}

  bool Connected() const override { return params_.connected; }

  uint32_t GetServerProtocolVersion() const override {
    return params_.server_protocol_version;
  }

  const std::string& GetServerProductVersion() const override {
    return params_.server_product_version;
  }

  uint32_t GetServerProcessId() const override { return 0; }

  // just count up how many times Call is called.
  bool Call(const std::string& request, std::string* response,
            absl::Duration timeout) override {
    ++params_.counter;
    return true;
  }

  IPCErrorType GetLastIPCError() const override { return IPC_NO_ERROR; }

 protected:
  IPCClientParams& params_;
};

class TestIPCClientFactory : public IPCClientFactoryInterface {
 public:
  explicit TestIPCClientFactory(IPCClientParams& client_params)
      : client_params_(client_params) {}

  std::unique_ptr<IPCClientInterface> NewClient(
      zstring_view name, zstring_view path_name) override {
    return std::make_unique<TestIPCClient>(client_params_);
  }

  std::unique_ptr<IPCClientInterface> NewClient(zstring_view name) override {
    return std::make_unique<TestIPCClient>(client_params_);
  }

 private:
  IPCClientParams& client_params_;
};

class TestRendererLauncher : public RendererLauncherInterface {
 public:
  TestRendererLauncher()
      : start_renderer_called_(false),
        force_terminate_renderer_called_(false),
        available_(false),
        can_connect_(false) {}

  // implement StartServer.
  // return true if server can launched successfully.
  void StartRenderer(
      const std::string& name, const std::string& renderer_path,
      bool disable_renderer_path_check,
      IPCClientFactoryInterface* ipc_client_factory_interface) override {
    start_renderer_called_ = true;
    LOG(INFO) << name << " " << renderer_path;
  }

  bool ForceTerminateRenderer(const std::string& name) override {
    force_terminate_renderer_called_ = true;
    return true;
  }

  void OnFatal(RendererErrorType type) override {
    LOG(ERROR) << static_cast<int>(type);
  }

  // return true if the renderer is running
  bool IsAvailable() const override { return available_; }

  // return true if client can make a IPC connection.
  bool CanConnect() const override { return can_connect_; }

  void SetPendingCommand(const commands::RendererCommand& command) override {
    set_pending_command_called_ = true;
  }

  void set_suppress_error_dialog(bool suppress) override {}

  void Reset() {
    start_renderer_called_ = false;
    force_terminate_renderer_called_ = false;
    available_ = false;
    can_connect_ = false;
    set_pending_command_called_ = false;
  }

  void set_available(bool available) { available_ = available; }

  void set_can_connect(bool can_connect) { can_connect_ = can_connect; }

  bool is_start_renderer_called() const { return start_renderer_called_; }

  bool is_force_terminate_renderer_called() const {
    return force_terminate_renderer_called_;
  }

  bool is_set_pending_command_called() const {
    return set_pending_command_called_;
  }

 private:
  bool start_renderer_called_;
  bool force_terminate_renderer_called_;
  bool available_;
  bool can_connect_;
  bool set_pending_command_called_;
};

class RendererClientTest : public ::testing::Test {
 protected:
  RendererClientTest() : factory_(client_params_) {}

  RendererClient NewClient() {
    RendererClient client;
    client.SetIPCClientFactory(&factory_);
    client.SetRendererLauncherInterface(&launcher_);
    return client;
  }

  void Reset() { client_params_.counter = 0; }

  IPCClientParams client_params_;
  TestIPCClientFactory factory_;
  TestRendererLauncher launcher_;
};

TEST_F(RendererClientTest, InvalidTest) {
  RendererClient client = NewClient();

  client.SetIPCClientFactory(nullptr);
  client.SetRendererLauncherInterface(nullptr);
  commands::RendererCommand command;

  // IPCClientFactory and Launcher must be set.
  EXPECT_FALSE(client.ExecCommand(command));
  EXPECT_FALSE(client.IsAvailable());
  EXPECT_FALSE(client.Activate());
}

TEST_F(RendererClientTest, InitialState) {
  RendererClient client = NewClient();
  EXPECT_FALSE(client.IsAvailable());
}

TEST_F(RendererClientTest, ActivateTest) {
  RendererClient client = NewClient();
  {
    launcher_.set_available(true);
    EXPECT_TRUE(client.IsAvailable());
    launcher_.set_available(false);
    EXPECT_FALSE(client.IsAvailable());
  }

  {
    // No connection may happen if can_connect is false
    launcher_.set_available(false);
    launcher_.set_can_connect(false);
    Reset();
    EXPECT_TRUE(client.Activate());
    EXPECT_EQ(client_params_.counter, 0);
  }

  {
    // No connection may happen if connected return false
    launcher_.set_available(false);
    launcher_.set_can_connect(true);
    client_params_.connected = false;
    Reset();
    EXPECT_TRUE(client.Activate());
    EXPECT_EQ(client_params_.counter, 0);
  }

  {
    // one IPC call happens
    launcher_.set_available(false);
    launcher_.set_can_connect(true);
    client_params_.connected = true;
    Reset();
    EXPECT_TRUE(client.Activate());
    EXPECT_EQ(client_params_.counter, 1);
  }

  {
    // once launcher is available, no IPC call happens
    // with Activate()
    launcher_.set_available(true);
    Reset();
    EXPECT_TRUE(client.Activate());
    EXPECT_TRUE(client.Activate());
    EXPECT_TRUE(client.Activate());
    EXPECT_EQ(client_params_.counter, 0);
  }
}

TEST_F(RendererClientTest, LaunchTest) {
  RendererClient client = NewClient();

  commands::RendererCommand command;
  command.mutable_output()->set_id(0);
  command.set_type(commands::RendererCommand::NOOP);

  {
    // if can connect is false,
    // renderer is not launched
    launcher_.Reset();
    launcher_.set_can_connect(false);
    client_params_.connected = false;
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_FALSE(launcher_.is_start_renderer_called());
  }

  {
    // if connection is not available, start renderer process
    launcher_.Reset();
    launcher_.set_can_connect(true);
    client_params_.connected = false;
    command.set_visible(true);
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(launcher_.is_start_renderer_called());
  }

  {
    // if connection is not available,
    // but the command type is HIDE, renderer is not launched
    launcher_.Reset();
    launcher_.set_can_connect(true);
    client_params_.connected = false;
    command.set_visible(false);
    command.set_type(commands::RendererCommand::UPDATE);
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_FALSE(launcher_.is_start_renderer_called());
  }

  {
    command.set_type(commands::RendererCommand::NOOP);
    // if every state is OK, renderer is not launched
    launcher_.Reset();
    launcher_.set_can_connect(true);
    client_params_.connected = true;
    command.set_visible(true);
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_FALSE(launcher_.is_start_renderer_called());
  }
}

TEST_F(RendererClientTest, ConnectionTest) {
  RendererClient client = NewClient();

  commands::RendererCommand command;
  command.set_type(commands::RendererCommand::NOOP);

  {
    launcher_.Reset();
    launcher_.set_can_connect(true);
    client_params_.connected = true;
    Reset();
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(client.ExecCommand(command));

    // IPC should be called three times
    EXPECT_EQ(client_params_.counter, 3);
  }

  {
    // launcher denies connection
    launcher_.Reset();
    launcher_.set_can_connect(false);
    client_params_.connected = true;
    Reset();
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_EQ(client_params_.counter, 0);
  }

  {
    // IPC connection is lost.
    launcher_.Reset();
    launcher_.set_can_connect(true);
    client_params_.connected = false;
    Reset();
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_EQ(client_params_.counter, 0);
  }
}

TEST_F(RendererClientTest, ShutdownTest) {
  RendererClient client = NewClient();

  {
    launcher_.Reset();
    launcher_.set_can_connect(true);
    client_params_.connected = true;
    Reset();

    // Shutdown with commands::RendererCommand::SHUTDOWN command
    EXPECT_TRUE(client.Shutdown(false));
    EXPECT_FALSE(launcher_.is_force_terminate_renderer_called());
    EXPECT_EQ(client_params_.counter, 1);
  }

  {
    launcher_.Reset();
    launcher_.set_can_connect(true);
    client_params_.connected = true;
    Reset();

    // Shutdown with ForceTerminateRenderer
    EXPECT_TRUE(client.Shutdown(true));
    EXPECT_TRUE(launcher_.is_force_terminate_renderer_called());
    EXPECT_EQ(client_params_.counter, 0);
  }

  {
    launcher_.Reset();
    launcher_.set_can_connect(false);
    client_params_.connected = false;
    Reset();

    EXPECT_TRUE(client.Shutdown(false));
    EXPECT_FALSE(launcher_.is_force_terminate_renderer_called());
    EXPECT_EQ(client_params_.counter, 0);
  }

  {
    launcher_.Reset();
    launcher_.set_can_connect(false);
    client_params_.connected = false;
    Reset();

    EXPECT_TRUE(client.Shutdown(true));
    EXPECT_FALSE(launcher_.is_force_terminate_renderer_called());
    EXPECT_EQ(client_params_.counter, 0);
  }
}

TEST_F(RendererClientTest, ProtocolVersionMismatchNewer) {
  RendererClient client = NewClient();

  commands::RendererCommand command;
  command.set_type(commands::RendererCommand::NOOP);

  {
    launcher_.Reset();
    launcher_.set_can_connect(true);
    client_params_.connected = true;
    Reset();
    client_params_.server_protocol_version = IPC_PROTOCOL_VERSION - 1;
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(launcher_.is_force_terminate_renderer_called());
    EXPECT_EQ(client_params_.counter, 0);
  }
}

TEST_F(RendererClientTest, ProtocolVersionMismatchOlder) {
  RendererClient client = NewClient();

  commands::RendererCommand command;
  command.set_type(commands::RendererCommand::NOOP);

  {
    launcher_.Reset();
    launcher_.set_can_connect(true);
    client_params_.connected = true;
    Reset();
    client_params_.server_protocol_version = IPC_PROTOCOL_VERSION + 1;
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_FALSE(launcher_.is_force_terminate_renderer_called());
    EXPECT_EQ(client_params_.counter, 0);
  }
}

TEST_F(RendererClientTest, MozcVersionMismatchNewer) {
  RendererClient client = NewClient();

  commands::RendererCommand command;
  command.set_type(commands::RendererCommand::NOOP);
  client_params_.server_product_version = UpdateVersion(-1);

  {
    launcher_.Reset();
    launcher_.set_can_connect(true);
    client_params_.connected = true;
    Reset();
    client_params_.server_protocol_version = IPC_PROTOCOL_VERSION;
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_FALSE(launcher_.is_force_terminate_renderer_called());
    EXPECT_EQ(client_params_.counter, 1);
  }
}

TEST_F(RendererClientTest, MozcVersionMismatchOlder) {
  RendererClient client = NewClient();

  commands::RendererCommand command;
  command.set_type(commands::RendererCommand::NOOP);
  client_params_.server_product_version = UpdateVersion(1);

  {
    launcher_.Reset();
    launcher_.set_can_connect(true);
    client_params_.connected = true;
    Reset();
    client_params_.server_protocol_version = IPC_PROTOCOL_VERSION;
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_FALSE(launcher_.is_force_terminate_renderer_called());
    EXPECT_EQ(client_params_.counter, 1);
  }
}

TEST_F(RendererClientTest, SetPendingCommandTest) {
  RendererClient client = NewClient();

  commands::RendererCommand command;
  command.set_type(commands::RendererCommand::NOOP);

  {
    launcher_.Reset();
    launcher_.set_can_connect(true);
    client_params_.connected = false;
    command.set_visible(true);
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(launcher_.is_start_renderer_called());
    EXPECT_TRUE(launcher_.is_set_pending_command_called());
  }

  {
    launcher_.Reset();
    launcher_.set_can_connect(false);
    client_params_.connected = false;
    command.set_visible(true);
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(launcher_.is_set_pending_command_called());
  }

  {
    launcher_.Reset();
    launcher_.set_can_connect(true);
    client_params_.connected = true;
    command.set_visible(true);
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_FALSE(launcher_.is_set_pending_command_called());
  }
}

}  // namespace
}  // namespace renderer
}  // namespace mozc
