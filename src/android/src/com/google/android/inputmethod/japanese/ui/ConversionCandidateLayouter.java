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

import android.util.FloatMath;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Layouts the conversion candidate words.
 *
 * First, all the rows this layouter creates are split into "chunk"s.
 * The width of each chunk is equal to {@code pageWidth / numChunks} evenly.
 * Next, the candidates are assigned to chunks. The order of the candidates is kept.
 * A candidate may occupy one or more successive chunks which are on the same row.
 *
 * The height of each row is round up to integer, so that the snap-paging
 * should work well.
 *
 */
public class ConversionCandidateLayouter implements CandidateLayouter {

  /**
   * The metrics between chunk and span.
   *
   * The main purpose of this class is to inject the chunk compression
   * heuristics for testing.
   */
  static class ChunkMetrics {
    private final float chunkWidth;
    private final float compressionRatio;
    private final float horizontalPadding;
    private final float minWidth;

    ChunkMetrics(float chunkWidth,
                 float compressionRatio,
                 float horizontalPadding,
                 float minWidth) {
      this.chunkWidth = chunkWidth;
      this.compressionRatio = compressionRatio;
      this.horizontalPadding = horizontalPadding;
      this.minWidth = minWidth;
    }

    /** Returns the number of chunks which the span would consume. */
    int getNumChunks(Span span) {
      float compressedValueWidth =
          compressValueWidth(span.getValueWidth(), compressionRatio, horizontalPadding, minWidth);
      return (int) FloatMath.ceil((compressedValueWidth + span.getDescriptionWidth()) / chunkWidth);
    }

    static float compressValueWidth(
        float valueWidth, float compressionRatio, float horizontalPadding, float minWidth) {
      // Sum of geometric progression.
      // a == 1.0 (default pixel size)
      // r == candidateWidthCompressionRate (pixel width decay rate)
      // n == defaultWidth
      if (compressionRatio != 1) {
        valueWidth =
            (1f - (float) Math.pow(compressionRatio, valueWidth)) / (1f - compressionRatio);
      }
      return Math.max(valueWidth + horizontalPadding * 2, minWidth);
    }
  }

  private final SpanFactory spanFactory;

  /** Horizontal common ratio of the value size. */
  private final float valueWidthCompressionRate;

  /** Minimum width of the value. */
  private final float minValueWidth;

  /** The Minimum width of the chunk. */
  private final float minChunkWidth;

  /** Height of the value. */
  private final float valueHeight;

  private final float valueHorizontalPadding;
  private final float valueVerticalPadding;

  /** The current view's width. */
  private int viewWidth;

  private boolean reserveEmptySpan = false;

  public ConversionCandidateLayouter(
      SpanFactory spanFactory,
      float valueWidthCompressionRate,
      float minValueWidth,
      float minChunkWidth,
      float valueHeight,
      float valueHorizontalPadding,
      float valueVerticalPadding) {
    this.spanFactory = spanFactory;
    this.valueWidthCompressionRate = valueWidthCompressionRate;
    this.minValueWidth = minValueWidth;
    this.minChunkWidth = minChunkWidth;
    this.valueHeight = valueHeight;
    this.valueHorizontalPadding = valueHorizontalPadding;
    this.valueVerticalPadding = valueVerticalPadding;
  }

  @Override
  public boolean setViewSize(int width, int height) {
    if (viewWidth == width) {
      // Doesn't need to invalidate the layout if the width isn't changed.
      return false;
    }
    viewWidth = width;
    return true;
  }

  private int getNumChunks() {
    return (int) (viewWidth / minChunkWidth);
  }

  public float getChunkWidth() {
    return viewWidth / (float) getNumChunks();
  }

  @Override
  public int getPageWidth() {
    return Math.max(viewWidth, 0);
  }

  public int getRowHeight() {
    return (int) FloatMath.ceil(valueHeight + valueVerticalPadding * 2);
  }

  @Override
  public int getPageHeight() {
    return getRowHeight();
  }

  @Override
  public CandidateLayout layout(CandidateList candidateList) {
    if (minChunkWidth <= 0 || viewWidth <= 0 ||
        candidateList == null || candidateList.getCandidatesCount() == 0) {
      return null;
    }

    int numChunks = getNumChunks();
    float chunkWidth = getChunkWidth();
    ChunkMetrics chunkMetrics = new ChunkMetrics(
        chunkWidth, valueWidthCompressionRate, valueHorizontalPadding, minValueWidth);
    List<Row> rowList = buildRowList(candidateList, spanFactory, numChunks, chunkMetrics,
                                     reserveEmptySpan);
    int[] numAllocatedChunks = new int[numChunks];
    boolean isFirst = reserveEmptySpan;
    for (Row row : rowList) {
      layoutSpanList(
          row.getSpanList(),
          (isFirst ? (viewWidth - (int) chunkWidth) : viewWidth),
          (isFirst ? numChunks - 1 : numChunks),
          chunkMetrics, numAllocatedChunks);
      isFirst = false;
    }

    // Push empty span at the end of the first row.
    if (reserveEmptySpan) {
      Span emptySpan = new Span(null, 0, 0, Collections.<String>emptyList());
      List<Span> spanList = rowList.get(0).getSpanList();
      emptySpan.setLeft(spanList.get(spanList.size() - 1).getRight());
      emptySpan.setRight(viewWidth);
      rowList.get(0).addSpan(emptySpan);
    }

    // In order to snap the scrolling on any row boundary, rounding up the rowHeight
    // to align pixels.
    int rowHeight = getRowHeight();
    layoutRowList(rowList, viewWidth, rowHeight);

    return new CandidateLayout(rowList, viewWidth, rowHeight * rowList.size());
  }

  /**
   * Builds the row list based on the number of estimated chunks for each span.
   *
   * The order of the candidates will be kept.
   */
  static List<Row> buildRowList(
      CandidateList candidateList, SpanFactory spanFactory,
      int numChunks, ChunkMetrics chunkMetrics, boolean enableSpan) {
    List<Row> rowList = new ArrayList<Row>();

    int numRemainingChunks = 0;
    for (CandidateWord candidateWord : candidateList.getCandidatesList()) {
      Span span = spanFactory.newInstance(candidateWord);
      int numSpanChunks = chunkMetrics.getNumChunks(span);
      if (numRemainingChunks < numSpanChunks) {
        // There is no space on the current row to put the current span.
        // Create a new row.
        numRemainingChunks = numChunks;

        // For the first line, we reserve a chunk at right-top in order to place an icon
        // button for folding/expanding keyboard.
        if (enableSpan && rowList.isEmpty()) {
          numRemainingChunks--;
        }

        rowList.add(new Row());
      }

      // Add the span to the last row.
      rowList.get(rowList.size() - 1).addSpan(span);
      numRemainingChunks -= numSpanChunks;
    }

    return rowList;
  }

  /**
   * Sets left and right of each span. The left and right should be aligned to the chunks.
   * Also, the right of the last span should be equal to {@code pageWidth}.
   *
   * In order to avoid integer array memory allocation (as this method will be invoked
   * many times to layout a {@link CandidateList}), it is necessary to pass an integer
   * array for the calculation buffer, {@code numAllocatedChunks}.
   * The size of the buffer must be equal to or greater than {@code spanList.size()}.
   * Its elements needn't be initialized.
   */
  static void layoutSpanList(
      List<Span> spanList, int pageWidth,
      int numChunks, ChunkMetrics chunkMetrics, int[] numAllocatedChunks) {
    int numRemainingChunks = numChunks;
    // First, allocate the chunks based on the metrics.
    {
      int index = 0;
      for (Span span : spanList) {
        int numSpanChunks = Math.min(numRemainingChunks, chunkMetrics.getNumChunks(span));
        numAllocatedChunks[index] = numSpanChunks;
        numRemainingChunks -= numSpanChunks;
        ++index;
      }
    }

    // Then assign remaining chunks to each span as even as possible by round-robin
    // from tail to head to keep the backward compatibility.
    for (int index = spanList.size() - 1; numRemainingChunks > 0;
         --numRemainingChunks, index = (index + spanList.size() - 1) % spanList.size()) {
      ++numAllocatedChunks[index];
    }

    // Set the actual left and right to each span.
    {
      int index = 0;
      float left = 0;
      float spanWidth = pageWidth / (float) numChunks;
      int cumulativeNumAllocatedChunks = 0;
      for (Span span : spanList) {
        cumulativeNumAllocatedChunks += numAllocatedChunks[index++];
        float right = Math.min(spanWidth * cumulativeNumAllocatedChunks, pageWidth);
        span.setLeft(left);
        span.setRight(right);
        left = right;
      }
    }

    // Set the right of the last element to the pageWidth to align the page.
    spanList.get(spanList.size() - 1).setRight(pageWidth);
  }

  /** Sets top, width and height to the each row. */
  static void layoutRowList(List<Row> rowList, int pageWidth, int rowHeight) {
    int top = 0;
    for (Row row : rowList) {
      row.setTop(top);
      row.setWidth(pageWidth);
      row.setHeight(rowHeight);
      top += rowHeight;
    }
  }

  public void reserveEmptySpanForInputFoldButton(boolean reserveEmptySpan) {
    this.reserveEmptySpan = reserveEmptySpan;
  }
}
