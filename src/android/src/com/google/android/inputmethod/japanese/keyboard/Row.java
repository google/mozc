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

package org.mozc.android.inputmethod.japanese.keyboard;

import java.util.Collections;
import java.util.List;

/**
 * A simple model class of a row. This class corresponds to {@code &lt;Row&gt;} element
 * in a xml resource file. Each row can contain a sequence of {@code Key}s.
 * 
 * Here is a list this class supports.
 * <ul>
 *   <li> {@code height}: the row's height.
 *   <li> {@code verticalGap}: the gap between the next row and this class.
 * </ul>
 * 
 */
public class Row {
  private final List<? extends Key> keyList;
  private final int height;
  private final int verticalGap;
  
  public Row(List<? extends Key> keyList, int height, int verticalGap) {
    this.keyList = Collections.unmodifiableList(keyList);
    this.height = height;
    this.verticalGap = verticalGap;
  }

  public List<? extends Key> getKeyList() {
    return keyList;
  }

  public int getHeight() {
    return height;
  }

  public int getVerticalGap() {
    return verticalGap;
  }
}
