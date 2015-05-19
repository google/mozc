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
    android.view.KeyEvent getNativeEvent();
  }

  private static final int ASCII_MIN = 32; // Space.
  private static final int ASCII_MAX = 126; // Tilde.
  private static final int NUM_ASCII = ASCII_MAX - ASCII_MIN + 1;
  // Pre-generated KeyEvents for commonly used key codes.
  private static final ProtoCommands.KeyEvent[] keyCodeToMozcKeyCodeEvent =
      new ProtoCommands.KeyEvent[NUM_ASCII];

  public static final ProtoCommands.KeyEvent SPECIALKEY_SPACE =
      ProtoCommands.KeyEvent.newBuilder().setSpecialKey(SpecialKey.SPACE).build();
  public static final ProtoCommands.KeyEvent SPECIALKEY_ENTER =
      ProtoCommands.KeyEvent.newBuilder().setSpecialKey(SpecialKey.ENTER).build();
  public static final ProtoCommands.KeyEvent SPECIALKEY_UP =
      ProtoCommands.KeyEvent.newBuilder().setSpecialKey(SpecialKey.UP).build();
  public static final ProtoCommands.KeyEvent SPECIALKEY_LEFT =
      ProtoCommands.KeyEvent.newBuilder().setSpecialKey(SpecialKey.LEFT).build();
  public static final ProtoCommands.KeyEvent SPECIALKEY_RIGHT =
      ProtoCommands.KeyEvent.newBuilder().setSpecialKey(SpecialKey.RIGHT).build();
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

  /**
   * Converts Android's KeyEvent to Mozc's KeyEvent which has only SpecialKey.
   *
   * @param androidKeyEvent
   * @return Mozc's KeyEvent
   */
  public static ProtoCommands.KeyEvent getMozcSpecialKeyEvent(
      android.view.KeyEvent androidKeyEvent) {
    int metaState = androidKeyEvent.getMetaState();
    int keyCode = androidKeyEvent.getKeyCode();
    if (metaState == 0) {
      switch(keyCode) {
        // Space.
        case android.view.KeyEvent.KEYCODE_SPACE:
          return SPECIALKEY_SPACE;
        // Enter.
        case android.view.KeyEvent.KEYCODE_ENTER:
          return SPECIALKEY_ENTER;
        // Up.
        case android.view.KeyEvent.KEYCODE_DPAD_UP:
          return SPECIALKEY_UP;
        // Left.
        case android.view.KeyEvent.KEYCODE_DPAD_LEFT:
          return SPECIALKEY_LEFT;
        // Right.
        case android.view.KeyEvent.KEYCODE_DPAD_RIGHT:
          return SPECIALKEY_RIGHT;
        // Down.
        case android.view.KeyEvent.KEYCODE_DPAD_DOWN:
          return SPECIALKEY_DOWN;
        // Del -> Backspace.
        case android.view.KeyEvent.KEYCODE_DEL:
          return SPECIALKEY_BACKSPACE;
        default:
          return null;
      }
    } else if ((metaState & android.view.KeyEvent.META_SHIFT_ON) != 0) {
      switch(keyCode) {
        // Shift + Left.
        case android.view.KeyEvent.KEYCODE_DPAD_LEFT:
          return SPECIALKEY_SHIFT_LEFT;
        // Shift + Right.
        case android.view.KeyEvent.KEYCODE_DPAD_RIGHT:
          return SPECIALKEY_SHIFT_RIGHT;
        default:
          return null;
      }
    } else if ((metaState & android.view.KeyEvent.META_CTRL_ON) != 0
          && keyCode == android.view.KeyEvent.KEYCODE_DEL) {
      // Ctrl + Del -> Ctrl + Backspace.
      return SPECIALKEY_CTRL_BACKSPACE;
    }
    return null;
  }

  public static KeyEventInterface getKeyEventInterface(final android.view.KeyEvent keyEvent) {
    return new KeyEventInterface() {

      @Override
      public int getKeyCode() {
        return keyEvent.getKeyCode();
      }

      @Override
      public android.view.KeyEvent getNativeEvent() {
        return keyEvent;
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
      public android.view.KeyEvent getNativeEvent() {
        return null;
      }
    };
  }

  public static boolean isSystemKey(android.view.KeyEvent keyEvent) {
    switch(keyEvent.getKeyCode()) {
      case android.view.KeyEvent.KEYCODE_3D_MODE:
      case android.view.KeyEvent.KEYCODE_APP_SWITCH:
        // TODO(yoichio): When we support API 16, enable follow case.
        // case android.view.KeyEvent.KEYCODE_ASSIST:
      case android.view.KeyEvent.KEYCODE_AVR_INPUT:
      case android.view.KeyEvent.KEYCODE_AVR_POWER:
      case android.view.KeyEvent.KEYCODE_BOOKMARK:
      case android.view.KeyEvent.KEYCODE_BACK:
      case android.view.KeyEvent.KEYCODE_CALL:
      case android.view.KeyEvent.KEYCODE_CAMERA:
      case android.view.KeyEvent.KEYCODE_CHANNEL_DOWN:
      case android.view.KeyEvent.KEYCODE_CHANNEL_UP:
      case android.view.KeyEvent.KEYCODE_DVR:
      case android.view.KeyEvent.KEYCODE_FOCUS:
      case android.view.KeyEvent.KEYCODE_HEADSETHOOK:
      case android.view.KeyEvent.KEYCODE_HOME:
      case android.view.KeyEvent.KEYCODE_INFO:
      case android.view.KeyEvent.KEYCODE_LANGUAGE_SWITCH:
      case android.view.KeyEvent.KEYCODE_MANNER_MODE:
      case android.view.KeyEvent.KEYCODE_MEDIA_CLOSE:
      case android.view.KeyEvent.KEYCODE_MEDIA_EJECT:
      case android.view.KeyEvent.KEYCODE_MEDIA_FAST_FORWARD:
      case android.view.KeyEvent.KEYCODE_MEDIA_NEXT:
      case android.view.KeyEvent.KEYCODE_MEDIA_PAUSE:
      case android.view.KeyEvent.KEYCODE_MEDIA_PLAY:
      case android.view.KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
      case android.view.KeyEvent.KEYCODE_MEDIA_PREVIOUS:
      case android.view.KeyEvent.KEYCODE_MEDIA_RECORD:
      case android.view.KeyEvent.KEYCODE_MEDIA_REWIND:
      case android.view.KeyEvent.KEYCODE_MEDIA_STOP:
      case android.view.KeyEvent.KEYCODE_MENU:
      case android.view.KeyEvent.KEYCODE_MUTE:
      case android.view.KeyEvent.KEYCODE_NOTIFICATION:
      case android.view.KeyEvent.KEYCODE_POWER:
      case android.view.KeyEvent.KEYCODE_PROG_BLUE:
      case android.view.KeyEvent.KEYCODE_PROG_GREEN:
      case android.view.KeyEvent.KEYCODE_PROG_RED:
      case android.view.KeyEvent.KEYCODE_PROG_YELLOW:
      case android.view.KeyEvent.KEYCODE_SEARCH:
      case android.view.KeyEvent.KEYCODE_SYSRQ:
      case android.view.KeyEvent.KEYCODE_TV:
      case android.view.KeyEvent.KEYCODE_TV_INPUT:
      case android.view.KeyEvent.KEYCODE_TV_POWER:
      case android.view.KeyEvent.KEYCODE_VOLUME_DOWN:
      case android.view.KeyEvent.KEYCODE_VOLUME_MUTE:
      case android.view.KeyEvent.KEYCODE_VOLUME_UP:
      case android.view.KeyEvent.KEYCODE_ZOOM_IN:
      case android.view.KeyEvent.KEYCODE_ZOOM_OUT:
        return true;
    }
    return false;
  }

  public static boolean isMetaKey(android.view.KeyEvent keyEvent) {
    int keyCode = keyEvent.getKeyCode();
    return keyCode == android.view.KeyEvent.KEYCODE_SHIFT_LEFT ||
        keyCode == android.view.KeyEvent.KEYCODE_SHIFT_RIGHT ||
        keyCode == android.view.KeyEvent.KEYCODE_CTRL_LEFT ||
        keyCode == android.view.KeyEvent.KEYCODE_CTRL_RIGHT ||
        keyCode == android.view.KeyEvent.KEYCODE_ALT_LEFT ||
        keyCode == android.view.KeyEvent.KEYCODE_ALT_RIGHT;
  }
}
