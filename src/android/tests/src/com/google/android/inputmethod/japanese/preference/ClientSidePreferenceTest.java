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

import org.mozc.android.inputmethod.japanese.ViewManagerInterface.LayoutAdjustment;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.InputStyle;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.KeyboardLayout;
import org.mozc.android.inputmethod.japanese.testing.MozcPreferenceUtil;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import org.mozc.android.inputmethod.japanese.view.SkinType;

import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

/**
 */
public class ClientSidePreferenceTest extends InstrumentationTestCase {

  // Use different values for portrait and landscape.
  private static final KeyboardLayout PORTRAIT_KEYBOARD_LAYOUT = KeyboardLayout.TWELVE_KEYS;
  private static final KeyboardLayout LANDSCAPE_KEYBOARD_LAYOUT = KeyboardLayout.QWERTY;
  private static final InputStyle PORTRAIT_INPUT_STYLE = InputStyle.FLICK;
  private static final InputStyle LANDSCAPE_INPUT_STYLE = InputStyle.TOGGLE_FLICK;
  private static final boolean PORTRAIT_QWERTY_LAYOUT_FOR_ALPHABET = true;
  private static final boolean LANDSCAPE_QWERTY_LAYOUT_FOR_ALPHABET = false;
  private static final int PORTRAIT_FLICK_SENSITIVITY = 10;
  private static final int LANDSCAPE_FLICK_SENSITIVITY = -10;
  private static final boolean PORTRAIT_FULLSCREEN_MODE = true;
  private static final boolean LANDSCAPE_FULLSCREEN_MODE = false;
  private static final LayoutAdjustment PORTRAIT_LAYOUT_ADJUSTMENT = LayoutAdjustment.RIGHT;
  private static final LayoutAdjustment LANDSCAPE_LAYOUT_ADJUSTMENT = LayoutAdjustment.LEFT;

  @SmallTest
  public void testClientSidePreferenceSharedPreferencesConfiguration() {
    SharedPreferences sharedPreferences = MozcPreferenceUtil.getSharedPreferences(
        getInstrumentation().getContext(), "CLIENT_SIDE_PREF");
    MozcPreferenceUtil.updateSharedPreference(
        sharedPreferences, PreferenceUtil.PREF_PORTRAIT_KEYBOARD_LAYOUT_KEY,
        PORTRAIT_KEYBOARD_LAYOUT.name());
    MozcPreferenceUtil.updateSharedPreference(
        sharedPreferences, PreferenceUtil.PREF_PORTRAIT_INPUT_STYLE_KEY,
        PORTRAIT_INPUT_STYLE.name());
    MozcPreferenceUtil.updateSharedPreference(
        sharedPreferences, PreferenceUtil.PREF_PORTRAIT_QWERTY_LAYOUT_FOR_ALPHABET_KEY,
        PORTRAIT_QWERTY_LAYOUT_FOR_ALPHABET);
    MozcPreferenceUtil.updateSharedPreference(
        sharedPreferences, PreferenceUtil.PREF_PORTRAIT_FLICK_SENSITIVITY_KEY,
        PORTRAIT_FLICK_SENSITIVITY);

    MozcPreferenceUtil.updateSharedPreference(
        sharedPreferences, PreferenceUtil.PREF_LANDSCAPE_KEYBOARD_LAYOUT_KEY,
        LANDSCAPE_KEYBOARD_LAYOUT.name());
    MozcPreferenceUtil.updateSharedPreference(
        sharedPreferences, PreferenceUtil.PREF_LANDSCAPE_INPUT_STYLE_KEY,
        LANDSCAPE_INPUT_STYLE.name());
    MozcPreferenceUtil.updateSharedPreference(
        sharedPreferences, PreferenceUtil.PREF_LANDSCAPE_QWERTY_LAYOUT_FOR_ALPHABET_KEY,
        LANDSCAPE_QWERTY_LAYOUT_FOR_ALPHABET);
    MozcPreferenceUtil.updateSharedPreference(
        sharedPreferences, PreferenceUtil.PREF_LANDSCAPE_FLICK_SENSITIVITY_KEY,
        LANDSCAPE_FLICK_SENSITIVITY);

    MozcPreferenceUtil.updateSharedPreference(
        sharedPreferences, PreferenceUtil.PREF_PORTRAIT_FULLSCREEN_KEY,
        PORTRAIT_FULLSCREEN_MODE);
    MozcPreferenceUtil.updateSharedPreference(
        sharedPreferences, PreferenceUtil.PREF_LANDSCAPE_FULLSCREEN_KEY,
        LANDSCAPE_FULLSCREEN_MODE);

    MozcPreferenceUtil.updateSharedPreference(
        sharedPreferences, PreferenceUtil.PREF_PORTRAIT_LAYOUT_ADJUSTMENT_KEY,
        PORTRAIT_LAYOUT_ADJUSTMENT.name());
    MozcPreferenceUtil.updateSharedPreference(
        sharedPreferences, PreferenceUtil.PREF_LANDSCAPE_LAYOUT_ADJUSTMENT_KEY,
        LANDSCAPE_LAYOUT_ADJUSTMENT.name());

    MozcPreferenceUtil.updateSharedPreference(
        sharedPreferences, PreferenceUtil.PREF_SKIN_TYPE, SkinType.BLUE_LIGHTGRAY.name());

    class TestData extends Parameter {
      final int orientation;
      final boolean usePortraitKeyboardSettingsForLandscape;

      final KeyboardLayout expectedKeyboardLayout;
      final InputStyle expectedInputStyle;
      final boolean expectedQwertyLayoutForAlphabet;
      final int expectedFlickSensitivity;
      final boolean expectedFullscreenMode;
      final LayoutAdjustment expectedLayoutAdjustment;
      final SkinType expectedSkinType;

      TestData(int orientation,
               boolean usePortraitKeyboardSettingsForLandscape,
               KeyboardLayout expectedKeyboardLayout,
               InputStyle expectedInputStyle,
               boolean expectedQwertyLayoutForAlphabet,
               int expectedFlickSensitivity,
               LayoutAdjustment expectedLayoutAdjustment,
               boolean expectedFullscreenMode,
               SkinType expectedSkinType) {
        this.orientation = orientation;
        this.usePortraitKeyboardSettingsForLandscape = usePortraitKeyboardSettingsForLandscape;

        this.expectedKeyboardLayout = expectedKeyboardLayout;
        this.expectedInputStyle = expectedInputStyle;
        this.expectedQwertyLayoutForAlphabet = expectedQwertyLayoutForAlphabet;
        this.expectedFlickSensitivity = expectedFlickSensitivity;
        this.expectedLayoutAdjustment = expectedLayoutAdjustment;
        this.expectedFullscreenMode = expectedFullscreenMode;
        this.expectedSkinType = expectedSkinType;
      }
    }

    TestData[] testDataList = {
        new TestData(Configuration.ORIENTATION_PORTRAIT,
                     false,
                     PORTRAIT_KEYBOARD_LAYOUT,
                     PORTRAIT_INPUT_STYLE,
                     PORTRAIT_QWERTY_LAYOUT_FOR_ALPHABET,
                     PORTRAIT_FLICK_SENSITIVITY,
                     PORTRAIT_LAYOUT_ADJUSTMENT,
                     PORTRAIT_FULLSCREEN_MODE,
                     SkinType.BLUE_LIGHTGRAY
                     ),
        new TestData(Configuration.ORIENTATION_LANDSCAPE,
                     false,
                     LANDSCAPE_KEYBOARD_LAYOUT,
                     LANDSCAPE_INPUT_STYLE,
                     LANDSCAPE_QWERTY_LAYOUT_FOR_ALPHABET,
                     LANDSCAPE_FLICK_SENSITIVITY,
                     LANDSCAPE_LAYOUT_ADJUSTMENT,
                     LANDSCAPE_FULLSCREEN_MODE,
                     SkinType.BLUE_LIGHTGRAY),
        new TestData(Configuration.ORIENTATION_PORTRAIT,
                     true,
                     PORTRAIT_KEYBOARD_LAYOUT,
                     PORTRAIT_INPUT_STYLE,
                     PORTRAIT_QWERTY_LAYOUT_FOR_ALPHABET,
                     PORTRAIT_FLICK_SENSITIVITY,
                     PORTRAIT_LAYOUT_ADJUSTMENT,
                     PORTRAIT_FULLSCREEN_MODE,
                     SkinType.BLUE_LIGHTGRAY),
        new TestData(Configuration.ORIENTATION_LANDSCAPE,
                     true,
                     PORTRAIT_KEYBOARD_LAYOUT,
                     PORTRAIT_INPUT_STYLE,
                     PORTRAIT_QWERTY_LAYOUT_FOR_ALPHABET,
                     PORTRAIT_FLICK_SENSITIVITY,
                     PORTRAIT_LAYOUT_ADJUSTMENT,
                     LANDSCAPE_FULLSCREEN_MODE,
                     SkinType.BLUE_LIGHTGRAY),
    };

    for (TestData testData : testDataList) {
      MozcPreferenceUtil.updateSharedPreference(
          sharedPreferences,
          PreferenceUtil.PREF_USE_PORTRAIT_KEYBOARD_SETTINGS_FOR_LANDSCAPE_KEY,
          testData.usePortraitKeyboardSettingsForLandscape);

      Configuration configuration = new Configuration();
      configuration.orientation = testData.orientation;

      ClientSidePreference preference =
          new ClientSidePreference(sharedPreferences, configuration);
      assertEquals(testData.toString(),
                   testData.expectedKeyboardLayout, preference.getKeyboardLayout());
      assertEquals(testData.toString(),
                   testData.expectedInputStyle, preference.getInputStyle());
      assertEquals(testData.toString(),
                   testData.expectedQwertyLayoutForAlphabet,
                   preference.isQwertyLayoutForAlphabet());
      assertEquals(testData.toString(),
                   testData.expectedFlickSensitivity, preference.getFlickSensitivity());
      assertEquals(testData.toString(),
                   testData.expectedLayoutAdjustment, preference.getLayoutAdjustment());
      assertEquals(testData.toString(),
                   testData.expectedFullscreenMode, preference.isFullscreenMode());
      assertEquals(testData.toString(),
                   testData.expectedSkinType, preference.getSkinType());
    }
  }

  static enum TestGetEnum {
    BETA,
    GAMMA,
    DELTA,
  }

  @SmallTest
  public void testGetEnum() {
    SharedPreferences sharedPreferences = MozcPreferenceUtil.getSharedPreferences(
        getInstrumentation().getContext(), "CLIENT_SIDE_PREF");
    SharedPreferences.Editor editor = sharedPreferences.edit();
    String key1 = "key1";
    editor.putString(key1, "ALPHA");
    String key2 = "key2";
    editor.putString(key2, "DELTA");
    editor.commit();
    assertEquals(TestGetEnum.BETA,
                 PreferenceUtil.getEnum(sharedPreferences, key1, TestGetEnum.class,
                                        TestGetEnum.GAMMA, TestGetEnum.BETA));
    assertEquals(TestGetEnum.GAMMA,
                 PreferenceUtil.getEnum(sharedPreferences, key1, TestGetEnum.class,
                                        TestGetEnum.GAMMA));
    assertEquals(TestGetEnum.DELTA,
                 PreferenceUtil.getEnum(sharedPreferences, key2, TestGetEnum.class,
                                        TestGetEnum.GAMMA));
  }
}
