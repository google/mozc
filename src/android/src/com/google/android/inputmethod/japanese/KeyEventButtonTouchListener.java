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

package org.mozc.android.inputmethod.japanese;

import org.mozc.android.inputmethod.japanese.keyboard.Flick;
import org.mozc.android.inputmethod.japanese.keyboard.Flick.Direction;
import org.mozc.android.inputmethod.japanese.keyboard.Key;
import org.mozc.android.inputmethod.japanese.keyboard.Key.Stick;
import org.mozc.android.inputmethod.japanese.keyboard.KeyEntity;
import org.mozc.android.inputmethod.japanese.keyboard.KeyEventContext;
import org.mozc.android.inputmethod.japanese.keyboard.KeyEventHandler;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchAction;

import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnTouchListener;

import java.util.Collections;
import java.util.EnumSet;

/**
 * This class is an event listener for a view which sends a key to MozcServer.
 *
 * Note that currently, we assume all key are repeatable based on our
 * requirements. We can easily support non repeatable key if necessary.
 *
 */
public class KeyEventButtonTouchListener implements OnTouchListener {
  private final int sourceId;
  private final int keyCode;
  private KeyEventHandler keyEventHandler = null;
  private KeyEventContext keyEventContext = null;

  /**
   * This is exported as protected for testing.
   */
  protected KeyEventButtonTouchListener(int sourceId, int keyCode) {
    this.sourceId = sourceId;
    this.keyCode = keyCode;
  }

  /**
   * Resets the internal state of this listener.
   * This is exported as protected for testing.
   */
  protected void reset() {
    KeyEventHandler keyEventHandler = this.keyEventHandler;
    KeyEventContext keyEventContext = this.keyEventContext;
    if (keyEventHandler != null && keyEventContext != null) {
      keyEventHandler.cancelDelayedKeyEvent(keyEventContext);
    }
    this.keyEventContext = null;
  }

  /**
   * Sets a new event handler to this instance.
   * This is exported as protected for testing.
   */
  protected void setKeyEventHandler(KeyEventHandler keyEventHandler) {
    // We'll detach the current event context from the old key event handler.
    reset();
    this.keyEventHandler = keyEventHandler;
  }

  /**
   * Creates a (pseudo) {@link Key} instance based on the given {@code button} and its
   * {@code keyCode}.
   * This is exported as package private for testing.
   */
  static Key createKey(View button, int sourceId, int keyCode) {
    KeyEntity keyEntity =
        new KeyEntity(sourceId, keyCode, KeyEntity.INVALID_KEY_CODE, 0, null, false, null);
    Flick flick = new Flick(Direction.CENTER, keyEntity);
    KeyState keyState =
        new KeyState(EnumSet.of(MetaState.UNMODIFIED), null, Collections.singletonList(flick));
    // Now, we support repetable keys only.
    return new Key(0, 0, button.getWidth(), button.getHeight(), 0, 0,
                   true, false, false, Stick.EVEN, Collections.singletonList(keyState));
  }

  private static KeyEventContext createKeyEventContext(
      View button, int sourceId, int keyCode, float x, float y) {
    Key key = createKey(button, sourceId, keyCode);
    View parent = View.class.cast(button.getParent());
    return new KeyEventContext(
        key, 0, x, y, parent.getWidth(), parent.getHeight(), 0, MetaState.UNMODIFIED);
  }

  /**
   * Handles the button view's down event.
   * This is exported as package private for testing purpose.
   */
  void onDown(View button, float x, float y) {
    button.setPressed(true);

    KeyEventHandler keyEventHandler = this.keyEventHandler;
    if (keyEventHandler != null) {
      // At first, cancel pending repeat events and overwrite it by the given keyCode.
      KeyEventContext oldKeyEventContext = this.keyEventContext;
      if (oldKeyEventContext != null) {
        keyEventHandler.cancelDelayedKeyEvent(keyEventContext);
      }

      // Then create and start running new event. Note that we don't need create it
      // if keyEventHandler is not set, because it will be reset in setKeyEventHandler
      // when a new keyEventHandler is set.
      KeyEventContext newContext = createKeyEventContext(button, sourceId, keyCode, x, y);
      newContext.update(x, y, TouchAction.TOUCH_DOWN, 0);
      keyEventHandler.maybeStartDelayedKeyEvent(newContext);
      keyEventHandler.sendPress(newContext.getPressedKeyCode());
      this.keyEventContext = newContext;
    }
  }

  /**
   * Handles the button view's up event.
   * This is exported as package private for testing purpose.
   */
  void onUp(View button, float x, float y, long timestamp) {
    button.setPressed(false);

    KeyEventHandler keyEventHandler = this.keyEventHandler;
    KeyEventContext keyEventContext = this.keyEventContext;
    if (keyEventHandler != null && keyEventContext != null) {
      keyEventContext.update(x, y, TouchAction.TOUCH_UP, timestamp);
      keyEventHandler.cancelDelayedKeyEvent(keyEventContext);
      keyEventHandler.sendKey(keyEventContext.getKeyCode(),
                              Collections.singletonList(keyEventContext.getTouchEvent()));
      keyEventHandler.sendRelease(keyEventContext.getPressedKeyCode());
    }
    this.keyEventContext = null;
  }

  /**
   * Handles the button view's move event.
   * This is exported as package private for testing purpose.
   */
  void onMove(float x, float y, long timestamp) {
    KeyEventContext keyEventContext = this.keyEventContext;
    if (keyEventContext != null &&
        keyEventContext.update(x, y, TouchAction.TOUCH_MOVE, timestamp)) {
      // Here, the user's touching point is moved to outside of the button view,
      // so cancel the repeating key event.
      KeyEventHandler keyEventHandler = this.keyEventHandler;
      if (keyEventHandler != null) {
        keyEventHandler.cancelDelayedKeyEvent(keyEventContext);
      }
      this.keyEventContext = null;
    }
  }

  @Override
  public boolean onTouch(View button, MotionEvent event) {
    switch (event.getAction() & MotionEvent.ACTION_MASK) {
      case MotionEvent.ACTION_DOWN:
        onDown(button, event.getX(), event.getY());
        break;
      case MotionEvent.ACTION_UP:
        onUp(button, event.getX(), event.getY(), event.getEventTime() - event.getDownTime());
        break;
      case MotionEvent.ACTION_MOVE:
        onMove(event.getX(), event.getY(), event.getEventTime() - event.getDownTime());
        break;
      default:
        // Ignore other events intentionally.
    }

    // Consume all the events here in order not to propagate the event to views behind the button.
    return true;
  }
}
