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

import org.mozc.android.inputmethod.japanese.MozcUtil.TelephonyManagerInterface;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.preference.PreferenceUtil;
import org.mozc.android.inputmethod.japanese.preference.PreferenceUtil.PreferenceManagerStaticInterface;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.util.LauncherIconManagerFactory.LauncherIconManager;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Build;
import android.preference.PreferenceManager;
import android.support.v4.app.NotificationCompat;
import android.util.DisplayMetrics;

import java.io.File;

/**
 * Factory class of application initializer.
 *
 * <p>The initializer should be invoked once when the application starts.
 */
public class ApplicationInitializerFactory {

  /**
   * Represents initialization status.
   *
   * <p>Typically they are calculated by using {@link SharedPreferences}.
   * The interface is prepared mainly for testing.
   */
  @VisibleForTesting
  public interface ApplicationInitializationStatus {
    /**
     * @deprecated should use {@link #getLastLaunchAbiIndependentVersionCode()} and
     *     {@link #isWelcomeActivityShownAtLeastOnce()}
     * @return true if the application has launched at least once. False means nothing (remember,
     *     this is a deprecated flag).
     */
    @Deprecated
    boolean isLaunchedAtLeastOnce();

    /**
     * @return the latest ABI independent version code of the IME which has launched last time.
     *     Absent if this is the first time (or just migrated from {@link #isLaunchedAtLeastOnce()})
     */
    Optional<Integer> getLastLaunchAbiIndependentVersionCode();

    /**
     * @return true if the "Welcome" activity has been shown at least once. False means nothing.
     */
    boolean isWelcomeActivityShownAtLeastOnce();
  }

  @VisibleForTesting
  static final String PREF_LAUNCHED_AT_LEAST_ONCE = "pref_launched_at_least_once";
  @VisibleForTesting
  static final String PREF_WELCOME_ACTIVITY_SHOWN = "pref_welcome_activity_shown";

  private static class ApplicationInitializerImpl implements ApplicationInitializationStatus {

    private final SharedPreferences sharedPreferences;

    private ApplicationInitializerImpl(SharedPreferences sharedPreferences) {
      Preconditions.checkNotNull(sharedPreferences);

      this.sharedPreferences = sharedPreferences;
    }

    @Deprecated
    @Override
    public boolean isLaunchedAtLeastOnce() {
      return this.sharedPreferences.getBoolean(PREF_LAUNCHED_AT_LEAST_ONCE, false);
    }

    @Override
    public Optional<Integer> getLastLaunchAbiIndependentVersionCode() {
      if (!this.sharedPreferences.contains(
          PreferenceUtil.PREF_LAST_LAUNCH_ABI_INDEPENDENT_VERSION_CODE)) {
        return Optional.absent();
      }
      return Optional.of(
          this.sharedPreferences.getInt(
              PreferenceUtil.PREF_LAST_LAUNCH_ABI_INDEPENDENT_VERSION_CODE, 0));
    }

    @Override
    public boolean isWelcomeActivityShownAtLeastOnce() {
      return this.sharedPreferences.getBoolean(PREF_WELCOME_ACTIVITY_SHOWN, false);
    }
  }

  /**
   * The entry point of the application.
   */
  public static class ApplicationInitializer {

    static final int LAUNCHED_AT_LEAST_ONCE_DEPRECATED_VERSION_CODE = 1429;

    final ApplicationInitializationStatus initializationStatus;
    final Context context;
    final SharedPreferences sharedPreferences;
    final TelephonyManagerInterface telephonyManager;

    private ApplicationInitializer(
        ApplicationInitializationStatus initializationStatus,
        Context context,
        SharedPreferences sharedPreferences,
        TelephonyManagerInterface telephonyManager) {
      this.initializationStatus = Preconditions.checkNotNull(initializationStatus);
      this.context = Preconditions.checkNotNull(context);
      this.sharedPreferences = Preconditions.checkNotNull(sharedPreferences);
      this.telephonyManager = Preconditions.checkNotNull(telephonyManager);
    }

    /**
     * Initializes the application.
     *
     * <p>Updates some preferences.
     * Here we use three preference items.
     * <ul>
     * <li>pref_welcome_activity_shown: True if the "Welcome" activity has shown at least once.
     * <li>pref_last_launch_abi_independent_version_code: The latest version number which
     * has launched at least once.
     * <li>pref_launched_at_least_once: Deprecated. True if the the IME has launched at least once.
     * </ul>
     * Some preferences should be set at the first time launch.
     * If the IME is a system application (preinstalled), it shouldn't show "Welcome" activity.
     * If an update is performed (meaning that the IME becomes non-system app),
     * the activity should be shown at the first time launch.
     *
     * We have to do migration process.
     * If pref_launched_at_least_once exists, pref_welcome_activity_shown is recognized as
     * true and pref_last_launch_abi_independent_version_code is recognized as
     * LAUNCHED_AT_LEAST_ONCE_DEPRECATED_VERSION_CODE. And then pref_launched_at_least_once is
     * removed.
     *
     * @param isSystemApplication true if the app is a system application (== preinstall)
     * @param isDevChannel true if the app is built for dev channel
     * @param isWelcomeActivityPreferred true if the configuration prefers to shown welcome activity
     *        if it's not been shown yet.
     * @param abiIndependentVersionCode ABI independent version code, typically obtained
     *        from {@link MozcUtil#getAbiIndependentVersionCode(Context)}
     *
     * @return if forwarding is needed Intent is returned. The caller side should invoke the Intent.
     */
    public Optional<Intent> initialize(boolean isSystemApplication,
                                       boolean isDevChannel,
                                       boolean isWelcomeActivityPreferred,
                                       int abiIndependentVersionCode,
                                       LauncherIconManager launcherIconManager,
                                       PreferenceManagerStaticInterface preferenceManager) {
      Preconditions.checkNotNull(launcherIconManager);
      Preconditions.checkNotNull(preferenceManager);
      SharedPreferences.Editor editor = sharedPreferences.edit();
      Resources resources = context.getResources();
      try {
        File tempDirectory = MozcUtil.getUserDictionaryExportTempDirectory(context);
        if (tempDirectory.isDirectory()) {
          MozcUtil.deleteDirectoryContents(tempDirectory);
        }

        Optional<Integer> lastVersionCode;
        boolean isActivityShown;
        if (initializationStatus.isLaunchedAtLeastOnce()) {
          // Migration scenario
          lastVersionCode = Optional.of(LAUNCHED_AT_LEAST_ONCE_DEPRECATED_VERSION_CODE);
          isActivityShown = true;
        } else {
          lastVersionCode = initializationStatus.getLastLaunchAbiIndependentVersionCode();
          isActivityShown = initializationStatus.isWelcomeActivityShownAtLeastOnce();
        }

        // Preferences: Update if this is the first launch
        if (!lastVersionCode.isPresent()) {
          // Store full-screen relating preferences.
          DisplayMetrics portraitMetrics = getPortraitDisplayMetrics(
              resources.getDisplayMetrics(), resources.getConfiguration().orientation);
          storeDefaultFullscreenMode(
              sharedPreferences, portraitMetrics.heightPixels, portraitMetrics.widthPixels,
              (int) Math.ceil(MozcUtil.getDimensionForOrientation(
                  resources, R.dimen.input_frame_height, Configuration.ORIENTATION_PORTRAIT)),
              (int) Math.ceil(MozcUtil.getDimensionForOrientation(
                  resources, R.dimen.input_frame_height, Configuration.ORIENTATION_LANDSCAPE)),
              resources.getDimensionPixelOffset(R.dimen.fullscreen_threshold));

          // Run emoji provider type detection, so that the detected provider will be
          // used as the default values of the preference activity.
          EmojiProviderType.maybeSetDetectedEmojiProviderType(
              sharedPreferences, telephonyManager);
        }
        // Update launcher icon visibility and relating preference.
        launcherIconManager.updateLauncherIconVisibility(context);
        // Save default preference to the storage.
        // NOTE: This method must NOT be called before updateLauncherIconVisibility() above.
        //       Above method requires PREF_LAUNCHER_ICON_VISIBILITY_KEY is not filled with
        //       the default value.
        //       If PREF_LAUNCHER_ICON_VISIBILITY_KEY is filled prior to
        //       updateLauncherIconVisibility(), the launcher icon will be unexpectedly shown
        //       when 2.16.1955.3 (preinstall version) is overwritten by PlayStore version.
        PreferenceUtil.setDefaultValues(
            preferenceManager, context, MozcUtil.isDebug(context),
            resources.getBoolean(R.bool.sending_information_features_enabled));

        if (isDevChannel) {
          // Usage Stats: Make pref_other_usage_stats_key enabled when dev channel.
          editor.putBoolean(PreferenceUtil.PREF_OTHER_USAGE_STATS_KEY, true);
          maybeShowNotificationForDevChannel(abiIndependentVersionCode,
              lastVersionCode);
        }

        // Welcome Activity
        if (!isActivityShown && !isSystemApplication && isWelcomeActivityPreferred) {
          editor.putBoolean(PREF_WELCOME_ACTIVITY_SHOWN, true);
          Intent intent = new Intent(context, FirstTimeLaunchActivity.class);
          intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
          return Optional.of(intent);
        }
        return Optional.absent();
      } finally {
        editor.remove(PREF_LAUNCHED_AT_LEAST_ONCE);
        editor.putInt(PreferenceUtil.PREF_LAST_LAUNCH_ABI_INDEPENDENT_VERSION_CODE,
                      abiIndependentVersionCode);
        editor.commit();
      }
    }

    private void maybeShowNotificationForDevChannel(
        int abiIndependentVersionCode, Optional<Integer> lastVersionCode) {
    }

    /**
     * Returns a modified {@code DisplayMetrics} which equals to portrait modes's one.
     *
     * If current orientation is PORTRAIT, given {@code currentMetrics} is returned.
     * Otherwise {@code currentMetrics}'s {@code heightPixels} and {@code widthPixels} are swapped.
     */
    @VisibleForTesting
    static DisplayMetrics getPortraitDisplayMetrics(DisplayMetrics currentMetrics,
                                                    int currnetOrientation) {
      Preconditions.checkNotNull(currentMetrics);

      DisplayMetrics result = new DisplayMetrics();
      result.setTo(currentMetrics);
      if (currnetOrientation == Configuration.ORIENTATION_LANDSCAPE) {
        result.heightPixels = currentMetrics.widthPixels;
        result.widthPixels = currentMetrics.heightPixels;
      }
      return result;
    }

    /**
     * Stores the default value of "fullscreen mode" to the shared preference.
     */
    @VisibleForTesting
    static void storeDefaultFullscreenMode(
        SharedPreferences sharedPreferences,
        int portraitDisplayHeight, int landscapeDisplayHeight,
        int portraitInputFrameHeight, int landscapeInputFrameHeight, int fullscreenThreshold) {
      Preconditions.checkNotNull(sharedPreferences);

      SharedPreferences.Editor editor = sharedPreferences.edit();
      editor.putBoolean(
          "pref_portrait_fullscreen_key",
          portraitDisplayHeight - portraitInputFrameHeight < fullscreenThreshold);
      editor.putBoolean(
          "pref_landscape_fullscreen_key",
          landscapeDisplayHeight - landscapeInputFrameHeight < fullscreenThreshold);
      editor.commit();
    }
  }

  public static ApplicationInitializer createInstance(Context context) {
    Preconditions.checkNotNull(context);

    SharedPreferences sharedPreferences = PreferenceManager.getDefaultSharedPreferences(context);
    return createInstance(new ApplicationInitializerImpl(sharedPreferences),
                          context, sharedPreferences, MozcUtil.getTelephonyManager(context));
  }

  @VisibleForTesting
  public static ApplicationInitializer createInstance(
      ApplicationInitializationStatus initializationStatus,
      Context context, SharedPreferences sharedPreferences,
      TelephonyManagerInterface telephonyManager) {
    return new ApplicationInitializer(
        Preconditions.checkNotNull(initializationStatus),
        Preconditions.checkNotNull(context),
        Preconditions.checkNotNull(sharedPreferences),
        Preconditions.checkNotNull(telephonyManager));
  }
}
