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

#include "testing/gunit.h"
#include "absl/strings/string_view.h"
#include "absl/time/civil_time.h"
#include "absl/time/time.h"

namespace mozc {
namespace {

TEST(ParseTimeOrDieTest, ParsesTimeAtUtc) {
  EXPECT_EQ(ParseTimeOrDie("2020-12-23T13:24:35Z"),
            absl::FromCivil(absl::CivilSecond(2020, 12, 23, 13, 24, 35),
                            absl::UTCTimeZone()));
}

TEST(ClockMockTest, GetAbslTimeReturnsInitiallyGivenTime) {
  ClockMock mock(ParseTimeOrDie("2020-12-23T13:24:35Z"));
  EXPECT_EQ(mock.GetAbslTime(), ParseTimeOrDie("2020-12-23T13:24:35Z"));
}

TEST(ClockMockTest, AdvanceAdvancesByGivenDuration) {
  ClockMock mock(ParseTimeOrDie("2020-12-23T13:24:35Z"));

  mock.Advance(absl::Seconds(1));
  EXPECT_EQ(mock.GetAbslTime(), ParseTimeOrDie("2020-12-23T13:24:36Z"));

  mock.Advance(absl::Hours(12));
  EXPECT_EQ(mock.GetAbslTime(), ParseTimeOrDie("2020-12-24T01:24:36Z"));
}

TEST(ClockMockTest, AutoAdvanceAdvancesOnEveryGetAbslTimeCall) {
  ClockMock mock(ParseTimeOrDie("2020-12-23T13:24:35Z"));
  mock.AutoAdvance(absl::Seconds(5));
  EXPECT_EQ(mock.GetAbslTime(), ParseTimeOrDie("2020-12-23T13:24:35Z"));
  EXPECT_EQ(mock.GetAbslTime(), ParseTimeOrDie("2020-12-23T13:24:40Z"));
  EXPECT_EQ(mock.GetAbslTime(), ParseTimeOrDie("2020-12-23T13:24:45Z"));
}

TEST(ScopedClockMockTest, AbslTime) {
  {
    ScopedClockMock mock(ParseTimeOrDie("2020-12-23T13:24:35Z"));
    EXPECT_EQ(Clock::GetAbslTime(), ParseTimeOrDie("2020-12-23T13:24:35Z"));
  }
  EXPECT_NE(Clock::GetAbslTime(), ParseTimeOrDie("2020-12-23T13:24:35Z"));
}

}  // namespace
}  // namespace mozc
