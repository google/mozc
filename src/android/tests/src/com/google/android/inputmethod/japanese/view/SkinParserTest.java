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

package org.mozc.android.inputmethod.japanese.view;

import org.mozc.android.inputmethod.japanese.view.SkinParser.SkinParserException;

import android.content.res.Resources;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

/**
 * Test for {@code SkinParser}
 */
public class SkinParserTest extends InstrumentationTestCase {

  @SmallTest
  public void testParser_correctSkin() throws SkinParserException {
    Resources resources = getInstrumentation().getContext().getResources();
    int[] skinIds = {
        org.mozc.android.inputmethod.japanese.tests.R.xml.skinparser_correct_drawable_test_1};
    for (int id : skinIds) {
      SkinParser parser = new SkinParser(resources, resources.getXml(id));
      Skin skin = parser.parseSkin();
      assertTrue(skin.windowBackgroundDrawable != DummyDrawable.getInstance());
    }
  }

  @SmallTest
  public void testParser_incorrectSkin() {
    // Note: Use getContext() to get test packge's resources.
    Resources resources = getInstrumentation().getContext().getResources();
    int[] skinIds = {org.mozc.android.inputmethod.japanese.tests.R.xml.skinparser_test_1,
                      org.mozc.android.inputmethod.japanese.tests.R.xml.skinparser_test_2,
                      org.mozc.android.inputmethod.japanese.tests.R.xml.skinparser_test_3,
                      org.mozc.android.inputmethod.japanese.tests.R.xml.skinparser_test_4,
                      org.mozc.android.inputmethod.japanese.tests.R.xml.skinparser_test_5,
                      org.mozc.android.inputmethod.japanese.tests.R.xml.skinparser_test_6,
                      org.mozc.android.inputmethod.japanese.tests.R.xml.skinparser_test_7};
    for (int id : skinIds) {
      try {
        SkinParser parser = new SkinParser(resources, resources.getXml(id));
        parser.parseSkin();
      } catch (SkinParserException e) {
        // Expected.
        continue;
      }
      fail("SkinParserException must be thrown at ID 0x" + Integer.toString(id, 16));
    }
  }

  @SmallTest
  public void testDrawableParser_incorrectSkin() {
    Resources resources = getInstrumentation().getContext().getResources();
    int[] skinIds = {
        org.mozc.android.inputmethod.japanese.tests.R.xml.skinparser_incorrect_drawable_test_1};
    for (int id : skinIds) {
      try {
        SkinParser parser = new SkinParser(resources, resources.getXml(id));
        parser.parseSkin();
      } catch (SkinParserException e) {
        // Expected.
        continue;
      }
      fail("SkinParserException must be thrown at ID 0x" + Integer.toString(id, 16));
    }
  }
}
