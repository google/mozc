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

#include <cstdint>
#include <iostream>
#include <ostream>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/cpu_stats.h"
#include "base/init_mozc.h"
#include "base/thread.h"

ABSL_FLAG(int32_t, iterations, 1000, "number of iterations");
ABSL_FLAG(absl::Duration, polling_interval, absl::Seconds(1),
          "polling interval e.g. 1s, 500ms");
ABSL_FLAG(int32_t, dummy_threads_size, 0, "number of dummy threads");

int main(int argc, char** argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  absl::Notification cancel;

  std::vector<mozc::Thread> threads;
  threads.reserve(absl::GetFlag(FLAGS_dummy_threads_size));
  for (int i = 0; i < absl::GetFlag(FLAGS_dummy_threads_size); ++i) {
    threads.push_back(mozc::Thread([&cancel] {
      volatile uint64_t n = 0;
      while (!cancel.HasBeenNotified()) {
        ++n;
        --n;
      }
    }));
  }

  mozc::CPUStats stats;
  std::cout << "NumberOfProcessors: " << stats.GetNumberOfProcessors()
            << std::endl;
  for (int i = 0; i < absl::GetFlag(FLAGS_iterations); ++i) {
    std::cout << "CPUStats: " << stats.GetSystemCPULoad() << " "
              << stats.GetCurrentProcessCPULoad() << std::endl;
    absl::SleepFor(absl::GetFlag(FLAGS_polling_interval));
  }

  cancel.Notify();

  return 0;
}
