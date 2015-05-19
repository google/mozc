// Copyright 2010-2013, Google Inc.
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

#include <numeric>

#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "base/clock_mock.h"
#include "base/scoped_ptr.h"
#include "base/stopwatch.h"
#include "base/util.h"

namespace mozc {

class StopwatchTest : public testing::Test {
 protected:
  void SetUp() {
    clock_mock_.reset(new ClockMock(0, 0));
    // 1GHz (Accuracy = 1ns)
    clock_mock_->SetFrequency(1000000000uLL);
    Util::SetClockHandler(clock_mock_.get());
  }

  void TearDown() {
    Util::SetClockHandler(NULL);
  }

  void PutForwardNanoseconds(uint64 nano_sec) {
    clock_mock_->PutClockForwardByTicks(nano_sec);
  }

  scoped_ptr<ClockMock> clock_mock_;
};

TEST_F(StopwatchTest, MultipleGetElapsedMillisecondsTest) {
  const uint64 kWaitNanoseconds = 1000000000uLL;  // 1 sec

  Stopwatch stopwatch = Stopwatch::StartNew();
  PutForwardNanoseconds(kWaitNanoseconds);
  stopwatch.Stop();

  // GetElapsedX should return the same value if the stopwatch is not running.
  EXPECT_FALSE(stopwatch.IsRunning());
  const uint64 elapsed_time1 = stopwatch.GetElapsedMilliseconds();
  PutForwardNanoseconds(kWaitNanoseconds);
  const uint64 elapsed_time2 = stopwatch.GetElapsedMilliseconds();
  PutForwardNanoseconds(kWaitNanoseconds);
  const uint64 elapsed_time3 = stopwatch.GetElapsedMilliseconds();
  EXPECT_EQ(elapsed_time1, elapsed_time2);
  EXPECT_EQ(elapsed_time1, elapsed_time3);
}

TEST_F(StopwatchTest, GetElapsedXSecondsTest) {
  const uint64 kWaitNanoseconds = 1000000000uLL;  // 1 sec

  Stopwatch stopwatch = Stopwatch::StartNew();
  PutForwardNanoseconds(kWaitNanoseconds);
  stopwatch.Stop();

  EXPECT_EQ(kWaitNanoseconds, stopwatch.GetElapsedNanoseconds());
  EXPECT_EQ(kWaitNanoseconds / 1000, stopwatch.GetElapsedMicroseconds());
  EXPECT_EQ(kWaitNanoseconds / 1000000, stopwatch.GetElapsedMilliseconds());
}

TEST_F(StopwatchTest, RestartTest) {
  const uint64 kWaitNanoseconds1 = 1000000000uLL;  // 1 sec
  const uint64 kWaitNanoseconds2 = 2000000000uLL;  // 2 sec
  const uint64 kWaitNanoseconds3 = 4000000000uLL;  // 4 sec

  Stopwatch stopwatch = Stopwatch::StartNew();
  PutForwardNanoseconds(kWaitNanoseconds1);
  stopwatch.Stop();
  PutForwardNanoseconds(kWaitNanoseconds2);
  stopwatch.Start();
  PutForwardNanoseconds(kWaitNanoseconds3);
  stopwatch.Stop();

  const uint64 kExpected = kWaitNanoseconds1 + kWaitNanoseconds3;
  EXPECT_EQ(kExpected, stopwatch.GetElapsedNanoseconds());
  EXPECT_EQ(kExpected / 1000, stopwatch.GetElapsedMicroseconds());
  EXPECT_EQ(kExpected / 1000000, stopwatch.GetElapsedMilliseconds());
}

TEST_F(StopwatchTest, ResetTest) {
  const uint64 kWaitNanoseconds1 = 1000000000uLL;  // 1 sec
  const uint64 kWaitNanoseconds2 = 2000000000uLL;  // 2 sec
  Stopwatch stopwatch = Stopwatch::StartNew();
  PutForwardNanoseconds(kWaitNanoseconds1);
  stopwatch.Stop();
  stopwatch.Reset();

  stopwatch.Start();
  PutForwardNanoseconds(kWaitNanoseconds2);
  stopwatch.Stop();

  EXPECT_EQ(kWaitNanoseconds2, stopwatch.GetElapsedNanoseconds());
  EXPECT_EQ(kWaitNanoseconds2 / 1000, stopwatch.GetElapsedMicroseconds());
  EXPECT_EQ(kWaitNanoseconds2 / 1000000, stopwatch.GetElapsedMilliseconds());
}

}  // namespace mozc
