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

#include "base/base.h"
#include "base/thread.h"
#include "base/util.h"
#include "ipc/named_event.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {

namespace {
const char kName[] = "named_event_test";
const int kNumRequests = 5;

class NamedEventNotifierThread: public Thread {
 public:
  void Run() {
    Util::Sleep(100);
    NamedEventNotifier n(kName);
    EXPECT_TRUE(n.IsAvailable());
    Util::Sleep(500);
    EXPECT_TRUE(n.Notify());
  }
};

class NamedEventListenerThread: public Thread {
 public:
  void Run() {
    NamedEventListener n(kName);
    EXPECT_TRUE(n.IsAvailable());
    EXPECT_FALSE(n.Wait(500));
    EXPECT_TRUE(n.Wait(2000));
  }
};

TEST(NamedEventTest, NamedEventBasicTest) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

  for (int i = 0; i < kNumRequests; ++i) {
    NamedEventNotifierThread t;
    NamedEventListener l(kName);
    EXPECT_TRUE(l.IsAvailable());
    t.Start();
    EXPECT_FALSE(l.Wait(200));
    Util::Sleep(100);
    EXPECT_TRUE(l.Wait(500));
    t.Join();
  }
}

TEST(NamedEventTest, IsAvailableTest) {
  {
    NamedEventListener l(kName);
    EXPECT_TRUE(l.IsAvailable());
    NamedEventNotifier n(kName);
    EXPECT_TRUE(n.IsAvailable());
  }

  // no listner
  {
    NamedEventNotifier n(kName);
    EXPECT_FALSE(n.IsAvailable());
  }
}

TEST(NamedEventTest, IsOwnerTest) {
  for (int i = 0; i < kNumRequests; ++i) {
    NamedEventListener l1(kName);
    EXPECT_TRUE(l1.IsOwner());
    EXPECT_TRUE(l1.IsAvailable());
    NamedEventListener l2(kName);
    EXPECT_FALSE(l2.IsOwner());   // the instance is owneded by l1
    EXPECT_TRUE(l2.IsAvailable());
  }
}

TEST(NamedEventTest, NamedEventMultipleListenerTest) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

  vector<NamedEventListenerThread> t(kNumRequests);
  for (int i = 0; i < kNumRequests; ++i) {
    t[i].Start();
  }

  // all |kNumRequests| listener events should be raised
  // at once with single notifier event
  Util::Sleep(1000);
  NamedEventNotifier n(kName);
  EXPECT_TRUE(n.Notify());

  for (int i = 0; i < kNumRequests; ++i) {
    t[i].Join();
  }
}

TEST(NamedEventTest, NamedEventPathLengthTest) {
#ifndef OS_WINDOWS
  const string name_path = NamedEventUtil::GetEventPath(kName);
  // length should be less than 14 not includeing terminating null.
  EXPECT_EQ(13, strlen(name_path.c_str()));
#endif  // OS_WINDOWS
}
}  // namespace
}  // mozc
