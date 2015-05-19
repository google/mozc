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

package org.mozc.android.inputmethod.japanese.emoji;

import static org.easymock.EasyMock.expect;

import org.mozc.android.inputmethod.japanese.MozcUtil.TelephonyManagerInterface;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import org.mozc.android.inputmethod.japanese.testing.MozcPreferenceUtil;
import org.mozc.android.inputmethod.japanese.testing.Parameter;

import android.content.Context;
import android.content.SharedPreferences;
import android.test.suitebuilder.annotation.SmallTest;

/**
 */
public class EmojiProviderTypeTest extends InstrumentationTestCaseWithMock {

  @SmallTest
  public void testMaybeDetectEmojiProviderType_null() {
    // Just make sure that passing null-SharedPreferences does nothing.
    EmojiProviderType.maybeSetDetectedEmojiProviderType(null, null);
  }

  @SmallTest
  public void testMaybeDetectEmojiProviderType_valid() {
    Context context = getInstrumentation().getContext();
    SharedPreferences sharedPreferences = MozcPreferenceUtil.getSharedPreferences(
        context, "DETECT_EMOJI_PROVIDER");

    for (EmojiProviderType providerType : EmojiProviderType.values()) {
      MozcPreferenceUtil.updateSharedPreference(
          sharedPreferences, EmojiProviderType.EMOJI_PROVIDER_TYPE_PREFERENCE_KEY,
          providerType.name());

      EmojiProviderType.maybeSetDetectedEmojiProviderType(sharedPreferences, null);

      // Make sure nothing has been changed.
      assertEquals(
          providerType.name(),
          sharedPreferences.getString(EmojiProviderType.EMOJI_PROVIDER_TYPE_PREFERENCE_KEY, null));
    }
  }

  @SmallTest
  public void testMaybeDetectEmojiProviderType_invalid() {
    Context context = getInstrumentation().getContext();
    SharedPreferences sharedPreferences = MozcPreferenceUtil.getSharedPreferences(
        context, "DETECT_EMOJI_PROVIDER");
    MozcPreferenceUtil.updateSharedPreference(
        sharedPreferences, EmojiProviderType.EMOJI_PROVIDER_TYPE_PREFERENCE_KEY,
        "INVALID PROVIDER NAME");
    TelephonyManagerInterface telephonyManager = createNiceMock(TelephonyManagerInterface.class);
    // DOCOMO NMP
    expect(telephonyManager.getNetworkOperator()).andStubReturn("44010");
    replayAll();

    EmojiProviderType.maybeSetDetectedEmojiProviderType(sharedPreferences, telephonyManager);

    verifyAll();
    assertEquals(
        "DOCOMO",
        sharedPreferences.getString(EmojiProviderType.EMOJI_PROVIDER_TYPE_PREFERENCE_KEY, null));
  }

  @SmallTest
  public void testMaybeDetectEmojiProviderType_expectedMobileNetworkOperator() {
    class TestData extends Parameter {
      final String networkOperator;
      final String expectedEmojiProvider;

      TestData(String networkOperator, String expectedEmojiProvider) {
        this.networkOperator = networkOperator;
        this.expectedEmojiProvider = expectedEmojiProvider;
      }
    }

    Context context = getInstrumentation().getContext();
    SharedPreferences sharedPreferences = MozcPreferenceUtil.getSharedPreferences(
        context, "DETECT_EMOJI_PROVIDER");
    TelephonyManagerInterface telephonyManager = createNiceMock(TelephonyManagerInterface.class);

    TestData[] testDataList = {
        new TestData("44010", "DOCOMO"),
        new TestData("44020", "SOFTBANK"),
        new TestData("44070", "KDDI"),
    };
    for (TestData testData : testDataList) {
      sharedPreferences.edit().clear().commit();
      resetAll();
      expect(telephonyManager.getNetworkOperator()).andStubReturn(testData.networkOperator);
      replayAll();

      EmojiProviderType.maybeSetDetectedEmojiProviderType(sharedPreferences, telephonyManager);

      verifyAll();
      assertEquals(
          testData.toString(),
          testData.expectedEmojiProvider,
          sharedPreferences.getString(EmojiProviderType.EMOJI_PROVIDER_TYPE_PREFERENCE_KEY, null));
    }
  }

  @SmallTest
  public void testMaybeDetectEmojiProviderType_unexpectedMobileNetworkCode() {
    Context context = getInstrumentation().getContext();
    SharedPreferences sharedPreferences = MozcPreferenceUtil.getSharedPreferences(
        context, "DETECT_EMOJI_PROVIDER");
    sharedPreferences.edit().clear().commit();
    TelephonyManagerInterface telephonyManager = createNiceMock(TelephonyManagerInterface.class);
    expect(telephonyManager.getNetworkOperator()).andStubReturn("INVALID NETWORK OPERATOR");
    replayAll();

    EmojiProviderType.maybeSetDetectedEmojiProviderType(sharedPreferences, telephonyManager);

    verifyAll();
    assertNull(
        sharedPreferences.getString(EmojiProviderType.EMOJI_PROVIDER_TYPE_PREFERENCE_KEY, null));
  }
}
