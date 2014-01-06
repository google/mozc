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

import org.mozc.android.inputmethod.japanese.R;
import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory;

import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Path.Direction;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Shader.TileMode;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;

/**
 * Factory to produce the buttons for SymbolMajorCategories.
 *
 */
public class SymbolMajorCategoryButtonDrawableFactory {

  private interface PathFactory {
    Path newInstance(Rect bounds);
  }

  private static class LeftButtonPathFactory implements PathFactory {
    private final float round;

    LeftButtonPathFactory(float round) {
      this.round = round;
    }

    @Override
    public Path newInstance(Rect bounds) {
      float left = bounds.left;
      float top = bounds.top;
      float right = bounds.right - 2;
      float bottom = bounds.bottom - 1;

      Path path = new Path();
      path.moveTo(right, bottom);
      path.arcTo(new RectF(left, bottom - round * 2, left + round * 2, bottom), 90, 90);
      path.arcTo(new RectF(left, top, left + round + 2, top + round * 2), 180, 90);
      path.lineTo(right, top);
      path.close();

      return path;
    }
  }

  private static class CenterButtonPathFactory implements PathFactory {
    @Override
    public Path newInstance(Rect bounds) {
      float left = bounds.left;
      float top = bounds.top;
      float right = bounds.right - 2;
      float bottom = bounds.bottom - 1;

      Path path = new Path();
      path.addRect(left, top, right, bottom, Direction.CW);
      return path;
    }
  }

  private static class RightButtonPathFactory implements PathFactory {
    private final float round;

    RightButtonPathFactory(float round) {
      this.round = round;
    }

    @Override
    public Path newInstance(Rect bounds) {
      float left = bounds.left;
      float top = bounds.top;
      float right = bounds.right - 1;
      float bottom = bounds.bottom - 1;

      Path path = new Path();
      path.moveTo(left, top);
      path.arcTo(new RectF(right - round * 2, top, right, top + round * 2), 270, 90);
      path.arcTo(new RectF(right - round * 2, bottom - round * 2, right, bottom), 0, 90);
      path.lineTo(left, bottom);
      path.close();

      return path;
    }
  }

  private static class ButtonDrawable extends BaseBackgroundDrawable {
    private final PathFactory pathFactory;
    private final int topColor;
    private final int bottomColor;
    private final int shadowColor;

    private final Paint paint = new Paint();
    private Path path = null;
    private Shader shader = null;

    ButtonDrawable(PathFactory pathFactory, int topColor, int bottomColor, int shadowColor) {
      super(0, 0, 0, 0);  // No padding.
      this.pathFactory = pathFactory;
      this.topColor = topColor;
      this.bottomColor = bottomColor;
      this.shadowColor = shadowColor;
    }

    @Override
    public void draw(Canvas canvas) {
      if (path == null) {
        return;
      }

      Paint paint = this.paint;
      if ((shadowColor & 0xFF000000) != 0) {
        paint.reset();
        paint.setAntiAlias(true);
        paint.setColor(shadowColor);
        int saveCount = canvas.save();
        try {
          canvas.translate(1, 1);
          canvas.drawPath(path, paint);
        } finally {
          canvas.restoreToCount(saveCount);
        }
      }

      paint.reset();
      paint.setAntiAlias(true);
      paint.setShader(shader);
      canvas.drawPath(path, paint);
    }

    @Override
    protected void onBoundsChange(Rect bounds) {
      super.onBoundsChange(bounds);

      if (isCanvasRectEmpty()) {
        path = null;
        shader = null;
        return;
      }

      path = pathFactory.newInstance(bounds);
      shader = new LinearGradient(
          0, bounds.top, 0, bounds.bottom - 1, topColor, bottomColor, TileMode.CLAMP);
    }
  }

  private static class EmojiDisableIconDrawable extends BaseBackgroundDrawable {
    private final Drawable sourceDrawable;

    EmojiDisableIconDrawable(Drawable sourceDrawable) {
      super(0, 0, 0, 0);
      this.sourceDrawable = sourceDrawable;
    }

    @Override
    public void draw(Canvas canvas) {
      Rect bounds = getBounds();

      // Heuristically, the size is 1/3 of the height.
      int size = bounds.height() / 3;
      sourceDrawable.setBounds(0, 0, size, size);

      int saveCount = canvas.save();
      try {
        canvas.translate(bounds.right - size - 3, bounds.bottom - size - 3);
        canvas.clipRect(0, 0, size, size);
        sourceDrawable.draw(canvas);
      } finally {
        canvas.restoreToCount(saveCount);
      }
    }
  }

  private final MozcDrawableFactory factory;

  private final int topColor;
  private final int bottomColor;
  private final int pressedTopColor;
  private final int pressedBottomColor;
  private final int shadowColor;

  private final PathFactory leftButtonPathFactory;
  private final PathFactory centerButtonPathFactory;
  private final PathFactory rightButtonPathFactory;

  public SymbolMajorCategoryButtonDrawableFactory(
      MozcDrawableFactory factory, int topColor, int bottomColor,
      int pressedTopColor, int pressedBottomColor, int shadowColor,
      float round) {
    this.factory = factory;
    this.topColor = topColor;
    this.bottomColor = bottomColor;
    this.pressedTopColor = pressedTopColor;
    this.pressedBottomColor = pressedBottomColor;
    this.shadowColor = shadowColor;

    this.leftButtonPathFactory = new LeftButtonPathFactory(round);
    this.centerButtonPathFactory = new CenterButtonPathFactory();
    this.rightButtonPathFactory = new RightButtonPathFactory(round);
  }

  public Drawable createLeftButtonDrawable() {
    return BackgroundDrawableFactory.createSelectableDrawable(
        new ButtonDrawable(leftButtonPathFactory, pressedTopColor, pressedBottomColor, 0),
        new ButtonDrawable(leftButtonPathFactory, topColor, bottomColor, shadowColor));
  }

  public Drawable createCenterButtonDrawable() {
    return BackgroundDrawableFactory.createSelectableDrawable(
        new ButtonDrawable(centerButtonPathFactory, pressedTopColor, pressedBottomColor, 0),
        new ButtonDrawable(centerButtonPathFactory, topColor, bottomColor, shadowColor));
  }

  public Drawable createRightButtonDrawable(boolean emojiEnabled) {
    Drawable drawable = BackgroundDrawableFactory.createSelectableDrawable(
        new ButtonDrawable(rightButtonPathFactory, pressedTopColor, pressedBottomColor, 0),
        new ButtonDrawable(rightButtonPathFactory, topColor, bottomColor, shadowColor));
    if (emojiEnabled) {
      return drawable;
    }
    return new LayerDrawable(new Drawable[] {
        drawable,
        new EmojiDisableIconDrawable(factory.getDrawable(R.raw.emoji_disable_icon)),
    });
  }
}
