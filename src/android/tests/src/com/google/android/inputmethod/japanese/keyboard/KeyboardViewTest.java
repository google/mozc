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

import static org.easymock.EasyMock.eq;
import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.expectLastCall;
import static org.easymock.EasyMock.isA;
import static org.easymock.EasyMock.same;

import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.keyboard.Flick.Direction;
import org.mozc.android.inputmethod.japanese.keyboard.Key.Stick;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchAction;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import org.mozc.android.inputmethod.japanese.testing.VisibilityProxy;
import org.mozc.android.inputmethod.japanese.view.DrawableCache;
import org.mozc.android.inputmethod.japanese.view.MozcDrawableFactory;
import org.mozc.android.inputmethod.japanese.view.SkinType;

import android.content.res.Resources;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.Looper;
import android.test.mock.MockResources;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.MotionEvent;
import android.view.View;

import org.easymock.EasyMock;
import org.easymock.IAnswer;

import java.util.Arrays;
import java.util.Collections;
import java.util.EnumSet;
import java.util.List;
import java.util.Map;

/**
 * Unit tests for {@code org.mozc.android.inputmethod.japanese.keyboard.KeyboardView}.
 */
public class KeyboardViewTest extends InstrumentationTestCaseWithMock {

  /**
   * The number of steps of a drag path.
   */
  private static final int STEP_COUNT = 20;

  // Followings are parameters of a dummy key.
  private static final int WIDTH = 50;
  private static final int HEIGHT = 30;
  private static final int HORIZONTAL_GAP = 0;
  private static final int VERTICAL_GAP = 0;

  private KeyboardView view;

  private static boolean touchEvent(KeyboardView view, int action, int x, int y) {
    return touchEvent(view, 0, 0, action, x, y);
  }

  private static boolean touchEvent(
      KeyboardView view, long downTime, long eventTime, int action, int x, int y) {
    MotionEvent e = MotionEvent.obtain(downTime, eventTime, action, x, y, 0);
    try {
      return view.onTouchEvent(e);
    } finally {
      e.recycle();
    }
  }

  /** Emulates dragging from @{code (fromX, fromY)} to @{code (toX, toY)}. */
  private static boolean drag(KeyboardView view, int fromX, int toX, int fromY, int toY) {
    long downTime = MozcUtil.getUptimeMillis();

    boolean result = true;
    result &= touchEvent(view, downTime, downTime, MotionEvent.ACTION_DOWN, fromX, fromY);

    for (int i = 0; i < STEP_COUNT; ++i) {
      int x = fromX + (toX - fromX) * i / STEP_COUNT;
      int y = fromY + (toY - fromY) * i / STEP_COUNT;

      long eventTime = MozcUtil.getUptimeMillis();
      result &= touchEvent(view, downTime, eventTime, MotionEvent.ACTION_MOVE, x, y);
    }

    long eventTime = MozcUtil.getUptimeMillis();
    result &= touchEvent(view, downTime, eventTime, MotionEvent.ACTION_UP, toX, toY);
    return result;
  }

  static KeyState createKeyState(MetaState metaState, int keyCode) {
    KeyEntity entity = new KeyEntity(1, keyCode, KeyEntity.INVALID_KEY_CODE, 0, null, false, null);
    Flick flick = new Flick(Flick.Direction.CENTER, entity);
    return new KeyState(EnumSet.of(metaState), null, Collections.singletonList(flick));
  }

  static Key createKey(int x, int y, int keyCode) {
    return new Key(
        x, y, WIDTH, HEIGHT, HORIZONTAL_GAP, 0, false, false, false, Stick.EVEN,
        Collections.singletonList(createKeyState(MetaState.UNMODIFIED, keyCode)));
  }

  private static Key createSpacer(int x, int y, Stick stick) {
    return new Key(
        x, y, WIDTH, HEIGHT, HORIZONTAL_GAP, 0, false, false, false, stick,
        Collections.<KeyState>emptyList());
  }

  private static Key createKeyWithModifiedState(int x, int y, int keyCode, int modifiedKeyCode) {
    return new Key(
        x, y, WIDTH, HEIGHT, HORIZONTAL_GAP, 0, false, false, false, Stick.EVEN,
        Arrays.asList(createKeyState(MetaState.UNMODIFIED, keyCode),
                      createKeyState(MetaState.CAPS_LOCK, modifiedKeyCode)));
  }

  private static Key createFlickKey(
      int x, int y,
      int centerKeyCode, int leftKeyCode, int rightKeyCode, int upKeyCode, int downKeyCode) {
    Flick center = new Flick(
        Flick.Direction.CENTER,
        new KeyEntity(1, centerKeyCode, KeyEntity.INVALID_KEY_CODE, 0, null, false, null));
    Flick left = new Flick(
        Flick.Direction.LEFT,
        new KeyEntity(1, leftKeyCode, KeyEntity.INVALID_KEY_CODE, 0, null, false, null));
    Flick right = new Flick(
        Flick.Direction.RIGHT,
        new KeyEntity(1, rightKeyCode, KeyEntity.INVALID_KEY_CODE, 0, null, false, null));
    Flick up = new Flick(
        Flick.Direction.UP,
        new KeyEntity(1, upKeyCode, KeyEntity.INVALID_KEY_CODE, 0, null, false, null));
    Flick down = new Flick(
        Flick.Direction.DOWN,
        new KeyEntity(1, downKeyCode, KeyEntity.INVALID_KEY_CODE, 0, null, false, null));
    KeyState keyState = new KeyState(
        EnumSet.of(MetaState.UNMODIFIED), null, Arrays.asList(center, left, right, up, down));
    return new Key(x, y, WIDTH, HEIGHT, HORIZONTAL_GAP, 0, false, false, false, Stick.EVEN,
                   Collections.singletonList(keyState));
  }

  private static Key createModifierKey(int x, int y, int keyCode, MetaState nextMetaState) {
    KeyEntity entity = new KeyEntity(1, keyCode, KeyEntity.INVALID_KEY_CODE, 0, null, false, null);
    Flick flick = new Flick(Flick.Direction.CENTER, entity);
    KeyState keyState = new KeyState(
        EnumSet.of(MetaState.UNMODIFIED), nextMetaState, Collections.singletonList(flick));
    return new Key(x, y, WIDTH, HEIGHT, HORIZONTAL_GAP, 0, false, true, false, Stick.EVEN,
                   Collections.singletonList(keyState));
  }

  /**
   * Creates a dummy keyboard which has only {@code key}.
   * @return a new dummy keyboard.
   */
  private static Keyboard createDummyKeyboard(Key key) {
    Row row = new Row(Collections.singletonList(key), HEIGHT, VERTICAL_GAP);
    return new Keyboard(Collections.singletonList(row), 1);
  }

  private KeyEventHandler createKeyEventHandlerMock() {
    KeyboardActionListener keyboardActionListener = createNiceMock(KeyboardActionListener.class);
    return createMockBuilder(KeyEventHandler.class)
        .withConstructor(Looper.class, KeyboardActionListener.class,
                         int.class, int.class, int.class)
        .withArgs(Looper.myLooper(), keyboardActionListener, 0, 0, 0)
        .createMock();
  }

  private KeyboardViewBackgroundSurface createKeyboardViewBackgroundSurfaceMock() {
    return createMockBuilder(KeyboardViewBackgroundSurface.class)
        .withConstructor(BackgroundDrawableFactory.class, DrawableCache.class)
        .withArgs(new BackgroundDrawableFactory(1f), new DrawableCache(null))
        .createMock();
  }

  @Override
  public void setUp() throws Exception {
    super.setUp();

    view = new KeyboardView(getInstrumentation().getTargetContext());
    view.setKeyboard(createDummyKeyboard(createKey(0, 0, 'a')));
    view.layout(0, 0, 100, 60);
  }

  @Override
  public void tearDown() throws Exception {
    view = null;
    super.tearDown();
  }

  @SmallTest
  public void testFlushPendingKeyEventRecursiveCall() {
    Key targetKey = view.getKeyboard().getRowList().get(0).getKeyList().get(0);
    KeyEventContext keyEventContext =
        new KeyEventContext(targetKey, 0, 0, 0, 100, 60, 1, MetaState.UNMODIFIED);

    KeyEventHandler keyEventHandler = createKeyEventHandlerMock();
    keyEventHandler.cancelDelayedKeyEvent(keyEventContext);
    keyEventHandler.sendKey(
        keyEventContext.getKeyCode(),
        Collections.singletonList(keyEventContext.getTouchEvent()));
    expectLastCall().andAnswer(new IAnswer<Void>() {
      @Override
      public Void answer() throws Throwable {
        // Set Keyboard in this callback which invokes flushPendingKeyEvent again.
        Key key = createKeyWithModifiedState(0, 0, 'a', 'A');
        view.setKeyboard(createDummyKeyboard(key));
        return null;
      }
    });
    keyEventHandler.sendRelease(keyEventContext.getPressedKeyCode());
    replayAll();

    view.setKeyEventHandler(keyEventHandler);

    // Set pressed condition.
    view.metaState = MetaState.CAPS_LOCK;
    Map<Integer, KeyEventContext> keyEventContextMap = view.keyEventContextMap;
    keyEventContextMap.put(0, keyEventContext);

    Key key = createKeyWithModifiedState(0, 0, 'a', 'A');
    view.setKeyboard(createDummyKeyboard(key));

    verifyAll();
  }

  @SmallTest
  public void testOnTouchEvent_press() {
    KeyEventHandler keyEventHandler = createKeyEventHandlerMock();
    keyEventHandler.cancelDelayedKeyEvent(isA(KeyEventContext.class));
    expectLastCall().anyTimes();
    keyEventHandler.maybeStartDelayedKeyEvent(isA(KeyEventContext.class));
    keyEventHandler.sendPress('a');
    replayAll();

    view.setKeyEventHandler(keyEventHandler);

    // By pressing a key, internal pressedKey field should be filled.
    Map<Integer, KeyEventContext> keyEventContextMap = view.keyEventContextMap;
    assertTrue(keyEventContextMap.isEmpty());
    assertTrue(touchEvent(view, MotionEvent.ACTION_DOWN, 25, 15));

    verifyAll();
    assertEquals(1, keyEventContextMap.size());
    assertEquals(Flick.Direction.CENTER,
                 keyEventContextMap.values().iterator().next().flickDirection);
    assertTrue(view.isKeyPressed);
  }

  @SmallTest
  public void testOnTouchEvent_pressModified() {
    KeyEventHandler keyEventHandler = createKeyEventHandlerMock();
    keyEventHandler.cancelDelayedKeyEvent(isA(KeyEventContext.class));
    expectLastCall().anyTimes();
    keyEventHandler.maybeStartDelayedKeyEvent(isA(KeyEventContext.class));
    keyEventHandler.sendPress('A');
    replayAll();

    view.setKeyEventHandler(keyEventHandler);

    Key key = createKeyWithModifiedState(0, 0, 'a', 'A');
    view.setKeyboard(createDummyKeyboard(key));

    // Set modified state.
    view.metaState = MetaState.CAPS_LOCK;

    // By pressing a key, internal pressedKey field should be filled.
    Map<Integer, KeyEventContext> keyEventContextMap = view.keyEventContextMap;
    assertTrue(keyEventContextMap.isEmpty());
    assertTrue(touchEvent(view, MotionEvent.ACTION_DOWN, 25, 15));

    verifyAll();
    assertEquals(1, keyEventContextMap.size());
    assertEquals(Flick.Direction.CENTER,
                 keyEventContextMap.values().iterator().next().flickDirection);
  }

  @SmallTest
  public void testOnTouchEvent_pressModifier() {
    KeyEventHandler keyEventHandler = createKeyEventHandlerMock();
    keyEventHandler.cancelDelayedKeyEvent(isA(KeyEventContext.class));
    expectLastCall().anyTimes();
    keyEventHandler.sendKey(
        'a',
        Arrays.asList(
            // A TouchEvent for the modifier Key.
            TouchEvent.newBuilder()
                .setSourceId(1)
                .addStroke(KeyEventContext.createTouchPosition(
                    TouchAction.TOUCH_DOWN, 25, 15, 100, 60, 0))
                .build(),
            // A TouchEvent for 'a' key.
            TouchEvent.newBuilder()
                .setSourceId(1)
                .addStroke(KeyEventContext.createTouchPosition(
                    TouchAction.TOUCH_DOWN, 100, 60, 100, 60, 0))
                .build()));
    keyEventHandler.sendRelease('a');
    keyEventHandler.maybeStartDelayedKeyEvent(isA(KeyEventContext.class));
    keyEventHandler.sendPress(-1);
    replayAll();
    view.setKeyEventHandler(keyEventHandler);

    view.setKeyboard(createDummyKeyboard(createModifierKey(0, 0, -1, MetaState.CAPS_LOCK)));

    // Set modified state.
    view.metaState = MetaState.UNMODIFIED;

    KeyEventContext pendingKeyEventContext =
        new KeyEventContext(createKey(0, 0, 'a'), 1, 100, 60, 100, 60, 0, MetaState.UNMODIFIED);
    // By pressing a key, the pending events should be flushed and
    // the metaState should be updated.
    Map<Integer, KeyEventContext> keyEventContextMap = view.keyEventContextMap;
    keyEventContextMap.put(1, pendingKeyEventContext);
    assertTrue(touchEvent(view, MotionEvent.ACTION_DOWN, 25, 15));

    verifyAll();
    assertEquals(1, keyEventContextMap.size());
    assertSame(MetaState.CAPS_LOCK, view.metaState);
  }

  @SmallTest
  public void testOnTouchEvent_pressOutside() {
    KeyEventHandler keyEventHandler = createKeyEventHandlerMock();
    replayAll();
    view.setKeyEventHandler(keyEventHandler);

    // Pressing non-key region shouldn't cause filling the pressedKey.
    Map<Integer, KeyEventContext> keyEventContextMap = view.keyEventContextMap;
    assertTrue(keyEventContextMap.isEmpty());
    assertTrue(touchEvent(view, MotionEvent.ACTION_DOWN, 100, -100));

    verifyAll();
    assertTrue(keyEventContextMap.isEmpty());
  }

  @SmallTest
  public void testOnTouchEvent_pressBelowOutside() {
    KeyEventHandler keyEventHandler = createKeyEventHandlerMock();
    keyEventHandler.cancelDelayedKeyEvent(isA(KeyEventContext.class));
    expectLastCall().anyTimes();
    keyEventHandler.maybeStartDelayedKeyEvent(isA(KeyEventContext.class));
    keyEventHandler.sendPress('a');
    replayAll();
    view.setKeyEventHandler(keyEventHandler);

    Map<Integer, KeyEventContext> keyEventContextMap = view.keyEventContextMap;
    assertTrue(keyEventContextMap.isEmpty());
    assertTrue(touchEvent(view, MotionEvent.ACTION_DOWN, 100, 100));

    verifyAll();
    assertEquals(1, keyEventContextMap.size());
    assertEquals(Flick.Direction.CENTER,
                 keyEventContextMap.values().iterator().next().flickDirection);
    assertTrue(view.isKeyPressed);
  }

  @SmallTest
  public void testOnTouchEvent_release() {
    Key targetKey = view.getKeyboard().getRowList().get(0).getKeyList().get(0);
    KeyEventContext keyEventContext =
        new KeyEventContext(targetKey, 0, 0, 0, 100, 60, 1, MetaState.UNMODIFIED);

    KeyEventHandler keyEventHandler = createKeyEventHandlerMock();
    // The listener should receive two events.
    // - onKey event with 'a' code.
    // - onRelease event with 'a' code.
    // in this order.
    keyEventHandler.cancelDelayedKeyEvent(keyEventContext);
    keyEventHandler.sendKey(
        'a',
        Collections.singletonList(TouchEvent.newBuilder()
            .setSourceId(1)
            .addStroke(KeyEventContext.createTouchPosition(
                TouchAction.TOUCH_DOWN, 0, 0, 100, 60, 0))
            .addStroke(KeyEventContext.createTouchPosition(
                TouchAction.TOUCH_UP, 25, 15, 100, 60, 0))
            .build()));
    keyEventHandler.sendRelease('a');
    replayAll();
    view.setKeyEventHandler(keyEventHandler);

    // Set pressed condition.
    Map<Integer, KeyEventContext> keyEventContextMap = view.keyEventContextMap;
    keyEventContextMap.put(0, keyEventContext);

    assertEquals(Flick.Direction.CENTER,
                 keyEventContextMap.values().iterator().next().flickDirection);
    assertTrue(touchEvent(view, MotionEvent.ACTION_UP, 25, 15));

    verifyAll();
    assertTrue(keyEventContextMap.isEmpty());
  }

  @SmallTest
  public void testOnTouchEvent_releaseInvalidKey() {
    class TestData extends Parameter {
      private final Key key;
      private final int deltaX;
      private final int deltaY;
      TestData(Key key, int deltaX, int deltaY) {
        this.key = key;
        this.deltaX = deltaX;
        this.deltaY = deltaY;
      }
    }

    // Center is valid, but other dictions are invalid.
    Key validCenter = createFlickKey(0, 0, 'a',
                                     KeyEntity.INVALID_KEY_CODE,
                                     KeyEntity.INVALID_KEY_CODE,
                                     KeyEntity.INVALID_KEY_CODE,
                                     KeyEntity.INVALID_KEY_CODE);

    int delta = 100;
    TestData[] testDataList = {
        new TestData(createKey(0, 0, KeyEntity.INVALID_KEY_CODE), 0, 0),
        // Try four directions, which are assigned INVALID_KEY_CODE.
        new TestData(validCenter, -delta, 0),
        new TestData(validCenter, +delta, 0),
        new TestData(validCenter, 0, -delta),
        new TestData(validCenter, 0, +delta),
    };

    for (TestData testData : testDataList) {
      resetAll();

      // Prepare a keyboard having a key with invalid key code.
      KeyboardView view = new KeyboardView(getInstrumentation().getTargetContext());
      view.setKeyboard(createDummyKeyboard(testData.key));
      view.layout(0, 0, 100, 60);

      Key targetKey = view.getKeyboard().getRowList().get(0).getKeyList().get(0);
      KeyEventContext keyEventContext =
          new KeyEventContext(targetKey, 0, 0, 0, 100, 60, 1, MetaState.UNMODIFIED);

      KeyEventHandler keyEventHandler = createKeyEventHandlerMock();
      // cancelDelayedKeyEvent is called by each MOVE event.
      keyEventHandler.cancelDelayedKeyEvent(keyEventContext);
      expectLastCall().asStub();
      // Pay no attention the parameter of sendRelease.
      keyEventHandler.sendRelease(EasyMock.anyInt());
      // The handler's sendKey shouldn't be sent.
      replayAll();

      view.setKeyEventHandler(keyEventHandler);

      // Set pressed condition.
      Map<Integer, KeyEventContext> keyEventContextMap = view.keyEventContextMap;
      keyEventContextMap.put(0, keyEventContext);

      int startX = 25;
      int startY = 15;
      assertTrue(testData.toString(),
                 drag(view, startX, startX + testData.deltaX, startY, startY + testData.deltaY));

      verifyAll();
      assertTrue(testData.toString(), keyEventContextMap.isEmpty());
    }
  }

  @SmallTest
  public void testOnTouchEvent_releaseModified() {
    Key key = createKeyWithModifiedState(0, 0, 'a', 'A');
    view.setKeyboard(createDummyKeyboard(key));
    Key targetKey = view.getKeyboard().getRowList().get(0).getKeyList().get(0);
    KeyEventContext keyEventContext =
        new KeyEventContext(targetKey, 0, 0, 0, 100, 60, 1, MetaState.CAPS_LOCK);

    KeyEventHandler keyEventHandler = createKeyEventHandlerMock();
    // The listener should receive two events.
    // - onKey event with 'A' code.
    // - onRelease event with 'A' code.
    // in this order.
    keyEventHandler.cancelDelayedKeyEvent(keyEventContext);
    keyEventHandler.sendKey(
        'A',
        Collections.singletonList(TouchEvent.newBuilder()
            .setSourceId(1)
            .addStroke(KeyEventContext.createTouchPosition(
                TouchAction.TOUCH_DOWN, 0, 0, 100, 60, 0))
            .addStroke(KeyEventContext.createTouchPosition(
                TouchAction.TOUCH_UP, 25, 15, 100, 60, 0))
            .build()));
    keyEventHandler.sendRelease('A');
    replayAll();
    view.setKeyEventHandler(keyEventHandler);

    // Set pressed condition.
    view.metaState = MetaState.CAPS_LOCK;
    Map<Integer, KeyEventContext> keyEventContextMap = view.keyEventContextMap;
    keyEventContextMap.put(0, keyEventContext);

    assertTrue(touchEvent(view, MotionEvent.ACTION_UP, 25, 15));

    verifyAll();
    assertTrue(keyEventContextMap.isEmpty());
    assertSame(MetaState.CAPS_LOCK, view.metaState);
  }

  @SmallTest
  public void testOnTouchEvent_releaseModifierSimple() {
    Key key = createModifierKey(0, 0, -1, MetaState.SHIFT);
    view.setKeyboard(createDummyKeyboard(key));

    KeyEventContext keyEventContext =
        new KeyEventContext(key, 0, 0, 0, 100, 60, 1, MetaState.UNMODIFIED);

    KeyEventHandler keyEventHandler = createKeyEventHandlerMock();
    keyEventHandler.cancelDelayedKeyEvent(keyEventContext);
    keyEventHandler.sendKey(
        -1,
        Collections.singletonList(TouchEvent.newBuilder()
            .setSourceId(1)
            .addStroke(KeyEventContext.createTouchPosition(
                TouchAction.TOUCH_DOWN, 0, 0, 100, 60, 0))
            .addStroke(KeyEventContext.createTouchPosition(
                TouchAction.TOUCH_UP, 25, 15, 100, 60, 0))
            .build()));
    keyEventHandler.sendRelease(-1);
    replayAll();
    view.setKeyEventHandler(keyEventHandler);

    // Set pressed condition.
    view.metaState = MetaState.SHIFT;
    Map<Integer, KeyEventContext> keyEventContextMap = view.keyEventContextMap;
    view.isKeyPressed = false;
    keyEventContextMap.put(0, keyEventContext);

    assertTrue(touchEvent(view, MotionEvent.ACTION_UP, 25, 15));

    verifyAll();
    assertTrue(keyEventContextMap.isEmpty());
    // Simple releasing a modifier key shouldn't change the metaState.
    assertSame(MetaState.SHIFT, view.metaState);
  }

  @SmallTest
  public void testOnTouchEvent_releaseModifierWithOtherKey() {
    Key key = createModifierKey(0, 0, -1, MetaState.SHIFT);
    view.setKeyboard(createDummyKeyboard(key));

    KeyEventContext keyEventContext =
        new KeyEventContext(key, 0, 0, 0, 100, 60, 1, MetaState.UNMODIFIED);

    // Another key event context for 'a' key.
    KeyEventContext keyEventContext2 =
        new KeyEventContext(createKey(0, 0, 'a'), 0, 0, 0, 100, 60, 1, MetaState.SHIFT);

    KeyEventHandler keyEventHandler = createKeyEventHandlerMock();
    // The listener should receive two events.
    // - onKey event with 'A' code.
    // - onRelease event with 'A' code.
    // in this order.
    keyEventHandler.cancelDelayedKeyEvent(keyEventContext2);
    keyEventHandler.sendKey(
        'a',
        Arrays.asList(
            // A TouchEvent for the SHIFT key.
            TouchEvent.newBuilder()
                .setSourceId(1)
                .addStroke(KeyEventContext.createTouchPosition(
                    TouchAction.TOUCH_DOWN, 0, 0, 100, 60, 0))
                .addStroke(KeyEventContext.createTouchPosition(
                    TouchAction.TOUCH_UP, 25, 15, 100, 60, 0))
                .build(),
            // A TouchEvent for 'a' key.
            TouchEvent.newBuilder()
                .setSourceId(1)
                .addStroke(KeyEventContext.createTouchPosition(
                    TouchAction.TOUCH_DOWN, 0, 0, 100, 60, 0))
                .build()));
    keyEventHandler.sendRelease('a');
    keyEventHandler.cancelDelayedKeyEvent(keyEventContext);
    keyEventHandler.sendKey(
        -1,
        Collections.singletonList(TouchEvent.newBuilder()
            .setSourceId(1)
            .addStroke(KeyEventContext.createTouchPosition(
                TouchAction.TOUCH_DOWN, 0, 0, 100, 60, 0))
            .addStroke(KeyEventContext.createTouchPosition(
                TouchAction.TOUCH_UP, 25, 15, 100, 60, 0))
            .build()));
    keyEventHandler.sendRelease(-1);
    replayAll();
    view.setKeyEventHandler(keyEventHandler);

    // Set pressed condition.
    view.metaState = MetaState.SHIFT;
    Map<Integer, KeyEventContext> keyEventContextMap = view.keyEventContextMap;
    keyEventContextMap.put(0, keyEventContext);
    keyEventContextMap.put(1, keyEventContext2);
    view.isKeyPressed = true;

    assertTrue(touchEvent(view, MotionEvent.ACTION_UP, 25, 15));

    verifyAll();
    assertTrue(keyEventContextMap.isEmpty());
    // Simple releasing a modifier key shouldn't change the metaState.
    assertSame(MetaState.UNMODIFIED, view.metaState);
  }

  @SmallTest
  public void testOnTouchEvent_releaseModifierOneTime() {
    Key targetKey = view.getKeyboard().getRowList().get(0).getKeyList().get(0);
    KeyEventContext keyEventContext =
        new KeyEventContext(targetKey, 0, 0, 0, 100, 60, 1, MetaState.UNMODIFIED);

    KeyEventHandler keyEventHandler = createKeyEventHandlerMock();
    keyEventHandler.cancelDelayedKeyEvent(keyEventContext);
    keyEventHandler.sendKey(
        'a',
        Collections.singletonList(TouchEvent.newBuilder()
            .setSourceId(1)
            .addStroke(KeyEventContext.createTouchPosition(
                TouchAction.TOUCH_DOWN, 0, 0, 100, 60, 0))
            .addStroke(KeyEventContext.createTouchPosition(
                TouchAction.TOUCH_UP, 25, 15, 100, 60, 0))
            .build()));
    keyEventHandler.sendRelease('a');
    replayAll();
    view.setKeyEventHandler(keyEventHandler);

    // Set pressed condition.
    view.metaState = MetaState.SHIFT;
    Map<Integer, KeyEventContext> keyEventContextMap = view.keyEventContextMap;
    keyEventContextMap.put(0, keyEventContext);

    assertTrue(touchEvent(view, MotionEvent.ACTION_UP, 25, 15));

    verifyAll();
    assertTrue(keyEventContextMap.isEmpty());
    // Simple releasing a modifier key shouldn't change the metaState.
    assertSame(MetaState.UNMODIFIED, view.metaState);
  }

  @SmallTest
  public void testOnTouchEvent_move() {
    // Moving events shouldn't trigger the listener, except canceling the delayed key event.
    KeyEventHandler keyEventHandler = createKeyEventHandlerMock();
    keyEventHandler.cancelDelayedKeyEvent(isA(KeyEventContext.class));
    expectLastCall().anyTimes();
    replayAll();

    // Following code emulates:
    // - press the center of the key
    // - move to up, which will make internal flickState to UP
    // - move to left, which will make internal flickState to LEFT
    // - move to down, which will make internal flickState to DOWN
    // - move to right, which will make internal flickState to RIGHT
    // - and, move back to the key, which will make internal flickState to CENTER.
    Map<Integer, KeyEventContext> keyEventContextMap = view.keyEventContextMap;
    assertTrue(keyEventContextMap.isEmpty());

    assertTrue(touchEvent(view, MotionEvent.ACTION_DOWN, 25, 15));
    assertEquals(1, keyEventContextMap.size());
    assertEquals(Flick.Direction.CENTER,
                 keyEventContextMap.values().iterator().next().flickDirection);

    view.setKeyEventHandler(keyEventHandler);

    assertTrue(touchEvent(view, MotionEvent.ACTION_MOVE, 25, -85));
    assertEquals(1, keyEventContextMap.size());
    assertEquals(Flick.Direction.UP,
                 keyEventContextMap.values().iterator().next().flickDirection);

    assertTrue(touchEvent(view, MotionEvent.ACTION_MOVE, -75, 15));
    assertEquals(1, keyEventContextMap.size());
    assertEquals(Flick.Direction.LEFT,
                 keyEventContextMap.values().iterator().next().flickDirection);

    assertTrue(touchEvent(view, MotionEvent.ACTION_MOVE, 25, 115));
    assertEquals(1, keyEventContextMap.size());
    assertEquals(Flick.Direction.DOWN,
                 keyEventContextMap.values().iterator().next().flickDirection);

    assertTrue(touchEvent(view, MotionEvent.ACTION_MOVE, 125, 15));
    assertEquals(1, keyEventContextMap.size());
    assertEquals(Flick.Direction.RIGHT,
                 keyEventContextMap.values().iterator().next().flickDirection);

    assertTrue(touchEvent(view, MotionEvent.ACTION_MOVE, 25, 15));
    assertEquals(1, keyEventContextMap.size());
    assertEquals(Flick.Direction.CENTER,
                 keyEventContextMap.values().iterator().next().flickDirection);

    verifyAll();
  }

  @SmallTest
  public void testOnTouchEvent_cancel() {
    Key targetKey = view.getKeyboard().getRowList().get(0).getKeyList().get(0);
    KeyEventContext eventContext =
        new KeyEventContext(targetKey, 0, 0, 0, 100, 60, 1, MetaState.UNMODIFIED);

    KeyEventHandler keyEventHandler = createKeyEventHandlerMock();
    keyEventHandler.cancelDelayedKeyEvent(same(eventContext));
    keyEventHandler.sendCancel();
    replayAll();

    view.setKeyEventHandler(keyEventHandler);

    // Set pressed condition.
    Map<Integer, KeyEventContext> keyEventContextMap = view.keyEventContextMap;
    keyEventContextMap.put(0, eventContext);

    assertTrue(touchEvent(view, MotionEvent.ACTION_CANCEL, 25, 15));
    assertTrue(keyEventContextMap.isEmpty());

    verifyAll();
  }

  @SmallTest
  public void testFlick() {
    view.setKeyboard(createDummyKeyboard(createFlickKey(0, 0, 'a', 'b', 'c', 'd', 'e')));

    view.layout(0, 0, 50, 30);

    int fromX = view.getWidth() / 2;
    int fromY = view.getHeight() / 2;

    class TestData {
      final int toX;
      final int toY;
      final int expectedCode;
      TestData(int toX, int toY, int expectedCode) {
        this.toX = toX;
        this.toY = toY;
        this.expectedCode = expectedCode;
      }
    }
    TestData[] testCases = {
        new TestData(fromX - 75, fromY, 'b'),  // Left
        new TestData(fromX + 75, fromY, 'c'),  // Right
        new TestData(fromX, fromY - 75, 'd'),  // Up
        new TestData(fromX, fromY + 75, 'e'),  // Down
    };

    KeyboardActionListener keyboardActionListener = createStrictMock(KeyboardActionListener.class);
    for (TestData testCase : testCases) {
      resetAll();
      keyboardActionListener.onPress('a');
      keyboardActionListener.onKey(
          eq(testCase.expectedCode), EasyMock.<List<? extends TouchEvent>>notNull());
      keyboardActionListener.onRelease('a');
      replayAll();

      KeyEventHandler keyEventHandler =
          new KeyEventHandler(Looper.myLooper(), keyboardActionListener, 0, 0, 0);

      view.setKeyEventHandler(keyEventHandler);

      assertTrue(view.keyEventContextMap.isEmpty());
      assertTrue(drag(view, fromX, testCase.toX, fromY, testCase.toY));
      assertTrue(view.keyEventContextMap.isEmpty());

      view.setKeyEventHandler(null);
      verifyAll();
    }
  }

  @SmallTest
  public void testFlickSensitivity() {
    view.setKeyboard(createDummyKeyboard(createFlickKey(0, 0, 'a', 'b', 'c', 'd', 'e')));
    view.layout(0, 0, 50, 30);

    Map<Integer, KeyEventContext> keyEventContextMap = view.keyEventContextMap;

    int fromX = view.getWidth() / 2;
    int fromY = view.getHeight() / 2;

    view.setFlickSensitivity(-10);
    assertTrue(touchEvent(view, MotionEvent.ACTION_DOWN, fromX, fromY));
    float threshold1 = keyEventContextMap.values().iterator().next().getFlickThresholdSquared();

    view.resetState();
    view.setFlickSensitivity(0);
    assertTrue(touchEvent(view, MotionEvent.ACTION_DOWN, fromX, fromY));
    float threshold2 = keyEventContextMap.values().iterator().next().getFlickThresholdSquared();

    view.resetState();
    view.setFlickSensitivity(10);
    assertTrue(touchEvent(view, MotionEvent.ACTION_DOWN, fromX, fromY));
    float threshold3 = keyEventContextMap.values().iterator().next().getFlickThresholdSquared();

    assertTrue(0 < threshold3);
    assertTrue(threshold3 <= threshold2);
    assertTrue(threshold2 <= threshold1);
  }

  @SmallTest
  public void testPopUp() {
    final Drawable icon = new ColorDrawable();
    final Drawable flickIcon = new ColorDrawable();
    final int iconResourceId = 1;
    final int flickIconResourceId = 3;

    PopUp popup = new PopUp(iconResourceId, 40, 40, 0, -30);
    PopUp flickPopup = new PopUp(flickIconResourceId, 40, 40, 0, -30);

    // Inject drawables as resources.
    Resources mockResources = new MockResources() {
      @Override
      public Drawable getDrawable(int resourceId) {
        if (resourceId == iconResourceId) {
          return icon;
        }
        if (resourceId == flickIconResourceId) {
          return flickIcon;
        }
        return null;
      }
    };
    DrawableCache drawableCache = new DrawableCache(new MozcDrawableFactory(mockResources));
    VisibilityProxy.setField(view, "drawableCache", drawableCache);

    int x1 = 0;
    int x2 = WIDTH + HORIZONTAL_GAP;
    int x3 = (WIDTH + HORIZONTAL_GAP) * 2;
    int y1 = 0;
    int y2 = HEIGHT + VERTICAL_GAP;
    int y3 = (HEIGHT + VERTICAL_GAP) * 2;

    Key popupKey = new Key(
        x2, y2,
        WIDTH, HEIGHT, HORIZONTAL_GAP, 0, false, false, false, Stick.EVEN,
        Collections.singletonList(new KeyState(
            EnumSet.of(MetaState.UNMODIFIED), null,
            Arrays.asList(
                new Flick(
                    Direction.CENTER,
                    new KeyEntity(1, 'a', KeyEntity.INVALID_KEY_CODE, 0, null, false, popup)),
                new Flick(
                    Direction.LEFT,
                    new KeyEntity(2, 'b', KeyEntity.INVALID_KEY_CODE, 0, null, false,
                                  flickPopup))))));

    Row row1 = new Row(
        Arrays.asList(createKey(x1, y1, 'c'), createKey(x2, y1, 'd'), createKey(x3, y1, 'e')),
        HEIGHT, VERTICAL_GAP);
    Row row2 = new Row(
        Arrays.asList(createKey(x1, y2, 'f'), popupKey, createKey(x3, y2, 'g')),
        HEIGHT, VERTICAL_GAP);
    Row row3 = new Row(
        Arrays.asList(createKey(x1, y3, 'h'), createKey(x2, y3, 'i'), createKey(x3, y3, 'j')),
        HEIGHT, VERTICAL_GAP);
    Keyboard keyboard = new Keyboard(Arrays.asList(row1, row2, row3), 1);
    view.setKeyboard(keyboard);

    // Set mock preview.
    PopUpPreview mockPopUpPreview = createMockBuilder(PopUpPreview.class)
        .withConstructor(View.class, BackgroundDrawableFactory.class, DrawableCache.class)
        .withArgs(view, new BackgroundDrawableFactory(1f), drawableCache)
        .createMock();
    PopUpPreview.Pool popupPreviewPool = VisibilityProxy.getField(view, "popupPreviewPool");
    VisibilityProxy.<List<PopUpPreview>>getField(
        popupPreviewPool, "freeList").add(mockPopUpPreview);
    Handler dismissHandler = new Handler(Looper.myLooper());
    VisibilityProxy.setField(popupPreviewPool, "dismissHandler", dismissHandler);

    // At first, emulate press event.
    mockPopUpPreview.showIfNecessary(popupKey, popup);
    replayAll();

    assertTrue(touchEvent(view, MotionEvent.ACTION_DOWN, 75, 45));
    verifyAll();

    // Then, moving to left.
    resetAll();
    mockPopUpPreview.showIfNecessary(popupKey, flickPopup);
    replayAll();

    assertTrue(touchEvent(view, MotionEvent.ACTION_MOVE, 25, 45));
    verifyAll();

    // Moving to top.
    resetAll();
    mockPopUpPreview.showIfNecessary(popupKey, null);
    replayAll();

    assertTrue(touchEvent(view, MotionEvent.ACTION_MOVE, 75, 15));
    verifyAll();

    // Moving to center again.
    resetAll();
    mockPopUpPreview.showIfNecessary(popupKey, popup);
    replayAll();

    assertTrue(touchEvent(view, MotionEvent.ACTION_MOVE, 75, 45));
    verifyAll();

    // Finally, release.
    resetAll();
    replayAll();

    assertTrue(touchEvent(view, MotionEvent.ACTION_UP, 75, 45));
    verifyAll();

    assertTrue(dismissHandler.hasMessages(0, mockPopUpPreview));
    dismissHandler.removeMessages(0);

    // If popup is disabled, no events should happen.
    resetAll();
    replayAll();

    view.setPopupEnabled(false);
    verifyAll();

    resetAll();
    replayAll();

    assertTrue(touchEvent(view, MotionEvent.ACTION_DOWN, 75, 45));
    verifyAll();
  }

  // Unfortunately, there are no way to test multi-touch events because we can create neither
  // MotionEvent class instances with multi-touch events nor mock instances.
  // So, just skip the tests for those cases.
  // TODO(hidehiko): Figure out how to write those tests, though we may need to get rid of
  //   supporting devices with lower APIs for that purpose...

  @SmallTest
  public void testGetKeyByCoord() {
    Key key1 = createKey(0, 0, 'a');
    Key key2 = createKey(WIDTH * 2, 0, 'b');

    class TestCase {
      final Key spacer;
      final Key expectedLeftHalf;
      final Key expectedRightHalf;

      TestCase(Key spacer, Key expectedLeftHalf, Key expectedRightHalf) {
        this.spacer = spacer;
        this.expectedLeftHalf = expectedLeftHalf;
        this.expectedRightHalf = expectedRightHalf;
      }
    }

    final TestCase[] testCases = {
        new TestCase(createSpacer(WIDTH, 0, Stick.EVEN), key1, key2),
        new TestCase(createSpacer(WIDTH, 0, Stick.LEFT), key1, key1),
        new TestCase(createSpacer(WIDTH, 0, Stick.RIGHT), key2, key2),
    };

    for (TestCase testCase : testCases) {
      Row row = new Row(Arrays.asList(key1, testCase.spacer, key2), HEIGHT, VERTICAL_GAP);
      view.setKeyboard(new Keyboard(Collections.singletonList(row), 1));

      // Center of key1.
      assertSame(key1, view.getKeyByCoord(WIDTH / 2, HEIGHT / 2));

      // Center of key2.
      assertSame(key2, view.getKeyByCoord(WIDTH * 5 / 2, HEIGHT / 2));

      // On a spacer.
      assertSame(testCase.expectedLeftHalf, view.getKeyByCoord(WIDTH * 4 / 3, HEIGHT / 2));
      assertSame(testCase.expectedRightHalf, view.getKeyByCoord(WIDTH * 5 / 3, HEIGHT / 2));

      // Both outside of the keyboard.
      assertSame(key1, view.getKeyByCoord(-WIDTH / 2, HEIGHT / 2));
      assertSame(key2, view.getKeyByCoord(WIDTH * 7 / 2, HEIGHT / 2));
    }
  }

  @SmallTest
  public void testOnTouchEvent_releaseSymbolCusorkeySecondly() {
    Resources res = view.getContext().getResources();
    KeyEventHandler keyEventHandler = createKeyEventHandlerMock();
    int keyCode = res.getInteger(R.integer.key_symbol);
    resetAll();
    Key targetKey = createKey(0, 0, keyCode);
    KeyEventContext keyEventContext =
        new KeyEventContext(targetKey, 0, 0, 0, 100, 60, 1, MetaState.UNMODIFIED);

    // The listener should receive only onRelease event.
    keyEventHandler.cancelDelayedKeyEvent(keyEventContext);
    keyEventHandler.sendRelease(keyCode);
    replayAll();
    view.setKeyEventHandler(keyEventHandler);

    // Set pressed condition.
    Map<Integer, KeyEventContext> keyEventContextMap = view.keyEventContextMap;
    keyEventContextMap.put(0, keyEventContext);

    assertEquals(Flick.Direction.CENTER,
                 keyEventContextMap.values().iterator().next().flickDirection);
    assertTrue(touchEvent(view, MotionEvent.ACTION_POINTER_UP, 0, 0));
    assertTrue(keyEventContextMap.isEmpty());

    view.setKeyEventHandler(null);
    verifyAll();
  }

  @SmallTest
  public void testOnDetachedFromWindow() {
    KeyboardViewBackgroundSurface keyboardViewBackgroundSurface =
        createKeyboardViewBackgroundSurfaceMock();
    keyboardViewBackgroundSurface.reset();
    replayAll();
    VisibilityProxy.setField(view, "backgroundSurface", keyboardViewBackgroundSurface);

    view.onDetachedFromWindow();

    verifyAll();
  }

  @SmallTest
  public void testSetSkinType() {
    KeyboardViewBackgroundSurface keyboardViewBackgroundSurface =
        createKeyboardViewBackgroundSurfaceMock();
    keyboardViewBackgroundSurface.requestUpdateKeyboard(
        isA(Keyboard.class), isA(MetaState.class));

    DrawableCache drawableCache = createMockBuilder(DrawableCache.class)
        .withConstructor(MozcDrawableFactory.class)
        .withArgs(new MozcDrawableFactory(new MockResources()))
        .createMock();
    drawableCache.setSkinType(SkinType.ORANGE_LIGHTGRAY);
    expect(drawableCache.getDrawable(SkinType.ORANGE_LIGHTGRAY.windowBackgroundResourceId))
        .andReturn(null);

    BackgroundDrawableFactory backgroundDrawableFactory =
        createMockBuilder(BackgroundDrawableFactory.class)
            .withConstructor(Float.TYPE)
            .withArgs(1f)
            .createMock();
    backgroundDrawableFactory.setSkinType(SkinType.ORANGE_LIGHTGRAY);
    replayAll();

    VisibilityProxy.setField(view, "backgroundSurface", keyboardViewBackgroundSurface);
    VisibilityProxy.setField(view, "drawableCache", drawableCache);
    VisibilityProxy.setField(view, "backgroundDrawableFactory", backgroundDrawableFactory);

    view.setSkinType(SkinType.ORANGE_LIGHTGRAY);

    verifyAll();
  }
}
