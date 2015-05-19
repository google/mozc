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

package org.mozc.android.inputmethod.japanese.preference;

import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory;
import org.mozc.android.inputmethod.japanese.keyboard.Keyboard;
import org.mozc.android.inputmethod.japanese.keyboard.KeyboardParser;
import org.mozc.android.inputmethod.japanese.keyboard.KeyboardViewBackgroundSurface;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.KeyboardLayout;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.view.DrawableCache;
import org.mozc.android.inputmethod.japanese.view.MozcDrawableFactory;
import org.mozc.android.inputmethod.japanese.view.SkinType;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.Bitmap.Config;
import android.graphics.Shader.TileMode;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.util.EnumMap;
import java.util.Map;
import java.util.WeakHashMap;

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
    private static BitmapCache INSTANCE = new BitmapCache();
    private static Object DUMMY_VALUE = new Object();

    private final Map<KeyboardLayout, Bitmap> map =
        new EnumMap<KeyboardLayout, Bitmap>(KeyboardLayout.class);
    private SkinType skinType = null;
    private final WeakHashMap<CacheReferenceKey, Object> referenceMap =
        new WeakHashMap<CacheReferenceKey, Object>();

    private BitmapCache() {
    }

    static BitmapCache getInstance() {
      return INSTANCE;
    }

    Bitmap get(KeyboardLayout keyboardLayout, int width, int height, SkinType skinType) {
      if (keyboardLayout == null || width <= 0 || height <= 0 || skinType == null) {
        return null;
      }

      if (skinType != this.skinType) {
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

    void put(KeyboardLayout keyboardLayout, SkinType skinType, Bitmap bitmap) {
      if (keyboardLayout == null || skinType == null || bitmap == null) {
        return;
      }

      if (skinType != this.skinType) {
        clear();
        this.skinType = skinType;
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
      skinType = null;
    }
  }

  private final Resources resources;
  private final KeyboardLayout keyboardLayout;
  private final int keyboardResourceId;
  private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);

  private SkinType skinType = SkinType.BLUE_LIGHTGRAY;
  private boolean enabled = true;

  KeyboardPreviewDrawable(
      Resources resources, KeyboardLayout keyboardLayout, int keyboardResourceId) {
    this.resources = resources;
    this.keyboardLayout = keyboardLayout;
    this.keyboardResourceId = keyboardResourceId;
  }

  @Override
  public void draw(Canvas canvas) {
    Rect bounds = getBounds();
    if (bounds.isEmpty()) {
      return;
    }

    // Look up cache.
    BitmapCache cache = BitmapCache.getInstance();
    Bitmap bitmap = cache.get(keyboardLayout, bounds.width(), bounds.height(), skinType);
    if (bitmap == null) {
      bitmap = createBitmap(
          resources, keyboardResourceId, bounds.width(), bounds.height(), skinType);
      if (bitmap != null) {
        cache.put(keyboardLayout, skinType, bitmap);
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

  private static Bitmap createBitmap(
      Resources resources, int resourceId, int width, int height, SkinType skinType) {
    Keyboard keyboard = getParsedKeyboard(resources, resourceId, width, height);
    if (keyboard == null) {
      return null;
    }

    Bitmap bitmap = MozcUtil.createBitmap(width, height, Config.ARGB_8888);
    Canvas canvas = new Canvas(bitmap);
    DrawableCache drawableCache = new DrawableCache(new MozcDrawableFactory(resources));
    drawableCache.setSkinType(skinType);

    // Fill background.
    {
      Drawable keyboardBackground =
          drawableCache.getDrawable(skinType.windowBackgroundResourceId);
      if (keyboardBackground == null) {
        // Default black drawing.
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setColor(0xFF000000);
        canvas.drawRect(0, 0, bitmap.getWidth(), bitmap.getHeight(), paint);
      } else {
        if (keyboardBackground instanceof BitmapDrawable) {
          // If the background is bitmap resource, set repeat mode.
          BitmapDrawable.class.cast(keyboardBackground).setTileModeXY(
              TileMode.REPEAT, TileMode.REPEAT);
        }
        keyboardBackground.setBounds(0, 0, bitmap.getWidth(), bitmap.getHeight());
        keyboardBackground.draw(canvas);
      }
    }

    // Draw keyboard layout.
    {
      BackgroundDrawableFactory backgroundDrawableFactory =
          new BackgroundDrawableFactory(resources.getDisplayMetrics().density);
      backgroundDrawableFactory.setSkinType(skinType);
      KeyboardViewBackgroundSurface backgroundSurface =
          new KeyboardViewBackgroundSurface(backgroundDrawableFactory, drawableCache);
      backgroundSurface.requestUpdateKeyboard(keyboard, MetaState.UNMODIFIED);
      backgroundSurface.requestUpdateSize(bitmap.getWidth(), bitmap.getHeight());
      backgroundSurface.update();
      backgroundSurface.draw(canvas);
      backgroundSurface.reset();  // Release the background bitmap and its canvas.
    }

    return bitmap;
  }

  /** Create a Keyboard instance which fits the current bitmap. */
  private static Keyboard getParsedKeyboard(
      Resources resources, int resourceId, int width, int height) {
    KeyboardParser parser = new KeyboardParser(
        resources, resources.getXml(resourceId), width, height);
    try {
      return parser.parseKeyboard();
    } catch (XmlPullParserException e) {
      MozcLog.e("Failed to parse keyboard layout: ", e);
    } catch (IOException e) {
      MozcLog.e("Failed to parse keyboard layout: ", e);
    }

    return null;
  }

  void setSkinType(SkinType skinType) {
    this.skinType = skinType;
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
