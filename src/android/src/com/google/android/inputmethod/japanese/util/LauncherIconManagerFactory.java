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

package org.mozc.android.inputmethod.japanese.util;

import org.mozc.android.inputmethod.japanese.LauncherActivity;
import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.preference.PreferenceUtil;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Preconditions;

import android.content.ComponentName;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.preference.PreferenceManager;

/**
 * Manager of launcher icon's visibility.
 */
public class LauncherIconManagerFactory {

  /**
   * Interface for the manager.
   */
  public interface LauncherIconManager {
    /**
     * Updates launcher icon's visibility by checking the value in preferences or
     * by checking whether the app is (updated) system application or not.
     *
     * @param context The application's context.
     */
    public void updateLauncherIconVisibility(Context context);
  }

  @VisibleForTesting
  static class DefaultImplementation implements LauncherIconManager {

    @Override
    public void updateLauncherIconVisibility(Context context) {
      Preconditions.checkNotNull(context);
      SharedPreferences sharedPreferences = PreferenceManager.getDefaultSharedPreferences(context);
      boolean visible = shouldLauncherIconBeVisible(context, sharedPreferences);
      updateComponentEnableSetting(context, LauncherActivity.class, visible);
      sharedPreferences.edit()
          .putBoolean(PreferenceUtil.PREF_LAUNCHER_ICON_VISIBILITY_KEY, visible)
          .apply();
    }

    /**
     * Enables/disables component.
     *
     * @param context The application's context
     * @param component The component to be enabled/disabled
     * @param enabled true for enabled, false for disabled
     */
    private void updateComponentEnableSetting(
        Context context, Class<?> component, boolean enabled) {
      Preconditions.checkNotNull(context);
      Preconditions.checkNotNull(component);
      PackageManager packageManager = context.getPackageManager();
      ComponentName componentName = new ComponentName(context.getApplicationContext(), component);
      int newState = enabled ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                             : PackageManager.COMPONENT_ENABLED_STATE_DISABLED;
      if (newState != packageManager.getComponentEnabledSetting(componentName)) {
        packageManager.setComponentEnabledSetting(
            componentName, newState, PackageManager.DONT_KILL_APP);
      }
    }

    @VisibleForTesting
    static boolean shouldLauncherIconBeVisible(Context context,
                                               SharedPreferences sharedPreferences) {
      // NOTE: Both flags below can be true at the same time.
      boolean isSystemApplication = MozcUtil.isSystemApplication(context);
      boolean isUpdatedSystemApplication = MozcUtil.isUpdatedSystemApplication(context);
      if (sharedPreferences.contains(PreferenceUtil.PREF_LAUNCHER_ICON_VISIBILITY_KEY)) {
        return sharedPreferences.getBoolean(
            PreferenceUtil.PREF_LAUNCHER_ICON_VISIBILITY_KEY, true);
      } else {
        // If PREF_LAUNCHER_ICON_VISIBILITY_KEY is not set, we don't show launcher icon in
        // following conditions:
        if (isSystemApplication && !isUpdatedSystemApplication) {
          // System app (not updated) doesn't show the icon.
          return false;
        }
        if (isUpdatedSystemApplication
            && sharedPreferences.contains(
                PreferenceUtil.PREF_LAST_LAUNCH_ABI_INDEPENDENT_VERSION_CODE)) {
          // Workaround for updated system app from preinstalled 2.16.1955.3.
          // Preinstalled 2.16.1955.3 doesn't put PREF_LAUNCHER_ICON_VISIBILITY_KEY
          // unless preference screen is shown so checking PREF_LAUNCHER_ICON_VISIBILITY_KEY
          // doesn't work for the version. 
          // However PREF_LAST_LAUNCH_ABI_INDEPENDENT_VERSION_CODE is always written,
          // use the preference instaed.
          // If the preference  exists, this means the IME has been launched as a preinstall
          // app at least once.
          // Therefore we should take over the visibility (== hide the icon).
          return false;
        }
        return true;
      }
    }
  }

  private static LauncherIconManager defaultInstance = new DefaultImplementation();

  private LauncherIconManagerFactory() {}

  public static LauncherIconManager getDefaultInstance() {
    return defaultInstance;
  }
}
