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

import org.mozc.android.inputmethod.japanese.keyboard.Key.Stick;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchAction;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;

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

  @Override
  public void setUp() throws Exception {
    super.setUp();

    defaultPopUp = new PopUp(0, 0, 0, 0, 0);
    leftPopUp = new PopUp(0, 0, 0, 0, 0);

    defaultEntity = new KeyEntity(1, 'a', 'A', 0, null, false, defaultPopUp);
    leftFlickEntity = new KeyEntity(2, 'b', 'B',  0, null, false, leftPopUp);
    modifiedCenterEntity = new KeyEntity(3, 'c', 'C', 0, null, false, null);
    modifiedLeftEntity = new KeyEntity(4, 'd', 'D', 0, null, false, null);

    defaultState = new KeyState(
        EnumSet.of(MetaState.UNMODIFIED), null,
        Arrays.asList(new Flick(Flick.Direction.CENTER, defaultEntity),
                      new Flick(Flick.Direction.LEFT, leftFlickEntity)));
    modifiedState = new KeyState(
        EnumSet.of(MetaState.CAPS_LOCK), null,
        Arrays.asList(new Flick(Flick.Direction.CENTER, modifiedCenterEntity),
                      new Flick(Flick.Direction.LEFT, modifiedLeftEntity)));

    modifiableKey = new Key(0, 0, 16, 16, 0, 0, false, false, false, Stick.EVEN,
                            Arrays.asList(defaultState, modifiedState));
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

  @SmallTest
  public void testIsContained() {
    Key key = new Key(
        0, 0, 30, 50, 0, 0, false, false, false, Stick.EVEN, Collections.<KeyState>emptyList());
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
        new Key(0, 0, 30, 50, 0, 0, false, false, false, Stick.EVEN,
                Collections.<KeyState>emptyList()),
        MetaState.UNMODIFIED));

    // If the key doesn't have flick related data other than CENTER, it is not flickable.
    KeyEntity dummyKeyEntity =
        new KeyEntity(1, 'a', KeyEntity.INVALID_KEY_CODE, 0, null, false, null);
    Flick center = new Flick(Flick.Direction.CENTER, dummyKeyEntity);
    assertFalse(KeyEventContext.isFlickable(
        new Key(0, 0, 30, 50, 0, 0, false, false, false, Stick.EVEN,
                Collections.singletonList(
                    new KeyState(EnumSet.of(MetaState.UNMODIFIED), null,
                                 Collections.singletonList(center)))),
        MetaState.UNMODIFIED));

    // If the key has flick related data other than CENTER, it is flickable.
    Flick left = new Flick(Flick.Direction.LEFT, dummyKeyEntity);
    assertTrue(KeyEventContext.isFlickable(
        new Key(0, 0, 30, 50, 0, 0, false, false, false, Stick.EVEN,
                Collections.singletonList(
                    new KeyState(EnumSet.of(MetaState.UNMODIFIED), null,
                                 Arrays.asList(center, left)))),
        MetaState.UNMODIFIED));
  }

  @SmallTest
  public void testGetKeyEntity() {
    assertSame(defaultEntity,
               KeyEventContext.getKeyEntity(
                   modifiableKey, MetaState.UNMODIFIED, Flick.Direction.CENTER));
    assertSame(leftFlickEntity,
               KeyEventContext.getKeyEntity(
                   modifiableKey, MetaState.UNMODIFIED, Flick.Direction.LEFT));
    assertSame(modifiedCenterEntity,
               KeyEventContext.getKeyEntity(
                   modifiableKey, MetaState.CAPS_LOCK, Flick.Direction.CENTER));
    assertSame(modifiedLeftEntity,
               KeyEventContext.getKeyEntity(
                   modifiableKey, MetaState.CAPS_LOCK, Flick.Direction.LEFT));

    // If a key entity for the flick direction is not specified,
    // null should be returned.
    assertNull(KeyEventContext.getKeyEntity(
        modifiableKey, MetaState.UNMODIFIED, Flick.Direction.RIGHT));
    assertNull(KeyEventContext.getKeyEntity(
        modifiableKey, MetaState.CAPS_LOCK, Flick.Direction.RIGHT));

    Key unmodifiableKey = new Key(0, 0, 0, 0, 0, 0, false, false, false, Stick.EVEN,
                                  Collections.singletonList(defaultState));

    // If a key doesn't have modified state, default state should be used as its fall back.
    assertSame(defaultEntity,
               KeyEventContext.getKeyEntity(
                   unmodifiableKey, MetaState.UNMODIFIED, Flick.Direction.CENTER));
    assertSame(leftFlickEntity,
               KeyEventContext.getKeyEntity(
                   unmodifiableKey, MetaState.UNMODIFIED, Flick.Direction.LEFT));
    assertSame(defaultEntity,
               KeyEventContext.getKeyEntity(
                   unmodifiableKey, MetaState.CAPS_LOCK, Flick.Direction.CENTER));
    assertSame(leftFlickEntity,
               KeyEventContext.getKeyEntity(
                   unmodifiableKey, MetaState.CAPS_LOCK, Flick.Direction.LEFT));

    // If both state is not specified, null should be returned.
    Key spacer = new Key(0, 0, 0, 0, 0, 0, false, false, false, Stick.EVEN,
                         Collections.<KeyState>emptyList());
    assertNull(KeyEventContext.getKeyEntity(spacer, MetaState.UNMODIFIED, Flick.Direction.CENTER));
    assertNull(KeyEventContext.getKeyEntity(spacer, MetaState.CAPS_LOCK, Flick.Direction.CENTER));
  }

  @SmallTest
  public void testGetKeyCode() {
    KeyEventContext keyEventContext =
        new KeyEventContext(modifiableKey, 0, 0, 0, 100, 100, 1, MetaState.UNMODIFIED);
    assertEquals('a', keyEventContext.getKeyCode());
    keyEventContext.flickDirection = Flick.Direction.LEFT;
    assertEquals('b', keyEventContext.getKeyCode());
    keyEventContext.flickDirection = Flick.Direction.RIGHT;
    assertEquals(KeyEntity.INVALID_KEY_CODE, keyEventContext.getKeyCode());
    keyEventContext.longPressSent = true;
    keyEventContext.flickDirection = Flick.Direction.CENTER;
    assertEquals(KeyEntity.INVALID_KEY_CODE, keyEventContext.getKeyCode());
  }

  @SmallTest
  public void testGetLongPressKeyCode() {
    KeyEventContext keyEventContext =
        new KeyEventContext(modifiableKey, 0, 0, 0, 100, 100, 1, MetaState.UNMODIFIED);
    assertEquals('A', keyEventContext.getLongPressKeyCode());
    keyEventContext.longPressSent = true;
    assertEquals(KeyEntity.INVALID_KEY_CODE, keyEventContext.getLongPressKeyCode());
  }

  @SmallTest
  public void testGetPressedKeyCode() {
    KeyEventContext keyEventContext =
        new KeyEventContext(modifiableKey, 0, 0, 0, 100, 100, 1, MetaState.UNMODIFIED);
    assertEquals('a', keyEventContext.getPressedKeyCode());
    keyEventContext.flickDirection = Flick.Direction.LEFT;
    assertEquals('a', keyEventContext.getPressedKeyCode());
    keyEventContext.flickDirection = Flick.Direction.RIGHT;
    assertEquals('a', keyEventContext.getPressedKeyCode());
    keyEventContext.longPressSent = true;
    keyEventContext.flickDirection = Flick.Direction.CENTER;
    assertEquals('a', keyEventContext.getPressedKeyCode());
  }

  @SmallTest
  public void testGetNextMetaState() {
    assertNull(new KeyEventContext(
        modifiableKey, 0, 0, 0, 100, 100, 1, MetaState.UNMODIFIED).getNextMetaState());

    KeyState capsLockState = new KeyState(
        EnumSet.of(MetaState.UNMODIFIED), MetaState.CAPS_LOCK,
        Collections.singletonList(new Flick(Flick.Direction.CENTER, defaultEntity)));
    Key capsLockKey = new Key(0, 0, 10, 10, 0, 0, false, true, false, Stick.EVEN,
                              Collections.singletonList(capsLockState));
    assertSame(
        MetaState.CAPS_LOCK,
        new KeyEventContext(capsLockKey, 0, 0, 0, 100, 100, 1, MetaState.UNMODIFIED)
            .getNextMetaState());
  }

  @SmallTest
  public void testIsMetaStateToggleEvent() {
    Key key = new Key(0, 0, 30, 50, 0, 0, false, false, false, Stick.EVEN,
                      Collections.<KeyState>emptyList());
    KeyEventContext keyEventContext =
        new KeyEventContext(key, 0, 0, 0, 100, 100, 1, MetaState.UNMODIFIED);
    assertFalse(keyEventContext.isMetaStateToggleEvent());
    Key modifierKey = new Key(0, 0, 30, 50, 0, 0, false, true, false, Stick.EVEN,
                              Collections.<KeyState>emptyList());
    KeyEventContext modifierKeyEventContext =
        new KeyEventContext(modifierKey, 0, 0, 0, 100, 100, 1, MetaState.UNMODIFIED);
    assertTrue(modifierKeyEventContext.isMetaStateToggleEvent());
    modifierKeyEventContext.longPressSent = true;
    assertFalse(modifierKeyEventContext.isMetaStateToggleEvent());
    modifierKeyEventContext.longPressSent = false;
    modifierKeyEventContext.flickDirection = Flick.Direction.LEFT;
    assertFalse(modifierKeyEventContext.isMetaStateToggleEvent());
  }

  @SmallTest
  public void testGetCurrentPopUp() {
    KeyEventContext keyEventContext =
        new KeyEventContext(modifiableKey, 0, 0, 0, 100, 100, 1, MetaState.UNMODIFIED);
    assertSame(defaultPopUp, keyEventContext.getCurrentPopUp());
    keyEventContext.flickDirection = Flick.Direction.LEFT;
    assertSame(leftPopUp, keyEventContext.getCurrentPopUp());
    keyEventContext.flickDirection = Flick.Direction.RIGHT;
    assertNull(keyEventContext.getCurrentPopUp());
    keyEventContext.longPressSent = true;
    keyEventContext.flickDirection = Flick.Direction.CENTER;
    assertNull(keyEventContext.getCurrentPopUp());
  }

  @SmallTest
  public void testUpdate() {
    KeyEntity centerEntity = new KeyEntity(1, 'a', 'A', 0, null, false, null);
    KeyEntity leftEntity = new KeyEntity(2, 'b', 'B', 0, null, false, null);
    KeyEntity upEntity = new KeyEntity(3, 'c', 'C', 0, null, false, null);
    KeyEntity rightEntity = new KeyEntity(4, 'd', 'D', 0, null, false, null);
    KeyEntity downEntity = new KeyEntity(5, 'e', 'E', 0, null, false, null);

    KeyState keyState = new KeyState(
        EnumSet.of(MetaState.UNMODIFIED), null,
        Arrays.asList(new Flick(Flick.Direction.CENTER, centerEntity),
                      new Flick(Flick.Direction.LEFT, leftEntity),
                      new Flick(Flick.Direction.UP, upEntity),
                      new Flick(Flick.Direction.RIGHT, rightEntity),
                      new Flick(Flick.Direction.DOWN, downEntity)));

    Key key = new Key(0, 0, 16, 16, 0, 0, false, false, false, Stick.EVEN,
                      Collections.singletonList(keyState));
    KeyEventContext keyEventContext =
        new KeyEventContext(key, 0, 8, 8, 64, 64, 50, MetaState.UNMODIFIED);

    assertEquals(Flick.Direction.CENTER, keyEventContext.flickDirection);
    assertEquals(
        TouchEvent.newBuilder()
            .setSourceId(1)
            .addStroke(
                KeyEventContext.createTouchPosition(TouchAction.TOUCH_DOWN, 8, 8, 64, 64, 0))
            .build(),
         keyEventContext.getTouchEvent());

    class TestData {
      final int x;
      final int y;
      final long timestamp;
      final boolean expectedUpdateResult;
      final Flick.Direction expectedDirection;
      final int expectedSourceId;

      TestData(int x, int y, long timestamp,
               boolean expectedUpdateResult, Flick.Direction expectedDirection,
               int expectedSourceId) {
        this.x = x;
        this.y = y;
        this.timestamp = timestamp;
        this.expectedUpdateResult = expectedUpdateResult;
        this.expectedDirection = expectedDirection;
        this.expectedSourceId = expectedSourceId;
      }
    }

    TestData[] testDataList = {
        // Center
        new TestData(4, 4, 10, false, Flick.Direction.CENTER, 1),

        // Up
        new TestData(8, -16, 20, true, Flick.Direction.UP, 3),
        new TestData(8, -32, 30, false, Flick.Direction.UP, 3),

        // Left
        new TestData(-16, 8, 40, true, Flick.Direction.LEFT, 2),
        new TestData(-32, 8, 50, false, Flick.Direction.LEFT, 2),

        // Down
        new TestData(8, 32, 60, true, Flick.Direction.DOWN, 5),
        new TestData(8, 48, 70, false, Flick.Direction.DOWN, 5),

        // Right
        new TestData(32, 8, 80, true, Flick.Direction.RIGHT, 4),
        new TestData(48, 8, 90, false, Flick.Direction.RIGHT, 4),

        // Back to center
        new TestData(8, 8, 100, true, Flick.Direction.CENTER, 1),
        new TestData(4, 4, 110, false, Flick.Direction.CENTER, 1),
    };

    for (TestData testData : testDataList) {
      assertEquals(testData.expectedUpdateResult,
                   keyEventContext.update(testData.x, testData.y,
                                          TouchAction.TOUCH_MOVE, testData.timestamp));
      assertEquals(testData.expectedDirection, keyEventContext.flickDirection);
      assertEquals(
          TouchEvent.newBuilder()
              .setSourceId(testData.expectedSourceId)
              .addStroke(KeyEventContext.createTouchPosition(
                  TouchAction.TOUCH_DOWN, 8, 8, 64, 64, 0))
              .addStroke(KeyEventContext.createTouchPosition(
                  TouchAction.TOUCH_MOVE, testData.x, testData.y, 64, 64, testData.timestamp))
              .build(),
          keyEventContext.getTouchEvent());
    }

    // Release the key.
    assertFalse(keyEventContext.update(4, 4, TouchAction.TOUCH_UP, 120));

    TouchEvent expectedEvent = TouchEvent.newBuilder()
        .setSourceId(1)
        .addStroke(KeyEventContext.createTouchPosition(TouchAction.TOUCH_DOWN, 8, 8, 64, 64, 0))
        .addStroke(KeyEventContext.createTouchPosition(TouchAction.TOUCH_UP, 4, 4, 64, 64, 120))
        .build();
    assertEquals(expectedEvent, keyEventContext.getTouchEvent());
  }

  @SmallTest
  public void testUpdateNonFlickable() {
    KeyEntity keyEntity = new KeyEntity(256, 'a', 'A', 0, null, false, null);
    KeyState keyState = new KeyState(
        EnumSet.of(MetaState.UNMODIFIED), null,
        Collections.singletonList(new Flick(Flick.Direction.CENTER, keyEntity)));
    Key key = new Key(0, 0, 10, 10, 0, 0, false, false, false, Stick.EVEN,
                      Collections.singletonList(keyState));

    // For unflickable keys, moving inside the key's region is NOT a flick action
    // regardless of the flick's threshold.
    KeyEventContext keyEventContext =
        new KeyEventContext(key, 0, 5, 5, 100, 100, 9, MetaState.UNMODIFIED);
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

    assertNull(keyEventContext.getTouchEvent());
  }
}
