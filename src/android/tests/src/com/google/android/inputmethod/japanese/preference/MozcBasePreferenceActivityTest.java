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

import org.mozc.android.inputmethod.japanese.ApplicationInitializerFactory;
import org.mozc.android.inputmethod.japanese.ApplicationInitializerFactory.ApplicationInitializationStatus;
import org.mozc.android.inputmethod.japanese.ApplicationInitializerFactory.ApplicationInitializer;
import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.MozcUtil.TelephonyManagerInterface;
import org.mozc.android.inputmethod.japanese.testing.ActivityInstrumentationTestCase2WithMock;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import com.google.common.base.Optional;

import android.app.AlertDialog;
import android.content.Context;
import android.preference.PreferenceManager;
import android.test.suitebuilder.annotation.SmallTest;

/**
 */
public class MozcBasePreferenceActivityTest
    extends ActivityInstrumentationTestCase2WithMock<MozcPreferenceDummy> {
  public MozcBasePreferenceActivityTest() {
    super(MozcPreferenceDummy.class);
  }

  @Override
  protected void tearDown() throws Exception {
    MozcUtil.setDebug(Optional.<Boolean>absent());
    MozcUtil.setDevChannel(Optional.<Boolean>absent());
    MozcUtil.setMozcEnabled(Optional.<Boolean>absent());
    MozcUtil.setMozcDefaultIme(Optional.<Boolean>absent());
    super.tearDown();
  }

  @SmallTest
  public void testMaybeShowAlertDialog() {
    MozcBasePreferenceActivity activity = getActivity();
    AlertDialog imeEnableDialog = createDialogMock(AlertDialog.class);
    AlertDialog imeSwitchDialog = createDialogMock(AlertDialog.class);

    activity.imeEnableDialog = imeEnableDialog;
    activity.imeSwitchDialog = imeSwitchDialog;

    class TestData extends Parameter {
      final boolean isMozcEnabled;
      final boolean isMozcDefaultIme;
      final boolean expectImeEnableDialogShown;
      final boolean expectImeSwitchDialogShown;
      TestData(boolean isMozcEnabled, boolean isMozcDefaultIme,
               boolean expectImeEnableDialogShown, boolean expectImeSwitchDialogShown) {
        this.isMozcEnabled = isMozcEnabled;
        this.isMozcDefaultIme = isMozcDefaultIme;
        this.expectImeEnableDialogShown = expectImeEnableDialogShown;
        this.expectImeSwitchDialogShown = expectImeSwitchDialogShown;
      }
    }

    TestData[] testData = {
        new TestData(false, false, true, false),
        new TestData(false, true, true, false),
        new TestData(true, false, false, true),
        new TestData(true, true, false, false),
    };

    ApplicationInitializationStatus initializationStatus = new ApplicationInitializationStatus() {
      @Deprecated
      @Override
      public boolean isLaunchedAtLeastOnce() {
        return false;
      }
      @Override
      public Optional<Integer> getLastLaunchAbiIndependentVersionCode() {
        return Optional.of(Integer.MAX_VALUE);
      }
      @Override
      public boolean isWelcomeActivityShownAtLeastOnce() {
        return true;
      }
    };

    for (TestData test : testData) {
      resetAll();
      expect(imeEnableDialog.isShowing()).andStubReturn(false);
      expect(imeSwitchDialog.isShowing()).andStubReturn(false);
      if (test.expectImeEnableDialogShown) {
        imeEnableDialog.show();
      }
      if (test.expectImeSwitchDialogShown) {
        imeSwitchDialog.show();
      }
      replayAll();

      MozcUtil.setMozcEnabled(Optional.of(test.isMozcEnabled));
      MozcUtil.setMozcDefaultIme(Optional.of(test.isMozcDefaultIme));

      Context context = getInstrumentation().getTargetContext();
      ApplicationInitializer initializer =
          ApplicationInitializerFactory.createInstance(
              initializationStatus,
              context,
              PreferenceManager.getDefaultSharedPreferences(context),
              new TelephonyManagerInterface() {
                @Override
                public String getNetworkOperator() {
                  return "INVALID NETWORK OPERATOR";
                }
              });

      activity.onPostResumeInternal(initializer);

      verifyAll();
    }
  }
}
