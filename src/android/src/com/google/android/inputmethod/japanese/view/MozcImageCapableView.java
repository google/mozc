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

import org.mozc.android.inputmethod.japanese.resources.R;
import com.google.common.base.Preconditions;

import android.content.res.Resources;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.widget.ImageView;

/**
 * Views which support MozcDrawable should implement this interface
 * delegate some methods.
 */
public interface MozcImageCapableView {

  public void setMaxImageWidth(int maxImageWidth);
  public void setMaxImageHeight(int maxImageHeight);
  public ImageView asImageView();

  /**
   * Delagate object used by the views implementing MozcImageCapableView.
   */
  class MozcImageCapableViewDelegate {

    private static final int INVALID_RESOURCE_ID = 0;
    private static final int UNSPECIFIED_SIZE = -1;
    private static final String XML_NAMESPACE_ANDROID =
        "http://schemas.android.com/apk/res/android";
    private static final String XML_NAMESPACE_MOZC =
        "http://schemas.android.com/apk/res-auto";
    private Skin skin = Skin.getFallbackInstance();
    private int rawId = INVALID_RESOURCE_ID;
    private int maxImageWidth = UNSPECIFIED_SIZE;
    private int maxImageHeight = UNSPECIFIED_SIZE;
    private final MozcImageCapableView baseView;

    MozcImageCapableViewDelegate(MozcImageCapableView baseView) {
      this.baseView = Preconditions.checkNotNull(baseView);
    }

    static boolean assertSrcAttribute(Resources resources, AttributeSet attrs) {
      int srcId = attrs.getAttributeResourceValue(
          XML_NAMESPACE_ANDROID, "src", INVALID_RESOURCE_ID);
      if (srcId == INVALID_RESOURCE_ID) {
        return true;
      }
      return !"raw".equals(resources.getResourceTypeName(srcId));
    }

    void loadMozcImageSource(AttributeSet attrs) {
      rawId = attrs.getAttributeResourceValue(
          XML_NAMESPACE_MOZC, "rawSrc", INVALID_RESOURCE_ID);
      TypedArray typedArray =
          baseView.asImageView().getContext().obtainStyledAttributes(attrs,
                                                                     R.styleable.MozcImageView);
      try {
        maxImageWidth =
            typedArray.getDimensionPixelOffset(R.styleable.MozcImageView_maxImageWidth,
                                               UNSPECIFIED_SIZE);
        maxImageHeight =
            typedArray.getDimensionPixelOffset(R.styleable.MozcImageView_maxImageHeight,
                                               UNSPECIFIED_SIZE);
      } finally {
        typedArray.recycle();
      }
    }

    private void updateMozcDrawable() {
      if (rawId != INVALID_RESOURCE_ID) {
        ImageView view = baseView.asImageView();
        view.setImageDrawable(
            skin.getDrawable(baseView.asImageView().getContext().getResources(), rawId)
                .getConstantState().newDrawable());
        view.invalidate();
      }
    }

    void setRawId(int rawId) {
      this.rawId = rawId;
      updateMozcDrawable();
    }

    @SuppressWarnings("deprecation")
    void setSkin(Skin skin) {
      this.skin = Preconditions.checkNotNull(skin);
      updateMozcDrawable();
    }

    void updateAdditionalPadding() {
      updateAdditionalHorizontalPadding();
      updateAdditionalVerticalPadding();
    }

    void setMaxImageWidth(int maxImageWidth) {
      this.maxImageWidth = maxImageWidth;
      updateAdditionalHorizontalPadding();
    }

    void setMaxImageHeight(int maxImageHeight) {
      this.maxImageHeight = maxImageHeight;
      updateAdditionalVerticalPadding();
    }

    private void updateAdditionalHorizontalPadding() {
      ImageView imageView = baseView.asImageView();
      int paddingLeft = baseView.asImageView().getPaddingLeft();
      int paddingRight = baseView.asImageView().getPaddingRight();
      int additionalHorizontalPadding = maxImageWidth < 0 ? 0 : Math.max(
          0, baseView.asImageView().getWidth() - paddingLeft - paddingRight - maxImageWidth);
      if (additionalHorizontalPadding == 0) {
        return;
      }
      int additionalLeftPadding = additionalHorizontalPadding / 2;
      int additionalRightPadding = additionalHorizontalPadding - additionalLeftPadding;
      baseView.asImageView().setPadding(
          paddingLeft + additionalLeftPadding,
          imageView.getPaddingTop(),
          paddingRight + additionalRightPadding,
          imageView.getPaddingBottom());
      imageView.invalidate();
    }

    private void updateAdditionalVerticalPadding() {
      ImageView imageView = baseView.asImageView();
      int paddingTop = imageView.getPaddingTop();
      int paddingBottom = imageView.getPaddingBottom();
      int additionalVerticalPadding = maxImageHeight < 0
          ? 0 : imageView.getHeight() - paddingTop - paddingBottom - maxImageHeight;
      if (additionalVerticalPadding == 0) {
        return;
      }
      int additionalTopPadding = additionalVerticalPadding / 2;
      int additionalBottomPadding = additionalVerticalPadding - additionalTopPadding;
      baseView.asImageView().setPadding(
          imageView.getPaddingLeft(),
          paddingTop + additionalTopPadding,
          imageView.getPaddingRight(),
          paddingBottom + additionalBottomPadding);
      imageView.invalidate();
    }
  }
}
