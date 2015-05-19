// Copyright 2010-2013, Google Inc.
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

#include "base/base.h"
#include "base/logging.h"
#include "base/thread.h"
#include "base/unnamed_event.h"
#include "base/util.h"
#include "testing/base/public/gunit.h"

namespace {
class UnnamedEventNotifierThread: public mozc::Thread {
 public:
  UnnamedEventNotifierThread(mozc::UnnamedEvent *event,
                             int timeout)
      : event_(event), timeout_(timeout) {}

 public:
  void Run() {
    mozc::Util::Sleep(timeout_);
    LOG(INFO) << "Notify event";
    event_->Notify();
  }

 private:
  mozc::UnnamedEvent *event_;
  int timeout_;
};
}  // namespace

TEST(UnnamedEventTest, UnnamedEventTest) {
  {
    mozc::UnnamedEvent event;
    EXPECT_TRUE(event.IsAvailable());
    UnnamedEventNotifierThread t(&event, 400);
    t.Start();
    EXPECT_FALSE(event.Wait(100));
    EXPECT_FALSE(event.Wait(100));
    EXPECT_TRUE(event.Wait(800));
    t.Join();
  }

  {
    mozc::UnnamedEvent event;
    EXPECT_TRUE(event.IsAvailable());
    UnnamedEventNotifierThread t(&event, 100);
    t.Start();
    EXPECT_TRUE(event.Wait(2000));
    t.Join();
  }

  {
    mozc::UnnamedEvent event;
    EXPECT_TRUE(event.IsAvailable());
    UnnamedEventNotifierThread t(&event, 3000);
    t.Start();
    EXPECT_FALSE(event.Wait(100));
    EXPECT_FALSE(event.Wait(1000));
    EXPECT_FALSE(event.Wait(1000));
    t.Join();
  }

  {
    mozc::UnnamedEvent event;
    EXPECT_TRUE(event.IsAvailable());
    UnnamedEventNotifierThread t(&event, 2000);
    t.Start();
    EXPECT_FALSE(event.Wait(500));
    EXPECT_FALSE(event.Wait(500));
    EXPECT_TRUE(event.Wait(5000));
    t.Join();
  }
}

TEST(UnnamedEventTest, NotifyBeforeWait) {
  mozc::UnnamedEvent event;
  ASSERT_TRUE(event.Notify());
  EXPECT_TRUE(event.Wait(100));
}

TEST(UnnamedEventTest, DoubleNotifyBeforeWait) {
  mozc::UnnamedEvent event;
  ASSERT_TRUE(event.Notify());
  ASSERT_TRUE(event.Notify());
  EXPECT_TRUE(event.Wait(100));
  EXPECT_FALSE(event.Wait(100));
}
