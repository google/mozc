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

package org.mozc.android.inputmethod.japanese.preference;

import org.mozc.android.inputmethod.japanese.ApplicationInitializerFactory;
import org.mozc.android.inputmethod.japanese.ApplicationInitializerFactory.ApplicationInitializer;
import org.mozc.android.inputmethod.japanese.DependencyFactory;
import org.mozc.android.inputmethod.japanese.HardwareKeyboardSpecification;
import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.preference.KeyboardPreviewDrawable.BitmapCache;
import org.mozc.android.inputmethod.japanese.preference.KeyboardPreviewDrawable.CacheReferenceKey;
import org.mozc.android.inputmethod.japanese.resources.R;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnClickListener;
import android.content.DialogInterface.OnDismissListener;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.os.Bundle;
import android.preference.PreferenceActivity;
import android.preference.PreferenceManager;
import android.provider.Settings;

/**
 * This class handles the preferences of mozc.
 *
 * The same information might be stored in both SharedPreference and Config.
 * In this case SharedPreference is the master and Config is just a copy.
 *
 * Note: Seems like due to the bug of Android 2.1, it is necessary to put this class
 *   into the same package as MozcService.
 *
 */
public class MozcBasePreferenceActivity extends PreferenceActivity {
  private static class ImeEnableDialogClickListener implements OnClickListener {
    private final Context context;

    ImeEnableDialogClickListener(Context context) {
      if (context == null) {
        throw new NullPointerException("context is null.");
      }
      this.context = context;
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
      if (which == DialogInterface.BUTTON_POSITIVE) {
        // Open the keyboard & language setting page.
        Intent intent = new Intent();
        intent.setAction(Settings.ACTION_INPUT_METHOD_SETTINGS);
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        context.startActivity(intent);
      }
    }
  }

  private static class ImeSwitchDialogListener
      implements OnClickListener, OnDismissListener, OnCancelListener {
    private final Context context;

    // This variable should be reset every dialog shown timing, but it is not supported
    // on API level 7. So, instead, we check all possible finishing pass.
    // TODO(hidehiko): use OnShownListener after we upgrade the API level.
    private boolean showInputMethodPicker = false;

    ImeSwitchDialogListener(Context context) {
      if (context == null) {
        throw new NullPointerException("context is null.");
      }
      this.context = context;
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
      showInputMethodPicker = (which == DialogInterface.BUTTON_POSITIVE);
    }

    @Override
    public void onCancel(DialogInterface arg0) {
      showInputMethodPicker = false;
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
      if (showInputMethodPicker) {
        // Send an input method picker shown event after the dismissing of this dialog is
        // completed. Otherwise, the new dialog will be dismissed, too.
        showInputMethodPicker = false;
        MozcUtil.requestShowInputMethodPicker(context);
      }
    }
  }

  @VisibleForTesting AlertDialog imeEnableDialog;
  @VisibleForTesting AlertDialog imeSwitchDialog;

  // Cache the SharedPreferences instance, otherwise PreferenceManager.getDefaultSharedPreferences
  // would try to read it from the storage every time.
  private SharedPreferences sharedPreferences;

  private final CacheReferenceKey cacheKey = new CacheReferenceKey();

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    sharedPreferences = PreferenceManager.getDefaultSharedPreferences(this);
    initializeAlertDialog();
  }

  @Override
  protected void onResume() {
    super.onResume();

    HardwareKeyboardSpecification.maybeSetDetectedHardwareKeyMap(
        sharedPreferences, getResources().getConfiguration(), false);
  }

  @Override
  protected void onPostResume() {
    super.onPostResume();
    onPostResumeInternal(ApplicationInitializerFactory.createInstance(this));
  }

  @VisibleForTesting
  void onPostResumeInternal(ApplicationInitializer initializer) {
    Context context = getApplicationContext();
    boolean omitWelcomeActivity = false;
    Optional<Intent> forwardIntent = initializer.initialize(
        omitWelcomeActivity,
        MozcUtil.isDevChannel(context),
        DependencyFactory.getDependency(getApplicationContext()).isWelcomeActivityPreferrable(),
        MozcUtil.getAbiIndependentVersionCode(context));
    if (forwardIntent.isPresent()) {
      startActivity(forwardIntent.get());
    } else {
      // After all resuming processes, we check the default IME status.
      // Note that we need to do this only when first launch activity is *not* launched.
      maybeShowAlertDialogIme();
    }
  }

  // Note: when an activity A starts another activity B, the order of their onStart/onStop
  // invocation sequence is:
  //   B's onStart -> A's onStop.
  // (of course, some other methods, such as onPause or onResume etc, are also invoked accordingly).
  // Thus, we can keep and pass the cached keyboard preview bitmaps by reference counting
  // in onStart/onStop methods from the single-pane preference top page to
  // software keyboard advanced settings, and vice versa.
  // Note that if a user tries to move an activity other than Mozc's preference even from
  // software keyboard advanced settings preference page, the counter will simply get 0, and all the
  // cached bitmap resources will be released.
  @Override
  protected void onStart() {
    super.onStart();
    // Tell bitmap cache that this Activity uses it.
    BitmapCache.getInstance().addReference(cacheKey);
  }

  @Override
  protected void onStop() {
    // Release all the bitmap cache (if necessary) to utilize the used memory.
    BitmapCache.getInstance().removeReference(cacheKey);
    super.onStop();
  }

  private void initializeAlertDialog() {
    Resources resource = getResources();
    imeEnableDialog = createAlertDialog(
        this,
        R.string.pref_ime_enable_alert_title,
        resource.getString(R.string.pref_ime_enable_alert_message,
                           resource.getString(R.string.app_name)),
        new ImeEnableDialogClickListener(this));
    ImeSwitchDialogListener listener = new ImeSwitchDialogListener(this);
    imeSwitchDialog = createAlertDialog(
        this,
        R.string.pref_ime_switch_alert_title,
        resource.getString(R.string.pref_ime_switch_alert_message,
                           resource.getString(R.string.app_name)),
        listener);
    imeSwitchDialog.setOnCancelListener(listener);
    imeSwitchDialog.setOnDismissListener(listener);
  }

  private static AlertDialog createAlertDialog(
      Context context, int titleResourceId, String message, OnClickListener clickListener) {
    AlertDialog.Builder builder = new AlertDialog.Builder(context);
    builder.setTitle(titleResourceId);
    builder.setMessage(message);
    builder.setPositiveButton(R.string.pref_ime_alert_next, clickListener);
    builder.setNegativeButton(R.string.pref_ime_alert_cancel, clickListener);
    return builder.create();
  }

  /**
   * Check the current default IME status, and show alerting dialog if necessary.
   */
  private void maybeShowAlertDialogIme() {
    if (imeEnableDialog.isShowing() || imeSwitchDialog.isShowing()) {
      // Either dialog is already shown, right now. So just skip the check.
      return;
    }

    Context context = getApplicationContext();
    if (!MozcUtil.isMozcEnabled(context)) {
      imeEnableDialog.show();
      return;
    }
    if (!MozcUtil.isMozcDefaultIme(context)) {
      imeSwitchDialog.show();
      return;
    }
  }
}
