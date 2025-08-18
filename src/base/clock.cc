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

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/singleton.h"

#if !(defined(OS_CHROMEOS) || defined(_WIN32))
#define MOZC_USE_ABSL_TIME_ZONE
#endif  // !(defined(OS_CHROMEOS) || defined(_WIN32))

#ifndef MOZC_USE_ABSL_TIME_ZONE
#include <ctime>
#endif  // MOZC_USE_ABSL_TIME_ZONE

namespace mozc {
namespace {

absl::TimeZone GetLocalTimeZone() {
#ifdef MOZC_USE_ABSL_TIME_ZONE
  return absl::LocalTimeZone();
#else   // MOZC_USE_ABSL_TIME_ZONE
  // Do not use absl::LocalTimeZone() here because
  // - on Chrome OS, it returns UTC: b/196271425
  // - on Windows, it crashes: https://github.com/google/mozc/issues/856
  const time_t epoch(24 * 60 * 60);  // 1970-01-02 00:00:00 UTC
  const std::tm* offset = std::localtime(&epoch);
  if (offset == nullptr) {
    return absl::FixedTimeZone(9 * 60 * 60);  // JST as fallback
  }
  return absl::FixedTimeZone(
      (offset->tm_mday - 2) * 24 * 60 * 60  // date offset from Jan 2.
      + offset->tm_hour * 60 * 60           // hour offset from 00 am.
      + offset->tm_min * 60);               // minute offset.
#endif  // MOZC_USE_ABSL_TIME_ZONE
}

class ClockImpl : public ClockInterface {
 public:
  ClockImpl() = default;
  ~ClockImpl() override = default;

  absl::Time GetAbslTime() override { return absl::Now(); }

  absl::TimeZone GetTimeZone() override { return GetLocalTimeZone(); }
};
}  // namespace

using ClockSingleton = SingletonMockable<ClockInterface, ClockImpl>;

absl::Time Clock::GetAbslTime() { return ClockSingleton::Get()->GetAbslTime(); }

absl::TimeZone Clock::GetTimeZone() {
  return ClockSingleton::Get()->GetTimeZone();
}

void Clock::SetClockForUnitTest(ClockInterface* clock) {
  ClockSingleton::SetMock(clock);
}

}  // namespace mozc
