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

import android.os.Bundle;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

/**
 * TODO(matsuzakit): Test #addPreferences method.
 * Currently it is skpped because partial mock is not available
 * <http://b/6241958#ISSUE_HistoryHeader59>
 *
 */
@ApiLevel(11)
public class PreferenceBaseFragmentTest extends InstrumentationTestCase {

  @SmallTest
  public void testToPreferencePage() {
    // For valid names, the PreferencePage should be returned.
    for (PreferencePage preferencePage : PreferencePage.values()) {
      assertSame(preferencePage,
                 PreferenceBaseFragment.toPreferencePage(preferencePage.name()));
    }

    // For null, the FLAT should be returned as the default value.
    assertSame(PreferencePage.FLAT,
               PreferenceBaseFragment.toPreferencePage(null));

    // For invalid page name (like "TAKENOKO"), also FLAT should be returned as the default value.
    assertSame(PreferencePage.FLAT,
               PreferenceBaseFragment.toPreferencePage("TAKENOKO"));
  }

  @SmallTest
  public void testGetPrefrerencePageName() {
    // bundle is null.
    assertNull(PreferenceBaseFragment.getPrefrerencePageName(null));

    // "DICTIONARY"
    Bundle bundle = new Bundle();
    bundle.putString(PreferencePage.EXTRA_ARGUMENT_PREFERENCE_PAGE_NAME,
                     PreferencePage.DICTIONARY.name());
    assertEquals(PreferencePage.DICTIONARY.name(),
                 PreferenceBaseFragment.getPrefrerencePageName(bundle));

    // "KINOKO" (invalid page name, but returns it as is. Expectedly)
    bundle.clear();
    bundle.putString(PreferencePage.EXTRA_ARGUMENT_PREFERENCE_PAGE_NAME, "KINOKO");
    assertEquals("KINOKO",
                 PreferenceBaseFragment.getPrefrerencePageName(bundle));

    // null value in bundle.
    bundle.clear();
    bundle.putString(PreferencePage.EXTRA_ARGUMENT_PREFERENCE_PAGE_NAME, null);
    assertNull(PreferenceBaseFragment.getPrefrerencePageName(bundle));
  }
}
