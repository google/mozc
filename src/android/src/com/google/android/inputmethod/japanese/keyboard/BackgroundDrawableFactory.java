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

package org.mozc.android.inputmethod.japanese.keyboard;

import org.mozc.android.inputmethod.japanese.view.CandidateBackgroundDrawable;
import org.mozc.android.inputmethod.japanese.view.CandidateBackgroundFocusedDrawable;
import org.mozc.android.inputmethod.japanese.view.CenterCircularHighlightDrawable;
import org.mozc.android.inputmethod.japanese.view.LightIconDrawable;
import org.mozc.android.inputmethod.japanese.view.PopUpFrameWindowDrawable;
import org.mozc.android.inputmethod.japanese.view.RectKeyDrawable;
import org.mozc.android.inputmethod.japanese.view.RoundRectKeyDrawable;
import org.mozc.android.inputmethod.japanese.view.SkinType;
import org.mozc.android.inputmethod.japanese.view.TriangularHighlightDrawable;
import org.mozc.android.inputmethod.japanese.view.TriangularHighlightDrawable.HighlightDirection;

import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.StateListDrawable;

import java.util.EnumMap;
import java.util.Map;

/**
 * Utilities for {@code Drawable} instances of key background.
 *
 * Currently we supports following Drawables.
 * <ul>
 * <li> Regular character key for twelvekeys layout.
 * <li> Function key for twelvekeys layout.
 * <li> Circular highlight for twelvekeys representing "center" flicking.
 * <li> Triangle highlight for twelvekeys representing directional flicking.
 * <li> Regular character key for qwerty.
 * <li> Function key for qwerty.
 * <li> Function key with "light" mark at top-right corner, for alt key on qwerty layout.
 * </ul>
 *
 * The highlight design is:
 * <ul>
 * <li> put a circle highlight with inner shadow for just pressing a key.
 * <li> put a triangle highlight with inner shadow for some directions.
 * </ul>
 *
 */
public class BackgroundDrawableFactory {
  /**
   * Drawable to create.
   */
  public enum DrawableType {
    // Key background for twelvekeys layout.
    TWELVEKEYS_REGULAR_KEY_BACKGROUND,
    TWELVEKEYS_FUNCTION_KEY_BACKGROUND,

    // Key background for qwerty layout.
    QWERTY_REGULAR_KEY_BACKGROUND,
    QWERTY_FUNCTION_KEY_BACKGROUND,
    QWERTY_FUNCTION_KEY_LIGHT_ON_BACKGROUND,
    QWERTY_FUNCTION_KEY_LIGHT_OFF_BACKGROUND,

    // Highlight for flicking.
    TWELVEKEYS_CENTER_FLICK,
    TWELVEKEYS_LEFT_FLICK,
    TWELVEKEYS_UP_FLICK,
    TWELVEKEYS_RIGHT_FLICK,
    TWELVEKEYS_DOWN_FLICK,

    // Background drawable for PopUp window.
    POPUP_BACKGROUND_WINDOW,

    CANDIDATE_BACKGROUND,
    SYMBOL_CANDIDATE_BACKGROUND,
  }

  private static final int [] EMPTY_STATE = new int[] {};

  // According to the original design mock, the pressed key background contains some padding.
  // Here are hardcoded parameters.
  private static final float TWELVEKEYS_LEFT_OFFSET = 0.5f;
  private static final float TWELVEKEYS_TOP_OFFSET = 0;
  private static final float TWELVEKEYS_RIGHT_OFFSET = 1.0f;
  private static final float TWELVEKEYS_BOTTOM_OFFSET = 1.0f;

  private static final float QWERTY_LEFT_OFFSET = 2.0f;
  private static final float QWERTY_TOP_OFFSET = 1.0f;
  private static final float QWERTY_RIGHT_OFFSET = 2.0f;
  private static final float QWERTY_BOTTOM_OFFSET = 3.0f;
  private static final float QWERTY_LIGHT_TOP_OFFSET = 2.0f;
  private static final float QWERTY_LIGHT_RIGHT_OFFSET = 2.0f;
  private static final float QWERTY_LIGHT_RADIUS = 2.25f;

  private static final float POPUP_WINDOW_PADDING = 7.0f;
  private static final float POPUP_FRAME_BORDER_WIDTH = 5.0f;
  private static final float POPUP_SHADOW_SIZE = 1.5f;

  private static final float CANDIDATE_BACKGROUND_PADDING = 0.0f;
  private static final float SYMBOL_CANDIDATE_BACKGROUND_PADDING = 0.0f;

  private final float density;
  private final Map<DrawableType, Drawable> drawableMap =
      new EnumMap<DrawableType, Drawable>(DrawableType.class);
  private SkinType skinType = SkinType.ORANGE_LIGHTGRAY;

  public BackgroundDrawableFactory(float density) {
    this.density = density;
  }

  public Drawable getDrawable(DrawableType drawableType) {
    if (drawableType == null) {
      return null;
    }

    Drawable result = drawableMap.get(drawableType);
    if (result == null) {
      result = createBackgroundDrawable(drawableType);
      drawableMap.put(drawableType, result);
    }

    return result;
  }

  public void setSkinType(SkinType skinType) {
    if (this.skinType == skinType) {
      return;
    }
    this.skinType = skinType;
    drawableMap.clear();
  }

  private Drawable createBackgroundDrawable(DrawableType drawableType) {
    switch (drawableType) {
      case TWELVEKEYS_REGULAR_KEY_BACKGROUND:
        return createPressableDrawable(
            new RectKeyDrawable(
                (int) (TWELVEKEYS_LEFT_OFFSET * density),
                (int) (TWELVEKEYS_TOP_OFFSET * density),
                (int) (TWELVEKEYS_RIGHT_OFFSET * density),
                (int) (TWELVEKEYS_BOTTOM_OFFSET * density),
                skinType.twelvekeysLayoutPressedKeyTopColor,
                skinType.twelvekeysLayoutPressedKeyBottomColor,
                skinType.twelvekeysLayoutPressedKeyHighlightColor,
                skinType.twelvekeysLayoutPressedKeyLightShadeColor,
                skinType.twelvekeysLayoutPressedKeyDarkShadeColor,
                skinType.twelvekeysLayoutPressedKeyShadowColor),
            new RectKeyDrawable(
                (int) (TWELVEKEYS_LEFT_OFFSET * density),
                (int) (TWELVEKEYS_TOP_OFFSET * density),
                (int) (TWELVEKEYS_RIGHT_OFFSET * density),
                (int) (TWELVEKEYS_BOTTOM_OFFSET * density),
                skinType.twelvekeysLayoutReleasedKeyTopColor,
                skinType.twelvekeysLayoutReleasedKeyBottomColor,
                skinType.twelvekeysLayoutReleasedKeyHighlightColor,
                skinType.twelvekeysLayoutReleasedKeyLightShadeColor,
                skinType.twelvekeysLayoutReleasedKeyDarkShadeColor,
                skinType.twelvekeysLayoutReleasedKeyShadowColor));

      case TWELVEKEYS_FUNCTION_KEY_BACKGROUND:
        return createPressableDrawable(
            new RectKeyDrawable(
                (int) (TWELVEKEYS_LEFT_OFFSET * density),
                (int) (TWELVEKEYS_TOP_OFFSET * density),
                (int) (TWELVEKEYS_RIGHT_OFFSET * density),
                (int) (TWELVEKEYS_BOTTOM_OFFSET * density),
                skinType.twelvekeysLayoutPressedFunctionKeyTopColor,
                skinType.twelvekeysLayoutPressedFunctionKeyBottomColor,
                skinType.twelvekeysLayoutPressedFunctionKeyHighlightColor,
                skinType.twelvekeysLayoutPressedFunctionKeyLightShadeColor,
                skinType.twelvekeysLayoutPressedFunctionKeyDarkShadeColor,
                skinType.twelvekeysLayoutPressedFunctionKeyShadowColor),
            new RectKeyDrawable(
                (int) (TWELVEKEYS_LEFT_OFFSET * density),
                (int) (TWELVEKEYS_TOP_OFFSET * density),
                (int) (TWELVEKEYS_RIGHT_OFFSET * density),
                (int) (TWELVEKEYS_BOTTOM_OFFSET * density),
                skinType.twelvekeysLayoutReleasedFunctionKeyTopColor,
                skinType.twelvekeysLayoutReleasedFunctionKeyBottomColor,
                skinType.twelvekeysLayoutReleasedFunctionKeyHighlightColor,
                skinType.twelvekeysLayoutReleasedFunctionKeyLightShadeColor,
                skinType.twelvekeysLayoutReleasedFunctionKeyDarkShadeColor,
                skinType.twelvekeysLayoutReleasedFunctionKeyShadowColor));

      case QWERTY_REGULAR_KEY_BACKGROUND:
        return createPressableDrawable(
            new RoundRectKeyDrawable(
                (int) (QWERTY_LEFT_OFFSET * density),
                (int) (QWERTY_TOP_OFFSET * density),
                (int) (QWERTY_RIGHT_OFFSET * density),
                (int) (QWERTY_BOTTOM_OFFSET * density),
                (int) (skinType.qwertyKeyRoundRadius * density),
                skinType.qwertyLayoutPressedKeyTopColor,
                skinType.qwertyLayoutPressedKeyBottomColor,
                skinType.qwertyLayoutPressedKeyHighlightColor,
                skinType.qwertyLayoutPressedKeyShadowColor),
            new RoundRectKeyDrawable(
                (int) (QWERTY_LEFT_OFFSET * density),
                (int) (QWERTY_TOP_OFFSET * density),
                (int) (QWERTY_RIGHT_OFFSET * density),
                (int) (QWERTY_BOTTOM_OFFSET * density),
                (int) (skinType.qwertyKeyRoundRadius * density),
                skinType.qwertyLayoutReleasedKeyTopColor,
                skinType.qwertyLayoutReleasedKeyBottomColor,
                skinType.qwertyLayoutReleasedKeyHighlightColor,
                skinType.qwertyLayoutReleasedKeyShadowColor));

      case QWERTY_FUNCTION_KEY_BACKGROUND:
        return createPressableDrawable(
            new RoundRectKeyDrawable(
                (int) (QWERTY_LEFT_OFFSET * density),
                (int) (QWERTY_TOP_OFFSET * density),
                (int) (QWERTY_RIGHT_OFFSET * density),
                (int) (QWERTY_BOTTOM_OFFSET * density),
                (int) (skinType.qwertyKeyRoundRadius * density),
                skinType.qwertyLayoutPressedFunctionKeyTopColor,
                skinType.qwertyLayoutPressedFunctionKeyBottomColor,
                skinType.qwertyLayoutPressedFunctionKeyHighlightColor,
                skinType.qwertyLayoutPressedFunctionKeyShadowColor),
            new RoundRectKeyDrawable(
                (int) (QWERTY_LEFT_OFFSET * density),
                (int) (QWERTY_TOP_OFFSET * density),
                (int) (QWERTY_RIGHT_OFFSET * density),
                (int) (QWERTY_BOTTOM_OFFSET * density),
                (int) (skinType.qwertyKeyRoundRadius * density),
                skinType.qwertyLayoutReleasedFunctionKeyTopColor,
                skinType.qwertyLayoutReleasedFunctionKeyBottomColor,
                skinType.qwertyLayoutReleasedFunctionKeyHighlightColor,
                skinType.qwertyLayoutReleasedFunctionKeyShadowColor));

      case QWERTY_FUNCTION_KEY_LIGHT_ON_BACKGROUND:
        return new LayerDrawable(new Drawable[] {
            createPressableDrawable(
                new RoundRectKeyDrawable(
                    (int) (QWERTY_LEFT_OFFSET * density),
                    (int) (QWERTY_TOP_OFFSET * density),
                    (int) (QWERTY_RIGHT_OFFSET * density),
                    (int) (QWERTY_BOTTOM_OFFSET * density),
                    (int) (skinType.qwertyKeyRoundRadius * density),
                    skinType.qwertyLayoutPressedFunctionKeyTopColor,
                    skinType.qwertyLayoutPressedFunctionKeyBottomColor,
                    skinType.qwertyLayoutPressedFunctionKeyHighlightColor,
                    skinType.qwertyLayoutPressedFunctionKeyShadowColor),
                new RoundRectKeyDrawable(
                    (int) (QWERTY_LEFT_OFFSET * density),
                    (int) (QWERTY_TOP_OFFSET * density),
                    (int) (QWERTY_RIGHT_OFFSET * density),
                    (int) (QWERTY_BOTTOM_OFFSET * density),
                    (int) (skinType.qwertyKeyRoundRadius * density),
                    skinType.qwertyLayoutReleasedFunctionKeyTopColor,
                    skinType.qwertyLayoutReleasedFunctionKeyBottomColor,
                    skinType.qwertyLayoutReleasedFunctionKeyHighlightColor,
                    skinType.qwertyLayoutReleasedFunctionKeyShadowColor)),
            new LightIconDrawable(
                (int) ((QWERTY_TOP_OFFSET + QWERTY_LIGHT_TOP_OFFSET + QWERTY_LIGHT_RADIUS)
                          * density),
                (int) ((QWERTY_RIGHT_OFFSET + QWERTY_LIGHT_RIGHT_OFFSET + QWERTY_LIGHT_RADIUS)
                          * density),
                skinType.qwertyLightOnSignLightColor,
                skinType.qwertyLightOnSignDarkColor,
                skinType.qwertyLightOnSignShadeColor,
                (int) (QWERTY_LIGHT_RADIUS * density))
            });

      case QWERTY_FUNCTION_KEY_LIGHT_OFF_BACKGROUND:
        return new LayerDrawable(new Drawable[] {
            createPressableDrawable(
                new RoundRectKeyDrawable(
                    (int) (QWERTY_LEFT_OFFSET * density),
                    (int) (QWERTY_TOP_OFFSET * density),
                    (int) (QWERTY_RIGHT_OFFSET * density),
                    (int) (QWERTY_BOTTOM_OFFSET * density),
                    (int) (skinType.qwertyKeyRoundRadius * density),
                    skinType.qwertyLayoutPressedFunctionKeyTopColor,
                    skinType.qwertyLayoutPressedFunctionKeyBottomColor,
                    skinType.qwertyLayoutPressedFunctionKeyHighlightColor,
                    skinType.qwertyLayoutPressedFunctionKeyShadowColor),
                new RoundRectKeyDrawable(
                    (int) (QWERTY_LEFT_OFFSET * density),
                    (int) (QWERTY_TOP_OFFSET * density),
                    (int) (QWERTY_RIGHT_OFFSET * density),
                    (int) (QWERTY_BOTTOM_OFFSET * density),
                    (int) (skinType.qwertyKeyRoundRadius * density),
                    skinType.qwertyLayoutReleasedFunctionKeyTopColor,
                    skinType.qwertyLayoutReleasedFunctionKeyBottomColor,
                    skinType.qwertyLayoutReleasedFunctionKeyHighlightColor,
                    skinType.qwertyLayoutReleasedFunctionKeyShadowColor)),
            new LightIconDrawable(
                (int) ((QWERTY_TOP_OFFSET + QWERTY_LIGHT_TOP_OFFSET + QWERTY_LIGHT_RADIUS)
                          * density),
                (int) ((QWERTY_RIGHT_OFFSET + QWERTY_LIGHT_RIGHT_OFFSET + QWERTY_LIGHT_RADIUS)
                          * density),
                skinType.qwertyLightOffSignLightColor,
                skinType.qwertyLightOffSignDarkColor,
                skinType.qwertyLightOffSignShadeColor,
                (int) (QWERTY_LIGHT_RADIUS * density))
            });

      case TWELVEKEYS_CENTER_FLICK:
        return new CenterCircularHighlightDrawable(
            (int) (TWELVEKEYS_LEFT_OFFSET * density),
            (int) (TWELVEKEYS_TOP_OFFSET * density),
            (int) (TWELVEKEYS_RIGHT_OFFSET * density),
            (int) (TWELVEKEYS_BOTTOM_OFFSET * density),
            skinType.flickBaseColor,
            skinType.flickShadeColor);

      case TWELVEKEYS_LEFT_FLICK:
        return new TriangularHighlightDrawable(
            (int) (TWELVEKEYS_LEFT_OFFSET * density),
            (int) (TWELVEKEYS_TOP_OFFSET * density),
            (int) (TWELVEKEYS_RIGHT_OFFSET * density),
            (int) (TWELVEKEYS_BOTTOM_OFFSET * density),
            skinType.flickBaseColor,
            skinType.flickShadeColor,
            HighlightDirection.LEFT);

      case TWELVEKEYS_UP_FLICK:
        return new TriangularHighlightDrawable(
            (int) (TWELVEKEYS_LEFT_OFFSET * density),
            (int) (TWELVEKEYS_TOP_OFFSET * density),
            (int) (TWELVEKEYS_RIGHT_OFFSET * density),
            (int) (TWELVEKEYS_BOTTOM_OFFSET * density),
            skinType.flickBaseColor,
            skinType.flickShadeColor,
            HighlightDirection.UP);

      case TWELVEKEYS_RIGHT_FLICK:
        return new TriangularHighlightDrawable(
            (int) (TWELVEKEYS_LEFT_OFFSET * density),
            (int) (TWELVEKEYS_TOP_OFFSET * density),
            (int) (TWELVEKEYS_RIGHT_OFFSET * density),
            (int) (TWELVEKEYS_BOTTOM_OFFSET * density),
            skinType.flickBaseColor,
            skinType.flickShadeColor,
            HighlightDirection.RIGHT);

      case TWELVEKEYS_DOWN_FLICK:
        return new TriangularHighlightDrawable(
            (int) (TWELVEKEYS_LEFT_OFFSET * density),
            (int) (TWELVEKEYS_TOP_OFFSET * density),
            (int) (TWELVEKEYS_RIGHT_OFFSET * density),
            (int) (TWELVEKEYS_BOTTOM_OFFSET * density),
            skinType.flickBaseColor,
            skinType.flickShadeColor,
            HighlightDirection.DOWN);

      case POPUP_BACKGROUND_WINDOW:
        // TODO(hidehiko): add a flag to control popup style in SkinType.
        return skinType == SkinType.ORANGE_LIGHTGRAY
            ? new PopUpFrameWindowDrawable(
                  (int) (POPUP_WINDOW_PADDING * density),
                  (int) (POPUP_WINDOW_PADDING * density),
                  (int) (POPUP_WINDOW_PADDING * density),
                  (int) (POPUP_WINDOW_PADDING * density),
                  (int) (POPUP_FRAME_BORDER_WIDTH * density),
                  POPUP_SHADOW_SIZE * density,
                  skinType.popupFrameWindowTopColor,
                  skinType.popupFrameWindowBottomColor,
                  skinType.popupFrameWindowBorderColor,
                  skinType.popupFrameWindowInnerPaneColor)
            : new RoundRectKeyDrawable(
                  (int) (POPUP_WINDOW_PADDING * density),
                  (int) (POPUP_WINDOW_PADDING * density),
                  (int) (POPUP_WINDOW_PADDING * density),
                  (int) (POPUP_WINDOW_PADDING * density),
                  (int) (skinType.qwertyKeyRoundRadius * density),
                  skinType.popupFrameWindowTopColor,
                  skinType.popupFrameWindowBottomColor,
                  0,  // No highlight.
                  skinType.popupFrameWindowShadowColor);
      case CANDIDATE_BACKGROUND:
        return createFocusableDrawable(
            new CandidateBackgroundFocusedDrawable(
                (int) (CANDIDATE_BACKGROUND_PADDING * density),
                (int) (CANDIDATE_BACKGROUND_PADDING * density),
                (int) (CANDIDATE_BACKGROUND_PADDING * density),
                (int) (CANDIDATE_BACKGROUND_PADDING * density),
                skinType.candidateBackgroundFocusedTopColor,
                skinType.candidateBackgroundFocusedBottomColor,
                skinType.candidateBackgroundFocusedShadowColor),
            new CandidateBackgroundDrawable(
                (int) (CANDIDATE_BACKGROUND_PADDING * density),
                (int) (CANDIDATE_BACKGROUND_PADDING * density),
                (int) (CANDIDATE_BACKGROUND_PADDING * density),
                (int) (CANDIDATE_BACKGROUND_PADDING * density),
                skinType.candidateBackgroundTopColor,
                skinType.candidateBackgroundBottomColor,
                skinType.candidateBackgroundHighlightColor,
                skinType.candidateBackgroundBorderColor));
      case SYMBOL_CANDIDATE_BACKGROUND:
        return createFocusableDrawable(
            new CandidateBackgroundFocusedDrawable(
                (int) (SYMBOL_CANDIDATE_BACKGROUND_PADDING * density),
                (int) (SYMBOL_CANDIDATE_BACKGROUND_PADDING * density),
                (int) (SYMBOL_CANDIDATE_BACKGROUND_PADDING * density),
                (int) (SYMBOL_CANDIDATE_BACKGROUND_PADDING * density),
                skinType.candidateBackgroundFocusedTopColor,
                skinType.candidateBackgroundFocusedBottomColor,
                skinType.candidateBackgroundFocusedShadowColor),
            new CandidateBackgroundDrawable(
                (int) (SYMBOL_CANDIDATE_BACKGROUND_PADDING * density),
                (int) (SYMBOL_CANDIDATE_BACKGROUND_PADDING * density),
                (int) (SYMBOL_CANDIDATE_BACKGROUND_PADDING * density),
                (int) (SYMBOL_CANDIDATE_BACKGROUND_PADDING * density),
                skinType.symbolCandidateBackgroundTopColor,
                skinType.symbolCandidateBackgroundBottomColor,
                skinType.symbolCandidateBackgroundHighlightColor,
                skinType.symbolCandidateBackgroundBorderColor));
    }

    throw new IllegalArgumentException("Unknown drawable type: " + drawableType);
  }

  // TODO(hidehiko): Move this method to the somewhere in view package.
  public static Drawable createPressableDrawable(
      Drawable pressedDrawable, Drawable releasedDrawable) {
    return createTwoStateDrawable(
        android.R.attr.state_pressed, pressedDrawable, releasedDrawable);
  }

  public static Drawable createFocusableDrawable(
      Drawable focusedDrawable, Drawable unfocusedDrawable) {
    return createTwoStateDrawable(
        android.R.attr.state_focused, focusedDrawable, unfocusedDrawable);
  }

  public static Drawable createSelectableDrawable(
      Drawable selectedDrawable, Drawable unselectedDrawable) {
    return createTwoStateDrawable(
        android.R.attr.state_selected, selectedDrawable, unselectedDrawable);
  }

  private static Drawable createTwoStateDrawable(
      int state, Drawable enabledDrawable, Drawable disabledDrawable) {
    StateListDrawable drawable = new StateListDrawable();
    drawable.addState(new int[] { state }, enabledDrawable);
    if (disabledDrawable != null) {
      drawable.addState(EMPTY_STATE, disabledDrawable);
    }
    return drawable;
  }
}
