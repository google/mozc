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

#include "base/scheduler.h"

#ifdef OS_WINDOWS
#include <time.h>  // time()
#else
#include <sys/time.h>  // time()
#endif

#include <cstdlib>
#include <map>
#include <utility>

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

class Job {
 public:
  explicit Job(const Scheduler::JobSetting &setting) :
      setting_(setting),
      skip_count_(0),
      backoff_count_(0),
      timer_(NULL),
      running_(false) {}

  virtual ~Job() {
    if (timer_ != NULL) {
      timer_->Stop();
      delete timer_;
    }
  }

  const Scheduler::JobSetting setting() const {
    return setting_;
  }

  void set_skip_count(uint32 skip_count) {
    skip_count_ = skip_count;
  }

  uint32 skip_count() const {
    return skip_count_;
  }

  void set_backoff_count(uint32 backoff_count) {
    backoff_count_ = backoff_count;
  }

  uint32 backoff_count() const {
    return backoff_count_;
  }

  void set_timer(QueueTimer *timer) {
    timer_ = timer;
  }

  const QueueTimer *timer() const {
    return timer_;
  }

  QueueTimer *mutable_timer() {
    return timer_;
  }

  void set_running(bool running) {
    running_ = running;
  }

  bool running() const {
    return running_;
  }

 private:
  Scheduler::JobSetting setting_;
  uint32 skip_count_;
  uint32 backoff_count_;
  QueueTimer *timer_;
  bool running_;
};

class SchedulerImpl : public Scheduler::SchedulerInterface {
 public:
  SchedulerImpl() {
    srand(static_cast<uint32>(time(NULL)));
  }

  virtual ~SchedulerImpl() {
    RemoveAllJobs();
  }

  void RemoveAllJobs() {
    scoped_lock l(&mutex_);
    jobs_.clear();
  }

  void ValidateSetting(const Scheduler::JobSetting &job_setting) const {
    DCHECK_GT(job_setting.name().size(), 0);
    DCHECK_NE(0, job_setting.default_interval());
    DCHECK_NE(0, job_setting.max_interval());
    // do not use DCHECK_NE as a type checker raises an error.
    DCHECK(job_setting.callback() != NULL);
  }

  bool AddJob(const Scheduler::JobSetting &job_setting) {
    scoped_lock l(&mutex_);

    ValidateSetting(job_setting);
    if (HasJob(job_setting.name())) {
      LOG(WARNING) << "Job " << job_setting.name() << " is already registered";
      return false;
    }

    pair<map<string, Job>::iterator, bool> insert_result
        = jobs_.insert(make_pair(job_setting.name(), Job(job_setting)));
    if (!insert_result.second) {
      LOG(ERROR) << "insert failed";
      return false;
    }
    Job *job = &insert_result.first->second;
    DCHECK(job);

    const uint32 delay = CalcDelay(job_setting);
    job->set_timer(new QueueTimer(&TimerCallback, job, delay,
                                  job_setting.default_interval()));
    if (job->timer() == NULL) {
      LOG(ERROR) << "failed to create QueueTimer";
      return false;
    }
    const bool started = job->mutable_timer()->Start();
    if (started) {
      return true;
    } else {
      delete job->mutable_timer();
      return false;
    }
  }

  bool RemoveJob(const string &name) {
    scoped_lock l(&mutex_);
    if (!HasJob(name)) {
      LOG(WARNING) << "Job " << name << " is not registered";
      return false;
    }
    return (jobs_.erase(name) != 0);
  }

 private:
  static void TimerCallback(void *param) {
    Job *job = reinterpret_cast<Job *>(param);
    DCHECK(job);
    if (job->running()) {
      return;
    }
    if (job->skip_count()) {
      job->set_skip_count(job->skip_count() - 1);
      VLOG(3) << "Backoff = " << job->backoff_count()
              << " skip_count = " << job->skip_count();
      return;
    }
    job->set_running(true);
    Scheduler::JobSetting::CallbackFunc callback = job->setting().callback();
    DCHECK(callback != NULL);
    const bool success = callback(job->setting().data());
    job->set_running(false);
    if (success) {
      job->set_backoff_count(0);
    } else {
      const uint32 new_backoff_count = (job->backoff_count() == 0) ?
          1 : job->backoff_count() * 2;
      if (new_backoff_count * job->setting().default_interval()
          < job->setting().max_interval()) {
        job->set_backoff_count(new_backoff_count);
      }
      job->set_skip_count(job->backoff_count());
    }
  }

  bool HasJob(const string &name) const {
    return (jobs_.find(name) != jobs_.end());
  }

  uint32 CalcDelay(const Scheduler::JobSetting &job_setting) {
    uint32 delay = job_setting.delay_start();
    if (job_setting.random_delay() != 0) {
      const uint64 r = rand();
      const uint64 d = job_setting.random_delay() * r;
      const uint64 random_delay = d / RAND_MAX;
      delay += random_delay;
    }
    return delay;
  }

  map<string, Job> jobs_;
  Mutex mutex_;
};

Scheduler::SchedulerInterface *g_scheduler_handler = NULL;

Scheduler::SchedulerInterface *GetSchedulerHandler() {
  if (g_scheduler_handler != NULL) {
    return g_scheduler_handler;
  } else {
    return Singleton<SchedulerImpl>::get();
  }
}
}  // namespace

bool Scheduler::AddJob(const Scheduler::JobSetting &job_setting) {
  return GetSchedulerHandler()->AddJob(job_setting);
}

bool Scheduler::RemoveJob(const string &name) {
  return GetSchedulerHandler()->RemoveJob(name);
}

void Scheduler::RemoveAllJobs() {
  GetSchedulerHandler()->RemoveAllJobs();
}

void Scheduler::SetSchedulerHandler(SchedulerInterface *handler) {
  g_scheduler_handler = handler;
}
}  // namespace mozc
