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

package org.mozc.android.inputmethod.japanese.preference;

import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.KeyboardLayout;
import org.mozc.android.inputmethod.japanese.resources.R;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.PreferenceGroup;
import android.preference.PreferenceManager;
import android.util.DisplayMetrics;

/**
 * Utilities for Mozc preferences.
 *
 */
public class PreferenceUtil {

  /** Simple {@code PreferenceManager} wrapper for testing purpose. */
  interface PreferenceManagerInterface {
    public Preference findPreference(CharSequence key);
  }

  private static class PreferenceManagerInterfaceImpl implements PreferenceManagerInterface {
    private final PreferenceManager preferenceManager;

    PreferenceManagerInterfaceImpl(PreferenceManager preferenceManager) {
      this.preferenceManager = preferenceManager;
    }

    @Override
    public Preference findPreference(CharSequence key) {
      return preferenceManager.findPreference(key);
    }
  }

  static class CurrentKeyboardLayoutPreferenceChangeListener implements OnPreferenceChangeListener {
    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
      if (!(newValue instanceof KeyboardLayout)) {
        return false;
      }

      // Write back to with the appropriate preference key.
      SharedPreferences sharedPreferences = preference.getSharedPreferences();
      boolean isLandscapeKeyboardSettingActive = isLandscapeKeyboardSettingActive(
          sharedPreferences, preference.getContext().getResources().getConfiguration());
      sharedPreferences.edit()
          .putString(
              isLandscapeKeyboardSettingActive
                  ? PREF_LANDSCAPE_KEYBOARD_LAYOUT_KEY
                  : PREF_PORTRAIT_KEYBOARD_LAYOUT_KEY,
              KeyboardLayout.class.cast(newValue).name())
          .commit();
      return true;
    }
  }

  // Keys for Keyboard Layout.
  public static final String PREF_CURRENT_KEYBOARD_LAYOUT_KEY =
      "pref_current_keyboard_layout_key";

  public static final String PREF_SOFTWARE_KEYBOARD_ADVANED_PORTRAIT_KEY =
      "pref_software_keyboard_advanced_portrait_key";
  public static final String PREF_PORTRAIT_KEYBOARD_LAYOUT_KEY =
      "pref_portrait_keyboard_layout_key";
  public static final String PREF_PORTRAIT_INPUT_STYLE_KEY =
      "pref_portrait_input_style_key";
  public static final String PREF_PORTRAIT_QWERTY_LAYOUT_FOR_ALPHABET_KEY =
      "pref_portrait_qwerty_layout_for_alphabet_key";
  public static final String PREF_PORTRAIT_FLICK_SENSITIVITY_KEY =
      "pref_portrait_flick_sensitivity_key";
  public static final String PREF_PORTRAIT_LAYOUT_ADJUSTMENT_KEY =
      "pref_portrait_layout_adjustment_key";
  public static final String PREF_PORTRAIT_KEYBOARD_HEIGHT_RATIO_KEY =
      "pref_portrait_keyboard_height_ratio_key";

  public static final String PREF_SOFTWARE_KEYBOARD_ADVANED_LANDSCAPE_KEY =
      "pref_software_keyboard_advanced_landscape_key";
  public static final String PREF_LANDSCAPE_KEYBOARD_LAYOUT_KEY =
      "pref_landscape_keyboard_layout_key";
  public static final String PREF_LANDSCAPE_INPUT_STYLE_KEY =
      "pref_landscape_input_style_key";
  public static final String PREF_LANDSCAPE_QWERTY_LAYOUT_FOR_ALPHABET_KEY =
      "pref_landscape_qwerty_layout_for_alphabet_key";
  public static final String PREF_LANDSCAPE_FLICK_SENSITIVITY_KEY =
      "pref_landscape_flick_sensitivity_key";
  public static final String PREF_LANDSCAPE_LAYOUT_ADJUSTMENT_KEY =
      "pref_landscape_layout_adjustment_key";
  public static final String PREF_LANDSCAPE_KEYBOARD_HEIGHT_RATIO_KEY =
      "pref_landscape_keyboard_height_ratio_key";

  public static final String PREF_USE_PORTRAIT_KEYBOARD_SETTINGS_FOR_LANDSCAPE_KEY =
      "pref_use_portrait_keyboard_settings_for_landscape_key";

  // Full screen keys.
  public static final String PREF_PORTRAIT_FULLSCREEN_KEY =
      "pref_portrait_fullscreen_key";
  public static final String PREF_LANDSCAPE_FULLSCREEN_KEY =
      "pref_landscape_fullscreen_key";
  public static final String PREF_SKIN_TYPE = "pref_skin_type_key";
  public static final String PREF_HARDWARE_KEYMAP = "pref_hardware_keymap";

  // Keys for generic preferences.
  public static final String PREF_HAPTIC_FEEDBACK_KEY = "pref_haptic_feedback_key";
  public static final String PREF_HAPTIC_FEEDBACK_DURATION_KEY =
      "pref_haptic_feedback_duration_key";
  public static final String PREF_SOUND_FEEDBACK_KEY = "pref_sound_feedback_key";
  public static final String PREF_SOUND_FEEDBACK_VOLUME_KEY =
      "pref_sound_feedback_volume_key";
  public static final String PREF_POPUP_FEEDBACK_KEY = "pref_popup_feedback_key";
  public static final String PREF_SPACE_CHARACTER_FORM_KEY =
      "pref_space_character_form_key";
  public static final String PREF_KANA_MODIFIER_INSENSITIVE_CONVERSION_KEY =
      "pref_kana_modifier_insensitive_conversion";
  public static final String PREF_TYPING_CORRECTION_KEY =
      "pref_typing_correction";
  public static final String PREF_EMOJI_PROVIDER_TYPE =
      "pref_emoji_provider_type";
  public static final String PREF_DICTIONARY_PERSONALIZATION_KEY =
      "pref_dictionary_personalization_key";
  public static final String PREF_DICTIONARY_USER_DICTIONARY_TOOL_KEY =
      "pref_dictionary_user_dictionary_tool_key";
  public static final String PREF_OTHER_INCOGNITO_MODE_KEY = "pref_other_anonimous_mode_key";
  public static final String PREF_OTHER_USAGE_STATS_KEY = "pref_other_usage_stats_key";
  public static final String PREF_ABOUT_VERSION = "pref_about_version";

  private static final OnPreferenceChangeListener
      CURRENT_KEYBOARD_LAYOUT_PREFERENCE_CHANGE_LISTENER =
          new CurrentKeyboardLayoutPreferenceChangeListener();

  // Disallow instantiation.
  private PreferenceUtil() {
  }

  static boolean isLandscapeKeyboardSettingActive(SharedPreferences sharedPreferences,
                                                  Configuration configuration) {
    if (sharedPreferences.getBoolean(PREF_USE_PORTRAIT_KEYBOARD_SETTINGS_FOR_LANDSCAPE_KEY, true)) {
      // Always use portrait configuration.
      return false;
    }

    return configuration.orientation == Configuration.ORIENTATION_LANDSCAPE;
  }

  /**
   * Initializes some preferences which need special initialization.
   */
  static void initializeSpecialPreferences(PreferenceManager preferenceManager) {
    if (preferenceManager == null) {
      return;
    }

    initializeSpecialPreferencesInternal(new PreferenceManagerInterfaceImpl(preferenceManager));
  }

  static void initializeSpecialPreferencesInternal(PreferenceManagerInterface preferenceManager) {
    initializeCurrentKeyboardLayoutPreference(
        preferenceManager.findPreference(PREF_CURRENT_KEYBOARD_LAYOUT_KEY));
    initializeUsageStatsPreference(preferenceManager.findPreference(PREF_OTHER_USAGE_STATS_KEY));
    initializeVersionPreference(preferenceManager.findPreference(PREF_ABOUT_VERSION));
    initializeLayoutAdjustmentPreference(preferenceManager);

  }

  private static void initializeCurrentKeyboardLayoutPreference(Preference preference) {
    if (!(preference instanceof KeyboardLayoutPreference)) {
      return;
    }

    // Initialize the value based on the current orientation.
    SharedPreferences sharedPreferences = preference.getSharedPreferences();
    boolean isLandscapeKeyboardSettingActive = isLandscapeKeyboardSettingActive(
        sharedPreferences, preference.getContext().getResources().getConfiguration());
    KeyboardLayoutPreference.class.cast(preference).setValue(
        getKeyboardLayout(sharedPreferences,
                          isLandscapeKeyboardSettingActive
                              ? PREF_LANDSCAPE_KEYBOARD_LAYOUT_KEY
                              : PREF_PORTRAIT_KEYBOARD_LAYOUT_KEY));
    preference.setOnPreferenceChangeListener(CURRENT_KEYBOARD_LAYOUT_PREFERENCE_CHANGE_LISTENER);
  }

  /**
   * Returns parsed {@link KeyboardLayout} instance, or TWELVE_KEYS if any error is found.
   */
  private static KeyboardLayout getKeyboardLayout(SharedPreferences sharedPreferences, String key) {
    if (sharedPreferences == null || key == null) {
      return KeyboardLayout.TWELVE_KEYS;
    }

    String value = sharedPreferences.getString(key, null);
    if (value == null) {
      return KeyboardLayout.TWELVE_KEYS;
    }

    try {
      return KeyboardLayout.valueOf(value);
    } catch (IllegalArgumentException e) {
      MozcLog.e("Invalid KeyboardLayout: " + value);
    }

    return KeyboardLayout.TWELVE_KEYS;
  }

  private static void initializeUsageStatsPreference(Preference preference) {
    if (preference == null) {
      return;
    }

    Context context = preference.getContext();
    Resources resources = context.getResources();
    preference.setSummary(
        resources.getString(R.string.pref_other_usage_stats_description,
                            resources.getString(R.string.developer_organization)));
    // Disable (always on to send usage stats) for dev-channel build.
    preference.setEnabled(!MozcUtil.isDevChannel(context));
  }

  private static void initializeVersionPreference(Preference preference) {
    if (preference == null) {
      return;
    }
    preference.setSummary(MozcUtil.getVersionName(preference.getContext()));
  }

  static boolean shouldRemoveLayoutAdjustmentPreferences(
      PreferenceManagerInterface preferenceManager) {
    Preference preference = preferenceManager.findPreference(
        PREF_SOFTWARE_KEYBOARD_ADVANED_PORTRAIT_KEY);
    if (preference == null) {
      return false;
    }

    Resources resouces = preference.getContext().getResources();
    int smallestWidth = resouces.getDimensionPixelSize(R.dimen.ime_window_partial_width)
        + resouces.getDimensionPixelSize(R.dimen.side_frame_width);
    DisplayMetrics displayMetrics = resouces.getDisplayMetrics();
    return displayMetrics.widthPixels < smallestWidth
        || displayMetrics.heightPixels < smallestWidth;
  }

  static void removePreference(PreferenceManagerInterface preferenceManager, CharSequence key) {
    CharSequence parentKey;
    if (key == PREF_PORTRAIT_LAYOUT_ADJUSTMENT_KEY) {
      parentKey = PREF_SOFTWARE_KEYBOARD_ADVANED_PORTRAIT_KEY;
    } else if (key == PREF_LANDSCAPE_LAYOUT_ADJUSTMENT_KEY) {
      parentKey = PREF_SOFTWARE_KEYBOARD_ADVANED_LANDSCAPE_KEY;
    } else {
      return;
    }

    Preference preference = preferenceManager.findPreference(key);
    PreferenceGroup parentPreference = PreferenceGroup.class.cast(preferenceManager
        .findPreference(parentKey));
    if (preference == null || parentPreference == null) {
      return;
    }
    parentPreference.removePreference(preference);
  }

  static void initializeLayoutAdjustmentPreference(PreferenceManagerInterface preferenceManager) {
    if (shouldRemoveLayoutAdjustmentPreferences(preferenceManager)) {
      removePreference(preferenceManager, PREF_PORTRAIT_LAYOUT_ADJUSTMENT_KEY);
      removePreference(preferenceManager, PREF_LANDSCAPE_LAYOUT_ADJUSTMENT_KEY);
    }
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
  public static <T extends Enum<T>> T getEnum(
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
   * Same as {@link getEnum}.
   *
   * {@code defaultValue} is used as {@code conversionRecoveryValue}
   */
  public static <T extends Enum<T>> T getEnum(
      SharedPreferences sharedPreference, String key, Class<T> type, T defaultValue) {
    return getEnum(sharedPreference, key, type, defaultValue, defaultValue);
  }
}
