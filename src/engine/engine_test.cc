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

#include "engine/engine.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/thread.h"
#include "composer/query.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "engine/data_loader.h"
#include "engine/modules.h"
#include "engine/spellchecker_interface.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace engine {

namespace {

using ::testing::InSequence;
using ::testing::Return;

class MockSpellchecker : public engine::SpellcheckerInterface {
 public:
  MOCK_METHOD(commands::CheckSpellingResponse, CheckSpelling,
              (const commands::CheckSpellingRequest &), (const, override));
  MOCK_METHOD(std::optional<std::vector<composer::TypeCorrectedQuery>>,
              CheckCompositionSpelling,
              (absl::string_view, absl::string_view, const commands::Request &),
              (const, override));
  MOCK_METHOD(void, MaybeApplyHomonymCorrection, (Segments *),
              (const override));
};

class MockDataLoader : public DataLoader {
 public:
  MOCK_METHOD(std::unique_ptr<ResponseFuture>, Build, (uint64_t),
              (const override));
};
}  // namespace

TEST(EngineTest, ReloadModulesTest) {
  auto data_manager = std::make_unique<testing::MockDataManager>();
  absl::StatusOr<std::unique_ptr<Engine>> engine_status =
      Engine::CreateMobileEngine(std::move(data_manager));
  CHECK_OK(engine_status);

  std::unique_ptr<Engine> engine = std::move(engine_status.value());
  MockSpellchecker spellchecker;
  engine->SetSpellchecker(&spellchecker);
  EXPECT_EQ(engine->GetModulesForTesting()->GetSpellchecker(), &spellchecker);

  auto modules = std::make_unique<engine::Modules>();
  CHECK_OK(modules->Init(std::make_unique<testing::MockDataManager>()));

  const bool is_mobile = true;
  CHECK_OK(engine->ReloadModules(std::move(modules), is_mobile));

  EXPECT_EQ(engine->GetModulesForTesting()->GetSpellchecker(), &spellchecker);
}

// Tests the interaction with DataLoader for successful Engine
// reload event.
TEST(EngineTest, DataLoadSuccessfulScenarioTest) {
  auto data_manager = std::make_unique<testing::MockDataManager>();
  absl::string_view data_version = "DataLoadSuccessfulScenarioTest";
  EXPECT_CALL(*data_manager, GetDataVersion())
      .WillRepeatedly(Return(data_version));
  auto modules = std::make_unique<engine::Modules>();
  CHECK_OK(modules->Init(std::move(data_manager)));

  auto data_loader = std::make_unique<MockDataLoader>();

  EngineReloadRequest request;
  request.set_engine_type(EngineReloadRequest::MOBILE);
  request.set_file_path("placeholder");  // OK for MockDataLoader
  const uint64_t id = data_loader->GetRequestId(request);

  EXPECT_CALL(*data_loader, Build(id))
      .WillOnce(Return(
          std::make_unique<BackgroundFuture<DataLoader::Response>>([&]() {
            // takes 0.1 seconds to make engine.
            absl::SleepFor(absl::Milliseconds(100));
            DataLoader::Response result;
            result.id = id;
            result.response.set_status(EngineReloadResponse::RELOAD_READY);
            result.modules = std::move(modules);
            return result;
          })));

  absl::StatusOr<std::unique_ptr<Engine>> engine_status =
      Engine::CreateMobileEngine(std::make_unique<testing::MockDataManager>());
  EXPECT_OK(engine_status);

  Engine &engine = *engine_status.value();
  engine.SetDataLoaderForTesting(std::move(data_loader));

  EXPECT_TRUE(engine.SendEngineReloadRequest(request));

  EngineReloadResponse response;
  EXPECT_TRUE(engine.MaybeReloadEngine(&response));

  // When the engine is created first, we wait until the engine gets ready.
  EXPECT_EQ(engine.GetDataVersion(), data_version);

  // New session is created, but Build is not called as the id is the same.
  EXPECT_TRUE(engine.SendEngineReloadRequest(request));
  EXPECT_FALSE(engine.MaybeReloadEngine(&response));
  EXPECT_EQ(engine.GetDataVersion(), data_version);
}

// Tests situations to handle multiple new requests.
TEST(EngineTest, DataUpdateSuccessfulScenarioTest) {
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
  const uint64_t id1 = data_loader->GetRequestId(request1);

  EngineReloadRequest request2;
  request2.set_engine_type(EngineReloadRequest::MOBILE);
  request2.set_file_path("placeholder2");  // OK for MockDataLoader
  const uint64_t id2 = data_loader->GetRequestId(request2);

  EXPECT_CALL(*data_loader, Build(id1))
      .WillOnce(Return(
          std::make_unique<BackgroundFuture<DataLoader::Response>>([&]() {
            absl::SleepFor(absl::Milliseconds(100));
            DataLoader::Response result;
            result.id = id1;
            result.response.set_status(EngineReloadResponse::RELOAD_READY);
            result.modules = std::move(modules1);
            return result;
          })));

  absl::StatusOr<std::unique_ptr<Engine>> engine_status =
      Engine::CreateMobileEngine(std::make_unique<testing::MockDataManager>());
  EXPECT_OK(engine_status);

  Engine &engine = *engine_status.value();
  engine.SetDataLoaderForTesting(std::move(data_loader));
  engine.SetAlwaysWaitForLoaderResponseFutureForTesting(true);

  // Send a request, and get a response with id=1.
  EXPECT_TRUE(engine.SendEngineReloadRequest(request1));

  EngineReloadResponse response;
  EXPECT_TRUE(engine.MaybeReloadEngine(&response));

  EXPECT_EQ(engine.GetDataVersion(), data_version1);

  EXPECT_CALL(*data_loader_ptr, Build(id2))
      .WillOnce(Return(
          std::make_unique<BackgroundFuture<DataLoader::Response>>([&]() {
            absl::SleepFor(absl::Milliseconds(100));
            DataLoader::Response result;
            result.id = id2;
            result.response.set_status(EngineReloadResponse::RELOAD_READY);
            result.modules = std::move(modules2);
            return result;
          })));

  // Send a request, and get a response with id=2.
  EXPECT_TRUE(engine.SendEngineReloadRequest(request2));
  EXPECT_TRUE(engine.MaybeReloadEngine(&response));
  EXPECT_EQ(engine.GetDataVersion(), data_version2);
}

// Tests the interaction with DataLoader in the situation where
// requested data is broken.
TEST(EngineTest, ReloadInvalidDataTest) {
  auto data_loader = std::make_unique<MockDataLoader>();
  MockDataLoader *data_loader_ptr = data_loader.get();

  InSequence seq;  // EXPECT_CALL is called sequentially.

  absl::StatusOr<std::unique_ptr<Engine>> engine_status =
      Engine::CreateMobileEngine(std::make_unique<testing::MockDataManager>());
  EXPECT_OK(engine_status);

  Engine &engine = *engine_status.value();
  engine.SetDataLoaderForTesting(std::move(data_loader));

  EngineReloadRequest request;
  request.set_engine_type(EngineReloadRequest::MOBILE);
  request.set_file_path("placeholder");  // OK for MockDataLoader
  const uint64_t id = data_loader_ptr->GetRequestId(request);

  EXPECT_CALL(*data_loader_ptr, Build(id))
      .WillOnce(Return(
          std::make_unique<BackgroundFuture<DataLoader::Response>>([&]() {
            absl::SleepFor(absl::Milliseconds(100));
            DataLoader::Response result;
            result.id = id;
            result.response.set_status(EngineReloadResponse::DATA_BROKEN);
            return result;
          })));

  EXPECT_TRUE(engine.SendEngineReloadRequest(request));

  // Build() is called, but it returns invalid data, so new data is not used.
  EngineReloadResponse response;
  EXPECT_FALSE(engine.MaybeReloadEngine(&response));

  // Sends the same request again, but the request is already marked as
  // unregistered.
  EXPECT_TRUE(engine.SendEngineReloadRequest(request));
  EXPECT_FALSE(engine.MaybeReloadEngine(&response));
}

// Tests the rollback scenario
TEST(EngineTest, RollbackDataTest) {
  auto data_loader = std::make_unique<MockDataLoader>();
  MockDataLoader *data_loader_ptr = data_loader.get();

  InSequence seq;  // EXPECT_CALL is called sequentially.

  auto data_manager1 = std::make_unique<testing::MockDataManager>();
  testing::MockDataManager *data_manager_ptr1 = data_manager1.get();
  absl::string_view data_version1 = "EngineRollbackDataTest1";
  auto modules1 = std::make_unique<engine::Modules>();
  CHECK_OK(modules1->Init(std::move(data_manager1)));

  auto data_manager2 = std::make_unique<testing::MockDataManager>();
  testing::MockDataManager *data_manager_ptr2 = data_manager2.get();
  absl::string_view data_version2 = "EngineRollbackDataTest2";
  auto modules2 = std::make_unique<engine::Modules>();
  CHECK_OK(modules2->Init(std::move(data_manager2)));

  absl::StatusOr<std::unique_ptr<Engine>> engine_status =
      Engine::CreateMobileEngine(std::make_unique<testing::MockDataManager>());
  EXPECT_OK(engine_status);
  Engine &engine = *engine_status.value();

  engine.SetDataLoaderForTesting(std::move(data_loader));
  engine.SetAlwaysWaitForLoaderResponseFutureForTesting(true);

  EngineReloadRequest request1_ready;
  request1_ready.set_engine_type(EngineReloadRequest::MOBILE);
  request1_ready.set_file_path("placeholder1");  // OK for MockDataLoader
  const uint64_t id1 = data_loader_ptr->GetRequestId(request1_ready);

  EngineReloadRequest request2_ready;
  request2_ready.set_engine_type(EngineReloadRequest::MOBILE);
  request2_ready.set_file_path("placeholder2");  // OK for MockDataLoader
  const uint64_t id2 = data_loader_ptr->GetRequestId(request2_ready);

  EngineReloadRequest request3_broken;
  request3_broken.set_engine_type(EngineReloadRequest::MOBILE);
  request3_broken.set_file_path("placeholder3");  // OK for MockDataLoader
  const uint64_t id3 = data_loader_ptr->GetRequestId(request3_broken);

  EngineReloadRequest request4_broken;
  request4_broken.set_engine_type(EngineReloadRequest::MOBILE);
  request4_broken.set_file_path("placeholder4");  // OK for MockDataLoader
  const uint64_t id4 = data_loader_ptr->GetRequestId(request4_broken);

  EXPECT_CALL(*data_loader_ptr, Build(id1))
      .WillOnce(Return(
          std::make_unique<BackgroundFuture<DataLoader::Response>>([&]() {
            absl::SleepFor(absl::Milliseconds(100));
            DataLoader::Response result;
            result.id = id1;
            result.response.set_status(EngineReloadResponse::RELOAD_READY);
            result.modules = std::move(modules1);
            return result;
          })));

  // Sends multiple requests.

  // Request#1
  // Build is called and Module1 is initialized since this is the first call.
  // Build initializes Module1 and sets Modules1 to response_.
  ASSERT_TRUE(engine.SendEngineReloadRequest(request1_ready));

  // Request#2
  // Module2 will be initialized later, since Module1 is already in response_.
  ASSERT_TRUE(engine.SendEngineReloadRequest(request2_ready));

  // Request#3
  // The initialization of Module3 will fail later and will fallback to Module2.
  ASSERT_TRUE(engine.SendEngineReloadRequest(request3_broken));

  // Request#4
  // The initialization of Module3 will fail later and will fallback to Module2.
  ASSERT_TRUE(engine.SendEngineReloadRequest(request4_broken));

  // Engine loads Module1 is used and response_ is cleared.
  EngineReloadResponse response;
  EXPECT_TRUE(engine.MaybeReloadEngine(&response));

  EXPECT_CALL(*data_manager_ptr1, GetDataVersion())
    .WillRepeatedly(Return(data_version1));
  EXPECT_EQ(engine.GetDataVersion(), data_version1);

  // Request#4 fails for the initialization. The current Request#1 is used.
  EXPECT_CALL(*data_loader_ptr, Build(id4))
      .WillOnce(Return(
          std::make_unique<BackgroundFuture<DataLoader::Response>>([&]() {
            absl::SleepFor(absl::Milliseconds(100));
            DataLoader::Response result;
            result.id = id4;
            result.response.set_status(EngineReloadResponse::DATA_BROKEN);
            return result;
          })));
  EXPECT_FALSE(engine.MaybeReloadEngine(&response));
  EXPECT_CALL(*data_manager_ptr1, GetDataVersion())
    .WillRepeatedly(Return(data_version1));
  EXPECT_EQ(engine.GetDataVersion(), data_version1);

  // Request#3 fails for the initialization. The current Request#1 is used.
  EXPECT_CALL(*data_loader_ptr, Build(id3))
      .WillOnce(Return(
          std::make_unique<BackgroundFuture<DataLoader::Response>>([&]() {
            absl::SleepFor(absl::Milliseconds(100));
            DataLoader::Response result;
            result.id = id3;
            result.response.set_status(EngineReloadResponse::DATA_BROKEN);
            return result;
          })));
  EXPECT_FALSE(engine.MaybeReloadEngine(&response));
  EXPECT_CALL(*data_manager_ptr1, GetDataVersion())
    .WillRepeatedly(Return(data_version1));
  EXPECT_EQ(engine.GetDataVersion(), data_version1);

  // Request#2 is initialized.
  EXPECT_CALL(*data_loader_ptr, Build(id2))
      .WillOnce(Return(
          std::make_unique<BackgroundFuture<DataLoader::Response>>([&]() {
            absl::SleepFor(absl::Milliseconds(100));
            DataLoader::Response result;
            result.id = id2;
            result.response.set_status(EngineReloadResponse::RELOAD_READY);
            result.modules = std::move(modules2);
            return result;
          })));
  EXPECT_TRUE(engine.MaybeReloadEngine(&response));

  // Engine uses Module2
  EXPECT_CALL(*data_manager_ptr2, GetDataVersion())
      .WillRepeatedly(Return(data_version2));
  EXPECT_EQ(engine.GetDataVersion(), data_version2);

  // Engine already uses Module2. No need to reload the same request.
  EXPECT_FALSE(engine.MaybeReloadEngine(&response));
  EXPECT_CALL(*data_manager_ptr2, GetDataVersion())
      .WillRepeatedly(Return(data_version2));
  EXPECT_EQ(engine.GetDataVersion(), data_version2);
}

}  // namespace engine
}  // namespace mozc
