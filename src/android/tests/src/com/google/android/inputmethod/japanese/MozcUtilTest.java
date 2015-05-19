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

package org.mozc.android.inputmethod.japanese;

import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request.CrossingEdgeBehavior;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request.EmojiCarrierType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request.RewriterCapability;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request.SpaceOnAlphanumeric;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request.SpecialRomanjiTable;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import com.google.common.base.Optional;
import com.google.protobuf.ByteString;

import android.content.Context;
import android.content.res.Configuration;
import android.os.Bundle;
import android.test.InstrumentationTestCase;
import android.test.mock.MockContext;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.inputmethod.EditorInfo;

import java.lang.reflect.InvocationTargetException;
import java.util.Collections;
import java.util.EnumSet;
import java.util.Set;

/**
 */
public class MozcUtilTest extends InstrumentationTestCase {
  @SmallTest
  public void testIsDevChannel() throws InvocationTargetException {
    class TestData {
      final String versionName;
      final boolean isDevChannel;
      public TestData(String versionName, boolean isDevChanel) {
        this.versionName = versionName;
        this.isDevChannel = isDevChanel;
      }
    }
    TestData testDataList[] = {
      new TestData("", false),
      new TestData("1", false),
      new TestData(".", false),
      new TestData("1.2.3.000", false),
      new TestData("111111", false),
      new TestData("1.1.1.1", false),
      new TestData("1.1.1.99", false),
      new TestData("1.1.1.100", true),
      new TestData(".1", false),
      new TestData(".a", false),
    };
    for (TestData testData : testDataList) {
      assertEquals(testData.isDevChannel, MozcUtil.isDevChannelVersionName(testData.versionName));
    }
  }

  @SmallTest
  public void testGetRequest() {
    Configuration configuration = new Configuration();
    for (KeyboardSpecification specification : KeyboardSpecification.values()) {
      for (int orientation : new int[] {Configuration.ORIENTATION_PORTRAIT,
                                        Configuration.ORIENTATION_LANDSCAPE}) {
        Request request =
            MozcUtil.getRequestForKeyboard(specification.getKeyboardSpecificationName(),
                                           specification.getSpecialRomanjiTable(),
                                           specification.getSpaceOnAlphanumeric(),
                                           specification.isKanaModifierInsensitiveConversion(),
                                           specification.getCrossingEdgeBehavior(),
                                           configuration);
        assertEquals(specification.getKeyboardSpecificationName()
                         .formattedKeyboardName(configuration),
                     request.getKeyboardName());
        assertEquals(specification.getSpecialRomanjiTable(), request.getSpecialRomanjiTable());
        assertEquals(specification.getSpaceOnAlphanumeric(), request.getSpaceOnAlphanumeric());
      }
    }
  }

  @SmallTest
  public void testGetRequest_pseudoKeyboards() {
    Configuration configuration = new Configuration();
    {
      Request request =
          MozcUtil.getRequestForKeyboard(new KeyboardSpecificationName("baseName", 1, 2, 3),
                                         SpecialRomanjiTable.DEFAULT_TABLE,
                                         null,
                                         null,
                                         null,
                                         configuration);
      assertEquals(SpecialRomanjiTable.DEFAULT_TABLE, request.getSpecialRomanjiTable());
      assertFalse(request.hasSpaceOnAlphanumeric());
      assertFalse(request.hasKanaModifierInsensitiveConversion());
      assertFalse(request.hasCrossingEdgeBehavior());
    }
    {
      Request request =
          MozcUtil.getRequestForKeyboard(new KeyboardSpecificationName("baseName", 1, 2, 3),
                                         null,
                                         SpaceOnAlphanumeric.COMMIT,
                                         null,
                                         null,
                                         configuration);
      assertFalse(request.hasSpecialRomanjiTable());
      assertEquals(SpaceOnAlphanumeric.COMMIT, request.getSpaceOnAlphanumeric());
      assertFalse(request.hasKanaModifierInsensitiveConversion());
      assertFalse(request.hasCrossingEdgeBehavior());
    }
    {
      Request request =
          MozcUtil.getRequestForKeyboard(new KeyboardSpecificationName("baseName", 1, 2, 3),
                                         null,
                                         null,
                                         Boolean.TRUE,
                                         null,
                                         configuration);
      assertFalse(request.hasSpaceOnAlphanumeric());
      assertFalse(request.hasSpecialRomanjiTable());
      assertTrue(request.getKanaModifierInsensitiveConversion());
      assertFalse(request.hasCrossingEdgeBehavior());
    }
    {
      Request request =
          MozcUtil.getRequestForKeyboard(new KeyboardSpecificationName("baseName", 1, 2, 3),
                                         null,
                                         null,
                                         null,
                                         CrossingEdgeBehavior.COMMIT_WITHOUT_CONSUMING,
                                         configuration);
      assertFalse(request.hasSpaceOnAlphanumeric());
      assertFalse(request.hasSpecialRomanjiTable());
      assertFalse(request.hasKanaModifierInsensitiveConversion());
      assertEquals(CrossingEdgeBehavior.COMMIT_WITHOUT_CONSUMING,
                   request.getCrossingEdgeBehavior());
    }
  }

  @SmallTest
  public void testShowInputMethodPicker() {
    Context context1 = new MockContext();
    Context context2 = new MockContext();

    MozcUtil.requestShowInputMethodPicker(context1);
    try {
      assertTrue(MozcUtil.hasShowInputMethodPickerRequest(context1));
      assertFalse(MozcUtil.hasShowInputMethodPickerRequest(context2));

      MozcUtil.cancelShowInputMethodPicker(context1);

      assertFalse(MozcUtil.hasShowInputMethodPickerRequest(context1));
      assertFalse(MozcUtil.hasShowInputMethodPickerRequest(context2));
    } finally {
      // Cancel the request in case.
      MozcUtil.cancelShowInputMethodPicker(context1);
    }
  }

  @SmallTest
  public void testIsEmojiAllowed() {
    EditorInfo editorInfo = new EditorInfo();
    assertFalse(MozcUtil.isEmojiAllowed(editorInfo));
    editorInfo.extras = new Bundle();
    assertFalse(MozcUtil.isEmojiAllowed(editorInfo));
    editorInfo.extras.putBoolean("allowEmoji", false);
    assertFalse(MozcUtil.isEmojiAllowed(editorInfo));
    editorInfo.extras.putBoolean("allowEmoji", true);
    assertTrue(MozcUtil.isEmojiAllowed(editorInfo));
  }

  @SmallTest
  public void testCreateEmojiRequest() {
    class TestData extends Parameter {
      final int sdkInt;
      final EmojiProviderType emojiProviderType;
      final Set<EmojiCarrierType> expectedEmojiCarrierTypeSet;

      TestData(int sdkInt, EmojiProviderType emojiProviderType,
               Set<EmojiCarrierType> expectedEmojiCarrierTypeSet) {
        this.sdkInt = sdkInt;
        this.emojiProviderType = emojiProviderType;
        this.expectedEmojiCarrierTypeSet = expectedEmojiCarrierTypeSet;
      }
    }

    TestData[] testDataList = {
        new TestData(7, EmojiProviderType.NONE, Collections.<EmojiCarrierType>emptySet()),
        new TestData(7, EmojiProviderType.DOCOMO, EnumSet.of(EmojiCarrierType.DOCOMO_EMOJI)),
        new TestData(7, EmojiProviderType.SOFTBANK, EnumSet.of(EmojiCarrierType.SOFTBANK_EMOJI)),
        new TestData(7, EmojiProviderType.KDDI, EnumSet.of(EmojiCarrierType.KDDI_EMOJI)),

        // Boundary check. Unicode 6.0 is not yet available on Api level 15.
        new TestData(15, EmojiProviderType.NONE, Collections.<EmojiCarrierType>emptySet()),
        new TestData(15, EmojiProviderType.DOCOMO, EnumSet.of(EmojiCarrierType.DOCOMO_EMOJI)),
        new TestData(15, EmojiProviderType.SOFTBANK, EnumSet.of(EmojiCarrierType.SOFTBANK_EMOJI)),
        new TestData(15, EmojiProviderType.KDDI, EnumSet.of(EmojiCarrierType.KDDI_EMOJI)),

        // Unicode 6.0 is available on API level 16 or higher.
        new TestData(16, EmojiProviderType.NONE, EnumSet.of(EmojiCarrierType.UNICODE_EMOJI)),
        new TestData(16, EmojiProviderType.DOCOMO,
                     EnumSet.of(EmojiCarrierType.UNICODE_EMOJI, EmojiCarrierType.DOCOMO_EMOJI)),
        new TestData(16, EmojiProviderType.SOFTBANK,
                     EnumSet.of(EmojiCarrierType.UNICODE_EMOJI, EmojiCarrierType.SOFTBANK_EMOJI)),
        new TestData(16, EmojiProviderType.KDDI,
                     EnumSet.of(EmojiCarrierType.UNICODE_EMOJI, EmojiCarrierType.KDDI_EMOJI)),
    };
    for (TestData testData : testDataList) {
      int carrier = 0;
      for (EmojiCarrierType carrierType : testData.expectedEmojiCarrierTypeSet) {
        carrier |= carrierType.getNumber();
      }
      assertEquals(
          Request.newBuilder()
              .setAvailableEmojiCarrier(carrier)
              .setEmojiRewriterCapability(RewriterCapability.ALL.getNumber())
              .build(),
        MozcUtil.createEmojiRequest(testData.sdkInt, testData.emojiProviderType));
    }
  }

  @SmallTest
  public void testUtf8CStyleByteStringToString() {
    class TestData extends Parameter {
      final ByteString input;
      final String expected;

      TestData (String input, String expected) {
        this.input = ByteString.copyFromUtf8(input);
        this.expected = expected;
      }
    }
    TestData testDataList[] = {
        new TestData("\u0000", ""),
        new TestData("1\u0000", "1"),
        new TestData("12\u0000", "12"),
        new TestData("123\u00004\0", "123"),
        new TestData("12345", "12345"),
    };

    for (TestData testData : testDataList) {
      assertEquals(testData.toString(),
                   testData.expected,
                   MozcUtil.utf8CStyleByteStringToString(testData.input));
    }
  }

  @SmallTest
  public void testClamp() {
    assertEquals(0, MozcUtil.clamp(-1, 0, 2));
    assertEquals(0, MozcUtil.clamp(0, 0, 2));
    assertEquals(1, MozcUtil.clamp(1, 0, 2));
    assertEquals(2, MozcUtil.clamp(2, 0, 2));
    assertEquals(2, MozcUtil.clamp(3, 0, 2));

    assertEquals(0.0f, MozcUtil.clamp(-1.0f, 0.0f, 2.0f));
    assertEquals(0.0f, MozcUtil.clamp(0.0f, 0.0f, 2.0f));
    assertEquals(1.0f, MozcUtil.clamp(1.0f, 0.0f, 2.0f));
    assertEquals(2.0f, MozcUtil.clamp(2.0f, 0.0f, 2.0f));
    assertEquals(2.0f, MozcUtil.clamp(3.0f, 0.0f, 2.0f));
  }

  @SmallTest
  public void testGetAbiIndependentVersionCode() {
    class TestData extends Parameter {
      final int versionCode;
      final int expectation;
      TestData(int versionCode, int expectation) {
        this.versionCode = versionCode;
        this.expectation = expectation;
      }
    }
    TestData[] testDataList = {
        new TestData(1, 1),
        new TestData(0, 0),
        new TestData(123456, 123456),
        new TestData(1000000, 0),
        new TestData(7123456, 123456),
    };
    for (TestData testData : testDataList) {
      try {
        MozcUtil.setVersionCode(Optional.of(testData.versionCode));
        assertEquals(
            testData.toString(),
            testData.expectation,
            MozcUtil.getAbiIndependentVersionCode(getInstrumentation().getTargetContext()));
      } finally {
        MozcUtil.setVersionCode(Optional.<Integer>absent());
      }
    }
  }
}
