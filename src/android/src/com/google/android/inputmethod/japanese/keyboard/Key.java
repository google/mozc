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

import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory.DrawableType;
import com.google.common.base.MoreObjects;
import com.google.common.base.MoreObjects.ToStringHelper;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;
import com.google.common.collect.Iterables;
import com.google.common.collect.Sets;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Set;

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
  private final Stick stick;
  // Default KeyState.
  // Absent if this is a spacer.
  private final Optional<KeyState> defaultKeyState;
  private final DrawableType keyBackgroundDrawableType;

  private final List<KeyState> keyStateList;

  public Key(int x, int y, int width, int height, int horizontalGap,
             int edgeFlags, boolean isRepeatable, boolean isModifier,
             Stick stick, DrawableType keyBackgroundDrawableType,
             List<? extends KeyState> keyStateList) {
    Preconditions.checkNotNull(stick);
    Preconditions.checkNotNull(keyBackgroundDrawableType);
    Preconditions.checkNotNull(keyStateList);
    Preconditions.checkArgument(width >= 0);
    Preconditions.checkArgument(height >= 0);

    this.x = x;
    this.y = y;
    this.width = width;
    this.height = height;
    this.horizontalGap = horizontalGap;
    this.edgeFlags = edgeFlags;
    this.isRepeatable = isRepeatable;
    this.isModifier = isModifier;
    this.stick = stick;
    this.keyBackgroundDrawableType = keyBackgroundDrawableType;

    List<KeyState> tmpKeyStateList = null;  // Lazy creation.
    Optional<KeyState> defaultKeyState = Optional.absent();
    for (KeyState keyState : keyStateList) {
      Set<KeyState.MetaState> metaStateSet = keyState.getMetaStateSet();
      if (metaStateSet.isEmpty() || metaStateSet.contains(KeyState.MetaState.FALLBACK)) {
        if (defaultKeyState.isPresent()) {
          throw new IllegalArgumentException("Found duplicate default meta state");
        }
        defaultKeyState = Optional.of(keyState);
        if (metaStateSet.size() <= 1) {  // metaStateSet contains only FALLBACK
          continue;
        }
      }
      if (tmpKeyStateList == null) {
        tmpKeyStateList = new ArrayList<KeyState>();
      }
      tmpKeyStateList.add(keyState);
    }
    Preconditions.checkArgument(defaultKeyState.isPresent() || tmpKeyStateList == null,
                                "Default KeyState is mandatory for non-spacer.");
    this.defaultKeyState = defaultKeyState;
    this.keyStateList = tmpKeyStateList == null
        ? Collections.<KeyState>emptyList()
        : Collections.unmodifiableList(tmpKeyStateList);
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

  public Stick getStick() {
    return stick;
  }

  public DrawableType getKeyBackgroundDrawableType() {
    return keyBackgroundDrawableType;
  }

  /**
   * Returns {@code KeyState} at least one of which the metaState is in given {@code metaStates}.
   * <p>
   * For example, if there are following {@code KeyState}s (registered in this order);
   * <ul>
   * <li>KeyState1 : metaStates=A|B
   * <li>KeyState2 : metaStates=C
   * <li>KeyState3 : metaStates=null (default)
   * </ul>
   * <ul>
   * <li>metaStates=A gets KeyState1
   * <li>metaStates=A|B gets KeyState1
   * <li>metaStates=D gets KeyState3 as default
   * <li>metaStates=A|C gets KeyState1 as it is registered earlier than KeyState2.
   * </ul>
   */
  public Optional<KeyState> getKeyState(Set<KeyState.MetaState> metaStates) {
    Preconditions.checkNotNull(metaStates);

    for (KeyState state : keyStateList) {
      if (!Sets.intersection(state.getMetaStateSet(), metaStates).isEmpty()) {
        return Optional.of(state);
      }
    }
    return defaultKeyState;
  }

  public boolean isSpacer() {
    return !defaultKeyState.isPresent();
  }

  @Override
  public String toString() {
    ToStringHelper helper = MoreObjects.toStringHelper(this);
    helper.add("defaultKeyState",
               defaultKeyState.isPresent() ? defaultKeyState.get().toString() : "empty");
    for (KeyState entry : keyStateList) {
      helper.addValue(entry.toString());
    }
    return helper.toString();
  }

  /**
   * Gets all the {@code KeyState}s, including default one.
   */
  public Iterable<KeyState> getKeyStates() {
    if (defaultKeyState.isPresent()) {
      return Iterables.concat(keyStateList,
                              Collections.singletonList(defaultKeyState.get()));
    } else {
      // Spacer
      return Collections.emptySet();
    }
  }
}
