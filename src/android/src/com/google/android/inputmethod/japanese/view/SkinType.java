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

package org.mozc.android.inputmethod.japanese.view;

import org.mozc.android.inputmethod.japanese.resources.R;

import android.graphics.Paint;
import android.graphics.Paint.Style;

/**
 * Type of skins.
 */
public enum SkinType {
  ORANGE_LIGHTGRAY(
      // 12keys layout regular released key config.
      0xFFF5F5F5, 0xFFD2D2D2, 0xFFFAFAFA, 0xFFAFAFAF, 0xFF909090, 0xFF1E1E1E,

      // 12keys layout regular pressed key config.
      0xFFAAAAAA, 0xFF828282, 0, 0, 0, 0,

      // 12keys layout function released key config.
      0xFF858087, 0xFF67645F, 0xFF898588, 0xFF5C5759, 0xFF555555, 0xFF1E1E1E,

      // 12keys layout regular pressed key config.
      0xFFBFBFBD, 0xFFF7F5EC, 0, 0, 0, 0,

      // Qwerty layout regular released key config.
      0xFFF5F5F5, 0xFFD2D2D2, 0, 0xFF1E1E1E,

      // Qwerty layout regular pressed key config.
      0xFFCCCCCC, 0xFF797979, 0, 0xFF1E1E1E,

      // Qwerty layout function released key config.
      0xFF858087, 0xFF67645F, 0, 0xFF1E1E1E,

      // Qwerty layout function pressed key config.
      0xFFE9E4E4, 0xFFB2ADAD, 0, 0xFF1E1E1E,

      // Flick color config.
      // According to the original design mock, the base color is orange for now.
      // The shade is 25% alpha-black. The following value is offline-calculated to mix base color.
      0xFFFF9A28, 0xFFC0741e,

      // Qwerty light on/off sign config.
      0xFFFFCC00, 0xFFFF9900, 0xFFFF9966, 0xFF333333, 0xFF333333, 0xFF000000,

      // Qwerty round coner radius.
      3.5f,

      // Framed popup window config.
      0xFFFFB53A, 0xFFFF7A25, 0xFF793D00, 0xFFFFFFFF, 0,

      // Candidate scroll indicator config.
      0xFFFFE39D, 0xFFFFCC33,

      // Candidate background config.
      0xFFE8E8E8, 0xFFDCDCDC, 0xFFFFFFFF, 0xFF8C8C8C,

      // Candidate focused background config.
      0xFFFFC142, 0xFFFFe096, 0x40000000,

      // Symbol function released key config.
      0xFF858087, 0xFF67645F, 0, 0xFF1E1E1E,

      // Symbol function pressed key config.
      0xFFE9E4E4, 0xFFB2ADAD, 0, 0xFF1E1E1E,

      // Symbol scroll indicator config.
      0xFFFFCC00, 0xFFFF9C00,

      // Symbol minor category tab config.
      0xFFFF9200, 0x80FF9200,

      // Symbol candidate background config.
      0xFFFEFEFE, 0xFFECECEC, 0xFFFFFFFF, 0x7C666666,

      // Window background resource.
      R.drawable.window__background) {
    @Override
    public void apply(Paint paint, int category) {
      // At the moment, caller has responsibility to reset paint.
      // TODO(hidehiko): move all style based logic from MozcDrawableFactory to here.
      switch (category) {
        case STYLE_CATEGORY_KEYICON_MAIN:
        case STYLE_CATEGORY_KEYICON_FUNCTION_DARK:
        case STYLE_CATEGORY_KEYICON_POPUP_FUNCTION:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFF272727);
          break;
        case STYLE_CATEGORY_KEYICON_POPUP_FUNCTION_DARK:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFFFFFFFF);
          paint.setShadowLayer(0.5f, 0.f, -1.f, 0xFF404040);
          break;
        case STYLE_CATEGORY_KEYICON_GUIDE:
        case STYLE_CATEGORY_SYMBOL_MAJOR:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFF333333);
          break;
        case STYLE_CATEGORY_KEYICON_GUIDE_LIGHT:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFF999999);
          break;
        case STYLE_CATEGORY_KEYICON_MAIN_HIGHLIGHT:
        case STYLE_CATEGORY_KEYICON_GUIDE_HIGHLIGHT:
        case STYLE_CATEGORY_SYMBOL_MAJOR_SELECTED:
        case STYLE_CATEGORY_SYMBOL_MINOR:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFFFFFFFF);
          break;
        case STYLE_CATEGORY_KEYICON_BOUND:
          paint.setStyle(Style.STROKE);
          paint.setColor(0xFFCCCCCC);
          break;
        case STYLE_CATEGORY_KEYICON_FUNCTION:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFFDDDDDD);
          paint.setShadowLayer(2.f, 0.f, -1.f, 0xFF404040);
          break;
        case SkinType.STYLE_CATEGORY_KEYICON_QWERTY_SHIFT_ON_ARROW:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFFFFB005);
          paint.setShadowLayer(2.f, 0.f, -1.f, 0xFF404040);
          break;
        case SkinType.STYLE_CATEGORY_KEYICON_QWERTY_CAPS_ON_ARROW:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFFC1F300);
          paint.setShadowLayer(2.f, 0.f, -1.f, 0xFF404040);
          break;
        case STYLE_CATEGORY_KEYPOPUP_HIGHLIGHT:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFFF7982d);
          break;
        case STYLE_CATEGORY_SYMBOL_MINOR_SELECTED:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFFFF9a28);
          break;
        case STYLE_CATEGORY_KEYBOARD_FOLDING_BUTTON_BACKGROUND:
          paint.setStyle(Style.FILL);
          paint.setColor(0x8C676767);
          break;
        default:
          throw new IllegalStateException("Unknown category: " + category);
      }
    }
  },

  BLUE_LIGHTGRAY(
      // 12keys layout regular released key config.
      0xFFF5F5F5, 0xFFD2D2D2, 0xFFFAFAFA, 0xFFAFAFAF, 0xFF909090, 0xFF1E1E1E,

      // 12keys layout regular pressed key config.
      0xFFAAAAAA, 0xFF828282, 0, 0, 0, 0,

      // 12keys layout function released key config.
      0xFF858087, 0xFF67645F, 0xFF898588, 0xFF5C5759, 0xFF555555, 0xFF1E1E1E,

      // 12keys layout regular pressed key config.
      0xFFBFBFBD, 0xFFF7F5EC, 0, 0, 0, 0,

      // Qwerty layout regular released key config.
      0xFFF5F5F5, 0xFFD2D2D2, 0, 0xFF1E1E1E,

      // Qwerty layout regular pressed key config.
      0xFFCCCCCC, 0xFF797979, 0, 0xFF1E1E1E,

      // Qwerty layout function released key config.
      0xFF858087, 0xFF67645F, 0, 0xFF1E1E1E,

      // Qwerty layout function pressed key config.
      0xFFE9E4E4, 0xFFB2ADAD, 0, 0xFF1E1E1E,

      // Flick color config.
      // According to the original design mock, the base color is orange for now.
      // The shade is 25% alpha-black. The following value is offline-calculated to mix base color.
      0xFF55C6EE, 0xFF3F94B2,

      // Qwerty light on/off sign config.
      0xFF73DAF7, 0xFF3393E5, 0xFF53B6EE, 0xFF333333, 0xFF333333, 0xFF000000,

      // Qwerty round coner radius.
      3.5f,

      // Framed popup window config.
      0xFFFBFBFB, 0xFFEAEAEA, 0, 0, 0xFF1E1E1E,

      // Candidate scroll indicator config.
      0xFF73DAF7, 0xFF3393E5,

      // Candidate background config.
      0xFFE8E8E8, 0xFFDCDCDC, 0xFFFFFFFF, 0xFF8C8C8C,

      // Candidate focused background config.
      0xFFA5DDF6, 0xFFC1E9F5, 0x40000000,

      // Symbol function released key config.
      0xFF858087, 0xFF67645F, 0, 0xFF1E1E1E,

      // Symbol function pressed key config.
      0xFFE9E4E4, 0xFFB2ADAD, 0, 0xFF1E1E1E,

      // Symbol scroll indicator config.
      0xFF73DAF7, 0xFF53B6EE,

      // Symbol minor category tab config.
      0xFF33B5E5, 0x8033B5E5,

      // Symbol candidate background config.
      0xFFFEFEFE, 0xFFECECEC, 0xFFFFFFFF, 0x7C666666,

      // Window background resource.
      R.drawable.window__background) {
    @Override
    public void apply(Paint paint, int category) {
      // At the moment, caller has responsibility to reset paint.
      // TODO(hidehiko): move all style based logic from MozcDrawableFactory to here.
      switch (category) {
        case STYLE_CATEGORY_KEYICON_MAIN:
        case STYLE_CATEGORY_KEYICON_FUNCTION_DARK:
        case STYLE_CATEGORY_KEYICON_POPUP_FUNCTION:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFF272727);
          break;
        case STYLE_CATEGORY_KEYICON_POPUP_FUNCTION_DARK:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFFFFFFFF);
          paint.setShadowLayer(0.5f, 0.f, -1.f, 0xFF404040);
          break;
        case STYLE_CATEGORY_KEYICON_GUIDE:
        case STYLE_CATEGORY_SYMBOL_MAJOR:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFF333333);
          break;
        case STYLE_CATEGORY_KEYICON_GUIDE_LIGHT:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFF999999);
          break;
        case STYLE_CATEGORY_KEYICON_MAIN_HIGHLIGHT:
        case STYLE_CATEGORY_KEYICON_GUIDE_HIGHLIGHT:
        case STYLE_CATEGORY_SYMBOL_MAJOR_SELECTED:
        case STYLE_CATEGORY_SYMBOL_MINOR:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFFFFFFFF);
          break;
        case STYLE_CATEGORY_KEYICON_BOUND:
          paint.setStyle(Style.STROKE);
          paint.setColor(0xFFCCCCCC);
          break;
        case STYLE_CATEGORY_KEYICON_FUNCTION:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFFDDDDDD);
          paint.setShadowLayer(2.f, 0.f, -1.f, 0xFF404040);
          break;
        case SkinType.STYLE_CATEGORY_KEYICON_QWERTY_SHIFT_ON_ARROW:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFF55C6EE);
          paint.setShadowLayer(2.f, 0.f, -1.f, 0xFF404040);
          break;
        case SkinType.STYLE_CATEGORY_KEYICON_QWERTY_CAPS_ON_ARROW:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFFC1F300);
          paint.setShadowLayer(2.f, 0.f, -1.f, 0xFF404040);
          break;
        case STYLE_CATEGORY_KEYPOPUP_HIGHLIGHT:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFF57B8E5);
          break;
        case STYLE_CATEGORY_SYMBOL_MINOR_SELECTED:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFF63CFFF);
          break;
        case STYLE_CATEGORY_KEYBOARD_FOLDING_BUTTON_BACKGROUND:
          paint.setStyle(Style.FILL);
          paint.setColor(0x8C676767);
          break;
        default:
          throw new IllegalStateException("Unknown category: " + category);
      }
    }
  },

  BLUE_DARKGRAY(
      // 12keys layout regular released key config.
      0xFF656565, 0xFF656565, 0xFF8E8E8E, 0xFF535353, 0xFF535353, 0xFF1E1E1E,

      // 12keys layout regular pressed key config.
      0xFFAAAAAA, 0xFF828282, 0, 0, 0, 0,

      // 12keys layout function released key config.
      0xFF222222, 0xFF222222, 0xFF535353, 0xFF000000, 0xFF000000, 0xFF1E1E1E,

      // 12keys layout function pressed key config.
      0xFF066696, 0xDD066696, 0, 0, 0, 0,

      // Qwerty layout regular released key config.
      0xFF656565, 0xFF656565, 0xFF8E8E8E, 0xFF1E1E1E,

      // Qwerty layout regular pressed key config.
      0xFF066696, 0xDD066696, 0, 0xFF1E1E1E,

      // Qwerty layout function released key config.
      0xFF222222, 0xFF222222, 0xFF535353, 0xFF1E1E1E,

      // Qwerty layout function pressed key config.
      0xFF066696, 0xDD066696, 0, 0xFF1E1E1E,

      // Flick color config.
      // According to the original design mock, the base color is orange for now.
      // The shade is 25% alpha-black. The following value is offline-calculated to mix base color.
      0xFF1B78A3, 0xFF145A7A,

      // Qwerty light on/off sign config.
      0xFF73DAF7, 0xFF3393E5, 0xFF53B6EE, 0xFF333333, 0xFF333333, 0xFF000000,

      // Qwerty round corner radius.
      2.0f,

      // Framed popup window config.
      0xFF066696, 0xDD066696, 0, 0, 0xFF1E1E1E,

      // Candidate scroll indicator config.
      0xFF73DAF7, 0xFF3393E5,

      // Candidate background config.
      0xFFE8E8E8, 0xFFDCDCDC, 0xFFFFFFFF, 0xFF8C8C8C,

      // Candidate focused background config.
      0xFFA5DDF6, 0xFFC1E9F5, 0x40000000,

      // Symbol function released key config.
      0xFF656565, 0xFF656565, 0xFF8E8E8E, 0xFF1E1E1E,

      // Symbol function pressed key config.
      0xFF066696, 0xDD066696, 0, 0xFF1E1E1E,

      // Symbol scroll indicator config.
      0xFF73DAF7, 0xFF53B6EE,

      // Symbol minor category tab config.
      0xFF33B5E5, 0x8033B5E5,

      // Symbol candidate background config.
      0xFFFEFEFE, 0xFFECECEC, 0xFFFFFFFF, 0x7C666666,

      // Window background resource.
      R.drawable.window_background_black) {
    @Override
    public void apply(Paint paint, int category) {
      // At the moment, caller has responsibility to reset paint.
      // TODO(hidehiko): move all style based logic from MozcDrawableFactory to here.
      switch (category) {
        case STYLE_CATEGORY_KEYICON_MAIN:
        case STYLE_CATEGORY_KEYICON_POPUP_FUNCTION:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFFFFFFFF);
          break;
        case STYLE_CATEGORY_KEYICON_FUNCTION_DARK:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFF868686);
          break;
        case STYLE_CATEGORY_KEYICON_GUIDE:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFFBEBEBE);
          break;
        case STYLE_CATEGORY_KEYICON_GUIDE_LIGHT:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFF999999);
          break;
        case STYLE_CATEGORY_KEYICON_MAIN_HIGHLIGHT:
        case STYLE_CATEGORY_KEYICON_GUIDE_HIGHLIGHT:
        case STYLE_CATEGORY_SYMBOL_MAJOR_SELECTED:
        case STYLE_CATEGORY_SYMBOL_MINOR:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFFFFFFFF);
          break;
        case STYLE_CATEGORY_KEYICON_BOUND:
          paint.setStyle(Style.STROKE);
          paint.setColor(0xFFCCCCCC);
          break;
        case STYLE_CATEGORY_KEYICON_FUNCTION:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFFF7F7F7);
          paint.setShadowLayer(2.f, 0.f, -1.f, 0xFF404040);
          break;
        case SkinType.STYLE_CATEGORY_KEYICON_QWERTY_SHIFT_ON_ARROW:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFF55C6EE);
          paint.setShadowLayer(2.f, 0.f, -1.f, 0xFF404040);
          break;
        case SkinType.STYLE_CATEGORY_KEYICON_QWERTY_CAPS_ON_ARROW:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFFC1F300);
          paint.setShadowLayer(2.f, 0.f, -1.f, 0xFF404040);
          break;
        case STYLE_CATEGORY_KEYPOPUP_HIGHLIGHT:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFF4DA7CF);
          break;
        case STYLE_CATEGORY_SYMBOL_MAJOR:
        case STYLE_CATEGORY_KEYICON_POPUP_FUNCTION_DARK:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFF333333);
          break;
        case STYLE_CATEGORY_SYMBOL_MINOR_SELECTED:
          paint.setStyle(Style.FILL);
          paint.setColor(0xFF63CFFF);
          break;
        case STYLE_CATEGORY_KEYBOARD_FOLDING_BUTTON_BACKGROUND:
          paint.setStyle(Style.FILL);
          paint.setColor(0x8C676767);
          break;
        default:
          throw new IllegalStateException("Unknown category: " + category);
      }
    }
  },

  // This is an instance for testing of skin support in some classes.
  // TODO(hidehiko): remove this value when other skin types (described in above TODO)
  // are supported.
  TEST(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) {
    @Override
    public void apply(Paint paint, int category) {
      // Do nothing.
    }
  }
  ;

  public static final int STYLE_CATEGORY_KEYICON_MAIN = 0;
  public static final int STYLE_CATEGORY_KEYICON_GUIDE = 1;
  public static final int STYLE_CATEGORY_KEYICON_GUIDE_LIGHT = 2;
  public static final int STYLE_CATEGORY_KEYICON_MAIN_HIGHLIGHT = 3;
  public static final int STYLE_CATEGORY_KEYICON_GUIDE_HIGHLIGHT = 4;
  public static final int STYLE_CATEGORY_KEYICON_BOUND = 5;
  public static final int STYLE_CATEGORY_KEYICON_FUNCTION = 6;
  public static final int STYLE_CATEGORY_KEYICON_FUNCTION_DARK = 7;
  public static final int STYLE_CATEGORY_KEYICON_QWERTY_SHIFT_ON_ARROW = 8;
  public static final int STYLE_CATEGORY_KEYICON_QWERTY_CAPS_ON_ARROW = 9;
  public static final int STYLE_CATEGORY_KEYPOPUP_HIGHLIGHT = 10;
  public static final int STYLE_CATEGORY_KEYICON_POPUP_FUNCTION = 11;
  public static final int STYLE_CATEGORY_KEYICON_POPUP_FUNCTION_DARK = 12;
  public static final int STYLE_CATEGORY_SYMBOL_MAJOR = 13;
  public static final int STYLE_CATEGORY_SYMBOL_MAJOR_SELECTED = 14;
  public static final int STYLE_CATEGORY_SYMBOL_MINOR = 15;
  public static final int STYLE_CATEGORY_SYMBOL_MINOR_SELECTED = 16;
  public static final int STYLE_CATEGORY_KEYBOARD_FOLDING_BUTTON_BACKGROUND = 17;

  public final int twelvekeysLayoutReleasedKeyTopColor;
  public final int twelvekeysLayoutReleasedKeyBottomColor;
  public final int twelvekeysLayoutReleasedKeyHighlightColor;
  public final int twelvekeysLayoutReleasedKeyLightShadeColor;
  public final int twelvekeysLayoutReleasedKeyDarkShadeColor;
  public final int twelvekeysLayoutReleasedKeyShadowColor;

  public final int twelvekeysLayoutPressedKeyTopColor;
  public final int twelvekeysLayoutPressedKeyBottomColor;
  public final int twelvekeysLayoutPressedKeyHighlightColor;
  public final int twelvekeysLayoutPressedKeyLightShadeColor;
  public final int twelvekeysLayoutPressedKeyDarkShadeColor;
  public final int twelvekeysLayoutPressedKeyShadowColor;

  public final int twelvekeysLayoutReleasedFunctionKeyTopColor;
  public final int twelvekeysLayoutReleasedFunctionKeyBottomColor;
  public final int twelvekeysLayoutReleasedFunctionKeyHighlightColor;
  public final int twelvekeysLayoutReleasedFunctionKeyLightShadeColor;
  public final int twelvekeysLayoutReleasedFunctionKeyDarkShadeColor;
  public final int twelvekeysLayoutReleasedFunctionKeyShadowColor;

  public final int twelvekeysLayoutPressedFunctionKeyTopColor;
  public final int twelvekeysLayoutPressedFunctionKeyBottomColor;
  public final int twelvekeysLayoutPressedFunctionKeyHighlightColor;
  public final int twelvekeysLayoutPressedFunctionKeyLightShadeColor;
  public final int twelvekeysLayoutPressedFunctionKeyDarkShadeColor;
  public final int twelvekeysLayoutPressedFunctionKeyShadowColor;

  public final int qwertyLayoutReleasedKeyTopColor;
  public final int qwertyLayoutReleasedKeyBottomColor;
  public final int qwertyLayoutReleasedKeyHighlightColor;
  public final int qwertyLayoutReleasedKeyShadowColor;

  public final int qwertyLayoutPressedKeyTopColor;
  public final int qwertyLayoutPressedKeyBottomColor;
  public final int qwertyLayoutPressedKeyHighlightColor;
  public final int qwertyLayoutPressedKeyShadowColor;

  public final int qwertyLayoutReleasedFunctionKeyTopColor;
  public final int qwertyLayoutReleasedFunctionKeyBottomColor;
  public final int qwertyLayoutReleasedFunctionKeyHighlightColor;
  public final int qwertyLayoutReleasedFunctionKeyShadowColor;

  public final int qwertyLayoutPressedFunctionKeyTopColor;
  public final int qwertyLayoutPressedFunctionKeyBottomColor;
  public final int qwertyLayoutPressedFunctionKeyHighlightColor;
  public final int qwertyLayoutPressedFunctionKeyShadowColor;

  public final int flickBaseColor;
  public final int flickShadeColor;

  public final int qwertyLightOnSignLightColor;
  public final int qwertyLightOnSignDarkColor;
  public final int qwertyLightOnSignShadeColor;

  public final int qwertyLightOffSignLightColor;
  public final int qwertyLightOffSignDarkColor;
  public final int qwertyLightOffSignShadeColor;

  public final float qwertyKeyRoundRadius;

  public final int popupFrameWindowTopColor;
  public final int popupFrameWindowBottomColor;
  public final int popupFrameWindowBorderColor;
  public final int popupFrameWindowInnerPaneColor;
  public final int popupFrameWindowShadowColor;

  public final int candidateScrollBarTopColor;
  public final int candidateScrollBarBottomColor;
  public final int candidateBackgroundTopColor;
  public final int candidateBackgroundBottomColor;
  public final int candidateBackgroundHighlightColor;
  public final int candidateBackgroundBorderColor;
  public final int candidateBackgroundFocusedTopColor;
  public final int candidateBackgroundFocusedBottomColor;
  public final int candidateBackgroundFocusedShadowColor;

  public final int symbolReleasedFunctionKeyTopColor;
  public final int symbolReleasedFunctionKeyBottomColor;
  public final int symbolReleasedFunctionKeyHighlightColor;
  public final int symbolReleasedFunctionKeyShadowColor;

  public final int symbolPressedFunctionKeyTopColor;
  public final int symbolPressedFunctionKeyBottomColor;
  public final int symbolPressedFunctionKeyHighlightColor;
  public final int symbolPressedFunctionKeyShadowColor;

  public final int symbolScrollBarTopColor;
  public final int symbolScrollBarBottomColor;

  public final int symbolMinorCategoryTabSelectedColor;
  public final int symbolMinorCategoryTabPressedColor;

  public final int symbolCandidateBackgroundTopColor;
  public final int symbolCandidateBackgroundBottomColor;
  public final int symbolCandidateBackgroundHighlightColor;
  public final int symbolCandidateBackgroundBorderColor;

  public final int windowBackgroundResourceId;

  public abstract void apply(Paint paint, int category);

  private SkinType(
      int twelvekeysLayoutReleasedKeyTopColor,
      int twelvekeysLayoutReleasedKeyBottomColor,
      int twelvekeysLayoutReleasedKeyHighlightColor,
      int twelvekeysLayoutReleasedKeyLightShadeColor,
      int twelvekeysLayoutReleasedKeyDarkShadeColor,
      int twelvekeysLayoutReleasedKeyShadowColor,
      int twelvekeysLayoutPressedKeyTopColor,
      int twelvekeysLayoutPressedKeyBottomColor,
      int twelvekeysLayoutPressedKeyHighlightColor,
      int twelvekeysLayoutPressedKeyLightShadeColor,
      int twelvekeysLayoutPressedKeyDarkShadeColor,
      int twelvekeysLayoutPressedKeyShadowColor,
      int twelvekeysLayoutReleasedFunctionKeyTopColor,
      int twelvekeysLayoutReleasedFunctionKeyBottomColor,
      int twelvekeysLayoutReleasedFunctionKeyHighlightColor,
      int twelvekeysLayoutReleasedFunctionKeyLightShadeColor,
      int twelvekeysLayoutReleasedFunctionKeyDarkShadeColor,
      int twelvekeysLayoutReleasedFunctionKeyShadowColor,
      int twelvekeysLayoutPressedFunctionKeyTopColor,
      int twelvekeysLayoutPressedFunctionKeyBottomColor,
      int twelvekeysLayoutPressedFunctionKeyHighlightColor,
      int twelvekeysLayoutPressedFunctionKeyLightShadeColor,
      int twelvekeysLayoutPressedFunctionKeyDarkShadeColor,
      int twelvekeysLayoutPressedFunctionKeyShadowColor,
      int qwertyLayoutReleasedKeyTopColor,
      int qwertyLayoutReleasedKeyBottomColor,
      int qwertyLayoutReleasedKeyHighlightColor,
      int qwertyLayoutReleasedKeyShadowColor,
      int qwertyLayoutPressedKeyTopColor,
      int qwertyLayoutPressedKeyBottomColor,
      int qwertyLayoutPressedKeyHighlightColor,
      int qwertyLayoutPressedKeyShadowColor,
      int qwertyLayoutReleasedFunctionKeyTopColor,
      int qwertyLayoutReleasedFunctionKeyBottomColor,
      int qwertyLayoutReleasedFunctionKeyHighlightColor,
      int qwertyLayoutReleasedFunctionKeyShadowColor,
      int qwertyLayoutPressedFunctionKeyTopColor,
      int qwertyLayoutPressedFunctionKeyBottomColor,
      int qwertyLayoutPressedFunctionKeyHighlightColor,
      int qwertyLayoutPressedFunctionKeyShadowColor,
      int flickBaseColor,
      int flickShadeColor,
      int qwertyLightOnSignLightColor,
      int qwertyLightOnSignDarkColor,
      int qwertyLightOnSignShadeColor,
      int qwertyLightOffSignLightColor,
      int qwertyLightOffSignDarkColor,
      int qwertyLightOffSignShadeColor,
      float qwertyKeyRoundRadius,
      int popupFrameWindowTopColor,
      int popupFrameWindowBottomColor,
      int popupFrameWindowBorderColor,
      int popupFrameWindowInnerPaneColor,
      int popupFrameWindowShadowColor,
      int candidateScrollBarTopColor,
      int candidateScrollBarBottomColor,
      int candidateBackgroundTopColor,
      int candidateBackgroundBottomColor,
      int candidateBackgroundHighlightColor,
      int candidateBackgroundBorderColor,
      int candidateBackgroundFocusedTopColor,
      int candidateBackgroundFocusedBottomColor,
      int candidateBackgroundFocusedShadowColor,
      int symbolReleasedFunctionKeyTopColor,
      int symbolReleasedFunctionKeyBottomColor,
      int symbolReleasedFunctionKeyHighlightColor,
      int symbolReleasedFunctionKeyShadowColor,
      int symbolPressedFunctionKeyTopColor,
      int symbolPressedFunctionKeyBottomColor,
      int symbolPressedFunctionKeyHighlightColor,
      int symbolPressedFunctionKeyShadowColor,
      int symbolScrollBarTopColor,
      int symbolScrollBarBottomColor,
      int symbolMinorCategoryTabSelectedColor,
      int symbolMinorCategoryTabPressedColor,
      int symbolCandidateBackgroundTopColor,
      int symbolCandidateBackgroundBottomColor,
      int symbolCandidateBackgroundHighlightColor,
      int symbolCandidateBackgroundBorderColor,
      int windowBackgroundResourceId) {
    this.twelvekeysLayoutReleasedKeyTopColor = twelvekeysLayoutReleasedKeyTopColor;
    this.twelvekeysLayoutReleasedKeyBottomColor = twelvekeysLayoutReleasedKeyBottomColor;
    this.twelvekeysLayoutReleasedKeyHighlightColor = twelvekeysLayoutReleasedKeyHighlightColor;
    this.twelvekeysLayoutReleasedKeyLightShadeColor = twelvekeysLayoutReleasedKeyLightShadeColor;
    this.twelvekeysLayoutReleasedKeyDarkShadeColor = twelvekeysLayoutReleasedKeyDarkShadeColor;
    this.twelvekeysLayoutReleasedKeyShadowColor = twelvekeysLayoutReleasedKeyShadowColor;

    this.twelvekeysLayoutPressedKeyTopColor = twelvekeysLayoutPressedKeyTopColor;
    this.twelvekeysLayoutPressedKeyBottomColor = twelvekeysLayoutPressedKeyBottomColor;
    this.twelvekeysLayoutPressedKeyHighlightColor = twelvekeysLayoutPressedKeyHighlightColor;
    this.twelvekeysLayoutPressedKeyLightShadeColor = twelvekeysLayoutPressedKeyLightShadeColor;
    this.twelvekeysLayoutPressedKeyDarkShadeColor = twelvekeysLayoutPressedKeyDarkShadeColor;
    this.twelvekeysLayoutPressedKeyShadowColor = twelvekeysLayoutPressedKeyShadowColor;

    this.twelvekeysLayoutReleasedFunctionKeyTopColor = twelvekeysLayoutReleasedFunctionKeyTopColor;
    this.twelvekeysLayoutReleasedFunctionKeyBottomColor =
        twelvekeysLayoutReleasedFunctionKeyBottomColor;
    this.twelvekeysLayoutReleasedFunctionKeyHighlightColor =
        twelvekeysLayoutReleasedFunctionKeyHighlightColor;
    this.twelvekeysLayoutReleasedFunctionKeyLightShadeColor =
        twelvekeysLayoutReleasedFunctionKeyLightShadeColor;
    this.twelvekeysLayoutReleasedFunctionKeyDarkShadeColor =
        twelvekeysLayoutReleasedFunctionKeyDarkShadeColor;
    this.twelvekeysLayoutReleasedFunctionKeyShadowColor =
        twelvekeysLayoutReleasedFunctionKeyShadowColor;

    this.twelvekeysLayoutPressedFunctionKeyTopColor = twelvekeysLayoutPressedFunctionKeyTopColor;
    this.twelvekeysLayoutPressedFunctionKeyBottomColor =
        twelvekeysLayoutPressedFunctionKeyBottomColor;
    this.twelvekeysLayoutPressedFunctionKeyHighlightColor =
        twelvekeysLayoutPressedFunctionKeyHighlightColor;
    this.twelvekeysLayoutPressedFunctionKeyLightShadeColor =
        twelvekeysLayoutPressedFunctionKeyLightShadeColor;
    this.twelvekeysLayoutPressedFunctionKeyDarkShadeColor =
        twelvekeysLayoutPressedFunctionKeyDarkShadeColor;
    this.twelvekeysLayoutPressedFunctionKeyShadowColor =
        twelvekeysLayoutPressedFunctionKeyShadowColor;

    this.qwertyLayoutReleasedKeyTopColor = qwertyLayoutReleasedKeyTopColor;
    this.qwertyLayoutReleasedKeyBottomColor = qwertyLayoutReleasedKeyBottomColor;
    this.qwertyLayoutReleasedKeyHighlightColor = qwertyLayoutReleasedKeyHighlightColor;
    this.qwertyLayoutReleasedKeyShadowColor = qwertyLayoutReleasedKeyShadowColor;

    this.qwertyLayoutPressedKeyTopColor = qwertyLayoutPressedKeyTopColor;
    this.qwertyLayoutPressedKeyBottomColor = qwertyLayoutPressedKeyBottomColor;
    this.qwertyLayoutPressedKeyHighlightColor = qwertyLayoutPressedKeyHighlightColor;
    this.qwertyLayoutPressedKeyShadowColor = qwertyLayoutPressedKeyShadowColor;

    this.qwertyLayoutReleasedFunctionKeyTopColor = qwertyLayoutReleasedFunctionKeyTopColor;
    this.qwertyLayoutReleasedFunctionKeyBottomColor = qwertyLayoutReleasedFunctionKeyBottomColor;
    this.qwertyLayoutReleasedFunctionKeyHighlightColor = qwertyLayoutReleasedFunctionKeyHighlightColor;
    this.qwertyLayoutReleasedFunctionKeyShadowColor = qwertyLayoutReleasedFunctionKeyShadowColor;

    this.qwertyLayoutPressedFunctionKeyTopColor = qwertyLayoutPressedFunctionKeyTopColor;
    this.qwertyLayoutPressedFunctionKeyBottomColor = qwertyLayoutPressedFunctionKeyBottomColor;
    this.qwertyLayoutPressedFunctionKeyHighlightColor = qwertyLayoutPressedFunctionKeyHighlightColor;
    this.qwertyLayoutPressedFunctionKeyShadowColor = qwertyLayoutPressedFunctionKeyShadowColor;

    this.flickBaseColor = flickBaseColor;
    this.flickShadeColor = flickShadeColor;

    this.qwertyLightOnSignLightColor = qwertyLightOnSignLightColor;
    this.qwertyLightOnSignDarkColor = qwertyLightOnSignDarkColor;
    this.qwertyLightOnSignShadeColor = qwertyLightOnSignShadeColor;

    this.qwertyLightOffSignLightColor = qwertyLightOffSignLightColor;
    this.qwertyLightOffSignDarkColor = qwertyLightOffSignDarkColor;
    this.qwertyLightOffSignShadeColor = qwertyLightOffSignShadeColor;

    this.qwertyKeyRoundRadius = qwertyKeyRoundRadius;

    this.popupFrameWindowTopColor = popupFrameWindowTopColor;
    this.popupFrameWindowBottomColor = popupFrameWindowBottomColor;
    this.popupFrameWindowBorderColor = popupFrameWindowBorderColor;
    this.popupFrameWindowInnerPaneColor = popupFrameWindowInnerPaneColor;
    this.popupFrameWindowShadowColor = popupFrameWindowShadowColor;

    this.candidateScrollBarTopColor = candidateScrollBarTopColor;
    this.candidateScrollBarBottomColor = candidateScrollBarBottomColor;
    this.candidateBackgroundTopColor = candidateBackgroundTopColor;
    this.candidateBackgroundBottomColor = candidateBackgroundBottomColor;
    this.candidateBackgroundHighlightColor = candidateBackgroundHighlightColor;
    this.candidateBackgroundBorderColor = candidateBackgroundBorderColor;
    this.candidateBackgroundFocusedTopColor = candidateBackgroundFocusedTopColor;
    this.candidateBackgroundFocusedBottomColor = candidateBackgroundFocusedBottomColor;
    this.candidateBackgroundFocusedShadowColor = candidateBackgroundFocusedShadowColor;

    this.symbolReleasedFunctionKeyTopColor = symbolReleasedFunctionKeyTopColor;
    this.symbolReleasedFunctionKeyBottomColor = symbolReleasedFunctionKeyBottomColor;
    this.symbolReleasedFunctionKeyHighlightColor = symbolReleasedFunctionKeyHighlightColor;
    this.symbolReleasedFunctionKeyShadowColor = symbolReleasedFunctionKeyShadowColor;
    this.symbolPressedFunctionKeyTopColor = symbolPressedFunctionKeyTopColor;
    this.symbolPressedFunctionKeyBottomColor = symbolPressedFunctionKeyBottomColor;
    this.symbolPressedFunctionKeyHighlightColor = symbolPressedFunctionKeyHighlightColor;
    this.symbolPressedFunctionKeyShadowColor = symbolPressedFunctionKeyShadowColor;

    this.symbolScrollBarTopColor = symbolScrollBarTopColor;
    this.symbolScrollBarBottomColor = symbolScrollBarBottomColor;

    this.symbolMinorCategoryTabSelectedColor = symbolMinorCategoryTabSelectedColor;
    this.symbolMinorCategoryTabPressedColor = symbolMinorCategoryTabPressedColor;

    this.symbolCandidateBackgroundTopColor = symbolCandidateBackgroundTopColor;
    this.symbolCandidateBackgroundBottomColor = symbolCandidateBackgroundBottomColor;
    this.symbolCandidateBackgroundHighlightColor = symbolCandidateBackgroundHighlightColor;
    this.symbolCandidateBackgroundBorderColor = symbolCandidateBackgroundBorderColor;

    this.windowBackgroundResourceId = windowBackgroundResourceId;
  }
}
