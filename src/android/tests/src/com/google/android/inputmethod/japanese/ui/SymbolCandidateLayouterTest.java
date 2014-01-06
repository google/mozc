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
import org.mozc.android.inputmethod.japanese.testing.MozcLayoutUtil;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Row;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Span;

import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.TestCase;

import java.util.ArrayList;
import java.util.List;

/**
 */
public class SymbolCandidateLayouterTest extends TestCase {
  // Dummy span factory.
  private static final SpanFactory DUMMY_SPAN_FACTORY = new SpanFactory();

  @SmallTest
  public void testSetViewSize() {
    SymbolCandidateLayouter layouter = new SymbolCandidateLayouter();
    layouter.setSpanFactory(DUMMY_SPAN_FACTORY);
    layouter.setViewSize(0, 0);

    // If and only if the width is changed, the layout should be invalidated.
    assertTrue(layouter.setViewSize(320, 240));
    assertFalse(layouter.setViewSize(320, 240));
    assertFalse(layouter.setViewSize(320, 480));
    assertTrue(layouter.setViewSize(160, 480));
  }

  public void testPageSize() {
    SymbolCandidateLayouter layouter = new SymbolCandidateLayouter();
    layouter.setSpanFactory(DUMMY_SPAN_FACTORY);
    layouter.setRowHeight(10);

    // The page width should be equal to the view width.
    // The page height should be equal to the given row height.
    layouter.setViewSize(320, 240);
    assertEquals(320, layouter.getPageWidth());
    assertEquals(10, layouter.getPageHeight());

    layouter.setViewSize(800, 640);
    assertEquals(800, layouter.getPageWidth());
    assertEquals(10, layouter.getPageHeight());
  }

  @SmallTest
  public void testBuildRowList() {
    // Create ten candidate words numbered 0 to 9.
    CandidateList.Builder builder = CandidateList.newBuilder();
    for (int i = 0; i < 10; ++i) {
      builder.addCandidates(CandidateWord.newBuilder().setId(i));
    }
    CandidateList candidateList = builder.build();

    class TestData extends Parameter {
      final int numColumns;
      final int[][] expectedIdTable;
      TestData(int numColumns, int[][] expectedIdTable) {
        this.numColumns = numColumns;
        this.expectedIdTable = expectedIdTable;
      }
    }

    TestData[] testDataList = {
        // Layout for 2 columns. The result should be
        // [row 0] 0 1
        // [row 1] 2 3
        // [row 2] 4 5
        // [row 3] 6 7
        // [row 4] 8 9
        new TestData(
            2,
            new int[][] {
                {0, 1},
                {2, 3},
                {4, 5},
                {6, 7},
                {8, 9},
            }),

        // Layout for 3 columns. The result should be
        // [row 0] 0 1 2
        // [row 1] 3 4 5
        // [row 2] 6 7 8
        // [row 3] 9
        new TestData(
            3,
            new int[][] {
                {0, 1, 2},
                {3, 4, 5},
                {6, 7, 8},
                {9},
            }),

        // Layout for 4 columns. The result should be
        // [row 0] 0 1 2 3
        // [row 1] 4 5 6 7
        // [row 2] 8 9
        new TestData(
            4,
            new int[][] {
                {0, 1, 2, 3},
                {4, 5, 6, 7},
                {8, 9},
            }),
    };

    for (TestData testData : testDataList) {
      List<Row> rowList = SymbolCandidateLayouter.buildRowList(
          candidateList, DUMMY_SPAN_FACTORY, testData.numColumns);

      // Make sure that the candidates are layouted as expected.
      int[][] expectedIdTable = testData.expectedIdTable;
      assertEquals(expectedIdTable.length, rowList.size());
      for (int i = 0; i < expectedIdTable.length; ++i) {
        Row row = rowList.get(i);
        int[] expectedIdRow = expectedIdTable[i];
        assertEquals(expectedIdRow.length, row.getSpanList().size());
        for (int j = 0; j < expectedIdRow.length; ++j) {
          assertEquals(expectedIdRow[j], row.getSpanList().get(j).getCandidateWord().getId());
        }
      }
    }
  }

  @SmallTest
  public void testLayoutSpanList() {
    List<Span> spanList = new ArrayList<Span>();
    for (int i = 0; i < 6; ++i) {
      spanList.add(MozcLayoutUtil.createSpan(i, 0, 0));
    }

    SymbolCandidateLayouter.layoutSpanList(spanList, 240, 6);
    {
      float expectedXCoord[] = {0, 40, 80, 120, 160, 200, 240};
      for (int i = 0; i < spanList.size(); ++i) {
        assertEquals(expectedXCoord[i], spanList.get(i).getLeft());
        assertEquals(expectedXCoord[i + 1], spanList.get(i).getRight());
      }
    }

    SymbolCandidateLayouter.layoutSpanList(spanList, 240, 8);
    {
      float expectedXCoord[] = {0, 30, 60, 90, 120, 150, 180};  // Not reached to the end.
      for (int i = 0; i < spanList.size(); ++i) {
        assertEquals(expectedXCoord[i], spanList.get(i).getLeft());
        assertEquals(expectedXCoord[i + 1], spanList.get(i).getRight());
      }
    }
  }

  @SmallTest
  public void testLayoutRowList() {
    List<Row> rowList = new ArrayList<Row>();
    for (int i = 0; i < 4; ++i) {
      Row row = new Row();
      row.addSpan(MozcLayoutUtil.createSpan(i, 0, 240));
      rowList.add(row);
    }

    SymbolCandidateLayouter.layoutRowList(rowList, 80);

    float expectedYCoord[] = {0, 80, 160, 240};
    for (int i = 0; i < rowList.size(); ++i) {
      assertEquals(expectedYCoord[i], rowList.get(i).getTop());
      assertEquals(240f, rowList.get(i).getWidth());
      assertEquals(80f, rowList.get(i).getHeight());
    }
  }
}
