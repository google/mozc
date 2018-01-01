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

import java.util.List;

import javax.annotation.Nullable;

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
  public void onKeyEvent(@Nullable ProtoCommands.KeyEvent mozcKeyEvent,
                         @Nullable KeyEventInterface keyEvent,
                         @Nullable KeyboardSpecification keyboardSpecification,
                         List<TouchEvent> touchEventList);

  /**
   * Called when Undo is fired (by soft keyboard).
   * @param touchEventList {@code TouchEvent} instances related to this undo for logging
   *        usage stats.
   */
  public void onUndo(List<TouchEvent> touchEventList);

  /**
   * Called when a conversion candidate is selected.
   *
   * @param candidateId the id which Candidate and CandidateWord has.
   * @param rowIndex index of row in which the candidate is. If absent no stats are sent.
   */
  public void onConversionCandidateSelected(int candidateId, Optional<Integer> rowIndex);

  /** Called when page down button is tapped. */
  public void onPageUp();

  /** Called when page down button is tapped. */
  public void onPageDown();

  /**
   * Called when a candidate on symbol input view is selected.
   */
  public void onSymbolCandidateSelected(SymbolMajorCategory majorCategory, String candidate,
                                        boolean updateHistory);

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
  // TODO(matsuzakit): Rename. onFlushTouchEventStats ?
  public void onShowMenuDialog(List<TouchEvent> touchEventList);

  /**
   * Called when the symbol input view is shown.
   *
   * @param touchEventList {@code TouchEvent} instances which is related to this event
   *        for logging usage stats.
   */
  public void onShowSymbolInputView(List<TouchEvent> touchEventList);

  /**
   * Called when the symbol input view is closed.
   */
  public void onCloseSymbolInputView();

  /**
   * Called when the hardware_composition_button is clicked.
   * @param mode new mode
   */
  public void onHardwareKeyboardCompositionModeChange(CompositionSwitchMode mode);

  /**
   * Called when the key for editor action is pressed.
   */
  public void onActionKey();

  /** Called when the narrow mode of the view is changed. */
  public void onNarrowModeChanged(boolean newNarrowMode);

  /**
   * Called when the keyboard layout preference should be updated.
   * <p>
   * The visible keyboard will also be updated as the result through a callback object.
   */
  public void onUpdateKeyboardLayoutAdjustment(
      ViewManagerInterface.LayoutAdjustment layoutAdjustment);

  /** Called when the mushroom selection dialog is shown. */
  public void onShowMushroomSelectionDialog();
}
