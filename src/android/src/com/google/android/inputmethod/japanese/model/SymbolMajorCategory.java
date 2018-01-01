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
import org.mozc.android.inputmethod.japanese.ui.CandidateLayoutRenderer;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayoutRenderer.DescriptionLayoutPolicy;
import com.google.common.base.Preconditions;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Symbol's major category to which the minor categories belong.
 */
public enum SymbolMajorCategory {
  NUMBER(
      R.id.category_selector_major_number,
      R.raw.symbol__major__number,
      R.raw.symbol__major__number_selected,
      R.dimen.symbol_major_number_height,
      Collections.singletonList(SymbolMinorCategory.NUMBER),
      R.dimen.symbol_view_symbol_min_column_width,
      DescriptionLayoutPolicy.GONE),  // DescriptionLayoutPolicy is not effective for NUMBER.
  SYMBOL(
      R.id.category_selector_major_symbol,
      R.raw.symbol__major__symbol,
      R.raw.symbol__major__symbol_selected,
      R.dimen.symbol_major_symbol_height,
      Arrays.asList(
          SymbolMinorCategory.SYMBOL_HISTORY,
          SymbolMinorCategory.SYMBOL_GENERAL,
          SymbolMinorCategory.SYMBOL_HALF,
          SymbolMinorCategory.SYMBOL_PARENTHESIS,
          SymbolMinorCategory.SYMBOL_ARROW,
          SymbolMinorCategory.SYMBOL_MATH
      ),
      R.dimen.symbol_view_symbol_min_column_width,
      DescriptionLayoutPolicy.OVERLAY),
  EMOTICON(
      R.id.category_selector_major_emoticon,
      R.raw.symbol__major__emoticon,
      R.raw.symbol__major__emoticon_selected,
      R.dimen.symbol_major_emoticon_height,
      Arrays.asList(
          SymbolMinorCategory.EMOTICON_HISTORY,
          SymbolMinorCategory.EMOTICON_SMILE,
          SymbolMinorCategory.EMOTICON_SWEAT,
          SymbolMinorCategory.EMOTICON_SURPRISE,
          SymbolMinorCategory.EMOTICON_SADNESS,
          SymbolMinorCategory.EMOTICON_DISPLEASURE
      ),
      R.dimen.symbol_view_emoticon_min_column_width,
      DescriptionLayoutPolicy.OVERLAY),
  EMOJI(
      R.id.category_selector_major_emoji,
      R.raw.symbol__major__emoji,
      R.raw.symbol__major__emoji_selected,
      R.dimen.symbol_major_emoji_height,
      Arrays.asList(
          SymbolMinorCategory.EMOJI_HISTORY,
          SymbolMinorCategory.EMOJI_FACE,
          SymbolMinorCategory.EMOJI_FOOD,
          SymbolMinorCategory.EMOJI_ACTIVITY,
          SymbolMinorCategory.EMOJI_CITY,
          SymbolMinorCategory.EMOJI_NATURE
      ),
      R.dimen.symbol_view_emoji_min_column_width,
      DescriptionLayoutPolicy.GONE)  // Description is not show in the light of UX
  ;

  // All fields are invariant so access directly.
  public final int buttonResourceId;
  public final int buttonImageResourceId;
  public final int buttonSelectedImageResourceId;
  public final int maxImageHeightResourceId;
  public final List<SymbolMinorCategory> minorCategories;
  public final int minColumnWidthResourceId;
  public final DescriptionLayoutPolicy layoutPolicy;

  /**
   * @param buttonResourceId the resource id (R.id.xxxx) of corresponding selector button.
   * @param buttonImageResourceId is the resource id (R.raw.xxx) of corresponding major
   *        category button image.
   * @param buttonSelectedImageResourceId is the resource id (R.raw.xxx) of corresponding major
   *        category button image for selected state.
   * @param minorCategories the minor categories which belong to this SymbolMajorCategory.
   *        {@code minorCategories.get(0)} is treated as default one.
   * @param minColumnWidthResourceId the resource id (R.dimen.xxxx) which represents
   *        the minimum width of each column.
   */
  private SymbolMajorCategory(
      int buttonResourceId,
      int buttonImageResourceId,
      int buttonSelectedImageResourceId,
      int maxImageHeightResourceId,
      List<SymbolMinorCategory> minorCategories,
      int minColumnWidthResourceId,
      CandidateLayoutRenderer.DescriptionLayoutPolicy layoutPolicy) {
    // We just store resource id instead of real bitmap/integer
    // because we cannot obtain them here (Context instance is needed).
    this.buttonResourceId = buttonResourceId;
    this.buttonImageResourceId = buttonImageResourceId;
    this.buttonSelectedImageResourceId = buttonSelectedImageResourceId;
    this.maxImageHeightResourceId = maxImageHeightResourceId;
    this.minorCategories = Preconditions.checkNotNull(minorCategories);
    this.minColumnWidthResourceId = minColumnWidthResourceId;
    this.layoutPolicy = layoutPolicy;
  }

  /**
   * Returns default minor category.
   *
   * When this major category is selected,
   * the default minor category returned by this method will be activated.
   */
  public SymbolMinorCategory getDefaultMinorCategory() {
    return minorCategories.get(0);
  }

  public SymbolMinorCategory getMinorCategoryByRelativeIndex(SymbolMinorCategory minorCategory,
                                                             int relativeIndex) {
    int index = minorCategories.indexOf(Preconditions.checkNotNull(minorCategory));
    int newIndex = (index + relativeIndex + minorCategories.size())
        % minorCategories.size();
    return minorCategories.get(newIndex);
  }

  static SymbolMajorCategory findMajorCategory(SymbolMinorCategory minorCategory) {
    Preconditions.checkNotNull(minorCategory);
    for (SymbolMajorCategory majorCategory : SymbolMajorCategory.values()) {
      if (majorCategory.minorCategories.contains(minorCategory)) {
        return majorCategory;
      }
    }
    throw new IllegalArgumentException("Invalid minor category.");
  }
}
