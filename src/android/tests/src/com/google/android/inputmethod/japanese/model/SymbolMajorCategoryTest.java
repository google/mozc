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

package org.mozc.android.inputmethod.japanese.model;

import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;

import android.test.suitebuilder.annotation.SmallTest;

/**
 * Tests for SymbolMajorCategory.
 */
public class SymbolMajorCategoryTest extends InstrumentationTestCaseWithMock {

  @SmallTest
  public void testGetMajorCategory() {
    for (SymbolMinorCategory minorCategory : SymbolMinorCategory.values()) {
      if (minorCategory.name().startsWith("EMOTICON")) {
        assertEquals(SymbolMajorCategory.EMOTICON,
                     SymbolMajorCategory.findMajorCategory(minorCategory));
      } else if (minorCategory.name().startsWith("SYMBOL")) {
        assertEquals(SymbolMajorCategory.SYMBOL,
                     SymbolMajorCategory.findMajorCategory(minorCategory));
      } else if (minorCategory.name().startsWith("EMOJI")) {
        assertEquals(SymbolMajorCategory.EMOJI,
                     SymbolMajorCategory.findMajorCategory(minorCategory));
      } else if (minorCategory == SymbolMinorCategory.NUMBER) {
        assertEquals(SymbolMajorCategory.NUMBER,
                     SymbolMajorCategory.findMajorCategory(minorCategory));
      } else {
        fail("Unexpected category name");
      }
    }
  }

  @SmallTest
  public void testGetMinorCategoryByRelativeIndex() {
    assertEquals(
        SymbolMinorCategory.EMOJI_FACE,
        SymbolMajorCategory.EMOJI.getMinorCategoryByRelativeIndex(
            SymbolMinorCategory.EMOJI_HISTORY, +1));
    assertEquals(
        SymbolMinorCategory.EMOJI_HISTORY,
        SymbolMajorCategory.EMOJI.getMinorCategoryByRelativeIndex(
            SymbolMinorCategory.EMOJI_HISTORY, 0));
    assertEquals(
        SymbolMinorCategory.EMOJI_NATURE,
        SymbolMajorCategory.EMOJI.getMinorCategoryByRelativeIndex(
            SymbolMinorCategory.EMOJI_HISTORY, -1));
  }
}
