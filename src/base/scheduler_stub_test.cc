// Copyright 2010-2020, Google Inc.
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

#include "base/scheduler_stub.h"

#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

static int g_counter = 0;
static bool g_result = true;

bool TestFunc(void *data) {
  ++g_counter;
  return g_result;
}

class SchedulerStubTest : public ::testing::Test {
 protected:
  void SetUp() override {
    g_counter = 0;
    g_result = true;
  }

  void TearDown() override {
    g_counter = 0;
    g_result = true;
  }
};

TEST_F(SchedulerStubTest, AddRemoveJob) {
  SchedulerStub scheduler_stub;
  EXPECT_FALSE(scheduler_stub.HasJob("Test"));
  scheduler_stub.AddJob(
      Scheduler::JobSetting("Test", 1000, 100000, 5000, 0, &TestFunc, nullptr));
  EXPECT_TRUE(scheduler_stub.HasJob("Test"));
  EXPECT_EQ(0, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(0, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(0, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(0, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(0, g_counter);
  scheduler_stub.PutClockForward(1000);  // delay_start
  EXPECT_EQ(1, g_counter);

  scheduler_stub.PutClockForward(1000);  // default_interval
  EXPECT_EQ(2, g_counter);

  scheduler_stub.PutClockForward(1000);  // default_interval
  EXPECT_EQ(3, g_counter);

  scheduler_stub.RemoveJob("Test");
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(3, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(3, g_counter);
  EXPECT_FALSE(scheduler_stub.HasJob("Test"));
}

TEST_F(SchedulerStubTest, BackOff) {
  SchedulerStub scheduler_stub;
  scheduler_stub.AddJob(
      Scheduler::JobSetting("Test", 1000, 6000, 3000, 0, &TestFunc, nullptr));
  g_result = false;
  EXPECT_EQ(0, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(0, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(0, g_counter);
  scheduler_stub.PutClockForward(1000);  // delay_start
  EXPECT_EQ(1, g_counter);

  scheduler_stub.PutClockForward(1000);  // backoff (wait 1000 + 1000)
  EXPECT_EQ(1, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(2, g_counter);

  scheduler_stub.PutClockForward(1000);  // backoff (wait 1000 + 1000 * 2)
  EXPECT_EQ(2, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(2, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(3, g_counter);

  scheduler_stub.PutClockForward(1000);  // backoff (wait 1000 + 1000 * 4)
  EXPECT_EQ(3, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(3, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(3, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(3, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(4, g_counter);

  // backoff (wait 1000 + 1000 * 8) > 6000, use same delay
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(4, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(4, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(4, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(4, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(5, g_counter);

  g_result = true;

  // use same delay
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(5, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(5, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(5, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(5, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(6, g_counter);

  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(7, g_counter);
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(8, g_counter);
}

TEST_F(SchedulerStubTest, AddRemoveJobs) {
  SchedulerStub scheduler_stub;
  scheduler_stub.AddJob(Scheduler::JobSetting("Test1", 1000, 100000, 1000, 0,
                                              &TestFunc, nullptr));
  EXPECT_EQ(0, g_counter);
  scheduler_stub.PutClockForward(1000);  // delay
  EXPECT_EQ(1, g_counter);

  scheduler_stub.AddJob(Scheduler::JobSetting("Test2", 1000, 100000, 1000, 0,
                                              &TestFunc, nullptr));

  scheduler_stub.PutClockForward(1000);  // delay + interval
  EXPECT_EQ(3, g_counter);

  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(5, g_counter);

  scheduler_stub.RemoveJob("Test3");  // nothing happens
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(7, g_counter);

  scheduler_stub.RemoveJob("Test2");
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(8, g_counter);

  scheduler_stub.RemoveAllJobs();
  scheduler_stub.PutClockForward(1000);
  EXPECT_EQ(8, g_counter);
}
}  // namespace
}  // namespace mozc
