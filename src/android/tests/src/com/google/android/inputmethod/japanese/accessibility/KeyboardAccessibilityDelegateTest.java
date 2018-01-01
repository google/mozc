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

import static org.easymock.EasyMock.anyInt;
import static org.easymock.EasyMock.anyObject;
import static org.easymock.EasyMock.eq;
import static org.easymock.EasyMock.expect;

import org.mozc.android.inputmethod.japanese.accessibility.KeyboardAccessibilityDelegate.TouchEventEmulator;
import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory.DrawableType;
import org.mozc.android.inputmethod.japanese.keyboard.Flick;
import org.mozc.android.inputmethod.japanese.keyboard.Flick.Direction;
import org.mozc.android.inputmethod.japanese.keyboard.Key;
import org.mozc.android.inputmethod.japanese.keyboard.Key.Stick;
import org.mozc.android.inputmethod.japanese.keyboard.KeyEntity;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.keyboard.PopUp;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import com.google.common.base.Optional;

import android.content.Context;
import android.support.v4.view.accessibility.AccessibilityEventCompat;
import android.support.v4.view.accessibility.AccessibilityNodeInfoCompat;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.MotionEvent;
import android.view.View;

import java.util.Collections;
import java.util.List;

/**
 * Test for KeyboardAccessibilityDelegate.
 */
public class KeyboardAccessibilityDelegateTest extends InstrumentationTestCaseWithMock {

  private static final int SOURCE_ID = 10;
  private static final int KEY_CODE = 11;
  private static final int LONGPRESS_KEY_CODE = 12;

  @Override
  protected void setUp() throws Exception {
    super.setUp();
    AccessibilityUtil.reset();
  }

  @Override
  protected void tearDown() throws Exception {
    AccessibilityUtil.reset();
    super.tearDown();
  }

  private static MotionEvent createMotionEvent(int action) {
    return MotionEvent.obtain(0, 0, action, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  }

  private static Optional<Key> createOptionalKey() {
    // Currently following values are not used.
    KeyEntity keyEntity =
        new KeyEntity(SOURCE_ID, KEY_CODE, LONGPRESS_KEY_CODE, true, 0,
                      Optional.<String>absent(), false, Optional.<PopUp>absent(), 0, 0, 0, 0);
    Flick flick = new Flick(Direction.CENTER, keyEntity);
    List<KeyState> keyStates =
        Collections.singletonList(
            new KeyState("",
                         Collections.<MetaState>emptySet(),
                         Collections.<MetaState>emptySet(),
                         Collections.<MetaState>emptySet(),
                         Collections.singletonList(flick)));
    return Optional.of(new Key(0, 0, 10, 10, 0, 0, false, false, Stick.EVEN,
                               DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND, keyStates));
  }

  private KeyboardAccessibilityNodeProvider createMockProvider(View view) {
    return createMockBuilder(KeyboardAccessibilityNodeProvider.class)
        .withConstructor(View.class).withArgs(view).createMock();
  }

  @SmallTest
  public void testDispatchHoverEvent_NoCorrespondingAction() {
    // Action: No corresponding action
    // Expectation:
    //   - No accessibility events are sent.
    //   - No methods in the KeyEventHandler are called back.
    //   - MotionEvent is not consumed.
    View view = createViewMock(View.class);
    KeyboardAccessibilityNodeProvider provider = createMockProvider(view);
    expect(provider.getKey(anyInt(), anyInt())).andReturn(Optional.<Key>absent());
    replayAll();
    KeyboardAccessibilityDelegate delegate =
        new KeyboardAccessibilityDelegate(view, provider, 0, new TouchEventEmulator() {
          @Override
          public void emulateLongPress(Key key) {
          }
          @Override
          public void emulateKeyInput(Key key) {
          }
        });
    assertFalse(delegate.dispatchHoverEvent(createMotionEvent(MotionEvent.ACTION_CANCEL)));
    verifyAll();
  }

  @SmallTest
  public void testDispatchHoverEvent_EnterMoveExitOnNonKey() {
    // Action: Enter, Move and Exit
    // Position: On non-key position
    // Expectation:
    //   - No accessibility events are sent.
    //   - No methods in the KeyEventHandler are called back.
    //   - MotionEvent is not consumed.
    for (int action : new int[] {MotionEvent.ACTION_HOVER_ENTER,
                                 MotionEvent.ACTION_HOVER_MOVE,
                                 MotionEvent.ACTION_HOVER_EXIT}) {
      resetAll();
      View view = createViewMock(View.class);
      TouchEventEmulator emulator = createMock(TouchEventEmulator.class);
      KeyboardAccessibilityNodeProvider provider = createMockProvider(view);
      expect(provider.getKey(anyInt(), anyInt())).andReturn(Optional.<Key>absent());
      replayAll();
      KeyboardAccessibilityDelegate delegate =
          new KeyboardAccessibilityDelegate(view, provider, 0, emulator);
      assertFalse(delegate.dispatchHoverEvent(createMotionEvent(action)));
      verifyAll();
    }
  }

  @SmallTest
  public void testDispatchHoverEvent_EnterMoveOnKey() {
    // Action: Enter and Move
    // Position: On key position
    // Precondition: No lastHoverKey exists (Enter, or move from non-key to key)
    // Expectation:
    //   - Accessibility events TYPE_VIEW_HOVER_ENTER and ACTION_ACCESSIBILITY_FOCUS are sent.
    //   - No methods in the KeyEventHandler are called back.
    //   - MotionEvent is consumed.
    for (int action : new int[] {MotionEvent.ACTION_HOVER_ENTER,
                                 MotionEvent.ACTION_HOVER_MOVE}) {
      resetAll();
      View view = createViewMock(View.class);
      TouchEventEmulator emulator = createMock(TouchEventEmulator.class);
      KeyboardAccessibilityNodeProvider provider = createMockProvider(view);
      expect(provider.getKey(anyInt(), anyInt())).andReturn(createOptionalKey());
      provider.sendAccessibilityEventForKeyIfAccessibilityEnabled(
          anyObject(Key.class), eq(AccessibilityEventCompat.TYPE_VIEW_HOVER_ENTER));
      expect(provider.performActionForKey(
          anyObject(Key.class), eq(AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS)))
          .andReturn(true);
      replayAll();
      KeyboardAccessibilityDelegate delegate =
          new KeyboardAccessibilityDelegate(view, provider, 0, emulator);
      assertTrue(delegate.dispatchHoverEvent(createMotionEvent(action)));
      verifyAll();
    }
  }

  @SmallTest
  public void testDispatchHoverEvent_ExitOnKey() {
    // Action: Enter
    // Position: On key position
    // Expectation:
    //   - Accessibility events TYPE_VIEW_HOVER_EXIT.
    //   - KeyEventHandler#onKey is invoked.
    //   - MotionEvent is consumed.
    View view = createViewMock(View.class);
    TouchEventEmulator emulator = createMock(TouchEventEmulator.class);
    KeyboardAccessibilityNodeProvider provider = createMockProvider(view);

    Optional<Key> optionalKey = createOptionalKey();
    expect(provider.getKey(anyInt(), anyInt())).andReturn(optionalKey);
    emulator.emulateKeyInput(anyObject(Key.class));
    provider.sendAccessibilityEventForKeyIfAccessibilityEnabled(
        anyObject(Key.class), eq(AccessibilityEventCompat.TYPE_VIEW_HOVER_EXIT));
    expect(provider.performActionForKey(
        anyObject(Key.class), eq(AccessibilityNodeInfoCompat.ACTION_CLEAR_ACCESSIBILITY_FOCUS)))
        .andReturn(true);
    replayAll();
    KeyboardAccessibilityDelegate delegate =
        new KeyboardAccessibilityDelegate(view, provider, 0, emulator);
    assertTrue(delegate.dispatchHoverEvent(createMotionEvent(MotionEvent.ACTION_HOVER_EXIT)));
    verifyAll();
  }

  @SmallTest
  public void testDispatchHoverEvent_MoveFromKeyToKey() {
    // Action: Move
    // Position: From key to another key
    // Expectation:
    //   - Accessibility events TYPE_VIEW_HOVER_EXIT, ACTION_CLEAR_ACCESSIBILITY_FOCUS,
    //     TYPE_VIEW_HOVER_ENTER and ACTION_ACCESSIBILITY_FOCUS.
    //   - No methods in the KeyEventHandler are called back.
    //   - MotionEvent is consumed.
    View view = createViewMock(View.class);
    TouchEventEmulator emulator = createMock(TouchEventEmulator.class);
    KeyboardAccessibilityNodeProvider provider = createMockProvider(view);
    KeyboardAccessibilityDelegate delegate =
        new KeyboardAccessibilityDelegate(view, provider, 0, emulator);
    // Preparing precondition.
    expect(provider.getKey(anyInt(), anyInt())).andReturn(createOptionalKey());
    provider.sendAccessibilityEventForKeyIfAccessibilityEnabled(
        anyObject(Key.class), eq(AccessibilityEventCompat.TYPE_VIEW_HOVER_ENTER));
    expect(provider.performActionForKey(
        anyObject(Key.class), eq(AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS)))
        .andReturn(true);
    replayAll();
    delegate.dispatchHoverEvent(createMotionEvent(MotionEvent.ACTION_HOVER_ENTER));
    // Start recording
    resetAll();
    expect(provider.getKey(anyInt(), anyInt())).andReturn(createOptionalKey());
    provider.sendAccessibilityEventForKeyIfAccessibilityEnabled(
        anyObject(Key.class), eq(AccessibilityEventCompat.TYPE_VIEW_HOVER_EXIT));
    expect(provider.performActionForKey(
        anyObject(Key.class), eq(AccessibilityNodeInfoCompat.ACTION_CLEAR_ACCESSIBILITY_FOCUS)))
        .andReturn(true);
    provider.sendAccessibilityEventForKeyIfAccessibilityEnabled(
        anyObject(Key.class), eq(AccessibilityEventCompat.TYPE_VIEW_HOVER_ENTER));
    expect(provider.performActionForKey(
        anyObject(Key.class), eq(AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS)))
        .andReturn(true);
    replayAll();
    assertTrue(delegate.dispatchHoverEvent(createMotionEvent(MotionEvent.ACTION_HOVER_MOVE)));
    verifyAll();
  }

  @SmallTest
  public void testDispatchHoverEvent_MoveFromKeyToNonKey() {
    // Action: Move
    // Position: From key to non-key
    // Expectation:
    //   - Accessibility events TYPE_VIEW_HOVER_EXIT, ACTION_CLEAR_ACCESSIBILITY_FOCUS,
    //   - No methods in the KeyEventHandler are called back.
    //   - MotionEvent is consumed.
    View view = createViewMock(View.class);
    TouchEventEmulator emulator = createMock(TouchEventEmulator.class);
    KeyboardAccessibilityNodeProvider provider = createMockProvider(view);
    KeyboardAccessibilityDelegate delegate =
        new KeyboardAccessibilityDelegate(view, provider, 0, emulator);
    // Preparing precondition.
    expect(provider.getKey(anyInt(), anyInt())).andReturn(createOptionalKey());
    provider.sendAccessibilityEventForKeyIfAccessibilityEnabled(
        anyObject(Key.class), eq(AccessibilityEventCompat.TYPE_VIEW_HOVER_ENTER));
    expect(provider.performActionForKey(
        anyObject(Key.class), eq(AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS)))
        .andReturn(true);
    replayAll();
    delegate.dispatchHoverEvent(createMotionEvent(MotionEvent.ACTION_HOVER_ENTER));
    // Start recording
    resetAll();
    expect(provider.getKey(anyInt(), anyInt())).andReturn(Optional.<Key>absent());
    provider.sendAccessibilityEventForKeyIfAccessibilityEnabled(
        anyObject(Key.class), eq(AccessibilityEventCompat.TYPE_VIEW_HOVER_EXIT));
    expect(provider.performActionForKey(
        anyObject(Key.class), eq(AccessibilityNodeInfoCompat.ACTION_CLEAR_ACCESSIBILITY_FOCUS)))
        .andReturn(true);
    replayAll();
    assertFalse(delegate.dispatchHoverEvent(createMotionEvent(MotionEvent.ACTION_HOVER_MOVE)));
    verifyAll();
  }

  @SmallTest
  public void testSetPasswordField() {
    View view = createViewMock(View.class);
    AudioManagerWrapper audioManager = createMockBuilder(AudioManagerWrapper.class)
        .withConstructor(Context.class)
        .withArgs(getInstrumentation().getTargetContext())
        .addMockedMethods("isWiredHeadsetOn", "isBluetoothA2dpOn")
        .createMock();
    AccessibilityUtil.setAudioManagerForTesting(Optional.of(audioManager));
    TouchEventEmulator emulator = createMock(TouchEventEmulator.class);
    KeyboardAccessibilityNodeProvider provider = createMockProvider(view);
    KeyboardAccessibilityDelegate delegate =
        new KeyboardAccessibilityDelegate(view, provider, 0, emulator);
    for (boolean isPasswordField : new boolean[] {true, false}) {
      for (boolean isAccessibilityEnabled : new boolean[] {true, false}) {
        AccessibilityUtil.setAccessibilityEnabled(Optional.of(isAccessibilityEnabled));
        for (boolean isAccessibilitySpeakPasswordEnabled : new boolean[] {true, false}) {
          AccessibilityUtil.setAccessibilitySpeakPasswordEnabled(
              Optional.of(isAccessibilitySpeakPasswordEnabled));
          for (boolean isWiredHeadsetOn : new boolean[] {true, false}) {
            for (boolean isBluetoothA2dpOn : new boolean[] {true, false}) {
              resetAll();
              expect(audioManager.isWiredHeadsetOn()).andStubReturn(isWiredHeadsetOn);
              expect(audioManager.isBluetoothA2dpOn()).andStubReturn(isBluetoothA2dpOn);
              provider.setObscureInput(isPasswordField && isAccessibilityEnabled
                                       && !isAccessibilitySpeakPasswordEnabled
                                       && !isWiredHeadsetOn && !isBluetoothA2dpOn);
              replayAll();
              delegate.setPasswordField(isPasswordField);
              verifyAll();
            }
          }
        }
      }
    }
  }
}
