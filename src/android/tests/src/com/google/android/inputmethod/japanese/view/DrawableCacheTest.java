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

import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.view.MozcDrawableFactory;

import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.test.mock.MockResources;
import android.test.suitebuilder.annotation.SmallTest;

/**
 */
public class DrawableCacheTest extends InstrumentationTestCaseWithMock {

  @SmallTest
  public void testDrawableCache() {
    Resources resources = createMock(MockResources.class);
    DrawableCache drawableCache = new DrawableCache(new MozcDrawableFactory(resources));

    // For invalid resource id (0), getDrawable returns null without looking up resources.
    replayAll();

    assertNull(drawableCache.getDrawable(0));

    verifyAll();

    // For first getDrawable, it loads from resources instance.
    Drawable drawable = new ColorDrawable(Color.BLACK);
    resetAll();
    expect(resources.getResourceTypeName(1)).andReturn("drawable");
    expect(resources.getDrawable(1)).andReturn(drawable);
    replayAll();

    assertSame(drawable, drawableCache.getDrawable(1));

    verifyAll();

    // For second getDrawable (or later), it returns cached instance.
    resetAll();
    replayAll();

    assertSame(drawable, drawableCache.getDrawable(1));

    verifyAll();

    // Once clear is invoked, the drawable will be loaded from resources again.
    drawableCache.clear();
    resetAll();
    expect(resources.getResourceTypeName(1)).andReturn("drawable");
    expect(resources.getDrawable(1)).andReturn(drawable);
    replayAll();

    assertSame(drawable, drawableCache.getDrawable(1));

    verifyAll();
  }

  @SmallTest
  public void testSetSkinType() {
    Resources resources = createMock(MockResources.class);
    DrawableCache drawableCache = new DrawableCache(new MozcDrawableFactory(resources));
    Drawable drawable = new ColorDrawable(Color.BLACK);
    expect(resources.getResourceTypeName(1)).andReturn("drawable");
    expect(resources.getDrawable(1)).andReturn(drawable);
    replayAll();

    assertSame(drawable, drawableCache.getDrawable(1));

    verifyAll();

    // If setSkinType is invoked but actually the skin is not changed,
    // cached drawables should be alive.
    resetAll();
    replayAll();

    drawableCache.setSkinType(SkinType.ORANGE_LIGHTGRAY);
    assertSame(drawable, drawableCache.getDrawable(1));

    verifyAll();

    // If setSkinType is invoked and actually the skin is changed,
    // cached drawables should be invalidated.
    resetAll();
    expect(resources.getResourceTypeName(1)).andReturn("drawable");
    expect(resources.getDrawable(1)).andReturn(drawable);
    replayAll();

    drawableCache.setSkinType(SkinType.TEST);
    assertSame(drawable, drawableCache.getDrawable(1));

    verifyAll();
  }
}
