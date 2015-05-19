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

package org.mozc.android.inputmethod.japanese.view;

import android.graphics.BlurMaskFilter;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.BlurMaskFilter.Blur;
import android.graphics.Paint.Style;
import android.graphics.Shader.TileMode;

/**
 * Drawable to render a rounded-corner rectangle key's background.
 *
 */
public class RoundRectKeyDrawable extends BaseBackgroundDrawable {
  private final int BLUR_SIZE = 3;

  private final int roundSize;

  private final int topColor;
  private final int bottomColor;

  private final Paint basePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
  private final Paint shadowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
  private final Paint highlightPaint;

  private final RectF baseBound = new RectF();
  private final RectF shadowBound = new RectF();

  public RoundRectKeyDrawable(
      int leftPadding, int topPadding, int rightPadding, int bottomPadding,
      int roundSize, int topColor, int bottomColor, int highlightColor, int shadowColor) {
    super(leftPadding, topPadding, rightPadding, bottomPadding);
    this.roundSize = roundSize;
    this.topColor = topColor;
    this.bottomColor = bottomColor;

    shadowPaint.setColor(shadowColor);
    shadowPaint.setStyle(Style.FILL);
    shadowPaint.setMaskFilter(new BlurMaskFilter(BLUR_SIZE, Blur.NORMAL));
    if ((highlightColor & 0xFF000000) != 0) {
      highlightPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
      highlightPaint.setColor(highlightColor);
    } else {
      highlightPaint = null;
    }
  }

  @Override
  public void draw(Canvas canvas) {
    if (isCanvasRectEmpty()) {
      return;
    }

    // Each qwerty key looks round corner'ed rectangle.
    canvas.drawRoundRect(shadowBound, roundSize, roundSize, shadowPaint);
    canvas.drawRoundRect(baseBound, roundSize, roundSize, basePaint);

    // Draw 1-px height highlight at the top of key if necessary.
    if (highlightPaint != null) {
      canvas.drawRect(baseBound.left + roundSize, baseBound.top,
                      baseBound.right - roundSize, baseBound.top + 1,
                      highlightPaint);
    }
  }

  @Override
  protected void onBoundsChange(Rect rect) {
    super.onBoundsChange(rect);

    Rect canvasRect = getCanvasRect();
    baseBound.set(canvasRect.left, canvasRect.top, canvasRect.right, canvasRect.bottom);
    shadowBound.set(Math.max(canvasRect.left, rect.left + BLUR_SIZE),
                    Math.max(canvasRect.top + 1, rect.top + BLUR_SIZE),
                    Math.min(canvasRect.right + 1, rect.right - BLUR_SIZE),
                    Math.min(canvasRect.bottom + 2, rect.bottom - BLUR_SIZE));
    basePaint.setShader(new LinearGradient(
        0, canvasRect.top, 0, canvasRect.bottom, topColor, bottomColor, TileMode.CLAMP));
  }
}
