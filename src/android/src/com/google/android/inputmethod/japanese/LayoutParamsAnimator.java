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

import android.os.Handler;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.animation.Interpolator;

/**
 * This class manages layout animation.
 *
 * <code>Animation</code> and related classes of Android's framework can perform
 * animation by transforming <code>Canvas</code> on which <code>View</code>s are
 * drawn.
 * In other words, <code>Animation</code> does not modify layout information.
 *
 * This class can animate layout information.
 * Note that layout animation is heavier than canvas animation so this class should
 * be used only when updating layout information is mandatory.
 *
 */
public class LayoutParamsAnimator {
  /**
   * Callback object for layout animation.
   */
  interface InterpolationListener {
    /**
     * Called back by <code>Handler</code> repeatedly to update <code>LayoutParams</code>.
     * @param interpolation the value of interpolation. c.f. Interporator
     * @param currentLayoutParams the LayoutParams instance of the registered view.
     * @return updated LayoutParams. This can be identical object to
     *     <code>currentLayoutParams</code>.
     */
    LayoutParams calculateAnimatedParams(float interpolation, LayoutParams currentLayoutParams);
  }

  private Handler handler;

  /**
   * Exported as protected for testing.
   */
  protected LayoutParamsAnimator(Handler handler) {
    if (handler == null) {
      throw new NullPointerException("handler must be non-null.");
    }
    this.handler = handler;
  }

  /**
   * Starts layout animation.
   *
   * This method periodically updates <code>view</code>'s LayoutParams.
   * @param view the view to be animated.
   * @param callback the object to be called back.
   * @param interpolator the interpolator to be used.
   * @param duration the duration for which this method calls <code>callback</code>.
   * @param interval the interval (millisecond) of the animation.
   */
  public void startAnimation(final View view, final InterpolationListener callback,
      final Interpolator interpolator, final long duration, final long interval) {
    if (view.getLayoutParams() == null) {
      throw new NullPointerException("view.getLayoutParams() must return non-null.");
    }
    final long startTime = System.currentTimeMillis();
    handler.post(new Runnable() {
      @Override
      public void run() {
        float input = Math.min(1.0f, (System.currentTimeMillis() - startTime) / (float) duration);
        LayoutParams newLayoutParams =
            callback.calculateAnimatedParams(interpolator.getInterpolation(input),
                view.getLayoutParams());
        // Setting new LayoutParams will invoke requestLayout().
        view.setLayoutParams(newLayoutParams);
        if (input < 1.0f) {
          handler.postDelayed(this, interval);
        }
      }
    });
  }
}
