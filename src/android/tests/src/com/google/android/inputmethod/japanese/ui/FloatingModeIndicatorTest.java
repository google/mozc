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

package org.mozc.android.inputmethod.japanese.ui;

import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Output;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Preedit;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Preedit.Segment;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.SessionCommand;
import org.mozc.android.inputmethod.japanese.testing.ApiLevel;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;

import android.annotation.TargetApi;
import android.os.Handler;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.view.inputmethod.EditorInfo;

/**
 * Test for FloatingCandidateLayoutRenderer.
 */
@ApiLevel(21)
@TargetApi(21)
public class FloatingModeIndicatorTest extends InstrumentationTestCaseWithMock {

  private final Command emptyCommand = Command.getDefaultInstance();
  private final Command nonEmptyCommand = Command.newBuilder()
      .setOutput(Output.newBuilder()
          .setPreedit(Preedit.newBuilder()
              .addSegment(Segment.getDefaultInstance())
              .buildPartial())
          .buildPartial())
      .buildPartial();
  private final Command switchInputModeCommand = Command.newBuilder()
      .setInput(Input.newBuilder()
          .setType(Input.CommandType.SEND_COMMAND)
          .setCommand(SessionCommand.newBuilder()
              .setType(SessionCommand.CommandType.SWITCH_INPUT_MODE)
              .buildPartial())
          .buildPartial())
      .buildPartial();

  private void checkMessages(
      FloatingModeIndicator modeIndicator, boolean hasHideMessage, boolean hasShowMessage) {
    Handler handler = modeIndicator.handler;
    assertEquals(hasHideMessage, handler.hasMessages(FloatingModeIndicator.HIDE_MODE_INDICATOR));
    assertEquals(hasShowMessage, handler.hasMessages(FloatingModeIndicator.SHOW_MODE_INDICATOR));
  }

  private void checkMessageAndClear(Handler handler, int what) {
    assertTrue(handler.hasMessages(what));
    handler.removeMessages(what);
  }

  private void checkMessageAndInvoke(FloatingModeIndicator modeIndicator, int what) {
    checkMessageAndClear(modeIndicator.handler, what);
    modeIndicator.messageCallback.handleWhat(what);
  }

  @UiThreadTest
  @SmallTest
  public void testBasicBehavior() {
    View parentView = createViewMockBuilder(View.class).createNiceMock();
    FloatingModeIndicator modeIndicator = new FloatingModeIndicator(parentView);

    EditorInfo editorInfo = new EditorInfo();
    editorInfo.packageName = "test_package";

    assertEquals(View.GONE, modeIndicator.popup.getContentView().getVisibility());
    assertFalse(modeIndicator.isVisible());

    replayAll();

    // Show then hide indicator by onStartInputView().
    modeIndicator.onStartInputView(editorInfo);
    assertFalse(modeIndicator.isVisible());
    checkMessageAndInvoke(modeIndicator, FloatingModeIndicator.SHOW_MODE_INDICATOR);
    assertTrue(modeIndicator.isVisible());
    checkMessageAndInvoke(modeIndicator, FloatingModeIndicator.HIDE_MODE_INDICATOR);
    assertFalse(modeIndicator.isVisible());
    checkMessages(modeIndicator, false, false);

    // Show indicator by setCompositionMode().
    modeIndicator.setCompositionMode(CompositionMode.HALF_ASCII);
    assertFalse(modeIndicator.isVisible());
    checkMessageAndInvoke(modeIndicator, FloatingModeIndicator.SHOW_MODE_INDICATOR);
    assertTrue(modeIndicator.isVisible());
    assertSame(modeIndicator.abcIndicatorDrawable,
               modeIndicator.popup.getContentView().getDrawable());
    checkMessages(modeIndicator, true, false);

    // Change the indicator drawable by setCompositionMode().
    modeIndicator.setCompositionMode(CompositionMode.HIRAGANA);
    // show() is invoked instead of showWithDelay() since the indicator is already shown.
    checkMessages(modeIndicator, true, false);
    assertTrue(modeIndicator.isVisible());
    assertSame(modeIndicator.kanaIndicatorDrawable,
               modeIndicator.popup.getContentView().getDrawable());
    checkMessageAndInvoke(modeIndicator, FloatingModeIndicator.HIDE_MODE_INDICATOR);
    assertFalse(modeIndicator.isVisible());
    checkMessages(modeIndicator, false, false);

    // Set composition then indicator is hidden.
    modeIndicator.setCommand(nonEmptyCommand);
    assertFalse(modeIndicator.isVisible());
    checkMessages(modeIndicator, false, false);

    // Change the composition mode. Indicator doesn't appear since we have composition.
    modeIndicator.setCompositionMode(CompositionMode.HALF_ASCII);
    assertFalse(modeIndicator.isVisible());
    checkMessages(modeIndicator, false, false);

    // Clear composition then set composition mode.
    modeIndicator.setCommand(emptyCommand);
    modeIndicator.setCompositionMode(CompositionMode.HIRAGANA);
    checkMessageAndInvoke(modeIndicator, FloatingModeIndicator.SHOW_MODE_INDICATOR);
    assertTrue(modeIndicator.isVisible());
    checkMessages(modeIndicator, true, false);

    // SWITCH_INPUT_MODE should be ignored.
    modeIndicator.setCommand(switchInputModeCommand);
    assertTrue(modeIndicator.isVisible());
    checkMessages(modeIndicator, true, false);

    // Invoke remained messages.
    checkMessageAndInvoke(modeIndicator, FloatingModeIndicator.HIDE_MODE_INDICATOR);
    assertFalse(modeIndicator.isVisible());
    checkMessages(modeIndicator, false, false);

    verifyAll();
  }
}
