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

#include "base/clock.h"

#include "base/singleton.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

#if defined(OS_CHROMEOS) || defined(_WIN32)
#include <time.h>
#endif  // defined(OS_CHROMEOS) || defined(_WIN32)

namespace mozc {
namespace {

class ClockImpl : public ClockInterface {
 public:
  ClockImpl() : timezone_(absl::LocalTimeZone()) {
#if defined(OS_CHROMEOS) || defined(_WIN32)
    // Because absl::LocalTimeZone() always returns UTC timezone on Chrome OS
    // and Windows, a work-around for Chrome OS and Windows is required.
    int offset_sec = 9 * 60 * 60;      // JST as fallback
    const time_t epoch(24 * 60 * 60);  // 1970-01-02 00:00:00 UTC
    const std::tm *offset = std::localtime(&epoch);
    if (offset) {
      offset_sec =
          (offset->tm_mday - 2) * 24 * 60 * 60  // date offset from Jan 2.
          + offset->tm_hour * 60 * 60           // hour offset from 00 am.
          + offset->tm_min * 60;                // minute offset.
    }
    timezone_ = absl::FixedTimeZone(offset_sec);
#endif  // defined(OS_CHROMEOS) || defined(_WIN32)
  }
  ~ClockImpl() override = default;

  absl::Time GetAbslTime() override { return absl::Now(); }

  absl::TimeZone GetTimeZone() override { return timezone_; }

 private:
  absl::TimeZone timezone_;
};
}  // namespace

using ClockSingleton = SingletonMockable<ClockInterface, ClockImpl>;

absl::Time Clock::GetAbslTime() { return ClockSingleton::Get()->GetAbslTime(); }

absl::TimeZone Clock::GetTimeZone() {
  return ClockSingleton::Get()->GetTimeZone();
}

void Clock::SetClockForUnitTest(ClockInterface *clock) {
  ClockSingleton::SetMock(clock);
}

}  // namespace mozc
