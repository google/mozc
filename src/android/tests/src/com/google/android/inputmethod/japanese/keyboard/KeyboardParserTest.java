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

package org.mozc.android.inputmethod.japanese.keyboard;

import org.mozc.android.inputmethod.japanese.keyboard.Keyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.keyboard.KeyboardParser.KeyAttributes;
import com.google.common.base.Optional;

import android.content.res.Resources;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.DisplayMetrics;
import android.util.TypedValue;

import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/**
 */
public class KeyboardParserTest extends InstrumentationTestCase {

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
    assertEquals(10, KeyboardParser.getDimensionOrFraction(
        Optional.<TypedValue>absent(), 500, 10, metrics));
    assertEquals(30, KeyboardParser.getDimensionOrFraction(
        Optional.<TypedValue>absent(), 500, 30, metrics));

    // If the type is dimension, the pixel should be calculated based on metrics.
    value.type = TypedValue.TYPE_DIMENSION;
    value.data = intToComplex(100) | (TypedValue.COMPLEX_UNIT_DIP << TypedValue.COMPLEX_UNIT_SHIFT);
    metrics.density = 10;
    assertEquals(1000, KeyboardParser.getDimensionOrFraction(Optional.of(value), 500, 30, metrics));

    // If the type is fraction, the result is calculated based on given 'base' parameter.
    value.type = TypedValue.TYPE_FRACTION;
    value.data = intToComplex(10) |
        (TypedValue.COMPLEX_UNIT_FRACTION << TypedValue.COMPLEX_UNIT_SHIFT);
    assertEquals(5000, KeyboardParser.getDimensionOrFraction(Optional.of(value), 500, 30, metrics));

    // Otherwise exception should be thrown.
    value.type = TypedValue.TYPE_STRING;
    value.data = 0;
    try {
      KeyboardParser.getDimensionOrFraction(Optional.of(value), 500, 30, metrics);
      fail("IllegalArgumentException is expected.");
    } catch (IllegalArgumentException e) {
      // Pass.
    }
  }

  @SmallTest
  public void testGetCode() {
    TypedValue value = new TypedValue();

    // If the value is Optional.absent(), default value should be returned.
    assertEquals(10, KeyboardParser.getCode(Optional.<TypedValue>absent(), 10));
    assertEquals(0x61, KeyboardParser.getCode(Optional.<TypedValue>absent(), 0x61));

    // If the value is decimal or hexadecimal integer, the value should be returned.
    value.type = TypedValue.TYPE_INT_DEC;
    value.data = 30;
    assertEquals(30, KeyboardParser.getCode(Optional.of(value), 10));

    value.type = TypedValue.TYPE_INT_HEX;
    value.data = 30;
    assertEquals(30, KeyboardParser.getCode(Optional.of(value), 10));

    // If type is string, the whole string will be parsed.
    value.type = TypedValue.TYPE_STRING;
    value.data = 0;
    value.string = "12345";
    assertEquals(12345, KeyboardParser.getCode(Optional.of(value), 10));

    value.string = "123asd";
    try {
      KeyboardParser.getCode(Optional.of(value), 10);
      fail("An exception is expected.");
    } catch (NumberFormatException e) {
      // pass
    }
  }

  @SmallTest
  public void testKeyboardParserBlackBoxTest() throws XmlPullParserException, IOException {
    // Just in order to test all keyboard xml files can be parsed without exception,
    // we'll try to just parse them.
    // TODO(hidehiko): Move this test to somewhere, when we support regression test framework
    //                 for MechaMozc.
    Resources resources = getInstrumentation().getTargetContext().getResources();
    for (KeyboardSpecification specification : KeyboardSpecification.values()) {
      if (specification.getXmlLayoutResourceId() == 0) {
        continue;
      }
      KeyboardParser parser = new KeyboardParser(resources, 480, 200, specification);
      assertNotNull(parser.parseKeyboard());
    }
  }

  @SmallTest
  public void testBuildKeyList_simpleCase() {
    List<Key> keys = KeyboardParser.buildKeyList(Arrays.asList(
        KeyAttributes.newBuilder().setWidth(50).build(),
        KeyAttributes.newBuilder().setHorizontalLayoutWeight(4).build(),
        KeyAttributes.newBuilder().setHorizontalLayoutWeight(1).build()),
        0, 100);
    assertEquals(0, keys.get(0).getX());
    assertEquals(50, keys.get(0).getWidth());
    assertEquals(50, keys.get(1).getX());
    assertEquals(40, keys.get(1).getWidth());
    assertEquals(90, keys.get(2).getX());
    assertEquals(10, keys.get(2).getWidth());
  }

  @SmallTest
  public void testBuildKeyList_widthAndWeight1() {
    List<Key> keys = KeyboardParser.buildKeyList(Arrays.asList(
        KeyAttributes.newBuilder().setWidth(50).build(),
        KeyAttributes.newBuilder().setWidth(10).setHorizontalLayoutWeight(2).build(),
        KeyAttributes.newBuilder().setWidth(10).setHorizontalLayoutWeight(1).build()),
        0, 100);
    assertEquals(0, keys.get(0).getX());
    assertEquals(50, keys.get(0).getWidth());
    assertEquals(50, keys.get(1).getX());
    assertEquals(30, keys.get(1).getWidth());
    assertEquals(80, keys.get(2).getX());
    assertEquals(20, keys.get(2).getWidth());
  }

  @SmallTest
  public void testBuildKeyList_widthAndWeight2() {
    List<Key> keys = KeyboardParser.buildKeyList(Arrays.asList(
        KeyAttributes.newBuilder().setWidth(10).setHorizontalLayoutWeight(1).build(),
        KeyAttributes.newBuilder().setWidth(50).build(),
        KeyAttributes.newBuilder().setWidth(10).setHorizontalLayoutWeight(2).build()),
        0, 100);
    assertEquals(0, keys.get(0).getX());
    assertEquals(20, keys.get(0).getWidth());
    assertEquals(20, keys.get(1).getX());
    assertEquals(50, keys.get(1).getWidth());
    assertEquals(70, keys.get(2).getX());
    assertEquals(30, keys.get(2).getWidth());
  }

  @SmallTest
  public void testBuildKeyList_widthAndWeight3() {
    List<Key> keys = KeyboardParser.buildKeyList(Arrays.asList(
        KeyAttributes.newBuilder().setWidth(10).setHorizontalLayoutWeight(1).build(),
        KeyAttributes.newBuilder().setWidth(10).setHorizontalLayoutWeight(2).build(),
        KeyAttributes.newBuilder().setWidth(50).build()),
        0, 100);
    assertEquals(0, keys.get(0).getX());
    assertEquals(20, keys.get(0).getWidth());
    assertEquals(20, keys.get(1).getX());
    assertEquals(30, keys.get(1).getWidth());
    assertEquals(50, keys.get(2).getX());
    assertEquals(50, keys.get(2).getWidth());
  }

  @SmallTest
  public void testBuildKeyList_widthAndWeight4() {
    List<Key> keys = KeyboardParser.buildKeyList(Arrays.asList(
        KeyAttributes.newBuilder().setWidth(20).build(),
        KeyAttributes.newBuilder().setHorizontalLayoutWeight(1).build(),
        KeyAttributes.newBuilder().setHorizontalLayoutWeight(1).build(),
        KeyAttributes.newBuilder().setHorizontalLayoutWeight(1).build()),
        0, 100);
    assertEquals(0, keys.get(0).getX());
    assertEquals(20, keys.get(0).getWidth());
    assertEquals(20, keys.get(1).getX());
    assertEquals(27, keys.get(1).getWidth());
    assertEquals(47, keys.get(2).getX());
    assertEquals(26, keys.get(2).getWidth());
    assertEquals(73, keys.get(3).getX());
    assertEquals(27, keys.get(3).getWidth());
  }
}
