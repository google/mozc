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

#include "base/date_util.h"

#include <optional>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/time/civil_time.h"
#include "absl/time/time.h"
#include "base/clock_mock.h"
#include "testing/gunit.h"

namespace mozc::date_util {
namespace {

TEST(DateUtilTest, ParseTrailingDate) {
  ScopedClockMock scoped_clock(absl::FromCivil(
      absl::CivilSecond(2026, 9, 16, 12, 0, 0), absl::UTCTimeZone()));

  EXPECT_EQ(ParseTrailingDate("9/16"), absl::CivilDay(2026, 9, 16));
  EXPECT_EQ(ParseTrailingDate("2026/09/16"), absl::CivilDay(2026, 9, 16));
  EXPECT_EQ(ParseTrailingDate("9月16日"), absl::CivilDay(2026, 9, 16));
  EXPECT_EQ(ParseTrailingDate("2026年9月16日"), absl::CivilDay(2026, 9, 16));
  EXPECT_EQ(ParseTrailingDate("９／１６"), absl::CivilDay(2026, 9, 16));
  EXPECT_EQ(ParseTrailingDate("予定は9/16"), absl::CivilDay(2026, 9, 16));

  // Long context (> 64 bytes) with multi-byte characters.
  const std::string long_prefix(30, 'a');
  EXPECT_EQ(ParseTrailingDate(absl::StrCat(
                long_prefix, "あいうえおあいうえおあいうえお9月16日")),
            absl::CivilDay(2026, 9, 16));

  // Invalid dates and non-date expressions.
  EXPECT_EQ(ParseTrailingDate(""), std::nullopt);
  EXPECT_EQ(ParseTrailingDate("2/30"), std::nullopt);
  EXPECT_EQ(ParseTrailingDate("2026/02/29"), std::nullopt);
  EXPECT_EQ(ParseTrailingDate("2024/02/29"), absl::CivilDay(2024, 2, 29));
  EXPECT_EQ(ParseTrailingDate("令和8年9月16日"), std::nullopt);
  EXPECT_EQ(ParseTrailingDate("1/2/3"), std::nullopt);
}

TEST(DateUtilTest, GetWeekdayFromTrailingDateAndStrings) {
  ScopedClockMock scoped_clock(absl::FromCivil(
      absl::CivilSecond(2026, 9, 16, 12, 0, 0), absl::UTCTimeZone()));

  const std::optional<absl::Weekday> wd = GetWeekdayFromTrailingDate("9/16");
  ASSERT_TRUE(wd.has_value());
  EXPECT_EQ(*wd, absl::Weekday::wednesday);
  EXPECT_EQ(GetShortWeekdayString(*wd), "水");
  EXPECT_EQ(GetShortWeekdayReading(*wd), "すい");

  EXPECT_EQ(GetShortWeekdayString(absl::Weekday::monday), "月");
  EXPECT_EQ(GetShortWeekdayReading(absl::Weekday::monday), "げつ");
  EXPECT_EQ(GetShortWeekdayString(absl::Weekday::sunday), "日");
  EXPECT_EQ(GetShortWeekdayReading(absl::Weekday::sunday), "にち");
}

}  // namespace
}  // namespace mozc::date_util
