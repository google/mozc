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

#include <cstdint>
#include <vector>

#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/cpu_stats.h"
#include "base/thread.h"
#include "testing/gunit.h"

namespace mozc {
namespace {
TEST(CPUStats, CPUStatsTest) {
  CPUStats stats;
  EXPECT_GE(stats.GetSystemCPULoad(), 0.0);
  EXPECT_LE(stats.GetSystemCPULoad(), 1.0);

  EXPECT_GE(stats.GetCurrentProcessCPULoad(), 0.0);

  EXPECT_GE(stats.GetNumberOfProcessors(), 1);
}

TEST(CPUStats, MultiThreadTest) {
  absl::Notification cancel;

  constexpr int kDummyThreadsSize = 32;

  std::vector<mozc::Thread> threads;
  threads.reserve(kDummyThreadsSize);
  for (int i = 0; i < kDummyThreadsSize; ++i) {
    threads.push_back(mozc::Thread([&cancel] {
      volatile uint64_t n = 0;
      // Makes busy loop.
      while (!cancel.HasBeenNotified()) {
        ++n;
        --n;
      }
    }));
  }

  mozc::CPUStats stats;
  constexpr int kNumIterations = 10;
  for (int i = 0; i < kNumIterations; ++i) {
    EXPECT_GE(stats.GetSystemCPULoad(), 0.0);
    EXPECT_GE(stats.GetCurrentProcessCPULoad(), 0.0);
    EXPECT_GE(stats.GetNumberOfProcessors(), 1);
    absl::SleepFor(absl::Milliseconds(10));
  }

  cancel.Notify();

  for (auto& thread : threads) {
    thread.Join();
  }
}
}  // namespace
}  // namespace mozc
