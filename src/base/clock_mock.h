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

#ifndef MOZC_BASE_CLOCK_MOCK_H_
#define MOZC_BASE_CLOCK_MOCK_H_

#include <cstdint>

#include "base/clock.h"
#include "absl/base/attributes.h"
#include "absl/time/time.h"

namespace mozc {

// Standard mock clock implementation.
// This mock behaves in UTC
class ClockMock : public ClockInterface {
 public:
  explicit ClockMock(absl::Time time) : time_(time) {}
  ABSL_DEPRECATED("Use the constructor with absl::Time")
  ClockMock(uint64_t sec, uint32_t usec);

  ClockMock(const ClockMock &) = delete;
  ClockMock &operator=(const ClockMock &) = delete;

  ~ClockMock() override = default;

  ABSL_DEPRECATED("Use GetAbslTime()")
  void GetTimeOfDay(uint64_t *sec, uint32_t *usec) override;
  ABSL_DEPRECATED("Use GetAbslTime()")
  uint64_t GetTime() override;
  absl::Time GetAbslTime() override;

  absl::TimeZone GetTimeZone() override;

  // Advances the clock.
  ABSL_DEPRECATED("Use Advance()")
  void PutClockForward(uint64_t delta_sec, uint32_t delta_usec);
  void Advance(absl::Duration delta) { time_ += delta; }

  // Automatically advances the clock every time it returns time.
  ABSL_DEPRECATED("Use AutoAdvance()")
  void SetAutoPutClockForward(uint64_t delta_sec, uint32_t delta_usec);
  void AutoAdvance(absl::Duration delta) { auto_delta_ = delta; }

  ABSL_DEPRECATED("Use the overload with absl::Time")
  void SetTime(uint64_t sec, uint32_t usec);
  void SetTime(absl::Time time) { time_ = time; }

 private:
  absl::Time time_ = absl::InfinitePast();
  absl::TimeZone timezone_ = absl::UTCTimeZone();
  // Every time user requests the time, auto_delta_ is added to time_.
  absl::Duration auto_delta_ = absl::ZeroDuration();
};

// Changes the global clock with a mock during the life time of this object.
class ScopedClockMock {
 public:
  explicit ScopedClockMock(absl::Time time) : mock_(time) {}
  ABSL_DEPRECATED("Use the constructor with absl::Time")
  ScopedClockMock(uint64_t sec, uint32_t usec) : mock_(sec, usec) {
    Clock::SetClockForUnitTest(&mock_);
  }

  ScopedClockMock(const ScopedClockMock &) = delete;
  ScopedClockMock &operator=(const ScopedClockMock &) = delete;

  ~ScopedClockMock() { Clock::SetClockForUnitTest(nullptr); }

  constexpr ClockMock *operator->() { return &mock_; }
  constexpr const ClockMock *operator->() const { return &mock_; }

 private:
  ClockMock mock_;
};

}  // namespace mozc

#endif  // MOZC_BASE_CLOCK_MOCK_H_
