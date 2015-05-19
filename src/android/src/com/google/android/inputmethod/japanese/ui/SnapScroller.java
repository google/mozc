// Copyright 2010-2013, Google Inc.
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

import org.mozc.android.inputmethod.japanese.MozcLog;

import android.view.animation.AnimationUtils;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.Interpolator;

/**
 * Similar class to {@code android.widget.Scroller}, but this class has "snapping" feature.
 *
 * This class treats such content which have "page"s.
 * Every page is supposed to have the same width.
 *
 * During the scroll animation, the velocity is reduced on every page edge position and
 * finally the animation stops on page edge.
 *
 */
public class SnapScroller {

  /**
   * Interface for getting current time stamp.
   *
   * By default {@code #DEFAULT_TIMESTAMP_CALCULATOR} is used.
   * You can use your own instance for testing via constructor.
   */
  interface TimestampCalculator {
    long getTimestamp();
  }

  /** The interpolator of scroll animation. */
  private static final Interpolator SCROLL_INTERPOLATOR = new DecelerateInterpolator();

  static final TimestampCalculator DEFAULT_TIMESTAMP_CALCULATOR = new TimestampCalculator() {
    @Override
    public long getTimestamp() {
      return AnimationUtils.currentAnimationTimeMillis();
    }
  };

  private static final Float ZERO = new Float(0);

  /** The time stamp calculator. */
  private final TimestampCalculator timestampCalculator;

  /** The size of each page (in pixel). */
  private int pageSize;

  /** The total content size (in pixel). */
  private int contentSize;

  private int viewSize;

  /**
   * Minimum velocity (in pixel per sec).
   *
   * If velocity is lower than this value, scroll animation stops.
   */
  private int minimumVelocity;

  /** The rate applied to {@code velocity} when scroll position reaches page edge position. */
  private float decayRate;

  /**
   * Current scroll position (in pixel).
   *
   * Updated by {@link #computeScrollOffset()}, {@link #scrollTo(int)} and {@link #scrollBy(int)}.
   */
  private int scrollPosition;

  /**
   * The scroll position at which scroll animation starts (in pixel).
   */
  private int startScrollPosition;

  /**
   * The scroll position to which scroll animation goes (in pixel).
   *
   * This value is always a multiple of {@code pageWidth}.
   * {@code Math.abs(scrollStartPosition - scrollEndPosition) <= pageWidth}
   * is guaranteed.
   * If given velocity is enough large, scroll animation is done repeatedly (scroll to
   * next page edge, and next page edge, and next....).
   */
  private int endScrollPosition;

  /** Scroll velocity (pixel per sec). */
  private int velocity;

  /** The timestamp when scroll animation starts. */
  private long startScrollTime;

  public SnapScroller() {
    this(null);
  }

  /** Injecting TimestampCalculator for testing */
  SnapScroller(TimestampCalculator timestampCalculator) {
    this.timestampCalculator =
        (timestampCalculator == null) ? DEFAULT_TIMESTAMP_CALCULATOR : timestampCalculator;
  }

  private static int clamp(int value, int min, int max) {
    return Math.max(Math.min(value, max), min);
  }

  public void setPageSize(int pageSize) {
    if (pageSize < 0) {
      throw new IllegalArgumentException("pageSize must be non-negative: " + pageSize);
    }
    this.pageSize = pageSize;
  }

  public int getPageSize() {
    return pageSize;
  }

  public void setContentSize(int contentSize) {
    if (contentSize < 0) {
      throw new IllegalArgumentException("contentSize must be non-negative: " + contentSize);
    }
    this.contentSize = contentSize;
  }

  public int getContentSize() {
    return contentSize;
  }

  public void setViewSize(int viewSize) {
    if (viewSize < 0) {
      throw new IllegalArgumentException("contentSize must be non-negative: " + contentSize);
    }
    this.viewSize = viewSize;
  }

  public int getViewSize() {
    return viewSize;
  }

  public int getMaxScrollPosition() {
    return Math.max(scrollPosition, Math.max(contentSize - viewSize, 0));
  }

  public void setMinimumVelocity(int minimumVelocity) {
    if (minimumVelocity < 0) {
      throw new IllegalArgumentException("minimumVelocity must be >= 0.");
    }
    this.minimumVelocity = minimumVelocity;
  }

  public void setDecayRate(float decayRate) {
    this.decayRate = decayRate;
  }

  public int getScrollPosition() {
    return scrollPosition;
  }

  public int getStartScrollPosition() {
    return startScrollPosition;
  }

  public int getEndScrollPosition() {
    return endScrollPosition;
  }

  public long getStartScrollTime() {
    return startScrollTime;
  }

  public void stopScrolling() {
    velocity = 0;
  }

  public boolean isScrolling() {
    return velocity != 0;
  }

  /**
   * Scrolls to {@code toPosition}.
   *
   * This method stops the scroll animation.
   * @param toPosition the position to be scrolled to.
   */
  public void scrollTo(int toPosition) {
    scrollPosition = clamp(toPosition, 0, getMaxScrollPosition());
    velocity = 0;
  }

  /**
   * Scrolls by {@code delta}.
   *
   * Completely equivalent to {@code scrollTo(getScrollPosition() + delta)}.
   * @param delta the delta to be scrolled by.
   */
  public void scrollBy(int delta) {
    scrollTo(scrollPosition + delta);
  }

  /**
   * Starts scroll animation.
   *
   * The scroll position is updated by calling {@link #computeScrollOffset()}.
   * While scrolling, following conditions are always guaranteed.
   * <ul>
   * <li>0 &lt;= {@link #getScrollPosition()}
   * <li>{@link #getScrollPosition()} &lt;= this.maxPosition
   * <li>this.scrollStatus is FINISHED or ABOUT_TO_FINISH.
   * </ul>
   * Now, the scroll will snap to page boundaries due to historical reason.
   * We may want to split snapping boundary and page size,
   * for example, to support snapping to each row/column instead of page boundaries.
   * @param velocity the velocity of scroll animation (pixel per sec).
   */
  public void fling(int velocity) {
    int pageSize = this.pageSize;
    if (Math.abs(velocity) < minimumVelocity || velocity == 0 || pageSize == 0) {
      // The velocity is low enough, or no page is available. So, do not scroll.
      this.velocity = 0;
      return;
    }

    int scrollPosition = this.scrollPosition;
    // Find the end scroll position heuristically.
    // It is basically the next page end. In more precise:
    // 1) For right/down scrolling, the end position should be the right/bottom side of
    //    the current page, which is the scrollPosition rounded to the nearest pageSize above.
    //    If the scrollPosition is already aligned to the pageSize, we'd like to scroll one more
    //    page.
    // 2) For left/up scrolling, the end position should be the left/top side of the current page,
    //    which is the scrollPosition rounded to the nearest pageSize below.
    //    Similar to 1), if the scrollPosition is already aligned to the pageSize, we'd like to
    //    scroll one more page.
    // The end scroll position should be clipped between 0 and (contentSize - pageSize) to avoid
    // exceeding the contents.
    int endScrollPosition = clamp(
        (velocity > 0)
            ? ((scrollPosition + pageSize) / pageSize * pageSize)
            : ((scrollPosition - 1) / pageSize * pageSize),
        0, getMaxScrollPosition());
    if (scrollPosition == endScrollPosition) {
      // No needs to scroll.
      // Reaches here typically when scrollPosition is at the head or the tail.
      velocity = 0;
    }

    // Update the fields.
    this.velocity = velocity;
    startScrollTime = timestampCalculator.getTimestamp();
    startScrollPosition = scrollPosition;
    this.endScrollPosition = endScrollPosition;
  }

  public Float computeScrollOffset() {
    if (velocity == 0) {
      return ZERO;
    }

    // Scroll animation is on going so the caller needs to call getScrollPosition().
    // Update the scroll position.
    long now = timestampCalculator.getTimestamp();
    int distance = endScrollPosition - startScrollPosition;
    // Note that distance and velocity have the same sign
    // so totalAnimationDuration becomes always positive.
    long totalAnimationDuration = 1000 * distance / velocity;
    long elapsedTime = Math.min(now - startScrollTime, totalAnimationDuration);
    float rateOfChange = totalAnimationDuration == 0
        ? 1f : SCROLL_INTERPOLATOR.getInterpolation(elapsedTime / (float) totalAnimationDuration);
    scrollPosition = (int) (startScrollPosition + distance * rateOfChange);
    if (elapsedTime == totalAnimationDuration) {
      // This scroll animation is finished.
      // Let's start next animation with decayed velocity.
      float oldVelocity = velocity;
      fling((int) (velocity * decayRate));
      if (velocity == 0) {
        if (scrollPosition == 0 || scrollPosition == getMaxScrollPosition()) {
          return Float.valueOf(oldVelocity);
        } else {
          return ZERO;
        }
      }
    }

    // Continue to scroll.
    return null;
  }
}
