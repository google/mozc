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

package org.mozc.android.inputmethod.japanese.util;

import junit.framework.TestCase;

import java.util.Map;

/**
 */
public class LeastRecentlyUsedCacheMapTest extends TestCase {
  public void testLeastRecentlyUsedCacheMap() {
    // Set max entry size to 5.
    Map<Integer, Integer> cacheMap = new LeastRecentlyUsedCacheMap<Integer, Integer>(5);

    // Put 0, 1, 2, 3 and 4 to the map.
    for (int i = 0; i < 5; ++i) {
      assertEquals(i, cacheMap.size());
      cacheMap.put(i, i);
      assertEquals(i + 1, cacheMap.size());
    }

    // Look up each element.
    for (int i = 0; i < 5; ++i) {
      assertEquals(Integer.valueOf(i), cacheMap.get(i));
    }

    // Put 5 to the map. Then 0, which is least recently used element, should be removed.
    cacheMap.put(5, 5);
    assertEquals(5, cacheMap.size());
    assertFalse(cacheMap.containsKey(0));

    // Then access 5, 4, 3, 2 and 1.
    for (int i = 5; i >= 1; --i) {
      assertEquals(Integer.valueOf(i), cacheMap.get(i));
    }

    // Put 6 to the map, then 5, which is least recently used element here, should be removed.
    cacheMap.put(6, 6);
    assertEquals(5, cacheMap.size());
    assertFalse(cacheMap.containsKey(5));
  }
}
