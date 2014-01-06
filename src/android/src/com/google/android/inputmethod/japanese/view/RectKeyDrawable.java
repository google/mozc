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

import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.Shader;
import android.graphics.Shader.TileMode;

/**
 * Drawable to render a rectangle key's background.
 *
 */
public class RectKeyDrawable extends BaseBackgroundDrawable {
  private final int topColor;
  private final int bottomColor;

  private final int highlightColor;
  private final int lightShadeColor;
  private final int darkShadeColor;
  private final int shadowColor;

  private int left;
  private int top;
  private int right;
  private int bottom;

  /** Cached Paint instance to reuse for performance reason. */
  private Paint paint = new Paint();
  private Shader shader = null;

  public RectKeyDrawable(
      int leftPadding, int topPadding, int rightPadding, int bottomPadding,
      int topColor, int bottomColor,
      int highlightColor, int lightShadeColor, int darkShadeColor,
      int shadowColor) {
    super(leftPadding, topPadding, rightPadding, bottomPadding);
    this.topColor = topColor;
    this.bottomColor = bottomColor;

    this.highlightColor = highlightColor;
    this.lightShadeColor = lightShadeColor;
    this.darkShadeColor = darkShadeColor;
    this.shadowColor = shadowColor;
  }

  @Override
  public void draw(Canvas canvas) {
    if (isCanvasRectEmpty()) {
      return;
    }

    // Local variable cache for better performance.
    Paint paint = this.paint;
    int left = this.left;
    int top = this.top;
    int right = this.right;
    int bottom = this.bottom;

    // Render shadow first.
    if ((shadowColor & 0xFF000000) != 0) {
      Rect bound = getBounds();
      paint.reset();
      paint.setAntiAlias(true);
      paint.setColor(shadowColor);
      canvas.drawRect(left + 1, top + 1,
                      Math.min(right + 1, bound.right),
                      Math.min(bottom + 1, bound.bottom),
                      paint);
    }

    // Render base region.
    if (shader != null) {
      paint.reset();
      paint.setAntiAlias(true);
      paint.setShader(shader);
      canvas.drawRect(left, top, right, bottom, paint);
    }

    // Render highlight and shade.
    if (((highlightColor | lightShadeColor | darkShadeColor) & 0xFF000000) != 0) {
      paint.reset();
      paint.setAntiAlias(true);

      paint.setColor(highlightColor);
      canvas.drawRect(left, top, right, top + 1, paint);
      paint.setColor(lightShadeColor);
      canvas.drawRect(left, top, left + 1, bottom, paint);
      canvas.drawRect(right - 1, top, right, bottom, paint);
      paint.setColor(darkShadeColor);
      canvas.drawRect(left, bottom - 1, right, bottom, paint);
    }
  }

  @Override
  protected void onBoundsChange(Rect rect) {
    super.onBoundsChange(rect);

    Rect canvasRect = getCanvasRect();
    left = canvasRect.left;
    top = canvasRect.top;
    right = canvasRect.right;
    bottom = canvasRect.bottom;
    shader = new LinearGradient(0, top, 0, bottom, topColor, bottomColor, TileMode.CLAMP);
  }
}
