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

import org.mozc.android.inputmethod.japanese.FeedbackManager.FeedbackEvent;
import org.mozc.android.inputmethod.japanese.KeycodeConverter.KeyEventInterface;
import org.mozc.android.inputmethod.japanese.hardwarekeyboard.HardwareKeyboard.CompositionSwitchMode;
import org.mozc.android.inputmethod.japanese.keyboard.Keyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.model.SymbolMajorCategory;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import java.util.List;

import javax.annotation.Nullable;

/**
 * This class delegates all method calls to a ViewEventListener, passed to the constructor.
 * Typical usage is to hook/override some of listener's methods to change their behavior.
 *
 * <pre>
 * {@code
 * void foo(ViewEventListener listener) {
 *   childView.setListener(new ViewEventDelegator(listener) {
 *     @Override
 *     public void onFireFeedbackEvent(FeedbackEvent event) {
 *       // Disables all the feedback event.
 *     }
 *   })
 * }
 * }
 * </pre>
 *
 */
@SuppressWarnings("javadoc")
public abstract class ViewEventDelegator implements ViewEventListener {

  private final ViewEventListener delegated;

  public ViewEventDelegator(ViewEventListener delegated) {
    this.delegated = delegated;
  }

  @Override
  public void onKeyEvent(@Nullable ProtoCommands.KeyEvent mozcKeyEvent,
                         @Nullable KeyEventInterface keyEvent,
                         @Nullable KeyboardSpecification keyboardSpecification,
                         List<TouchEvent> touchEventList) {
    delegated.onKeyEvent(mozcKeyEvent, keyEvent, keyboardSpecification, touchEventList);
  }

  @Override
  public void onUndo(List<TouchEvent> touchEventList) {
    delegated.onUndo(touchEventList);
  }

  @Override
  public void onConversionCandidateSelected(int candidateId, Optional<Integer> rowIndex) {
    delegated.onConversionCandidateSelected(candidateId, Preconditions.checkNotNull(rowIndex));
  }

  @Override
  public void onPageUp() {
    delegated.onPageUp();
  }

  @Override
  public void onPageDown() {
    delegated.onPageDown();
  }

  @Override
  public void onSymbolCandidateSelected(SymbolMajorCategory majorCategory, String candidate,
                                        boolean updateHistory) {
    delegated.onSymbolCandidateSelected(majorCategory, candidate, updateHistory);
  }

  @Override
  public void onFireFeedbackEvent(FeedbackEvent event) {
    delegated.onFireFeedbackEvent(event);
  }

  @Override
  public void onSubmitPreedit() {
    delegated.onSubmitPreedit();
  }

  @Override
  public void onExpandSuggestion() {
    delegated.onExpandSuggestion();
  }

  @Override
  public void onShowMenuDialog(List<TouchEvent> touchEventList) {
    delegated.onShowMenuDialog(touchEventList);
  }

  @Override
  public void onShowSymbolInputView(List<TouchEvent> touchEventList) {
    delegated.onShowSymbolInputView(touchEventList);
  }

  @Override
  public void onCloseSymbolInputView() {
    delegated.onCloseSymbolInputView();
  }

  @Override
  public void onHardwareKeyboardCompositionModeChange(CompositionSwitchMode mode) {
    delegated.onHardwareKeyboardCompositionModeChange(mode);
  }

  @Override
  public void onActionKey() {
    delegated.onActionKey();
  }

  @Override
  public void onNarrowModeChanged(boolean newNarrowMode) {
    delegated.onNarrowModeChanged(newNarrowMode);
  }

  @Override
  public void onUpdateKeyboardLayoutAdjustment(
      ViewManagerInterface.LayoutAdjustment layoutAdjustment) {
    delegated.onUpdateKeyboardLayoutAdjustment(layoutAdjustment);
  }

  @Override
  public void onShowMushroomSelectionDialog() {
    delegated.onShowMushroomSelectionDialog();
  }
}
