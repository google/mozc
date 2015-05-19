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
import org.mozc.android.inputmethod.japanese.resources.R;

import android.preference.PreferenceActivity;
import android.preference.PreferenceScreen;

import java.util.List;

/**
 * Classic preference UI for API Level &lt; 11.
 *
 * This class depends on deprecated APIs of {@link PreferenceActivity}.
 *
 */
public abstract class MozcClassicBasePreferenceActivity extends MozcBasePreferenceActivity {
  private final PreferencePage preferencePage;

  protected MozcClassicBasePreferenceActivity(PreferencePage preferencePage) {
    this.preferencePage = preferencePage;
  }

  @Override
  protected void onResume() {
    super.onResume();

    // Prepare the UI.
    // This has to be done here (not in onCreate) otherwise updated
    // SharedPreference will be ignored.
    initializeScreen();
  }

  /**
   * Initializes the screen.
   *
   * This method needs to be called in order to reflect the update of SharedPreference.
   * This method will have to be called from onConfigurationChanged
   * in order to reflect the configuration because
   * for small change (which does not need recreation of the activity)
   * updating the screen is our responsibility.
   * Activity#recreate() is probably good for this purpose (recreate on every configuration change)
   * but this is for API Level 11 or above.
   */
  @SuppressWarnings("deprecation")
  void initializeScreen() {
    PreferenceScreen preferenceScreen = getPreferenceScreen();
    if (preferenceScreen != null) {
      preferenceScreen.removeAll();
    }

    List<Integer> resourceIds =
        PreferencePage.getResourceIdList(preferencePage,
                                         MozcUtil.isDebug(this),
                                         getResources().getBoolean(
                                             R.bool.sending_information_features_enabled));
    for (int resourceId : resourceIds) {
      addPreferencesFromResource(resourceId);
    }
    PreferenceUtil.initializeSpecialPreferences(getPreferenceManager());
  }
}
