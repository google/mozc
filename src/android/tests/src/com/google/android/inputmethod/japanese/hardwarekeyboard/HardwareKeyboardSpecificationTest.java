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

import org.mozc.android.inputmethod.japanese.KeycodeConverter.KeyEventInterface;
import org.mozc.android.inputmethod.japanese.hardwarekeyboard.HardwareKeyboard.CompositionSwitchMode;
import org.mozc.android.inputmethod.japanese.keyboard.Keyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.HardwareKeyMap;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.ModifierKey;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.SpecialKey;
import org.mozc.android.inputmethod.japanese.testing.MozcPreferenceUtil;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import com.google.common.base.Optional;

import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.os.Build;
import android.test.InstrumentationTestCase;
import android.test.MoreAsserts;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.InputDevice;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 *
 */
public class HardwareKeyboardSpecificationTest extends InstrumentationTestCase {

  @SmallTest
  public void testIsPrintable() {
    int[] printables = {'a', ' ', '猫', '☀'};
    int[] nonPrintables = {'\t', '\n', '\0', '\uFFFF'};

    for (int codepoint : printables) {
      assertTrue(String.format("%d is printable.", codepoint),
                 HardwareKeyboardSpecification.isPrintable(codepoint));
    }
    for (int codepoint : nonPrintables) {
      assertFalse(String.format("%d is not printable.", codepoint),
                 HardwareKeyboardSpecification.isPrintable(codepoint));
    }
  }

  @SmallTest
  public void testDefaultConverter() {
    HardwareKeyboardSpecification keyboardSpecification =
        HardwareKeyboardSpecification.DEFAULT;

    class KeyEventMock extends KeyEvent {
      private final int unicode;
      KeyEventMock(int action, int keycode, int meta, int unicode) {
        super(0, 0, action, keycode, 0, meta);
        this.unicode = unicode;
      }
      @Override
      public int getUnicodeChar() {
        return unicode;
      }
    }

    {
      KeyEvent keyEvent = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A);
      keyEvent.setSource(InputDevice.SOURCE_KEYBOARD);

      ProtoCommands.KeyEvent mozcKeyEvent = keyboardSpecification.getMozcKeyEvent(keyEvent);
      assertEquals(keyEvent.getUnicodeChar(), mozcKeyEvent.getKeyCode());

      KeyEventInterface keyEventInterface = keyboardSpecification.getKeyEventInterface(keyEvent);
      assertEquals(keyEvent, keyEventInterface.getNativeEvent().orNull());
    }

    {
      KeyEvent keyEvent = new KeyEventMock(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_GRAVE,
          0, '`' | KeyCharacterMap.COMBINING_ACCENT);
      keyEvent.setSource(InputDevice.SOURCE_KEYBOARD);

      ProtoCommands.KeyEvent mozcKeyEvent = keyboardSpecification.getMozcKeyEvent(keyEvent);
      assertEquals('`', mozcKeyEvent.getKeyCode());

      KeyEventInterface keyEventInterface = keyboardSpecification.getKeyEventInterface(keyEvent);
      assertEquals(keyEvent, keyEventInterface.getNativeEvent().orNull());
    }

    {
      KeyEvent keyEvent = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_SPACE);
      keyEvent.setSource(InputDevice.SOURCE_KEYBOARD);

      ProtoCommands.KeyEvent mozcKeyEvent = keyboardSpecification.getMozcKeyEvent(keyEvent);
      assertFalse(mozcKeyEvent.hasKeyCode());
      assertEquals(SpecialKey.SPACE, mozcKeyEvent.getSpecialKey());

      KeyEventInterface keyEventInterface = keyboardSpecification.getKeyEventInterface(keyEvent);
      assertEquals(keyEvent, keyEventInterface.getNativeEvent().orNull());
    }

    {
      KeyEvent keyEvent = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER);
      keyEvent.setSource(InputDevice.SOURCE_KEYBOARD);

      ProtoCommands.KeyEvent mozcKeyEvent = keyboardSpecification.getMozcKeyEvent(keyEvent);
      assertFalse(mozcKeyEvent.hasKeyCode());
      assertEquals(SpecialKey.ENTER, mozcKeyEvent.getSpecialKey());

      KeyEventInterface keyEventInterface = keyboardSpecification.getKeyEventInterface(keyEvent);
      assertEquals(keyEvent, keyEventInterface.getNativeEvent().orNull());
    }

    {
      KeyEvent keyEvent = new KeyEventMock(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A,
                                           KeyEvent.META_SHIFT_ON, 'A');
      keyEvent.setSource(InputDevice.SOURCE_KEYBOARD);

      ProtoCommands.KeyEvent mozcKeyEvent = keyboardSpecification.getMozcKeyEvent(keyEvent);
      assertEquals(keyEvent.getUnicodeChar(), mozcKeyEvent.getKeyCode());
      // shift+a is printable ('A') so no modifier shouldn't be sent.
      assertEquals(0, mozcKeyEvent.getModifierKeysCount());

      KeyEventInterface keyEventInterface = keyboardSpecification.getKeyEventInterface(keyEvent);
      assertEquals(keyEvent, keyEventInterface.getNativeEvent().orNull());
    }

    {
      KeyEvent keyEvent = new KeyEventMock(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A,
                                           KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0);
      keyEvent.setSource(InputDevice.SOURCE_KEYBOARD);

      ProtoCommands.KeyEvent mozcKeyEvent = keyboardSpecification.getMozcKeyEvent(keyEvent);
      assertEquals('a', mozcKeyEvent.getKeyCode());
      // shift+alt+a is not printable so modifier should be sent.
      HashSet<ModifierKey> expected = new HashSet<ProtoCommands.KeyEvent.ModifierKey>();
      expected.add(ModifierKey.SHIFT);
      expected.add(ModifierKey.ALT);
      MoreAsserts.assertEquals(expected,
                               new HashSet<ModifierKey>(mozcKeyEvent.getModifierKeysList()));

      KeyEventInterface keyEventInterface = keyboardSpecification.getKeyEventInterface(keyEvent);
      assertEquals(keyEvent, keyEventInterface.getNativeEvent().orNull());
    }

    {
      KeyEvent keyEvent = new KeyEventMock(
          KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A,
          KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0);
      keyEvent.setSource(InputDevice.SOURCE_KEYBOARD);

      ProtoCommands.KeyEvent mozcKeyEvent = keyboardSpecification.getMozcKeyEvent(keyEvent);
      assertEquals('a', mozcKeyEvent.getKeyCode());
      // shift+ctrl+alt+a is not printable so modifier should be sent.
      HashSet<ModifierKey> expected = new HashSet<ProtoCommands.KeyEvent.ModifierKey>();
      expected.add(ModifierKey.SHIFT);
      expected.add(ModifierKey.ALT);
      expected.add(ModifierKey.CTRL);
      MoreAsserts.assertEquals(expected,
                               new HashSet<ModifierKey>(mozcKeyEvent.getModifierKeysList()));

      KeyEventInterface keyEventInterface = keyboardSpecification.getKeyEventInterface(keyEvent);
      assertEquals(keyEvent, keyEventInterface.getNativeEvent().orNull());
    }

    {
      KeyEvent keyEvent = new KeyEventMock(
          KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_GRAVE, KeyEvent.META_ALT_ON, 0);
      keyEvent.setSource(InputDevice.SOURCE_KEYBOARD);
      assertEquals(Optional.of(CompositionSwitchMode.TOGGLE),
                   keyboardSpecification.getCompositionSwitchMode(keyEvent));
    }

    assertFalse(keyboardSpecification.getCompositionSwitchMode(new KeyEvent(0, 0)).isPresent());

    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 keyboardSpecification.getKanaKeyboardSpecification());

    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_ALPHABET,
                 keyboardSpecification.getAlphabetKeyboardSpecification());
  }

  // TODO(matsuzakit): This test is overkill. Simplify.
  @SmallTest
  public void testOADG109A() {
    HardwareKeyboardSpecification keyboardSpecification =
        HardwareKeyboardSpecification.JAPANESE109A;

    class TestData extends Parameter{
      final int scanCode;
      final int keyCode;
      final int metaState;
      final int expectKeyCode;
      final SpecialKey expectSpecialKey;
      final Optional<CompositionSwitchMode> expectCompositionSwitchMode;
      final Set<ModifierKey> expectModifierKeySet;
      TestData(int scanCode, int keyCode, int metaState,
               int expectKeyCode, SpecialKey expectSpecialKey,
               Set<ModifierKey> expectModifierKeySet,
               CompositionSwitchMode expectCompositionSwitchMode) {
        this.scanCode = scanCode;
        this.keyCode = keyCode;
        this.metaState = metaState;
        this.expectKeyCode = expectKeyCode;
        this.expectSpecialKey = expectSpecialKey;
        this.expectCompositionSwitchMode = Optional.fromNullable(expectCompositionSwitchMode);
        this.expectModifierKeySet = expectModifierKeySet;
      }
    }

    final Set<ModifierKey> noModifierKeys = Collections.emptySet();
    final Set<ModifierKey> shiftModified = new HashSet<ModifierKey>(
        Collections.singletonList(ModifierKey.SHIFT));
    Set<ModifierKey> shiftAltModified = new HashSet<ModifierKey>();
    shiftAltModified.add(ModifierKey.SHIFT);
    shiftAltModified.add(ModifierKey.ALT);
    Set<ModifierKey> shiftAltCtrlModified = new HashSet<ModifierKey>();
    shiftAltCtrlModified.add(ModifierKey.SHIFT);
    shiftAltCtrlModified.add(ModifierKey.ALT);
    shiftAltCtrlModified.add(ModifierKey.CTRL);

    int keycodeYen;
    int keycodeRo;
    int keycodeMunhenkan;
    int keycodeHenkan;
    int keycodeKana;
    int keycodeKatakana;
    int keycodeZenkakuHankaku;
    if (Build.VERSION.SDK_INT >= 16) {
      keycodeYen = KeyEvent.KEYCODE_YEN;
      keycodeRo = KeyEvent.KEYCODE_RO;
      keycodeMunhenkan = KeyEvent.KEYCODE_MUHENKAN;
      keycodeHenkan = KeyEvent.KEYCODE_HENKAN;
      keycodeKana = KeyEvent.KEYCODE_KANA;
      keycodeKatakana = KeyEvent.KEYCODE_KATAKANA_HIRAGANA;
      keycodeZenkakuHankaku = KeyEvent.KEYCODE_ZENKAKU_HANKAKU;
    } else {
      keycodeYen = 0;
      keycodeRo = 0;
      keycodeMunhenkan = 0;
      keycodeHenkan = 0;
      keycodeKana = 0;
      keycodeKatakana = 0;
      keycodeZenkakuHankaku = KeyEvent.KEYCODE_GRAVE;
    }

    TestData[] testDataList = {
        new TestData(0x02, KeyEvent.KEYCODE_1, 0, '1', null, noModifierKeys, null),
        new TestData(0x02, KeyEvent.KEYCODE_1,
                     KeyEvent.META_SHIFT_ON, '!', null, noModifierKeys, null),
        new TestData(0x03, KeyEvent.KEYCODE_2, 0, '2', null, noModifierKeys, null),
        new TestData(0x03, KeyEvent.KEYCODE_2,
                     KeyEvent.META_SHIFT_ON, '"', null, noModifierKeys, null),
        new TestData(0x04, KeyEvent.KEYCODE_3, 0, '3', null, noModifierKeys, null),
        new TestData(0x04, KeyEvent.KEYCODE_3,
                     KeyEvent.META_SHIFT_ON, '#', null, noModifierKeys, null),
        new TestData(0x05, KeyEvent.KEYCODE_4, 0, '4', null, noModifierKeys, null),
        new TestData(0x05, KeyEvent.KEYCODE_4,
                     KeyEvent.META_SHIFT_ON, '$', null, noModifierKeys, null),
        new TestData(0x06, KeyEvent.KEYCODE_5, 0, '5', null, noModifierKeys, null),
        new TestData(0x06, KeyEvent.KEYCODE_5,
                     KeyEvent.META_SHIFT_ON, '%', null, noModifierKeys, null),
        new TestData(0x07, KeyEvent.KEYCODE_6, 0, '6', null, noModifierKeys, null),
        new TestData(0x07, KeyEvent.KEYCODE_6,
                     KeyEvent.META_SHIFT_ON, '&', null, noModifierKeys, null),
        new TestData(0x08, KeyEvent.KEYCODE_7, 0, '7', null, noModifierKeys, null),
        new TestData(0x08, KeyEvent.KEYCODE_7,
                     KeyEvent.META_SHIFT_ON, '\'', null, noModifierKeys, null),
        new TestData(0x09, KeyEvent.KEYCODE_8, 0, '8', null, noModifierKeys, null),
        new TestData(0x09, KeyEvent.KEYCODE_8,
                     KeyEvent.META_SHIFT_ON, '(', null, noModifierKeys, null),
        new TestData(0x0A, KeyEvent.KEYCODE_9, 0, '9', null, noModifierKeys, null),
        new TestData(0x0A, KeyEvent.KEYCODE_9,
                     KeyEvent.META_SHIFT_ON, ')', null, noModifierKeys, null),
        new TestData(0x0B, KeyEvent.KEYCODE_0, 0, '0', null, noModifierKeys, null),
        // Note: Expect nothing for shift+0.
        new TestData(0x0C, KeyEvent.KEYCODE_MINUS, 0, '-', null, noModifierKeys, null),
        new TestData(0x0C, KeyEvent.KEYCODE_MINUS,
                     KeyEvent.META_SHIFT_ON, '=', null, noModifierKeys, null),
        new TestData(0x0D, KeyEvent.KEYCODE_EQUALS, 0, '^', null, noModifierKeys, null),
        new TestData(0x0D, KeyEvent.KEYCODE_EQUALS,
                     KeyEvent.META_SHIFT_ON, '~', null, noModifierKeys, null),
        new TestData(0x7C, keycodeYen, 0, '\u00a5', null, noModifierKeys, null),
        new TestData(0x7C, keycodeYen, KeyEvent.META_SHIFT_ON, '|', null, noModifierKeys, null),

        new TestData(0x10, KeyEvent.KEYCODE_Q, 0, 'q', null, noModifierKeys, null),
        new TestData(0x10, KeyEvent.KEYCODE_Q,
                     KeyEvent.META_SHIFT_ON, 'Q', null, noModifierKeys, null),
        new TestData(0x11, KeyEvent.KEYCODE_W, 0, 'w', null, noModifierKeys, null),
        new TestData(0x11, KeyEvent.KEYCODE_W,
                     KeyEvent.META_SHIFT_ON, 'W', null, noModifierKeys, null),
        new TestData(0x12, KeyEvent.KEYCODE_E, 0, 'e', null, noModifierKeys, null),
        new TestData(0x12, KeyEvent.KEYCODE_E,
                     KeyEvent.META_SHIFT_ON, 'E', null, noModifierKeys, null),
        new TestData(0x13, KeyEvent.KEYCODE_R, 0, 'r', null, noModifierKeys, null),
        new TestData(0x13, KeyEvent.KEYCODE_R,
                     KeyEvent.META_SHIFT_ON, 'R', null, noModifierKeys, null),
        new TestData(0x14, KeyEvent.KEYCODE_T, 0, 't', null, noModifierKeys, null),
        new TestData(0x14, KeyEvent.KEYCODE_T,
                     KeyEvent.META_SHIFT_ON, 'T', null, noModifierKeys, null),
        new TestData(0x15, KeyEvent.KEYCODE_Y, 0, 'y', null, noModifierKeys, null),
        new TestData(0x15, KeyEvent.KEYCODE_Y,
                     KeyEvent.META_SHIFT_ON, 'Y', null, noModifierKeys, null),
        new TestData(0x16, KeyEvent.KEYCODE_U, 0, 'u', null, noModifierKeys, null),
        new TestData(0x16, KeyEvent.KEYCODE_U,
                     KeyEvent.META_SHIFT_ON, 'U', null, noModifierKeys, null),
        new TestData(0x17, KeyEvent.KEYCODE_I, 0, 'i', null, noModifierKeys, null),
        new TestData(0x17, KeyEvent.KEYCODE_I,
                     KeyEvent.META_SHIFT_ON, 'I', null, noModifierKeys, null),
        new TestData(0x18, KeyEvent.KEYCODE_O, 0, 'o', null, noModifierKeys, null),
        new TestData(0x18, KeyEvent.KEYCODE_O,
                     KeyEvent.META_SHIFT_ON, 'O', null, noModifierKeys, null),
        new TestData(0x19, KeyEvent.KEYCODE_P, 0, 'p', null, noModifierKeys, null),
        new TestData(0x19, KeyEvent.KEYCODE_P,
                     KeyEvent.META_SHIFT_ON, 'P', null, noModifierKeys, null),
        new TestData(0x1A, KeyEvent.KEYCODE_LEFT_BRACKET, 0, '@', null, noModifierKeys, null),
        new TestData(0x1A, KeyEvent.KEYCODE_LEFT_BRACKET,
                     KeyEvent.META_SHIFT_ON, '`', null, noModifierKeys, null),
        new TestData(0x1B, KeyEvent.KEYCODE_RIGHT_BRACKET, 0, '[', null, noModifierKeys, null),
        new TestData(0x1B, KeyEvent.KEYCODE_RIGHT_BRACKET,
                     KeyEvent.META_SHIFT_ON, '{', null, noModifierKeys, null),

        new TestData(0x1E, KeyEvent.KEYCODE_A, 0, 'a', null, noModifierKeys, null),
        new TestData(0x1E, KeyEvent.KEYCODE_A,
                     KeyEvent.META_SHIFT_ON, 'A', null, noModifierKeys, null),
        new TestData(0x1F, KeyEvent.KEYCODE_S, 0, 's', null, noModifierKeys, null),
        new TestData(0x1F, KeyEvent.KEYCODE_S,
                     KeyEvent.META_SHIFT_ON, 'S', null, noModifierKeys, null),
        new TestData(0x20, KeyEvent.KEYCODE_D, 0, 'd', null, noModifierKeys, null),
        new TestData(0x20, KeyEvent.KEYCODE_D,
                     KeyEvent.META_SHIFT_ON, 'D', null, noModifierKeys, null),
        new TestData(0x21, KeyEvent.KEYCODE_F, 0, 'f', null, noModifierKeys, null),
        new TestData(0x21, KeyEvent.KEYCODE_F,
                     KeyEvent.META_SHIFT_ON, 'F', null, noModifierKeys, null),
        new TestData(0x22, KeyEvent.KEYCODE_G, 0, 'g', null, noModifierKeys, null),
        new TestData(0x22, KeyEvent.KEYCODE_G,
                     KeyEvent.META_SHIFT_ON, 'G', null, noModifierKeys, null),
        new TestData(0x23, KeyEvent.KEYCODE_H, 0, 'h', null, noModifierKeys, null),
        new TestData(0x23, KeyEvent.KEYCODE_H,
                     KeyEvent.META_SHIFT_ON, 'H', null, noModifierKeys, null),
        new TestData(0x24, KeyEvent.KEYCODE_J, 0, 'j', null, noModifierKeys, null),
        new TestData(0x24, KeyEvent.KEYCODE_J,
                     KeyEvent.META_SHIFT_ON, 'J', null, noModifierKeys, null),
        new TestData(0x25, KeyEvent.KEYCODE_K, 0, 'k', null, noModifierKeys, null),
        new TestData(0x25, KeyEvent.KEYCODE_K,
                     KeyEvent.META_SHIFT_ON, 'K', null, noModifierKeys, null),
        new TestData(0x26, KeyEvent.KEYCODE_L, 0, 'l', null, noModifierKeys, null),
        new TestData(0x26, KeyEvent.KEYCODE_L,
                     KeyEvent.META_SHIFT_ON, 'L', null, noModifierKeys, null),
        new TestData(0x27, KeyEvent.KEYCODE_SEMICOLON, 0, ';', null, noModifierKeys, null),
        new TestData(0x27, KeyEvent.KEYCODE_SEMICOLON,
                     KeyEvent.META_SHIFT_ON, '+', null, noModifierKeys, null),
        new TestData(0x28, KeyEvent.KEYCODE_APOSTROPHE, 0, ':', null, noModifierKeys, null),
        new TestData(0x28, KeyEvent.KEYCODE_APOSTROPHE,
                     KeyEvent.META_SHIFT_ON, '*', null, noModifierKeys, null),
        new TestData(0x2B, KeyEvent.KEYCODE_BACKSLASH, 0, ']', null, noModifierKeys, null),
        new TestData(0x2B, KeyEvent.KEYCODE_BACKSLASH,
                     KeyEvent.META_SHIFT_ON, '}', null, noModifierKeys, null),

        new TestData(0x2C, KeyEvent.KEYCODE_Z, 0, 'z', null, noModifierKeys, null),
        new TestData(0x2C, KeyEvent.KEYCODE_Z,
                     KeyEvent.META_SHIFT_ON, 'Z', null, noModifierKeys, null),
        new TestData(0x2D, KeyEvent.KEYCODE_X, 0, 'x', null, noModifierKeys, null),
        new TestData(0x2D, KeyEvent.KEYCODE_X,
                     KeyEvent.META_SHIFT_ON, 'X', null, noModifierKeys, null),
        new TestData(0x2E, KeyEvent.KEYCODE_C, 0, 'c', null, noModifierKeys, null),
        new TestData(0x2E, KeyEvent.KEYCODE_C,
                     KeyEvent.META_SHIFT_ON, 'C', null, noModifierKeys, null),
        new TestData(0x2F, KeyEvent.KEYCODE_V, 0, 'v', null, noModifierKeys, null),
        new TestData(0x2F, KeyEvent.KEYCODE_V,
                     KeyEvent.META_SHIFT_ON, 'V', null, noModifierKeys, null),
        new TestData(0x30, KeyEvent.KEYCODE_B, 0, 'b', null, noModifierKeys, null),
        new TestData(0x30, KeyEvent.KEYCODE_B,
                     KeyEvent.META_SHIFT_ON, 'B', null, noModifierKeys, null),
        new TestData(0x31, KeyEvent.KEYCODE_N, 0, 'n', null, noModifierKeys, null),
        new TestData(0x31, KeyEvent.KEYCODE_N,
                     KeyEvent.META_SHIFT_ON, 'N', null, noModifierKeys, null),
        new TestData(0x32, KeyEvent.KEYCODE_M, 0, 'm', null, noModifierKeys, null),
        new TestData(0x32, KeyEvent.KEYCODE_M,
                     KeyEvent.META_SHIFT_ON, 'M', null, noModifierKeys, null),
        new TestData(0x33, KeyEvent.KEYCODE_COMMA, 0, ',', null, noModifierKeys, null),
        new TestData(0x33, KeyEvent.KEYCODE_COMMA,
                     KeyEvent.META_SHIFT_ON, '<', null, noModifierKeys, null),
        new TestData(0x34, KeyEvent.KEYCODE_PERIOD, 0, '.', null, noModifierKeys, null),
        new TestData(0x34, KeyEvent.KEYCODE_PERIOD,
                     KeyEvent.META_SHIFT_ON, '>', null, noModifierKeys, null),
        new TestData(0x35, KeyEvent.KEYCODE_SLASH, 0, '/', null, noModifierKeys, null),
        new TestData(0x35, KeyEvent.KEYCODE_SLASH,
                     KeyEvent.META_SHIFT_ON, '?', null, noModifierKeys, null),
        new TestData(0x59, keycodeRo, 0, '\\', null, noModifierKeys, null),
        new TestData(0x59, keycodeRo, KeyEvent.META_SHIFT_ON, '_', null, noModifierKeys, null),

        new TestData(0x01, KeyEvent.KEYCODE_ESCAPE, 0, 0, SpecialKey.ESCAPE, noModifierKeys, null),
        new TestData(0x01, KeyEvent.KEYCODE_ESCAPE,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.ESCAPE, shiftModified, null),
        new TestData(0x01, KeyEvent.KEYCODE_ESCAPE,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.ESCAPE, shiftAltModified, null),
        new TestData(0x01, KeyEvent.KEYCODE_ESCAPE,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.ESCAPE, shiftAltCtrlModified, null),
        new TestData(0x3B, KeyEvent.KEYCODE_F1, 0, 0, SpecialKey.F1, noModifierKeys, null),
        new TestData(0x3B, KeyEvent.KEYCODE_F1,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.F1, shiftModified, null),
        new TestData(0x3B, KeyEvent.KEYCODE_F1,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F1, shiftAltModified, null),
        new TestData(0x3B, KeyEvent.KEYCODE_F1,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F1, shiftAltCtrlModified, null),
        new TestData(0x3C, KeyEvent.KEYCODE_F2, 0, 0, SpecialKey.F2, noModifierKeys, null),
        new TestData(0x3C, KeyEvent.KEYCODE_F2,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.F2, shiftModified, null),
        new TestData(0x3C, KeyEvent.KEYCODE_F2,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F2, shiftAltModified, null),
        new TestData(0x3C, KeyEvent.KEYCODE_F2,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F2, shiftAltCtrlModified, null),
        new TestData(0x3D, KeyEvent.KEYCODE_F3, 0, 0, SpecialKey.F3, noModifierKeys, null),
        new TestData(0x3D, KeyEvent.KEYCODE_F3,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.F3, shiftModified, null),
        new TestData(0x3D, KeyEvent.KEYCODE_F3,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F3, shiftAltModified, null),
        new TestData(0x3D, KeyEvent.KEYCODE_F3,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F3, shiftAltCtrlModified, null),
        new TestData(0x3E, KeyEvent.KEYCODE_F4, 0, 0, SpecialKey.F4, noModifierKeys, null),
        new TestData(0x3E, KeyEvent.KEYCODE_F4,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.F4, shiftModified, null),
        new TestData(0x3E, KeyEvent.KEYCODE_F4,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F4, shiftAltModified, null),
        new TestData(0x3E, KeyEvent.KEYCODE_F4,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F4, shiftAltCtrlModified, null),
        new TestData(0x3F, KeyEvent.KEYCODE_F5, 0, 0, SpecialKey.F5, noModifierKeys, null),
        new TestData(0x3F, KeyEvent.KEYCODE_F5,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.F5, shiftModified, null),
        new TestData(0x3F, KeyEvent.KEYCODE_F5,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F5, shiftAltModified, null),
        new TestData(0x3F, KeyEvent.KEYCODE_F5,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F5, shiftAltCtrlModified, null),
        new TestData(0x40, KeyEvent.KEYCODE_F6, 0, 0, SpecialKey.F6, noModifierKeys, null),
        new TestData(0x40, KeyEvent.KEYCODE_F6,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.F6, shiftModified, null),
        new TestData(0x40, KeyEvent.KEYCODE_F6,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F6, shiftAltModified, null),
        new TestData(0x40, KeyEvent.KEYCODE_F6,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F6, shiftAltCtrlModified, null),
        new TestData(0x41, KeyEvent.KEYCODE_F7, 0, 0, SpecialKey.F7, noModifierKeys, null),
        new TestData(0x41, KeyEvent.KEYCODE_F7,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.F7, shiftModified, null),
        new TestData(0x41, KeyEvent.KEYCODE_F7,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F7, shiftAltModified, null),
        new TestData(0x41, KeyEvent.KEYCODE_F7,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F7, shiftAltCtrlModified, null),
        new TestData(0x42, KeyEvent.KEYCODE_F8, 0, 0, SpecialKey.F8, noModifierKeys, null),
        new TestData(0x42, KeyEvent.KEYCODE_F8,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.F8, shiftModified, null),
        new TestData(0x42, KeyEvent.KEYCODE_F8,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F8, shiftAltModified, null),
        new TestData(0x42, KeyEvent.KEYCODE_F8,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F8, shiftAltCtrlModified, null),
        new TestData(0x43, KeyEvent.KEYCODE_F9, 0, 0, SpecialKey.F9, noModifierKeys, null),
        new TestData(0x43, KeyEvent.KEYCODE_F9,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.F9, shiftModified, null),
        new TestData(0x43, KeyEvent.KEYCODE_F9,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F9, shiftAltModified, null),
        new TestData(0x43, KeyEvent.KEYCODE_F9,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F9, shiftAltCtrlModified, null),
        new TestData(0x44, KeyEvent.KEYCODE_F10, 0, 0, SpecialKey.F10, noModifierKeys, null),
        new TestData(0x44, KeyEvent.KEYCODE_F10,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.F10, shiftModified, null),
        new TestData(0x44, KeyEvent.KEYCODE_F10,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F10, shiftAltModified, null),
        new TestData(0x44, KeyEvent.KEYCODE_F10,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F10, shiftAltCtrlModified, null),
        new TestData(0x57, KeyEvent.KEYCODE_F11, 0, 0, SpecialKey.F11, noModifierKeys, null),
        new TestData(0x57, KeyEvent.KEYCODE_F11,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.F11, shiftModified, null),
        new TestData(0x57, KeyEvent.KEYCODE_F11,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F11, shiftAltModified, null),
        new TestData(0x57, KeyEvent.KEYCODE_F11,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F11, shiftAltCtrlModified, null),
        new TestData(0x58, KeyEvent.KEYCODE_F12, 0, 0, SpecialKey.F12, noModifierKeys, null),
        new TestData(0x58, KeyEvent.KEYCODE_F12,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.F12, shiftModified, null),
        new TestData(0x58, KeyEvent.KEYCODE_F12,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F12, shiftAltModified, null),
        new TestData(0x58, KeyEvent.KEYCODE_F12,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F12, shiftAltCtrlModified, null),
        new TestData(0x6F, KeyEvent.KEYCODE_FORWARD_DEL,
                     0, 0, SpecialKey.DEL, noModifierKeys, null),
        new TestData(0x6F, KeyEvent.KEYCODE_FORWARD_DEL,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.DEL, shiftModified, null),
        new TestData(0x6F, KeyEvent.KEYCODE_FORWARD_DEL,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.DEL, shiftAltModified, null),
        new TestData(0x6F, KeyEvent.KEYCODE_FORWARD_DEL,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.DEL, shiftAltCtrlModified, null),

        new TestData(0x0E, KeyEvent.KEYCODE_DEL,
                     0, 0, SpecialKey.BACKSPACE, noModifierKeys, null),
        new TestData(0x0E, KeyEvent.KEYCODE_DEL,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.BACKSPACE, shiftModified, null),
        new TestData(0x0E, KeyEvent.KEYCODE_DEL,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.BACKSPACE, shiftAltModified, null),
        new TestData(0x0E, KeyEvent.KEYCODE_DEL,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.BACKSPACE, shiftAltCtrlModified, null),
        new TestData(0x0F, KeyEvent.KEYCODE_TAB, 0, 0, SpecialKey.TAB, noModifierKeys, null),
        new TestData(0x0F, KeyEvent.KEYCODE_TAB,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.TAB, shiftModified, null),
        new TestData(0x0F, KeyEvent.KEYCODE_TAB,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.TAB, shiftAltModified, null),
        new TestData(0x0F, KeyEvent.KEYCODE_TAB,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.TAB, shiftAltCtrlModified, null),
        new TestData(0x1C, KeyEvent.KEYCODE_ENTER,
                     0, 0, SpecialKey.ENTER, noModifierKeys, null),
        new TestData(0x1C, KeyEvent.KEYCODE_ENTER,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.ENTER, shiftModified, null),
        new TestData(0x1C, KeyEvent.KEYCODE_ENTER,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.ENTER, shiftAltModified, null),
        new TestData(0x1C, KeyEvent.KEYCODE_ENTER,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.ENTER, shiftAltCtrlModified, null),

        new TestData(0x5E, keycodeMunhenkan, 0, 0, SpecialKey.MUHENKAN, noModifierKeys, null),
        new TestData(0x5E, keycodeMunhenkan,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.MUHENKAN, shiftModified, null),
        new TestData(0x5E, keycodeMunhenkan,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.MUHENKAN, shiftAltModified, null),
        new TestData(0x5E, keycodeMunhenkan,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.MUHENKAN, shiftAltCtrlModified, null),
        new TestData(0x39, KeyEvent.KEYCODE_SPACE,
                     0, 0, SpecialKey.SPACE, noModifierKeys, null),
        new TestData(0x39, KeyEvent.KEYCODE_SPACE,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.SPACE, shiftModified, null),
        new TestData(0x39, KeyEvent.KEYCODE_SPACE,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.SPACE, shiftAltModified, null),
        new TestData(0x39, KeyEvent.KEYCODE_SPACE,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.SPACE, shiftAltCtrlModified, null),
        new TestData(0x5C, keycodeHenkan, 0, 0, SpecialKey.HENKAN, noModifierKeys, null),
        new TestData(0x5C, keycodeHenkan,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.HENKAN, shiftModified, null),
        new TestData(0x5C, keycodeHenkan, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.HENKAN, shiftAltModified, null),
        new TestData(0x5C, keycodeHenkan,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.HENKAN, shiftAltCtrlModified, null),
        new TestData(0x5D, keycodeKatakana, 0, 0, SpecialKey.KATAKANA, noModifierKeys, null),
        new TestData(0x5D, keycodeKatakana,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.KATAKANA, shiftModified, null),
        new TestData(0x5D, keycodeKatakana,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.KATAKANA, shiftAltModified, null),
        new TestData(0x5D, keycodeKatakana,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.KATAKANA, shiftAltCtrlModified, null),
        new TestData(0x7A, keycodeKana, 0, 0, SpecialKey.KANA, noModifierKeys, null),
        new TestData(0x7A, keycodeKana,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.KANA, shiftModified, null),
        new TestData(0x7A, keycodeKana,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.KANA, shiftAltModified, null),
        new TestData(0x7A, keycodeKana,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.KANA, shiftAltCtrlModified, null),

        new TestData(0x67, KeyEvent.KEYCODE_DPAD_UP,
                     0, 0, SpecialKey.UP, noModifierKeys, null),
        new TestData(0x67, KeyEvent.KEYCODE_DPAD_UP,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.UP, shiftModified, null),
        new TestData(0x67, KeyEvent.KEYCODE_DPAD_UP,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.UP, shiftAltModified, null),
        new TestData(0x67, KeyEvent.KEYCODE_DPAD_UP,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.UP, shiftAltCtrlModified, null),
        new TestData(0x69, KeyEvent.KEYCODE_DPAD_LEFT,
                     0, 0, SpecialKey.LEFT, noModifierKeys, null),
        new TestData(0x69, KeyEvent.KEYCODE_DPAD_LEFT,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.LEFT, shiftModified, null),
        new TestData(0x69, KeyEvent.KEYCODE_DPAD_LEFT,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.LEFT, shiftAltModified, null),
        new TestData(0x69, KeyEvent.KEYCODE_DPAD_LEFT,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.LEFT, shiftAltCtrlModified, null),
        new TestData(0x6C, KeyEvent.KEYCODE_DPAD_DOWN,
                     0, 0, SpecialKey.DOWN, noModifierKeys, null),
        new TestData(0x6C, KeyEvent.KEYCODE_DPAD_DOWN,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.DOWN, shiftModified, null),
        new TestData(0x6C, KeyEvent.KEYCODE_DPAD_DOWN,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.DOWN, shiftAltModified, null),
        new TestData(0x6C, KeyEvent.KEYCODE_DPAD_DOWN,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.DOWN, shiftAltCtrlModified, null),
        new TestData(0x6A, KeyEvent.KEYCODE_DPAD_RIGHT,
                     0, 0, SpecialKey.RIGHT, noModifierKeys, null),
        new TestData(0x6A, KeyEvent.KEYCODE_DPAD_RIGHT,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.RIGHT, shiftModified, null),
        new TestData(0x6A, KeyEvent.KEYCODE_DPAD_RIGHT,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.RIGHT, shiftAltModified, null),
        new TestData(0x6A, KeyEvent.KEYCODE_DPAD_RIGHT,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.RIGHT, shiftAltCtrlModified, null),

        new TestData(0x66, KeyEvent.KEYCODE_MOVE_HOME,
                     0, 0, SpecialKey.HOME, noModifierKeys, null),
        new TestData(0x66, KeyEvent.KEYCODE_MOVE_HOME,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.HOME, shiftModified, null),
        new TestData(0x66, KeyEvent.KEYCODE_MOVE_HOME,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.HOME, shiftAltModified, null),
        new TestData(0x66, KeyEvent.KEYCODE_MOVE_HOME,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.HOME, shiftAltCtrlModified, null),
        new TestData(0x6B, KeyEvent.KEYCODE_MOVE_END,
                     0, 0, SpecialKey.END, noModifierKeys, null),
        new TestData(0x6B, KeyEvent.KEYCODE_MOVE_END,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.END, shiftModified, null),
        new TestData(0x6B, KeyEvent.KEYCODE_MOVE_END,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.END, shiftAltModified, null),
        new TestData(0x6B, KeyEvent.KEYCODE_MOVE_END,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.END, shiftAltCtrlModified, null),
        new TestData(0x6E, KeyEvent.KEYCODE_INSERT,
                     0, 0, SpecialKey.INSERT, noModifierKeys, null),
        new TestData(0x6E, KeyEvent.KEYCODE_INSERT,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.INSERT, shiftModified, null),
        new TestData(0x6E, KeyEvent.KEYCODE_INSERT,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.INSERT, shiftAltModified, null),
        new TestData(0x6E, KeyEvent.KEYCODE_INSERT,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.INSERT, shiftAltCtrlModified, null),
        new TestData(0x68, KeyEvent.KEYCODE_PAGE_UP,
                     0, 0, SpecialKey.PAGE_UP, noModifierKeys, null),
        new TestData(0x68, KeyEvent.KEYCODE_PAGE_UP,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.PAGE_UP, shiftModified, null),
        new TestData(0x68, KeyEvent.KEYCODE_PAGE_UP,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.PAGE_UP, shiftAltModified, null),
        new TestData(0x68, KeyEvent.KEYCODE_PAGE_UP,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.PAGE_UP, shiftAltCtrlModified, null),
        new TestData(0x6D, KeyEvent.KEYCODE_PAGE_DOWN,
                     0, 0, SpecialKey.PAGE_DOWN, noModifierKeys, null),
        new TestData(0x6D, KeyEvent.KEYCODE_PAGE_DOWN,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.PAGE_DOWN, shiftModified, null),
        new TestData(0x6D, KeyEvent.KEYCODE_PAGE_DOWN,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.PAGE_DOWN, shiftAltModified, null),
        new TestData(0x6D, KeyEvent.KEYCODE_PAGE_DOWN,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.PAGE_DOWN, shiftAltCtrlModified, null),

        new TestData(0x62, KeyEvent.KEYCODE_NUMPAD_DIVIDE,
                     0, 0, SpecialKey.DIVIDE, noModifierKeys, null),
        new TestData(0x62, KeyEvent.KEYCODE_NUMPAD_DIVIDE,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.DIVIDE, shiftModified, null),
        new TestData(0x62, KeyEvent.KEYCODE_NUMPAD_DIVIDE,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.DIVIDE, shiftAltModified, null),
        new TestData(0x62, KeyEvent.KEYCODE_NUMPAD_DIVIDE,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.DIVIDE, shiftAltCtrlModified, null),
        new TestData(0x37, KeyEvent.KEYCODE_NUMPAD_MULTIPLY,
                     0, 0, SpecialKey.MULTIPLY, noModifierKeys, null),
        new TestData(0x37, KeyEvent.KEYCODE_NUMPAD_MULTIPLY,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.MULTIPLY, shiftModified, null),
        new TestData(0x37, KeyEvent.KEYCODE_NUMPAD_MULTIPLY,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.MULTIPLY, shiftAltModified, null),
        new TestData(0x37, KeyEvent.KEYCODE_NUMPAD_MULTIPLY,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.MULTIPLY, shiftAltCtrlModified, null),
        new TestData(0x4A, KeyEvent.KEYCODE_NUMPAD_SUBTRACT,
                     0, 0, SpecialKey.SUBTRACT, noModifierKeys, null),
        new TestData(0x4A, KeyEvent.KEYCODE_NUMPAD_SUBTRACT,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.SUBTRACT, shiftModified, null),
        new TestData(0x4A, KeyEvent.KEYCODE_NUMPAD_SUBTRACT,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.SUBTRACT, shiftAltModified, null),
        new TestData(0x4A, KeyEvent.KEYCODE_NUMPAD_SUBTRACT,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.SUBTRACT, shiftAltCtrlModified, null),
        new TestData(0x4E, KeyEvent.KEYCODE_NUMPAD_ADD,
                     0, 0, SpecialKey.ADD, noModifierKeys, null),
        new TestData(0x4E, KeyEvent.KEYCODE_NUMPAD_ADD,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.ADD, shiftModified, null),
        new TestData(0x4E, KeyEvent.KEYCODE_NUMPAD_ADD,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.ADD, shiftAltModified, null),
        new TestData(0x4E, KeyEvent.KEYCODE_NUMPAD_ADD,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.ADD, shiftAltCtrlModified, null),
        new TestData(0x60, KeyEvent.KEYCODE_NUMPAD_ENTER,
                     0, 0, SpecialKey.SEPARATOR, noModifierKeys, null),
        new TestData(0x60, KeyEvent.KEYCODE_NUMPAD_ENTER,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.SEPARATOR, shiftModified, null),
        new TestData(0x60, KeyEvent.KEYCODE_NUMPAD_ENTER,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.SEPARATOR, shiftAltModified, null),
        new TestData(0x60, KeyEvent.KEYCODE_NUMPAD_ENTER,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.SEPARATOR, shiftAltCtrlModified, null),
        new TestData(0x53, KeyEvent.KEYCODE_NUMPAD_DOT,
                     0, 0, SpecialKey.DECIMAL, noModifierKeys, null),
        new TestData(0x53, KeyEvent.KEYCODE_NUMPAD_DOT,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.DECIMAL, shiftModified, null),
        new TestData(0x53, KeyEvent.KEYCODE_NUMPAD_DOT,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.DECIMAL, shiftAltModified, null),
        new TestData(0x53, KeyEvent.KEYCODE_NUMPAD_DOT,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.DECIMAL, shiftAltCtrlModified, null),

        new TestData(0x52, KeyEvent.KEYCODE_NUMPAD_0,
                     0, 0, SpecialKey.NUMPAD0, noModifierKeys, null),
        new TestData(0x52, KeyEvent.KEYCODE_NUMPAD_0,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.NUMPAD0, shiftModified, null),
        new TestData(0x52, KeyEvent.KEYCODE_NUMPAD_0,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.NUMPAD0, shiftAltModified, null),
        new TestData(0x52, KeyEvent.KEYCODE_NUMPAD_0,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.NUMPAD0, shiftAltCtrlModified, null),
        new TestData(0x4F, KeyEvent.KEYCODE_NUMPAD_1,
                     0, 0, SpecialKey.NUMPAD1, noModifierKeys, null),
        new TestData(0x4F, KeyEvent.KEYCODE_NUMPAD_1,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.NUMPAD1, shiftModified, null),
        new TestData(0x4F, KeyEvent.KEYCODE_NUMPAD_1,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.NUMPAD1, shiftAltModified, null),
        new TestData(0x4F, KeyEvent.KEYCODE_NUMPAD_1,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.NUMPAD1, shiftAltCtrlModified, null),
        new TestData(0x50, KeyEvent.KEYCODE_NUMPAD_2,
                     0, 0, SpecialKey.NUMPAD2, noModifierKeys, null),
        new TestData(0x50, KeyEvent.KEYCODE_NUMPAD_2,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.NUMPAD2, shiftModified, null),
        new TestData(0x50, KeyEvent.KEYCODE_NUMPAD_2,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.NUMPAD2, shiftAltModified, null),
        new TestData(0x50, KeyEvent.KEYCODE_NUMPAD_2,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.NUMPAD2, shiftAltCtrlModified, null),
        new TestData(0x51, KeyEvent.KEYCODE_NUMPAD_3,
                     0, 0, SpecialKey.NUMPAD3, noModifierKeys, null),
        new TestData(0x51, KeyEvent.KEYCODE_NUMPAD_3,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.NUMPAD3, shiftModified, null),
        new TestData(0x51, KeyEvent.KEYCODE_NUMPAD_3,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.NUMPAD3, shiftAltModified, null),
        new TestData(0x51, KeyEvent.KEYCODE_NUMPAD_3,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.NUMPAD3, shiftAltCtrlModified, null),
        new TestData(0x4B, KeyEvent.KEYCODE_NUMPAD_4,
                     0, 0, SpecialKey.NUMPAD4, noModifierKeys, null),
        new TestData(0x4B, KeyEvent.KEYCODE_NUMPAD_4,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.NUMPAD4, shiftModified, null),
        new TestData(0x4B, KeyEvent.KEYCODE_NUMPAD_4,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.NUMPAD4, shiftAltModified, null),
        new TestData(0x4B, KeyEvent.KEYCODE_NUMPAD_4,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.NUMPAD4, shiftAltCtrlModified, null),
        new TestData(0x4C, KeyEvent.KEYCODE_NUMPAD_5,
                     0, 0, SpecialKey.NUMPAD5, noModifierKeys, null),
        new TestData(0x4C, KeyEvent.KEYCODE_NUMPAD_5,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.NUMPAD5, shiftModified, null),
        new TestData(0x4C, KeyEvent.KEYCODE_NUMPAD_5,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.NUMPAD5, shiftAltModified, null),
        new TestData(0x4C, KeyEvent.KEYCODE_NUMPAD_5,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.NUMPAD5, shiftAltCtrlModified, null),
        new TestData(0x4D, KeyEvent.KEYCODE_NUMPAD_6,
                     0, 0, SpecialKey.NUMPAD6, noModifierKeys, null),
        new TestData(0x4D, KeyEvent.KEYCODE_NUMPAD_6,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.NUMPAD6, shiftModified, null),
        new TestData(0x4D, KeyEvent.KEYCODE_NUMPAD_6,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.NUMPAD6, shiftAltModified, null),
        new TestData(0x4D, KeyEvent.KEYCODE_NUMPAD_6,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.NUMPAD6, shiftAltCtrlModified, null),
        new TestData(0x47, KeyEvent.KEYCODE_NUMPAD_7,
                     0, 0, SpecialKey.NUMPAD7, noModifierKeys, null),
        new TestData(0x47, KeyEvent.KEYCODE_NUMPAD_7,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.NUMPAD7, shiftModified, null),
        new TestData(0x47, KeyEvent.KEYCODE_NUMPAD_7,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.NUMPAD7, shiftAltModified, null),
        new TestData(0x47, KeyEvent.KEYCODE_NUMPAD_7,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.NUMPAD7, shiftAltCtrlModified, null),
        new TestData(0x48, KeyEvent.KEYCODE_NUMPAD_8,
                     0, 0, SpecialKey.NUMPAD8, noModifierKeys, null),
        new TestData(0x48, KeyEvent.KEYCODE_NUMPAD_8,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.NUMPAD8, shiftModified, null),
        new TestData(0x48, KeyEvent.KEYCODE_NUMPAD_8,
                    KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.NUMPAD8, shiftAltModified, null),
        new TestData(0x48, KeyEvent.KEYCODE_NUMPAD_8,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.NUMPAD8, shiftAltCtrlModified, null),
        new TestData(0x49, KeyEvent.KEYCODE_NUMPAD_9,
                     0, 0, SpecialKey.NUMPAD9, noModifierKeys, null),
        new TestData(0x49, KeyEvent.KEYCODE_NUMPAD_9,
                     KeyEvent.META_SHIFT_ON, 0, SpecialKey.NUMPAD9, shiftModified, null),
        new TestData(0x49, KeyEvent.KEYCODE_NUMPAD_9,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.NUMPAD9, shiftAltModified, null),
        new TestData(0x49, KeyEvent.KEYCODE_NUMPAD_9,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.NUMPAD9, shiftAltCtrlModified, null),

        new TestData(0x29, keycodeZenkakuHankaku,
                     0, 0, null, noModifierKeys, CompositionSwitchMode.TOGGLE),
        new TestData(0x29, keycodeZenkakuHankaku,
                     KeyEvent.META_SHIFT_ON, 0, null, shiftModified, null),
        new TestData(0x29, keycodeZenkakuHankaku, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
            null, noModifierKeys, null),
        new TestData(0x29, keycodeZenkakuHankaku,
                     KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     null, noModifierKeys, null),
    };

    for (final TestData testData : testDataList) {
      KeyEvent keyEvent =
          new KeyEvent(0, 0, 0, testData.keyCode, 0, testData.metaState, 0, testData.scanCode) {
        @Override
        public int getUnicodeChar() {
          return testData.expectKeyCode;
        }
      };
      keyEvent.setSource(InputDevice.SOURCE_KEYBOARD);

      ProtoCommands.KeyEvent mozcKeyEvent = keyboardSpecification.getMozcKeyEvent(keyEvent);
      if (testData.expectKeyCode == 0 && testData.expectSpecialKey == null) {
        assertNull(testData.toString(), mozcKeyEvent);
      } else if (testData.expectKeyCode != 0) {
        assertNotNull(testData.toString(), mozcKeyEvent);
        assertEquals(testData.toString(), testData.expectKeyCode, mozcKeyEvent.getKeyCode());
        assertFalse(testData.toString(), mozcKeyEvent.hasSpecialKey());
      } else if (testData.expectSpecialKey != null) {
        assertNotNull(testData.toString(), mozcKeyEvent);
        assertEquals(testData.toString(), testData.expectSpecialKey, mozcKeyEvent.getSpecialKey());
        assertFalse(testData.toString(), mozcKeyEvent.hasKeyCode());
      }

      if (mozcKeyEvent != null) {
        MoreAsserts.assertEquals(testData.toString(), testData.expectModifierKeySet,
                                 new HashSet<ModifierKey>(mozcKeyEvent.getModifierKeysList()));
      }

      assertEquals(testData.toString(), keyEvent,
                   keyboardSpecification.getKeyEventInterface(keyEvent).getNativeEvent().orNull());

      assertEquals(testData.toString(), testData.expectCompositionSwitchMode,
                   keyboardSpecification.getCompositionSwitchMode(keyEvent));
    }

    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 keyboardSpecification.getKanaKeyboardSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_ALPHABET,
                 keyboardSpecification.getAlphabetKeyboardSpecification());
  }

  @SmallTest
  public void testMaybeSetDetectedHardwareKeyMap() {
    class TestData extends Parameter {
      final HardwareKeyMap presetHardwareKeyMap;
      final int keyboard;
      final boolean forceToSet;
      final HardwareKeyMap expectHardwareKeyMap;

      TestData(HardwareKeyMap presetHardwareKeyMap,
               int keyboard,
               boolean forceToSet,
               HardwareKeyMap expectHardwareKeyMap) {
        this.presetHardwareKeyMap = presetHardwareKeyMap;
        this.keyboard = keyboard;
        this.forceToSet = forceToSet;
        this.expectHardwareKeyMap = expectHardwareKeyMap;
      }
    }

    TestData[] testDataList = {
        new TestData(HardwareKeyMap.JAPANESE109A, Configuration.KEYBOARD_12KEY, false,
                     HardwareKeyMap.JAPANESE109A),
        new TestData(null, Configuration.KEYBOARD_12KEY, false, HardwareKeyMap.DEFAULT),
        new TestData(null, Configuration.KEYBOARD_QWERTY, false, HardwareKeyMap.DEFAULT),
        new TestData(null, Configuration.KEYBOARD_NOKEYS, false, null),
        new TestData(null, Configuration.KEYBOARD_UNDEFINED, false, null),
        new TestData(null, Configuration.KEYBOARD_NOKEYS, true, HardwareKeyMap.DEFAULT),
        new TestData(null, Configuration.KEYBOARD_UNDEFINED, true, HardwareKeyMap.DEFAULT),
    };

    SharedPreferences sharedPreferences = MozcPreferenceUtil.getSharedPreferences(
        getInstrumentation().getContext(), "HARDWARE_KEYMAP");
    for (TestData testData : testDataList) {
      sharedPreferences.edit().clear().commit();
      if (testData.presetHardwareKeyMap != null) {
        HardwareKeyboardSpecification.setHardwareKeyMap(
            sharedPreferences, testData.presetHardwareKeyMap);
      }
      Configuration configuration = new Configuration();
      configuration.keyboard = testData.keyboard;
      HardwareKeyboardSpecification.maybeSetDetectedHardwareKeyMap(
          sharedPreferences, configuration, testData.forceToSet);
      assertEquals(testData.toString(), testData.expectHardwareKeyMap,
                   HardwareKeyboardSpecification.getHardwareKeyMap(sharedPreferences));
    }

  }
}
