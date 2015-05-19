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

import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.resources.R;

import android.annotation.TargetApi;
import android.os.Bundle;
import android.preference.PreferenceFragment;

import java.util.List;

/**
 * A PreferenceFragment for each {@link PreferencePage}.
 *
 * A referenceBaseFragment instance corresponds to a &lt;header&gt;(in prefernce header xml).
 *
 */
@TargetApi(11)
public class PreferenceBaseFragment extends PreferenceFragment {
  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    String preferencePageName = getPrefrerencePageName(getArguments());
    PreferencePage preferencePage = toPreferencePage(preferencePageName);
    addPreferences(
        PreferencePage.getResourceIdList(
            preferencePage,
            MozcUtil.isDebug(getActivity()),
            getResources().getBoolean(R.bool.sending_information_features_enabled)));
  }

  void addPreferences(List<Integer> resourceIds) {
    for (int resourceId : resourceIds) {
      addPreferencesFromResource(resourceId);
    }
    PreferenceUtil.initializeSpecialPreferences(getPreferenceManager());
  }

  /**
   * Does same thing as {@link PreferencePage#valueOf}, but also handles errors.
   * Returns FLAT if an error is found.
   */
  static PreferencePage toPreferencePage(String preferencePageName) {
    if (preferencePageName == null) {
      // No preference page name is given.
      MozcLog.e("preferencePageName is not set.");
      return PreferencePage.FLAT;
    }

    try {
      return PreferencePage.valueOf(preferencePageName);
    } catch (IllegalArgumentException e) {
      MozcLog.e("value '" + preferencePageName + "' is not defined.");
    }

    // Returns FLAT by default.
    return PreferencePage.FLAT;
  }

  /**
   * Returns {@link PreferencePage}'s name which corresponds to the instance
   * based on {@code bundle}.
   *
   * Typically {@code bundle} is set in preference header xml.
   */
  static String getPrefrerencePageName(Bundle bundle) {
    if (bundle == null) {
      return null;
    }
    return bundle.getString(PreferencePage.EXTRA_ARGUMENT_PREFERENCE_PAGE_NAME);
  }
}
