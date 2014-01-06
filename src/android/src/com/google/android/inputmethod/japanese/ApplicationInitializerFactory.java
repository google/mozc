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

import org.mozc.android.inputmethod.japanese.MozcUtil.TelephonyManagerInterface;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.preference.PreferenceUtil;
import org.mozc.android.inputmethod.japanese.resources.R;
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
  static final String PREF_LAST_LAUNCH_ABI_INDEPENDENT_VERSION_CODE =
      "pref_last_launch_abi_independent_version_code";
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
      if (!this.sharedPreferences.contains(PREF_LAST_LAUNCH_ABI_INDEPENDENT_VERSION_CODE)) {
        return Optional.absent();
      }
      return Optional.of(
          this.sharedPreferences.getInt(PREF_LAST_LAUNCH_ABI_INDEPENDENT_VERSION_CODE, 0));
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

    public Optional<Intent> initialize(boolean omitWelcomeActivity,
                                       boolean isDevChannel,
                                       boolean isWelcomeActivityPreferred,
                                       int abiIndependentVersionCode) {
      SharedPreferences.Editor editor = sharedPreferences.edit();
      Resources resources = context.getResources();
      try {
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
          storeDefaultFullscreenMode(
              sharedPreferences,
              getPortraitDisplayMetrics(resources.getDisplayMetrics(),
                                        resources.getConfiguration().orientation),
              resources.getDimension(R.dimen.fullscreen_threshold),
              resources.getDimension(R.dimen.ime_window_height_portrait),
              resources.getDimension(R.dimen.ime_window_height_landscape));

          // Run emoji provider type detection, so that the detected provider will be
          // used as the default values of the preference activity.
          EmojiProviderType.maybeSetDetectedEmojiProviderType(
              sharedPreferences, telephonyManager);
        }

        if (isDevChannel) {
          // Usage Stats: Make pref_other_usage_stats_key enabled when dev channel.
          editor.putBoolean(PreferenceUtil.PREF_OTHER_USAGE_STATS_KEY, true);
        }

        // Welcome Activity
        if (!isActivityShown && !omitWelcomeActivity && isWelcomeActivityPreferred) {
          editor.putBoolean(PREF_WELCOME_ACTIVITY_SHOWN, true);
          Intent intent = new Intent(context, FirstTimeLaunchActivity.class);
          intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
          return Optional.of(intent);
        }
        return Optional.absent();
      } finally {
        editor.remove(PREF_LAUNCHED_AT_LEAST_ONCE);
        editor.putInt(PREF_LAST_LAUNCH_ABI_INDEPENDENT_VERSION_CODE, abiIndependentVersionCode);
        editor.commit();
      }
    }

    /**
     * Returns a modified {@code DisplayMetrics} which equals to portrait modes's one.
     *
     * If current orientation is PORTRAIT, given {@code currentMetrics} is returned.
     * Otherwise {@code currentMetrics}'s {@code heightPixels} and {@code widthPixels} are swapped.
     *
     * Package private for testing purpose.
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
     *
     * Package private for testing purpose.
     */
    @VisibleForTesting
    static void storeDefaultFullscreenMode(
        SharedPreferences sharedPreferences, DisplayMetrics displayMetrics,
        float fullscreenThresholdInPixel,
        float portraitImeHeightInPixel, float landscapeImeHeightInPixel) {
      Preconditions.checkNotNull(sharedPreferences);
      Preconditions.checkNotNull(displayMetrics);

      SharedPreferences.Editor editor = sharedPreferences.edit();
      editor.putBoolean(
          "pref_portrait_fullscreen_key",
          displayMetrics.heightPixels - portraitImeHeightInPixel
              < fullscreenThresholdInPixel);
      editor.putBoolean(
          "pref_landscape_fullscreen_key",
          displayMetrics.widthPixels - landscapeImeHeightInPixel
              < fullscreenThresholdInPixel);
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
