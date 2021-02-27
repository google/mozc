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

// Scheduler is a timer, call registered callback at a given interval.
// Scheduler has following features.
//  1. Backoff when the registered callback returns false.
//    - When the backoff is occurred, next try will be after 2 * interval msec.
//    - Interval will be doubled as long as callback returns false, but
//      will not exceed max_interval.
//  2. Randomised delayed start to reduce server traffic peak.
//
// usage:
// // start scheduled job
// Scheduler::AddJob(Scheduler::JobSetting(
//     "TimerName", 60*1000, 60*60*1000, 30*1000, 60*1000, &Callback, nullptr));
// (nullptr is the args for Callback)
// // stop job
// Scheduler::RemoveJob("TimerName");

#ifndef MOZC_BASE_SCHEDULER_H_
#define MOZC_BASE_SCHEDULER_H_

#include <cstdint>
#include <string>

#include "base/port.h"

namespace mozc {
class Timer;

class Scheduler {
 public:
  // simple container class for the job setting to be scheduled.
  class JobSetting {
   public:
    typedef bool (*CallbackFunc)(void *);

    JobSetting(const std::string &name, uint32_t default_interval,
               uint32_t max_interval, uint32_t delay_start,
               uint32_t random_delay, CallbackFunc callback, void *data)
        : name_(name),
          default_interval_(default_interval),
          max_interval_(max_interval),
          delay_start_(delay_start),
          random_delay_(random_delay),
          callback_(callback),
          data_(data) {}

    ~JobSetting() {}

    std::string name() const { return name_; }
    uint32_t default_interval() const { return default_interval_; }
    uint32_t max_interval() const { return max_interval_; }
    uint32_t delay_start() const { return delay_start_; }
    uint32_t random_delay() const { return random_delay_; }
    CallbackFunc callback() const { return callback_; }
    void *data() const { return data_; }

   private:
    std::string name_;
    uint32_t default_interval_;
    uint32_t max_interval_;
    uint32_t delay_start_;
    uint32_t random_delay_;
    CallbackFunc callback_;
    void *data_;
  };

  // start scheduled job
  // return false if timer is already existed.
  // job will start after (delay_start + random_delay*rand()) msec
  static bool AddJob(const JobSetting &job_setting);

  // stop scheduled job specified by neme.
  static bool RemoveJob(const std::string &name);

  // stop all jobs
  static void RemoveAllJobs();

  // returns true is the job has been registered.
  static bool HasJob(const std::string &name);

  // This function is provided for test.
  // The behavior of scheduler can be customized by replacing an underlying
  // helper class inside this.
  class SchedulerInterface;
  static void SetSchedulerHandler(SchedulerInterface *handler);

  // Interface of the helper class.
  // Default implementation is defined in the .cc file.
  class SchedulerInterface {
   public:
    virtual ~SchedulerInterface() {}
    virtual bool AddJob(const JobSetting &job_setting) = 0;
    virtual bool RemoveJob(const std::string &name) = 0;
    virtual void RemoveAllJobs() = 0;
    virtual bool HasJob(const std::string &name) const = 0;
  };

 private:
  // should never be allocated.
  DISALLOW_IMPLICIT_CONSTRUCTORS(Scheduler);
};
}  // namespace mozc
#endif  // MOZC_BASE_SCHEDULER_H_
