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

import org.mozc.android.inputmethod.japanese.keyboard.Keyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.HardwareKeyMap;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;

import android.test.suitebuilder.annotation.SmallTest;
import android.view.KeyEvent;

/**
 */
public class HardwareKeyboardTest extends InstrumentationTestCaseWithMock {

  @SmallTest
  public void testDefaultKeyboard() {
    HardwareKeyboard hardwareKeyboard = new HardwareKeyboard();
    hardwareKeyboard.setHardwareKeyMap(HardwareKeyMap.DEFAULT);
    assertEquals(HardwareKeyMap.DEFAULT, hardwareKeyboard.getHardwareKeyMap());

    KeyEvent zenhan = new KeyEvent(0, 0, 0, KeyEvent.KEYCODE_ZENKAKU_HANKAKU, 0);

    assertEquals(CompositionMode.HIRAGANA, hardwareKeyboard.getCompositionMode());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 hardwareKeyboard.getKeyboardSpecification());

    hardwareKeyboard.setCompositionModeByKey(zenhan);
    assertEquals(CompositionMode.HALF_ASCII, hardwareKeyboard.getCompositionMode());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_ALPHABET,
                 hardwareKeyboard.getKeyboardSpecification());

    hardwareKeyboard.setCompositionModeByKey(zenhan);
    assertEquals(CompositionMode.HIRAGANA, hardwareKeyboard.getCompositionMode());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 hardwareKeyboard.getKeyboardSpecification());
  }

  @SmallTest
  public void testJapaneseKeyboard() {
    HardwareKeyboard hardwareKeyboard = new HardwareKeyboard();
    hardwareKeyboard.setHardwareKeyMap(HardwareKeyMap.JAPANESE109A);
    assertEquals(HardwareKeyMap.JAPANESE109A, hardwareKeyboard.getHardwareKeyMap());

    KeyEvent zenhan = new KeyEvent(0, 0, 0, KeyEvent.KEYCODE_ZENKAKU_HANKAKU, 0);

    assertEquals(CompositionMode.HIRAGANA, hardwareKeyboard.getCompositionMode());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 hardwareKeyboard.getKeyboardSpecification());

    hardwareKeyboard.setCompositionModeByKey(zenhan);
    assertEquals(CompositionMode.HALF_ASCII, hardwareKeyboard.getCompositionMode());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_ALPHABET,
                 hardwareKeyboard.getKeyboardSpecification());

    hardwareKeyboard.setCompositionModeByKey(zenhan);
    assertEquals(CompositionMode.HIRAGANA, hardwareKeyboard.getCompositionMode());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 hardwareKeyboard.getKeyboardSpecification());
  }
}
