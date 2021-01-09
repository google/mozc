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

#include "base/unnamed_event.h"

#ifdef OS_WIN
#include <windows.h>
#else  // OS_WIN
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#endif  // OS_WIN

#include "base/logging.h"
#include "base/port.h"

namespace mozc {

#ifdef OS_WIN
UnnamedEvent::UnnamedEvent()
    : handle_(::CreateEvent(nullptr, FALSE, FALSE, nullptr)) {
  // Use Auto reset mode of Win32 Event. (2nd arg of CreateEvent).
  // pthread_cond_signal: auto reset mode.
  // pthread_cond_broadcast: manual reset mode.
  if (nullptr == handle_.get()) {
    LOG(ERROR) << "CreateEvent failed: " << ::GetLastError();
  }
}

UnnamedEvent::~UnnamedEvent() {}

bool UnnamedEvent::IsAvailable() const { return (nullptr != handle_.get()); }

bool UnnamedEvent::Notify() {
  if (!IsAvailable()) {
    LOG(WARNING) << "Event object is not available";
    return false;
  }

  if (!::SetEvent(handle_.get())) {
    LOG(ERROR) << "SetEvent failed: " << ::GetLastError();
    return false;
  }

  return true;
}

bool UnnamedEvent::Wait(int msec) {
  if (!IsAvailable()) {
    return true;  // assume that it is already raised
  }

  if (msec < 0) {
    msec = INFINITE;
  }

  return WAIT_TIMEOUT != ::WaitForSingleObject(handle_.get(), msec);
}

#else   // OS_WIN

namespace {
class ScopedPthreadMutexLock {
 public:
  explicit ScopedPthreadMutexLock(pthread_mutex_t *mutex) : mutex_(mutex) {
    pthread_mutex_lock(mutex_);
  }
  ~ScopedPthreadMutexLock() { pthread_mutex_unlock(mutex_); }

 private:
  pthread_mutex_t *mutex_;
  DISALLOW_COPY_AND_ASSIGN(ScopedPthreadMutexLock);
};
}  // namespace

UnnamedEvent::UnnamedEvent() : notified_(false) {
  pthread_mutex_init(&mutex_, nullptr);
  pthread_cond_init(&cond_, nullptr);
}

// It is necessary to ensure that no threads wait for this event before the
// destruction.
UnnamedEvent::~UnnamedEvent() {
  pthread_mutex_destroy(&mutex_);
  pthread_cond_destroy(&cond_);
}

bool UnnamedEvent::IsAvailable() const { return true; }

bool UnnamedEvent::Notify() {
  {
    ScopedPthreadMutexLock lock(&mutex_);
    notified_ = true;
  }

  // Note: Need to awake all threads waiting for this event. Otherwise
  //   some thread would start to work incorrectly in some cases.
  //   An example error scenario is:
  //     - there are four threads, A, B, C and D.
  //     - A and B are producers. C and D are consumers.
  //     - C and D start to wait this event.
  //     - Then A notifies.
  //       - A takes the lock.
  //       - set notified_ true.
  //       - A releases the lock.
  //     - Then before A sends a signal to awake a thread (either C or D),
  //       B takes the lock.
  //     - A sends a signal, but both C and D are sleeping as the lock is
  //       still taken by B.
  //     - B releases the lock. So one of the thread (let's assume C for this
  //       example) starts to run, because it can take the lock.
  //     - B also sends a signal again.
  //     - C overrides notified_ to false, and releases the lock.
  //     - At last, D starts to run wrongly.
  //   The broadcast and while (in Wait) idiom is a popular way to fix such
  //   cases.
  pthread_cond_broadcast(&cond_);
  return true;
}

bool UnnamedEvent::Wait(int msec) {
  ScopedPthreadMutexLock lock(&mutex_);
  if (!notified_) {
    // Need to wait actually.
    if (msec < 0) {
      // Wait forever.
      while (!notified_) {
        pthread_cond_wait(&cond_, &mutex_);
      }
    } else {
      // Wait with time out.
      struct timeval tv;
      if (gettimeofday(&tv, nullptr) != 0) {
        LOG(ERROR) << "Failed to take the current time: " << errno;
        return false;
      }

      struct timespec timeout;
      timeout.tv_sec = tv.tv_sec + msec / 1000;
      timeout.tv_nsec = 1000 * (tv.tv_usec + 1000 * (msec % 1000));

      // if tv_nsec >= 10^9, pthread_cond_timedwait may return EINVAL
      while (timeout.tv_nsec >= 1000000000) {
        timeout.tv_sec++;
        timeout.tv_nsec -= 1000000000;
      }

      int result = 0;
      while (!notified_ && result == 0) {
        result = pthread_cond_timedwait(&cond_, &mutex_, &timeout);
      }

      if (result != 0) {
        // Time out.
        return false;
      }
    }
  }
  DCHECK(notified_);
  notified_ = false;
  return true;
}
#endif  // OS_WIN
}  // namespace mozc
