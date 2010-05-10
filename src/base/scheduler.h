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

// Scheduler is a timer, call registered callback at a given interval.
// Scheduler has following features.
//  1. Backoff when the registered callback returns false.
//    - When the backoff is occured, next try will be after 2 * interval msec.
//    - Interval will be doubled as long as callback returns false, but
//      will not exceed max_interval.
//  2. Randomised delayed start to reduce server traffic peak.
//
// usage:
// // start scheduled job
// Scheduler::AddJob("TimerName", 60*1000, 60*60*1000, 30*1000,
//                   60*1000, &Callback);
// // stop job
// Scheduler::RemoveJob("TimerName");

#ifndef MOZC_BASE_SCHEDULER_H_
#define MOZC_BASE_SCHEDULER_H_

#include <string>

#include "base/util.h"

namespace mozc {
class Timer;

class Scheduler {
 public:
  // start scheduled job
  // return false if timer is already existed.
  // job will start after (delay_start + random_delay*rand()) msec
  static bool AddJob(const string &name, uint32 default_interval,
                     uint32 max_interval, uint32 delay_start,
                     uint32 random_delay,
                     bool (*callback)(void *), void *data);

  // stop scheduled job
  static bool RemoveJob(const string &name);

  // stop all jobs
  static void RemoveAllJobs();

  // should never be allocated.
 private:
  Scheduler();
  virtual ~Scheduler();
};
}  // namespace mozc
#endif  // MOZC_BASE_SCHEDULER_H_
