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

#include "base/thread2.h"

#include <atomic>
#include <string>
#include <utility>

#include "testing/gunit.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace mozc {
namespace {

class CopyCounter {
 public:
  explicit CopyCounter(std::atomic<int> *copied) : copied_(copied) {}

  CopyCounter(const CopyCounter &other) { *this = other; }
  CopyCounter(CopyCounter &&other) = default;

  CopyCounter &operator=(const CopyCounter &other) {
    this->copied_ = other.copied_;

    copied_->fetch_add(1);
    return *this;
  }
  CopyCounter &operator=(CopyCounter &&other) = default;

 private:
  std::atomic<int> *copied_;
};

TEST(ThreadTest, SpawnsSuccessfully) {
  std::atomic<int> counter = 0;

  Thread2 t1([&counter] {
    for (int i = 1; i <= 100; ++i) {
      counter.fetch_add(i);
    }
  });
  Thread2 t2([&counter](int x) { counter.fetch_add(x); }, 50);
  Thread2 t3([&counter](int x, int y) { counter.fetch_sub(x * y); }, 10, 10);
  t1.Join();
  t2.Join();
  t3.Join();
  EXPECT_EQ(counter.load(), 5000);
}

TEST(ThreadTest, CopiesThingsAtMostOnce) {
  std::atomic<int> copied1 = 0, copied2 = 0;
  CopyCounter cc1(&copied1), cc2(&copied2);
  Thread2 t(
      [](const CopyCounter &, const CopyCounter &) {
        absl::SleepFor(absl::Milliseconds(100));
      },
      cc1, std::move(cc2));
  t.Join();
  EXPECT_EQ(copied1, 1);
  EXPECT_EQ(copied2, 0);
}

TEST(CreateThreadWithDoneNotificationTest, NotifiesOnCompletion) {
  absl::Notification done;
  Thread2 t = CreateThreadWithDoneNotification(
      &done, [] { absl::SleepFor(absl::Milliseconds(100)); });
  EXPECT_FALSE(done.HasBeenNotified());
  t.Join();
  EXPECT_TRUE(done.HasBeenNotified());
}

TEST(CreateThreadWithDoneNotificationTest, NotifiesOnlyWhenCompleted) {
  absl::Notification done;
  Thread2 t = CreateThreadWithDoneNotification(
      &done, [](absl::Duration d) { absl::SleepFor(d); },
      absl::Milliseconds(100));
  EXPECT_FALSE(done.HasBeenNotified());
  done.WaitForNotification();
  EXPECT_TRUE(done.HasBeenNotified());
  t.Join();
}

TEST(CreateThreadWithDoneNotificationTest, CopiesThingsAtMostOnce) {
  std::atomic<int> copied1 = 0, copied2 = 0;
  CopyCounter cc1(&copied1), cc2(&copied2);

  absl::Notification done;
  Thread2 t = CreateThreadWithDoneNotification(
      &done,
      [](const CopyCounter &, const CopyCounter &) {
        absl::SleepFor(absl::Milliseconds(100));
      },
      cc1, std::move(cc2));
  done.WaitForNotification();
  t.Join();
  EXPECT_EQ(copied1, 1);
  EXPECT_EQ(copied2, 0);
}

}  // namespace
}  // namespace mozc
