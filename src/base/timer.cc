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

#include "base/timer.h"

#ifdef OS_WINDOWS
#include <windows.h>
#endif

#include "base/unnamed_event.h"
#include "base/util.h"

namespace mozc {
#ifdef OS_WINDOWS
bool Timer::Start(uint32 due_time, uint32 interval) {
  // http://msdn.microsoft.com/en-us/library/ms681985.aspx
  // Remarks:
  // If you call ChangeTimerQueueTimer on a one-shot timer (its period is zero)
  // that has already expired, the timer is not updated.
  const bool is_oneshot_signaled = (num_signaled_ > 0 && one_shot_);

  one_shot_ = (interval == 0);

  if (!is_oneshot_signaled &&
      timer_handle_ != NULL && timer_queue_ != NULL) {
    VLOG(2) << "ChangeTimerQueueTimer() is called "
            << due_time << " " << interval;
    if (!::ChangeTimerQueueTimer(timer_queue_,
                                 timer_handle_,
                                 due_time,
                                 interval)) {
      LOG(ERROR) << "ChangeTimerQueueTime() failed: " << ::GetLastError();
      return false;
    }
    return true;
  }

  Stop();

  VLOG(2) << "Creating Timer Queue";
  timer_queue_ = ::CreateTimerQueue();
  if (NULL == timer_queue_) {
    LOG(ERROR) << "CreateTimerQueue() failed: " << ::GetLastError();
    return false;
  }

  VLOG(2) << "CreateTimerQueueTimer() is called: "
          << due_time << " " << interval;
  if (!::CreateTimerQueueTimer(&timer_handle_,
                               timer_queue_,
                               &Timer::TimerCallback,
                               reinterpret_cast<void *>(this),
                               due_time,
                               interval,
                               WT_EXECUTEINIOTHREAD)) {
    Stop();
    LOG(ERROR) << "CreateTimerQueueTimer() failed: " << ::GetLastError();
    return false;
  }

  VLOG(2) << "Timer has started";

  return true;
}

void Timer::Stop() {
  if (NULL != timer_handle_) {
    VLOG(2) << "Deleting Timer Queue Timer";
    // If the last param is  INVALID_HANDLE_VALUE,
    // the function waits for the timer callback function to
    // complete before returning.
    // It is far safer than killing the thread.
    if (!::DeleteTimerQueueTimer(timer_queue_, timer_handle_,
                                 INVALID_HANDLE_VALUE)) {
      LOG(ERROR) << "DeleteTimerQueueTimer failed: " << ::GetLastError();
    }
  }

  if (NULL != timer_queue_) {
    VLOG(2) << "Deleting Timer Queue";
    if (!::DeleteTimerQueueEx(timer_queue_,
                              INVALID_HANDLE_VALUE)) {
      LOG(ERROR) << "DeleteTimerQueueEx failed: " << ::GetLastError();
    }
  }

  VLOG(2) << "Timer has stopped";
  timer_handle_ = NULL;
  timer_queue_ = NULL;
  num_signaled_ = 0;
}

Timer::Timer()
    : timer_queue_(NULL), timer_handle_(NULL), one_shot_(false),
      num_signaled_(0) {}

Timer::~Timer() {
  Stop();
}

#else   // OS_WINDOWS

class TimerThread: public Thread {
 public:
  TimerThread(uint32 due_time, uint32 interval,
              Timer *timer, UnnamedEvent *event)
      : Thread(),
        first_(true),
        due_time_(due_time),
        interval_(interval),
        timer_(timer),
        event_(event) {}

  virtual ~TimerThread() {
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

 private:
  bool first_;
  uint32 due_time_;
  uint32 interval_;
  Timer *timer_;
  UnnamedEvent *event_;
};

bool Timer::Start(uint32 due_time, uint32 interval) {
  if (timer_thread_.get() != NULL) {
    Stop();
  }
  VLOG(1) << "Starting " << due_time << " " << interval;
  event_.reset(new UnnamedEvent);
  timer_thread_.reset(new TimerThread(due_time, interval, this, event_.get()));
  timer_thread_->Start();
  return true;
}

void Timer::Stop() {
  if (timer_thread_.get() == NULL) {
    return;
  }
  scoped_lock l(&mutex_);
  event_->Notify();
  timer_thread_->Join();
  timer_thread_.reset(NULL);
  event_.reset(NULL);
}

Timer::Timer() : num_signaled_(0) {}

Timer::~Timer() {
  Stop();
}
#endif  // OS_WINDOWS
}  // namespace mozc
