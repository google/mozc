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

#include <ctime>

#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/clock_mock.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

TEST(ClockTest, TimeTestWithMock) {
  ClockMock clock_mock(ParseTimeOrDie("2020-12-23T13:24:35Z"));
  Clock::SetClockForUnitTest(&clock_mock);

  EXPECT_EQ(Clock::GetAbslTime(), ParseTimeOrDie("2020-12-23T13:24:35Z"));

  // Restore the default clock
  Clock::SetClockForUnitTest(nullptr);

  // Can be flaky.
  EXPECT_LE(absl::AbsDuration(Clock::GetAbslTime() - absl::Now()),
            absl::Seconds(1));
}

TEST(ClockTest, TimeTestWithoutMock) {
  // Can be flaky.
  EXPECT_LE(absl::AbsDuration(Clock::GetAbslTime() - absl::Now()),
            absl::Seconds(1));
}

TEST(ClockTest, TimeZone) {
  const absl::TimeZone tz = Clock::GetTimeZone();
  const absl::TimeZone::CivilInfo ci = tz.At(absl::UnixEpoch());
  const int absl_offset = ci.offset;

  const time_t epoch(24 * 60 * 60);  // 1970-01-02 00:00:00 UTC
  const std::tm* offset = std::localtime(&epoch);
  const int tm_offset =
      (offset->tm_mday - 2) * 24 * 60 * 60  // date offset from Jan 2.
      + offset->tm_hour * 60 * 60           // hour offset from 00 am.
      + offset->tm_min * 60;                // minute offset.

  EXPECT_EQ(tm_offset, absl_offset);
}

}  // namespace
}  // namespace mozc
