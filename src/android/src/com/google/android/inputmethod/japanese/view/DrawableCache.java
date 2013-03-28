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

import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.view.MozcDrawableFactory;

import android.graphics.drawable.Drawable;

import java.util.HashMap;
import java.util.Map;

/**
 * Cache of android's Drawable instances.
 *
 */
public class DrawableCache {
  private final Map<Integer, Drawable> cacheMap = new HashMap<Integer, Drawable>(256);
  private final MozcDrawableFactory mozcDrawableFactory;
  private SkinType skinType = SkinType.ORANGE_LIGHTGRAY;

  public DrawableCache(MozcDrawableFactory mozcDrawableFactory) {
    this.mozcDrawableFactory = mozcDrawableFactory;
  }

  public void setSkinType(SkinType skinType) {
    if (this.skinType == skinType) {
      return;
    }

    this.skinType = skinType;
    cacheMap.clear();
    mozcDrawableFactory.setSkinType(skinType);
  }

  /**
   * First, looks up cache data in this instance, and returns the value if found.
   * If not found, tries to load {@code Drawable} instance from resources given via the constructor,
   * stores it into this instance, and returns it.
   */
  public Drawable getDrawable(int resourceId) {
    if (resourceId == 0) {
      // 0 is invalid resource id.
      return null;
    }

    Integer key = Integer.valueOf(resourceId);
    Drawable drawable = cacheMap.get(key);
    if (drawable == null) {
      drawable = mozcDrawableFactory.getDrawable(resourceId);
      if (drawable == null) {
        MozcLog.e("Failed to load: " + resourceId);
      }
      cacheMap.put(key, drawable);
    }
    return drawable;
  }

  /**
   * Clears all {@code Drawable}s stored in this instance.
   */
  public void clear() {
    cacheMap.clear();
  }
}
