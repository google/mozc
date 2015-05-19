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

package org.mozc.android.inputmethod.japanese.keyboard;

import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory.DrawableType;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.view.DrawableCache;

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
 * backgroundSurface.requestUpdateKey(key1, null);
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
public class KeyboardViewBackgroundSurface {

  /**
   * A simple rendering related utilities for keyboard rendering.
   */
  interface SurfaceCanvas {

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
      this.canvas = canvas;
    }

    @Override
    public void clearRegion(int x, int y, int width, int height) {
      Canvas canvas = this.canvas;
      canvas.save();
      try {
        canvas.clipRect(x, y, x + width, y + height, Op.REPLACE);
        canvas.drawColor(CLEAR_COLOR, PorterDuff.Mode.CLEAR);
      } finally {
        canvas.restore();
      }
    }

    @Override
    public void drawDrawable(Drawable drawable, int x, int y, int width, int height) {
      if (drawable == null) {
        return;
      }
      drawDrawableInternal(drawable, x, y, width, height,
                           height / (float) drawable.getIntrinsicHeight());
    }

    @Override
    public void drawDrawableAtCenterWithKeepAspectRatio(
        Drawable drawable, int x, int y, int width, int height) {
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
      Canvas canvas = this.canvas;

      canvas.save();
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
        canvas.restore();
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

  public KeyboardViewBackgroundSurface(
      BackgroundDrawableFactory backgroundDrawableFactory, DrawableCache drawableCache) {
    this.backgroundDrawableFactory = backgroundDrawableFactory;
    this.drawableCache = drawableCache;
  }

  // The current image and its rendering object.
  // surfaceCanvas is package private for testing purpose.
  private Bitmap surfaceBitmap;
  SurfaceCanvas surfaceCanvas;

  // Width and height this background surface is to be in pixels
  private int requestedWidth;
  private int requestedHeight;

  private boolean fullUpdateRequested;
  private Keyboard requestedKeyboard;
  private MetaState requestedMetaState;

  /**
   * A set of pending keys to be updated.
   * Note that the null value of Flick.Direction means the Key is released.  This is a hack to
   * represents all the states of released, pressed(CENTER), and flicks to four directions
   * in a single value.
   *
   * TODO(hidehiko): We should have direction state in somewhere else, not in the pending requests,
   *   so that we can re-render the correct image even if we have any sequence of requests.
   */
  private final Map<Key, Flick.Direction> pendingKeys = new HashMap<Key, Flick.Direction>();

  /**
   * Resets and release the current image this instance holds.
   * By this method's invocation, we assume all key's directions are also reset.
   */
  public void reset() {
    if (surfaceBitmap != null) {
      surfaceBitmap.recycle();
      surfaceBitmap = null;
    }
    surfaceCanvas = null;
    pendingKeys.clear();
  }

  /**
   * Adds the given {@code key} to the pending key set to render it lazily.
   * In other words, the key isn't rendered until {@link #update()} is invoked.
   * If {@code key} is {@code null}, it will be just ignored.
   */
  public void requestUpdateKey(Key key, Flick.Direction flickDirection) {
    if ((key != null) && !key.isSpacer()) {
      pendingKeys.put(key, flickDirection);
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
  public void requestUpdateKeyboard(Keyboard keyboard, MetaState metaState) {
    requestedKeyboard = keyboard;
    requestedMetaState = metaState;
    fullUpdateRequested = true;

    // We set all keyboard update request here, and it overwrites the current pending key update
    // requests.
    pendingKeys.clear();
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

    if (requestedKeyboard == null) {
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
   * Returns {@code true} iff (re-)initialization is needed. This method is package private
   * just for testing purpose.
   */
  boolean isInitializationNeeded() {
    // We need to re-create background buffer if
    // - no initialization is done (after construction or reset).
    // - the size of view has been changed.
    Bitmap bitmap = this.surfaceBitmap;
    return (bitmap == null)
        || (bitmap.getWidth() != requestedWidth)
        || (bitmap.getHeight() != requestedHeight);
  }

  void initialize() {
    if (surfaceBitmap != null) {
      surfaceBitmap.recycle();
    }
    surfaceBitmap = MozcUtil.createBitmap(requestedWidth, requestedHeight, Bitmap.Config.ARGB_8888);
    surfaceCanvas = new SurfaceCanvasImpl(new Canvas(surfaceBitmap));
  }

  /**
   * Draws the current keyboard image to the given canvas.
   * It is required to invoke update() method before this method's invocation.
   */
  public void draw(Canvas canvas) {
    canvas.drawBitmap(surfaceBitmap, requestedKeyboard.contentLeft, requestedKeyboard.contentTop,
                      null);
  }

  private void clearCanvas() {
    Bitmap bitmap = this.surfaceBitmap;
    surfaceCanvas.clearRegion(0, 0, bitmap.getWidth(), bitmap.getHeight());
  }

  private void renderKey(Key key, Flick.Direction flickDirection) {
    int horizontalGap = key.getHorizontalGap();
    int leftGap = horizontalGap / 2;
    boolean isPressed = flickDirection != null;

    // We split the gap to both sides evenly.
    int x = key.getX() + leftGap - requestedKeyboard.contentLeft;
    int y = key.getY() - requestedKeyboard.contentTop;
    int width = key.getWidth() - horizontalGap;
    int height = key.getHeight();

    KeyEntity keyEntity = getKeyEntityForRendering(key, requestedMetaState, flickDirection);
    surfaceCanvas.drawDrawable(getKeyBackground(keyEntity, isPressed), x, y, width, height);
    if (keyEntity != null && keyEntity.isFlickHighlightEnabled() &&
        KeyEventContext.getKeyEntity(key, requestedMetaState, flickDirection) == keyEntity) {
      surfaceCanvas.drawDrawable(
          backgroundDrawableFactory.getDrawable(FLICK_DRAWABLE_TYPE_MAP.get(flickDirection)),
          x, y, width, height);
    }
    surfaceCanvas.drawDrawableAtCenterWithKeepAspectRatio(
        getKeyIcon(drawableCache, keyEntity, isPressed), x, y, width, height);
  }

  /**
   * Draws all keys in the keyboard.
   */
  private void renderKeyboard() {
    for (Row row : requestedKeyboard.getRowList()) {
      for (Key key : row.getKeyList()) {
        if (!key.isSpacer()) {
          renderKey(key, pendingKeys.get(key));
        }
      }
    }
  }

  private void renderPendingKeys() {
    // The canvas object is used many times, so cache it on local stack.
    SurfaceCanvas canvas = this.surfaceCanvas;
    int offsetX = requestedKeyboard.contentLeft;
    int offsetY = requestedKeyboard.contentTop;
    for (Map.Entry<Key, Flick.Direction> entry : pendingKeys.entrySet()) {
      Key key = entry.getKey();

      // Clear the key's region and then draw the key there.
      canvas.clearRegion(key.getX() - offsetX, key.getY() - offsetY, key.getWidth(),
                         key.getHeight());
      renderKey(key, entry.getValue());
    }
  }

  /**
   * Returns KeyEntity which should be used for the {@code key}'s rendering with the given state.
   * {@code null} will be returned if we don't need to render the key.
   * This is package private for testing purpose.
   */
  static KeyEntity getKeyEntityForRendering(
      Key key, MetaState metaState, Flick.Direction flickDirection) {
    if (flickDirection != null) {
      // If the key is under flick state, check if there is corresponding key entity for the
      // direction first.
      KeyEntity keyEntity = KeyEventContext.getKeyEntity(key, metaState, flickDirection);
      if (keyEntity != null) {
        return keyEntity;
      }
    }

    // Use CENTER direction for released key, or a key flicked to a unsupported direction.
    return KeyEventContext.getKeyEntity(key, metaState, Flick.Direction.CENTER);
  }

  private static Drawable setDrawableState(Drawable drawable, boolean isPressed) {
    if (drawable != null) {
      drawable.setState(isPressed ? STATE_PRESSED : STATE_DEFAULT);
    }
    return drawable;
  }

  /**
   * Returns {@code Drawable} for the given key's background with setting appropriate state.
   * This method is package private just for testing purpose.
   */
  Drawable getKeyBackground(KeyEntity keyEntity, boolean isPressed) {
    if (keyEntity == null) {
      return null;
    }
    return setDrawableState(
        backgroundDrawableFactory.getDrawable(keyEntity.getKeyBackgroundDrawableType()),
        isPressed);
 }

  /**
   * Returns {@code Drawable} for the given key's icon with setting appropriate state.
   * This method is package private just for testing purpose.
   */
  static Drawable getKeyIcon(
      DrawableCache drawableCache, KeyEntity keyEntity, boolean isPressed) {
    if (keyEntity == null) {
      return null;
    }
    return setDrawableState(
        drawableCache.getDrawable(keyEntity.getKeyIconResourceId()), isPressed);
  }
}
