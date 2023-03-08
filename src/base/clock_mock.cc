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

#include "base/clock_mock.h"

#include <cstdint>
#include <ctime>

#include "absl/time/time.h"

namespace mozc {

namespace {
constexpr uint64_t kMicro = 1'000'000;
}

ClockMock::ClockMock(uint64_t sec, uint32_t usec)
    : time_(absl::FromUnixMicros(sec * kMicro + usec)) {}

void ClockMock::GetTimeOfDay(uint64_t *sec, uint32_t *usec) {
  const absl::Time time = GetAbslTime();
  *sec = absl::ToUnixSeconds(time);
  *usec = absl::ToUnixMicros(time) % kMicro;
}

uint64_t ClockMock::GetTime() { return absl::ToUnixSeconds(GetAbslTime()); }

absl::Time ClockMock::GetAbslTime() {
  const absl::Time output = time_;
  Advance(auto_delta_);
  return output;
}

absl::TimeZone ClockMock::GetTimeZone() { return timezone_; }

void ClockMock::PutClockForward(uint64_t delta_sec, uint32_t delta_usec) {
  time_ += absl::Seconds(delta_sec) + absl::Microseconds(delta_usec);
}

void ClockMock::SetAutoPutClockForward(uint64_t delta_sec,
                                       uint32_t delta_usec) {
  auto_delta_ = absl::Seconds(delta_sec) + absl::Microseconds(delta_usec);
}

void ClockMock::SetTime(uint64_t sec, uint32_t usec) {
  time_ = absl::FromUnixMicros(sec * kMicro + usec);
}

}  // namespace mozc
