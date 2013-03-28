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

import org.mozc.android.inputmethod.japanese.preference.PreferenceUtil;
import org.mozc.android.inputmethod.japanese.resources.R;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.text.style.URLSpan;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.Window;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.TextView;

/**
 * This activity should be launched only when a user launched the IME as the first time.
 *
 */
public class FirstTimeLaunchActivity extends Activity {
  /**
   * A listener to catch the change of the check box.
   */
  static class SendUsageStatsChangeListener implements OnCheckedChangeListener {
    private final SharedPreferences sharedPreferences;

    SendUsageStatsChangeListener(SharedPreferences sharedPreferences) {
      this.sharedPreferences = sharedPreferences;
    }

    @Override
    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
      sharedPreferences.edit()
          .putBoolean(PreferenceUtil.PREF_OTHER_USAGE_STATS_KEY, isChecked)
          .commit();
    }
  }

  /**
   * A listener to catch the change of the SharedPreferences.
   */
  class UpdateViewListener implements OnSharedPreferenceChangeListener {
    @Override
    public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
      updateView(sharedPreferences);
    }
  }

  private SharedPreferences sharedPreferences;

  // We have to keep the instance in order to unregister this.
  OnSharedPreferenceChangeListener updateViewListener = new UpdateViewListener();

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    requestWindowFeature(Window.FEATURE_NO_TITLE);
    sharedPreferences = PreferenceManager.getDefaultSharedPreferences(this);
    initializeContentView(getResources().getBoolean(R.bool.sending_information_features_enabled),
                          sharedPreferences);
  }

  void initializeContentView(boolean sendingInformationFeaturesEnabled,
                             SharedPreferences sharedPreferences) {
    setContentView(R.layout.first_time_launch);

    // Fill strings, which needs replacement.
    Resources resources = getResources();
    TextView thankYouTextView = TextView.class.cast(findViewById(R.id.description_thank_you));
    thankYouTextView.setText(resources.getString(R.string.firsttime_description_thank_you,
                                                 resources.getString(R.string.app_full_name)));
    if (sendingInformationFeaturesEnabled) {
      findViewById(R.id.usage_stats_views).setVisibility(View.VISIBLE);
      TextView usageStatsTextView = TextView.class.cast(findViewById(R.id.description_usage_stats));
      usageStatsTextView.setText(resources.getString(R.string.firsttime_description_usage_stats,
                                                     resources.getString(
                                                         R.string.developer_organization)));

      CheckBox usageStatsCheckBox = CheckBox.class.cast(findViewById(R.id.send_usage_stats));
      usageStatsCheckBox.setOnCheckedChangeListener(
          new SendUsageStatsChangeListener(sharedPreferences));
      if (MozcUtil.isDevChannel(this)) {
        usageStatsCheckBox.setEnabled(false);
        findViewById(R.id.send_usage_stats_devchannel_description).setVisibility(View.VISIBLE);
      }
    } else {
      findViewById(R.id.usage_stats_views).setVisibility(View.GONE);
    }
    findViewById(R.id.close_button).setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        finish();  // Close this activity.
      }
    });
    initializeAnchorTextView(
        R.id.link_terms_of_service,
        R.string.pref_about_terms_of_service_url,
        R.string.pref_about_terms_of_service_title);
    initializeAnchorTextView(
        R.id.link_privacy_policy,
        R.string.pref_about_privacy_policy_url,
        R.string.pref_about_privacy_policy_title);
  }

  private void initializeAnchorTextView(int textViewId, int urlId, int descriptionId) {
    Resources resources = getResources();
    TextView textView = TextView.class.cast(findViewById(textViewId));
    SpannableString spannable = new SpannableString(resources.getString(descriptionId));
    spannable.setSpan(
        new URLSpan(resources.getString(urlId)),
        0, spannable.length(),
        Spanned.SPAN_INCLUSIVE_INCLUSIVE);
    textView.setText(spannable);
    // Make the URLSpan clickable.
    textView.setMovementMethod(LinkMovementMethod.getInstance());
  }

  @Override
  protected void onResume() {
    super.onResume();
    updateView(sharedPreferences);
    sharedPreferences.registerOnSharedPreferenceChangeListener(updateViewListener);
  }

  @Override
  protected void onPause() {
    sharedPreferences.unregisterOnSharedPreferenceChangeListener(updateViewListener);
    super.onPause();
  }

  private void updateView(SharedPreferences sharedPreferences) {
    CheckBox.class.cast(findViewById(R.id.send_usage_stats)).setChecked(
        sharedPreferences.getBoolean(PreferenceUtil.PREF_OTHER_USAGE_STATS_KEY, false));
  }

  /**
   * Processes something which should be done at the first launch.
   * If this is not the first launch, does nothing.
   *
   * This method is static because this is also called from MozcPreference.
   * @return {@code true} if activity has been started.
   */
  public static boolean maybeProcessFirstTimeLaunchAction(
      SharedPreferences sharedPreferences, Context context) {
    if (sharedPreferences == null) {
      MozcLog.w("sharedPreferences must be non-null.");
      return false;
    }
    if (sharedPreferences.getBoolean("pref_launched_at_least_once", false)) {
      return false;
    }

    // Make pref_other_usage_stats_key enabled when dev channel.
    if (MozcUtil.isDevChannel(context)) {
      sharedPreferences.edit()
          .putBoolean(PreferenceUtil.PREF_OTHER_USAGE_STATS_KEY, true)
          .commit();
    }

    // TODO(matsuzakit): Following block should be extracted into separate class,
    //                   like "Main" class (surely better name should be used).
    Resources resources = context.getResources();
    storeDefaultFullscreenMode(
        sharedPreferences,
        getPortraitDisplayMetrics(resources.getDisplayMetrics(),
                                  resources.getConfiguration().orientation),
        resources.getDimension(R.dimen.fullscreen_threshold),
        resources.getDimension(R.dimen.ime_window_height_portrait),
        resources.getDimension(R.dimen.ime_window_height_landscape));

    // Turn on pref_launched_at_least_once flag after all other preference relating
    // processes are done.
    sharedPreferences.edit().putBoolean("pref_launched_at_least_once", true).commit();

    Intent intent = new Intent(context, FirstTimeLaunchActivity.class);
    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
    context.startActivity(intent);
    return true;
  }

  /**
   * Stores the default value of "fullscreen mode" to the shared preference.
   *
   * Package private for testing purpose.
   */
  static void storeDefaultFullscreenMode(
      SharedPreferences sharedPreferences, DisplayMetrics displayMetrics,
      float fullscreenThresholdInPixel,
      float portraitImeHeightInPixel, float landscapeImeHeightInPixel) {
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

  /**
   * Returns a modified {@code DisplayMetrics} which equals to portrait modes's one.
   *
   * If current orientation is PORTRAIT, given {@code currentMetrics} is returned.
   * Otherwise {@code currentMetrics}'s {@code heightPixels} and {@code widthPixels} are swapped.
   *
   * Package private for testing purpose.
   */
  static DisplayMetrics getPortraitDisplayMetrics(DisplayMetrics currentMetrics,
                                                  int currnetOrientation) {
    DisplayMetrics result = new DisplayMetrics();
    result.setTo(currentMetrics);
    if (currnetOrientation == Configuration.ORIENTATION_LANDSCAPE) {
      result.heightPixels = currentMetrics.widthPixels;
      result.widthPixels = currentMetrics.heightPixels;
    }
    return result;
  }
}
