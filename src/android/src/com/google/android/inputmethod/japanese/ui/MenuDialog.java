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

import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.mushroom.MushroomUtil;
import org.mozc.android.inputmethod.japanese.resources.R;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;
import com.google.common.collect.Lists;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface.OnDismissListener;
import android.content.DialogInterface.OnShowListener;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.os.IBinder;
import android.view.InflateException;
import android.view.WindowManager;

import java.util.Collections;
import java.util.List;

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
  @VisibleForTesting
  static class MenuDialogListenerHandler
      implements OnClickListener, OnDismissListener, OnShowListener {
    private final Context context;
    /** Table to convert from a menu item index to a string resource id. */
    private final int[] indexToIdTable;
    private final Optional<MenuDialogListener> listener;

    MenuDialogListenerHandler(
        Context context, int[] indexToIdMap, Optional<MenuDialogListener> listener) {
      this.context = Preconditions.checkNotNull(context);
      this.indexToIdTable = Preconditions.checkNotNull(indexToIdMap);
      this.listener = Preconditions.checkNotNull(listener);
    }

    @Override
    public void onShow(DialogInterface dialog) {
      if (!listener.isPresent()) {
        return;
      }
      listener.get().onShow(context);
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
      if (!listener.isPresent()) {
        return;
      }
      listener.get().onDismiss(context);
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
      if (!listener.isPresent() || indexToIdTable.length <= which) {
        return;
      }

      switch (indexToIdTable[which]) {
        case R.string.menu_item_input_method:
          listener.get().onShowInputMethodPickerSelected(context);
          break;
        case R.string.menu_item_preferences:
          listener.get().onLaunchPreferenceActivitySelected(context);
          break;
        case R.string.menu_item_mushroom:
          listener.get().onShowMushroomSelectionDialogSelected(context);
          break;
        default:
          MozcLog.e("Unknown menu index: " + which);
      }
    }
  }

  private final Optional<AlertDialog> dialog;
  private final MenuDialogListenerHandler listenerHandler;

  public MenuDialog(Context context, Optional<MenuDialogListener> listener) {
    Preconditions.checkNotNull(context);
    Preconditions.checkNotNull(listener);

    Resources resources = context.getResources();
    String appName = resources.getString(R.string.app_name);

    // R.string.menu_item_* resources needs to be formatted.
    List<Integer> menuItemIds = getEnabledMenuIds(context);
    int menuNum = menuItemIds.size();
    String[] menuTextList = new String[menuNum];
    int[] indexToIdTable = new int[menuNum];
    for (int i = 0; i < menuNum; ++i) {
      int id = menuItemIds.get(i);
      menuTextList[i] = resources.getString(id, appName);
      indexToIdTable[i] = id;
    }

    listenerHandler = new MenuDialogListenerHandler(
        context, indexToIdTable, listener);

    AlertDialog tempDialog = null;
    try {
      tempDialog = new AlertDialog.Builder(context)
          .setTitle(R.string.menu_dialog_title)
          .setItems(menuTextList, listenerHandler)
          .create();
      tempDialog.getWindow().addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
      tempDialog.getWindow().getAttributes().dimAmount = 0.60f;
      tempDialog.setOnDismissListener(listenerHandler);
      tempDialog.setOnShowListener(listenerHandler);
    } catch (InflateException e) {
      // Ignore the exception.
    }
    dialog = Optional.fromNullable(tempDialog);
  }

  public void show() {
    if (dialog.isPresent()) {
      dialog.get().show();
    }
  }

  public void dismiss() {
    if (dialog.isPresent()) {
      dialog.get().dismiss();
    }
  }

  public void setWindowToken(IBinder windowToken) {
    if (dialog.isPresent()) {
      MozcUtil.setWindowToken(Preconditions.checkNotNull(windowToken), dialog.get());
    }
  }

  @VisibleForTesting
  static List<Integer> getEnabledMenuIds(Context context) {
    // "Mushroom" item is enabled only when Mushroom-aware applications are available.
    PackageManager packageManager = Preconditions.checkNotNull(context).getPackageManager();
    boolean isMushroomEnabled = !MushroomUtil.getMushroomApplicationList(packageManager).isEmpty();

    List<Integer> menuItemIds = Lists.newArrayListWithCapacity(4);
    menuItemIds.add(R.string.menu_item_input_method);
    menuItemIds.add(R.string.menu_item_preferences);
    if (isMushroomEnabled) {
      menuItemIds.add(R.string.menu_item_mushroom);
    }
    return Collections.unmodifiableList(menuItemIds);
  }
}
