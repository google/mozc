// Copyright 2010-2018, Google Inc.
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

import android.graphics.BlurMaskFilter;
import android.graphics.BlurMaskFilter.Blur;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.Paint.Style;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;

/**
 * Three-dots overlay icon for keys which accept long-press. e.g., space.
 */
public class ThreeDotsIconDrawable extends Drawable {

  private final float bottomOffset;
  private final float rightOffset;

  private final float width;
  private final float span;

  private static final int DOT_NUMBER = 3;
  private static final float BLUR_RADIUS = 0.5f;

  private final RectF[] dotRects = new RectF[DOT_NUMBER];

  private final Paint basePaint = new Paint(Paint.ANTI_ALIAS_FLAG);

  public ThreeDotsIconDrawable(float bottomOffset, float rightOffset,
                               int color, float width, float span) {
    this.bottomOffset = bottomOffset;
    this.rightOffset = rightOffset;
    this.width = width;
    this.span = span;

    basePaint.setColor(color);
    basePaint.setStyle(Style.FILL);
    basePaint.setMaskFilter(new BlurMaskFilter(BLUR_RADIUS, Blur.NORMAL));

    for (int i = 0; i < DOT_NUMBER; ++i) {
      dotRects[i] = new RectF();
    }
  }

  @Override
  public void draw(Canvas canvas) {
    for (RectF rect : dotRects) {
      canvas.drawRect(rect, basePaint);
    }
  }

  @Override
  protected void onBoundsChange(Rect bound) {
    super.onBoundsChange(bound);

    for (int i = 0; i < dotRects.length; ++i) {
      float right = bound.right - (width + span) * i - rightOffset;
      float bottom = bound.bottom - bottomOffset;
      dotRects[i].set(right - width, bottom - width, right, bottom);
    }
  }

  @Override
  public int getOpacity() {
    return PixelFormat.TRANSLUCENT;
  }

  @Override
  public void setAlpha(int alpha) {
  }

  @Override
  public void setColorFilter(ColorFilter cf) {
  }
}
