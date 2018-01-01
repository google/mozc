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

package org.mozc.android.inputmethod.japanese.util;

import com.google.common.base.MoreObjects;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.StringTokenizer;

/**
 * Utility to handle candidate description.
 */
public class CandidateDescriptionUtil {

  private static final Set<String> DESCRIPTION_BLACKLIST_SET;
  static {
    String[] blacklist = new String[] {
        "ひらがな",
        "数字",
        "丸数字",
        "大字",
        "絵文字",
        "顔文字",
        "<機種依存>",
        "捨て仮名",
    };
    DESCRIPTION_BLACKLIST_SET = Collections.unmodifiableSet(
        new HashSet<String>(Arrays.asList(blacklist)));
  }

  private static final String[] DESCRIPTION_SUFFIX_BLACKLIST = new String[] {
    "の旧字体",
    "の簡易慣用字体",
    "の印刷標準字体",
    "の俗字",
    "の正字",
    "の本字",
    "の異体字",
    "の略字",
    "の別字",
  };

  private static final Map<String, String> DESCRIPTION_SHORTEN_MAP;
  static {
    Map<String, String> map = new HashMap<String, String>();
    map.put("小書き文字", "小書き");
    map.put("ローマ数字(大文字)", "ローマ数字");
    map.put("ローマ数字(小文字)", "ローマ数字");
    DESCRIPTION_SHORTEN_MAP = Collections.unmodifiableMap(map);
  }

  private CandidateDescriptionUtil() {}

  public static List<String> extractDescriptions(
      String description, Optional<String> descriptionDelimiter) {
    Preconditions.checkNotNull(description);
    Preconditions.checkNotNull(descriptionDelimiter);

    if (description.length() == 0) {
      // No description is available.
      return Collections.emptyList();
    }

    if (!descriptionDelimiter.isPresent()) {
      // If the delimiter is not set, return the description as is.
      return Collections.singletonList(description);
    }

    // Split the description by delimiter.
    StringTokenizer tokenizer = new StringTokenizer(description, descriptionDelimiter.get());
    List<String> result = new ArrayList<String>();
    while (tokenizer.hasMoreTokens()) {
      String token = tokenizer.nextToken();
      if (isEligibleDescriptionFragment(token)) {
        result.add(shortenDescriptionFragment(token));
      }
    }

    return result;
  }

  private static boolean isEligibleDescriptionFragment(String descriptionFragment) {
    // We'd like to always remove some descriptions because the description fragment frequently
    // and largely increases the width of a candidate span.
    // Increased width reduces the number of the candidates which are shown in a screen.
    // TODO(matsuzakit): Such filtering/optimization should be done in the server side.
    if (DESCRIPTION_BLACKLIST_SET.contains(descriptionFragment)) {
      return false;
    }
    for (String suffix : DESCRIPTION_SUFFIX_BLACKLIST) {
      if (descriptionFragment.endsWith(suffix)) {
        return false;
      }
    }
    return true;
  }

  private static String shortenDescriptionFragment(String descriptionFragment) {
    Preconditions.checkNotNull(descriptionFragment);
    return MoreObjects.firstNonNull(DESCRIPTION_SHORTEN_MAP.get(descriptionFragment),
                                descriptionFragment);
  }
}
