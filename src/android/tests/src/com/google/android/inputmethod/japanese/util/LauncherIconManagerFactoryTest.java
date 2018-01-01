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

import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.preference.PreferenceUtil;
import org.mozc.android.inputmethod.japanese.testing.MozcPreferenceUtil;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import com.google.common.base.Optional;

import android.content.SharedPreferences;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

/**
 * Test for LauncherIconManagerFactory.
 */
public class LauncherIconManagerFactoryTest extends InstrumentationTestCase {

  /**
   * Test for shouldLauncherIconBeVisible.
   * Note that this test focus on the situation where PREF_LAUNCHER_ICON_VISIBILITY_KEY is not set.
   * Preinstall version (2.16.1955.3) without showing preference screen OR first launch.
   */
  @SmallTest
  public void testShouldLauncherIconBeVisible() {
    SharedPreferences sharedPreferences = MozcPreferenceUtil.getSharedPreferences(
        getInstrumentation().getContext(), "CLIENT_SIDE_PREF");
    class TestData extends Parameter {
      final boolean isSystemApplication;
      final boolean isUpdatedSystemApplication;
      final boolean isLaunchedAtLeastOnce;
      final boolean expectation;
      public TestData(boolean isSystemApplication, boolean isUpdatedSystemApplication,
                      boolean isLaunchedAtLeastOnce, boolean expectation) {
        this.isSystemApplication = isSystemApplication;
        this.isUpdatedSystemApplication = isUpdatedSystemApplication;
        this.isLaunchedAtLeastOnce = isLaunchedAtLeastOnce;
        this.expectation = expectation;
      }
    }
    // NOTE: Basically if isLaunchedAtLeastOnce is true shouldLauncherIconBeVisible is not called.
    //       However Preinstall (2.16.1955.3) doesn't set PREF_LAUNCHER_ICON_VISIBILITY_KEY
    //       preference so the version and the first launch after upgrade from the version
    //       reach here.
    TestData[] testDataList = {
        // Launch PlayStore as the first time.
        new TestData(false, false, false, true),
        // Preinstall version (never launched) -> Launch PlayStore
        // This situation should be treated as the same as usual installation from PlayStore.
        new TestData(false, true, false, true),  // Never happen. Just in case.
        new TestData(true, true, false, true),
        // Launch Preinstall as the first time (2.16.1955.3) -> Launch PlayStore.
        // The icon should be kept invisible.
        // NOTE: PlayStore -> Playstore scenario doesn't reach here because visibility preference is
        //       set on the first launch.
        new TestData(false, true, true, false),  // Never happen. Just in case.
        new TestData(true, true, true, false),
        // Launch Preinstall (any version) as the first time.
        new TestData(true, false, false, false),
        // Launch Preinstall (2.16.1955.3) as the first time -> Launch Preinstall
        new TestData(true, false, true, false),
        // Below situation should never happen.
        // new TestData(false, false, true, n/a),
    };
    for (TestData testData : testDataList) {
      SharedPreferences.Editor editor = sharedPreferences.edit().clear();
      if (testData.isLaunchedAtLeastOnce) {
        editor.putInt(PreferenceUtil.PREF_LAST_LAUNCH_ABI_INDEPENDENT_VERSION_CODE, 1234);
      }
      editor.apply();
      MozcUtil.setSystemApplication(Optional.of(testData.isSystemApplication));
      MozcUtil.setUpdatedSystemApplication(Optional.of(testData.isUpdatedSystemApplication));
      assertEquals(
          testData.toString(),
          testData.expectation,
          LauncherIconManagerFactory.DefaultImplementation
              .shouldLauncherIconBeVisible(getInstrumentation().getTargetContext(),
                                           sharedPreferences));
    }
  }
}
