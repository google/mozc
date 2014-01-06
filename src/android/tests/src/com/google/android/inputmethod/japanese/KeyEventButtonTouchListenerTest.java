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

package org.mozc.android.inputmethod.japanese;

import static org.easymock.EasyMock.eq;
import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.isA;

import org.mozc.android.inputmethod.japanese.keyboard.Key;
import org.mozc.android.inputmethod.japanese.keyboard.Key.Stick;
import org.mozc.android.inputmethod.japanese.keyboard.KeyEventContext;
import org.mozc.android.inputmethod.japanese.keyboard.KeyEventHandler;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.keyboard.KeyboardActionListener;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchAction;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;

import android.content.Context;
import android.os.Looper;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.easymock.EasyMock;

import java.util.Collections;
import java.util.List;

/**
 */
public class KeyEventButtonTouchListenerTest extends InstrumentationTestCaseWithMock {
  private KeyEventHandler createKeyEventHandlerMock() {
    KeyboardActionListener keyboardActionListener = createMock(KeyboardActionListener.class);
    return createMockBuilder(KeyEventHandler.class)
        .withConstructor(
            Looper.class, KeyboardActionListener.class, int.class, int.class, int.class)
        .withArgs(Looper.myLooper(), keyboardActionListener, 0, 0, 0)
        .createMock();
  }

  private KeyEventContext createKeyEventContextMock() {
    Key key = new Key(0, 0, 0, 0, 0, 0, false, false, false, Stick.EVEN,
        Collections.<KeyState>emptyList());
    return createMockBuilder(KeyEventContext.class)
        .withConstructor(Key.class, int.class, float.class, float.class,
                         int.class, int.class, float.class, MetaState.class)
        .withArgs(key, 0, 0f, 0f, 0, 0, 0f, MetaState.UNMODIFIED)
        .createMock();
  }

  @SmallTest
  public void testCreateKey() {
    View view = new View(getInstrumentation().getTargetContext());
    view.layout(10, 15, 50, 45);
    Key key = KeyEventButtonTouchListener.createKey(view, 1, 10);
    assertEquals(0, key.getX());
    assertEquals(0, key.getY());
    assertEquals(40, key.getWidth());
    assertEquals(30, key.getHeight());
  }

  @SmallTest
  public void testReset() {
    KeyEventButtonTouchListener keyEventButtonTouchListener =
        new KeyEventButtonTouchListener(1, 10);
    KeyEventHandler keyEventHandler = createKeyEventHandlerMock();
    keyEventButtonTouchListener.setKeyEventHandler(keyEventHandler);

    View view = new View(getInstrumentation().getTargetContext());
    view.layout(0, 0, 50, 50);
    Key key = KeyEventButtonTouchListener.createKey(view, 1, 10);
    KeyEventContext keyEventContext =
        new KeyEventContext(key, 0, 0, 0, 0, 100, 100, MetaState.UNMODIFIED);

    keyEventHandler.cancelDelayedKeyEvent(keyEventContext);
    replayAll();

    keyEventButtonTouchListener.keyEventContext = keyEventContext;
    keyEventButtonTouchListener.reset();

    verifyAll();
    assertNull(keyEventButtonTouchListener.keyEventContext);
  }

  @SmallTest
  public void testOnDown() {
    KeyEventButtonTouchListener keyEventButtonTouchListener =
        new KeyEventButtonTouchListener(1, 10);
    KeyEventHandler keyEventHandler = createKeyEventHandlerMock();
    keyEventButtonTouchListener.setKeyEventHandler(keyEventHandler);

    Context context = getInstrumentation().getContext();
    ViewGroup parent = new FrameLayout(context);
    parent.layout(0, 0, 100, 100);
    View view = new View(context);
    parent.addView(view);
    view.layout(0, 0, 50, 50);
    Key key = KeyEventButtonTouchListener.createKey(view, 1, 10);
    KeyEventContext keyEventContext =
        new KeyEventContext(key, 0, 0, 0, 0, 100, 100, MetaState.UNMODIFIED);
    keyEventButtonTouchListener.keyEventContext = keyEventContext;

    // For onDown, first, cancelDelayedKeyEvent should be invoked to cancel old
    // event, then maybeStartDelayedKeyEvent and sendPress should follow.
    keyEventHandler.cancelDelayedKeyEvent(keyEventContext);
    keyEventHandler.maybeStartDelayedKeyEvent(isA(KeyEventContext.class));
    keyEventHandler.sendPress(10);
    replayAll();

    keyEventButtonTouchListener.onDown(view, 0, 0);

    verifyAll();
    assertNotSame(keyEventContext, keyEventButtonTouchListener.keyEventContext);
    assertTrue(view.isPressed());
  }

  @SmallTest
  public void testOnUp() {
    KeyEventButtonTouchListener keyEventButtonTouchListener =
        new KeyEventButtonTouchListener(1, 10);
    KeyEventHandler keyEventHandler = createKeyEventHandlerMock();
    keyEventButtonTouchListener.setKeyEventHandler(keyEventHandler);

    View view = new View(getInstrumentation().getTargetContext());
    view.layout(0, 0, 50, 50);
    Key key = KeyEventButtonTouchListener.createKey(view, 1, 10);
    KeyEventContext keyEventContext =
        new KeyEventContext(key, 0, 0, 0, 0, 100, 100, MetaState.UNMODIFIED);
    keyEventButtonTouchListener.keyEventContext = keyEventContext;

    keyEventHandler.cancelDelayedKeyEvent(keyEventContext);
    keyEventHandler.sendKey(eq(10), EasyMock.<List<TouchEvent>>notNull());
    keyEventHandler.sendRelease(10);
    replayAll();

    view.setPressed(true);
    keyEventButtonTouchListener.onUp(view, 25, 25, 100);

    verifyAll();
    assertNull(keyEventButtonTouchListener.keyEventContext);
    assertFalse(view.isPressed());
  }

  @SmallTest
  public void testOnMove() {
    KeyEventButtonTouchListener keyEventButtonTouchListener =
        new KeyEventButtonTouchListener(1, 10);

    KeyEventHandler keyEventHandler = createKeyEventHandlerMock();
    keyEventButtonTouchListener.setKeyEventHandler(keyEventHandler);

    KeyEventContext keyEventContext = createKeyEventContextMock();
    keyEventButtonTouchListener.keyEventContext = keyEventContext;

    expect(keyEventContext.update(10, 20, TouchAction.TOUCH_MOVE, 100)).andReturn(true);
    keyEventHandler.cancelDelayedKeyEvent(keyEventContext);
    replayAll();

    keyEventButtonTouchListener.onMove(10, 20, 100);

    verifyAll();
    assertNull(keyEventButtonTouchListener.keyEventContext);
  }
}
