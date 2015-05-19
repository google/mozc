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

import org.mozc.android.inputmethod.japanese.keyboard.Flick.Direction;
import org.mozc.android.inputmethod.japanese.keyboard.Key.Stick;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchAction;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;

import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.test.suitebuilder.annotation.SmallTest;

import java.util.Collections;
import java.util.EnumSet;

/**
 */
public class KeyEventHandlerTest extends InstrumentationTestCaseWithMock {
  private static Key createDummyKey(int keyCode, int longPressKeyCode, boolean isRepeatable) {
    KeyEntity keyEntity = new KeyEntity(1, keyCode, longPressKeyCode, 0, null, false, null);
    Flick flick = new Flick(Direction.CENTER, keyEntity);
    KeyState keyState =
        new KeyState(EnumSet.of(MetaState.UNMODIFIED), null, Collections.singletonList(flick));
    return new Key(0, 0, 0, 0, 0, 0, isRepeatable, false, false, Stick.EVEN,
                   Collections.singletonList(keyState));
  }

  private static KeyEventContext createDummyKeyEventContext(Key key) {
    return new KeyEventContext(key, 0, 0, 0, 100, 100, 0, MetaState.UNMODIFIED);
  }

  @SmallTest
  public void testHandleMessage_repeatKey() {
    KeyboardActionListener keyboardActionListener = createMock(KeyboardActionListener.class);
    keyboardActionListener.onKey(
        'a',
        Collections.singletonList(TouchEvent.newBuilder()
            .setSourceId(1)
            .addStroke(KeyEventContext.createTouchPosition(
                TouchAction.TOUCH_DOWN, 0, 0, 100, 60, 0))
            .build()));
    replayAll();

    Key key = createDummyKey('a', KeyEntity.INVALID_KEY_CODE, true);
    KeyEventContext keyEventContext = createDummyKeyEventContext(key);

    KeyEventHandler keyEventHandler =
        new KeyEventHandler(Looper.myLooper(), keyboardActionListener, 0, 0, 0);
    Handler handler = keyEventHandler.handler;
    Message message =
        handler.obtainMessage(KeyEventHandler.REPEAT_KEY, 'a', 0, keyEventContext);
    try {
      keyEventHandler.handleMessage(message);

      verifyAll();
      assertTrue(handler.hasMessages(KeyEventHandler.REPEAT_KEY, keyEventContext));
    } finally {
      handler.removeMessages(KeyEventHandler.REPEAT_KEY);
    }
  }

  @SmallTest
  public void testHandlerMessage_longPressKey() {
    KeyboardActionListener keyboardActionListener = createMock(KeyboardActionListener.class);
    keyboardActionListener.onKey(
        'A',
        Collections.singletonList(TouchEvent.newBuilder()
            .setSourceId(1)
            .addStroke(KeyEventContext.createTouchPosition(
                TouchAction.TOUCH_DOWN, 0, 0, 100, 60, 0))
            .build()));
    replayAll();

    Key key = createDummyKey('a', 'A', false);
    KeyEventContext keyEventContext = createDummyKeyEventContext(key);

    KeyEventHandler keyEventHandler =
        new KeyEventHandler(Looper.myLooper(), keyboardActionListener, 0, 0, 0);
    Handler handler = keyEventHandler.handler;
    Message message = handler.obtainMessage(
        KeyEventHandler.LONG_PRESS_KEY, 'A', 0, keyEventContext);
    keyEventHandler.handleMessage(message);

    verifyAll();
    assertTrue(keyEventContext.longPressSent);
    assertFalse(handler.hasMessages(KeyEventHandler.LONG_PRESS_KEY, keyEventContext));
  }

  @SmallTest
  public void testMaybeStartDelayedKeyEvent_repeatKey() {
    Key key = createDummyKey('a', KeyEntity.INVALID_KEY_CODE, true);
    KeyEventContext keyEventContext = createDummyKeyEventContext(key);

    KeyboardActionListener keyboardActionListener = createNiceMock(KeyboardActionListener.class);
    replayAll();
    KeyEventHandler keyEventHandler =
        new KeyEventHandler(Looper.myLooper(), keyboardActionListener, 0, 0, 0);
    keyEventHandler.maybeStartDelayedKeyEvent(keyEventContext);

    Handler handler = keyEventHandler.handler;
    assertTrue(handler.hasMessages(KeyEventHandler.REPEAT_KEY, keyEventContext));
    handler.removeMessages(KeyEventHandler.REPEAT_KEY);
  }

  @SmallTest
  public void testMaybeStartDelayedKeyEvent_longPressKey() {
    Key key = createDummyKey('a', 'A', false);
    KeyEventContext keyEventContext = createDummyKeyEventContext(key);

    KeyboardActionListener keyboardActionListener = createNiceMock(KeyboardActionListener.class);
    replayAll();
    KeyEventHandler keyEventHandler =
        new KeyEventHandler(Looper.myLooper(), keyboardActionListener, 0, 0, 0);
    keyEventHandler.maybeStartDelayedKeyEvent(keyEventContext);

    Handler handler = keyEventHandler.handler;
    assertTrue(handler.hasMessages(KeyEventHandler.LONG_PRESS_KEY, keyEventContext));
    handler.removeMessages(KeyEventHandler.LONG_PRESS_KEY);
  }
}
