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

package org.mozc.android.inputmethod.japanese.util;

import org.mozc.android.inputmethod.japanese.testing.Parameter;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import java.util.List;

/**
 * Teset for CandidateDescriptionUtil class.
 */
public class CandidateDescriptionUtilTest extends InstrumentationTestCase {

  @SmallTest
  public void testExtractDescriptions() {
    class TestData extends Parameter {
      final String input;
      final String[] expectedDescriptions;

      public TestData(String input, String[] expectedDescriptions) {
        this.input = Preconditions.checkNotNull(input);
        this.expectedDescriptions = Preconditions.checkNotNull(expectedDescriptions);
      }
    }

    TestData[] testDataArray = new TestData[] {
        new TestData("[全]", new String[] {"[全]"}),
        new TestData("[全] てすと", new String[] {"[全]", "てすと"}),
        new TestData("[全]\nてすと", new String[] {"[全]", "てすと"}),
        new TestData("丸数字", new String[] {}),
        new TestData("丸数字 [全]", new String[] {"[全]"}),
        new TestData("[全] 丸数字", new String[] {"[全]"}),
        new TestData("[全] 丸数字 てすと", new String[] {"[全]", "てすと"}),
        new TestData("小書き文字", new String[] {"小書き"}),
        new TestData("小書き文字 [全]", new String[] {"小書き", "[全]"}),
        new TestData("[全] 小書き文字", new String[] {"[全]", "小書き"}),
        new TestData("[全] 小書き文字 てすと", new String[] {"[全]", "小書き", "てすと"}),
        new TestData("亜の旧字体", new String[] {}),
        new TestData("亜の旧字体 [全]", new String[] {"[全]"}),
        new TestData("[全] 亜の旧字体", new String[] {"[全]"}),
        new TestData("[全] 亜の旧字体\n小書き文字 丸数字 てすと", new String[] {"[全]", "小書き", "てすと"}),
    };

    assertEquals(
        CandidateDescriptionUtil.extractDescriptions("", Optional.<String>absent()).size(), 0);
    assertEquals(CandidateDescriptionUtil.extractDescriptions("", Optional.of(" \n")).size(), 0);

    for (TestData testData : testDataArray) {
      List<String> descriptionsWithoutDelimiter =
          CandidateDescriptionUtil.extractDescriptions(testData.input, Optional.<String>absent());
      assertEquals(testData.input, 1, descriptionsWithoutDelimiter.size());
      assertEquals(testData.input, testData.input, descriptionsWithoutDelimiter.get(0));

      List<String> descriptionsWithDelimiter =
          CandidateDescriptionUtil.extractDescriptions(testData.input, Optional.of(" \n"));
      assertEquals(testData.input,
                   testData.expectedDescriptions.length, descriptionsWithDelimiter.size());
      for (int i = 0; i < testData.expectedDescriptions.length; ++i) {
        assertEquals(testData.input + " i=" + i,
                     testData.expectedDescriptions[i], descriptionsWithDelimiter.get(i));
      }
    }
  }
}
