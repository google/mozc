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

package org.mozc.android.inputmethod.japanese;

import android.view.inputmethod.EditorInfo;

import java.util.EnumSet;

/**
 * Configuration for small behavior changing on Mozc depending on the target application.
 *
 */
public class ApplicationCompatibility {
  private static enum CompatibilityMode {
    // Set the selection position always. This operation can be dangerous.
    //
    // -- Background --
    // This could have some unexpected behavior, especially if the target application modifies
    // the composition text, or some sent operations via InputConnection.
    // Unfortunately, under the current framework, there are no ways to treat a caret position
    // *correctly*. For example, it is difficult to just keep the caret position at the end of
    // the composition text, because onUpdateSelection caused by *something* other than Mozc
    // are sometimes just skipped.
    // So, this should be just our best effort. If we hit some bad situation,
    // we should be able to modify (or simply remove) this behavior to keep the caret at
    // the end of the composition text.
    ALWAYS_SET_CURSOR,

    // A flag to check if the full screen mode is supported on the application.
    //
    // -- Background --
    // This is a hack to disable the feature on Chrome.
    // WebTextView on Chrome has a bug in the fullscreen mode so we have to avoid using it.
    // Omnibar can be used without the bug so this workaround is only for text forms.
    // However, the fullscreen mode is not so useful for Omnibar, because it is on the top
    // of the screen. So, we don't take care of if the connected field is Omnibar or not.
    FULLSCREEN_MODE_SUPPORTED,

    // TODO(hidehiko): Move hack for WebView in SelectionTracker.
  }

  /** The default configuration */
  private static final ApplicationCompatibility DEFAULT_INSTANCE =
      new ApplicationCompatibility(EnumSet.of(CompatibilityMode.FULLSCREEN_MODE_SUPPORTED));

  /** The special configuration for firefox. */
  private static final ApplicationCompatibility FIREFOX_INSTANCE =
      new ApplicationCompatibility(EnumSet.of(CompatibilityMode.ALWAYS_SET_CURSOR,
                                              CompatibilityMode.FULLSCREEN_MODE_SUPPORTED));

  /** The special configuration for Chrome. */
  private static final ApplicationCompatibility CHROME_INSTANCE =
      new ApplicationCompatibility(EnumSet.noneOf(CompatibilityMode.class));

  private final EnumSet<CompatibilityMode> compatibilityModeSet;

  private ApplicationCompatibility(EnumSet<CompatibilityMode> compatibilityModeSet) {
    this.compatibilityModeSet = compatibilityModeSet;
  }

  /**
   * @return {@code true} if we should send setSelection via inputConnection always when preedit
   *   is set.
   */
  public boolean isAlwaysSetCursorEnabled() {
    return compatibilityModeSet.contains(CompatibilityMode.ALWAYS_SET_CURSOR);
  }

  /** @return {@code true} if the target application supports full screen mode. */
  public boolean isFullScreenModeSupported() {
    return compatibilityModeSet.contains(CompatibilityMode.FULLSCREEN_MODE_SUPPORTED);
  }

  /**
   * @return an instance for the connected application.
   */
  public static ApplicationCompatibility getInstance(EditorInfo editorInfo) {
    if (editorInfo != null) {
      if ("com.android.chrome".equals(editorInfo.packageName)) {
        return CHROME_INSTANCE;
      }
      if ("org.mozilla.firefox".equals(editorInfo.packageName)) {
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
