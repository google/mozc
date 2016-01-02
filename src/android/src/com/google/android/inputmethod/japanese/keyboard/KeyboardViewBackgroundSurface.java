// Copyright 2010-2016, Google Inc.
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

import org.mozc.android.inputmethod.japanese.MemoryManageable;
import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory.DrawableType;
import org.mozc.android.inputmethod.japanese.keyboard.Flick.Direction;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.view.DrawableCache;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.PorterDuff;
import android.graphics.Region.Op;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.PictureDrawable;

import java.util.Collections;
import java.util.EnumMap;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

import javax.annotation.Nullable;

/**
 * Implementation of the background surface for {@link KeyboardView}.
 * This class takes care of double-buffering and diff-only-updating to improve the rendering
 * performance for better user experiences.
 *
 * <p>An example usage of this class is as follows:
 * <pre>{@code
 * KeyboardViewBackgroundSurface backgroundSurface = new KeyboardViewBackgroundSurface();
 * // Request to set the keyboard and its meta state.
 * backgroundSurface.requestUpdateKeyboard(keyboard, metaState);
 *
 * // Request to set the size of the image.
 * backgroundSurface.requestUpdateSize(viewWidth, viewHeight);
 *
 * // Add requests to update keys' images. Note: the actual update is not done by requests.
 * backgroundSurface.requestUpdateKey(key1, Flick.Direction.CENTER);
 * backgroundSurface.requestUpdateKey(key2, Flick.Direction.CENTER);
 * backgroundSurface.requestUpdateKey(key3, Flick.Direction.CENTER);
 *
 * // Many update requests for the same key is valid.
 * backgroundSurface.requestUpdateKey(key1, Flick.Direction.LEFT);
 * backgroundSurface.requestUpdateKey(key3, Flick.Direction.RIGHT);
 *
 * // Set flick direction to null means the key is released.
 * backgroundSurface.requestUpdateKey(key1, Optional.<Direction>absent());
 *
 * // Actual update is done below.
 * backgroundSurface.update();
 *
 * // Render the image to canvas.
 * backgroundSurface.draw(canvas);
 * }</pre>
 *
 * This class is exposed as public in order to mock for testing purpose.
 *
 */
@VisibleForTesting public class KeyboardViewBackgroundSurface implements MemoryManageable {

  /**
   * A simple rendering related utilities for keyboard rendering.
   */
  @VisibleForTesting interface SurfaceCanvas {

    /**
     * Clears a rectangle region of
     * {@code (x, y) -- (x + width, y + height)}.
     */
    void clearRegion(int x, int y, int width, int height);

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

    /**
     * Clear the canvas by transparent color.
     */
    private static final int CLEAR_COLOR = 0x00000000;

    private final Canvas canvas;

    SurfaceCanvasImpl(Canvas canvas) {
      this.canvas = Preconditions.checkNotNull(canvas);
    }

    @Override
    public void clearRegion(int x, int y, int width, int height) {
      int saveCount = canvas.save();
      try {
        canvas.clipRect(x, y, x + width, y + height, Op.REPLACE);
        canvas.drawColor(CLEAR_COLOR, PorterDuff.Mode.CLEAR);
      } finally {
        canvas.restoreToCount(saveCount);
      }
    }

    @Override
    public void drawDrawable(@Nullable Drawable drawable, int x, int y, int width, int height) {
      if (drawable == null) {
        return;
      }
      drawDrawableInternal(drawable, x, y, width, height,
                           height / (float) drawable.getIntrinsicHeight());
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
          scaledWidth, scaledHeight, scale);
    }

    private void drawDrawableInternal(Drawable drawable, int x, int y, int width, int height,
                                      float scale) {
      int saveCount = canvas.save();
      try {
        canvas.translate(x, y);
        if (drawable.getCurrent() instanceof PictureDrawable) {
          canvas.scale(scale, scale);
          drawable.setBounds(0, 0, drawable.getIntrinsicWidth(), drawable.getIntrinsicHeight());
        } else {
          drawable.setBounds(0, 0, width, height);
        }
        drawable.draw(canvas);
      } finally {
        canvas.restoreToCount(saveCount);
      }
    }
  }

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

  // The current image and its rendering object.
  private Optional<Bitmap> surfaceBitmap = Optional.absent();
  @VisibleForTesting Optional<SurfaceCanvas> surfaceCanvas = Optional.absent();

  // Width and height this background surface is to be in pixels
  private int requestedWidth;
  private int requestedHeight;

  private boolean fullUpdateRequested;
  private Optional<Keyboard> requestedKeyboard = Optional.absent();
  private Set<MetaState> requestedMetaState = Collections.emptySet();

  public KeyboardViewBackgroundSurface(
      BackgroundDrawableFactory backgroundDrawableFactory, DrawableCache drawableCache) {
    this.backgroundDrawableFactory = Preconditions.checkNotNull(backgroundDrawableFactory);
    this.drawableCache = Preconditions.checkNotNull(drawableCache);
  }

  /**
   * A set of pending keys to be updated.
   * Note that the null value of Flick.Direction means the Key is released.  This is a hack to
   * represents all the states of released, pressed(CENTER), and flicks to four directions
   * in a single value.
   *
   * TODO(hidehiko): We should have direction state in somewhere else, not in the pending requests,
   *   so that we can re-render the correct image even if we have any sequence of requests.
   */
  @VisibleForTesting
  final Map<Key, Optional<Flick.Direction>> pendingKeys =
      new HashMap<Key, Optional<Flick.Direction>>();

  /**
   * Resets and release the current image this instance holds.
   * By this method's invocation, we assume all key's directions are also reset.
   */
  public void reset() {
    if (surfaceBitmap.isPresent()) {
      surfaceBitmap.get().recycle();
      surfaceBitmap = Optional.absent();
    }
    surfaceCanvas = Optional.absent();
    pendingKeys.clear();
  }

  /**
   * Adds the given {@code key} to the pending key set to render it lazily.
   * In other words, the key isn't rendered until {@link #update()} is invoked.
   */
  public void requestUpdateKey(Key key, Optional<Flick.Direction> flickDirection) {
    if (!Preconditions.checkNotNull(key).isSpacer()) {
      pendingKeys.put(key, Preconditions.checkNotNull(flickDirection));
    }
  }

  /**
   * Adds a task to update the size of the image. The actual update is done lazily (when
   * {@link #update()} is invoked).
   */
  public void requestUpdateSize(int width, int height) {
    requestedWidth = width;
    requestedHeight = height;
  }

  /**
   * Resets the {@code keyboard} and {@code metaState} to be rendered on this surface.
   * This also cancels all key's direction for now.
   */
  public void requestUpdateKeyboard(Keyboard keyboard, Set<MetaState> metaState) {
    requestedKeyboard = Optional.of(keyboard);
    requestedMetaState = Preconditions.checkNotNull(metaState);
    fullUpdateRequested = true;

    // We set all keyboard update request here, and it overwrites the current pending key update
    // requests.
    pendingKeys.clear();
  }

  /**
   * Requests new {@code metaState} to be rendered on this surface.
   *
   * This method requests redraw only the keys which are required to be redrawn according to
   * metastate's change.
   * This also cancels such keys' direction for now.
   */
  public void requestMetaState(Set<MetaState> newMetaState) {
    Preconditions.checkNotNull(newMetaState);

    Set<MetaState> previousMetaState = requestedMetaState;
    requestedMetaState = newMetaState;

    if (newMetaState.equals(previousMetaState) || !requestedKeyboard.isPresent()) {
      return;
    }

    // Update only the keys which should update corresponding KeyState based on given metaState.
    for (Row row : requestedKeyboard.get().getRowList()) {
      for (Key key : row.getKeyList()) {
        KeyState previousKeyState = key.getKeyState(previousMetaState).orNull();
        KeyState newKeyState = key.getKeyState(newMetaState).orNull();
        // Intentionally using != operator instead of equals method.
        // - Faster than full-spec equals method.
        // - The values of Optional which are returned by Key#getKeyState are always
        //   the same object so equals is overkill.
        if (previousKeyState != newKeyState) {
          // Request to draw the key.
          // pendingKeys may have already contained corresponding key but overwrite here.
          pendingKeys.put(key, Optional.<Direction>absent());
        }
      }
    }
  }

  /**
   * Actually updates the image this instance holds based on pending update requests.
   */
  public void update() {
    if (isInitializationNeeded()) {
      initialize();
      // The bitmap has been re-created, so it is necessary to re-render all of the keyboard.
      fullUpdateRequested = true;
    }

    if (!requestedKeyboard.isPresent()) {
      // We have nothing to do.
      return;
    }

    // A new keyboard or meta state is set, so we need re-render all of the keyboard.
    if (fullUpdateRequested) {
      clearCanvas();
      renderKeyboard();
      fullUpdateRequested = false;
    } else {
      renderPendingKeys();
    }

    // Clean up state.
    pendingKeys.clear();
  }

  /**
   * Returns {@code true} iff (re-)initialization is needed.
   */
  @VisibleForTesting boolean isInitializationNeeded() {
    // We need to re-create background buffer if
    // - no initialization is done (after construction or reset).
    // - the size of view has been changed.
    Optional<Bitmap> bitmap = surfaceBitmap;
    return (!bitmap.isPresent())
        || (bitmap.get().getWidth() != requestedWidth)
        || (bitmap.get().getHeight() != requestedHeight);
  }

  void initialize() {
    if (surfaceBitmap.isPresent()) {
      surfaceBitmap.get().recycle();
    }
    surfaceBitmap = Optional.of(
        MozcUtil.createBitmap(requestedWidth, requestedHeight, Bitmap.Config.ARGB_8888));
    surfaceCanvas = Optional.<SurfaceCanvas>of(
        new SurfaceCanvasImpl(new Canvas(surfaceBitmap.get())));
  }

  /**
   * Draws the current keyboard image to the given canvas.
   * It is required to invoke update() method before this method's invocation.
   */
  public void draw(Canvas canvas) {
    canvas.drawBitmap(
        surfaceBitmap.orNull(), requestedKeyboard.get().contentLeft,
        requestedKeyboard.get().contentTop, null);
  }

  private void clearCanvas() {
    if (surfaceBitmap.isPresent() && surfaceCanvas.isPresent()) {
      Bitmap bitmap = surfaceBitmap.get();
      surfaceCanvas.get().clearRegion(0, 0, bitmap.getWidth(), bitmap.getHeight());
    }
  }

  private void renderKey(Key key, Optional<Flick.Direction> flickDirection) {
    Preconditions.checkNotNull(key);
    Preconditions.checkNotNull(flickDirection);
    Preconditions.checkState(surfaceCanvas.isPresent());
    Preconditions.checkState(requestedKeyboard.isPresent());
    SurfaceCanvas canvas = surfaceCanvas.get();

    int horizontalGap = key.getHorizontalGap();
    int leftGap = horizontalGap / 2;
    boolean isPressed = flickDirection.isPresent();

    // We split the gap to both sides evenly.
    int x = key.getX() + leftGap - requestedKeyboard.get().contentLeft;
    int y = key.getY() - requestedKeyboard.get().contentTop;
    // Given width/height for the key.
    // The icon is drawn inside the width/height.
    int givenWidth = key.getWidth() - horizontalGap;
    int givenHeight = key.getHeight();

    canvas.drawDrawable(getKeyBackground(key, isPressed).orNull(), x, y, givenWidth, givenHeight);
    Optional<KeyEntity> keyEntity =
        getKeyEntityForRendering(key, requestedMetaState, flickDirection);
    if (flickDirection.isPresent() && keyEntity.isPresent()
        && keyEntity.get().isFlickHighlightEnabled()
        && KeyEventContext.getKeyEntity(key, requestedMetaState, flickDirection)
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
   * Draws all keys in the keyboard.
   */
  private void renderKeyboard() {
    for (Row row : requestedKeyboard.get().getRowList()) {
      for (Key key : row.getKeyList()) {
        Optional<Direction> direction = pendingKeys.get(key);
        if (direction == null) {
          renderKey(key, Optional.<Direction>absent());
        } else {
          renderKey(key, direction);
        }
      }
    }
  }

  private void renderPendingKeys() {
    Preconditions.checkState(surfaceCanvas.isPresent());
    // The canvas object is used many times, so cache it on local stack.
    SurfaceCanvas canvas = surfaceCanvas.get();
    int offsetX = requestedKeyboard.get().contentLeft;
    int offsetY = requestedKeyboard.get().contentTop;
    for (Map.Entry<Key, Optional<Flick.Direction>> entry : pendingKeys.entrySet()) {
      Key key = entry.getKey();

      // Clear the key's region and then draw the key there.
      canvas.clearRegion(key.getX() - offsetX, key.getY() - offsetY, key.getWidth(),
                         key.getHeight());
      renderKey(key, entry.getValue());
    }
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

  @Override
  public void trimMemory() {
    // drawableCache is not cleared here. It's done in KeyboardView where it is created.
    reset();
  }
}
