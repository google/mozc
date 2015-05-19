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

import org.mozc.android.inputmethod.japanese.MemoryManageable;
import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import com.google.common.base.Preconditions;

import android.content.res.Resources;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.util.SparseArray;

/**
 * Factory to create Japanese carrier dependent Emoji drawables.
 *
 * This manages the (vector based) Emoji drawables. The managed resources should be placed
 * at "res/raw" directory, and its name should have prefix:
 * - For docomo: docomo_emoji_XXXXX
 * - For kddi: kddi_emoji_XXXXX, and
 * - For softbank: softbank_emoji_XXXXX
 * where XXX is five characters hex formatted code point (in pua),
 * used for Emoji code point in Android.
 *
 * In order to reduce IO, this class caches the created Drawables.
 *
 */
class EmojiDrawableFactory implements MemoryManageable {

  /**
   * Dummy {@code Drawable} instance to represent {@code null} inside the {@code cacheMap}.
   * We can put {@code null} directly into the cache,
   * but unfortunately {@code get} returns {@code null} when:
   * 1) the value is actually null, or
   * 2) the key is not contained in the map,
   * and we have no way to check either one.
   * Thus, instead of putting {@code null} directly, put {@code NULL_DRAWABLE} to represent
   * 1)-case above, so that we don't need to search in the map twice.
   */
  private static final Drawable NULL_DRAWABLE = new ColorDrawable();

  static final int MIN_EMOJI_PUA_CODE_POINT = 0xFE000;
  static final int MAX_EMOJI_PUA_CODE_POINT = 0xFEEA0;

  private final Resources resources;
  private final MozcDrawableFactory factory;
  // Initial size is zero for memory performance.
  // We can prepare large array for cache but it is useless for the uses who don't want Emoji.
  private final SparseArray<Drawable> cacheMap = new SparseArray<Drawable>(0);
  private EmojiProviderType providerType = EmojiProviderType.NONE;

  EmojiDrawableFactory(Resources resources) {
    this.resources = resources;
    this.factory = new MozcDrawableFactory(resources);
  }

  void setProviderType(EmojiProviderType providerType) {
    Preconditions.checkNotNull(providerType);

    if (this.providerType != providerType) {
      cacheMap.clear();
    }
    this.providerType = providerType;
  }

  Drawable getDrawable(int codePoint) {
    if (!isInEmojiPuaRange(codePoint)) {
      // Non-emoji pua code point.
      return null;
    }

    Integer codePointKey = Integer.valueOf(codePoint);
    Drawable result = cacheMap.get(codePointKey);
    if (result == null) {
      // Not found in the cache. So create Drawable resource.
      // We use resource 'name' for this purpose instead of having a table of resource 'id's
      // internally. This is because
      // 1) this is just to help users using a device which doesn't support emoji natively,
      //    but some application (e.g. carrier mailer) supports in application level.
      // 2) we don't expect users would switch the provider so often, thus almost Drawable
      //    resources should be in the cache.
      // So, we look up by name by paying runtime cost, in order to keep out from unneeded
      // tables from apk file.
      int resourceId = getIdentifier(resources, providerType, codePoint);
      if (resourceId != 0) {
        result = factory.getDrawable(resourceId);
      }

      if (result == null) {
        // Put NULL_DRAWABLE instead of null directly.
        cacheMap.put(codePointKey, NULL_DRAWABLE);
      } else {
        cacheMap.put(codePointKey, result);
      }
    } else {
      // Hit the cache. Just use it.
      if (result == NULL_DRAWABLE) {
        // Unwrapping NULL_DRAWABLE.
        result = null;
      }
    }

    return result;
  }

  /** @return {@code true} if the drawable resource is available. */
  boolean hasDrawable(int codePoint) {
    if (!isInEmojiPuaRange(codePoint)) {
      // Non-Emoji PUA code point.
      return false;
    }

    Integer codePointKey = Integer.valueOf(codePoint);
    Drawable cachedDrawable = cacheMap.get(codePointKey);
    if (cachedDrawable != null) {
      return cachedDrawable != NULL_DRAWABLE;
    }

    int resourceId = getIdentifier(resources, providerType, codePoint);
    if (resourceId == 0) {
      // Remember the code point is invalid.
      cacheMap.put(codePointKey, NULL_DRAWABLE);
      return false;
    }

    return true;
  }

  /**
   * @return {@code true} if the given {@code codePoint} is in the emoji pua range.
   *   Note that the current system may not support codepoint nor the package
   *   may not have the corresponding drawable resource for the codepoint,
   *   even if this method returns {@code true}.
   */
  static boolean isInEmojiPuaRange(int codePoint) {
    return MIN_EMOJI_PUA_CODE_POINT <= codePoint && codePoint <= MAX_EMOJI_PUA_CODE_POINT;
  }

  /** @return the resource id for the code point. */
  private static int getIdentifier(
      Resources resources, EmojiProviderType providerType, int codePoint) {
    Preconditions.checkNotNull(providerType);

    return resources.getIdentifier(
        toResourceName(providerType, codePoint),
        "raw", "org.mozc.android.inputmethod.japanese");
  }

  /** @return the resource name for the code point. */
  private static String toResourceName(EmojiProviderType providerType, int codePoint) {
    Preconditions.checkNotNull(providerType);

    switch (providerType) {
      case DOCOMO:
        return "docomo_emoji_" + Integer.toHexString(codePoint);
      case SOFTBANK:
        return "softbank_emoji_" + Integer.toHexString(codePoint);
      case KDDI:
        return "kddi_emoji_" + Integer.toHexString(codePoint);
      default:
        MozcLog.e("Unknown providerType: " + providerType.name());
    }
    return null;
  }

  @Override
  public void trimMemory() {
    cacheMap.clear();
  }
}
