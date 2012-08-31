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

#include "base/scheduler_stub.h"

#include <map>

namespace mozc {

SchedulerStub::SchedulerStub() {}

SchedulerStub::~SchedulerStub() {}

bool SchedulerStub::AddJob(const Scheduler::JobSetting &job_setting) {
  if (jobs_.find(job_setting.name()) != jobs_.end()) {
    LOG(WARNING) << "Job " << job_setting.name() << " is already registered";
    return false;
  }

  jobs_.insert(pair<string, JobForStub>(
      job_setting.name(), JobForStub(job_setting)));
  return true;
}

bool SchedulerStub::RemoveJob(const string &name) {
  return (jobs_.erase(name) != 0);
}

void SchedulerStub::RemoveAllJobs() {
  jobs_.clear();
}

void SchedulerStub::PutClockForward(uint64 delta_usec) {
  for (map<string, JobForStub>::iterator itr = jobs_.begin();
       itr != jobs_.end(); ++itr) {
    JobForStub *job_for_stub = &itr->second;
    uint64 time_usec = delta_usec;
    while (true) {
      if (job_for_stub->remaining_usec > time_usec) {
        job_for_stub->remaining_usec -= time_usec;
        break;
      }

      time_usec -= job_for_stub->remaining_usec;
      const bool result = job_for_stub->job.callback()(
          job_for_stub->job.data());
      if (result) {
        job_for_stub->backoff_count = 0;
        job_for_stub->remaining_usec = job_for_stub->job.default_interval();
      } else {
        const int new_backoff_count = (job_for_stub->backoff_count == 0) ?
            1 : job_for_stub->backoff_count * 2;
        if ((new_backoff_count + 1) * job_for_stub->job.default_interval()
            < job_for_stub->job.max_interval()) {
          job_for_stub->backoff_count = new_backoff_count;
        }
        job_for_stub->remaining_usec =
            (job_for_stub->job.default_interval() *
             (job_for_stub->backoff_count + 1));
      }
    }
  }
}

}  // namespace mozc
