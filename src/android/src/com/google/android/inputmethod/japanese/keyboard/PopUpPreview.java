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

package org.mozc.android.inputmethod.japanese.keyboard;

import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory.DrawableType;
import org.mozc.android.inputmethod.japanese.view.DrawableCache;
import com.google.common.annotations.VisibleForTesting;

import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.SparseArray;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;
import android.widget.ImageView;

import java.util.ArrayList;
import java.util.List;

/**
 * This class represents a popup preview which is shown at key-pressing timing.
 *
 * Note that this should be package private class because it is just an implementation of
 * the preview used in {@link KeyboardView}. However, we annotate {@code public} to this class
 * just for testing purpose. The methods we want to mock also annotated as {@code protected}.
 *
 * Production clients shouldn't use this class from outside of this package.
 *
 */
public class PopUpPreview {

  /**
   * For performance reason, we want to reuse instances of {@link PopUpPreview}.
   * We start to support multi-touch, so the number of PopUpPreview we need can be two or more
   * (though, we expect the number of the previews is at most two in most cases).
   *
   * This class provides simple functionality of instance pooling of PopUpPreview.
   *
   */
  static class Pool {
    // Typically 2 or 3 popups are shown in maximum.
    private final SparseArray<PopUpPreview> pool = new SparseArray<PopUpPreview>(3);
    private final List<PopUpPreview> freeList = new ArrayList<PopUpPreview>();
    private final View parent;
    private final BackgroundDrawableFactory backgroundDrawableFactory;
    private final DrawableCache drawableCache;
    private final Handler dismissHandler;

    Pool(View parent, Looper looper,
         BackgroundDrawableFactory backgroundDrawableFactory, DrawableCache drawableCache) {
      this.parent = parent;
      this.backgroundDrawableFactory = backgroundDrawableFactory;
      this.drawableCache = drawableCache;
      this.dismissHandler = new Handler(looper, new Handler.Callback() {
        @Override
        public boolean handleMessage(Message message) {
          PopUpPreview preview = PopUpPreview.class.cast(message.obj);
          preview.dismiss();
          freeList.add(preview);
          return true;
        }
      });
    }

    PopUpPreview getInstance(int pointerId) {
      PopUpPreview preview = pool.get(pointerId);
      if (preview == null) {
        if (freeList.isEmpty()) {
          preview = new PopUpPreview(parent, backgroundDrawableFactory, drawableCache);
        } else {
          preview = freeList.remove(freeList.size() - 1);
        }
        pool.put(pointerId, preview);
      }
      return preview;
    }

    void releaseDelayed(int pointerId, long delay) {
      PopUpPreview preview = pool.get(pointerId);
      if (preview == null) {
        return;
      }
      pool.remove(pointerId);
      dismissHandler.sendMessageDelayed(
          dismissHandler.obtainMessage(0, preview), delay);
    }

    void releaseAll() {
      // Remove all messages.
      dismissHandler.removeMessages(0);

      for (int i = 0; i < pool.size(); ++i) {
        pool.valueAt(i).dismiss();
      }
      pool.clear();

      for (PopUpPreview preview : freeList) {
        preview.dismiss();
      }
      freeList.clear();

      drawableCache.clear();
    }
  }

  private static final float POPUP_VIEW_PADDING = 12f;

  private final int padding;
  private final View parent;
  private final BackgroundDrawableFactory backgroundDrawableFactory;
  private final DrawableCache drawableCache;
  @VisibleForTesting final ImageView popupView;

  protected PopUpPreview(
      View parent, BackgroundDrawableFactory backgroundDrawableFactory,
      DrawableCache drawableCache) {
    this.parent = parent;
    this.backgroundDrawableFactory = backgroundDrawableFactory;
    this.drawableCache = drawableCache;
    this.popupView = new ImageView(parent.getContext());
    popupView.setVisibility(View.GONE);
    View rootView = parent.getRootView();
    if (rootView != null) {
      FrameLayout screenContent =
          FrameLayout.class.cast(rootView.findViewById(android.R.id.content));
      if (screenContent != null) {
        screenContent.addView(
            popupView, new FrameLayout.LayoutParams(0, 0, Gravity.LEFT | Gravity.TOP));
      }
    }
    padding = (int) (popupView.getResources().getDisplayMetrics().density * POPUP_VIEW_PADDING);
  }

  /**
   * Shows the popup preview of the given {@code key} and {@code popup} if needed.
   */
  protected void showIfNecessary(Key key, PopUp popup) {
    if (key == null || popup == null) {
      // No images to be rendered.
      hidePopupView();
      return;
    }

    // Set images.
    popupView.setImageDrawable(drawableCache.getDrawable(popup.getPopUpIconResourceId()));
    popupView.setBackgroundDrawable(
        backgroundDrawableFactory.getDrawable(DrawableType.POPUP_BACKGROUND_WINDOW));
    popupView.setPadding(padding, padding, padding, padding);

    // Calculate the location to show the popup in window's coordinate system.
    int centerX = key.getX() + key.getWidth() / 2;
    int centerY = key.getY() + key.getHeight() / 2;

    int width = popup.getWidth();
    int height = popup.getHeight();

    ViewGroup.LayoutParams layoutParams = popupView.getLayoutParams();
    if (layoutParams != null) {
      layoutParams.width = width;
      layoutParams.height = height;
    }

    if (MarginLayoutParams.class.isInstance(layoutParams)) {
      int x = centerX + popup.getXOffset() - width / 2;
      int y = centerY + popup.getYOffset() - height / 2;

      int[] location = new int[2];
      parent.getLocationInWindow(location);
      x += location[0];
      y += location[1];

      MarginLayoutParams marginLayoutParams = MarginLayoutParams.class.cast(layoutParams);
      // Clip XY.
      View root = View.class.cast(popupView.getParent());
      if (root != null) {
        x = Math.max(Math.min(x, root.getWidth() - width), 0);
        y = Math.max(Math.min(y, root.getHeight() - height), 0);
      }
      marginLayoutParams.setMargins(x, y, 0, 0);
    }
    popupView.setLayoutParams(layoutParams);
    popupView.setVisibility(View.VISIBLE);
  }

  /**
   * Hides the pop up preview.
   * protected only for testing.
   */
  protected void dismiss() {
    hidePopupView();
  }

  private void hidePopupView() {
    popupView.setVisibility(View.GONE);
    popupView.setImageDrawable(null);
    popupView.setBackgroundDrawable(null);
  }
}
