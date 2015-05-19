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

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

/**
 * A proxy activity forwarding to another activity.
 *
 * This activity is used to switch target activity based on runtime configuration.
 * For example,
 * <ul>
 * <li>"Modern" preference screen vs "Classic" one, based on API level.
 * </ul>
 * This can be done by using string resources (defining destination activity by
 * string resources in preference XML file) except for launching from home screen,
 * which sees AndroidManifest.xml which cannot refer string resources.
 * In fact the initial motivation to introduce this class is to launch appropriate
 * preference activity from home screen.
 *
 * It is found that switching based on string resource is hard to test because
 * precise control is impossible.
 * Now {@link org.mozc.android.inputmethod.japanese.DependencyFactory.Dependency}
 * has been introduced so switching feature becomes dependent on it.
 *
 */
public abstract class MozcProxyActivity extends Activity {

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    startActivity(getForwardIntent());
    finish();
  }

  /**
   * Returns an Intent to move to the destination activity.
   *
   * Called from {@link #onCreate(Bundle)}.
   */
  protected abstract Intent getForwardIntent();
}
