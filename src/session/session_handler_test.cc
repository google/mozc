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
#include <cstdint>
#include <iterator>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "base/clock_mock.h"
#include "base/port.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "converter/converter_mock.h"
#include "engine/engine_builder_interface.h"
#include "engine/engine_mock.h"
#include "engine/engine_stub.h"
#include "engine/mock_data_engine_factory.h"
#include "engine/user_data_manager_mock.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/session_handler_test_util.h"
#include "testing/gmock.h"
#include "testing/googletest.h"
#include "testing/gunit.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"

ABSL_DECLARE_FLAG(int32_t, max_session_size);
ABSL_DECLARE_FLAG(int32_t, create_session_min_interval);
ABSL_DECLARE_FLAG(int32_t, last_command_timeout);
ABSL_DECLARE_FLAG(int32_t, last_create_session_timeout);

namespace mozc {
namespace {

using ::mozc::session::testing::SessionHandlerTestBase;
using ::testing::Return;

// Used to test interaction between SessionHandler and EngineBuilder in engine
// reload event.
class MockEngineBuilder : public EngineBuilderInterface {
 public:
  enum class State {
    STOP,
    RUNNING,
    RELOAD_READY,
    INVALID_DATA,
  };

  void PrepareAsync(const EngineReloadRequest &request,
                    EngineReloadResponse *response) override {
    ++num_prepare_async_called_;
    response->set_status(state_ != State::RUNNING
                             ? EngineReloadResponse::ACCEPTED
                             : EngineReloadResponse::ALREADY_RUNNING);
  }

  bool HasResponse() const override {
    return state_ == State::RELOAD_READY || state_ == State::INVALID_DATA;
  }

  void GetResponse(EngineReloadResponse *response) const override {
    switch (state_) {
      case State::RELOAD_READY:
        response->set_status(EngineReloadResponse::RELOAD_READY);
        break;
      case State::INVALID_DATA:
        response->set_status(EngineReloadResponse::DATA_BROKEN);
        break;
      default:
        response->set_status(EngineReloadResponse::UNKNOWN_ERROR);
        break;
    }
  }

  std::unique_ptr<EngineInterface> BuildFromPreparedData() override {
    ++num_build_from_prepared_data_called_;
    return std::unique_ptr<EngineInterface>(new EngineStub());
  }

  void Clear() override {
    ++num_clear_called_;
    state_ = State::STOP;
  }

  State state() const { return state_; }
  void set_state(State state) { state_ = state; }
  int num_prepare_async_called() const { return num_prepare_async_called_; }
  int num_build_from_prepared_data_called() const {
    return num_build_from_prepared_data_called_;
  }
  int num_clear_called() const { return num_clear_called_; }

 private:
  State state_ = State::STOP;
  int num_prepare_async_called_ = 0;
  int num_build_from_prepared_data_called_ = 0;
  int num_clear_called_ = 0;
};

EngineReloadResponse::Status SendDummyEngineCommand(SessionHandler *handler) {
  commands::Command command;
  command.mutable_input()->set_type(
      commands::Input::SEND_ENGINE_RELOAD_REQUEST);
  auto *request = command.mutable_input()->mutable_engine_reload_request();
  request->set_engine_type(EngineReloadRequest::MOBILE);
  request->set_file_path("dummy");  // Dummy is OK for MockEngineBuilder.
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
  ClockMock clock(1000, 0);
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
      clock.PutClockForward(interval_time, 0);
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
      clock.PutClockForward(interval_time, 0);
    }

    std::random_device rd;
    std::mt19937 urbg(rd());
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

TEST_F(SessionHandlerTest, CreateSession_Config) {
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

TEST_F(SessionHandlerTest, CreateSessionMinInterval) {
  const int32_t interval_time = 10;  // 10 sec
  absl::SetFlag(&FLAGS_create_session_min_interval, interval_time);
  ClockMock clock(1000, 0);
  Clock::SetClockForUnitTest(&clock);

  SessionHandler handler(CreateMockDataEngine());

  uint64_t id = 0;
  EXPECT_TRUE(CreateSession(&handler, &id));
  EXPECT_FALSE(CreateSession(&handler, &id));

  clock.PutClockForward(interval_time - 1, 0);
  EXPECT_FALSE(CreateSession(&handler, &id));

  clock.PutClockForward(1, 0);
  EXPECT_TRUE(CreateSession(&handler, &id));

  Clock::SetClockForUnitTest(nullptr);
}

TEST_F(SessionHandlerTest, LastCreateSessionTimeout) {
  const int32_t timeout = 10;  // 10 sec
  absl::SetFlag(&FLAGS_last_create_session_timeout, timeout);
  ClockMock clock(1000, 0);
  Clock::SetClockForUnitTest(&clock);

  SessionHandler handler(CreateMockDataEngine());

  uint64_t id = 0;
  EXPECT_TRUE(CreateSession(&handler, &id));

  clock.PutClockForward(timeout, 0);
  EXPECT_TRUE(CleanUp(&handler, id));

  // the session is removed by server
  EXPECT_FALSE(IsGoodSession(&handler, id));

  Clock::SetClockForUnitTest(nullptr);
}

TEST_F(SessionHandlerTest, LastCommandTimeout) {
  const int32_t timeout = 10;  // 10 sec
  absl::SetFlag(&FLAGS_last_command_timeout, timeout);
  ClockMock clock(1000, 0);
  Clock::SetClockForUnitTest(&clock);

  SessionHandler handler(CreateMockDataEngine());

  uint64_t id = 0;
  EXPECT_TRUE(CreateSession(&handler, &id));

  EXPECT_TRUE(CleanUp(&handler, id));
  EXPECT_TRUE(IsGoodSession(&handler, id));

  clock.PutClockForward(timeout, 0);
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

  ClockMock clock(1000, 0);
  Clock::SetClockForUnitTest(&clock);
  EXPECT_TRUE(CreateSession(&handler, &id));
  EXPECT_TIMING_STATS("ElapsedTimeUSec", 0, 1, 0, 0);
  Clock::SetClockForUnitTest(nullptr);
}

TEST_F(SessionHandlerTest, ConfigTest) {
  config::Config config;
  config::ConfigHandler::GetStoredConfig(&config);
  SessionHandler handler(CreateMockDataEngine());

  {
    // Set KOTOERI keymap
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_type(commands::Input::SET_CONFIG);
    config.set_session_keymap(config::Config::KOTOERI);
    *input->mutable_config() = config;
    EXPECT_TRUE(handler.EvalCommand(&command));
    config::ConfigHandler::GetStoredConfig(&config);
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
    config::ConfigHandler::GetStoredConfig(&config);
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

TEST_F(SessionHandlerTest, KeyMapTest) {
  config::Config config;
  config::ConfigHandler::GetStoredConfig(&config);
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
    // As different keymap is set, the handler's kaymap manager should be
    // updated.
    EXPECT_NE(handler.key_map_manager_.get(), msime_keymap);
  }
}

TEST_F(SessionHandlerTest, VerifySyncIsCalled) {
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

// Tests the interaction with EngineBuilderInterface for successful Engine
// reload event.
TEST_F(SessionHandlerTest, EngineReloadSuccessfulScenario) {
  MockEngineBuilder *engine_builder = new MockEngineBuilder();
  SessionHandler handler(std::make_unique<EngineStub>(),
                         std::unique_ptr<MockEngineBuilder>(engine_builder));

  // Session handler receives reload request when engine builder is not running.
  // EngineBuilderInterface::PrepareAsync() should be called once.
  engine_builder->set_state(MockEngineBuilder::State::STOP);
  ASSERT_EQ(SendDummyEngineCommand(&handler), EngineReloadResponse::ACCEPTED);
  EXPECT_EQ(engine_builder->num_prepare_async_called(), 1);

  // Emulate the state after successful data load.
  engine_builder->set_state(MockEngineBuilder::State::RELOAD_READY);

  // A new engine should be built on create session event because the session
  // handler currently holds no session.
  uint64_t id = 0;
  ASSERT_TRUE(CreateSession(&handler, &id));
  EXPECT_EQ(engine_builder->num_build_from_prepared_data_called(), 1);
  EXPECT_EQ(engine_builder->num_clear_called(), 1);
}

// Tests the interaction with EngineBuilderInterface in the situation where
// async data load is already running.
TEST_F(SessionHandlerTest, EngineReloadAlreadyRunning) {
  MockEngineBuilder *engine_builder = new MockEngineBuilder();
  SessionHandler handler(std::make_unique<EngineStub>(),
                         std::unique_ptr<MockEngineBuilder>(engine_builder));

  // Emulate the state where async data load is running.
  engine_builder->set_state(MockEngineBuilder::State::RUNNING);

  // Session handler receives reload request when engine builder is running.
  ASSERT_EQ(SendDummyEngineCommand(&handler),
            EngineReloadResponse::ALREADY_RUNNING);
  EXPECT_EQ(engine_builder->num_prepare_async_called(), 1);

  // BuildFromPreparedData() shouldn't be called on create session event when
  // async data load is running.
  uint64_t id = 0;
  ASSERT_TRUE(CreateSession(&handler, &id));
  EXPECT_EQ(engine_builder->num_build_from_prepared_data_called(), 0);
  EXPECT_EQ(engine_builder->num_clear_called(), 0);
}

// Tests the interaction with EngineBuilderInterface in the situation where
// requested data is broken.
TEST_F(SessionHandlerTest, EngineReloadInvalidData) {
  MockEngineBuilder *engine_builder = new MockEngineBuilder();
  SessionHandler handler(std::make_unique<EngineStub>(),
                         std::unique_ptr<MockEngineBuilder>(engine_builder));

  // Emulate the state where requested data is broken.
  engine_builder->set_state(MockEngineBuilder::State::INVALID_DATA);

  // A new engine is not built but the builder should be cleared for next reload
  // request.
  uint64_t id = 0;
  ASSERT_TRUE(CreateSession(&handler, &id));
  EXPECT_EQ(engine_builder->num_build_from_prepared_data_called(), 0);
  EXPECT_EQ(engine_builder->num_clear_called(), 1);
}

// Tests the interaction with EngineBuilderInterface in the situation where
// sessions exist in create session event.
TEST_F(SessionHandlerTest, EngineReloadSessionExists) {
  MockEngineBuilder *engine_builder = new MockEngineBuilder();
  SessionHandler handler(std::make_unique<EngineStub>(),
                         std::unique_ptr<MockEngineBuilder>(engine_builder));

  // A session is created before data is loaded.
  engine_builder->set_state(MockEngineBuilder::State::STOP);
  uint64_t id1 = 0;
  ASSERT_TRUE(CreateSession(&handler, &id1));
  EXPECT_EQ(engine_builder->num_build_from_prepared_data_called(), 0);
  EXPECT_EQ(engine_builder->num_clear_called(), 0);

  // Emulate the state where async data load is complete.
  engine_builder->set_state(MockEngineBuilder::State::RELOAD_READY);

  // Another session is created.  Since the handler already holds one session
  // (id1), engine reload should not happen.
  uint64_t id2 = 0;
  ASSERT_TRUE(CreateSession(&handler, &id2));
  EXPECT_EQ(engine_builder->num_build_from_prepared_data_called(), 0);
  EXPECT_EQ(engine_builder->num_clear_called(), 0);

  // All the sessions were deleted.
  ASSERT_TRUE(DeleteSession(&handler, id1));
  ASSERT_TRUE(DeleteSession(&handler, id2));

  // A new session is created.  Since the handler holds no session, engine is
  // reloaded at this timing.
  uint64_t id3 = 0;
  ASSERT_TRUE(CreateSession(&handler, &id3));
  EXPECT_EQ(engine_builder->num_build_from_prepared_data_called(), 1);
  EXPECT_EQ(engine_builder->num_clear_called(), 1);
}

}  // namespace mozc
