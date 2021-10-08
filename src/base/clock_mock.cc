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

namespace mozc {

ClockMock::ClockMock(uint64_t sec, uint32_t usec)
    : seconds_(sec),
      frequency_(1000000000),
      ticks_(0),
      timezone_(absl::UTCTimeZone()),
      timezone_offset_sec_(0),
      micro_seconds_(usec),
      delta_seconds_(0),
      delta_micro_seconds_(0) {}

ClockMock::~ClockMock() {}

void ClockMock::GetTimeOfDay(uint64_t *sec, uint32_t *usec) {
  *sec = seconds_;
  *usec = micro_seconds_;
  PutClockForward(delta_seconds_, delta_micro_seconds_);
}

uint64_t ClockMock::GetTime() {
  const uint64_t ret_sec = seconds_;
  PutClockForward(delta_seconds_, delta_micro_seconds_);
  return ret_sec;
}

absl::Time ClockMock::GetAbslTime() {
  absl::Time at = absl::FromUnixMicros(seconds_ * 1'000'000 + micro_seconds_);
  PutClockForward(delta_seconds_, delta_micro_seconds_);
  return at;
}

uint64_t ClockMock::GetFrequency() { return frequency_; }

uint64_t ClockMock::GetTicks() { return ticks_; }

const absl::TimeZone& ClockMock::GetTimeZone() {
  return timezone_;
}

void ClockMock::SetTimeZoneOffset(int32_t timezone_offset_sec) {
  timezone_offset_sec_ = timezone_offset_sec;
  timezone_ = absl::FixedTimeZone(timezone_offset_sec);
}

void ClockMock::PutClockForward(uint64_t delta_sec, uint32_t delta_usec) {
  const uint32_t one_second = 1000000u;

  if (micro_seconds_ + delta_usec < one_second) {
    seconds_ += delta_sec;
    micro_seconds_ += delta_usec;
  } else {
    seconds_ += delta_sec + 1uLL;
    micro_seconds_ += delta_usec - one_second;
  }
}

void ClockMock::PutClockForwardByTicks(uint64_t ticks) { ticks_ += ticks; }

void ClockMock::SetAutoPutClockForward(uint64_t delta_sec,
                                       uint32_t delta_usec) {
  delta_seconds_ = delta_sec;
  delta_micro_seconds_ = delta_usec;
}

void ClockMock::SetTime(uint64_t sec, uint32_t usec) {
  seconds_ = sec;
  micro_seconds_ = usec;
}

void ClockMock::SetFrequency(uint64_t frequency) { frequency_ = frequency; }

void ClockMock::SetTicks(uint64_t ticks) { ticks_ = ticks; }

}  // namespace mozc
