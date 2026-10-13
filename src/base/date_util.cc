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

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/string_view.h"
#include "absl/time/civil_time.h"
#include "absl/time/time.h"
#include "base/clock.h"
#include "base/japanese_util.h"

namespace mozc::date_util {
namespace {

absl::string_view ConsumeTrailingDigits(absl::string_view& s) {
  const size_t end = s.size();
  size_t pos = end;
  while (pos > 0 && absl::ascii_isdigit(s[pos - 1])) {
    --pos;
  }
  const absl::string_view digits = s.substr(pos, end - pos);
  s.remove_suffix(end - pos);
  return digits;
}

}  // namespace

std::optional<absl::CivilDay> ParseTrailingDate(absl::string_view text) {
  if (text.empty()) {
    return std::nullopt;
  }

  // Bound the suffix length to avoid unnecessary allocation on long context
  // strings. Since `text.size() - kMaxSuffixBytes` is a byte offset that may
  // land in the middle of a multi-byte UTF-8 character, advance `start` past
  // any UTF-8 continuation bytes (10xxxxxx) to split on a character boundary.
  constexpr size_t kMaxSuffixBytes = 64;
  if (text.size() > kMaxSuffixBytes) {
    size_t start = text.size() - kMaxSuffixBytes;
    while (start < text.size() &&
           (static_cast<uint8_t>(text[start]) & 0xC0) == 0x80) {
      ++start;
    }
    text.remove_prefix(start);
  }

  const std::string normalized =
      japanese_util::FullWidthAsciiToHalfWidthAscii(text);
  absl::string_view s = normalized;

  constexpr absl::string_view kDayKanji = "日";
  constexpr absl::string_view kMonthKanji = "月";
  constexpr absl::string_view kYearKanji = "年";

  std::optional<int> explicit_year;
  int month = 0;
  int day = 0;

  // Parses the "yyyy年mm月dd日" (or "mm月dd日") format.
  if (s.ends_with(kDayKanji)) {
    s.remove_suffix(kDayKanji.size());
    const absl::string_view day_str = ConsumeTrailingDigits(s);
    if (day_str.empty() || day_str.size() > 2 || !s.ends_with(kMonthKanji)) {
      return std::nullopt;
    }
    s.remove_suffix(kMonthKanji.size());
    const absl::string_view month_str = ConsumeTrailingDigits(s);
    if (month_str.empty() || month_str.size() > 2) {
      return std::nullopt;
    }
    // Japanese era (e.g., "令和8年") is not supported.
    if (s.ends_with(kYearKanji)) {
      s.remove_suffix(kYearKanji.size());
      const absl::string_view year_str = ConsumeTrailingDigits(s);
      if (year_str.size() != 4 ||
          (!s.empty() && absl::ascii_isdigit(s.back()))) {
        return std::nullopt;
      }
      int y = 0;
      if (!absl::SimpleAtoi(year_str, &y)) {
        return std::nullopt;
      }
      explicit_year = y;
    } else if (!s.empty() && absl::ascii_isdigit(s.back())) {
      return std::nullopt;
    }
    if (!absl::SimpleAtoi(month_str, &month) ||
        !absl::SimpleAtoi(day_str, &day)) {
      return std::nullopt;
    }
  } else {
    // Parses the "yyyy/mm/dd" (or "mm/dd") format.
    const absl::string_view day_str = ConsumeTrailingDigits(s);
    if (day_str.empty() || day_str.size() > 2 || !s.ends_with('/')) {
      return std::nullopt;
    }
    s.remove_suffix(1);
    const absl::string_view month_str = ConsumeTrailingDigits(s);
    if (month_str.empty() || month_str.size() > 2) {
      return std::nullopt;
    }
    if (s.ends_with('/')) {
      s.remove_suffix(1);
      const absl::string_view year_str = ConsumeTrailingDigits(s);
      if (year_str.size() != 4 ||
          (!s.empty() && (absl::ascii_isdigit(s.back()) || s.back() == '/' ||
                          s.back() == '.'))) {
        return std::nullopt;
      }
      int y = 0;
      if (!absl::SimpleAtoi(year_str, &y)) {
        return std::nullopt;
      }
      explicit_year = y;
    } else if (!s.empty() && (absl::ascii_isdigit(s.back()) ||
                              s.back() == '/' || s.back() == '.')) {
      return std::nullopt;
    }
    if (!absl::SimpleAtoi(month_str, &month) ||
        !absl::SimpleAtoi(day_str, &day)) {
      return std::nullopt;
    }
  }

  if (month < 1 || month > 12 || day < 1 || day > 31) {
    return std::nullopt;
  }

  int target_year = 0;
  if (explicit_year.has_value()) {
    if (*explicit_year < 1900 || *explicit_year > 2100) {
      return std::nullopt;
    }
    target_year = *explicit_year;
  } else {
    const absl::CivilDay today =
        absl::ToCivilDay(Clock::GetAbslTime(), Clock::GetTimeZone());
    const int current_year = today.year();
    const absl::CivilDay cand(current_year, month, day);
    const int64_t diff_days = cand - today;
    if (diff_days < -90) {
      target_year = current_year + 1;
    } else if (diff_days > 275) {
      target_year = current_year - 1;
    } else {
      target_year = current_year;
    }
  }

  const absl::CivilDay result_day(target_year, month, day);
  if (result_day.year() != target_year || result_day.month() != month ||
      result_day.day() != day) {
    return std::nullopt;
  }
  return result_day;
}

std::optional<absl::Weekday> GetWeekdayFromTrailingDate(
    absl::string_view text) {
  const std::optional<absl::CivilDay> date = ParseTrailingDate(text);
  if (!date.has_value()) {
    return std::nullopt;
  }
  return absl::GetWeekday(*date);
}

absl::string_view GetShortWeekdayString(absl::Weekday weekday) {
  // absl::Weekday starts from Monday (0) through Sunday (6).
  constexpr absl::string_view kShortWeekdays[] = {
      "月", "火", "水", "木", "金", "土", "日",
  };
  const int idx = static_cast<int>(weekday);
  if (idx < 0 || idx >= 7) {
    return "";
  }
  return kShortWeekdays[idx];
}

absl::string_view GetShortWeekdayReading(absl::Weekday weekday) {
  constexpr absl::string_view kShortWeekdayReadings[] = {
      "げつ", "か", "すい", "もく", "きん", "ど", "にち",
  };
  const int idx = static_cast<int>(weekday);
  if (idx < 0 || idx >= 7) {
    return "";
  }
  return kShortWeekdayReadings[idx];
}

}  // namespace mozc::date_util
