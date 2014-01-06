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

package org.mozc.android.inputmethod.japanese.keyboard;

import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.resources.R;

import android.content.res.Resources;
import android.content.res.Resources.NotFoundException;
import android.test.InstrumentationTestCase;
import android.test.MoreAsserts;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.DisplayMetrics;
import android.util.TypedValue;

import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;

/**
 */
public class KeyboardParserTest extends InstrumentationTestCase {
  private static int getDefaultKeyCode(Keyboard keyboard, int rowIndex, int colIndex) {
    Row row = keyboard.getRowList().get(rowIndex);
    Key key = row.getKeyList().get(colIndex);
    KeyState keyState = key.getKeyState(MetaState.UNMODIFIED);
    return keyState.getFlick(Flick.Direction.CENTER).getKeyEntity().getKeyCode();
  }

  private static int intToComplex(int value) {
    if (value != value << TypedValue.COMPLEX_MANTISSA_SHIFT >> TypedValue.COMPLEX_MANTISSA_SHIFT) {
      throw new IllegalArgumentException("Complex doesn't support: " + value);
    }
    return ((value & TypedValue.COMPLEX_MANTISSA_MASK) << TypedValue.COMPLEX_MANTISSA_SHIFT)
        | (TypedValue.COMPLEX_RADIX_23p0 << TypedValue.COMPLEX_RADIX_SHIFT);
  }

  @SmallTest
  public void testGetDimensionOrFraction() {
    DisplayMetrics metrics = new DisplayMetrics();
    TypedValue value = new TypedValue();

    // If the value is null, default value should be returned.
    assertEquals(10, KeyboardParser.getDimensionOrFraction(null, 500, 10, metrics));
    assertEquals(30, KeyboardParser.getDimensionOrFraction(null, 500, 30, metrics));

    // If the type is dimension, the pixel should be calculated based on metrics.
    value.type = TypedValue.TYPE_DIMENSION;
    value.data = intToComplex(100) | (TypedValue.COMPLEX_UNIT_DIP << TypedValue.COMPLEX_UNIT_SHIFT);
    metrics.density = 10;
    assertEquals(1000, KeyboardParser.getDimensionOrFraction(value, 500, 30, metrics));

    // If the type is fraction, the result is calculated based on given 'base' parameter.
    value.type = TypedValue.TYPE_FRACTION;
    value.data = intToComplex(10) |
        (TypedValue.COMPLEX_UNIT_FRACTION << TypedValue.COMPLEX_UNIT_SHIFT);
    assertEquals(5000, KeyboardParser.getDimensionOrFraction(value, 500, 30, metrics));

    // Otherwise exception should be thrown.
    value.type = TypedValue.TYPE_STRING;
    value.data = 0;
    try {
      KeyboardParser.getDimensionOrFraction(value, 500, 30, metrics);
      fail("IllegalArgumentException is expected.");
    } catch (IllegalArgumentException e) {
      // Pass.
    }
  }

  @SmallTest
  public void testGetCode() {
    TypedValue value = new TypedValue();

    // If the value is null, default value should be returned.
    assertEquals(10, KeyboardParser.getCode(null, 10));
    assertEquals(0x61, KeyboardParser.getCode(null, 0x61));

    // If the value is decimal or hexadecimal integer, the value should be returned.
    value.type = TypedValue.TYPE_INT_DEC;
    value.data = 30;
    assertEquals(30, KeyboardParser.getCode(value, 10));

    value.type = TypedValue.TYPE_INT_HEX;
    value.data = 30;
    assertEquals(30, KeyboardParser.getCode(value, 10));

    // If type is string, the whole string will be parsed.
    value.type = TypedValue.TYPE_STRING;
    value.data = 0;
    value.string = "12345";
    assertEquals(12345, KeyboardParser.getCode(value, 10));

    value.string = "123asd";
    try {
      KeyboardParser.getCode(value, 10);
      fail("An exception is expected.");
    } catch (NumberFormatException e) {
      // pass
    }
  }

  @SmallTest
  public void testToStringOrNull() {
    // Null should be returned when null is a given argument.
    assertNull(KeyboardParser.toStringOrNull(null));

    // Otherwise, the toString'ed value is returned.
    assertEquals("abcde", KeyboardParser.toStringOrNull("abcde"));
  }

  @SmallTest
  public void testParseKeyboard()
      throws NotFoundException, XmlPullParserException, IOException {
    Resources resources = getInstrumentation().getTargetContext().getResources();
    int keyboardWidth = 480;
    int keyboardHeight = 200;
    // Adding a mock resource on testing package causes a compile error for now,
    // So, we'll use product resource for testing purpose.
    // TODO(hidehiko): Figure out how to create a mock resource and replace this by it.
    KeyboardParser parser = new KeyboardParser(
        resources, resources.getXml(R.xml.kbd_12keys_flick_kana), keyboardWidth, keyboardHeight);

    // Note: put test XML file under tests/res directory doesn't work,
    // so, instead, read actual 12keys keyboard layout and check its properties.
    Keyboard keyboard = parser.parseKeyboard();

    // Make sure (x,y)-coords are expectedly annotated.
    {
      int y = 0;
      for (Row row : keyboard.getRowList()) {
        int x = 0;
        for (Key key : row.getKeyList()) {
          assertEquals(x, key.getX());
          assertEquals(y, key.getY());
          x += key.getWidth();
        }
        y += row.getHeight() + row.getVerticalGap();
      }
    }

    // The keyboard should have 5 rows, and each row should contain 5 keys.
    // The top row is a dummy row in order to make a space at top of the keyboard.
    // We'll skip it in the assertions below.
    assertEquals(5, keyboard.getRowList().size());
    for (Row row : keyboard.getRowList().subList(1, 5)) {
      assertEquals(5, row.getKeyList().size());
    }

    // Make sure main 12 keys' keycodes.
    assertEquals('1', getDefaultKeyCode(keyboard, 1, 1));
    assertEquals('2', getDefaultKeyCode(keyboard, 1, 2));
    assertEquals('3', getDefaultKeyCode(keyboard, 1, 3));

    assertEquals('4', getDefaultKeyCode(keyboard, 2, 1));
    assertEquals('5', getDefaultKeyCode(keyboard, 2, 2));
    assertEquals('6', getDefaultKeyCode(keyboard, 2, 3));

    assertEquals('7', getDefaultKeyCode(keyboard, 3, 1));
    assertEquals('8', getDefaultKeyCode(keyboard, 3, 2));
    assertEquals('9', getDefaultKeyCode(keyboard, 3, 3));

    assertEquals('*', getDefaultKeyCode(keyboard, 4, 1));
    assertEquals('0', getDefaultKeyCode(keyboard, 4, 2));
    assertEquals('#', getDefaultKeyCode(keyboard, 4, 3));

    // A key to change keyboard-layout should have longPressKeycode.
    MoreAsserts.assertNotEqual(
        KeyEntity.INVALID_KEY_CODE,
        keyboard.getRowList().get(4).getKeyList().get(0).getKeyState(KeyState.MetaState.UNMODIFIED)
            .getFlick(Flick.Direction.CENTER).getKeyEntity().getLongPressKeyCode());

    // Heights of rows are inherited from Keyboard element.
    for (Row row : keyboard.getRowList().subList(1, 5)) {
      assertTrue(row.getHeight() > 0);
    }

    // Following hard-coded parameters come from kbd_12keys_flick_kana.xml.
    // TODO(hidehiko): Use a layout resource only for testing purpose, or create a mock xml data.
    int keyWidth = (int) Math.round(keyboardWidth * 21.8 / 100);
    int functionKeyWidth = (int) Math.round(keyboardWidth * 17.3 / 100);

    // Width of keys are inherited from Keyboard element, but can be overwritten.
    for (Row row : keyboard.getRowList().subList(1, 5)) {
      // Left/Right side key.
      assertEquals(functionKeyWidth, row.getKeyList().get(0).getWidth());
      assertEquals(functionKeyWidth, row.getKeyList().get(4).getWidth());

      // Other keys.
      assertEquals(keyWidth, row.getKeyList().get(1).getWidth());
      assertEquals(keyWidth, row.getKeyList().get(2).getWidth());
      assertEquals(keyWidth, row.getKeyList().get(3).getWidth());
    }

    // All right side keys have isRepeatable attribute.
    for (Row row : keyboard.getRowList().subList(1, 5)) {
      assertTrue(row.getKeyList().get(4).isRepeatable());
    }

    // All keys should have popup.
    for (Row row : keyboard.getRowList().subList(1, 5)) {
      for (Key key : row.getKeyList()) {
        Flick flick = key.getKeyState(MetaState.UNMODIFIED).getFlick(Flick.Direction.CENTER);
        assertNotNull(flick.getKeyEntity().getPopUp());
      }
    }
  }

  @SmallTest
  public void testKeyboardParserBlackBoxTest() throws XmlPullParserException, IOException {
    // Just in order to test all keyboard xml files can be parsed without exception,
    // we'll try to just parse them.
    // TODO(hidehiko): Move this test to somewhere, when we support regression test framework
    //                 for MechaMozc.
    int[] resourceIds = {
        R.xml.kbd_12keys_kana,
        R.xml.kbd_12keys_flick_kana,
        R.xml.kbd_12keys_abc,
        R.xml.kbd_12keys_123,
        R.xml.kbd_12keys_qwerty_abc,
        R.xml.kbd_qwerty_kana,
        R.xml.kbd_qwerty_kana_123,
        R.xml.kbd_qwerty_abc,
        R.xml.kbd_qwerty_abc_123,
    };

    Resources resources = getInstrumentation().getTargetContext().getResources();
    for (int resourceId : resourceIds) {
      KeyboardParser parser = new KeyboardParser(resources, resources.getXml(resourceId), 480, 200);
      assertNotNull(parser.parseKeyboard());
    }
  }
}
