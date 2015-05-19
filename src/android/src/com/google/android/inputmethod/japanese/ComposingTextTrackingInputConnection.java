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

import android.annotation.TargetApi;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.inputmethod.CompletionInfo;
import android.view.inputmethod.CorrectionInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputConnection;

/**
 * The proxy of the {@code InputConnection} with tracking the composing text.
 *
 * There is no interface to extract the current composing text from {@code InputConnection}.
 * So, instead, we track all the message to {@code InputConnection} and keep the current composing
 * text as fall back. Note that the kept composing text might be different from the composing text
 * in the application because it can overwrite any events from IME freely in theory.
 * However, we can do nothing in such cases unfortunately, so just give them up.
 *
 */
public class ComposingTextTrackingInputConnection implements InputConnection {

  private final InputConnection baseConnection;
  private String composingText = "";

  public ComposingTextTrackingInputConnection(InputConnection baseConnection) {
    if (baseConnection == null) {
      throw new NullPointerException();
    }
    this.baseConnection = baseConnection;
  }

  public String getComposingText() {
    return composingText;
  }

  @Override
  public boolean beginBatchEdit() {
    return baseConnection.beginBatchEdit();
  }

  @Override
  public boolean clearMetaKeyStates(int states) {
    return baseConnection.clearMetaKeyStates(states);
  }

  @Override
  public boolean commitCompletion(CompletionInfo text) {
    return baseConnection.commitCompletion(text);
  }

  @TargetApi(11)
  @Override
  public boolean commitCorrection(CorrectionInfo correctionInfo) {
    return baseConnection.commitCorrection(correctionInfo);
  }

  @Override
  public boolean commitText(CharSequence text, int newCursorPosition) {
    return baseConnection.commitText(text, newCursorPosition);
  }

  @Override
  public boolean deleteSurroundingText(int beforeLength, int afterLength) {
    return baseConnection.deleteSurroundingText(beforeLength, afterLength);
  }

  @Override
  public boolean endBatchEdit() {
    return baseConnection.endBatchEdit();
  }

  @Override
  public boolean finishComposingText() {
    composingText = "";
    return baseConnection.finishComposingText();
  }

  @Override
  public int getCursorCapsMode(int reqModes) {
    return baseConnection.getCursorCapsMode(reqModes);
  }

  @Override
  public ExtractedText getExtractedText(ExtractedTextRequest request, int flags) {
    return baseConnection.getExtractedText(request, flags);
  }

  @Override
  public CharSequence getSelectedText(int flags) {
    return baseConnection.getSelectedText(flags);
  }

  @Override
  public CharSequence getTextAfterCursor(int n, int flags) {
    return baseConnection.getTextAfterCursor(n, flags);
  }

  @Override
  public CharSequence getTextBeforeCursor(int n, int flags) {
    return baseConnection.getTextBeforeCursor(n, flags);
  }

  @Override
  public boolean performContextMenuAction(int id) {
    return baseConnection.performContextMenuAction(id);
  }

  @Override
  public boolean performEditorAction(int editorAction) {
    return baseConnection.performEditorAction(editorAction);
  }

  @Override
  public boolean performPrivateCommand(String action, Bundle data) {
    return baseConnection.performPrivateCommand(action, data);
  }

  @Override
  public boolean reportFullscreenMode(boolean enabled) {
    return baseConnection.reportFullscreenMode(enabled);
  }

  @Override
  public boolean sendKeyEvent(KeyEvent event) {
    return baseConnection.sendKeyEvent(event);
  }

  @TargetApi(9)
  @Override
  public boolean setComposingRegion(int start, int end) {
    // Note: This method is introduced since API level 9. Mozc supports API level 7,
    // so we don't need to track the composing text by the invocation of this method.
    return baseConnection.setComposingRegion(start, end);
  }

  @Override
  public boolean setComposingText(CharSequence text, int newCursorPosition) {
    composingText = text == null ? "" : text.toString();
    return baseConnection.setComposingText(text, newCursorPosition);
  }

  @Override
  public boolean setSelection(int start, int end) {
    return baseConnection.setSelection(start, end);
  }

  /**
   * Returns the instance of ComposingTextTrackingInputConnection based on the given baseConnection.
   * This method will return:
   * - {@code null}, if the given connection is {@code null}.
   * - the given connection instance as is, if it is the instance of
   *   ComposingTextTrackingInputConnection.
   * - the new instance of ComposingTextTrackingInputConnection wrapping baseConnection, otherwise.
   */
  public static ComposingTextTrackingInputConnection newInstance(InputConnection baseConnection) {
    if (baseConnection == null) {
      return null;
    }
    if (baseConnection instanceof ComposingTextTrackingInputConnection) {
      // The InputConnection is already wrapped by ComposingTextTrackingInputConnection,
      // so we don't need to re-wrap it.
      return ComposingTextTrackingInputConnection.class.cast(baseConnection);
    }
    return new ComposingTextTrackingInputConnection(baseConnection);
  }
}
