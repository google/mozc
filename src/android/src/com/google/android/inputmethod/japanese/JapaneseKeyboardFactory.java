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

package org.mozc.android.inputmethod.japanese;

import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.util.LeastRecentlyUsedCacheMap;

import android.content.res.Resources;
import android.content.res.Resources.NotFoundException;

import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.util.Map;

/**
 * Factory of the keyboard data based on xml.
 *
 */
public class JapaneseKeyboardFactory {

  /**
   * Key for the cache map of keyboard.
   *
   * Currently the keyboard is depending on its specification and display's size.
   */
  private static class CacheKey {
    private final KeyboardSpecification specification;
    private final int width;
    private final int height;

    CacheKey(KeyboardSpecification specification, int width, int height) {
      this.specification = specification;
      this.width = width;
      this.height = height;
    }

    @Override
    public boolean equals(Object obj) {
      if (obj instanceof CacheKey) {
        CacheKey other = CacheKey.class.cast(obj);
        return specification == other.specification &&
               width == other.width &&
               height == other.height;
      }
      return false;
    }

    @Override
    public int hashCode() {
      return (specification.hashCode() * 31 ^ width) * 31 ^ height;
    }
  }

  /**
   * The max size of cached keyboards. This is based on the max number of keyboard variation
   * for a configuration.
   */
  private static final int CACHE_SIZE = 6;

  private final Map<CacheKey, JapaneseKeyboard> cache =
      new LeastRecentlyUsedCacheMap<CacheKey, JapaneseKeyboard>(CACHE_SIZE);

  /**
   * @return JapaneseKeyboard instance based on given resources and specification.
   *         If it is already parsed, just returns cached one. Otherwise, tries to parse
   *         corresponding xml data, then caches and returns it.
   *         Returns {@code null} if parsing is failed.
   * @throws NullPointerException if given {@code resources} or {@code specification} is
   *         {@code null}.
   */
  public JapaneseKeyboard get(Resources resources, KeyboardSpecification specification,
                              int keyboardWidth, int keyboardHeight) {
    if (resources == null) {
      throw new NullPointerException("resources is null.");
    }
    if (specification == null) {
      throw new NullPointerException("specification is null.");
    }

    CacheKey cacheKey =
        new CacheKey(specification, keyboardWidth, keyboardHeight);

    // First, look up from the cache.
    JapaneseKeyboard keyboard = cache.get(cacheKey);
    if (keyboard == null) {
      // If not found, parse keyboard from a xml resource file. The result will be cached in
      // the cache map.
      keyboard = parseKeyboard(resources, specification, keyboardWidth, keyboardHeight);
      if (keyboard != null) {
        cache.put(cacheKey, keyboard);
      }
    }
    return keyboard;
  }

  private static JapaneseKeyboard parseKeyboard(
      Resources resources, KeyboardSpecification specification,
      int keyboardWidth, int keyboardHeight) {
    JapaneseKeyboardParser parser = new JapaneseKeyboardParser(
        resources, resources.getXml(specification.getXmlLayoutResourceId()), specification,
        keyboardWidth, keyboardHeight);
    try {
      return parser.parseKeyboard();
    } catch (NotFoundException e) {
      MozcLog.e(e.getMessage());
    } catch (XmlPullParserException e) {
      MozcLog.e(e.getMessage());
    } catch (IOException e) {
      MozcLog.e(e.getMessage());
    }

    // Returns null if failed.
    return null;
  }

  /**
   * Clears cached keyboards.
   */
  public void clear() {
    cache.clear();
  }
}
