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

package org.mozc.android.inputmethod.japanese.ui;

import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.mushroom.MushroomUtil;
import org.mozc.android.inputmethod.japanese.resources.R;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface.OnDismissListener;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.os.IBinder;
import android.widget.ArrayAdapter;

/**
 * UI component implementation for the Mozc's menu dialog.
 *
 */
public class MenuDialog {

  /**
   * Listener interface for the menu dialog.
   */
  public static interface MenuDialogListener {

    /** Invoked when the dialog is shown. */
    public void onShow(Context context);

    /** Invoked when the dialog is dismissed. */
    public void onDismiss(Context context);

    /** Invoked when "Show Input Method Picker" item is selected. */
    public void onShowInputMethodPickerSelected(Context context);

    /** Invoked when "Launch Preference Activity" item is selected. */
    public void onLaunchPreferenceActivitySelected(Context context);

    /** Invoked when "Launch Mushroom" item is selected. */
    public void onShowMushroomSelectionDialogSelected(Context context);
  }

  /**
   * Internal implementation of callback invocation dispatching.
   */
  static class MenuDialogListenerHandler implements OnClickListener, OnDismissListener {
    private final Context context;
    private final MenuDialogListener listener;

    MenuDialogListenerHandler(Context context, MenuDialogListener listener) {
      this.context = context;
      this.listener = listener;
    }

    // TODO(hidehiko): use DialogInterface.OnShowListener when we get rid of API level 7.
    public void onShow(DialogInterface dialog) {
      if (listener == null) {
        return;
      }
      listener.onShow(context);
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
      if (listener == null) {
        return;
      }
      listener.onDismiss(context);
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
      if (listener == null) {
        return;
      }

      switch (which) {
        case INPUT_METHOD_PICKER_INDEX:
          listener.onShowInputMethodPickerSelected(context);
          break;
        case PREFERENCE_INDEX:
          listener.onLaunchPreferenceActivitySelected(context);
          break;
        case MUSHROOM_INDEX:
          listener.onShowMushroomSelectionDialogSelected(context);
          break;
        default:
          MozcLog.e("Unknown menu index: " + which);
      }
    }
  }

  /**
   * Adapter to handle enabling/disabling mushroom launching item.
   */
  static class MenuDialogAdapter extends ArrayAdapter<String> {
    MenuDialogAdapter(Context context, String[] menuDialogItems) {
      super(context, android.R.layout.select_dialog_item, android.R.id.text1, menuDialogItems);
    }

    @Override
    public boolean areAllItemsEnabled() {
      return false;
    }

    @Override
    public boolean isEnabled(int position) {
      if (position != MUSHROOM_INDEX) {
        return true;
      }

      // "Mushroom" item is enabled only when Mushroom-aware applications are available.
      PackageManager packageManager = getContext().getPackageManager();
      return !MushroomUtil.getMushroomApplicationList(packageManager).isEmpty();
    }
  }

  static final int INPUT_METHOD_PICKER_INDEX = 0;
  static final int PREFERENCE_INDEX = 1;
  static final int MUSHROOM_INDEX = 2;

  private final MenuDialogListenerHandler listenerHandler;
  private final AlertDialog dialog;

  public MenuDialog(Context context, MenuDialogListener listener) {
    this.listenerHandler = new MenuDialogListenerHandler(context, listener);
    this.dialog = createDialog(context, listenerHandler);
  }

  private static AlertDialog createDialog(
      Context context, MenuDialogListenerHandler listenerHandler) {
    // R.array.menu_dialog_items's resources needs to be formatted.
    Resources resources = context.getResources();
    String[] menuDialogItems = resources.getStringArray(R.array.menu_dialog_items);
    String appName = resources.getString(R.string.app_name);
    for (int i = 0; i < menuDialogItems.length; ++i) {
      menuDialogItems[i] = String.format(menuDialogItems[i], appName);
    }

    AlertDialog dialog = new AlertDialog.Builder(context)
        .setTitle(R.string.menu_dialog_title)
        .setAdapter(new MenuDialogAdapter(context, menuDialogItems), listenerHandler)
        .create();
    dialog.setOnDismissListener(listenerHandler);
    return dialog;
  }

  public void show() {
    // Note that unfortunately, we don't have a callback which is invoked when the dialog is
    // shown on API level 7. So, instead, we manually invoke the method here.
    listenerHandler.onShow(dialog);
    dialog.show();
  }

  public void dismiss() {
    dialog.dismiss();
  }

  public void setWindowToken(IBinder windowToken) {
    if (windowToken != null) {
      MozcUtil.setWindowToken(windowToken, dialog);
    } else {
      MozcLog.w("Unknown window token.");
    }
  }
}
