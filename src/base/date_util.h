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

#ifndef MOZC_BASE_DATE_UTIL_H_
#define MOZC_BASE_DATE_UTIL_H_

#include <optional>

#include "absl/strings/string_view.h"
#include "absl/time/civil_time.h"

namespace mozc::date_util {

// Parses a trailing Japanese or slash-separated date expression from `text`
// (e.g., "9/16", "2026/09/16", "9月16日", "2026年9月16日") and resolves
// omitted years using a [-90 days, +275 days] sliding window relative to
// Clock::GetAbslTime().
std::optional<absl::CivilDay> ParseTrailingDate(absl::string_view text);

// Parses a trailing date expression from `text` using ParseTrailingDate() and
// returns its day of the week.
std::optional<absl::Weekday> GetWeekdayFromTrailingDate(absl::string_view text);

// Returns the short Japanese weekday string ("月", "火", "水", "木", "金",
// "土", "日") for `weekday`.
absl::string_view GetShortWeekdayString(absl::Weekday weekday);

// Returns the Japanese reading ("げつ", "か", "すい", "もく", "きん", "ど",
// "にち") for the short weekday string of `weekday`.
absl::string_view GetShortWeekdayReading(absl::Weekday weekday);

}  // namespace mozc::date_util

#endif  // MOZC_BASE_DATE_UTIL_H_
