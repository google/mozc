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

package org.mozc.android.inputmethod.japanese.ui;

import static android.test.MoreAsserts.assertEmpty;

import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.Annotation;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Span;

import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.TestCase;

import java.util.Arrays;

/**
 * Unit test for {@link SpanFactory}.
 *
 */
public class SpanFactoryTest extends TestCase {
  @SmallTest
  public void testMeasuredWidth() {
    SpanFactory spanFactory = new SpanFactory();
    spanFactory.setValueTextSize(10);
    spanFactory.setDescriptionTextSize(5);

    // Empty candidate.
    {
      Span span = spanFactory.newInstance(CandidateWord.getDefaultInstance());
      assertEquals(0f, span.getValueWidth());
      assertEquals(0f, span.getDescriptionWidth());
      assertEmpty(span.getSplitDescriptionList());
    }

    // Candidate with only value.
    CandidateWord valueCandidate = CandidateWord.newBuilder().setValue("This is candidate").build();
    Span valueSpan = spanFactory.newInstance(valueCandidate);
    assertTrue(valueSpan.getValueWidth() > 0);
    assertEquals(0f, valueSpan.getDescriptionWidth());

    // Candidate with only description.
    CandidateWord descriptionCandidate =
        CandidateWord.newBuilder().setAnnotation(Annotation.newBuilder().setDescription("desc"))
            .build();
    Span descriptionSpan = spanFactory.newInstance(descriptionCandidate);
    assertEquals(0f, descriptionSpan.getValueWidth());
    assertTrue(descriptionSpan.getDescriptionWidth() > 0);

    // Candidate with both value and description.
    CandidateWord mergedCandidate =
        valueCandidate.toBuilder().mergeFrom(descriptionCandidate).build();
    Span mergedSpan = spanFactory.newInstance(mergedCandidate);
    assertEquals(valueSpan.getValueWidth(), mergedSpan.getValueWidth());
    assertEquals(descriptionSpan.getDescriptionWidth(), mergedSpan.getDescriptionWidth());
  }

  @SmallTest
  public void testSplitDescriptionList() {
    class TestData extends Parameter {
      final String description;
      final String[] expectedResult;

      TestData(String description, String[] expectedResult) {
        super();
        this.description = description;
        this.expectedResult = expectedResult;
      }
    }
    TestData[] testDataList = {
        new TestData("", new String[]{}),
        new TestData("a", new String[]{"a"}),
        new TestData("a b", new String[]{"a", "b"}),
        new TestData("a b c d", new String[]{"a", "b", "c", "d"}),
        new TestData("a b c d e", new String[]{"a", "b", "c", "d", "e"}),
        // NG words
        new TestData("ひらがな", new String[]{}),
        new TestData("a ひらがな b", new String[]{"a", "b"}),
    };

    SpanFactory spanFactory = new SpanFactory();
    spanFactory.setDescriptionDelimiter(" ");
    for (TestData testData : testDataList) {
      CandidateWord candidateWord = CandidateWord.newBuilder()
          .setAnnotation(Annotation.newBuilder()
              .setDescription(testData.description))
          .build();
      assertEquals(testData.toString(),
                   Arrays.asList(testData.expectedResult),
                   spanFactory.newInstance(candidateWord).getSplitDescriptionList());
    }
  }
}
