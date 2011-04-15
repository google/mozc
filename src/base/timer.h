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

#ifndef MOZC_BASE_TIMER_H_
#define MOZC_BASE_TIMER_H_

#include "base/base.h"
#include "base/mutex.h"
#include "base/thread.h"

namespace mozc {

#ifndef OS_WINDOWS
class TimerThread;
class UnnamedEvent;
#endif  // !OS_WINDOWS

class Timer {
 public:
  // Start timer.
  // due_time:
  // The amount of time to elapse before the timer is to be set to the
  // signaled state for the first time, in milliseconds.
  // interval:
  // The period of the timer, in milliseconds. If |interval| is zero,
  // the timer is one-shot timer. If this parameter is greater than zero,
  // the timer is periodic.
  //
  // Either due_time and interval must be non 0.
  bool Start(uint32 due_time, uint32 interval);

  // Stop timer:
  // If pending signal functions remain, Stop() blocks the current thread
  // Be sure that signal handler never be blocked.
  void Stop();

  Timer();

  // Timer() call Stop() internally, so it is blocking
  virtual ~Timer();

#ifndef OS_WINDOWS
  void TimerCallback() {
    scoped_lock l(&mutex_);
    num_signaled_++;
    Signaled();
  }
#endif

 protected:
  // overwrite this function to implement
  // signal handler.
  virtual void Signaled() {}

 private:
#ifdef OS_WINDOWS
  HANDLE timer_queue_;
  HANDLE timer_handle_;
  bool one_shot_;
#else
  Mutex mutex_;
  scoped_ptr<UnnamedEvent> event_;
  scoped_ptr<TimerThread> timer_thread_;
#endif

  uint32 num_signaled_;

#ifdef OS_WINDOWS
  static void CALLBACK TimerCallback(void *ptr, BOOLEAN timer_or_wait) {
    Timer *p = static_cast<Timer *>(ptr);
    p->num_signaled_++;
    p->Signaled();
  }
#endif
};
}  // namespace mozc
#endif  // MOZC_BASE_TIMER_H_
