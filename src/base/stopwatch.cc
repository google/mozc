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

#include "base/stopwatch.h"

#include "absl/time/time.h"
#include "base/clock.h"

namespace mozc {

Stopwatch Stopwatch::StartNew() {
  Stopwatch stopwatch = Stopwatch();
  stopwatch.Start();
  return stopwatch;
}

void Stopwatch::Reset() {
  state_ = STOPWATCH_STOPPED;
  elapsed_ = absl::ZeroDuration();
}

void Stopwatch::Start() {
  if (state_ == STOPWATCH_STOPPED) {
    start_ = Clock::GetAbslTime();
    state_ = STOPWATCH_RUNNING;
  }
}

void Stopwatch::Stop() {
  if (state_ == STOPWATCH_RUNNING) {
    elapsed_ += GetElapsed();
    state_ = STOPWATCH_STOPPED;
  }
}

absl::Duration Stopwatch::GetElapsed() const {
  if (state_ == STOPWATCH_STOPPED) {
    return elapsed_;
  }
  return Clock::GetAbslTime() - start_;
}

bool Stopwatch::IsRunning() const { return state_ == STOPWATCH_RUNNING; }

}  // namespace mozc
