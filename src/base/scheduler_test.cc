// Copyright 2010-2012, Google Inc.
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

#include "base/scheduler.h"

#include <algorithm>

#include "base/util.h"
#include "base/mutex.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

static int g_counter1 = 0;
static Mutex g_counter_mutex1;
bool TestFuncOk1(void *data) {
  scoped_lock l(&g_counter_mutex1);
  ++g_counter1;
  return true;
}

static int g_counter2 = 0;
static Mutex g_counter_mutex2;
bool TestFuncOk2(void *data) {
  scoped_lock l(&g_counter_mutex2);
  ++g_counter2;
  return true;
}

static int g_counter_ng = 0;
bool TestFuncNg(void *data) {
  ++g_counter_ng;
  return false;
}

static int g_num = 0;
bool TestFunc(void *num) {
  CHECK(num);
  g_num = *(reinterpret_cast<int *>(num));
  return true;
}

// The callback for testing random_delay.
static Mutex g_delay_mutex;
static volatile int g_num_run[6];
static volatile int g_total_run;
static volatile bool g_is_incremented;
bool TestFuncForRandomDelay(void *data) {
  // Take the time at first every time to avoid the delay of the lock checking,
  // below.
  uint64 sec;
  uint32 usec;
  Util::GetTimeOfDay(&sec, &usec);

  scoped_lock l(&g_delay_mutex);
  if (!g_is_incremented) {
    g_is_incremented = true;

    uint64 start_time = *reinterpret_cast<const uint64*>(data);
    uint64 current_time = sec * 1000000 + usec;
    // Delay in usec.
    int64 delay = current_time - start_time;
    CHECK_GE(delay, 0) << delay;

    // Ceiling by 5 to handle overflow.
    ++g_num_run[min(delay / 100000, 5LL)];
    ++g_total_run;
  }
  return true;
}

}  // namespace


TEST(SchedulerTest, SchedulerTestData) {
  const string kTestJob = "Test";
  g_num = 0;
  int num = 10;
  Scheduler::AddJob(Scheduler::JobSetting(
      kTestJob, 100000, 100000, 500, 0, &TestFunc, &num));
  EXPECT_EQ(0, g_num);
  Util::Sleep(1000);
  EXPECT_EQ(10, g_num);
  Scheduler::RemoveJob(kTestJob);
}

TEST(SchedulerTest, SchedulerTestDelay) {
  const string kTestJob = "Test";
  g_counter1 = 0;
  Scheduler::AddJob(Scheduler::JobSetting(
      kTestJob, 100000, 100000, 500, 0, &TestFuncOk1, NULL));
  EXPECT_EQ(0, g_counter1);
  Util::Sleep(1000);
  EXPECT_EQ(1, g_counter1);
  Scheduler::RemoveJob(kTestJob);
}

TEST(SchedulerTest, SchedulerTestRandomDelay) {
  // Reset counters for this test.
  g_total_run = 0;
  for (int i = 0; i < 6; ++i) {
    g_num_run[i] = 0;
  }

  for (int i = 0; i < 100; ++i) {
    const string kTestJob = "Test";
    {
      scoped_lock l(&g_delay_mutex);
      g_is_incremented = false;
    }
    uint64 sec;
    uint32 usec;
    Util::GetTimeOfDay(&sec, &usec);
    uint64 start_time = sec * 1000000 + usec;
    Scheduler::AddJob(Scheduler::JobSetting(
        kTestJob, 100000, 100000, 100, 500,
        &TestFuncForRandomDelay, &start_time));
    // Wait 700 msec for the task. This is 100ms longer than the expected
    // maximum duration to absorb smaller errors of the system.
    Util::Sleep(700);
    EXPECT_TRUE(g_is_incremented);
    Scheduler::RemoveJob(kTestJob);
  }

  EXPECT_EQ(100, g_total_run);
  EXPECT_LT(0, g_num_run[1]);
  EXPECT_LT(0, g_num_run[2]);
  EXPECT_LT(0, g_num_run[3]);
  EXPECT_LT(0, g_num_run[4]);
  EXPECT_LT(0, g_num_run[5]);
}

TEST(SchedulerTest, SchedulerTestNoDelay) {
  const string kTestJob = "Test";
  g_counter1 = 0;
  Scheduler::AddJob(Scheduler::JobSetting(
      kTestJob, 1000, 1000, 0, 0, &TestFuncOk1, NULL));
  Util::Sleep(500);
  EXPECT_EQ(1, g_counter1);
  Scheduler::RemoveJob(kTestJob);
}

// Update each variables for each tasks and count the number each
// functions are called.
TEST(SchedulerTest, SchedulerTestInterval) {
  const string kTestJob1 = "Test1";
  const string kTestJob2 = "Test2";
  {
    g_counter1 = 0;
    Scheduler::AddJob(Scheduler::JobSetting(
        kTestJob1, 1000, 1000, 500, 0, &TestFuncOk1, NULL));

    Util::Sleep(3000);
    EXPECT_EQ(3, g_counter1);
    Scheduler::RemoveJob(kTestJob1);

    Util::Sleep(3000);
    EXPECT_EQ(3, g_counter1);
  }
  {
    g_counter1 = 0;
    Scheduler::AddJob(Scheduler::JobSetting(
        kTestJob1, 1000, 1000, 500, 0, &TestFuncOk1, NULL));

    Util::Sleep(3000);
    EXPECT_EQ(3, g_counter1);

    g_counter2 = 0;
    Scheduler::AddJob(Scheduler::JobSetting(
        kTestJob2, 1000, 1000, 500, 0, &TestFuncOk2, NULL));

    Util::Sleep(3000);
    EXPECT_EQ(6, g_counter1);
    EXPECT_EQ(3, g_counter2);
    Scheduler::RemoveJob(kTestJob1);

    Util::Sleep(3000);
    EXPECT_EQ(6, g_counter1);
    EXPECT_EQ(6, g_counter2);
  }
  Scheduler::RemoveJob(kTestJob2);

  Util::Sleep(3000);
  EXPECT_EQ(6, g_counter1);
  EXPECT_EQ(6, g_counter2);
}

TEST(SchedulerTest, SchedulerTestRemoveAll) {
  const string kTestJob1 = "Test1";
  const string kTestJob2 = "Test2";

  g_counter1 = 0;
  g_counter2 = 0;
  Scheduler::AddJob(Scheduler::JobSetting(
      kTestJob1, 1000, 1000, 500, 0, &TestFuncOk1, NULL));
  Scheduler::AddJob(Scheduler::JobSetting(
      kTestJob2, 1000, 1000, 500, 0, &TestFuncOk2, NULL));
  Util::Sleep(3000);
  EXPECT_EQ(3, g_counter1);
  EXPECT_EQ(3, g_counter2);

  Scheduler::RemoveAllJobs();

  Util::Sleep(3000);
  EXPECT_EQ(3, g_counter1);
  EXPECT_EQ(3, g_counter2);
}

TEST(SchedulerTest, SchedulerTestFailed) {
  const string kTestJob = "Test";
  g_counter_ng = 0;
  Scheduler::AddJob(Scheduler::JobSetting(
      kTestJob, 1000, 5000, 500, 0, &TestFuncNg, NULL));

  Util::Sleep(1000);  // 1000 count=1 next 2000
  EXPECT_EQ(1, g_counter_ng);
  Util::Sleep(1000);  // 2000
  EXPECT_EQ(1, g_counter_ng);
  Util::Sleep(1000);  // 3000 count=2 next 3000
  EXPECT_EQ(2, g_counter_ng);
  Util::Sleep(3000);  // 6000 count=4 next 5000
  EXPECT_EQ(3, g_counter_ng);
  Util::Sleep(5000);  // 11000 count=4 next 5000
  EXPECT_EQ(4, g_counter_ng);
  Util::Sleep(5000);  // 16000
  EXPECT_EQ(5, g_counter_ng);

  Scheduler::RemoveJob(kTestJob);
}

class NameCheckScheduler : public Scheduler::SchedulerInterface {
 public:
  explicit NameCheckScheduler(const string &expected_name)
      : expected_name_(expected_name) {}

  void RemoveAllJobs() {}
  bool RemoveJob(const string &name) { return true; }
  bool AddJob(const Scheduler::JobSetting &job_setting) {
    return (expected_name_ == job_setting.name());
  }

 private:
  const string expected_name_;
};

TEST(SchedulerTest, SchedulerHandler) {
  scoped_ptr<NameCheckScheduler> scheduler_mock(new NameCheckScheduler("test"));
  Scheduler::SetSchedulerHandler(scheduler_mock.get());
  EXPECT_TRUE(Scheduler::AddJob(
      Scheduler::JobSetting("test", 0, 0, 0, 0, NULL, NULL)));
  EXPECT_FALSE(Scheduler::AddJob(
      Scheduler::JobSetting("not_test", 0, 0, 0, 0, NULL, NULL)));
  EXPECT_TRUE(Scheduler::RemoveJob("not_have"));
  Scheduler::RemoveAllJobs();
  Scheduler::SetSchedulerHandler(NULL);
}
}  // namespace mozc
