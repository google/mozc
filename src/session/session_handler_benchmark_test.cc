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

// Key event level benchmark for mozc.
//
// Sample command line:
// $ blaze build --config=benchmark
//   //session:session_handler_benchmark_test
//
// $ ./blaze-bin/session/session_handler_benchmark_test
//   --benchmark_filter=BM_PerformanceForRandomKeyEvents
//   --heap_check=
//   --benchmark_min_iters=100
//   --cpu_profile=/tmp/output.prof
//
// $ pprof -flame /tmp/output.prof

#include <cstdint>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "base/file/temp_dir.h"
#include "base/system_util.h"
#include "config/config_handler.h"
#include "engine/engine.h"
#include "engine/oss_engine_factory.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/request_test_util.h"
#include "session/random_keyevents_generator.h"
#include "session/session_handler_tool.h"
#include "testing/benchmark.h"
#include "testing/mozctest.h"

ABSL_FLAG(uint32_t, random_seed, 0,
          "Random seed value. This value will be interpreted as uint32_t.");
ABSL_DECLARE_FLAG(bool, use_history_rewriter);

namespace mozc {
namespace {

using ::mozc::commands::KeyEvent;
using ::mozc::commands::Output;
using ::mozc::config::Config;
using ::mozc::config::ConfigHandler;
using ::mozc::session::RandomKeyEventsGenerator;
using ::mozc::session::SessionHandlerTool;

// Sets up and tears down benchmark environment.
class Fixture {
 public:
  Fixture() : temp_profile_dir_(testing::MakeTempDirectoryOrDie()) { SetUp(); }

  ~Fixture() { TearDown(); }

 private:
  void SetUp() {
    SystemUtil::SetUserProfileDirectory(temp_profile_dir_.path());
    use_history_rewriter_flag_backup_ =
        absl::GetFlag(FLAGS_use_history_rewriter);
    absl::SetFlag(&FLAGS_use_history_rewriter, true);
    config_backup_ = ConfigHandler::GetSharedConfig();
  }

  void TearDown() {
    ConfigHandler::SetConfig(*config_backup_);
    absl::SetFlag(&FLAGS_use_history_rewriter,
                  use_history_rewriter_flag_backup_);
  }

  TempDirectory temp_profile_dir_;
  bool use_history_rewriter_flag_backup_;
  std::shared_ptr<const Config> config_backup_;
};

void BM_PerformanceForRandomKeyEvents(benchmark::State& state) {
  {
    const Fixture fixture;

    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    config.set_use_realtime_conversion(true);
    ConfigHandler::SetConfig(config);

    absl::StatusOr<std::unique_ptr<Engine>> engine = OssEngineFactory::Create();
    CHECK_OK(engine);
    SessionHandlerTool client(*std::move(engine));
    CHECK(client.CreateSession());

    std::vector<KeyEvent> key_events;
    Output output;
    RandomKeyEventsGenerator generator(
        std::seed_seq{absl::GetFlag(FLAGS_random_seed)});
    for (auto s : state) {
      state.PauseTiming();
      key_events.clear();
      generator.GenerateSequence(&key_events);
      state.ResumeTiming();
      for (const KeyEvent& key_event : key_events) {
        client.TestSendKey(key_event, &output);
        client.SendKey(key_event, &output);
      }
    }

    CHECK(client.DeleteSession());
  }
}
BENCHMARK(BM_PerformanceForRandomKeyEvents);

void BM_PerformanceForSingleKeyEvent(benchmark::State& state) {
  {
    const Fixture fixture;

    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    config.set_use_realtime_conversion(true);
    ConfigHandler::SetConfig(config);

    absl::StatusOr<std::unique_ptr<Engine>> engine = OssEngineFactory::Create();
    CHECK_OK(engine);
    SessionHandlerTool client(*std::move(engine));
    CHECK(client.CreateSession());

    std::vector<KeyEvent> key_events;
    Output output;
    RandomKeyEventsGenerator generator(
        std::seed_seq{absl::GetFlag(FLAGS_random_seed)});

    commands::Request request;
    request_test_util::FillMobileRequest(&request);
    client.SetRequest(request, &output);

    for (auto s : state) {
      state.PauseTiming();
      key_events.clear();
      generator.GenerateSequence(&key_events);
      state.ResumeTiming();

      const KeyEvent& key_event = key_events.front();
      client.TestSendKey(key_event, &output);
      client.SendKey(key_event, &output);
    }

    CHECK(client.DeleteSession());
  }
}
BENCHMARK(BM_PerformanceForSingleKeyEvent);

}  // namespace
}  // namespace mozc
