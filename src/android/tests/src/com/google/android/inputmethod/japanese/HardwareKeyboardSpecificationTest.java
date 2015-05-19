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

import org.mozc.android.inputmethod.japanese.HardwareKeyboard.CompositionSwitchMode;
import org.mozc.android.inputmethod.japanese.HardwareKeyboard.HardwareKeyboadSpecificationInterface;
import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.KeycodeConverter.KeyEventInterface;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.HardwareKeyMap;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.ModifierKey;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.SpecialKey;
import org.mozc.android.inputmethod.japanese.testing.MozcPreferenceUtil;
import org.mozc.android.inputmethod.japanese.testing.Parameter;

import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.test.InstrumentationTestCase;
import android.test.MoreAsserts;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.KeyEvent;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 *
 */
public class HardwareKeyboardSpecificationTest extends InstrumentationTestCase {
  @SmallTest
  public void testDefaultConverter() {
    HardwareKeyboadSpecificationInterface keyboardSpecification =
        HardwareKeyboardSpecification.DEFAULT;

    assertTrue(keyboardSpecification.isKeyToConsume(new KeyEvent(0, 0)));

    {
      KeyEvent keyEvent = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A);

      ProtoCommands.KeyEvent mozcKeyEvent = keyboardSpecification.getMozcKeyEvent(keyEvent);
      assertEquals(keyEvent.getUnicodeChar(), mozcKeyEvent.getKeyCode());

      KeyEventInterface keyEventInterface = keyboardSpecification.getKeyEventInterface(keyEvent);
      assertEquals(keyEvent, keyEventInterface.getNativeEvent());
    }

    {
      KeyEvent keyEvent = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_SPACE);

      ProtoCommands.KeyEvent mozcKeyEvent = keyboardSpecification.getMozcKeyEvent(keyEvent);
      assertFalse(mozcKeyEvent.hasKeyCode());
      assertEquals(SpecialKey.SPACE, mozcKeyEvent.getSpecialKey());

      KeyEventInterface keyEventInterface = keyboardSpecification.getKeyEventInterface(keyEvent);
      assertEquals(keyEvent, keyEventInterface.getNativeEvent());
    }

    {
      KeyEvent keyEvent = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER);

      ProtoCommands.KeyEvent mozcKeyEvent = keyboardSpecification.getMozcKeyEvent(keyEvent);
      assertFalse(mozcKeyEvent.hasKeyCode());
      assertEquals(SpecialKey.ENTER, mozcKeyEvent.getSpecialKey());

      KeyEventInterface keyEventInterface = keyboardSpecification.getKeyEventInterface(keyEvent);
      assertEquals(keyEvent, keyEventInterface.getNativeEvent());
    }

    {
      KeyEvent keyEvent = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A, 0,
                                       KeyEvent.META_SHIFT_ON);

      ProtoCommands.KeyEvent mozcKeyEvent = keyboardSpecification.getMozcKeyEvent(keyEvent);
      assertEquals(keyEvent.getUnicodeChar(), mozcKeyEvent.getKeyCode());
      assertEquals(1, mozcKeyEvent.getModifierKeysCount());
      assertEquals(ModifierKey.SHIFT, mozcKeyEvent.getModifierKeys(0));

      KeyEventInterface keyEventInterface = keyboardSpecification.getKeyEventInterface(keyEvent);
      assertEquals(keyEvent, keyEventInterface.getNativeEvent());
    }

    {
      KeyEvent keyEvent = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A, 0,
                                       KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON);

      ProtoCommands.KeyEvent mozcKeyEvent = keyboardSpecification.getMozcKeyEvent(keyEvent);
      assertEquals(keyEvent.getUnicodeChar(), mozcKeyEvent.getKeyCode());
      HashSet<ModifierKey> expected = new HashSet<ProtoCommands.KeyEvent.ModifierKey>();
      expected.add(ModifierKey.SHIFT);
      expected.add(ModifierKey.ALT);
      MoreAsserts.assertEquals(expected,
                               new HashSet<ModifierKey>(mozcKeyEvent.getModifierKeysList()));

      KeyEventInterface keyEventInterface = keyboardSpecification.getKeyEventInterface(keyEvent);
      assertEquals(keyEvent, keyEventInterface.getNativeEvent());
    }

    {
      KeyEvent keyEvent = new KeyEvent(0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A, 0,
                                       KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON |
                                           KeyEvent.META_CTRL_ON);

      ProtoCommands.KeyEvent mozcKeyEvent = keyboardSpecification.getMozcKeyEvent(keyEvent);
      assertEquals(keyEvent.getUnicodeChar(), mozcKeyEvent.getKeyCode());
      HashSet<ModifierKey> expected = new HashSet<ProtoCommands.KeyEvent.ModifierKey>();
      expected.add(ModifierKey.SHIFT);
      expected.add(ModifierKey.ALT);
      expected.add(ModifierKey.CTRL);
      MoreAsserts.assertEquals(expected,
                               new HashSet<ModifierKey>(mozcKeyEvent.getModifierKeysList()));

      KeyEventInterface keyEventInterface = keyboardSpecification.getKeyEventInterface(keyEvent);
      assertEquals(keyEvent, keyEventInterface.getNativeEvent());
    }

    assertNull(keyboardSpecification.getCompositionSwitchMode(new KeyEvent(0, 0)));

    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 keyboardSpecification.getKanaKeyboardSpecification());

    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_ALPHABET,
                 keyboardSpecification.getAlphabetKeyboardSpecification());
  }

  @SmallTest
  public void testOADG109A() {
    HardwareKeyboadSpecificationInterface keyboardSpecification =
        HardwareKeyboardSpecification.JAPANESE109A;

    class TestData extends Parameter{
      final int scanCode;
      final int metaState;
      final int expectKeyCode;
      final SpecialKey expectSpecialKey;
      final CompositionSwitchMode expectCompositionSwitchMode;
      final Set<ModifierKey> expectModifierKeySet;
      TestData(int scanCode, int metaState,
               int expectKeyCode, SpecialKey expectSpecialKey,
               Set<ModifierKey> expectModifierKeySet,
               CompositionSwitchMode expectCompositionSwitchMode) {
        this.scanCode = scanCode;
        this.metaState = metaState;
        this.expectKeyCode = expectKeyCode;
        this.expectSpecialKey = expectSpecialKey;
        this.expectCompositionSwitchMode = expectCompositionSwitchMode;
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

    TestData[] testDataList = {
        new TestData(0x02, 0, '1', null, noModifierKeys, null),
        new TestData(0x02, KeyEvent.META_SHIFT_ON, '!', null, noModifierKeys, null),
        new TestData(0x03, 0, '2', null, noModifierKeys, null),
        new TestData(0x03, KeyEvent.META_SHIFT_ON, '"', null, noModifierKeys, null),
        new TestData(0x04, 0, '3', null, noModifierKeys, null),
        new TestData(0x04, KeyEvent.META_SHIFT_ON, '#', null, noModifierKeys, null),
        new TestData(0x05, 0, '4', null, noModifierKeys, null),
        new TestData(0x05, KeyEvent.META_SHIFT_ON, '$', null, noModifierKeys, null),
        new TestData(0x06, 0, '5', null, noModifierKeys, null),
        new TestData(0x06, KeyEvent.META_SHIFT_ON, '%', null, noModifierKeys, null),
        new TestData(0x07, 0, '6', null, noModifierKeys, null),
        new TestData(0x07, KeyEvent.META_SHIFT_ON, '&', null, noModifierKeys, null),
        new TestData(0x08, 0, '7', null, noModifierKeys, null),
        new TestData(0x08, KeyEvent.META_SHIFT_ON, '\'', null, noModifierKeys, null),
        new TestData(0x09, 0, '8', null, noModifierKeys, null),
        new TestData(0x09, KeyEvent.META_SHIFT_ON, '(', null, noModifierKeys, null),
        new TestData(0x0A, 0, '9', null, noModifierKeys, null),
        new TestData(0x0A, KeyEvent.META_SHIFT_ON, ')', null, noModifierKeys, null),
        new TestData(0x0B, 0, '0', null, noModifierKeys, null),
        new TestData(0x0B, KeyEvent.META_SHIFT_ON, '~', null, noModifierKeys, null),
        new TestData(0x0C, 0, '-', null, noModifierKeys, null),
        new TestData(0x0C, KeyEvent.META_SHIFT_ON, '=', null, noModifierKeys, null),
        new TestData(0x0D, 0, '^', null, noModifierKeys, null),
        new TestData(0x0D, KeyEvent.META_SHIFT_ON, '~', null, noModifierKeys, null),
        new TestData(0x7C, 0, '\\', null, noModifierKeys, null),
        new TestData(0x7C, KeyEvent.META_SHIFT_ON, '|', null, noModifierKeys, null),

        new TestData(0x10, 0, 'q', null, noModifierKeys, null),
        new TestData(0x10, KeyEvent.META_SHIFT_ON, 'Q', null, noModifierKeys, null),
        new TestData(0x11, 0, 'w', null, noModifierKeys, null),
        new TestData(0x11, KeyEvent.META_SHIFT_ON, 'W', null, noModifierKeys, null),
        new TestData(0x12, 0, 'e', null, noModifierKeys, null),
        new TestData(0x12, KeyEvent.META_SHIFT_ON, 'E', null, noModifierKeys, null),
        new TestData(0x13, 0, 'r', null, noModifierKeys, null),
        new TestData(0x13, KeyEvent.META_SHIFT_ON, 'R', null, noModifierKeys, null),
        new TestData(0x14, 0, 't', null, noModifierKeys, null),
        new TestData(0x14, KeyEvent.META_SHIFT_ON, 'T', null, noModifierKeys, null),
        new TestData(0x15, 0, 'y', null, noModifierKeys, null),
        new TestData(0x15, KeyEvent.META_SHIFT_ON, 'Y', null, noModifierKeys, null),
        new TestData(0x16, 0, 'u', null, noModifierKeys, null),
        new TestData(0x16, KeyEvent.META_SHIFT_ON, 'U', null, noModifierKeys, null),
        new TestData(0x17, 0, 'i', null, noModifierKeys, null),
        new TestData(0x17, KeyEvent.META_SHIFT_ON, 'I', null, noModifierKeys, null),
        new TestData(0x18, 0, 'o', null, noModifierKeys, null),
        new TestData(0x18, KeyEvent.META_SHIFT_ON, 'O', null, noModifierKeys, null),
        new TestData(0x19, 0, 'p', null, noModifierKeys, null),
        new TestData(0x19, KeyEvent.META_SHIFT_ON, 'P', null, noModifierKeys, null),
        new TestData(0x1A, 0, '@', null, noModifierKeys, null),
        new TestData(0x1A, KeyEvent.META_SHIFT_ON, '`', null, noModifierKeys, null),
        new TestData(0x1B, 0, '[', null, noModifierKeys, null),
        new TestData(0x1B, KeyEvent.META_SHIFT_ON, '{', null, noModifierKeys, null),

        new TestData(0x1E, 0, 'a', null, noModifierKeys, null),
        new TestData(0x1E, KeyEvent.META_SHIFT_ON, 'A', null, noModifierKeys, null),
        new TestData(0x1F, 0, 's', null, noModifierKeys, null),
        new TestData(0x1F, KeyEvent.META_SHIFT_ON, 'S', null, noModifierKeys, null),
        new TestData(0x20, 0, 'd', null, noModifierKeys, null),
        new TestData(0x20, KeyEvent.META_SHIFT_ON, 'D', null, noModifierKeys, null),
        new TestData(0x21, 0, 'f', null, noModifierKeys, null),
        new TestData(0x21, KeyEvent.META_SHIFT_ON, 'F', null, noModifierKeys, null),
        new TestData(0x22, 0, 'g', null, noModifierKeys, null),
        new TestData(0x22, KeyEvent.META_SHIFT_ON, 'G', null, noModifierKeys, null),
        new TestData(0x23, 0, 'h', null, noModifierKeys, null),
        new TestData(0x23, KeyEvent.META_SHIFT_ON, 'H', null, noModifierKeys, null),
        new TestData(0x24, 0, 'j', null, noModifierKeys, null),
        new TestData(0x24, KeyEvent.META_SHIFT_ON, 'J', null, noModifierKeys, null),
        new TestData(0x25, 0, 'k', null, noModifierKeys, null),
        new TestData(0x25, KeyEvent.META_SHIFT_ON, 'K', null, noModifierKeys, null),
        new TestData(0x26, 0, 'l', null, noModifierKeys, null),
        new TestData(0x26, KeyEvent.META_SHIFT_ON, 'L', null, noModifierKeys, null),
        new TestData(0x27, 0, ';', null, noModifierKeys, null),
        new TestData(0x27, KeyEvent.META_SHIFT_ON, '+', null, noModifierKeys, null),
        new TestData(0x28, 0, ':', null, noModifierKeys, null),
        new TestData(0x28, KeyEvent.META_SHIFT_ON, '*', null, noModifierKeys, null),
        new TestData(0x2B, 0, ']', null, noModifierKeys, null),
        new TestData(0x2B, KeyEvent.META_SHIFT_ON, '}', null, noModifierKeys, null),

        new TestData(0x2C, 0, 'z', null, noModifierKeys, null),
        new TestData(0x2C, KeyEvent.META_SHIFT_ON, 'Z', null, noModifierKeys, null),
        new TestData(0x2D, 0, 'x', null, noModifierKeys, null),
        new TestData(0x2D, KeyEvent.META_SHIFT_ON, 'X', null, noModifierKeys, null),
        new TestData(0x2E, 0, 'c', null, noModifierKeys, null),
        new TestData(0x2E, KeyEvent.META_SHIFT_ON, 'C', null, noModifierKeys, null),
        new TestData(0x2F, 0, 'v', null, noModifierKeys, null),
        new TestData(0x2F, KeyEvent.META_SHIFT_ON, 'V', null, noModifierKeys, null),
        new TestData(0x30, 0, 'b', null, noModifierKeys, null),
        new TestData(0x30, KeyEvent.META_SHIFT_ON, 'B', null, noModifierKeys, null),
        new TestData(0x31, 0, 'n', null, noModifierKeys, null),
        new TestData(0x31, KeyEvent.META_SHIFT_ON, 'N', null, noModifierKeys, null),
        new TestData(0x32, 0, 'm', null, noModifierKeys, null),
        new TestData(0x32, KeyEvent.META_SHIFT_ON, 'M', null, noModifierKeys, null),
        new TestData(0x33, 0, ',', null, noModifierKeys, null),
        new TestData(0x33, KeyEvent.META_SHIFT_ON, '<', null, noModifierKeys, null),
        new TestData(0x34, 0, '.', null, noModifierKeys, null),
        new TestData(0x34, KeyEvent.META_SHIFT_ON, '>', null, noModifierKeys, null),
        new TestData(0x35, 0, '/', null, noModifierKeys, null),
        new TestData(0x35, KeyEvent.META_SHIFT_ON, '?', null, noModifierKeys, null),
        new TestData(0x73, 0, '\\', null, noModifierKeys, null),
        new TestData(0x73, KeyEvent.META_SHIFT_ON, '_', null, noModifierKeys, null),

        new TestData(0x01, 0, 0, SpecialKey.ESCAPE, noModifierKeys, null),
        new TestData(0x01, KeyEvent.META_SHIFT_ON, 0, SpecialKey.ESCAPE, shiftModified, null),
        new TestData(0x01, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0, 
                     SpecialKey.ESCAPE, shiftAltModified, null),
        new TestData(0x01, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.ESCAPE, shiftAltCtrlModified, null),
        new TestData(0x3B, 0, 0, SpecialKey.F1, noModifierKeys, null),
        new TestData(0x3B, KeyEvent.META_SHIFT_ON, 0, SpecialKey.F1, shiftModified, null),
        new TestData(0x3B, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F1, shiftAltModified, null),
        new TestData(0x3B, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F1, shiftAltCtrlModified, null),
        new TestData(0x3C, 0, 0, SpecialKey.F2, noModifierKeys, null),
        new TestData(0x3C, KeyEvent.META_SHIFT_ON, 0, SpecialKey.F2, shiftModified, null),
        new TestData(0x3C, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F2, shiftAltModified, null),
        new TestData(0x3C, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F2, shiftAltCtrlModified, null),
        new TestData(0x3D, 0, 0, SpecialKey.F3, noModifierKeys, null),
        new TestData(0x3D, KeyEvent.META_SHIFT_ON, 0, SpecialKey.F3, shiftModified, null),
        new TestData(0x3D, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F3, shiftAltModified, null),
        new TestData(0x3D, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F3, shiftAltCtrlModified, null),
        new TestData(0x3E, 0, 0, SpecialKey.F4, noModifierKeys, null),
        new TestData(0x3E, KeyEvent.META_SHIFT_ON, 0, SpecialKey.F4, shiftModified, null),
        new TestData(0x3E, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F4, shiftAltModified, null),
        new TestData(0x3E, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F4, shiftAltCtrlModified, null),
        new TestData(0x3F, 0, 0, SpecialKey.F5, noModifierKeys, null),
        new TestData(0x3F, KeyEvent.META_SHIFT_ON, 0, SpecialKey.F5, shiftModified, null),
        new TestData(0x3F, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F5, shiftAltModified, null),
        new TestData(0x3F, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F5, shiftAltCtrlModified, null),
        new TestData(0x40, 0, 0, SpecialKey.F6, noModifierKeys, null),
        new TestData(0x40, KeyEvent.META_SHIFT_ON, 0, SpecialKey.F6, shiftModified, null),
        new TestData(0x40, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F6, shiftAltModified, null),
        new TestData(0x40, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F6, shiftAltCtrlModified, null),
        new TestData(0x41, 0, 0, SpecialKey.F7, noModifierKeys, null),
        new TestData(0x41, KeyEvent.META_SHIFT_ON, 0, SpecialKey.F7, shiftModified, null),
        new TestData(0x41, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F7, shiftAltModified, null),
        new TestData(0x41, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F7, shiftAltCtrlModified, null),
        new TestData(0x42, 0, 0, SpecialKey.F8, noModifierKeys, null),
        new TestData(0x42, KeyEvent.META_SHIFT_ON, 0, SpecialKey.F8, shiftModified, null),
        new TestData(0x42, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F8, shiftAltModified, null),
        new TestData(0x42, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F8, shiftAltCtrlModified, null),
        new TestData(0x43, 0, 0, SpecialKey.F9, noModifierKeys, null),
        new TestData(0x43, KeyEvent.META_SHIFT_ON, 0, SpecialKey.F9, shiftModified, null),
        new TestData(0x43, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F9, shiftAltModified, null),
        new TestData(0x43, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F9, shiftAltCtrlModified, null),
        new TestData(0x44, 0, 0, SpecialKey.F10, noModifierKeys, null),
        new TestData(0x44, KeyEvent.META_SHIFT_ON, 0, SpecialKey.F10, shiftModified, null),
        new TestData(0x44, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F10, shiftAltModified, null),
        new TestData(0x44, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F10, shiftAltCtrlModified, null),
        new TestData(0x57, 0, 0, SpecialKey.F11, noModifierKeys, null),
        new TestData(0x57, KeyEvent.META_SHIFT_ON, 0, SpecialKey.F11, shiftModified, null),
        new TestData(0x57, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F11, shiftAltModified, null),
        new TestData(0x57, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F11, shiftAltCtrlModified, null),
        new TestData(0x58, 0, 0, SpecialKey.F12, noModifierKeys, null),
        new TestData(0x58, KeyEvent.META_SHIFT_ON, 0, SpecialKey.F12, shiftModified, null),
        new TestData(0x58, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.F12, shiftAltModified, null),
        new TestData(0x58, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.F12, shiftAltCtrlModified, null),
        new TestData(0x6F, 0, 0, SpecialKey.DEL, noModifierKeys, null),
        new TestData(0x6F, KeyEvent.META_SHIFT_ON, 0, SpecialKey.DEL, shiftModified, null),
        new TestData(0x6F, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.DEL, shiftAltModified, null),
        new TestData(0x6F, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.DEL, shiftAltCtrlModified, null),

        new TestData(0x0E, 0, 0, SpecialKey.BACKSPACE, noModifierKeys, null),
        new TestData(0x0E, KeyEvent.META_SHIFT_ON, 0, SpecialKey.BACKSPACE, shiftModified, null),
        new TestData(0x0E, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.BACKSPACE, shiftAltModified, null),
        new TestData(0x0E, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.BACKSPACE, shiftAltCtrlModified, null),
        new TestData(0x0F, 0, 0, SpecialKey.TAB, noModifierKeys, null),
        new TestData(0x0F, KeyEvent.META_SHIFT_ON, 0, SpecialKey.TAB, shiftModified, null),
        new TestData(0x0F, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.TAB, shiftAltModified, null),
        new TestData(0x0F, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.TAB, shiftAltCtrlModified, null),
        new TestData(0x1C, 0, 0, SpecialKey.ENTER, noModifierKeys, null),
        new TestData(0x1C, KeyEvent.META_SHIFT_ON, 0, SpecialKey.ENTER, shiftModified, null),
        new TestData(0x1C, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.ENTER, shiftAltModified, null),
        new TestData(0x1C, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.ENTER, shiftAltCtrlModified, null),

        new TestData(0x5E, 0, 0, SpecialKey.MUHENKAN, noModifierKeys, null),
        new TestData(0x5E, KeyEvent.META_SHIFT_ON, 0, SpecialKey.MUHENKAN, shiftModified, null),
        new TestData(0x5E, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.MUHENKAN, shiftAltModified, null),
        new TestData(0x5E, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.MUHENKAN, shiftAltCtrlModified, null),
        new TestData(0x39, 0, 0, SpecialKey.SPACE, noModifierKeys, null),
        new TestData(0x39, KeyEvent.META_SHIFT_ON, 0, SpecialKey.SPACE, shiftModified, null),
        new TestData(0x39, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.SPACE, shiftAltModified, null),
        new TestData(0x39, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.SPACE, shiftAltCtrlModified, null),
        new TestData(0x5C, 0, 0, SpecialKey.HENKAN, noModifierKeys, null),
        new TestData(0x5C, KeyEvent.META_SHIFT_ON, 0, SpecialKey.HENKAN, shiftModified, null),
        new TestData(0x5C, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.HENKAN, shiftAltModified, null),
        new TestData(0x5C, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.HENKAN, shiftAltCtrlModified, null),
        new TestData(0x5D, 0, 0, SpecialKey.KANA, noModifierKeys, null),
        new TestData(0x5D, KeyEvent.META_SHIFT_ON, 0, SpecialKey.KANA, shiftModified, null),
        new TestData(0x5D, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.KANA, shiftAltModified, null),
        new TestData(0x5D, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.KANA, shiftAltCtrlModified, null),

        new TestData(0x67, 0, 0, SpecialKey.UP, noModifierKeys, null),
        new TestData(0x67, KeyEvent.META_SHIFT_ON, 0, SpecialKey.UP, shiftModified, null),
        new TestData(0x67, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.UP, shiftAltModified, null),
        new TestData(0x67, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.UP, shiftAltCtrlModified, null),
        new TestData(0x69, 0, 0, SpecialKey.LEFT, noModifierKeys, null),
        new TestData(0x69, KeyEvent.META_SHIFT_ON, 0, SpecialKey.LEFT, shiftModified, null),
        new TestData(0x69, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.LEFT, shiftAltModified, null),
        new TestData(0x69, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.LEFT, shiftAltCtrlModified, null),
        new TestData(0x6C, 0, 0, SpecialKey.DOWN, noModifierKeys, null),
        new TestData(0x6C, KeyEvent.META_SHIFT_ON, 0, SpecialKey.DOWN, shiftModified, null),
        new TestData(0x6C, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.DOWN, shiftAltModified, null),
        new TestData(0x6C, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.DOWN, shiftAltCtrlModified, null),
        new TestData(0x6A, 0, 0, SpecialKey.RIGHT, noModifierKeys, null),
        new TestData(0x6A, KeyEvent.META_SHIFT_ON, 0, SpecialKey.RIGHT, shiftModified, null),
        new TestData(0x6A, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.RIGHT, shiftAltModified, null),
        new TestData(0x6A, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.RIGHT, shiftAltCtrlModified, null),

        new TestData(0x66, 0, 0, SpecialKey.HOME, noModifierKeys, null),
        new TestData(0x66, KeyEvent.META_SHIFT_ON, 0, SpecialKey.HOME, shiftModified, null),
        new TestData(0x66, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.HOME, shiftAltModified, null),
        new TestData(0x66, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.HOME, shiftAltCtrlModified, null),
        new TestData(0x6B, 0, 0, SpecialKey.END, noModifierKeys, null),
        new TestData(0x6B, KeyEvent.META_SHIFT_ON, 0, SpecialKey.END, shiftModified, null),
        new TestData(0x6B, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.END, shiftAltModified, null),
        new TestData(0x6B, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.END, shiftAltCtrlModified, null),
        new TestData(0x6E, 0, 0, SpecialKey.INSERT, noModifierKeys, null),
        new TestData(0x6E, KeyEvent.META_SHIFT_ON, 0, SpecialKey.INSERT, shiftModified, null),
        new TestData(0x6E, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.INSERT, shiftAltModified, null),
        new TestData(0x6E, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.INSERT, shiftAltCtrlModified, null),
        new TestData(0x68, 0, 0, SpecialKey.PAGE_UP, noModifierKeys, null),
        new TestData(0x68, KeyEvent.META_SHIFT_ON, 0, SpecialKey.PAGE_UP, shiftModified, null),
        new TestData(0x68, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.PAGE_UP, shiftAltModified, null),
        new TestData(0x68, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.PAGE_UP, shiftAltCtrlModified, null),
        new TestData(0x6D, 0, 0, SpecialKey.PAGE_DOWN, noModifierKeys, null),
        new TestData(0x6D, KeyEvent.META_SHIFT_ON, 0, SpecialKey.PAGE_DOWN, shiftModified, null),
        new TestData(0x6D, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.PAGE_DOWN, shiftAltModified, null),
        new TestData(0x6D, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.PAGE_DOWN, shiftAltCtrlModified, null),

        new TestData(0x62, 0, 0, SpecialKey.DIVIDE, noModifierKeys, null),
        new TestData(0x62, KeyEvent.META_SHIFT_ON, 0, SpecialKey.DIVIDE, shiftModified, null),
        new TestData(0x62, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.DIVIDE, shiftAltModified, null),
        new TestData(0x62, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.DIVIDE, shiftAltCtrlModified, null),
        new TestData(0x37, 0, 0, SpecialKey.MULTIPLY, noModifierKeys, null),
        new TestData(0x37, KeyEvent.META_SHIFT_ON, 0, SpecialKey.MULTIPLY, shiftModified, null),
        new TestData(0x37, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.MULTIPLY, shiftAltModified, null),
        new TestData(0x37, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.MULTIPLY, shiftAltCtrlModified, null),
        new TestData(0x4A, 0, 0, SpecialKey.SUBTRACT, noModifierKeys, null),
        new TestData(0x4A, KeyEvent.META_SHIFT_ON, 0, SpecialKey.SUBTRACT, shiftModified, null),
        new TestData(0x4A, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.SUBTRACT, shiftAltModified, null),
        new TestData(0x4A, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.SUBTRACT, shiftAltCtrlModified, null),
        new TestData(0x4E, 0, 0, SpecialKey.ADD, noModifierKeys, null),
        new TestData(0x4E, KeyEvent.META_SHIFT_ON, 0, SpecialKey.ADD, shiftModified, null),
        new TestData(0x4E, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.ADD, shiftAltModified, null),
        new TestData(0x4E, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.ADD, shiftAltCtrlModified, null),
        new TestData(0x60, 0, 0, SpecialKey.SEPARATOR, noModifierKeys, null),
        new TestData(0x60, KeyEvent.META_SHIFT_ON, 0, SpecialKey.SEPARATOR, shiftModified, null),
        new TestData(0x60, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.SEPARATOR, shiftAltModified, null),
        new TestData(0x60, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.SEPARATOR, shiftAltCtrlModified, null),
        new TestData(0x53, 0, 0, SpecialKey.DECIMAL, noModifierKeys, null),
        new TestData(0x53, KeyEvent.META_SHIFT_ON, 0, SpecialKey.DECIMAL, shiftModified, null),
        new TestData(0x53, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.DECIMAL, shiftAltModified, null),
        new TestData(0x53, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.DECIMAL, shiftAltCtrlModified, null),

        new TestData(0x52, 0, 0, SpecialKey.NUMPAD0, noModifierKeys, null),
        new TestData(0x52, KeyEvent.META_SHIFT_ON, 0, SpecialKey.NUMPAD0, shiftModified, null),
        new TestData(0x52, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.NUMPAD0, shiftAltModified, null),
        new TestData(0x52, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.NUMPAD0, shiftAltCtrlModified, null),
        new TestData(0x4F, 0, 0, SpecialKey.NUMPAD1, noModifierKeys, null),
        new TestData(0x4F, KeyEvent.META_SHIFT_ON, 0, SpecialKey.NUMPAD1, shiftModified, null),
        new TestData(0x4F, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.NUMPAD1, shiftAltModified, null),
        new TestData(0x4F, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.NUMPAD1, shiftAltCtrlModified, null),
        new TestData(0x50, 0, 0, SpecialKey.NUMPAD2, noModifierKeys, null),
        new TestData(0x50, KeyEvent.META_SHIFT_ON, 0, SpecialKey.NUMPAD2, shiftModified, null),
        new TestData(0x50, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.NUMPAD2, shiftAltModified, null),
        new TestData(0x50, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.NUMPAD2, shiftAltCtrlModified, null),
        new TestData(0x51, 0, 0, SpecialKey.NUMPAD3, noModifierKeys, null),
        new TestData(0x51, KeyEvent.META_SHIFT_ON, 0, SpecialKey.NUMPAD3, shiftModified, null),
        new TestData(0x51, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.NUMPAD3, shiftAltModified, null),
        new TestData(0x51, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.NUMPAD3, shiftAltCtrlModified, null),
        new TestData(0x4B, 0, 0, SpecialKey.NUMPAD4, noModifierKeys, null),
        new TestData(0x4B, KeyEvent.META_SHIFT_ON, 0, SpecialKey.NUMPAD4, shiftModified, null),
        new TestData(0x4B, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.NUMPAD4, shiftAltModified, null),
        new TestData(0x4B, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.NUMPAD4, shiftAltCtrlModified, null),
        new TestData(0x4C, 0, 0, SpecialKey.NUMPAD5, noModifierKeys, null),
        new TestData(0x4C, KeyEvent.META_SHIFT_ON, 0, SpecialKey.NUMPAD5, shiftModified, null),
        new TestData(0x4C, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.NUMPAD5, shiftAltModified, null),
        new TestData(0x4C, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.NUMPAD5, shiftAltCtrlModified, null),
        new TestData(0x4D, 0, 0, SpecialKey.NUMPAD6, noModifierKeys, null),
        new TestData(0x4D, KeyEvent.META_SHIFT_ON, 0, SpecialKey.NUMPAD6, shiftModified, null),
        new TestData(0x4D, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.NUMPAD6, shiftAltModified, null),
        new TestData(0x4D, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.NUMPAD6, shiftAltCtrlModified, null),
        new TestData(0x47, 0, 0, SpecialKey.NUMPAD7, noModifierKeys, null),
        new TestData(0x47, KeyEvent.META_SHIFT_ON, 0, SpecialKey.NUMPAD7, shiftModified, null),
        new TestData(0x47, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.NUMPAD7, shiftAltModified, null),
        new TestData(0x47, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.NUMPAD7, shiftAltCtrlModified, null),
        new TestData(0x48, 0, 0, SpecialKey.NUMPAD8, noModifierKeys, null),
        new TestData(0x48, KeyEvent.META_SHIFT_ON, 0, SpecialKey.NUMPAD8, shiftModified, null),
        new TestData(0x48, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.NUMPAD8, shiftAltModified, null),
        new TestData(0x48, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.NUMPAD8, shiftAltCtrlModified, null),
        new TestData(0x49, 0, 0, SpecialKey.NUMPAD9, noModifierKeys, null),
        new TestData(0x49, KeyEvent.META_SHIFT_ON, 0, SpecialKey.NUMPAD9, shiftModified, null),
        new TestData(0x49, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON, 0,
                     SpecialKey.NUMPAD9, shiftAltModified, null),
        new TestData(0x49, KeyEvent.META_SHIFT_ON | KeyEvent.META_ALT_ON | KeyEvent.META_CTRL_ON, 0,
                     SpecialKey.NUMPAD9, shiftAltCtrlModified, null),

        new TestData(0x29, 0, 0, null, noModifierKeys, CompositionSwitchMode.TOGGLE),
    };

    for (TestData testData : testDataList) {
      KeyEvent keyEvent = new KeyEvent(0, 0, 0, 0, 0, testData.metaState, 0, testData.scanCode);
      assertTrue(testData.toString(), keyboardSpecification.isKeyToConsume(keyEvent));

      ProtoCommands.KeyEvent mozcKeyEvent = keyboardSpecification.getMozcKeyEvent(keyEvent);
      if (testData.expectKeyCode == 0 && testData.expectSpecialKey == null) {
        assertNull(testData.toString(), mozcKeyEvent);
      } else if (testData.expectKeyCode != 0) {
        assertEquals(testData.toString(), testData.expectKeyCode, mozcKeyEvent.getKeyCode());
        assertFalse(testData.toString(), mozcKeyEvent.hasSpecialKey());
      } else if (testData.expectSpecialKey != null) {
        assertEquals(testData.toString(), testData.expectSpecialKey, mozcKeyEvent.getSpecialKey());
        assertFalse(testData.toString(), mozcKeyEvent.hasKeyCode());
      }

      if (mozcKeyEvent != null) {
        MoreAsserts.assertEquals(testData.toString(), testData.expectModifierKeySet,
                                 new HashSet<ModifierKey>(mozcKeyEvent.getModifierKeysList()));
      }

      assertEquals(testData.toString(), keyEvent,
                   keyboardSpecification.getKeyEventInterface(keyEvent).getNativeEvent());

      assertEquals(testData.toString(), testData.expectCompositionSwitchMode,
                   keyboardSpecification.getCompositionSwitchMode(keyEvent));
    }

    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 keyboardSpecification.getKanaKeyboardSpecification());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_ALPHABET,
                 keyboardSpecification.getAlphabetKeyboardSpecification());
  }

  @SmallTest
  public void testTweleveKeyConverter() {
    HardwareKeyboadSpecificationInterface keyboardSpecification =
        HardwareKeyboardSpecification.TWELVEKEY;

    assertTrue(keyboardSpecification.isKeyToConsume(new KeyEvent(0, 0)));

    KeyEvent keyEventA = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A);
    assertEquals(keyEventA.getUnicodeChar(),
                 keyboardSpecification.getMozcKeyEvent(keyEventA).getKeyCode());

    class TestData extends Parameter {
      final int keyCode;
      final int expectMozcKeyCode;

      TestData(int keyCode, int expectMozcKeyCode) {
        this.keyCode = keyCode;
        this.expectMozcKeyCode = expectMozcKeyCode;
      }
    }

    TestData[] testDataList = {
        new TestData(KeyEvent.KEYCODE_0, '0'),
        new TestData(KeyEvent.KEYCODE_1, '1'),
        new TestData(KeyEvent.KEYCODE_2, '2'),
        new TestData(KeyEvent.KEYCODE_3, '3'),
        new TestData(KeyEvent.KEYCODE_4, '4'),
        new TestData(KeyEvent.KEYCODE_5, '5'),
        new TestData(KeyEvent.KEYCODE_6, '6'),
        new TestData(KeyEvent.KEYCODE_7, '7'),
        new TestData(KeyEvent.KEYCODE_8, '8'),
        new TestData(KeyEvent.KEYCODE_9, '9'),
        new TestData(KeyEvent.KEYCODE_STAR, '*'),
    };

    for (TestData testData : testDataList) {
      KeyEvent keyEvent  = new KeyEvent(KeyEvent.ACTION_DOWN, testData.keyCode);
      assertTrue(testData.toString(), keyboardSpecification.isKeyToConsume(keyEvent));
      assertEquals(testData.toString(), testData.expectMozcKeyCode,
                   keyboardSpecification.getMozcKeyEvent(keyEvent).getKeyCode());
      KeyEventInterface keyEventInterface = keyboardSpecification.getKeyEventInterface(keyEvent);
      assertNull(testData.toString(), keyEventInterface.getNativeEvent());
      assertEquals(testData.toString(), keyEvent.getKeyCode(), keyEventInterface.getKeyCode());
    }

    {
      KeyEvent keyEvent  = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_POUND);
      assertTrue(keyboardSpecification.isKeyToConsume(keyEvent));
      assertEquals(SpecialKey.ENTER,
                   keyboardSpecification.getMozcKeyEvent(keyEvent).getSpecialKey());
      KeyEventInterface keyEventInterface = keyboardSpecification.getKeyEventInterface(keyEvent);
      assertNull(keyEventInterface.getNativeEvent());
      assertEquals(KeyEvent.KEYCODE_ENTER, keyEventInterface.getKeyCode());
    }

    {
      KeyEvent keyEvent  = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_CENTER);
      assertTrue(keyboardSpecification.isKeyToConsume(keyEvent));
      assertEquals(SpecialKey.ENTER,
                   keyboardSpecification.getMozcKeyEvent(keyEvent).getSpecialKey());
      KeyEventInterface keyEventInterface = keyboardSpecification.getKeyEventInterface(keyEvent);
      assertNull(keyEventInterface.getNativeEvent());
      assertEquals(KeyEvent.KEYCODE_ENTER, keyEventInterface.getKeyCode());
    }

    assertNull(keyboardSpecification.getCompositionSwitchMode(new KeyEvent(0, 0)));

    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA,
                 keyboardSpecification.getKanaKeyboardSpecification());
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_ALPHABET,
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
        new TestData(null, Configuration.KEYBOARD_12KEY, false, HardwareKeyMap.TWELVEKEY),
        new TestData(null, Configuration.KEYBOARD_QWERTY, false, HardwareKeyMap.JAPANESE109A),
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
