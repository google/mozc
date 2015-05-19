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

package org.mozc.android.inputmethod.japanese.model;

import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.model.JapaneseSoftwareKeyboardModel.KeyboardMode;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.InputStyle;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.KeyboardLayout;
import org.mozc.android.inputmethod.japanese.testing.Parameter;

import android.text.InputType;

import junit.framework.TestCase;

/**
 */
public class JapaneseSoftwareKeyboardModelTest extends TestCase {

  public void testGetKeyboardSpecification() {
    class TestData extends Parameter {
      final KeyboardLayout keyboardLayout;
      final KeyboardMode keyboardMode;
      final InputStyle inputStyle;
      final boolean qwertyLayoutForAlphabet;
      final KeyboardSpecification expectedKeyboardSpecification;

      TestData(KeyboardLayout keyboardLayout, KeyboardMode keyboardMode,
               InputStyle inputStyle, boolean qwertyLayoutForAlphabet,
               KeyboardSpecification expectedKeyboardSpecification) {
        this.keyboardLayout = keyboardLayout;
        this.keyboardMode = keyboardMode;
        this.inputStyle = inputStyle;
        this.qwertyLayoutForAlphabet = qwertyLayoutForAlphabet;
        this.expectedKeyboardSpecification = expectedKeyboardSpecification;
      }
    }

    TestData[] testDataList = {
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.KANA,
                     InputStyle.TOGGLE, false,
                     KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.KANA,
                     InputStyle.TOGGLE, true,
                     KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.KANA,
                     InputStyle.FLICK, false,
                     KeyboardSpecification.TWELVE_KEY_FLICK_KANA),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.KANA,
                     InputStyle.FLICK, true,
                     KeyboardSpecification.TWELVE_KEY_FLICK_KANA),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.KANA,
                     InputStyle.TOGGLE_FLICK, false,
                     KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.KANA,
                     InputStyle.TOGGLE_FLICK, true,
                     KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.ALPHABET,
                     InputStyle.TOGGLE, false,
                     KeyboardSpecification.TWELVE_KEY_TOGGLE_ALPHABET),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.ALPHABET,
                     InputStyle.TOGGLE, true,
                     KeyboardSpecification.TWELVE_KEY_TOGGLE_QWERTY_ALPHABET),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.ALPHABET,
                     InputStyle.FLICK, false,
                     KeyboardSpecification.TWELVE_KEY_FLICK_ALPHABET),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.ALPHABET,
                     InputStyle.FLICK, true,
                     KeyboardSpecification.TWELVE_KEY_TOGGLE_QWERTY_ALPHABET),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.ALPHABET,
                     InputStyle.TOGGLE_FLICK, false,
                     KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_ALPHABET),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.ALPHABET,
                     InputStyle.TOGGLE_FLICK, true,
                     KeyboardSpecification.TWELVE_KEY_TOGGLE_QWERTY_ALPHABET),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.KANA_NUMBER,
                     InputStyle.TOGGLE, false,
                     KeyboardSpecification.TWELVE_KEY_TOGGLE_NUMBER),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.KANA_NUMBER,
                     InputStyle.TOGGLE, true,
                     KeyboardSpecification.TWELVE_KEY_TOGGLE_NUMBER),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.KANA_NUMBER,
                     InputStyle.FLICK, false,
                     KeyboardSpecification.TWELVE_KEY_FLICK_NUMBER),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.KANA_NUMBER,
                     InputStyle.FLICK, true,
                     KeyboardSpecification.TWELVE_KEY_FLICK_NUMBER),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.KANA_NUMBER,
                     InputStyle.TOGGLE_FLICK, false,
                     KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_NUMBER),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.KANA_NUMBER,
                     InputStyle.TOGGLE_FLICK, true,
                     KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_NUMBER),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.ALPHABET_NUMBER,
                     InputStyle.TOGGLE, false,
                     KeyboardSpecification.TWELVE_KEY_TOGGLE_ALPHABET),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.ALPHABET_NUMBER,
                     InputStyle.TOGGLE, true,
                     KeyboardSpecification.QWERTY_ALPHABET_NUMBER),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.ALPHABET_NUMBER,
                     InputStyle.FLICK, false,
                     KeyboardSpecification.TWELVE_KEY_FLICK_ALPHABET),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.ALPHABET_NUMBER,
                     InputStyle.FLICK, true,
                     KeyboardSpecification.QWERTY_ALPHABET_NUMBER),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.ALPHABET_NUMBER,
                     InputStyle.TOGGLE_FLICK, false,
                     KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_ALPHABET),
        new TestData(KeyboardLayout.TWELVE_KEYS, KeyboardMode.ALPHABET_NUMBER,
                     InputStyle.TOGGLE_FLICK, true,
                     KeyboardSpecification.QWERTY_ALPHABET_NUMBER),

        new TestData(KeyboardLayout.QWERTY, KeyboardMode.KANA,
                     InputStyle.TOGGLE, false,
                     KeyboardSpecification.QWERTY_KANA),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.KANA,
                     InputStyle.TOGGLE, true,
                     KeyboardSpecification.QWERTY_KANA),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.KANA,
                     InputStyle.FLICK, false,
                     KeyboardSpecification.QWERTY_KANA),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.KANA,
                     InputStyle.FLICK, true,
                     KeyboardSpecification.QWERTY_KANA),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.KANA,
                     InputStyle.TOGGLE_FLICK, false,
                     KeyboardSpecification.QWERTY_KANA),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.KANA,
                     InputStyle.TOGGLE_FLICK, true,
                     KeyboardSpecification.QWERTY_KANA),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.ALPHABET,
                     InputStyle.TOGGLE, false,
                     KeyboardSpecification.QWERTY_ALPHABET),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.ALPHABET,
                     InputStyle.TOGGLE, true,
                     KeyboardSpecification.QWERTY_ALPHABET),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.ALPHABET,
                     InputStyle.FLICK, false,
                     KeyboardSpecification.QWERTY_ALPHABET),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.ALPHABET,
                     InputStyle.FLICK, true,
                     KeyboardSpecification.QWERTY_ALPHABET),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.ALPHABET,
                     InputStyle.TOGGLE_FLICK, false,
                     KeyboardSpecification.QWERTY_ALPHABET),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.ALPHABET,
                     InputStyle.TOGGLE_FLICK, true,
                     KeyboardSpecification.QWERTY_ALPHABET),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.KANA_NUMBER,
                     InputStyle.TOGGLE, false,
                     KeyboardSpecification.QWERTY_KANA_NUMBER),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.KANA_NUMBER,
                     InputStyle.TOGGLE, true,
                     KeyboardSpecification.QWERTY_KANA_NUMBER),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.KANA_NUMBER,
                     InputStyle.FLICK, false,
                     KeyboardSpecification.QWERTY_KANA_NUMBER),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.KANA_NUMBER,
                     InputStyle.FLICK, true,
                     KeyboardSpecification.QWERTY_KANA_NUMBER),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.KANA_NUMBER,
                     InputStyle.TOGGLE_FLICK, false,
                     KeyboardSpecification.QWERTY_KANA_NUMBER),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.KANA_NUMBER,
                     InputStyle.TOGGLE_FLICK, true,
                     KeyboardSpecification.QWERTY_KANA_NUMBER),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.ALPHABET_NUMBER,
                     InputStyle.TOGGLE, false,
                     KeyboardSpecification.QWERTY_ALPHABET_NUMBER),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.ALPHABET_NUMBER,
                     InputStyle.TOGGLE, true,
                     KeyboardSpecification.QWERTY_ALPHABET_NUMBER),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.ALPHABET_NUMBER,
                     InputStyle.FLICK, false,
                     KeyboardSpecification.QWERTY_ALPHABET_NUMBER),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.ALPHABET_NUMBER,
                     InputStyle.FLICK, true,
                     KeyboardSpecification.QWERTY_ALPHABET_NUMBER),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.ALPHABET_NUMBER,
                     InputStyle.TOGGLE_FLICK, false,
                     KeyboardSpecification.QWERTY_ALPHABET_NUMBER),
        new TestData(KeyboardLayout.QWERTY, KeyboardMode.ALPHABET_NUMBER,
                     InputStyle.TOGGLE_FLICK, true,
                     KeyboardSpecification.QWERTY_ALPHABET_NUMBER),

        new TestData(KeyboardLayout.GODAN, KeyboardMode.KANA,
                     InputStyle.TOGGLE, false,
                     KeyboardSpecification.GODAN_KANA),
        new TestData(KeyboardLayout.GODAN, KeyboardMode.KANA,
                     InputStyle.TOGGLE, true,
                     KeyboardSpecification.GODAN_KANA),
        new TestData(KeyboardLayout.GODAN, KeyboardMode.KANA,
                     InputStyle.FLICK, false,
                     KeyboardSpecification.GODAN_KANA),
        new TestData(KeyboardLayout.GODAN, KeyboardMode.KANA,
                     InputStyle.FLICK, true,
                     KeyboardSpecification.GODAN_KANA),
        new TestData(KeyboardLayout.GODAN, KeyboardMode.KANA,
                     InputStyle.TOGGLE_FLICK, false,
                     KeyboardSpecification.GODAN_KANA),
        new TestData(KeyboardLayout.GODAN, KeyboardMode.KANA,
                     InputStyle.TOGGLE_FLICK, true,
                     KeyboardSpecification.GODAN_KANA),
        new TestData(KeyboardLayout.GODAN, KeyboardMode.ALPHABET,
                     InputStyle.TOGGLE, false,
                     KeyboardSpecification.QWERTY_ALPHABET),
        new TestData(KeyboardLayout.GODAN, KeyboardMode.ALPHABET,
                     InputStyle.TOGGLE, true,
                     KeyboardSpecification.QWERTY_ALPHABET),
        new TestData(KeyboardLayout.GODAN, KeyboardMode.ALPHABET,
                     InputStyle.FLICK, false,
                     KeyboardSpecification.QWERTY_ALPHABET),
        new TestData(KeyboardLayout.GODAN, KeyboardMode.ALPHABET,
                     InputStyle.FLICK, true,
                     KeyboardSpecification.QWERTY_ALPHABET),
        new TestData(KeyboardLayout.GODAN, KeyboardMode.ALPHABET,
                     InputStyle.TOGGLE_FLICK, false,
                     KeyboardSpecification.QWERTY_ALPHABET),
        new TestData(KeyboardLayout.GODAN, KeyboardMode.ALPHABET,
                     InputStyle.TOGGLE_FLICK, true,
                     KeyboardSpecification.QWERTY_ALPHABET),
        new TestData(KeyboardLayout.GODAN, KeyboardMode.ALPHABET_NUMBER,
                     InputStyle.TOGGLE, false,
                     KeyboardSpecification.QWERTY_ALPHABET_NUMBER),
        new TestData(KeyboardLayout.GODAN, KeyboardMode.ALPHABET_NUMBER,
                     InputStyle.TOGGLE, true,
                     KeyboardSpecification.QWERTY_ALPHABET_NUMBER),
        new TestData(KeyboardLayout.GODAN, KeyboardMode.ALPHABET_NUMBER,
                     InputStyle.FLICK, false,
                     KeyboardSpecification.QWERTY_ALPHABET_NUMBER),
        new TestData(KeyboardLayout.GODAN, KeyboardMode.ALPHABET_NUMBER,
                     InputStyle.FLICK, true,
                     KeyboardSpecification.QWERTY_ALPHABET_NUMBER),
        new TestData(KeyboardLayout.GODAN, KeyboardMode.ALPHABET_NUMBER,
                     InputStyle.TOGGLE_FLICK, false,
                     KeyboardSpecification.QWERTY_ALPHABET_NUMBER),
        new TestData(KeyboardLayout.GODAN, KeyboardMode.ALPHABET_NUMBER,
                     InputStyle.TOGGLE_FLICK, true,
                     KeyboardSpecification.QWERTY_ALPHABET_NUMBER),
    };

    JapaneseSoftwareKeyboardModel model = new JapaneseSoftwareKeyboardModel();
    for (TestData parameter : testDataList) {
      model.setKeyboardLayout(parameter.keyboardLayout);
      model.setKeyboardMode(parameter.keyboardMode);
      model.setInputStyle(parameter.inputStyle);
      model.setQwertyLayoutForAlphabet(parameter.qwertyLayoutForAlphabet);
      assertEquals(parameter.toString(),
                   parameter.expectedKeyboardSpecification,
                   model.getKeyboardSpecification());
    }
  }

  public void testInputType() {
    class TestData extends Parameter {
      final int inputType;
      final KeyboardMode expectedKeyboardMode;

      TestData(int inputType, KeyboardMode expectedKeyboardMode) {
        this.inputType = inputType;
        this.expectedKeyboardMode = expectedKeyboardMode;
      }
    }

    TestData[] testDataListForTwelveKeys = {
        new TestData(InputType.TYPE_CLASS_DATETIME, KeyboardMode.KANA_NUMBER),
        new TestData(InputType.TYPE_CLASS_DATETIME | InputType.TYPE_DATETIME_VARIATION_DATE,
                     KeyboardMode.KANA_NUMBER),
        new TestData(InputType.TYPE_CLASS_DATETIME | InputType.TYPE_DATETIME_VARIATION_NORMAL,
                     KeyboardMode.KANA_NUMBER),
        new TestData(InputType.TYPE_CLASS_DATETIME | InputType.TYPE_DATETIME_VARIATION_TIME,
                     KeyboardMode.KANA_NUMBER),
        new TestData(InputType.TYPE_CLASS_NUMBER, KeyboardMode.KANA_NUMBER),
        new TestData(InputType.TYPE_CLASS_PHONE, KeyboardMode.KANA_NUMBER),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS,
                     null),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_EMAIL_SUBJECT,
                     null),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_FILTER, null),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_LONG_MESSAGE, null),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_NORMAL, null),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD,
                     KeyboardMode.ALPHABET),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PERSON_NAME, null),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PHONETIC, null),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_POSTAL_ADDRESS,
                     null),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_SHORT_MESSAGE,
                     null),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI,
                     null),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD,
                     KeyboardMode.ALPHABET),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_WEB_EDIT_TEXT,
                     null),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS,
                     KeyboardMode.ALPHABET),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD,
                     KeyboardMode.ALPHABET),
    };

    JapaneseSoftwareKeyboardModel model = new JapaneseSoftwareKeyboardModel();
    model.setKeyboardLayout(KeyboardLayout.TWELVE_KEYS);
    model.setKeyboardMode(KeyboardMode.KANA);

    // Test for setting input type.
    for (TestData test : testDataListForTwelveKeys) {
      assertEquals(test.toString(), KeyboardMode.KANA, model.getKeyboardMode());
      model.setInputType(test.inputType);
      if (test.expectedKeyboardMode == null) {
        assertEquals(test.toString(), KeyboardMode.KANA, model.getKeyboardMode());
      } else {
        assertEquals(test.toString(), test.expectedKeyboardMode, model.getKeyboardMode());
      }
      model.setInputType(InputType.TYPE_NULL);
      assertEquals(test.toString(), KeyboardMode.KANA, model.getKeyboardMode());
    }

    // Test for setting keyboard layout.
    for (TestData test : testDataListForTwelveKeys) {
      model.setInputType(test.inputType);
      // Flips keyboard layout.
      model.setKeyboardLayout(KeyboardLayout.QWERTY);
      model.setKeyboardLayout(KeyboardLayout.TWELVE_KEYS);

      if (test.expectedKeyboardMode == null) {
        assertEquals(test.toString(), KeyboardMode.KANA, model.getKeyboardMode());
      } else {
        assertEquals(test.toString(), test.expectedKeyboardMode, model.getKeyboardMode());
      }
    }

    TestData[] testDataListForQwertyAndGodan = {
        new TestData(InputType.TYPE_CLASS_DATETIME, KeyboardMode.ALPHABET_NUMBER),
        new TestData(InputType.TYPE_CLASS_DATETIME | InputType.TYPE_DATETIME_VARIATION_DATE,
                     KeyboardMode.ALPHABET_NUMBER),
        new TestData(InputType.TYPE_CLASS_DATETIME | InputType.TYPE_DATETIME_VARIATION_NORMAL,
                     KeyboardMode.ALPHABET_NUMBER),
        new TestData(InputType.TYPE_CLASS_DATETIME | InputType.TYPE_DATETIME_VARIATION_TIME,
                     KeyboardMode.ALPHABET_NUMBER),
        new TestData(InputType.TYPE_CLASS_NUMBER, KeyboardMode.ALPHABET_NUMBER),
        new TestData(InputType.TYPE_CLASS_PHONE, KeyboardMode.ALPHABET_NUMBER),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS,
                     null),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_EMAIL_SUBJECT,
                     null),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_FILTER, null),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_LONG_MESSAGE, null),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_NORMAL, null),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD,
                     KeyboardMode.ALPHABET),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PERSON_NAME, null),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PHONETIC, null),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_POSTAL_ADDRESS,
                     null),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_SHORT_MESSAGE,
                     null),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI, null),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD,
                     KeyboardMode.ALPHABET),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_WEB_EDIT_TEXT,
                     null),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS,
                     KeyboardMode.ALPHABET),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD,
                     KeyboardMode.ALPHABET),
    };

    // Reset to qwerty.
    model.setInputType(InputType.TYPE_NULL);
    model.setKeyboardLayout(KeyboardLayout.QWERTY);
    model.setKeyboardMode(KeyboardMode.KANA);

    // Test for setting input type.
    for (TestData test : testDataListForQwertyAndGodan) {
      assertEquals(test.toString(), KeyboardMode.KANA, model.getKeyboardMode());
      model.setInputType(test.inputType);
      if (test.expectedKeyboardMode == null) {
        assertEquals(test.toString(), KeyboardMode.KANA, model.getKeyboardMode());
      } else {
        assertEquals(test.toString(), test.expectedKeyboardMode, model.getKeyboardMode());
      }
      model.setInputType(InputType.TYPE_NULL);
      assertEquals(test.toString(), KeyboardMode.KANA, model.getKeyboardMode());
    }

    // Test for setting keyboard layout.
    for (TestData test : testDataListForQwertyAndGodan) {
      model.setInputType(test.inputType);
      // Flips keyboard layout.
      model.setKeyboardLayout(KeyboardLayout.TWELVE_KEYS);
      model.setKeyboardLayout(KeyboardLayout.QWERTY);

      if (test.expectedKeyboardMode == null) {
        assertEquals(test.toString(), KeyboardMode.KANA, model.getKeyboardMode());
      } else {
        assertEquals(test.toString(), test.expectedKeyboardMode, model.getKeyboardMode());
      }
    }

    // Reset to Godan.
    model.setInputType(InputType.TYPE_NULL);
    model.setKeyboardLayout(KeyboardLayout.GODAN);
    model.setKeyboardMode(KeyboardMode.KANA);

    // Test for setting input type.
    for (TestData test : testDataListForQwertyAndGodan) {
      assertEquals(test.toString(), KeyboardMode.KANA, model.getKeyboardMode());
      model.setInputType(test.inputType);
      if (test.expectedKeyboardMode == null) {
        assertEquals(test.toString(), KeyboardMode.KANA, model.getKeyboardMode());
      } else {
        assertEquals(test.toString(), test.expectedKeyboardMode, model.getKeyboardMode());
      }
      model.setInputType(InputType.TYPE_NULL);
      assertEquals(test.toString(), KeyboardMode.KANA, model.getKeyboardMode());
    }

    // Test for setting keyboard layout.
    for (TestData test : testDataListForQwertyAndGodan) {
      model.setInputType(test.inputType);
      // Flips keyboard layout.
      model.setKeyboardLayout(KeyboardLayout.TWELVE_KEYS);
      model.setKeyboardLayout(KeyboardLayout.GODAN);

      if (test.expectedKeyboardMode == null) {
        assertEquals(test.toString(), KeyboardMode.KANA, model.getKeyboardMode());
      } else {
        assertEquals(test.toString(), test.expectedKeyboardMode, model.getKeyboardMode());
      }
    }

  }

  public void testInputTypeScenario() {
    JapaneseSoftwareKeyboardModel model = new JapaneseSoftwareKeyboardModel();
    model.setKeyboardLayout(KeyboardLayout.TWELVE_KEYS);
    model.setKeyboardMode(KeyboardMode.KANA);
    model.setInputType(InputType.TYPE_CLASS_NUMBER);
    assertEquals(KeyboardMode.KANA_NUMBER, model.getKeyboardMode());
    model.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
    assertEquals(KeyboardMode.ALPHABET, model.getKeyboardMode());
    model.setInputType(InputType.TYPE_NULL);
    // The initial state should be remembered.
    assertEquals(KeyboardMode.KANA, model.getKeyboardMode());

    model.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
    assertEquals(KeyboardMode.ALPHABET, model.getKeyboardMode());
    model.setKeyboardMode(KeyboardMode.KANA_NUMBER);
    assertEquals(KeyboardMode.KANA_NUMBER, model.getKeyboardMode());
    // Even after the keyboard mode overwriting, the keyboard mode should be revereted before
    // input-type-setting.
    model.setInputType(InputType.TYPE_NULL);
    assertEquals(KeyboardMode.KANA, model.getKeyboardMode());
  }
}
