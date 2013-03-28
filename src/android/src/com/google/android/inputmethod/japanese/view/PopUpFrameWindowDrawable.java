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

import android.graphics.BlurMaskFilter;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.BlurMaskFilter.Blur;
import android.graphics.Paint.Style;
import android.graphics.Shader;
import android.graphics.Shader.TileMode;

/**
 * Drawable to render a popup window with a frame.
 *
 */
public class PopUpFrameWindowDrawable extends BaseBackgroundDrawable {
  private final int frameWidth;

  private final int topColor;
  private final int bottomColor;
  private final int borderColor;
  private final int innerPaneColor;
  private final float shadowSize;

  private final Rect outerFrameRect = new Rect();
  private final Rect innerFrameRect = new Rect();
  private final Paint paint = new Paint();
  private final BlurMaskFilter blurMaskFilter;
  private Shader shader;

  public PopUpFrameWindowDrawable(
      int leftPadding, int topPadding, int rightPadding, int bottomPadding,
      int frameWidth, float shadowSize,
      int topColor, int bottomColor, int borderColor, int innerPaneColor) {
    super(leftPadding, topPadding, rightPadding, bottomPadding);

    this.frameWidth = frameWidth;
    this.topColor = topColor;
    this.bottomColor = bottomColor;
    this.borderColor = borderColor;
    this.innerPaneColor = innerPaneColor;
    this.shadowSize = shadowSize;
    this.blurMaskFilter = new BlurMaskFilter(shadowSize, Blur.INNER);
  }

  @Override
  public void draw(Canvas canvas) {
    if (isCanvasRectEmpty()) {
      return;
    }

    Paint paint = this.paint;

    // First of all, render the shadow.
    paint.reset();
    paint.setAntiAlias(true);
    paint.setColor(0x80000000);
    paint.setShadowLayer(shadowSize * 0.6f, shadowSize, shadowSize, 0xFF404040);
    canvas.drawRect(outerFrameRect, paint);

    // Draw the main part of the frame.
    if (shader != null) {
      paint.reset();
      paint.setAntiAlias(true);
      paint.setShader(shader);
      canvas.drawRect(outerFrameRect, paint);
    }

    // Draw the inner pane.
    paint.reset();
    paint.setAntiAlias(true);
    paint.setColor(0xFF000000);
    canvas.drawRect(innerFrameRect, paint);

    paint.setColor(innerPaneColor);
    paint.setMaskFilter(blurMaskFilter);
    canvas.drawRect(innerFrameRect, paint);

    // Draw the border.
    paint.reset();
    paint.setAntiAlias(true);
    paint.setColor(borderColor);
    paint.setStyle(Style.STROKE);

    canvas.drawRect(outerFrameRect, paint);
    canvas.drawRect(innerFrameRect, paint);
  }

  @Override
  protected void onBoundsChange(Rect bounds) {
    super.onBoundsChange(bounds);

    Rect canvasRect = getCanvasRect();

    outerFrameRect.set(canvasRect);
    innerFrameRect.set(
        canvasRect.left + frameWidth, canvasRect.top + frameWidth,
        canvasRect.right - frameWidth, canvasRect.bottom - frameWidth);
    shader = new LinearGradient(
        0, canvasRect.top, 0, canvasRect.bottom, topColor, bottomColor, TileMode.CLAMP);
  }
}
