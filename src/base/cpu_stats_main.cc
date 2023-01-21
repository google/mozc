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
#include <memory>
#include <ostream>
#include <string>

#include "base/cpu_stats.h"
#include "base/init_mozc.h"
#include "base/port.h"
#include "base/thread.h"
#include "base/util.h"
#include "absl/flags/flag.h"

ABSL_FLAG(int32_t, iterations, 1000, "number of iterations");
ABSL_FLAG(int32_t, polling_duration, 1000, "duration period in msec");
ABSL_FLAG(int32_t, dummy_threads_size, 0, "number of dummy threads");

namespace {
class DummyThread : public mozc::Thread {
 public:
  DummyThread() {}
  DummyThread(const DummyThread&) = delete;
  DummyThread& operator=(const DummyThread&) = delete;
  void Run() override {
    volatile uint64_t n = 0;
    while (true) {
      ++n;
      --n;
    }
  }
};
}  // namespace

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  std::unique_ptr<DummyThread[]> threads;

  if (absl::GetFlag(FLAGS_dummy_threads_size) > 0) {
    threads = std::make_unique<DummyThread[]>(
        absl::GetFlag(FLAGS_dummy_threads_size));
    for (int i = 0; i < absl::GetFlag(FLAGS_dummy_threads_size); ++i) {
      threads[i].Start("CpuStatsMain");
    }
  }

  mozc::CPUStats stats;
  std::cout << "NumberOfProcessors: " << stats.GetNumberOfProcessors()
            << std::endl;
  for (int i = 0; i < absl::GetFlag(FLAGS_iterations); ++i) {
    std::cout << "CPUStats: " << stats.GetSystemCPULoad() << " "
              << stats.GetCurrentProcessCPULoad() << std::endl;
    mozc::Util::Sleep(absl::GetFlag(FLAGS_polling_duration));
  }

  return 0;
}
