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

package org.mozc.android.inputmethod.japanese.keyboard;

import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.vectorgraphic.BufferedDrawable;
import org.mozc.android.inputmethod.japanese.view.CandidateBackgroundDrawable;
import org.mozc.android.inputmethod.japanese.view.CandidateBackgroundFocusedDrawable;
import org.mozc.android.inputmethod.japanese.view.CenterCircularHighlightDrawable;
import org.mozc.android.inputmethod.japanese.view.DummyDrawable;
import org.mozc.android.inputmethod.japanese.view.QwertySpaceKeyDrawable;
import org.mozc.android.inputmethod.japanese.view.RectKeyDrawable;
import org.mozc.android.inputmethod.japanese.view.RoundRectKeyDrawable;
import org.mozc.android.inputmethod.japanese.view.Skin;
import org.mozc.android.inputmethod.japanese.view.ThreeDotsIconDrawable;
import org.mozc.android.inputmethod.japanese.view.TriangularHighlightDrawable;
import org.mozc.android.inputmethod.japanese.view.TriangularHighlightDrawable.HighlightDirection;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.content.res.Resources;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
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
    TWELVEKEYS_FUNCTION_KEY_BACKGROUND_WITH_THREEDOTS,

    // Key background for qwerty layout.
    QWERTY_REGULAR_KEY_BACKGROUND,
    QWERTY_FUNCTION_KEY_BACKGROUND,
    QWERTY_FUNCTION_KEY_BACKGROUND_WITH_THREEDOTS,
    QWERTY_FUNCTION_KEY_SPACE_WITH_THREEDOTS,

    // Separator on keyboard.
    KEYBOARD_SEPARATOR_TOP,
    KEYBOARD_SEPARATOR_CENTER,
    KEYBOARD_SEPARATOR_BOTTOM,

    TRNASPARENT,

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
  public static final float POPUP_WINDOW_PADDING = 7.0f;

  private static final float TWELVEKEYS_THREEDOTS_BOTTOM_OFFSET = 3.0f;
  private static final float TWELVEKEYS_THREEDOTS_RIGHT_OFFSET = 5.0f;
  private static final float TWELVEKEYS_THREEDOTS_WIDTH = 2.0f;
  private static final float TWELVEKEYS_THREEDOTS_SPAN = 2.0f;

  private static final float QWERTY_THREEDOTS_BOTTOM_OFFSET = 3.0f;
  private static final float QWERTY_THREEDOTS_RIGHT_OFFSET = 5.0f;
  private static final float QWERTY_THREEDOTS_WIDTH = 2.0f;
  private static final float QWERTY_THREEDOTS_SPAN = 2.0f;

  private static final float CANDIDATE_BACKGROUND_PADDING = 0.0f;
  private static final float SYMBOL_CANDIDATE_BACKGROUND_PADDING = 0.0f;

  private final Resources resources;
  private final float density;
  private final Map<DrawableType, Drawable> drawableMap =
      new EnumMap<DrawableType, Drawable>(DrawableType.class);
  private Skin skin = Skin.getFallbackInstance();

  public BackgroundDrawableFactory(Resources resources) {
    this.resources = Preconditions.checkNotNull(resources);
    density = resources.getDisplayMetrics().density;
  }

  public Drawable getDrawable(DrawableType drawableType) {
    Drawable result = drawableMap.get(Preconditions.checkNotNull(drawableType));
    if (result == null) {
      result = createBackgroundDrawable(drawableType);
      drawableMap.put(drawableType, result);
    }
    return result;
  }

  public void setSkin(Skin skin) {
    Preconditions.checkNotNull(skin);
    if (this.skin.equals(skin)) {
      return;
    }
    this.skin = skin;
    drawableMap.clear();
  }

  private Drawable createBackgroundDrawable(DrawableType drawableType) {
    // Some Drawable instances are decorated by BufferedDrawable.
    // The motivations are
    // - To improve performance.
    // - To support Picture for h/w accelerated canvas.
    switch (Preconditions.checkNotNull(drawableType)) {
      case TWELVEKEYS_REGULAR_KEY_BACKGROUND:
        return createPressableDrawable(
            new RectKeyDrawable(
                (int) (skin.twelvekeysLeftOffsetDimension),
                (int) (skin.twelvekeysTopOffsetDimension),
                (int) (skin.twelvekeysRightOffsetDimension),
                (int) (skin.twelvekeysBottomOffsetDimension),
                skin.twelvekeysLayoutPressedKeyTopColor,
                skin.twelvekeysLayoutPressedKeyBottomColor,
                skin.twelvekeysLayoutPressedKeyHighlightColor,
                skin.twelvekeysLayoutPressedKeyLightShadeColor,
                skin.twelvekeysLayoutPressedKeyDarkShadeColor,
                skin.twelvekeysLayoutPressedKeyShadowColor),
            Optional.<Drawable>of(new RectKeyDrawable(
                (int) (skin.twelvekeysLeftOffsetDimension),
                (int) (skin.twelvekeysTopOffsetDimension),
                (int) (skin.twelvekeysRightOffsetDimension),
                (int) (skin.twelvekeysBottomOffsetDimension),
                skin.twelvekeysLayoutReleasedKeyTopColor,
                skin.twelvekeysLayoutReleasedKeyBottomColor,
                skin.twelvekeysLayoutReleasedKeyHighlightColor,
                skin.twelvekeysLayoutReleasedKeyLightShadeColor,
                skin.twelvekeysLayoutReleasedKeyDarkShadeColor,
                skin.twelvekeysLayoutReleasedKeyShadowColor)));

      case TWELVEKEYS_FUNCTION_KEY_BACKGROUND:
        return createPressableDrawable(
            new BufferedDrawable(new RectKeyDrawable(
                (int) (skin.twelvekeysLeftOffsetDimension),
                (int) (skin.twelvekeysTopOffsetDimension),
                (int) (skin.twelvekeysRightOffsetDimension),
                (int) (skin.twelvekeysBottomOffsetDimension),
                skin.twelvekeysLayoutPressedFunctionKeyTopColor,
                skin.twelvekeysLayoutPressedFunctionKeyBottomColor,
                skin.twelvekeysLayoutPressedFunctionKeyHighlightColor,
                skin.twelvekeysLayoutPressedFunctionKeyLightShadeColor,
                skin.twelvekeysLayoutPressedFunctionKeyDarkShadeColor,
                skin.twelvekeysLayoutPressedFunctionKeyShadowColor)),
            Optional.<Drawable>of(new BufferedDrawable(new RectKeyDrawable(
                (int) (skin.twelvekeysLeftOffsetDimension),
                (int) (skin.twelvekeysTopOffsetDimension),
                (int) (skin.twelvekeysRightOffsetDimension),
                (int) (skin.twelvekeysBottomOffsetDimension),
                skin.twelvekeysLayoutReleasedFunctionKeyTopColor,
                skin.twelvekeysLayoutReleasedFunctionKeyBottomColor,
                skin.twelvekeysLayoutReleasedFunctionKeyHighlightColor,
                skin.twelvekeysLayoutReleasedFunctionKeyLightShadeColor,
                skin.twelvekeysLayoutReleasedFunctionKeyDarkShadeColor,
                skin.twelvekeysLayoutReleasedFunctionKeyShadowColor))));

      case TWELVEKEYS_FUNCTION_KEY_BACKGROUND_WITH_THREEDOTS:
        return new LayerDrawable(new Drawable[] {
            createPressableDrawable(
                new BufferedDrawable(new RectKeyDrawable(
                    (int) (skin.twelvekeysLeftOffsetDimension),
                    (int) (skin.twelvekeysTopOffsetDimension),
                    (int) (skin.twelvekeysRightOffsetDimension),
                    (int) (skin.twelvekeysBottomOffsetDimension),
                    skin.twelvekeysLayoutPressedFunctionKeyTopColor,
                    skin.twelvekeysLayoutPressedFunctionKeyBottomColor,
                    skin.twelvekeysLayoutPressedFunctionKeyHighlightColor,
                    skin.twelvekeysLayoutPressedFunctionKeyLightShadeColor,
                    skin.twelvekeysLayoutPressedFunctionKeyDarkShadeColor,
                    skin.twelvekeysLayoutPressedFunctionKeyShadowColor)),
                Optional.<Drawable>of(new BufferedDrawable(new RectKeyDrawable(
                    (int) (skin.twelvekeysLeftOffsetDimension),
                    (int) (skin.twelvekeysTopOffsetDimension),
                    (int) (skin.twelvekeysRightOffsetDimension),
                    (int) (skin.twelvekeysBottomOffsetDimension),
                    skin.twelvekeysLayoutReleasedFunctionKeyTopColor,
                    skin.twelvekeysLayoutReleasedFunctionKeyBottomColor,
                    skin.twelvekeysLayoutReleasedFunctionKeyHighlightColor,
                    skin.twelvekeysLayoutReleasedFunctionKeyLightShadeColor,
                    skin.twelvekeysLayoutReleasedFunctionKeyDarkShadeColor,
                    skin.twelvekeysLayoutReleasedFunctionKeyShadowColor)))),
              new BufferedDrawable(new ThreeDotsIconDrawable(
                (int) (skin.twelvekeysBottomOffsetDimension
                    + TWELVEKEYS_THREEDOTS_BOTTOM_OFFSET * density),
                (int) (skin.twelvekeysRightOffsetDimension
                    + TWELVEKEYS_THREEDOTS_RIGHT_OFFSET * density),
                skin.threeDotsColor,
                (int) (TWELVEKEYS_THREEDOTS_WIDTH * density),
                (int) (TWELVEKEYS_THREEDOTS_SPAN * density)))});

      case QWERTY_REGULAR_KEY_BACKGROUND:
        return createPressableDrawable(
            new BufferedDrawable(new RoundRectKeyDrawable(
                (int) (skin.qwertyLeftOffsetDimension),
                (int) (skin.qwertyTopOffsetDimension),
                (int) (skin.qwertyRightOffsetDimension),
                (int) (skin.qwertyBottomOffsetDimension),
                (int) (skin.qwertyRoundRadiusDimension),
                skin.qwertyLayoutPressedKeyTopColor,
                skin.qwertyLayoutPressedKeyBottomColor,
                skin.qwertyLayoutPressedKeyHighlightColor,
                skin.qwertyLayoutPressedKeyShadowColor)),
            Optional.<Drawable>of(new BufferedDrawable(new RoundRectKeyDrawable(
                (int) (skin.qwertyLeftOffsetDimension),
                (int) (skin.qwertyTopOffsetDimension),
                (int) (skin.qwertyRightOffsetDimension),
                (int) (skin.qwertyBottomOffsetDimension),
                (int) (skin.qwertyRoundRadiusDimension),
                skin.qwertyLayoutReleasedKeyTopColor,
                skin.qwertyLayoutReleasedKeyBottomColor,
                skin.qwertyLayoutReleasedKeyHighlightColor,
                skin.qwertyLayoutReleasedKeyShadowColor))));

      case QWERTY_FUNCTION_KEY_BACKGROUND:
        return createPressableDrawable(
            new BufferedDrawable(new RoundRectKeyDrawable(
                (int) (skin.qwertyLeftOffsetDimension),
                (int) (skin.qwertyTopOffsetDimension),
                (int) (skin.qwertyRightOffsetDimension),
                (int) (skin.qwertyBottomOffsetDimension),
                (int) (skin.qwertyRoundRadiusDimension),
                skin.qwertyLayoutPressedFunctionKeyTopColor,
                skin.qwertyLayoutPressedFunctionKeyBottomColor,
                skin.qwertyLayoutPressedFunctionKeyHighlightColor,
                skin.qwertyLayoutPressedFunctionKeyShadowColor)),
            Optional.<Drawable>of(new BufferedDrawable(new RoundRectKeyDrawable(
                (int) (skin.qwertyLeftOffsetDimension),
                (int) (skin.qwertyTopOffsetDimension),
                (int) (skin.qwertyRightOffsetDimension),
                (int) (skin.qwertyBottomOffsetDimension),
                (int) (skin.qwertyRoundRadiusDimension),
                skin.qwertyLayoutReleasedFunctionKeyTopColor,
                skin.qwertyLayoutReleasedFunctionKeyBottomColor,
                skin.qwertyLayoutReleasedFunctionKeyHighlightColor,
                skin.qwertyLayoutReleasedFunctionKeyShadowColor))));

      case QWERTY_FUNCTION_KEY_BACKGROUND_WITH_THREEDOTS:
        return new LayerDrawable(new Drawable[] {
            createPressableDrawable(
                new BufferedDrawable(new RoundRectKeyDrawable(
                    (int) (skin.qwertyLeftOffsetDimension),
                    (int) (skin.qwertyTopOffsetDimension),
                    (int) (skin.qwertyRightOffsetDimension),
                    (int) (skin.qwertyBottomOffsetDimension),
                    (int) (skin.qwertyRoundRadiusDimension),
                    skin.qwertyLayoutPressedFunctionKeyTopColor,
                    skin.qwertyLayoutPressedFunctionKeyBottomColor,
                    skin.qwertyLayoutPressedFunctionKeyHighlightColor,
                    skin.qwertyLayoutPressedFunctionKeyShadowColor)),
                Optional.<Drawable>of(new BufferedDrawable(new RoundRectKeyDrawable(
                    (int) (skin.qwertyLeftOffsetDimension),
                    (int) (skin.qwertyTopOffsetDimension),
                    (int) (skin.qwertyRightOffsetDimension),
                    (int) (skin.qwertyBottomOffsetDimension),
                    (int) (skin.qwertyRoundRadiusDimension),
                    skin.qwertyLayoutReleasedFunctionKeyTopColor,
                    skin.qwertyLayoutReleasedFunctionKeyBottomColor,
                    skin.qwertyLayoutReleasedFunctionKeyHighlightColor,
                    skin.qwertyLayoutReleasedFunctionKeyShadowColor)))),
            new BufferedDrawable(new ThreeDotsIconDrawable(
                (int) (skin.qwertyBottomOffsetDimension
                    + (QWERTY_THREEDOTS_BOTTOM_OFFSET * density)),
                (int) (skin.qwertyRightOffsetDimension
                    + (QWERTY_THREEDOTS_RIGHT_OFFSET * density)),
                skin.threeDotsColor,
                (int) (QWERTY_THREEDOTS_WIDTH * density),
                (int) (QWERTY_THREEDOTS_SPAN * density)))});

      case QWERTY_FUNCTION_KEY_SPACE_WITH_THREEDOTS:
        return new LayerDrawable(new Drawable[] {
            createPressableDrawable(
                new BufferedDrawable(new QwertySpaceKeyDrawable(
                    (int) (skin.qwertySpaceKeyHeightDimension),
                    (int) (skin.qwertySpaceKeyHorizontalOffsetDimension),
                    (int) (skin.qwertyTopOffsetDimension),
                    (int) (skin.qwertySpaceKeyHorizontalOffsetDimension),
                    (int) (skin.qwertyBottomOffsetDimension),
                    (int) (skin.qwertySpaceKeyRoundRadiusDimension),
                    skin.qwertyLayoutPressedSpaceKeyTopColor,
                    skin.qwertyLayoutPressedSpaceKeyBottomColor,
                    skin.qwertyLayoutPressedSpaceKeyHighlightColor,
                    skin.qwertyLayoutPressedSpaceKeyShadowColor)),
                Optional.<Drawable>of(new BufferedDrawable(new QwertySpaceKeyDrawable(
                    (int) (skin.qwertySpaceKeyHeightDimension),
                    (int) (skin.qwertySpaceKeyHorizontalOffsetDimension),
                    (int) (skin.qwertyTopOffsetDimension),
                    (int) (skin.qwertySpaceKeyHorizontalOffsetDimension),
                    (int) (skin.qwertyBottomOffsetDimension),
                    (int) (skin.qwertySpaceKeyRoundRadiusDimension),
                    skin.qwertyLayoutReleasedSpaceKeyTopColor,
                    skin.qwertyLayoutReleasedSpaceKeyBottomColor,
                    skin.qwertyLayoutReleasedSpaceKeyHighlightColor,
                    skin.qwertyLayoutReleasedSpaceKeyShadowColor)))),
            new BufferedDrawable(new ThreeDotsIconDrawable(
                (int) (skin.qwertyBottomOffsetDimension
                    + (QWERTY_THREEDOTS_BOTTOM_OFFSET * density)),
                (int) (skin.qwertySpaceKeyHorizontalOffsetDimension
                    + (QWERTY_THREEDOTS_RIGHT_OFFSET * density)),
                skin.threeDotsColor,
                (int) (QWERTY_THREEDOTS_WIDTH * density),
                (int) (QWERTY_THREEDOTS_SPAN * density)))});

      case KEYBOARD_SEPARATOR_TOP:
        return new InsetDrawable(
            new ColorDrawable(skin.keyboardSeparatorColor), 0,
                              resources.getDimensionPixelSize(R.dimen.keyboard_separator_padding),
                              0, 0);

      case KEYBOARD_SEPARATOR_CENTER:
        return new ColorDrawable(skin.keyboardSeparatorColor);

      case KEYBOARD_SEPARATOR_BOTTOM:
        return new InsetDrawable(
            new ColorDrawable(skin.keyboardSeparatorColor), 0, 0, 0,
            resources.getDimensionPixelSize(R.dimen.keyboard_separator_padding));

      case TRNASPARENT:
        return DummyDrawable.getInstance();

      case TWELVEKEYS_CENTER_FLICK:
        return new CenterCircularHighlightDrawable(
            (int) (skin.twelvekeysLeftOffsetDimension),
            (int) (skin.twelvekeysTopOffsetDimension),
            (int) (skin.twelvekeysRightOffsetDimension),
            (int) (skin.twelvekeysBottomOffsetDimension),
            skin.flickBaseColor,
            skin.flickShadeColor);

      case TWELVEKEYS_LEFT_FLICK:
        // TriangularHighlightDrawable depends on Picture so buffering is required.
        return new BufferedDrawable(new TriangularHighlightDrawable(
            (int) (skin.twelvekeysLeftOffsetDimension),
            (int) (skin.twelvekeysTopOffsetDimension),
            (int) (skin.twelvekeysRightOffsetDimension),
            (int) (skin.twelvekeysBottomOffsetDimension),
            skin.flickBaseColor,
            skin.flickShadeColor,
            HighlightDirection.LEFT));

      case TWELVEKEYS_UP_FLICK:
        // TriangularHighlightDrawable depends on Picture so buffering is required.
        return new BufferedDrawable(new TriangularHighlightDrawable(
            (int) (skin.twelvekeysLeftOffsetDimension),
            (int) (skin.twelvekeysTopOffsetDimension),
            (int) (skin.twelvekeysRightOffsetDimension),
            (int) (skin.twelvekeysBottomOffsetDimension),
            skin.flickBaseColor,
            skin.flickShadeColor,
            HighlightDirection.UP));

      case TWELVEKEYS_RIGHT_FLICK:
        // TriangularHighlightDrawable depends on Picture so buffering is required.
        return new BufferedDrawable(new TriangularHighlightDrawable(
            (int) (skin.twelvekeysLeftOffsetDimension),
            (int) (skin.twelvekeysTopOffsetDimension),
            (int) (skin.twelvekeysRightOffsetDimension),
            (int) (skin.twelvekeysBottomOffsetDimension),
            skin.flickBaseColor,
            skin.flickShadeColor,
            HighlightDirection.RIGHT));

      case TWELVEKEYS_DOWN_FLICK:
        // TriangularHighlightDrawable depends on Picture so buffering is required.
        return new BufferedDrawable(new TriangularHighlightDrawable(
            (int) (skin.twelvekeysLeftOffsetDimension),
            (int) (skin.twelvekeysTopOffsetDimension),
            (int) (skin.twelvekeysRightOffsetDimension),
            (int) (skin.twelvekeysBottomOffsetDimension),
            skin.flickBaseColor,
            skin.flickShadeColor,
            HighlightDirection.DOWN));

      case POPUP_BACKGROUND_WINDOW:
        return
            new RoundRectKeyDrawable(
                (int) (POPUP_WINDOW_PADDING * density),
                (int) (POPUP_WINDOW_PADDING * density),
                (int) (POPUP_WINDOW_PADDING * density),
                (int) (POPUP_WINDOW_PADDING * density),
                (int) (skin.qwertyRoundRadiusDimension),
                skin.popupFrameWindowTopColor,
                skin.popupFrameWindowBottomColor,
                0,  // No highlight.
                skin.popupFrameWindowShadowColor);

      case CANDIDATE_BACKGROUND:
        // The size of candidate drawable varies so buffering will take much memory.
        // Don't use BufferedDrawable.
        return createFocusableDrawable(
            new CandidateBackgroundFocusedDrawable(
                (int) (CANDIDATE_BACKGROUND_PADDING * density),
                (int) (CANDIDATE_BACKGROUND_PADDING * density),
                (int) (CANDIDATE_BACKGROUND_PADDING * density),
                (int) (CANDIDATE_BACKGROUND_PADDING * density),
                skin.candidateBackgroundFocusedTopColor,
                skin.candidateBackgroundFocusedBottomColor,
                skin.candidateBackgroundFocusedShadowColor),
            Optional.<Drawable>of(new CandidateBackgroundDrawable(
                (int) (CANDIDATE_BACKGROUND_PADDING * density),
                (int) (CANDIDATE_BACKGROUND_PADDING * density),
                (int) (CANDIDATE_BACKGROUND_PADDING * density),
                (int) (CANDIDATE_BACKGROUND_PADDING * density),
                skin.candidateBackgroundTopColor,
                skin.candidateBackgroundBottomColor,
                skin.candidateBackgroundHighlightColor,
                skin.candidateBackgroundBorderColor)));

      case SYMBOL_CANDIDATE_BACKGROUND:
        // The size of candidate drawable varies so buffering will take much memory.
        // Don't use BufferedDrawable.
        return createFocusableDrawable(
            new CandidateBackgroundFocusedDrawable(
                (int) (SYMBOL_CANDIDATE_BACKGROUND_PADDING * density),
                (int) (SYMBOL_CANDIDATE_BACKGROUND_PADDING * density),
                (int) (SYMBOL_CANDIDATE_BACKGROUND_PADDING * density),
                (int) (SYMBOL_CANDIDATE_BACKGROUND_PADDING * density),
                skin.candidateBackgroundFocusedTopColor,
                skin.candidateBackgroundFocusedBottomColor,
                skin.candidateBackgroundFocusedShadowColor),
            Optional.<Drawable>of(new CandidateBackgroundDrawable(
                (int) (SYMBOL_CANDIDATE_BACKGROUND_PADDING * density),
                (int) (SYMBOL_CANDIDATE_BACKGROUND_PADDING * density),
                (int) (SYMBOL_CANDIDATE_BACKGROUND_PADDING * density),
                (int) (SYMBOL_CANDIDATE_BACKGROUND_PADDING * density),
                skin.symbolCandidateBackgroundTopColor,
                skin.symbolCandidateBackgroundBottomColor,
                skin.symbolCandidateBackgroundHighlightColor,
                skin.symbolCandidateBackgroundBorderColor)));
    }

    throw new IllegalArgumentException("Unknown drawable type: " + drawableType);
  }

  // TODO(hidehiko): Move this method to the somewhere in view package.
  public static Drawable createPressableDrawable(
      Drawable pressedDrawable, Optional<Drawable> releasedDrawable) {
    return createTwoStateDrawable(
        android.R.attr.state_pressed, Preconditions.checkNotNull(pressedDrawable),
        Preconditions.checkNotNull(releasedDrawable));
  }

  public static Drawable createFocusableDrawable(
      Drawable focusedDrawable, Optional<Drawable> unfocusedDrawable) {
    return createTwoStateDrawable(
        android.R.attr.state_focused, Preconditions.checkNotNull(focusedDrawable),
        Preconditions.checkNotNull(unfocusedDrawable));
  }

  public static Drawable createSelectableDrawable(
      Drawable selectedDrawable, Optional<Drawable> unselectedDrawable) {
    return createTwoStateDrawable(
        android.R.attr.state_selected, Preconditions.checkNotNull(selectedDrawable),
        Preconditions.checkNotNull(unselectedDrawable));
  }

  private static Drawable createTwoStateDrawable(
      int state, Drawable enabledDrawable, Optional<Drawable> disabledDrawable) {
    StateListDrawable drawable = new StateListDrawable();
    drawable.addState(new int[] { state }, enabledDrawable);
    if (disabledDrawable.isPresent()) {
      drawable.addState(EMPTY_STATE, disabledDrawable.get());
    }
    return drawable;
  }
}
