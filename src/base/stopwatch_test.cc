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

#include "base/stopwatch.h"

#include <cstdint>
#include <memory>
#include <numeric>

#include "base/clock.h"
#include "base/clock_mock.h"
#include "testing/gunit.h"
#include "absl/time/time.h"

namespace mozc {

class StopwatchTest : public testing::Test {
 protected:
  void SetUp() override {
    clock_mock_ = std::make_unique<ClockMock>(0, 0);
    // 1GHz (Accuracy = 1ns)
    clock_mock_->SetFrequency(uint64_t{1000000000});
    Clock::SetClockForUnitTest(clock_mock_.get());
  }

  void TearDown() override { Clock::SetClockForUnitTest(nullptr); }

  void PutForward(absl::Duration duration) {
    clock_mock_->PutClockForward(
        absl::ToInt64Seconds(duration),
        absl::ToInt64Microseconds(duration % absl::Seconds(1)));
  }

  std::unique_ptr<ClockMock> clock_mock_;
};

TEST_F(StopwatchTest, MultipleGetElapsedMillisecondsTest) {
  constexpr absl::Duration kWait = absl::Milliseconds(20021001);

  Stopwatch stopwatch = Stopwatch::StartNew();
  PutForward(kWait);
  stopwatch.Stop();

  // GetElapsedX should return the same value if the stopwatch is not running.
  EXPECT_FALSE(stopwatch.IsRunning());
  const uint64_t elapsed_time1 = stopwatch.GetElapsedMilliseconds();
  PutForward(kWait);
  const uint64_t elapsed_time2 = stopwatch.GetElapsedMilliseconds();
  PutForward(kWait);
  const uint64_t elapsed_time3 = stopwatch.GetElapsedMilliseconds();
  EXPECT_EQ(elapsed_time2, elapsed_time1);
  EXPECT_EQ(elapsed_time3, elapsed_time1);
}

TEST_F(StopwatchTest, GetElapsedXSecondsTest) {
  constexpr absl::Duration kWait = absl::Microseconds(12122323);

  Stopwatch stopwatch = Stopwatch::StartNew();
  PutForward(kWait);
  stopwatch.Stop();

  EXPECT_EQ(stopwatch.GetElapsedNanoseconds(),
            absl::ToDoubleNanoseconds(kWait));
  EXPECT_EQ(stopwatch.GetElapsedMicroseconds(),
            absl::ToDoubleMicroseconds(kWait));
  EXPECT_EQ(stopwatch.GetElapsedMilliseconds(),
            absl::ToInt64Milliseconds(kWait));
}

TEST_F(StopwatchTest, RestartTest) {
  constexpr absl::Duration kWait1 = absl::Seconds(1);
  constexpr absl::Duration kWait2 = absl::Microseconds(42);
  constexpr absl::Duration kWait3 = absl::Hours(100);

  Stopwatch stopwatch = Stopwatch::StartNew();
  PutForward(kWait1);
  stopwatch.Stop();
  PutForward(kWait2);
  stopwatch.Start();
  PutForward(kWait3);
  stopwatch.Stop();

  const absl::Duration kExpected = kWait1 + kWait3;
  EXPECT_EQ(stopwatch.GetElapsedNanoseconds(),
            absl::ToDoubleNanoseconds(kExpected));
  EXPECT_EQ(stopwatch.GetElapsedMicroseconds(),
            absl::ToDoubleMicroseconds(kExpected));
  EXPECT_EQ(stopwatch.GetElapsedMilliseconds(),
            absl::ToInt64Milliseconds(kExpected));
}

TEST_F(StopwatchTest, ResetTest) {
  constexpr absl::Duration kWait1 = absl::Milliseconds(12345);
  constexpr absl::Duration kWait2 = absl::Microseconds(54321);
  Stopwatch stopwatch = Stopwatch::StartNew();
  PutForward(kWait1);
  stopwatch.Stop();
  stopwatch.Reset();

  stopwatch.Start();
  PutForward(kWait2);
  stopwatch.Stop();

  EXPECT_EQ(stopwatch.GetElapsedNanoseconds(),
            absl::ToDoubleNanoseconds(kWait2));
  EXPECT_EQ(stopwatch.GetElapsedMicroseconds(),
            absl::ToDoubleMicroseconds(kWait2));
  EXPECT_EQ(stopwatch.GetElapsedMilliseconds(),
            absl::ToInt64Milliseconds(kWait2));
}

}  // namespace mozc
