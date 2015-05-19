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

import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Row;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Span;

import java.util.ArrayList;
import java.util.List;
import java.util.ListIterator;

/**
 * Layouts the symbol candidate words.
 *
 * This is a table-like layout, which can be scrollable vertically.
 * The client can set the minimum column width and the row height.
 * The number of columns is calculated based on the given minimum column width
 * and the view's width. Each row should horizontally fit the view width.
 *
 */
public class SymbolCandidateLayouter implements CandidateLayouter {
  private final SpanFactory spanFactory;

  /** The minimum width for each column. */
  private float minColumnWidth;

  /** The number of rows in a page. */
  private int rowHeight;

  /** The current view's width. */
  private int viewWidth;

  public SymbolCandidateLayouter(SpanFactory spanFactory) {
    this.spanFactory = spanFactory;
  }

  public void setMinColumnWidth(float minColumnWidth) {
    this.minColumnWidth = minColumnWidth;
  }

  public void setRowHeight(int rowHeight) {
    this.rowHeight = rowHeight;
  }

  @Override
  public boolean setViewSize(int width, int height) {
    // Ignore the height.
    if (viewWidth == width) {
      return false;
    }

    viewWidth = width;
    return true;
  }

  @Override
  public int getPageWidth() {
    return viewWidth;
  }

  @Override
  public int getPageHeight() {
    return rowHeight;
  }

  @Override
  public CandidateLayout layout(CandidateList candidateList) {
    if (viewWidth <= 0 || rowHeight <= 0 || minColumnWidth <= 0 ||
        candidateList == null || candidateList.getCandidatesCount() == 0) {
      return null;
    }

    int numColumns = getNumColumns(viewWidth, minColumnWidth);
    List<Row> rowList = buildRowList(candidateList, spanFactory, numColumns);
    for (Row row : rowList) {
      layoutSpanList(row.getSpanList(), viewWidth, numColumns);
    }
    layoutRowList(rowList, rowHeight);
    return new CandidateLayout(rowList, viewWidth, rowHeight * rowList.size());
  }

  private static int getNumColumns(int viewWidth, float minColumnWidth) {
    // Ensure at least one column.
    return Math.max((int) (viewWidth / minColumnWidth), 1);
  }

  /** Builds a list of {@code Row}s from candidate word list. */
  static List<Row> buildRowList(
      CandidateList candidateList, SpanFactory spanFactory, int numColumns) {
    if (candidateList == null) {
      return null;
    }

    List<Row> rowList = new ArrayList<Row>(
        (candidateList.getCandidatesCount() + numColumns - 1) / numColumns);
    int columnIndex = 0;
    Row row = null;
    for (CandidateWord candidateWord : candidateList.getCandidatesList()) {
      if (columnIndex == 0) {
        row = new Row();
        rowList.add(row);
      }

      row.addSpan(spanFactory.newInstance(candidateWord));
      columnIndex = (columnIndex + 1) % numColumns;
    }
    return rowList;
  }

  /** Sets the left and right position to all spans. */
  static void layoutSpanList(List<Span> spanList, int viewWidth, int numColumns) {
    float left = 0;
    for (ListIterator<Span> iter = spanList.listIterator(); iter.hasNext(); ) {
      Span span = iter.next();
      // To avoid fp error at the end of the span list, we use the following expression.
      float right = viewWidth * iter.nextIndex() / (float) numColumns;
      span.setLeft(left);
      span.setRight(right);
      left = right;
    }
  }

  /** Sets the top, height and width to all rows. */
  static void layoutRowList(List<Row> rowList, int rowHeight) {
    // The pageHeight will be divided evenly to the each row.
    for (ListIterator<Row> iter = rowList.listIterator(); iter.hasNext(); ) {
      int index = iter.nextIndex();
      Row row = iter.next();
      row.setTop(rowHeight * index);
      row.setHeight(rowHeight);
      List<Span> spanList = row.getSpanList();
      row.setWidth(spanList.isEmpty() ? 0 : spanList.get(spanList.size() - 1).getRight());
    }
  }
}
