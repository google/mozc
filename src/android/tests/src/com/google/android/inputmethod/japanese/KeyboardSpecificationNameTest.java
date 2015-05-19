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

import org.mozc.android.inputmethod.japanese.testing.Parameter;

import android.content.res.Configuration;
import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.TestCase;

/**
 */
public class KeyboardSpecificationNameTest extends TestCase {
  @SmallTest
  public void testGetDeviceOrientationString() {
    class TestData extends Parameter {
      private final int orientation;
      private final String expectation;
      public TestData(int orientation, String expectation) {
        this.orientation = orientation;
        this.expectation = expectation;
      }
    }
    TestData[] testDataList = {
        new TestData(Configuration.ORIENTATION_LANDSCAPE, "LANDSCAPE"),
        new TestData(Configuration.ORIENTATION_PORTRAIT, "PORTRAIT"),
        new TestData(Configuration.ORIENTATION_SQUARE, "SQUARE"),
        new TestData(Configuration.ORIENTATION_UNDEFINED, "UNDEFINED"),
        new TestData(Integer.MAX_VALUE, "UNKNOWN"),
    };
    Configuration configuration = new Configuration();
    for (TestData testData : testDataList) {
      configuration.orientation = testData.orientation;
      assertEquals(testData.expectation,
                   KeyboardSpecificationName.getDeviceOrientationString(configuration));
    }
  }

  @SmallTest
  public void testFormatKeyboardName() {
    Configuration configuration = new Configuration();
    configuration.orientation = Configuration.ORIENTATION_PORTRAIT;
    assertEquals("TEST_KEYBOARD-1.2.3-PORTRAIT",
        new KeyboardSpecificationName("TEST_KEYBOARD", 1, 2, 3)
            .formattedKeyboardName(configuration));
  }
}
