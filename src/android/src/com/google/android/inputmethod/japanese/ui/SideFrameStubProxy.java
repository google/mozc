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

import org.mozc.android.inputmethod.japanese.view.Skin;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.content.res.Resources;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewStub;
import android.view.ViewStub.OnInflateListener;
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

  private Optional<View> currentView = Optional.absent();

  private Optional<ImageView> adjustButton = Optional.absent();
  private int inputFrameHeight = 0;
  private Optional<OnClickListener> buttonOnClickListener = Optional.absent();

  private int adjustButtonResourceId;
  private Skin skin = Skin.getFallbackInstance();

  private Resources resources;

  private void updateAdjustButtonImage() {
    Preconditions.checkState(adjustButton.isPresent());
    adjustButton.get().setImageDrawable(skin.getDrawable(resources, adjustButtonResourceId));
  }

  public void initialize(View view, int stubId, final int adjustButtonId,
                         int adjustButtonResourceId) {
    this.resources = Preconditions.checkNotNull(view).getResources();
    ViewStub viewStub = ViewStub.class.cast(view.findViewById(stubId));
    currentView = Optional.<View>of(viewStub);
    this.adjustButtonResourceId = adjustButtonResourceId;

    viewStub.setOnInflateListener(new OnInflateListener() {

      @SuppressWarnings("deprecation")
      @Override
      public void onInflate(ViewStub stub, View view) {
        inflated = true;

        currentView = Optional.of(view);
        currentView.get().setVisibility(View.VISIBLE);
        adjustButton = Optional.of(ImageView.class.cast(view.findViewById(adjustButtonId)));
        adjustButton.get().setOnClickListener(buttonOnClickListener.orNull());
        updateAdjustButtonImage();
        resetAdjustButtonBottomMarginInternal(inputFrameHeight);
      }
    });
  }

  public void setSkin(Skin skin) {
    this.skin = Preconditions.checkNotNull(skin);
    if (adjustButton.isPresent()) {
      updateAdjustButtonImage();
    }
  }

  public void setButtonOnClickListener(OnClickListener onClickListener) {
    buttonOnClickListener = Optional.of(Preconditions.checkNotNull(onClickListener));
  }

  public void setFrameVisibility(int visibility) {
    if (currentView.isPresent()) {
      currentView.get().setVisibility(visibility);
    }
  }

  private void resetAdjustButtonBottomMarginInternal(int inputFrameHeight) {
    if (adjustButton.isPresent()) {
      ImageView imageView = adjustButton.get();
      FrameLayout.LayoutParams layoutParams = FrameLayout.LayoutParams.class.cast(
          imageView.getLayoutParams());
      layoutParams.bottomMargin = (inputFrameHeight - layoutParams.height) / 2;
      imageView.setLayoutParams(layoutParams);
    }
  }

  public void resetAdjustButtonBottomMargin(int inputFrameHeight) {
    if (inflated) {
      resetAdjustButtonBottomMarginInternal(inputFrameHeight);
    }
    this.inputFrameHeight = inputFrameHeight;
  }
}
