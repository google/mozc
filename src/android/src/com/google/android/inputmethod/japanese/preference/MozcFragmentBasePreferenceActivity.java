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

import android.content.Intent;
import android.os.Bundle;
import android.preference.PreferenceActivity;

import java.util.List;

/**
 * Fragment based preference UI for API Level &gt;= 11.
 *
 * Deprecated APIs (which {@link MozcClassicPreferenceActivity} uses) are not used.
 * Instead newly introduced ones are used.
 *
 */
public abstract class MozcFragmentBasePreferenceActivity extends MozcBasePreferenceActivity {
  private final PreferencePage preferencePage;

  protected MozcFragmentBasePreferenceActivity(PreferencePage preferencePage) {
    this.preferencePage = preferencePage;
  }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    // If single-pane is preferred on this device and no fragment is specified,
    // puts an extra string in order to show the only fragment, "All".
    // By this, header list will not be shown.
    // It seems that #onCreate is the only injection point.
    Intent redirectingIntent =
        maybeCreateRedirectingIntent(getIntent(), onIsMultiPane(), preferencePage);
    if (redirectingIntent != null) {
      setIntent(redirectingIntent);
    }
    super.onCreate(savedInstanceState);
  }

  static Intent maybeCreateRedirectingIntent(
      Intent currentIntent, boolean isMultipane, PreferencePage preferencePage) {
    // See #onCreate to understand what's going.
    if (currentIntent.getStringExtra(PreferenceActivity.EXTRA_SHOW_FRAGMENT) != null
        || isMultipane) {
      return null;
    }
    Intent intent = new Intent(currentIntent);
    intent.putExtra(PreferenceActivity.EXTRA_SHOW_FRAGMENT,
                    PreferenceBaseFragment.class.getCanonicalName());
    // Send arguments of a fragment as a bundle.
    // This is equivalent to below xml.
    // <preference-headers>...<header ...><extra name="xxxx" value="xxxx"/>
    Bundle bundle = new Bundle();
    bundle.putString(PreferencePage.EXTRA_ARGUMENT_PREFERENCE_PAGE_NAME, preferencePage.name());
    intent.putExtra(PreferenceActivity.EXTRA_SHOW_FRAGMENT_ARGUMENTS, bundle);
    return intent;
  }

  @Override
  public void onBuildHeaders(List<Header> target) {
    loadHeaders(target, onIsMultiPane());
  }

  void loadHeaders(List<Header> target, boolean isMultiPane) {
    if (!isMultiPane) {
      // It is not needed to load the header for single pane preference,
      // because the view will be switched to the contents directly by above hack.
      return;
    }
    loadHeadersFromResource(
        getResources().getBoolean(R.bool.sending_information_features_enabled)
            ? R.xml.preference_headers_multipane
            : R.xml.preference_headers_multipane_without_stats,
        target);
    if (MozcUtil.isDebug(this)) {
      // For debug build, we load additional header.
      loadHeadersFromResource(R.xml.preference_headers_multipane_development, target);
    }
  }
}
