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

#include <atomic>
#include <ctime>

#include "absl/time/clock.h"
#include "absl/time/time.h"

#if defined(OS_CHROMEOS) || defined(_WIN32)
constexpr bool kUseAbslLocalTimeZone = false;

#else  // defined(OS_CHROMEOS) || defined(_WIN32)
constexpr bool kUseAbslLocalTimeZone = true;

#endif  // defined(OS_CHROMEOS) || defined(_WIN32)

namespace mozc {
namespace {

constinit static std::atomic<ClockInterface*> g_mock = nullptr;

absl::TimeZone GetLocalTimeZone() {
  if constexpr (kUseAbslLocalTimeZone) {
    return absl::LocalTimeZone();
  } else {
    // Do not use absl::LocalTimeZone() here because
    // - on Chrome OS, it returns UTC: b/196271425
    // - on Windows, it crashes: https://github.com/google/mozc/issues/856
    const time_t epoch(24 * 60 * 60);  // 1970-01-02 00:00:00 UTC
    const std::tm* offset = std::localtime(&epoch);
    if (offset == nullptr) {
      return absl::FixedTimeZone(9 * 60 * 60);  // JST as fallback
    }
    return absl::FixedTimeZone(
        (offset->tm_mday - 2) * 24 * 60 * 60  // date offset from Jan 2.
        + offset->tm_hour * 60 * 60           // hour offset from 00 am.
        + offset->tm_min * 60);               // minute offset.
  }
}

}  // namespace

absl::Time Clock::GetAbslTime() {
  if (ClockInterface* mock = g_mock.load(std::memory_order_acquire);
      mock != nullptr) {
    return mock->GetAbslTime();
  }
  return absl::Now();
}

absl::TimeZone Clock::GetTimeZone() {
  if (ClockInterface* mock = g_mock.load(std::memory_order_acquire);
      mock != nullptr) {
    return mock->GetTimeZone();
  }
  return GetLocalTimeZone();
}

void Clock::SetClockForUnitTest(ClockInterface* clock) {
  g_mock.store(clock, std::memory_order_release);
}

}  // namespace mozc
