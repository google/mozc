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

import static org.easymock.EasyMock.anyObject;
import static org.easymock.EasyMock.capture;
import static org.easymock.EasyMock.expect;

import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory.DrawableType;
import org.mozc.android.inputmethod.japanese.keyboard.Flick;
import org.mozc.android.inputmethod.japanese.keyboard.Flick.Direction;
import org.mozc.android.inputmethod.japanese.keyboard.Key;
import org.mozc.android.inputmethod.japanese.keyboard.Key.Stick;
import org.mozc.android.inputmethod.japanese.keyboard.KeyEntity;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.keyboard.Keyboard;
import org.mozc.android.inputmethod.japanese.keyboard.Keyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.keyboard.PopUp;
import org.mozc.android.inputmethod.japanese.keyboard.Row;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import com.google.common.base.Optional;

import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.support.v4.view.accessibility.AccessibilityEventCompat;
import android.support.v4.view.accessibility.AccessibilityNodeInfoCompat;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;

import org.easymock.Capture;

import java.util.Arrays;
import java.util.Collections;
import java.util.EnumSet;
import java.util.List;

/**
 * Test for KeyboardAccessibilityNodeProvider.
 */
public class KeyboardAccessibilityNodeProviderTest extends InstrumentationTestCaseWithMock {

  private static final int SOURCE_ID_UNMODIFIED = 1;
  private static final int SOURCE_ID_SHIFTED = 2;
  private static final int SOURCE_ID_BACKSPACE = 3;
  private static final int KEYCODE_UNMODIFIED = 'a';
  private static final int KEYCODE_SHIFTED = 'A';
  private static final int KEYCODE_BACKSPACE = 0x08;  // Code point of backspace

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

  private Keyboard createMockKeyboard() {
    List<Row> rowList = Collections.singletonList(
        new Row(Arrays.asList(
            new Key(0, 0, 10, 10, 0, 0, true, false, Stick.EVEN,
                DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND, Arrays.asList(
                new KeyState(
                    "meta:unmodified",
                    Collections.<MetaState>emptySet(),
                    Collections.<MetaState>emptySet(),
                    Collections.<MetaState>emptySet(),
                    Collections.singletonList(
                        new Flick(Direction.CENTER,
                            new KeyEntity(SOURCE_ID_UNMODIFIED, KEYCODE_UNMODIFIED, 0, true, 0,
                                Optional.<String>absent(), false, Optional.<PopUp>absent(),
                                0, 0, 0, 0)))),
                new KeyState(
                    "meta:shift",
                    EnumSet.of(MetaState.SHIFT),
                    Collections.<MetaState>emptySet(),
                    Collections.<MetaState>emptySet(),
                    Collections.singletonList(
                        new Flick(Direction.CENTER,
                            new KeyEntity(SOURCE_ID_SHIFTED, KEYCODE_SHIFTED, 0, true, 0,
                                Optional.<String>absent(), false, Optional.<PopUp>absent(),
                                0, 0, 0, 0)))))),
            new Key(10, 0, 10, 10, 0, 0, true, false, Stick.EVEN,
                DrawableType.TWELVEKEYS_REGULAR_KEY_BACKGROUND, Collections.singletonList(
                new KeyState(
                    "backspace",
                    Collections.<MetaState>emptySet(),
                    Collections.<MetaState>emptySet(),
                    Collections.<MetaState>emptySet(),
                    Collections.singletonList(
                        new Flick(Direction.CENTER,
                            new KeyEntity(SOURCE_ID_BACKSPACE, KEYCODE_BACKSPACE, 0, true, 0,
                                Optional.<String>absent(), false,
                                Optional.<PopUp>absent(), 0, 0, 0, 0))))))),
            10, 0));
    return new Keyboard(Optional.of("testkeyboard"), rowList, 0,
                        KeyboardSpecification.TWELVE_KEY_TOGGLE_FLICK_KANA);
  }

  private View createMockView() {
    return new View(getInstrumentation().getTargetContext()) {
      /**
       * Original onInitializeAccessibilityNodeInfo requires hidden field mAttachInfo
       * to be non-null but to set is very hard.
       * Use mock implementation instead.
       */
      @Override
      public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        return;
      }
    };
  }

  @SmallTest
  public void testCreateAccessibilityNodeInfo_AbsentKeyboard() {
    KeyboardAccessibilityNodeProvider provider =
        new KeyboardAccessibilityNodeProvider(createMockView());
    assertNull(provider.createAccessibilityNodeInfo(KeyboardAccessibilityNodeProvider.UNDEFINED));
    assertEquals(0, provider.createAccessibilityNodeInfo(View.NO_ID).getChildCount());
    assertNull(provider.createAccessibilityNodeInfo(SOURCE_ID_UNMODIFIED));
  }

  @SmallTest
  public void testCreateAccessibilityNodeInfo_UndefinedId() {
    KeyboardAccessibilityNodeProvider provider =
        new KeyboardAccessibilityNodeProvider(createMockView());
    provider.setKeyboard(Optional.of(createMockKeyboard()));
    assertNull(provider.createAccessibilityNodeInfo(KeyboardAccessibilityNodeProvider.UNDEFINED));
  }

  @SmallTest
  public void testCreateAccessibilityNodeInfo_NoId() {
    KeyboardAccessibilityNodeProvider provider =
        new KeyboardAccessibilityNodeProvider(createMockView());
    provider.setKeyboard(Optional.of(createMockKeyboard()));
    AccessibilityNodeInfoCompat info = provider.createAccessibilityNodeInfo(View.NO_ID);
    if (Build.VERSION.SDK_INT >= 16) {
      assertEquals(2, info.getChildCount());
    }
    // Note: There is no way to get the id of children. Testing is omitted.
  }

  @SmallTest
  public void testCreateAccessibilityNodeInfo_WithId() {
    View view = createMockView();
    KeyboardAccessibilityNodeProvider provider =
        new KeyboardAccessibilityNodeProvider(view);
    provider.setKeyboard(Optional.of(createMockKeyboard()));
    AccessibilityNodeInfoCompat info = provider.createAccessibilityNodeInfo(SOURCE_ID_UNMODIFIED);

    assertEquals(0, info.getChildCount());
    assertEquals(getInstrumentation().getTargetContext().getPackageName(), info.getPackageName());
    assertEquals(Key.class.getName(), info.getClassName());
    assertEquals("meta:unmodified", info.getContentDescription());
    Rect tempRect = new Rect();
    info.getBoundsInParent(tempRect);
    assertEquals(new Rect(0, 0, 10, 10), tempRect);
    info.getBoundsInScreen(tempRect);
    assertEquals(new Rect(0, 0, 10, 10), tempRect);
    assertTrue(info.isEnabled());
    if (Build.VERSION.SDK_INT >= 16) {
      assertTrue(info.isVisibleToUser());
    }
    assertTrue((info.getActions() & AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS) != 0);
  }

  @SmallTest
  public void testCreateAccessibilityNodeInfo_ObscureInput() {
    View view = createMockView();
    KeyboardAccessibilityNodeProvider provider =
        new KeyboardAccessibilityNodeProvider(view);
    provider.setKeyboard(Optional.of(createMockKeyboard()));
    for (int sourceId : new int[] {SOURCE_ID_UNMODIFIED, SOURCE_ID_BACKSPACE}) {
      for (boolean shouldObscureInput : new boolean[] {true, false}) {
        provider.setObscureInput(shouldObscureInput);
        AccessibilityNodeInfoCompat info =
            provider.createAccessibilityNodeInfo(sourceId);
        assertEquals(0, info.getChildCount());
        String expectedDescription;
        if (sourceId == SOURCE_ID_BACKSPACE) {
          expectedDescription = "backspace";
        } else if (shouldObscureInput) {
          expectedDescription = getInstrumentation().getTargetContext().getString(
              KeyboardAccessibilityNodeProvider.PASSWORD_RESOURCE_ID);
        } else {
          expectedDescription = "meta:unmodified";
        }
        assertEquals(expectedDescription, info.getContentDescription());
      }
    }
  }

  @SmallTest
  public void testPerformAction() {
    KeyboardAccessibilityNodeProvider provider =
        new KeyboardAccessibilityNodeProvider(createMockView());
    AccessibilityManagerWrapper manager = createMockBuilder(
        AccessibilityManagerWrapper.class)
            .withConstructor(Context.class)
            .withArgs(getInstrumentation().getTargetContext())
            .addMockedMethods("isEnabled",
                              "isTouchExplorationEnabled",
                              "sendAccessibilityEvent")
            .createMock();
    AccessibilityUtil.setAccessibilityManagerForTesting(Optional.of(manager));

    expect(manager.isEnabled()).andStubReturn(true);
    expect(manager.isTouchExplorationEnabled()).andStubReturn(true);
    manager.sendAccessibilityEvent(anyObject(AccessibilityEvent.class));
    replayAll();
    provider.setKeyboard(Optional.of(createMockKeyboard()));

    // action: ACTION_ACCESSIBILITY_FOCUS
    // key: SOURCE_ID_UNMODIFIED
    // previous key: UNDEFINED
    // expectation: TYPE_VIEW_ACCESSIBILITY_FOCUSED
    {
      resetAll();
      expect(manager.isEnabled()).andStubReturn(true);
      expect(manager.isTouchExplorationEnabled()).andStubReturn(true);
      Capture<AccessibilityEvent> event = new Capture<AccessibilityEvent>();
      manager.sendAccessibilityEvent(capture(event));
      replayAll();
      assertTrue(
          provider.performAction(SOURCE_ID_UNMODIFIED,
                                 AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS, null));
      assertEquals(AccessibilityEventCompat.TYPE_VIEW_ACCESSIBILITY_FOCUSED,
                   event.getValue().getEventType());
      verifyAll();
    }
    // action: ACTION_ACCESSIBILITY_FOCUS
    // key: SOURCE_ID_UNMODIFIED
    // previous key: SOURCE_ID_UNMODIFIED (set by above test)
    // expectation: returning false
    {
      resetAll();
      expect(manager.isEnabled()).andStubReturn(true);
      expect(manager.isTouchExplorationEnabled()).andStubReturn(true);
      replayAll();
      assertFalse(
          provider.performAction(SOURCE_ID_UNMODIFIED,
                                 AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS, null));
      verifyAll();
    }
    // action: ACTION_ACCESSIBILITY_FOCUS
    // key: SOURCE_ID_BACKSPACE
    // previous key: SOURCE_ID_UNMODIFIED (set by above test)
    // expectation: TYPE_VIEW_ACCESSIBILITY_FOCUSED
    {
      resetAll();
      expect(manager.isEnabled()).andStubReturn(true);
      expect(manager.isTouchExplorationEnabled()).andStubReturn(true);
      Capture<AccessibilityEvent> event = new Capture<AccessibilityEvent>();
      manager.sendAccessibilityEvent(capture(event));
      replayAll();
      assertTrue(
          provider.performAction(SOURCE_ID_BACKSPACE,
                                 AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS, null));
      assertEquals(AccessibilityEventCompat.TYPE_VIEW_ACCESSIBILITY_FOCUSED,
                   event.getValue().getEventType());
      verifyAll();
    }
    // action: ACTION_CLEAR_ACCESSIBILITY_FOCUS
    // key: SOURCE_ID_UNMODIFIED
    // previous key: SOURCE_ID_BACKSPACE (set by above test)
    // expectation: returning false
    {
      resetAll();
      expect(manager.isEnabled()).andStubReturn(true);
      expect(manager.isTouchExplorationEnabled()).andStubReturn(true);
      replayAll();
      assertFalse(
          provider.performAction(SOURCE_ID_UNMODIFIED,
                                 AccessibilityNodeInfoCompat.ACTION_CLEAR_ACCESSIBILITY_FOCUS,
                                 null));
      verifyAll();
    }
    // action: ACTION_CLEAR_ACCESSIBILITY_FOCUS
    // key: SOURCE_ID_UNMODIFIED
    // previous key: SOURCE_ID_UNMODIFIED (set by *this* test)
    // expectation: TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED
    {
      // Preparation
      resetAll();
      expect(manager.isEnabled()).andStubReturn(true);
      expect(manager.isTouchExplorationEnabled()).andStubReturn(true);
      manager.sendAccessibilityEvent(anyObject(AccessibilityEvent.class));
      replayAll();
      provider.performAction(SOURCE_ID_UNMODIFIED,
          AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS, null);
      // Testing
      resetAll();
      expect(manager.isEnabled()).andStubReturn(true);
      expect(manager.isTouchExplorationEnabled()).andStubReturn(true);
      Capture<AccessibilityEvent> event = new Capture<AccessibilityEvent>();
      manager.sendAccessibilityEvent(capture(event));
      replayAll();
      assertTrue(
          provider.performAction(SOURCE_ID_UNMODIFIED,
                                 AccessibilityNodeInfoCompat.ACTION_CLEAR_ACCESSIBILITY_FOCUS,
                                 null));
      assertEquals(AccessibilityEventCompat.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED,
                   event.getValue().getEventType());
      verifyAll();
    }
  }

  // Note AccessibilityEvent doesn't contain information enough to verify
  // the behavior of performActionForKey method (sourceId is required but not accessible).
  // Indirect verification instead.
  @SmallTest
  public void testPerformActionForKey() {
    KeyboardAccessibilityNodeProvider provider =
        new KeyboardAccessibilityNodeProvider(createMockView());
    AccessibilityManagerWrapper manager = createMockBuilder(
        AccessibilityManagerWrapper.class)
            .withConstructor(Context.class)
            .withArgs(getInstrumentation().getTargetContext())
            .addMockedMethods("isEnabled",
                              "isTouchExplorationEnabled",
                              "sendAccessibilityEvent")
            .createMock();
    AccessibilityUtil.setAccessibilityManagerForTesting(Optional.of(manager));

    expect(manager.isEnabled()).andStubReturn(true);
    expect(manager.isTouchExplorationEnabled()).andStubReturn(true);
    manager.sendAccessibilityEvent(anyObject(AccessibilityEvent.class));
    replayAll();
    Keyboard keyboard = createMockKeyboard();
    provider.setKeyboard(Optional.of(keyboard));
    // action: ACTION_ACCESSIBILITY_FOCUS
    // key: SOURCE_ID_UNMODIFIED
    // previous key: UNDEFINED
    // expectation: TYPE_VIEW_ACCESSIBILITY_FOCUSED
    {
      Key unmodified = keyboard.getRowList().get(0).getKeyList().get(0);
      resetAll();
      expect(manager.isEnabled()).andStubReturn(true);
      expect(manager.isTouchExplorationEnabled()).andStubReturn(true);
      Capture<AccessibilityEvent> event = new Capture<AccessibilityEvent>();
      manager.sendAccessibilityEvent(capture(event));
      replayAll();
      assertTrue(
          provider.performActionForKey(unmodified,
                                       AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS));
      assertEquals(AccessibilityEventCompat.TYPE_VIEW_ACCESSIBILITY_FOCUSED,
                   event.getValue().getEventType());
      verifyAll();
    }
    // action: ACTION_ACCESSIBILITY_FOCUS
    // key: SOURCE_ID_UNMODIFIED
    // previous key: SOURCE_ID_UNMODIFIED (set by above test)
    // expectation: returning false
    {
      Key unmodified = keyboard.getRowList().get(0).getKeyList().get(0);
      resetAll();
      expect(manager.isEnabled()).andStubReturn(true);
      expect(manager.isTouchExplorationEnabled()).andStubReturn(true);
      replayAll();
      assertFalse(
          provider.performActionForKey(unmodified,
                                       AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS));
      verifyAll();
    }
  }
}
