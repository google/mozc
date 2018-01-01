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

package org.mozc.android.inputmethod.japanese.hardwarekeyboard;

import android.annotation.SuppressLint;
import android.os.Build;
import android.util.SparseIntArray;
import android.view.KeyEvent;

/**
 * Processes overlay key-layout mapping and key-character mapping.
 *
 * <p>Key-layout mapping is done only in the framework so this class overlays additional
 * behavior.
 * <p>Key-character mapping can be done by a user but it is applicable since API Level 16.
 * This class does overlay mapping for all API Level.
 * <p>Tested by HardwareKeyboardSpecificationTest.
 */
class KeyEventMapperFactory {

  /**
   * Applies key-layout and key-character mapping.
   */
  public interface KeyEventMapper {
    void applyMapping(CompactKeyEvent keyEvent);
  }

  public static final KeyEventMapper DEFAULT_KEYBOARD_MAPPER = new DefaultKeyboardMapper();
  public static final KeyEventMapper JAPANESE_KEYBOARD_MAPPER = new JapaneseKeyboardMapper();

  /**
   * Basically does nothing.
   * For older OS missing key-layout mapping is done.
   */
  private static class DefaultKeyboardMapper implements KeyEventMapper {
    @Override
    public void applyMapping(CompactKeyEvent keyEvent) {
      keyEvent.setKeyCode(doKeyLayoutMappingForOldAndroids(keyEvent.getKeyCode(),
                                                           keyEvent.getScanCode()));
    }
  }

  /**
   * Key-layout and key-character mapping for Japanese keyboard.
   */
  @SuppressLint("InlinedApi")
  private static class JapaneseKeyboardMapper implements KeyEventMapper {

    static final SparseIntArray unshiftedMap;
    static final SparseIntArray shiftedMap;

    static {
      SparseIntArray tempShiftedMap = new SparseIntArray();
      // Row #1
      tempShiftedMap.put(KeyEvent.KEYCODE_1, '!');
      tempShiftedMap.put(KeyEvent.KEYCODE_2, '"');
      tempShiftedMap.put(KeyEvent.KEYCODE_3, '#');
      tempShiftedMap.put(KeyEvent.KEYCODE_4, '$');
      tempShiftedMap.put(KeyEvent.KEYCODE_5, '%');
      tempShiftedMap.put(KeyEvent.KEYCODE_6, '&');
      tempShiftedMap.put(KeyEvent.KEYCODE_7, '\'');
      tempShiftedMap.put(KeyEvent.KEYCODE_8, '(');
      tempShiftedMap.put(KeyEvent.KEYCODE_9, ')');
      tempShiftedMap.put(KeyEvent.KEYCODE_0, 0);  // No character
      tempShiftedMap.put(KeyEvent.KEYCODE_MINUS, '=');
      tempShiftedMap.put(KeyEvent.KEYCODE_EQUALS, '~');
      tempShiftedMap.put(KeyEvent.KEYCODE_YEN, '|');
      // Row #2
      tempShiftedMap.put(KeyEvent.KEYCODE_LEFT_BRACKET, '`');
      tempShiftedMap.put(KeyEvent.KEYCODE_RIGHT_BRACKET, '{');
      // Row #3
      tempShiftedMap.put(KeyEvent.KEYCODE_SEMICOLON, '+');
      tempShiftedMap.put(KeyEvent.KEYCODE_APOSTROPHE, '*');
      tempShiftedMap.put(KeyEvent.KEYCODE_BACKSLASH, '}');
      // Row #4
      tempShiftedMap.put(KeyEvent.KEYCODE_COMMA, '<');
      tempShiftedMap.put(KeyEvent.KEYCODE_PERIOD, '>');
      tempShiftedMap.put(KeyEvent.KEYCODE_SLASH, '?');
      tempShiftedMap.put(KeyEvent.KEYCODE_RO, '_');
      shiftedMap = tempShiftedMap;

      SparseIntArray tempUnshiftedMap = new SparseIntArray();
      // Row #1
      tempUnshiftedMap.put(KeyEvent.KEYCODE_EQUALS, '^');
      tempUnshiftedMap.put(KeyEvent.KEYCODE_YEN, '\u00a5');  // Yen mark
      // Row #2
      tempUnshiftedMap.put(KeyEvent.KEYCODE_LEFT_BRACKET, '@');
      tempUnshiftedMap.put(KeyEvent.KEYCODE_RIGHT_BRACKET, '[');
      // Row #3
      tempUnshiftedMap.put(KeyEvent.KEYCODE_APOSTROPHE, ':');
      tempUnshiftedMap.put(KeyEvent.KEYCODE_BACKSLASH, ']');
      // Row #4
      tempUnshiftedMap.put(KeyEvent.KEYCODE_RO, '\\');
      unshiftedMap = tempUnshiftedMap;
    }

    /**
     * Key-character mapping.
     * Ideally this should be done by preinstalled .kcm file or
     * application injected .kcm.
     */
    private static void doKeyCharacterMapping(CompactKeyEvent keyEvent) {
      boolean isShifted = (keyEvent.getMetaState() & KeyEvent.META_SHIFT_MASK) != 0;
      int keyCode = keyEvent.getKeyCode();
      // Special handling for GRAVE, which should be mapped into ZEN/HAN key with no character.
      if (keyCode == KeyEvent.KEYCODE_GRAVE) {
        keyEvent.setKeyCode(KeyEvent.KEYCODE_ZENKAKU_HANKAKU);
        keyEvent.setUnicodeCharacter(0);
        return;
      }
      SparseIntArray mapping = isShifted ? shiftedMap : unshiftedMap;
      keyEvent.setUnicodeCharacter(mapping.get(keyCode, keyEvent.getUnicodeCharacter()));
    }

    @Override
    public void applyMapping(CompactKeyEvent keyEvent) {
      keyEvent.setKeyCode(doKeyLayoutMappingForOldAndroids(keyEvent.getKeyCode(),
                                                           keyEvent.getScanCode()));
      doKeyCharacterMapping(keyEvent);
    }
  }
  /**
   * Does .kl file's behavior.
   *
   * Should be applied for all the keyboard type.
   */
  @SuppressLint("InlinedApi")
  private static int doKeyLayoutMappingForOldAndroids(int keyCode, int scanCode) {
    if (Build.VERSION.SDK_INT >= 16) {
      return keyCode;
    }
    // c.f., base/data/keyboards/Generic.kl
    switch (scanCode) {
      case 85:
        return KeyEvent.KEYCODE_ZENKAKU_HANKAKU;
      case 89:
        return KeyEvent.KEYCODE_RO;
      case 92:
        return KeyEvent.KEYCODE_HENKAN;
      case 93:
        return KeyEvent.KEYCODE_KATAKANA_HIRAGANA;
      case 94:
        return KeyEvent.KEYCODE_MUHENKAN;
      case 122:
        return KeyEvent.KEYCODE_KANA;
      case 123:
        return KeyEvent.KEYCODE_EISU;
      case 124:
        return KeyEvent.KEYCODE_YEN;
      default:
        return keyCode;
    }
  }
}
