// Copyright 2010, Google Inc.
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
#include "base/util.h"
#include "base/timer.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {
class TestTimer : public Timer {
 public:
  TestTimer() : counter_(0) {}

  void Signaled() {
    ++counter_;
  }

  int counter() const {
    return counter_;
  }

 private:
  int counter_;
};

static int g_counter = 0;
class DelayTimer : public Timer {
 public:
  void Signaled() {
    Util::Sleep(1000);
    g_counter++;
    *v = 1;   // accessing to local object
  }

  DelayTimer() : v(new int) {
    *v = 0;
  }

  virtual ~DelayTimer() {
    Stop();
    delete v;
    v = NULL;
  }

  int *v;
};
}  // namespace

TEST(TimerTest, TimerTestOneShot) {
  TestTimer test_timer;
  EXPECT_TRUE(test_timer.Start(500, 0));
  Util::Sleep(100);
  EXPECT_EQ(0, test_timer.counter());
  Util::Sleep(500);
  EXPECT_EQ(1, test_timer.counter());

  EXPECT_TRUE(test_timer.Start(200, 0));
  Util::Sleep(100);
  EXPECT_EQ(1, test_timer.counter());

  Util::Sleep(500);
  EXPECT_EQ(2, test_timer.counter());

  test_timer.Stop();
  EXPECT_EQ(2, test_timer.counter());

  Util::Sleep(1000);
  EXPECT_EQ(2, test_timer.counter());
}

TEST(TimerTest, TimerTestInterval) {
  TestTimer test_timer;
  EXPECT_TRUE(test_timer.Start(0, 1000));
  Util::Sleep(5500);
  EXPECT_EQ(6, test_timer.counter());

  EXPECT_TRUE(test_timer.Start(0, 1000));
  Util::Sleep(5600);
  EXPECT_EQ(12, test_timer.counter());

  test_timer.Stop();
  EXPECT_EQ(12, test_timer.counter());
  Util::Sleep(1000);
  EXPECT_EQ(12, test_timer.counter());
}

TEST(TimerTest, TimerTestOverrun) {
  {
    DelayTimer *delay_timer = new DelayTimer;
    EXPECT_TRUE(delay_timer->Start(10, 0));
    Util::Sleep(100);
    // Delete the timer here.
    // Note that signal handler is doing "Sleep(1000)" and
    // accessing to a local member variable.
    // Deleting the timer object may cause segmentation fault,
    // since the singal handelr will access to freed object.
    // The expected behavior is that the destructor
    // waits for all pending functions.
    delete delay_timer;
    delay_timer = NULL;
  }
  Util::Sleep(2000);
  EXPECT_EQ(1, g_counter);
}
}  // namespace mozc
