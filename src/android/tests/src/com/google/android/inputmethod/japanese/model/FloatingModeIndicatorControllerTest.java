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

package org.mozc.android.inputmethod.japanese.model;

import static org.easymock.EasyMock.isA;

import org.mozc.android.inputmethod.japanese.model.FloatingModeIndicatorController.ControllerListener;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;

import android.test.suitebuilder.annotation.SmallTest;
import android.view.inputmethod.EditorInfo;

/**
 * Test for FloatingModeIndicatorController.
 */
public class FloatingModeIndicatorControllerTest extends InstrumentationTestCaseWithMock {

  private FloatingModeIndicatorController createAndInitializeControllerWithMockListener(
      long time, ControllerListener mockListener) {
    FloatingModeIndicatorController controller = new FloatingModeIndicatorController(mockListener);
    EditorInfo editorInfo = new EditorInfo();
    editorInfo.packageName = "teset_package_name";

    resetAll();
    mockListener.showWithDelay(CompositionMode.HIRAGANA);
    replayAll();
    controller.onStartInputView(time, editorInfo);
    verifyAll();

    return controller;
  }

  @SmallTest
  public void testOnStartInputView() {
    ControllerListener listener = createMock(ControllerListener.class);
    FloatingModeIndicatorController controller = new FloatingModeIndicatorController(listener);

    int minimumInterval =
        FloatingModeIndicatorController.MIN_INTERVAL_TO_SHOW_INDICATOR_ON_START_MILLIS;
    EditorInfo editorInfo1 = new EditorInfo();
    editorInfo1.packageName = "package1";

    long currentTime = 0L;

    resetAll();
    listener.showWithDelay(CompositionMode.HIRAGANA);
    replayAll();
    controller.onStartInputView(currentTime, editorInfo1);
    verifyAll();

    resetAll();
    replayAll();
    currentTime += 1;
    controller.onStartInputView(currentTime, editorInfo1);
    currentTime += minimumInterval - 1;
    controller.onStartInputView(currentTime, editorInfo1);
    verifyAll();

    resetAll();
    listener.showWithDelay(CompositionMode.HIRAGANA);
    replayAll();
    currentTime += minimumInterval;
    controller.onStartInputView(currentTime, editorInfo1);
    verifyAll();

    EditorInfo editorInfo2 = new EditorInfo();
    editorInfo2.packageName = "package2";

    resetAll();
    listener.showWithDelay(CompositionMode.HIRAGANA);
    replayAll();
    currentTime += 1;
    controller.onStartInputView(currentTime, editorInfo2);
    verifyAll();
  }

  @SmallTest
  public void testStabilizeCursorPosition() {
    ControllerListener listener = createMock(ControllerListener.class);
    long currentTime = 0L;
    FloatingModeIndicatorController controller =
        createAndInitializeControllerWithMockListener(currentTime, listener);

    assertFalse(controller.isCursorPositionStabilized(currentTime));
    controller.markCursorPositionStabilized();
    assertTrue(controller.isCursorPositionStabilized(currentTime));

    // setCompositionMode() unstabilizes the cursor position.
    resetAll();
    listener.showWithDelay(CompositionMode.HALF_ASCII);
    replayAll();
    currentTime += 1;
    controller.setCompositionMode(currentTime, CompositionMode.HALF_ASCII);
    assertFalse(controller.isCursorPositionStabilized(currentTime));
    controller.markCursorPositionStabilized();
    assertTrue(controller.isCursorPositionStabilized(currentTime));
  }

  @SmallTest
  public void testOnCursorAnchorInfoChanged() {
    ControllerListener listener = createMock(ControllerListener.class);
    long currentTime = 0L;
    FloatingModeIndicatorController controller =
        createAndInitializeControllerWithMockListener(currentTime, listener);

    int stabilizedMaxTime = FloatingModeIndicatorController.MAX_UNSTABLE_TIME_ON_START_MILLIS;

    resetAll();
    listener.showWithDelay(isA(CompositionMode.class));
    replayAll();
    currentTime = 1;
    controller.onCursorAnchorInfoChanged(currentTime);
    verifyAll();
    assertFalse(controller.isCursorPositionStabilized(currentTime));

    resetAll();
    listener.showWithDelay(isA(CompositionMode.class));
    replayAll();
    currentTime = stabilizedMaxTime - 1;
    controller.onCursorAnchorInfoChanged(currentTime);
    verifyAll();
    assertFalse(controller.isCursorPositionStabilized(currentTime));

    resetAll();
    replayAll();
    currentTime = stabilizedMaxTime;
    controller.onCursorAnchorInfoChanged(currentTime);
    verifyAll();
    assertTrue(controller.isCursorPositionStabilized(currentTime));
  }

  @SmallTest
  public void testSetCompositionMode() {
    ControllerListener listener = createMock(ControllerListener.class);
    long currentTime = 0L;
    FloatingModeIndicatorController controller =
        createAndInitializeControllerWithMockListener(currentTime, listener);
    int stabilizedMaxTime = FloatingModeIndicatorController.MAX_UNSTABLE_TIME_ON_START_MILLIS;

    // Nothing happens since composition mode is not changed.
    resetAll();
    replayAll();
    currentTime += stabilizedMaxTime;
    controller.setCompositionMode(currentTime, CompositionMode.HIRAGANA);
    verifyAll();
    assertTrue(controller.isCursorPositionStabilized(currentTime));

    // Composition mode is changed.
    resetAll();
    listener.showWithDelay(CompositionMode.HALF_ASCII);
    replayAll();
    currentTime += stabilizedMaxTime;
    controller.setCompositionMode(currentTime, CompositionMode.HALF_ASCII);
    verifyAll();
    assertFalse(controller.isCursorPositionStabilized(currentTime));
  }

  @SmallTest
  public void testSetHasComposition() {
    ControllerListener listener = createMock(ControllerListener.class);
    long currentTime = 0L;
    FloatingModeIndicatorController controller =
        createAndInitializeControllerWithMockListener(currentTime, listener);

    // Nothing happens since composition state is not changed.
    resetAll();
    replayAll();
    currentTime += 1;
    controller.setHasComposition(currentTime, false);
    verifyAll();

    // Hide indicator since there is a composition.
    resetAll();
    listener.hide();
    replayAll();
    currentTime += 1;
    controller.setHasComposition(currentTime, true);
    verifyAll();

    // Nothing happens since composition state is not changed.
    resetAll();
    replayAll();
    currentTime += 1;
    controller.setHasComposition(currentTime, true);
    verifyAll();
  }
}
