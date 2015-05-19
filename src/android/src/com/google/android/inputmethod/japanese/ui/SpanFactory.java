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

import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Span;

import android.graphics.Paint;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.StringTokenizer;

/**
 * Factory to create Span instances based on given CandidateWord instances.
 */
public class SpanFactory {

  /** Paint to measure value width in pixels */
  private final Paint valuePaint = new Paint();

  /** Paint to measure description width in pixels. */
  private final Paint descriptionPaint = new Paint();

  /** Delimiter characters for descriptions. */
  private String descriptionDelimiter = null;

  public void setValueTextSize(float valueTextSize) {
    valuePaint.setTextSize(valueTextSize);
  }

  public void setDescriptionTextSize(float descriptionTextSize) {
    descriptionPaint.setTextSize(descriptionTextSize);
  }

  public void setDescriptionDelimiter(String descriptionDelimiter) {
    this.descriptionDelimiter = descriptionDelimiter;
  }

  public Span newInstance(CandidateWord candidateWord) {
    float valueWidth = valuePaint.measureText(candidateWord.getValue());
    List<String> splitDescriptionList = splitDescription(
        candidateWord.getAnnotation().getDescription(), descriptionDelimiter);
    float descriptionWidth = 0;
    for (String line : splitDescriptionList) {
      float width = descriptionPaint.measureText(line);
      if (width > descriptionWidth) {
        descriptionWidth = width;
      }
    }
    return new Span(candidateWord, valueWidth, descriptionWidth, splitDescriptionList);
  }

  private static List<String> splitDescription(
      String description, String descriptionDelimiter) {
    if (description == null || description.length() == 0) {
      // No description is available.
      return Collections.emptyList();
    }

    if (descriptionDelimiter == null) {
      // If the delimiter is not set, return the description as is.
      return Collections.singletonList(description);
    }

    // Split the description by delimiter.
    StringTokenizer tokenizer = new StringTokenizer(description, descriptionDelimiter);
    List<String> result = new ArrayList<String>();
    while (tokenizer.hasMoreTokens()) {
      String token = tokenizer.nextToken();
      if (isEligibleDescriptionFragment(token)) {
        result.add(token);
      }
    }

    return result;
  }

  private static boolean isEligibleDescriptionFragment(String descriptionFragment) {
    // We'd like to always remove "ひらがな"  because the description fragment frequently
    // and largely increases the width of a candidate span.
    // Increased width reduces the number of the candidates which are shown in a screen.
    // This behavior is especially harmful for zero-query suggestion
    // because zero-query suggestion mainly shows Hiragana candidates.
    // TODO(matsuzakit): Such filtering/oprimization should be done in the server side.
    return !"ひらがな".equals(descriptionFragment);
  }
}
