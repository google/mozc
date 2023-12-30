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

#include "renderer/renderer_server.h"

#include <memory>
#include <string>

#include "protocol/renderer_command.pb.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/logging.h"
#include "ipc/ipc_test_util.h"
#include "renderer/renderer_client.h"
#include "renderer/renderer_interface.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace renderer {
namespace {

class TestRenderer : public RendererInterface {
 public:
  TestRenderer() : counter_(0), finished_(false) {}

  bool Activate() override { return true; }

  bool IsAvailable() const override { return true; }

  bool ExecCommand(const commands::RendererCommand &command) override {
    if (finished_) {
      return false;
    }
    counter_++;
    return true;
  }

  void Reset() { counter_ = 0; }

  int counter() const { return counter_; }

  void Shutdown() { finished_ = true; }

 private:
  int counter_;
  bool finished_;
};

class TestRendererServer : public RendererServer {
 public:
  int StartMessageLoop() override { return 0; }

  // Not async for testing
  bool AsyncExecCommand(absl::string_view proto_message) override {
    commands::RendererCommand command;
    command.ParseFromArray(proto_message.data(), proto_message.size());
    return ExecCommandInternal(command);
  }
};

// A renderer launcher which does nothing.
class DummyRendererLauncher : public RendererLauncherInterface {
 public:
  void StartRenderer(
      const std::string &name, const std::string &renderer_path,
      bool disable_renderer_path_check,
      IPCClientFactoryInterface *ipc_client_factory_interface) override {
    LOG(INFO) << name << " " << renderer_path;
  }

  bool ForceTerminateRenderer(const std::string &name) override { return true; }

  void OnFatal(RendererErrorType type) override {
    LOG(ERROR) << static_cast<int>(type);
  }

  bool IsAvailable() const override { return true; }

  bool CanConnect() const override { return true; }

  void SetPendingCommand(const commands::RendererCommand &command) override {}

  void set_suppress_error_dialog(bool suppress) override {}
};

class RendererServerTest : public testing::TestWithTempUserProfile {};

TEST_F(RendererServerTest, IPCTest) {
  mozc::IPCClientFactoryOnMemory on_memory_client_factory;

  std::unique_ptr<TestRendererServer> server(new TestRendererServer);
  TestRenderer renderer;
  server->SetRendererInterface(&renderer);
#ifdef __APPLE__
  server->SetMachPortManager(on_memory_client_factory.OnMemoryPortManager());
#endif  // __APPLE__
  renderer.Reset();

  // listning event
  server->StartServer();
  absl::SleepFor(absl::Seconds(1));

  DummyRendererLauncher launcher;
  RendererClient client;
  client.SetIPCClientFactory(&on_memory_client_factory);
  client.DisableRendererServerCheck();
  client.SetRendererLauncherInterface(&launcher);
  commands::RendererCommand command;
  command.set_type(commands::RendererCommand::NOOP);

  // renderer is called via IPC
  client.ExecCommand(command);
  EXPECT_EQ(renderer.counter(), 1);

  client.ExecCommand(command);
  client.ExecCommand(command);
  client.ExecCommand(command);
  EXPECT_EQ(renderer.counter(), 4);

  // Gracefully shutdown the server.
  renderer.Shutdown();
  client.ExecCommand(command);
  server->Wait();
}

}  // namespace
}  // namespace renderer
}  // namespace mozc
