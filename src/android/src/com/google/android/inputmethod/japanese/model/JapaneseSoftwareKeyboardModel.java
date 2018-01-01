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

package org.mozc.android.inputmethod.japanese.model;

import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.keyboard.Keyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.InputStyle;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.KeyboardLayout;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.inputmethodservice.InputMethodService;
import android.text.InputType;

import java.util.Collections;
import java.util.EnumSet;
import java.util.Set;

/**
 * Model class to represent Mozc's software keyboard.
 *
 * Currently, this class manages four states:
 * <ul>
 * <li>{@code KeyboardLayout}
 * <li>{@code KeyboardMode}
 * <li>{@code InputStyle}
 * <li>{@code qwertyLayoutForAlphabet}
 * </ul>
 *
 * For {@code TWLEVE_KEY} layout, we have three {@code InputStyle}, which are {@code TOGGLE},
 * {@code FLICK} and {@code TOGGLE_FLICK}.
 * Also, users can use {@code QWERTY} style layout for alphabet mode by setting
 * {@code qwertyLayoutForAlphabet} {@code true}.
 *
 * {@code TWLEVE_KEY} layout has two {@code KeyboardMode}, which are {@code KANA}, {@code ALPHABET}.
 *
 * On {@code SymbolInputView}, we have a special {@code KeyboardMode}, which is
 * {@code SYMBOL_NUMBER}. It is NOT used on normal view.
 *
 * For {@code QWERTY} layout, we have two {@code KeyboardMode}, which are {@code KANA},
 * {@code ALPHABET}. The parameters, {@code InputStyle} and {@code qwertyLayoutForAlphabet}, are
 * simply ignored.
 *
 * This class manages the "default mode" of software keyboard depending on {@code inputType}.
 * It is expected that the {@code inputType} is given by system via
 * {@link InputMethodService#onStartInputView}.
 *
 */
@SuppressWarnings("javadoc")
public class JapaneseSoftwareKeyboardModel {

  /**
   * Keyboard mode that indicates supported character types.
   */
  public enum KeyboardMode {
    KANA, ALPHABET, ALPHABET_NUMBER, NUMBER, SYMBOL_NUMBER,
  }

  /**
   * Keyboard modes which can be kept even if a focused text field is changed.
   */
  private static final Set<KeyboardMode> REMEMBERABLE_KEYBOARD_MODE_SET =
      Collections.unmodifiableSet(EnumSet.of(
          KeyboardMode.KANA,
          KeyboardMode.ALPHABET,
          KeyboardMode.ALPHABET_NUMBER));

  private static final KeyboardMode DEFAULT_KEYBOARD_MODE = KeyboardMode.KANA;

  private KeyboardLayout keyboardLayout = KeyboardLayout.TWELVE_KEYS;
  private KeyboardMode keyboardMode = DEFAULT_KEYBOARD_MODE;
  private InputStyle inputStyle = InputStyle.TOGGLE;
  private boolean qwertyLayoutForAlphabet = false;
  private int inputType;

  // This is just saved mode for setInputType. So, after that when qwertyLayoutForAlphabet is
  // edited, the mode can be un-expected one.
  // (e.g., TWELVE_KEY_LAYOUT + (qwertyLayoutForAlphabet = false) + ALPHABET_NUMBER).
  // For now, getKeyboardLayout handles it, so it should fine. We may want to change the
  // strategy in future.
  private Optional<KeyboardMode> savedMode = Optional.absent();

  public JapaneseSoftwareKeyboardModel() {
  }

  public KeyboardLayout getKeyboardLayout() {
    return keyboardLayout;
  }

  public void setKeyboardLayout(KeyboardLayout keyboardLayout) {
    Preconditions.checkNotNull(keyboardLayout);
    Optional<KeyboardMode> optionalMode = getPreferredKeyboardMode(this.inputType, keyboardLayout);
    KeyboardMode mode = optionalMode.or(DEFAULT_KEYBOARD_MODE);

    this.keyboardLayout = keyboardLayout;
    // Reset keyboard mode as well.
    this.keyboardMode = mode;
    this.savedMode = Optional.absent();
  }

  public KeyboardMode getKeyboardMode() {
    return keyboardMode;
  }

  public void setKeyboardMode(KeyboardMode keyboardMode) {
    this.keyboardMode = Preconditions.checkNotNull(keyboardMode);
  }

  public InputStyle getInputStyle() {
    return inputStyle;
  }

  public void setInputStyle(InputStyle inputStyle) {
    this.inputStyle = Preconditions.checkNotNull(inputStyle);
  }

  public boolean isQwertyLayoutForAlphabet() {
    return qwertyLayoutForAlphabet;
  }

  public void setQwertyLayoutForAlphabet(boolean qwertyLayoutForAlphabet) {
    this.qwertyLayoutForAlphabet = qwertyLayoutForAlphabet;
  }

  /**
   * Sets {@code inputType} and update the {@code keyboardMode} if necessary.
   * See the class comment for details.
   */
  public void setInputType(int inputType) {
    Optional<KeyboardMode> mode = getPreferredKeyboardMode(inputType, this.keyboardLayout);
    if (mode.isPresent()) {
      if (!getPreferredKeyboardMode(this.inputType, this.keyboardLayout).isPresent()) {
        // Remember the current keyboard mode.
        savedMode = Optional.of(keyboardMode);
      }
    } else {
      // Restore the saved mode.
      mode = savedMode;
      savedMode = Optional.absent();
    }

    this.inputType = inputType;
    if (mode.isPresent()) {
      this.keyboardMode = mode.get();
    } else if (!REMEMBERABLE_KEYBOARD_MODE_SET.contains(this.keyboardMode)) {
      // Disable a non-rememberable keyboard here since neither mode nor saved mode are specified.
      this.keyboardMode = DEFAULT_KEYBOARD_MODE;
    }
  }

  public static Optional<KeyboardMode> getPreferredKeyboardMode(
      int inputType, KeyboardLayout layout) {
    if (MozcUtil.isNumberKeyboardPreferred(inputType)) {
      switch (Preconditions.checkNotNull(layout)) {
        case GODAN:
        case QWERTY:
        case TWELVE_KEYS:
          return Optional.of(KeyboardMode.NUMBER);
      }
    }
    if ((inputType & InputType.TYPE_MASK_CLASS) == InputType.TYPE_CLASS_TEXT) {
      switch (inputType & InputType.TYPE_MASK_VARIATION) {
        case InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS:
        case InputType.TYPE_TEXT_VARIATION_PASSWORD:
        case InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD:
        case InputType.TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS:
        case InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD:
          return Optional.of(KeyboardMode.ALPHABET);
      }
    }
    // KeyboardMode recommended strongly is not found here, so just return null.
    return Optional.<KeyboardMode>absent();
  }

  /**
   * Returns {@link KeyboardSpecification} instance based on the current state.
   */
  public KeyboardSpecification getKeyboardSpecification() {
    return getKeyboardSpecificationInternal(
        keyboardLayout, keyboardMode, inputStyle, qwertyLayoutForAlphabet);
  }

  /**
   * Returns {@link KeyboardSpecification} instance based on the given parameters.
   */
  private static KeyboardSpecification getKeyboardSpecificationInternal(
      KeyboardLayout keyboardLayout,
      KeyboardMode keyboardMode,
      InputStyle inputStyle,
      boolean qwertyLayoutForAlphabet) {
    try {
      switch (keyboardLayout) {
        case TWELVE_KEYS:
          return getTwelveKeysKeyboardSpecification(
              keyboardMode, inputStyle, qwertyLayoutForAlphabet);
        case QWERTY:
          return getQwertyKeyboardSpecification(keyboardMode);
        case GODAN:
          return getGodanKeyboardSpecification(keyboardMode);
      }
    } catch (IllegalArgumentException e) {
      MozcLog.w("Unknown keyboard specification: ", e);
    }

    // Use TWELVE_KEY_TOGGLE_KANA for unknown state.
    return KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA;
  }

  private static KeyboardSpecification getTwelveKeysKeyboardSpecification(
      KeyboardMode keyboardMode, InputStyle inputStyle, boolean qwertyLayoutForAlphabet) {
    switch (keyboardMode) {
      case KANA: {
        switch (inputStyle) {
          case TOGGLE: return KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA;
          case FLICK: return KeyboardSpecification.TWELVE_KEY_FLICK_KANA;
          case TOGGLE_FLICK: return KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA;
        }
        break;
      }
      case ALPHABET: {
        if (qwertyLayoutForAlphabet) {
          return KeyboardSpecification.TWELVE_KEY_TOGGLE_QWERTY_ALPHABET;
        }
        switch (inputStyle) {
          case TOGGLE: return KeyboardSpecification.TWELVE_KEY_TOGGLE_ALPHABET;
          case FLICK: return KeyboardSpecification.TWELVE_KEY_FLICK_ALPHABET;
          case TOGGLE_FLICK: return KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_ALPHABET;
        }
        break;
      }
      case ALPHABET_NUMBER: {
        if (qwertyLayoutForAlphabet) {
          return KeyboardSpecification.QWERTY_ALPHABET_NUMBER;
        }
        switch (inputStyle) {
          case TOGGLE: return KeyboardSpecification.TWELVE_KEY_TOGGLE_ALPHABET;
          case FLICK: return KeyboardSpecification.TWELVE_KEY_FLICK_ALPHABET;
          case TOGGLE_FLICK: return KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_ALPHABET;
        }
        break;
      }
      case NUMBER:
        return KeyboardSpecification.NUMBER;
      case SYMBOL_NUMBER:
        return KeyboardSpecification.SYMBOL_NUMBER;
    }
    throw new IllegalArgumentException(
        "Unknown keyboard state: "
            + keyboardMode + ", " + inputStyle + ", " + qwertyLayoutForAlphabet);
  }

  private static KeyboardSpecification getQwertyKeyboardSpecification(KeyboardMode keyboardMode) {
    switch (keyboardMode) {
      case KANA: return KeyboardSpecification.QWERTY_KANA;
      case ALPHABET: return KeyboardSpecification.QWERTY_ALPHABET;
      case ALPHABET_NUMBER: return KeyboardSpecification.QWERTY_ALPHABET_NUMBER;
      case NUMBER: return KeyboardSpecification.NUMBER;
      case SYMBOL_NUMBER: return KeyboardSpecification.SYMBOL_NUMBER;
    }
    throw new IllegalArgumentException("Unknown keyboard mode: " + keyboardMode);
  }

  private static KeyboardSpecification getGodanKeyboardSpecification(KeyboardMode keyboardMode) {
    switch (keyboardMode) {
      case KANA: return KeyboardSpecification.GODAN_KANA;
      case ALPHABET: return KeyboardSpecification.QWERTY_ALPHABET;
      case ALPHABET_NUMBER: return KeyboardSpecification.QWERTY_ALPHABET_NUMBER;
      case NUMBER: return KeyboardSpecification.NUMBER;
      case SYMBOL_NUMBER: return KeyboardSpecification.SYMBOL_NUMBER;
    }
    throw new IllegalArgumentException("Unknown keyboard mode: " + keyboardMode);
  }

}
