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

#include "base/cpu_stats.h"

#include <cstddef>
#include <cstdint>

#include "absl/log/log.h"

#ifdef _WIN32
#include <windows.h>
#endif  // _WIN32

#ifdef __APPLE__
#include <mach/host_info.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/task.h>
#include <sys/time.h>
#endif  // __APPLE__

namespace mozc {
namespace {

#ifdef _WIN32
uint64_t FileTimeToInt64(const FILETIME& file_time) {
  return (static_cast<uint64_t>(file_time.dwHighDateTime) << 32) |
         (static_cast<uint64_t>(file_time.dwLowDateTime));
}
#endif  // _WIN32

#ifdef __APPLE__
uint64_t TimeValueTToInt64(const time_value_t& time_value) {
  return 1000000ULL * time_value.seconds + time_value.microseconds;
}
#endif  // __APPLE__

float UpdateCPULoad(uint64_t current_total_times, uint64_t current_cpu_times,
                    uint64_t* prev_total_times, uint64_t* prev_cpu_times) {
  float result = 0.0;
  if (current_total_times < *prev_total_times ||
      current_cpu_times < *prev_cpu_times) {
    LOG(ERROR) << "Inconsistent time values are passed. ignored";
  } else {
    const uint64_t total_diff = current_total_times - *prev_total_times;
    const uint64_t cpu_diff = current_cpu_times - *prev_cpu_times;
    result =
        (total_diff == 0ULL ? 0.0
                            : static_cast<float>(1.0 * cpu_diff / total_diff));
  }

  *prev_total_times = current_total_times;
  *prev_cpu_times = current_cpu_times;
  return result;
}
}  // namespace

float CPUStats::GetSystemCPULoad() {
#ifdef _WIN32
  FILETIME idle_time, kernel_time, user_time;
  // Note that GetSystemTimes is available in Windows XP SP1 or later.
  // http://msdn.microsoft.com/en-us/library/ms724400.aspx
  if (!::GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
    LOG(ERROR) << "::GetSystemTimes() failed: " << ::GetLastError();
    return 0.0;
  }

  // kernel_time includes Kernel idle time, so no need to
  // include cpu_time as total_times
  const uint64_t total_times =
      FileTimeToInt64(kernel_time) + FileTimeToInt64(user_time);
  const uint64_t cpu_times = total_times - FileTimeToInt64(idle_time);
#endif  // _WIN32

#ifdef __APPLE__
  host_cpu_load_info_data_t cpu_info;
  mach_msg_type_number_t info_count = HOST_CPU_LOAD_INFO_COUNT;
  if (KERN_SUCCESS != host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
                                      reinterpret_cast<host_info_t>(&cpu_info),
                                      &info_count)) {
    LOG(ERROR) << "::host_statistics() failed";
    return 0.0;
  }

  const uint64_t cpu_times = cpu_info.cpu_ticks[CPU_STATE_NICE] +
                             cpu_info.cpu_ticks[CPU_STATE_SYSTEM] +
                             cpu_info.cpu_ticks[CPU_STATE_USER];
  const uint64_t total_times = cpu_times + cpu_info.cpu_ticks[CPU_STATE_IDLE];

#endif  // __APPLE__

#if defined(__linux__) || defined(__wasm__)
  // NOT IMPLEMENTED
  // TODO(taku): implement Linux version
  // can take the info from /proc/stats
  const uint64_t total_times = 0;
  const uint64_t cpu_times = 0;
#endif  // __linux__ || __wasm__

  return UpdateCPULoad(total_times, cpu_times, &prev_system_total_times_,
                       &prev_system_cpu_times_);
}

float CPUStats::GetCurrentProcessCPULoad() {
#ifdef _WIN32
  FILETIME current_file_time;
  ::GetSystemTimeAsFileTime(&current_file_time);

  FILETIME create_time, exit_time, kernel_time, user_time;
  if (!::GetProcessTimes(::GetCurrentProcess(), &create_time, &exit_time,
                         &kernel_time, &user_time)) {
    LOG(ERROR) << "::GetProcessTimes() failed: " << ::GetLastError();
    return 0.0;
  }

  const uint64_t total_times =
      FileTimeToInt64(current_file_time) - FileTimeToInt64(create_time);
  const uint64_t cpu_times =
      FileTimeToInt64(kernel_time) + FileTimeToInt64(user_time);
#endif  // _WIN32

#ifdef __APPLE__
  task_thread_times_info task_times_info;
  mach_msg_type_number_t info_count = TASK_THREAD_TIMES_INFO_COUNT;

  if (KERN_SUCCESS != task_info(mach_task_self(), TASK_THREAD_TIMES_INFO,
                                reinterpret_cast<task_info_t>(&task_times_info),
                                &info_count)) {
    LOG(ERROR) << "::task_info() failed";
    return 0.0;
  }

  // OSX has no host_get_time(), use gettimeofday instead.
  // http://www.gnu.org/software/hurd/gnumach-doc/Host-Time.html
  // OSX's basic_info_t has no filed |creation_time|, we cannot use it.
  // The initial value might be different from the real CPU load.
  struct timeval tv;
  gettimeofday(&tv, nullptr);

  const uint64_t total_times = 1000000ULL * tv.tv_sec + tv.tv_usec;
  const uint64_t cpu_times = TimeValueTToInt64(task_times_info.user_time) +
                             TimeValueTToInt64(task_times_info.system_time);
#endif  // __APPLE__

#if defined(__linux__) || defined(__wasm__)
  // not implemented
  const uint64_t total_times = 0;
  const uint64_t cpu_times = 0;
#endif  // __linux__ || __wasm__

  return UpdateCPULoad(total_times, cpu_times,
                       &prev_current_process_total_times_,
                       &prev_current_process_cpu_times_);
}

size_t CPUStats::GetNumberOfProcessors() const {
#ifdef _WIN32
  SYSTEM_INFO info;
  ::GetSystemInfo(&info);
  return static_cast<size_t>(info.dwNumberOfProcessors);
#endif  // _WIN32

#ifdef __APPLE__
  host_basic_info basic_info;
  mach_msg_type_number_t info_count = HOST_BASIC_INFO_COUNT;
  if (KERN_SUCCESS != host_info(mach_host_self(), HOST_BASIC_INFO,
                                reinterpret_cast<task_info_t>(&basic_info),
                                &info_count)) {
    LOG(ERROR) << "::host_info() failed";
    return static_cast<size_t>(1);
  }

  return static_cast<size_t>(basic_info.avail_cpus);
#endif  // __APPLE__

#if defined(__linux__) || defined(__wasm__)
  // Not implemented
  return 1;
#endif  // __linux__ || __wasm__
}
}  // namespace mozc
