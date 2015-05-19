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

package org.mozc.android.inputmethod.japanese.keyboard;

import java.util.Collections;
import java.util.List;

/**
 * A simple model class of a keyboard.
 * A keyboard can contain a sequence of {@code Row}s.
 *
 */
public class Keyboard {

  private final float flickThreshold;
  private final List<Row> rowList;

  public final int contentLeft;
  public final int contentRight;
  public final int contentTop;
  public final int contentBottom;

  public Keyboard(List<? extends Row> rowList, float flickThreshold) {
    this.flickThreshold = flickThreshold;
    this.rowList = Collections.unmodifiableList(rowList);

    int left = Integer.MAX_VALUE, right = Integer.MIN_VALUE,
        top = Integer.MAX_VALUE, bottom = Integer.MIN_VALUE;
    for (Row row : this.rowList) {
      for (Key key : row.getKeyList()) {
        left = Math.min(left, key.getX());
        right = Math.max(right, key.getX() + key.getWidth());
        top = Math.min(top, key.getY());
        bottom = Math.max(bottom, key.getY() + key.getHeight());
      }
    }
    this.contentLeft = left;
    this.contentRight = right;
    this.contentTop = top;
    this.contentBottom = bottom;
  }

  public float getFlickThreshold() {
    return flickThreshold;
  }

  public List<Row> getRowList() {
    return rowList;
  }
}
