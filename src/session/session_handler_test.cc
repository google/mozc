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

#include "session/session_handler.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/clock.h"
#include "base/clock_mock.h"
#include "base/thread.h"
#include "composer/query.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "engine/data_loader.h"
#include "engine/engine.h"
#include "engine/engine_mock.h"
#include "engine/minimal_engine.h"
#include "engine/mock_data_engine_factory.h"
#include "engine/modules.h"
#include "engine/user_data_manager_mock.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/internal/keymap.h"
#include "session/session_handler_interface.h"
#include "session/session_handler_test_util.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "usage_stats/usage_stats_testing_util.h"


ABSL_DECLARE_FLAG(int32_t, max_session_size);
ABSL_DECLARE_FLAG(int32_t, create_session_min_interval);
ABSL_DECLARE_FLAG(int32_t, last_command_timeout);
ABSL_DECLARE_FLAG(int32_t, last_create_session_timeout);

namespace mozc {
namespace {

using ::mozc::session::testing::SessionHandlerTestBase;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;

class MockDataLoader : public DataLoader {
 public:
  MOCK_METHOD(std::unique_ptr<ResponseFuture>, Build, (uint64_t),
              (const override));
};

EngineReloadResponse::Status SendMockEngineReloadRequest(
    SessionHandler *handler, const EngineReloadRequest &request) {
  commands::Command command;
  command.mutable_input()->set_type(
      commands::Input::SEND_ENGINE_RELOAD_REQUEST);
  *command.mutable_input()->mutable_engine_reload_request() = request;
  handler->EvalCommand(&command);
  return command.output().engine_reload_response().status();
}

bool CreateSession(SessionHandlerInterface *handler, uint64_t *id) {
  commands::Command command;
  command.mutable_input()->set_type(commands::Input::CREATE_SESSION);
  command.mutable_input()->mutable_capability()->set_text_deletion(
      commands::Capability::DELETE_PRECEDING_TEXT);
  handler->EvalCommand(&command);
  if (id != nullptr) {
    *id = command.has_output() ? command.output().id() : 0;
  }
  return (command.output().error_code() == commands::Output::SESSION_SUCCESS);
}

bool DeleteSession(SessionHandlerInterface *handler, uint64_t id) {
  commands::Command command;
  command.mutable_input()->set_id(id);
  command.mutable_input()->set_type(commands::Input::DELETE_SESSION);
  return handler->EvalCommand(&command);
}

bool CleanUp(SessionHandlerInterface *handler, uint64_t id) {
  commands::Command command;
  command.mutable_input()->set_id(id);
  command.mutable_input()->set_type(commands::Input::CLEANUP);
  return handler->EvalCommand(&command);
}

bool IsGoodSession(SessionHandlerInterface *handler, uint64_t id) {
  commands::Command command;
  command.mutable_input()->set_id(id);
  command.mutable_input()->set_type(commands::Input::SEND_KEY);
  command.mutable_input()->mutable_key()->set_special_key(
      commands::KeyEvent::SPACE);
  handler->EvalCommand(&command);
  return (command.output().error_code() == commands::Output::SESSION_SUCCESS);
}

}  // namespace

class SessionHandlerTest : public SessionHandlerTestBase {
 protected:
  void SetUp() override {
    SessionHandlerTestBase::SetUp();
    Clock::SetClockForUnitTest(nullptr);
  }

  void TearDown() override {
    Clock::SetClockForUnitTest(nullptr);
    SessionHandlerTestBase::TearDown();
  }

  static std::unique_ptr<Engine> CreateMockDataEngine() {
    return MockDataEngineFactory::Create().value();
  }
};

TEST_F(SessionHandlerTest, MaxSessionSizeTest) {
  uint32_t expected_session_created_num = 0;
  const int32_t interval_time = 10;  // 10 sec
  absl::SetFlag(&FLAGS_create_session_min_interval, interval_time);
  ClockMock clock(absl::FromUnixSeconds(1000));
  Clock::SetClockForUnitTest(&clock);

  // The oldest item is removed
  const size_t session_size = 3;
  absl::SetFlag(&FLAGS_max_session_size, static_cast<int32_t>(session_size));
  {
    SessionHandler handler(CreateMockDataEngine());

    // Create session_size + 1 sessions
    std::vector<uint64_t> ids;
    for (size_t i = 0; i <= session_size; ++i) {
      uint64_t id = 0;
      EXPECT_TRUE(CreateSession(&handler, &id));
      ++expected_session_created_num;
      EXPECT_COUNT_STATS("SessionCreated", expected_session_created_num);
      ids.push_back(id);
      clock.Advance(absl::Seconds(interval_time));
    }

    for (int i = static_cast<int>(ids.size() - 1); i >= 0; --i) {
      if (i > 0) {  // this id is alive
        EXPECT_TRUE(IsGoodSession(&handler, ids[i]));
      } else {  // the first id should be removed
        EXPECT_FALSE(IsGoodSession(&handler, ids[i]));
      }
    }
  }

  absl::SetFlag(&FLAGS_max_session_size, static_cast<int32_t>(session_size));
  {
    SessionHandler handler(CreateMockDataEngine());

    // Create session_size sessions
    std::vector<uint64_t> ids;
    for (size_t i = 0; i < session_size; ++i) {
      uint64_t id = 0;
      EXPECT_TRUE(CreateSession(&handler, &id));
      ++expected_session_created_num;
      EXPECT_COUNT_STATS("SessionCreated", expected_session_created_num);
      ids.push_back(id);
      clock.Advance(absl::Seconds(interval_time));
    }

    absl::BitGen urbg;
    std::shuffle(ids.begin(), ids.end(), urbg);
    const uint64_t oldest_id = ids[0];
    for (size_t i = 0; i < session_size; ++i) {
      EXPECT_TRUE(IsGoodSession(&handler, ids[i]));
    }

    // Create new session
    uint64_t id = 0;
    EXPECT_TRUE(CreateSession(&handler, &id));
    ++expected_session_created_num;
    EXPECT_COUNT_STATS("SessionCreated", expected_session_created_num);

    // the oldest id no longer exists
    EXPECT_FALSE(IsGoodSession(&handler, oldest_id));
  }

  Clock::SetClockForUnitTest(nullptr);
}

TEST_F(SessionHandlerTest, CreateSession_ConfigTest) {
  // Setting ATOK to ConfigHandler before all other initializations.
  //  Not using SET_CONFIG command
  // because we're emulating the behavior of initial launch of Mozc
  // decoder, where SET_CONFIG isn't sent.
  config::Config config;
  config.set_session_keymap(config::Config::ATOK);
  config::ConfigHandler::SetConfig(config);

  SessionHandler handler(CreateMockDataEngine());

  // The created session should be using ATOK keymap.
  uint64_t session_id = 0;
  EXPECT_TRUE(CreateSession(&handler, &session_id));
  {
    // Move to PRECOMPOSITION mode.
    // On Windows, its initial mode is DIRECT.
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_id(session_id);
    input->set_type(commands::Input::SEND_KEY);
    input->mutable_key()->set_special_key(commands::KeyEvent::ON);
    EXPECT_TRUE(handler.EvalCommand(&command));
  }
  {
    // Check if the config in ConfigHandler is respected
    // even without SET_CONFIG command.
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_id(session_id);
    input->set_type(commands::Input::SEND_KEY);
    input->mutable_key()->set_special_key(commands::KeyEvent::F7);
    input->mutable_key()->add_modifier_keys(commands::KeyEvent::CTRL);
    EXPECT_TRUE(handler.EvalCommand(&command));
    EXPECT_EQ(command.output().launch_tool_mode(),
              commands::Output::WORD_REGISTER_DIALOG);
  }
}

TEST_F(SessionHandlerTest, CreateSessionMinIntervalTest) {
  constexpr absl::Duration kIntervalTime = absl::Seconds(10);
  absl::SetFlag(&FLAGS_create_session_min_interval,
                absl::ToInt64Seconds(kIntervalTime));
  ClockMock clock(absl::FromUnixSeconds(1000));
  Clock::SetClockForUnitTest(&clock);

  SessionHandler handler(CreateMockDataEngine());

  uint64_t id = 0;
  EXPECT_TRUE(CreateSession(&handler, &id));
  EXPECT_FALSE(CreateSession(&handler, &id));

  clock.Advance(kIntervalTime - absl::Seconds(1));
  EXPECT_FALSE(CreateSession(&handler, &id));

  clock.Advance(absl::Seconds(1));
  EXPECT_TRUE(CreateSession(&handler, &id));

  Clock::SetClockForUnitTest(nullptr);
}

TEST_F(SessionHandlerTest, CreateSessionNegativeIntervalTest) {
  absl::SetFlag(&FLAGS_create_session_min_interval, 0);
  ClockMock clock(absl::FromTimeT(1000));
  Clock::SetClockForUnitTest(&clock);

  SessionHandler handler(CreateMockDataEngine());

  uint64_t id = 0;
  EXPECT_TRUE(CreateSession(&handler, &id));

  // A user can modify their system clock.
  clock.Advance(-absl::Seconds(1));
  EXPECT_TRUE(CreateSession(&handler, &id));

  Clock::SetClockForUnitTest(nullptr);
}

TEST_F(SessionHandlerTest, LastCreateSessionTimeoutTest) {
  constexpr absl::Duration kTimeout = absl::Seconds(10);
  absl::SetFlag(&FLAGS_last_create_session_timeout,
                absl::ToInt64Seconds(kTimeout));
  ClockMock clock(absl::FromUnixSeconds(1000));
  Clock::SetClockForUnitTest(&clock);

  SessionHandler handler(CreateMockDataEngine());

  uint64_t id = 0;
  EXPECT_TRUE(CreateSession(&handler, &id));

  clock.Advance(kTimeout);
  EXPECT_TRUE(CleanUp(&handler, id));

  // the session is removed by server
  EXPECT_FALSE(IsGoodSession(&handler, id));

  Clock::SetClockForUnitTest(nullptr);
}

TEST_F(SessionHandlerTest, LastCommandTimeoutTest) {
  const int32_t timeout = 10;  // 10 sec
  absl::SetFlag(&FLAGS_last_command_timeout, timeout);
  ClockMock clock(absl::FromUnixSeconds(1000));
  Clock::SetClockForUnitTest(&clock);

  SessionHandler handler(CreateMockDataEngine());

  uint64_t id = 0;
  EXPECT_TRUE(CreateSession(&handler, &id));

  EXPECT_TRUE(CleanUp(&handler, id));
  EXPECT_TRUE(IsGoodSession(&handler, id));

  clock.Advance(absl::Seconds(timeout));
  EXPECT_TRUE(CleanUp(&handler, id));
  EXPECT_FALSE(IsGoodSession(&handler, id));

  Clock::SetClockForUnitTest(nullptr);
}

TEST_F(SessionHandlerTest, ShutdownTest) {
  SessionHandler handler(CreateMockDataEngine());

  uint64_t session_id = 0;
  EXPECT_TRUE(CreateSession(&handler, &session_id));

  {
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_id(session_id);
    input->set_type(commands::Input::SHUTDOWN);
    // EvalCommand returns false since the session no longer exists.
    EXPECT_FALSE(handler.EvalCommand(&command));
    EXPECT_EQ(command.output().id(), session_id);
  }

  {  // Any command should be rejected after shutdown.
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_id(session_id);
    input->set_type(commands::Input::NO_OPERATION);
    EXPECT_FALSE(handler.EvalCommand(&command));
  }

  EXPECT_COUNT_STATS("ShutDown", 1);
  // CreateSession and Shutdown.
  EXPECT_COUNT_STATS("SessionAllEvent", 2);
}

TEST_F(SessionHandlerTest, ClearHistoryTest) {
  SessionHandler handler(CreateMockDataEngine());

  uint64_t session_id = 0;
  EXPECT_TRUE(CreateSession(&handler, &session_id));

  {
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_id(session_id);
    input->set_type(commands::Input::CLEAR_USER_HISTORY);
    EXPECT_TRUE(handler.EvalCommand(&command));
    EXPECT_EQ(command.output().id(), session_id);
    EXPECT_COUNT_STATS("ClearUserHistory", 1);
  }

  {
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_id(session_id);
    input->set_type(commands::Input::CLEAR_USER_PREDICTION);
    EXPECT_TRUE(handler.EvalCommand(&command));
    EXPECT_EQ(command.output().id(), session_id);
    EXPECT_COUNT_STATS("ClearUserPrediction", 1);
  }

  {
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_id(session_id);
    input->set_type(commands::Input::CLEAR_UNUSED_USER_PREDICTION);
    EXPECT_TRUE(handler.EvalCommand(&command));
    EXPECT_EQ(command.output().id(), session_id);
    EXPECT_COUNT_STATS("ClearUnusedUserPrediction", 1);
  }

  // CreateSession and Clear{History|UserPrediction|UnusedUserPrediction}.
  EXPECT_COUNT_STATS("SessionAllEvent", 4);
}

TEST_F(SessionHandlerTest, ElapsedTimeTest) {
  SessionHandler handler(CreateMockDataEngine());

  uint64_t id = 0;

  ClockMock clock(absl::FromUnixSeconds(1000));
  Clock::SetClockForUnitTest(&clock);
  EXPECT_TRUE(CreateSession(&handler, &id));
  EXPECT_TIMING_STATS("ElapsedTimeUSec", 0, 1, 0, 0);
  Clock::SetClockForUnitTest(nullptr);
}

TEST_F(SessionHandlerTest, ConfigTest) {
  config::Config config;
  config::ConfigHandler::GetConfig(&config);
  SessionHandler handler(CreateMockDataEngine());

  {
    // Set KOTOERI keymap
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_type(commands::Input::SET_CONFIG);
    config.set_session_keymap(config::Config::KOTOERI);
    *input->mutable_config() = config;
    EXPECT_TRUE(handler.EvalCommand(&command));
    config::ConfigHandler::GetConfig(&config);
    EXPECT_EQ(command.output().config().session_keymap(),
              config::Config::KOTOERI);
  }

  uint64_t session_id = 0;
  EXPECT_TRUE(CreateSession(&handler, &session_id));
  {
    // Move to PRECOMPOSITION mode.
    // On Windows, its initial mode is DIRECT.
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_id(session_id);
    input->set_type(commands::Input::SEND_KEY);
    input->mutable_key()->set_special_key(commands::KeyEvent::ON);
    EXPECT_TRUE(handler.EvalCommand(&command));
  }
  {
    // KOTOERI doesn't assign anything to ctrl+shift+space (precomposition) so
    // SEND_KEY shouldn't consume it.
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_id(session_id);
    input->set_type(commands::Input::SEND_KEY);
    input->mutable_key()->set_special_key(commands::KeyEvent::SPACE);
    input->mutable_key()->add_modifier_keys(commands::KeyEvent::SHIFT);
    input->mutable_key()->add_modifier_keys(commands::KeyEvent::CTRL);
    EXPECT_TRUE(handler.EvalCommand(&command));
    EXPECT_FALSE(command.output().consumed());
  }
  {
    // Set ATOK keymap
    // The existing Session should apply it immediately.
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_id(session_id);
    input->set_type(commands::Input::SET_CONFIG);
    config.set_session_keymap(config::Config::ATOK);
    *input->mutable_config() = config;
    EXPECT_TRUE(handler.EvalCommand(&command));
    EXPECT_EQ(command.output().id(), command.input().id());
    config::ConfigHandler::GetConfig(&config);
    EXPECT_EQ(command.output().config().session_keymap(), config::Config::ATOK);
  }
  {
    // ATOK assigns a function to ctrl+f7 (precomposition) (KOTOERI doesn't) so
    // TEST_SEND_KEY should consume it.
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_id(session_id);
    input->set_type(commands::Input::SEND_KEY);
    input->mutable_key()->set_special_key(commands::KeyEvent::F7);
    input->mutable_key()->add_modifier_keys(commands::KeyEvent::CTRL);
    EXPECT_TRUE(handler.EvalCommand(&command));
    EXPECT_EQ(command.output().launch_tool_mode(),
              commands::Output::WORD_REGISTER_DIALOG);
  }

  EXPECT_COUNT_STATS("SetConfig", 1);
  // CreateSession, GetConfig and SetConfig.
  EXPECT_COUNT_STATS("SessionAllEvent", 3);
}

TEST_F(SessionHandlerTest, UpdateComposition) {
  config::Config config;
  config::ConfigHandler::GetConfig(&config);
  config::ConfigHandler::SetConfig(config);
  SessionHandler handler(CreateMockDataEngine());

  uint64_t session_id = 0;
  EXPECT_TRUE(CreateSession(&handler, &session_id));
  {
    // Move to PRECOMPOSITION mode.
    // On Windows, its initial mode is DIRECT.
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_id(session_id);
    input->set_type(commands::Input::SEND_KEY);
    input->mutable_key()->set_special_key(commands::KeyEvent::ON);
    EXPECT_TRUE(handler.EvalCommand(&command));
  }
  {
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_id(session_id);
    input->set_type(commands::Input::SEND_COMMAND);
    input->mutable_command()->set_type(
        commands::SessionCommand::UPDATE_COMPOSITION);
    commands::SessionCommand::CompositionEvent *composition_event =
        input->mutable_command()->add_composition_events();
    composition_event->set_composition_string("かん字");
    composition_event->set_probability(1.0);
    EXPECT_TRUE(handler.EvalCommand(&command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_EQ(command.output().preedit().segment(0).value(), "かん字");
  }
}

TEST_F(SessionHandlerTest, KeyMapTest) {
  config::Config config;
  config::ConfigHandler::GetConfig(&config);
  config::ConfigHandler::SetConfig(config);
  const keymap::KeyMapManager *msime_keymap;

  SessionHandler handler(CreateMockDataEngine());

  uint64_t session_id = 0;
  EXPECT_TRUE(CreateSession(&handler, &session_id));

  {
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_id(session_id);
    input->set_type(commands::Input::SET_CONFIG);
    input->mutable_config()->set_session_keymap(config::Config::MSIME);
    EXPECT_TRUE(handler.EvalCommand(&command));
    msime_keymap = handler.key_map_manager_.get();
  }
  {
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_id(session_id);
    input->set_type(commands::Input::SET_CONFIG);
    input->mutable_config()->set_session_keymap(config::Config::KOTOERI);
    EXPECT_TRUE(handler.EvalCommand(&command));
    // As different keymap is set, the handler's keymap manager should be
    // updated.
    EXPECT_NE(handler.key_map_manager_.get(), msime_keymap);
  }
}

TEST_F(SessionHandlerTest, VerifySyncIsCalledTest) {
  // Tests if sync is called for the following input commands.
  commands::Input::CommandType command_types[] = {
      commands::Input::DELETE_SESSION,
      commands::Input::CLEANUP,
  };
  for (size_t i = 0; i < std::size(command_types); ++i) {
    MockUserDataManager mock_user_data_manager;
    auto engine = std::make_unique<MockEngine>();
    EXPECT_CALL(*engine, GetUserDataManager())
        .WillRepeatedly(Return(&mock_user_data_manager));

    // Set up a session handler and a input command.
    SessionHandler handler(std::move(engine));
    commands::Command command;
    command.mutable_input()->set_id(0);
    command.mutable_input()->set_type(command_types[i]);

    EXPECT_CALL(mock_user_data_manager, Sync()).WillOnce(Return(true));
    handler.EvalCommand(&command);
  }
}

TEST_F(SessionHandlerTest, SyncDataTest) {
  MockUserDataManager mock_user_data_manager;
  auto engine = std::make_unique<MockEngine>();
  EXPECT_CALL(*engine, GetUserDataManager())
      .WillRepeatedly(Return(&mock_user_data_manager));

  // Set up a session handler and a input command.
  SessionHandler handler(std::move(engine));
  commands::Command command;
  command.mutable_input()->set_id(0);
  command.mutable_input()->set_type(commands::Input::SYNC_DATA);

  EXPECT_CALL(mock_user_data_manager, Sync()).WillOnce(Return(true));
  EXPECT_CALL(mock_user_data_manager, Wait()).WillOnce(Return(true));
  handler.EvalCommand(&command);
}

// TODO(b/326372188): Reduce redundancy with EngineTest.
// Tests the interaction with DataLoader for successful Engine
// reload event.
TEST_F(SessionHandlerTest, EngineReloadSuccessfulScenarioTest) {
  auto data_manager = std::make_unique<testing::MockDataManager>();
  absl::string_view data_version = "EngineReloadSuccessfulScenarioTest";
  EXPECT_CALL(*data_manager, GetDataVersion())
      .WillRepeatedly(Return(data_version));
  auto modules = std::make_unique<engine::Modules>();
  CHECK_OK(modules->Init(std::move(data_manager)));

  auto data_loader = std::make_unique<MockDataLoader>();

  EngineReloadRequest request;
  request.set_engine_type(EngineReloadRequest::MOBILE);
  request.set_file_path("placeholder");  // OK for MockDataLoader
  const uint64_t data_id = data_loader->GetRequestId(request);

  EXPECT_CALL(*data_loader, Build(data_id))
      .WillOnce(Return(
          std::make_unique<BackgroundFuture<DataLoader::Response>>([&]() {
            // takes 0.1 seconds to make engine.
            absl::SleepFor(absl::Milliseconds(100));
            DataLoader::Response result;
            result.id = data_id;
            result.response.set_status(EngineReloadResponse::RELOAD_READY);
            result.modules = std::move(modules);
            return result;
          })));

  absl::StatusOr<std::unique_ptr<Engine>> engine_status =
      Engine::CreateMobileEngine(std::make_unique<testing::MockDataManager>());
  EXPECT_OK(engine_status);
  (*engine_status)->SetDataLoaderForTesting(std::move(data_loader));
  SessionHandler handler(std::move(*engine_status));

  ASSERT_EQ(SendMockEngineReloadRequest(&handler, request),
            EngineReloadResponse::ACCEPTED);

  // A new engine should be built on create session event because the session
  // handler currently holds no session.
  commands::Command command;
  command.mutable_input()->set_type(commands::Input::CREATE_SESSION);
  handler.EvalCommand(&command);
  EXPECT_EQ(command.output().error_code(), commands::Output::SESSION_SUCCESS);
  EXPECT_TRUE(command.output().has_engine_reload_response());
  EXPECT_EQ(command.output().engine_reload_response().status(),
            EngineReloadResponse::RELOADED);
  EXPECT_NE(command.output().id(), 0);

  // When the engine is created first, we wait until the engine gets ready.
  EXPECT_EQ(handler.engine().GetDataVersion(), data_version);

  // New session is created, but Build is not called as the id is the same.
  ASSERT_EQ(SendMockEngineReloadRequest(&handler, request),
            EngineReloadResponse::ACCEPTED);

  uint64_t id = 0;
  ASSERT_TRUE(DeleteSession(&handler, id));
  ASSERT_TRUE(CreateSession(&handler, &id));
  EXPECT_EQ(handler.engine().GetDataVersion(), data_version);
}

// TODO(b/326372188): Reduce redundancy with EngineTest.
// Tests situations to handle multiple new requests.
TEST_F(SessionHandlerTest, EngineUpdateSuccessfulScenarioTest) {
  auto data_loader = std::make_unique<MockDataLoader>();
  MockDataLoader *data_loader_ptr = data_loader.get();

  auto data_manager1 = std::make_unique<testing::MockDataManager>();
  absl::string_view data_version1 = "EngineUpdateSuccessfulScenarioTest_1";
  EXPECT_CALL(*data_manager1, GetDataVersion())
      .WillRepeatedly(Return(data_version1));
  auto modules1 = std::make_unique<engine::Modules>();
  CHECK_OK(modules1->Init(std::move(data_manager1)));

  auto data_manager2 = std::make_unique<testing::MockDataManager>();
  absl::string_view data_version2 = "EngineUpdateSuccessfulScenarioTest_2";
  EXPECT_CALL(*data_manager2, GetDataVersion())
      .WillRepeatedly(Return(data_version2));
  auto modules2 = std::make_unique<engine::Modules>();
  CHECK_OK(modules2->Init(std::move(data_manager2)));

  InSequence seq;  // EXPECT_CALL is called sequentially.

  EngineReloadRequest request1;
  request1.set_engine_type(EngineReloadRequest::MOBILE);
  request1.set_file_path("placeholder1");  // OK for MockDataLoader
  const uint64_t data_id1 = data_loader->GetRequestId(request1);

  EngineReloadRequest request2;
  request2.set_engine_type(EngineReloadRequest::MOBILE);
  request2.set_file_path("placeholder2");  // OK for MockDataLoader
  const uint64_t data_id2 = data_loader->GetRequestId(request2);

  EXPECT_CALL(*data_loader, Build(data_id1))
      .WillOnce(Return(
          std::make_unique<BackgroundFuture<DataLoader::Response>>([&]() {
            absl::SleepFor(absl::Milliseconds(100));
            DataLoader::Response result;
            result.id = data_id1;
            result.response.set_status(EngineReloadResponse::RELOAD_READY);
            result.modules = std::move(modules1);
            return result;
          })));

  absl::StatusOr<std::unique_ptr<Engine>> engine_status =
      Engine::CreateMobileEngine(std::make_unique<testing::MockDataManager>());
  EXPECT_OK(engine_status);
  (*engine_status)->SetDataLoaderForTesting(std::move(data_loader));
  (*engine_status)->SetAlwaysWaitForLoaderResponseFutureForTesting(true);
  SessionHandler handler(std::move(*engine_status));

  // engine_id = 1
  ASSERT_EQ(SendMockEngineReloadRequest(&handler, request1),
            EngineReloadResponse::ACCEPTED);

  // build request is called one per new engine reload request.
  uint64_t id = 0;
  ASSERT_TRUE(CreateSession(&handler, &id));
  EXPECT_EQ(handler.engine().GetDataVersion(), data_version1);

  // Use data_loader_ptr after std::move(engine_build).
  EXPECT_CALL(*data_loader_ptr, Build(data_id2))
      .WillOnce(Return(
          std::make_unique<BackgroundFuture<DataLoader::Response>>([&]() {
            absl::SleepFor(absl::Milliseconds(100));
            DataLoader::Response result;
            result.id = data_id2;
            result.response.set_status(EngineReloadResponse::RELOAD_READY);
            result.modules = std::move(modules2);
            return result;
          })));

  // engine_id = 2
  ASSERT_EQ(SendMockEngineReloadRequest(&handler, request2),
            EngineReloadResponse::ACCEPTED);

  ASSERT_TRUE(DeleteSession(&handler, id));
  ASSERT_TRUE(CreateSession(&handler, &id));
  EXPECT_EQ(handler.engine().GetDataVersion(), data_version2);
}

// TODO(b/326372188): Reduce redundancy with EngineTest.
// Tests the interaction with DataLoader in the situation where
// requested data is broken.
TEST_F(SessionHandlerTest, EngineReloadInvalidDataTest) {
  auto data_loader = std::make_unique<MockDataLoader>();
  MockDataLoader *data_loader_ptr = data_loader.get();

  InSequence seq;  // EXPECT_CALL is called sequentially.

  absl::StatusOr<std::unique_ptr<Engine>> engine_status =
      Engine::CreateMobileEngine(std::make_unique<testing::MockDataManager>());
  EXPECT_OK(engine_status);
  const Engine *old_engine_ptr = engine_status.value().get();
  (*engine_status)->SetDataLoaderForTesting(std::move(data_loader));
  SessionHandler handler(std::move(*engine_status));

  EngineReloadRequest request;
  request.set_engine_type(EngineReloadRequest::MOBILE);
  request.set_file_path("placeholder");  // OK for MockDataLoader
  const uint64_t data_id = data_loader_ptr->GetRequestId(request);
  ASSERT_EQ(SendMockEngineReloadRequest(&handler, request),
            EngineReloadResponse::ACCEPTED);

  EXPECT_CALL(*data_loader_ptr, Build(data_id))
      .WillOnce(Return(
          std::make_unique<BackgroundFuture<DataLoader::Response>>([&]() {
            absl::SleepFor(absl::Milliseconds(100));
            DataLoader::Response result;
            result.id = data_id;
            result.response.set_status(EngineReloadResponse::DATA_BROKEN);
            return result;
          })));

  // Build() is called, but it returns invalid data, so new data is not used.
  EXPECT_EQ(&handler.engine(), old_engine_ptr);

  // CreateSession does not contain engine_reload_response.
  commands::Command command;
  command.mutable_input()->set_type(commands::Input::CREATE_SESSION);
  handler.EvalCommand(&command);
  EXPECT_EQ(command.output().error_code(), commands::Output::SESSION_SUCCESS);
  EXPECT_FALSE(command.output().has_engine_reload_response());
  EXPECT_NE(command.output().id(), 0);

  EXPECT_EQ(&handler.engine(), old_engine_ptr);

  // Sends the same request again, but the request is already marked as
  // unregistered.
  uint64_t id = 0;
  ASSERT_EQ(SendMockEngineReloadRequest(&handler, request),
            EngineReloadResponse::ACCEPTED);
  ASSERT_TRUE(DeleteSession(&handler, id));
  ASSERT_TRUE(CreateSession(&handler, &id));
  EXPECT_EQ(&handler.engine(), old_engine_ptr);
}

// TODO(b/326372188): Reduce redundancy with EngineTest.
// Tests the rollback scenario
TEST_F(SessionHandlerTest, EngineRollbackDataTest) {
  auto data_loader = std::make_unique<MockDataLoader>();
  MockDataLoader *data_loader_ptr = data_loader.get();

  InSequence seq;  // EXPECT_CALL is called sequentially.

  auto data_manager = std::make_unique<testing::MockDataManager>();
  testing::MockDataManager *data_manager_ptr = data_manager.get();
  absl::string_view data_version = "EngineRollbackDataTest";
  auto modules = std::make_unique<engine::Modules>();
  CHECK_OK(modules->Init(std::move(data_manager)));

  absl::StatusOr<std::unique_ptr<Engine>> engine_status =
      Engine::CreateMobileEngine(std::make_unique<testing::MockDataManager>());
  EXPECT_OK(engine_status);
  (*engine_status)->SetDataLoaderForTesting(std::move(data_loader));
  (*engine_status)->SetAlwaysWaitForLoaderResponseFutureForTesting(true);
  SessionHandler handler(std::move(*engine_status));

  EngineReloadRequest request1_ready;
  request1_ready.set_engine_type(EngineReloadRequest::MOBILE);
  request1_ready.set_file_path("placeholder1");  // OK for MockDataLoader
  const uint64_t data_id1 = data_loader_ptr->GetRequestId(request1_ready);

  EngineReloadRequest request2_broken;
  request2_broken.set_engine_type(EngineReloadRequest::MOBILE);
  request2_broken.set_file_path("placeholder2");  // OK for MockDataLoader
  const uint64_t data_id2 = data_loader_ptr->GetRequestId(request2_broken);

  EngineReloadRequest request3_broken;
  request3_broken.set_engine_type(EngineReloadRequest::MOBILE);
  request3_broken.set_file_path("placeholder3");  // OK for MockDataLoader
  const uint64_t data_id3 = data_loader_ptr->GetRequestId(request3_broken);

  // Sends multiple requests three times. 1 -> 2 -> 3.
  // 3 is the latest id.
  ASSERT_EQ(SendMockEngineReloadRequest(&handler, request1_ready),
            EngineReloadResponse::ACCEPTED);
  ASSERT_EQ(SendMockEngineReloadRequest(&handler, request2_broken),
            EngineReloadResponse::ACCEPTED);
  ASSERT_EQ(SendMockEngineReloadRequest(&handler, request3_broken),
            EngineReloadResponse::ACCEPTED);

  // Rollback as 3 -> 2 -> 1.  1 is only valid engine.
  EXPECT_CALL(*data_loader_ptr, Build(data_id3))
      .WillOnce(Return(
          std::make_unique<BackgroundFuture<DataLoader::Response>>([&]() {
            absl::SleepFor(absl::Milliseconds(100));
            DataLoader::Response result;
            result.id = data_id3;
            result.response.set_status(EngineReloadResponse::DATA_BROKEN);
            return result;
          })));
  EXPECT_CALL(*data_loader_ptr, Build(data_id2))
      .WillOnce(Return(
          std::make_unique<BackgroundFuture<DataLoader::Response>>([&]() {
            absl::SleepFor(absl::Milliseconds(100));
            DataLoader::Response result;
            result.id = data_id2;
            result.response.set_status(EngineReloadResponse::DATA_BROKEN);
            return result;
          })));
  EXPECT_CALL(*data_loader_ptr, Build(data_id1))
      .WillOnce(Return(
          std::make_unique<BackgroundFuture<DataLoader::Response>>([&]() {
            absl::SleepFor(absl::Milliseconds(100));
            DataLoader::Response result;
            result.id = data_id1;
            result.response.set_status(EngineReloadResponse::RELOAD_READY);
            result.modules = std::move(modules);
            return result;
          })));

  for (int eid = 3; eid >= 1; --eid) {
    // Engine of 3, and 2 are unregistered.
    // The second best id (2, and 1) are used.
    uint64_t id = 0;
    ASSERT_TRUE(CreateSession(&handler, &id));
    ASSERT_TRUE(DeleteSession(&handler, id));
  }

  EXPECT_CALL(*data_manager_ptr, GetDataVersion())
      .WillRepeatedly(Return(data_version));

  // Finally rollback to the new engine 1.
  EXPECT_EQ(handler.engine().GetDataVersion(), data_version);
}

// Tests the interaction with DataLoader in the situation where
// sessions exist in create session event.
TEST_F(SessionHandlerTest, EngineReloadSessionExistsTest) {
  auto data_manager = std::make_unique<testing::MockDataManager>();
  absl::string_view data_version = "EngineReloadSessionExistsTest";
  EXPECT_CALL(*data_manager, GetDataVersion())
      .WillRepeatedly(Return(data_version));
  auto modules = std::make_unique<engine::Modules>();
  CHECK_OK(modules->Init(std::move(data_manager)));

  auto data_loader = std::make_unique<MockDataLoader>();

  EngineReloadRequest request;
  request.set_engine_type(EngineReloadRequest::MOBILE);
  request.set_file_path("placeholder1");  // OK for MockDataLoader
  const uint64_t data_id = data_loader->GetRequestId(request);

  EXPECT_CALL(*data_loader, Build(data_id))
      .WillOnce(Return(
          std::make_unique<BackgroundFuture<DataLoader::Response>>([&]() {
            absl::SleepFor(absl::Milliseconds(100));
            DataLoader::Response result;
            result.id = data_id;
            result.response.set_status(EngineReloadResponse::RELOAD_READY);
            result.modules = std::move(modules);
            return result;
          })));

  absl::StatusOr<std::unique_ptr<Engine>> engine_status =
      Engine::CreateMobileEngine(std::make_unique<testing::MockDataManager>());
  EXPECT_OK(engine_status);
  const Engine *old_engine_ptr = engine_status.value().get();
  (*engine_status)->SetDataLoaderForTesting(std::move(data_loader));
  SessionHandler handler(std::move(*engine_status));

  // A session is created before data is loaded.
  // data_loader->Build() is called, but engine is not reloaded.
  uint64_t id1 = 0;
  ASSERT_TRUE(CreateSession(&handler, &id1));
  EXPECT_EQ(&handler.engine(), old_engine_ptr);
  EXPECT_NE(handler.engine().GetDataVersion(), data_version);

  ASSERT_EQ(SendMockEngineReloadRequest(&handler, request),
            EngineReloadResponse::ACCEPTED);

  // Another session is created. Since the handler already holds one session
  // (id1), new data manager is not used.
  uint64_t id2 = 0;
  ASSERT_TRUE(CreateSession(&handler, &id2));
  EXPECT_EQ(&handler.engine(), old_engine_ptr);
  EXPECT_NE(handler.engine().GetDataVersion(), data_version);

  // All the sessions were deleted.
  ASSERT_TRUE(DeleteSession(&handler, id1));
  ASSERT_TRUE(DeleteSession(&handler, id2));

  // A new session is created. Since the handler holds no session, engine
  // reloads the new data manager.
  uint64_t id3 = 0;
  ASSERT_TRUE(CreateSession(&handler, &id3));
  // New data is reloaded, but the engine is the same object.
  EXPECT_EQ(&handler.engine(), old_engine_ptr);
  EXPECT_EQ(handler.engine().GetDataVersion(), data_version);
}

TEST_F(SessionHandlerTest, GetServerVersionTest) {
  auto engine = std::make_unique<MockEngine>();
  EXPECT_CALL(*engine, GetDataVersion())
      .WillRepeatedly(Return("24.20240101.01"));

  SessionHandler handler(std::move(engine));
  commands::Command command;
  command.mutable_input()->set_type(commands::Input::GET_SERVER_VERSION);
  handler.EvalCommand(&command);
  EXPECT_EQ(command.output().server_version().data_version(), "24.20240101.01");
}


}  // namespace mozc
