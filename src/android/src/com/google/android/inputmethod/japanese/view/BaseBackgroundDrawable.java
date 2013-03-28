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

package org.mozc.android.inputmethod.japanese.view;

import android.graphics.ColorFilter;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;

/**
 * The basic implementation which is shared by drawables for background.
 *
 * Currently this class supports padding related stuff.
 */
abstract class BaseBackgroundDrawable extends Drawable {
  private final int leftPadding;
  private final int topPadding;
  private final int rightPadding;
  private final int bottomPadding;

  private Rect canvasRect = new Rect();

  BaseBackgroundDrawable(int leftPadding, int topPadding, int rightPadding, int bottomPadding) {
    this.leftPadding = leftPadding;
    this.topPadding = topPadding;
    this.rightPadding = rightPadding;
    this.bottomPadding = bottomPadding;
  }

  @Override
  protected void onBoundsChange(Rect bounds) {
    super.onBoundsChange(bounds);
    canvasRect.set(bounds.left + leftPadding,
                   bounds.top + topPadding,
                   bounds.right - rightPadding,
                   bounds.bottom - bottomPadding);
  }

  protected boolean isCanvasRectEmpty() {
    return canvasRect.isEmpty();
  }

  /**
   * @return the region without padding.
   */
  protected Rect getCanvasRect() {
    return new Rect(canvasRect);
  }

  /** Supporting alpha channel. */
  @Override
  public int getOpacity() {
    return PixelFormat.TRANSLUCENT;
  }

  @Override
  public void setAlpha(int alpha) {
    // Do nothing.
  }

  @Override
  public void setColorFilter(ColorFilter cf) {
    // Do nothing.
  }
}
