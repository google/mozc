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

package org.mozc.android.inputmethod.japanese.model;

import org.mozc.android.inputmethod.japanese.resources.R;

/**
 * Symbol's minor category to which the candidates belong.
 */
public enum SymbolMinorCategory {
  NUMBER(SymbolMinorCategory.INVALID_RESOURCE_ID, SymbolMinorCategory.INVALID_RESOURCE_ID,
         SymbolMinorCategory.INVALID_RESOURCE_ID, SymbolMinorCategory.INVALID_RESOURCE_ID),
  SYMBOL_HISTORY(
      R.raw.symbol__minor__history, R.raw.symbol__minor__history_selected,
      R.dimen.symbol_minor_default_height, R.string.cd_symbol_window_minor_history),
  SYMBOL_GENERAL(
      R.raw.symbol__minor__general, R.raw.symbol__minor__general_selected,
      R.dimen.symbol_minor_default_height, R.string.cd_symbol_window_minor_symbol_general),
  SYMBOL_HALF(
      R.raw.symbol__minor__fullhalf, R.raw.symbol__minor__fullhalf_selected,
      R.dimen.symbol_minor_default_height, R.string.cd_symbol_window_minor_symbol_half),
  SYMBOL_PARENTHESIS(
      R.raw.symbol__minor__parenthesis, R.raw.symbol__minor__parenthesis_selected,
      R.dimen.symbol_minor_default_height, R.string.cd_symbol_window_minor_symbol_parenthesis),
  SYMBOL_ARROW(
      R.raw.symbol__minor__arrow, R.raw.symbol__minor__arrow_selected,
      R.dimen.symbol_minor_default_height, R.string.cd_symbol_window_minor_symbol_arrow),
  SYMBOL_MATH(
      R.raw.symbol__minor__math, R.raw.symbol__minor__math_selected,
      R.dimen.symbol_minor_default_height, R.string.cd_symbol_window_minor_symbol_math),
  EMOTICON_HISTORY(
      R.raw.symbol__minor__history, R.raw.symbol__minor__history_selected,
      R.dimen.symbol_minor_default_height, R.string.cd_symbol_window_minor_history),
  EMOTICON_SMILE(
      R.raw.symbol__minor__smile, R.raw.symbol__minor__smile_selected,
      R.dimen.symbol_minor_emoticon_height, R.string.cd_symbol_window_minor_emoticon_smile),
  EMOTICON_SWEAT(
      R.raw.symbol__minor__sweat, R.raw.symbol__minor__sweat_selected,
      R.dimen.symbol_minor_emoticon_height, R.string.cd_symbol_window_minor_emoticon_sweat),
  EMOTICON_SURPRISE(
      R.raw.symbol__minor__surprise, R.raw.symbol__minor__surprise_selected,
      R.dimen.symbol_minor_emoticon_height, R.string.cd_symbol_window_minor_emoticon_surprise),
  EMOTICON_SADNESS(
      R.raw.symbol__minor__sadness, R.raw.symbol__minor__sadness_selected,
      R.dimen.symbol_minor_emoticon_height, R.string.cd_symbol_window_minor_emoticon_sadness),
  EMOTICON_DISPLEASURE(
      R.raw.symbol__minor__displeasure, R.raw.symbol__minor__displeasure_selected,
      R.dimen.symbol_minor_emoticon_height, R.string.cd_symbol_window_minor_emoticon_displeasure),
  EMOJI_HISTORY(
      R.raw.symbol__minor__history, R.raw.symbol__minor__history_selected,
      R.dimen.symbol_minor_default_height, R.string.cd_symbol_window_minor_history),
  EMOJI_FACE(
      R.raw.symbol__minor__face, R.raw.symbol__minor__face_selected,
      R.dimen.symbol_minor_default_height, R.string.cd_symbol_window_minor_emoji_face),
  EMOJI_FOOD(
      R.raw.symbol__minor__food, R.raw.symbol__minor__food_selected,
      R.dimen.symbol_minor_default_height, R.string.cd_symbol_window_minor_emoji_food),
  EMOJI_ACTIVITY(
      R.raw.symbol__minor__activity, R.raw.symbol__minor__activity_selected,
      R.dimen.symbol_minor_default_height, R.string.cd_symbol_window_minor_emoji_activity),
  EMOJI_CITY(
      R.raw.symbol__minor__city, R.raw.symbol__minor__city_selected,
      R.dimen.symbol_minor_default_height, R.string.cd_symbol_window_minor_emoji_city),
  EMOJI_NATURE(
      R.raw.symbol__minor__nature, R.raw.symbol__minor__nature_selected,
      R.dimen.symbol_minor_default_height, R.string.cd_symbol_window_minor_emoji_nature)
  ;

  public static final int INVALID_RESOURCE_ID = 0;
  public final int drawableResourceId;
  public final int selectedDrawableResourceId;
  public final int maxImageHeightResourceId;
  public final int contentDescriptionResourceId;

  /**
   * @param drawableResourceId the resource id of the corresponding drawable.
   * @param contentDescriptionResourceId the resource id of the corresponding content description.
   */
  SymbolMinorCategory(int drawableResourceId, int selectedDrawableResourceId,
                      int maxImageHeightResourceId, int contentDescriptionResourceId) {
    this.drawableResourceId = drawableResourceId;
    this.selectedDrawableResourceId = selectedDrawableResourceId;
    this.maxImageHeightResourceId = maxImageHeightResourceId;
    this.contentDescriptionResourceId = contentDescriptionResourceId;
  }
}
