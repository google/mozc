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

package org.mozc.android.inputmethod.japanese;

import org.mozc.android.inputmethod.japanese.FeedbackManager.FeedbackEvent;
import org.mozc.android.inputmethod.japanese.hardwarekeyboard.HardwareKeyboard.CompositionSwitchMode;
import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.view.MozcImageView;
import org.mozc.android.inputmethod.japanese.view.RoundRectKeyDrawable;
import org.mozc.android.inputmethod.japanese.view.Skin;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;

/**
 * Narrow frame view.
 */
public class NarrowFrameView extends LinearLayout {

  // TODO(hsumita): Move hard coded parameters to dimens.xml or skin.
  private static final float BUTTON_CORNOR_RADIUS = 3.5f;  // in dip.
  private static final float BUTTON_LEFT_OFFSET = 2.0f;
  private static final float BUTTON_TOP_OFFSET = 2.0f;
  private static final float BUTTON_RIGHT_OFFSET = 2.0f;
  private static final float BUTTON_BOTTOM_OFFSET = 2.0f;

  private Skin skin = Skin.getFallbackInstance();
  private CompositionMode hardwareKeyboardCompositionMode = CompositionMode.HIRAGANA;

  public NarrowFrameView(Context context) {
    super(context);
  }

  public NarrowFrameView(Context context, AttributeSet attrSet) {
    super(context, attrSet);
  }

  @Override
  public void onFinishInflate() {
    setSkin(skin);
  }

  private static Drawable createButtonBackgroundDrawable(float density, Skin skin) {
    return BackgroundDrawableFactory.createPressableDrawable(
        new RoundRectKeyDrawable(
            (int) (BUTTON_LEFT_OFFSET * density),
            (int) (BUTTON_TOP_OFFSET * density),
            (int) (BUTTON_RIGHT_OFFSET * density),
            (int) (BUTTON_BOTTOM_OFFSET * density),
            (int) (BUTTON_CORNOR_RADIUS * density),
            skin.twelvekeysLayoutPressedFunctionKeyTopColor,
            skin.twelvekeysLayoutPressedFunctionKeyBottomColor,
            skin.twelvekeysLayoutPressedFunctionKeyHighlightColor,
            skin.twelvekeysLayoutPressedFunctionKeyShadowColor),
        Optional.<Drawable>of(new RoundRectKeyDrawable(
            (int) (BUTTON_LEFT_OFFSET * density),
            (int) (BUTTON_TOP_OFFSET * density),
            (int) (BUTTON_RIGHT_OFFSET * density),
            (int) (BUTTON_BOTTOM_OFFSET * density),
            (int) (BUTTON_CORNOR_RADIUS * density),
            skin.twelvekeysLayoutReleasedFunctionKeyTopColor,
            skin.twelvekeysLayoutReleasedFunctionKeyBottomColor,
            skin.twelvekeysLayoutReleasedFunctionKeyHighlightColor,
            skin.twelvekeysLayoutReleasedFunctionKeyShadowColor)));
  }

  @SuppressWarnings("deprecation")
  private void setupImageButton(MozcImageView view, int resourceID) {
    float density = getResources().getDisplayMetrics().density;
    view.setImageDrawable(skin.getDrawable(getResources(), resourceID));
    view.setBackgroundDrawable(createButtonBackgroundDrawable(density, skin));
  }

  private void updateImageButton() {
    setupImageButton(getWidenButton(), R.raw.hardware__function__close);
    MozcImageView hardwareCompositionButton = getHardwareCompositionButton();
    if (hardwareKeyboardCompositionMode == CompositionMode.HIRAGANA) {
      setupImageButton(hardwareCompositionButton, R.raw.qwerty__function__kana__icon);
      hardwareCompositionButton.setContentDescription(
          getResources().getString(R.string.cd_key_chartype_to_abc));
    } else {
      setupImageButton(hardwareCompositionButton, R.raw.qwerty__function__alphabet__icon);
      hardwareCompositionButton.setContentDescription(
          getResources().getString(R.string.cd_key_chartype_to_kana));
    }
  }

  @SuppressWarnings("deprecation")
  public void setSkin(Skin skin) {
    this.skin = Preconditions.checkNotNull(skin);
    setBackgroundDrawable(skin.narrowFrameBackgroundDrawable);
    getNarrowFrameSeparator().setBackgroundDrawable(
        skin.keyboardFrameSeparatorBackgroundDrawable.getConstantState().newDrawable());
    updateImageButton();
  }

  public void setEventListener(
      final ViewEventListener viewEventListener, OnClickListener widenButtonClickListener) {
    Preconditions.checkNotNull(viewEventListener);
    Preconditions.checkNotNull(widenButtonClickListener);

    getHardwareCompositionButton().setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        viewEventListener.onFireFeedbackEvent(
            FeedbackEvent.NARROW_FRAME_HARDWARE_COMPOSITION_BUTTON_DOWN);
        viewEventListener.onHardwareKeyboardCompositionModeChange(CompositionSwitchMode.TOGGLE);
      }
    });
    getWidenButton().setOnClickListener(widenButtonClickListener);
  }

  public void setHardwareCompositionButtonImage(CompositionMode compositionMode) {
    this.hardwareKeyboardCompositionMode = Preconditions.checkNotNull(compositionMode);
    updateImageButton();
  }

  @VisibleForTesting
  MozcImageView getHardwareCompositionButton() {
    return MozcImageView.class.cast(findViewById(R.id.hardware_composition_button));
  }

  @VisibleForTesting
  MozcImageView getWidenButton() {
    return MozcImageView.class.cast(findViewById(R.id.widen_button));
  }

  @VisibleForTesting
  View getNarrowFrameSeparator() {
    return findViewById(R.id.narrow_frame_separator);
  }
}
