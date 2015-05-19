// Copyright 2010-2014, Google Inc.
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
#include "base/util.h"

namespace mozc {
Stopwatch Stopwatch::StartNew() {
  Stopwatch stopwatch = Stopwatch();
  stopwatch.Start();
  return stopwatch;
}

Stopwatch::Stopwatch()
    : state_(STOPWATCH_STOPPED),
      frequency_(1000),
      start_timestamp_(0),
      elapsed_timestamp_(0) {
  frequency_ = Util::GetFrequency();

  Reset();
}

void Stopwatch::Reset() {
  start_timestamp_ = elapsed_timestamp_ = 0;
  state_ = STOPWATCH_STOPPED;
}

void Stopwatch::Start() {
  if (state_ == STOPWATCH_STOPPED) {
    start_timestamp_ = Util::GetTicks();
    state_ = STOPWATCH_RUNNING;
  }
}

void Stopwatch::Stop() {
  if (state_ == STOPWATCH_RUNNING) {
    const int64 stop_timestamp = Util::GetTicks();
    elapsed_timestamp_ += (stop_timestamp - start_timestamp_);
    start_timestamp_ = 0;
    state_ = STOPWATCH_STOPPED;
  }
}

int64 Stopwatch::GetElapsedMilliseconds() {
  return GetElapsedTicks() * 1000 / frequency_;
}

double Stopwatch::GetElapsedMicroseconds() {
  return GetElapsedTicks() * 1.0e6 / frequency_;
}

double Stopwatch::GetElapsedNanoseconds() {
  return GetElapsedTicks() * 1.0e9 / frequency_;
}

int64 Stopwatch::GetElapsedTicks() {
  if (state_ == STOPWATCH_STOPPED) {
    return elapsed_timestamp_;
  }

  const int64 current_timestamp = Util::GetTicks();
  elapsed_timestamp_ += (current_timestamp - start_timestamp_);
  start_timestamp_ = current_timestamp;

  return elapsed_timestamp_;
}

bool Stopwatch::IsRunning() const {
  return state_ == STOPWATCH_RUNNING;
}
}  // namespace mozc
