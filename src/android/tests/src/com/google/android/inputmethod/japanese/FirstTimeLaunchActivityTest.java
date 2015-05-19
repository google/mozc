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

import org.mozc.android.inputmethod.japanese.FirstTimeLaunchActivity.SendUsageStatsChangeListener;
import org.mozc.android.inputmethod.japanese.preference.PreferenceUtil;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.testing.ActivityInstrumentationTestCase2WithMock;
import org.mozc.android.inputmethod.japanese.testing.MozcPreferenceUtil;
import com.google.common.base.Optional;

import android.content.SharedPreferences;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;
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

    MozcUtil.setDevChannel(Optional.of(false));
    activity.initializeContentView(true, null);
    assertEquals(
        View.VISIBLE,
        activity.findViewById(R.id.usage_stats_views).getVisibility());
    assertTrue(activity.findViewById(R.id.send_usage_stats).isEnabled());
    assertEquals(
        View.GONE,
        activity.findViewById(R.id.send_usage_stats_devchannel_description).getVisibility());

    MozcUtil.setDevChannel(Optional.of(true));
    activity.initializeContentView(true, null);
    assertEquals(
        View.VISIBLE,
        activity.findViewById(R.id.usage_stats_views).getVisibility());
    assertFalse(activity.findViewById(R.id.send_usage_stats).isEnabled());
    assertEquals(
        View.VISIBLE,
        activity.findViewById(R.id.send_usage_stats_devchannel_description).getVisibility());
  }
}
