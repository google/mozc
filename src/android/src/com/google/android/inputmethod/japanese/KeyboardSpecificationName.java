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

import android.content.res.Configuration;

/**
 * Name of keyboard (or keyboard-like-view).
 *
 * The formatted name is sent to the usage stats server.
 *
 */
public class KeyboardSpecificationName {
  public final String baseName;
  public final int major;
  public final int minor;
  public final int revision;
  public KeyboardSpecificationName(String baseName, int major, int minor, int revision) {
    this.baseName = baseName;
    this.major = major;
    this.minor = minor;
    this.revision = revision;
  }

  /**
   * Get formatted keyboard name based on given parameters.
   *
   * The main purpose of the formatted name is collecting usage stats.
   */
  public String formattedKeyboardName(Configuration configuration) {
      return new StringBuilder(baseName).append('-')
                                        .append(major)
                                        .append('.')
                                        .append(minor)
                                        .append('.')
                                        .append(revision)
                                        .append('-')
                                        .append(getDeviceOrientationString(configuration))
                                        .toString();
  }

  /**
   * Returns *Canonical* orientation string, which is used as a part of keyboard name.
   */
  public static String getDeviceOrientationString(Configuration configuration) {
    if (configuration != null) {
      switch (configuration.orientation) {
        case Configuration.ORIENTATION_PORTRAIT:
          return "PORTRAIT";
        case Configuration.ORIENTATION_LANDSCAPE:
          return "LANDSCAPE";
        case Configuration.ORIENTATION_SQUARE:
          return "SQUARE";
        case Configuration.ORIENTATION_UNDEFINED:
          return "UNDEFINED";
      }
    }
    // If none of above is matched to the orientation or configuration is null,
    // we return "UNKNOWN".
    return "UNKNOWN";
  }
}
