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

package org.mozc.android.inputmethod.japanese.util;

import static org.easymock.EasyMock.eq;
import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.same;

import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.testing.ApiLevel;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.content.res.Resources.NotFoundException;
import android.content.res.XmlResourceParser;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.test.mock.MockResources;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.TypedValue;

import org.xmlpull.v1.XmlPullParserException;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;

/**
 * Note that we skip testing for the equals, hashCode, getMovie, obtainAttributes,
 * obtainTypedArray and toString methods,
 * because these methods or their returned values are difficult to be mocked.
 *
 */
public class ResourcesWrapperTest extends InstrumentationTestCaseWithMock {
  private ResourcesWrapper resources;
  private Resources base;

  @Override
  public void setUp() throws Exception {
    super.setUp();

    base = createMock(MockResources.class);
    resources = new ResourcesWrapper(base);
    resetAll();
  }

  @Override
  protected void tearDown() throws Exception {
    resources = null;
    base = null;
    super.tearDown();
  }

  @SmallTest
  public void testGetAnimation()  {
    XmlResourceParser parser = createMock(XmlResourceParser.class);
    expect(base.getAnimation(10)).andReturn(parser);
    replayAll();

    assertSame(parser, resources.getAnimation(10));

    verifyAll();
  }

  @SmallTest
  public void testGetBoolean() {
    expect(base.getBoolean(10)).andReturn(true);
    replayAll();

    assertTrue(resources.getBoolean(10));

    verifyAll();
  }

  @SmallTest
  public void testGetColor() {
    expect(base.getColor(10)).andReturn(0xFF808080);
    replayAll();

    assertEquals(0xFF808080, resources.getColor(10));

    verifyAll();
  }

  @SmallTest
  public void testGetColorStateList() {
    ColorStateList stateList = new ColorStateList(new int[0][0], new int[0]);
    expect(base.getColorStateList(10)).andReturn(stateList);
    replayAll();

    assertSame(stateList, resources.getColorStateList(10));

    verifyAll();
  }

  @SmallTest
  public void  testGetConfiguration() {
    Configuration configuration = new Configuration();
    expect(base.getConfiguration()).andReturn(configuration);
    replayAll();

    assertSame(configuration, resources.getConfiguration());

    verifyAll();
  }

  @SmallTest
  public void testGetDimension() {
    expect(base.getDimension(10)).andReturn(1.0f);
    replayAll();

    assertEquals(1.0f, resources.getDimension(10));

    verifyAll();
  }

  @SmallTest
  public void testGetDimensionPixelOffset() {
    expect(base.getDimensionPixelOffset(10)).andReturn(100);
    replayAll();

    assertEquals(100, resources.getDimensionPixelOffset(10));

    verifyAll();
  }

  @SmallTest
  public void testGetDimensionPixelSize() {
    expect(base.getDimensionPixelSize(10)).andReturn(100);
    replayAll();

    assertEquals(100, resources.getDimensionPixelSize(10));

    verifyAll();
  }

  @SmallTest
  public void testGetDisplayMetrics() {
    DisplayMetrics metrics = new DisplayMetrics();
    expect(base.getDisplayMetrics()).andReturn(metrics);
    replayAll();

    assertSame(metrics, resources.getDisplayMetrics());

    verifyAll();
  }

  @SmallTest
  public void testGetDrawable() {
    Drawable drawable = new ColorDrawable();
    expect(base.getDrawable(10)).andReturn(drawable);
    replayAll();

    assertSame(drawable, resources.getDrawable(10));

    verifyAll();
  }

  @ApiLevel(15)
  @TargetApi(15)
  @SmallTest
  public void testGetDrawableForDensity() {
    Drawable drawable = new ColorDrawable();
    expect(base.getDrawableForDensity(10, 2)).andReturn(drawable);
    replayAll();

    assertSame(drawable, resources.getDrawableForDensity(10, 2));

    verifyAll();
  }

  @SmallTest
  public void testGetFraction() {
    expect(base.getFraction(10, 100, 200)).andReturn(2.0f);
    replayAll();

    assertEquals(2.0f, resources.getFraction(10, 100, 200));

    verifyAll();
  }

  @SmallTest
  public void testGetIdentifier() {
    expect(base.getIdentifier("abc", "def", "ghi")).andReturn(10);
    replayAll();

    assertEquals(10, resources.getIdentifier("abc", "def", "ghi"));

    verifyAll();
  }

  @SmallTest
  public void testGetIntArray() {
    int[] array = new int[0];
    expect(base.getIntArray(10)).andReturn(array);
    replayAll();

    assertSame(array, resources.getIntArray(10));

    verifyAll();
  }

  @SmallTest
  public void testGetInteger() {
    expect(base.getInteger(10)).andReturn(100);
    replayAll();

    assertEquals(100, resources.getInteger(10));

    verifyAll();
  }

  @SmallTest
  public void testGetLayout() {
    XmlResourceParser parser = createMock(XmlResourceParser.class);
    expect(base.getLayout(10)).andReturn(parser);
    replayAll();

    assertEquals(parser, resources.getLayout(10));

    verifyAll();
  }

  @SmallTest
  public void testGetQuantityString1() {
    expect(base.getQuantityString(10, 20, "abcde"))
        .andReturn("abcdefg");
    replayAll();

    assertEquals("abcdefg", resources.getQuantityString(10, 20, "abcde"));

    verifyAll();
  }

  @SmallTest
  public void testGetQuantityString2() {
    expect(base.getQuantityString(10, 20)).andReturn("abcde");
    replayAll();

    assertEquals("abcde", resources.getQuantityString(10, 20));

    verifyAll();
  }

  @SmallTest
  public void testGetQuantityText() {
    expect(base.getQuantityText(10, 20)).andReturn("abcde");
    replayAll();

    assertEquals("abcde", resources.getQuantityText(10, 20));

    verifyAll();
  }

  @SmallTest
  public void testGetResourceEntryName() {
    expect(base.getResourceEntryName(10)).andReturn("abcde");
    replayAll();

    assertEquals("abcde", resources.getResourceEntryName(10));

    verifyAll();
  }

  @SmallTest
  public void testGetResourceName() {
    expect(base.getResourceName(10)).andReturn("abcde");
    replayAll();

    assertEquals("abcde", resources.getResourceName(10));

    verifyAll();
  }

  @SmallTest
  public void testGetResourcePackageName() {
    expect(base.getResourcePackageName(10)).andReturn("abcde");
    replayAll();

    assertEquals("abcde", resources.getResourcePackageName(10));

    verifyAll();
  }

  @SmallTest
  public void testGetResourceTypeName() {
    expect(base.getResourceTypeName(10)).andReturn("abcde");
    replayAll();

    assertEquals("abcde", resources.getResourceTypeName(10));

    verifyAll();
  }

  @SmallTest
  public void testGetString1() {
    expect(base.getString(10, "abcde")).andReturn("abcdefg");
    replayAll();

    assertEquals("abcdefg", resources.getString(10, "abcde"));

    verifyAll();
  }

  @SmallTest
  public void testGetString2() {
    expect(base.getString(10)).andReturn("abcde");
    replayAll();

    assertEquals("abcde", resources.getString(10));

    verifyAll();
  }

  @SmallTest
  public void testGetStringArray() {
    String[] array = new String[0];
    expect(base.getStringArray(10)).andReturn(array);
    replayAll();

    assertSame(array, resources.getStringArray(10));

    verifyAll();
  }

  @SmallTest
  public void testGetText1() {
    expect(base.getText(10, "abcde")).andReturn("abcdefg");
    replayAll();

    assertEquals("abcdefg", resources.getText(10, "abcde"));

    verifyAll();
  }

  @SmallTest
  public void testGetText2() {
    expect(base.getText(10)).andReturn("abcde");
    replayAll();

    assertEquals("abcde", resources.getText(10));

    verifyAll();
  }

  @SmallTest
  public void testGetTextArray() {
    CharSequence[] array = new CharSequence[0];
    expect(base.getTextArray(10)).andReturn(array);
    replayAll();

    assertSame(array, resources.getTextArray(10));

    verifyAll();
  }

  @SmallTest
  public void testGetValue1() {
    TypedValue outValue = new TypedValue();
    base.getValue(eq(10), same(outValue), eq(true));
    replayAll();

    resources.getValue(10, outValue, true);

    verifyAll();
  }

  @SmallTest
  public void testGetValue2() {
    TypedValue outValue = new TypedValue();
    base.getValue(eq("abcde"), same(outValue), eq(true));
    replayAll();

    resources.getValue("abcde", outValue, true);

    verifyAll();
  }

  @ApiLevel(15)
  @TargetApi(15)
  @SmallTest
  public void testGetValueForDensity() {
    TypedValue outValue = new TypedValue();
    base.getValueForDensity(eq(10), eq(2), same(outValue), eq(true));
    replayAll();

    resources.getValueForDensity(10, 2, outValue, true);

    verifyAll();
  }

  @SmallTest
  public void testGetXml() {
    XmlResourceParser parser = createMock(XmlResourceParser.class);
    expect(base.getXml(10)).andReturn(parser);
    replayAll();

    assertSame(parser, resources.getXml(10));

    verifyAll();
  }

  @SmallTest
  public void testOpenRawResource1() {
    InputStream stream = createMock(InputStream.class);
    TypedValue value = new TypedValue();
    expect(base.openRawResource(eq(10), same(value))).andReturn(stream);
    replayAll();

    assertSame(stream, resources.openRawResource(10, value));

    verifyAll();
  }

  @SmallTest
  public void testOpenRawResource2() throws NotFoundException {
    InputStream stream = createMock(InputStream.class);
    expect(base.openRawResource(10)).andReturn(stream);
    replayAll();

    assertSame(stream, resources.openRawResource(10));

    verifyAll();
  }

  @SmallTest
  public void testOpenRawResourceFd() throws IOException {
    File tempDir = getInstrumentation().getTargetContext().getDir("test", Context.MODE_PRIVATE);
    ParcelFileDescriptor parcelFileDescriptor =
        ParcelFileDescriptor.open(
            File.createTempFile("temp", "file", tempDir),
            ParcelFileDescriptor.MODE_READ_ONLY);
    boolean succeeded = false;
    try {
      // The constructor doesn't accept null 1st parameter.
      // We need temporal descriptor.
      AssetFileDescriptor fd = new AssetFileDescriptor(parcelFileDescriptor, 0, 0);
      expect(base.openRawResourceFd(10)).andReturn(fd);
      replayAll();

      assertSame(fd, resources.openRawResourceFd(10));

      verifyAll();

      succeeded = true;
    } finally {
      MozcUtil.close(parcelFileDescriptor, !succeeded);
    }
  }

  @SmallTest
  public void testParseBundleExtra() throws XmlPullParserException {
    AttributeSet attrs = createMock(AttributeSet.class);
    Bundle outBundle = new Bundle();
    base.parseBundleExtra(eq("abcde"), same(attrs), same(outBundle));
    replayAll();

    resources.parseBundleExtra("abcde", attrs, outBundle);

    verifyAll();
  }

  @SmallTest
  public void testParseBundleExtras() throws XmlPullParserException, IOException {
    XmlResourceParser parser = createMock(XmlResourceParser.class);
    Bundle outBundle = new Bundle();
    base.parseBundleExtras(same(parser), same(outBundle));
    replayAll();

    resources.parseBundleExtras(parser, outBundle);

    verifyAll();
  }

  @SmallTest
  public void testUpdateConfiguration() {
    Configuration config = new Configuration();
    DisplayMetrics metrics = new DisplayMetrics();
    base.updateConfiguration(same(config), same(metrics));
    replayAll();

    resources.updateConfiguration(config, metrics);

    verifyAll();
  }
}
