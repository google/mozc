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

package org.mozc.android.inputmethod.japanese.ui;

import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.view.RectKeyDrawable;
import org.mozc.android.inputmethod.japanese.view.Skin;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;

/**
 * Skin-aware vertical indicator to represent the current scroller's position.
 *
 */
public class ScrollGuideView extends View {

  private final int scrollBarMinimumHeight = getScrollBarMinimumHeight(getResources());
  private Optional<SnapScroller> snapScroller = Optional.absent();
  private Skin skin = Skin.getFallbackInstance();
  @VisibleForTesting Drawable scrollBarDrawable = createScrollBarDrawable(skin);

  public ScrollGuideView(Context context) {
    super(context);
  }

  public ScrollGuideView(Context context, AttributeSet attributeSet) {
    super(context, attributeSet);
  }

  public ScrollGuideView(Context context, AttributeSet attributeSet, int defStyle) {
    super(context, attributeSet, defStyle);
  }

  private static Drawable createScrollBarDrawable(Skin skin) {
    // TODO(hidehiko): Probably we should rename the RectKeyDrawable,
    //   because this usage is not the key but actually we can reuse the code as is.
    return new RectKeyDrawable(1, 0, 1, 1,
                               skin.candidateScrollBarTopColor,
                               skin.candidateScrollBarBottomColor,
                               0, 0, 0, 0);
  }

  private static int getScrollBarMinimumHeight(Resources resources) {
    Preconditions.checkNotNull(resources);
    return resources.getDimensionPixelSize(R.dimen.candidate_scrollbar_minimum_height);
  }

  /** Sets the skin type, and regenerates an indicator drawable if necessary. */
  @SuppressWarnings("deprecation")
  public void setSkin(Skin skin) {
    Preconditions.checkNotNull(skin);
    if (this.skin.equals(skin)) {
      return;
    }
    this.skin = skin;
    scrollBarDrawable = createScrollBarDrawable(skin);
    setBackgroundDrawable(skin.scrollBarBackgroundDrawable.getConstantState().newDrawable());
    invalidate();
  }

  /** Attaches a {@link SnapScroller} instance to this view. */
  public void setScroller(SnapScroller scroller) {
    this.snapScroller = Optional.of(Preconditions.checkNotNull(scroller));
    invalidate();
  }

  @Override
  protected void onDraw(Canvas canvas) {
    super.onDraw(canvas);

    // Paint the scroll bar.
    if (!snapScroller.isPresent()) {
      return;
    }
    SnapScroller scroller = snapScroller.get();

    int contentSize = Math.max(
        scroller.getContentSize(), scroller.getMaxScrollPosition() + scroller.getViewSize());
    if (contentSize == 0) {
      return;
    }

    int height = getHeight();

    // Ceil the slide bar's size.
    int length = Math.max(
        (height * scroller.getViewSize() + contentSize - 1) / contentSize,
        scrollBarMinimumHeight);
    if (length == 0) {
      return;
    }
    int maxScrollPosition = scroller.getMaxScrollPosition();
    int top = (maxScrollPosition == 0)
        ? 0
        : (height - length) * scroller.getScrollPosition() / maxScrollPosition;
    scrollBarDrawable.setBounds(0, top, getWidth(), top + length);
    scrollBarDrawable.draw(canvas);
  }
}
