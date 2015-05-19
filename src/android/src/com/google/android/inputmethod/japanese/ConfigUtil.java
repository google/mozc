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

import org.mozc.android.inputmethod.japanese.preference.PreferenceUtil;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoConfig.Config;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoConfig.Config.FundamentalCharacterForm;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoConfig.Config.HistoryLearningLevel;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoConfig.GeneralConfig;

import android.content.SharedPreferences;

/**
 * Utilities to convert values stored in SharedPreferences into a Config proto instance.
 *
 * Note: Currently the primary configuration data is stored in SharedPreferences, and mozc server
 *   just reflects the configuration (via setConfig/setImposedConfig).
 *   However, when we start to support sync-features, mozc server may overwrite its configuration
 *   by itself. So we need to re-design configuration related stuff to support sync-features.
 */
public class ConfigUtil {
  // Disallow instantiation.
  private ConfigUtil() {
  }

  /**
   * Converts {@code SharedPreferences} to {@code Config}.
   *
   * Now this method only supports:
   * - space_input_character
   * - use_kana_modifier_insensitive_conversion
   * - use_typing_correction
   * - history_learning_level
   * - incognito_mode, and
   * - general_config (please see also toGeneralConfig(SharedPreferences)).
   */
  static Config toConfig(SharedPreferences sharedPreferences) {
    Config.Builder builder = null;

    FundamentalCharacterForm spaceCharacterForm =
        getFundamentalCharacterFormFromBooleanKey(
            sharedPreferences, PreferenceUtil.PREF_SPACE_CHARACTER_FORM_KEY);
    if (spaceCharacterForm != null) {
      builder = maybeCreateConfigBuilder(builder).setSpaceCharacterForm(spaceCharacterForm);
    }


    Boolean kanaModifierInsensitiveConversion = getBoolean(
        sharedPreferences, PreferenceUtil.PREF_KANA_MODIFIER_INSENSITIVE_CONVERSION_KEY);
    if (kanaModifierInsensitiveConversion != null) {
      builder = maybeCreateConfigBuilder(builder)
          .setUseKanaModifierInsensitiveConversion(
              kanaModifierInsensitiveConversion.booleanValue());
    }

    Boolean typingCorrection = getBoolean(
        sharedPreferences, PreferenceUtil.PREF_TYPING_CORRECTION_KEY);
    if (typingCorrection != null) {
      builder = maybeCreateConfigBuilder(builder)
          .setUseTypingCorrection(typingCorrection.booleanValue());
    }

    HistoryLearningLevel historyLearningLevel = getHistoryLearningLevel(
        sharedPreferences, PreferenceUtil.PREF_DICTIONARY_PERSONALIZATION_KEY);
    if (historyLearningLevel != null) {
      builder = maybeCreateConfigBuilder(builder)
          .setHistoryLearningLevel(historyLearningLevel);
    }

    Boolean incognitoMode = getBoolean(
        sharedPreferences, PreferenceUtil.PREF_OTHER_INCOGNITO_MODE_KEY);
    if (incognitoMode != null) {
      builder = maybeCreateConfigBuilder(builder)
          .setIncognitoMode(incognitoMode.booleanValue());
    }

    GeneralConfig generalConfig = toGeneralConfig(sharedPreferences);
    if (generalConfig != GeneralConfig.getDefaultInstance()) {
      builder = maybeCreateConfigBuilder(builder)
          .setGeneralConfig(toGeneralConfig(sharedPreferences));
    }

    if (builder != null) {
      return builder.build();
    }

    // No corresponding preference is found, so return the default instance.
    return Config.getDefaultInstance();
  }

  /**
   * Converts {@code SharedPreferences} to {@code GeneralConfig}.
   *
   * Now this method only supports:
   * - upload_usage_stats.
   */
  static GeneralConfig toGeneralConfig(SharedPreferences sharedPreferences) {
    GeneralConfig.Builder builder = null;

    Boolean uploadUsageStats = getBoolean(
        sharedPreferences, PreferenceUtil.PREF_OTHER_USAGE_STATS_KEY);
    if (uploadUsageStats != null) {
      builder = maybeCreateGeneralConfigBuilder(builder)
          .setUploadUsageStats(uploadUsageStats.booleanValue());
    }

    if (builder != null) {
      return builder.build();
    }

    // No corresponding preference is found, so return the default instance.
    return GeneralConfig.getDefaultInstance();
  }

  /**
   * @return {@code Boolean} value if {@code sharedPreferences} contains the value
   *   corresponding to the {@code key}. Otherwise returns {@code null}.
   */
  private static Boolean getBoolean(SharedPreferences sharedPreferences, String key) {
    if (!sharedPreferences.contains(key)) {
      return null;
    }

    // Default value wouldn't be used in actual case, but it is required.
    return Boolean.valueOf(sharedPreferences.getBoolean(key, false));
  }

  /**
   * Returns {@code FundamentalCharacterForm} value gotten from {@code sharedPreferences}.
   *
   * Note that FundamentalCharacterForm has three states (INPUT_MODE, FULL_WIDTH and HALF_WIDTH)
   * but current UI has only two states (INPUT_MODE and HALF_WIDTH).
   * This is because FULL_WIDTH doesn't work well for our alphabetical mode.
   * Even if FULL_WIDTH is set to the config,
   * <ul>
   * <li>On Precomposition mode if T13N is half-ascii a half-space is inserted regardless of
   * {@link Config#getSpaceCharacterForm()}.
   * <li>On Composition mode space-key event is mapped to Convert operation.
   * And on MechaMozc Convert operation on half-ascii mode behaves as
   * "Commit and insert half-space".
   * </ul>
   * Thus the behavior of FULL_WIDTH equals to INPUT_MODE.
   * Surely we can change above behavior but nobody won't want to input full-space on alphabet mode
   * so keeping the current implementation is reasonable.
   *
   * @return {@code FundamentalCharacterForm} value if {@code sharedPreferences} contains the value
   *   corresponding to the {@code key} (HALF_WIDTH for true, INPUT_MODE for false).
   *   Otherwise returns {@code null}.
   * @throws IllegalArgumentException if {@code FundamentalCharacterForm} doesn't have
   */
  private static FundamentalCharacterForm getFundamentalCharacterFormFromBooleanKey(
      SharedPreferences sharedPreferences, String key) {
    if (!sharedPreferences.contains(key)) {
      return null;
    }
    return sharedPreferences.getBoolean(PreferenceUtil.PREF_SPACE_CHARACTER_FORM_KEY, false)
        ? FundamentalCharacterForm.FUNDAMENTAL_HALF_WIDTH
        : FundamentalCharacterForm.FUNDAMENTAL_INPUT_MODE;
  }

  /**
   * @return {@code HistoryLearningLevel} value if {@code sharedPreferences} contains the value
   *   corresponding to the {@code key}. Otherwise returns {@code null}.
   * @throws IllegalArgumentException if {@code HistoryLearningLevel} doesn't have
   */
  private static HistoryLearningLevel getHistoryLearningLevel(
      SharedPreferences sharedPreferences, String key) {
    if (!sharedPreferences.contains(key)) {
      return null;
    }

    // Default value wouldn't be used in actual case, but it is required.
    return HistoryLearningLevel.valueOf(
        sharedPreferences.getString(key, HistoryLearningLevel.DEFAULT_HISTORY.name()));
  }

  private static Config.Builder maybeCreateConfigBuilder(Config.Builder builder) {
    if (builder != null) {
      return builder;
    }
    return Config.newBuilder();
  }

  private static GeneralConfig.Builder maybeCreateGeneralConfigBuilder(
      GeneralConfig.Builder builder) {
    if (builder != null) {
      return builder;
    }
    return GeneralConfig.newBuilder();
  }
}
