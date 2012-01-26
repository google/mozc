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

#include <string>
#include "base/base.h"
#include "base/util.h"
#include "ipc/ipc.h"
#include "ipc/ipc_test_util.h"
#include "languages/global_language_spec.h"
#include "languages/japanese/lang_dep_spec.h"
#include "renderer/renderer_interface.h"
#include "renderer/renderer_client.h"
#include "renderer/renderer_server.h"
#include "session/commands.pb.h"
#include "testing/base/public/gunit.h"

DECLARE_int32(timeout);
DECLARE_string(test_tmpdir);

namespace mozc {
namespace renderer {
namespace {

class TestRenderer : public RendererInterface {
 public:
  TestRenderer() : counter_(0), finished_(false) {}

  bool Activate() { return true; }

  bool IsAvailable() const { return true; }

  bool ExecCommand(const commands::RendererCommand &command) {
    if (finished_) {
      return false;
    }
    counter_++;
    return true;
  }

  void Reset() {
    counter_ = 0;
  }

  int counter() const {
    return counter_;
  }

  void Shutdown() {
    finished_ = true;
  }

 private:
  int counter_;
  bool finished_;
};

class TestRendererServer : public RendererServer {
 public:
  TestRendererServer() {}

  virtual ~TestRendererServer() {}

  int StartMessageLoop() {
    return 0;
  }

  // Not async for testing
  bool AsyncExecCommand(string *proto_message) {
    commands::RendererCommand command;
    command.ParseFromString(*proto_message);
    delete proto_message;
    return ExecCommandInternal(command);
  }
};

// A renderer launcher which does nothing.
class DummyRendererLauncher : public RendererLauncherInterface {
 public:
  void StartRenderer(const string &name,
                     const string &renderer_path,
                     bool disable_renderer_path_check,
                     IPCClientFactoryInterface *ipc_client_factory_interface) {
    LOG(INFO) << name << " " << renderer_path;
  }

  bool ForceTerminateRenderer(const string &name) {
    return true;
  }

  void OnFatal(RendererErrorType type) {
    LOG(ERROR) << static_cast<int>(type);
  }

  virtual bool IsAvailable() const {
    return true;
  }

  virtual bool CanConnect() const {
    return true;
  }

  virtual void SetPendingCommand(const commands::RendererCommand &command) {
  }
};
}  // namespace

class RendererServerTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    mozc::Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    mozc::language::GlobalLanguageSpec::SetLanguageDependentSpec(
        &lang_dep_spec_);
  }

  virtual void TearDown() {
    mozc::language::GlobalLanguageSpec::SetLanguageDependentSpec(NULL);
  }

 private:
  mozc::japanese::LangDepSpecJapanese lang_dep_spec_;
};

TEST_F(RendererServerTest, IPCTest) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  mozc::IPCClientFactoryOnMemory on_memory_client_factory;

  scoped_ptr<TestRendererServer> server(new TestRendererServer);
  TestRenderer renderer;
  server->SetRendererInterface(&renderer);
#ifdef OS_MACOSX
  server->SetMachPortManager(on_memory_client_factory.OnMemoryPortManager());
#endif
  renderer.Reset();

  // listning event
  server->StartServer();
  Util::Sleep(1000);

  DummyRendererLauncher launcher;
  RendererClient client;
  client.SetIPCClientFactory(&on_memory_client_factory);
  client.DisableRendererServerCheck();
  client.SetRendererLauncherInterface(&launcher);
  commands::RendererCommand command;
  command.set_type(commands::RendererCommand::NOOP);

  // renderer is called via IPC
  client.ExecCommand(command);
  EXPECT_EQ(1, renderer.counter());

  client.ExecCommand(command);
  client.ExecCommand(command);
  client.ExecCommand(command);
  EXPECT_EQ(4, renderer.counter());

  // Gracefully shutdown the server.
  renderer.Shutdown();
  client.ExecCommand(command);
  server->Wait();
}

} // renderer
} // mozc
