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

package org.mozc.android.inputmethod.japanese.preference;

import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.testing.ActivityInstrumentationTestCase2WithMock;

import android.content.SharedPreferences;
import android.preference.PreferenceScreen;
import android.test.suitebuilder.annotation.SmallTest;

/**
 */
public class MozcClassicPreferenceActivityTest
    extends ActivityInstrumentationTestCase2WithMock<MozcClassicPreferenceActivity> {
  public MozcClassicPreferenceActivityTest() {
    super(MozcClassicPreferenceActivity.class);
  }

  @Override
  protected void tearDown() throws Exception {
    MozcUtil.setDebug(null);
    MozcUtil.setDebug(null);
    super.tearDown();
  }

  @SmallTest
  public void testPrepareScreen() {
    MozcClassicPreferenceActivity activity = getActivity();
    // Precondition
    PreferenceScreen preferenceScreen = activity.getPreferenceScreen();
    preferenceScreen.removeAll();
    assertEquals(0, preferenceScreen.getPreferenceCount());
    // Some preferences are appended.
    activity.initializeScreen();
    int count = preferenceScreen.getPreferenceCount();
    assertTrue(count > 0);
    // Invoking again shouldn't update the preference's count.
    activity.initializeScreen();
    assertTrue(count == preferenceScreen.getPreferenceCount());
  }

  @SmallTest
  public void testPrepareScreenSpecialPreferences() {
    MozcClassicPreferenceActivity activity = getActivity();
    activity.initializeScreen();

    // Preference for tweak should exist only when debug build.
    MozcUtil.setDebug(true);
    activity.onResume();
    assertNotNull(activity.findPreference("pref_tweak_logging_protocol_buffers"));

    MozcUtil.setDebug(false);
    activity.onResume();
    assertNull(activity.findPreference("pref_tweak_logging_protocol_buffers"));

    SharedPreferences sharedPreferences = activity.getPreferenceManager().getSharedPreferences();
    String usageStatsKey = "pref_other_usage_stats_key";
    // If on dev channel, the preference should be disabled.
    MozcUtil.setDevChannel(true);
    activity.onResume();
    assertFalse(activity.findPreference(usageStatsKey).isEnabled());

    // If not on dev channel, the preference should be enabled.
    MozcUtil.setDevChannel(false);
    activity.onResume();
    assertTrue(activity.findPreference(usageStatsKey).isEnabled());
  }
}
