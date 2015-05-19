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

import org.mozc.android.inputmethod.japanese.ViewManager.LayoutAdjustment;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.view.SkinType;

import android.content.SharedPreferences;
import android.content.res.Configuration;

/**
 * This class expresses the client-side preferences which corresponds to current
 * device configuration.
 *
 */
public class ClientSidePreference {
  /**
   * Keyboard layout.
   * A user can choose one of the layouts, and then the keyboard (sub)layouts are activated.
   *
   * Here "(sub)layouts" means that for example QWERTY layout contains following (sub)layouts;
   * QWERTY for Hiragana, QWERTY for Alphabet, QWERTY Symbol for Hiragana,
   * and QWERTY Symbol for Alphabet.
   * If a user choose a keyboard layout, (s)he can switch among the (sub)layouts which
   * belong to the selected layout.
   */
  public enum KeyboardLayout {
    TWELVE_KEYS,
    QWERTY,
    GODAN,
  }

  /**
   * A user's input style.
   */
  public enum InputStyle {
    TOGGLE,
    FLICK,
    TOGGLE_FLICK,
  }

  /**
   *  Hardware Keyboard Mapping.
   */
  public enum HardwareKeyMap {
    DEFAULT,
    JAPANESE109A,
    TWELVEKEY,
  }

  private final boolean isHapticFeedbackEnabled;
  private final long hapticFeedbackDuration;
  private final boolean isSoundFeedbackEnabled;
  private final int soundFeedbackVolume;
  private final boolean isPopupFeedbackEnabled;
  private final KeyboardLayout keyboardLayout;
  private final InputStyle inputStyle;
  private final boolean qwertyLayoutForAlphabet;
  private final boolean fullscreenMode;
  private final int flickSensitivity;
  private final EmojiProviderType emojiProviderType;
  private final HardwareKeyMap hardwareKeyMap;
  private final SkinType skinType;
  private final LayoutAdjustment layoutAdjustment;

  /** Percentage of keyboard height */
  private final int keyboardHeightRatio;

  /**
   * For testing purpose.
   *
   * If you want to use this method,
   * consider using {@link #ClientSidePreference(SharedPreferences, Configuration)} instead.
   */
  public ClientSidePreference(boolean isHapticFeedbackEnabled,
                              long hapticFeedbackDuration,
                              boolean isSoundFeedbackEnabled,
                              int soundFeedbackVolume,
                              boolean isPopupFeedbackEnabled,
                              KeyboardLayout keyboardLayout, InputStyle inputStyle,
                              boolean qwertyLayoutForAlphabet, boolean fullscreenMode,
                              int flickSensitivity,
                              EmojiProviderType emojiProviderType, HardwareKeyMap hardwareKeyMap,
                              SkinType skinType, LayoutAdjustment layoutAdjustment,
                              int keyboardHeightRatio) {
    this.isHapticFeedbackEnabled = isHapticFeedbackEnabled;
    this.hapticFeedbackDuration = hapticFeedbackDuration;
    this.isSoundFeedbackEnabled = isSoundFeedbackEnabled;
    this.soundFeedbackVolume = soundFeedbackVolume;
    this.isPopupFeedbackEnabled = isPopupFeedbackEnabled;
    this.keyboardLayout = keyboardLayout;
    this.inputStyle = inputStyle;
    this.qwertyLayoutForAlphabet = qwertyLayoutForAlphabet;
    this.fullscreenMode = fullscreenMode;
    this.flickSensitivity = flickSensitivity;
    this.emojiProviderType = emojiProviderType;
    this.hardwareKeyMap = hardwareKeyMap;
    this.skinType = skinType;
    this.layoutAdjustment = layoutAdjustment;
    this.keyboardHeightRatio = keyboardHeightRatio;
  }

  public ClientSidePreference(SharedPreferences sharedPreferences,
                              Configuration deviceConfiguration) {
    isHapticFeedbackEnabled =
        sharedPreferences.getBoolean("pref_haptic_feedback_key", false);
    hapticFeedbackDuration =
        sharedPreferences.getInt("pref_haptic_feedback_duration_key", 30);
    isSoundFeedbackEnabled =
        sharedPreferences.getBoolean("pref_sound_feedback_key", false);
    soundFeedbackVolume =
        sharedPreferences.getInt("pref_sound_feedback_volume_key", 50);
    isPopupFeedbackEnabled =
        sharedPreferences.getBoolean("pref_popup_feedback_key", true);

    String keyboardLayoutKey;
    String inputStyleKey;
    String qwertyLayoutForAlphabetKey;
    String flickSensitivityKey;
    String layoutAdjustmentKey;
    String keyboardHeightRatioKey;

    if (PreferenceUtil.isLandscapeKeyboardSettingActive(sharedPreferences, deviceConfiguration)) {
      keyboardLayoutKey = PreferenceUtil.PREF_LANDSCAPE_KEYBOARD_LAYOUT_KEY;
      inputStyleKey = PreferenceUtil.PREF_LANDSCAPE_INPUT_STYLE_KEY;
      qwertyLayoutForAlphabetKey = PreferenceUtil.PREF_LANDSCAPE_QWERTY_LAYOUT_FOR_ALPHABET_KEY;
      flickSensitivityKey = PreferenceUtil.PREF_LANDSCAPE_FLICK_SENSITIVITY_KEY;
      layoutAdjustmentKey = PreferenceUtil.PREF_LANDSCAPE_LAYOUT_ADJUSTMENT_KEY;
      keyboardHeightRatioKey = PreferenceUtil.PREF_LANDSCAPE_KEYBOARD_HEIGHT_RATIO_KEY;
    } else {
      keyboardLayoutKey = PreferenceUtil.PREF_PORTRAIT_KEYBOARD_LAYOUT_KEY;
      inputStyleKey = PreferenceUtil.PREF_PORTRAIT_INPUT_STYLE_KEY;
      qwertyLayoutForAlphabetKey = PreferenceUtil.PREF_PORTRAIT_QWERTY_LAYOUT_FOR_ALPHABET_KEY;
      flickSensitivityKey = PreferenceUtil.PREF_PORTRAIT_FLICK_SENSITIVITY_KEY;
      layoutAdjustmentKey = PreferenceUtil.PREF_PORTRAIT_LAYOUT_ADJUSTMENT_KEY;
      keyboardHeightRatioKey = PreferenceUtil.PREF_PORTRAIT_KEYBOARD_HEIGHT_RATIO_KEY;
    }

    // Don't apply pref_portrait_keyboard_settings_for_landscape for fullscreen mode.
    String fullscreenKey = (deviceConfiguration.orientation == Configuration.ORIENTATION_LANDSCAPE)
        ? PreferenceUtil.PREF_LANDSCAPE_FULLSCREEN_KEY
        : PreferenceUtil.PREF_PORTRAIT_FULLSCREEN_KEY;

    keyboardLayout = getEnum(
        sharedPreferences, keyboardLayoutKey,
        KeyboardLayout.class, KeyboardLayout.TWELVE_KEYS, KeyboardLayout.GODAN);
    inputStyle = getEnum(
        sharedPreferences, inputStyleKey,
        InputStyle.class, InputStyle.TOGGLE_FLICK);
    qwertyLayoutForAlphabet =
        sharedPreferences.getBoolean(qwertyLayoutForAlphabetKey, false);
    // On large screen device, pref_portrait_fullscreen_key and
    // pref_landscape_fullscreen_key are omitted
    // so below default value "false" is applied.
    fullscreenMode =
        sharedPreferences.getBoolean(fullscreenKey, false);
    flickSensitivity = sharedPreferences.getInt(flickSensitivityKey, 0);

    emojiProviderType = getEnum(
        sharedPreferences, "pref_emoji_provider_type", EmojiProviderType.class, null);

    hardwareKeyMap = getEnum(
        sharedPreferences, "pref_hardware_keymap", HardwareKeyMap.class, null);
    skinType = getEnum(
        sharedPreferences, "pref_skin_type_key", SkinType.class, SkinType.BLUE_LIGHTGRAY);
    layoutAdjustment = getEnum(
        sharedPreferences, layoutAdjustmentKey, LayoutAdjustment.class, LayoutAdjustment.FILL);
    keyboardHeightRatio = sharedPreferences.getInt(keyboardHeightRatioKey, 100);
  }

  /**
   * Gets an enum value in the SharedPreference.
   *
   * @param sharedPreference a {@link SharedPreferences} to be loaded.
   * @param key a key name
   * @param type a class of enum value
   * @param defaultValue default value if the {@link SharedPreferences} doesn't have corresponding
   *        entry.
   * @param conversionRecoveryValue default value if unknown value is stored.
   *        For example, if the value is "ALPHA" and {@code type} doesn't have "ALPHA" entry,
   *        this argument is returned.
   */
  static <T extends Enum<T>> T getEnum(
      SharedPreferences sharedPreference, String key, Class<T> type,
      T defaultValue, T conversionRecoveryValue) {
    if (sharedPreference == null) {
      return defaultValue;
    }
    String name = sharedPreference.getString(key, null);
    if (name != null) {
      try {
        return Enum.valueOf(type, name);
      } catch (IllegalArgumentException e) {
        return conversionRecoveryValue;
      }
    }
    return defaultValue;
  }

  /**
   * Same as {@link #getEnum(SharedPreferences, String, Class, Enum, Enum)}.
   *
   * {@code defaultValue} is used as {@code conversionRecoveryValue}
   */
  static <T extends Enum<T>> T getEnum(
      SharedPreferences sharedPreference, String key, Class<T> type, T defaultValue) {
    return getEnum(sharedPreference, key, type, defaultValue, defaultValue);
  }

  public boolean isHapticFeedbackEnabled() {
    return isHapticFeedbackEnabled;
  }

  public long getHapticFeedbackDuration() {
    return hapticFeedbackDuration;
  }

  public boolean isSoundFeedbackEnabled() {
    return isSoundFeedbackEnabled;
  }

  public int getSoundFeedbackVolume() {
    return soundFeedbackVolume;
  }

  public boolean isPopupFeedbackEnabled() {
    return isPopupFeedbackEnabled;
  }

  public KeyboardLayout getKeyboardLayout() {
    return keyboardLayout;
  }

  public InputStyle getInputStyle() {
    return inputStyle;
  }

  public boolean isQwertyLayoutForAlphabet() {
    return qwertyLayoutForAlphabet;
  }

  public boolean isFullscreenMode() {
    return fullscreenMode;
  }

  public int getFlickSensitivity() {
    return flickSensitivity;
  }

  public EmojiProviderType getEmojiProviderType() {
    return emojiProviderType;
  }

  public HardwareKeyMap getHardwareKeyMap() {
    return hardwareKeyMap;
  }

  public SkinType getSkinType() {
    return skinType;
  }

  public LayoutAdjustment getLayoutAdjustment() {
    return layoutAdjustment;
  }

  public int getKeyboardHeightRatio() {
    return keyboardHeightRatio;
  }
}
