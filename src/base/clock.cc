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

#ifdef _WIN32
#include <windows.h>
#include <time.h>
#else  // _WIN32
#ifdef __APPLE__
#include <mach/mach_time.h>
#endif  // __APPLE__
#include <sys/time.h>
#endif  // _WIN32

#include <cstdint>

#include "base/singleton.h"
#include "absl/time/clock.h"

namespace mozc {
namespace {

class ClockImpl : public ClockInterface {
 public:
  ClockImpl() : timezone_offset_sec_(0), timezone_(absl::LocalTimeZone()) {
#if defined(OS_CHROMEOS) || defined(_WIN32)
    // Because absl::LocalTimeZone() always returns UTC timezone on Chrome OS
    // and Windows, a work-around for Chrome OS and Windows is required.
    int offset_sec = 9 * 60 * 60;  // JST as fallback
    const time_t epoch(24 * 60 * 60);  // 1970-01-02 00:00:00 UTC
    const std::tm *offset = std::localtime(&epoch);
    if (offset) {
      offset_sec =
          (offset->tm_mday - 2) * 24 * 60 * 60  // date offset from Jan 2.
          + offset->tm_hour * 60 * 60  // hour offset from 00 am.
          + offset->tm_min * 60;  // minute offset.
    }
    timezone_ = absl::FixedTimeZone(offset_sec);
#endif  // defined(OS_CHROMEOS) || defined(_WIN32)
  }
  ~ClockImpl() override = default;

  void GetTimeOfDay(uint64_t *sec, uint32_t *usec) override {
#ifdef _WIN32
    FILETIME file_time;
    GetSystemTimeAsFileTime(&file_time);
    ULARGE_INTEGER time_value;
    time_value.HighPart = file_time.dwHighDateTime;
    time_value.LowPart = file_time.dwLowDateTime;
    // Convert into microseconds
    time_value.QuadPart /= 10;
    // kDeltaEpochInMicroSecs is difference between January 1, 1970 and
    // January 1, 1601 in microsecond.
    // This number is calculated as follows.
    // ((1970 - 1601) * 365 + 89) * 24 * 60 * 60 * 1000000
    // 89 is the number of leap years between 1970 and 1601.
    const uint64_t kDeltaEpochInMicroSecs = 11644473600000000ULL;
    // Convert file time to unix epoch
    time_value.QuadPart -= kDeltaEpochInMicroSecs;
    *sec = static_cast<uint64_t>(time_value.QuadPart / 1000000UL);
    *usec = static_cast<uint32_t>(time_value.QuadPart % 1000000UL);
#else   // _WIN32
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    *sec = tv.tv_sec;
    *usec = tv.tv_usec;
#endif  // _WIN32
  }

  uint64_t GetTime() override {
#ifdef _WIN32
    return static_cast<uint64_t>(_time64(nullptr));
#else   // _WIN32
    return static_cast<uint64_t>(time(nullptr));
#endif  // _WIN32
  }

  absl::Time GetAbslTime() override {
    return absl::Now();
  }

  const absl::TimeZone& GetTimeZone() override {
    return timezone_;
  }

  void SetTimeZoneOffset(int32_t timezone_offset_sec) override {
    timezone_offset_sec_ = timezone_offset_sec;
    timezone_ = absl::FixedTimeZone(timezone_offset_sec);
  }

 private:
  int32_t timezone_offset_sec_;
  absl::TimeZone timezone_;
};
}  // namespace

using ClockSingleton = SingletonMockable<ClockInterface, ClockImpl>;

void Clock::GetTimeOfDay(uint64_t *sec, uint32_t *usec) {
  ClockSingleton::Get()->GetTimeOfDay(sec, usec);
}

uint64_t Clock::GetTime() { return ClockSingleton::Get()->GetTime(); }

absl::Time Clock::GetAbslTime() { return ClockSingleton::Get()->GetAbslTime(); }

const absl::TimeZone& Clock::GetTimeZone() {
  return ClockSingleton::Get()->GetTimeZone();
}

void Clock::SetTimeZoneOffset(int32_t timezone_offset_sec) {
  return ClockSingleton::Get()->SetTimeZoneOffset(timezone_offset_sec);
}

void Clock::SetClockForUnitTest(ClockInterface *clock) {
  ClockSingleton::SetMock(clock);
}


}  // namespace mozc
