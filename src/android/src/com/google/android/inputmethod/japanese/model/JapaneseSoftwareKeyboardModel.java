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

package org.mozc.android.inputmethod.japanese.model;

import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.InputStyle;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.KeyboardLayout;

import android.inputmethodservice.InputMethodService;
import android.text.InputType;

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
 * We have two {@code KeyboardLayout}, which are {@code TWELVE_KEY} and {@code QWERTY},
 * and one experimental layout, {@code GODAN}.
 *
 * For {@code TWLEVE_KEY} layout, we have three {@code InputStyle}, which are {@code TOGGLE},
 * {@code FLICK} and {@code TOGGLE_FLICK}.
 * Also, users can use {@code QWERTY} style layout for alphabet mode by setting
 * {@code qwertyLayoutForAlphabet} {@code true}.
 *
 * {@code TWLEVE_KEY} layout has mainly three {@code KeyboardMode}, which are {@code KANA},
 * {@code ALPHABET} and {@code KANA_NUMBER}. If a user uses {@code QWERTY} style layout for
 * alphabet mode, s/he can use {@code ALPHABET_NUMBER} mode, which is {@code QWERTY} style number
 * key layout, as well in addition to the three {@code KeyboardMode}.
 *
 * For {@code QWERTY} layout, we have four {@code KeyboardMode}, which are {@code KANA},
 * {@code ALPHABET}, {@code KANA_NUMBER} and {@code ALPHABET_NUMBER}. The parameters,
 * {@code InputStyle} and {@code qwertyLayoutForAlphabet}, are simply ignored.
 *
 * This class manages the "default mode" of software keyboard depending on {@code inputType}.
 * It is expected that the {@code inputType} is given by system via
 * {@link InputMethodService#onStartInputView}.
 *
 */
public class JapaneseSoftwareKeyboardModel {
  public enum KeyboardMode {
    KANA, ALPHABET, KANA_NUMBER, ALPHABET_NUMBER,
  }

  private KeyboardLayout keyboardLayout = KeyboardLayout.TWELVE_KEYS;
  private KeyboardMode keyboardMode = KeyboardMode.KANA;
  private InputStyle inputStyle = InputStyle.TOGGLE;
  private boolean qwertyLayoutForAlphabet = false;
  private int inputType;

  // This is just saved mode for setInputType. So, after that when qwertyLayoutForAlphabet is
  // edited, the mode can be un-expected one.
  // (e.g., TWELVE_KEY_LAYOUT + (qwertyLayoutForAlphabet = false) + ALPHABET_NUMBER).
  // For now, getKeyboardLayout handles it, so it should fine. We may want to change the
  // strategy in future.
  private KeyboardMode savedMode = null;

  public JapaneseSoftwareKeyboardModel() {
  }

  public KeyboardLayout getKeyboardLayout() {
    return keyboardLayout;
  }

  public void setKeyboardLayout(KeyboardLayout keyboardLayout) {
    if (keyboardLayout == null) {
      throw new NullPointerException();
    }
    KeyboardMode mode = getPreferredKeyboardMode(this.inputType, keyboardLayout);
    if (mode == null) {
      mode = KeyboardMode.KANA;
    }

    this.keyboardLayout = keyboardLayout;
    // Reset keyboard mode as well.
    this.keyboardMode = mode;
    this.savedMode = null;
  }

  public KeyboardMode getKeyboardMode() {
    return keyboardMode;
  }

  public void setKeyboardMode(KeyboardMode keyboardMode) {
    if (keyboardMode == null) {
      throw new NullPointerException();
    }
    this.keyboardMode = keyboardMode;
  }

  public InputStyle getInputStyle() {
    return inputStyle;
  }

  public void setInputStyle(InputStyle inputStyle) {
    if (inputStyle == null) {
      throw new NullPointerException();
    }
    this.inputStyle = inputStyle;
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
    KeyboardMode mode = getPreferredKeyboardMode(inputType, this.keyboardLayout);
    if (mode != null) {
      if (getPreferredKeyboardMode(this.inputType, this.keyboardLayout) == null) {
        // Remember the current keyboard mode.
        savedMode = keyboardMode;
      }
    } else {
      // Restore the saved mode.
      mode = savedMode;
      savedMode = null;
    }

    this.inputType = inputType;
    if (mode != null) {
      this.keyboardMode = mode;
    }
  }

  private static KeyboardMode getPreferredKeyboardMode(int inputType, KeyboardLayout layout) {
    switch (inputType & InputType.TYPE_MASK_CLASS) {
      case InputType.TYPE_CLASS_DATETIME:
      case InputType.TYPE_CLASS_PHONE:
      case InputType.TYPE_CLASS_NUMBER:
        return layout == KeyboardLayout.TWELVE_KEYS
            ? KeyboardMode.KANA_NUMBER
            : KeyboardMode.ALPHABET_NUMBER;
      case InputType.TYPE_CLASS_TEXT:
        switch (inputType & InputType.TYPE_MASK_VARIATION) {
          case InputType.TYPE_TEXT_VARIATION_PASSWORD:
          case InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD:
          case InputType.TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS:
          case InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD:
            return KeyboardMode.ALPHABET;
        }
    }
    // KeyboardMode recommended strongly is not found here, so just return null.
    return null;
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
      KeyboardMode keyboardMode,
      InputStyle inputStyle,
      boolean qwertyLayoutForAlphabet) {
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
      case KANA_NUMBER: {
        switch (inputStyle) {
          case TOGGLE: return KeyboardSpecification.TWELVE_KEY_TOGGLE_NUMBER;
          case FLICK: return KeyboardSpecification.TWELVE_KEY_FLICK_NUMBER;
          case TOGGLE_FLICK: return KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_NUMBER;
        }
        break;
      }
    }
    throw new IllegalArgumentException(
        "Unknown keyboard state: "
            + keyboardMode + ", " + inputStyle + ", " + qwertyLayoutForAlphabet);
  }

  private static KeyboardSpecification getQwertyKeyboardSpecification(KeyboardMode keyboardMode) {
    switch (keyboardMode) {
      case KANA: return KeyboardSpecification.QWERTY_KANA;
      case KANA_NUMBER: return KeyboardSpecification.QWERTY_KANA_NUMBER;
      case ALPHABET: return KeyboardSpecification.QWERTY_ALPHABET;
      case ALPHABET_NUMBER: return KeyboardSpecification.QWERTY_ALPHABET_NUMBER;
    }
    throw new IllegalArgumentException("Unknown keyboard mode: " + keyboardMode);
  }

  private static KeyboardSpecification getGodanKeyboardSpecification(KeyboardMode keyboardMode) {
    switch (keyboardMode) {
      case KANA: return KeyboardSpecification.GODAN_KANA;
      case ALPHABET: return KeyboardSpecification.QWERTY_ALPHABET;
      case ALPHABET_NUMBER: return KeyboardSpecification.QWERTY_ALPHABET_NUMBER;
    }
    // KANA_NUMBER must be never used.
    throw new IllegalArgumentException("Unknown keyboard mode: " + keyboardMode);
  }
}
