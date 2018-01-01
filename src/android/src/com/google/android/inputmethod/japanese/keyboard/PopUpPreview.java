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

import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory.DrawableType;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.ui.PopUpLayouter;
import org.mozc.android.inputmethod.japanese.view.DrawableCache;
import org.mozc.android.inputmethod.japanese.view.Skin;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.annotation.SuppressLint;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.SparseArray;
import android.view.View;
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
@VisibleForTesting public class PopUpPreview {

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
         BackgroundDrawableFactory backgroundDrawableFactory, Resources resources) {
      this.parent = Preconditions.checkNotNull(parent);
      this.backgroundDrawableFactory = Preconditions.checkNotNull(backgroundDrawableFactory);
      this.drawableCache = new DrawableCache(Preconditions.checkNotNull(resources));
      this.dismissHandler = new Handler(Preconditions.checkNotNull(looper), new Handler.Callback() {
        @Override
        public boolean handleMessage(Message message) {
          PopUpPreview preview = PopUpPreview.class.cast(Preconditions.checkNotNull(message).obj);
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

    void setSkin(Skin skin) {
      drawableCache.setSkin(Preconditions.checkNotNull(skin));
    }
  }

  private final BackgroundDrawableFactory backgroundDrawableFactory;
  private final DrawableCache drawableCache;
  @VisibleForTesting final PopUpLayouter<ImageView> popUp;

  protected PopUpPreview(
      View parent, BackgroundDrawableFactory backgroundDrawableFactory,
      DrawableCache drawableCache) {
    this.backgroundDrawableFactory = Preconditions.checkNotNull(backgroundDrawableFactory);
    this.drawableCache = Preconditions.checkNotNull(drawableCache);
    ImageView popUpView = new ImageView(Preconditions.checkNotNull(parent).getContext());
    popUpView.setVisibility(View.GONE);
    this.popUp = new PopUpLayouter<ImageView>(parent, popUpView);
  }

  /**
   * Shows the pop-up preview of the given {@code key} and {@code optionalPopup} if needed.
   */
  @SuppressLint("NewApi")
  @SuppressWarnings("deprecation")
  protected void showIfNecessary(Key key, Optional<PopUp> optionalPopup, boolean isDelayedPopup) {
    Preconditions.checkNotNull(key);
    if (!Preconditions.checkNotNull(optionalPopup).isPresent()) {
      hidePopupView();
      return;
    }
    PopUp popup = optionalPopup.get();
    Optional<Drawable> popUpIconDrawable = drawableCache.getDrawable(isDelayedPopup
        ? popup.getPopUpLongPressIconResourceId() : popup.getPopUpIconResourceId());
    if (!popUpIconDrawable.isPresent()) {
      hidePopupView();
      return;
    }

    ImageView popupView = popUp.getContentView();
    Resources resources = popupView.getContext().getResources();
    float density = resources.getDisplayMetrics().density;
    int popUpWindowPadding = (int) (BackgroundDrawableFactory.POPUP_WINDOW_PADDING * density);
    int width =
        Math.min(key.getWidth(), resources.getDimensionPixelSize(R.dimen.popup_width_limitation))
        + popUpWindowPadding * 2;
    int height = popup.getHeight() + popUpWindowPadding * 2;

    popupView.setImageDrawable(popUpIconDrawable.get());
    popupView.setBackgroundDrawable(
        backgroundDrawableFactory.getDrawable(DrawableType.POPUP_BACKGROUND_WINDOW));

    Preconditions.checkState(popup.getIconWidth() != 0 || popup.getIconHeight() != 0);
    int horizontalPadding = (popup.getIconWidth() == 0)
        ? popUpWindowPadding : (width - popup.getIconWidth()) / 2;
    int verticalPadding = (popup.getIconHeight() == 0)
        ? popUpWindowPadding : (height - popup.getIconHeight()) / 2;
    popupView.setPadding(horizontalPadding, verticalPadding, horizontalPadding, verticalPadding);

    // Calculate the location to show the pop-up in window's coordinate system.
    int centerX = key.getX() + key.getWidth() / 2;
    int centerY = key.getY() + key.getHeight() / 2;

    int left = centerX + popup.getXOffset() - width / 2;
    int top = centerY + popup.getYOffset() - height / 2;
    popUp.setBounds(left, top, left + width, top + height);
    popupView.setVisibility(View.VISIBLE);
  }

  /**
   * Hides the pop up preview.
   */
  void dismiss() {
    hidePopupView();
  }

  @SuppressWarnings("deprecation")
  private void hidePopupView() {
    ImageView popupView = popUp.getContentView();
    popupView.setVisibility(View.GONE);
    popupView.setImageDrawable(null);
    popupView.setBackgroundDrawable(null);
  }
}
