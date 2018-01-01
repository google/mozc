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

package org.mozc.android.inputmethod.japanese.preference;

import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.keyboard.Keyboard;
import org.mozc.android.inputmethod.japanese.keyboard.Keyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.keyboard.KeyboardParser;
import org.mozc.android.inputmethod.japanese.keyboard.KeyboardViewBackgroundSurface;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.KeyboardLayout;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.view.DrawableCache;
import org.mozc.android.inputmethod.japanese.view.Skin;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;

import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.util.Collections;
import java.util.EnumMap;
import java.util.Map;
import java.util.WeakHashMap;

import javax.annotation.Nullable;

/**
 * A Drawable to render keyboard preview.
 *
 */
public class KeyboardPreviewDrawable extends Drawable {

  /**
   * The key of the activity which uses the Bitmap cache.
   *
   * In order to utilize memories, it is necessary to tell
   * to the BitmapCache what activities use it now.
   * The current heuristic is register the activity in its onStart
   * and unregister it in its onStop.
   * In theory, it should work well as long as the onStop
   * corresponding to the already-invoked onStart is invoked without errors.
   *
   * As the last resort, we use a finalizer guardian.
   * The instance of the key should be referred only by an Activity instance
   * in its field. Then:
   * - the activity can register and unregister in its onStart/onStop methods.
   * - Even if onStop is NOT invoked accidentally, the Activity will be
   *   collected by GC, and the key is also collected at the same time.
   *   Then the finalize of the key will be invoked, and it will unregister
   *   itself from Bitmap cache.
   * - Overriding the finalizer may cause the delay of memory collecting.
   *   However, the CacheReferenceKey is small enough (at least compared to
   *   Activity as the Activity refers many other instances, too). So the
   *   risk (or damage) of the remaining phantom instances should be low enough.
   * Note: Regardless of the finalizer, if onStop is correctly invoked, the bitmap
   * cache will be released correctly.
   */
  static class CacheReferenceKey {

    @Override
    protected void finalize() throws Throwable {
      BitmapCache.getInstance().removeReference(this);
      super.finalize();
    }
  }

  /**
   * Global cache of bitmap for the keyboard preview.
   *
   * Assuming that the size of each bitmap preview is same, and skin type is globally unique,
   * we can use global bitmap cache to keep the memory usage low.
   * This cache also manages the referencing Activities. See {@link MozcBasePreferenceActivity}
   * for the details.
   */
  static class BitmapCache {

    private static final BitmapCache INSTANCE = new BitmapCache();
    private static final Object DUMMY_VALUE = new Object();

    private final Map<KeyboardLayout, Bitmap> map =
        new EnumMap<KeyboardLayout, Bitmap>(KeyboardLayout.class);
    private Skin skin = Skin.getFallbackInstance();
    private final WeakHashMap<CacheReferenceKey, Object> referenceMap =
        new WeakHashMap<CacheReferenceKey, Object>();

    private BitmapCache() {
    }

    static BitmapCache getInstance() {
      return INSTANCE;
    }

    @Nullable Bitmap get(KeyboardLayout keyboardLayout, int width, int height, Skin skin) {
      Preconditions.checkNotNull(skin);
      if (keyboardLayout == null || width <= 0 || height <= 0) {
        return null;
      }

      if (!skin.equals(this.skin)) {
        return null;
      }

      Bitmap result = map.get(keyboardLayout);
      if (result != null) {
        // Check the size.
        if (result.getWidth() != width || result.getHeight() != height) {
          result = null;
        }
      }

      return result;
    }

    void put(@Nullable KeyboardLayout keyboardLayout, Skin skin, @Nullable Bitmap bitmap) {
      Preconditions.checkNotNull(skin);
      if (keyboardLayout == null || bitmap == null) {
        return;
      }

      if (!skin.equals(this.skin)) {
        clear();
        this.skin = skin;
      }

      Bitmap oldBitmap = map.put(keyboardLayout, bitmap);
      if (oldBitmap != null) {
        // Recycle old bitmap if exists.
        oldBitmap.recycle();
      }
    }

    void addReference(CacheReferenceKey key) {
      referenceMap.put(key, DUMMY_VALUE);
    }

    void removeReference(CacheReferenceKey key) {
      referenceMap.remove(key);
      if (referenceMap.isEmpty()) {
        // When all referring activities are gone, we don't need keep the cache.
        // To reduce the memory usage, release all the cached bitmap.
        clear();
      }
    }

    private void clear() {
      for (Bitmap bitmap : map.values()) {
        if (bitmap != null) {
          bitmap.recycle();
        }
      }

      map.clear();
      skin = Skin.getFallbackInstance();
    }
  }

  private final Resources resources;
  private final KeyboardLayout keyboardLayout;
  private final KeyboardSpecification specification;
  private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);

  private Skin skin = Skin.getFallbackInstance();
  private boolean enabled = true;

  KeyboardPreviewDrawable(
      Resources resources, KeyboardLayout keyboardLayout, KeyboardSpecification specification) {
    this.resources = resources;
    this.keyboardLayout = keyboardLayout;
    this.specification = specification;
  }

  @Override
  public void draw(Canvas canvas) {
    Rect bounds = getBounds();
    if (bounds.isEmpty()) {
      return;
    }

    // Look up cache.
    BitmapCache cache = BitmapCache.getInstance();
    Bitmap bitmap = cache.get(keyboardLayout, bounds.width(), bounds.height(), skin);
    if (bitmap == null) {
      bitmap = createBitmap(
          resources, specification, bounds.width(), bounds.height(),
          resources.getDimensionPixelSize(R.dimen.pref_inputstyle_reference_width), skin);
      if (bitmap != null) {
        cache.put(keyboardLayout, skin, bitmap);
      }
    }

    canvas.drawBitmap(bitmap, bounds.left, bounds.top, paint);

    if (!enabled) {
      // To represent disabling, gray it out.
      Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
      paint.setColor(0x80000000);
      canvas.drawRect(bounds, paint);
    }
  }

  /**
   * @param width width of returned {@code Bitmap}
   * @param height height of returned {@code Bitmap}
   * @param virtualWidth virtual width of keyboard. This value is used when rendering.
   *        virtualHeight is internally calculated based on given arguments keeping aspect ratio.
   */
  @Nullable
  private static Bitmap createBitmap(
      Resources resources, KeyboardSpecification specification, int width, int height,
      int virtualWidth, Skin skin) {
    Preconditions.checkNotNull(skin);
    // Scaling is required because some icons are draw with specified fixed size.
    float scale = width / (float) virtualWidth;
    int virtualHeight = (int) (height / scale);
    Keyboard keyboard = getParsedKeyboard(resources, specification, virtualWidth, virtualHeight);
    if (keyboard == null) {
      return null;
    }

    Bitmap bitmap = Bitmap.createBitmap(width, height, Config.ARGB_8888);
    Canvas canvas = new Canvas(bitmap);
    canvas.scale(scale, scale);
    DrawableCache drawableCache = new DrawableCache(resources);
    drawableCache.setSkin(skin);

    // Fill background.
    {
      Drawable keyboardBackground =
          skin.windowBackgroundDrawable.getConstantState().newDrawable();
      keyboardBackground.setBounds(0, 0, virtualWidth, virtualHeight);
      keyboardBackground.draw(canvas);
    }

    // Draw keyboard layout.
    {
      BackgroundDrawableFactory backgroundDrawableFactory =
          new BackgroundDrawableFactory(resources);
      backgroundDrawableFactory.setSkin(skin);
      KeyboardViewBackgroundSurface backgroundSurface =
          new KeyboardViewBackgroundSurface(backgroundDrawableFactory, drawableCache);
      backgroundSurface.reset(Optional.of(keyboard), Collections.<MetaState>emptySet());
      backgroundSurface.draw(canvas);
    }

    return bitmap;
  }

  /** Create a Keyboard instance which fits the current bitmap. */
  @Nullable
  private static Keyboard getParsedKeyboard(
      Resources resources, KeyboardSpecification specification, int width, int height) {
    KeyboardParser parser = new KeyboardParser(
        resources, width, height, specification);
    try {
      return parser.parseKeyboard();
    } catch (XmlPullParserException e) {
      MozcLog.e("Failed to parse keyboard layout: ", e);
    } catch (IOException e) {
      MozcLog.e("Failed to parse keyboard layout: ", e);
    }

    return null;
  }

  void setSkin(Skin skin) {
    this.skin = skin;
    invalidateSelf();
  }

  @Override
  public int getOpacity() {
    return PixelFormat.TRANSLUCENT;
  }

  @Override
  public void setAlpha(int alpha) {
    // Do nothing.
  }

  @Override
  public void setColorFilter(ColorFilter cf) {
    // Do nothing.
  }

  // Hard coded size to adapt the old implementation.
  @Override
  public int getIntrinsicWidth() {
    return resources.getDimensionPixelSize(R.dimen.pref_inputstyle_image_width);
  }

  @Override
  public int getIntrinsicHeight() {
    return resources.getDimensionPixelSize(R.dimen.pref_inputstyle_image_width) * 348 / 480;
  }

  @Override
  public boolean isStateful() {
    return true;
  }

  @Override
  protected boolean onStateChange(int[] state) {
    boolean enabled = isEnabled(state);
    if (this.enabled == enabled) {
      return false;
    }
    this.enabled = enabled;
    invalidateSelf();
    return true;
  }

  private static boolean isEnabled(int[] state) {
    for (int i = 0; i < state.length; ++i) {
      if (state[i] == android.R.attr.state_enabled) {
        return true;
      }
    }
    return false;
  }
}
