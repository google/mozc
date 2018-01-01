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

import org.mozc.android.inputmethod.japanese.MozcUtil;
import com.google.common.base.Preconditions;

import android.graphics.Rect;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

/**
 * A pop-up view layouter.
 * <p>
 * This class registers given popup View as a direct child of root View.
 * This makes the popup be able to appear to anywhere in the screen.
 * To control the position a Bounds should be specified. It will be converted into a LayoutParams
 * internally.
 */
public class PopUpLayouter<T extends View> {

  private final View parent;
  private final T contentView;

  private boolean isRegistered = false;

  public PopUpLayouter(View parent, T popUpView) {
    this.parent = Preconditions.checkNotNull(parent);
    this.contentView = Preconditions.checkNotNull(popUpView);
  }

  private void registerToViewHierarchyIfNecessary() {
    if (isRegistered) {
      return;
    }

    View rootView = parent.getRootView();
    if (rootView != null) {
      FrameLayout screenContent =
          FrameLayout.class.cast(rootView.findViewById(android.R.id.content));
      if (screenContent != null) {
        screenContent.addView(
            contentView, new FrameLayout.LayoutParams(0, 0, Gravity.LEFT | Gravity.TOP));
        isRegistered = true;
      }
    }
  }

  public T getContentView() {
    return contentView;
  }

  public void setBounds(Rect rect) {
    Preconditions.checkNotNull(rect);
    setBounds(rect.left, rect.top, rect.right, rect.bottom);
  }

  public void setBounds(int left, int top, int right, int bottom) {
    registerToViewHierarchyIfNecessary();

    int width = right - left;
    int height = bottom - top;

    ViewGroup.LayoutParams layoutParams = contentView.getLayoutParams();
    if (layoutParams != null) {
      layoutParams.width = width;
      layoutParams.height = height;
      if (MarginLayoutParams.class.isInstance(layoutParams)) {
        int x = left;
        int y = top;

        int[] location = new int[2];
        parent.getLocationInWindow(location);
        x += location[0];
        y += location[1];

        MarginLayoutParams marginLayoutParams = MarginLayoutParams.class.cast(layoutParams);
        // Clip XY.
        View rootView = View.class.cast(contentView.getParent());
        if (rootView != null) {
          x = MozcUtil.clamp(x, 0, rootView.getWidth() - width);
          y = MozcUtil.clamp(y, 0, rootView.getHeight() - height);
        }

        marginLayoutParams.setMargins(x, y, 0, 0);
      }
      contentView.setLayoutParams(layoutParams);
    }
  }
}
