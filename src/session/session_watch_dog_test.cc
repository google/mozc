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

#include "session/session_watch_dog.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/log/check.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/cpu_stats.h"
#include "client/client_mock.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

using ::testing::Return;

// Returns the given CPU load values in order.
class TestCPUStats : public CPUStatsInterface {
 public:
  explicit TestCPUStats(std::vector<float> cpu_loads) {
    Set(std::move(cpu_loads));
  }

  float GetSystemCPULoad() override {
    absl::MutexLock lock(mutex_);
    CHECK_GT(cpu_loads_.size(), 0);
    float load = cpu_loads_.back();
    cpu_loads_.pop_back();
    return load;
  }

  float GetCurrentProcessCPULoad() override { return 0.0; }

  size_t GetNumberOfProcessors() const override { return size_t{1}; }

  void Set(std::vector<float> cpu_loads) {
    absl::MutexLock lock(mutex_);
    cpu_loads_ = std::move(cpu_loads);
    std::reverse(cpu_loads_.begin(), cpu_loads_.end());
  }

 private:
  absl::Mutex mutex_;
  std::vector<float> cpu_loads_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace

client::ClientMock* CreateMockClient() {
  auto* client = new client::ClientMock();
  EXPECT_CALL(*client, PingServer()).WillRepeatedly(Return(true));
  ON_CALL(*client, Cleanup()).WillByDefault(Return(true));
  return client;
}

TEST(SessionWatchDogTest, SessionWatchDogTest) {
  constexpr absl::Duration kInterval = absl::Seconds(1);
  auto* client = CreateMockClient();
  auto stats = std::make_unique<TestCPUStats>(std::vector<float>(5, 0.0f));
  EXPECT_CALL(*client, Cleanup()).Times(5);

  SessionWatchDog watchdog(kInterval, absl::WrapUnique(client),
                           std::move(stats));
  EXPECT_EQ(watchdog.interval(), kInterval);

  absl::SleepFor(absl::Milliseconds(100));
  EXPECT_EQ(watchdog.interval(), kInterval);

  absl::SleepFor(absl::Milliseconds(5500));  // 5.5 sec
}

TEST(SessionWatchDogTest, SessionWatchDogCPUStatsTest) {
  constexpr absl::Duration kInterval = absl::Seconds(1);
  auto* client = CreateMockClient();
  auto* cpu_loads = new TestCPUStats(std::vector<float>(5, 0.8f));

  mozc::SessionWatchDog watchdog(kInterval, absl::WrapUnique(client),
                                 absl::WrapUnique(cpu_loads));
  EXPECT_EQ(watchdog.interval(), kInterval);

  absl::SleepFor(absl::Milliseconds(100));
  EXPECT_EQ(watchdog.interval(), kInterval);
  absl::SleepFor(absl::Milliseconds(5500));  // 5.5 sec
}

TEST(SessionWatchDogTest, SessionCanSendCleanupCommandTest) {
  mozc::SessionWatchDog watchdog(absl::Seconds(2));

  // suspend
  EXPECT_FALSE(watchdog.CanSendCleanupCommand(
      {0.0, 0.0}, absl::FromUnixSeconds(5), absl::FromUnixSeconds(0)));

  // not suspend
  EXPECT_TRUE(watchdog.CanSendCleanupCommand(
      {0.0, 0.0}, absl::FromUnixSeconds(1), absl::FromUnixSeconds(0)));

  // error (the same time stamp)
  EXPECT_FALSE(watchdog.CanSendCleanupCommand(
      {0.0, 0.0}, absl::FromUnixSeconds(0), absl::FromUnixSeconds(0)));

  // average CPU loads >= 0.33
  EXPECT_FALSE(watchdog.CanSendCleanupCommand({0.4, 0.5, 0.4, 0.6},
                                              absl::FromUnixSeconds(1),
                                              absl::FromUnixSeconds(0)));

  // recent CPU loads >= 0.66
  EXPECT_FALSE(watchdog.CanSendCleanupCommand({0.1, 0.1, 0.7, 0.7},
                                              absl::FromUnixSeconds(1),
                                              absl::FromUnixSeconds(0)));

  // average CPU loads >= 0.33
  EXPECT_FALSE(watchdog.CanSendCleanupCommand({1.0, 1.0, 1.0, 1.0, 0.1, 0.1},
                                              absl::FromUnixSeconds(1),
                                              absl::FromUnixSeconds(0)));

  EXPECT_TRUE(watchdog.CanSendCleanupCommand({0.1, 0.1, 0.1, 0.1, 0.1},
                                             absl::FromUnixSeconds(1),
                                             absl::FromUnixSeconds(0)));
}

}  // namespace mozc
