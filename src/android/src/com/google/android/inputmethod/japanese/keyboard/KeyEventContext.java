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

import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchAction;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchPosition;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import java.util.Set;

import javax.annotation.Nullable;

/**
 * This class represents user's one action, e.g., the sequence of:
 * press -> move -> move -> ... -> move -> release.
 *
 * For user's multi touch events, multiple instances will be instantiated.
 * E.g. for user's two finger strokes, two instances will be instantiated.
 *
 */
// TODO(matsuzakit): Get rid of @Nullable.
public class KeyEventContext {
  final Key key;
  final int pointerId;
  private final float pressedX;
  private final float pressedY;
  private final float flickThresholdSquared;
  private final boolean isFlickableKey;
  private final Optional<KeyState> keyState;
  Flick.Direction flickDirection = Flick.Direction.CENTER;

  // TODO(hidehiko): Move logging code to an upper layer, e.g., MozcService or ViewManager etc.
  //   after refactoring the architecture.
  private TouchAction lastAction = null;
  private float lastX;
  private float lastY;
  private long lastTimestamp;
  private final int keyboardWidth;
  private final int keyboardHeight;

  // This variable will be updated in the callback of long press key event (if necessary).
  boolean longPressSent = false;

  public KeyEventContext(Key key, int pointerId, float pressedX, float pressedY,
                         int keyboardWidth, int keyboardHeight,
                         float flickThresholdSquared, Set<MetaState> metaState) {
    Preconditions.checkNotNull(key);
    Preconditions.checkNotNull(metaState);

    this.key = key;
    this.pressedX = pressedX;
    this.pressedY = pressedY;
    this.flickThresholdSquared = flickThresholdSquared;
    this.isFlickableKey = isFlickable(key, metaState);
    this.keyState = key.getKeyState(metaState);
    this.pointerId = pointerId;
    this.keyboardWidth = keyboardWidth;
    this.keyboardHeight = keyboardHeight;
  }

  float getFlickThresholdSquared() {
    return flickThresholdSquared;
  }

  /**
   * Returns true iff the point ({@code x}, {@code y}) is contained by the {@code key}'s region.
   * This is package private for testing purpose.
   */
  static boolean isContained(float x, float y, Key key) {
    float relativeX = x - key.getX();
    float relativeY = y - key.getY();
    return 0 <= relativeX && relativeX < key.getWidth() &&
           0 <= relativeY && relativeY < key.getHeight();
  }

  /**
   * Returns true iff the key is flickable. Otherwise returns false.
   */
  @VisibleForTesting
  static boolean isFlickable(Key key, Set<MetaState> metaState) {
    Preconditions.checkNotNull(key);
    Preconditions.checkNotNull(metaState);

    Optional<KeyState> optionalKeyState = key.getKeyState(metaState);
    if (!optionalKeyState.isPresent()) {
      return false;
    }
    KeyState keyState = optionalKeyState.get();
    return keyState.getFlick(Flick.Direction.LEFT) != null ||
           keyState.getFlick(Flick.Direction.UP) != null ||
           keyState.getFlick(Flick.Direction.RIGHT) != null ||
           keyState.getFlick(Flick.Direction.DOWN) != null;
  }

  /**
   * Returns the key entity corresponding to {@code metaState} and {@code direction}.
   */
  @Nullable
  public static KeyEntity getKeyEntity(Key key, Set<MetaState> metaState,
                                       @Nullable Flick.Direction direction) {
    Preconditions.checkNotNull(key);
    Preconditions.checkNotNull(metaState);

    if (key.isSpacer()) {
      return null;
    }
    // Key is not spacer for at least one KeyState is available.
    return getKeyEntityInternal(key.getKeyState(metaState).get(), direction).orNull();
  }

  private Optional<KeyEntity> getKeyEntity(Flick.Direction direction) {
    return keyState.isPresent()
        ? getKeyEntityInternal(keyState.get(), direction)
        : Optional.<KeyEntity>absent();
  }

  private static Optional<KeyEntity> getKeyEntityInternal(KeyState keyState,
                                                          @Nullable Flick.Direction direction) {
    Preconditions.checkNotNull(keyState);

    if (direction == null) {
      return Optional.absent();
    }

    Flick flick = keyState.getFlick(direction);
    return flick == null ? Optional.<KeyEntity>absent() : Optional.of(flick.getKeyEntity());
  }

  /**
   * Returns the key code to be sent via {@link KeyboardActionListener#onKey(int, java.util.List)}.
   */
  public int getKeyCode() {
    if (longPressSent) {
      // If the long-press-key event is already sent, just return INVALID_KEY_CODE.
      return KeyEntity.INVALID_KEY_CODE;
    }

    Optional<KeyEntity> keyEntity = getKeyEntity(flickDirection);
    return keyEntity.isPresent()
        ? keyEntity.get().getKeyCode()
        : KeyEntity.INVALID_KEY_CODE;
  }

  Set<MetaState> getNextMetaStates(Set<MetaState> originalMetaStates) {
    if (!key.isModifier() || key.isSpacer()) {
      // Non-modifier key shouldn't change meta state.
      return originalMetaStates;
    }
    Set<MetaState> result = keyState.get().getNextMetaStates(originalMetaStates);
    return result;
  }

  /**
   * Returns the key code to be sent for long press event.
   */
  int getLongPressKeyCode() {
    if (longPressSent) {
      // If the long-press-key event is already sent, just return INVALID_KEY_CODE.
      return KeyEntity.INVALID_KEY_CODE;
    }

    // Note that we always use CENTER flick direction for long press key events.
    Optional<KeyEntity> keyEntity = getKeyEntity(Flick.Direction.CENTER);
    return keyEntity.isPresent()
        ? keyEntity.get().getLongPressKeyCode()
        : KeyEntity.INVALID_KEY_CODE;
  }

  /**
   * Returns the key code to be send via {@link KeyboardActionListener#onPress(int)} and
   * {@link KeyboardActionListener#onRelease(int)}.
   */
  public int getPressedKeyCode() {
    Optional<KeyEntity> keyEntity = getKeyEntity(Flick.Direction.CENTER);
    return keyEntity.isPresent()
        ? keyEntity.get().getKeyCode()
        : KeyEntity.INVALID_KEY_CODE;
  }

  /**
   * Returns true if this key event sequence represents toggling meta state.
   */
  boolean isMetaStateToggleEvent() {
    return !longPressSent && key.isModifier() && flickDirection == Flick.Direction.CENTER;
  }

  /**
   * Returns the pop up data for the current state.
   */
  @Nullable
  PopUp getCurrentPopUp() {
    if (longPressSent) {
      return null;
    }

    Optional<KeyEntity> keyEntity = getKeyEntity(flickDirection);
    return keyEntity.isPresent() ? keyEntity.get().getPopUp() : null;
  }

  /**
   * Updates the internal state of this context when the touched position is moved to
   * {@code (x, y)} at time {@code timestamp} in milliseconds since the press.
   * @return {@code true} if the internal state is actually updated.
   */
  public boolean update(float x, float y, TouchAction touchAction, long timestamp) {
    Flick.Direction originalDirection = flickDirection;
    lastAction = touchAction;
    lastX = x;
    lastY = y;
    lastTimestamp = timestamp;
    float deltaX = x - pressedX;
    float deltaY = y - pressedY;

    if (deltaX * deltaX + deltaY * deltaY < flickThresholdSquared ||
        !isFlickableKey && isContained(x, y, key)) {
      // A user touches (or returns back to) the same key, so we don't fire flick.
      // If the key isn't flickable, we also look at the key's region to avoid unexpected
      // cancellation.
      flickDirection = Flick.Direction.CENTER;
    } else {
      if (Math.abs(deltaX) < Math.abs(deltaY)) {
        // Vertical flick
        flickDirection = deltaY < 0 ? Flick.Direction.UP : Flick.Direction.DOWN;
      } else {
        // Horizontal flick
        flickDirection = deltaX > 0 ? Flick.Direction.RIGHT : Flick.Direction.LEFT;
      }
    }
    return flickDirection != originalDirection;
  }

  /**
   * @return {@code TouchEvent} instance which includes the stroke related to this context.
   */
  @Nullable
  public TouchEvent getTouchEvent() {
    Optional<KeyEntity> keyEntity = getKeyEntity(flickDirection);
    if (!keyEntity.isPresent()) {
      return null;
    }

    TouchEvent.Builder builder = TouchEvent.newBuilder()
        .setSourceId(keyEntity.get().getSourceId());
    builder.addStroke(createTouchPosition(
        TouchAction.TOUCH_DOWN, pressedX, pressedY, keyboardWidth, keyboardHeight, 0));
    if (lastAction != null) {
      builder.addStroke(createTouchPosition(
          lastAction, lastX, lastY, keyboardWidth, keyboardHeight, lastTimestamp));
    }
    return builder.build();
  }

  public static TouchPosition createTouchPosition(
      TouchAction action, float x, float y, int width, int height, long timestamp) {
    return TouchPosition.newBuilder()
        .setAction(action)
        .setX(x / width)
        .setY(y / height)
        .setTimestamp(timestamp)
        .build();
  }
}
