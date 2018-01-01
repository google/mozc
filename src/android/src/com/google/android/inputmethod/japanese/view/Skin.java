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

package org.mozc.android.inputmethod.japanese.view;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Paint.Style;
import android.graphics.drawable.Drawable;


/**
 * Skin data.
 */
public class Skin {

  private static final Skin FALLBACK_INSTANCE = new Skin();

  @VisibleForTesting static final int STYLE_CATEGORY_KEYICON_MAIN = 0;
  @VisibleForTesting static final int STYLE_CATEGORY_KEYICON_GUIDE = 1;
  @VisibleForTesting static final int STYLE_CATEGORY_KEYICON_GUIDE_LIGHT = 2;
  @VisibleForTesting static final int STYLE_CATEGORY_KEYICON_MAIN_HIGHLIGHT = 3;
  @VisibleForTesting static final int STYLE_CATEGORY_KEYICON_GUIDE_HIGHLIGHT = 4;
  @VisibleForTesting static final int STYLE_CATEGORY_KEYICON_BOUND = 5;
  @VisibleForTesting static final int STYLE_CATEGORY_KEYICON_TWELVEKEYS_FUNCTION = 6;
  @VisibleForTesting static final int STYLE_CATEGORY_KEYICON_TWELVEKEYS_GLOBE = 7;
  @VisibleForTesting static final int STYLE_CATEGORY_KEYICON_QWERTY_FUNCTION = 8;
  @VisibleForTesting static final int STYLE_CATEGORY_KEYICON_FUNCTION_DARK = 9;
  @VisibleForTesting static final int STYLE_CATEGORY_KEYICON_ENTER = 10;
  @VisibleForTesting static final int STYLE_CATEGORY_KEYICON_ENTER_CIRCLE = 11;
  @VisibleForTesting static final int STYLE_CATEGORY_KEYICON_QWERTY_SHIFT_ON_ARROW = 12;
  @VisibleForTesting static final int STYLE_CATEGORY_KEYPOPUP_HIGHLIGHT = 13;
  @VisibleForTesting static final int STYLE_CATEGORY_SYMBOL_MAJOR = 14;
  @VisibleForTesting static final int STYLE_CATEGORY_SYMBOL_MAJOR_SELECTED = 15;
  @VisibleForTesting static final int STYLE_CATEGORY_SYMBOL_MAJOR_EMOJI_DISABLE_CIRCLE = 16;
  @VisibleForTesting static final int STYLE_CATEGORY_SYMBOL_MINOR = 17;
  @VisibleForTesting static final int STYLE_CATEGORY_SYMBOL_MINOR_SELECTED = 18;
  @VisibleForTesting
  static final int STYLE_CATEGORY_KEYBOARD_FOLDING_BUTTON_BACKGROUND_DEFAULT = 19;
  @VisibleForTesting
  static final int STYLE_CATEGORY_KEYBOARD_FOLDING_BUTTON_BACKGROUND_SCROLLED = 20;

  public int keyboardSeparatorColor;
  public int twelvekeysLayoutReleasedKeyTopColor;
  public int twelvekeysLayoutReleasedKeyBottomColor;
  public int twelvekeysLayoutReleasedKeyHighlightColor;
  public int twelvekeysLayoutReleasedKeyLightShadeColor;
  public int twelvekeysLayoutReleasedKeyDarkShadeColor;
  public int twelvekeysLayoutReleasedKeyShadowColor;

  public int twelvekeysLayoutPressedKeyTopColor;
  public int twelvekeysLayoutPressedKeyBottomColor;
  public int twelvekeysLayoutPressedKeyHighlightColor;
  public int twelvekeysLayoutPressedKeyLightShadeColor;
  public int twelvekeysLayoutPressedKeyDarkShadeColor;
  public int twelvekeysLayoutPressedKeyShadowColor;

  public int twelvekeysLayoutReleasedFunctionKeyTopColor;
  public int twelvekeysLayoutReleasedFunctionKeyBottomColor;
  public int twelvekeysLayoutReleasedFunctionKeyHighlightColor;
  public int twelvekeysLayoutReleasedFunctionKeyLightShadeColor;
  public int twelvekeysLayoutReleasedFunctionKeyDarkShadeColor;
  public int twelvekeysLayoutReleasedFunctionKeyShadowColor;

  public int twelvekeysLayoutPressedFunctionKeyTopColor;
  public int twelvekeysLayoutPressedFunctionKeyBottomColor;
  public int twelvekeysLayoutPressedFunctionKeyHighlightColor;
  public int twelvekeysLayoutPressedFunctionKeyLightShadeColor;
  public int twelvekeysLayoutPressedFunctionKeyDarkShadeColor;
  public int twelvekeysLayoutPressedFunctionKeyShadowColor;

  public int qwertyLayoutReleasedKeyTopColor;
  public int qwertyLayoutReleasedKeyBottomColor;
  public int qwertyLayoutReleasedKeyHighlightColor;
  public int qwertyLayoutReleasedKeyShadowColor;

  public int qwertyLayoutPressedKeyTopColor;
  public int qwertyLayoutPressedKeyBottomColor;
  public int qwertyLayoutPressedKeyHighlightColor;
  public int qwertyLayoutPressedKeyShadowColor;

  public int qwertyLayoutReleasedFunctionKeyTopColor;
  public int qwertyLayoutReleasedFunctionKeyBottomColor;
  public int qwertyLayoutReleasedFunctionKeyHighlightColor;
  public int qwertyLayoutReleasedFunctionKeyShadowColor;

  public int qwertyLayoutPressedFunctionKeyTopColor;
  public int qwertyLayoutPressedFunctionKeyBottomColor;
  public int qwertyLayoutPressedFunctionKeyHighlightColor;
  public int qwertyLayoutPressedFunctionKeyShadowColor;

  public int qwertyLayoutReleasedSpaceKeyTopColor;
  public int qwertyLayoutReleasedSpaceKeyBottomColor;
  public int qwertyLayoutReleasedSpaceKeyHighlightColor;
  public int qwertyLayoutReleasedSpaceKeyShadowColor;

  public int qwertyLayoutPressedSpaceKeyTopColor;
  public int qwertyLayoutPressedSpaceKeyBottomColor;
  public int qwertyLayoutPressedSpaceKeyHighlightColor;
  public int qwertyLayoutPressedSpaceKeyShadowColor;

  public int flickBaseColor;
  public int flickShadeColor;

  public int popupFrameWindowTopColor;
  public int popupFrameWindowBottomColor;
  public int popupFrameWindowShadowColor;

  public int candidateScrollBarTopColor;
  public int candidateScrollBarBottomColor;
  public int candidateBackgroundTopColor;
  public int candidateBackgroundBottomColor;
  public int candidateBackgroundSeparatorColor;
  public int candidateBackgroundHighlightColor;
  public int candidateBackgroundBorderColor;
  public int candidateBackgroundFocusedTopColor;
  public int candidateBackgroundFocusedBottomColor;
  public int candidateBackgroundFocusedShadowColor;

  public int symbolReleasedFunctionKeyTopColor;
  public int symbolReleasedFunctionKeyBottomColor;
  public int symbolReleasedFunctionKeyHighlightColor;
  public int symbolReleasedFunctionKeyShadowColor;

  public int symbolPressedFunctionKeyTopColor;
  public int symbolPressedFunctionKeyBottomColor;
  public int symbolPressedFunctionKeyHighlightColor;
  public int symbolPressedFunctionKeyShadowColor;

  public int symbolScrollBarTopColor;
  public int symbolScrollBarBottomColor;

  public int symbolMinorCategoryTabSelectedColor;
  public int symbolMinorCategoryTabPressedColor;

  public int symbolCandidateBackgroundTopColor;
  public int symbolCandidateBackgroundBottomColor;
  public int symbolCandidateBackgroundHighlightColor;
  public int symbolCandidateBackgroundBorderColor;
  public int symbolCandidateBackgroundSeparatorColor;
  public int symbolMinorCategoryVerticalSeparatorColor;
  public int symbolSeparatorColor;

  public int threeDotsColor;

  public int keyIconMainColor;
  public int keyIconGuideColor;
  public int keyIconGuideLightColor;
  public int keyIconMainHighlightColor;
  public int keyIconGuideHighlightColor;
  public int keyIconBoundColor;
  public int keyIconTwelvekeysFunctionColor;
  public int keyIconTwelvekeysGlobeColor;
  public int keyIconQwertyFunctionColor;
  public int keyIconFunctionDarkColor;
  public int keyIconEnterColor;
  public int keyIconEnterCircleColor;
  public int keyIconQwertyShiftOnArrowColor;
  public int keyPopupHighlightColor;
  public int symbolMajorColor;
  public int symbolMajorSelectedColor;
  public int symbolMinorColor;
  public int symbolMinorSelectedColor;
  public int symbolMajorButtonTopColor;
  public int symbolMajorButtonBottomColor;
  public int symbolMajorButtonPressedTopColor;
  public int symbolMajorButtonPressedBottomColor;
  public int symbolMajorButtonSelectedTopColor;
  public int symbolMajorButtonSelectedBottomColor;
  public int symbolMajorButtonShadowColor;
  public int keyboardFoldingButtonBackgroundDefaultColor;
  public int keyboardFoldingButtonBackgroundScrolledColor;
  public int candidateValueTextColor = Color.BLACK;
  public int candidateValueFocusedTextColor = Color.BLACK;
  public int candidateDescriptionTextColor = Color.GRAY;
  public int buttonFrameButtonPressedColor;

  public float twelvekeysLeftOffsetDimension;
  public float twelvekeysRightOffsetDimension;
  public float twelvekeysTopOffsetDimension;
  public float twelvekeysBottomOffsetDimension;
  public float qwertyLeftOffsetDimension;
  public float qwertyRightOffsetDimension;
  public float qwertyTopOffsetDimension;
  public float qwertyBottomOffsetDimension;
  public float qwertyRoundRadiusDimension;
  public float qwertySpaceKeyHeightDimension;
  public float qwertySpaceKeyHorizontalOffsetDimension;
  public float qwertySpaceKeyRoundRadiusDimension;
  public float symbolMajorButtonRoundDimension;
  public float symbolMajorButtonPaddingDimension;
  public float symbolMinorIndicatorHeightDimension;

  public Drawable buttonFrameBackgroundDrawable = DummyDrawable.getInstance();
  public Drawable narrowFrameBackgroundDrawable = DummyDrawable.getInstance();
  public Drawable keyboardFrameSeparatorBackgroundDrawable = DummyDrawable.getInstance();
  public Drawable symbolSeparatorAboveMajorCategoryBackgroundDrawable = DummyDrawable.getInstance();
  public Drawable windowBackgroundDrawable = DummyDrawable.getInstance();
  public Drawable conversionCandidateViewBackgroundDrawable = DummyDrawable.getInstance();
  public Drawable symbolCandidateViewBackgroundDrawable = DummyDrawable.getInstance();
  public Drawable symbolMajorCategoryBackgroundDrawable = DummyDrawable.getInstance();
  public Drawable scrollBarBackgroundDrawable = DummyDrawable.getInstance();

  private Optional<MozcDrawableFactory> drawableFactory = Optional.absent();

  public static Skin getFallbackInstance() {
    return FALLBACK_INSTANCE;
  }

  public void apply(Paint paint, int category) {
    Preconditions.checkNotNull(paint);

    // At the moment, caller has responsibility to reset paint.
    // TODO(matsuzakit): move all style based logic from MozcDrawableFactory to here.
    paint.setStyle(Style.FILL);
    switch (category) {
      case STYLE_CATEGORY_KEYICON_MAIN:
        paint.setColor(keyIconMainColor);
        break;
      case STYLE_CATEGORY_KEYICON_GUIDE:
        paint.setColor(keyIconGuideColor);
        break;
      case STYLE_CATEGORY_KEYICON_GUIDE_LIGHT:
        paint.setColor(keyIconGuideLightColor);
        break;
      case STYLE_CATEGORY_KEYICON_MAIN_HIGHLIGHT:
        paint.setColor(keyIconMainHighlightColor);
        break;
      case STYLE_CATEGORY_KEYICON_GUIDE_HIGHLIGHT:
        paint.setColor(keyIconGuideHighlightColor);
        break;
      case STYLE_CATEGORY_KEYICON_BOUND:
        paint.setColor(keyIconBoundColor);
        paint.setStyle(Style.STROKE);
        break;
      case STYLE_CATEGORY_KEYICON_TWELVEKEYS_FUNCTION:
        paint.setColor(keyIconTwelvekeysFunctionColor);
        break;
      case STYLE_CATEGORY_KEYICON_TWELVEKEYS_GLOBE:
        paint.setColor(keyIconTwelvekeysGlobeColor);
        break;
      case STYLE_CATEGORY_KEYICON_QWERTY_FUNCTION:
        paint.setColor(keyIconQwertyFunctionColor);
        break;
      case STYLE_CATEGORY_KEYICON_FUNCTION_DARK:
        paint.setColor(keyIconFunctionDarkColor);
        break;
      case STYLE_CATEGORY_KEYICON_ENTER:
        paint.setColor(keyIconEnterColor);
        break;
      case STYLE_CATEGORY_KEYICON_ENTER_CIRCLE:
        paint.setColor(keyIconEnterCircleColor);
        break;
      case STYLE_CATEGORY_KEYICON_QWERTY_SHIFT_ON_ARROW:
        paint.setColor(keyIconQwertyShiftOnArrowColor);
        break;
      case STYLE_CATEGORY_KEYPOPUP_HIGHLIGHT:
        paint.setColor(keyPopupHighlightColor);
        break;
      case STYLE_CATEGORY_SYMBOL_MAJOR:
        paint.setColor(symbolMajorColor);
        break;
      case STYLE_CATEGORY_SYMBOL_MAJOR_SELECTED:
        paint.setColor(symbolMajorSelectedColor);
        break;
      case STYLE_CATEGORY_SYMBOL_MAJOR_EMOJI_DISABLE_CIRCLE:
        paint.setStyle(Style.STROKE);
        paint.setColor(symbolMajorColor);
        paint.setStrokeWidth(0.75f);
        paint.setStrokeMiter(10);
        break;
      case STYLE_CATEGORY_SYMBOL_MINOR:
        paint.setColor(symbolMinorColor);
        break;
      case STYLE_CATEGORY_SYMBOL_MINOR_SELECTED:
        paint.setColor(symbolMinorSelectedColor);
        break;
      case STYLE_CATEGORY_KEYBOARD_FOLDING_BUTTON_BACKGROUND_DEFAULT:
        paint.setColor(keyboardFoldingButtonBackgroundDefaultColor);
        break;
      case STYLE_CATEGORY_KEYBOARD_FOLDING_BUTTON_BACKGROUND_SCROLLED:
        paint.setColor(keyboardFoldingButtonBackgroundScrolledColor);
        break;
      default:
        throw new IllegalStateException("Unknown category: " + category);
    }
  }

  public Drawable getDrawable(Resources resources, int resourceId) {
    Preconditions.checkNotNull(resources);
    return getDrawableFactory(resources).getDrawable(resourceId).or(DummyDrawable.getInstance());
  }

  private MozcDrawableFactory getDrawableFactory(Resources resources) {
    if (!drawableFactory.isPresent()) {
      drawableFactory = Optional.of(new MozcDrawableFactory(resources, this));
    }
    return drawableFactory.get();
  }

  @VisibleForTesting
  void resetDrawableFactory() {
    drawableFactory = Optional.absent();
  }
}
