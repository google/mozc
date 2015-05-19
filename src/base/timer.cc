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

#include "base/timer.h"

#include "base/logging.h"
#include "base/thread.h"
#include "base/unnamed_event.h"
#include "base/util.h"

namespace mozc {

class Timer::TimerThread : public Thread {
 public:
  TimerThread(uint32 due_time, uint32 interval, Timer *timer)
      : Thread(),
        due_time_(due_time),
        interval_(interval),
        timer_(timer),
        event_(new UnnamedEvent) {}

  virtual ~TimerThread() {
    SignalQuit();
    Join();
  }

  virtual void Run() {
    if (event_->Wait(due_time_)) {
      VLOG(1) << "Received notification event";
      return;
    }
    VLOG(2) << "call TimerCallback()";
    timer_->TimerCallback();

    if (interval_ == 0) {
      VLOG(2) << "Run() end";
      return;
    } else {
      while (true) {
        if (event_->Wait(interval_)) {
          VLOG(1) << "Received notification event";
          return;
        }
        VLOG(2) << "call TimerCallback()";
        timer_->TimerCallback();
      }
    }
  }

  void SignalQuit() {
    const bool result = event_->Notify();
    DCHECK(result);
  }

 private:
  uint32 due_time_;
  uint32 interval_;
  Timer *timer_;
  scoped_ptr<UnnamedEvent> event_;

  DISALLOW_COPY_AND_ASSIGN(TimerThread);
};

void Timer::TimerCallback() {
  num_signaled_++;
  Signaled();
}

bool Timer::Start(uint32 due_time, uint32 interval) {
  VLOG(1) << "Starting " << due_time << " " << interval;
  // Note: We can simply destroy |*timer_thread_.get()| since
  // TimerThread::~TimerThread internally joins its background thread.
  timer_thread_.reset(new TimerThread(due_time, interval, this));
  timer_thread_->Start();
  return true;
}

void Timer::Stop() {
  // Note: We can simply destroy |*timer_thread_.get()| since
  // TimerThread::~TimerThread internally joins its background thread.
  timer_thread_.reset(NULL);
}

Timer::Timer()
    : num_signaled_(0) {}

Timer::~Timer() {
  Stop();
}

void Timer::Signaled() {
  // Note: This code can be called back even if a subclass overrides this
  // method when the main thread reached to Timer::~Timer. Do not make this
  // method pure-virtual.
}

}  // namespace mozc
