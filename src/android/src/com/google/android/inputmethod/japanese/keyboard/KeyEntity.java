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

import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory.DrawableType;

/**
 * A class corresponding to a {@code &lt;KeyEntity&gt;} element in xml resource files.
 * This class has basic attributes for a key, which can have various values based on conditions,
 * e.g. meta keys' state or flick directions.
 *
 */
public class KeyEntity {
  /** INVALID_KEY_CODE represents the key has no key code to call the Mozc server back. */
  public static final int INVALID_KEY_CODE = Integer.MIN_VALUE;

  private final int sourceId;
  private final int keyCode;
  private final int longPressKeyCode;
  private final int keyIconResourceId;
  // If |keyIconResourceId| is empty and |keyCharacter| is set,
  // use this character for rendering key top.
  // TODO(team): Implement rendering method.
  private final String keyCharacter;
  // TODO(hidehiko): Move this field to Key.
  private final DrawableType keyBackgroundDrawableType;
  private final boolean flickHighlightEnabled;
  private final PopUp popUp;

  // Constructor for compatibility.
  // TODO(team): Remove this constructor after changing all callers including tests.
  public KeyEntity(int sourceId, int keyCode, int longPressKeyCode,
                   int keyIconResourceId,
                   DrawableType keyBackgroundDrawableType,
                   boolean flickHighlightEnabled, PopUp popUp) {
    this(
        sourceId, keyCode, longPressKeyCode, keyIconResourceId, null, keyBackgroundDrawableType,
        flickHighlightEnabled, popUp);
  }

  public KeyEntity(int sourceId, int keyCode, int longPressKeyCode,
                   int keyIconResourceId, String keyCharacter,
                   DrawableType keyBackgroundDrawableType,
                   boolean flickHighlightEnabled, PopUp popUp) {
    this.sourceId = sourceId;
    this.keyCode = keyCode;
    this.longPressKeyCode = longPressKeyCode;
    this.keyIconResourceId = keyIconResourceId;
    this.keyCharacter = keyCharacter;
    this.keyBackgroundDrawableType = keyBackgroundDrawableType;
    this.flickHighlightEnabled = flickHighlightEnabled;
    this.popUp = popUp;
  }

  public int getSourceId() {
    return sourceId;
  }

  public int getKeyCode() {
    return keyCode;
  }

  public int getLongPressKeyCode() {
    return longPressKeyCode;
  }

  public int getKeyIconResourceId() {
    return keyIconResourceId;
  }

  public String getKeyCharacter() {
    return keyCharacter;
  }

  public DrawableType getKeyBackgroundDrawableType() {
    return keyBackgroundDrawableType;
  }

  public boolean isFlickHighlightEnabled() {
    return flickHighlightEnabled;
  }

  public PopUp getPopUp() {
    return popUp;
  }
}
