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

#ifndef MOZC_BASE_CLOCK_H_
#define MOZC_BASE_CLOCK_H_

#include <cstdint>
#include <ctime>

#include "base/port.h"
#include "absl/time/time.h"

namespace mozc {

class ClockInterface {
 public:
  virtual ~ClockInterface() = default;

  virtual void GetTimeOfDay(uint64_t *sec, uint32_t *usec) = 0;
  virtual uint64_t GetTime() = 0;
  virtual absl::Time GetAbslTime() = 0;

  // High accuracy clock.
  virtual uint64_t GetFrequency() = 0;
  virtual uint64_t GetTicks() = 0;

  virtual const absl::TimeZone& GetTimeZone() = 0;
  virtual void SetTimeZoneOffset(int32_t timezone_offset_sec) = 0;

 protected:
  ClockInterface() = default;
};

class Clock {
 public:
  // Gets the current time using gettimeofday-like functions.
  // sec: number of seconds from epoch
  // usec: micro-second passed: [0,1000000)
  static void GetTimeOfDay(uint64_t *sec, uint32_t *usec);

  // Gets the current time using time-like function
  // For Windows, _time64() is used.
  // For Linux/Mac, time() is used.
  static uint64_t GetTime();

  // Returns the current time in absl::Time.
  static absl::Time GetAbslTime();

  // Gets the system frequency to calculate the time from ticks.
  static uint64_t GetFrequency();

  // Gets the current ticks. It may return incorrect value on Virtual Machines.
  // If you'd like to get a value in secs, it is necessary to divide a result by
  // GetFrequency().
  static uint64_t GetTicks();

  // Returns the timezone. LocalTimeZone is usually returned.
  static const absl::TimeZone& GetTimeZone();

  // Sets the time difference between local time and UTC time in seconds.
  // We use this function in NaCl Mozc because we can't know the local timezone
  // in NaCl environment.
  static void SetTimeZoneOffset(int32_t timezone_offset_sec);

  // TESTONLY: The behavior of global system clock can be overridden by using
  // this method.  Set to nullptr to restore the default clock.  This method
  // doesn't take the ownership of |clock|.
  static void SetClockForUnitTest(ClockInterface *clock);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Clock);
};

}  // namespace mozc

#endif  // MOZC_BASE_CLOCK_H_
