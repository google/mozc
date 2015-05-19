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

import android.graphics.drawable.Drawable;

import java.lang.ref.ReferenceQueue;
import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Map;

/**
 * Simple cache implementation of {@code Drawable}.
 * This class uses WeakReference mechanism. So, drawables in this instance will be
 * released when gc runs and there are no strong references for them.
 *
 */
public class WeakDrawableCache {
  private static class WeakEntry extends WeakReference<Drawable> {
    final Integer key;
    WeakEntry(Integer key, Drawable value, ReferenceQueue<? super Drawable> queue) {
      super(value, queue);
      this.key = key;
    }
  }

  private final ReferenceQueue<Drawable> queue = new ReferenceQueue<Drawable>();
  private final Map<Integer, WeakEntry> map = new HashMap<Integer, WeakEntry>();

  private void cleanUp() {
    while (true) {
      WeakEntry reference = WeakEntry.class.cast(queue.poll());
      if (reference == null) {
        return;
      }
      map.remove(reference.key);
    }
  }

  /**
   * Put the {@code drawable} to this cache whose resource id is {@code key}.
   */
  public void put(Integer key, Drawable value) {
    cleanUp();
    WeakEntry entry = map.put(key, new WeakEntry(key, value, queue));
  }

  /**
   * Returns {@code Drawable} instance for the {@code key}, or {@code null} if this doesn't
   * contain the corresponding {@code Drawable}.
   */
  public Drawable get(Integer key) {
    cleanUp();
    WeakEntry entry = map.get(key);
    return entry == null ? null : entry.get();
  }

  /**
   * Clears the cache content.
   */
  public void clear() {
    map.clear();

    // Clear the queue.
    while (true) {
      if (queue.poll() == null) {
        break;
      }
    }
  }
}
