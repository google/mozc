// Copyright 2010-2012, Google Inc.
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
}

TEST(ClockMockTest, ClockMockTest) {
  // GetTime,
  {
    ClockMock mock(kTestSeconds, kTestMicroSeconds);
    uint64 current_time = mock.GetTime();
    EXPECT_EQ(kTestSeconds, current_time);
  }

  // GetTimeOfDay
  {
    ClockMock mock(kTestSeconds, kTestMicroSeconds);
    uint64 current_sec;
    uint32 current_usec;
    mock.GetTimeOfDay(&current_sec, &current_usec);
    EXPECT_EQ(kTestSeconds, current_sec);
    EXPECT_EQ(kTestMicroSeconds, current_usec);
  }

  // GetCurrentTmWithOffset
  {  // 2020-12-23 13:24:00 (Wed)
    ClockMock mock(kTestSeconds, kTestMicroSeconds);
    const int offset = -35;
    tm current_tm;

    EXPECT_TRUE(mock.GetTmWithOffsetSecond(offset, &current_tm));
    EXPECT_EQ(120, current_tm.tm_year);
    EXPECT_EQ(11, current_tm.tm_mon);
    EXPECT_EQ(23, current_tm.tm_mday);
    EXPECT_EQ(13, current_tm.tm_hour);
    EXPECT_EQ(24, current_tm.tm_min);
    EXPECT_EQ(00, current_tm.tm_sec);
    EXPECT_EQ(3, current_tm.tm_wday);
  }

  // PutClockForward
  // 2024/02/22 23:11:15
  {
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
      mock.PutClockForward(0uLL, offset_micro_seconds);

      mock.GetTimeOfDay(&current_sec, &current_usec);
      EXPECT_EQ(kTestSeconds, current_sec);
      EXPECT_EQ(kTestMicroSeconds + offset_micro_seconds, current_usec);
    }

    // add micro seconds
    // 123456 [usec] + 900000 [usec] => 1 [sec] + 023456 [usec]
    {
      ClockMock mock(kTestSeconds, kTestMicroSeconds);
      const uint64 offset_micro_seconds = 900000uLL;
      mock.PutClockForward(0uLL, offset_micro_seconds);

      mock.GetTimeOfDay(&current_sec, &current_usec);
      EXPECT_EQ(kTestSeconds + 1, current_sec);
      const uint32 expected_usec =
          kTestMicroSeconds + offset_micro_seconds - 1000000;
      EXPECT_EQ(expected_usec, current_usec);
    }
  }
}

}  // namespace mozc
