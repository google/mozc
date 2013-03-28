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

package org.mozc.android.inputmethod.japanese.view;

import static org.easymock.EasyMock.expect;

import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;

import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.PictureDrawable;
import android.graphics.drawable.StateListDrawable;
import android.test.mock.MockResources;
import android.test.suitebuilder.annotation.SmallTest;

/**
 */
public class MozcDrawableFactoryTest extends InstrumentationTestCaseWithMock {
  @SmallTest
  public void testGetDrawableRaw() {
    Resources resources = getInstrumentation().getTargetContext().getResources();
    MozcDrawableFactory factory = new MozcDrawableFactory(resources);

    // Unfortunately it is difficult to check if the returned drawable renders as what we want,
    // because PictureDrawable renders in native code so we cannot intercept the Canvas,
    // and Pixel-wise diff can have some noise due to anti-aliasing.
    // So we made sure that we can parse the data from stream expectedly.

    // Symbol icon contains path and circle.
    assertTrue(
        factory.getDrawable(R.raw.twelvekeys__function__symbol__icon) instanceof PictureDrawable);
    // For polyline.
    assertTrue(
        factory.getDrawable(R.raw.twelvekeys__kana__support__12_left) instanceof PictureDrawable);
    // For polygon.
    assertTrue(
        factory.getDrawable(R.raw.twelvekeys__function__space__icon) instanceof PictureDrawable);
    // For rectangle.
    assertTrue(
        factory.getDrawable(R.raw.symbol__function__close) instanceof PictureDrawable);

    // For StateListDrawable.
    assertTrue(
        factory.getDrawable(R.raw.twelvekeys__kana__support__01_center)
            instanceof StateListDrawable);
  }

  @SmallTest
  public void testGetDrawableDrawable() {
    // Make sure that the resource whose type is "drawable" should be loaded by resources directly.
    Resources resources = createMock(MockResources.class);
    Drawable drawable = new ColorDrawable(Color.WHITE);
    expect(resources.getResourceTypeName(1)).andReturn("drawable");
    expect(resources.getDrawable(1)).andReturn(drawable);
    replayAll();

    MozcDrawableFactory factory = new MozcDrawableFactory(resources);
    assertSame(drawable, factory.getDrawable(1));

    verifyAll();
  }

  @SmallTest
  public void testEmojiDrawable() {
    // Try to parse all emoji Drawable.
    Resources resources = getInstrumentation().getTargetContext().getResources();
    MozcDrawableFactory factory = new MozcDrawableFactory(resources);

    for (String prefix : new String[] { "docomo_emoji_", "softbank_emoji_", "kddi_emoji_" }) {
      for (int codePoint = 0xFE000; codePoint <= 0xFEEA0; ++codePoint) {
        int resourceId = resources.getIdentifier(
            prefix + Integer.toHexString(codePoint),
            "raw", "org.mozc.android.inputmethod.japanese");
        if (resourceId != 0) {
          assertNotNull(factory.getDrawable(resourceId));
        }
      }
    }
  }
}
