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
import android.graphics.ComposeShader;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Rect;
import android.graphics.Shader;
import android.graphics.PorterDuff.Mode;
import android.util.FloatMath;

/**
 * Drawable to render a triangular highlight, whose vertices are the center and two adjacent
 * vertices of the key background.
 *
 */
public class TriangularHighlightDrawable extends BaseBackgroundDrawable {
  public enum HighlightDirection {
    LEFT, UP, RIGHT, DOWN,
  }

  // According to the original design mock, the ratio of the inner shadow width is about 0.046
  // to height.
  private static final float SHADE_LENGTH_RATIO = 0.046f;

  private int baseColor;
  private int shadeColor;

  private final HighlightDirection direction;
  private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
  private final Path path = new Path();

  public TriangularHighlightDrawable(
      int leftPadding, int topPadding, int rightPadding, int bottomPadding,
      int baseColor, int shadeColor, HighlightDirection direction) {
    super(leftPadding, topPadding, rightPadding, bottomPadding);
    this.baseColor = baseColor;
    this.shadeColor = shadeColor;
    this.direction = direction;
  }

  @Override
  public void draw(Canvas canvas) {
    canvas.drawPath(path, paint);
  }

  @Override
  protected void onBoundsChange(Rect bounds) {
    super.onBoundsChange(bounds);
    Rect canvasRect = getCanvasRect();

    switch (direction) {
      case LEFT:
        reset(canvasRect.exactCenterX(), canvasRect.exactCenterY(),
              canvasRect.left, canvasRect.top, canvasRect.left, canvasRect.bottom,
              canvasRect.height() * SHADE_LENGTH_RATIO);
        break;
      case UP:
        reset(canvasRect.exactCenterX(), canvasRect.exactCenterY(),
              canvasRect.right, canvasRect.top, canvasRect.left, canvasRect.top,
              canvasRect.height() * SHADE_LENGTH_RATIO);
        break;
      case RIGHT:
        reset(canvasRect.exactCenterX(), canvasRect.exactCenterY(),
              canvasRect.right, canvasRect.bottom, canvasRect.right, canvasRect.top,
              canvasRect.height() * SHADE_LENGTH_RATIO);
        break;
      case DOWN:
        reset(canvasRect.exactCenterX(), canvasRect.exactCenterY(),
              canvasRect.left, canvasRect.bottom, canvasRect.right, canvasRect.bottom,
              canvasRect.height() * SHADE_LENGTH_RATIO);
        break;
      default:
        throw new AssertionError();
    }
  }

  /**
   * Reset the internal paint and path based on given coordinates and shade's length.
   * Note that it is necessary that the order of (centerX, centerY)-(x1, y1)-(x2, y2)
   * is counter-clockwise.
   */
  private void reset(
      float centerX, float centerY, int x1, int y1, int x2, int y2, float shadeLength) {
    resetPaint(centerX, centerY, x1, y1, x2, y2, shadeLength);
    resetPath(centerX, centerY, x1, y1, x2, y2);
  }

  private void resetPaint(
      float centerX, float centerY, int x1, int y1, int x2, int y2, float shadeLength) {
    float dx1 = x1 - centerX;
    float dy1 = y1 - centerY;

    float shadeLengthRatio = shadeLength / FloatMath.sqrt(dx1 * dx1 + dy1 * dy1);
    float nx1 = dy1 * shadeLengthRatio;
    float ny1 = -dx1 * shadeLengthRatio;

    LinearGradient gradient1 = new LinearGradient(
        centerX, centerY, centerX + nx1, centerY + ny1,
        shadeColor, baseColor, Shader.TileMode.CLAMP);

    float dx2 = x2 - centerX;
    float dy2 = y2 - centerY;

    float nx2 = -dy2 * shadeLengthRatio;
    float ny2 = dx2 * shadeLengthRatio;

    LinearGradient gradient2 = new LinearGradient(
        centerX, centerY, centerX + nx2, centerY + ny2,
        shadeColor, baseColor, Shader.TileMode.CLAMP);

    paint.setShader(new ComposeShader(gradient1, gradient2, Mode.DARKEN));
  }

  private void resetPath(float centerX, float centerY, int x1, int y1, int x2, int y2) {
    // Create triangle path.
    path.reset();
    path.moveTo(centerX, centerY);
    path.lineTo(x1, y1);
    path.lineTo(x2, y2);
    path.close();
  }
}
