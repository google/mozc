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

package org.mozc.android.inputmethod.japanese.ui;

import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.view.MozcDrawableFactory;
import com.google.common.annotations.VisibleForTesting;

import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.GradientDrawable.Orientation;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.ViewStub.OnInflateListener;
import android.view.animation.Animation;
import android.widget.FrameLayout;
import android.widget.ImageView;

/**
 * Proxy between ViewStub and left/right frame.
 * left/right frame is inflated when mozcview needs.
 * SideFrameStubProxy caches and adapts each parameter.
 *
 */
public class SideFrameStubProxy {

  @VisibleForTesting public boolean inflated = false;

  private View currentView = null;

  private ImageView adjustButton = null;
  private int inputFrameHeight = 0;
  private OnClickListener buttonOnClickListener = null;

  private View dropshadowShort = null;
  private View dropshadowLong = null;
  private int dropShadowShortVisibility = View.GONE;
  private int dropShadowShortLayoutHeight = 0;
  private int dropShadowLongLayoutHeight = 0;

  private static void setLayoutHeight(View view, int height) {
    ViewGroup.LayoutParams layoutParams = view.getLayoutParams();
    layoutParams.height = height;
    view.setLayoutParams(layoutParams);
  }

  private static Drawable createDropShadowGradientDrawable(
      int startColor, int endColor, float radius, float centerX, float centerY) {
    GradientDrawable gradientDrawable =
        new GradientDrawable(Orientation.TL_BR, new int[]{startColor, endColor});
    gradientDrawable.setGradientType(GradientDrawable.RADIAL_GRADIENT);
    gradientDrawable.setGradientRadius(radius);
    gradientDrawable.setGradientCenter(centerX, centerY);
    return gradientDrawable;
  }

  public void initialize(View view, int stubId, int frameId,
                         final int dropShadowShortTopId, final int dropShadowLongTopId,
                         final int adjustButtonId, final int adjustButtonResouceId,
                         final float dropShadowGradientCenterX,
                         final int dropshadowShortId, final int dropshadowLongId) {
    ViewStub viewStub = ViewStub.class.cast(view.findViewById(stubId));
    currentView = viewStub;

    viewStub.setOnInflateListener(new OnInflateListener() {

      @Override
      public void onInflate(ViewStub stub, View view) {
        inflated = true;

        currentView = view;
        currentView.setVisibility(View.VISIBLE);
        dropshadowShort = view.findViewById(dropshadowShortId);
        dropshadowLong = view.findViewById(dropshadowLongId);
        adjustButton = ImageView.class.cast(view.findViewById(adjustButtonId));
        adjustButton.setOnClickListener(buttonOnClickListener);
        Resources resources = view.getResources();
        MozcDrawableFactory drawableFactory = new MozcDrawableFactory(resources);
        adjustButton.setImageDrawable(drawableFactory.getDrawable(adjustButtonResouceId));

        // Create dropshadow corner drawable.
        // Because resource xml cannot set gradientRadius in dip style, code it.
        int startColor = resources.getColor(R.color.dropshadow_start);
        int endColor = resources.getColor(R.color.dropshadow_end);
        float radius = resources.getDimensionPixelSize(R.dimen.translucent_border_height);
        Drawable gradientTop = createDropShadowGradientDrawable(startColor, endColor, radius,
                                                                dropShadowGradientCenterX, 1.0f);
        view.findViewById(dropShadowShortTopId).setBackgroundDrawable(gradientTop);
        view.findViewById(dropShadowLongTopId).setBackgroundDrawable(gradientTop);

        resetAdjustButtonBottomMarginInternal(inputFrameHeight);
        flipDropShadowVisibilityInternal(dropShadowShortVisibility);
        setDropShadowHeightInternal(dropShadowShortLayoutHeight, dropShadowLongLayoutHeight);
      }
    });
  }

  public void setButtonOnClickListener(OnClickListener onClickListener) {
    buttonOnClickListener = onClickListener;
  }

  public void setFrameVisibility(int visibility) {
    currentView.setVisibility(visibility);
  }

  private void resetAdjustButtonBottomMarginInternal(int inputFrameHeight) {
    ImageView imageView = ImageView.class.cast(adjustButton);
    FrameLayout.LayoutParams layoutParams = FrameLayout.LayoutParams.class.cast(
        imageView.getLayoutParams());
    layoutParams.bottomMargin = (inputFrameHeight - layoutParams.height) / 2;
    imageView.setLayoutParams(layoutParams);
  }

  public void resetAdjustButtonBottomMargin(int inputFrameHeight) {
    if (inflated) {
      resetAdjustButtonBottomMarginInternal(inputFrameHeight);
    }
    this.inputFrameHeight = inputFrameHeight;
  }

  private void flipDropShadowVisibilityInternal(int shortVisibility) {
    dropshadowShort.setVisibility(shortVisibility);
    dropshadowLong.setVisibility(shortVisibility == View.VISIBLE ? View.INVISIBLE : View.VISIBLE);
  }

  public void flipDropShadowVisibility(int shortVisibility) {
    if (inflated) {
      flipDropShadowVisibilityInternal(shortVisibility);
      return;
    }
    dropShadowShortVisibility = shortVisibility;
  }

  private void setDropShadowHeightInternal(int shortHeight, int longHeight) {
    setLayoutHeight(dropshadowShort, shortHeight);
    setLayoutHeight(dropshadowLong, longHeight);
  }

  public void setDropShadowHeight(int shortHeight, int longHeight) {
    if (inflated) {
      setDropShadowHeightInternal(shortHeight, longHeight);
      return;
    }
    dropShadowShortLayoutHeight = shortHeight;
    dropShadowLongLayoutHeight = longHeight;
  }

  public void startDropShadowAnimation(Animation shortAnimation, Animation longAnimation) {
    if (inflated) {
      dropshadowShort.startAnimation(shortAnimation);
      dropshadowLong.startAnimation(longAnimation);
    }
  }
}
