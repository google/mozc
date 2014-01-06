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

import org.mozc.android.inputmethod.japanese.mushroom.MushroomUtil;
import org.mozc.android.inputmethod.japanese.ui.MenuDialog.MenuDialogListener;

import android.content.Context;
import android.content.Intent;
import android.inputmethodservice.InputMethodService;
import android.view.inputmethod.InputConnection;

/**
 * Real implementation of MozcDialogListener.
 *
 */
class MozcMenuDialogListenerImpl implements MenuDialogListener {
  private final InputMethodService inputMethodService;
  private boolean showInputMethodPicker = false;

  MozcMenuDialogListenerImpl(InputMethodService inputMethodService) {
    this.inputMethodService = inputMethodService;
  }

  @Override
  public void onShow(Context context) {
    showInputMethodPicker = false;
  }

  @Override
  public void onDismiss(Context context) {
    if (showInputMethodPicker) {
      // Send a message to show the input method picker dialog.
      if (!MozcUtil.requestShowInputMethodPicker(context)) {
        MozcLog.e("Failed to send message to launch the input method picker dialog.");
      }
    }
  }

  @Override
  public void onShowInputMethodPickerSelected(Context context) {
    // We can't show the input method picker here, due to some event handling in android
    // framework. So, we postpone it and invoke at dismissing timing of this dialog.
    showInputMethodPicker = true;
  }

  @Override
  public void onLaunchPreferenceActivitySelected(Context context) {
    // Launch the preference activity.
    Intent intent =
        new Intent(context,
                   DependencyFactory.getDependency(context).getPreferenceActivityClass());
    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
    context.startActivity(intent);
  }

  @Override
  public void onShowMushroomSelectionDialogSelected(Context context) {
    // Reset the composing text, otherwise the composing text will be committed automatically
    // and as the result the user would see the duplicated committing.
    InputConnection inputConnection = inputMethodService.getCurrentInputConnection();
    String composingText = "";
    if (inputConnection != null) {
      if (inputConnection instanceof ComposingTextTrackingInputConnection) {
        composingText =
            ComposingTextTrackingInputConnection.class.cast(inputConnection).getComposingText();
      }
      inputConnection.setComposingText("", MozcUtil.CURSOR_POSITION_TAIL);
    }

    // Launch the activity.
    Intent intent = MushroomUtil.createMushroomSelectionActivityLaunchingIntent(
        context, inputMethodService.getCurrentInputEditorInfo().fieldId, composingText);
    context.startActivity(intent);

    // TODO(hidehiko): Do we need to logging?
  }
}
