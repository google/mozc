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

import java.util.EnumMap;
import java.util.List;


/**
 * This is a model class of a key, corresponding to a {@code &lt;Key&gt;} element
 * in a xml resource file.
 * 
 * Here is a list this class supports.
 * <ul>
 *   <li> {@code keyBackground}: an background image for the key.
 *   <li> {@code width}: key width.
 *   <li> {@code height}: key height.
 *   <li> {@code horizontalGap}: the gap between the (both) next keys and this key.
 *        Note that the width should includes horizontalGap. HorizontalGap will be evenly
 *        divided into each side.
 *   <li> {@code edgeFlags}: flags whether the key should stick to keyboard's boundary.
 *   <li> {@code isRepeatable}: whether the key long press cause a repeated key tapping.
 *   <li> {@code isModifier}: whether the key is modifier (e.g. shift key).
 *   <li> {@code isSticky}: whether the key behaves sticky (e.g. caps lock).
 * </ul>
 * 
 * The {@code &lt;Key&gt;} element can have (at most) two {@code &lt;KeyState&gt;} elements.
 * See also KeyState class for details.
 * 
 */
public class Key {
  /**
   * This enum is only for spacers, not actual keys, to specify clicking behavior.
   * If this is set to:
   * - {@code EVEN}, then the spacer will be split into two regions, left and right.
   *     If a user touches left half, it is considered a part of the key on the spacer's immediate
   *     left. If a user touches right half, it is considered a part of the right one.
   * - {@code LEFT}, then the corresponding key is the left one.
   * - {@code RIGHT}, then the corresponding key is the right one.
   */
  public enum Stick {
    EVEN, LEFT, RIGHT,
  }

  private final int x;
  private final int y;
  private final int width;
  private final int height;
  private final int horizontalGap;
  private final int edgeFlags;
  private final boolean isRepeatable;
  private final boolean isModifier;
  private final boolean isSticky;
  private final Stick stick;
  private final EnumMap<KeyState.MetaState, KeyState> keyStateMap;

  public Key(int x, int y, int width, int height, int horizontalGap, 
             int edgeFlags, boolean isRepeatable, boolean isModifier, boolean isSticky,
             Stick stick, List<? extends KeyState> keyStateList) {
    this.x = x;
    this.y = y;
    this.width = width;
    this.height = height;
    this.horizontalGap = horizontalGap;
    this.edgeFlags = edgeFlags;
    this.isRepeatable = isRepeatable;
    this.isModifier = isModifier;
    this.isSticky = isSticky;
    this.stick = stick;

    this.keyStateMap =
        new EnumMap<KeyState.MetaState, KeyState>(KeyState.MetaState.class);
    for (KeyState keyState : keyStateList) {
      for (KeyState.MetaState metaState : keyState.getMetaStateSet()) {
        if (this.keyStateMap.put(metaState, keyState) != null) {
          throw new IllegalArgumentException("Found duplicate meta state: " + metaState);
        }
      }
    }
  }

  public int getX() {
    return x;
  }

  public int getY() {
    return y;
  }

  public int getWidth() {
    return width;
  }

  public int getHeight() {
    return height;
  }

  public int getHorizontalGap() {
    return horizontalGap;
  }

  public int getEdgeFlags() {
    return edgeFlags;
  }

  public boolean isRepeatable() {
    return isRepeatable;
  }

  public boolean isModifier() {
    return isModifier;
  }

  public boolean isSticky() {
    return isSticky;
  }

  public Stick getStick() {
    return stick;
  }

  public KeyState getKeyState(KeyState.MetaState metaState) {
    return keyStateMap.get(metaState);
  }

  public boolean isSpacer() {
    return keyStateMap.isEmpty();
  }
}
