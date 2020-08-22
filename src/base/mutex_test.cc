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

#include "base/mutex.h"

#include <atomic>
#include <memory>
#include <vector>

#include "base/clock.h"
#include "base/logging.h"
#include "base/thread.h"
#include "base/util.h"
#include "testing/base/public/gunit.h"
#include "absl/base/thread_annotations.h"

namespace mozc {
namespace {

class IncrementThread : public Thread {
 public:
  IncrementThread(int max_loop, Mutex *mutex, int *counter)
      : max_loop_(max_loop),
        stop_requested_(false),
        mutex_(mutex),
        counter_(counter) {}

  ~IncrementThread() override = default;

  void Run() override {
    for (int i = 0; i < max_loop_ && !stop_requested_; ++i) {
      {
        scoped_lock l(mutex_);
        ++*counter_;
      }
      Util::Sleep(10);
    }
  }

  void RequestStop() { stop_requested_ = true; }

 private:
  const int max_loop_;
  std::atomic<bool> stop_requested_;
  Mutex *mutex_;
  int *counter_ ABSL_PT_GUARDED_BY(mutex_);
};

TEST(MutexTest, LockTest) {
  Mutex mutex;
  int counter = 0;
  IncrementThread thread(10000, &mutex, &counter);
  ThreadJoiner joiner(&thread);

  thread.Start("Increment");  // Start incrementing counter.
  Util::Sleep(10);
  {
    // Test exclusive access to counter by checking if its value is not
    // incremented by the increment thread while holding the lock.
    scoped_lock l(&mutex);
    const int prev_count = counter;
    Util::Sleep(100);                // Sleep while holding the lock.
    EXPECT_EQ(prev_count, counter);  // The counter should not be incremented.
  }
  thread.RequestStop();
}

// Marked as NO_THREAD_SAFETY_ANALYSIS to avoid the compiler warning "acquiring
// mutex 'mutex' that is already held".
TEST(MutexTest, RecursiveLockTest) ABSL_NO_THREAD_SAFETY_ANALYSIS {
  const int kNumLocks = 5;
  Mutex mutex;
  for (int i = 0; i < kNumLocks; ++i) {
    mutex.Lock();
    EXPECT_TRUE(mutex.TryLock());
  }
  for (int i = 0; i < kNumLocks; ++i) {
    mutex.Unlock();
    mutex.Unlock();
  }
}

class HoldLockThread : public Thread {
 public:
  explicit HoldLockThread(Mutex *mutex)
      : holds_lock_(false), release_requested_(false), mutex_(mutex) {}
  ~HoldLockThread() override = default;

  void Run() override {
    {
      scoped_lock l(mutex_);
      holds_lock_ = true;
      while (!release_requested_) {
        Util::Sleep(10);
      }
    }
    holds_lock_ = false;
  }

  bool holds_lock() const { return holds_lock_; }

  void RequestRelease() { release_requested_ = true; }

 private:
  std::atomic<bool> holds_lock_;
  std::atomic<bool> release_requested_;
  Mutex *mutex_;
};

// Marked as NO_THREAD_SAFETY_ANALYSIS to avoid the error: "releasing mutex
// 'mutex' that was not held".
void TryLockThenUnlock(Mutex *mutex) ABSL_NO_THREAD_SAFETY_ANALYSIS {
  EXPECT_TRUE(mutex->TryLock());
  mutex->Unlock();
}

TEST(MutexTest, TryLockTest) {
  Mutex mutex;

  // No one holds lock, so it should be lockable.
  TryLockThenUnlock(&mutex);
  {
    // Now create the situation where another thread holds the lock.
    HoldLockThread thread(&mutex);
    ThreadJoiner joiner(&thread);
    thread.Start("HoldLock");
    while (!thread.holds_lock()) {  // Wait for thread to hold the lock.
      Util::Sleep(10);
    }
    // Since the above thread holds the lock, the main thread cannot hold the
    // lock.
    EXPECT_FALSE(mutex.TryLock());
    thread.RequestRelease();
  }
  // Again, no one holds lock, so it should be lockable.
  TryLockThenUnlock(&mutex);
}

TEST(MutexTest, ExclusiveAccessByManyThreads) {
  const int kThreadsSize = 5;
  const int kLoopSize = 10;

  Mutex mutex;
  int counter = 0;
  {
    // Increment counter by many threads, each of which increments counter by
    // kLoopSize.
    std::vector<std::unique_ptr<IncrementThread>> threads;
    std::vector<ThreadJoiner> joiners;
    for (int i = 0; i < kThreadsSize; ++i) {
      threads.emplace_back(new IncrementThread(kLoopSize, &mutex, &counter));
      joiners.emplace_back(threads.back().get());
    }
    for (auto &thread : threads) {
      thread->Start("Increment");
    }
  }

  EXPECT_EQ(kThreadsSize * kLoopSize, counter);
}

class ReaderThread : public Thread {
 public:
  ReaderThread(ReaderWriterMutex *mutex, int *counter)
      : mutex_(mutex),
        counter_(counter),
        read_value_(-1),
        stop_requested_(false) {}

  ~ReaderThread() override = default;

  void Run() override {
    while (!stop_requested_) {
      {
        scoped_reader_lock l(mutex_);
        read_value_ = *counter_;
      }
      Util::Sleep(5);
    }
  }

  int read_value() const { return read_value_; }
  void set_read_value(int v) { read_value_ = v; }

  void RequestStop() { stop_requested_ = true; }

  // The negative value is used to indicate that counter value is not yet read.
  // This method waits until a nonnegative value is read.
  void WaitUntilValueIsRead() const {
    while (read_value_ < 0) {
      Util::Sleep(10);
    }
  }

 private:
  ReaderWriterMutex *mutex_;
  int *counter_ ABSL_PT_GUARDED_BY(mutex_);
  std::atomic<int> read_value_;
  std::atomic<bool> stop_requested_;
};

TEST(MutexTest, ReaderWriterMutexTest) {
  if (!ReaderWriterMutex::MultipleReadersThreadsSupported()) {
    LOG(INFO) << "ReaderWriterMutex does not support multiple "
                 "reader threads. Skipping ReaderWriterTest test.";
    return;
  }

  const int kThreadsSize = 3;

  ReaderWriterMutex mutex;
  int counter = 12345;

  // Create reader threads that simultaneously read the value of counter.
  std::vector<std::unique_ptr<ReaderThread>> threads;
  std::vector<ThreadJoiner> joiners;
  for (int i = 0; i < kThreadsSize; ++i) {
    threads.emplace_back(new ReaderThread(&mutex, &counter));
    joiners.emplace_back(threads.back().get());
  }
  for (auto &thread : threads) {
    thread->Start("Read");
  }

  // Verify that all the threads read the value.
  for (const auto &thread : threads) {
    thread->WaitUntilValueIsRead();
    EXPECT_EQ(counter, thread->read_value());
  }

  {
    // Hold a writer lock to modify counter.
    scoped_writer_lock l(&mutex);
    const int prev_counter_value = counter;
    // While the writer lock is held, the readers cannot read the updated
    // counter value.  Therefore, their read values should be equal to
    // prev_counter_value.  Repeated this check for several times.
    for (int i = 1; i < 10; ++i) {
      counter = 7890 * i;
      for (const auto &thread : threads) {
        EXPECT_EQ(prev_counter_value, thread->read_value());
      }
      Util::Sleep(5);
    }
  }

  // Now the writer lock was released.  Check if the readers can again see the
  // updated value.
  for (auto &thread : threads) {
    // Reset to the value to guarantee the new value is read.
    thread->set_read_value(-1);
  }
  for (const auto &thread : threads) {
    thread->WaitUntilValueIsRead();
    EXPECT_EQ(counter, thread->read_value());
  }

  for (auto &thread : threads) {
    thread->RequestStop();
  }
}

std::atomic<int> g_num_called;

void IncrementGNumCalled() {
  ++g_num_called;
  Util::Sleep(20);
}

class OnceIncrementGNumCalledThread : public Thread {
 public:
  explicit OnceIncrementGNumCalledThread(once_t *once) : once_(once) {}
  ~OnceIncrementGNumCalledThread() override = default;

  void Run() override {
    for (int i = 0; i < 10; ++i) {
      CallOnce(once_, &IncrementGNumCalled);
      Util::Sleep(5);
    }
  }

 private:
  once_t *once_;
};

TEST(CallOnceTest, SingleThread) {
  g_num_called = 0;
  once_t once = MOZC_ONCE_INIT;
  for (int i = 0; i < 3; ++i) {
    CallOnce(&once, &IncrementGNumCalled);
    EXPECT_EQ(1, g_num_called);
  }
}

TEST(CallOnceTest, MultiThread) {
  g_num_called = 0;
  once_t once = MOZC_ONCE_INIT;
  std::vector<std::unique_ptr<OnceIncrementGNumCalledThread>> threads;
  std::vector<ThreadJoiner> joiners;
  for (int i = 0; i < 5; ++i) {
    threads.emplace_back(new OnceIncrementGNumCalledThread(&once));
    joiners.emplace_back(threads.back().get());
  }
  for (auto &thread : threads) {
    thread->Start("Increment");
  }
  for (int i = 0; i < 3; ++i) {
    CallOnce(&once, &IncrementGNumCalled);
  }
  EXPECT_EQ(1, g_num_called);
}

}  // namespace
}  // namespace mozc
