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

import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.Annotation;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Row;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Span;
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

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Test for CandidateWindowAccessibilityNodeProvider.
 */
public class CandidateWindowAccessibilityNodeProviderTest extends InstrumentationTestCaseWithMock {

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

  private Optional<CandidateWord> createCandidateWord(int id) {
    return Optional.of(CandidateWord
        .newBuilder()
        .setId(id)
        .setValue("" + id)
        .setAnnotation(Annotation.newBuilder().setDescription("" + id))
        .build());
  }

  private CandidateLayout createMockLayout() {
    List<Row> rows = new ArrayList<Row>();
    {
      Row row = new Row();
      row.addSpan(new Span(createCandidateWord(0), 10, 0, Collections.<String>emptyList()));
      row.addSpan(new Span(createCandidateWord(1), 10, 0, Collections.<String>emptyList()));
      row.addSpan(new Span(Optional.<CandidateWord>absent(),
                           10, 0, Collections.<String>emptyList()));
      row.setTop(0);
      row.setHeight(100);
      row.setWidth(300);
      rows.add(row);
    }
    {
      Row row = new Row();
      row.setTop(100);
      row.setHeight(100);
      row.setWidth(300);
      rows.add(row);
    }
    {
      Row row = new Row();
      row.addSpan(new Span(createCandidateWord(3), 10, 0, Collections.<String>emptyList()));
      row.addSpan(new Span(createCandidateWord(4), 10, 0, Collections.<String>emptyList()));
      row.addSpan(new Span(createCandidateWord(5), 10, 0, Collections.<String>emptyList()));
      row.setTop(200);
      row.setHeight(100);
      row.setWidth(300);
      rows.add(row);
    }
    CandidateLayout layout = new CandidateLayout(rows, 300, 300);

    for (Row row : rows) {
      float left = 0;
      for (Span span : row.getSpanList()) {
        span.setLeft(left);
        left += span.getValueWidth();
        span.setRight(left);
      }
    }

    return layout;
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
  public void testCreateAccessibilityNodeInfo_AbsentLayout() {
    CandidateWindowAccessibilityNodeProvider provider =
        new CandidateWindowAccessibilityNodeProvider(createMockView());
    assertNull(provider.createAccessibilityNodeInfo(
        CandidateWindowAccessibilityNodeProvider.UNDEFINED_VIRTUAL_VIEW_ID));
    assertEquals(0, provider.createAccessibilityNodeInfo(View.NO_ID).getChildCount());
    assertNull(provider.createAccessibilityNodeInfo(1));
  }

  @SmallTest
  public void testCreateAccessibilityNodeInfo_UndefinedId() {
    CandidateWindowAccessibilityNodeProvider provider =
        new CandidateWindowAccessibilityNodeProvider(createMockView());
    provider.setCandidateLayout(Optional.of(createMockLayout()));
    assertNull(provider.createAccessibilityNodeInfo(
        CandidateWindowAccessibilityNodeProvider.UNDEFINED_VIRTUAL_VIEW_ID));
  }

  @SmallTest
  public void testCreateAccessibilityNodeInfo_NoId() {
    CandidateWindowAccessibilityNodeProvider provider =
        new CandidateWindowAccessibilityNodeProvider(createMockView());
    provider.setCandidateLayout(Optional.of(createMockLayout()));
    AccessibilityNodeInfoCompat info = provider.createAccessibilityNodeInfo(View.NO_ID);
    if (Build.VERSION.SDK_INT >= 16) {
      assertEquals(6, info.getChildCount());
    }
    // Note: There is no way to get the id of children. Testing is omitted.
  }

  @SmallTest
  public void testCreateAccessibilityNodeInfo_WithId() {
    View view = createMockView();
    CandidateWindowAccessibilityNodeProvider provider =
        new CandidateWindowAccessibilityNodeProvider(view);
    provider.setCandidateLayout(Optional.of(createMockLayout()));
    AccessibilityNodeInfoCompat info = provider.createAccessibilityNodeInfo(10001);

    assertEquals(0, info.getChildCount());
    assertEquals(getInstrumentation().getTargetContext().getPackageName(), info.getPackageName());
    assertEquals(Span.class.getName(), info.getClassName());
    assertEquals("1 1", info.getContentDescription());
    Rect tempRect = new Rect();
    info.getBoundsInParent(tempRect);
    assertEquals(new Rect(10, 0, 20, 100), tempRect);
    info.getBoundsInScreen(tempRect);
    assertEquals(new Rect(10, 0, 20, 100), tempRect);
    assertTrue(info.isEnabled());
    if (Build.VERSION.SDK_INT >= 16) {
      assertTrue(info.isVisibleToUser());
    }
    assertTrue((info.getActions() & AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS) != 0);
  }

  @SmallTest
  public void testPerformAction() {
    CandidateWindowAccessibilityNodeProvider provider =
        new CandidateWindowAccessibilityNodeProvider(createMockView());
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
    provider.setCandidateLayout(Optional.of(createMockLayout()));


    // action: ACTION_ACCESSIBILITY_FOCUS
    // key: 10001
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
          provider.performAction(10001,
                                 AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS, null));
      assertEquals(AccessibilityEventCompat.TYPE_VIEW_ACCESSIBILITY_FOCUSED,
                   event.getValue().getEventType());
      verifyAll();
    }
    // action: ACTION_ACCESSIBILITY_FOCUS
    // key: 10001
    // previous key: 10001 (set by above test)
    // expectation: returning false
    {
      resetAll();
      expect(manager.isEnabled()).andStubReturn(true);
      expect(manager.isTouchExplorationEnabled()).andStubReturn(true);
      replayAll();
      assertFalse(
          provider.performAction(10001,
                                 AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS, null));
      verifyAll();
    }
    // action: ACTION_ACCESSIBILITY_FOCUS
    // key: 10004
    // previous key: 10001 (set by above test)
    // expectation: TYPE_VIEW_ACCESSIBILITY_FOCUSED
    {
      resetAll();
      expect(manager.isEnabled()).andStubReturn(true);
      expect(manager.isTouchExplorationEnabled()).andStubReturn(true);
      Capture<AccessibilityEvent> event = new Capture<AccessibilityEvent>();
      manager.sendAccessibilityEvent(capture(event));
      replayAll();
      assertTrue(
          provider.performAction(10004,
                                 AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS, null));
      assertEquals(AccessibilityEventCompat.TYPE_VIEW_ACCESSIBILITY_FOCUSED,
                   event.getValue().getEventType());
      verifyAll();
    }
    // action: ACTION_CLEAR_ACCESSIBILITY_FOCUS
    // key: 10001
    // previous key: 10004 (set by above test)
    // expectation: returning false
    {
      resetAll();
      expect(manager.isEnabled()).andStubReturn(true);
      expect(manager.isTouchExplorationEnabled()).andStubReturn(true);
      replayAll();
      assertFalse(
          provider.performAction(10001,
                                 AccessibilityNodeInfoCompat.ACTION_CLEAR_ACCESSIBILITY_FOCUS,
                                 null));
      verifyAll();
    }
    // action: ACTION_CLEAR_ACCESSIBILITY_FOCUS
    // key: 10001
    // previous key: 10001 (set by *this* test)
    // expectation: TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED
    {
      // Preparation
      resetAll();
      expect(manager.isEnabled()).andStubReturn(true);
      expect(manager.isTouchExplorationEnabled()).andStubReturn(true);
      manager.sendAccessibilityEvent(anyObject(AccessibilityEvent.class));
      replayAll();
      provider.performAction(10001,
          AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS, null);
      // Testing
      resetAll();
      expect(manager.isEnabled()).andStubReturn(true);
      expect(manager.isTouchExplorationEnabled()).andStubReturn(true);
      Capture<AccessibilityEvent> event = new Capture<AccessibilityEvent>();
      manager.sendAccessibilityEvent(capture(event));
      replayAll();
      assertTrue(
          provider.performAction(10001,
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
  public void testPerformActionForWord() {
    CandidateWindowAccessibilityNodeProvider provider =
        new CandidateWindowAccessibilityNodeProvider(createMockView());
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
    CandidateLayout layout = createMockLayout();
    provider.setCandidateLayout(Optional.of(layout));
    // action: ACTION_ACCESSIBILITY_FOCUS
    // key: id=1
    // previous key: UNDEFINED
    // expectation: TYPE_VIEW_ACCESSIBILITY_FOCUSED
    {
      CandidateWord wordId1 =
          layout.getRowList().get(0).getSpanList().get(0).getCandidateWord().get();
      resetAll();
      expect(manager.isEnabled()).andStubReturn(true);
      expect(manager.isTouchExplorationEnabled()).andStubReturn(true);
      Capture<AccessibilityEvent> event = new Capture<AccessibilityEvent>();
      manager.sendAccessibilityEvent(capture(event));
      replayAll();
      assertTrue(
          provider.performActionForCandidateWord(
              wordId1, AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS));
      assertEquals(AccessibilityEventCompat.TYPE_VIEW_ACCESSIBILITY_FOCUSED,
                   event.getValue().getEventType());
      verifyAll();
    }
    // action: ACTION_ACCESSIBILITY_FOCUS
    // key: SOURCE_ID_UNMODIFIED
    // previous key: SOURCE_ID_UNMODIFIED (set by above test)
    // expectation: returning false
    {
      CandidateWord wordId1 =
          layout.getRowList().get(0).getSpanList().get(0).getCandidateWord().get();
      resetAll();
      expect(manager.isEnabled()).andStubReturn(true);
      expect(manager.isTouchExplorationEnabled()).andStubReturn(true);
      replayAll();
      assertFalse(
          provider.performActionForCandidateWord(
              wordId1, AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS));
      verifyAll();
    }
  }
}
