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

import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.MozcUtil.TelephonyManagerInterface;

import android.content.SharedPreferences;

import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Providers whose emoji set MechaMozc supports.
 *
 */
public enum EmojiProviderType {
  DOCOMO((byte) 1), KDDI((byte) 2), SOFTBANK((byte) 4),
  // TODO(hidehiko): add unicode 6.0, when supported.
  ;

  public static String EMOJI_PROVIDER_TYPE_PREFERENCE_KEY = "pref_emoji_provider_type";
  private static final Map<String, EmojiProviderType> NETWORK_OPERATOR_MAP;
  private static final Set<String> NAME_SET;

  private final byte bitMask;

  static {
    // The key is "Mobile Country Code" + "Mobile Network Code".
    // Note that if the network operator uses MVNO (Mobile Virtual Network Operator),
    // the network operator provided by TelephonyManager would be the one of their partner's.
    Map<String, EmojiProviderType> networkOperatorMap = new HashMap<String, EmojiProviderType>();
    networkOperatorMap.put("44010", DOCOMO);
    networkOperatorMap.put("44020", SOFTBANK);
    networkOperatorMap.put("44070", KDDI);
    NETWORK_OPERATOR_MAP = Collections.unmodifiableMap(networkOperatorMap);

    Set<String> nameSet = new HashSet<String>();
    for (EmojiProviderType emojiProviderType : values()) {
      nameSet.add(emojiProviderType.name());
    }
    NAME_SET = Collections.unmodifiableSet(nameSet);
  }

  private EmojiProviderType(byte bitMask) {
    this.bitMask = bitMask;
  }

  public byte getBitMask() {
    return bitMask;
  }

  /**
   * Detects emoji provider type and sets it to the given {@code sharedPreferences},
   * if necessary based on the Mobile Network Code provided by {@code telephonyManager}.
   * If the {@code sharedPreferences} is {@code null}, or already has valid emoji provider type,
   * just does nothing.
   *
   * Note: if the new detected emoji provider type is set to the {@code sharedPreferences}
   * and if it has registered callbacks, of course, they will be invoked as usual.
   */
  public static void maybeSetDetectedEmojiProviderType(
      SharedPreferences sharedPreferences, TelephonyManagerInterface telephonyManager) {
    if (sharedPreferences == null) {
      return;
    }

    // First, check if the emoji provider has already set to the preference.
    if (NAME_SET.contains(sharedPreferences.getString(EMOJI_PROVIDER_TYPE_PREFERENCE_KEY, null))) {
      // Found the valid value.
      return;
    }

    // Here, the EmojiProviderType hasn't set yet, so detect emoji provider.
    EmojiProviderType detectedType =
        detectEmojiProviderTypeByMobileNetworkOperator(telephonyManager);
    if (detectedType != null) {
      sharedPreferences.edit()
          .putString(EMOJI_PROVIDER_TYPE_PREFERENCE_KEY, detectedType.name())
          .commit();
    }
    MozcLog.i("RUN EMOJI PROVIDER DETECTION: " + detectedType);
  }

  private static EmojiProviderType detectEmojiProviderTypeByMobileNetworkOperator(
      TelephonyManagerInterface telephonyManager) {
    return NETWORK_OPERATOR_MAP.get(telephonyManager.getNetworkOperator());
  }
}
