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

package org.mozc.android.inputmethod.japanese.mushroom;

import android.util.SparseArray;

/**
 * Simple map object to proxy the result from Mushroom application.
 *
 * Background: MozcService is the {@code Service} instance but Mushroom application is
 * {@code Activity}, and its protocol to tell the result of the activity uses
 * "result" of Activity stack.
 * Thus, we need to tell it to ImeService somehow. Unfortunately, there seems
 * no good way, so we use global map instance for the proxy.
 *
 * This class is a singleton.
 *
 */
public class MushroomResultProxy {
  private static final MushroomResultProxy INSTANCE = new MushroomResultProxy();

  /** A map from fieldId to mushroom result. */
  private final SparseArray<String> replaceKeyMap = new SparseArray<String>();
  private MushroomResultProxy() {
  }

  /**
   * @return the singleton instance of this class.
   */
  public static MushroomResultProxy getInstance() {
    return INSTANCE;
  }

  /**
   * Registers the {@code replaceKey} which should be filled into the field with the given
   * {@fieldId}.
   */
  public void addReplaceKey(int fieldId, String replaceKey) {
    replaceKeyMap.put(fieldId, replaceKey);
  }

  /**
   * @return the String to be filled in the field with the given {@code fieldId} if available.
   *   Otherwise, {@code null}.
   */
  public String getReplaceKey(int fieldId) {
    return replaceKeyMap.get(fieldId);
  }

  /**
   * Clears all the data in the instance.
   */
  public void clear() {
    replaceKeyMap.clear();
  }
}
