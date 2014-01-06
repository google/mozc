// Copyright 2010-2014, Google Inc.
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
import org.mozc.android.inputmethod.japanese.HardwareKeyboard.CompositionSwitchMode;
import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.KeycodeConverter.KeyEventInterface;
import org.mozc.android.inputmethod.japanese.SymbolInputView.MajorCategory;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;

import java.util.List;

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
 *   }
 * }
 * }
 * </pre>
 *
 */
public abstract class ViewEventDelegator implements ViewEventListener {
  private final ViewEventListener delegated;

  public ViewEventDelegator(ViewEventListener delegated) {
    this.delegated = delegated;
  }

  @Override
  public void onKeyEvent(ProtoCommands.KeyEvent mozcKeyEvent, KeyEventInterface keyEvent,
                         KeyboardSpecification keyboardSpecification,
                         List<? extends TouchEvent> touchEventList) {
    delegated.onKeyEvent(mozcKeyEvent, keyEvent, keyboardSpecification, touchEventList);
  }

  @Override
  public void onUndo(List<? extends TouchEvent> touchEventList) {
    delegated.onUndo(touchEventList);
  }

  @Override
  public void onConversionCandidateSelected(int candidateId) {
    delegated.onConversionCandidateSelected(candidateId);
  }

  @Override
  public void onSymbolCandidateSelected(MajorCategory majorCategory, String candidate) {
    delegated.onSymbolCandidateSelected(majorCategory, candidate);
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
  public void onShowMenuDialog(List<? extends TouchEvent> touchEventList) {
    delegated.onShowMenuDialog(touchEventList);
  }

  @Override
  public void onShowSymbolInputView(List<? extends TouchEvent> touchEventList) {
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
}
