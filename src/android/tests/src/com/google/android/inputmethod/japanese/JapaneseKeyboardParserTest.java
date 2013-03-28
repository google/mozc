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

package org.mozc.android.inputmethod.japanese;

import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.resources.R;

import android.content.res.Resources;
import android.content.res.Resources.NotFoundException;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;

/**
 */
public class JapaneseKeyboardParserTest extends InstrumentationTestCase {
  @SmallTest
  public void testParseKeyboard()
      throws NotFoundException, XmlPullParserException, IOException {
    // The only diff from KeyboardParser, the base of this class, is
    // that the result type is JapaneseKeyboard, and it has a corresponding
    // specification.
    Resources resources = getInstrumentation().getTargetContext().getResources();
    JapaneseKeyboardParser parser = new JapaneseKeyboardParser(
        resources, resources.getXml(R.xml.kbd_12keys_kana),
        KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA, 480, 200);

    // ClassCastException shouldn't be raised.
    JapaneseKeyboard keyboard = parser.parseKeyboard();

    // The keyboard should have specified Specification.
    assertEquals(KeyboardSpecification.TWELVE_KEY_TOGGLE_KANA,
                 keyboard.getSpecification());
  }
}
