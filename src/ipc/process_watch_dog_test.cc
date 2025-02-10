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

#include "ipc/process_watch_dog.h"

#include <cstdlib>

#include "absl/log/log.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/clock.h"
#include "testing/gunit.h"

namespace mozc {

TEST(ProcessWatchDog, ProcessWatchDogTest) {
#ifndef _WIN32
  absl::Time start = absl::Now();
  // revoke myself with different parameter
  pid_t pid = fork();
  if (pid == 0) {
    // Child;
    absl::SleepFor(absl::Seconds(2));
    exit(0);
  } else if (pid > 0) {
    ProcessWatchDog watchdog([start](ProcessWatchDog::SignalType type) {
      EXPECT_EQ(type, ProcessWatchDog::SignalType::PROCESS_SIGNALED);
      const absl::Duration diff = Clock::GetAbslTime() - start;
      EXPECT_LE(absl::AbsDuration(diff), absl::Seconds(1));
    });
    watchdog.SetId(static_cast<ProcessWatchDog::ProcessId>(pid),
                   ProcessWatchDog::kUnknownThreadId);
    absl::SleepFor(absl::Seconds(4));
  } else {
    LOG(ERROR) << "cannot execute fork";
  }
#endif  // !_WIN32
}
}  // namespace mozc
