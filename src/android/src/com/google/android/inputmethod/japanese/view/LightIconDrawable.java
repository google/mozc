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
import android.graphics.ColorFilter;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.BlurMaskFilter.Blur;
import android.graphics.Paint.Style;
import android.graphics.Shader.TileMode;
import android.graphics.drawable.Drawable;

/**
 * The light (indicator) mark for ALT key on qwerty layout.
 *
 */
public class LightIconDrawable extends Drawable {
  private final float topOffset;
  private final float rightOffset;

  private final int lightColor;
  private final int darkColor;
  private final float radius;

  private float centerX;
  private float centerY;

  private final Paint basePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
  private final Paint shadePaint = new Paint(Paint.ANTI_ALIAS_FLAG);

  public LightIconDrawable(float topOffset, float rightOffset,
                           int lightColor, int darkColor, int shadeColor, float radius) {
    this.topOffset = topOffset;
    this.rightOffset = rightOffset;
    this.lightColor = lightColor;
    this.darkColor = darkColor;
    this.radius = radius;

    if (lightColor == darkColor) {
      basePaint.setColor(lightColor);
    }

    shadePaint.setStyle(Style.STROKE);
    shadePaint.setStrokeWidth(1);
    shadePaint.setMaskFilter(new BlurMaskFilter(2, Blur.INNER));
    shadePaint.setColor(shadeColor);
  }

  /* (non-Javadoc)
   * @see android.graphics.drawable.Drawable#draw(android.graphics.Canvas)
   */
  @Override
  public void draw(Canvas canvas) {
    canvas.drawCircle(centerX, centerY, radius, basePaint);
    canvas.drawCircle(centerX, centerY, radius, shadePaint);
  }

  @Override
  protected void onBoundsChange(Rect bound) {
    super.onBoundsChange(bound);

    centerX = bound.right - rightOffset;
    centerY = bound.top + topOffset;

    if (lightColor != darkColor) {
      float top = centerY - radius;
      // Hard coded parameters are based on design mock.
      basePaint.setShader(new LinearGradient(
          centerX, top, centerX + 2, top + 8, lightColor, darkColor, TileMode.CLAMP));
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
