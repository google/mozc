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

package org.mozc.android.inputmethod.japanese;

import static org.easymock.EasyMock.isA;

import org.mozc.android.inputmethod.japanese.CandidateViewManager.KeyboardCandidateViewHeightListener;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Output;
import org.mozc.android.inputmethod.japanese.testing.ApiLevel;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.util.CursorAnchorInfoWrapper;
import org.mozc.android.inputmethod.japanese.view.Skin;
import com.google.common.base.Optional;

import android.annotation.TargetApi;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.View;
import android.view.animation.Animation;
import android.view.inputmethod.EditorInfo;

/**
 * Test for CandidateViewManager.
 */
public class CandidateViewManagerTest extends InstrumentationTestCaseWithMock {

  private Command createCommand(int candidateNum) {
    CandidateList.Builder candidateListBuilder = CandidateList.newBuilder();
    for (int i = 0; i < candidateNum; ++i) {
      candidateListBuilder.addCandidates(CandidateWord.getDefaultInstance());
    }

    return Command.newBuilder()
        .setOutput(Output.newBuilder()
            .setAllCandidateWords(candidateListBuilder.buildPartial())
            .buildPartial())
        .buildPartial();
  }

  @SmallTest
  public void testUpdateForKeyboardCandidateView() {
    CandidateView keyboardCandidateView = createViewMock(CandidateView.class);
    FloatingCandidateView floatingCandidateView = createViewMock(FloatingCandidateView.class);
    CandidateViewManager candidateViewManager =
        new CandidateViewManager(keyboardCandidateView, floatingCandidateView);

    assertFalse(candidateViewManager.isFloatingMode());

    {
      Command emptyCommand = createCommand(0);
      resetAll();
      keyboardCandidateView.startOutAnimation();
      replayAll();
      candidateViewManager.update(emptyCommand);
      verifyAll();
    }

    {
      Command nonEmptyCommand = createCommand(1);
      resetAll();
      keyboardCandidateView.update(nonEmptyCommand);
      keyboardCandidateView.startInAnimation();
      replayAll();
      candidateViewManager.update(nonEmptyCommand);
      verifyAll();
    }
  }

  @ApiLevel(21)
  @TargetApi(21)
  @SmallTest
  public void testUpdateForFloatingCandidateView() {
    CandidateView keyboardCandidateView = createViewMock(CandidateView.class);
    FloatingCandidateView floatingCandidateView = createViewMock(FloatingCandidateView.class);
    CandidateViewManager candidateViewManager =
        new CandidateViewManager(keyboardCandidateView, floatingCandidateView);

    resetAll();
    keyboardCandidateView.enableFoldButton(false);
    keyboardCandidateView.update(CandidateViewManager.EMPTY_COMMAND);
    keyboardCandidateView.setVisibility(View.GONE);
    floatingCandidateView.setEditorInfo(isA(EditorInfo.class));
    floatingCandidateView.setCursorAnchorInfo(isA(CursorAnchorInfoWrapper.class));
    floatingCandidateView.setCandidates(CandidateViewManager.EMPTY_COMMAND);
    floatingCandidateView.setVisibility(View.VISIBLE);
    replayAll();
    candidateViewManager.setNarrowMode(true);
    candidateViewManager.setAllowFloatingMode(true);
    candidateViewManager.setExtractedMode(false);
    verifyAll();
    assertTrue(candidateViewManager.isFloatingMode());

    {
      Command emptyCommand = createCommand(0);
      resetAll();
      floatingCandidateView.setCandidates(emptyCommand);
      replayAll();
      candidateViewManager.update(emptyCommand);
      verifyAll();
    }

    {
      Command nonEmptyCommand = createCommand(1);
      resetAll();
      floatingCandidateView.setCandidates(nonEmptyCommand);
      replayAll();
      candidateViewManager.update(nonEmptyCommand);
      verifyAll();
    }
  }

  @SmallTest
  public void testNarrowMode() {
    CandidateView keyboardCandidateView = createViewMock(CandidateView.class);
    FloatingCandidateView floatingCandidateView = createViewMock(FloatingCandidateView.class);
    CandidateViewManager candidateViewManager =
        new CandidateViewManager(keyboardCandidateView, floatingCandidateView);

    assertFalse(candidateViewManager.isFloatingMode());
    EditorInfo editorInfo = new EditorInfo();
    candidateViewManager.setEditorInfo(editorInfo);
    CursorAnchorInfoWrapper cursorAnchorInfo = new CursorAnchorInfoWrapper();
    candidateViewManager.setCursorAnchorInfo(cursorAnchorInfo);

    resetAll();
    keyboardCandidateView.enableFoldButton(false);
    replayAll();
    candidateViewManager.setNarrowMode(true);
    candidateViewManager.setAllowFloatingMode(false);
    verifyAll();
    assertFalse(candidateViewManager.isFloatingMode());

    resetAll();
    keyboardCandidateView.enableFoldButton(true);
    replayAll();
    candidateViewManager.setNarrowMode(false);
    candidateViewManager.setAllowFloatingMode(false);
    verifyAll();
    assertFalse(candidateViewManager.isFloatingMode());

    // Sets candidates
    Command command = createCommand(1);
    resetAll();
    keyboardCandidateView.update(command);
    keyboardCandidateView.startInAnimation();
    replayAll();
    candidateViewManager.update(command);
    verifyAll();

    if (FloatingCandidateView.isAvailable()) {
      resetAll();
      keyboardCandidateView.enableFoldButton(false);
      keyboardCandidateView.update(CandidateViewManager.EMPTY_COMMAND);
      keyboardCandidateView.setVisibility(View.GONE);
      floatingCandidateView.setCandidates(CandidateViewManager.EMPTY_COMMAND);
      floatingCandidateView.setCursorAnchorInfo(cursorAnchorInfo);
      floatingCandidateView.setEditorInfo(editorInfo);
      floatingCandidateView.setVisibility(View.VISIBLE);
      replayAll();
      candidateViewManager.setNarrowMode(true);
      candidateViewManager.setAllowFloatingMode(true);
      verifyAll();
      assertTrue(candidateViewManager.isFloatingMode());

      resetAll();
      keyboardCandidateView.enableFoldButton(true);
      keyboardCandidateView.update(CandidateViewManager.EMPTY_COMMAND);
      keyboardCandidateView.setVisibility(View.GONE);
      floatingCandidateView.setCandidates(CandidateViewManager.EMPTY_COMMAND);
      floatingCandidateView.setVisibility(View.GONE);
      replayAll();
      candidateViewManager.setNarrowMode(false);
      candidateViewManager.setAllowFloatingMode(true);
      verifyAll();
      assertFalse(candidateViewManager.isFloatingMode());
    }
  }

  @SmallTest
  public void testKeyboardCandidateViewAnimationListener() {
    CandidateView keyboardCandidateView = createViewMockBuilder(CandidateView.class)
        .createNiceMock();
    FloatingCandidateView floatingCandidateView = createViewMockBuilder(FloatingCandidateView.class)
        .createNiceMock();
    CandidateViewManager candidateViewManager =
        new CandidateViewManager(keyboardCandidateView, floatingCandidateView);

    ViewEventListener viewEventListener = createNiceMock(ViewEventListener.class);
    KeyboardCandidateViewHeightListener animationListener =
        createMock(KeyboardCandidateViewHeightListener.class);

    resetAll();
    keyboardCandidateView.setViewEventListener(viewEventListener);
    floatingCandidateView.setViewEventListener(viewEventListener);
    replayAll();
    candidateViewManager.setEventListener(viewEventListener, animationListener);
    verifyAll();

    Command nonEmptyCommand = createCommand(1);
    assertFalse(candidateViewManager.isFloatingMode());
    resetAll();
    animationListener.onExpanded();
    replayAll();
    candidateViewManager.update(nonEmptyCommand);
    verifyAll();

    Command emptyCommand = createCommand(0);
    resetAll();
    animationListener.onCollapse();
    replayAll();
    candidateViewManager.update(emptyCommand);
    verifyAll();

    if (FloatingCandidateView.isAvailable()) {
      candidateViewManager.setNarrowMode(true);
      candidateViewManager.setAllowFloatingMode(true);
      assertTrue(candidateViewManager.isFloatingMode());
      resetAll();
      replayAll();
      candidateViewManager.update(nonEmptyCommand);
      candidateViewManager.update(emptyCommand);
      verifyAll();
    }
  }

  @SmallTest
  public void testShowNumberKeyboard() {
    int candidateTextSize = 10;
    int descriptionTextSize = 5;
    ViewEventListener viewEventListener = createNiceMock(ViewEventListener.class);
    KeyboardCandidateViewHeightListener heightListener =
        createNiceMock(KeyboardCandidateViewHeightListener.class);
    InOutAnimatedFrameLayout.VisibilityChangeListener visibilityChangeListener =
        createNiceMock(InOutAnimatedFrameLayout.VisibilityChangeListener.class);

    Skin skin = new Skin();

    {  // Set a view and update.
      CandidateViewManager candidateViewManager = new CandidateViewManager(
              createViewMockBuilder(CandidateView.class).createNiceMock(),
              createViewMockBuilder(FloatingCandidateView.class).createNiceMock());
      CandidateView view = createViewMock(CandidateView.class);

      resetAll();
      view.setCandidateTextDimension(candidateTextSize, descriptionTextSize);
      view.setViewEventListener(viewEventListener);
      view.setOnVisibilityChangeListener(visibilityChangeListener);
      view.setSkin(skin);
      view.enableFoldButton(true);
      view.setInAnimation(isA(Animation.class));
      view.setOutAnimation(isA(Animation.class));
      replayAll();

      candidateViewManager.setCandidateTextDimension(candidateTextSize, descriptionTextSize);
      candidateViewManager.setEventListener(viewEventListener, heightListener);
      candidateViewManager.setOnVisibilityChangeListener(Optional.of(visibilityChangeListener));
      candidateViewManager.setSkin(skin);
      candidateViewManager.setNumberCandidateView(view);

      verifyAll();
    }

    {  // Update and set a view.
      CandidateViewManager candidateViewManager = new CandidateViewManager(
              createViewMockBuilder(CandidateView.class).createNiceMock(),
              createViewMockBuilder(FloatingCandidateView.class).createNiceMock());
      CandidateView view = createViewMock(CandidateView.class);

      resetAll();
      // Set default values when number keyboard is set.
      view.setSkin(Skin.getFallbackInstance());
      view.setOnVisibilityChangeListener(null);
      // Set actual values.
      view.setCandidateTextDimension(candidateTextSize, descriptionTextSize);
      view.setViewEventListener(viewEventListener);
      view.enableFoldButton(true);
      view.setOnVisibilityChangeListener(visibilityChangeListener);
      view.setSkin(skin);
      view.setInAnimation(isA(Animation.class));
      view.setOutAnimation(isA(Animation.class));
      replayAll();

      candidateViewManager.setNumberCandidateView(view);
      candidateViewManager.setCandidateTextDimension(candidateTextSize, descriptionTextSize);
      candidateViewManager.setEventListener(viewEventListener, heightListener);
      candidateViewManager.setOnVisibilityChangeListener(Optional.of(visibilityChangeListener));
      candidateViewManager.setSkin(skin);

      verifyAll();
    }
  }

  @SmallTest
  public void testSwitchNumberKeyboard() {
    CandidateView keyboardCandidateView = createViewMock(CandidateView.class);
    CandidateView numberCandidateView = createViewMock(CandidateView.class);
    CandidateViewManager candidateViewManager = new CandidateViewManager(
        keyboardCandidateView, createViewMockBuilder(FloatingCandidateView.class).createNiceMock());
    candidateViewManager.setNumberCandidateView(numberCandidateView);

    Command command = createCommand(1);

    {
      resetAll();
      keyboardCandidateView.update(CandidateViewManager.EMPTY_COMMAND);
      keyboardCandidateView.setVisibility(View.GONE);
      numberCandidateView.update(CandidateViewManager.EMPTY_COMMAND);
      numberCandidateView.setVisibility(View.GONE);
      replayAll();
      candidateViewManager.setNumberMode(true);
      verifyAll();

      resetAll();
      keyboardCandidateView.update(CandidateViewManager.EMPTY_COMMAND);
      keyboardCandidateView.setVisibility(View.GONE);
      replayAll();
      candidateViewManager.setNumberMode(false);
      verifyAll();
    }

    {
      resetAll();
      keyboardCandidateView.update(command);
      keyboardCandidateView.startInAnimation();
      replayAll();
      candidateViewManager.update(command);
      verifyAll();

      resetAll();
      keyboardCandidateView.update(CandidateViewManager.EMPTY_COMMAND);
      keyboardCandidateView.setVisibility(View.GONE);
      numberCandidateView.update(CandidateViewManager.EMPTY_COMMAND);
      numberCandidateView.setVisibility(View.GONE);
      replayAll();
      candidateViewManager.setNumberMode(true);
      verifyAll();

      resetAll();
      numberCandidateView.update(command);
      numberCandidateView.startInAnimation();
      replayAll();
      candidateViewManager.update(command);
      verifyAll();

      resetAll();
      keyboardCandidateView.update(CandidateViewManager.EMPTY_COMMAND);
      keyboardCandidateView.setVisibility(View.GONE);
      replayAll();
      candidateViewManager.setNumberMode(false);
      verifyAll();
    }
  }
}
