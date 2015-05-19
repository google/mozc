// Copyright 2010-2014, Google Inc.
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

package org.mozc.android.inputmethod.japanese;

import org.mozc.android.inputmethod.japanese.InOutAnimatedFrameLayout.VisibilityChangeListener;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.Parameter;

import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.view.animation.AlphaAnimation;
import android.view.animation.Animation;

/**
 */
public class InOutAnimatedFrameLayoutTest extends InstrumentationTestCaseWithMock {
  private static class TestData extends Parameter {
    final Animation animation;
    final int visibility;
    final boolean expectAnimation;
    final int expectedVisibility;

    TestData(Animation animation, int visibility, boolean expectAnimation, int expectedVisibility) {
      this.animation = animation;
      this.visibility = visibility;
      this.expectAnimation = expectAnimation;
      this.expectedVisibility = expectedVisibility;
    }
  }

  private InOutAnimatedFrameLayout view;

  @Override
  protected void setUp() throws Exception {
    super.setUp();
    view = new InOutAnimatedFrameLayout(getInstrumentation().getTargetContext()) {};
  }

  @Override
  protected void tearDown() throws Exception {
    view.clearAnimation();
    view = null;
    super.tearDown();
  }

  @SmallTest
  public void testInAnimation() {
    TestData[] testDataList = {
        // If view is hidden, in animation should be started.
        new TestData(new AlphaAnimation(0, 0), View.GONE, true, View.VISIBLE),
        new TestData(new AlphaAnimation(0, 0), View.INVISIBLE, true, View.VISIBLE),

        // If view has been already shown, in animation should not be started.
        new TestData(new AlphaAnimation(0, 0), View.VISIBLE, false, View.VISIBLE),

        // If animation is not loaded, the visibility should be just updated.
        new TestData(null, View.VISIBLE, false, View.VISIBLE),
        new TestData(null, View.INVISIBLE, false, View.VISIBLE),
        new TestData(null, View.GONE, false, View.VISIBLE),
    };

    for (TestData testData : testDataList) {
      view.setInAnimation(testData.animation);
      view.clearAnimation();
      view.setVisibility(testData.visibility);

      Animation inAnimation = view.inAnimation;
      if (testData.animation == null) {
        assertNull(testData.toString(), inAnimation);
      } else {
        assertNotNull(testData.toString(), inAnimation);
      }

      view.startInAnimation();

      if (testData.expectAnimation) {
        assertSame(testData.toString(), inAnimation, view.getAnimation());
      } else {
        assertNull(testData.toString(), view.getAnimation());
      }
      assertEquals(testData.toString(), testData.expectedVisibility, view.getVisibility());
    }
  }

  @SmallTest
  public void testOutAnimation() {
    TestData[] testDataList = {
        // If view is shown, out animation should be started.
        // For animation, we need to keep visibility VISIBLE.
        new TestData(new AlphaAnimation(0, 0), View.VISIBLE, true, View.VISIBLE),

        // If view is hidden, out animation should not be started.
        new TestData(new AlphaAnimation(0, 0), View.INVISIBLE, false, View.INVISIBLE),
        new TestData(new AlphaAnimation(0, 0), View.GONE, false, View.GONE),

        // If animation is not loaded, the visibility should be just updated.
        new TestData(null, View.VISIBLE, false, View.GONE),
        new TestData(null, View.INVISIBLE, false, View.INVISIBLE),
        new TestData(null, View.GONE, false, View.GONE),
    };

    for (TestData testData : testDataList) {
      view.setOutAnimation(testData.animation);
      view.clearAnimation();
      view.setVisibility(testData.visibility);

      Animation outAnimation = view.outAnimation;
      if (testData.animation == null) {
        assertNull(testData.toString(), outAnimation);
      } else {
        assertNotNull(testData.toString(), outAnimation);
      }

      view.startOutAnimation();

      if (testData.expectAnimation) {
        assertSame(testData.toString(), outAnimation, view.getAnimation());
      } else {
        assertNull(testData.toString(), view.getAnimation());
      }

      assertEquals(testData.toString(), testData.expectedVisibility, view.getVisibility());
    }
  }

  @SmallTest
  public void testOnVisibilityChangeListener() {
    VisibilityChangeListener onVisibilityChangeListener =
        createMock(VisibilityChangeListener.class);
    view.setVisibility(View.GONE);
    view.setOnVisibilityChangeListener(onVisibilityChangeListener);

    int[] nextVisibilities = {View.VISIBLE, View.INVISIBLE, View.GONE};
    for (int nextVisibility : nextVisibilities) {
      int currentVisibility = view.getVisibility();
      resetAll();
      onVisibilityChangeListener.onVisibilityChange(currentVisibility, nextVisibility);
      replayAll();

      view.setVisibility(nextVisibility);
      verifyAll();
    }
  }
}
