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

#include "base/scheduler.h"

#include <algorithm>
#include <memory>

#include "base/logging.h"
#include "base/unnamed_event.h"
#include "base/util.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

const int32 kTimeout = 30 * 1000;  // 30 sec.
const int32 kNoRandomDelay = 0;
const int32 kImmediately = 0;
const int32 kShortPeriod = 10;              // 10 millisec.
const int32 kMediumPeriod = 100;            // 100 millisec.
const int32 kTooLongTime = 24 * 60 * 1000;  // 24 hours.

class SchedulerTest : public testing::Test {
 public:
  class ScopedJob {
   public:
    explicit ScopedJob(const Scheduler::JobSetting &setting)
        : name_(setting.name()) {
      Scheduler::AddJob(setting);
    }
    ~ScopedJob() { Scheduler::RemoveJob(name_); }

   private:
    const std::string name_;
  };

 protected:
  void TearDown() override { Scheduler::RemoveAllJobs(); }
};

TEST_F(SchedulerTest, SimpleJob) {
  struct SharedInfo {
    UnnamedEvent first_event;
    UnnamedEvent second_event;
  };

  class TestCallback {
   public:
    static bool Do(void *ptr) {
      SharedInfo *info = static_cast<SharedInfo *>(ptr);
      static bool first_time = true;
      if (first_time) {
        EXPECT_TRUE(info->first_event.Notify());
        first_time = false;
        return true;  // continue
      }
      EXPECT_TRUE(info->second_event.Notify());
      return false;  // stop
    }
  };

  SharedInfo info;
  ASSERT_TRUE(info.first_event.IsAvailable());
  ASSERT_TRUE(info.second_event.IsAvailable());

  ScopedJob job(Scheduler::JobSetting("Test", kShortPeriod, kShortPeriod,
                                      kImmediately, kNoRandomDelay,
                                      &TestCallback::Do, &info));
  ASSERT_TRUE(info.first_event.Wait(kTimeout));
  ASSERT_TRUE(info.second_event.Wait(kTimeout));
}

TEST_F(SchedulerTest, RemoveJob) {
  struct SharedInfo {
    SharedInfo() : running(false) {}
    UnnamedEvent first_event;
    volatile bool running;
  };

  class TestCallback {
   public:
    static bool Do(void *ptr) {
      SharedInfo *info = static_cast<SharedInfo *>(ptr);
      info->running = true;
      static bool first_time = true;
      if (first_time) {
        EXPECT_TRUE(info->first_event.Notify());
        first_time = false;
      }
      return true;  // continue
    }
  };

  SharedInfo info;
  ASSERT_TRUE(info.first_event.IsAvailable());
  {
    ScopedJob job(Scheduler::JobSetting("Test", kShortPeriod, kShortPeriod,
                                        kImmediately, kNoRandomDelay,
                                        &TestCallback::Do, &info));
    // Make sure that the job is running.
    ASSERT_TRUE(info.first_event.Wait(kTimeout));
    ASSERT_TRUE(info.running);
    // Job is removed here.
  }

  info.running = false;

  // The sleep time is arbitrary, but longer sleep time makes the assertion
  // below stronger. Feel free to shorten it.
  Util::Sleep(kMediumPeriod);

  // Job should not be running anymore.
  EXPECT_FALSE(info.running);
}

TEST_F(SchedulerTest, Delay) {
  class TestCallback {
   public:
    static bool Do(void *ptr) {
      UnnamedEvent *event = static_cast<UnnamedEvent *>(ptr);
      EXPECT_TRUE(event->Notify());
      return false;
    }
  };

  {
    UnnamedEvent event;
    ASSERT_TRUE(event.IsAvailable());
    // This job will be delayed |kTooLongTime|.
    ScopedJob job(Scheduler::JobSetting("Test", kShortPeriod, kShortPeriod,
                                        kTooLongTime, kNoRandomDelay,
                                        &TestCallback::Do, &event));
    // The timeout period is arbitrary, but longer timeout period makes the
    // assertion stronger. Feel free to shorten it.
    ASSERT_FALSE(event.Wait(kMediumPeriod));
  }

  {
    UnnamedEvent event;
    ASSERT_TRUE(event.IsAvailable());
    // This job will be delayed |kShortPeriod|.
    ScopedJob job(Scheduler::JobSetting("Test", kShortPeriod, kShortPeriod,
                                        kShortPeriod, kNoRandomDelay,
                                        &TestCallback::Do, &event));
    ASSERT_TRUE(event.Wait(kTimeout));
  }
}

TEST_F(SchedulerTest, RandomDelay) {
  struct SharedInfo {
    UnnamedEvent first_event;
    UnnamedEvent second_event;
  };

  class TestCallback {
   public:
    static bool Do(void *ptr) {
      SharedInfo *info = static_cast<SharedInfo *>(ptr);
      static bool first_time = true;
      if (first_time) {
        EXPECT_TRUE(info->first_event.Notify());
        first_time = false;
        return true;  // continue
      }
      EXPECT_TRUE(info->second_event.Notify());
      return false;  // stop
    }
  };

  SharedInfo info;
  ASSERT_TRUE(info.first_event.IsAvailable());
  ASSERT_TRUE(info.second_event.IsAvailable());

  ScopedJob job(Scheduler::JobSetting("Test", kShortPeriod, kShortPeriod,
                                      kImmediately, kMediumPeriod,
                                      &TestCallback::Do, &info));
  ASSERT_TRUE(info.first_event.Wait(kTimeout));
  ASSERT_TRUE(info.second_event.Wait(kTimeout));
}

TEST_F(SchedulerTest, DontBlockOtherJobs) {
  struct SharedInfo {
    UnnamedEvent notify_event;
    UnnamedEvent quit_event;
  };
  class BlockingCallback {
   public:
    static bool Do(void *ptr) {
      SharedInfo *info = static_cast<SharedInfo *>(ptr);
      const bool succeeded = info->notify_event.Notify();
      EXPECT_TRUE(succeeded);
      if (!succeeded) {
        return false;
      }
      EXPECT_TRUE(info->quit_event.Wait(kTimeout));
      return false;  // stop
    }
  };

  class SecondaryCallback {
   public:
    static bool Do(void *ptr) {
      UnnamedEvent *event = static_cast<UnnamedEvent *>(ptr);
      EXPECT_TRUE(event->Notify());
      return false;  // stop
    }
  };

  SharedInfo info;
  ASSERT_TRUE(info.notify_event.IsAvailable());
  ASSERT_TRUE(info.quit_event.IsAvailable());
  ScopedJob blocking_job(Scheduler::JobSetting(
      "TestJob1", kShortPeriod, kShortPeriod, kImmediately, kNoRandomDelay,
      &BlockingCallback::Do, &info));
  ASSERT_TRUE(info.notify_event.Wait(kTimeout));

  UnnamedEvent event;
  ScopedJob secondary_job(Scheduler::JobSetting(
      "TestJob2", kShortPeriod, kShortPeriod, kImmediately, kNoRandomDelay,
      &SecondaryCallback::Do, &event));
  ASSERT_TRUE(event.Wait(kTimeout));

  // Unblock |blocking_job|.
  EXPECT_TRUE(info.quit_event.Notify());
}

class NameCheckScheduler : public Scheduler::SchedulerInterface {
 public:
  explicit NameCheckScheduler(const std::string &expected_name)
      : expected_name_(expected_name) {}

  void RemoveAllJobs() override {}
  bool RemoveJob(const std::string &name) override { return true; }
  bool AddJob(const Scheduler::JobSetting &job_setting) override {
    return (expected_name_ == job_setting.name());
  }
  bool HasJob(const std::string &name) const override {
    return expected_name_ == name;
  }

 private:
  const std::string expected_name_;
};

TEST(SchedulerInterfaceTest, SchedulerHandler) {
  std::unique_ptr<NameCheckScheduler> scheduler_mock(
      new NameCheckScheduler("test"));
  Scheduler::SetSchedulerHandler(scheduler_mock.get());
  EXPECT_TRUE(Scheduler::AddJob(
      Scheduler::JobSetting("test", 0, 0, 0, 0, nullptr, nullptr)));
  EXPECT_FALSE(Scheduler::AddJob(
      Scheduler::JobSetting("not_test", 0, 0, 0, 0, nullptr, nullptr)));
  EXPECT_TRUE(Scheduler::RemoveJob("not_have"));
  Scheduler::RemoveAllJobs();
  Scheduler::SetSchedulerHandler(nullptr);
}

}  // namespace
}  // namespace mozc
