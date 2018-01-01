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

import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import com.google.common.base.Optional;

import android.support.v4.view.accessibility.AccessibilityEventCompat;
import android.support.v4.view.accessibility.AccessibilityNodeInfoCompat;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.MotionEvent;
import android.view.View;

/**
 * Test for CandidateWindowAccessibilityDelegate.
 */
public class CandidateWindowAccessibilityDelegateTest extends InstrumentationTestCaseWithMock {

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

  private static Optional<CandidateWord> createOptionalCandidateWord() {
    return Optional.of(CandidateWord.newBuilder().setValue("" + Math.random()).build());
  }

  private CandidateWindowAccessibilityNodeProvider createMockProvider(View view) {
    return createMockBuilder(CandidateWindowAccessibilityNodeProvider.class)
        .withConstructor(View.class).withArgs(view).createMock();
  }

  @SmallTest
  public void testDispatchHoverEvent_NoCorrespondingAction() {
    // Action: No corresponding action
    // Expectation:
    //   - No accessibility events are sent.
    //   - No methods in the view are called back.
    //   - MotionEvent is not consumed.
    View view = createViewMock(View.class);
    CandidateWindowAccessibilityNodeProvider provider = createMockProvider(view);
    expect(provider.getCandidateWord(anyInt(), anyInt()))
        .andReturn(Optional.<CandidateWord>absent());
    replayAll();
    CandidateWindowAccessibilityDelegate delegate =
        new CandidateWindowAccessibilityDelegate(view, provider);
    assertFalse(delegate.dispatchHoverEvent(createMotionEvent(MotionEvent.ACTION_CANCEL)));
    verifyAll();
  }

  @SmallTest
  public void testDispatchHoverEvent_EnterMoveExitOnNonWord() {
    // Action: Enter, Move and Exit
    // Position: On non-candidateWord position
    // Expectation:
    //   - No accessibility events are sent.
    //   - No methods in the view are called back.
    //   - MotionEvent is not consumed.
    for (int action : new int[] {MotionEvent.ACTION_HOVER_ENTER,
                                 MotionEvent.ACTION_HOVER_MOVE,
                                 MotionEvent.ACTION_HOVER_EXIT}) {
      resetAll();
      View view = createViewMock(View.class);
      CandidateWindowAccessibilityNodeProvider provider = createMockProvider(view);
      expect(provider.getCandidateWord(anyInt(), anyInt()))
          .andReturn(Optional.<CandidateWord>absent());
      replayAll();
      CandidateWindowAccessibilityDelegate delegate =
          new CandidateWindowAccessibilityDelegate(view, provider);
      assertFalse(delegate.dispatchHoverEvent(createMotionEvent(action)));
      verifyAll();
    }
  }

  @SmallTest
  public void testDispatchHoverEvent_EnterMoveOnWord() {
    // Action: Enter and Move
    // Position: On candidate word position
    // Precondition: No lastHoverCandidateWord exists (Enter, or move from non-word to word)
    // Expectation:
    //   - Accessibility events TYPE_VIEW_HOVER_ENTER and ACTION_ACCESSIBILITY_FOCUS are sent.
    //   - No methods in the view are called back.
    //   - MotionEvent is consumed.
    for (int action : new int[] {MotionEvent.ACTION_HOVER_ENTER,
                                 MotionEvent.ACTION_HOVER_MOVE}) {
      resetAll();
      View view = createViewMock(View.class);
      CandidateWindowAccessibilityNodeProvider provider = createMockProvider(view);
      expect(provider.getCandidateWord(anyInt(), anyInt()))
          .andReturn(createOptionalCandidateWord());
      provider.sendAccessibilityEventForCandidateWordIfAccessibilityEnabled(
          anyObject(CandidateWord.class), eq(AccessibilityEventCompat.TYPE_VIEW_HOVER_ENTER));
      expect(provider.performActionForCandidateWord(
          anyObject(CandidateWord.class),
          eq(AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS)))
          .andReturn(true);
      replayAll();
      CandidateWindowAccessibilityDelegate delegate =
          new CandidateWindowAccessibilityDelegate(view, provider);
      assertTrue(delegate.dispatchHoverEvent(createMotionEvent(action)));
      verifyAll();
    }
  }

  @SmallTest
  public void testDispatchHoverEvent_ExitOnCandidateWord() {
    // Action: Enter
    // Position: On word position
    // Expectation:
    //   - Accessibility events TYPE_VIEW_HOVER_EXIT.
    //   - MotionEvent is consumed.
    View view = createViewMock(View.class);
    CandidateWindowAccessibilityNodeProvider provider = createMockProvider(view);
    expect(provider.getCandidateWord(anyInt(), anyInt())).andReturn(createOptionalCandidateWord());
    provider.sendAccessibilityEventForCandidateWordIfAccessibilityEnabled(
        anyObject(CandidateWord.class), eq(AccessibilityEventCompat.TYPE_VIEW_HOVER_EXIT));
    expect(provider.performActionForCandidateWord(
        anyObject(CandidateWord.class),
        eq(AccessibilityNodeInfoCompat.ACTION_CLEAR_ACCESSIBILITY_FOCUS)))
        .andReturn(true);
    replayAll();
    CandidateWindowAccessibilityDelegate delegate =
        new CandidateWindowAccessibilityDelegate(view, provider);
    assertTrue(delegate.dispatchHoverEvent(createMotionEvent(MotionEvent.ACTION_HOVER_EXIT)));
    verifyAll();
  }

  @SmallTest
  public void testDispatchHoverEvent_MoveFromWordToWord() {
    // Action: Move
    // Position: From word to another word
    // Expectation:
    //   - Accessibility events TYPE_VIEW_HOVER_EXIT, ACTION_CLEAR_ACCESSIBILITY_FOCUS,
    //     TYPE_VIEW_HOVER_ENTER and ACTION_ACCESSIBILITY_FOCUS.
    //   - No methods in the view are called back.
    //   - MotionEvent is consumed.
    View view = createViewMock(View.class);
    CandidateWindowAccessibilityNodeProvider provider = createMockProvider(view);
    CandidateWindowAccessibilityDelegate delegate =
        new CandidateWindowAccessibilityDelegate(view, provider);
    // Preparing precondition.
    expect(provider.getCandidateWord(anyInt(), anyInt())).andReturn(createOptionalCandidateWord());
    provider.sendAccessibilityEventForCandidateWordIfAccessibilityEnabled(
        anyObject(CandidateWord.class), eq(AccessibilityEventCompat.TYPE_VIEW_HOVER_ENTER));
    expect(provider.performActionForCandidateWord(
        anyObject(CandidateWord.class),
        eq(AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS)))
        .andReturn(true);
    replayAll();
    delegate.dispatchHoverEvent(createMotionEvent(MotionEvent.ACTION_HOVER_ENTER));
    // Start recording
    resetAll();
    expect(provider.getCandidateWord(anyInt(), anyInt())).andReturn(createOptionalCandidateWord());
    provider.sendAccessibilityEventForCandidateWordIfAccessibilityEnabled(
        anyObject(CandidateWord.class), eq(AccessibilityEventCompat.TYPE_VIEW_HOVER_EXIT));
    expect(provider.performActionForCandidateWord(
        anyObject(CandidateWord.class),
        eq(AccessibilityNodeInfoCompat.ACTION_CLEAR_ACCESSIBILITY_FOCUS)))
        .andReturn(true);
    provider.sendAccessibilityEventForCandidateWordIfAccessibilityEnabled(
        anyObject(CandidateWord.class), eq(AccessibilityEventCompat.TYPE_VIEW_HOVER_ENTER));
    expect(provider.performActionForCandidateWord(
        anyObject(CandidateWord.class),
        eq(AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS)))
        .andReturn(true);
    replayAll();
    assertTrue(delegate.dispatchHoverEvent(createMotionEvent(MotionEvent.ACTION_HOVER_MOVE)));
    verifyAll();
  }

  @SmallTest
  public void testDispatchHoverEvent_MoveFromWordToNonWord() {
    // Action: Move
    // Position: From word to non-word
    // Expectation:
    //   - Accessibility events TYPE_VIEW_HOVER_EXIT, ACTION_CLEAR_ACCESSIBILITY_FOCUS,
    //   - No methods in the view are called back.
    //   - MotionEvent is consumed.
    View view = createViewMock(View.class);
    CandidateWindowAccessibilityNodeProvider provider = createMockProvider(view);
    CandidateWindowAccessibilityDelegate delegate =
        new CandidateWindowAccessibilityDelegate(view, provider);
    // Preparing precondition.
    expect(provider.getCandidateWord(anyInt(), anyInt())).andReturn(createOptionalCandidateWord());
    provider.sendAccessibilityEventForCandidateWordIfAccessibilityEnabled(
        anyObject(CandidateWord.class), eq(AccessibilityEventCompat.TYPE_VIEW_HOVER_ENTER));
    expect(provider.performActionForCandidateWord(
        anyObject(CandidateWord.class),
        eq(AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS)))
        .andReturn(true);
    replayAll();
    delegate.dispatchHoverEvent(createMotionEvent(MotionEvent.ACTION_HOVER_ENTER));
    // Start recording
    resetAll();
    expect(provider.getCandidateWord(anyInt(), anyInt()))
        .andReturn(Optional.<CandidateWord>absent());
    provider.sendAccessibilityEventForCandidateWordIfAccessibilityEnabled(
        anyObject(CandidateWord.class), eq(AccessibilityEventCompat.TYPE_VIEW_HOVER_EXIT));
    expect(provider.performActionForCandidateWord(
        anyObject(CandidateWord.class),
        eq(AccessibilityNodeInfoCompat.ACTION_CLEAR_ACCESSIBILITY_FOCUS)))
        .andReturn(true);
    replayAll();
    assertFalse(delegate.dispatchHoverEvent(createMotionEvent(MotionEvent.ACTION_HOVER_MOVE)));
    verifyAll();
  }
}
