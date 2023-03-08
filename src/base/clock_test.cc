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

#include <cstdint>
#include <ctime>

#include "base/clock_mock.h"
#include "testing/gunit.h"
#include "absl/time/time.h"

#ifdef _WIN32
#include <windows.h>
#else  // _WIN32
#include <sys/time.h>
#endif  // !_WIN32

namespace mozc {
namespace {

// 2020-12-23 13:24:35 (Wed) UTC
// 123456 [usec]
constexpr uint64_t kTestSeconds = 1608729875uLL;
constexpr uint32_t kTestMicroSeconds = 123456u;

// This is the previous implementation of GetTimeOfDay to check the
// compatibility.
// TODO(yuryu): Delete this function and related tests when we remove
// GetTimeOfDay().
void LegacyGetTimeOfDay(uint64_t *sec, uint32_t *usec) {
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
#endif  // !_WIN32
}

TEST(ClockTest, TimeTestWithMock) {
  ClockMock clock_mock(kTestSeconds, kTestMicroSeconds);
  Clock::SetClockForUnitTest(&clock_mock);

  // GetTime
  { EXPECT_EQ(Clock::GetTime(), kTestSeconds); }

  // GetTimeOfDay
  {
    uint64_t current_sec;
    uint32_t current_usec;
    Clock::GetTimeOfDay(&current_sec, &current_usec);
    EXPECT_EQ(current_sec, kTestSeconds);
    EXPECT_EQ(current_usec, kTestMicroSeconds);
  }

  // GetAbslTime
  // 2020-12-23 13:24:35 (Wed)
  {
    const absl::Time at = Clock::GetAbslTime();
    const absl::TimeZone &tz = Clock::GetTimeZone();
    const absl::CivilSecond cs = absl::ToCivilSecond(at, tz);

    EXPECT_EQ(cs.year(), 2020);
    EXPECT_EQ(cs.month(), 12);
    EXPECT_EQ(cs.day(), 23);
    EXPECT_EQ(cs.hour(), 13);
    EXPECT_EQ(cs.minute(), 24);
    EXPECT_EQ(cs.second(), 35);
    EXPECT_EQ(absl::GetWeekday(cs), absl::Weekday::wednesday);
  }

  // GetAbslTime + offset
  // 2024/02/23 23:11:15 (Fri)
  {
    const int offset_seconds = 100000000;
    const absl::Time at = Clock::GetAbslTime();
    const absl::TimeZone &tz = Clock::GetTimeZone();
    const absl::CivilSecond cs = absl::ToCivilSecond(at, tz) + offset_seconds;
    EXPECT_EQ(cs.year(), 2024);
    EXPECT_EQ(cs.month(), 2);
    EXPECT_EQ(cs.day(), 23);
    EXPECT_EQ(cs.hour(), 23);
    EXPECT_EQ(cs.minute(), 11);
    EXPECT_EQ(cs.second(), 15);
    EXPECT_EQ(absl::GetWeekday(cs), absl::Weekday::friday);
  }

  // Restore the default clock
  Clock::SetClockForUnitTest(nullptr);
}

TEST(ClockTest, TimeTestWithoutMock) {
  uint64_t get_time_of_day_sec, get_time_sec;
  uint32_t get_time_of_day_usec;

  Clock::GetTimeOfDay(&get_time_of_day_sec, &get_time_of_day_usec);
  get_time_sec = Clock::GetTime();

  // hmm, unstable test.
  constexpr int margin = 1;
  EXPECT_NEAR(get_time_of_day_sec, get_time_sec, margin)
      << ": This test have possibilities to fail "
      << "when system is busy and slow.";
}

TEST(ClockTest, CompatibilityTest) {
  uint64_t sec, legacy_sec;
  uint32_t usec, legacy_usec;
  Clock::GetTimeOfDay(&sec, &usec);
  LegacyGetTimeOfDay(&legacy_sec, &legacy_usec);
  const absl::Time current =
      absl::FromUnixSeconds(sec) + absl::Microseconds(usec);
  const absl::Time legacy =
      absl::FromUnixSeconds(legacy_sec) + absl::Microseconds(legacy_usec);
#ifdef _WIN32
  // On Windows, the resolution of GetSystemTimeAsFileTime is 55 ms.
  constexpr absl::Duration margin = absl::Milliseconds(55 * 2);
#else   // _WIN32
  // On other platforms, gettimeofday usually has ~10 us resolution.
  constexpr absl::Duration margin = absl::Microseconds(50);
#endif  // !_WIN32
  EXPECT_LE(absl::AbsDuration(current - legacy), margin);
}

TEST(ClockTest, TimeZone) {
  const absl::TimeZone tz = Clock::GetTimeZone();
  const absl::TimeZone::CivilInfo ci = tz.At(absl::UnixEpoch());
  const int absl_offset = ci.offset;

  const time_t epoch(24 * 60 * 60);  // 1970-01-02 00:00:00 UTC
  const std::tm *offset = std::localtime(&epoch);
  const int tm_offset =
      (offset->tm_mday - 2) * 24 * 60 * 60  // date offset from Jan 2.
      + offset->tm_hour * 60 * 60           // hour offset from 00 am.
      + offset->tm_min * 60;                // minute offset.

  EXPECT_EQ(tm_offset, absl_offset);
}

}  // namespace
}  // namespace mozc
