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

#include "client/client.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "base/number_util.h"
#include "base/strings/assign.h"
#include "base/strings/zstring_view.h"
#include "base/version.h"
#include "client/client_interface.h"
#include "composer/key_parser.h"
#include "config/config_handler.h"
#include "ipc/ipc.h"
#include "ipc/ipc_mock.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "testing/test_peer.h"

namespace mozc {
namespace client {

class ClientTestPeer : public testing::TestPeer<Client> {
 public:
  explicit ClientTestPeer(Client &client) : testing::TestPeer<Client>(client) {}

  PEER_METHOD(GetHistoryInputs);
};

namespace {

constexpr char kPrecedingText[] = "preceding_text";
constexpr char kFollowingText[] = "following_text";
constexpr bool kSuppressSuggestion = true;

std::string UpdateVersion(int diff) {
  std::vector<std::string> tokens =
      absl::StrSplit(Version::GetMozcVersion(), '.', absl::SkipEmpty());
  EXPECT_EQ(tokens.size(), 4);
  tokens[3] = absl::StrCat(NumberUtil::SimpleAtoi(tokens[3]) + diff);
  return absl::StrJoin(tokens, ".");
}

}  // namespace

class TestServerLauncher : public ServerLauncherInterface {
 public:
  explicit TestServerLauncher(IPCClientFactoryMock *factory)
      : factory_(factory),
        start_server_result_(false),
        start_server_called_(false),
        force_terminate_server_result_(false),
        force_terminate_server_called_(false),
        server_protocol_version_(IPC_PROTOCOL_VERSION) {}

  virtual void Ready() {}
  virtual void Wait() {}
  virtual void Error() {}

  bool StartServer(ClientInterface *client) override {
    if (!response_.empty()) {
      factory_->SetMockResponse(response_);
    }
    if (!product_version_after_start_server_.empty()) {
      factory_->SetServerProductVersion(product_version_after_start_server_);
    }
    factory_->SetServerProtocolVersion(server_protocol_version_);
    start_server_called_ = true;
    return start_server_result_;
  }

  bool ForceTerminateServer(const absl::string_view name) override {
    force_terminate_server_called_ = true;
    return force_terminate_server_result_;
  }

  bool WaitServer(uint32_t pid) override { return true; }

  void OnFatal(ServerLauncherInterface::ServerErrorType type) override {
    LOG(ERROR) << static_cast<int>(type);
    error_map_[static_cast<int>(type)]++;
  }

  int error_count(ServerLauncherInterface::ServerErrorType type) {
    return error_map_[static_cast<int>(type)];
  }

  bool start_server_called() const { return start_server_called_; }

  void set_start_server_called(bool start_server_called) {
    start_server_called_ = start_server_called;
  }

  bool force_terminate_server_called() const {
    return force_terminate_server_called_;
  }

  void set_force_terminate_server_called(bool force_terminate_server_called) {
    force_terminate_server_called_ = force_terminate_server_called;
  }

  void set_server_program(const absl::string_view server_path) override {}

  zstring_view server_program() const override {
    return placeholder_server_program_path_;
  }

  void set_suppress_error_dialog(bool suppress) override {}

  void set_start_server_result(const bool result) {
    start_server_result_ = result;
  }

  void set_force_terminate_server_result(const bool result) {
    force_terminate_server_result_ = result;
  }

  void set_server_protocol_version(uint32_t server_protocol_version) {
    server_protocol_version_ = server_protocol_version;
  }

  void set_mock_after_start_server(const commands::Output &mock_output) {
    mock_output.SerializeToString(&response_);
  }

  void set_product_version_after_start_server(const absl::string_view version) {
    strings::Assign(product_version_after_start_server_, version);
  }

 private:
  const std::string placeholder_server_program_path_;
  IPCClientFactoryMock *factory_;
  bool start_server_result_;
  bool start_server_called_;
  bool force_terminate_server_result_;
  bool force_terminate_server_called_;
  uint32_t server_protocol_version_;
  std::string response_;
  std::string product_version_after_start_server_;
  absl::flat_hash_map<int, int> error_map_;
};

class ClientTest : public testing::TestWithTempUserProfile {
 protected:
  ClientTest() : version_diff_(0) {}
  ClientTest(const ClientTest &) = delete;
  ClientTest &operator=(const ClientTest &) = delete;

  void SetUp() override {
    client_factory_ = std::make_unique<IPCClientFactoryMock>();
    client_ = std::make_unique<Client>();
    client_->SetIPCClientFactory(client_factory_.get());

    auto server_launcher =
        std::make_unique<TestServerLauncher>(client_factory_.get());
    server_launcher_ = server_launcher.get();
    client_->SetServerLauncher(std::move(server_launcher));
  }

  void TearDown() override {
    client_.reset();
    client_factory_.reset();
  }

  void SetMockOutput(const commands::Output &mock_output) {
    std::string response;
    mock_output.SerializeToString(&response);
    client_factory_->SetMockResponse(response);
  }

  void GetGeneratedInput(commands::Input *input) {
    input->ParseFromString(client_factory_->GetGeneratedRequest());
    if (input->type() != commands::Input::CREATE_SESSION) {
      ASSERT_TRUE(input->has_id());
    }
  }

  void SetupProductVersion(int version_diff) { version_diff_ = version_diff; }

  bool SetupConnection(const int id) {
    client_factory_->SetConnection(true);
    client_factory_->SetResult(true);
    if (version_diff_ == 0) {
      client_factory_->SetServerProductVersion(Version::GetMozcVersion());
    } else {
      client_factory_->SetServerProductVersion(UpdateVersion(version_diff_));
    }
    server_launcher_->set_start_server_result(true);

    // TODO(komatsu): Due to the limitation of the testing mock,
    // EnsureConnection should be explicitly called before calling
    // SendKey.  Fix the testing mock.
    commands::Output mock_output;
    mock_output.set_id(id);
    SetMockOutput(mock_output);
    return client_->EnsureConnection();
  }

  std::unique_ptr<IPCClientFactoryMock> client_factory_;
  std::unique_ptr<Client> client_;
  TestServerLauncher *server_launcher_;
  int version_diff_;
};

TEST_F(ClientTest, ConnectionError) {
  client_factory_->SetConnection(false);
  server_launcher_->set_start_server_result(false);
  EXPECT_FALSE(client_->EnsureConnection());

  commands::KeyEvent key;
  commands::Output output;
  EXPECT_FALSE(client_->SendKey(key, &output));

  key.Clear();
  output.Clear();
  EXPECT_FALSE(client_->TestSendKey(key, &output));

  commands::SessionCommand command;
  output.Clear();
  EXPECT_FALSE(client_->SendCommand(command, &output));
}

TEST_F(ClientTest, SendKey) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(output.consumed(), mock_output.consumed());

  commands::Input input;
  GetGeneratedInput(&input);
  EXPECT_EQ(input.id(), mock_id);
  EXPECT_EQ(input.type(), commands::Input::SEND_KEY);
}

TEST_F(ClientTest, SendKeyWithContext) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  commands::Context context;
  context.set_preceding_text(kPrecedingText);
  context.set_following_text(kFollowingText);
  context.set_suppress_suggestion(kSuppressSuggestion);

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(client_->SendKeyWithContext(key_event, context, &output));
  EXPECT_EQ(output.consumed(), mock_output.consumed());

  commands::Input input;
  GetGeneratedInput(&input);
  EXPECT_EQ(input.id(), mock_id);
  EXPECT_EQ(input.type(), commands::Input::SEND_KEY);
  EXPECT_EQ(input.context().preceding_text(), kPrecedingText);
  EXPECT_EQ(input.context().following_text(), kFollowingText);
  EXPECT_EQ(input.context().suppress_suggestion(), kSuppressSuggestion);
}

TEST_F(ClientTest, TestSendKey) {
  const int mock_id = 512;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  commands::Output mock_output;
  mock_output.Clear();
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(client_->TestSendKey(key_event, &output));
  EXPECT_EQ(output.consumed(), mock_output.consumed());

  commands::Input input;
  GetGeneratedInput(&input);
  EXPECT_EQ(input.id(), mock_id);
  EXPECT_EQ(input.type(), commands::Input::TEST_SEND_KEY);
}

TEST_F(ClientTest, TestSendKeyWithContext) {
  const int mock_id = 512;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  commands::Context context;
  context.set_preceding_text(kPrecedingText);
  context.set_following_text(kFollowingText);
  context.set_suppress_suggestion(kSuppressSuggestion);

  commands::Output mock_output;
  mock_output.Clear();
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(client_->TestSendKeyWithContext(key_event, context, &output));
  EXPECT_EQ(output.consumed(), mock_output.consumed());

  commands::Input input;
  GetGeneratedInput(&input);
  EXPECT_EQ(input.id(), mock_id);
  EXPECT_EQ(input.type(), commands::Input::TEST_SEND_KEY);
  EXPECT_EQ(input.context().preceding_text(), kPrecedingText);
  EXPECT_EQ(input.context().following_text(), kFollowingText);
  EXPECT_EQ(input.context().suppress_suggestion(), kSuppressSuggestion);
}

TEST_F(ClientTest, SendCommand) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::SessionCommand session_command;
  session_command.set_type(commands::SessionCommand::SUBMIT);

  commands::Output mock_output;
  mock_output.Clear();
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(client_->SendCommand(session_command, &output));

  commands::Input input;
  GetGeneratedInput(&input);
  EXPECT_EQ(input.id(), mock_id);
  EXPECT_EQ(input.type(), commands::Input::SEND_COMMAND);
}

TEST_F(ClientTest, SendCommandWithContext) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::SessionCommand session_command;
  session_command.set_type(commands::SessionCommand::SUBMIT);

  commands::Context context;
  context.set_preceding_text(kPrecedingText);
  context.set_following_text(kFollowingText);
  context.set_suppress_suggestion(kSuppressSuggestion);

  commands::Output mock_output;
  mock_output.Clear();
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(
      client_->SendCommandWithContext(session_command, context, &output));

  commands::Input input;
  GetGeneratedInput(&input);
  EXPECT_EQ(input.id(), mock_id);
  EXPECT_EQ(input.type(), commands::Input::SEND_COMMAND);
  EXPECT_EQ(input.context().preceding_text(), kPrecedingText);
  EXPECT_EQ(input.context().following_text(), kFollowingText);
  EXPECT_EQ(input.context().suppress_suggestion(), kSuppressSuggestion);
}

TEST_F(ClientTest, IsDirectModeCommandPresetTest) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  SetMockOutput(mock_output);

  config::Config config = config::ConfigHandler::DefaultConfig();
  config.set_session_keymap(config::Config::ATOK);
  client_->SetConfig(config);
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("HENKAN", &key);
    EXPECT_TRUE(client_->IsDirectModeCommand(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("EISU", &key);
    EXPECT_FALSE(client_->IsDirectModeCommand(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ON", &key);
    EXPECT_TRUE(client_->IsDirectModeCommand(key));
  }
  {
    // Reconcersion
    commands::KeyEvent key;
    KeyParser::ParseKey("Shift HENKAN", &key);
    EXPECT_TRUE(client_->IsDirectModeCommand(key));
  }

  config.set_session_keymap(config::Config::MSIME);
  client_->SetConfig(config);
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("HENKAN", &key);
    EXPECT_TRUE(client_->IsDirectModeCommand(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("EISU", &key);
    EXPECT_TRUE(client_->IsDirectModeCommand(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ON", &key);
    EXPECT_TRUE(client_->IsDirectModeCommand(key));
  }

  config.set_session_keymap(config::Config::KOTOERI);
  client_->SetConfig(config);
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("HENKAN", &key);
    EXPECT_FALSE(client_->IsDirectModeCommand(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("EISU", &key);
    EXPECT_FALSE(client_->IsDirectModeCommand(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ON", &key);
    EXPECT_TRUE(client_->IsDirectModeCommand(key));
  }
  {
    // Reconcersion
    commands::KeyEvent key;
    KeyParser::ParseKey("Ctrl Shift r", &key);
    EXPECT_TRUE(client_->IsDirectModeCommand(key));
  }
}

TEST_F(ClientTest, IsDirectModeCommandDefaultTest) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  SetMockOutput(mock_output);

  config::Config config = config::ConfigHandler::DefaultConfig();
  config.set_session_keymap(config::Config::NONE);
  client_->SetConfig(config);
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("HENKAN", &key);
    // HENKAN key in MSIME is TurnOn key while it's not in KOTOERI.
    if (config::ConfigHandler::GetDefaultKeyMap() == config::Config::MSIME) {
      EXPECT_TRUE(client_->IsDirectModeCommand(key));
    } else {
      EXPECT_FALSE(client_->IsDirectModeCommand(key));
    }
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("EISU", &key);
    if (config::ConfigHandler::GetDefaultKeyMap() == config::Config::MSIME) {
      EXPECT_TRUE(client_->IsDirectModeCommand(key));
    } else {
      EXPECT_FALSE(client_->IsDirectModeCommand(key));
    }
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ON", &key);
    key.set_special_key(commands::KeyEvent::ON);
    if (config::ConfigHandler::GetDefaultKeyMap() == config::Config::CHROMEOS) {
      EXPECT_FALSE(client_->IsDirectModeCommand(key));
    } else {
      EXPECT_TRUE(client_->IsDirectModeCommand(key));
    }
  }
}

TEST_F(ClientTest, IsDirectModeCommandFailureTest) {
  // As SetupConnection is not called, SetConfig fails to update the config.

  config::Config config = config::ConfigHandler::DefaultConfig();
  const bool is_kotoeri = (config.session_keymap() == config::Config::KOTOERI);
  config.set_session_keymap(config::Config::ATOK);
  // SetConfig should be failed.
  EXPECT_FALSE(client_->SetConfig(config));
  {
    commands::KeyEvent key;
    KeyParser::ParseKey(is_kotoeri ? "Ctrl Shift r" : "HENKAN", &key);
    EXPECT_TRUE(client_->IsDirectModeCommand(key));
  }
  {
    // Make sure that the keymap is not updated with no connections.
    // "Shift HENKAN" is not a direct mode command in the default keymap.
    commands::KeyEvent key;
    KeyParser::ParseKey("Shift HENKAN", &key);
    EXPECT_FALSE(client_->IsDirectModeCommand(key));
  }
}

TEST_F(ClientTest, IsDirectModeCommandCustomTest) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  SetMockOutput(mock_output);

  config::Config config = config::ConfigHandler::DefaultConfig();

  const std::string custom_keymap_table =
      "status\tkey\tcommand\n"
      "DirectInput\tHenkan\tIMEOn\n"
      "DirectInput\tCtrl j\tIMEOn\n"
      "DirectInput\tCtrl k\tIMEOff\n"
      "DirectInput\tCtrl l\tLaunchWordRegisterDialog\n"
      "Precomposition\tCtrl m\tIMEOn\n";

  config.set_session_keymap(config::Config::CUSTOM);
  config.set_custom_keymap_table(custom_keymap_table);
  client_->SetConfig(config);
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("HENKAN", &key);
    EXPECT_TRUE(client_->IsDirectModeCommand(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("EISU", &key);
    EXPECT_FALSE(client_->IsDirectModeCommand(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ctrl j", &key);
    EXPECT_TRUE(client_->IsDirectModeCommand(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ctrl k", &key);
    EXPECT_TRUE(client_->IsDirectModeCommand(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ctrl l", &key);
    EXPECT_TRUE(client_->IsDirectModeCommand(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ctrl m", &key);
    EXPECT_FALSE(client_->IsDirectModeCommand(key));
  }
}

TEST_F(ClientTest, SetConfig) {
  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  config::Config config;
  EXPECT_TRUE(client_->SetConfig(config));
}

TEST_F(ClientTest, GetConfig) {
  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.mutable_config()->set_verbose_level(2);
  mock_output.mutable_config()->set_incognito_mode(true);
  SetMockOutput(mock_output);

  config::Config config;
  EXPECT_TRUE(client_->GetConfig(&config));

  EXPECT_EQ(config.verbose_level(), 2);
  EXPECT_EQ(config.incognito_mode(), true);
}

TEST_F(ClientTest, EnableCascadingWindow) {
  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->EnsureConnection());

  client_->NoOperation();
  commands::Input input;
  GetGeneratedInput(&input);
  EXPECT_FALSE(input.has_config());

  client_->EnableCascadingWindow(false);
  client_->NoOperation();
  GetGeneratedInput(&input);
  ASSERT_TRUE(input.has_config());
  ASSERT_TRUE(input.config().has_use_cascading_window());
  EXPECT_FALSE(input.config().use_cascading_window());

  client_->EnableCascadingWindow(true);
  client_->NoOperation();
  GetGeneratedInput(&input);
  ASSERT_TRUE(input.has_config());
  ASSERT_TRUE(input.config().has_use_cascading_window());
  EXPECT_TRUE(input.config().use_cascading_window());

  client_->NoOperation();
  GetGeneratedInput(&input);
  ASSERT_TRUE(input.has_config());
  EXPECT_TRUE(input.config().has_use_cascading_window());
}

TEST_F(ClientTest, VersionMismatch) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  SetMockOutput(mock_output);

  // Suddenly, connects to a different server
  client_factory_->SetServerProtocolVersion(IPC_PROTOCOL_VERSION + 1);
  commands::Output output;
  EXPECT_FALSE(client_->SendKey(key_event, &output));
  EXPECT_FALSE(client_->EnsureConnection());
  EXPECT_EQ(1, server_launcher_->error_count(
                   ServerLauncherInterface::SERVER_VERSION_MISMATCH));
}

TEST_F(ClientTest, ProtocolUpdate) {
  server_launcher_->set_start_server_result(true);

  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->EnsureConnection());

  server_launcher_->set_force_terminate_server_called(false);
  server_launcher_->set_force_terminate_server_result(true);
  server_launcher_->set_start_server_called(false);

  // Now connecting to an old server
  client_factory_->SetServerProtocolVersion(IPC_PROTOCOL_VERSION - 1);
  // after start server, protocol version becomes the same
  server_launcher_->set_server_protocol_version(IPC_PROTOCOL_VERSION);

  EXPECT_TRUE(client_->EnsureSession());
  EXPECT_TRUE(server_launcher_->start_server_called());
  EXPECT_TRUE(server_launcher_->force_terminate_server_called());
}

TEST_F(ClientTest, ProtocolUpdateFailSameBinary) {
  server_launcher_->set_start_server_result(true);

  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->EnsureConnection());

  server_launcher_->set_force_terminate_server_called(false);
  server_launcher_->set_force_terminate_server_result(true);
  server_launcher_->set_start_server_called(false);

  EXPECT_FALSE(server_launcher_->start_server_called());

  // version is updated after restart the server
  client_factory_->SetServerProtocolVersion(IPC_PROTOCOL_VERSION - 1);
  // even after server reboot, protocol version is old
  server_launcher_->set_server_protocol_version(IPC_PROTOCOL_VERSION - 1);
  server_launcher_->set_mock_after_start_server(mock_output);
  EXPECT_FALSE(client_->EnsureSession());
  EXPECT_TRUE(server_launcher_->start_server_called());
  EXPECT_TRUE(server_launcher_->force_terminate_server_called());
  EXPECT_FALSE(client_->EnsureConnection());
  EXPECT_EQ(1, server_launcher_->error_count(
                   ServerLauncherInterface::SERVER_BROKEN_MESSAGE));
}

TEST_F(ClientTest, ProtocolUpdateFailOnTerminate) {
  server_launcher_->set_start_server_result(true);

  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->EnsureConnection());

  server_launcher_->set_force_terminate_server_called(false);
  server_launcher_->set_force_terminate_server_result(false);
  server_launcher_->set_start_server_called(false);

  EXPECT_FALSE(server_launcher_->start_server_called());

  // version is updated after restart the server
  client_factory_->SetServerProtocolVersion(IPC_PROTOCOL_VERSION - 1);
  // even after server reboot, protocol version is old
  server_launcher_->set_server_protocol_version(IPC_PROTOCOL_VERSION);
  server_launcher_->set_mock_after_start_server(mock_output);
  EXPECT_FALSE(client_->EnsureSession());
  EXPECT_FALSE(server_launcher_->start_server_called());
  EXPECT_TRUE(server_launcher_->force_terminate_server_called());
  EXPECT_FALSE(client_->EnsureConnection());
  EXPECT_EQ(1, server_launcher_->error_count(
                   ServerLauncherInterface::SERVER_BROKEN_MESSAGE));
}

TEST_F(ClientTest, ServerUpdate) {
  SetupProductVersion(-1);  // old version
  server_launcher_->set_start_server_result(true);

  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  LOG(ERROR) << Version::GetMozcVersion();

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->EnsureConnection());

  server_launcher_->set_start_server_called(false);
  EXPECT_FALSE(server_launcher_->start_server_called());

  // version is updated after restart the server
  server_launcher_->set_product_version_after_start_server(
      Version::GetMozcVersion());
  EXPECT_TRUE(client_->EnsureSession());
  EXPECT_TRUE(server_launcher_->start_server_called());
}

TEST_F(ClientTest, ServerUpdateToNewer) {
  SetupProductVersion(1);  // new version
  server_launcher_->set_start_server_result(true);

  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  LOG(ERROR) << Version::GetMozcVersion();

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->EnsureConnection());
  server_launcher_->set_start_server_called(false);
  EXPECT_TRUE(client_->EnsureSession());
  EXPECT_FALSE(server_launcher_->start_server_called());
}

TEST_F(ClientTest, ServerUpdateFail) {
  SetupProductVersion(-1);  // old
  server_launcher_->set_start_server_result(true);

  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->EnsureConnection());

  server_launcher_->set_start_server_called(false);
  EXPECT_FALSE(server_launcher_->start_server_called());

  // version is not updated after restart the server
  server_launcher_->set_mock_after_start_server(mock_output);
  EXPECT_FALSE(client_->EnsureSession());
  EXPECT_TRUE(server_launcher_->start_server_called());
  EXPECT_FALSE(client_->EnsureConnection());
  EXPECT_EQ(1, server_launcher_->error_count(
                   ServerLauncherInterface::SERVER_BROKEN_MESSAGE));
}

TEST_F(ClientTest, TranslateProtoBufToMozcToolArgTest) {
  commands::Output output;
  std::string mode = "";

  // If no value is set, we expect to return false
  EXPECT_FALSE(client::Client::TranslateProtoBufToMozcToolArg(output, &mode));
  EXPECT_EQ(mode, "");

  // If NO_TOOL is set, we  expect to return false
  output.set_launch_tool_mode(commands::Output::NO_TOOL);
  EXPECT_FALSE(client::Client::TranslateProtoBufToMozcToolArg(output, &mode));
  EXPECT_EQ(mode, "");

  output.set_launch_tool_mode(commands::Output::CONFIG_DIALOG);
  EXPECT_TRUE(client::Client::TranslateProtoBufToMozcToolArg(output, &mode));
  EXPECT_EQ(mode, "config_dialog");

  output.set_launch_tool_mode(commands::Output::DICTIONARY_TOOL);
  EXPECT_TRUE(client::Client::TranslateProtoBufToMozcToolArg(output, &mode));
  EXPECT_EQ(mode, "dictionary_tool");

  output.set_launch_tool_mode(commands::Output::WORD_REGISTER_DIALOG);
  EXPECT_TRUE(client::Client::TranslateProtoBufToMozcToolArg(output, &mode));
  EXPECT_EQ(mode, "word_register_dialog");
}

TEST_F(ClientTest, InitRequestForSvsJapaneseTest) {
  const int mock_id = 1;
  EXPECT_TRUE(SetupConnection(mock_id));

  client_->InitRequestForSvsJapanese(true);
  EXPECT_TRUE(client_->EnsureSession());

  commands::Input input;
  GetGeneratedInput(&input);
  EXPECT_TRUE(input.has_request());
  EXPECT_TRUE(
      input.request().decoder_experiment_params().variation_character_types() &
      commands::DecoderExperimentParams::SVS_JAPANESE);
}

TEST_F(ClientTest, NoInitRequestForSvsJapaneseTest) {
  const int mock_id = 1;
  EXPECT_TRUE(SetupConnection(mock_id));

  client_->InitRequestForSvsJapanese(false);
  EXPECT_TRUE(client_->EnsureSession());

  commands::Input input;
  GetGeneratedInput(&input);
  EXPECT_TRUE(input.has_request());
  EXPECT_FALSE(
      input.request().decoder_experiment_params().variation_character_types() &
      commands::DecoderExperimentParams::SVS_JAPANESE);
}

class SessionPlaybackTestServerLauncher : public ServerLauncherInterface {
 public:
  explicit SessionPlaybackTestServerLauncher(IPCClientFactoryMock *factory)
      : factory_(factory),
        start_server_result_(false),
        start_server_called_(false),
        force_terminate_server_result_(false),
        force_terminate_server_called_(false),
        server_protocol_version_(IPC_PROTOCOL_VERSION) {}

  virtual void Ready() {}
  virtual void Wait() {}
  virtual void Error() {}

  bool StartServer(ClientInterface *client) override {
    if (!response_.empty()) {
      factory_->SetMockResponse(response_);
    }
    if (!product_version_after_start_server_.empty()) {
      factory_->SetServerProductVersion(product_version_after_start_server_);
    }
    factory_->SetServerProtocolVersion(server_protocol_version_);
    start_server_called_ = true;
    return start_server_result_;
  }

  bool ForceTerminateServer(const absl::string_view name) override {
    force_terminate_server_called_ = true;
    return force_terminate_server_result_;
  }

  bool WaitServer(uint32_t pid) override { return true; }

  void OnFatal(ServerLauncherInterface::ServerErrorType type) override {}

  void set_server_program(const absl::string_view server_path) override {}

  void set_suppress_error_dialog(bool suppress) override {}

  void set_start_server_result(const bool result) {
    start_server_result_ = result;
  }

  zstring_view server_program() const override { return ""; }

 private:
  IPCClientFactoryMock *factory_;
  bool start_server_result_;
  bool start_server_called_;
  bool force_terminate_server_result_;
  bool force_terminate_server_called_;
  uint32_t server_protocol_version_;
  std::string response_;
  std::string product_version_after_start_server_;
  absl::flat_hash_map<int, int> error_map_;
};

class SessionPlaybackTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ipc_client_factory_ = std::make_unique<IPCClientFactoryMock>();
    ipc_client_ = ipc_client_factory_->NewClient("");
    client_ = std::make_unique<Client>();
    client_->SetIPCClientFactory(ipc_client_factory_.get());
    auto server_launcher = std::make_unique<SessionPlaybackTestServerLauncher>(
        ipc_client_factory_.get());
    server_launcher_ = server_launcher.get();
    client_->SetServerLauncher(std::move(server_launcher));
  }

  void TearDown() override {
    client_.reset();
    ipc_client_factory_.reset();
  }

  bool SetupConnection(const int id) {
    ipc_client_factory_->SetConnection(true);
    ipc_client_factory_->SetResult(true);
    ipc_client_factory_->SetServerProductVersion(Version::GetMozcVersion());
    server_launcher_->set_start_server_result(true);

    // TODO(komatsu): Due to the limitation of the testing mock,
    // EnsureConnection should be explicitly called before calling
    // SendKey.  Fix the testing mock.
    commands::Output mock_output;
    mock_output.set_id(id);
    SetMockOutput(mock_output);
    return client_->EnsureConnection();
  }

  void SetMockOutput(const commands::Output &mock_output) {
    std::string response;
    mock_output.SerializeToString(&response);
    ipc_client_factory_->SetMockResponse(response);
  }

  ClientTestPeer client_peer() { return ClientTestPeer(*client_); }

  std::unique_ptr<IPCClientFactoryMock> ipc_client_factory_;
  std::unique_ptr<IPCClientInterface> ipc_client_;
  std::unique_ptr<Client> client_;
  SessionPlaybackTestServerLauncher *server_launcher_;
};

// b/2797557
TEST_F(SessionPlaybackTest, PushAndResetHistoryWithNoModeTest) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(output.consumed(), mock_output.consumed());

  std::vector<commands::Input> history;
  client_peer().GetHistoryInputs(&history);
  EXPECT_EQ(history.size(), 1);

  mock_output.Clear();
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  mock_output.mutable_result()->set_type(commands::Result::STRING);
  mock_output.mutable_result()->set_value("output");
  EXPECT_FALSE(mock_output.has_mode());
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(output.consumed(), mock_output.consumed());

  // history should be reset.
  client_peer().GetHistoryInputs(&history);
  EXPECT_EQ(history.size(), 0);
}

// b/2797557
TEST_F(SessionPlaybackTest, PushAndResetHistoryWithModeTest) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);
  key_event.set_mode(commands::HIRAGANA);
  key_event.set_activated(true);

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  mock_output.set_mode(commands::HIRAGANA);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(output.consumed(), mock_output.consumed());
  EXPECT_TRUE(output.has_mode());
  EXPECT_EQ(output.mode(), commands::HIRAGANA);

  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(output.consumed(), mock_output.consumed());
  EXPECT_TRUE(output.has_mode());
  EXPECT_EQ(output.mode(), commands::HIRAGANA);

  std::vector<commands::Input> history;
  client_peer().GetHistoryInputs(&history);
  EXPECT_EQ(history.size(), 2);

  mock_output.Clear();
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  mock_output.mutable_result()->set_type(commands::Result::STRING);
  mock_output.mutable_result()->set_value("output");
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(output.consumed(), mock_output.consumed());
  client_peer().GetHistoryInputs(&history);
#ifdef __APPLE__
  // history is reset, but initializer should be added because the last mode
  // is not DIRECT.
  // TODO(team): fix b/10250883 to remove this special treatment.
  EXPECT_EQ(history.size(), 1);
  // Implicit IMEOn key must be added. See b/2797557 and b/>10250883.
  EXPECT_EQ(history[0].type(), commands::Input::SEND_KEY);
  EXPECT_EQ(history[0].key().special_key(), commands::KeyEvent::ON);
  EXPECT_EQ(history[0].key().mode(), commands::HIRAGANA);
#else   // __APPLE__
  // history is reset, but initializer is not required.
  EXPECT_EQ(history.size(), 0);
#endif  // __APPLE__
}

// b/2797557
TEST_F(SessionPlaybackTest, PushAndResetHistoryWithDirectTest) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  mock_output.set_mode(commands::DIRECT);
  SetMockOutput(mock_output);

  commands::Output output;
  // Send key twice
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(output.consumed(), mock_output.consumed());
  EXPECT_TRUE(output.has_mode());
  EXPECT_EQ(output.mode(), commands::DIRECT);

  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(output.consumed(), mock_output.consumed());
  EXPECT_TRUE(output.has_mode());
  EXPECT_EQ(output.mode(), commands::DIRECT);

  std::vector<commands::Input> history;
  client_peer().GetHistoryInputs(&history);
  EXPECT_EQ(history.size(), 2);

  mock_output.Clear();
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  mock_output.mutable_result()->set_type(commands::Result::STRING);
  mock_output.mutable_result()->set_value("output");
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(output.consumed(), mock_output.consumed());

  // history is reset, and initializer should not be added.
  client_peer().GetHistoryInputs(&history);
  EXPECT_EQ(history.size(), 0);
}

TEST_F(SessionPlaybackTest, PlaybackHistoryTest) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  // On Windows, mode initializer should be added if the output contains mode.
  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(output.consumed(), mock_output.consumed());

  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(output.consumed(), mock_output.consumed());

  std::vector<commands::Input> history;
  client_peer().GetHistoryInputs(&history);
  EXPECT_EQ(history.size(), 2);

  // Invalid id
  const int new_id = 456;
  mock_output.set_id(new_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->SendKey(key_event, &output));

#ifndef DEBUG
  // PlaybackHistory and push history
  client_peer().GetHistoryInputs(&history);
  EXPECT_EQ(history.size(), 3);
#else   // DEBUG
  // PlaybackHistory, dump history(including reset), and add last input
  client_peer().GetHistoryInputs(&history);
  EXPECT_EQ(history.size(), 1);
#endif  // DEBUG
}

// b/2797557
TEST_F(SessionPlaybackTest, SetModeInitializerTest) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  mock_output.set_mode(commands::HIRAGANA);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(output.consumed(), mock_output.consumed());

  mock_output.set_mode(commands::DIRECT);
  SetMockOutput(mock_output);

  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(output.consumed(), mock_output.consumed());
  EXPECT_TRUE(output.has_mode());
  EXPECT_EQ(output.mode(), commands::DIRECT);

  mock_output.set_mode(commands::FULL_KATAKANA);
  SetMockOutput(mock_output);

  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(output.consumed(), mock_output.consumed());
  EXPECT_TRUE(output.has_mode());
  EXPECT_EQ(output.mode(), commands::FULL_KATAKANA);

  std::vector<commands::Input> history;
  client_peer().GetHistoryInputs(&history);
  EXPECT_EQ(history.size(), 3);

  mock_output.Clear();
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  mock_output.mutable_result()->set_type(commands::Result::STRING);
  mock_output.mutable_result()->set_value("output");
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(output.consumed(), mock_output.consumed());
  client_peer().GetHistoryInputs(&history);
#ifdef __APPLE__
  // history is reset, but initializer should be added.
  // TODO(team): fix b/10250883 to remove this special treatment.
  EXPECT_EQ(history.size(), 1);
  EXPECT_EQ(history[0].type(), commands::Input::SEND_KEY);
  EXPECT_EQ(history[0].key().special_key(), commands::KeyEvent::ON);
  EXPECT_EQ(history[0].key().mode(), commands::FULL_KATAKANA);
#else   // __APPLE__
  // history is reset, but initializer is not required.
  EXPECT_EQ(history.size(), 0);
#endif  // __APPLE__
}

TEST_F(SessionPlaybackTest, ConsumedTest) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(output.consumed(), mock_output.consumed());

  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(output.consumed(), mock_output.consumed());

  std::vector<commands::Input> history;
  client_peer().GetHistoryInputs(&history);
  EXPECT_EQ(history.size(), 2);

  mock_output.set_consumed(false);
  SetMockOutput(mock_output);

  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(output.consumed(), mock_output.consumed());

  // Do not push unconsumed input
  client_peer().GetHistoryInputs(&history);
  EXPECT_EQ(history.size(), 2);
}
}  // namespace client
}  // namespace mozc
