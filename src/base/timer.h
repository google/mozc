// Copyright 2010-2014, Google Inc.
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

#include "base/port.h"
#include "base/scoped_ptr.h"

namespace mozc {

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

  void TimerCallback();

 protected:
  // overwrite this function to implement signal handler.
  // Caveat: Even if you overrides this methods, the timer stop calling back
  // the overridden version once the object destruction reached to
  // Timer::~Timer(). This is because the callback is internally implemented
  // as |this->Signaled()| but the vtable dynamically changes during
  // object construction and destruction. This also means you cannot make
  // this method pure-virtual, because this method can be called back when
  // the main thread is running inside Timer::~Timer()
  // TODO(yukawa): Switch to more robust design.
  virtual void Signaled();

 private:
  class TimerThread;
  scoped_ptr<TimerThread> timer_thread_;
  uint32 num_signaled_;

  DISALLOW_COPY_AND_ASSIGN(Timer);
};

}  // namespace mozc
#endif  // MOZC_BASE_TIMER_H_
