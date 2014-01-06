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

import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;

import junit.framework.TestCase;

/**
 */
public class EmojiAnimationDrawableTest extends TestCase {
  public void testTotalDuration() {
    Drawable dummy = new ColorDrawable();
    EmojiAnimationDrawable drawable = new EmojiAnimationDrawable();
    drawable.addFrame(dummy, 10);
    assertEquals(10, drawable.getTotalDuration());
    drawable.addFrame(dummy, 10);
    assertEquals(20, drawable.getTotalDuration());
    drawable.addFrame(dummy, 100);
    assertEquals(120, drawable.getTotalDuration());
  }

  public void testSelectDrawableByTime() {
    Drawable dummy1 = new ColorDrawable();
    Drawable dummy2 = new ColorDrawable();
    Drawable dummy3 = new ColorDrawable();
    Drawable dummy4 = new ColorDrawable();
    Drawable dummy5 = new ColorDrawable();

    EmojiAnimationDrawable drawable = new EmojiAnimationDrawable();
    drawable.addFrame(dummy1, 100);
    drawable.addFrame(dummy2, 100);
    drawable.addFrame(dummy3, 100);
    drawable.addFrame(dummy4, 100);
    drawable.addFrame(dummy5, 100);

    // The returned value depends on the API Level.
    // According to the framework's sourcecode, Level 10 (Gingerbread) returns true
    // and Level 14 (ICS) returns false.
    // This difference comes from the change of AnimationDrawable#addFrame().
    // Newer framework invokes AnimationDrawable#setFrame() from addFrame
    // so valid "current frame" exists after addFrame.
    // On the other hand older frameworks do not such things so no valid "current frame".
    drawable.selectDrawableByTime(0);
    assertSame(dummy1, drawable.getCurrent());
    assertFalse(drawable.selectDrawableByTime(99));
    assertSame(dummy1, drawable.getCurrent());
    assertTrue(drawable.selectDrawableByTime(100));
    assertSame(dummy2, drawable.getCurrent());
    assertFalse(drawable.selectDrawableByTime(199));
    assertSame(dummy2, drawable.getCurrent());
    assertTrue(drawable.selectDrawableByTime(200));
    assertSame(dummy3, drawable.getCurrent());
    assertFalse(drawable.selectDrawableByTime(299));
    assertSame(dummy3, drawable.getCurrent());
    assertTrue(drawable.selectDrawableByTime(300));
    assertSame(dummy4, drawable.getCurrent());
    assertFalse(drawable.selectDrawableByTime(399));
    assertSame(dummy4, drawable.getCurrent());
    assertTrue(drawable.selectDrawableByTime(400));
    assertSame(dummy5, drawable.getCurrent());
    assertFalse(drawable.selectDrawableByTime(499));
    assertSame(dummy5, drawable.getCurrent());
    assertTrue(drawable.selectDrawableByTime(500));
    assertSame(dummy1, drawable.getCurrent());
  }

  public void testGetNextFrameTiming() {
    Drawable dummy1 = new ColorDrawable();
    Drawable dummy2 = new ColorDrawable();
    Drawable dummy3 = new ColorDrawable();
    Drawable dummy4 = new ColorDrawable();
    Drawable dummy5 = new ColorDrawable();

    EmojiAnimationDrawable drawable = new EmojiAnimationDrawable();
    drawable.addFrame(dummy1, 100);
    drawable.addFrame(dummy2, 100);
    drawable.addFrame(dummy3, 100);
    drawable.addFrame(dummy4, 100);
    drawable.addFrame(dummy5, 100);

    assertEquals(100, drawable.getNextFrameTiming(0));
    assertEquals(100, drawable.getNextFrameTiming(99));
    assertEquals(200, drawable.getNextFrameTiming(100));
    assertEquals(200, drawable.getNextFrameTiming(199));
    assertEquals(300, drawable.getNextFrameTiming(200));
    assertEquals(300, drawable.getNextFrameTiming(299));
    assertEquals(400, drawable.getNextFrameTiming(300));
    assertEquals(400, drawable.getNextFrameTiming(399));
    assertEquals(500, drawable.getNextFrameTiming(400));
    assertEquals(500, drawable.getNextFrameTiming(499));
    assertEquals(600, drawable.getNextFrameTiming(500));
    assertEquals(600, drawable.getNextFrameTiming(599));
    assertEquals(700, drawable.getNextFrameTiming(600));
    assertEquals(700, drawable.getNextFrameTiming(699));
    assertEquals(800, drawable.getNextFrameTiming(700));
    assertEquals(800, drawable.getNextFrameTiming(799));
    assertEquals(900, drawable.getNextFrameTiming(800));
    assertEquals(900, drawable.getNextFrameTiming(899));
    assertEquals(1000, drawable.getNextFrameTiming(900));
    assertEquals(1000, drawable.getNextFrameTiming(999));
  }
}
