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

package org.mozc.android.inputmethod.japanese;

import org.mozc.android.inputmethod.japanese.LayoutParamsAnimator.InterpolationListener;

import android.content.Context;
import android.os.Handler;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.animation.LinearInterpolator;

/**
 */
public class LayoutParamsAnimatorTest extends InstrumentationTestCase {
  @SmallTest
  public void testStartAnimation() throws InterruptedException {
    Context context = getInstrumentation().getTargetContext();
    LayoutParamsAnimator animator =
        new LayoutParamsAnimator(new Handler(context.getMainLooper()));
    class AssertionStateMachine {
      static final int PREPARING = -1;
      static final int INITIAL = 0;
      static final int CALLED_BY_HANDLER = 1;
      static final int LAYOUTPARAMS_WAS_UPDATED = 2;
      static final int FINISHED = 3;
      volatile int state = PREPARING;
      float interpolation;
      volatile int cycles = 0;
      void setLayoutParams(LayoutParams params) {
        if (state == PREPARING) {
          return;
        }
        assertTrue(state == CALLED_BY_HANDLER);
        state = LAYOUTPARAMS_WAS_UPDATED;
        assertEquals((int) (interpolation * 100), params.width);
      }
      void requestLayout() {
        if (state == PREPARING) {
          return;
        }
        assertEquals(LAYOUTPARAMS_WAS_UPDATED, state);
        state = (interpolation == 1.0f ? FINISHED : INITIAL);
        ++cycles;
      }
      void calculateAnimatedParams(float interpolation, LayoutParams currentLayoutParams) {
        if (state == PREPARING) {
          return;
        }
        assertEquals(INITIAL, state);
        state = CALLED_BY_HANDLER;
        this.interpolation = interpolation;
      }
    }
    final AssertionStateMachine stateMachine = new AssertionStateMachine();
    View mockView = new View(context) {
      @Override
      public void setLayoutParams(LayoutParams params) {
        stateMachine.setLayoutParams(params);
        super.setLayoutParams(params);
      }
      @Override
      public void requestLayout() {
        stateMachine.requestLayout();
      }
    };
    mockView.setLayoutParams(new LayoutParams(0, 0));
    stateMachine.state = AssertionStateMachine.INITIAL;
    animator.startAnimation(mockView, new InterpolationListener() {
      @Override
      public LayoutParams calculateAnimatedParams(float interpolation,
          LayoutParams currentLayoutParams) {
        stateMachine.calculateAnimatedParams(interpolation, currentLayoutParams);
        currentLayoutParams.width = (int) (interpolation * 100);
        return currentLayoutParams;
      }
    }, new LinearInterpolator(), 500, 0);
    while (stateMachine.state != AssertionStateMachine.FINISHED) {
      // TODO(hidehiko): Get rid of Thread.sleep.
      Thread.sleep(100);
    }
    assertTrue(stateMachine.cycles != 0);
  }
}
