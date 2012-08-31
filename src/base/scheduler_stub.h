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

#ifndef MOZC_BASE_SCHEDULER_STUB_H_
#define MOZC_BASE_SCHEDULER_STUB_H_

// Stub scheduler implementation for unittest
//
// Usage:
// {
//   SchedulerStub scheduler_stub;
//   Scheduler::SetSchedulerHandler(&scheduler_stub);
//   ... (Do something)
//   scheduler_stub.PutClockForward(60*1000);
//   ... (Do something)
//   Scheduler::SetSchedulerHandler(NULL);
// }

#include <map>
#include <string>

#include "base/port.h"
#include "base/scheduler.h"

namespace mozc {

class SchedulerStub : public Scheduler::SchedulerInterface {
 public:
  SchedulerStub();
  virtual ~SchedulerStub();

  // |random_delay| will be ignored.
  virtual bool AddJob(const Scheduler::JobSetting &job_setting);
  virtual bool RemoveJob(const string &name);
  virtual void RemoveAllJobs();

  // Puts stub internal clock forward.
  // Jobs will be executed according to forwarded time.
  // Note that jobs will be executed individually for making
  // implementation simple. Interactions between dirrefent jobs will
  // not be simulated.
  void PutClockForward(uint64 delta_usec);

 private:
  struct JobForStub {
    const Scheduler::JobSetting job;
    uint32 remaining_usec;
    int backoff_count;

    explicit JobForStub(const Scheduler::JobSetting &job) :
        job(job), remaining_usec(job.delay_start()), backoff_count(0) {}
  };

  map<string, JobForStub> jobs_;
};
}  // namespace mozc
#endif  // MOZC_BASE_SCHEDULER_STUB_H_
