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

import com.google.common.annotations.VisibleForTesting;

import android.media.AudioManager;

/**
 * FeedbackManager manages feed back events, like haptic and sound.
 *
 */
public class FeedbackManager {
  /**
   * Evnet types.
   */
  public enum FeedbackEvent {
    /**
     * Fired when a key is down.
     */
    KEY_DOWN(true, AudioManager.FX_KEYPRESS_STANDARD),
    /**
     * Fired when a candidate is selected by using candidate view.
     */
    CANDIDATE_SELECTED(true, AudioManager.FX_KEY_CLICK),
    /**
     * Fired when the input view is expanded (the candidate view is fold).
     */
    INPUTVIEW_EXPAND(true),
    /**
     * Fired when the input view is fold (the candidate view is expand).
     */
    INPUTVIEW_FOLD(true),
    ;
    // Constant value to indicate no sound feedback should be played.
    static final int NO_SOUND = -1;
    // If true, haptic feedback is fired.
    final boolean isHapticFeedbackTarget;
    final int soundEffectType;

    /**
     * @param isHapticFeedbackTarget true if the device should vibrate at the event.
     * @param soundEffectType the effect type of the feedback sound,
     *        defined in {@link AudioManager}.
     *        FeedbackListener.NO_SOUND for no sound feedback.
     */
    private FeedbackEvent(boolean isHapticFeedbackTarget, int soundEffectType) {
      this.isHapticFeedbackTarget = isHapticFeedbackTarget;
      this.soundEffectType = soundEffectType;
    }

    /**
     * @param isHapticFeedbackTarget true if the device should vibrate at the event.
     */
    private FeedbackEvent(boolean isHapticFeedbackTarget) {
      this(isHapticFeedbackTarget, NO_SOUND);
    }
  }

  interface FeedbackListener {
    /**
     * Called when vibrate feedback is fired.
     * @param duration the duration of vibration in millisecond.
     */
    public void onVibrate(long duration);

    /**
     * Called when sound feedback is fired.
     *
     * @param soundEffectType the effect type of the sound to be played.
     *        If FeedbackManager.NO_SOUND, no sound will be played.
     */
    public void onSound(int soundEffectType, float volume);
  }

  private boolean isHapticFeedbackEnabled;
  private long hapticFeedbackDuration = 30;  // 30ms by default.
  private boolean isSoundFeedbackEnabled;
  private float soundFeedbackVolume = 0.1f;  // System default volume parameter.
  @VisibleForTesting final FeedbackListener feedbackListener;

  /**
   * @param listener the listener which is called when feedback event is fired.
   */
  FeedbackManager(FeedbackListener listener) {
    // TODO(matsuzakit): This initial value should be changed
    //     after implementing setting screen.
    isHapticFeedbackEnabled = false;
    isSoundFeedbackEnabled = false;
    this.feedbackListener = listener;
  }

  void fireFeedback(FeedbackEvent event) {
    if (isHapticFeedbackEnabled && event.isHapticFeedbackTarget) {
      feedbackListener.onVibrate(hapticFeedbackDuration);
    }
    if (isSoundFeedbackEnabled && event.soundEffectType != FeedbackEvent.NO_SOUND) {
      feedbackListener.onSound(event.soundEffectType, soundFeedbackVolume);
    }
  }

  boolean isHapticFeedbackEnabled() {
    return isHapticFeedbackEnabled;
  }

  void setHapticFeedbackEnabled(boolean enable) {
    isHapticFeedbackEnabled = enable;
  }

  long getHapticFeedbackDuration() {
    return hapticFeedbackDuration;
  }

  void setHapticFeedbackDuration(long duration) {
    this.hapticFeedbackDuration = duration;
  }

  boolean isSoundFeedbackEnabled() {
    return isSoundFeedbackEnabled;
  }

  void setSoundFeedbackEnabled(boolean enable) {
    isSoundFeedbackEnabled = enable;
  }

  float getSoundFeedbackVolume() {
    return soundFeedbackVolume;
  }

  void setSoundFeedbackVolume(float volume) {
    this.soundFeedbackVolume = volume;
  }

  void release() {
    setSoundFeedbackEnabled(false);
  }
}
