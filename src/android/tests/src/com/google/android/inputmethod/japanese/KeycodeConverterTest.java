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

package org.mozc.android.inputmethod.japanese;

import android.view.KeyEvent;

import junit.framework.TestCase;

/**
 */
public class KeycodeConverterTest extends TestCase {
  public void testGetMozcKey() {
    for (int i = 0; i < 128; ++i) {
      assertEquals(i, KeycodeConverter.getMozcKeyEvent(i).getKeyCode());
    }
  }

  public void testIsMetaKey() {
    assertFalse(KeycodeConverter.isMetaKey(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A)));
    assertTrue(KeycodeConverter.isMetaKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_SHIFT_LEFT)));
    assertTrue(KeycodeConverter.isMetaKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_SHIFT_RIGHT)));
    assertTrue(KeycodeConverter.isMetaKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_CTRL_LEFT)));
    assertTrue(KeycodeConverter.isMetaKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_CTRL_RIGHT)));
    assertTrue(KeycodeConverter.isMetaKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ALT_LEFT)));
    assertTrue(KeycodeConverter.isMetaKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ALT_RIGHT)));
  }
}
