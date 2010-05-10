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

#include "base/scheduler.h"

#include <cstdlib>

#include <map>

#ifdef OS_WINDOWS
#include <time.h>  // time()
#else
#include <sys/time.h>  // time()
#endif

#include "base/mutex.h"
#include "base/singleton.h"
#include "base/timer.h"
#include "base/util.h"

namespace mozc {
namespace {
class QueueTimer : public Timer {
 public:
  QueueTimer(void (*callback)(void *),
             void *arg,
             uint32 due_time,
             uint32 period)
  : callback_(callback),
    arg_(arg),
    due_time_(due_time),
    period_(period) {
  }

  bool Start() {
    return Timer::Start(due_time_, period_);
  }

  virtual void Signaled() {
    callback_(arg_);
  }

 private:
  void (*callback_)(void *);
  void *arg_;
  uint32 due_time_;
  uint32 period_;
};

struct Job {
  uint32 default_interval;
  uint32 max_interval;
  uint32 delay_start;
  uint32 random_delay;
  bool (*callback)(void *);
  void *data;
  uint32 skip_count;
  uint32 backoff_count;
  QueueTimer* timer;
  bool running;
};

class SchedulerImpl {
 public:
  SchedulerImpl() {
    srand(static_cast<uint32>(time(NULL)));
  }

  virtual ~SchedulerImpl() {
    RemoveAllJobs();
  }

  void RemoveAllJobs() {
    scoped_lock l(&mutex_);
    for (map<string, Job>::iterator itr = jobs_.begin();
         itr != jobs_.end();
         ++itr) {
      Job *job = &itr->second;
      if (job->timer) {
        job->timer->Stop();
        delete job->timer;
      }
    }
    jobs_.clear();
  }

  bool AddJob(const string &name, uint32 default_interval, uint32 max_interval,
              uint32 delay_start, uint32 random_delay, bool (*callback)(void *),
              void *data) {
    scoped_lock l(&mutex_);
    map<string, Job>::iterator find_itr = jobs_.find(name);
    if (find_itr != jobs_.end()) {
      LOG(WARNING) << "Job " << name << " is already registered";
      return false;
    }

    DCHECK(default_interval);
    DCHECK(max_interval);
    DCHECK(callback);
    Job newjob;
    newjob.default_interval = default_interval;
    newjob.max_interval = max_interval;
    newjob.delay_start = delay_start;
    newjob.random_delay = random_delay;
    newjob.callback = callback;
    newjob.data = data;
    newjob.skip_count = 0;
    newjob.backoff_count = 0;
    newjob.timer = NULL;
    newjob.running = false;

    pair<map<string, Job>::iterator, bool> result
      = jobs_.insert(make_pair(name, newjob));
    if (!result.second) {
      LOG(ERROR) << "insert failed";
      return false;
    }
    Job *job = &result.first->second;
    DCHECK(job);

    uint32 delay = job->delay_start;
    if (job->random_delay != 0) {
      const uint64 r = rand();
      const uint64 d = job->random_delay * r;
      const uint64 random_delay = d / RAND_MAX;
      delay += random_delay;
    }
    job->timer = new QueueTimer(&TimerCallback, job, delay,
                                job->default_interval);
    if (job->timer == NULL) {
      LOG(ERROR) << "failed to create QueueTimer";
    }
    const bool started =  job->timer->Start();
    if (started) {
      return true;
    } else {
      delete job->timer;
      return false;
    }
  }

  bool RemoveJob(const string &name) {
    scoped_lock l(&mutex_);
    map<string, Job>::iterator itr = jobs_.find(name);
    if (itr == jobs_.end()) {
      LOG(WARNING) << "Job " << name << " is not registered";
      return false;
    } else {
      Job *job = &itr->second;
      if (job->timer != NULL) {
        job->timer->Stop();
        delete job->timer;
      }
      jobs_.erase(itr);
      return true;
    }
  }

 private:
  static void TimerCallback(void *param) {
    Job *job = reinterpret_cast<Job *>(param);
    DCHECK(job);
    if (job->running) {
      return;
    }
    if (job->skip_count) {
      job->skip_count--;
      VLOG(3) << "Backoff = " << job->backoff_count
              << " skip_count = " << job->skip_count;
      return;
    }
    job->running = true;
    const bool success = job->callback(job->data);
    job->running = false;
    if (success) {
      job->backoff_count = 0;
    } else {
      const uint32 new_backoff_count = (job->backoff_count == 0) ?
          1 : job->backoff_count * 2;
      if (new_backoff_count * job->default_interval < job->max_interval) {
        job->backoff_count = new_backoff_count;
      }
      job->skip_count = job->backoff_count;
    }
  }

  map<string, Job> jobs_;
  Mutex mutex_;
};

}  // namespace

bool Scheduler::AddJob(const string &name, uint32 default_interval,
                       uint32 max_interval, uint32 delay_start,
                       uint32 random_delay,
                       bool (*callback)(void *), void *data) {
  return Singleton<SchedulerImpl>::get()->AddJob(name, default_interval,
                                                 max_interval, delay_start,
                                                 random_delay,
                                                 callback, data);
}

bool Scheduler::RemoveJob(const string &name) {
  return Singleton<SchedulerImpl>::get()->RemoveJob(name);
}

void Scheduler::RemoveAllJobs() {
  Singleton<SchedulerImpl>::get()->RemoveAllJobs();
}

}  // namespace mozc
