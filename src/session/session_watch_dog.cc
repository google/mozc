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

#include "session/session_watch_dog.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <numeric>
#include <string>
#include <utility>

#include "base/clock.h"
#include "base/cpu_stats.h"
#include "base/logging.h"
#include "base/system_util.h"
#include "base/thread.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "client/client.h"
#include "client/client_interface.h"

namespace mozc {
namespace {

// IPC timeout
constexpr absl::Duration kCleanupTimeout = absl::Seconds(30);
constexpr absl::Duration kPingTimeout = absl::Seconds(5);

// number of trials for ping
constexpr int32_t kPingTrial = 3;
constexpr absl::Duration kPingInterval = absl::Seconds(1);

// Average CPU load for last 1 min.
// If the load > kMinimumAllCPULoad, don't send Cleanup
constexpr float kMinimumAllCPULoad = 0.33f;

// Average CPU load for last 10 secs.
// If the load > kMinimumLatestCPULoad, don't send Cleanup
constexpr float kMinimumLatestCPULoad = 0.66f;

}  // namespace

SessionWatchDog::SessionWatchDog(absl::Duration interval)
    : SessionWatchDog(interval, client::ClientFactory::NewClient(),
                      std::make_unique<CPUStats>()) {}

SessionWatchDog::SessionWatchDog(
    absl::Duration interval, std::unique_ptr<client::ClientInterface> client,
    std::unique_ptr<CPUStatsInterface> cpu_stats)
    // allow [1..600].
    : interval_(std::clamp(interval, absl::Seconds(1), absl::Seconds(600))),
      client_(std::move(client)),
      cpu_stats_(std::move(cpu_stats)),
      thread_([this] { ThreadMain(); }) {}

SessionWatchDog::~SessionWatchDog() {
  stop_.Notify();
  thread_.Join();
}

void SessionWatchDog::ThreadMain() {
  // CPU load check
  std::array<float, 16> cpu_loads = {};  // 60/5 = 12 is the minimal size
  float total_cpu_load = 0.0;
  float current_process_cpu_load = 0.0;
  const size_t number_of_processors = cpu_stats_->GetNumberOfProcessors();

  DCHECK_GE(number_of_processors, 1);

  // the first (interval_sec_ - 60) sec: -> Do nothing
  const absl::Duration idle_interval =
      std::max(absl::ZeroDuration(), (interval_ - absl::Seconds(60)));

  // last 60 sec: -> check CPU usage
  const absl::Duration cpu_check_interval =
      std::min(absl::Seconds(60), interval_);

  // for every 5 second, get CPU load percentage
  const absl::Duration cpu_check_duration =
      std::min(absl::Seconds(5), interval_);

  absl::Time last_cleanup_time = Clock::GetAbslTime();

  while (true) {
    VLOG(1) << "Start sleeping " << idle_interval;
    if (stop_.WaitForNotificationWithTimeout(idle_interval)) {
      VLOG(1) << "Received stop signal";
      return;
    }
    VLOG(1) << "Finish sleeping " << idle_interval;

    int32_t cpu_loads_index = 0;
    for (absl::Duration n = absl::ZeroDuration(); n < cpu_check_interval;
         n += cpu_check_duration) {
      if (stop_.WaitForNotificationWithTimeout(cpu_check_duration)) {
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
      const float extracted_cpu_load =
          total_cpu_load - current_process_cpu_load / number_of_processors;
      cpu_loads[cpu_loads_index++] = std::max(0.0f, extracted_cpu_load);
    }

    DCHECK_GT(cpu_loads_index, 0);

    const absl::Time current_cleanup_time = Clock::GetAbslTime();
    if (!CanSendCleanupCommand(
            absl::MakeSpan(cpu_loads).subspan(0, cpu_loads_index),
            current_cleanup_time, last_cleanup_time)) {
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
      if (stop_.WaitForNotificationWithTimeout(kPingInterval)) {
        VLOG(1) << "Received stop signal";
        return;
      }
      if (client_->PingServer()) {
        VLOG(2) << "Ping command succeeded";
        failed = false;
        break;
      }
      LOG(ERROR) << "Ping command failed, waiting " << kPingInterval
                 << ", trial: " << i;
    }

    if (failed) {
      if (stop_.WaitForNotificationWithTimeout(absl::Milliseconds(100))) {
        VLOG(1) << "Parent thread is already terminated";
        return;
      }
#ifndef MOZC_NO_LOGGING
      // We have received crash dumps caused by the following LOG(FATAL).
      // Unfortunately, we cannot investigate the cause of this error,
      // as the crash dump doesn't contain any logging information.
      // Here we temporary save the user name into stack in order
      // to obtain the log file before the LOG(FATAL).
      char user_name[32];
      const std::string tmp = SystemUtil::GetUserNameAsString();
      strncpy(user_name, tmp.c_str(), sizeof(user_name));
      VLOG(1) << "user_name: " << user_name;
#endif  // !MOZC_NO_LOGGING
      LOG(FATAL) << "Cleanup commands failed. Rasing exception...";
    }
  }
}

bool SessionWatchDog::CanSendCleanupCommand(
    absl::Span<const float> cpu_loads, absl::Time current_cleanup_time,
    absl::Time last_cleanup_time) const {
  if (current_cleanup_time <= last_cleanup_time) {
    LOG(ERROR) << "time stamps are the same. clock may be altered";
    return false;
  }

  const float all_avg =
      std::accumulate(cpu_loads.begin(), cpu_loads.end(), 0.0) /
      cpu_loads.size();

  const size_t latest_size = std::min(size_t{2}, cpu_loads.size());
  const float latest_avg =
      std::accumulate(cpu_loads.begin(), cpu_loads.end(), 0.0) / latest_size;

  VLOG(1) << "Average CPU load=" << all_avg
          << " latest CPU load=" << latest_avg;

  if (all_avg > kMinimumAllCPULoad || latest_avg > kMinimumLatestCPULoad) {
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
