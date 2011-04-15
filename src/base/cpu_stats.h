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

#ifndef MOZC_BASE_CPU_STATS_H_
#define MOZC_BASE_CPU_STATS_H_

#include "base/base.h"

namespace mozc {

class CPUStatsInterface {
 public:
  CPUStatsInterface() {}
  virtual ~CPUStatsInterface() {}

  // returns the percentage of total CPU load since the last time
  // this method was called.
  // The return value must be in the range of [0.0 .. 1.0].
  virtual float GetSystemCPULoad() = 0;

  // returns the percentage of current process's CPU load
  // since the last time this method was called.
  // If the process has multi-threads, the return value
  // may be larger than 1.0.
  // Use GetNumberOfProcessors to normalize the CPU load by
  // the number of processors.
  virtual float GetCurrentProcessCPULoad() = 0;

  // returns the number of processors.
  virtual size_t GetNumberOfProcessors() const = 0;
};

// default implementation
class CPUStats : public CPUStatsInterface {
 public:
  // return 0.0 if CPU load is unknown
  float GetSystemCPULoad();

  // return 0.0 if CPU load is unknown
  float GetCurrentProcessCPULoad();

  // return the number of processors
  size_t GetNumberOfProcessors() const;

  CPUStats();
  virtual ~CPUStats();

 private:
  uint64 prev_system_total_times_;
  uint64 prev_system_cpu_times_;
  uint64 prev_current_process_total_times_;
  uint64 prev_current_process_cpu_times_;
};
}  // namespace mozc

#endif  // MOZC_BASE_CPU_STATS_H_
