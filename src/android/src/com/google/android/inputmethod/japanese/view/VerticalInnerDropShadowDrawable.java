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
import android.graphics.Shader.TileMode;

/**
 * Drawable for drop shadow on top and bottom of the boundary.
 */
public class VerticalInnerDropShadowDrawable extends BaseBackgroundDrawable {

  private final int shadowSize;
  private LinearGradient topShadow = null;
  private LinearGradient bottomShadow = null;
  private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);

  public VerticalInnerDropShadowDrawable(
      int leftPadding, int topPadding, int rightPadding, int bottomPadding,
      int shadowSize) {
    super(leftPadding, topPadding, rightPadding, bottomPadding);
    this.shadowSize = shadowSize;
  }

  @Override
  public void draw(Canvas canvas) {
    if (isCanvasRectEmpty()) {
      return;
    }

    Rect rect = getCanvasRect();
    if (topShadow != null) {
      paint.setShader(topShadow);
      canvas.drawRect(rect, paint);
    }

    if (bottomShadow != null) {
      paint.setShader(bottomShadow);
      canvas.drawRect(rect, paint);
    }
  }

  @Override
  protected void onBoundsChange(Rect bounds) {
    super.onBoundsChange(bounds);
    if (isCanvasRectEmpty()) {
      topShadow = null;
      bottomShadow = null;
      return;
    }

    Rect canvasRect = getCanvasRect();
    topShadow = new LinearGradient(
        0, canvasRect.top, 0, Math.min(canvasRect.bottom, canvasRect.top + shadowSize),
        0xA6000000, 0x00000000, TileMode.CLAMP);
    bottomShadow = new LinearGradient(
        0, Math.max(canvasRect.top, canvasRect.bottom - shadowSize), 0, canvasRect.bottom,
        0x00000000, 0xA6000000, TileMode.CLAMP);
  }
}
