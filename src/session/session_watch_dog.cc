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

#include "session/session_watch_dog.h"

#ifdef OS_WINDOWS
#include <windows.h>
#endif  // OS_WINDOWS

#include <algorithm>
#include <cstring>
#include <numeric>
#include <string>

#include "base/base.h"
#include "base/cpu_stats.h"
#include "base/unnamed_event.h"
#include "base/util.h"
#include "client/client_interface.h"

namespace mozc {
namespace {

// IPC timeout
const int32 kCleanupTimeout = 30 * 1000;   // 30 sec for Cleanup Command
const int32 kPingTimeout    = 5 * 1000;    // 5 sec for Ping

// number of trials for ping
const int32 kPingTrial = 3;
const int32 kPingInterval = 1000;

// Average CPU load for last 1min.
// If the load > kMinimumAllCPULoad, don't send Cleanup
const float kMinimumAllCPULoad = 0.33f;

// Average CPU load for last 10secs.
// If the load > kMinimumLatestCPULoad, don't send Cleanup
const float kMinimumLatestCPULoad = 0.66f;
}

SessionWatchDog::SessionWatchDog(int32 interval_sec)
    : interval_sec_(interval_sec),
      client_(NULL), cpu_stats_(NULL), event_(new UnnamedEvent) {
  // allow [1..600].
  interval_sec_ = max(1, min(interval_sec_, 600));
  DCHECK(event_->IsAvailable())
      << "Unnamed event is not available";
}

SessionWatchDog::~SessionWatchDog() {
  Terminate();
}

void SessionWatchDog::SetClientInterface(client::ClientInterface *client) {
  client_ = client;
}

void SessionWatchDog::SetCPUStatsInterface(CPUStatsInterface *cpu_stats) {
  cpu_stats_ = cpu_stats;
}

void SessionWatchDog::Terminate() {
  if (!IsRunning()) {
    return;
  }

  if (!event_->Notify()) {
    LOG(ERROR) << "UnnamedEvent::Notify() failed";
    Thread::Terminate();
  }

  Join();
}

void SessionWatchDog::Run() {
  scoped_ptr<client::ClientInterface> client_impl(NULL);
  if (client_ == NULL) {
    VLOG(2) << "default client is used";
    client_impl.reset(client::ClientFactory::NewClient());
    client_ = client_impl.get();
  }

  scoped_ptr<CPUStatsInterface> cpu_stats_impl(NULL);
  if (cpu_stats_ == NULL) {
    VLOG(2) << "default cpu_stats is used";
    cpu_stats_impl.reset(new CPUStats);
    cpu_stats_ = cpu_stats_impl.get();
  }

  if (!event_->IsAvailable()) {
    LOG(ERROR) << "Unnamed event is not available";
    return;
  }

  // CPU load check
  // add volatile to store this array in stack
  volatile float cpu_loads[16];  // 60/5 = 12 is the minimal size
  volatile float total_cpu_load = 0.0;
  volatile float current_process_cpu_load = 0.0;
  const volatile size_t number_of_processors =
      cpu_stats_->GetNumberOfProcessors();

  DCHECK_GE(number_of_processors, 1);

  // the first (interval_sec_ - 60) sec: -> Do nothing
  const int32 idle_interval_msec = max(0, (interval_sec_ - 60)) * 1000;

  // last 60 sec: -> check CPU usage
  const int32 cpu_check_interval_msec = min(60, interval_sec_) * 1000;

  // for every 5 second, get CPU load percentage
  const int32 cpu_check_duration_msec = min(5, interval_sec_) * 1000;

  fill(cpu_loads, cpu_loads + arraysize(cpu_loads), 0.0);

  uint64 last_cleanup_time = Util::GetTime();

  while (true) {
    VLOG(1) << "Start sleeping " << idle_interval_msec;
    if (event_->Wait(idle_interval_msec)) {
      VLOG(1) << "Received stop signal";
      return;
    }
    VLOG(1) << "Finish sleeping " << idle_interval_msec;

    int32 cpu_loads_index = 0;
    for (int n = 0; n < cpu_check_interval_msec;
         n += cpu_check_duration_msec) {
      if (event_->Wait(cpu_check_duration_msec)) {
        VLOG(1) << "Received stop signal";
        return;
      }
      // save them in stack for debugging
      total_cpu_load = cpu_stats_->GetSystemCPULoad();
      current_process_cpu_load = cpu_stats_->GetCurrentProcessCPULoad();
      VLOG(1) << "total=" << total_cpu_load
              << " current=" << current_process_cpu_load
              << " normalized_current="
              << current_process_cpu_load / number_of_processors;
      // subtract the CPU load of my process from total CPU load.
      // This is required for running stress test.
      cpu_loads[cpu_loads_index++] =
          max(0.0f,
              total_cpu_load -
              current_process_cpu_load / number_of_processors);
    }

    DCHECK_GT(cpu_loads_index, 0);

    const uint64 current_cleanup_time = Util::GetTime();
    if (!CanSendCleanupCommand(cpu_loads,
                               cpu_loads_index,
                               current_cleanup_time,
                               last_cleanup_time)) {
      VLOG(1) << "CanSendCleanupCommand returned false";
      last_cleanup_time = current_cleanup_time;
      continue;
    }

    last_cleanup_time = current_cleanup_time;

    VLOG(2) << "Sending Cleanup command";
    client_->set_timeout(kCleanupTimeout);
    if (client_->Cleanup()) {
      VLOG(2) << "Cleanup command succeeded";
      continue;
    }

    LOG(WARNING) << "Cleanup failed "
                 << "execute PingCommand to check server is running";

    bool failed = true;
    client_->Reset();
    client_->set_timeout(kPingTimeout);
    for (int i = 0; i < kPingTrial; ++i) {
      if (event_->Wait(kPingInterval)) {
        VLOG(1) << "Received stop signal";
        return;
      }
      if (client_->PingServer()) {
        VLOG(2) << "Ping command succeeded";
        failed = false;
        break;
      }
      LOG(ERROR) << "Ping command failed, waiting "
                 << kPingInterval << " msec, trial: "
                 << i;
    }

    if (failed) {
      if (event_->Wait(100)) {
        VLOG(1) << "Parent thread is already terminated";
        return;
      }
#ifndef NO_LOGGING
      // We have received crash dumps caused by the following LOG(FATAL).
      // Unfortunately, we cannot investigate the cause of this error,
      // as the crash dump doesn't contain any logging information.
      // Here we temporary save the user name into stack in order
      // to obtain the log file before the LOG(FATAL).
      char user_name[32];
      const string tmp = Util::GetUserNameAsString();
      strncpy(user_name, tmp.c_str(), sizeof(user_name));
      VLOG(1) << "user_name: " << user_name;
#endif
      LOG(FATAL) << "Cleanup commands failed. Rasing exception...";
    }
  }
}

bool SessionWatchDog::CanSendCleanupCommand(
    const volatile float *cpu_loads,
    int cpu_loads_index,
    uint64 current_cleanup_time,
    uint64 last_cleanup_time) const {
  if (current_cleanup_time <= last_cleanup_time) {
    LOG(ERROR) << "time stamps are the same. clock may be altered";
    return false;
  }

  const float all_avg =
      accumulate(cpu_loads, cpu_loads + cpu_loads_index, 0.0)
      / cpu_loads_index;

  const size_t latest_size = min(2, cpu_loads_index);
  const float latest_avg =
      accumulate(cpu_loads, cpu_loads + latest_size, 0.0)
      / latest_size;

  VLOG(1) << "Average CPU load=" << all_avg
          << " latest CPU load=" << latest_avg;

  if (all_avg > kMinimumAllCPULoad ||
      latest_avg > kMinimumLatestCPULoad) {
    VLOG(1) << "Don't send Cleanup command, since CPU load is too high: "
            << all_avg << " " << latest_avg;
    return false;
  }

  // if the real interval from the last cleanup command
  // is 2 * interval(), assume that the computer went to
  // suspend mode
  if ((current_cleanup_time - last_cleanup_time) > 2 * interval()) {
    VLOG(1) << "Don't send cleanup because "
            << "Server went to suspend mode.";
    return false;
  }

  VLOG(2) << "CanSendCleanupCommand passed";
  return true;
}
}  // namespace mozc
