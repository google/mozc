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

package org.mozc.android.inputmethod.japanese;

import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;

import android.content.res.Resources;
import android.graphics.Rect;
import android.test.InstrumentationTestCase;

/**
 */
public class JapaneseKeyboardFactoryTest extends InstrumentationTestCase {

  public void testGet() {
    Resources resources = getInstrumentation().getTargetContext().getResources();
    // With portrait/landscape keyboard size for HVGA display;
    Rect portrait = new Rect(0, 0, 320, 480);
    Rect landscape = new Rect(0, 0, 480, 320);

    JapaneseKeyboardFactory factory = new JapaneseKeyboardFactory();

    // Tests for portrait ones.
    JapaneseKeyboard portraitTwelveKeyKeyboard =
        factory.get(resources, KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA, portrait.width(),
                    portrait.height());
    assertNotNull(portraitTwelveKeyKeyboard);

    JapaneseKeyboard portraitQwertyKeyboard =
        factory.get(resources, KeyboardSpecification.QWERTY_KANA, portrait.width(),
                    portrait.height());
    assertNotNull(portraitQwertyKeyboard);

    // Those instances should be different.
    assertNotSame(portraitTwelveKeyKeyboard, portraitQwertyKeyboard);

    // The same instance should be returned for same resource and specification.
    assertSame(portraitTwelveKeyKeyboard,
               factory.get(resources, KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA,
                           portrait.width(), portrait.height()));
    assertSame(portraitQwertyKeyboard,
               factory.get(resources, KeyboardSpecification.QWERTY_KANA, portrait.width(),
                           portrait.height()));

    // Then tests for landscape ones. The keyboards for landscape should be different from portrait
    // ones.
    JapaneseKeyboard landscapeTwelveKeyKeyboard =
        factory.get(resources, KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA, landscape.width(),
                    landscape.height());
    assertNotNull(landscapeTwelveKeyKeyboard);
    assertNotSame(portraitTwelveKeyKeyboard, landscapeTwelveKeyKeyboard);

    JapaneseKeyboard landscapeQwertyKeyboard =
        factory.get(resources, KeyboardSpecification.QWERTY_KANA, landscape.width(),
                    landscape.height());
    assertNotNull(landscapeQwertyKeyboard);
    assertNotSame(portraitQwertyKeyboard, landscapeQwertyKeyboard);

    // Those instances should be different.
    assertNotSame(landscapeTwelveKeyKeyboard, landscapeQwertyKeyboard);

    // The same instance should be returned for same resource and specification.
    assertSame(landscapeTwelveKeyKeyboard,
               factory.get(resources, KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA, 
                           landscape.width(), landscape.height()));
    assertSame(landscapeQwertyKeyboard,
               factory.get(resources, KeyboardSpecification.QWERTY_KANA, landscape.width(),
                           landscape.height()));

    // Assuming the cache is large enough, the same instance should be used for portrait even after
    // we load ones for landscape.
    assertSame(portraitTwelveKeyKeyboard,
               factory.get(resources, KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA,
                           portrait.width(), portrait.height()));
    assertSame(portraitQwertyKeyboard,
               factory.get(resources, KeyboardSpecification.QWERTY_KANA, portrait.width(),
                           portrait.height()));
  }
}
