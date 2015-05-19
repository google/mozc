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

import android.view.inputmethod.EditorInfo;

import java.util.EnumSet;

/**
 * Configuration for small behavior changing on Mozc depending on the target application.
 *
 */
public class ApplicationCompatibility {

  private static enum CompatibilityMode {

    // A flag to check if the full screen mode is supported on the application.
    //
    // -- Background --
    // This is a hack to disable the feature on Chrome.
    // WebTextView on Chrome has a bug in the fullscreen mode so we have to avoid using it.
    // Omnibar can be used without the bug so this workaround is only for text forms.
    // However, the fullscreen mode is not so useful for Omnibar, because it is on the top
    // of the screen. So, we don't take care of if the connected field is Omnibar or not.
    FULLSCREEN_MODE_SUPPORTED,

    // A flag for the applications which requires special treatment like WebEditText.
    //
    // -- Background --
    // WebEditText requires special treatment to track selection range.
    // In addition some applications (e.g. Chrome) requires the same treatment.
    // The treatment is for the applications/views where onUpdateSelection is not called back
    // when they updates the composition.
    // The treatment could be applicable always but the solution might be slight hacky so
    // currently it is applied on white-listed applications.
    PRETEND_WEB_EDIT_TEXT,
  }

  /** The default configuration */
  private static final ApplicationCompatibility DEFAULT_INSTANCE =
      new ApplicationCompatibility(EnumSet.of(CompatibilityMode.FULLSCREEN_MODE_SUPPORTED));

  /** The special configuration for firefox. */
  private static final ApplicationCompatibility FIREFOX_INSTANCE =
      new ApplicationCompatibility(EnumSet.of(CompatibilityMode.FULLSCREEN_MODE_SUPPORTED));

  /** The special configuration for Chrome. */
  private static final ApplicationCompatibility CHROME_INSTANCE =
      new ApplicationCompatibility(EnumSet.noneOf(CompatibilityMode.class));

  /** The special configuration for Evernote, of which onSelectionUpdate is unreliable. */
  private static final ApplicationCompatibility EVERNOTE_INSTANCE =
      new ApplicationCompatibility(EnumSet.of(CompatibilityMode.PRETEND_WEB_EDIT_TEXT));

  private final EnumSet<CompatibilityMode> compatibilityModeSet;

  private ApplicationCompatibility(EnumSet<CompatibilityMode> compatibilityModeSet) {
    this.compatibilityModeSet = compatibilityModeSet;
  }

  /** @return {@code true} if the target application supports full screen mode. */
  public boolean isFullScreenModeSupported() {
    return compatibilityModeSet.contains(CompatibilityMode.FULLSCREEN_MODE_SUPPORTED);
  }

  /** @return {@code true} if the target application requires special behavior like WebEditText. */
  public boolean isPretendingWebEditText() {
    return compatibilityModeSet.contains(CompatibilityMode.PRETEND_WEB_EDIT_TEXT);
  }

  /**
   * @return an instance for the connected application.
   */
  public static ApplicationCompatibility getInstance(EditorInfo editorInfo) {
    if (editorInfo != null) {
      String packageName = editorInfo.packageName;
      if ("com.android.chrome".equals(packageName) || "com.chrome.beta".equals(packageName)) {
        return CHROME_INSTANCE;
      }
      if ("com.evernote".equals(packageName)) {
        return EVERNOTE_INSTANCE;
      }
      if ("org.mozilla.firefox".equals(packageName)) {
        return FIREFOX_INSTANCE;
      }
    }
    return DEFAULT_INSTANCE;
  }

  /**
   * @return the default configuration instance.
   */
  public static ApplicationCompatibility getDefaultInstance() {
    return DEFAULT_INSTANCE;
  }
}
