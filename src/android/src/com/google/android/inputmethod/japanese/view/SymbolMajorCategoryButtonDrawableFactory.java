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

import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory;
import org.mozc.android.inputmethod.japanese.resources.R;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.content.res.Resources;
import android.graphics.BlurMaskFilter;
import android.graphics.BlurMaskFilter.Blur;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Paint.Style;
import android.graphics.Path;
import android.graphics.Path.Direction;
import android.graphics.Rect;
import android.graphics.RectF;
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
    private final float padding;
    private final float round;

    // Padding is applied to left, top and bottom.
    // Right doesn't have padding in order to center left/right buttons.
    LeftButtonPathFactory(float padding, float round) {
      this.padding = padding;
      this.round = round;
    }

    @Override
    public Path newInstance(Rect bounds) {
      Preconditions.checkNotNull(bounds);
      float left = bounds.left + padding;
      float top = bounds.top + padding;
      float right = bounds.right - 2;
      float bottom = bounds.bottom - 1 - padding;

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
    private final float padding;

    // Padding is applied only to top and bottom.
    CenterButtonPathFactory(float padding) {
      this.padding = padding;
    }

    @Override
    public Path newInstance(Rect bounds) {
      Preconditions.checkNotNull(bounds);
      float left = bounds.left;
      float top = bounds.top + padding;
      float right = bounds.right - 2;
      float bottom = bounds.bottom - 1 - padding;

      Path path = new Path();
      path.addRect(left, top, right, bottom, Direction.CW);
      return path;
    }
  }

  private static class RightButtonPathFactory implements PathFactory {
    private final float padding;
    private final float round;

    // Padding is applied to right, top and bottom.
    // Left doesn't have padding in order to center left/right buttons.
    RightButtonPathFactory(float padding, float round) {
      this.padding = padding;
      this.round = round;
    }

    @Override
    public Path newInstance(Rect bounds) {
      Preconditions.checkNotNull(bounds);
      float left = bounds.left;
      float top = bounds.top + padding;
      float right = bounds.right - 1 - padding;
      float bottom = bounds.bottom - 1 - padding;

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

    private static final int BLUR_SIZE = 3;

    private final PathFactory pathFactory;
    private final int topColor;
    private final int bottomColor;

    private final Paint backgroundPaint = new Paint();
    private final Optional<Paint> shadowPaint;
    private Optional<Path> path = Optional.absent();

    ButtonDrawable(PathFactory pathFactory, int topColor, int bottomColor, int shadowColor) {
      super(0, 0, 0, 0);  // No padding.
      this.pathFactory = Preconditions.checkNotNull(pathFactory);
      this.topColor = topColor;
      this.bottomColor = bottomColor;

      backgroundPaint.setAntiAlias(true);

      if (Color.alpha(shadowColor) != 0) {
        shadowPaint = Optional.of(new Paint());
        shadowPaint.get().setColor(shadowColor);
        shadowPaint.get().setStyle(Style.FILL);
        shadowPaint.get().setMaskFilter(new BlurMaskFilter(BLUR_SIZE, Blur.NORMAL));
      } else {
        shadowPaint = Optional.absent();
      }
    }

    @Override
    public void draw(Canvas canvas) {
      if (!path.isPresent()) {
        return;
      }

      if (shadowPaint.isPresent()) {
        int saveCount = canvas.save();
        try {
          canvas.translate(0, 2);
          canvas.drawPath(path.get(), shadowPaint.get());
        } finally {
          canvas.restoreToCount(saveCount);
        }
      }

      canvas.drawPath(path.get(), backgroundPaint);
    }

    @Override
    protected void onBoundsChange(Rect bounds) {
      super.onBoundsChange(bounds);

      if (isCanvasRectEmpty()) {
        path = Optional.absent();
        backgroundPaint.setShader(null);
        return;
      }

      path = Optional.of(pathFactory.newInstance(bounds));
      backgroundPaint.setShader(new LinearGradient(
          0, bounds.top, 0, bounds.bottom - 1, topColor, bottomColor, TileMode.CLAMP));
    }
  }

  private static class EmojiDisableIconDrawable extends BaseBackgroundDrawable {

    private final int size;
    private final Drawable sourceDrawable;

    EmojiDisableIconDrawable(Resources resources, Drawable sourceDrawable) {
      super(0, 0, 0, 0);
      size = Preconditions.checkNotNull(resources).getDimensionPixelSize(
          R.dimen.symbol_major_emoji_disable_icon_height);
      sourceDrawable.setBounds(0, 0, size, size);
      this.sourceDrawable = Preconditions.checkNotNull(sourceDrawable);
    }

    @Override
    public void draw(Canvas canvas) {
      Rect bounds = getBounds();


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

  private Skin skin = Skin.getFallbackInstance();

  private final Resources resources;

  public SymbolMajorCategoryButtonDrawableFactory(Resources resources) {
    this.resources = Preconditions.checkNotNull(resources);
  }

  public Drawable createLeftButtonDrawable() {
    return createSelectableDrawableWithPathFactory(
        new LeftButtonPathFactory(skin.symbolMajorButtonPaddingDimension,
                                  skin.symbolMajorButtonRoundDimension));
  }

  public Drawable createCenterButtonDrawable() {
    return createSelectableDrawableWithPathFactory(
        new CenterButtonPathFactory(skin.symbolMajorButtonPaddingDimension));
  }

  public Drawable createRightButtonDrawable(boolean emojiEnabled) {
    Drawable drawable = createSelectableDrawableWithPathFactory(
        new RightButtonPathFactory(skin.symbolMajorButtonPaddingDimension,
                                   skin.symbolMajorButtonRoundDimension));
    if (emojiEnabled) {
      return drawable;
    }
    return new LayerDrawable(new Drawable[] {
        drawable,
        new EmojiDisableIconDrawable(
            resources, skin.getDrawable(resources, R.raw.emoji_disable_icon)),
    });
  }

  private Drawable createSelectableDrawableWithPathFactory(PathFactory pathFactory) {
    return BackgroundDrawableFactory.createSelectableDrawable(
        new ButtonDrawable(pathFactory,
                           skin.symbolMajorButtonSelectedTopColor,
                           skin.symbolMajorButtonSelectedBottomColor, 0),
        Optional.<Drawable>of(BackgroundDrawableFactory.createPressableDrawable(
            new ButtonDrawable(pathFactory,
                               skin.symbolMajorButtonPressedTopColor,
                               skin.symbolMajorButtonPressedBottomColor, 0),
            Optional.<Drawable>of(new ButtonDrawable(pathFactory,
                                                     skin.symbolMajorButtonTopColor,
                                                     skin.symbolMajorButtonBottomColor,
                                                     skin.symbolMajorButtonShadowColor)))));
  }

  public void setSkin(Skin skin) {
    this.skin = Preconditions.checkNotNull(skin);
  }
}
