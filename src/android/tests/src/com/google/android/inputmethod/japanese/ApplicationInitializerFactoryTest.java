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

import org.mozc.android.inputmethod.japanese.ApplicationInitializerFactory.ApplicationInitializationStatus;
import org.mozc.android.inputmethod.japanese.ApplicationInitializerFactory.ApplicationInitializer;
import org.mozc.android.inputmethod.japanese.MozcUtil.TelephonyManagerInterface;
import org.mozc.android.inputmethod.japanese.preference.PreferenceUtil;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.MozcPreferenceUtil;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import com.google.common.base.Optional;

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


  @SmallTest
  public void testStoreDefaultFullscreenMode() throws IllegalArgumentException {
    Resources resources = getInstrumentation().getTargetContext().getResources();
    float currentDisplayDensity = resources.getDisplayMetrics().density;
    Configuration defaultConfiguration = new Configuration();
    defaultConfiguration.setToDefaults();
    defaultConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
    float portraitNormalImeHeightInDip =
        resources.getDimension(R.dimen.ime_window_height_portrait_normal) / currentDisplayDensity;
    float landscapeNormalImeHeightInDip =
        resources.getDimension(R.dimen.ime_window_height_landscape_normal) / currentDisplayDensity;
    float portraitXlargeImeHeightInDip =
        resources.getDimension(R.dimen.ime_window_height_portrait_xlarge) / currentDisplayDensity;
    float landscapeXlargeImeHeightInDip =
        resources.getDimension(R.dimen.ime_window_height_landscape_xlarge) / currentDisplayDensity;
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
                          portraitNormalImeHeightInDip,
                          landscapeNormalImeHeightInDip,
                          productionFullscreenThresholdInDip,
                          false, true),
        // HTC ARIA
        new TestParameter(320, 480, 1.0f,
                          portraitNormalImeHeightInDip,
                          landscapeNormalImeHeightInDip,
                          productionFullscreenThresholdInDip,
                          true, true),
        // IS03
        new TestParameter(640, 960, 2.0f,
                          portraitNormalImeHeightInDip,
                          landscapeNormalImeHeightInDip,
                          productionFullscreenThresholdInDip,
                          true, true),
        // IS01
        new TestParameter(854, 480, 1.5f,
                          portraitNormalImeHeightInDip,
                          landscapeNormalImeHeightInDip,
                          productionFullscreenThresholdInDip,
                          true, false),
        // Optimus Pad
        new TestParameter(1280, 768, 1.0f,
                          portraitNormalImeHeightInDip,
                          landscapeNormalImeHeightInDip,
                          productionFullscreenThresholdInDip,
                          false, false),
        // Galaxy Nexus
        new TestParameter(720, 1280, 2.0f,
                          portraitNormalImeHeightInDip,
                          landscapeNormalImeHeightInDip,
                          productionFullscreenThresholdInDip,
                          false, true),
        // Nexus 7
        new TestParameter(1280, 800, 1.33f,
                          portraitNormalImeHeightInDip,
                          landscapeNormalImeHeightInDip,
                          productionFullscreenThresholdInDip,
                          false, false),
        // Nexus 10
        new TestParameter(2464, 1600, 2.0f,
                          portraitXlargeImeHeightInDip,
                          landscapeXlargeImeHeightInDip,
                          productionFullscreenThresholdInDip,
                          false, false),
    };

    for (TestParameter testParameter : parameters) {
      SharedPreferences sharedPreferences =
          MozcPreferenceUtil.getSharedPreferences(
              getInstrumentation().getTargetContext(), "DEFAULT_FULLSCREEN");
      sharedPreferences.edit().clear().commit();
      DisplayMetrics metrics = new DisplayMetrics();
      metrics.widthPixels = testParameter.displayWidthPixels;
      metrics.heightPixels = testParameter.displayHeightPixels;
      metrics.density = testParameter.displayDensity;
      ApplicationInitializer.storeDefaultFullscreenMode(
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
