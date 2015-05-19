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

import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.Shader;
import android.graphics.Shader.TileMode;

import java.util.Arrays;

/**
 * Drawable to render a candidate background.
 *
 */
public class CandidateBackgroundFocusedDrawable extends BaseBackgroundDrawable {
  private static final int SHADOW_PIXELS = 3;

  private final int topColor;
  private final int bottomColor;
  private final int shadowColor;

  private int left;
  private int top;
  private int right;
  private int bottom;

  /** Cached Paint instance to reuse for performance reason. */
  private Paint paint = new Paint();
  private Shader[] shaderList = new Shader[3];

  public CandidateBackgroundFocusedDrawable(
      int leftPadding, int topPadding, int rightPadding, int bottomPadding,
      int topColor, int bottomColor, int shadowColor) {
    super(leftPadding, topPadding, rightPadding, bottomPadding);
    this.topColor = topColor;
    this.bottomColor = bottomColor;
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

    for (Shader shader : shaderList) {
      if (shader != null) {
        paint.reset();
        paint.setAntiAlias(true);
        paint.setShader(shader);
        canvas.drawRect(left, top, right, bottom, paint);
      }
    }
  }

  @Override
  protected void onBoundsChange(Rect rect) {
    super.onBoundsChange(rect);

    if (isCanvasRectEmpty()) {
      Arrays.fill(shaderList, null);
      return;
    }

    Rect canvasRect = getCanvasRect();
    left = canvasRect.left;
    top = canvasRect.top;
    right = canvasRect.right;
    bottom = canvasRect.bottom;
    // Shader filling with simple gradient color.
    shaderList[0] = new LinearGradient(0, top, 0, bottom, topColor, bottomColor, TileMode.CLAMP);

    // Shader rendering top/bottom edge gradient shadows.
    float verticalShadowStep0 = (SHADOW_PIXELS + 1) / (float) (bottom - top);
    float verticalShadowStep1 = 1.0f - verticalShadowStep0;
    shaderList[1] = new LinearGradient(0, top, 0, bottom,
        new int[]{shadowColor, 0, 0, shadowColor},
        new float[]{0.0f, verticalShadowStep0, verticalShadowStep1, 1.0f}, TileMode.CLAMP);

    // Shader rendering left/right edge gradient shadows.
    float horizontalShadowStep0 = (SHADOW_PIXELS + 1) / (float) (right - left);
    float horizontalShadowStep1 = 1.0f - horizontalShadowStep0;
    shaderList[2] = new LinearGradient(left, 0, right, 0,
        new int[]{shadowColor, 0, 0, shadowColor},
        new float[]{0.0f, horizontalShadowStep0, horizontalShadowStep1, 1.0f}, TileMode.CLAMP);
  }
}
