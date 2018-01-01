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

import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.ModifierKey;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.SpecialKey;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

/**
 * Converts Androids's KeyEvent to Mozc's KeyEvent.
 *
 * Because Mozc to Android conversion is impossible (by information lack), such
 * feature is not provided.
 *
 */
public class KeycodeConverter {

  /**
   * KeyEventInterface just enables lazy keycode evaluation than android.view.KeyEvent.
   * This interface is only used from InputMethodService.sendDownUpKeyEvents.
   * The value returned by {@code getKeyCode} method is used only when a user's input
   * (from s/w or h/w keyboard) is not consumed by Mozc server.
   * In such a case we send back the event as a key-code to the framework.
   * If consumed, {@code getKeyCode} is not invoked.
   */
  public interface KeyEventInterface {
    int getKeyCode();
    Optional<android.view.KeyEvent> getNativeEvent();
  }

  private static final int ASCII_MIN = 32; // Space.
  private static final int ASCII_MAX = 126; // Tilde.
  private static final int NUM_ASCII = ASCII_MAX - ASCII_MIN + 1;
  // Pre-generated KeyEvents for commonly used key codes.
  private static final ProtoCommands.KeyEvent[] keyCodeToMozcKeyCodeEvent =
      new ProtoCommands.KeyEvent[NUM_ASCII];

  public static final ProtoCommands.KeyEvent SPECIALKEY_SPACE =
      ProtoCommands.KeyEvent.newBuilder().setSpecialKey(SpecialKey.SPACE).build();
  // TODO(matsuzakit): UP (and DOWN) is no more used. Remove.
  public static final ProtoCommands.KeyEvent SPECIALKEY_UP =
      ProtoCommands.KeyEvent.newBuilder().setSpecialKey(SpecialKey.UP).build();
  public static final ProtoCommands.KeyEvent SPECIALKEY_VIRTUAL_LEFT =
      ProtoCommands.KeyEvent.newBuilder().setSpecialKey(SpecialKey.VIRTUAL_LEFT).build();
  public static final ProtoCommands.KeyEvent SPECIALKEY_VIRTUAL_RIGHT =
      ProtoCommands.KeyEvent.newBuilder().setSpecialKey(SpecialKey.VIRTUAL_RIGHT).build();
  public static final ProtoCommands.KeyEvent SPECIALKEY_VIRTUAL_ENTER =
      ProtoCommands.KeyEvent.newBuilder().setSpecialKey(SpecialKey.VIRTUAL_ENTER).build();
  public static final ProtoCommands.KeyEvent SPECIALKEY_DOWN =
      ProtoCommands.KeyEvent.newBuilder().setSpecialKey(SpecialKey.DOWN).build();
  public static final ProtoCommands.KeyEvent SPECIALKEY_BACKSPACE =
      ProtoCommands.KeyEvent.newBuilder().setSpecialKey(SpecialKey.BACKSPACE).build();
  public static final ProtoCommands.KeyEvent SPECIALKEY_ESCAPE =
      ProtoCommands.KeyEvent.newBuilder().setSpecialKey(SpecialKey.ESCAPE).build();
  public static final ProtoCommands.KeyEvent SPECIALKEY_SHIFT_LEFT =
      ProtoCommands.KeyEvent.newBuilder()
          .setSpecialKey(SpecialKey.LEFT)
          .addModifierKeys(ModifierKey.SHIFT)
          .build();
  public static final ProtoCommands.KeyEvent SPECIALKEY_SHIFT_RIGHT =
      ProtoCommands.KeyEvent.newBuilder()
          .setSpecialKey(SpecialKey.RIGHT)
          .addModifierKeys(ModifierKey.SHIFT)
          .build();
  public static final ProtoCommands.KeyEvent SPECIALKEY_CTRL_BACKSPACE =
      ProtoCommands.KeyEvent.newBuilder()
          .setSpecialKey(SpecialKey.BACKSPACE)
          .addModifierKeys(ModifierKey.CTRL)
          .build();

  static {
    for (int i = ASCII_MIN; i <= ASCII_MAX; ++i) {
      keyCodeToMozcKeyCodeEvent[i - ASCII_MIN] =
          ProtoCommands.KeyEvent.newBuilder().setKeyCode(i).build();
    }
  }

  public static ProtoCommands.KeyEvent getMozcKeyEvent(int keyCode) {
    int offsetKeyCode = keyCode - ASCII_MIN;
    if (offsetKeyCode >= 0 && offsetKeyCode < NUM_ASCII) {
      return keyCodeToMozcKeyCodeEvent[offsetKeyCode];
    }
    // FallBack.
    return ProtoCommands.KeyEvent.newBuilder().setKeyCode(keyCode).build();
  }

  public static KeyEventInterface getKeyEventInterface(final android.view.KeyEvent keyEvent) {
    Preconditions.checkNotNull(keyEvent);
    return new KeyEventInterface() {

      @Override
      public int getKeyCode() {
        return keyEvent.getKeyCode();
      }

      @Override
      public Optional<android.view.KeyEvent> getNativeEvent() {
        return Optional.of(keyEvent);
      }
    };
  }

  public static KeyEventInterface getKeyEventInterface(final int keyCode) {
    return new KeyEventInterface() {

      @Override
      public int getKeyCode() {
        return keyCode;
      }

      @Override
      public Optional<android.view.KeyEvent> getNativeEvent() {
        return Optional.<android.view.KeyEvent>absent();
      }
    };
  }

  public static boolean isMetaKey(android.view.KeyEvent keyEvent) {
    int keyCode = Preconditions.checkNotNull(keyEvent).getKeyCode();
    return keyCode == android.view.KeyEvent.KEYCODE_SHIFT_LEFT ||
        keyCode == android.view.KeyEvent.KEYCODE_SHIFT_RIGHT ||
        keyCode == android.view.KeyEvent.KEYCODE_CTRL_LEFT ||
        keyCode == android.view.KeyEvent.KEYCODE_CTRL_RIGHT ||
        keyCode == android.view.KeyEvent.KEYCODE_ALT_LEFT ||
        keyCode == android.view.KeyEvent.KEYCODE_ALT_RIGHT;
  }
}
