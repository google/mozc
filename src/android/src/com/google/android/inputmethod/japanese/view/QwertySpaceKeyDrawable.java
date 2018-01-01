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

package org.mozc.android.inputmethod.japanese.view;

import android.graphics.Rect;

/** The space key implementation for qwerty keyboards. */
public class QwertySpaceKeyDrawable extends RoundRectKeyDrawable {
  private static final int UNLIMITED_HEIGHT = 0;
  private final int height;

  public QwertySpaceKeyDrawable(
      int height, int leftPadding, int topPadding, int rightPadding, int bottomPadding,
      int roundSize, int topColor, int bottomColor, int highlightColor, int shadowColor) {
    super(leftPadding, topPadding, rightPadding, bottomPadding,
          roundSize, topColor, bottomColor, highlightColor, shadowColor);
    this.height = height;
  }

  @Override
  protected void onBoundsChange(Rect bounds) {
    if (height != UNLIMITED_HEIGHT && bounds.height() > height) {
      int topPadding = (bounds.height() - height) / 2;
      int bottomPadding = (bounds.height() - height) - topPadding;
      bounds = new Rect(bounds.left, bounds.top + topPadding,
                        bounds.right, bounds.bottom - bottomPadding);
    }
    super.onBoundsChange(bounds);
  }
}
