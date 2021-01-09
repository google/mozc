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

#include "base/util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {

namespace {
// 2020-12-23 13:24:35 (Wed) UTC
// 123456 [usec]
const uint64 kTestSeconds = 1608729875uLL;
const uint32 kTestMicroSeconds = 123456u;

const uint64 kDeltaSeconds = 12uLL;
const uint32 kDeltaMicroSeconds = 654321u;
}  // namespace

TEST(ClockMockTest, GetTimeTest) {
  ClockMock mock(kTestSeconds, kTestMicroSeconds);
  uint64 current_time = mock.GetTime();
  EXPECT_EQ(kTestSeconds, current_time);
}

TEST(ClockMockTest, GetTimeOfDayTest) {
  ClockMock mock(kTestSeconds, kTestMicroSeconds);
  uint64 current_sec;
  uint32 current_usec;
  mock.GetTimeOfDay(&current_sec, &current_usec);
  EXPECT_EQ(kTestSeconds, current_sec);
  EXPECT_EQ(kTestMicroSeconds, current_usec);
}

TEST(ClockMockTest, GetCurrentTmWithOffsetTest) {
  // 2020-12-23 13:24:00 (Wed)
  ClockMock mock(kTestSeconds, kTestMicroSeconds);
  const int offset = -35;

  const absl::Time at = mock.GetAbslTime();
  const absl::TimeZone &tz = mock.GetTimeZone();
  const absl::CivilSecond cs = absl::ToCivilSecond(at, tz) + offset;

  EXPECT_EQ(2020, cs.year());
  EXPECT_EQ(12, cs.month());
  EXPECT_EQ(23, cs.day());
  EXPECT_EQ(13, cs.hour());
  EXPECT_EQ(24, cs.minute());
  EXPECT_EQ(0, cs.second());
  EXPECT_EQ(absl::Weekday::wednesday, absl::GetWeekday(cs));
}

TEST(ClockMockTest, GetCurrentTmWithOffsetWithTimeZoneOffsetTest) {
  // 2020-12-23 13:24:00 (Wed)
  ClockMock mock(kTestSeconds, kTestMicroSeconds);
  mock.SetTimeZoneOffset(3600);
  const int offset = -35;

  const absl::Time at = mock.GetAbslTime();
  const absl::TimeZone &tz = mock.GetTimeZone();
  const absl::CivilSecond cs = absl::ToCivilSecond(at, tz) + offset;

  EXPECT_EQ(2020, cs.year());
  EXPECT_EQ(12, cs.month());
  EXPECT_EQ(23, cs.day());
  EXPECT_EQ(14, cs.hour());
  EXPECT_EQ(24, cs.minute());
  EXPECT_EQ(0, cs.second());
  EXPECT_EQ(absl::Weekday::wednesday, absl::GetWeekday(cs));
}

TEST(ClockMockTest, GetFrequencyAndTicks) {
  ClockMock mock(0, 0);
  EXPECT_NE(0, mock.GetFrequency());
  EXPECT_EQ(0, mock.GetTicks());

  const uint64 kFrequency = 123456789uLL;
  mock.SetFrequency(kFrequency);
  EXPECT_EQ(kFrequency, mock.GetFrequency());

  const uint64 kTicks = 987654321uLL;
  mock.SetTicks(kTicks);
  EXPECT_EQ(kTicks, mock.GetTicks());
}

TEST(ClockMockTest, PutClockForwardTest) {
  // 2024/02/22 23:11:15
  uint64 current_sec;
  uint32 current_usec;

  // add seconds
  {
    ClockMock mock(kTestSeconds, kTestMicroSeconds);
    const uint64 offset_seconds = 100uLL;
    mock.PutClockForward(offset_seconds, 0u);

    mock.GetTimeOfDay(&current_sec, &current_usec);
    EXPECT_EQ(kTestSeconds + offset_seconds, current_sec);
    EXPECT_EQ(kTestMicroSeconds, current_usec);
  }

  // add micro seconds
  // 123456 [usec] + 1 [usec] => 123457 [usec]
  {
    ClockMock mock(kTestSeconds, kTestMicroSeconds);
    const uint64 offset_micro_seconds = 1uLL;
    mock.PutClockForward(uint64{0}, offset_micro_seconds);

    mock.GetTimeOfDay(&current_sec, &current_usec);
    EXPECT_EQ(kTestSeconds, current_sec);
    EXPECT_EQ(kTestMicroSeconds + offset_micro_seconds, current_usec);
  }

  // add micro seconds
  // 123456 [usec] + 900000 [usec] => 1 [sec] + 023456 [usec]
  {
    ClockMock mock(kTestSeconds, kTestMicroSeconds);
    const uint64 offset_micro_seconds = 900000uLL;
    mock.PutClockForward(uint64{0}, offset_micro_seconds);

    mock.GetTimeOfDay(&current_sec, &current_usec);
    EXPECT_EQ(kTestSeconds + 1, current_sec);
    const uint32 expected_usec =
        kTestMicroSeconds + offset_micro_seconds - 1000000;
    EXPECT_EQ(expected_usec, current_usec);
  }
}

TEST(ClockMockTest, PutClockForwardByTicksTest) {
  ClockMock mock(0, 0);
  ASSERT_EQ(0, mock.GetTicks());

  const uint64 kPutForwardTicks = 100;
  mock.PutClockForwardByTicks(kPutForwardTicks);
  EXPECT_EQ(kPutForwardTicks, mock.GetTicks());
}

TEST(ClockMockTest, AutoPutForwardTest) {
  // GetTime()
  {
    ClockMock mock(kTestSeconds, kTestMicroSeconds);
    mock.SetAutoPutClockForward(kDeltaSeconds, kDeltaMicroSeconds);
    EXPECT_EQ(kTestSeconds, mock.GetTime());
    EXPECT_EQ(kTestSeconds + kDeltaSeconds, mock.GetTime());
    EXPECT_EQ(kTestSeconds + 2 * kDeltaSeconds + 1, mock.GetTime());
  }

  // GetTimeOfDay()
  {
    ClockMock mock(kTestSeconds, kTestMicroSeconds);
    mock.SetAutoPutClockForward(kDeltaSeconds, kDeltaMicroSeconds);
    uint64 current_sec;
    uint32 current_usec;
    mock.GetTimeOfDay(&current_sec, &current_usec);
    EXPECT_EQ(kTestSeconds, current_sec);
    EXPECT_EQ(kTestMicroSeconds, current_usec);
    mock.GetTimeOfDay(&current_sec, &current_usec);
    EXPECT_EQ(kTestSeconds + kDeltaSeconds, current_sec);
    EXPECT_EQ(kTestMicroSeconds + kDeltaMicroSeconds, current_usec);
  }

  // GetTmWithOffsetSecond()
  {
    ClockMock mock(kTestSeconds, kTestMicroSeconds);
    mock.SetAutoPutClockForward(kDeltaSeconds, kDeltaMicroSeconds);
    const int offset = -35;

    const absl::Time at = mock.GetAbslTime();
    const absl::TimeZone &tz = mock.GetTimeZone();
    absl::CivilSecond cs = absl::ToCivilSecond(at, tz) + offset;

    EXPECT_EQ(2020, cs.year());
    EXPECT_EQ(12, cs.month());
    EXPECT_EQ(23, cs.day());
    EXPECT_EQ(13, cs.hour());
    EXPECT_EQ(24, cs.minute());
    EXPECT_EQ(0, cs.second());
    EXPECT_EQ(absl::Weekday::wednesday, absl::GetWeekday(cs));

    const absl::Time at2 = mock.GetAbslTime();
    cs = absl::ToCivilSecond(at2, tz) + offset;
    EXPECT_EQ(2020, cs.year());
    EXPECT_EQ(12, cs.month());
    EXPECT_EQ(23, cs.day());
    EXPECT_EQ(13, cs.hour());
    EXPECT_EQ(24, cs.minute());
    EXPECT_EQ(kDeltaSeconds, cs.second());
    EXPECT_EQ(absl::Weekday::wednesday, absl::GetWeekday(cs));
  }
}

}  // namespace mozc
