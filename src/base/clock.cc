// Copyright 2010-2018, Google Inc.
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
#ifdef OS_MACOSX
#include <mach/mach_time.h>
#elif defined(OS_NACL)  // OS_MACOSX
#include <irt.h>
#endif  // OS_MACOSX or OS_NACL
#include <sys/time.h>
#endif  // OS_WIN

#include "base/singleton.h"

namespace mozc {
namespace {

class ClockImpl : public ClockInterface {
 public:
#ifndef OS_NACL
  ClockImpl() {}
#else  // OS_NACL
  ClockImpl() : timezone_offset_sec_(0) {}
#endif  // OS_NACL

  virtual ~ClockImpl() {}

  virtual void GetTimeOfDay(uint64 *sec, uint32 *usec) {
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
#else  // OS_WIN
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    *sec = tv.tv_sec;
    *usec = tv.tv_usec;
#endif  // OS_WIN
  }

  virtual uint64 GetTime() {
#ifdef OS_WIN
    return static_cast<uint64>(_time64(nullptr));
#else
    return static_cast<uint64>(time(nullptr));
#endif  // OS_WIN
  }

  virtual bool GetTmWithOffsetSecond(time_t offset_sec, tm *output) {
    const time_t current_sec = static_cast<time_t>(this->GetTime());
    const time_t modified_sec = current_sec + offset_sec;

#ifdef OS_WIN
    if (_localtime64_s(output, &modified_sec) != 0) {
      return false;
    }
#elif defined(OS_NACL)
    const time_t localtime_sec = modified_sec + timezone_offset_sec_;
    if (gmtime_r(&localtime_sec, output) == nullptr) {
      return false;
    }
#else  // !OS_WIN && !OS_NACL
    if (localtime_r(&modified_sec, output) == nullptr) {
      return false;
    }
#endif  // OS_WIN
    return true;
  }

  virtual uint64 GetFrequency() {
#if defined(OS_WIN)
    LARGE_INTEGER timestamp;
    // TODO(yukawa): Consider the case where QueryPerformanceCounter is not
    // available.
    const BOOL result = ::QueryPerformanceFrequency(&timestamp);
    return static_cast<uint64>(timestamp.QuadPart);
#elif defined(OS_MACOSX)
    static mach_timebase_info_data_t timebase_info;
    mach_timebase_info(&timebase_info);
    return static_cast<uint64>(
        1.0e9 * timebase_info.denom / timebase_info.numer);
#elif defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_NACL)
    return 1000000uLL;
#else  // platforms (OS_WIN, OS_MACOSX, OS_LINUX, ...)
#error "Not supported platform"
#endif  // platforms (OS_WIN, OS_MACOSX, OS_LINUX, ...)
  }

  virtual uint64 GetTicks() {
    // TODO(team): Use functions in <chrono> once the use of it is approved.
#if defined(OS_WIN)
    LARGE_INTEGER timestamp;
    // TODO(yukawa): Consider the case where QueryPerformanceCounter is not
    // available.
    const BOOL result = ::QueryPerformanceCounter(&timestamp);
    return static_cast<uint64>(timestamp.QuadPart);
#elif defined(OS_MACOSX)
    return static_cast<uint64>(mach_absolute_time());
#elif defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_NACL)
    uint64 sec;
    uint32 usec;
    GetTimeOfDay(&sec, &usec);
    return sec * 1000000 + usec;
#else  // platforms (OS_WIN, OS_MACOSX, OS_LINUX, ...)
#error "Not supported platform"
#endif  // platforms (OS_WIN, OS_MACOSX, OS_LINUX, ...)
  }

#ifdef OS_NACL
  virtual void SetTimezoneOffset(int32 timezone_offset_sec) {
    timezone_offset_sec_ = timezone_offset_sec;
  }

 private:
  int32 timezone_offset_sec_;
#endif  // OS_NACL
};

ClockInterface *g_clock = nullptr;

inline ClockInterface *GetClock() {
  return g_clock != nullptr ? g_clock : Singleton<ClockImpl>::get();
}

}  // namespace

void Clock::GetTimeOfDay(uint64 *sec, uint32 *usec) {
  GetClock()->GetTimeOfDay(sec, usec);
}

uint64 Clock::GetTime() {
  return GetClock()->GetTime();
}

bool Clock::GetTmWithOffsetSecond(tm *time_with_offset, int offset_sec) {
  return GetClock()->GetTmWithOffsetSecond(offset_sec, time_with_offset);
}

uint64 Clock::GetFrequency() {
  return GetClock()->GetFrequency();
}

uint64 Clock::GetTicks() {
  return GetClock()->GetTicks();
}

#ifdef OS_NACL
void Clock::SetTimezoneOffset(int32 timezone_offset_sec) {
  return GetClock()->SetTimezoneOffset(timezone_offset_sec);
}
#endif  // OS_NACL

void Clock::SetClockForUnitTest(ClockInterface *clock) {
  g_clock = clock;
}

}  // namespace mozc
