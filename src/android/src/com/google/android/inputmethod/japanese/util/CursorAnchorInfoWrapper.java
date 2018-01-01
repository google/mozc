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

package org.mozc.android.inputmethod.japanese.util;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;

import android.annotation.TargetApi;
import android.graphics.Matrix;
import android.graphics.RectF;
import android.view.inputmethod.CursorAnchorInfo;

/**
 * Simple wrapper class of {@link CursorAnchorInfo}.
 * <p>
 * This is introduced for the compatibility on KitKat or prior, since {@link CursorAnchorInfo} class
 * is available on Lollipop or later.
 * We can construct this class safely on any environments iff we use the default constructor.
 */
@TargetApi(21)
public class CursorAnchorInfoWrapper {

  @VisibleForTesting public final Optional<CursorAnchorInfo> rawData;

  public CursorAnchorInfoWrapper() {
    this.rawData = Optional.absent();
  }

  public CursorAnchorInfoWrapper(CursorAnchorInfo info) {
    this.rawData = Optional.of(info);
  }

  public int describeContents() {
    if (rawData.isPresent()) {
      return rawData.get().describeContents();
    }
    return 0;
  }

  @Override
  public boolean equals(Object obj) {
    if (!(obj instanceof CursorAnchorInfoWrapper)) {
      return false;
    }
    CursorAnchorInfoWrapper other = CursorAnchorInfoWrapper.class.cast(obj);
    return this.rawData.equals(other.rawData);
  }

  public RectF getCharacterBounds(int index) {
    if (rawData.isPresent()) {
      return rawData.get().getCharacterBounds(index);
    }
    return new RectF();
  }

  public int getCharacterBoundsFlags(int index) {
    if (rawData.isPresent()) {
      return rawData.get().getCharacterBoundsFlags(index);
    }
    return 0;
  }

  public CharSequence getComposingText() {
    if (rawData.isPresent()) {
      return rawData.get().getComposingText();
    }
    return "";
  }

  public int getComposingTextStart() {
    if (rawData.isPresent()) {
      return rawData.get().getComposingTextStart();
    }
    return 0;
  }

  public float getInsertionMarkerBaseline() {
    if (rawData.isPresent()) {
      return rawData.get().getInsertionMarkerBaseline();
    }
    return 0f;
  }

  public float getInsertionMarkerBottom() {
    if (rawData.isPresent()) {
      return rawData.get().getInsertionMarkerBottom();
    }
    return 0f;
  }

  public int getInsertionMarkerFlags() {
    if (rawData.isPresent()) {
      return rawData.get().getInsertionMarkerFlags();
    }
    return 0;
  }

  public float getInsertionMarkerHorizontal() {
    if (rawData.isPresent()) {
      return rawData.get().getInsertionMarkerHorizontal();
    }
    return 0f;
  }

  public float getInsertionMarkerTop() {
    if (rawData.isPresent()) {
      return rawData.get().getInsertionMarkerTop();
    }
    return 0;
  }

  public Matrix getMatrix() {
    if (rawData.isPresent()) {
      return rawData.get().getMatrix();
    }
    return new Matrix();
  }

  public int getSelectionEnd() {
    if (rawData.isPresent()) {
      return rawData.get().getSelectionEnd();
    }
    return 0;
  }

  public int getSelectionStart() {
    if (rawData.isPresent()) {
      return rawData.get().getSelectionStart();
    }
    return 0;
  }

  @Override
  public int hashCode() {
    return rawData.hashCode();
  }

  @Override
  public String toString() {
    return rawData.toString();
  }
}
