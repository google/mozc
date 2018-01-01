// Copyright 2010-2018, Google Inc.
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

import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request.EmojiCarrierType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Request.RewriterCapability;
import org.mozc.android.inputmethod.japanese.testing.Parameter;
import com.google.common.base.Optional;

import android.os.Bundle;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.inputmethod.EditorInfo;

import java.util.Collections;
import java.util.EnumSet;
import java.util.Set;

/**
 */
public class EmojiUtilTest extends InstrumentationTestCase {

  @SmallTest
  public void testIsCarrierEmoji() {
    assertFalse(EmojiUtil.isCarrierEmoji(0));

    // Boundary check at min code point.
    assertFalse(EmojiUtil.isCarrierEmoji(EmojiUtil.MIN_EMOJI_PUA_CODE_POINT - 1));
    assertTrue(EmojiUtil.isCarrierEmoji(EmojiUtil.MIN_EMOJI_PUA_CODE_POINT));

    // Middle code point.
    assertTrue(EmojiUtil.isCarrierEmoji(
        (EmojiUtil.MIN_EMOJI_PUA_CODE_POINT + EmojiUtil.MAX_EMOJI_PUA_CODE_POINT) / 2));

    // Boundary check at max code point.
    assertTrue(EmojiUtil.isCarrierEmoji(EmojiUtil.MAX_EMOJI_PUA_CODE_POINT));
    assertFalse(EmojiUtil.isCarrierEmoji(EmojiUtil.MAX_EMOJI_PUA_CODE_POINT + 1));

    assertFalse(EmojiUtil.isCarrierEmoji(Integer.MAX_VALUE));
  }

  @SmallTest
  public void testIsCarrierEmojiProvider() {
    assertFalse(EmojiUtil.isCarrierEmojiProviderType(EmojiProviderType.NONE));
    assertTrue(EmojiUtil.isCarrierEmojiProviderType(EmojiProviderType.DOCOMO));
    assertTrue(EmojiUtil.isCarrierEmojiProviderType(EmojiProviderType.KDDI));
    assertTrue(EmojiUtil.isCarrierEmojiProviderType(EmojiProviderType.SOFTBANK));
  }

  @SmallTest
  public void testIsEmojiAllowed() {
    EditorInfo editorInfo = new EditorInfo();
    assertFalse(EmojiUtil.isCarrierEmojiAllowed(editorInfo));
    editorInfo.extras = new Bundle();
    assertFalse(EmojiUtil.isCarrierEmojiAllowed(editorInfo));
    editorInfo.extras.putBoolean("allowEmoji", false);
    assertFalse(EmojiUtil.isCarrierEmojiAllowed(editorInfo));
    editorInfo.extras.putBoolean("allowEmoji", true);
    assertTrue(EmojiUtil.isCarrierEmojiAllowed(editorInfo));
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

    EmojiUtil.unicodeEmojiRenderable = Optional.of(true);
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
        EmojiUtil.createEmojiRequest(testData.sdkInt, testData.emojiProviderType));
    }
  }
}
