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

package org.mozc.android.inputmethod.japanese.ui;

import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Row;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Span;
import org.mozc.android.inputmethod.japanese.ui.ConversionCandidateLayouter.ChunkMetrics;

import junit.framework.TestCase;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 */
public class ConversionCandidateLayouterTest extends TestCase {
  private static final int VALUE_HEIGHT = 30;
  private static final int VALUE_VERTICAL_PADDING = 10;

  private static SpanFactory DUMMY_SPAN_FACTORY = new SpanFactory();
  private static ChunkMetrics DUMMY_CHUNK_METRICS = new ChunkMetrics(0, 0, 0, 0) {
    @Override
    int getNumChunks(Span span) {
      // Inject the number of consuming chunks by value text for testing.
      return Integer.parseInt(span.getCandidateWord().getValue());
    }
  };

  public void testSetViewSize() {
    ConversionCandidateLayouter layouter = new ConversionCandidateLayouter();
    layouter.setSpanFactory(DUMMY_SPAN_FACTORY);
    layouter.setViewSize(0, 0);

    assertTrue(layouter.setViewSize(320, 240));
    assertFalse(layouter.setViewSize(320, 240));
    assertFalse(layouter.setViewSize(320, 480));
    assertTrue(layouter.setViewSize(160, 480));
  }

  public void testPageSize() {
    ConversionCandidateLayouter layouter = new ConversionCandidateLayouter();
    layouter.setSpanFactory(DUMMY_SPAN_FACTORY);
    layouter.setValueHeight(VALUE_HEIGHT);
    layouter.setValueVerticalPadding(VALUE_VERTICAL_PADDING);
    layouter.setViewSize(320, 640);

    // The page width is equal to the view's width;
    assertEquals(320, layouter.getPageWidth());

    // The page height is view's height equals
    // valueHeight + verticalPadding * 2 (top and bottom). (= 50 in this case)
    assertEquals(VALUE_HEIGHT + VALUE_VERTICAL_PADDING * 2, layouter.getPageHeight());

    // Test for another view size.
    layouter.setViewSize(1280, 768);
    assertEquals(1280, layouter.getPageWidth());
    assertEquals(VALUE_HEIGHT + VALUE_VERTICAL_PADDING * 2, layouter.getPageHeight());
  }

  public void testBuildRowList() {
    class TestData extends Parameter {
      final String[] valueList;
      final int numChunks;
      final boolean enableSpan;
      final int[] expectedNumSpansList;

      TestData(String[] valueList, int numChunks, boolean enableSpan, int[] expectedNumSpansList) {
        this.valueList = valueList;
        this.numChunks = numChunks;
        this.enableSpan = enableSpan;
        this.expectedNumSpansList = expectedNumSpansList;
      }
    }

    TestData[] testDataList = {
        // Disable span for input fold button.
        // Usual cases.
        new TestData(new String[] {"5", "5"}, 10, false, new int[] {2}),
        new TestData(new String[] {"4", "5"}, 10, false, new int[] {2}),
        new TestData(new String[] {"4", "4", "4"}, 10, false, new int[] {2, 1}),
        new TestData(new String[] {"4", "4", "4"}, 1, false, new int[] {1, 1, 1}),

        // Too large span must occupy a row exclusively.
        new TestData(new String[] {"1", "11", "1"}, 10, false, new int[] {1, 1, 1}),
        new TestData(new String[] {"5", "6", "1"}, 10, false, new int[] {1, 2}),

        // The first candidate is very long.
        new TestData(new String[] {"11", "10", "10"}, 10, false, new int[] {1, 1, 1}),

        // Enable span for input fold button.
        // Usual cases.
        new TestData(new String[] {"5", "5"}, 10, true, new int[] {1, 1}),
        new TestData(new String[] {"4", "5"}, 10, true, new int[] {2}),
        new TestData(new String[] {"4", "4", "4"}, 10, true, new int[] {2, 1}),
        new TestData(new String[] {"4", "4", "4"}, 1, true, new int[] {1, 1, 1}),

        // Too large span must occupy a row exclusively.
        new TestData(new String[] {"1", "11", "1"}, 10, true, new int[] {1, 1, 1}),
        new TestData(new String[] {"5", "6", "1"}, 10, true, new int[] {1, 2}),

        // The first candidate is very long.
        new TestData(new String[] {"11", "10", "10"}, 10, true, new int[] {1, 1, 1}),
    };

    for (TestData testData : testDataList) {
      // Build a candidate list.
      CandidateList.Builder builder = CandidateList.newBuilder();
      {
        int index = 0;
        for (String value : testData.valueList) {
          builder.addCandidates(CandidateWord.newBuilder()
              .setId(index++)
              .setValue(value));
        }
      }
      CandidateList candidateList = builder.build();

      List<Row> rowList = ConversionCandidateLayouter.buildRowList(
          candidateList, DUMMY_SPAN_FACTORY, testData.numChunks, DUMMY_CHUNK_METRICS,
          testData.enableSpan);

      // Make sure each row has the expected number of spans.
      assertEquals(testData.toString(), testData.expectedNumSpansList.length, rowList.size());
      for (int i = 0; i < rowList.size(); ++i) {
        assertEquals(
            testData.toString(),
            testData.expectedNumSpansList[i], rowList.get(i).getSpanList().size());
      }

      // Make sure the candidates' order is kept.
      {
        int index = 0;
        for (Row row : rowList) {
          for (Span span : row.getSpanList()) {
            assertEquals(index++, span.getCandidateWord().getId());
          }
        }
      }
    }
  }

  private static Span createSpan(String value) {
    return new Span(CandidateWord.newBuilder().setValue(value).build(), 0, 0,
                    Collections.<String>emptyList());
  }

  public void testLayoutSpanList_simple() {
    List<Span> spanList = new ArrayList<Span>();
    for (int i = 0; i < 5; ++i) {
      spanList.add(createSpan("2"));
    }

    ConversionCandidateLayouter.layoutSpanList(spanList, 100, 10, DUMMY_CHUNK_METRICS, new int[10]);

    float[] expectedXCoord = {0, 20, 40, 60, 80, 100};
    for (int i = 0; i < spanList.size(); ++i) {
      assertEquals(expectedXCoord[i], spanList.get(i).getLeft());
      assertEquals(expectedXCoord[i + 1], spanList.get(i).getRight());
    }
  }

  public void testLayoutSpanList_underflow() {
    List<Span> spanList = new ArrayList<Span>();
    for (int i = 0; i < 5; ++i) {
      spanList.add(createSpan("2"));
    }

    ConversionCandidateLayouter.layoutSpanList(spanList, 180, 18, DUMMY_CHUNK_METRICS, new int[18]);

    // The remaining chunks will be assigned to the spans.
    float[] expectedXCoord = {0, 30, 60, 100, 140, 180};
    for (int i = 0; i < spanList.size(); ++i) {
      assertEquals(expectedXCoord[i], spanList.get(i).getLeft());
      assertEquals(expectedXCoord[i + 1], spanList.get(i).getRight());
    }
  }

  public void testLayoutSpanList_overflow() {
    List<Span> spanList = new ArrayList<Span>();
    spanList.add(createSpan("20"));

    ConversionCandidateLayouter.layoutSpanList(spanList, 100, 10, DUMMY_CHUNK_METRICS, new int[10]);

    assertEquals(0f, spanList.get(0).getLeft());
    assertEquals(100f, spanList.get(0).getRight());
  }

  public void testLayoutRow() {
    List<Row> rowList = new ArrayList<Row>();
    for (int i = 0; i < 5; ++i) {
      rowList.add(new Row());
    }

    ConversionCandidateLayouter.layoutRowList(rowList, 100, 30);
    for (int i = 0; i < rowList.size(); ++i) {
      Row row = rowList.get(i);
      assertEquals(i * 30f, row.getTop());
      assertEquals(100f, row.getWidth());
      assertEquals(30f, row.getHeight());
    }
  }
}
