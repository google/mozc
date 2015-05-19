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

package org.mozc.android.inputmethod.japanese.view;

import static org.easymock.EasyMock.expect;

import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import org.mozc.android.inputmethod.japanese.testing.VisibilityProxy;

import android.content.res.Resources;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.test.mock.MockResources;

/**
 */
public class EmojiDrawableFactoryTest extends InstrumentationTestCaseWithMock {
  private static final String PACKAGE_NAME = "org.mozc.android.inputmethod.japanese";

  public void testGetDrawable_nonEmojiCodePoint() {
    Resources resources = createMock(MockResources.class);
    EmojiDrawableFactory emojiFactory = new EmojiDrawableFactory(resources);
    MozcDrawableFactory drawableFactory = createMockBuilder(MozcDrawableFactory.class)
        .withConstructor(Resources.class)
        .withArgs(resources)
        .createMock();
    VisibilityProxy.setField(emojiFactory, "factory", drawableFactory);

    resetAll();
    replayAll();

    assertNull(emojiFactory.getDrawable('A'));

    verifyAll();
  }

  public void testGetDrawable_notFound() {
    Resources resources = createMock(MockResources.class);
    EmojiDrawableFactory emojiFactory = new EmojiDrawableFactory(resources);
    MozcDrawableFactory drawableFactory = createMockBuilder(MozcDrawableFactory.class)
        .withConstructor(Resources.class)
        .withArgs(resources)
        .createMock();
    VisibilityProxy.setField(emojiFactory, "factory", drawableFactory);

    resetAll();
    expect(resources.getIdentifier("docomo_emoji_fe000", "raw", PACKAGE_NAME)).andReturn(0);
    replayAll();

    emojiFactory.setProviderType(EmojiProviderType.DOCOMO);
    assertNull(emojiFactory.getDrawable(0xFE000));

    verifyAll();

    resetAll();
    // For the second time, cached null should be hit.
    replayAll();

    assertNull(emojiFactory.getDrawable(0xFE000));

    verifyAll();
  }

  public void testGetDrawable() {
    Resources resources = createMock(MockResources.class);
    EmojiDrawableFactory emojiFactory = new EmojiDrawableFactory(resources);
    MozcDrawableFactory drawableFactory = createMockBuilder(MozcDrawableFactory.class)
        .withConstructor(Resources.class)
        .withArgs(resources)
        .createMock();
    VisibilityProxy.setField(emojiFactory, "factory", drawableFactory);

    Drawable mockDrawable = new ColorDrawable();

    class TestData extends Parameter {
      final EmojiProviderType emojiProviderType;
      final int codePoint;
      final String expectedResourceName;

      TestData(EmojiProviderType emojiProviderType, int codePoint, String expectedResourceName) {
        this.emojiProviderType = emojiProviderType;
        this.codePoint = codePoint;
        this.expectedResourceName = expectedResourceName;
      }
    }
    TestData[] testDataList = new TestData[] {
        new TestData(EmojiProviderType.DOCOMO, 0xFE000, "docomo_emoji_fe000"),
        new TestData(EmojiProviderType.DOCOMO, 0xFE001, "docomo_emoji_fe001"),

        new TestData(EmojiProviderType.SOFTBANK, 0xFE000, "softbank_emoji_fe000"),
        new TestData(EmojiProviderType.SOFTBANK, 0xFE001, "softbank_emoji_fe001"),

        new TestData(EmojiProviderType.KDDI, 0xFE000, "kddi_emoji_fe000"),
        new TestData(EmojiProviderType.KDDI, 0xFE001, "kddi_emoji_fe001"),
    };

    for (TestData testData : testDataList) {
      resetAll();
      expect(resources.getIdentifier(testData.expectedResourceName, "raw", PACKAGE_NAME))
          .andReturn(1);
      expect(drawableFactory.getDrawable(1)).andReturn(mockDrawable);
      replayAll();

      emojiFactory.setProviderType(testData.emojiProviderType);
      assertSame(mockDrawable, emojiFactory.getDrawable(testData.codePoint));

      verifyAll();

      resetAll();
      // For the second time, cached drawable should be hit.
      replayAll();

      assertSame(mockDrawable, emojiFactory.getDrawable(0xFE000));

      verifyAll();

      // Once the provider is changed, the cache should be cleared.
      resetAll();
      expect(resources.getIdentifier(testData.expectedResourceName, "raw", PACKAGE_NAME))
          .andReturn(1);
      expect(drawableFactory.getDrawable(1)).andReturn(mockDrawable);
      replayAll();

      emojiFactory.setProviderType(EmojiProviderType.NONE);
      emojiFactory.setProviderType(testData.emojiProviderType);
      assertSame(mockDrawable, emojiFactory.getDrawable(testData.codePoint));

      verifyAll();
    }
  }

  public void testHasDrawable() {
    Resources resources = createNiceMock(MockResources.class);
    EmojiDrawableFactory emojiFactory = new EmojiDrawableFactory(resources);

    // Set up mock.
    expect(resources.getIdentifier("docomo_emoji_fe000", "raw", PACKAGE_NAME))
        .andStubReturn(1);
    expect(resources.getIdentifier("docomo_emoji_fe001", "raw", PACKAGE_NAME))
        .andStubReturn(2);
    expect(resources.getIdentifier("kddi_emoji_fe000", "raw", PACKAGE_NAME))
        .andStubReturn(3);
    expect(resources.getIdentifier("kddi_emoji_fe001", "raw", PACKAGE_NAME))
        .andStubReturn(4);
    expect(resources.getIdentifier("softbank_emoji_fe000", "raw", PACKAGE_NAME))
        .andStubReturn(5);
    expect(resources.getIdentifier("softbank_emoji_fe001", "raw", PACKAGE_NAME))
        .andStubReturn(6);

    replayAll();

    emojiFactory.setProviderType(EmojiProviderType.DOCOMO);
    assertFalse(emojiFactory.hasDrawable(0xFDFFF));
    assertTrue(emojiFactory.hasDrawable(0xFE000));
    assertTrue(emojiFactory.hasDrawable(0xFE001));
    assertFalse(emojiFactory.hasDrawable(0xFE002));

    emojiFactory.setProviderType(EmojiProviderType.KDDI);
    assertFalse(emojiFactory.hasDrawable(0xFDFFF));
    assertTrue(emojiFactory.hasDrawable(0xFE000));
    assertTrue(emojiFactory.hasDrawable(0xFE001));
    assertFalse(emojiFactory.hasDrawable(0xFE002));

    emojiFactory.setProviderType(EmojiProviderType.SOFTBANK);
    assertFalse(emojiFactory.hasDrawable(0xFDFFF));
    assertTrue(emojiFactory.hasDrawable(0xFE000));
    assertTrue(emojiFactory.hasDrawable(0xFE001));
    assertFalse(emojiFactory.hasDrawable(0xFE002));
  }

  public void testIsInEmojiPuaRange() {
    assertFalse(EmojiDrawableFactory.isInEmojiPuaRange(0));

    // Boundary check at min code point.
    assertFalse(EmojiDrawableFactory.isInEmojiPuaRange(
        EmojiDrawableFactory.MIN_EMOJI_PUA_CODE_POINT - 1));
    assertTrue(EmojiDrawableFactory.isInEmojiPuaRange(
        EmojiDrawableFactory.MIN_EMOJI_PUA_CODE_POINT));

    // Middle code point.
    assertTrue(EmojiDrawableFactory.isInEmojiPuaRange(
        (EmojiDrawableFactory.MIN_EMOJI_PUA_CODE_POINT
            + EmojiDrawableFactory.MAX_EMOJI_PUA_CODE_POINT) / 2));

    // Boundary check at max code point.
    assertTrue(EmojiDrawableFactory.isInEmojiPuaRange(
        EmojiDrawableFactory.MAX_EMOJI_PUA_CODE_POINT));
    assertFalse(EmojiDrawableFactory.isInEmojiPuaRange(
        EmojiDrawableFactory.MAX_EMOJI_PUA_CODE_POINT + 1));

    assertFalse(EmojiDrawableFactory.isInEmojiPuaRange(Integer.MAX_VALUE));
  }
}
