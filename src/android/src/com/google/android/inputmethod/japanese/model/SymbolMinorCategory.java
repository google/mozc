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

import org.mozc.android.inputmethod.japanese.resources.R;

/**
 * Symbol's minor category to which the candidates belong.
 */
public enum SymbolMinorCategory {
  SYMBOL_HISTORY(R.string.symbol_minor_symbol_history_title),
  SYMBOL_GENERAL(R.string.symbol_minor_symbol_general_title),
  SYMBOL_HALF(R.string.symbol_minor_symbol_half_title),
  SYMBOL_PARENTHESIS(R.string.symbol_minor_symbol_parenthesis_title),
  SYMBOL_ARROW(R.string.symbol_minor_symbol_arrow_title),
  SYMBOL_MATH(R.string.symbol_minor_symbol_math_title),
  EMOTICON_HISTORY(R.string.symbol_minor_emoticon_history_title),
  EMOTICON_SMILE(R.string.symbol_minor_emoticon_smile_title),
  EMOTICON_SWEAT(R.string.symbol_minor_emoticon_sweat_title),
  EMOTICON_SURPRISE(R.string.symbol_minor_emoticon_surprise_title),
  EMOTICON_SADNESS(R.string.symbol_minor_emoticon_sadness_title),
  EMOTICON_DISPLEASURE(R.string.symbol_minor_emoticon_displeasure_title),
  EMOJI_HISTORY(R.string.symbol_minor_emoji_history_title),
  EMOJI_FACE(R.string.symbol_minor_emoji_face_title),
  EMOJI_FOOD(R.string.symbol_minor_emoji_food_title),
  EMOJI_ACTIVITY(R.string.symbol_minor_emoji_activity_title),
  EMOJI_CITY(R.string.symbol_minor_emoji_city_title),
  EMOJI_NATURE(R.string.symbol_minor_emoji_nature_title)
  ;

  public final int textResourceId;

  /**
   * @param textResourceId the resource id of the corresponding text title.
   */
  SymbolMinorCategory(int textResourceId) {
    this.textResourceId = textResourceId;
  }
}