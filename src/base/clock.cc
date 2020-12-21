// Copyright 2010-2020, Google Inc.
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

#ifdef OS_WIN
#include <Windows.h>
#include <time.h>
#else  // OS_WIN
#ifdef __APPLE__
#include <mach/mach_time.h>
#elif defined(OS_NACL)  // __APPLE__
#include <irt.h>
#endif  // __APPLE__ or OS_NACL
#include <sys/time.h>
#endif  // OS_WIN

#include "base/singleton.h"
#include "absl/time/clock.h"

namespace mozc {
namespace {

class ClockImpl : public ClockInterface {
 public:
  ClockImpl() : timezone_offset_sec_(0), timezone_(absl::LocalTimeZone()) {
#if OS_WIN
    // Because absl::LocalTimeZone() always returns UTC timezone on Windows,
    // a work-around for Windows is required.
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
#endif  // OS_WIN
  }
  ~ClockImpl() override {}

  void GetTimeOfDay(uint64 *sec, uint32 *usec) override {
#ifdef OS_WIN
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
    const uint64 kDeltaEpochInMicroSecs = 11644473600000000ULL;
    // Convert file time to unix epoch
    time_value.QuadPart -= kDeltaEpochInMicroSecs;
    *sec = static_cast<uint64>(time_value.QuadPart / 1000000UL);
    *usec = static_cast<uint32>(time_value.QuadPart % 1000000UL);
#else   // OS_WIN
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    *sec = tv.tv_sec;
    *usec = tv.tv_usec;
#endif  // OS_WIN
  }

  uint64 GetTime() override {
#ifdef OS_WIN
    return static_cast<uint64>(_time64(nullptr));
#else
    return static_cast<uint64>(time(nullptr));
#endif  // OS_WIN
  }

  absl::Time GetAbslTime() override {
    return absl::Now();
  }

  uint64 GetFrequency() override {
#if defined(OS_WIN)
    LARGE_INTEGER timestamp;
    // TODO(yukawa): Consider the case where QueryPerformanceCounter is not
    // available.
    const BOOL result = ::QueryPerformanceFrequency(&timestamp);
    return static_cast<uint64>(timestamp.QuadPart);
#elif defined(__APPLE__)
    static mach_timebase_info_data_t timebase_info;
    mach_timebase_info(&timebase_info);
    return static_cast<uint64>(1.0e9 * timebase_info.denom /
                               timebase_info.numer);
#elif defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_NACL) || \
    defined(OS_WASM)
    return 1000000uLL;
#else  // platforms (OS_WIN, __APPLE__, OS_LINUX, ...)
#error "Not supported platform"
#endif  // platforms (OS_WIN, __APPLE__, OS_LINUX, ...)
  }

  uint64 GetTicks() override {
    // TODO(team): Use functions in <chrono> once the use of it is approved.
#if defined(OS_WIN)
    LARGE_INTEGER timestamp;
    // TODO(yukawa): Consider the case where QueryPerformanceCounter is not
    // available.
    const BOOL result = ::QueryPerformanceCounter(&timestamp);
    return static_cast<uint64>(timestamp.QuadPart);
#elif defined(__APPLE__)
    return static_cast<uint64>(mach_absolute_time());
#elif defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_NACL) || \
    defined(OS_WASM)
    uint64 sec;
    uint32 usec;
    GetTimeOfDay(&sec, &usec);
    return sec * 1000000 + usec;
#else  // platforms (OS_WIN, __APPLE__, OS_LINUX, ...)
#error "Not supported platform"
#endif  // platforms (OS_WIN, __APPLE__, OS_LINUX, ...)
  }

  const absl::TimeZone& GetTimeZone() override {
    return timezone_;
  }

  void SetTimeZoneOffset(int32 timezone_offset_sec) override {
    timezone_offset_sec_ = timezone_offset_sec;
    timezone_ = absl::FixedTimeZone(timezone_offset_sec);
  }

 private:
  int32 timezone_offset_sec_;
  absl::TimeZone timezone_;
};
}  // namespace

using ClockSingleton = SingletonMockable<ClockInterface, ClockImpl>;

void Clock::GetTimeOfDay(uint64 *sec, uint32 *usec) {
  ClockSingleton::Get()->GetTimeOfDay(sec, usec);
}

uint64 Clock::GetTime() { return ClockSingleton::Get()->GetTime(); }

absl::Time Clock::GetAbslTime() { return ClockSingleton::Get()->GetAbslTime(); }

uint64 Clock::GetFrequency() { return ClockSingleton::Get()->GetFrequency(); }

uint64 Clock::GetTicks() { return ClockSingleton::Get()->GetTicks(); }

const absl::TimeZone& Clock::GetTimeZone() {
  return ClockSingleton::Get()->GetTimeZone();
}

void Clock::SetTimeZoneOffset(int32 timezone_offset_sec) {
  return ClockSingleton::Get()->SetTimeZoneOffset(timezone_offset_sec);
}

void Clock::SetClockForUnitTest(ClockInterface *clock) {
  ClockSingleton::SetMock(clock);
}


}  // namespace mozc
