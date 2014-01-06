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
    private final AnimationListener baseListener;
    OutAnimationAdapter(AnimationListener baseListener) {
      this.baseListener = baseListener;
    }

    @Override
    public void onAnimationEnd(Animation animation) {
      if (baseListener != null) {
        baseListener.onAnimationEnd(animation);
      }

      // We need to set this view's visibility to {@code GONE} *after* the out animation is
      // finished. This listener handles it.
      setVisibility(View.GONE);
    }

    @Override
    public void onAnimationRepeat(Animation animation) {
      if (baseListener != null) {
        baseListener.onAnimationRepeat(animation);
      }
    }

    @Override
    public void onAnimationStart(Animation animation) {
      if (baseListener != null) {
        baseListener.onAnimationStart(animation);
      }
    }
  }

  /**
   * An event listener for visibility change.
   * This interface (and relating methods) is an alternative of
   * View#onVisibilityChanged method,
   * which is introduced since API Level 8 (==Android 2.2).
   * TODO(yoichio): Get rid of this interface when we can end to support API Level 7
   * (==2.1) and replace with View#onVisibilityChanged.
   */
  public interface VisibilityChangeListener {
    public void onVisibilityChange(int oldvisibility, int newvisibility);
  }

  /** Animation used when this view is shown. */
  @VisibleForTesting Animation inAnimation;

  /** Animation used when this view is hidden. */
  @VisibleForTesting Animation outAnimation;

  /** AnimationListener for in-animation. */
  private AnimationListener inAnimationListener;

  /** AnimationListener for out-animation. */
  private AnimationListener outAnimationListener;

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
    // Cancel the current "in animation".
    inAnimation = loadAnimation(getContext(), resourceId);
    setAnimationListenerInternal(inAnimation, inAnimationListener);
  }

  public void setInAnimation(Animation inAnimation) {
    this.inAnimation = inAnimation;
    setAnimationListenerInternal(inAnimation, inAnimationListener);
  }

  public void setOutAnimation(int resourceId) {
    outAnimation = loadAnimation(getContext(), resourceId);
    setAnimationListenerInternal(outAnimation, new OutAnimationAdapter(outAnimationListener));
  }

  public void setOutAnimation(Animation outAnimation) {
    this.outAnimation = outAnimation;
    setAnimationListenerInternal(outAnimation, new OutAnimationAdapter(outAnimationListener));
  }

  private static Animation loadAnimation(Context context, int resourceId) {
    // Note: '0' is an invalid resource id.
    if (context == null || resourceId == 0) {
      return null;
    }
    return AnimationUtils.loadAnimation(context, resourceId);
  }

  public void setInAnimationListener(AnimationListener inAnimationListener) {
    this.inAnimationListener = inAnimationListener;
    setAnimationListenerInternal(inAnimation, inAnimationListener);
  }

  public void setOutAnimationListener(AnimationListener outAnimationListener) {
    this.outAnimationListener = outAnimationListener;
    setAnimationListenerInternal(outAnimation, new OutAnimationAdapter(outAnimationListener));
  }

  /**
   * Registers {@code listener} to {@code animation} as its callback, iff both are non-null.
   * Otherwise just do nothing.
   */
  private static void setAnimationListenerInternal(
      Animation animation, AnimationListener listener) {
    if (animation != null && listener != null) {
      animation.setAnimationListener(listener);
    }
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
  public void setVisibility(int visibility){
    int oldvisibility = getVisibility();
    super.setVisibility(visibility);
    if (oldvisibility != visibility && onVisibilityChangeListener != null) {
      onVisibilityChangeListener.onVisibilityChange(oldvisibility, visibility);
    }
  }

  public void setOnVisibilityChangeListener(VisibilityChangeListener listner) {
    onVisibilityChangeListener = listner;
  }
}
