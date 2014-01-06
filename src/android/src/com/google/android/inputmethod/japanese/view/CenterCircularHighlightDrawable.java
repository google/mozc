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
import android.graphics.Paint;
import android.graphics.RadialGradient;
import android.graphics.Rect;
import android.graphics.Shader.TileMode;

/**
 * Drawable to render a circular highlight at the center of the key background.
 *
 */
public class CenterCircularHighlightDrawable extends BaseBackgroundDrawable {
  // According to the original design mock, the ratio of radius to height is 0.23.
  private static final float RADIUS_RATIO = 0.23f;

  private final int baseColor;
  private final int shadeColor;

  private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
  private float centerX;
  private float centerY;
  private float radius;

  public CenterCircularHighlightDrawable(
      int leftPadding, int topPadding, int rightPadding, int bottomPadding,
      int baseColor, int shadeColor) {
    super(leftPadding, topPadding, rightPadding, bottomPadding);
    this.baseColor = baseColor;
    this.shadeColor = shadeColor;
  }

  @Override
  public void draw(Canvas canvas) {
    canvas.drawCircle(centerX, centerY, radius, paint);
  }

  @Override
  protected void onBoundsChange(Rect rect) {
    super.onBoundsChange(rect);
    Rect canvasRect = getCanvasRect();
    centerX = canvasRect.exactCenterX();
    centerY = canvasRect.exactCenterY();
    radius = canvasRect.height() * RADIUS_RATIO;

    // According to the original design mock, the circle has inner shadow,
    // whose width is about 20% of the radius.
    paint.setShader(new RadialGradient(centerX, centerY, radius,
                                       new int[] { baseColor, shadeColor },
                                       new float[] { 0.8f, 1.0f },
                                       TileMode.CLAMP));
  }
}
