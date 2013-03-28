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
import org.mozc.android.inputmethod.japanese.testing.MozcPreferenceUtil;
import org.mozc.android.inputmethod.japanese.testing.Parameter;

import android.content.SharedPreferences;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

/**
 */
public class ConfigUtilTest extends InstrumentationTestCase {
  @SmallTest
  public void testToConfig_fundamentalCharacterFormFromBooleanKey() {
    class TestData extends Parameter {
      public final boolean halfSpace;
      public final Config expectedConfig;
      public TestData(boolean halfSpace, FundamentalCharacterForm expectedForm) {
        this.halfSpace = halfSpace;
        this.expectedConfig = Config.newBuilder().setSpaceCharacterForm(expectedForm).build();
      }
    }
    TestData[] testDataList = {
        new TestData(true, FundamentalCharacterForm.FUNDAMENTAL_HALF_WIDTH),
        new TestData(false, FundamentalCharacterForm.FUNDAMENTAL_INPUT_MODE),
    };

    SharedPreferences sharedPreferences = MozcPreferenceUtil.getSharedPreferences(
        getInstrumentation().getContext(), "TEST_FOR_FUNDAMENTAL_CHAR_FORM");

    for (TestData testData : testDataList) {
      sharedPreferences.edit()
          .clear()
          .putBoolean(PreferenceUtil.PREF_SPACE_CHARACTER_FORM_KEY, testData.halfSpace)
          .commit();

      assertEquals(testData.toString(),
                   testData.expectedConfig, ConfigUtil.toConfig(sharedPreferences));
    }
  }

  @SmallTest
  public void testToConfig_kanaModifierInsensitiveConversion() {
    SharedPreferences sharedPreferences = MozcPreferenceUtil.getSharedPreferences(
        getInstrumentation().getContext(), "TEST_FOR_KANA_MODIFIER_INSENSITIVE_CONVERSION");

    for (boolean value : new boolean[] {true, false}) {
      sharedPreferences.edit()
          .clear()
          .putBoolean(PreferenceUtil.PREF_KANA_MODIFIER_INSENSITIVE_CONVERSION_KEY, value)
          .commit();

      assertEquals(Config.newBuilder()
                       .setUseKanaModifierInsensitiveConversion(value)
                       .build(),
                   ConfigUtil.toConfig(sharedPreferences));
    }
  }

  @SmallTest
  public void testToConfig_typingCorrection() {
    SharedPreferences sharedPreferences = MozcPreferenceUtil.getSharedPreferences(
        getInstrumentation().getContext(), "TEST_FOR_TYPING_CORRECTION");

    for (boolean value : new boolean[] {true, false}) {
      sharedPreferences.edit()
          .clear()
          .putBoolean(PreferenceUtil.PREF_TYPING_CORRECTION_KEY, value)
          .commit();

      assertEquals(Config.newBuilder()
                       .setUseTypingCorrection(value)
                       .build(),
                   ConfigUtil.toConfig(sharedPreferences));
    }
  }


  @SmallTest
  public void testToConfig_historyLearningLevel() {
    SharedPreferences sharedPreferences = MozcPreferenceUtil.getSharedPreferences(
        getInstrumentation().getContext(), "TEST_FOR_HISTORY_LEARNING_LEVEL");

    for (HistoryLearningLevel historyLearningLevel : HistoryLearningLevel.values()) {
      sharedPreferences.edit()
          .clear()
          .putString(PreferenceUtil.PREF_DICTIONARY_PERSONALIZATION_KEY,
                     historyLearningLevel.name())
          .commit();

      assertEquals(Config.newBuilder()
                       .setHistoryLearningLevel(historyLearningLevel)
                       .build(),
                   ConfigUtil.toConfig(sharedPreferences));
    }
  }

  @SmallTest
  public void testToConfig_incognitoMode() {
    SharedPreferences sharedPreferences = MozcPreferenceUtil.getSharedPreferences(
        getInstrumentation().getContext(), "TEST_FOR_INCOGNITO_MODE");

    for (boolean incognitoMode : new boolean[] { true, false }) {
      sharedPreferences.edit()
          .clear()
          .putBoolean(PreferenceUtil.PREF_OTHER_INCOGNITO_MODE_KEY, incognitoMode)
          .commit();

      assertEquals(Config.newBuilder()
                       .setIncognitoMode(incognitoMode)
                       .build(),
                   ConfigUtil.toConfig(sharedPreferences));
    }
  }

  @SmallTest
  public void testToConfig_generalConfig() {
    SharedPreferences sharedPreferences = MozcPreferenceUtil.getSharedPreferences(
        getInstrumentation().getContext(), "TEST_FOR_GENERAL_CONFIG");

    for (boolean uploadUsageStats : new boolean[] { true, false }) {
      sharedPreferences.edit()
          .clear()
          .putBoolean(PreferenceUtil.PREF_OTHER_USAGE_STATS_KEY, uploadUsageStats)
          .commit();

      assertEquals(Config.newBuilder()
                       .setGeneralConfig(GeneralConfig.newBuilder()
                            .setUploadUsageStats(uploadUsageStats))
                       .build(),
                   ConfigUtil.toConfig(sharedPreferences));
    }
  }
}
