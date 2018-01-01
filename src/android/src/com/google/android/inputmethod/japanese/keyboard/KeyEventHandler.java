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

import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Preconditions;

import android.os.Handler;
import android.os.Looper;
import android.os.Message;

import java.util.Collections;
import java.util.List;

/**
 * A class to handle key events including repeat-keys/long-press-keys.
 *
 */
public class KeyEventHandler implements Handler.Callback {

  // A dummy argument which is passed to the callback's message.
  private static final int DUMMY_ARG = 0;

  // Keys to figure out what message is sent in the callback.
  @VisibleForTesting static final int REPEAT_KEY = 1;
  @VisibleForTesting static final int LONG_PRESS_KEY = 2;

  @VisibleForTesting final Handler handler;
  private final KeyboardActionListener keyboardActionListener;

  // Following three variables representing delay time are in milliseconds.
  private final int repeatKeyDelay;
  private final int repeatKeyInterval;
  private final int longPressKeyDelay;

  /**
   * Note that we expect that the {@code looper} is UI thread's looper when it's working on
   * the production. It is injectable for testing purpose.
   */
  public KeyEventHandler(
      Looper looper, KeyboardActionListener keyboardActionListener,
      int repeatKeyDelay, int repeatKeyInterval, int longPressKeyDelay) {
    this.handler = new Handler(Preconditions.checkNotNull(looper), this);
    this.keyboardActionListener = Preconditions.checkNotNull(keyboardActionListener);
    this.repeatKeyDelay = repeatKeyDelay;
    this.repeatKeyInterval = repeatKeyInterval;
    this.longPressKeyDelay = longPressKeyDelay;
  }

  @Override
  public boolean handleMessage(Message message) {
    KeyEventContext context = KeyEventContext.class.cast(message.obj);
    switch (message.what) {
      case REPEAT_KEY: {
        handleMessageRepeatKey(context);
        break;
      }
      case LONG_PRESS_KEY:
        handleMessageLongPress(context);
        break;
    }

    return true;
  }

  private void handleMessageRepeatKey(KeyEventContext context) {
    int keyCode = context.getPressedKeyCode();
    // TODO(hsumita): confirm that we can put null as a touch event or not.
    keyboardActionListener.onKey(
        keyCode, Collections.singletonList(context.getTouchEvent().orNull()));
    Message newMessage = handler.obtainMessage(REPEAT_KEY, keyCode, DUMMY_ARG, context);
    handler.sendMessageDelayed(newMessage, repeatKeyInterval);
  }

  /**
   * Does the things which should be done when long-press operation is done.
   * <p>
   * This is public because this is called from KeyboardView directory in order to implement
   * accessibility feature.
   */
  public void handleMessageLongPress(KeyEventContext context) {
    int keyCode = context.getLongPressKeyCode();
    if (context.isLongPressTimeoutTrigger()) {
      // TODO(hsumita): confirm that we can put null as a touch event or not.
      keyboardActionListener.onKey(
          keyCode, Collections.singletonList(context.getTouchEvent().orNull()));
    }
    // Callback a function if present then flip the flag for long-press timeout.
    // If isLongPressTimeoutTrigger is true, key-code for long-press has already been sent.
    // If false, touch-up event for the context will send long-press key code.
    if (context.longPressCallback.isPresent()) {
      context.longPressCallback.get().run();
    }
    context.pastLongPressSentTimeout = true;
  }

  public void sendPress(int keyCode) {
    keyboardActionListener.onPress(keyCode);
  }

  public void sendRelease(int keyCode) {
    keyboardActionListener.onRelease(keyCode);
  }

  public void sendKey(int keyCode, List<TouchEvent> touchEventList) {
    keyboardActionListener.onKey(keyCode, touchEventList);
  }

  public void sendCancel() {
    keyboardActionListener.onCancel();
  }

  private void startDelayedKeyEventInternal(int what, KeyEventContext context, int delay) {
    Message message = handler.obtainMessage(what, DUMMY_ARG, DUMMY_ARG, context);
    handler.sendMessageDelayed(message, delay);
  }

  /**
   * Maybe trigger repeating onKey events or a long press key event,
   * based on the given {@code context}.
   */
  public void maybeStartDelayedKeyEvent(KeyEventContext context) {
    Key key = Preconditions.checkNotNull(context).key;
    if (key.isRepeatable()) {
      int keyCode = context.getPressedKeyCode();
      if (keyCode != KeyEntity.INVALID_KEY_CODE) {
        startDelayedKeyEventInternal(REPEAT_KEY, context, repeatKeyDelay);
      }
    } else {
      int longPressKeyCode = context.getLongPressKeyCode();
      if (longPressKeyCode != KeyEntity.INVALID_KEY_CODE) {
        startDelayedKeyEventInternal(LONG_PRESS_KEY, context, longPressKeyDelay);
      }
    }
  }

  /**
   * Cancel pending repeating onKey events or a long press key event, related to
   * the given {@code context}. Note that it is necessary to invoke this method
   * on the thread whose {@code Looper} is passed to this instance via the constructor.
   * Otherwise, some events may NOT be canceled.
   */
  public void cancelDelayedKeyEvent(KeyEventContext context) {
    handler.removeMessages(REPEAT_KEY, context);
    handler.removeMessages(LONG_PRESS_KEY, context);
  }
}
