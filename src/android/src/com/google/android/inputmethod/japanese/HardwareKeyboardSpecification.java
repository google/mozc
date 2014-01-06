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

import org.mozc.android.inputmethod.japanese.HardwareKeyboard.CompositionSwitchMode;
import org.mozc.android.inputmethod.japanese.HardwareKeyboard.HardwareKeyboadSpecificationInterface;
import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.KeycodeConverter.KeyEventInterface;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.HardwareKeyMap;
import org.mozc.android.inputmethod.japanese.preference.PreferenceUtil;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.ModifierKey;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.SpecialKey;

import android.content.SharedPreferences;
import android.content.res.Configuration;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Converts Hardware KeyEvent to Mozc's KeyEvent
 *
 */
public enum HardwareKeyboardSpecification implements HardwareKeyboadSpecificationInterface {

  /**
   * System key map.
   */
  DEFAULT(HardwareKeyMap.DEFAULT,
          new DefaultConverter(),
          KeyboardSpecification.HARDWARE_QWERTY_KANA,
          KeyboardSpecification.HARDWARE_QWERTY_ALPHABET),

  /**
   * Represents Japanese 109 Keyboard
   */
  JAPANESE109A(HardwareKeyMap.JAPANESE109A,
               CompactKeyCodeConverterFactory.createOADG109AConverter(),
               KeyboardSpecification.HARDWARE_QWERTY_KANA,
               KeyboardSpecification.HARDWARE_QWERTY_ALPHABET),

  /**
   * Represents Japanese Mobile "12" Keyboard
   */
  TWELVEKEY(HardwareKeyMap.TWELVEKEY,
            new TwelveKeyConverter(),
            KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA,
            KeyboardSpecification.TWELVE_KEY_TOGGLE_ALPHABET);

  private interface HardwareKeyCodeConverter {
    ProtoCommands.KeyEvent getMozcKeyEvent(android.view.KeyEvent keyEvent);

    KeyEventInterface getKeyEventInterface(android.view.KeyEvent keyEvent);

    CompositionSwitchMode getCompositionSwitchMode(android.view.KeyEvent keyEvent);

    boolean isKeyToConsume(android.view.KeyEvent keyEvent);
  }

  // Visible for testing
  static boolean isPrintable(int codepoint) {
    if (Character.isISOControl(codepoint)) {
      return false;
    }
    Character.UnicodeBlock block = Character.UnicodeBlock.of(codepoint);
    return block != null &&
           block != Character.UnicodeBlock.SPECIALS;
  }

  private static class DefaultConverter implements HardwareKeyCodeConverter {
    @Override
    public boolean isKeyToConsume(android.view.KeyEvent keyEvent) {
      return true;
    }

    @Override
    public ProtoCommands.KeyEvent getMozcKeyEvent(android.view.KeyEvent keyEvent) {
      int metaState = keyEvent.getMetaState();
      int unicodeChar = keyEvent.getUnicodeChar();
      int keyCode = keyEvent.getKeyCode();
      ProtoCommands.KeyEvent.Builder builder = ProtoCommands.KeyEvent.newBuilder();
      switch(keyCode) {
        case android.view.KeyEvent.KEYCODE_SPACE:
          builder.setSpecialKey(SpecialKey.SPACE);
          break;
        case android.view.KeyEvent.KEYCODE_DEL:
          builder.setSpecialKey(SpecialKey.BACKSPACE);
          break;
        case android.view.KeyEvent.KEYCODE_TAB:
          builder.setSpecialKey(SpecialKey.TAB);
          break;
        case android.view.KeyEvent.KEYCODE_ENTER:
          builder.setSpecialKey(SpecialKey.ENTER);
          break;
        case android.view.KeyEvent.KEYCODE_HOME:
          builder.setSpecialKey(SpecialKey.HOME);
          break;
        case android.view.KeyEvent.KEYCODE_PAGE_UP:
          builder.setSpecialKey(SpecialKey.PAGE_UP);
          break;
        case android.view.KeyEvent.KEYCODE_PAGE_DOWN:
          builder.setSpecialKey(SpecialKey.PAGE_DOWN);
          break;
        case android.view.KeyEvent.KEYCODE_NUMPAD_DIVIDE:
          builder.setSpecialKey(SpecialKey.DIVIDE);
          break;
        case android.view.KeyEvent.KEYCODE_NUMPAD_MULTIPLY:
          builder.setSpecialKey(SpecialKey.MULTIPLY);
          break;
        case android.view.KeyEvent.KEYCODE_NUMPAD_SUBTRACT:
          builder.setSpecialKey(SpecialKey.SUBTRACT);
          break;
        case android.view.KeyEvent.KEYCODE_NUMPAD_ADD:
          builder.setSpecialKey(SpecialKey.ADD);
          break;
        case android.view.KeyEvent.KEYCODE_NUMPAD_ENTER:
          builder.setSpecialKey(SpecialKey.SEPARATOR);
          break;
        case android.view.KeyEvent.KEYCODE_NUMPAD_DOT:
          builder.setSpecialKey(SpecialKey.DECIMAL);
          break;
        case android.view.KeyEvent.KEYCODE_NUMPAD_0:
          builder.setSpecialKey(SpecialKey.NUMPAD0);
          break;
        case android.view.KeyEvent.KEYCODE_NUMPAD_1:
          builder.setSpecialKey(SpecialKey.NUMPAD1);
          break;
        case android.view.KeyEvent.KEYCODE_NUMPAD_2:
          builder.setSpecialKey(SpecialKey.NUMPAD2);
          break;
        case android.view.KeyEvent.KEYCODE_NUMPAD_3:
          builder.setSpecialKey(SpecialKey.NUMPAD3);
          break;
        case android.view.KeyEvent.KEYCODE_NUMPAD_4:
          builder.setSpecialKey(SpecialKey.NUMPAD4);
          break;
        case android.view.KeyEvent.KEYCODE_NUMPAD_5:
          builder.setSpecialKey(SpecialKey.NUMPAD5);
          break;
        case android.view.KeyEvent.KEYCODE_NUMPAD_6:
          builder.setSpecialKey(SpecialKey.NUMPAD6);
          break;
        case android.view.KeyEvent.KEYCODE_NUMPAD_7:
          builder.setSpecialKey(SpecialKey.NUMPAD7);
          break;
        case android.view.KeyEvent.KEYCODE_NUMPAD_8:
          builder.setSpecialKey(SpecialKey.NUMPAD8);
          break;
        case android.view.KeyEvent.KEYCODE_NUMPAD_9:
          builder.setSpecialKey(SpecialKey.NUMPAD9);
          break;
        default:
          if (unicodeChar != 0) {
            builder.setKeyCode(unicodeChar);
          }
          break;
      }

      if (!isPrintable(unicodeChar)) {
        // Mozc server accepts modifiers only if non-printable key event is sent.
        if ((metaState & android.view.KeyEvent.META_SHIFT_MASK) != 0) {
            builder.addModifierKeys(ModifierKey.SHIFT);
        }
        if ((metaState & android.view.KeyEvent.META_ALT_MASK) != 0) {
          builder.addModifierKeys(ModifierKey.ALT);
        }
        if ((metaState & android.view.KeyEvent.META_CTRL_MASK) != 0) {
          builder.addModifierKeys(ModifierKey.CTRL);
        }
      }
      return builder.build();
    }

    @Override
    public KeyEventInterface getKeyEventInterface(final android.view.KeyEvent keyEvent) {
      return KeycodeConverter.getKeyEventInterface(keyEvent);
    }

    @Override
    public CompositionSwitchMode getCompositionSwitchMode(android.view.KeyEvent keyEvent) {
      return null;
    }
  }

  private static class CompactKeyCodeConverterFactory {

    private static class CompactKeyEvent {
      final int scanCode;
      final int compactMetaState;

      public CompactKeyEvent(int scanCode, int compactMetaState) {
        this.scanCode = scanCode;
        this.compactMetaState = compactMetaState;
      }

      @Override
      public boolean equals(Object o) {
        if (this == o) {
          return true;
        }

        if (!(o instanceof CompactKeyEvent)) {
          return false;
        }

        CompactKeyEvent lhs = CompactKeyEvent.class.cast(o);
        return (scanCode == lhs.scanCode) && (compactMetaState == lhs.compactMetaState);
      }

      @Override
      public int hashCode() {
        return scanCode ^ Integer.reverseBytes(compactMetaState);
      }
    }

    private static class CompactKeyCodeConverter implements HardwareKeyCodeConverter {
      private static CompactKeyEvent getCompactKeyEvent(android.view.KeyEvent keyEvent) {
        return new CompactKeyEvent(keyEvent.getScanCode(),
                                   getCompactMetaState(keyEvent.getMetaState()));
      }

      private final Map<CompactKeyEvent, ProtoCommands.KeyEvent> compactKeyToMozcKey;
      private final Map<CompactKeyEvent, CompositionSwitchMode> compactKeyToSwichMode;

      CompactKeyCodeConverter(Map<CompactKeyEvent, ProtoCommands.KeyEvent> compactKeyToMozcKey,
                              Map<CompactKeyEvent, CompositionSwitchMode> compactKeyToSwichMode) {
        this.compactKeyToMozcKey = compactKeyToMozcKey;
        this.compactKeyToSwichMode = compactKeyToSwichMode;
      }

      @Override
      public ProtoCommands.KeyEvent getMozcKeyEvent(android.view.KeyEvent keyEvent) {
        return compactKeyToMozcKey.get(getCompactKeyEvent(keyEvent));
      }

      @Override
      public KeyEventInterface getKeyEventInterface(final android.view.KeyEvent keyEvent) {
        return KeycodeConverter.getKeyEventInterface(keyEvent);
      }

      @Override
      public CompositionSwitchMode getCompositionSwitchMode(android.view.KeyEvent keyEvent) {
        return compactKeyToSwichMode.get(getCompactKeyEvent(keyEvent));
      }

      @Override
      public boolean isKeyToConsume(android.view.KeyEvent keyEvent) {
        CompactKeyEvent compactKeyEvent = getCompactKeyEvent(keyEvent);
        return compactKeyToMozcKey.containsKey(compactKeyEvent)
            || compactKeyToSwichMode.containsKey(compactKeyEvent);
      }
    }

    private CompactKeyCodeConverterFactory () {
    }

    // The order of 'metaState' is defined only by shift.
    private static int getCompactMetaState(int metaState) {
      return (((metaState & android.view.KeyEvent.META_SHIFT_MASK) != 0) ? 1 : 0)
          + (((metaState & android.view.KeyEvent.META_ALT_MASK) != 0) ? 2 : 0)
          + (((metaState & android.view.KeyEvent.META_CTRL_MASK) != 0) ? 4 : 0);
    }

    private static void putKeyCode(Map<CompactKeyEvent, ProtoCommands.KeyEvent> table, int scanCode,
                                   int metaState, int keycode) {
      putUnique(table,
                new CompactKeyEvent(scanCode, getCompactMetaState(metaState)),
                KeycodeConverter.getMozcKeyEvent(keycode));
    }

    private static void putSpecialKey(Map<CompactKeyEvent, ProtoCommands.KeyEvent> table,
                                      int scanCode, ProtoCommands.KeyEvent.SpecialKey specialKey) {
      for (boolean shift : new boolean[]{true, false}) {
        for (boolean alt : new boolean[]{true, false}) {
          for (boolean ctrl : new boolean[]{true, false}) {
            int metaState = 0;
            ProtoCommands.KeyEvent.Builder builder = ProtoCommands.KeyEvent.newBuilder()
               .setSpecialKey(specialKey);
            if (shift) {
              metaState |= android.view.KeyEvent.META_SHIFT_ON;
              builder.addModifierKeys(ModifierKey.SHIFT);
            }
            if (alt) {
              metaState |= android.view.KeyEvent.META_ALT_ON;
              builder.addModifierKeys(ModifierKey.ALT);
            }
            if (ctrl) {
              metaState |= android.view.KeyEvent.META_CTRL_ON;
              builder.addModifierKeys(ModifierKey.CTRL);
            }
            putUnique(table,
                      new CompactKeyEvent(scanCode, getCompactMetaState(metaState)),
                      builder.build());
          }
        }
      }
    }

    private static <K, V> void putUnique(Map<K, V> table, K key, V value) {
      if (table.put(key, value) != null) {
        throw new IllegalArgumentException("map has the key already");
      }
    }

    private static HardwareKeyCodeConverter createOADG109AConverter() {
      Map<CompactKeyEvent, ProtoCommands.KeyEvent> tmpTable =
          new HashMap<CompactKeyEvent, ProtoCommands.KeyEvent>();

      putKeyCode(tmpTable, 0x02, 0, '1');
      putKeyCode(tmpTable, 0x02, android.view.KeyEvent.META_SHIFT_ON, '!');
      putKeyCode(tmpTable, 0x03, 0, '2');
      putKeyCode(tmpTable, 0x03, android.view.KeyEvent.META_SHIFT_ON, '"');
      putKeyCode(tmpTable, 0x04, 0, '3');
      putKeyCode(tmpTable, 0x04, android.view.KeyEvent.META_SHIFT_ON, '#');
      putKeyCode(tmpTable, 0x05, 0, '4');
      putKeyCode(tmpTable, 0x05, android.view.KeyEvent.META_SHIFT_ON, '$');
      putKeyCode(tmpTable, 0x06, 0, '5');
      putKeyCode(tmpTable, 0x06, android.view.KeyEvent.META_SHIFT_ON, '%');
      putKeyCode(tmpTable, 0x07, 0, '6');
      putKeyCode(tmpTable, 0x07, android.view.KeyEvent.META_SHIFT_ON, '&');
      putKeyCode(tmpTable, 0x08, 0, '7');
      putKeyCode(tmpTable, 0x08, android.view.KeyEvent.META_SHIFT_ON, '\'');
      putKeyCode(tmpTable, 0x09, 0, '8');
      putKeyCode(tmpTable, 0x09, android.view.KeyEvent.META_SHIFT_ON, '(');
      putKeyCode(tmpTable, 0x0A, 0, '9');
      putKeyCode(tmpTable, 0x0A, android.view.KeyEvent.META_SHIFT_ON, ')');
      putKeyCode(tmpTable, 0x0B, 0, '0');
      putKeyCode(tmpTable, 0x0B, android.view.KeyEvent.META_SHIFT_ON, '~');
      putKeyCode(tmpTable, 0x0C, 0, '-');
      putKeyCode(tmpTable, 0x0C, android.view.KeyEvent.META_SHIFT_ON, '=');
      putKeyCode(tmpTable, 0x0D, 0, '^');
      putKeyCode(tmpTable, 0x0D, android.view.KeyEvent.META_SHIFT_ON, '~');
      putKeyCode(tmpTable, 0x7C, 0, '\\');
      putKeyCode(tmpTable, 0x7C, android.view.KeyEvent.META_SHIFT_ON, '|');

      putKeyCode(tmpTable, 0x10, 0, 'q');
      putKeyCode(tmpTable, 0x10, android.view.KeyEvent.META_SHIFT_ON, 'Q');
      putKeyCode(tmpTable, 0x11, 0, 'w');
      putKeyCode(tmpTable, 0x11, android.view.KeyEvent.META_SHIFT_ON, 'W');
      putKeyCode(tmpTable, 0x12, 0, 'e');
      putKeyCode(tmpTable, 0x12, android.view.KeyEvent.META_SHIFT_ON, 'E');
      putKeyCode(tmpTable, 0x13, 0, 'r');
      putKeyCode(tmpTable, 0x13, android.view.KeyEvent.META_SHIFT_ON, 'R');
      putKeyCode(tmpTable, 0x14, 0, 't');
      putKeyCode(tmpTable, 0x14, android.view.KeyEvent.META_SHIFT_ON, 'T');
      putKeyCode(tmpTable, 0x15, 0, 'y');
      putKeyCode(tmpTable, 0x15, android.view.KeyEvent.META_SHIFT_ON, 'Y');
      putKeyCode(tmpTable, 0x16, 0, 'u');
      putKeyCode(tmpTable, 0x16, android.view.KeyEvent.META_SHIFT_ON, 'U');
      putKeyCode(tmpTable, 0x17, 0, 'i');
      putKeyCode(tmpTable, 0x17, android.view.KeyEvent.META_SHIFT_ON, 'I');
      putKeyCode(tmpTable, 0x18, 0, 'o');
      putKeyCode(tmpTable, 0x18, android.view.KeyEvent.META_SHIFT_ON, 'O');
      putKeyCode(tmpTable, 0x19, 0, 'p');
      putKeyCode(tmpTable, 0x19, android.view.KeyEvent.META_SHIFT_ON, 'P');
      putKeyCode(tmpTable, 0x1A, 0, '@');
      putKeyCode(tmpTable, 0x1A, android.view.KeyEvent.META_SHIFT_ON, '`');
      putKeyCode(tmpTable, 0x1B, 0, '[');
      putKeyCode(tmpTable, 0x1B, android.view.KeyEvent.META_SHIFT_ON, '{');

      putKeyCode(tmpTable, 0x1E, 0, 'a');
      putKeyCode(tmpTable, 0x1E, android.view.KeyEvent.META_SHIFT_ON, 'A');
      putKeyCode(tmpTable, 0x1F, 0, 's');
      putKeyCode(tmpTable, 0x1F, android.view.KeyEvent.META_SHIFT_ON, 'S');
      putKeyCode(tmpTable, 0x20, 0, 'd');
      putKeyCode(tmpTable, 0x20, android.view.KeyEvent.META_SHIFT_ON, 'D');
      putKeyCode(tmpTable, 0x21, 0, 'f');
      putKeyCode(tmpTable, 0x21, android.view.KeyEvent.META_SHIFT_ON, 'F');
      putKeyCode(tmpTable, 0x22, 0, 'g');
      putKeyCode(tmpTable, 0x22, android.view.KeyEvent.META_SHIFT_ON, 'G');
      putKeyCode(tmpTable, 0x23, 0, 'h');
      putKeyCode(tmpTable, 0x23, android.view.KeyEvent.META_SHIFT_ON, 'H');
      putKeyCode(tmpTable, 0x24, 0, 'j');
      putKeyCode(tmpTable, 0x24, android.view.KeyEvent.META_SHIFT_ON, 'J');
      putKeyCode(tmpTable, 0x25, 0, 'k');
      putKeyCode(tmpTable, 0x25, android.view.KeyEvent.META_SHIFT_ON, 'K');
      putKeyCode(tmpTable, 0x26, 0, 'l');
      putKeyCode(tmpTable, 0x26, android.view.KeyEvent.META_SHIFT_ON, 'L');
      putKeyCode(tmpTable, 0x27, 0, ';');
      putKeyCode(tmpTable, 0x27, android.view.KeyEvent.META_SHIFT_ON, '+');
      putKeyCode(tmpTable, 0x28, 0, ':');
      putKeyCode(tmpTable, 0x28, android.view.KeyEvent.META_SHIFT_ON, '*');
      putKeyCode(tmpTable, 0x2B, 0, ']');
      putKeyCode(tmpTable, 0x2B, android.view.KeyEvent.META_SHIFT_ON, '}');

      putKeyCode(tmpTable, 0x2C, 0, 'z');
      putKeyCode(tmpTable, 0x2C, android.view.KeyEvent.META_SHIFT_ON, 'Z');
      putKeyCode(tmpTable, 0x2D, 0, 'x');
      putKeyCode(tmpTable, 0x2D, android.view.KeyEvent.META_SHIFT_ON, 'X');
      putKeyCode(tmpTable, 0x2E, 0, 'c');
      putKeyCode(tmpTable, 0x2E, android.view.KeyEvent.META_SHIFT_ON, 'C');
      putKeyCode(tmpTable, 0x2F, 0, 'v');
      putKeyCode(tmpTable, 0x2F, android.view.KeyEvent.META_SHIFT_ON, 'V');
      putKeyCode(tmpTable, 0x30, 0, 'b');
      putKeyCode(tmpTable, 0x30, android.view.KeyEvent.META_SHIFT_ON, 'B');
      putKeyCode(tmpTable, 0x31, 0, 'n');
      putKeyCode(tmpTable, 0x31, android.view.KeyEvent.META_SHIFT_ON, 'N');
      putKeyCode(tmpTable, 0x32, 0, 'm');
      putKeyCode(tmpTable, 0x32, android.view.KeyEvent.META_SHIFT_ON, 'M');
      putKeyCode(tmpTable, 0x33, 0, ',');
      putKeyCode(tmpTable, 0x33, android.view.KeyEvent.META_SHIFT_ON, '<');
      putKeyCode(tmpTable, 0x34, 0, '.');
      putKeyCode(tmpTable, 0x34, android.view.KeyEvent.META_SHIFT_ON, '>');
      putKeyCode(tmpTable, 0x35, 0, '/');
      putKeyCode(tmpTable, 0x35, android.view.KeyEvent.META_SHIFT_ON, '?');
      putKeyCode(tmpTable, 0x73, 0, '\\');
      putKeyCode(tmpTable, 0x73, android.view.KeyEvent.META_SHIFT_ON, '_');

      putSpecialKey(tmpTable, 0x01, SpecialKey.ESCAPE);
      putSpecialKey(tmpTable, 0x3B, SpecialKey.F1);
      putSpecialKey(tmpTable, 0x3C, SpecialKey.F2);
      putSpecialKey(tmpTable, 0x3D, SpecialKey.F3);
      putSpecialKey(tmpTable, 0x3E, SpecialKey.F4);
      putSpecialKey(tmpTable, 0x3F, SpecialKey.F5);
      putSpecialKey(tmpTable, 0x40, SpecialKey.F6);
      putSpecialKey(tmpTable, 0x41, SpecialKey.F7);
      putSpecialKey(tmpTable, 0x42, SpecialKey.F8);
      putSpecialKey(tmpTable, 0x43, SpecialKey.F9);
      putSpecialKey(tmpTable, 0x44, SpecialKey.F10);
      putSpecialKey(tmpTable, 0x57, SpecialKey.F11);
      putSpecialKey(tmpTable, 0x58, SpecialKey.F12);
      putSpecialKey(tmpTable, 0x6F, SpecialKey.DEL);

      putSpecialKey(tmpTable, 0x0E, SpecialKey.BACKSPACE);
      putSpecialKey(tmpTable, 0x0F, SpecialKey.TAB);
      putSpecialKey(tmpTable, 0x1C, SpecialKey.ENTER);

      putSpecialKey(tmpTable, 0x5E, SpecialKey.MUHENKAN);
      putSpecialKey(tmpTable, 0x39, SpecialKey.SPACE);
      putSpecialKey(tmpTable, 0x5C, SpecialKey.HENKAN);
      putSpecialKey(tmpTable, 0x5D, SpecialKey.KANA);

      putSpecialKey(tmpTable, 0x67, SpecialKey.UP);
      putSpecialKey(tmpTable, 0x69, SpecialKey.LEFT);
      putSpecialKey(tmpTable, 0x6C, SpecialKey.DOWN);
      putSpecialKey(tmpTable, 0x6A, SpecialKey.RIGHT);

      putSpecialKey(tmpTable, 0x66, SpecialKey.HOME);
      putSpecialKey(tmpTable, 0x6B, SpecialKey.END);
      putSpecialKey(tmpTable, 0x6E, SpecialKey.INSERT);
      putSpecialKey(tmpTable, 0x68, SpecialKey.PAGE_UP);
      putSpecialKey(tmpTable, 0x6D, SpecialKey.PAGE_DOWN);

      putSpecialKey(tmpTable, 0x62, SpecialKey.DIVIDE);
      putSpecialKey(tmpTable, 0x37, SpecialKey.MULTIPLY);
      putSpecialKey(tmpTable, 0x4A, SpecialKey.SUBTRACT);
      putSpecialKey(tmpTable, 0x4E, SpecialKey.ADD);
      putSpecialKey(tmpTable, 0x60, SpecialKey.SEPARATOR);
      putSpecialKey(tmpTable, 0x53, SpecialKey.DECIMAL);
      putSpecialKey(tmpTable, 0x52, SpecialKey.NUMPAD0);
      putSpecialKey(tmpTable, 0x4F, SpecialKey.NUMPAD1);
      putSpecialKey(tmpTable, 0x50, SpecialKey.NUMPAD2);
      putSpecialKey(tmpTable, 0x51, SpecialKey.NUMPAD3);
      putSpecialKey(tmpTable, 0x4B, SpecialKey.NUMPAD4);
      putSpecialKey(tmpTable, 0x4C, SpecialKey.NUMPAD5);
      putSpecialKey(tmpTable, 0x4D, SpecialKey.NUMPAD6);
      putSpecialKey(tmpTable, 0x47, SpecialKey.NUMPAD7);
      putSpecialKey(tmpTable, 0x48, SpecialKey.NUMPAD8);
      putSpecialKey(tmpTable, 0x49, SpecialKey.NUMPAD9);

      return new CompactKeyCodeConverter(
          Collections.unmodifiableMap(tmpTable),
          // Decode 0x29 Zenkaku/Hankaku to CompositionMode changing.
          Collections.singletonMap(new CompactKeyEvent(0x29, 0), CompositionSwitchMode.TOGGLE));
    }
  }

  private static class TwelveKeyConverter extends DefaultConverter {
    @Override
    public ProtoCommands.KeyEvent getMozcKeyEvent(android.view.KeyEvent keyEvent) {
      int keyCode = keyEvent.getKeyCode();
      if (keyCode == android.view.KeyEvent.KEYCODE_POUND ||
          keyCode == android.view.KeyEvent.KEYCODE_DPAD_CENTER) {
        return KeycodeConverter.SPECIALKEY_ENTER;
      }

      return super.getMozcKeyEvent(keyEvent);
    }

    @Override
    public KeyEventInterface getKeyEventInterface(final android.view.KeyEvent keyEvent) {
      return new KeyEventInterface() {
        @Override
        public int getKeyCode() {
          int keyCode = keyEvent.getKeyCode();
          if (keyCode == android.view.KeyEvent.KEYCODE_POUND ||
              keyCode == android.view.KeyEvent.KEYCODE_DPAD_CENTER) {
            return android.view.KeyEvent.KEYCODE_ENTER;
          }
          return keyCode;
        }

        @Override
        public android.view.KeyEvent getNativeEvent() {
          return null;
        }
      };
    }
  }

  private final HardwareKeyMap hardwareKeyMap;

  private final HardwareKeyCodeConverter hardwareKeyCodeConverter;

  private final KeyboardSpecification kanaKeyboardSpecification;

  private final KeyboardSpecification alphabetKeyboardSpecification;

  private static final Map<HardwareKeyMap, HardwareKeyboadSpecificationInterface>
      hardwareKeyMapToSpecificationMap;

  static {
    HashMap<HardwareKeyMap, HardwareKeyboadSpecificationInterface> tmpMap =
        new HashMap<HardwareKeyMap, HardwareKeyboadSpecificationInterface>();
    for (HardwareKeyboardSpecification hardwareKeyboardSpecification :
        HardwareKeyboardSpecification.values()) {
      tmpMap.put(hardwareKeyboardSpecification.hardwareKeyMap, hardwareKeyboardSpecification);
    }
    hardwareKeyMapToSpecificationMap = Collections.unmodifiableMap(tmpMap);
  }

  private HardwareKeyboardSpecification(
      HardwareKeyMap hardwareKeyMap,
      HardwareKeyCodeConverter hardwareKeyCodeConverter,
      KeyboardSpecification kanaKeyboardSpecification,
      KeyboardSpecification alphabetKeyboardSpecification) {
    this.hardwareKeyMap = hardwareKeyMap;
    this.hardwareKeyCodeConverter = hardwareKeyCodeConverter;
    this.kanaKeyboardSpecification = kanaKeyboardSpecification;
    this.alphabetKeyboardSpecification = alphabetKeyboardSpecification;
  }

  static HardwareKeyboadSpecificationInterface getHardwareKeyboardSpecification(
      HardwareKeyMap hardwareKeyMap) {
    return hardwareKeyMapToSpecificationMap.get(hardwareKeyMap);
  }

  /**
   * Detects hardware keyboard type and sets it to the given {@code sharedPreferences}.
   * If the {@code sharedPreferences} is {@code null}, or already has valid hardware keyboard type,
   * just does nothing.
   *
   * Note: if the new detected hardware keyboard type is set to the {@code sharedPreferences}
   * and if it has registered callbacks, of course, they will be invoked as usual.
   */
  public static void maybeSetDetectedHardwareKeyMap(
      SharedPreferences sharedPreferences, Configuration configuration, boolean forceToSet) {
    if (sharedPreferences == null) {
      return;
    }

    // First, check if the hardware keyboard type has already set to the preference.
    if (getHardwareKeyMap(sharedPreferences) != null) {
      // Found the valid value.
      return;
    }

    // Here, the HardwareKeyMap hasn't set yet, so detect hardware keyboard type.
    HardwareKeyMap detectedKeyMap = null;
    switch(configuration.keyboard) {
      case Configuration.KEYBOARD_12KEY:
        detectedKeyMap = HardwareKeyMap.TWELVEKEY;
        break;
      case Configuration.KEYBOARD_QWERTY:
        detectedKeyMap = HardwareKeyMap.JAPANESE109A;
        break;
      case Configuration.KEYBOARD_NOKEYS:
      case Configuration.KEYBOARD_UNDEFINED:
      default:
        break;
    }

    if (detectedKeyMap != null) {
      setHardwareKeyMap(sharedPreferences, detectedKeyMap);
    } else if (forceToSet) {
      setHardwareKeyMap(sharedPreferences, HardwareKeyMap.DEFAULT);
    }

    MozcLog.i("RUN HARDWARE KEYBOARD DETECTION: " + detectedKeyMap);
  }

  /**
   * Returns currently set key map based on the default SharedPreferences.
   *
   * @param sharedPreferences a SharedPreference to be retrieved
   * @return null if no preference has not been set
   */
  public static HardwareKeyMap getHardwareKeyMap(SharedPreferences sharedPreferences) {
    String prefHardwareKeyMap =
        sharedPreferences.getString(PreferenceUtil.PREF_HARDWARE_KEYMAP, null);
    if (prefHardwareKeyMap != null) {
      try {
        return HardwareKeyMap.valueOf(prefHardwareKeyMap);
      } catch (IllegalArgumentException e) {
        return null;
      }
    }
    return null;
  }

  static void setHardwareKeyMap(
      SharedPreferences sharedPreference, HardwareKeyMap hardwareKeyMap) {
    sharedPreference.edit()
         .putString(PreferenceUtil.PREF_HARDWARE_KEYMAP, hardwareKeyMap.name()).commit();
  }

  @Override
  public HardwareKeyMap getHardwareKeyMap() {
    return hardwareKeyMap;
  }

  @Override
  public ProtoCommands.KeyEvent getMozcKeyEvent(android.view.KeyEvent keyEvent) {
    return hardwareKeyCodeConverter.getMozcKeyEvent(keyEvent);
  }

  @Override
  public KeyEventInterface getKeyEventInterface(final android.view.KeyEvent keyEvent) {
    return hardwareKeyCodeConverter.getKeyEventInterface(keyEvent);
  }

  @Override
  public CompositionSwitchMode getCompositionSwitchMode(android.view.KeyEvent keyEvent) {
    return hardwareKeyCodeConverter.getCompositionSwitchMode(keyEvent);
  }

  @Override
  public boolean isKeyToConsume(android.view.KeyEvent keyEvent) {
    return hardwareKeyCodeConverter.isKeyToConsume(keyEvent);
  }

  @Override
  public KeyboardSpecification getKanaKeyboardSpecification() {
    return kanaKeyboardSpecification;
  }

  @Override
  public KeyboardSpecification getAlphabetKeyboardSpecification() {
    return alphabetKeyboardSpecification;
  }
}
