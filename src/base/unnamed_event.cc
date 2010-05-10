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

#include "base/unnamed_event.h"
#include "base/base.h"

#ifdef OS_WINDOWS
#include <windows.h>
#else  // OS_WINDOWS
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#endif  // OS_WINDOWS

namespace mozc {

#ifdef OS_WINDOWS
UnnamedEvent::UnnamedEvent()
    : handle_(::CreateEvent(NULL, TRUE, FALSE, NULL)) {
  if (NULL == handle_.get()) {
    LOG(ERROR) << "CreateEvent failed: " << ::GetLastError();
  }
}

UnnamedEvent::~UnnamedEvent() {}

bool UnnamedEvent::IsAvailable() const {
  return (NULL != handle_.get());
}

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
    return true;   // assume that it is already raised
  }

  if (msec < 0) {
    msec = INFINITE;
  }

  return WAIT_TIMEOUT !=
      ::WaitForSingleObject(handle_.get(), msec);
}

#else  // OS_WINDOWS

UnnamedEvent::UnnamedEvent() {
  pthread_mutex_init(&mutex_, NULL);
  pthread_cond_init(&cond_, NULL);
}

UnnamedEvent::~UnnamedEvent() {
  pthread_mutex_unlock(&mutex_);
  pthread_mutex_destroy(&mutex_);
  pthread_cond_signal(&cond_);
  pthread_cond_destroy(&cond_);
}

bool UnnamedEvent::IsAvailable() const {
  return true;
}

bool UnnamedEvent::Notify() {
  if (pthread_cond_signal(&cond_) != 0) {
    LOG(ERROR) << "pthread_cond_signal failed: " << errno;
    return false;
  }
  return true;
}

bool UnnamedEvent::Wait(int msec) {
  pthread_mutex_lock(&mutex_);

  if (msec >= 0) {
    struct timeval tv;
    if (0 != gettimeofday(&tv, NULL)) {
      pthread_mutex_unlock(&mutex_);
      return true;
    }

    struct timespec timeout;
    timeout.tv_sec = tv.tv_sec + msec / 1000;
    timeout.tv_nsec = 1000 * (tv.tv_usec + 1000 * (msec % 1000));

    // if tv_nsec >= 10^9, pthread_cond_timedwait may return EINVAL
    while (timeout.tv_nsec >= 1000000000) {
      timeout.tv_sec++;
      timeout.tv_nsec -= 1000000000;
    }

    const int result = pthread_cond_timedwait(&cond_, &mutex_, &timeout);
    pthread_mutex_unlock(&mutex_);
    return ETIMEDOUT != result;
  }

  pthread_cond_wait(&cond_, &mutex_);
  pthread_mutex_unlock(&mutex_);
  return true;
}
#endif   // OS_WINDOWS
}  // namespace mozc
