// Copyright 2010-2014, Google Inc.
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

import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.KeycodeConverter.KeyEventInterface;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.HardwareKeyMap;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import com.google.common.annotations.VisibleForTesting;

/**
 * Converter from android key events to mozc key events.
 * It has a composition mode for hardware keyboards.
 *
 */
public class HardwareKeyboard {

  /**
   * Used to switch the composition mode of harwdware keyboard.
   **/
  public static enum CompositionSwitchMode {
    TOGGLE,
    KANA,
    ALPHABET
  }

  interface HardwareKeyboadSpecificationInterface {
    HardwareKeyMap getHardwareKeyMap();

    ProtoCommands.KeyEvent getMozcKeyEvent(android.view.KeyEvent keyEvent);

    KeyEventInterface getKeyEventInterface(android.view.KeyEvent keyEvent);

    CompositionSwitchMode getCompositionSwitchMode(android.view.KeyEvent keyEvent);

    boolean isKeyToConsume(android.view.KeyEvent keyEvent);

    KeyboardSpecification getKanaKeyboardSpecification();

    KeyboardSpecification getAlphabetKeyboardSpecification();
  }

  @VisibleForTesting HardwareKeyboadSpecificationInterface hardwareKeyboardSpecification =
      HardwareKeyboardSpecification.JAPANESE109A;

  private CompositionMode compositionMode = CompositionMode.HIRAGANA;

  public ProtoCommands.KeyEvent getMozcKeyEvent(android.view.KeyEvent keyEvent) {
    return hardwareKeyboardSpecification.getMozcKeyEvent(keyEvent);
  }

  public KeyEventInterface getKeyEventInterface(android.view.KeyEvent keyEvent) {
    return hardwareKeyboardSpecification.getKeyEventInterface(keyEvent);
  }

  public boolean setCompositionModeByKey(android.view.KeyEvent keyEvent) {
    CompositionSwitchMode compositionSwitchMode =
        hardwareKeyboardSpecification.getCompositionSwitchMode(keyEvent);

    if (compositionSwitchMode == null) {
      return false;
    }

    setCompositionMode(compositionSwitchMode);

    // Changed composition mode.
    return true;
  }

  public void setCompositionMode(CompositionSwitchMode compositionSwitchMode) {
    switch (compositionSwitchMode) {
      case TOGGLE:
        if (compositionMode == CompositionMode.HIRAGANA) {
          compositionMode = CompositionMode.HALF_ASCII;
        } else {
          compositionMode = CompositionMode.HIRAGANA;
        }
        break;
      case KANA:
        compositionMode = CompositionMode.HIRAGANA;
        break;
      case ALPHABET:
        compositionMode = CompositionMode.HALF_ASCII;
        break;
    }
  }

  public KeyboardSpecification getKeyboardSpecification() {
    switch(compositionMode) {
      case HIRAGANA:
        return hardwareKeyboardSpecification.getKanaKeyboardSpecification();
      default:
        return hardwareKeyboardSpecification.getAlphabetKeyboardSpecification();
    }
  }

  public CompositionMode getCompositionMode() {
    return compositionMode;
  }

  public boolean isKeyToConsume(android.view.KeyEvent keyEvent) {
    return hardwareKeyboardSpecification.isKeyToConsume(keyEvent);
  }

  public void setHardwareKeyMap(HardwareKeyMap hardwareKeyMap) {
    HardwareKeyboadSpecificationInterface nextSpecification =
        HardwareKeyboardSpecification.getHardwareKeyboardSpecification(hardwareKeyMap);
    if (nextSpecification == null) {
      MozcLog.w("Invalid HardwareKeyMap: " + hardwareKeyMap);
      return;
    }
    hardwareKeyboardSpecification = nextSpecification;
    setCompositionMode(CompositionSwitchMode.KANA);
  }

  public HardwareKeyMap getHardwareKeyMap() {
    return hardwareKeyboardSpecification.getHardwareKeyMap();
  }
}
