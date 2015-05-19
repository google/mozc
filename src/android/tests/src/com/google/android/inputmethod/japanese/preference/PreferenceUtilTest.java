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

import static org.easymock.EasyMock.expect;
import static org.easymock.EasyMock.isA;

import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.KeyboardLayout;
import org.mozc.android.inputmethod.japanese.preference.PreferenceUtil.CurrentKeyboardLayoutPreferenceChangeListener;
import org.mozc.android.inputmethod.japanese.preference.PreferenceUtil.PreferenceManagerInterface;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import com.google.common.base.Optional;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.PreferenceGroup;
import android.test.mock.MockResources;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.Xml;

/**
 */
public class PreferenceUtilTest extends InstrumentationTestCaseWithMock {

  @Override
  protected void tearDown() throws Exception {
    MozcUtil.setDevChannel(Optional.<Boolean>absent());
    super.tearDown();
  }

  @SmallTest
  public void testInitializeSpecialPreference() {
    Context context = getInstrumentation().getTargetContext();
    Preference preference = createMockBuilder(Preference.class)
        .withConstructor(Context.class)
        .withArgs(context)
        .addMockedMethods("setEnabled", "getKey", "setOnPreferenceClickListener")
        .addMockedMethod("setSummary", CharSequence.class)
        .createMock();
    PreferenceManagerInterface preferenceManager = createMock(PreferenceManagerInterface.class);

    // PREF_OTHER_USAGE_STATS_KEY, devChannel==true
    MozcUtil.setDevChannel(Optional.of(true));

    expect(preferenceManager.findPreference(PreferenceUtil.PREF_OTHER_USAGE_STATS_KEY))
        .andStubReturn(preference);
    expect(preferenceManager.findPreference(isA(String.class))).andStubReturn(null);
    preference.setSummary(isA(String.class));
    preference.setEnabled(false);
    replayAll();

    PreferenceUtil.initializeSpecialPreferencesInternal(preferenceManager);

    verifyAll();

    // PREF_OTHER_USAGE_STATS_KEY, devChannel==false
    resetAll();
    MozcUtil.setDevChannel(Optional.of(false));

    expect(preferenceManager.findPreference(PreferenceUtil.PREF_OTHER_USAGE_STATS_KEY))
        .andStubReturn(preference);
    expect(preferenceManager.findPreference(isA(String.class))).andStubReturn(null);
    preference.setSummary(isA(String.class));
    preference.setEnabled(true);
    replayAll();

    PreferenceUtil.initializeSpecialPreferencesInternal(preferenceManager);

    verifyAll();

    // PREF_ABOUT_VERSION
    resetAll();
    expect(preferenceManager.findPreference(PreferenceUtil.PREF_ABOUT_VERSION))
        .andStubReturn(preference);
    expect(preferenceManager.findPreference(isA(String.class))).andStubReturn(null);
    preference.setSummary(isA(String.class));
    replayAll();

    PreferenceUtil.initializeSpecialPreferencesInternal(preferenceManager);

    verifyAll();

    // initializeLayoutAdjustmentPreference
    {
      resetAll();
      Preference prefPortrait = createMockBuilder(Preference.class)
          .withConstructor(Context.class)
          .withArgs(context)
          .createMock();
      Preference prefLandscape = createMockBuilder(Preference.class)
          .withConstructor(Context.class)
          .withArgs(context)
          .createMock();
      PreferenceGroup prefPortraitGroup = createMockBuilder(PreferenceGroup.class)
          .withConstructor(Context.class, AttributeSet.class)
          .withArgs(context, Xml.asAttributeSet(context.getResources().getXml(
              R.xml.pref_software_keyboard_advanced)))
          .createMock();
      PreferenceGroup prefLandscapeGroup = createMockBuilder(PreferenceGroup.class)
          .withConstructor(Context.class, AttributeSet.class)
          .withArgs(context, Xml.asAttributeSet(context.getResources().getXml(
              R.xml.pref_software_keyboard_advanced)))
          .createMock();

      expect(preferenceManager.findPreference(PreferenceUtil.PREF_PORTRAIT_LAYOUT_ADJUSTMENT_KEY))
          .andStubReturn(prefPortrait);
      expect(preferenceManager.findPreference(PreferenceUtil.PREF_LANDSCAPE_LAYOUT_ADJUSTMENT_KEY))
          .andStubReturn(prefLandscape);
      expect(preferenceManager.findPreference(PreferenceUtil
          .PREF_SOFTWARE_KEYBOARD_ADVANED_PORTRAIT_KEY))
          .andStubReturn(prefPortraitGroup);
      expect(preferenceManager.findPreference(PreferenceUtil
          .PREF_SOFTWARE_KEYBOARD_ADVANED_LANDSCAPE_KEY))
          .andStubReturn(prefLandscapeGroup);
      expect(preferenceManager.findPreference(isA(String.class))).andStubReturn(null);

      Context contextMock = createMock(Context.class);
      Resources resources = createMock(MockResources.class);
      expect(prefPortraitGroup.getContext()).andStubReturn(contextMock);
      expect(contextMock.getResources()).andStubReturn(resources);
      expect(resources.getDimensionPixelSize(R.dimen.ime_window_partial_width))
          .andStubReturn(100);
      expect(resources.getDimensionPixelSize(R.dimen.side_frame_width)).andStubReturn(50);
      DisplayMetrics displayMetrics = new DisplayMetrics();
      displayMetrics.widthPixels = 100;
      displayMetrics.heightPixels = 200;
      expect(resources.getDisplayMetrics()).andStubReturn(displayMetrics);

      expect(prefPortraitGroup.removePreference(prefPortrait)).andReturn(true);
      expect(prefLandscapeGroup.removePreference(prefLandscape)).andReturn(true);

      replayAll();
      PreferenceUtil.initializeLayoutAdjustmentPreference(preferenceManager);

      verifyAll();
    }
  }

  @SmallTest
  public void testCurrentKeyboardLayoutPreferenceChangeListener() {
    OnPreferenceChangeListener listener = new CurrentKeyboardLayoutPreferenceChangeListener();

    // TODO(hidehiko): Replace following test in a less side-effective way.
    final SharedPreferences sharedPreferences = createMock(SharedPreferences.class);
    Editor editor = createMock(Editor.class);
    Context context = getInstrumentation().getTargetContext();
    Preference preference = new Preference(context) {
      @Override
      public SharedPreferences getSharedPreferences() {
        return sharedPreferences;
      }
    };

    int originalOrientation = preference.getContext().getResources().getConfiguration().orientation;
    try {
      class TestData extends Parameter {
        final int orientation;
        final boolean usePortraitKeyboardSettingForLandscapeKey;
        final String expectKeyboardLayoutKey;
        TestData(int orientation,
                 boolean usePortraitKeyboardSettingForLandscapeKey,
                 String expectKeyboardLayoutKey) {
          this.orientation = orientation;
          this.usePortraitKeyboardSettingForLandscapeKey =
              usePortraitKeyboardSettingForLandscapeKey;
          this.expectKeyboardLayoutKey = expectKeyboardLayoutKey;
        }
      }

      TestData[] testDataList = new TestData[] {
          new TestData(Configuration.ORIENTATION_PORTRAIT,
                       false,
                       PreferenceUtil.PREF_PORTRAIT_KEYBOARD_LAYOUT_KEY),
          new TestData(Configuration.ORIENTATION_LANDSCAPE,
                       false,
                       PreferenceUtil.PREF_LANDSCAPE_KEYBOARD_LAYOUT_KEY),
          new TestData(Configuration.ORIENTATION_SQUARE,
                       false,
                       PreferenceUtil.PREF_PORTRAIT_KEYBOARD_LAYOUT_KEY),
          new TestData(Configuration.ORIENTATION_UNDEFINED,
                       false,
                       PreferenceUtil.PREF_PORTRAIT_KEYBOARD_LAYOUT_KEY),
          new TestData(Configuration.ORIENTATION_PORTRAIT,
                       true,
                       PreferenceUtil.PREF_PORTRAIT_KEYBOARD_LAYOUT_KEY),
          new TestData(Configuration.ORIENTATION_LANDSCAPE,
                       true,
                       PreferenceUtil.PREF_PORTRAIT_KEYBOARD_LAYOUT_KEY),
          new TestData(Configuration.ORIENTATION_SQUARE,
                       true,
                       PreferenceUtil.PREF_PORTRAIT_KEYBOARD_LAYOUT_KEY),
          new TestData(Configuration.ORIENTATION_UNDEFINED,
                       true,
                       PreferenceUtil.PREF_PORTRAIT_KEYBOARD_LAYOUT_KEY),
      };

      for (TestData testData : testDataList) {
        resetAll();
        preference.getContext().getResources().getConfiguration().orientation =
            testData.orientation;
        expect(sharedPreferences.edit()).andStubReturn(editor);
        expect(sharedPreferences.getBoolean(
            PreferenceUtil.PREF_USE_PORTRAIT_KEYBOARD_SETTINGS_FOR_LANDSCAPE_KEY, true))
            .andStubReturn(testData.usePortraitKeyboardSettingForLandscapeKey);
        expect(editor.putString(testData.expectKeyboardLayoutKey,
                                KeyboardLayout.TWELVE_KEYS.name()))
            .andReturn(editor);
        expect(editor.commit()).andReturn(true);
        replayAll();

        listener.onPreferenceChange(preference, KeyboardLayout.TWELVE_KEYS);

        verifyAll();
      }
    } finally {
      preference.getContext().getResources().getConfiguration().orientation = originalOrientation;
    }
  }
}
