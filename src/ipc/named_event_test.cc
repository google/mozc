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

#include "ipc/named_event.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/clock.h"
#include "base/system_util.h"
#include "base/thread2.h"
#include "testing/googletest.h"
#include "testing/gunit.h"
#include "absl/flags/flag.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace mozc {
namespace {

constexpr char kName[] = "named_event_test";

class NamedEventListenerThread {
 public:
  NamedEventListenerThread(const char *name, absl::Duration initial_wait,
                           absl::Duration wait, int max_num_wait)
      : listener_(name), first_triggered_time_(0) {
    EXPECT_TRUE(listener_.IsAvailable());
    thread_ =
        mozc::Thread2([initial_wait, wait, max_num_wait, &listener = listener_,
                       &first_triggered_time = first_triggered_time_]() {
          absl::SleepFor(initial_wait);
          for (int i = 0; i < max_num_wait; ++i) {
            if (listener.Wait(wait)) {
              first_triggered_time = absl::ToUnixNanos(Clock::GetAbslTime());
              return;
            }
          }
        });
  }

  absl::Time first_triggered_time() const {
    return absl::FromUnixNanos(first_triggered_time_);
  }

  bool IsTriggered() const { return first_triggered_time_ != 0; }

  void Join() { thread_.Join(); }

 private:
  NamedEventListener listener_;
  mozc::Thread2 thread_;

  // std::atomic requires the type to be is_trivially_copyable, and some
  // (older?) versions of msvc and clang think absl::Time is not.
  // Store UnixNanos instead of absl::Time.
  std::atomic<uint64_t> first_triggered_time_;
};

class NamedEventTest : public testing::Test {
  void SetUp() override {
    original_user_profile_directory_ = SystemUtil::GetUserProfileDirectory();
    SystemUtil::SetUserProfileDirectory(absl::GetFlag(FLAGS_test_tmpdir));
  }

  void TearDown() override {
    SystemUtil::SetUserProfileDirectory(original_user_profile_directory_);
  }

 private:
  std::string original_user_profile_directory_;
};

TEST_F(NamedEventTest, NamedEventBasicTest) {
  NamedEventListenerThread listener(kName, absl::ZeroDuration(),
                                    absl::Milliseconds(50), 100);
  absl::SleepFor(absl::Milliseconds(200));
  NamedEventNotifier notifier(kName);
  ASSERT_TRUE(notifier.IsAvailable());
  const absl::Time notified_time = Clock::GetAbslTime();
  notifier.Notify();
  listener.Join();

  // There is a chance that |listener| is not triggered.
  if (listener.IsTriggered()) {
    EXPECT_LT(notified_time, listener.first_triggered_time());
  }
}

TEST_F(NamedEventTest, IsAvailableTest) {
  {
    NamedEventListener l(kName);
    EXPECT_TRUE(l.IsAvailable());
    NamedEventNotifier n(kName);
    EXPECT_TRUE(n.IsAvailable());
  }

  // no listener
  {
    NamedEventNotifier n(kName);
    EXPECT_FALSE(n.IsAvailable());
  }
}

TEST_F(NamedEventTest, IsOwnerTest) {
  NamedEventListener l1(kName);
  EXPECT_TRUE(l1.IsOwner());
  EXPECT_TRUE(l1.IsAvailable());
  NamedEventListener l2(kName);
  EXPECT_FALSE(l2.IsOwner());  // the instance is owneded by l1
  EXPECT_TRUE(l2.IsAvailable());
}

TEST_F(NamedEventTest, NamedEventMultipleListenerTest) {
  std::vector<std::unique_ptr<NamedEventListenerThread>> listeners;
  for (int i = 0; i < 4; ++i) {
    listeners.push_back(std::make_unique<NamedEventListenerThread>(
        kName, absl::Milliseconds(33 * i), absl::Milliseconds(50), 100));
  }

  absl::SleepFor(absl::Milliseconds(200));

  // all |kNumRequests| listener events should be raised
  // at once with single notifier event
  NamedEventNotifier notifier(kName);
  ASSERT_TRUE(notifier.IsAvailable());
  const absl::Time notify_time = Clock::GetAbslTime();
  ASSERT_TRUE(notifier.Notify());

  for (auto &listener : listeners) {
    listener->Join();
  }

  for (const auto &listener : listeners) {
    // There is a chance that |listener| is not triggered.
    if (listener->IsTriggered()) {
      EXPECT_LT(notify_time, listener->first_triggered_time());
    }
  }
}

TEST_F(NamedEventTest, NamedEventPathLengthTest) {
#ifndef _WIN32
  const std::string name_path = NamedEventUtil::GetEventPath(kName);
  // length should be less than 14 not includeing terminating null.
  EXPECT_EQ(name_path.length(), 13);
#endif  // _WIN32
}

}  // namespace
}  // namespace mozc
