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

package org.mozc.android.inputmethod.japanese.testing;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;

/**
 * Testing utilities about {@link SharedPreferences}.
 *
 */
public class MozcPreferenceUtil {
  // Disallow instantiation.
  private MozcPreferenceUtil() {
  }

  /**
   * @return a private {@link SharedPreferences} instance based on the given {@code name}.
   */
  public static SharedPreferences getSharedPreferences(Context context, String name) {
    return context.getSharedPreferences(name, Activity.MODE_PRIVATE);
  }

  /**
   * @return a private {@link SharedPreferences} instance, which contains given
   * {@code (key, value)}, based on the given {@code preferenceName}.
   */
  public static SharedPreferences newSharedPreference(
      Context context, String preferenceName, String key, Object value) {
    SharedPreferences sharedPreferences = getSharedPreferences(context, preferenceName);
    updateSharedPreference(sharedPreferences, key, value);
    return sharedPreferences;
  }

  /**
   * Add a {@code (key, value)} data into the given {@code sharedPreferences}.
   * Now, this method supports only {@code boolean} and {@link String}.
   */
  public static void updateSharedPreference(
      SharedPreferences sharedPreferences, String key, Object value) {
    SharedPreferences.Editor editor = sharedPreferences.edit();
    if (value.getClass() == Boolean.class) {
      editor.putBoolean(key, Boolean.class.cast(value));
    } else if (value.getClass() == String.class) {
      editor.putString(key, String.class.cast(value));
    } else if (value.getClass() == Integer.class) {
      editor.putInt(key, Integer.class.cast(value));
    } else {
      throw new IllegalArgumentException("value's type can be only Boolean or String.");
    }
    editor.commit();
  }
}
