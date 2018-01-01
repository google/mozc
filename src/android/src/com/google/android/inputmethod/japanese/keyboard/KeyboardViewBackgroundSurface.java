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

package org.mozc.android.inputmethod.japanese.keyboard;

import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory.DrawableType;
import org.mozc.android.inputmethod.japanese.keyboard.Flick.Direction;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.view.DrawableCache;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;
import com.google.common.collect.Maps;
import com.google.common.collect.Sets;

import android.graphics.Canvas;
import android.graphics.drawable.Drawable;

import java.util.Collections;
import java.util.EnumMap;
import java.util.Map;
import java.util.Set;

import javax.annotation.Nullable;

/**
 * Renderer of the keyboard.
 * <p>
 * Prior to calling {@code #draw(Canvas)}, current keyboard, metastates and pressed keys should be
 * registered.
 * {@code #isDirty()} should be useful to check if the keyboard requires rendering again.
 * <p>
 * TODO(matsuzakit): Rename this class. This class doesn't have any background surface any more.
 *
 * <p>An example usage of this class is as follows:
 * <pre>{@code
 * KeyboardViewBackgroundSurface backgroundSurface = new KeyboardViewBackgroundSurface();
 * // Reset the keyboard and its meta state.
 * backgroundSurface.reset(keyboard, metaState);
 *
 * // Add pressed keys with direction.
 * backgroundSurface.addPressedKey(key1, Flick.Direction.CENTER);
 * backgroundSurface.addPressedKey(key2, Flick.Direction.CENTER);
 * backgroundSurface.addPressedKey(key3, Flick.Direction.CENTER);
 *
 * // Many update requests for the same key is valid (Overrides).
 * backgroundSurface.addPressedKey(key1, Flick.Direction.LEFT);
 * backgroundSurface.addPressedKey(key3, Flick.Direction.RIGHT);
 *
 * // Remove a key if it has been released.
 * backgroundSurface.removePressedKey(key1);
 *
 * // Iff this object is "dirty", request invalidation.
 * if (backgroundSurface.isDirty()) {
 *   invalidate();
 * }
 * ......
 * // Render the image to canvas.
 * backgroundSurface.draw(canvas);
 * }</pre>
 *
 * This class is exposed as public in order to mock for testing purpose.
 *
 */
@VisibleForTesting public class KeyboardViewBackgroundSurface {

  /**
   * A simple rendering related utilities for keyboard rendering.
   */
  @VisibleForTesting interface SurfaceCanvas {

    /**
     * Draws the given {@code drawable} in the given region with scaling.
     * Does nothing if drawable is {@code null}.
     */
    void drawDrawable(Drawable drawable, int x, int y, int width, int height);

    /**
     * Draws the given {@code drawable} at the center of the given region,
     * and this method scales the drawable to fit the given {@code height}, but it scales
     * horizontal direction to keep its aspect ratio.
     */
    void drawDrawableAtCenterWithKeepAspectRatio(
        Drawable drawable, int x, int y, int width, int height);
  }

  private static class SurfaceCanvasImpl implements SurfaceCanvas {

    private final Canvas canvas;

    SurfaceCanvasImpl(Canvas canvas) {
      this.canvas = Preconditions.checkNotNull(canvas);
    }

    @Override
    public void drawDrawable(@Nullable Drawable drawable, int x, int y, int width, int height) {
      if (drawable == null) {
        return;
      }
      drawDrawableInternal(drawable, x, y, width, height);
    }

    @Override
    public void drawDrawableAtCenterWithKeepAspectRatio(
        @Nullable Drawable drawable, int x, int y, int width, int height) {
      if (drawable == null) {
        return;
      }

      float scale = Math.min(width / (float) drawable.getIntrinsicWidth(),
                             height / (float) drawable.getIntrinsicHeight());
      int scaledWidth = Math.round(drawable.getIntrinsicWidth() * scale);
      int scaledHeight = Math.round(drawable.getIntrinsicHeight() * scale);
      drawDrawableInternal(drawable, x + (width - scaledWidth) / 2, y + (height - scaledHeight) / 2,
          scaledWidth, scaledHeight);
    }

    private void drawDrawableInternal(Drawable drawable, int x, int y, int width, int height) {
      int saveCount = canvas.save();
      try {
        canvas.translate(x, y);
        drawable.setBounds(0, 0, width, height);
        drawable.draw(canvas);
      } finally {
        canvas.restoreToCount(saveCount);
      }
    }
  }

  /**
   * True if this instance requires to be redrawn.
   */
  private boolean isDirty = true;
  private Optional<Keyboard> keyboard = Optional.absent();
  private Set<MetaState> metaStates = Collections.emptySet();

  /**
   * A set of pressed keys with their direction.
   */
  private Map<Key, Direction> pressedKeys = Maps.newHashMap();

  /**
   * Mapping table from Flick.Direction to appropriate DrawableType.
   */
  private static final Map<Flick.Direction, DrawableType> FLICK_DRAWABLE_TYPE_MAP;
  static {
    Map<Flick.Direction, DrawableType> map =
        new EnumMap<Flick.Direction, DrawableType>(Flick.Direction.class);
    map.put(Flick.Direction.CENTER, DrawableType.TWELVEKEYS_CENTER_FLICK);
    map.put(Flick.Direction.LEFT, DrawableType.TWELVEKEYS_LEFT_FLICK);
    map.put(Flick.Direction.UP, DrawableType.TWELVEKEYS_UP_FLICK);
    map.put(Flick.Direction.RIGHT, DrawableType.TWELVEKEYS_RIGHT_FLICK);
    map.put(Flick.Direction.DOWN, DrawableType.TWELVEKEYS_DOWN_FLICK);
    FLICK_DRAWABLE_TYPE_MAP = Collections.unmodifiableMap(map);
  }

  // Note: This array is mutable unfortunately due to language spec of Java,
  // but mustn't be overwritten by any methods, and should be treated as a constant value.
  private static final int[] STATE_PRESSED = { android.R.attr.state_pressed };
  private static final int[] STATE_DEFAULT = {};

  private final BackgroundDrawableFactory backgroundDrawableFactory;
  private final DrawableCache drawableCache;

  public KeyboardViewBackgroundSurface(
      BackgroundDrawableFactory backgroundDrawableFactory, DrawableCache drawableCache) {
    this.backgroundDrawableFactory = Preconditions.checkNotNull(backgroundDrawableFactory);
    this.drawableCache = Preconditions.checkNotNull(drawableCache);
  }

  @VisibleForTesting void draw(SurfaceCanvas surfaceCanvas) {
    if (!keyboard.isPresent()) {
      return;
    }
    Keyboard keyboard = this.keyboard.get();
    for (Row row : keyboard.getRowList()) {
      for (Key key : row.getKeyList()) {
        renderKey(surfaceCanvas, keyboard, metaStates, key,
                  Optional.fromNullable(pressedKeys.get(key)));
      }
    }
    isDirty = false;
  }

  public void draw(Canvas canvas) {
    Preconditions.checkNotNull(canvas);
    draw(new SurfaceCanvasImpl(canvas));
  }

  private void renderKey(SurfaceCanvas canvas, Keyboard keyboard, Set<MetaState> metaStates,
                         Key key, Optional<Flick.Direction> flickDirection) {
    Preconditions.checkNotNull(canvas);
    Preconditions.checkNotNull(keyboard);
    Preconditions.checkNotNull(metaStates);
    Preconditions.checkNotNull(key);
    Preconditions.checkNotNull(flickDirection);

    int horizontalGap = key.getHorizontalGap();
    int leftGap = horizontalGap / 2;
    boolean isPressed = flickDirection.isPresent();

    // We split the gap to both sides evenly.
    int x = key.getX() + leftGap;
    int y = key.getY();
    // Given width/height for the key.
    // The icon is drawn inside the width/height.
    int givenWidth = key.getWidth() - horizontalGap;
    int givenHeight = key.getHeight();

    canvas.drawDrawable(getKeyBackground(key, isPressed).orNull(), x, y, givenWidth, givenHeight);
    Optional<KeyEntity> keyEntity =
        getKeyEntityForRendering(key, metaStates, flickDirection);
    if (flickDirection.isPresent() && keyEntity.isPresent()
        && keyEntity.get().isFlickHighlightEnabled()
        && KeyEventContext.getKeyEntity(key, metaStates, flickDirection)
            .equals(keyEntity)) {
      DrawableType drawableType = FLICK_DRAWABLE_TYPE_MAP.get(flickDirection.get());
      Drawable backgroundDrawable = (drawableType != null)
          ? backgroundDrawableFactory.getDrawable(drawableType) : null;
      canvas.drawDrawable(backgroundDrawable, x, y, givenWidth, givenHeight);
    }
    int horizontalPadding = keyEntity.isPresent() ? keyEntity.get().getHorizontalPadding() : 0;
    int verticalPadding = keyEntity.isPresent() ? keyEntity.get().getVerticalPadding() : 0;
    int iconWidth = keyEntity.isPresent() ? keyEntity.get().getIconWidth() : givenWidth;
    int iconHeight = keyEntity.isPresent() ? keyEntity.get().getIconHeight() : givenHeight;
    iconWidth = Math.min(iconWidth, givenWidth - horizontalPadding * 2);
    iconHeight = Math.min(iconHeight, givenHeight - verticalPadding * 2);
    canvas.drawDrawableAtCenterWithKeepAspectRatio(
        getKeyIcon(drawableCache, keyEntity, isPressed).orNull(),
        x + (givenWidth - iconWidth) / 2, y + (givenHeight - iconHeight) / 2,
        iconWidth, iconHeight);
  }

  /**
   * Returns KeyEntity which should be used for the {@code key}'s rendering with the given state.
   * {@code Optional.absent()} will be returned if we don't need to render the key.
   */
  @VisibleForTesting static Optional<KeyEntity> getKeyEntityForRendering(
      Key key, Set<MetaState> metaState, Optional<Flick.Direction> flickDirection) {
    if (flickDirection.isPresent()) {
      // If the key is under flick state, check if there is corresponding key entity for the
      // direction first.
      Optional<KeyEntity> keyEntity = KeyEventContext.getKeyEntity(key, metaState, flickDirection);
      if (keyEntity.isPresent()) {
        return keyEntity;
      }
    }

    // Use CENTER direction for released key, or a key flicked to a unsupported direction.
    return KeyEventContext.getKeyEntity(key, metaState, Optional.of(Flick.Direction.CENTER));
  }

  private static Optional<Drawable> setDrawableState(
      Optional<Drawable> drawable, boolean isPressed) {
    if (drawable.isPresent()) {
      drawable.get().setState(isPressed ? STATE_PRESSED : STATE_DEFAULT);
    }
    return drawable;
  }

  /**
   * Returns {@code Drawable} for the given key's background with setting appropriate state.
   */
  @VisibleForTesting Optional<Drawable> getKeyBackground(Key key, boolean isPressed) {
    Preconditions.checkNotNull(key);
    return setDrawableState(
        Optional.of(backgroundDrawableFactory.getDrawable(key.getKeyBackgroundDrawableType())),
        isPressed);
 }

  /**
   * Returns {@code Drawable} for the given key's icon with setting appropriate state.
   */
  @VisibleForTesting static Optional<Drawable> getKeyIcon(
      DrawableCache drawableCache, Optional<KeyEntity> keyEntity, boolean isPressed) {
    if (!keyEntity.isPresent()) {
      return Optional.absent();
    }

    return setDrawableState(
        drawableCache.getDrawable(keyEntity.get().getKeyIconResourceId()), isPressed);
  }

  public void addPressedKey(Key key, Direction direction) {
    Preconditions.checkNotNull(key);
    Preconditions.checkNotNull(direction);
    if (!direction.equals(pressedKeys.put(key, direction))) {
      isDirty = true;
    }
  }

  public void removePressedKey(Key key) {
    Preconditions.checkNotNull(key);
    if (pressedKeys.remove(key) != null) {
      isDirty = true;
    }
  }

  public void clearPressedKey() {
    if (!pressedKeys.isEmpty()) {
      pressedKeys.clear();
      isDirty = true;
    }
  }

  public void setMetaStates(Set<MetaState> metaStates) {
    Preconditions.checkNotNull(metaStates);
    if (!this.metaStates.equals(metaStates)) {
      this.metaStates = Sets.newEnumSet(metaStates, MetaState.class);
      isDirty = true;
    }
  }

  public boolean isDirty() {
    return isDirty;
  }

  public void reset(Optional<Keyboard> keyboard, Set<MetaState> metaStates) {
    Preconditions.checkNotNull(keyboard);
    Preconditions.checkNotNull(metaStates);
    clearPressedKey();
    this.keyboard = keyboard;
    setMetaStates(metaStates);
    isDirty = true;
  }
}
