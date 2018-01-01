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
import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.hardwarekeyboard.HardwareKeyboard.CompositionSwitchMode;
import org.mozc.android.inputmethod.japanese.hardwarekeyboard.KeyEventMapperFactory.KeyEventMapper;
import org.mozc.android.inputmethod.japanese.keyboard.Keyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.HardwareKeyMap;
import org.mozc.android.inputmethod.japanese.preference.PreferenceUtil;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.ModifierKey;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.KeyEvent.SpecialKey;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.annotation.SuppressLint;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.view.KeyEvent;

import java.util.Collections;
import java.util.EnumMap;
import java.util.Map;

import javax.annotation.Nullable;

/**
 * Converts Hardware KeyEvent to Mozc's KeyEvent
 *
 * Android OS's keyboard management is like following;
 * <ol>
 * <li>Android OS receives LKC (Linux Key Code, ≒ scan code) from the keyboard.
 * <li>Android OS converts LKC into AKC (Android Key Code) using key layout file (.kl).
 * A user/developer cannot override this behavior.
 * <li>Android OS converts AKC into a tuple of (AKC, unicode character)
 * using key character mapping file (.kcm).
 * <ul>
 * <li>Given AKC might be converted into another AKC (e.g. KEYCODE_GRAVE might be converted
 * into KEYCODE_ZENKAKU_HANKAKU).
 * <li>Since API Level 16 a user/developer has been able to override the behavior.
 * </ul>
 * </ol>
 * To use Japanese keyboard, the default behavior is not sufficient.
 * <ul>
 * <li>Even the latest OS (API Level 19) Japanese key character mapping is not shipped by default.
 * <li>There are Japanese unique AKC (e.g. KEYCODE_ZENKAKU_HANKAKU) but they are available
 * since API Level 16. (Note since API Level 16 some Japanese unique ones (e.g. KEYCODE_RO)
 * can be sent though Japanese KCM is not supported)
 * </ul>
 * The main responsibility of this class is to convert android's KeyEvent into Mozc's KeyEvent.
 * To do this above behavior must be respected. CompactKeyEvent (and its internal KeyEventMapper)
 * does this.
 *
 */
public enum HardwareKeyboardSpecification {

  /**
   * System key map.
   */
  DEFAULT(HardwareKeyMap.DEFAULT,
          KeyEventMapperFactory.DEFAULT_KEYBOARD_MAPPER,
          KeyboardSpecification.HARDWARE_QWERTY_KANA,
          KeyboardSpecification.HARDWARE_QWERTY_ALPHABET),

  /**
   * Represents Japanese 109 Keyboard
   */
  JAPANESE109A(HardwareKeyMap.JAPANESE109A,
               KeyEventMapperFactory.JAPANESE_KEYBOARD_MAPPER,
               KeyboardSpecification.HARDWARE_QWERTY_KANA,
               KeyboardSpecification.HARDWARE_QWERTY_ALPHABET);

  /**
   * Returns true if the given {@code codepoint} is printable.
   *
   * {@link KeyEvent#isPrintingKey()} cannot be used for this purpose as
   * the method doesn't take codepoint but keycode.
   */
  @VisibleForTesting
  static boolean isPrintable(int codepoint) {
    Preconditions.checkArgument(codepoint >= 0);
    if (Character.isISOControl(codepoint)) {
      return false;
    }
    Character.UnicodeBlock block = Character.UnicodeBlock.of(codepoint);
    return block != null &&
           block != Character.UnicodeBlock.SPECIALS;
  }

  /**
   * Returns true if composition mode should be changed.
   */
  @SuppressLint("InlinedApi")
  private static boolean isKeyForCompositinoModeChange(int keyCode, int metaState) {
    boolean shift = (metaState & KeyEvent.META_SHIFT_MASK) != 0;
    boolean alt = (metaState & KeyEvent.META_ALT_MASK) != 0;
    boolean ctrl = (metaState & KeyEvent.META_CTRL_MASK) != 0;
    boolean meta = (metaState & KeyEvent.META_META_MASK) != 0;
    // ZEN/HAN with no modifier.
    if (keyCode == KeyEvent.KEYCODE_ZENKAKU_HANKAKU && !shift && !alt && !ctrl && !meta) {
      return true;
    }
    // GRAVE with ALT.
    if (keyCode == KeyEvent.KEYCODE_GRAVE && !shift && alt && !ctrl && !meta) {
      return true;
    }
    return false;
  }

  private final HardwareKeyMap hardwareKeyMap;

  private final KeyboardSpecification kanaKeyboardSpecification;

  private final KeyboardSpecification alphabetKeyboardSpecification;

  private final KeyEventMapper keyEventMapper;

  private static final Map<HardwareKeyMap, HardwareKeyboardSpecification>
      hardwareKeyMapToSpecificationMap;

  static {
    // Initialize preference name to HardwareKeyboardSpecification mapping.
    Map<HardwareKeyMap, HardwareKeyboardSpecification> tmpMap =
        new EnumMap<HardwareKeyMap, HardwareKeyboardSpecification>(HardwareKeyMap.class);
    for (HardwareKeyboardSpecification hardwareKeyboardSpecification :
        HardwareKeyboardSpecification.values()) {
      tmpMap.put(hardwareKeyboardSpecification.hardwareKeyMap, hardwareKeyboardSpecification);
    }
    hardwareKeyMapToSpecificationMap = Collections.unmodifiableMap(tmpMap);
  }

  private HardwareKeyboardSpecification(
      HardwareKeyMap hardwareKeyMap,
      KeyEventMapper keyEventMapper,
      KeyboardSpecification kanaKeyboardSpecification,
      KeyboardSpecification alphabetKeyboardSpecification) {
    this.hardwareKeyMap = hardwareKeyMap;
    this.keyEventMapper = keyEventMapper;
    this.kanaKeyboardSpecification = kanaKeyboardSpecification;
    this.alphabetKeyboardSpecification = alphabetKeyboardSpecification;
  }

  /**
   * Returns HardwareKeyboadSpecification based on given preference.
   */
  static Optional<HardwareKeyboardSpecification> getHardwareKeyboardSpecification(
     Optional<HardwareKeyMap> hardwareKeyMap) {
    return hardwareKeyMap.isPresent()
        ? Optional.of(hardwareKeyMapToSpecificationMap.get(hardwareKeyMap.get()))
        : Optional.<HardwareKeyboardSpecification>absent();
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
      @Nullable SharedPreferences sharedPreferences, Configuration configuration,
      boolean forceToSet) {
    Preconditions.checkNotNull(configuration);
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
      case Configuration.KEYBOARD_QWERTY:
        detectedKeyMap = HardwareKeyMap.DEFAULT;
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
        Preconditions.checkNotNull(sharedPreferences)
            .getString(PreferenceUtil.PREF_HARDWARE_KEYMAP, null);
    if (prefHardwareKeyMap != null) {
      try {
        return HardwareKeyMap.valueOf(prefHardwareKeyMap);
      } catch (IllegalArgumentException e) {
        return null;
      }
    }
    return null;
  }

  @VisibleForTesting
  static void setHardwareKeyMap(
      SharedPreferences sharedPreference, HardwareKeyMap hardwareKeyMap) {
    Preconditions.checkNotNull(sharedPreference);
    Preconditions.checkNotNull(hardwareKeyMap);
    sharedPreference.edit()
         .putString(PreferenceUtil.PREF_HARDWARE_KEYMAP, hardwareKeyMap.name()).commit();
  }

  public HardwareKeyMap getHardwareKeyMap() {
    return hardwareKeyMap;
  }

  public ProtoCommands.KeyEvent getMozcKeyEvent(android.view.KeyEvent keyEvent) {
    Preconditions.checkNotNull(keyEvent);
    CompactKeyEvent compactKeyEvent = new CompactKeyEvent(keyEvent, keyEventMapper);

    // Check if the key is for changing composition mode.
    // The event should be consumed client-side so nothing should be returned.
    if (isKeyForCompositinoModeChange(compactKeyEvent.getKeyCode(),
                                      compactKeyEvent.getMetaState())) {
      return null;
    }

    int keyCode = compactKeyEvent.getKeyCode();
    ProtoCommands.KeyEvent.Builder builder = ProtoCommands.KeyEvent.newBuilder();
    // Check if the event is special key.
    // This check should be done in advance of checking unicode-character (done in default block)
    // because KEYCODE_SPACE/TAB, which are special keys, are sent with unicode-character.
    switch(keyCode) {
      case android.view.KeyEvent.KEYCODE_SPACE:
        builder.setSpecialKey(SpecialKey.SPACE);
        break;
      case android.view.KeyEvent.KEYCODE_FORWARD_DEL:
        builder.setSpecialKey(SpecialKey.DEL);
        break;
      case android.view.KeyEvent.KEYCODE_DEL:
        builder.setSpecialKey(SpecialKey.BACKSPACE);
        break;
      case android.view.KeyEvent.KEYCODE_INSERT:
        builder.setSpecialKey(SpecialKey.INSERT);
        break;
      case android.view.KeyEvent.KEYCODE_HENKAN:
        builder.setSpecialKey(SpecialKey.HENKAN);
        break;
      case android.view.KeyEvent.KEYCODE_MUHENKAN:
        builder.setSpecialKey(SpecialKey.MUHENKAN);
        break;
      case android.view.KeyEvent.KEYCODE_KANA:
        builder.setSpecialKey(SpecialKey.KANA);
        break;
      case android.view.KeyEvent.KEYCODE_KATAKANA_HIRAGANA:
        builder.setSpecialKey(SpecialKey.KATAKANA);
        break;
      case android.view.KeyEvent.KEYCODE_TAB:
        builder.setSpecialKey(SpecialKey.TAB);
        break;
      case android.view.KeyEvent.KEYCODE_ENTER:
        builder.setSpecialKey(SpecialKey.ENTER);
        break;
      case android.view.KeyEvent.KEYCODE_DPAD_LEFT:
        builder.setSpecialKey(SpecialKey.LEFT);
        break;
      case android.view.KeyEvent.KEYCODE_DPAD_RIGHT:
        builder.setSpecialKey(SpecialKey.RIGHT);
        break;
      case android.view.KeyEvent.KEYCODE_DPAD_UP:
        builder.setSpecialKey(SpecialKey.UP);
        break;
      case android.view.KeyEvent.KEYCODE_DPAD_DOWN:
        builder.setSpecialKey(SpecialKey.DOWN);
        break;
      case android.view.KeyEvent.KEYCODE_ESCAPE:
        builder.setSpecialKey(SpecialKey.ESCAPE);
        break;
      case android.view.KeyEvent.KEYCODE_MOVE_HOME:
        builder.setSpecialKey(SpecialKey.HOME);
        break;
      case android.view.KeyEvent.KEYCODE_MOVE_END:
        builder.setSpecialKey(SpecialKey.END);
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
      case android.view.KeyEvent.KEYCODE_F1:
        builder.setSpecialKey(SpecialKey.F1);
        break;
      case android.view.KeyEvent.KEYCODE_F2:
        builder.setSpecialKey(SpecialKey.F2);
        break;
      case android.view.KeyEvent.KEYCODE_F3:
        builder.setSpecialKey(SpecialKey.F3);
        break;
      case android.view.KeyEvent.KEYCODE_F4:
        builder.setSpecialKey(SpecialKey.F4);
        break;
      case android.view.KeyEvent.KEYCODE_F5:
        builder.setSpecialKey(SpecialKey.F5);
        break;
      case android.view.KeyEvent.KEYCODE_F6:
        builder.setSpecialKey(SpecialKey.F6);
        break;
      case android.view.KeyEvent.KEYCODE_F7:
        builder.setSpecialKey(SpecialKey.F7);
        break;
      case android.view.KeyEvent.KEYCODE_F8:
        builder.setSpecialKey(SpecialKey.F8);
        break;
      case android.view.KeyEvent.KEYCODE_F9:
        builder.setSpecialKey(SpecialKey.F9);
        break;
      case android.view.KeyEvent.KEYCODE_F10:
        builder.setSpecialKey(SpecialKey.F10);
        break;
      case android.view.KeyEvent.KEYCODE_F11:
        builder.setSpecialKey(SpecialKey.F11);
        break;
      case android.view.KeyEvent.KEYCODE_F12:
        builder.setSpecialKey(SpecialKey.F12);
        break;
      default: {
        // Reach here as the key code is not special one.
        // There are some cases.
        // 1. The event has unicode-character. Simple key event should be sent to the engine.
        //    Note that in this case no modifiers should be sent.
        // 2. No unicode-character.
        //   2-1. Printable character with modifier (except for shift key).
        //   2-2. Ignoreable key event.
        int unicodeChar = compactKeyEvent.getUnicodeCharacter();
        if (isPrintable(unicodeChar)) {
          // Case 1.
          return builder.setKeyCode(unicodeChar).build();
        }
        if (keyCode >= android.view.KeyEvent.KEYCODE_A
            && keyCode <= android.view.KeyEvent.KEYCODE_Z) {
          // Case 2-1.
          builder.setKeyCode(keyCode - android.view.KeyEvent.KEYCODE_A + 'a');
        } else if (keyCode >= android.view.KeyEvent.KEYCODE_0
            && keyCode <= android.view.KeyEvent.KEYCODE_9) {
          // Case 2-1.
          builder.setKeyCode(keyCode - android.view.KeyEvent.KEYCODE_0 + '0');
        } else {
          // Case 2-2.
          return null;
        }
      }
    }
    // Reach here because the key event is special one (including printable + modifier).
    int metaState = compactKeyEvent.getMetaState();
    if ((metaState & android.view.KeyEvent.META_SHIFT_MASK) != 0) {
        builder.addModifierKeys(ModifierKey.SHIFT);
    }
    if ((metaState & android.view.KeyEvent.META_ALT_MASK) != 0) {
      builder.addModifierKeys(ModifierKey.ALT);
    }
    if ((metaState & android.view.KeyEvent.META_CTRL_MASK) != 0) {
      builder.addModifierKeys(ModifierKey.CTRL);
    }
    return builder.build();
  }

  public KeyEventInterface getKeyEventInterface(final android.view.KeyEvent keyEvent) {
    Preconditions.checkNotNull(keyEvent);
    final CompactKeyEvent compactKeyEvent = new CompactKeyEvent(keyEvent, keyEventMapper);
    return new KeyEventInterface() {

      @Override
      public int getKeyCode() {
        return compactKeyEvent.getKeyCode();
      }

      @Override
      public Optional<android.view.KeyEvent> getNativeEvent() {
        return Optional.of(keyEvent);
      }
    };
  }

  public Optional<CompositionSwitchMode> getCompositionSwitchMode(
      android.view.KeyEvent keyEvent) {
    CompactKeyEvent compactKeyEvent
        = new CompactKeyEvent(Preconditions.checkNotNull(keyEvent), keyEventMapper);
    return isKeyForCompositinoModeChange(compactKeyEvent.getKeyCode(),
                                         compactKeyEvent.getMetaState())
        ? Optional.of(CompositionSwitchMode.TOGGLE)
        : Optional.<CompositionSwitchMode>absent();
  }

  public KeyboardSpecification getKanaKeyboardSpecification() {
    return kanaKeyboardSpecification;
  }

  public KeyboardSpecification getAlphabetKeyboardSpecification() {
    return alphabetKeyboardSpecification;
  }
}
