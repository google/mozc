// Copyright 2010-2013, Google Inc.
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
import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.KeycodeConverter.KeyEventInterface;
import org.mozc.android.inputmethod.japanese.SymbolInputView.MajorCategory;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;

import java.util.List;

/**
 * Callback object for view evnets.
 *
 */
public interface ViewEventListener {
  /**
   * Called when KeyEvent is fired (by soft keyboard)
   *
   * @param mozcKeyEvent the key event to be processed by mozc server.
   * @param keyEvent the original key event
   * @param keyboardSpecification the keyboard specification used to input the key.
   * @param touchEventList {@code TouchEvent} instances related to this key event for logging
   *        usage stats.
   */
  public void onKeyEvent(ProtoCommands.KeyEvent mozcKeyEvent, KeyEventInterface keyEvent,
                         KeyboardSpecification keyboardSpecification,
                         List<? extends TouchEvent> touchEventList);

  /**
   * Called when Undo is fired (by soft keyboard).
   * @param touchEventList {@code TouchEvent} instances related to this undo for logging
   *        usage stats.
   */
  public void onUndo(List<? extends TouchEvent> touchEventList);

  /**
   * Called when a conversion candidate is selected.
   *
   * @param candidateId the id which Candidate and CandidateWord has.
   */
  public void onConversionCandidateSelected(int candidateId);

  /**
   * Called when a candidate on symbol input view is selected.
   * TODO(hidehiko): MajorCategory is in SymbolInputView. It's a bit weird structure. Refactor it.
   */
  public void onSymbolCandidateSelected(MajorCategory majorCategory, String candidate);

  /**
   * Called when a feedback event happens.
   * @param event the event which makes feedback.
   */
  public void onFireFeedbackEvent(FeedbackEvent event);

  /**
   * Called when the preedit should be submitted.
   */
  public void onSubmitPreedit();

  /**
   * Called when expanding suggestion is needed.
   */
  public void onExpandSuggestion();

  /**
   * Called when the menu dialog is shown.
   *
   * @param touchEventList {@code TouchEvent} instances which is related to this event
   *        for logging usage stats.
   */
  public void onShowMenuDialog(List<? extends TouchEvent> touchEventList);

  /**
   * Called when the symbol input view is shown.
   *
   * @param touchEventList {@code TouchEvent} instances which is related to this event
   *        for logging usage stats.
   */
  public void onShowSymbolInputView(List<? extends TouchEvent> touchEventList);

  /**
   * Called when the symbol input view is closed.
   */
  public void onCloseSymbolInputView();

  /**
   * Called when the hardware_composition_button is clicked.
   */
  public void onClickHardwareKeyboardCompositionModeButton();
}
