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

package org.mozc.android.inputmethod.japanese.ui;

import static org.easymock.EasyMock.expect;

import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import org.mozc.android.inputmethod.japanese.testing.VisibilityProxy;

import android.test.suitebuilder.annotation.SmallTest;

import org.easymock.IAnswer;

/**
 */
public class SnapScrollerTest extends InstrumentationTestCaseWithMock {
  @SmallTest
  public void testScrollTo() {
    class TestData extends Parameter {
      final int toPosition;
      final int expected;

      TestData(int toPosition, int expected) {
        this.toPosition = toPosition;
        this.expected = expected;
      }
    }
    TestData[] testDataList = {
        new TestData(-1, 0),
        new TestData(0, 0),
        new TestData(1, 1),
        new TestData(90, 90),
        new TestData(91, 90),
        new TestData(80, 80),
    };
    SnapScroller scroller = new SnapScroller();
    scroller.setContentSize(100);
    scroller.setViewSize(10);
    for (TestData testData : testDataList) {
      scroller.scrollTo(testData.toPosition);
      assertEquals(testData.toString(), testData.expected, scroller.getScrollPosition());
      assertFalse(testData.toString(), scroller.isScrolling());
    }

    // Widen view size.
    scroller.setViewSize(50);
    testDataList = new TestData[]{
        new TestData(81, 80),
        new TestData(70, 70),
        new TestData(71, 70),
        new TestData(1, 1),
        new TestData(50, 50),
        new TestData(51, 50),
    };
    for (TestData testData : testDataList) {
      scroller.scrollTo(testData.toPosition);
      assertEquals(testData.toString(), testData.expected, scroller.getScrollPosition());
      assertFalse(testData.toString(), scroller.isScrolling());
    }
  }

  @SmallTest
  public void testFling() {
    class TestData extends Parameter {
      final int velocity;
      final int mimimumVelocity;
      final int scrollPosition;
      final int expectedEndScrollPosition;
      final boolean expectedIsScrolling;

      TestData(int velocity, int mimimumVelocity, int scrollPosition,
          int expectedEndScrollPosition,
          boolean expectedIsScrolling) {
        this.velocity = velocity;
        this.mimimumVelocity = mimimumVelocity;
        this.scrollPosition = scrollPosition;
        this.expectedEndScrollPosition = expectedEndScrollPosition;
        this.expectedIsScrolling = expectedIsScrolling;
      }
    }
    int pageSize = 10;
    int viewSize = 20;
    int contentSize = 110;
    final long timestamp = 12345;
    TestData[] testDataList = {
        // Velocity is 0 so scroll animation does not start.
        new TestData(0, 0, 0, 0, false),
        // Usual situation to start scroll.
        new TestData(-1, 1, 5, 0, true),
        new TestData(+1, 1, 5, 10, true),
        // Velocity is lower than mininum velocity so animation does not start.
        new TestData(-1, 2, 5, 0, false),
        new TestData(+1, 2, 5, 0, false),
        // Animation starts on the page edge position.
        new TestData(-1, 1, 10, 0, true),
        new TestData(+1, 1, 10, 20, true),
        // Minimum and maximum boundary check.
        new TestData(-1, 1, 0, 0, false),
        new TestData(+1, 1, 100, 90, false),
    };
    SnapScroller scroller = new SnapScroller(new SnapScroller.TimestampCalculator() {
      @Override
      public long getTimestamp() {
        return timestamp;
      }
    });
    scroller.setContentSize(contentSize);
    scroller.setPageSize(pageSize);
    scroller.setViewSize(viewSize);
    for (TestData testData : testDataList) {
      scroller.scrollTo(testData.scrollPosition);
      scroller.setMinimumVelocity(testData.mimimumVelocity);
      scroller.fling(testData.velocity);
      assertEquals(testData.toString(), testData.expectedIsScrolling, scroller.isScrolling());
      if (scroller.isScrolling()) {
        assertEquals(testData.toString(),
                     testData.scrollPosition,
                     scroller.getStartScrollPosition());
        assertEquals(testData.toString(),
                     testData.expectedEndScrollPosition,
                     scroller.getEndScrollPosition());
        assertEquals(testData.toString(), timestamp, scroller.getStartScrollTime());
      }
    }
  }

  @SmallTest
  public void testComputeScrollOffsetForFling() throws Exception {
    class TestData extends Parameter {
      final int velocity;
      final float decayRate;
      final int expectedScrollPosition;

      TestData(int velocity, float decayRate, int expectedScrollPosition) {
        this.velocity = velocity;
        this.decayRate = decayRate;
        this.expectedScrollPosition = expectedScrollPosition;
      }
    }
    int scrollPosition = 50;
    TestData[] testDataList = {
        new TestData(0, 0f, 50),
        // Decay rate is 0 so scroll animation happens only once.
        new TestData(+1000, 0f, 60),
        // Decay rate is 1 so scroll animation happens repeatedly until
        // reaching the maximum boundary.
        new TestData(+1000, 1f, 90),
        // Check boundary value.
        new TestData(Integer.MAX_VALUE, 1f, 90),
    };
    SnapScroller.TimestampCalculator timestampCalculator =
        createMock(SnapScroller.TimestampCalculator.class);
    SnapScroller scroller = new SnapScroller(timestampCalculator);
    scroller.setContentSize(110);
    scroller.setPageSize(10);
    scroller.setViewSize(20);
    scroller.setMinimumVelocity(0);

    for (TestData testData : testDataList) {
      resetAll();
      expect(timestampCalculator.getTimestamp()).andStubAnswer(new IAnswer<Long>() {
        long counter = 0;

        @Override
        public Long answer() throws Throwable {
          return counter += 100;
        }
      });
      replayAll();
      scroller.setDecayRate(testData.decayRate);
      scroller.scrollTo(scrollPosition);
      scroller.fling(testData.velocity);
      while (scroller.isScrolling()) {
        scroller.computeScrollOffset();
      }
      assertEquals(testData.toString(),
                   testData.expectedScrollPosition,
                   VisibilityProxy.getField(scroller, "scrollPosition"));
    }
  }
}
