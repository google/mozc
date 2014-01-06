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

import org.mozc.android.inputmethod.japanese.KeycodeConverter.KeyEventInterface;
import org.mozc.android.inputmethod.japanese.keyboard.ProbableKeyEventGuesser;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.ProbableKeyEvent;
import com.google.common.base.Preconditions;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.view.KeyEvent;

import java.util.List;

import javax.annotation.Nullable;

/**
 * Util class for KeyEvents.
 */
public class KeyEventUtil {
  public final int keyCodeUp;
  public final int keyCodeLeft;
  public final int keyCodeRight;
  public final int keyCodeDown;
  public final int keyCodeBackspace;
  public final int keyCodeEnter;
  public final int keyCodeChartypeToKana;
  public final int keyCodeChartypeTo123;
  public final int keyCodeChartypeToAbc;
  public final int keyCodeChartypeToKana123;
  public final int keyCodeChartypeToAbc123;
  public final int keyCodeSymbol;
  public final int keyCodeUndo;
  public final int keyCodeCapslock;
  public final int keyCodeAlt;
  public final int keyCodeMenuDialog;

  private final ProbableKeyEventGuesser guesser;

  public KeyEventUtil(Context context) {
    this(context, new ProbableKeyEventGuesser(context.getAssets()));
  }

  // For testing
  KeyEventUtil(Context context, ProbableKeyEventGuesser guesser) {
    // Prefetch keycodes from resource
    Preconditions.checkNotNull(context);
    Resources res = Preconditions.checkNotNull(context.getResources());
    keyCodeUp = res.getInteger(R.integer.key_up);
    keyCodeLeft = res.getInteger(R.integer.key_left);
    keyCodeRight = res.getInteger(R.integer.key_right);
    keyCodeDown = res.getInteger(R.integer.key_down);
    keyCodeBackspace = res.getInteger(R.integer.key_backspace);
    keyCodeEnter = res.getInteger(R.integer.key_enter);
    keyCodeChartypeToKana = res.getInteger(R.integer.key_chartype_to_kana);
    keyCodeChartypeTo123 = res.getInteger(R.integer.key_chartype_to_123);
    keyCodeChartypeToAbc = res.getInteger(R.integer.key_chartype_to_abc);
    keyCodeChartypeToKana123 = res.getInteger(R.integer.key_chartype_to_kana_123);
    keyCodeChartypeToAbc123 = res.getInteger(R.integer.key_chartype_to_abc_123);
    keyCodeSymbol = res.getInteger(R.integer.key_symbol);
    keyCodeUndo = res.getInteger(R.integer.key_undo);
    keyCodeCapslock = res.getInteger(R.integer.key_capslock);
    keyCodeAlt = res.getInteger(R.integer.key_alt);
    keyCodeMenuDialog = res.getInteger(R.integer.key_menu_dialog);

    this.guesser = Preconditions.checkNotNull(guesser);
  }

  public void setJapaneseKeyboard(@Nullable JapaneseKeyboard japaneseKeyboard) {
    guesser.setJapaneseKeyboard(japaneseKeyboard);
  }

  public void setConfiguration(Configuration newConfig) {
    guesser.setConfiguration(Preconditions.checkNotNull(newConfig));
  }

  public ProtoCommands.KeyEvent createMozcKeyEvent(
      int primaryCode, List<? extends TouchEvent> touchEventList) {
    // Space
    if (primaryCode == ' ') {
      return KeycodeConverter.SPECIALKEY_SPACE;
    }

    // Enter
    if (primaryCode == keyCodeEnter) {
      return KeycodeConverter.SPECIALKEY_ENTER;
    }

    // Backspace
    if (primaryCode == keyCodeBackspace) {
      return KeycodeConverter.SPECIALKEY_BACKSPACE;
    }

    // Up arrow.
    if (primaryCode == keyCodeUp) {
      return KeycodeConverter.SPECIALKEY_UP;
    }

    // Left arrow.
    if (primaryCode == keyCodeLeft) {
      return KeycodeConverter.SPECIALKEY_LEFT;
    }

    // Right arrow.
    if (primaryCode == keyCodeRight) {
      return KeycodeConverter.SPECIALKEY_RIGHT;
    }

    // Down arrow.
    if (primaryCode == keyCodeDown) {
      return KeycodeConverter.SPECIALKEY_DOWN;
    }

    if (primaryCode > 0) {
      ProtoCommands.KeyEvent.Builder builder =
          ProtoCommands.KeyEvent.newBuilder().setKeyCode(primaryCode);
      if (guesser != null && touchEventList != null) {
        List<ProbableKeyEvent> probableKeyEvents = guesser.getProbableKeyEvents(touchEventList);
        if (probableKeyEvents != null) {
          builder.addAllProbableKeyEvent(probableKeyEvents);
        }
      }
      return builder.build();
    }
    return null;
  }

  public KeyEventInterface getPrimaryCodeKeyEvent(int primaryCode) {
    return new PrimaryCodeKeyEvent(primaryCode);
  }

  // Just used to InputMethodService.sendDownUpKeyEvents
  private class PrimaryCodeKeyEvent implements KeyEventInterface {
    private final int primaryCode;

    private PrimaryCodeKeyEvent(int primaryCode) {
      this.primaryCode = primaryCode;
    }

    @Override
    public int getKeyCode() {
      // Hack: as a work around for conflict between unicode-region and android's key event
      // code, make a reverse mapping from unicode to key event code.
      switch (primaryCode) {
        // Map of numbers.
        case '1': case '!': return KeyEvent.KEYCODE_1;
        case '2': return KeyEvent.KEYCODE_2;
        case '3': return KeyEvent.KEYCODE_3;
        case '4': case '$': return KeyEvent.KEYCODE_4;
        case '5': case '%': return KeyEvent.KEYCODE_5;
        case '6': case '^': return KeyEvent.KEYCODE_6;
        case '7': case '&': return KeyEvent.KEYCODE_7;
        case '8': return KeyEvent.KEYCODE_8;
        case '9': case '(': return KeyEvent.KEYCODE_9;
        case '0': case ')': return KeyEvent.KEYCODE_0;

        // Maps of latin alphabets.
        case 'a': case 'A': return KeyEvent.KEYCODE_A;
        case 'b': case 'B': return KeyEvent.KEYCODE_B;
        case 'c': case 'C': return KeyEvent.KEYCODE_C;
        case 'd': case 'D': return KeyEvent.KEYCODE_D;
        case 'e': case 'E': return KeyEvent.KEYCODE_E;
        case 'f': case 'F': return KeyEvent.KEYCODE_F;
        case 'g': case 'G': return KeyEvent.KEYCODE_G;
        case 'h': case 'H': return KeyEvent.KEYCODE_H;
        case 'i': case 'I': return KeyEvent.KEYCODE_I;
        case 'j': case 'J': return KeyEvent.KEYCODE_J;
        case 'k': case 'K': return KeyEvent.KEYCODE_K;
        case 'l': case 'L': return KeyEvent.KEYCODE_L;
        case 'm': case 'M': return KeyEvent.KEYCODE_M;
        case 'n': case 'N': return KeyEvent.KEYCODE_N;
        case 'o': case 'O': return KeyEvent.KEYCODE_O;
        case 'p': case 'P': return KeyEvent.KEYCODE_P;
        case 'q': case 'Q': return KeyEvent.KEYCODE_Q;
        case 'r': case 'R': return KeyEvent.KEYCODE_R;
        case 's': case 'S': return KeyEvent.KEYCODE_S;
        case 't': case 'T': return KeyEvent.KEYCODE_T;
        case 'u': case 'U': return KeyEvent.KEYCODE_U;
        case 'v': case 'V': return KeyEvent.KEYCODE_V;
        case 'w': case 'W': return KeyEvent.KEYCODE_W;
        case 'x': case 'X': return KeyEvent.KEYCODE_X;
        case 'y': case 'Y': return KeyEvent.KEYCODE_Y;
        case 'z': case 'Z': return KeyEvent.KEYCODE_Z;

        // Map of symbols.
        case '*': return KeyEvent.KEYCODE_STAR;
        case '#': return KeyEvent.KEYCODE_POUND;
        case '-': case '_': return KeyEvent.KEYCODE_MINUS;
        case '+': return KeyEvent.KEYCODE_PLUS;
        case '/': case '?': return KeyEvent.KEYCODE_SLASH;
        case '=': return KeyEvent.KEYCODE_EQUALS;
        case ';': case ':': return KeyEvent.KEYCODE_SEMICOLON;
        case '[': case '{': return KeyEvent.KEYCODE_LEFT_BRACKET;
        case ']': case '}': return KeyEvent.KEYCODE_RIGHT_BRACKET;
        case '.': case '>': return KeyEvent.KEYCODE_PERIOD;
        case ',': case '<': return KeyEvent.KEYCODE_COMMA;
        case '`': case '~': return KeyEvent.KEYCODE_GRAVE;
        case '\\': case '|': return KeyEvent.KEYCODE_BACKSLASH;
        case '@': return KeyEvent.KEYCODE_AT;
        case '\'': case '\"': return KeyEvent.KEYCODE_APOSTROPHE;
        // Space
        case ' ': return KeyEvent.KEYCODE_SPACE;
      }

      // Enter
      if (primaryCode == keyCodeEnter) {
        return KeyEvent.KEYCODE_ENTER;
      }
      // Backspace
      if (primaryCode == keyCodeBackspace) {
        return KeyEvent.KEYCODE_DEL;
      }
      // Up arrow.
      if (primaryCode == keyCodeUp) {
        return KeyEvent.KEYCODE_DPAD_UP;
      }
      // Left arrow.
      if (primaryCode == keyCodeLeft) {
        return KeyEvent.KEYCODE_DPAD_LEFT;
      }
      // Right arrow.
      if (primaryCode == keyCodeRight) {
        return KeyEvent.KEYCODE_DPAD_RIGHT;
      }
      // Down arrow.
      if (primaryCode == keyCodeDown) {
        return KeyEvent.KEYCODE_DPAD_DOWN;
      }
      return primaryCode;
    }

    @Override
    public KeyEvent getNativeEvent() {
      return null;
    }
  }
}
