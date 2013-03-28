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

import org.mozc.android.inputmethod.japanese.testing.ApiLevel;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;

import android.content.Intent;
import android.os.Bundle;
import android.preference.PreferenceActivity;
import android.test.suitebuilder.annotation.SmallTest;

/**
 */
public class MozcFragmentBasePreferenceActivityTest extends InstrumentationTestCaseWithMock {

  @ApiLevel(11)
  @SmallTest
  public void testMaybeCreateRedirectingIntent() {
    // Multi-pane && EXTRA_SHOW_FRAGMENT == null
    // If multi-pane is preferred, EXTRA_SHOW_FRAGMENT doesn't affect anything.
    {
      Intent intent = new Intent();
      assertNull(MozcFragmentBasePreferenceActivity.maybeCreateRedirectingIntent(
          intent, true, PreferencePage.FLAT));
    }

    // Multi-pane && EXTRA_SHOW_FRAGMENT != null
    // If multi-pane is preferred, EXTRA_SHOW_FRAGMENT doesn't affect anything.
    {
      Intent intent = new Intent();
      intent.putExtra(PreferenceActivity.EXTRA_SHOW_FRAGMENT, "foo.bar.Fragment");
      assertNull(MozcFragmentBasePreferenceActivity.maybeCreateRedirectingIntent(
          intent, true, PreferencePage.FLAT));
    }

    // Single-pane && EXTRA_SHOW_FRAGMENT == null
    // Corresponding to 1st call of onCreate so redirection is needed.
    for (PreferencePage preferencePage : PreferencePage.values()){
      Intent intent = new Intent();
      Intent redirectingIntent = MozcFragmentBasePreferenceActivity.maybeCreateRedirectingIntent(
          intent, false, preferencePage);
      assertNotNull(redirectingIntent);
      assertEquals(PreferenceBaseFragment.class.getCanonicalName(),
                   redirectingIntent.getStringExtra(PreferenceActivity.EXTRA_SHOW_FRAGMENT));
      Bundle bundle =
          redirectingIntent.getBundleExtra(PreferenceActivity.EXTRA_SHOW_FRAGMENT_ARGUMENTS);
      assertEquals(preferencePage.name(),
                   bundle.getString(PreferencePage.EXTRA_ARGUMENT_PREFERENCE_PAGE_NAME));
    }

    // Single-pane && EXTRA_SHOW_FRAGMENT != null
    // Corresponding to 2nd call of onCreate so no redirection is needed.
    {
      Intent intent = new Intent();
      intent.putExtra(PreferenceActivity.EXTRA_SHOW_FRAGMENT, "foo.bar.Fragment");
      assertNull(MozcFragmentBasePreferenceActivity.maybeCreateRedirectingIntent(
          intent, false, PreferencePage.FLAT));
    }
  }
}
