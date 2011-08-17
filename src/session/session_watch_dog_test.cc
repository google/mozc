// Copyright 2010-2011, Google Inc.
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
#include <vector>
#include "base/base.h"
#include "base/cpu_stats.h"
#include "base/mutex.h"
#include "base/util.h"
#include "client/client_mock.h"
#include "session/commands.pb.h"
#include "session/session_watch_dog.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/googletest.h"

namespace mozc {
namespace {

class TestCPUStats : public CPUStatsInterface {
 public:
  TestCPUStats() : cpu_loads_index_(0) {}

  float GetSystemCPULoad() {
    scoped_lock l(&mutex_);
    CHECK_LT(cpu_loads_index_, cpu_loads_.size());
    return cpu_loads_[cpu_loads_index_++];
  }

  float GetCurrentProcessCPULoad() {
    return 0.0;
  }

  size_t GetNumberOfProcessors() const {
    return static_cast<size_t>(1);
  }

  void SetCPULoads(const vector<float> &cpu_loads) {
    scoped_lock l(&mutex_);
    cpu_loads_index_ = 0;
    cpu_loads_ = cpu_loads;
  }

 private:
  Mutex mutex_;
  vector<float> cpu_loads_;
  int cpu_loads_index_;
};

}  // namespace

class SessionWatchDogTest : public testing::Test {
 protected:
  void InitializeClient(mozc::client::ClientMock *client) {
    client->SetBoolFunctionReturn("PingServer", true);
    client->SetBoolFunctionReturn("Cleanup", true);
  }
};

TEST_F(SessionWatchDogTest, SessionWatchDogTest) {
  static const int32 kInterval = 1;  // for every 1sec
  mozc::SessionWatchDog watchdog(kInterval);
  EXPECT_FALSE(watchdog.IsRunning());  // not running
  EXPECT_EQ(kInterval, watchdog.interval());

  mozc::client::ClientMock client;
  InitializeClient(&client);
  mozc::TestCPUStats stats;

  vector<float> cpu_loads;
  // no CPU loads
  for (int i = 0; i < 20; ++i) {
    cpu_loads.push_back(0.0);
  }
  stats.SetCPULoads(cpu_loads);

  watchdog.SetClientInterface(&client);
  watchdog.SetCPUStatsInterface(&stats);

  EXPECT_EQ(0, client.GetFunctionCallCount("Cleanup"));

  watchdog.Start();  // start

  mozc::Util::Sleep(100);
  EXPECT_TRUE(watchdog.IsRunning());
  EXPECT_EQ(kInterval, watchdog.interval());

  mozc::Util::Sleep(5500);  // 5.5 sec

  EXPECT_EQ(5, client.GetFunctionCallCount("Cleanup"));

  mozc::Util::Sleep(5000);  // 10.5 sec

  EXPECT_EQ(10, client.GetFunctionCallCount("Cleanup"));

  watchdog.Terminate();
}

TEST_F(SessionWatchDogTest, SessionWatchDogCPUStatsTest) {
  static const int32 kInterval = 1;  // for every 1sec
  mozc::SessionWatchDog watchdog(kInterval);
  EXPECT_FALSE(watchdog.IsRunning());  // not running
  EXPECT_EQ(kInterval, watchdog.interval());

  mozc::client::ClientMock client;
  InitializeClient(&client);
  mozc::TestCPUStats stats;

  vector<float> cpu_loads;
  // high CPU loads
  for (int i = 0; i < 20; ++i) {
    cpu_loads.push_back(0.8);
  }
  stats.SetCPULoads(cpu_loads);

  watchdog.SetClientInterface(&client);
  watchdog.SetCPUStatsInterface(&stats);

  EXPECT_EQ(0, client.GetFunctionCallCount("Cleanup"));

  watchdog.Start();  // start

  mozc::Util::Sleep(100);
  EXPECT_TRUE(watchdog.IsRunning());
  EXPECT_EQ(kInterval, watchdog.interval());
  mozc::Util::Sleep(5500);  // 5.5 sec

  // not called
  EXPECT_EQ(0, client.GetFunctionCallCount("Cleanup"));

  // cup loads become low
  cpu_loads.clear();
  for (int i = 0; i < 20; ++i) {
    cpu_loads.push_back(0.0);
  }
  stats.SetCPULoads(cpu_loads);

  mozc::Util::Sleep(5000);  // 5 sec
  // called
  EXPECT_EQ(5, client.GetFunctionCallCount("Cleanup"));

  watchdog.Terminate();
}

TEST_F(SessionWatchDogTest, SessionCanSendCleanupCommandTest) {
  volatile float cpu_loads[16];

  mozc::SessionWatchDog watchdog(2);

  cpu_loads[0] = 0.0;
  cpu_loads[1] = 0.0;

  // suspend
  EXPECT_FALSE(watchdog.CanSendCleanupCommand(cpu_loads, 2, 5, 0));

  // not suspend
  EXPECT_TRUE(watchdog.CanSendCleanupCommand(cpu_loads, 2, 1, 0));

  // error (the same time stamp)
  EXPECT_FALSE(watchdog.CanSendCleanupCommand(cpu_loads, 2, 0, 0));

  cpu_loads[0] = 0.4;
  cpu_loads[1] = 0.5;
  cpu_loads[2] = 0.4;
  cpu_loads[3] = 0.6;
  // average CPU loads >= 0.33
  EXPECT_FALSE(watchdog.CanSendCleanupCommand(cpu_loads, 4, 1, 0));

  cpu_loads[0] = 0.1;
  cpu_loads[1] = 0.1;
  cpu_loads[2] = 0.7;
  cpu_loads[3] = 0.7;
  // recent CPU loads >= 0.66
  EXPECT_FALSE(watchdog.CanSendCleanupCommand(cpu_loads, 4, 1, 0));

  cpu_loads[0] = 1.0;
  cpu_loads[1] = 1.0;
  cpu_loads[2] = 1.0;
  cpu_loads[3] = 1.0;
  cpu_loads[4] = 0.1;
  cpu_loads[5] = 0.1;
  // average CPU loads >= 0.33
  EXPECT_FALSE(watchdog.CanSendCleanupCommand(cpu_loads, 6, 1, 0));

  cpu_loads[0] = 0.1;
  cpu_loads[1] = 0.1;
  cpu_loads[2] = 0.1;
  cpu_loads[3] = 0.1;
  EXPECT_TRUE(watchdog.CanSendCleanupCommand(cpu_loads, 4, 1, 0));
}

}  // namespace mozc
