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

#include <string.h>
#include "base/logging.h"
#include "base/mutex.h"
#include "base/thread.h"
#include "base/util.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

volatile int g_counter = 0;

class MutexTestThread : public Thread {
 public:
  MutexTestThread(Mutex *mutex, int loop, int sleep)
      : mutex_(mutex), loop_(loop), sleep_(sleep) {}

  virtual void Run() {
    for (int i = 0; i < loop_; ++i) {
      {
        scoped_lock l(mutex_);
        ++g_counter;
      }
      Util::Sleep(sleep_);
    }
  }

 private:
  Mutex *mutex_;
  int loop_;
  int sleep_;
};

TEST(MutexTest, MutexBasicTest) {
  g_counter = 0;
  Mutex mutex;
  MutexTestThread t(&mutex, 1, 1000);
  t.Start();

  Util::Sleep(100);       // still g_counter is locked
  scoped_lock l(&mutex);  // get mutex 2nd
  ++g_counter;
  EXPECT_EQ(2, g_counter);
  t.Join();               // make sure that the thread no longer uses the mutex
}

TEST(MutexTest, MutexBatchTest) {
  const int kThreadsSize = 5;
  const int kLoopSize = 10;
  vector<MutexTestThread *> threads(kThreadsSize);

  g_counter = 0;

  Mutex mutex;
  for (int i = 0; i < kThreadsSize; ++i) {
    threads[i] = new MutexTestThread(&mutex, kLoopSize, 1);
  }

  for (int i = 0; i < kThreadsSize; ++i) {
    threads[i]->Start();
  }

  for (int i = 0; i < kThreadsSize; ++i) {
    threads[i]->Join();
  }

  for (int i = 0; i < kThreadsSize; ++i) {
    delete threads[i];
  }

  EXPECT_EQ(kThreadsSize * kLoopSize, g_counter);
}

void CallbackFunc() {
  ++g_counter;
  Util::Sleep(20);
}

class ReaderMutexTestThread : public Thread {
 public:
  explicit ReaderMutexTestThread(ReaderWriterMutex *mutex)
      : counter_(0), mutex_(mutex) {}

  static const int kStopCoutner = -1;

  virtual void Run() {
    while (g_counter != kStopCoutner) {
      Util::Sleep(1);
      scoped_reader_lock l(mutex_);
      counter_ = g_counter;
    }
  }

  int counter() const { return counter_; }

 private:
  int counter_;
  ReaderWriterMutex *mutex_;
};

// TODO(yukawa): Replace the following test with more stable one
//     which asserts deterministic conditions that will not be
//     affected by the CPU load average. b/6355447
TEST(MutexTest, ReaderWriterTest) {
  if (!ReaderWriterMutex::MultipleReadersThreadsSupported()) {
    LOG(INFO) << "ReaderWriterMutex does not support multiple "
                 "reader threads. Skipping ReaderWriterTest test.";
    return;
  }

  const size_t kThreadsSize = 3;
  vector<ReaderMutexTestThread *> threads(kThreadsSize);

  // shared variable
  g_counter = 1;

  ReaderWriterMutex mutex;
  for (size_t i = 0; i < kThreadsSize; ++i) {
    threads[i] = new ReaderMutexTestThread(&mutex);
  }

  for (size_t i = 0; i < kThreadsSize; ++i) {
    threads[i]->Start();
  }

  const uint32 kSleepTime = 500;  // 500 msec

  // every reader thread can get g_counter variable at the same time.
  Util::Sleep(kSleepTime);
  for (int i = 0; i < kThreadsSize; ++i) {
    EXPECT_EQ(g_counter, threads[i]->counter());
  }

  for (int counter = 1; counter < 10; ++counter) {
    {
      // stops the reader thread.
      scoped_writer_lock l(&mutex);

      // update counter
      g_counter = counter + 1;

      // Still the counter value is counter.
      for (size_t i = 0; i < kThreadsSize; ++i) {
        EXPECT_EQ(counter, threads[i]->counter());
      }
    }

    Util::Sleep(kSleepTime);
    // coutner value becomes the same as g_counter
    for (size_t i = 0; i < kThreadsSize; ++i) {
      EXPECT_EQ(g_counter, threads[i]->counter());
    }
  }

  {
    scoped_writer_lock l(&mutex);
    g_counter = ReaderMutexTestThread::kStopCoutner;
  }

  for (size_t i = 0; i < kThreadsSize; ++i) {
    threads[i]->Join();
    delete threads[i];
  }
}

TEST(CallOnceTest, CallOnceBasicTest) {
  g_counter = 0;
  once_t once = MOZC_ONCE_INIT;
  CallOnce(&once, CallbackFunc);
  EXPECT_EQ(1, g_counter);

  CallOnce(&once, CallbackFunc);
  EXPECT_EQ(1, g_counter);

  CallOnce(&once, CallbackFunc);
  EXPECT_EQ(1, g_counter);
}
}  // namespace
}  // namespace mozc
