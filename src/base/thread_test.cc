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

#include "base/thread.h"

#include "base/util.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

class TestThread : public Thread {
 public:
  explicit TestThread(int time) : time_(time), invoked_(false) {}
  ~TestThread() override = default;

  void Run() override {
    invoked_ = true;
    Util::Sleep(time_);
  }

  bool invoked() const { return invoked_; }
  void clear_invoked() { invoked_ = false; }

 private:
  int time_;
  bool invoked_;
};

}  // namespace

TEST(ThreadTest, BasicThreadTest) {
  {
    TestThread t(1000);
    t.Start("BasicThreadTest");
    EXPECT_TRUE(t.IsRunning());
    t.Join();
    EXPECT_FALSE(t.IsRunning());
    EXPECT_TRUE(t.invoked());
  }

  {
    TestThread t(1000);
    t.clear_invoked();
    t.Start("BasicThreadTest");

    Util::Sleep(3000);
    EXPECT_FALSE(t.IsRunning());
    EXPECT_TRUE(t.invoked());
    t.Join();
  }

  {
    TestThread t(3000);
    t.Start("BasicThreadTest");
    Util::Sleep(1000);
    t.Terminate();
    Util::Sleep(100);
    EXPECT_FALSE(t.IsRunning());
  }
}

TEST(ThreadTest, RestartTest) {
  {
    TestThread t(1000);
    t.clear_invoked();
    t.Start("RestartTest");
    EXPECT_TRUE(t.IsRunning());
    t.Join();
    EXPECT_TRUE(t.invoked());
    EXPECT_FALSE(t.IsRunning());
    t.clear_invoked();
    t.Start("RestartTest");
    EXPECT_TRUE(t.IsRunning());
    t.Join();
    EXPECT_TRUE(t.invoked());
    EXPECT_FALSE(t.IsRunning());
    t.clear_invoked();
    t.Start("RestartTest");
    EXPECT_TRUE(t.IsRunning());
    t.Join();
    EXPECT_TRUE(t.invoked());
    EXPECT_FALSE(t.IsRunning());
  }
}

}  // namespace mozc
