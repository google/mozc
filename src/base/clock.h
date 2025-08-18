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

#include "absl/time/time.h"

namespace mozc {

class ClockInterface {
 public:
  virtual ~ClockInterface() = default;

  virtual absl::Time GetAbslTime() = 0;
  virtual absl::TimeZone GetTimeZone() = 0;
};

class Clock {
 public:
  Clock() = delete;
  Clock(const Clock&) = delete;
  Clock& operator=(const Clock&) = delete;

  // Returns the current time in absl::Time.
  static absl::Time GetAbslTime();

  // Returns the timezone. LocalTimeZone is usually returned.
  static absl::TimeZone GetTimeZone();

  // TESTONLY: The behavior of global system clock can be overridden by using
  // this method.  Set to nullptr to restore the default clock.  This method
  // doesn't take the ownership of |clock|.
  static void SetClockForUnitTest(ClockInterface* clock);
};

}  // namespace mozc

#endif  // MOZC_BASE_CLOCK_H_
