// Copyright 2010-2014, Google Inc.
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

#include <string>

#include "base/logging.h"
#include "base/number_util.h"
#include "base/port.h"
#include "base/util.h"
#include "base/version.h"
#include "ipc/ipc.h"
#include "renderer/renderer_command.pb.h"
#include "renderer/renderer_interface.h"
#include "testing/base/public/gunit.h"

namespace mozc {

namespace renderer {

namespace {
const string UpdateVersion(int diff) {
  vector<string> tokens;
  Util::SplitStringUsing(Version::GetMozcVersion(), ".", &tokens);
  EXPECT_EQ(tokens.size(), 4);
  char buf[64];
  snprintf(buf, sizeof(buf), "%d", NumberUtil::SimpleAtoi(tokens[3]) + diff);
  tokens[3] = buf;
  string output;
  Util::JoinStrings(tokens, ".", &output);
  return output;
}

int g_counter = 0;
bool g_connected = false;
uint32 g_server_protocol_version = IPC_PROTOCOL_VERSION;
string g_server_product_version;

class TestIPCClient : public IPCClientInterface {
 public:
  TestIPCClient() {
    g_server_product_version = Version::GetMozcVersion();
  }
  ~TestIPCClient() {}

  bool Connected() const {
    return g_connected;
  }

  uint32 GetServerProtocolVersion() const {
    return g_server_protocol_version;
  }

  const string &GetServerProductVersion() const {
    return g_server_product_version;
  }


  uint32 GetServerProcessId() const {
    return 0;
  }

  // just count up how many times Call is called.
  virtual bool Call(const char *request,
                    size_t request_size,
                    char *response,
                    size_t *response_size,
                    int32 timeout) {
    g_counter++;
    return true;
  }

  static void set_connected(bool connected) {
    g_connected = connected;
  }

  static void Reset() {
    g_counter = 0;
  }

  static int counter() {
    return g_counter;
  }

  static void set_server_protocol_version(uint32 version) {
    g_server_protocol_version = version;
  }

  virtual IPCErrorType GetLastIPCError() const {
    return IPC_NO_ERROR;
  }
};

class TestIPCClientFactory : public IPCClientFactoryInterface {
 public:
  TestIPCClientFactory() {}
  ~TestIPCClientFactory() {}

  virtual IPCClientInterface *NewClient(const string &name,
                                        const string &path_name) {
    return new TestIPCClient;
  }

  virtual IPCClientInterface *NewClient(const string &name) {
    return new TestIPCClient;
  }
};

class TestRendererLauncher : public RendererLauncherInterface {
 public:
  TestRendererLauncher()
      : start_renderer_called_(false),
        force_terminate_renderer_called_(false),
        available_(false), can_connect_(false) {}
  ~TestRendererLauncher() {}

  // implement StartServer.
  // return true if server can launched successfully.
  void StartRenderer(const string &name,
                     const string &renderer_path,
                     bool disable_renderer_path_check,
                     IPCClientFactoryInterface *ipc_client_factory_interface) {
    start_renderer_called_ = true;
    LOG(INFO) << name << " " << renderer_path;
  }

  bool ForceTerminateRenderer(const string &name) {
    force_terminate_renderer_called_ = true;
    return true;
  }

  void OnFatal(RendererErrorType type) {
    LOG(ERROR) << static_cast<int>(type);
  }

  // return true if the renderer is running
  virtual bool IsAvailable() const {
    return available_;
  }

  // return true if client can make a IPC connection.
  virtual bool CanConnect() const {
    return can_connect_;
  }

  virtual void SetPendingCommand(
      const commands::RendererCommand &command) {
    set_pending_command_called_ = true;
  }

  virtual void set_suppress_error_dialog(bool suppress) {
  }

  void Reset() {
    start_renderer_called_ = false;
    force_terminate_renderer_called_ = false;
    available_ = false;
    can_connect_ = false;
    set_pending_command_called_ = false;
  }

  void set_available(bool available) {
    available_ = available;
  }

  void set_can_connect(bool can_connect) {
    can_connect_ = can_connect;
  }

  bool is_start_renderer_called() const {
    return start_renderer_called_;
  }

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
}  // namespace

TEST(RendererClient, InvalidTest) {
  RendererClient client;

  client.SetIPCClientFactory(NULL);
  client.SetRendererLauncherInterface(NULL);
  commands::RendererCommand command;

  // IPCClientFactory and Launcher must be set.
  EXPECT_FALSE(client.ExecCommand(command));
  EXPECT_FALSE(client.IsAvailable());
  EXPECT_FALSE(client.Activate());
}

TEST(RendererClient, ActivateTest) {
  TestIPCClientFactory factory;
  TestRendererLauncher launcher;

  RendererClient client;

  client.SetIPCClientFactory(&factory);
  client.SetRendererLauncherInterface(&launcher);

  {
    launcher.set_available(true);
    EXPECT_TRUE(client.IsAvailable());
    launcher.set_available(false);
    EXPECT_FALSE(client.IsAvailable());
  }

  {
    // No connection may happen if can_connect is false
    launcher.set_available(false);
    launcher.set_can_connect(false);
    TestIPCClient::Reset();
    EXPECT_TRUE(client.Activate());
    EXPECT_EQ(0, TestIPCClient::counter());
  }

  {
    // No connection may happen if connected return false
    launcher.set_available(false);
    launcher.set_can_connect(true);
    TestIPCClient::set_connected(false);
    TestIPCClient::Reset();
    EXPECT_TRUE(client.Activate());
    EXPECT_EQ(0, TestIPCClient::counter());
  }

  {
    // one IPC call happens
    launcher.set_available(false);
    launcher.set_can_connect(true);
    TestIPCClient::set_connected(true);
    TestIPCClient::Reset();
    EXPECT_TRUE(client.Activate());
    EXPECT_EQ(1, TestIPCClient::counter());
  }

  {
    // once launcher is available, no IPC call happens
    // with Activate()
    launcher.set_available(true);
    TestIPCClient::Reset();
    EXPECT_TRUE(client.Activate());
    EXPECT_TRUE(client.Activate());
    EXPECT_TRUE(client.Activate());
    EXPECT_EQ(0, TestIPCClient::counter());
  }
}

TEST(RendererClient, LaunchTest) {
  TestIPCClientFactory factory;
  TestRendererLauncher launcher;

  RendererClient client;

  client.SetIPCClientFactory(&factory);
  client.SetRendererLauncherInterface(&launcher);

  commands::RendererCommand command;
  command.mutable_output()->set_id(0);
  command.set_type(commands::RendererCommand::NOOP);

  {
    // if can connect is false,
    // renderer is not launched
    launcher.Reset();
    launcher.set_can_connect(false);
    TestIPCClient::set_connected(false);
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_FALSE(launcher.is_start_renderer_called());
  }

  {
    // if connection is not available, start renderer process
    launcher.Reset();
    launcher.set_can_connect(true);
    TestIPCClient::set_connected(false);
    command.set_visible(true);
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(launcher.is_start_renderer_called());
  }

  {
    // if connection is not available,
    // but the command type is HIDE, renderer is not launched
    launcher.Reset();
    launcher.set_can_connect(true);
    TestIPCClient::set_connected(false);
    command.set_visible(false);
    command.set_type(commands::RendererCommand::UPDATE);
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_FALSE(launcher.is_start_renderer_called());
  }

  {
    command.set_type(commands::RendererCommand::NOOP);
    // if every state is OK, renderer is not launched
    launcher.Reset();
    launcher.set_can_connect(true);
    TestIPCClient::set_connected(true);
    command.set_visible(true);
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_FALSE(launcher.is_start_renderer_called());
  }
}

TEST(RendererClient, ConnectionTest) {
  TestIPCClientFactory factory;
  TestRendererLauncher launcher;

  RendererClient client;

  client.SetIPCClientFactory(&factory);
  client.SetRendererLauncherInterface(&launcher);

  commands::RendererCommand command;
  command.set_type(commands::RendererCommand::NOOP);

  {
    launcher.Reset();
    launcher.set_can_connect(true);
    TestIPCClient::set_connected(true);
    TestIPCClient::Reset();
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(client.ExecCommand(command));

    // IPC should be called three times
    EXPECT_EQ(3, TestIPCClient::counter());
  }

  {
    // launcher denies connection
    launcher.Reset();
    launcher.set_can_connect(false);
    TestIPCClient::set_connected(true);
    TestIPCClient::Reset();
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_EQ(0, TestIPCClient::counter());
  }

  {
    // IPC connection is lost.
    launcher.Reset();
    launcher.set_can_connect(true);
    TestIPCClient::set_connected(false);
    TestIPCClient::Reset();
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_EQ(0, TestIPCClient::counter());
  }
}

TEST(RendererClient, ShutdownTest) {
  TestIPCClientFactory factory;
  TestRendererLauncher launcher;

  RendererClient client;

  client.SetIPCClientFactory(&factory);
  client.SetRendererLauncherInterface(&launcher);

  {
    launcher.Reset();
    launcher.set_can_connect(true);
    TestIPCClient::set_connected(true);
    TestIPCClient::Reset();

    // Shutdown with commands::RendererCommand::SHUTDOWN command
    EXPECT_TRUE(client.Shutdown(false));
    EXPECT_FALSE(launcher.is_force_terminate_renderer_called());
    EXPECT_EQ(1, TestIPCClient::counter());
  }

  {
    launcher.Reset();
    launcher.set_can_connect(true);
    TestIPCClient::set_connected(true);
    TestIPCClient::Reset();

    // Shutdown with ForceTerminateRenderer
    EXPECT_TRUE(client.Shutdown(true));
    EXPECT_TRUE(launcher.is_force_terminate_renderer_called());
    EXPECT_EQ(0, TestIPCClient::counter());
  }

  {
    launcher.Reset();
    launcher.set_can_connect(false);
    TestIPCClient::set_connected(false);
    TestIPCClient::Reset();

    EXPECT_TRUE(client.Shutdown(false));
    EXPECT_FALSE(launcher.is_force_terminate_renderer_called());
    EXPECT_EQ(0, TestIPCClient::counter());
  }

  {
    launcher.Reset();
    launcher.set_can_connect(false);
    TestIPCClient::set_connected(false);
    TestIPCClient::Reset();

    EXPECT_TRUE(client.Shutdown(true));
    EXPECT_FALSE(launcher.is_force_terminate_renderer_called());
    EXPECT_EQ(0, TestIPCClient::counter());
  }
}

TEST(RendererClient, ProtocolVersionMismatchNewer) {
  TestIPCClientFactory factory;
  TestRendererLauncher launcher;

  RendererClient client;

  client.SetIPCClientFactory(&factory);
  client.SetRendererLauncherInterface(&launcher);

  commands::RendererCommand command;
  command.set_type(commands::RendererCommand::NOOP);

  {
    launcher.Reset();
    launcher.set_can_connect(true);
    TestIPCClient::set_connected(true);
    TestIPCClient::Reset();
    TestIPCClient::set_server_protocol_version(IPC_PROTOCOL_VERSION - 1);
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(launcher.is_force_terminate_renderer_called());
    EXPECT_EQ(0, TestIPCClient::counter());
  }
}

TEST(RendererClient, ProtocolVersionMismatchOlder) {
  TestIPCClientFactory factory;
  TestRendererLauncher launcher;

  RendererClient client;

  client.SetIPCClientFactory(&factory);
  client.SetRendererLauncherInterface(&launcher);

  commands::RendererCommand command;
  command.set_type(commands::RendererCommand::NOOP);

  {
    launcher.Reset();
    launcher.set_can_connect(true);
    TestIPCClient::set_connected(true);
    TestIPCClient::Reset();
    TestIPCClient::set_server_protocol_version(IPC_PROTOCOL_VERSION + 1);
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_FALSE(launcher.is_force_terminate_renderer_called());
    EXPECT_EQ(0, TestIPCClient::counter());
  }
}

TEST(RendererClient, MozcVersionMismatchNewer) {
  TestIPCClientFactory factory;
  TestRendererLauncher launcher;

  RendererClient client;

  client.SetIPCClientFactory(&factory);
  client.SetRendererLauncherInterface(&launcher);

  commands::RendererCommand command;
  command.set_type(commands::RendererCommand::NOOP);
  g_server_product_version = UpdateVersion(-1);

  {
    launcher.Reset();
    launcher.set_can_connect(true);
    TestIPCClient::set_connected(true);
    TestIPCClient::Reset();
    TestIPCClient::set_server_protocol_version(IPC_PROTOCOL_VERSION);
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_FALSE(launcher.is_force_terminate_renderer_called());
    EXPECT_EQ(1, TestIPCClient::counter());
  }
}

TEST(RendererClient, MozcVersionMismatchOlder) {
  TestIPCClientFactory factory;
  TestRendererLauncher launcher;

  RendererClient client;

  client.SetIPCClientFactory(&factory);
  client.SetRendererLauncherInterface(&launcher);

  commands::RendererCommand command;
  command.set_type(commands::RendererCommand::NOOP);
  g_server_product_version = UpdateVersion(1);

  {
    launcher.Reset();
    launcher.set_can_connect(true);
    TestIPCClient::set_connected(true);
    TestIPCClient::Reset();
    TestIPCClient::set_server_protocol_version(IPC_PROTOCOL_VERSION);
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_FALSE(launcher.is_force_terminate_renderer_called());
    EXPECT_EQ(1, TestIPCClient::counter());
  }
}

TEST(RendererClient, SetPendingCommandTest) {
  TestIPCClientFactory factory;
  TestRendererLauncher launcher;

  RendererClient client;

  client.SetIPCClientFactory(&factory);
  client.SetRendererLauncherInterface(&launcher);

  commands::RendererCommand command;
  command.set_type(commands::RendererCommand::NOOP);

  {
    launcher.Reset();
    launcher.set_can_connect(true);
    TestIPCClient::set_connected(false);
    command.set_visible(true);
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(launcher.is_start_renderer_called());
    EXPECT_TRUE(launcher.is_set_pending_command_called());
  }

  {
    launcher.Reset();
    launcher.set_can_connect(false);
    TestIPCClient::set_connected(false);
    command.set_visible(true);
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_TRUE(launcher.is_set_pending_command_called());
  }

  {
    launcher.Reset();
    launcher.set_can_connect(true);
    TestIPCClient::set_connected(true);
    command.set_visible(true);
    EXPECT_TRUE(client.ExecCommand(command));
    EXPECT_FALSE(launcher.is_set_pending_command_called());
  }
}
}  // namespace renderer
}  // namespace mozc
