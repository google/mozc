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

import static org.easymock.EasyMock.capture;
import static org.easymock.EasyMock.eq;
import static org.easymock.EasyMock.expect;

import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.Parameter;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.net.Uri;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.webkit.WebView;

import org.easymock.Capture;

import java.util.Collections;
import java.util.List;

/**
 * Test for {@link MiniBrowserActivity}.
 */
public class MiniBrowserActivityTest extends InstrumentationTestCaseWithMock {

  @SmallTest
  @UiThreadTest
  public void testShouldOverrideUrlLoading() {
    class TestData extends Parameter {
      /**
       * Target URL
       */
      final String url;
      final String restrictionPattern;
      /**
       * True if external browser should be invoked (or do nothing in the MiniBrowser)
       */
      final boolean expectedDelegate;
      TestData(String url, String restrictionPattern,
          boolean expectedDelegate) {
        this.url = url;
        this.restrictionPattern = restrictionPattern;
        this.expectedDelegate = expectedDelegate;
      }
    }

    Resources targetResources = getInstrumentation().getTargetContext().getResources();
    TestData[] testDataList = {
        new TestData("validurl", "^validurl$", false),
        new TestData("invalidurl", "^validurl$", true),
        new TestData(targetResources.getString(R.string.pref_about_terms_of_service_url),
                     targetResources.getString(R.string.pref_url_restriction_regex),
                     false),
        new TestData(targetResources.getString(R.string.pref_about_privacy_policy_url),
                     targetResources.getString(R.string.pref_url_restriction_regex),
                     false),
        new TestData(targetResources.getString(R.string.pref_oss_credits_url),
                     targetResources.getString(R.string.pref_url_restriction_regex),
                     false),
    };

    for (TestData testData : testDataList) {
      for (boolean isDefaultBrowserExistent : new boolean[] {true, false}) {
        MozcLog.i(testData.toString());
        resetAll();
        PackageManager packageManager = createNiceMock(PackageManager.class);
        Context context = createNiceMock(Context.class);
        Capture<Intent> queryIntentCapture = new Capture<Intent>();
        Capture<Intent> activityIntentCapture = new Capture<Intent>();
        if (testData.expectedDelegate) {
          List<ResolveInfo> resolveInfo =
              isDefaultBrowserExistent
                  ? Collections.singletonList(new ResolveInfo())
                  : Collections.<ResolveInfo>emptyList();
          expect(packageManager.queryIntentActivities(capture(queryIntentCapture), eq(0)))
              .andReturn(resolveInfo);
          if (isDefaultBrowserExistent) {
            context.startActivity(capture(activityIntentCapture));
          }
        }
        replayAll();
        MiniBrowserActivity.MiniBrowserClient client =
            new MiniBrowserActivity.MiniBrowserClient(
                testData.restrictionPattern, packageManager, context);
        assertEquals(testData.toString(),
                     testData.expectedDelegate,
                     client.shouldOverrideUrlLoading(
                         new WebView(getInstrumentation().getTargetContext()), testData.url));
        if (testData.expectedDelegate) {
          Uri urlUri = Uri.parse(testData.url);
          assertEquals(urlUri, queryIntentCapture.getValue().getData());
          if (isDefaultBrowserExistent) {
            assertEquals(urlUri, activityIntentCapture.getValue().getData());
          }
        }
        verifyAll();
      }
    }
  }
}
