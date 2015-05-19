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

import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.session.SessionExecutor;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;

import android.content.Context;
import android.content.DialogInterface;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.AttributeSet;
import android.util.Xml;

/**
 */
public class ClearConversionHistoryDialogPreferenceTest extends InstrumentationTestCaseWithMock {
  private ClearConversionHistoryDialogPreference preference;
  private SessionExecutor mockSessionExecutor;
  private SessionExecutor originalSessionExecutor;

  @Override
  protected void setUp() throws Exception {
    super.setUp();

    Context context = getInstrumentation().getTargetContext();
    AttributeSet attrs = Xml.asAttributeSet(context.getResources().getXml(R.xml.pref_dictionary));
    preference = new ClearConversionHistoryDialogPreference(context, attrs);

    mockSessionExecutor = createMock(SessionExecutor.class);
    originalSessionExecutor = SessionExecutor.setInstanceForTest(mockSessionExecutor);
  }

  @Override
  protected void tearDown() throws Exception {
    SessionExecutor.setInstanceForTest(originalSessionExecutor);

    originalSessionExecutor = null;
    mockSessionExecutor = null;
    preference = null;
    super.tearDown();
  }

  @SmallTest
  public void testOnClick() {
    doTestOnClick(true, DialogInterface.BUTTON_POSITIVE);
    doTestOnClick(false, DialogInterface.BUTTON_NEUTRAL);
    doTestOnClick(false, DialogInterface.BUTTON_NEGATIVE);
  }

  private void doTestOnClick(boolean expectOnClickToFire, int button) {
    resetAll();
    if (expectOnClickToFire) {
      mockSessionExecutor.clearUserHistory();
      mockSessionExecutor.clearUserPrediction();
    }
    replayAll();

    preference.onClick(null, button);
    verifyAll();
  }
}
