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

import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.ModifierKey;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.SpecialKey;

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

  public void testGetMozcSpecialKeyEvent() {
    // Special key
    // Space
    assertEquals(
        SpecialKey.SPACE,
        KeycodeConverter.getMozcSpecialKeyEvent(
            new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_SPACE)).getSpecialKey());
    // Enter
    assertEquals(
        SpecialKey.ENTER,
        KeycodeConverter.getMozcSpecialKeyEvent(
            new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER)).getSpecialKey());
    // BackSpace
    assertEquals(
        SpecialKey.BACKSPACE,
        KeycodeConverter.getMozcSpecialKeyEvent(
            new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL)).getSpecialKey());
    // Left
    assertEquals(
        SpecialKey.LEFT,
        KeycodeConverter.getMozcSpecialKeyEvent(
            new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_LEFT)).getSpecialKey());
    // special + meta
    // Shift + Left
    assertEquals(
        SpecialKey.LEFT,
        KeycodeConverter.getMozcSpecialKeyEvent(
            new KeyEvent(
                0, 0, KeyEvent.ACTION_DOWN,
                KeyEvent.KEYCODE_DPAD_LEFT, 0, KeyEvent.META_SHIFT_ON))
            .getSpecialKey());
    assertEquals(
        ModifierKey.SHIFT,
        KeycodeConverter.getMozcSpecialKeyEvent(
            new KeyEvent(
                0, 0, KeyEvent.ACTION_DOWN,
                KeyEvent.KEYCODE_DPAD_LEFT, 0, KeyEvent.META_SHIFT_ON))
            .getModifierKeys(0));
    // Ctrl + Del.
    ProtoCommands.KeyEvent keyEvent = KeycodeConverter.getMozcSpecialKeyEvent(
        new KeyEvent(
            0, 0, KeyEvent.ACTION_DOWN,
            KeyEvent.KEYCODE_DEL, 0, KeyEvent.META_CTRL_ON));
    assertEquals(SpecialKey.BACKSPACE, keyEvent.getSpecialKey());
    assertEquals(ModifierKey.CTRL, keyEvent.getModifierKeys(0));
    // Keys not to be converted
    assertNull(KeycodeConverter.getMozcSpecialKeyEvent(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A)));
    assertNull(KeycodeConverter.getMozcSpecialKeyEvent(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_HOME)));
    assertNull(KeycodeConverter.getMozcSpecialKeyEvent(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MENU)));
    assertNull(KeycodeConverter.getMozcSpecialKeyEvent(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_SEARCH)));
  }

  public void testIsSystemKey() {
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_3D_MODE)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_APP_SWITCH)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_AVR_INPUT)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_AVR_POWER)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BOOKMARK)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_CALL)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_CAMERA)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_CHANNEL_DOWN)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_CHANNEL_UP)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DVR)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_FOCUS)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_HEADSETHOOK)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_HOME)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_INFO)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_LANGUAGE_SWITCH)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MANNER_MODE)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MEDIA_CLOSE)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MEDIA_EJECT)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MEDIA_FAST_FORWARD)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MEDIA_NEXT)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MEDIA_PAUSE)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MEDIA_PLAY)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MEDIA_PREVIOUS)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MEDIA_RECORD)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MEDIA_REWIND)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MEDIA_STOP)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MENU)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MUTE)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_NOTIFICATION)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_POWER)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_PROG_BLUE)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_PROG_GREEN)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_PROG_RED)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_PROG_YELLOW)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_SEARCH)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_SYSRQ)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TV)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TV_INPUT)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TV_POWER)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_VOLUME_DOWN)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_VOLUME_MUTE)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_VOLUME_UP)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ZOOM_IN)));
    assertTrue(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ZOOM_OUT)));

    assertFalse(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A)));
    // BlueTooth Hardware Keyboard Event may have KeyEvent.KEYCODE_UNKNOWN.
    assertFalse(KeycodeConverter.isSystemKey(
        new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_UNKNOWN)));
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
