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

#include <numeric>

#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "base/stopwatch.h"
#include "base/util.h"

namespace {
  // Allow 500 msec margin.
  const double kMarginMicroseconds = 500.0e+3;

  double GetElapsedMicroseconds(uint64 sec_begin, uint32 usec_begin,
                                uint64 sec_end, uint32 usec_end) {
    double usec_diff = usec_end;
    usec_diff -= usec_begin;

    double sec_diff = usec_end;
    if (sec_end > sec_begin) {
      sec_diff = static_cast<double>(sec_end - sec_begin);
    } else {
      sec_diff = - static_cast<double>(sec_begin - sec_end);
    }

    return sec_diff * 1.0e+6 + usec_diff;
  }
}  // anonymous namespace

namespace mozc {
TEST(StopWatchTest, MultipleGetElapsedMillisecondsTest) {
  const int kSleepMilliseconds = 200;

  Stopwatch stopwatch = Stopwatch::StartNew();
  Util::Sleep(kSleepMilliseconds);
  stopwatch.Stop();

  // GetElapsedX should return the same value if the stopwatch is not running.
  EXPECT_FALSE(stopwatch.IsRunning());
  const int64 elapsed_time1 = stopwatch.GetElapsedMilliseconds();
  Util::Sleep(kSleepMilliseconds);
  const int64 elapsed_time2 = stopwatch.GetElapsedMilliseconds();
  Util::Sleep(kSleepMilliseconds);
  const int64 elapsed_time3 = stopwatch.GetElapsedMilliseconds();
  EXPECT_EQ(elapsed_time1, elapsed_time2);
  EXPECT_EQ(elapsed_time1, elapsed_time3);
}

TEST(StopWatchTest, GetElapsedMicrosecondsTest) {
  const int kSleepMilliseconds = 1000;

  uint64 sec_begin, sec_end;
  uint32 usec_begin, usec_end;
  Util::GetTimeOfDay(&sec_begin, &usec_begin);
  Stopwatch stopwatch = Stopwatch::StartNew();
  Util::Sleep(kSleepMilliseconds);
  stopwatch.Stop();
  Util::GetTimeOfDay(&sec_end, &usec_end);

  const double expected_elapsed_usec =
      GetElapsedMicroseconds(sec_begin, usec_begin, sec_end, usec_end);
  EXPECT_GE(stopwatch.GetElapsedMicroseconds(),
            expected_elapsed_usec - kMarginMicroseconds);
  EXPECT_LE(stopwatch.GetElapsedMicroseconds(),
            expected_elapsed_usec + kMarginMicroseconds);
}

TEST(StopWatchTest, RestartTest) {
  const int kSleepMilliseconds1 = 1000;
  const int kSleepMilliseconds2 = 500;

  uint64 sec_begin, sec_end;
  uint32 usec_begin, usec_end;
  Util::GetTimeOfDay(&sec_begin, &usec_begin);
  Stopwatch stopwatch = Stopwatch::StartNew();
  Util::Sleep(kSleepMilliseconds1);
  stopwatch.Stop();
  stopwatch.Start();
  Util::Sleep(kSleepMilliseconds2);
  stopwatch.Stop();
  Util::GetTimeOfDay(&sec_end, &usec_end);

  const double expected_elapsed_usec =
      GetElapsedMicroseconds(sec_begin, usec_begin, sec_end, usec_end);
  EXPECT_GE(stopwatch.GetElapsedMicroseconds(),
            expected_elapsed_usec - kMarginMicroseconds);
  EXPECT_LE(stopwatch.GetElapsedMicroseconds(),
            expected_elapsed_usec + kMarginMicroseconds);
}

TEST(StopWatchTest, ResetTest) {
  const int kSleepMilliseconds1 = 1000;
  const int kSleepMilliseconds2 = 500;
  Stopwatch stopwatch = Stopwatch::StartNew();
  Util::Sleep(kSleepMilliseconds1);
  stopwatch.Stop();
  stopwatch.Reset();

  uint64 sec_begin, sec_end;
  uint32 usec_begin, usec_end;
  Util::GetTimeOfDay(&sec_begin, &usec_begin);
  stopwatch.Start();
  Util::Sleep(kSleepMilliseconds2);
  stopwatch.Stop();
  Util::GetTimeOfDay(&sec_end, &usec_end);

  const double expected_elapsed_usec =
      GetElapsedMicroseconds(sec_begin, usec_begin, sec_end, usec_end);
  EXPECT_GE(stopwatch.GetElapsedMicroseconds(),
            expected_elapsed_usec - kMarginMicroseconds);
  EXPECT_LE(stopwatch.GetElapsedMicroseconds(),
            expected_elapsed_usec + kMarginMicroseconds);
}
}  // namespace mozc
