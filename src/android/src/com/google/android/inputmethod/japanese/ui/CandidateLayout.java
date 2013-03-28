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

import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;

import android.text.Layout;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Represents the layout information calculated by a layouter.
 *
 */
public class CandidateLayout {

  /** Horizontal span which is occupied by a CandidateWord. */
  public static class Span {
    private final CandidateWord candidateWord;
    private float left;
    private float right;

    private final float valueWidth;
    private final float descriptionWidth;
    private final List<String> splitDescriptionList;

    /**
     * This is a cache to improve the rendering performance.
     * In theory, it's better to have another structure to keep the cache,
     * but put this here for the simpler implementation.
     */
    private Layout cachedLayout;

    /** @param candidateWord the candidate word of this span. Must not be null. */
    public Span(CandidateWord candidateWord,
                float valueWidth, float descriptionWidth,
                List<String> splitDescriptionList) {
      this.candidateWord = candidateWord;
      this.valueWidth = valueWidth;
      this.descriptionWidth = descriptionWidth;
      this.splitDescriptionList = splitDescriptionList;
    }

    public CandidateWord getCandidateWord() {
      return candidateWord;
    }

    public float getLeft() {
      return left;
    }

    /** @param left the position of the left edge. Must be non negative value. */
    public void setLeft(float left) {
      if (left < 0) {
        throw new IllegalArgumentException();
      }
      this.left = left;
    }

    public float getRight() {
      return right;
    }

    /** @param right the position of the right edge. Must be equal or greater than left. */
    public void setRight(float right) {
      if (left > right) {
        throw new IllegalArgumentException();
      }
      this.right = right;
    }

    /** @return the width (== right - left) */
    public float getWidth() {
      return right - left;
    }

    public float getValueWidth() {
      return valueWidth;
    }

    public float getDescriptionWidth() {
      return descriptionWidth;
    }

    public List<String> getSplitDescriptionList() {
      return splitDescriptionList;
    }

    public Layout getCachedLayout() {
      return cachedLayout;
    }

    public void setCachedLayout(Layout cachedLayout) {
      this.cachedLayout = cachedLayout;
    }
  }

  public static class Row {
    /** Heuristic parameter for the number of spans in a row. */
    private static final int TYPICAL_SPANS_PER_ROW = 5;
    private final List<Span> spans = new ArrayList<Span>(TYPICAL_SPANS_PER_ROW);

    private float top;
    private float height;
    private float width;

    public float getTop() {
      return top;
    }

    public void setTop(float top) {
      this.top = top;
    }

    public float getHeight() {
      return height;
    }

    public void setHeight(float height) {
      this.height = height;
    }

    public float getWidth() {
      return width;
    }

    public void setWidth(float width) {
      this.width = width;
    }

    public List<Span> getSpanList() {
      return Collections.unmodifiableList(spans);
    }

    public void addSpan(Span span) {
      spans.add(span);
    }
  }

  private final List<Row> rowList;

  private final float contentWidth;

  private final float contentHeight;

  public CandidateLayout(
      List<Row> rowList, float contentWidth, float contentHeight) {
    this.rowList = rowList;
    this.contentWidth = contentWidth;
    this.contentHeight = contentHeight;
  }

  public List<Row> getRowList() {
    return rowList;
  }

  public float getContentWidth() {
    return contentWidth;
  }

  public float getContentHeight() {
    return contentHeight;
  }
}
