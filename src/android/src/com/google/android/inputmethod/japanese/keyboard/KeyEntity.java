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

package org.mozc.android.inputmethod.japanese.keyboard;

import com.google.common.base.MoreObjects;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

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
  private final boolean longPressTimeoutTrigger;
  private final int keyIconResourceId;
  // If |keyIconResourceId| is empty and |keyCharacter| is set,
  // use this character for rendering key top.
  // TODO(team): Implement rendering method.
  private final Optional<String> keyCharacter;
  private final boolean flickHighlightEnabled;
  private final Optional<PopUp> popUp;
  private final int horizontalPadding;
  private final int verticalPadding;
  private final int iconWidth;
  private final int iconHeight;

  public KeyEntity(int sourceId, int keyCode, int longPressKeyCode,
                   boolean longPressTimeoutTrigger, int keyIconResourceId,
                   Optional<String> keyCharacter, boolean flickHighlightEnabled,
                   Optional<PopUp> popUp, int horizontalPadding, int verticalPadding,
                   int iconWidth, int iconHeight) {
    this.sourceId = sourceId;
    this.keyCode = keyCode;
    this.longPressKeyCode = longPressKeyCode;
    this.longPressTimeoutTrigger = longPressTimeoutTrigger;
    this.keyIconResourceId = keyIconResourceId;
    this.keyCharacter = Preconditions.checkNotNull(keyCharacter);
    this.flickHighlightEnabled = flickHighlightEnabled;
    this.popUp = Preconditions.checkNotNull(popUp);
    this.horizontalPadding = horizontalPadding;
    this.verticalPadding = verticalPadding;
    this.iconWidth = iconWidth;
    this.iconHeight = iconHeight;
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

  public boolean isLongPressTimeoutTrigger() {
    return longPressTimeoutTrigger;
  }

  public int getKeyIconResourceId() {
    return keyIconResourceId;
  }

  public Optional<String> getKeyCharacter() {
    return keyCharacter;
  }

  public boolean isFlickHighlightEnabled() {
    return flickHighlightEnabled;
  }

  public Optional<PopUp> getPopUp() {
    return popUp;
  }

  public int getHorizontalPadding() {
    return horizontalPadding;
  }

  public int getVerticalPadding() {
    return verticalPadding;
  }

  public int getIconWidth() {
    return iconWidth;
  }

  public int getIconHeight() {
    return iconHeight;
  }

  @Override
  public String toString() {
    return MoreObjects.toStringHelper(this)
                  .add("sourceId", sourceId)
                  .add("keyCode", keyCode)
                  .add("longPressKeyCode", longPressKeyCode)
                  .add("longPressTimeoutTrigger", longPressTimeoutTrigger)
                  .add("keyIconResourceId", keyIconResourceId)
                  .add("keyCharacter", keyCharacter)
                  .add("horizontalPadding", horizontalPadding)
                  .add("verticalPadding", verticalPadding)
                  .add("iconWidth", iconWidth)
                  .add("iconHeight", iconHeight)
                  .toString();
  }
}
