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
import org.mozc.android.inputmethod.japanese.emoji.EmojiRenderableChecker;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.os.Bundle;
import android.view.inputmethod.EditorInfo;

import java.util.Collections;
import java.util.EnumMap;
import java.util.Map;

/**
 * Utility class for emoji handling.
 *
 */
public class EmojiUtil {

  public static final int MIN_EMOJI_PUA_CODE_POINT = 0xFE000;
  public static final int MAX_EMOJI_PUA_CODE_POINT = 0xFEEA0;

  private static final int UNICODE_EMOJI_SUPPORT_API_VERSION = 16;
  // This field is initialized lazily.
  @VisibleForTesting static volatile Optional<Boolean> unicodeEmojiRenderable = Optional.absent();

  /**
   * Note that if the key is {@link EmojiProviderType#NONE}, {@code null} is returned.
   */
  private static final Map<EmojiProviderType, EmojiCarrierType> CARRIER_EMOJI_PROVIDER_TYPE_MAP;
  static {
    EnumMap<EmojiProviderType, EmojiCarrierType> map =
        new EnumMap<EmojiProviderType, EmojiCarrierType>(EmojiProviderType.class);
    map.put(EmojiProviderType.DOCOMO, EmojiCarrierType.DOCOMO_EMOJI);
    map.put(EmojiProviderType.SOFTBANK, EmojiCarrierType.SOFTBANK_EMOJI);
    map.put(EmojiProviderType.KDDI, EmojiCarrierType.KDDI_EMOJI);
    CARRIER_EMOJI_PROVIDER_TYPE_MAP = Collections.unmodifiableMap(map);
  }

  private EmojiUtil() {}

  /**
   * @return {@code true} if the given {@code codePoint} is in the emoji pua range.
   *   Note that the current system may not support codepoint nor the package
   *   may not have the corresponding drawable resource for the codepoint,
   *   even if this method returns {@code true}.
   */
  public static boolean isCarrierEmoji(int codePoint) {
    return MIN_EMOJI_PUA_CODE_POINT <= codePoint && codePoint <= MAX_EMOJI_PUA_CODE_POINT;
  }

  /** @return {@code true} if the given {@code type} is carrier emoji provider type. */
  public static boolean isCarrierEmojiProviderType(EmojiProviderType type) {
    return CARRIER_EMOJI_PROVIDER_TYPE_MAP.containsKey(Preconditions.checkNotNull(type));
  }

  /** @return {@code true} if carrier emoji is allowed on the text edit. */
  public static boolean isCarrierEmojiAllowed(EditorInfo editorInfo) {
    Bundle bundle = Preconditions.checkNotNull(editorInfo).extras;
    return (bundle != null) && bundle.getBoolean("allowEmoji");
  }

  /** @return {@code true} if Unicode 6.0 emoji is available. */
  public static boolean isUnicodeEmojiAvailable(int sdkInt) {
    if (sdkInt < UNICODE_EMOJI_SUPPORT_API_VERSION) {
      return false;
    }

    if (unicodeEmojiRenderable.isPresent()) {
      return unicodeEmojiRenderable.get().booleanValue();
    }

    // Lazy initialization
    synchronized (EmojiUtil.class) {
      if (!unicodeEmojiRenderable.isPresent()) {
        EmojiRenderableChecker checker = new EmojiRenderableChecker();
        String blackSunWithRays = "\u2600";
        boolean result = checker.isRenderable(blackSunWithRays);
        unicodeEmojiRenderable = Optional.of(result);
      }
    }
    return unicodeEmojiRenderable.get();
  }

  /** @return {@code Request} instance for the given emoji settings. */
  public static Request createEmojiRequest(int sdkInt, EmojiProviderType emojiProviderType) {
    Preconditions.checkNotNull(emojiProviderType);

    int availableEmojiCarrier = 0;
    if (isUnicodeEmojiAvailable(sdkInt)) {
      availableEmojiCarrier |= EmojiCarrierType.UNICODE_EMOJI.getNumber();
    }

    // NOTE: If emojiCarrierType is NONE, availableEmojiCarrier is not updated here.
    EmojiCarrierType emojiCarrierType =
        CARRIER_EMOJI_PROVIDER_TYPE_MAP.get(emojiProviderType);
    if (emojiCarrierType != null) {
      availableEmojiCarrier |= emojiCarrierType.getNumber();
    }

    return Request.newBuilder()
        .setAvailableEmojiCarrier(availableEmojiCarrier)
        .setEmojiRewriterCapability(RewriterCapability.ALL.getNumber())
        .build();
  }
}
