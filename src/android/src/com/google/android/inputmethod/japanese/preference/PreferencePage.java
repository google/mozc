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
import org.mozc.android.inputmethod.japanese.resources.R;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Corresponds to a preference fragment.
 *
 * FLAT is used for classic UI, which shows all preferences.
 */
enum PreferencePage {
  FLAT,

  SOFTWARE_KEYBOARD,
  INPUT_SUPPORT,
  CONVERSION,
  DICTIONARY,
  USER_FEEDBACK,
  ABOUT,

  // For debug build.
  DEVELOPMENT,
  ;

  static final String EXTRA_ARGUMENT_PREFERENCE_PAGE_NAME = "PREFERENCE_PAGE";

  /**
   * Returns resouce id list which corresponds to given {@link PreferencePage}.
   * @param page a page for resource ids
   * @param isDebug true if this is debug mode. Effective only when {@code page} is FLAT.
   * @param useUsageStats true if usage stats / user feedback page should be shown.
   *                      Effective only when {@code page} is FLAT.
   * @return resource id list
   */
  public static List<Integer> getResourceIdList(
      PreferencePage page, boolean isDebug, boolean useUsageStats) {
    switch (page) {
      case FLAT:
        List<Integer> result = new ArrayList<Integer>(Arrays.asList(
            R.xml.pref_software_keyboard, R.xml.pref_input_support, R.xml.pref_conversion,
            R.xml.pref_dictionary));
        if (useUsageStats) {
          result.add(R.xml.pref_user_feedback);
        }
        result.add(R.xml.pref_about);
        if (isDebug) {
          result.add(R.xml.pref_development);
        }
        return Collections.unmodifiableList(result);
      case SOFTWARE_KEYBOARD:
        return Collections.singletonList(R.xml.pref_software_keyboard_advanced);
      case INPUT_SUPPORT:
        return Collections.singletonList(R.xml.pref_input_support);
      case CONVERSION:
        return Collections.singletonList(R.xml.pref_conversion);
      case DICTIONARY:
        return Collections.singletonList(R.xml.pref_dictionary);
      case USER_FEEDBACK:
        return Collections.singletonList(R.xml.pref_user_feedback);
      case ABOUT:
        return Collections.singletonList(R.xml.pref_about);
      case DEVELOPMENT:
        return Collections.singletonList(R.xml.pref_development);
      default:
        MozcLog.e(String.format("Unexpected preference page: %s", page.toString()));
        return Collections.emptyList();
    }
  }
}
