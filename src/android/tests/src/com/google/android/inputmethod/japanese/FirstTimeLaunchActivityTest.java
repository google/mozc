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

import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.isA;

import org.mozc.android.inputmethod.japanese.FirstTimeLaunchActivity.SendUsageStatsChangeListener;
import org.mozc.android.inputmethod.japanese.preference.PreferenceUtil;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.testing.ActivityInstrumentationTestCase2WithMock;
import org.mozc.android.inputmethod.japanese.testing.MozcPreferenceUtil;
import org.mozc.android.inputmethod.japanese.testing.Parameter;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.DisplayMetrics;
import android.view.View;

/**
 */
public class FirstTimeLaunchActivityTest extends
    ActivityInstrumentationTestCase2WithMock<FirstTimeLaunchActivity> {
  public FirstTimeLaunchActivityTest() {
    super(FirstTimeLaunchActivity.class);
  }

  @SmallTest
  public void testOnSendUsageStatsChangeListener_onCheckedChanged() {
    SharedPreferences sharedPreferences =
        MozcPreferenceUtil.getSharedPreferences(getActivity(), "ON_CHECKED_CHANGED");
    sharedPreferences.edit().clear().commit();

    SendUsageStatsChangeListener listener =
        new SendUsageStatsChangeListener(sharedPreferences);

    listener.onCheckedChanged(null, true);
    assertTrue(sharedPreferences.getBoolean(
               PreferenceUtil.PREF_OTHER_USAGE_STATS_KEY, false));

    listener.onCheckedChanged(null, false);
    assertFalse(sharedPreferences.getBoolean(
                PreferenceUtil.PREF_OTHER_USAGE_STATS_KEY, true));
  }

  @SmallTest
  @UiThreadTest
  public void testInitializeContentView() {
    FirstTimeLaunchActivity activity = getActivity();

    activity.initializeContentView(false, null);
    assertEquals(
        View.GONE,
        activity.findViewById(R.id.usage_stats_views).getVisibility());

    MozcUtil.setDevChannel(false);
    activity.initializeContentView(true, null);
    assertEquals(
        View.VISIBLE,
        activity.findViewById(R.id.usage_stats_views).getVisibility());
    assertTrue(activity.findViewById(R.id.send_usage_stats).isEnabled());
    assertEquals(
        View.GONE,
        activity.findViewById(R.id.send_usage_stats_devchannel_description).getVisibility());

    MozcUtil.setDevChannel(true);
    activity.initializeContentView(true, null);
    assertEquals(
        View.VISIBLE,
        activity.findViewById(R.id.usage_stats_views).getVisibility());
    assertFalse(activity.findViewById(R.id.send_usage_stats).isEnabled());
    assertEquals(
        View.VISIBLE,
        activity.findViewById(R.id.send_usage_stats_devchannel_description).getVisibility());
  }

  @SmallTest
  public void testMaybePrepareFirstTimeLaunch() {
    SharedPreferences sharedPreferences = MozcPreferenceUtil.getSharedPreferences(
        getInstrumentation().getContext(), "1ST_TIME_LAUNCH");

    class TestData {
      final boolean isDevChannel;
      final boolean isFirstLaunch;
      final boolean expectedUsageStats;
      final boolean expectedStartActivity;
      TestData(boolean isDevChannel, boolean isFirstLaunch, boolean expectedUsageStats,
          boolean expectedStartActivity) {
        this.isDevChannel = isDevChannel;
        this.isFirstLaunch = isFirstLaunch;
        this.expectedUsageStats = expectedUsageStats;
        this.expectedStartActivity = expectedStartActivity;
      }
    }

    TestData[] testDataList = {
        new TestData(false, true, false, true),
        new TestData(false, false, false, false),
        new TestData(true, true, true, true),
        // expectedUsageStats is false because no update from default is performed.
        new TestData(true, false, false, false),
    };

    Context context = createNiceMock(Context.class);
    for (TestData testData : testDataList) {
      sharedPreferences.edit().clear().commit();
      MozcUtil.setDevChannel(testData.isDevChannel);
      resetAll();
      if (testData.expectedStartActivity) {
        context.startActivity(isA(Intent.class));
      }
      expect(context.getResources())
          .andStubReturn(getInstrumentation().getTargetContext().getResources());
      replayAll();
      if (!testData.isFirstLaunch) {
        sharedPreferences.edit().putBoolean("pref_launched_at_least_once", true).commit();
      }
      FirstTimeLaunchActivity.maybeProcessFirstTimeLaunchAction(sharedPreferences, context);
      assertTrue(sharedPreferences.getBoolean("pref_launched_at_least_once", false));
      // TODO(matsuzakit): Using android:defaultValue attribute is more preferable than using
      //                   contains method. Do it on another CL.
      if (testData.expectedUsageStats) {
        assertTrue(
            sharedPreferences.getBoolean(PreferenceUtil.PREF_OTHER_USAGE_STATS_KEY, false));
      } else {
        assertTrue(
            !sharedPreferences.contains(PreferenceUtil.PREF_OTHER_USAGE_STATS_KEY) ||
            !sharedPreferences.getBoolean(PreferenceUtil.PREF_OTHER_USAGE_STATS_KEY, true));
      }
      verifyAll();
    }
  }

  @SmallTest
  public void testStoreDefaultFullscreenMode() {
    Resources resources = getInstrumentation().getTargetContext().getResources();
    float currentDisplayDensity = resources.getDisplayMetrics().density;
    Configuration defaultConfiguration = new Configuration();
    defaultConfiguration.setToDefaults();
    defaultConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
    float productionPortraitImeHeightInDip =
        resources.getDimension(R.dimen.ime_window_height_portrait) / currentDisplayDensity;
    float productionLandscapeImeHeightInDip =
        resources.getDimension(R.dimen.ime_window_height_landscape) / currentDisplayDensity;
    float productionFullscreenThresholdInDip =
        resources.getDimension(R.dimen.fullscreen_threshold) / currentDisplayDensity;
    class TestParameter extends Parameter {
      final int displayWidthPixels;
      final int displayHeightPixels;
      final float displayDensity;
      final float portraitImeHeight;
      final float landscapeImeHeight;
      final float fullscreenThreshold;
      final boolean expectPortraitFullscreen;
      final boolean expectLandscapeFullscreen;
      TestParameter(int displayWidthPixels,
                    int displayHeightPixels,
                    float displayDensity,
                    float portraitImeHeight,
                    float landscapeImeHeight,
                    float fullscreenThreshold,
                    boolean expectPortraitFullscreen,
                    boolean expectLandscapeFullscreen) {
        this.displayWidthPixels = displayWidthPixels;
        this.displayHeightPixels = displayHeightPixels;
        this.displayDensity = displayDensity;
        this.portraitImeHeight = portraitImeHeight;
        this.landscapeImeHeight = landscapeImeHeight;
        this.fullscreenThreshold = fullscreenThreshold;
        this.expectPortraitFullscreen = expectPortraitFullscreen;
        this.expectLandscapeFullscreen = expectLandscapeFullscreen;
      }
    }
    TestParameter[] parameters = {
        new TestParameter(100, 100, 1f, 50, 80, 40, false, true),
        new TestParameter(100, 100, 2f, 50, 80, 40, true, true),
        // Galaxy S
        new TestParameter(480, 800, 1.5f,
                          productionPortraitImeHeightInDip,
                          productionLandscapeImeHeightInDip,
                          productionFullscreenThresholdInDip,
                          false, true),
        // HTC ARIA
        new TestParameter(320, 480, 1.0f,
                          productionPortraitImeHeightInDip,
                          productionLandscapeImeHeightInDip,
                          productionFullscreenThresholdInDip,
                          true, true),
        // IS03
        new TestParameter(640, 960, 2.0f,
                          productionPortraitImeHeightInDip,
                          productionLandscapeImeHeightInDip,
                          productionFullscreenThresholdInDip,
                          true, true),
        // IS01
        new TestParameter(854, 480, 1.5f,
                          productionPortraitImeHeightInDip,
                          productionLandscapeImeHeightInDip,
                          productionFullscreenThresholdInDip,
                          true, false),
        // Optimus Pad
        new TestParameter(1280, 768, 1.0f,
                          productionPortraitImeHeightInDip,
                          productionLandscapeImeHeightInDip,
                          productionFullscreenThresholdInDip,
                          false, false),
        // Galaxy Nexus
        new TestParameter(1280, 768, 1.0f,
                          productionPortraitImeHeightInDip,
                          productionLandscapeImeHeightInDip,
                          productionFullscreenThresholdInDip,
                          false, false),
    };

    for (TestParameter testParameter : parameters) {
      SharedPreferences sharedPreferences =
          MozcPreferenceUtil.getSharedPreferences(getActivity(), "DEFAULT_FULLSCREEN");
      sharedPreferences.edit().clear().commit();
      DisplayMetrics metrics = new DisplayMetrics();
      metrics.widthPixels = testParameter.displayWidthPixels;
      metrics.heightPixels = testParameter.displayHeightPixels;
      metrics.density = testParameter.displayDensity;
      FirstTimeLaunchActivity.storeDefaultFullscreenMode(
          sharedPreferences,
          metrics,
          testParameter.fullscreenThreshold * testParameter.displayDensity,
          testParameter.portraitImeHeight * testParameter.displayDensity,
          testParameter.landscapeImeHeight * testParameter.displayDensity);
      assertEquals(testParameter.toString(),
                   testParameter.expectPortraitFullscreen,
                   sharedPreferences.getBoolean("pref_portrait_fullscreen_key", false));
      assertEquals(testParameter.toString(),
                   testParameter.expectLandscapeFullscreen,
                   sharedPreferences.getBoolean("pref_landscape_fullscreen_key", false));
    }
  }

  @SmallTest
  public void testGetPortraitDisplayMetrics() {
    int heightPixels = 1;
    int widthPixels = 2;
    DisplayMetrics metrics = new DisplayMetrics();
    metrics.heightPixels = heightPixels;
    metrics.widthPixels = widthPixels;

    DisplayMetrics portraitMetrics =
        FirstTimeLaunchActivity.getPortraitDisplayMetrics(metrics,
                                                          Configuration.ORIENTATION_PORTRAIT);
    DisplayMetrics landscapeMetrics =
        FirstTimeLaunchActivity.getPortraitDisplayMetrics(metrics,
                                                          Configuration.ORIENTATION_LANDSCAPE);

    assertEquals(heightPixels, portraitMetrics.heightPixels);
    assertEquals(widthPixels, portraitMetrics.widthPixels);
    assertEquals(widthPixels, landscapeMetrics.heightPixels);
    assertEquals(heightPixels, landscapeMetrics.widthPixels);
  }
}
