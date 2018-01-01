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

import org.mozc.android.inputmethod.japanese.ApplicationInitializerFactory.ApplicationInitializationStatus;
import org.mozc.android.inputmethod.japanese.ApplicationInitializerFactory.ApplicationInitializer;
import org.mozc.android.inputmethod.japanese.MozcUtil.TelephonyManagerInterface;
import org.mozc.android.inputmethod.japanese.preference.PreferenceUtil;
import org.mozc.android.inputmethod.japanese.preference.PreferenceUtil.PreferenceManagerStaticInterface;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.MockContext;
import org.mozc.android.inputmethod.japanese.testing.MockPackageManager;
import org.mozc.android.inputmethod.japanese.testing.MozcPreferenceUtil;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import org.mozc.android.inputmethod.japanese.util.LauncherIconManagerFactory.LauncherIconManager;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.DisplayMetrics;

import org.easymock.EasyMock;

/**
 * Test for {@link ApplicationInitializerFactory}.
 */
public class ApplicationInitializerFactoryTest extends InstrumentationTestCaseWithMock {

  @Override
  protected void setUp() throws Exception {
    super.setUp();
    MozcUtil.setDebug(Optional.of(false));
  }

  @Override
  protected void tearDown() throws Exception {
    MozcUtil.setDebug(Optional.<Boolean>absent());
    super.tearDown();
  }

  @SmallTest
  public void testInitialize() {
    SharedPreferences sharedPreferences = MozcPreferenceUtil.getSharedPreferences(
        getInstrumentation().getContext(), "1ST_TIME_LAUNCH");

    class TestData extends Parameter {
      final boolean isSystemApplication;
      final boolean isLaunchedAtLeastOnce;
      final boolean isWelcomeActivityShown;
      final boolean isWelcomeActivityPreferred;
      final Optional<Integer> lastLaunchVersionCode;
      final int versionCode;
      final boolean expectedStartActivity;
      final boolean expectedPreferenceUpdate;
      TestData(boolean isSystemApplication,
               boolean isFirstLaunch,
               boolean isWelcomeActivityShown,
               boolean isWelcomeActivityPreferred,
               Optional<Integer> lastLaunchVersionCode,
               int versionCode,
               boolean expectedStartActivity,
               boolean expectedPreferenceUpdate) {
        this.isSystemApplication = isSystemApplication;
        this.isLaunchedAtLeastOnce = isFirstLaunch;
        this.isWelcomeActivityShown = isWelcomeActivityShown;
        this.isWelcomeActivityPreferred = isWelcomeActivityPreferred;
        this.lastLaunchVersionCode = lastLaunchVersionCode;
        this.versionCode = versionCode;
        this.expectedStartActivity = expectedStartActivity;
        this.expectedPreferenceUpdate = expectedPreferenceUpdate;
      }
    }

    // Expect false if isSystemApplication is true or isFirstLaunch is false.
    Optional<Integer> absentInt = Optional.<Integer>absent();
    TestData[] testDataList = {
        // Non-sys-app
        new TestData(false, false, true, true, Optional.of(100), 100, false, false),
        new TestData(false, false, true, true, absentInt, 100, false, true),

        new TestData(false, false, false, true, Optional.of(100), 100, true, false),
        new TestData(false, false, false, true, absentInt, 100, true, true),

        new TestData(false, true, true, true, Optional.of(100), 100, false, false),
        new TestData(false, true, true, true, absentInt, 100, false, false),

        new TestData(false, true, false, true, Optional.of(100), 100, false, false),
        new TestData(false, true, false, true, absentInt, 100, false, false),

        // Sys-app
        new TestData(true, false, true, true, Optional.of(100), 100, false, false),
        new TestData(true, false, true, true, absentInt, 100, false, true),

        new TestData(true, false, false, true, Optional.of(100), 100, false, false),
        new TestData(true, false, false, true, absentInt, 100, false, true),

        new TestData(true, true, true, true, Optional.of(100), 100, false, false),
        new TestData(true, true, true, true, absentInt, 100, false, false),

        new TestData(true, true, false, true, Optional.of(100), 100, false, false),
        new TestData(true, true, false, true, absentInt, 100, false, false),

        // Non-sys-app, Welcome acvitity shouldn't be shown
        new TestData(false, false, true, false, Optional.of(100), 100, false, false),
        new TestData(false, false, true, false, absentInt, 100, false, true),

        new TestData(false, false, false, false, Optional.of(100), 100, false, false),
        new TestData(false, false, false, false, absentInt, 100, false, true),

        new TestData(false, true, true, false, Optional.of(100), 100, false, false),
        new TestData(false, true, true, false, absentInt, 100, false, false),

        new TestData(false, true, false, false, Optional.of(100), 100, false, false),
        new TestData(false, true, false, false, absentInt, 100, false, false),

        // Sys-app, Welcome acvitity shouldn't be shown
        new TestData(true, false, true, false, Optional.of(100), 100, false, false),
        new TestData(true, false, true, false, absentInt, 100, false, true),

        new TestData(true, false, false, false, Optional.of(100), 100, false, false),
        new TestData(true, false, false, false, absentInt, 100, false, true),

        new TestData(true, true, true, false, Optional.of(100), 100, false, false),
        new TestData(true, true, true, false, absentInt, 100, false, false),

        new TestData(true, true, false, false, Optional.of(100), 100, false, false),
        new TestData(true, true, false, false, absentInt, 100, false, false),

    };

    TelephonyManagerInterface telephonyManager = new TelephonyManagerInterface() {
      @Override
      public String getNetworkOperator() {
        return "INVALID NETWORK OPERATOR";
      }
    };
    LauncherIconManager launcherIconManager = new LauncherIconManager() {
      @Override
      public void updateLauncherIconVisibility(Context context) {}
    };
    PreferenceManagerStaticInterface preferenceManager = new PreferenceManagerStaticInterface() {
      @Override
      public void setDefaultValues(Context context, int id, boolean readAgain) {}
    };

    Context targetContext = getInstrumentation().getTargetContext();
    Context context = createNiceMock(MockContext.class);
    EasyMock.expect(context.getPackageName())
        .andStubReturn(getInstrumentation().getTargetContext().getPackageName());
    EasyMock.expect(context.getResources())
        .andStubReturn(targetContext.getResources());
    EasyMock.expect(context.getContentResolver())
        .andStubReturn(targetContext.getContentResolver());
    EasyMock.expect(context.getCacheDir())
        .andStubReturn(targetContext.getCacheDir());
    EasyMock.expect(context.getApplicationInfo())
        .andStubReturn(targetContext.getApplicationInfo());
    EasyMock.expect(context.getSystemService(Context.USER_SERVICE))
        .andStubReturn(targetContext.getSystemService(Context.USER_SERVICE));
    EasyMock.expect(context.getPackageManager()).andStubReturn(new MockPackageManager());

    replayAll();
    for (final TestData testData : testDataList) {
      for (final boolean isDevChannel : new boolean[] {true, false}) {
        sharedPreferences.edit().clear().commit();
        ApplicationInitializationStatus initializationStatus =
            new ApplicationInitializationStatus() {
          @Deprecated
          @Override
          public boolean isLaunchedAtLeastOnce() {
            return testData.isLaunchedAtLeastOnce;
          }
          @Override
          public Optional<Integer> getLastLaunchAbiIndependentVersionCode() {
            return testData.lastLaunchVersionCode;
          }
          @Override
          public boolean isWelcomeActivityShownAtLeastOnce() {
            return testData.isWelcomeActivityShown;
          }
        };
        assertEquals(testData.toString(),
                     testData.expectedStartActivity,
                     ApplicationInitializerFactory.createInstance(
                         initializationStatus,
                         context,
                         sharedPreferences,
                         telephonyManager)
                             .initialize(testData.isSystemApplication,
                                         isDevChannel,
                                         testData.isWelcomeActivityPreferred,
                                         testData.versionCode,
                                         launcherIconManager,
                                         preferenceManager).isPresent());
        assertEquals(testData.toString(),
                     testData.versionCode,
                     sharedPreferences.getInt(
                         PreferenceUtil
                             .PREF_LAST_LAUNCH_ABI_INDEPENDENT_VERSION_CODE,
                         Integer.MIN_VALUE));
        if (testData.expectedStartActivity) {
          assertTrue(testData.toString(),
                     sharedPreferences.getBoolean(
                         ApplicationInitializerFactory.PREF_WELCOME_ACTIVITY_SHOWN, false));
        }
        assertEquals(testData.toString(),
            testData.expectedPreferenceUpdate,
            sharedPreferences.contains("pref_portrait_fullscreen_key"));
        assertEquals(testData.toString(),
            testData.expectedPreferenceUpdate,
            sharedPreferences.contains(PreferenceUtil.PREF_EMOJI_PROVIDER_TYPE));
        assertFalse(testData.toString(),
                    sharedPreferences.contains(
                        ApplicationInitializerFactory.PREF_LAUNCHED_AT_LEAST_ONCE));
        // TODO(matsuzakit): Using android:defaultValue attribute is more preferable than using
        //                   contains method. Do it on another CL.
        if (isDevChannel) {
          assertTrue(testData.toString(),
                     sharedPreferences.getBoolean(PreferenceUtil.PREF_OTHER_USAGE_STATS_KEY,
                                                  false));
        } else {
          assertTrue(testData.toString(),
                     !sharedPreferences.contains(PreferenceUtil.PREF_OTHER_USAGE_STATS_KEY) ||
                         !sharedPreferences.getBoolean(PreferenceUtil.PREF_OTHER_USAGE_STATS_KEY,
                             true));
        }
      }
    }
  }

  @SmallTest
  public void testStoreDefaultFullscreenMode() throws IllegalArgumentException {
    class TestParameter extends Parameter {
      final int displayHeightInPixels;
      final int displayWidthInPixels;
      final float displayDensity;
      final boolean expectPortraitFullscreen;
      final boolean expectLandscapeFullscreen;
      TestParameter(int displayHeightInPixels,
                    int displayWidthInPixels,
                    float displayDensity,
                    boolean expectPortraitFullscreen,
                    boolean expectLandscapeFullscreen) {
        Preconditions.checkArgument(displayHeightInPixels >= displayWidthInPixels);
        this.displayHeightInPixels = displayHeightInPixels;
        this.displayWidthInPixels = displayWidthInPixels;
        this.displayDensity = displayDensity;
        this.expectPortraitFullscreen = expectPortraitFullscreen;
        this.expectLandscapeFullscreen = expectLandscapeFullscreen;
      }
    }
    TestParameter[] parameters = {
        new TestParameter(300, 300, 1.0f, true, true),
        // Galaxy S
        new TestParameter(800, 480, 1.5f, false, true),
        // HTC ARIA
        new TestParameter(480, 320, 1.0f, false, true),
        // IS03
        new TestParameter(960, 640, 2.0f, false, true),
        // IS01
        new TestParameter(854, 480, 1.5f, false, true),
        // Optimus Pad
        new TestParameter(1280, 768, 1.0f, false, false),
        // Galaxy Nexus
        new TestParameter(1280, 720, 2.0f, false, true),
        // Nexus 7
        new TestParameter(1280, 800, 1.33f, false, false),
        // Nexus 10
        new TestParameter(2464, 1600, 2.0f, false, false),
    };

    Resources resources = getInstrumentation().getTargetContext().getResources();

    for (TestParameter testParameter : parameters) {
      SharedPreferences sharedPreferences =
          MozcPreferenceUtil.getSharedPreferences(
              getInstrumentation().getTargetContext(), "DEFAULT_FULLSCREEN");
      sharedPreferences.edit().clear().commit();

      DisplayMetrics originalMetrics = new DisplayMetrics();
      originalMetrics.setTo(resources.getDisplayMetrics());
      Configuration originalConfiguration = new Configuration(resources.getConfiguration());

      DisplayMetrics metrics = new DisplayMetrics();
      metrics.heightPixels = testParameter.displayHeightInPixels;
      metrics.widthPixels = testParameter.displayWidthInPixels;
      metrics.density = testParameter.displayDensity;

      Configuration configuration = new Configuration(resources.getConfiguration());
      configuration.orientation = Configuration.ORIENTATION_PORTRAIT;
      configuration.densityDpi = Math.round(160f * testParameter.displayDensity);
      configuration.screenHeightDp =
          Math.round(testParameter.displayHeightInPixels / testParameter.displayDensity);
      configuration.screenWidthDp =
          Math.round(testParameter.displayWidthInPixels / testParameter.displayDensity);

      try {
        resources.updateConfiguration(configuration, metrics);

        ApplicationInitializer.storeDefaultFullscreenMode(
            sharedPreferences,
            testParameter.displayHeightInPixels,
            testParameter.displayWidthInPixels,
            resources.getDimensionPixelOffset(R.dimen.input_frame_height),
            (int) Math.ceil(MozcUtil.getDimensionForOrientation(
                resources, R.dimen.input_frame_height, Configuration.ORIENTATION_LANDSCAPE)),
                resources.getDimensionPixelOffset(R.dimen.fullscreen_threshold));
        assertEquals("portrait check failed: " + testParameter.toString(),
                     testParameter.expectPortraitFullscreen,
                     sharedPreferences.getBoolean("pref_portrait_fullscreen_key", false));
        assertEquals("landscape check failed: " + testParameter.toString(),
                     testParameter.expectLandscapeFullscreen,
                     sharedPreferences.getBoolean("pref_landscape_fullscreen_key", false));
      } finally {
        resources.updateConfiguration(originalConfiguration, originalMetrics);
      }
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
        ApplicationInitializer.getPortraitDisplayMetrics(metrics,
                                                         Configuration.ORIENTATION_PORTRAIT);
    DisplayMetrics landscapeMetrics =
        ApplicationInitializer.getPortraitDisplayMetrics(metrics,
                                                         Configuration.ORIENTATION_LANDSCAPE);

    assertEquals(heightPixels, portraitMetrics.heightPixels);
    assertEquals(widthPixels, portraitMetrics.widthPixels);
    assertEquals(widthPixels, landscapeMetrics.heightPixels);
    assertEquals(heightPixels, landscapeMetrics.widthPixels);
  }
}
