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

#include "base/clock_mock.h"

#include <ctime>

namespace mozc {

ClockMock::ClockMock(uint64 sec, uint32 usec)
    : seconds_(sec), micro_seconds_(usec),
      frequency_(1000000000), ticks_(0),
#ifdef __native_client__
      timezone_offset_sec_(0),
#endif  // __native_client__
      delta_seconds_(0), delta_micro_seconds_(0) {
}

ClockMock::~ClockMock() {}

void ClockMock::GetTimeOfDay(uint64 *sec, uint32 *usec) {
  *sec = seconds_;
  *usec = micro_seconds_;
  PutClockForward(delta_seconds_, delta_micro_seconds_);
}

uint64 ClockMock::GetTime() {
  const uint64 ret_sec = seconds_;
  PutClockForward(delta_seconds_, delta_micro_seconds_);
  return ret_sec;
}

bool ClockMock::GetTmWithOffsetSecond(time_t offset_sec, tm *output) {
  const time_t current_sec = static_cast<time_t>(seconds_);
  const time_t modified_sec = current_sec + offset_sec;

#ifdef OS_WIN
  if (_gmtime64_s(output, &modified_sec) != 0) {
    return false;
  }
#elif defined(__native_client__)
  const time_t localtime_sec = modified_sec + timezone_offset_sec_;
  if (gmtime_r(&localtime_sec, output) == NULL) {
    return false;
  }
#else  // !OS_WIN && !__native_client__
  if (gmtime_r(&modified_sec, output) == NULL) {
    return false;
  }
#endif
  PutClockForward(delta_seconds_, delta_micro_seconds_);

  return true;
}

uint64 ClockMock::GetFrequency() {
  return frequency_;
}

uint64 ClockMock::GetTicks() {
  return ticks_;
}

#ifdef __native_client__
void ClockMock::SetTimezoneOffset(int32 timezone_offset_sec) {
  timezone_offset_sec_ = timezone_offset_sec;
}
#endif  // __native_client__

void ClockMock::PutClockForward(uint64 delta_sec, uint32 delta_usec) {
  const uint32 one_second = 1000000u;

  if (micro_seconds_ + delta_usec < one_second) {
    seconds_ += delta_sec;
    micro_seconds_ += delta_usec;
  } else {
    seconds_ += delta_sec + 1uLL;
    micro_seconds_ += delta_usec - one_second;
  }
}

void ClockMock::PutClockForwardByTicks(uint64 ticks) {
  ticks_ += ticks;
}

void ClockMock::SetAutoPutClockForward(uint64 delta_sec,
                                       uint32 delta_usec) {
  delta_seconds_ = delta_sec;
  delta_micro_seconds_ = delta_usec;
}

void ClockMock::SetTime(uint64 sec, uint32 usec) {
  seconds_ = sec;
  micro_seconds_ = usec;
}

void ClockMock::SetFrequency(uint64 frequency) {
  frequency_ = frequency;
}

void ClockMock::SetTicks(uint64 ticks) {
  ticks_ = ticks;
}

}  // namespace mozc
