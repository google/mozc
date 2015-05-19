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
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request.CrossingEdgeBehavior;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request.SpaceOnAlphanumeric;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request.SpecialRomanjiTable;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import com.google.common.base.Optional;
import com.google.protobuf.ByteString;

import android.content.Context;
import android.content.res.Configuration;
import android.test.InstrumentationTestCase;
import android.test.mock.MockContext;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.InputType;
import android.view.inputmethod.EditorInfo;

/**
 */
public class MozcUtilTest extends InstrumentationTestCase {
  @SmallTest
  public void testIsDevChannel() {
    class TestData extends Parameter {
      final String versionName;
      final boolean isDevChannel;
      public TestData(String versionName, boolean isDevChanel) {
        this.versionName = versionName;
        this.isDevChannel = isDevChanel;
      }
    }
    TestData testDataList[] = {
      new TestData("-neko", false),
      new TestData("1-neko", false),
      new TestData(".-neko", false),
      new TestData("1.2.3.000-neko", false),
      new TestData("111111-neko", false),
      new TestData("1.1.1.1-neko", false),
      new TestData("1.1.1.99-neko", false),
      new TestData("1.1.1.100-neko", true),
      new TestData(".1-neko", false),
      new TestData(".a-neko", false),
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
      assertEquals(testData.toString(),
                   testData.isDevChannel, MozcUtil.isDevChannelVersionName(testData.versionName));
    }
  }

  @SmallTest
  public void testGetRequest() {
    Configuration configuration = new Configuration();
    for (KeyboardSpecification specification : KeyboardSpecification.values()) {
      for (int orientation : new int[] {Configuration.ORIENTATION_PORTRAIT,
                                        Configuration.ORIENTATION_LANDSCAPE}) {
        configuration.orientation = orientation;
        Request request = MozcUtil.getRequestForKeyboard(
            specification.getKeyboardSpecificationName(),
            Optional.of(specification.getSpecialRomanjiTable()),
            Optional.of(specification.getSpaceOnAlphanumeric()),
            Optional.of(specification.isKanaModifierInsensitiveConversion()),
            Optional.of(specification.getCrossingEdgeBehavior()),
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
                                         Optional.of(SpecialRomanjiTable.DEFAULT_TABLE),
                                         Optional.<SpaceOnAlphanumeric>absent(),
                                         Optional.<Boolean>absent(),
                                         Optional.<CrossingEdgeBehavior>absent(),
                                         configuration);
      assertEquals(SpecialRomanjiTable.DEFAULT_TABLE, request.getSpecialRomanjiTable());
      assertFalse(request.hasSpaceOnAlphanumeric());
      assertFalse(request.hasKanaModifierInsensitiveConversion());
      assertFalse(request.hasCrossingEdgeBehavior());
    }
    {
      Request request =
          MozcUtil.getRequestForKeyboard(new KeyboardSpecificationName("baseName", 1, 2, 3),
                                         Optional.<SpecialRomanjiTable>absent(),
                                         Optional.of(SpaceOnAlphanumeric.COMMIT),
                                         Optional.<Boolean>absent(),
                                         Optional.<CrossingEdgeBehavior>absent(),
                                         configuration);
      assertFalse(request.hasSpecialRomanjiTable());
      assertEquals(SpaceOnAlphanumeric.COMMIT, request.getSpaceOnAlphanumeric());
      assertFalse(request.hasKanaModifierInsensitiveConversion());
      assertFalse(request.hasCrossingEdgeBehavior());
    }
    {
      Request request =
          MozcUtil.getRequestForKeyboard(new KeyboardSpecificationName("baseName", 1, 2, 3),
                                         Optional.<SpecialRomanjiTable>absent(),
                                         Optional.<SpaceOnAlphanumeric>absent(),
                                         Optional.of(Boolean.TRUE),
                                         Optional.<CrossingEdgeBehavior>absent(),
                                         configuration);
      assertFalse(request.hasSpaceOnAlphanumeric());
      assertFalse(request.hasSpecialRomanjiTable());
      assertTrue(request.getKanaModifierInsensitiveConversion());
      assertFalse(request.hasCrossingEdgeBehavior());
    }
    {
      Request request =
          MozcUtil.getRequestForKeyboard(new KeyboardSpecificationName("baseName", 1, 2, 3),
                                         Optional.<SpecialRomanjiTable>absent(),
                                         Optional.<SpaceOnAlphanumeric>absent(),
                                         Optional.<Boolean>absent(),
                                         Optional.of(CrossingEdgeBehavior.COMMIT_WITHOUT_CONSUMING),
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
  public void testIsPasswordField() {
    class TestData extends Parameter {
      final int inputType;
      final boolean expectedPasswordField;
      TestData(int inputType, boolean expectedPasswordField) {
        this.inputType = inputType;
        this.expectedPasswordField = expectedPasswordField;
      }
    }

    TestData[] testDataList = {
        new TestData(0, false),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD,
                     true),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD, true),
        new TestData(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD, true),
        new TestData(InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD, false),
        new TestData(InputType.TYPE_TEXT_VARIATION_PASSWORD, false),
        new TestData(InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD, false),
    };

    for (TestData testData : testDataList) {
      EditorInfo editorInfo = new EditorInfo();
      editorInfo.inputType = testData.inputType;
      assertEquals(testData.expectedPasswordField, MozcUtil.isPasswordField(editorInfo));
    }
  }

  @SmallTest
  public void testIsVoiceInputAllowed() {
    class TestData extends Parameter {
      final String privateImeOptions;
      final boolean expectedIsVoiceInputAllowed;
      TestData(String privateImeOptions, boolean expectedIsVoiceInputAllowed) {
        this.privateImeOptions = privateImeOptions;
        this.expectedIsVoiceInputAllowed = expectedIsVoiceInputAllowed;
      }
    }

    TestData[] testDataList = {
        new TestData(null, true),
        new TestData("", true),
        new TestData("nnm", true),
        new TestData("nnm,a", true),
        new TestData("nm", false),
        new TestData("a,nm", false),
        new TestData("nm,b", false),
        new TestData("a,nm,b", false),
        new TestData("com.google.android.inputmethod.latin.noMicrophoneKey", false),
    };

    for (TestData testData : testDataList) {
      EditorInfo editorInfo = new EditorInfo();
      editorInfo.privateImeOptions = testData.privateImeOptions;
      assertEquals(testData.expectedIsVoiceInputAllowed, MozcUtil.isVoiceInputAllowed(editorInfo));
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
        new TestData(5000001, 0),
        new TestData(5123456, 12345),
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
