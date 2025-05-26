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

#include <memory>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "data_manager/data_manager.h"
#include "data_manager/testing/mock_data_manager.h"
#include "engine/modules.h"
#include "engine/supplemental_model_interface.h"
#include "protocol/engine_builder.pb.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace engine {

namespace {

class SupplementalModelForTesting : public engine::SupplementalModelInterface {
};

constexpr absl::string_view kMockMagicNumber = "MOCK";
constexpr absl::string_view kOssMagicNumber = "\xEFMOZC\x0D\x0A";
constexpr int kMiddlePriority = 50;
}  // namespace

class EngineTest : public ::testing::Test {
 protected:
  EngineTest() {
    const std::string mock_path =
        testing::GetSourcePath({"data_manager", "testing", "mock_mozc.data"});
    mock_request_.set_engine_type(EngineReloadRequest::MOBILE);
    mock_request_.set_file_path(mock_path);
    mock_request_.set_magic_number(kMockMagicNumber);
    mock_request_.set_priority(kMiddlePriority);

    const std::string oss_path =
        testing::GetSourcePath({"data_manager", "oss", "mozc.data"});
    oss_request_.set_engine_type(EngineReloadRequest::MOBILE);
    oss_request_.set_file_path(oss_path);
    oss_request_.set_magic_number(kOssMagicNumber);
    oss_request_.set_priority(kMiddlePriority);

    const std::string invalid_path =
        testing::GetSourcePath({"data_manager", "invalid", "mozc.data"});
    invalid_path_request_.set_engine_type(EngineReloadRequest::MOBILE);
    invalid_path_request_.set_file_path(invalid_path);
    invalid_path_request_.set_magic_number(kOssMagicNumber);
    invalid_path_request_.set_priority(kMiddlePriority);

    invalid_data_request_.set_engine_type(EngineReloadRequest::MOBILE);
    invalid_data_request_.set_file_path(mock_path);
    invalid_data_request_.set_magic_number(kOssMagicNumber);
    invalid_data_request_.set_priority(kMiddlePriority);

    mock_version_ = DataManager::CreateFromFile(mock_request_.file_path(),
                                                mock_request_.magic_number())
                        .value()
                        ->GetDataVersion();
    oss_version_ = DataManager::CreateFromFile(oss_request_.file_path(),
                                               oss_request_.magic_number())
                       .value()
                       ->GetDataVersion();
  }

  void SetUp() override {
    engine_ = Engine::CreateEngine();
    engine_->SetAlwaysWaitForTesting(true);
  }

  std::unique_ptr<Engine> engine_;

  std::string mock_version_;
  std::string oss_version_;

  EngineReloadRequest mock_request_;
  EngineReloadRequest oss_request_;
  EngineReloadRequest invalid_path_request_;
  EngineReloadRequest invalid_data_request_;
};

TEST_F(EngineTest, ReloadModulesTest) {
  std::unique_ptr<Modules> modules =
      engine::Modules::Create(std::make_unique<testing::MockDataManager>())
          .value();

  const bool is_mobile = true;
  CHECK_OK(engine_->ReloadModules(std::move(modules), is_mobile));
}

// Tests the interaction with DataLoader for successful Engine
// reload event.
TEST_F(EngineTest, DataLoadSuccessfulScenarioTest) {
  EngineReloadResponse response;

  // The engine is not updated yet.
  EXPECT_NE(engine_->GetDataVersion(), mock_version_);

  // The engine is updated with the request.
  EXPECT_TRUE(engine_->SendEngineReloadRequest(mock_request_));
  EXPECT_TRUE(engine_->MaybeReloadEngine(&response));
  EXPECT_EQ(engine_->GetDataVersion(), mock_version_);

  // The engine is not updated with the same request.
  EXPECT_FALSE(engine_->SendEngineReloadRequest(mock_request_));
  EXPECT_FALSE(engine_->MaybeReloadEngine(&response));
  EXPECT_EQ(engine_->GetDataVersion(), mock_version_);
}

// Tests situations to handle multiple new requests.
TEST_F(EngineTest, DataUpdateSuccessfulScenarioTest) {
  EngineReloadResponse response;

  // Send a request, and update the engine.
  EXPECT_TRUE(engine_->SendEngineReloadRequest(mock_request_));
  EXPECT_TRUE(engine_->MaybeReloadEngine(&response));
  EXPECT_EQ(engine_->GetDataVersion(), mock_version_);

  // Send another request, and update the engine again.
  EXPECT_TRUE(engine_->SendEngineReloadRequest(oss_request_));
  EXPECT_TRUE(engine_->MaybeReloadEngine(&response));
  EXPECT_EQ(engine_->GetDataVersion(), oss_version_);
}

// Tests the interaction with DataLoader in the situation where
// requested data is broken.
TEST_F(EngineTest, ReloadInvalidDataTest) {
  EXPECT_TRUE(engine_->SendEngineReloadRequest(invalid_path_request_));

  // The new request is performed, but it returns invalid data.
  EngineReloadResponse response;
  EXPECT_FALSE(engine_->MaybeReloadEngine(&response));

  // Sends the same request again, but the request is already marked as
  // unregistered.
  EXPECT_FALSE(engine_->SendEngineReloadRequest(invalid_path_request_));
  EXPECT_FALSE(engine_->MaybeReloadEngine(&response));
}

// Tests the rollback scenario
TEST_F(EngineTest, RollbackDataTest) {
  // Sends multiple requests three times.
  EXPECT_TRUE(engine_->SendEngineReloadRequest(mock_request_));
  EXPECT_TRUE(engine_->SendEngineReloadRequest(invalid_path_request_));
  EXPECT_TRUE(engine_->SendEngineReloadRequest(invalid_data_request_));

  // The last two requests are invalid. The first request is immediately used as
  // a fallback.
  EngineReloadResponse response;
  EXPECT_TRUE(engine_->MaybeReloadEngine(&response));
  EXPECT_EQ(response.request().file_path(), mock_request_.file_path());

  // DataVersion comes from the first request (i.e. mock_request_).
  EXPECT_EQ(engine_->GetDataVersion(), mock_version_);
}
}  // namespace engine
}  // namespace mozc
