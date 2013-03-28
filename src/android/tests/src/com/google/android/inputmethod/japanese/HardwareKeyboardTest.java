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

import static org.easymock.EasyMock.expect;

import org.mozc.android.inputmethod.japanese.HardwareKeyboard.CompositionSwitchMode;
import org.mozc.android.inputmethod.japanese.HardwareKeyboard.HardwareKeyboadSpecificationInterface;
import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.KeycodeConverter.KeyEventInterface;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.HardwareKeyMap;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.VisibilityProxy;

import android.test.suitebuilder.annotation.SmallTest;
import android.view.KeyEvent;

/**
 */
public class HardwareKeyboardTest extends InstrumentationTestCaseWithMock {
  @SmallTest
  public void testGetMozcKeyEvent() {
    HardwareKeyboard hardwareKeyboard = new HardwareKeyboard();
    HardwareKeyboadSpecificationInterface hardwareKeyboardSpecification =
        createMock(HardwareKeyboadSpecificationInterface.class);
    VisibilityProxy.setField(hardwareKeyboard, "hardwareKeyboardSpecification",
                             hardwareKeyboardSpecification);

    android.view.KeyEvent keyEvent = new KeyEvent(1, 1);
    ProtoCommands.KeyEvent returnKey = ProtoCommands.KeyEvent.newBuilder().setKeyCode(1).build();
    expect(hardwareKeyboardSpecification.getMozcKeyEvent(keyEvent)).andReturn(returnKey);

    replayAll();

    assertEquals(returnKey, hardwareKeyboard.getMozcKeyEvent(keyEvent));

    verifyAll();
  }

  @SmallTest
  public void testGetKeyEventInterface() {
    HardwareKeyboard hardwareKeyboard = new HardwareKeyboard();
    HardwareKeyboadSpecificationInterface hardwareKeyboardSpecification =
        createMock(HardwareKeyboadSpecificationInterface.class);
    VisibilityProxy.setField(hardwareKeyboard, "hardwareKeyboardSpecification",
                             hardwareKeyboardSpecification);

    android.view.KeyEvent keyEvent = new KeyEvent(1, 1);
    KeyEventInterface keyEventInterface = createMock(KeyEventInterface.class);
    expect(hardwareKeyboardSpecification.getKeyEventInterface(keyEvent))
        .andReturn(keyEventInterface);

    replayAll();

    assertEquals(keyEventInterface, hardwareKeyboard.getKeyEventInterface(keyEvent));

    verifyAll();
  }

  @SmallTest
  public void testUpdateKeyboardSpecification() {
    HardwareKeyboard hardwareKeyboard = new HardwareKeyboard();
    HardwareKeyboadSpecificationInterface hardwareKeyboardSpecification =
        createMock(HardwareKeyboadSpecificationInterface.class);
    VisibilityProxy.setField(hardwareKeyboard, "hardwareKeyboardSpecification",
                             hardwareKeyboardSpecification);

    expect(hardwareKeyboardSpecification.getKanaKeyboardSpecification())
        .andStubReturn(KeyboardSpecification.HARDWARE_QWERTY_KANA);
    expect(hardwareKeyboardSpecification.getAlphabetKeyboardSpecification())
        .andStubReturn(KeyboardSpecification.HARDWARE_QWERTY_ALPHABET);

    android.view.KeyEvent keyEvent1 = new KeyEvent(1, 1);
    expect(hardwareKeyboardSpecification.getCompositionSwitchMode(keyEvent1))
        .andReturn(CompositionSwitchMode.TOGGLE);
    android.view.KeyEvent keyEvent2 = new KeyEvent(2, 2);
    expect(hardwareKeyboardSpecification.getCompositionSwitchMode(keyEvent2))
        .andReturn(CompositionSwitchMode.KANA);
    android.view.KeyEvent keyEvent3 = new KeyEvent(3, 3);
    expect(hardwareKeyboardSpecification.getCompositionSwitchMode(keyEvent3))
        .andReturn(CompositionSwitchMode.ALPHABET);
    android.view.KeyEvent keyEvent4 = new KeyEvent(4, 4);
    expect(hardwareKeyboardSpecification.getCompositionSwitchMode(keyEvent4))
        .andReturn(null);

    replayAll();

    assertEquals(CompositionMode.HIRAGANA, hardwareKeyboard.getCompositionMode());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 hardwareKeyboard.getKeyboardSpecification());
    assertTrue(hardwareKeyboard.setCompositionModeByKey(keyEvent1));
    assertEquals(CompositionMode.HALF_ASCII, hardwareKeyboard.getCompositionMode());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_ALPHABET,
                 hardwareKeyboard.getKeyboardSpecification());
    assertTrue(hardwareKeyboard.setCompositionModeByKey(keyEvent2));
    assertEquals(CompositionMode.HIRAGANA, hardwareKeyboard.getCompositionMode());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_KANA,
                 hardwareKeyboard.getKeyboardSpecification());
    assertTrue(hardwareKeyboard.setCompositionModeByKey(keyEvent3));
    assertEquals(CompositionMode.HALF_ASCII, hardwareKeyboard.getCompositionMode());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_ALPHABET,
                 hardwareKeyboard.getKeyboardSpecification());
    assertFalse(hardwareKeyboard.setCompositionModeByKey(keyEvent4));
    assertEquals(CompositionMode.HALF_ASCII, hardwareKeyboard.getCompositionMode());
    assertEquals(KeyboardSpecification.HARDWARE_QWERTY_ALPHABET,
                 hardwareKeyboard.getKeyboardSpecification());

    verifyAll();
  }

  @SmallTest
  public void testIsKeyToConsume() {
    HardwareKeyboard hardwareKeyboard = new HardwareKeyboard();
    HardwareKeyboadSpecificationInterface hardwareKeyboardSpecification =
        createMock(HardwareKeyboadSpecificationInterface.class);
    VisibilityProxy.setField(hardwareKeyboard, "hardwareKeyboardSpecification",
                             hardwareKeyboardSpecification);

    android.view.KeyEvent keyEvent1 = new KeyEvent(1, 1);
    expect(hardwareKeyboardSpecification.isKeyToConsume(keyEvent1)).andReturn(true);
    android.view.KeyEvent keyEvent2 = new KeyEvent(2, 2);
    expect(hardwareKeyboardSpecification.isKeyToConsume(keyEvent2)).andReturn(false);

    replayAll();

    assertTrue(hardwareKeyboard.isKeyToConsume(keyEvent1));
    assertFalse(hardwareKeyboard.isKeyToConsume(keyEvent2));

    verifyAll();
  }

  @SmallTest
  public void testSetHardwareKeyMap() {
    HardwareKeyboard hardwareKeyboard = new HardwareKeyboard();
    hardwareKeyboard.setHardwareKeyMap(HardwareKeyMap.DEFAULT);
    assertEquals(HardwareKeyboardSpecification.DEFAULT,
                 VisibilityProxy.getField(hardwareKeyboard, "hardwareKeyboardSpecification"));
    assertEquals(HardwareKeyMap.DEFAULT, hardwareKeyboard.getHardwareKeyMap());
    hardwareKeyboard.setHardwareKeyMap(HardwareKeyMap.JAPANESE109A);
    assertEquals(HardwareKeyboardSpecification.JAPANESE109A,
                 VisibilityProxy.getField(hardwareKeyboard, "hardwareKeyboardSpecification"));
    assertEquals(HardwareKeyMap.JAPANESE109A, hardwareKeyboard.getHardwareKeyMap());
    hardwareKeyboard.setHardwareKeyMap(HardwareKeyMap.TWELVEKEY);
    assertEquals(HardwareKeyboardSpecification.TWELVEKEY,
                 VisibilityProxy.getField(hardwareKeyboard, "hardwareKeyboardSpecification"));
    assertEquals(HardwareKeyMap.TWELVEKEY, hardwareKeyboard.getHardwareKeyMap());
  }
}
