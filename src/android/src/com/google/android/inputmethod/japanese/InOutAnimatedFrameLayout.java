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

import com.google.common.annotations.VisibleForTesting;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.animation.Animation;
import android.view.animation.Animation.AnimationListener;
import android.view.animation.AnimationUtils;
import android.widget.FrameLayout;

/**
 * FrameLayout supporting animation when the view is shown or hidden.
 *
 */
public abstract class InOutAnimatedFrameLayout extends FrameLayout {
  private class OutAnimationAdapter implements AnimationListener {
    @Override
    public void onAnimationEnd(Animation animation) {
      // We need to set this view's visibility to {@code GONE} *after* the out animation is
      // finished. This listener handles it.
      setVisibility(View.GONE);
    }

    @Override
    public void onAnimationRepeat(Animation animation) {
    }

    @Override
    public void onAnimationStart(Animation animation) {
    }
  }

  /**
   * An event listener for visibility change.
   */
  public interface VisibilityChangeListener {
    public void onVisibilityChange();
  }

  /** Animation used when this view is shown. */
  @VisibleForTesting Animation inAnimation;

  /** Animation used when this view is hidden. */
  @VisibleForTesting Animation outAnimation;

  @VisibleForTesting VisibilityChangeListener onVisibilityChangeListener = null;


  public InOutAnimatedFrameLayout(Context context) {
    super(context);
  }

  public InOutAnimatedFrameLayout(Context context, AttributeSet attrs) {
    super(context, attrs);
  }

  public InOutAnimatedFrameLayout(Context context, AttributeSet attrs, int defStyle) {
    super(context, attrs, defStyle);
  }

  public void setInAnimation(int resourceId) {
    setInAnimation(loadAnimation(getContext(), resourceId));
  }

  public void setInAnimation(Animation inAnimation) {
    this.inAnimation = inAnimation;
  }

  public void setOutAnimation(int resourceId) {
    setOutAnimation(loadAnimation(getContext(), resourceId));
  }

  public void setOutAnimation(Animation outAnimation) {
    this.outAnimation = outAnimation;
    if (outAnimation != null) {
      outAnimation.setAnimationListener(new OutAnimationAdapter());
    }
  }

  private static Animation loadAnimation(Context context, int resourceId) {
    // Note: '0' is an invalid resource id.
    if (context == null || resourceId == 0) {
      return null;
    }
    return AnimationUtils.loadAnimation(context, resourceId);
  }

  /**
   * Starts animation to show this view. This method also controls the view's visibility.
   */
  public void startInAnimation() {
    if (inAnimation == null) {
      // In case animation is not loaded, just set visibility.
      clearAnimation();
      setVisibility(View.VISIBLE);
      return;
    }

    Animation oldAnimation = getAnimation();
    if (getVisibility() != View.VISIBLE || (oldAnimation != null && oldAnimation != inAnimation)) {
      setVisibility(View.VISIBLE);
      startAnimation(inAnimation);
    }
  }

  /**
   * Starts animation to hide this view. This method also controls the view's visibility.
   * In more precise, the visibility will be set to {@code GONE}, when the animation is finished.
   */
  public void startOutAnimation() {
    if (outAnimation == null) {
      // In case animation is not loaded, just set visibility.
      clearAnimation();
      if (getVisibility() == View.VISIBLE) {
        setVisibility(View.GONE);
      }
      return;
    }

    if (getVisibility() == View.VISIBLE && getAnimation() != outAnimation) {
      startAnimation(outAnimation);
    }
  }

  @Override
  protected void onVisibilityChanged(View changedView, int visibility) {
    super.onVisibilityChanged(changedView, visibility);
    if (onVisibilityChangeListener != null) {
      onVisibilityChangeListener.onVisibilityChange();
    }
  }

  public void setOnVisibilityChangeListener(VisibilityChangeListener listner) {
    onVisibilityChangeListener = listner;
  }
}
