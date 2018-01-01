// Copyright 2010-2018, Google Inc.
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

package org.mozc.android.inputmethod.japanese.accessibility;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.content.Context;
import android.os.Build;
import android.provider.Settings;
import android.view.accessibility.AccessibilityEvent;

import java.lang.reflect.Field;

/**
 * Utility class for accessibility.
 */
public class AccessibilityUtil {

  private AccessibilityUtil() {}

  private static Optional<AccessibilityManagerWrapper> accessibilityManager =
      Optional.<AccessibilityManagerWrapper>absent();
  private static Optional<AudioManagerWrapper> audioManager =
      Optional.<AudioManagerWrapper>absent();
  private static Optional<Boolean> isAccessibilitySpeakPasswordEnabled =
      Optional.<Boolean>absent();
  private static Optional<Boolean> isAccessibilityEnabled =
      Optional.<Boolean>absent();

  /**
   * Resets internal status.
   *
   * <p>Should be used only from testcases.
   */
  @VisibleForTesting
  static void reset() {
    setAccessibilityManagerForTesting(Optional.<AccessibilityManagerWrapper>absent());
    setAudioManagerForTesting(Optional.<AudioManagerWrapper>absent());
    setAccessibilitySpeakPasswordEnabled(Optional.<Boolean>absent());
    setAccessibilityEnabled(Optional.<Boolean>absent());
  }

  /**
   * Sets manager instance for testing.
   *
   * If absent, real service is used.
   */
  @VisibleForTesting
  static void setAccessibilityManagerForTesting(Optional<AccessibilityManagerWrapper> manager) {
    accessibilityManager = Preconditions.checkNotNull(manager);
  }

  /**
   * Gets manager instance.
   *
   * <p>If {@code #setAccessibilityManagerForTesting(Optional)} nor this method have not been
   * invoked yet, cache instance is created inside.
   * If not, cached instance (or set one by setAccessibilityManagerForTesting)
   * is returned.
   * Therefore this is lightweight method.
   */
  static AccessibilityManagerWrapper getAccessibilityManager(Context context) {
    if (!accessibilityManager.isPresent()) {
      accessibilityManager =
          Optional.of(new AccessibilityManagerWrapper(Preconditions.checkNotNull(context)));
    }
    return accessibilityManager.get();
  }

  /**
   * @see #setAccessibilityManagerForTesting(Optional)
   */
  @VisibleForTesting
  static void setAudioManagerForTesting(Optional<AudioManagerWrapper> manager) {
    audioManager = Preconditions.checkNotNull(manager);
  }

  /**
   * @see #getAccessibilityManager(Context)
   */
  static AudioManagerWrapper getAudioManager(Context context) {
    if (!audioManager.isPresent()) {
      audioManager =
          Optional.of(new AudioManagerWrapper(Preconditions.checkNotNull(context)));
    }
    return audioManager.get();
  }

  static void setAccessibilityEnabled(Optional<Boolean> enabled) {
    isAccessibilityEnabled = Preconditions.checkNotNull(enabled);
  }

  static boolean isAccessibilityEnabled(Context context) {
    return isAccessibilityEnabled.isPresent()
        ? isAccessibilityEnabled.get()
        : getAccessibilityManager(context).isEnabled();
  }

  /**
   * @return true if touch exploration is enabled
   */
  public static boolean isTouchExplorationEnabled(Context context) {
    return getAccessibilityManager(context).isTouchExplorationEnabled();
  }
  /**
   * Sends {@code AccessibilityEvent} to {@code AccessibilityManager}.
   * Accessibility must be enabled.
   */
  static void sendAccessibilityEvent(Context context, AccessibilityEvent event) {
    AccessibilityManagerWrapper manager = getAccessibilityManager(context);
    Preconditions.checkState(manager.isEnabled());
    manager.sendAccessibilityEvent(event);
  }

  @VisibleForTesting
  static void setAccessibilitySpeakPasswordEnabled(Optional<Boolean> enabled) {
    isAccessibilitySpeakPasswordEnabled = Preconditions.checkNotNull(enabled);
  }

  /**
   * Gets Settings.Secure.ACCESSIBILITY_SPEAK_PASSWORD using reflection.
   * The field is since API Level 15.
   */
  static boolean isAccessibilitySpeakPasswordEnabled() {
    if (isAccessibilitySpeakPasswordEnabled.isPresent()) {
      return isAccessibilitySpeakPasswordEnabled.get();
    }
    if (Build.VERSION.SDK_INT < 15) {
      return false;
    }
    try {
      Field field = Settings.Secure.class.getField("ACCESSIBILITY_SPEAK_PASSWORD");
      return String.class.cast(field.get(null)).equals("1");
    } catch (NoSuchFieldException e) {
      // Do nothing.
    } catch (IllegalArgumentException e) {
      // Do nothing.
    } catch (IllegalAccessException e) {
      // Do nothing.
    }
    return false;
  }
}
