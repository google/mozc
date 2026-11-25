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

// Engine benchmark tests.
//
// # How to run all benchmarks
// $ blaze run --config=benchmark
// --copt=-gmlt engine:engine_benchmark_test
// -- --benchmark_filter=all
//
// # How to get pprof
// $ blaze build --config=benchmark
//   //engine:engine_benchmark_test
//
// $ ./blaze-bin/engine/engine_benchmark_test
//   --benchmark_filter=BM_InitEngineOnly
//   --heap_check=
//   --benchmark_min_iters=100
//   --cpu_profile=/tmp/output.prof
//
// $ pprof -flame /tmp/output.prof

#include <memory>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "base/file/temp_dir.h"
#include "base/system_util.h"
#include "data_manager/data_manager.h"
#include "data_manager/oss/oss_data_manager.h"
#include "data_manager/testing/mock_data_manager.h"
#include "engine/engine.h"
#include "testing/benchmark.h"
#include "testing/gmock.h"
#include "testing/mozctest.h"

namespace mozc {
namespace engine {

using ::testing::Return;

void BM_InitEngineWithMockData(benchmark::State& state) {
  TempDirectory tempdir = testing::MakeTempDirectoryOrDie();
  SystemUtil::SetUserProfileDirectory(tempdir.path());

  for (auto s : state) {
    {
      auto data_manager = std::make_unique<testing::MockDataManager>();
      // Prevent gmock from complaining about unexpected calls.
      EXPECT_CALL(*data_manager, GetDataVersion()).WillRepeatedly(Return(""));
      absl::StatusOr<std::unique_ptr<Engine>> engine_status =
          Engine::CreateEngine(std::move(data_manager));
      state.PauseTiming();
      CHECK_OK(engine_status);
      // Destructors run here.
    }
    state.ResumeTiming();
  }
}
BENCHMARK(BM_InitEngineWithMockData);

void BM_InitOssDataManager(benchmark::State& state) {
  TempDirectory tempdir = testing::MakeTempDirectoryOrDie();
  SystemUtil::SetUserProfileDirectory(tempdir.path());

  for (auto s : state) {
    {
      auto data_manager = std::make_unique<oss::OssDataManager>();
      state.PauseTiming();
      CHECK(data_manager);
      // Destructors run here.
    }
    state.ResumeTiming();
  }
}
BENCHMARK(BM_InitOssDataManager);

void BM_InitEngineWithOssData(benchmark::State& state) {
  TempDirectory tempdir = testing::MakeTempDirectoryOrDie();
  SystemUtil::SetUserProfileDirectory(tempdir.path());

  for (auto s : state) {
    {
      auto data_manager = std::make_unique<oss::OssDataManager>();
      absl::StatusOr<std::unique_ptr<Engine>> engine_status =
          Engine::CreateEngine(std::move(data_manager));
      state.PauseTiming();
      CHECK_OK(engine_status);
      // Destructors run here.
    }
    state.ResumeTiming();
  }
}
BENCHMARK(BM_InitEngineWithOssData);

void BM_InitDataManagerWithOssFile(benchmark::State& state) {
  TempDirectory tempdir = testing::MakeTempDirectoryOrDie();
  SystemUtil::SetUserProfileDirectory(tempdir.path());

  const std::string data_path =
      testing::GetSourceFileOrDie({"data_manager", "oss", "mozc.data"});

  for (auto s : state) {
    {
      absl::StatusOr<std::unique_ptr<const DataManager>> data_manager =
          DataManager::CreateFromFile(
              data_path, DataManager::GetDataSetMagicNumber("oss"));
      state.PauseTiming();
      CHECK_OK(data_manager);
      // Destructors run here.
    }
    state.ResumeTiming();
  }
}
BENCHMARK(BM_InitDataManagerWithOssFile);

void BM_InitEngineWithOssFile(benchmark::State& state) {
  TempDirectory tempdir = testing::MakeTempDirectoryOrDie();
  SystemUtil::SetUserProfileDirectory(tempdir.path());

  const std::string data_path =
      testing::GetSourceFileOrDie({"data_manager", "oss", "mozc.data"});

  for (auto s : state) {
    {
      absl::StatusOr<std::unique_ptr<const DataManager>> data_manager =
          DataManager::CreateFromFile(
              data_path, DataManager::GetDataSetMagicNumber("oss"));
      CHECK_OK(data_manager);
      absl::StatusOr<std::unique_ptr<Engine>> engine_status =
          Engine::CreateEngine(*std::move(data_manager));
      state.PauseTiming();
      CHECK_OK(engine_status);
      // Destructors run here.
    }
    state.ResumeTiming();
  }
}
BENCHMARK(BM_InitEngineWithOssFile);

void BM_InitEngineOnly(benchmark::State& state) {
  TempDirectory tempdir = testing::MakeTempDirectoryOrDie();
  SystemUtil::SetUserProfileDirectory(tempdir.path());

  const std::string data_path =
      testing::GetSourceFileOrDie({"data_manager", "oss", "mozc.data"});

  for (auto s : state) {
    state.PauseTiming();
    absl::StatusOr<std::unique_ptr<const DataManager>> data_manager =
        DataManager::CreateFromFile(data_path,
                                    DataManager::GetDataSetMagicNumber("oss"));
    CHECK_OK(data_manager);
    state.ResumeTiming();
    {
      absl::StatusOr<std::unique_ptr<Engine>> engine_status =
          Engine::CreateEngine(*std::move(data_manager));
      state.PauseTiming();
      CHECK_OK(engine_status);
      // Destructors run here.
    }
    state.ResumeTiming();
  }
}
BENCHMARK(BM_InitEngineOnly);

}  // namespace engine
}  // namespace mozc
