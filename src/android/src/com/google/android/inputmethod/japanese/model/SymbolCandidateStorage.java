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

package org.mozc.android.inputmethod.japanese.model;

import org.mozc.android.inputmethod.japanese.EmoticonData;
import org.mozc.android.inputmethod.japanese.SymbolData;
import org.mozc.android.inputmethod.japanese.SymbolInputView.MajorCategory;
import org.mozc.android.inputmethod.japanese.SymbolInputView.MinorCategory;
import org.mozc.android.inputmethod.japanese.emoji.EmojiData;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.Annotation;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import com.google.common.base.Preconditions;

import java.util.Arrays;
import java.util.Collections;
import java.util.EnumMap;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Manages between MinorCategory and its candidates.
 *
 */
public class SymbolCandidateStorage {

  public interface SymbolHistoryStorage {
    public List<String> getAllHistory(MajorCategory majorCategory);
    public void addHistory(MajorCategory majorCategory, String value);
  }

  /** Set of names of Emoji based on carriers. */
  private static class EmojiDescriptionSet {
    static final EmojiDescriptionSet NULL_INSTANCE =
        new EmojiDescriptionSet(null, null, null, null, null);
    final String[] faceDescription;
    final String[] foodDescription;
    final String[] activityDescription;
    final String[] cityDescription;
    final String[] natureDescription;

    EmojiDescriptionSet(String[] faceDescription,
                        String[] foodDescription,
                        String[] activityDescription,
                        String[] cityDescription,
                        String[] natureDescription) {
      this.faceDescription = faceDescription;
      this.foodDescription = foodDescription;
      this.activityDescription = activityDescription;
      this.cityDescription = cityDescription;
      this.natureDescription = natureDescription;
    }
  }

  /** Annotation for a Half-width one character candidate. */
  private static final String HALFWIDTH_DESCRIPTION = "[半]";
  private static final Annotation HALFWIDTH_ANNOTATION = Annotation.newBuilder()
      .setDescription(HALFWIDTH_DESCRIPTION)
      .build();

  /** Specialized description map. */
  private static final Map<String, String> DESCRIPTION_MAP;
  static {
    // TODO(team): Move this rules compile time generated code, rather than hard coding here.
    Map<String, String> descriptionMap = new HashMap<String, String>();
    descriptionMap.put("\u0020", "半角ｽﾍﾟｰｽ");  // (space)
    descriptionMap.put("\u002d", "[半]ﾊｲﾌﾝ,ﾏｲﾅｽ");  // -
    descriptionMap.put("\u2010", "[全]ﾊｲﾌﾝ");  // ‐
    descriptionMap.put("\u2015", "[全]ﾀﾞｯｼｭ");  // ―
    descriptionMap.put("\u2212", "[全]ﾏｲﾅｽ");  // −
    descriptionMap.put("\u3000", "全角ｽﾍﾟｰｽ"); // (space)
    descriptionMap.put("\uDBBA\uDF4C", "全部ﾌﾞﾗﾝｸ");  // Full-width space emoji.
    descriptionMap.put("\uDBBA\uDF4D", "半分ﾌﾞﾗﾝｸ");  // Half-width space emoji.
    descriptionMap.put("\uDBBA\uDF4E", "1/4ﾌﾞﾗﾝｸ");  // Quater-width space emoji.

    DESCRIPTION_MAP = Collections.unmodifiableMap(descriptionMap);
  }

  /** Map from the carrier to its Emoji description set. */
  private static final Map<EmojiProviderType, EmojiDescriptionSet> EMOJI_DESCRIPTION_SET_MAP;
  static {
    EnumMap<EmojiProviderType, EmojiDescriptionSet> map =
        new EnumMap<EmojiProviderType, EmojiDescriptionSet>(EmojiProviderType.class);
    map.put(EmojiProviderType.DOCOMO,
            new EmojiDescriptionSet(EmojiData.DOCOMO_FACE_NAME,
                                    EmojiData.DOCOMO_FOOD_NAME,
                                    EmojiData.DOCOMO_ACTIVITY_NAME,
                                    EmojiData.DOCOMO_CITY_NAME,
                                    EmojiData.DOCOMO_NATURE_NAME));
    map.put(EmojiProviderType.SOFTBANK,
            new EmojiDescriptionSet(EmojiData.SOFTBANK_FACE_NAME,
                                    EmojiData.SOFTBANK_FOOD_NAME,
                                    EmojiData.SOFTBANK_ACTIVITY_NAME,
                                    EmojiData.SOFTBANK_CITY_NAME,
                                    EmojiData.SOFTBANK_NATURE_NAME));
    map.put(EmojiProviderType.KDDI,
            new EmojiDescriptionSet(EmojiData.KDDI_FACE_NAME,
                                    EmojiData.KDDI_FOOD_NAME,
                                    EmojiData.KDDI_ACTIVITY_NAME,
                                    EmojiData.KDDI_CITY_NAME,
                                    EmojiData.KDDI_NATURE_NAME));
    EMOJI_DESCRIPTION_SET_MAP = Collections.unmodifiableMap(map);
  }

  private final SymbolHistoryStorage symbolHistoryStorage;
  private EmojiProviderType emojiProviderType = EmojiProviderType.NONE;
  private EmojiDescriptionSet emojiDescriptionSet = EmojiDescriptionSet.NULL_INSTANCE;
  private Map<String, String> emojiDescriptionMap = null;

  public SymbolCandidateStorage(SymbolHistoryStorage symbolHistoryStorage) {
    this.symbolHistoryStorage = symbolHistoryStorage;
  }

  public void setEmojiProviderType(EmojiProviderType emojiProviderType) {
    Preconditions.checkNotNull(emojiProviderType);

    if (this.emojiProviderType == emojiProviderType) {
      // No change.
      return;
    }

    // Update description data, too.
    EmojiDescriptionSet emojiDescriptionSet = EMOJI_DESCRIPTION_SET_MAP.get(emojiProviderType);
    this.emojiProviderType = emojiProviderType;
    this.emojiDescriptionSet = (emojiDescriptionSet == null)
        ? EmojiDescriptionSet.NULL_INSTANCE : emojiDescriptionSet;
    emojiDescriptionMap = createEmojiDescriptionMap(emojiDescriptionSet);
  }

  private static Map<String, String> createEmojiDescriptionMap(
      EmojiDescriptionSet emojiDescriptionSet) {
    if (emojiDescriptionSet == null) {
      return null;
    }

    Map<String, String> map = new HashMap<String, String>();
    createEmojiDescriptionMapInternal(
        EmojiData.FACE_VALUES, emojiDescriptionSet.faceDescription, map);
    createEmojiDescriptionMapInternal(
        EmojiData.FOOD_VALUES, emojiDescriptionSet.foodDescription, map);
    createEmojiDescriptionMapInternal(
        EmojiData.ACTIVITY_VALUES, emojiDescriptionSet.activityDescription, map);
    createEmojiDescriptionMapInternal(
        EmojiData.CITY_VALUES, emojiDescriptionSet.cityDescription, map);
    createEmojiDescriptionMapInternal(
        EmojiData.NATURE_VALUES, emojiDescriptionSet.natureDescription, map);
    return Collections.unmodifiableMap(map);
  }

  private static void createEmojiDescriptionMapInternal(
      String[] values, String[] descriptions, Map<String, String> map) {
    if (values.length != descriptions.length) {
      throw new IllegalArgumentException();
    }

    for (int i = 0; i < descriptions.length; ++i) {
      String description = descriptions[i];
      if (description != null) {
        map.put(values[i], description);
      }
    }
  }

  /** @return the {@link CandidateList} instance for the given {@code minorCategory}. */
  public CandidateList getCandidateList(MinorCategory minorCategory) {
    switch (minorCategory) {
      // SYMBOL major category candidates.
      case SYMBOL_HISTORY:
        return toCandidateList(symbolHistoryStorage.getAllHistory(MajorCategory.SYMBOL));
      case SYMBOL_GENERAL:
        return toCandidateList(Arrays.asList(SymbolData.GENERAL_VALUES));
      case SYMBOL_HALF:
        return toCandidateList(Arrays.asList(SymbolData.HALF_VALUES));
      case SYMBOL_PARENTHESIS:
        return toCandidateList(Arrays.asList(SymbolData.PARENTHESIS_VALUES));
      case SYMBOL_ARROW:
        return toCandidateList(Arrays.asList(SymbolData.ARROW_VALUES));
      case SYMBOL_MATH:
        return toCandidateList(Arrays.asList(SymbolData.MATH_VALUES));

      // EMOTICON major category candidates.
      case EMOTICON_HISTORY:
        return toCandidateList(symbolHistoryStorage.getAllHistory(MajorCategory.EMOTICON));
      case EMOTICON_SMILE:
        return toCandidateList(Arrays.asList(EmoticonData.SMILE_VALUES));
      case EMOTICON_SWEAT:
        return toCandidateList(Arrays.asList(EmoticonData.SWEAT_VALUES));
      case EMOTICON_SURPRISE:
        return toCandidateList(Arrays.asList(EmoticonData.SURPRISE_VALUES));
      case EMOTICON_SADNESS:
        return toCandidateList(Arrays.asList(EmoticonData.SADNESS_VALUES));
      case EMOTICON_DISPLEASURE:
        return toCandidateList(Arrays.asList(EmoticonData.DISPLEASURE_VALUES));

      // EMOJI major category candidates.
      case EMOJI_HISTORY:
        return toCandidateList(
            symbolHistoryStorage.getAllHistory(MajorCategory.EMOJI), emojiDescriptionMap);
      case EMOJI_FACE:
        return toEmojiCandidateList(EmojiData.FACE_VALUES, emojiDescriptionSet.faceDescription);
      case EMOJI_FOOD:
        return toEmojiCandidateList(EmojiData.FOOD_VALUES, emojiDescriptionSet.foodDescription);
      case EMOJI_ACTIVITY:
        return toEmojiCandidateList(
            EmojiData.ACTIVITY_VALUES, emojiDescriptionSet.activityDescription);
      case EMOJI_CITY:
        return toEmojiCandidateList(EmojiData.CITY_VALUES, emojiDescriptionSet.cityDescription);
      case EMOJI_NATURE:
        return toEmojiCandidateList(EmojiData.NATURE_VALUES, emojiDescriptionSet.natureDescription);
    }

    throw new IllegalArgumentException("Unknown minor category: " + minorCategory.toString());
  }

  /** Just short cut of {@code toCandidateList(values, null)}. */
  private static CandidateList toCandidateList(List<String> values) {
    return toCandidateList(values, null);
  }

  /** Build the {@link CandidateList} based on the given values and emojiDescriptionMap. */
  // Package private for testing.
  static CandidateList toCandidateList(
      List<String> values, Map<String, String> emojiDescriptionMap) {
    if (values.isEmpty()) {
      return CandidateList.getDefaultInstance();
    }

    CandidateList.Builder builder = CandidateList.newBuilder();
    int index = 0;
    for (String value : values) {
      CandidateWord.Builder wordBuilder = CandidateWord.newBuilder();
      wordBuilder.setValue(value);
      wordBuilder.setId(index);
      wordBuilder.setIndex(index);
      Annotation annotation = getAnnotation(value, emojiDescriptionMap);
      if (annotation != null) {
        wordBuilder.setAnnotation(annotation);
      }
      builder.addCandidates(wordBuilder);
      ++index;
    }
    return builder.build();
  }

  private static Annotation getAnnotation(String value, Map<String, String> emojiDescriptionMap) {
    // We do not use resource to store the string below because
    // there are no needs to translate the description.
    // In addition we cannot access the resource from here
    // because Context is not available.

    // Rule base annotation.
    {
      String description = DESCRIPTION_MAP.get(value);
      if (description != null) {
        return Annotation.newBuilder().setDescription(description).build();
      }
    }

    // Emoji specialized annotation. Note that the given map depends on the current provider type.
    // This is just only for history annotation.
    if (emojiDescriptionMap != null){
      String description = emojiDescriptionMap.get(value);
      if (description != null) {
        return Annotation.newBuilder().setDescription(description).build();
      }
    }

    // If the candidate has only one character and its codepoint is <= 0x7F,
    // add HALFWIDTH_ANNOTATION.
    // This criteria is different from the Mozc engine's
    // (it is basically the same as here but length check is omitted)
    // but the engine's criteria seems too aggressive for our purpose.
    if (value.length() == 1 && value.charAt(0) <= 0x7F) {
      return HALFWIDTH_ANNOTATION;
    }

    // No description is available.
    return null;
  }

  // Package private for testing.
  static CandidateList toEmojiCandidateList(String[] values, String[] descriptions) {
    if (descriptions == null) {
      return CandidateList.getDefaultInstance();
    }

    CandidateList.Builder builder = CandidateList.newBuilder();
    int index = 0;
    for (int i = 0; i < descriptions.length; ++i) {
      String description = descriptions[i];
      if (description == null) {
        // If no description (name) is available, we skip the value,
        // because the value is not supported under the current carrier.
        continue;
      }

      builder.addCandidates(CandidateWord.newBuilder()
          .setValue(values[i])
          .setId(index)
          .setIndex(index)
          .setAnnotation(Annotation.newBuilder()
              .setDescription(description)));
      ++index;
    }

    if (index == 0) {
      // No values are available.
      return CandidateList.getDefaultInstance();
    }
    return builder.build();
  }
}
