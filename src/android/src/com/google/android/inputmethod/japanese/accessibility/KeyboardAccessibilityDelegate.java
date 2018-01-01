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

package org.mozc.android.inputmethod.japanese.accessibility;

import org.mozc.android.inputmethod.japanese.keyboard.Flick.Direction;
import org.mozc.android.inputmethod.japanese.keyboard.Key;
import org.mozc.android.inputmethod.japanese.keyboard.KeyEntity;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.keyboard.Keyboard;
import org.mozc.android.inputmethod.japanese.resources.R;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.content.Context;
import android.os.Build;
import android.os.Handler;
import android.os.Message;
import android.os.SystemClock;
import android.support.v4.view.AccessibilityDelegateCompat;
import android.support.v4.view.ViewCompat;
import android.support.v4.view.accessibility.AccessibilityEventCompat;
import android.support.v4.view.accessibility.AccessibilityNodeInfoCompat;
import android.support.v4.view.accessibility.AccessibilityNodeProviderCompat;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.accessibility.AccessibilityEvent;

import java.util.Collections;
import java.util.Set;



/**
 * Delegate object for Keyboard view to support accessibility.
 */
public class KeyboardAccessibilityDelegate extends AccessibilityDelegateCompat {

  private final View view;
  private final KeyboardAccessibilityNodeProvider nodeProvider;
  private Optional<Key> lastHoverKey = Optional.absent();
  // Handler for long-press callback.
  // Contains 0 or 1 delayed message.
  private final Handler handler;
  // "what" value of a Message.
  private static final int LONGPRESS_WHAT_VALUE = 1;
  // Delay for long-press detection (in ms).
  private final int longpressDelay;
  // True if the touch sequence is consumed by long-press.
  // In such case touch-up shouldn't send any key events.
  // Reset to false when new touch sequence is started.
  private boolean consumedByLongpress = false;
  private final TouchEventEmulator emulator;

  /**
   * Emulator interface for touch events (Key input and long press).
   */
  public interface TouchEventEmulator {
    public void emulateKeyInput(Key key);
    public void emulateLongPress(Key key);
  }

  private class LongTapHandler implements Handler.Callback {
    @Override
    public boolean handleMessage(Message msg) {
      if (lastHoverKey.isPresent()) {
        simulateLongPress(lastHoverKey.get());
      }
      return true;
    }
  }


  public KeyboardAccessibilityDelegate(View view, TouchEventEmulator emulator) {
    this(view, new KeyboardAccessibilityNodeProvider(view),
         view.getContext().getResources().getInteger(
             R.integer.config_long_press_key_delay_accessibility),
         emulator);
  }

  @VisibleForTesting
  KeyboardAccessibilityDelegate(View view,
                                KeyboardAccessibilityNodeProvider nodeProvider,
                                int longpressDelay,
                                TouchEventEmulator emulator) {
    this.view = Preconditions.checkNotNull(view);
    this.nodeProvider = Preconditions.checkNotNull(nodeProvider);
    this.handler = new Handler(new LongTapHandler());
    this.longpressDelay = longpressDelay;
    this.emulator = Preconditions.checkNotNull(emulator);
  }

  private Context getContext() {
    return view.getContext();
  }

  /**
   * Intercepts touch events before dispatch when touch exploration is turned on in ICS and
   * higher.
   *
   * @param event The motion event being dispatched.
   * @return {@code true} if the event is handled
   */
  public boolean dispatchTouchEvent(MotionEvent event) {
    // To avoid accidental key presses during touch exploration, always drop
    // touch events generated by the user.
    return false;
  }

  /**
   * Dispatched from {@code View#dispatchHoverEvent}.
   *
   * @return {@code true} if the event was handled by the view, false otherwise
   */
  public boolean dispatchHoverEvent(MotionEvent event) {
    Preconditions.checkNotNull(event);

    Optional<Key> optionalKey = nodeProvider.getKey((int) event.getX(), (int) event.getY());
    switch (event.getAction()) {
      case MotionEvent.ACTION_HOVER_ENTER:
        Preconditions.checkState(!lastHoverKey.isPresent());
        consumedByLongpress = false;
        if (optionalKey.isPresent()) {
          Key key = optionalKey.get();
          // Notify the user that we are entering new virtual view.
          nodeProvider.sendAccessibilityEventForKeyIfAccessibilityEnabled(
              key, AccessibilityEventCompat.TYPE_VIEW_HOVER_ENTER);
          // Make virtual view focus on the key.
          nodeProvider.performActionForKey(
              key, AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS);
          // Register singleton message for long-press.
          handler.removeMessages(LONGPRESS_WHAT_VALUE);
          handler.sendMessageDelayed(
              handler.obtainMessage(LONGPRESS_WHAT_VALUE, 0, 0, getContext()), longpressDelay);
        }
        lastHoverKey = optionalKey;
        break;
      case MotionEvent.ACTION_HOVER_EXIT:
        if (optionalKey.isPresent()) {
          Key key = optionalKey.get();
          simulateKeyInput(key);
          // Notify the user that we are exiting from the key.
          nodeProvider.sendAccessibilityEventForKeyIfAccessibilityEnabled(
              key, AccessibilityEventCompat.TYPE_VIEW_HOVER_EXIT);
          // Make virtual view unfocused.
          nodeProvider.performActionForKey(
              key, AccessibilityNodeInfoCompat.ACTION_CLEAR_ACCESSIBILITY_FOCUS);
        }
        lastHoverKey = Optional.absent();
        // Remove all the long-press messages.
        handler.removeMessages(LONGPRESS_WHAT_VALUE);
        break;
      case MotionEvent.ACTION_HOVER_MOVE:
        if (optionalKey.equals(lastHoverKey)) {
          // Hovering status is unchanged.
          break;
        }
        if (lastHoverKey.isPresent()) {
          // Notify the user that we are exiting from lastHoverKey.
          nodeProvider.sendAccessibilityEventForKeyIfAccessibilityEnabled(
              lastHoverKey.get(), AccessibilityEventCompat.TYPE_VIEW_HOVER_EXIT);
          // Make virtual view unfocused.
          nodeProvider.performActionForKey(
              lastHoverKey.get(), AccessibilityNodeInfoCompat.ACTION_CLEAR_ACCESSIBILITY_FOCUS);
        }
        if (optionalKey.isPresent()) {
          Key key = optionalKey.get();
          // Notify the user that we are entering new virtual view.
          nodeProvider.sendAccessibilityEventForKeyIfAccessibilityEnabled(
              key, AccessibilityEventCompat.TYPE_VIEW_HOVER_ENTER);
          // Make virtual view focus on the key.
          nodeProvider.performActionForKey(
              key, AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS);
          // Register singleton message for long-press.
          handler.removeMessages(LONGPRESS_WHAT_VALUE);
          handler.sendMessageDelayed(
              handler.obtainMessage(LONGPRESS_WHAT_VALUE, 0, 0, getContext()), longpressDelay);
        }
        lastHoverKey = optionalKey;
        break;
    }
    return optionalKey.isPresent();
  }

  private void simulateKeyInput(Key key) {
    Preconditions.checkNotNull(key);
    Optional<KeyState> keyState = key.getKeyState(Collections.<MetaState>emptySet());
    if (!keyState.isPresent()) {
      return;
    }
    int keyCode = keyState.get().getFlick(Direction.CENTER).get().getKeyEntity().getKeyCode();
    if (keyCode == KeyEntity.INVALID_KEY_CODE || consumedByLongpress) {
      return;
    }
    emulator.emulateKeyInput(key);
  }

  private void simulateLongPress(Key key) {
    Preconditions.checkNotNull(key);
    Optional<KeyState> keyState = key.getKeyState(Collections.<MetaState>emptySet());
    if (!keyState.isPresent()) {
      return;
    }
    int longPressKeyCode =
        keyState.get().getFlick(Direction.CENTER).get().getKeyEntity().getLongPressKeyCode();
    if (longPressKeyCode == KeyEntity.INVALID_KEY_CODE || consumedByLongpress) {
      return;
    }
    emulator.emulateLongPress(key);
    consumedByLongpress = true;
  }

  @Override
  public AccessibilityNodeProviderCompat getAccessibilityNodeProvider(View host) {
    return nodeProvider;
  }

  /**
   * Sets metastate of the keyboard.
   *
   * <p>Node provider's internal state is reset here.
   */
  public void setMetaState(Set<MetaState> metaState) {
    nodeProvider.setMetaState(Preconditions.checkNotNull(metaState));
  }

  /**
   * Sets the keyboard.
   *
   * <p>Node provider's internal state is reset here.
   */
  public void setKeyboard(Optional<Keyboard> keyboard) {
    nodeProvider.setKeyboard(Preconditions.checkNotNull(keyboard));
    if (AccessibilityUtil.isAccessibilityEnabled(getContext())) {
      Optional<String> contentDescription = keyboard.isPresent()
          ? keyboard.get().getContentDescription()
          : Optional.<String>absent();
      sendWindowStateChanged(contentDescription);
    }
  }


  /**
   * Sets whether here is password field or not..
   *
   * <p>Node provider's internal state is reset here.
   */
  public void setPasswordField(boolean isPasswordField) {
    boolean shouldObscureInput = shouldObscureInput(isPasswordField);
    nodeProvider.setObscureInput(shouldObscureInput);

    if (shouldObscureInput) {
      announceForAccessibility(getContext().getResources().getString(
          R.string.spoken_use_headphone_for_password));
    }
  }

  private void announceForAccessibility(String text) {
    AccessibilityEvent event = AccessibilityEvent.obtain();
    event.setPackageName(getClass().getPackage().getName());
    event.setClassName(getClass().getName());
    event.setEventTime(SystemClock.uptimeMillis());
    event.setEnabled(true);
    event.getText().add(Preconditions.checkNotNull(text));
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
      event.setEventType(AccessibilityEventCompat.TYPE_ANNOUNCEMENT);
    } else {
      event.setEventType(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }
    requestSendAccessibilityEventIfPossible(event);
  }

  /**
   * Sends a window state change event with the specified text.
   *
   * @param newContentDescription the text to send with the event as content description
   */
  private void sendWindowStateChanged(Optional<String> newContentDescription) {
    Preconditions.checkNotNull(newContentDescription);
    AccessibilityEvent stateChange = AccessibilityEvent.obtain(
        AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
    ViewCompat.onInitializeAccessibilityEvent(view, stateChange);
    stateChange.setContentDescription(newContentDescription.orNull());
    requestSendAccessibilityEventIfPossible(stateChange);
  }

  /**
   * Sends an AccessibilityEvent throuth {@code view}'s parent.
   * If the API Level is <14, does nothing.
   */
  private void requestSendAccessibilityEventIfPossible(AccessibilityEvent event) {
    Preconditions.checkNotNull(event);

    ViewParent viewParent = view.getParent();
    if ((viewParent == null) || !(viewParent instanceof ViewGroup)) {
        return;
    }
    // requestSendAccessibilityEvent is since API Level 14 (ICS).
    // No fallback is provided for older framework. Just ignore.
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
      viewParent.requestSendAccessibilityEvent(view, event);
    }
  }

  /**
   * Returns whether raw password shouldn't be spoken.
   *
   * @return false is raw password should be spoken
   */
  private boolean shouldObscureInput(boolean isPasswordField) {
    // If accessibility is disabled, obscure input is not required.
    // This check should be done prior to isAccessibilitySpeakPasswordEnabled
    // since isAccessibilitySpeakPasswordEnabled is heavier.
    if (!AccessibilityUtil.isAccessibilityEnabled(getContext())) {
      return false;
    }

    // The user can optionally force speaking passwords.
    if (AccessibilityUtil.isAccessibilitySpeakPasswordEnabled()) {
      return false;
    }

    // Always speak if the user is listening through headphones.
    AudioManagerWrapper audioManager = AccessibilityUtil.getAudioManager(getContext());
    if (audioManager.isWiredHeadsetOn() || audioManager.isBluetoothA2dpOn()) {
        return false;
    }

    // Don't speak if the IME is connected to a password field.
    return isPasswordField;
  }
}
