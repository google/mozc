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
import org.mozc.android.inputmethod.japanese.keyboard.Key.Stick;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchAction;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import com.google.common.base.Optional;

import android.test.suitebuilder.annotation.SmallTest;

import junit.framework.TestCase;

import java.util.Arrays;
import java.util.Collections;
import java.util.EnumSet;

/**
 */
public class KeyEventContextTest extends TestCase {

  private PopUp defaultPopUp;
  private PopUp leftPopUp;
  private KeyEntity defaultEntity;
  private KeyEntity leftFlickEntity;
  private KeyEntity modifiedCenterEntity;
  private KeyEntity modifiedLeftEntity;
  private KeyState defaultState;
  private KeyState modifiedState;
  private Key modifiableKey;
  private Key longpressableKeyWithTimeoutTrigger;
  private Key longpressableKeyWithoutTimeoutTrigger;

  @Override
  public void setUp() throws Exception {
    super.setUp();

    defaultPopUp = new PopUp(0, 0, 0, 0, 0, 0, 0);
    leftPopUp = new PopUp(0, 0, 0, 0, 0, 0, 0);

    defaultEntity = new KeyEntity(
        1, 'a', 'A', true, 0, Optional.<String>absent(), false, Optional.of(defaultPopUp),
        0, 0, 0, 0);
    leftFlickEntity = new KeyEntity(
        2, 'b', 'B', true, 0, Optional.<String>absent(), false, Optional.of(leftPopUp), 0, 0, 0, 0);
    modifiedCenterEntity = createKeyEntity(3, 'c', 'C');
    modifiedLeftEntity = createKeyEntity(4, 'd', 'D');

    KeyEntity longpressableEntityWithTimeoutTrigger = new KeyEntity(
        5, 'e', 'E', true, 0, Optional.<String>absent(), false, Optional.of(defaultPopUp),
        0, 0, 0, 0);
    KeyEntity longpressableEntityWithoutTimeoutTrigger = new KeyEntity(
        6, 'f', 'F', false, 0, Optional.<String>absent(), false, Optional.of(defaultPopUp),
        0, 0, 0, 0);

    defaultState = new KeyState(
        "", Collections.<MetaState>emptySet(),
        Collections.<MetaState>emptySet(), Collections.<MetaState>emptySet(),
        Arrays.asList(new Flick(Flick.Direction.CENTER, defaultEntity),
                      new Flick(Flick.Direction.LEFT, leftFlickEntity)));
    modifiedState = new KeyState(
        "", EnumSet.of(MetaState.CAPS_LOCK), Collections.<MetaState>emptySet(),
        EnumSet.of(MetaState.CAPS_LOCK),
        Arrays.asList(new Flick(Flick.Direction.CENTER, modifiedCenterEntity),
                      new Flick(Flick.Direction.LEFT, modifiedLeftEntity)));

    modifiableKey = new Key(0, 0, 16, 16, 0, 0, false, false, Stick.EVEN,
                            DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND,
                            Arrays.asList(defaultState, modifiedState));
    longpressableKeyWithTimeoutTrigger = new Key(0, 0, 16, 16, 0, 0, false, false, Stick.EVEN,
        DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND,
        Collections.singletonList(
            new KeyState("", Collections.<MetaState>emptySet(),
                         Collections.<MetaState>emptySet(), Collections.<MetaState>emptySet(),
                         Collections.singletonList(
                             new Flick(Flick.Direction.CENTER,
                                       longpressableEntityWithTimeoutTrigger)))));
    longpressableKeyWithoutTimeoutTrigger = new Key(0, 0, 16, 16, 0, 0, false, false, Stick.EVEN,
        DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND,
        Collections.singletonList(
            new KeyState("", Collections.<MetaState>emptySet(),
                         Collections.<MetaState>emptySet(), Collections.<MetaState>emptySet(),
                         Collections.singletonList(
                             new Flick(Flick.Direction.CENTER,
                                       longpressableEntityWithoutTimeoutTrigger)))));
  }

  @Override
  public void tearDown() throws Exception {
    defaultPopUp = null;
    leftPopUp = null;
    defaultEntity = null;
    leftFlickEntity = null;
    modifiedCenterEntity = null;
    modifiedLeftEntity = null;
    defaultState = null;
    modifiedState = null;
    modifiableKey = null;

    super.tearDown();
  }

  private static KeyEntity createKeyEntity(int sourceId, int keyCode, int longPressKeyCode) {
    return new KeyEntity(
        sourceId, keyCode, longPressKeyCode, true, 0,
        Optional.<String>absent(), false, Optional.<PopUp>absent(), 0, 0, 0, 0);
  }

  @SmallTest
  public void testIsContained() {
    Key key = new Key(
        0, 0, 30, 50, 0, 0, false, false, Stick.EVEN,
        DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND, Collections.<KeyState>emptyList());
    assertTrue(KeyEventContext.isContained(15, 25, key));
    assertTrue(KeyEventContext.isContained(10, 5, key));
    assertTrue(KeyEventContext.isContained(29, 45, key));
    assertFalse(KeyEventContext.isContained(31, 25, key));
    assertFalse(KeyEventContext.isContained(-1, 45, key));
    assertFalse(KeyEventContext.isContained(15, -20, key));
    assertFalse(KeyEventContext.isContained(5, 60, key));
  }

  @SmallTest
  public void testIsFlickable() {
    // If the key doesn't return KeyState, isFlickable should return false.
    assertFalse(KeyEventContext.isFlickable(
        new Key(0, 0, 30, 50, 0, 0, false, false, Stick.EVEN,
            DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND, Collections.<KeyState>emptyList()),
        Collections.<MetaState>emptySet()));

    // If the key doesn't have flick related data other than CENTER, it is not flickable.
    KeyEntity dummyKeyEntity = createKeyEntity(1, 'a', KeyEntity.INVALID_KEY_CODE);
    Flick center = new Flick(Flick.Direction.CENTER, dummyKeyEntity);
    assertFalse(KeyEventContext.isFlickable(
        new Key(0, 0, 30, 50, 0, 0, false, false, Stick.EVEN,
                DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND,
                Collections.singletonList(
                    new KeyState("",
                                 Collections.<MetaState>emptySet(),
                                 Collections.<MetaState>emptySet(),
                                 Collections.<MetaState>emptySet(),
                                 Collections.singletonList(center)))),
        Collections.<MetaState>emptySet()));

    // If the key has flick related data other than CENTER, it is flickable.
    Flick left = new Flick(Flick.Direction.LEFT, dummyKeyEntity);
    assertTrue(KeyEventContext.isFlickable(
        new Key(0, 0, 30, 50, 0, 0, false, false, Stick.EVEN,
                DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND,
                Collections.singletonList(
                    new KeyState("",
                                 Collections.<MetaState>emptySet(),
                                 Collections.<MetaState>emptySet(),
                                 Collections.<MetaState>emptySet(),
                                 Arrays.asList(center, left)))),
        Collections.<MetaState>emptySet()));
  }

  @SmallTest
  public void testGetKeyEntity() {
    assertSame(defaultEntity,
               KeyEventContext.getKeyEntity(
                   modifiableKey, Collections.<MetaState>emptySet(),
                   Optional.of(Flick.Direction.CENTER)).get());
    assertSame(leftFlickEntity,
               KeyEventContext.getKeyEntity(
                   modifiableKey, Collections.<MetaState>emptySet(),
                   Optional.of(Flick.Direction.LEFT)).get());
    assertSame(modifiedCenterEntity,
               KeyEventContext.getKeyEntity(
                   modifiableKey, EnumSet.of(MetaState.CAPS_LOCK),
                   Optional.of(Flick.Direction.CENTER)).get());
    assertSame(modifiedLeftEntity,
               KeyEventContext.getKeyEntity(
                   modifiableKey, EnumSet.of(MetaState.CAPS_LOCK),
                   Optional.of(Flick.Direction.LEFT)).get());

    // If a key entity for the flick direction is not specified,
    // Optional.absent() should be returned.
    assertFalse(KeyEventContext.getKeyEntity(
        modifiableKey, Collections.<MetaState>emptySet(),
        Optional.of(Flick.Direction.RIGHT)).isPresent());
    assertFalse(KeyEventContext.getKeyEntity(
        modifiableKey, EnumSet.of(MetaState.CAPS_LOCK),
        Optional.of(Flick.Direction.RIGHT)).isPresent());

    Key unmodifiableKey = new Key(0, 0, 0, 0, 0, 0, false, false, Stick.EVEN,
                                  DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND,
                                  Collections.singletonList(defaultState));

    // If a key doesn't have modified state, default state should be used as its fall back.
    assertSame(defaultEntity,
               KeyEventContext.getKeyEntity(
                   unmodifiableKey, Collections.<MetaState>emptySet(),
                   Optional.of(Flick.Direction.CENTER)).get());
    assertSame(leftFlickEntity,
               KeyEventContext.getKeyEntity(
                   unmodifiableKey, Collections.<MetaState>emptySet(),
                   Optional.of(Flick.Direction.LEFT)).get());
    assertSame(defaultEntity,
               KeyEventContext.getKeyEntity(
                   unmodifiableKey, EnumSet.of(MetaState.CAPS_LOCK),
                   Optional.of(Flick.Direction.CENTER)).get());
    assertSame(leftFlickEntity,
               KeyEventContext.getKeyEntity(
                   unmodifiableKey, EnumSet.of(MetaState.CAPS_LOCK),
                   Optional.of(Flick.Direction.LEFT)).get());

    // If both state is not specified, Optional.absent() should be returned.
    Key spacer = new Key(0, 0, 0, 0, 0, 0, false, false, Stick.EVEN,
                         DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND,
                         Collections.<KeyState>emptyList());
    assertFalse(KeyEventContext.getKeyEntity(spacer, Collections.<MetaState>emptySet(),
                                             Optional.of(Flick.Direction.CENTER)).isPresent());
    assertFalse(KeyEventContext.getKeyEntity(spacer, EnumSet.of(MetaState.CAPS_LOCK),
                                             Optional.of(Flick.Direction.CENTER)).isPresent());
  }

  @SmallTest
  public void testGetKeyCode() {
    KeyEventContext keyEventContext =
        new KeyEventContext(modifiableKey, 0, 0, 0, 100, 100, 1, Collections.<MetaState>emptySet());
    assertEquals('a', keyEventContext.getKeyCode());
    keyEventContext.flickDirection = Flick.Direction.LEFT;
    assertEquals('b', keyEventContext.getKeyCode());
    keyEventContext.flickDirection = Flick.Direction.RIGHT;
    assertEquals(KeyEntity.INVALID_KEY_CODE, keyEventContext.getKeyCode());
    keyEventContext.pastLongPressSentTimeout = true;
    keyEventContext.flickDirection = Flick.Direction.CENTER;
    assertEquals(KeyEntity.INVALID_KEY_CODE, keyEventContext.getKeyCode());
  }

  @SmallTest
  public void testGetKeyCodeForLongPressWithTimeoutTrigger() {
    KeyEventContext keyEventContext =
        new KeyEventContext(longpressableKeyWithTimeoutTrigger, 0, 0, 0, 100, 100, 1,
                            Collections.<MetaState>emptySet());
    keyEventContext.pastLongPressSentTimeout = false;
    assertEquals('e', keyEventContext.getKeyCode());
    keyEventContext.pastLongPressSentTimeout = true;
    assertEquals(KeyEntity.INVALID_KEY_CODE, keyEventContext.getKeyCode());
  }

  @SmallTest
  public void testGetKeyCodeForLongPressWithoutTimeoutTrigger() {
    KeyEventContext keyEventContext =
        new KeyEventContext(longpressableKeyWithoutTimeoutTrigger, 0, 0, 0, 100, 100, 1,
                            Collections.<MetaState>emptySet());
    keyEventContext.pastLongPressSentTimeout = false;
    assertEquals('f', keyEventContext.getKeyCode());
    keyEventContext.pastLongPressSentTimeout = true;
    assertEquals('F', keyEventContext.getKeyCode());
  }

  @SmallTest
  public void testGetLongPressKeyCode() {
    KeyEventContext keyEventContext =
        new KeyEventContext(modifiableKey, 0, 0, 0, 100, 100, 1,
                            Collections.<MetaState>emptySet());
    assertEquals('A', keyEventContext.getLongPressKeyCode());
    keyEventContext.pastLongPressSentTimeout = true;
    assertEquals(KeyEntity.INVALID_KEY_CODE, keyEventContext.getLongPressKeyCode());
  }

  @SmallTest
  public void testGetPressedKeyCode() {
    KeyEventContext keyEventContext =
        new KeyEventContext(modifiableKey, 0, 0, 0, 100, 100, 1,
                            Collections.<MetaState>emptySet());
    assertEquals('a', keyEventContext.getPressedKeyCode());
    keyEventContext.flickDirection = Flick.Direction.LEFT;
    assertEquals('a', keyEventContext.getPressedKeyCode());
    keyEventContext.flickDirection = Flick.Direction.RIGHT;
    assertEquals('a', keyEventContext.getPressedKeyCode());
    keyEventContext.pastLongPressSentTimeout = true;
    keyEventContext.flickDirection = Flick.Direction.CENTER;
    assertEquals('a', keyEventContext.getPressedKeyCode());
  }

  @SmallTest
  public void testGetNextMetaState() {
    KeyState onUnmodified = new KeyState(
        "",
        Collections.<MetaState>emptySet(),
        EnumSet.of(MetaState.SHIFT),
        Collections.<MetaState>emptySet(),
        Collections.singletonList(new Flick(Flick.Direction.CENTER, defaultEntity)));
    KeyState onShift = new KeyState(
        "",
        EnumSet.of(MetaState.SHIFT),
        EnumSet.of(MetaState.CAPS_LOCK),
        EnumSet.of(MetaState.SHIFT),
        Collections.singletonList(new Flick(Flick.Direction.CENTER, defaultEntity)));
    KeyState onCapsLock = new KeyState(
        "",
        EnumSet.of(MetaState.CAPS_LOCK),
        Collections.<MetaState>emptySet(),
        EnumSet.of(MetaState.CAPS_LOCK),
        Collections.singletonList(new Flick(Flick.Direction.CENTER, defaultEntity)));
    Key capsLockKey = new Key(0, 0, 10, 10, 0, 0, false, true, Stick.EVEN,
                              DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND,
                              Arrays.asList(onUnmodified, onShift, onCapsLock));
    assertEquals(
        EnumSet.of(MetaState.SHIFT),
        new KeyEventContext(capsLockKey, 0, 0, 0, 100, 100, 1, Collections.<MetaState>emptySet())
            .getNextMetaStates(Collections.<MetaState>emptySet()));
    assertEquals(
        EnumSet.of(MetaState.CAPS_LOCK),
        new KeyEventContext(capsLockKey, 0, 0, 0, 100, 100, 1, EnumSet.of(MetaState.SHIFT))
            .getNextMetaStates(EnumSet.of(MetaState.SHIFT)));
    assertEquals(
        Collections.<MetaState>emptySet(),
        new KeyEventContext(capsLockKey, 0, 0, 0, 100, 100, 1, EnumSet.of(MetaState.CAPS_LOCK))
            .getNextMetaStates(EnumSet.of(MetaState.CAPS_LOCK)));
  }

  @SmallTest
  public void testIsMetaStateToggleEvent() {
    Key key = new Key(0, 0, 30, 50, 0, 0, false, false, Stick.EVEN,
                      DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND,
                      Collections.<KeyState>emptyList());
    KeyEventContext keyEventContext =
        new KeyEventContext(key, 0, 0, 0, 100, 100, 1, Collections.<MetaState>emptySet());
    assertFalse(keyEventContext.isMetaStateToggleEvent());
    Key modifierKey = new Key(0, 0, 30, 50, 0, 0, false, true, Stick.EVEN,
                              DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND,
                              Collections.<KeyState>emptyList());
    KeyEventContext modifierKeyEventContext =
        new KeyEventContext(modifierKey, 0, 0, 0, 100, 100, 1, Collections.<MetaState>emptySet());
    assertTrue(modifierKeyEventContext.isMetaStateToggleEvent());
    modifierKeyEventContext.pastLongPressSentTimeout = true;
    assertFalse(modifierKeyEventContext.isMetaStateToggleEvent());
    modifierKeyEventContext.pastLongPressSentTimeout = false;
    modifierKeyEventContext.flickDirection = Flick.Direction.LEFT;
    assertFalse(modifierKeyEventContext.isMetaStateToggleEvent());
  }

  @SmallTest
  public void testGetCurrentPopUp() {
    KeyEventContext keyEventContext =
        new KeyEventContext(modifiableKey, 0, 0, 0, 100, 100, 1,
                            Collections.<MetaState>emptySet());
    assertSame(defaultPopUp, keyEventContext.getCurrentPopUp().get());
    keyEventContext.flickDirection = Flick.Direction.LEFT;
    assertSame(leftPopUp, keyEventContext.getCurrentPopUp().get());
    keyEventContext.flickDirection = Flick.Direction.RIGHT;
    assertFalse(keyEventContext.getCurrentPopUp().isPresent());
    keyEventContext.pastLongPressSentTimeout = true;
    keyEventContext.flickDirection = Flick.Direction.CENTER;
    assertFalse(keyEventContext.getCurrentPopUp().isPresent());
  }

  @SmallTest
  public void testUpdate() {
    KeyEntity centerEntity = createKeyEntity(1, 'a', 'A');
    KeyEntity leftEntity = createKeyEntity(2, 'b', 'B');
    KeyEntity upEntity = createKeyEntity(3, 'c', 'C');
    KeyEntity rightEntity = createKeyEntity(4, 'd', 'D');
    KeyEntity downEntity = createKeyEntity(5, 'e', 'E');

    KeyState keyState = new KeyState(
        "",
        Collections.<MetaState>emptySet(),
        Collections.<MetaState>emptySet(),
        Collections.<MetaState>emptySet(),
        Arrays.asList(new Flick(Flick.Direction.CENTER, centerEntity),
                      new Flick(Flick.Direction.LEFT, leftEntity),
                      new Flick(Flick.Direction.UP, upEntity),
                      new Flick(Flick.Direction.RIGHT, rightEntity),
                      new Flick(Flick.Direction.DOWN, downEntity)));

    Key key = new Key(0, 0, 16, 16, 0, 0, false, false, Stick.EVEN,
                      DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND,
                      Collections.singletonList(keyState));
    KeyEventContext keyEventContext =
        new KeyEventContext(key, 0, 8, 8, 64, 64, 50, Collections.<MetaState>emptySet());

    assertEquals(Flick.Direction.CENTER, keyEventContext.flickDirection);
    assertEquals(
        TouchEvent.newBuilder()
            .setSourceId(1)
            .addStroke(
                KeyEventContext.createTouchPosition(TouchAction.TOUCH_DOWN, 8, 8, 64, 64, 0))
            .build(),
         keyEventContext.getTouchEvent().get());

    class TestData {
      final int x;
      final int y;
      final long timestamp;
      final boolean pastLongPressSentTimeout;
      final boolean expectedUpdateResult;
      final Flick.Direction expectedDirection;
      final int expectedSourceId;
      final boolean expectedPastLongPressSentTimeout;

      TestData(int x, int y, long timestamp, boolean pastLongPressSentTimeout,
               boolean expectedUpdateResult, Flick.Direction expectedDirection,
               int expectedSourceId, boolean expectedPastLongPressSentTimeout) {
        this.x = x;
        this.y = y;
        this.timestamp = timestamp;
        this.pastLongPressSentTimeout = pastLongPressSentTimeout;
        this.expectedUpdateResult = expectedUpdateResult;
        this.expectedDirection = expectedDirection;
        this.expectedSourceId = expectedSourceId;
        this.expectedPastLongPressSentTimeout = expectedPastLongPressSentTimeout;
      }
    }

    TestData[] testDataList = {
        // Center
        new TestData(4, 4, 10, false, false, Flick.Direction.CENTER, 1, false),

        // Up
        new TestData(8, -16, 20, false, true, Flick.Direction.UP, 3, false),
        new TestData(8, -32, 30, false, false, Flick.Direction.UP, 3, false),

        // Left
        new TestData(-16, 8, 40, false, true, Flick.Direction.LEFT, 2, false),
        new TestData(-32, 8, 50, false, false, Flick.Direction.LEFT, 2, false),

        // Down
        new TestData(8, 32, 60, true, true, Flick.Direction.DOWN, 5, false),
        new TestData(8, 48, 70, true, false, Flick.Direction.DOWN, 5, true),

        // Right
        new TestData(32, 8, 80, true, true, Flick.Direction.RIGHT, 4, false),
        new TestData(48, 8, 90, true, false, Flick.Direction.RIGHT, 4, true),

        // Back to center
        new TestData(8, 8, 100, true, true, Flick.Direction.CENTER, 1, false),
        new TestData(4, 4, 110, true, false, Flick.Direction.CENTER, 1, true),
    };

    for (TestData testData : testDataList) {
      keyEventContext.pastLongPressSentTimeout = testData.pastLongPressSentTimeout;
      assertEquals(testData.expectedUpdateResult,
                   keyEventContext.update(testData.x, testData.y,
                                          TouchAction.TOUCH_MOVE, testData.timestamp));
      assertEquals(testData.expectedDirection, keyEventContext.flickDirection);
      assertEquals(testData.expectedPastLongPressSentTimeout,
                   keyEventContext.pastLongPressSentTimeout);
      assertEquals(
          TouchEvent.newBuilder()
              .setSourceId(testData.expectedSourceId)
              .addStroke(KeyEventContext.createTouchPosition(
                  TouchAction.TOUCH_DOWN, 8, 8, 64, 64, 0))
              .addStroke(KeyEventContext.createTouchPosition(
                  TouchAction.TOUCH_MOVE, testData.x, testData.y, 64, 64, testData.timestamp))
              .build(),
          keyEventContext.getTouchEvent().get());
    }

    // Release the key.
    assertFalse(keyEventContext.update(4, 4, TouchAction.TOUCH_UP, 120));

    TouchEvent expectedEvent = TouchEvent.newBuilder()
        .setSourceId(1)
        .addStroke(KeyEventContext.createTouchPosition(TouchAction.TOUCH_DOWN, 8, 8, 64, 64, 0))
        .addStroke(KeyEventContext.createTouchPosition(TouchAction.TOUCH_UP, 4, 4, 64, 64, 120))
        .build();
    assertEquals(expectedEvent, keyEventContext.getTouchEvent().get());
  }

  @SmallTest
  public void testUpdateNonFlickable() {
    KeyEntity keyEntity = createKeyEntity(256, 'a', 'A');
    KeyState keyState = new KeyState(
        "",
        Collections.<MetaState>emptySet(),
        Collections.<MetaState>emptySet(),
        Collections.<MetaState>emptySet(),
        Collections.singletonList(new Flick(Flick.Direction.CENTER, keyEntity)));
    Key key = new Key(0, 0, 10, 10, 0, 0, false, false, Stick.EVEN,
                      DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND,
                      Collections.singletonList(keyState));

    // For unflickable keys, moving inside the key's region is NOT a flick action
    // regardless of the flick's threshold.
    KeyEventContext keyEventContext =
        new KeyEventContext(key, 0, 5, 5, 100, 100, 9, Collections.<MetaState>emptySet());
    assertFalse(keyEventContext.update(5, 5, TouchAction.TOUCH_DOWN, 0));

    assertEquals(Flick.Direction.CENTER, keyEventContext.flickDirection);
    assertFalse(keyEventContext.update(3, 3, TouchAction.TOUCH_MOVE, 10));
    assertEquals(Flick.Direction.CENTER, keyEventContext.flickDirection);
    assertFalse(keyEventContext.update(3, 8, TouchAction.TOUCH_MOVE, 20));
    assertEquals(Flick.Direction.CENTER, keyEventContext.flickDirection);
    assertFalse(keyEventContext.update(3, 8, TouchAction.TOUCH_MOVE, 30));
    assertEquals(Flick.Direction.CENTER, keyEventContext.flickDirection);

    // If the moving point exits from the key's region, it is recognized as a flick action
    // (cancellation a for non-flickable key).
    assertTrue(keyEventContext.update(-10, 5, TouchAction.TOUCH_MOVE, 40));
    assertEquals(Flick.Direction.LEFT, keyEventContext.flickDirection);

    assertFalse(keyEventContext.getTouchEvent().isPresent());
  }
}
