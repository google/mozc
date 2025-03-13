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

#include "engine/data_loader.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

#include "absl/base/optimization.h"
#include "absl/log/log.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/file/temp_dir.h"
#include "base/file_util.h"
#include "base/hash.h"
#include "data_manager/data_manager.h"
#include "protocol/engine_builder.pb.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

constexpr absl::string_view kMockMagicNumber = "MOCK";
constexpr absl::string_view kOssMagicNumber = "\xEFMOZC\x0D\x0A";

static auto kNeverCalled = [](std::unique_ptr<DataLoader::Response> resposne) {
  ABSL_UNREACHABLE();
  return absl::OkStatus();
};

struct Param {
  EngineReloadRequest::EngineType type;
  std::string predictor_name;
};

class DataLoaderTest : public testing::TestWithTempUserProfile,
                       public ::testing::WithParamInterface<Param> {
 protected:
  DataLoaderTest()
      : mock_data_path_(
            testing::GetSourcePath({MOZC_SRC_COMPONENTS("data_manager"),
                                    "testing", "mock_mozc.data"})) {
    const std::string mock_path = testing::GetSourcePath(
        {MOZC_SRC_COMPONENTS("data_manager"), "testing", "mock_mozc.data"});
    mock_request_.set_engine_type(EngineReloadRequest::MOBILE);
    mock_request_.set_file_path(mock_path);
    mock_request_.set_magic_number(kMockMagicNumber);
    mock_request_.set_priority(50);

    const std::string oss_path = testing::GetSourcePath(
        {MOZC_SRC_COMPONENTS("data_manager"), "oss", "mozc.data"});
    oss_request_.set_engine_type(EngineReloadRequest::MOBILE);
    oss_request_.set_file_path(oss_path);
    oss_request_.set_magic_number(kOssMagicNumber);
    oss_request_.set_priority(50);
  }

  void Clear() { request_.Clear(); }

  const std::string mock_data_path_;
  EngineReloadRequest request_;
  EngineReloadRequest mock_request_;
  EngineReloadRequest oss_request_;
};

TEST_P(DataLoaderTest, AsyncBuild) {
  request_.set_engine_type(GetParam().type);
  request_.set_file_path(mock_data_path_);
  request_.set_magic_number(kMockMagicNumber);

  DataManager data_manager;
  data_manager.InitFromFile(mock_data_path_, kMockMagicNumber);
  absl::string_view expected_version = data_manager.GetDataVersion();
  const std::string expected_filename = data_manager.GetFilename().value();

  int callback_called = 0;

  DataLoader loader;
  loader.NotifyHighPriorityDataRegisteredForTesting();

  EXPECT_TRUE(loader.StartNewDataBuildTask(
      request_, [&](std::unique_ptr<DataLoader::Response> response) {
        const DataManager &response_data_manager =
            response->modules->GetDataManager();
        EXPECT_EQ(response_data_manager.GetDataVersion(), expected_version);
        EXPECT_TRUE(response_data_manager.GetFilename());
        EXPECT_EQ(response_data_manager.GetFilename().value(),
                  expected_filename);
        EXPECT_EQ(response->response.request().engine_type(), GetParam().type);
        ++callback_called;
        return absl::OkStatus();
      }));

  // Sends the same request. It is accepted, but callback is not called.
  EXPECT_TRUE(loader.StartNewDataBuildTask(request_, kNeverCalled));

  loader.Wait();

  // Sends the same request. It is NOT accepted, as the loader finishes
  // the loading process.
  EXPECT_FALSE(loader.StartNewDataBuildTask(request_, kNeverCalled));

  EXPECT_EQ(callback_called, 1);
}

TEST_P(DataLoaderTest, AsyncBuildRepeatedly) {
  absl::BitGen bitgen;

  constexpr int kMaxPriority = 1000;

  int expected = kMaxPriority;
  int actual = kMaxPriority;
  DataLoader loader;
  loader.NotifyHighPriorityDataRegisteredForTesting();
  int callback_called = 0;

  // Sending multiple requests with random priority.
  for (int trial = 0; trial < 20; ++trial) {
    const int priority = absl::Uniform(bitgen, 0, kMaxPriority);
    EngineReloadRequest request = mock_request_;
    request.set_priority(priority);

    expected = std::min(expected, priority);

    loader.StartNewDataBuildTask(
        request, [&](std::unique_ptr<DataLoader::Response> response) {
          ++callback_called;
          // Keeps the modules loaded lastly.
          actual = response->response.request().priority();
          return absl::OkStatus();
        });
  }

  loader.Wait();

  // The request with highest priority should be loaded.
  EXPECT_GT(callback_called, 0);
  EXPECT_EQ(actual, expected);
}

TEST_P(DataLoaderTest, AsyncBuildWithSamePriorityRepeatedly) {
  std::string expected, actual;
  DataLoader loader;
  loader.NotifyHighPriorityDataRegisteredForTesting();

  // when the priority is the same, last request is loaded.
  for (int trial = 0; trial < 2; ++trial) {
    EngineReloadRequest request = trial == 0 ? mock_request_ : oss_request_;
    request.set_priority(100);
    expected = request.file_path();

    loader.StartNewDataBuildTask(
        request, [&](std::unique_ptr<DataLoader::Response> response) {
          actual = response->response.request().file_path();
          return absl::OkStatus();
        });
  }

  loader.Wait();
  EXPECT_EQ(actual, expected);
}

TEST_P(DataLoaderTest, FailureCaseDataBroken) {
  // Test the case where input file is invalid.
  request_.set_engine_type(GetParam().type);
  request_.set_file_path(testing::GetSourceFileOrDie(
      {MOZC_SRC_COMPONENTS("engine"), "data_loader_test.cc"}));
  request_.set_magic_number(kMockMagicNumber);

  DataLoader loader;
  loader.NotifyHighPriorityDataRegisteredForTesting();
  EXPECT_TRUE(loader.StartNewDataBuildTask(request_, kNeverCalled));

  loader.Wait();
  EXPECT_FALSE(loader.StartNewDataBuildTask(request_, kNeverCalled));
}

TEST_P(DataLoaderTest, FailureCaseFileDoesNotExist) {
  // Test the case where input file doesn't exist.
  request_.set_engine_type(GetParam().type);
  request_.set_file_path("file_does_not_exist");
  request_.set_magic_number(kMockMagicNumber);

  DataLoader loader;
  loader.NotifyHighPriorityDataRegisteredForTesting();
  EXPECT_TRUE(loader.StartNewDataBuildTask(request_, kNeverCalled));

  loader.Wait();
  EXPECT_FALSE(loader.StartNewDataBuildTask(request_, kNeverCalled));
}

TEST_P(DataLoaderTest, LowPriorityReuqestTest) {
  // Starts a new build of a higher request at first.
  DataLoader loader;
  loader.NotifyHighPriorityDataRegisteredForTesting();

  int callback_called = 0;
  EXPECT_TRUE(loader.StartNewDataBuildTask(
      mock_request_, [&](std::unique_ptr<DataLoader::Response> response) {
        EXPECT_EQ(response->response.request().priority(),
                  mock_request_.priority());
        ++callback_called;
        return absl::OkStatus();
      }));

  // Tries another build of a lower request. It waits for the previous task.
  // The new task is not started because of the priority.
  EngineReloadRequest low_priority_request = mock_request_;
  low_priority_request.set_priority(100);
  ASSERT_GT(low_priority_request.priority(), mock_request_.priority());

  EXPECT_TRUE(loader.StartNewDataBuildTask(low_priority_request, kNeverCalled));
  loader.Wait();
  EXPECT_EQ(callback_called, 1);
}

TEST_P(DataLoaderTest, WaitHighPriorityDataTest) {
  auto make_request = [&](int priority) {
    EngineReloadRequest request = mock_request_;
    request.set_priority(priority);
    return request;
  };

  DataLoader loader;

  int callback_called = 0;

  auto callback = [&](std::unique_ptr<DataLoader::Response> response) {
    EXPECT_EQ(response->response.request().priority(), 10);
    ++callback_called;
    return absl::OkStatus();
  };

  // These request are not processed, as they are low priority data.
  EXPECT_TRUE(loader.StartNewDataBuildTask(make_request(50), callback));
  EXPECT_TRUE(loader.StartNewDataBuildTask(make_request(100), callback));
  EXPECT_TRUE(loader.StartNewDataBuildTask(make_request(200), callback));

  // New high priority data is registered.
  EXPECT_TRUE(loader.StartNewDataBuildTask(make_request(10), callback));

  loader.Wait();

  EXPECT_EQ(callback_called, 1);  // only 10.
}

TEST_P(DataLoaderTest, WaitHighPriorityDataTimeoutTest) {
  auto make_request = [&](int priority) {
    EngineReloadRequest request = mock_request_;
    request.set_priority(priority);
    return request;
  };

  DataLoader loader;

  int callback_called = 0;

  constexpr int kExpectedPriority[] = {50, 10};

  auto callback = [&](std::unique_ptr<DataLoader::Response> response) {
    EXPECT_EQ(response->response.request().priority(),
              kExpectedPriority[callback_called]);
    ++callback_called;
    return absl::OkStatus();
  };

  EXPECT_TRUE(loader.StartNewDataBuildTask(make_request(50), callback));
  EXPECT_TRUE(loader.StartNewDataBuildTask(make_request(100), callback));
  EXPECT_TRUE(loader.StartNewDataBuildTask(make_request(200), callback));

  // Timeout. priority = 50 is loaded.
  absl::SleepFor(absl::Milliseconds(200));

  // Then priority = 10 is loaded.
  EXPECT_TRUE(loader.StartNewDataBuildTask(make_request(10), callback));

  loader.Wait();

  EXPECT_EQ(callback_called, 2);  // 50 and 10
}

INSTANTIATE_TEST_SUITE_P(
    DataLoaderTest, DataLoaderTest,
    ::testing::Values(Param{EngineReloadRequest::DESKTOP, "DefaultPredictor"},
                      Param{EngineReloadRequest::MOBILE, "MobilePredictor"}),
    [](const ::testing::TestParamInfo<Param> &info) -> std::string {
      return info.param.predictor_name;
    });

}  // namespace
}  // namespace mozc
