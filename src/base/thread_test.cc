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

#include <stdlib.h>
#include "base/thread.h"
#include "base/util.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

class TestThread : public Thread {
 public:
  TestThread(int time) : time_(time) {}
  void Run() {
    Util::Sleep(time_);
  }

 private:
  int time_;
};
}  // namespace

TEST(ThreadTest, BasicThreadTest) {
  {
    TestThread t(1000);
    t.Start();
    EXPECT_TRUE(t.IsRunning());
    t.Join();
    EXPECT_FALSE(t.IsRunning());
  }

  {
    TestThread t(3000);
    t.Start();

    Util::Sleep(1000);
    EXPECT_TRUE(t.IsRunning());

    Util::Sleep(1000);
    EXPECT_TRUE(t.IsRunning());

    Util::Sleep(2000);
    EXPECT_FALSE(t.IsRunning());
  }

  {
    TestThread t(3000);
    t.Start();

    Util::Sleep(1000);
    EXPECT_TRUE(t.IsRunning());

    t.Terminate();
    Util::Sleep(100);
    EXPECT_FALSE(t.IsRunning());
  }
}

namespace {
TLS_KEYWORD int g_tls_value = 0;
TLS_KEYWORD int g_tls_values[100] =  { 0 };

class TLSThread : public Thread {
 public:
  void Run() {
    ++g_tls_value;
    ++g_tls_value;
    ++g_tls_value;
    for (int i = 0; i < 100; ++i) {
      g_tls_values[i] = i;
    }
    for (int i = 0; i < 100; ++i) {
      g_tls_values[i] += i;
    }
    int result = 0;
    for (int i = 0; i < 100; ++i) {
      result += g_tls_values[i];
    }
    EXPECT_EQ(3, g_tls_value);
    EXPECT_EQ(9900, result);
  }
};
}

TEST(ThreadTest, TLSTest) {
#ifdef HAVE_TLS
  vector<TLSThread *> threads;
  for (int i = 0; i < 10; ++i) {
    threads.push_back(new TLSThread);
  }
  for (int i = 0; i < 10; ++i) {
    threads[i]->Start();
  }

  for (int i = 0; i < 10; ++i) {
    threads[i]->Join();
    delete threads[i];
  }
#endif
}
}  // namespace mozc
