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

#include "win32/base/indicator_visibility_tracker.h"

#include <memory>

#include "base/clock.h"
#include "base/clock_mock.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace win32 {
namespace {

using std::unique_ptr;

const uint64 kWaitDuration = 500;  // msec
const VirtualKey AKey = VirtualKey::FromVirtualKey('A');

class IndicatorVisibilityTrackerTest : public testing::Test {
 protected:
  virtual void SetUp() {
    clock_mock_.reset(new ClockMock(0, 0));
    // 1 kHz (Accuracy = 1msec)
    clock_mock_->SetFrequency(1000uLL);
    Clock::SetClockForUnitTest(clock_mock_.get());
  }

  virtual void TearDown() { Clock::SetClockForUnitTest(nullptr); }

  void PutForwardMilliseconds(uint64 milli_sec) {
    clock_mock_->PutClockForwardByTicks(milli_sec);
  }

 private:
  unique_ptr<ClockMock> clock_mock_;
};

TEST_F(IndicatorVisibilityTrackerTest, BasicTest) {
  IndicatorVisibilityTracker tracker;

  EXPECT_FALSE(tracker.IsVisible()) << "Should be hidden by default.";

  EXPECT_EQ(IndicatorVisibilityTracker::kUpdateUI, tracker.OnChangeInputMode());
  EXPECT_TRUE(tracker.IsVisible());  // ChangeInputMode -> Visible
  EXPECT_EQ(IndicatorVisibilityTracker::kNothing, tracker.OnChangeInputMode());
  EXPECT_TRUE(tracker.IsVisible());  // ChangeInputMode -> Visible
  EXPECT_EQ(IndicatorVisibilityTracker::kUpdateUI,
            tracker.OnDissociateContext());
  EXPECT_FALSE(tracker.IsVisible());  // KillFocus -> Invisible
  EXPECT_EQ(IndicatorVisibilityTracker::kUpdateUI, tracker.OnChangeInputMode());
  EXPECT_TRUE(tracker.IsVisible());  // ChangeInputMode -> Visible

  // 0 msec later -> WindowMove -> Visible
  EXPECT_EQ(IndicatorVisibilityTracker::kNothing,
            tracker.OnMoveFocusedWindow());
  EXPECT_TRUE(tracker.IsVisible());

  // |kWaitDuration/2| msec later -> WindowMove -> Visible
  PutForwardMilliseconds(kWaitDuration / 2);
  EXPECT_EQ(IndicatorVisibilityTracker::kNothing,
            tracker.OnMoveFocusedWindow());
  EXPECT_TRUE(tracker.IsVisible());

  // |kWaitDuration*2| msec later -> WindowMove -> Invisible
  PutForwardMilliseconds(kWaitDuration * 2);
  EXPECT_EQ(IndicatorVisibilityTracker::kUpdateUI,
            tracker.OnMoveFocusedWindow());
  EXPECT_FALSE(tracker.IsVisible());

  EXPECT_EQ(IndicatorVisibilityTracker::kUpdateUI, tracker.OnChangeInputMode());
  EXPECT_TRUE(tracker.IsVisible());  // ChangeInputMode -> Visible
  EXPECT_EQ(IndicatorVisibilityTracker::kUpdateUI,
            tracker.OnTestKey(AKey, true, false));
  EXPECT_FALSE(tracker.IsVisible());  // TestKeyDown -> Invisible
  EXPECT_EQ(IndicatorVisibilityTracker::kUpdateUI, tracker.OnChangeInputMode());
  EXPECT_TRUE(tracker.IsVisible());  // ChangeInputMode -> Visible
  EXPECT_EQ(IndicatorVisibilityTracker::kUpdateUI,
            tracker.OnKey(AKey, true, false));
  EXPECT_FALSE(tracker.IsVisible());  // KeyDown -> Invisible
  EXPECT_EQ(IndicatorVisibilityTracker::kUpdateUI, tracker.OnChangeInputMode());
  EXPECT_TRUE(tracker.IsVisible());  // ChangeInputMode -> Visible
  EXPECT_EQ(IndicatorVisibilityTracker::kNothing,
            tracker.OnTestKey(AKey, false, false));
  EXPECT_TRUE(tracker.IsVisible());  // TestKeyUp -> Visible
  EXPECT_EQ(IndicatorVisibilityTracker::kNothing,
            tracker.OnKey(AKey, false, false));
  EXPECT_TRUE(tracker.IsVisible());  // KeyUp -> Visible
}

}  // namespace

}  // namespace win32
}  // namespace mozc
