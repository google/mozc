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

import android.app.Instrumentation;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.content.res.Resources.NotFoundException;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 */
public class JapaneseKeyboardTest extends InstrumentationTestCase {
  public static JapaneseKeyboard createJapaneseKeyboard(KeyboardSpecification spec,
                                                        Instrumentation instrumentation) {
    Resources resources = instrumentation.getTargetContext().getResources();
    JapaneseKeyboardParser parser = new JapaneseKeyboardParser(
        resources, resources.getXml(spec.getXmlLayoutResourceId()), spec, 480, 200);
    try {
      return parser.parseKeyboard();
    } catch (NotFoundException e) {
      fail(e.getMessage());
    } catch (XmlPullParserException e) {
      fail(e.getMessage());
    } catch (IOException e) {
      fail(e.getMessage());
    }

    throw new AssertionError("Should never reach here");
  }

  @SmallTest
  public void testConstructor() {
    for (KeyboardSpecification specification : KeyboardSpecification.values()) {
      // when ResourceId is 0, it means this specification has no Keyboard
      if (specification.getXmlLayoutResourceId() == 0) {
        continue;
      }
      // Make sure that all specification has corresponding JapaneseKeyboard instance,
      // and it has the given specification.
      JapaneseKeyboard keyboard = createJapaneseKeyboard(specification, getInstrumentation());
      assertEquals(specification.toString(), specification, keyboard.getSpecification());
    }
  }

  @SmallTest
  public void testGetKeyboardName() {
    Pattern pattern = Pattern.compile("^(\\w+)-(\\d+)\\.(\\d+)\\.(\\d+)-(\\w+)$");
    Configuration configuration = new Configuration();
    for (KeyboardSpecification specification : KeyboardSpecification.values()) {
      for (int orientation : new int[] {Configuration.ORIENTATION_PORTRAIT,
                                        Configuration.ORIENTATION_LANDSCAPE}) {
        configuration.orientation = orientation;
        KeyboardSpecificationName keyboardSpecificationName =
            specification.getKeyboardSpecificationName();
        String name = keyboardSpecificationName.formattedKeyboardName(configuration);
        Matcher matcher = pattern.matcher(name);
        assertTrue(matcher.matches());
        assertEquals(keyboardSpecificationName.baseName, matcher.group(1));
        assertEquals(Integer.toString(keyboardSpecificationName.major), matcher.group(2));
        assertEquals(Integer.toString(keyboardSpecificationName.minor), matcher.group(3));
        assertEquals(Integer.toString(keyboardSpecificationName.revision), matcher.group(4));
        assertEquals(KeyboardSpecificationName.getDeviceOrientationString(configuration),
                     matcher.group(5));
      }
    }
  }
}
