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

package org.mozc.android.inputmethod.japanese;

import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory;
import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory.DrawableType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Row;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Span;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayoutRenderer;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayouter;
import org.mozc.android.inputmethod.japanese.ui.SnapScroller;
import org.mozc.android.inputmethod.japanese.view.SkinType;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Paint.Align;
import android.graphics.RectF;
import android.support.v4.view.ViewCompat;
import android.support.v4.widget.EdgeEffectCompat;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.GestureDetector.SimpleOnGestureListener;
import android.view.MotionEvent;
import android.view.View;

/**
 * A view for candidate words.
 *
 */
abstract class CandidateWordView extends View {

  /**
   * Callback interface to handle candidate selection.
   */
  interface CandidateSelectListener {

    /**
     * Called {@code onCandidateSelected}, when a candidate is selected by user's action.
     */
    public void onCandidateSelected(CandidateWord candidateWord);
  }

  /**
   * Handles gestures to scroll candidate list and choose a candidate.
   */
  class CandidateWordGestureDetector {
    class CandidateWordViewGestureListener extends SimpleOnGestureListener {
      @Override
      public boolean onFling(
          MotionEvent event1, MotionEvent event2, float velocityX, float velocityY) {
        float velocity = orientationTrait.projectVector(velocityX, velocityY);
        // As fling is started, current action is not tapping.
        // Reset pressing state so that candidate selection is not triggered at touch up event.
        releaseCandidate();
        // Fling makes scrolling.
        scroller.fling(-(int) velocity);
        invalidate();
        return true;
      }

      @Override
      public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
        float distance = orientationTrait.projectVector(distanceX, distanceY);
        int oldScrollPosition = scroller.getScrollPosition();
        int oldMaxScrollPosition = scroller.getMaxScrollPosition();
        scroller.scrollBy((int) distance);
        orientationTrait.scrollTo(CandidateWordView.this, scroller.getScrollPosition());
        // As scroll is started, current action is not tapping.
        // Reset pressing state so that candidate selection is not triggered at touch up event.
        releaseCandidate();

        // Edge effect. Now, in production, we only support vertical scroll.
        if (oldScrollPosition + distance < 0) {
          topEdgeEffect.onPull(distance / getHeight());
          if (!bottomEdgeEffect.isFinished()) {
            bottomEdgeEffect.onRelease();
          }
        } else if (oldScrollPosition + distance > oldMaxScrollPosition) {
          bottomEdgeEffect.onPull(distance / getHeight());
          if (!topEdgeEffect.isFinished()) {
            topEdgeEffect.onRelease();
          }
        }

        invalidate();
        return true;
      }
    }

    // GestureDetector cannot handle all complex gestures which we need.
    // But we use GestureDetector for some gesture recognition
    // because implementing whole gesture detection logic by ourselves is a bit tedious.
    final GestureDetector gestureDetector;

    /**
     * Points to an instance of currently pressed candidate word. Or {@code null} if any
     * candidates aren't pressed.
     */
    private CandidateWord pressedCandidate;
    private final RectF candidateRect = new RectF();

    public CandidateWordGestureDetector(Context context) {
      gestureDetector = new GestureDetector(context, new CandidateWordViewGestureListener());
    }

    private void pressCandidate(Row row, Span span) {
      pressedCandidate = span.getCandidateWord();
      // TODO(yamaguchi):maybe better to make this rect larger by several pixels to avoid that
      // users fail to select a candidate by unconscious small movement of tap point.
      // (i.e. give hysterisis for noise reduction)
      // Needs UX study.
      candidateRect.set(span.getLeft(), row.getTop(),
                        span.getRight(), row.getTop() + row.getHeight());
    }

    private void releaseCandidate() {
      pressedCandidate = null;
    }

    CandidateWord getPressedCandidate() {
      return pressedCandidate;
    }

    /**
     * Checks if a down event is fired inside a candidate rectangle.
     * If so, begin pressing it.
     *
     * It is assumed that rows are stored in up-to-down order,
     * and spans are in left-to-right order.
     *
     * @param scrolledX X coordinate of down event point including scroll offset
     * @param scrolledY Y coordinate of down event point including scroll offset
     * @return true if the down event is fired inside a candidate rectangle.
     */
    private boolean findCandidateAndPress(float scrolledX, float scrolledY) {
      if (calculatedLayout == null) {
        return false;
      }
      for (Row row : calculatedLayout.getRowList()) {
        if (scrolledY < row.getTop()) {
          break;
        }
        if (scrolledY >= row.getTop() + row.getHeight()) {
          continue;
        }
        for (Span span : row.getSpanList()) {
          if (scrolledX < span.getLeft()) {
            break;
          }
          if (scrolledX >= span.getRight()) {
            continue;
          }
          pressCandidate(row, span);
          invalidate();
          return true;
        }
        return false;
      }
      return false;
    }

    boolean onTouchEvent(MotionEvent event) {
      if (gestureDetector.onTouchEvent(event)) {
        return true;
      }

      float scrolledX = event.getX() + getScrollX();
      float scrolledY = event.getY() + getScrollY();
      switch (event.getAction()) {
        case MotionEvent.ACTION_DOWN:
          findCandidateAndPress(scrolledX, scrolledY);
          scroller.stopScrolling();
          if (!topEdgeEffect.isFinished()) {
            topEdgeEffect.onRelease();
          }
          if (!bottomEdgeEffect.isFinished()) {
            bottomEdgeEffect.onRelease();
          }
          return true;
        case MotionEvent.ACTION_MOVE:
          if (pressedCandidate != null) {
            // Turn off highlighting if contact point gets out of the candidate.
            if (!candidateRect.contains(scrolledX, scrolledY)) {
              releaseCandidate();
              invalidate();
            }
          }
          return true;
        case MotionEvent.ACTION_CANCEL:
          if (pressedCandidate != null) {
            releaseCandidate();
            invalidate();
          }
          return true;
        case MotionEvent.ACTION_UP:
          if (pressedCandidate != null) {
            if (candidateRect.contains(scrolledX, scrolledY) && candidateSelectListener != null) {
              candidateSelectListener.onCandidateSelected(pressedCandidate);
            }
            releaseCandidate();
            invalidate();
          }
          return true;
      }
      return false;
    }
  }

  /**
   * Polymorphic behavior based on scroll orientation.
   */
  // TODO(hidehiko): rename OrientationTrait to OrientationTraits.
  interface OrientationTrait {
    /** @return scroll position of which direction corresponds to the orientation. */
    int getScrollPosition(View view);

    /** @return the projected value. */
    float projectVector(float x, float y);

    /** Scrolls to {@code position}. {@code position} is applied to corresponding axis. */
    void scrollTo(View view, int position);

    /** @return left or top position based on the orientation. */
    float getCandidatePosition(Row row, Span span);

    /** @return width or height based on the orientation. */
    float getCandidateLength(Row row, Span span);

    /** @return view's width or height based on the orientation. */
    int getViewLength(View view);

    /** @return the page size of the layout for the scroll orientation. */
    int getPageSize(CandidateLayouter layouter);

    /** @return the content size for the scroll orientation of the layout. */
    float getContentSize(CandidateLayout layout);
  }

  enum Orientation implements OrientationTrait {
    HORIZONTAL {
      @Override
      public int getScrollPosition(View view) {
        return view.getScrollX();
      }
      @Override
      public void scrollTo(View view, int position) {
        view.scrollTo(position, 0);
      }
      @Override
      public float getCandidatePosition(Row row, Span span) {
        return span.getLeft();
      }
      @Override
      public float getCandidateLength(Row row, Span span) {
        return span.getWidth();
      }
      @Override
      public int getViewLength(View view) {
        return view.getWidth();
      }
      @Override
      public float projectVector(float x, float y) {
        return x;
      }
      @Override
      public int getPageSize(CandidateLayouter layouter) {
        return layouter.getPageWidth();
      }
      @Override
      public float getContentSize(CandidateLayout layout) {
        return layout.getContentWidth();
      }
    },
    VERTICAL {
      @Override
      public int getScrollPosition(View view) {
        return view.getScrollY();
      }
      @Override
      public void scrollTo(View view, int position) {
        view.scrollTo(0, position);
      }
      @Override
      public float getCandidatePosition(Row row, Span span) {
        return row.getTop();
      }
      @Override
      public float getCandidateLength(Row row, Span span) {
        return row.getHeight();
      }
      @Override
      public int getViewLength(View view) {
        return view.getHeight();
          }
      @Override
      public float projectVector(float x, float y) {
        return y;
      }
      @Override
      public int getPageSize(CandidateLayouter layouter) {
        return layouter.getPageHeight();
      }
      @Override
      public float getContentSize(CandidateLayout layout) {
        return layout.getContentHeight();
      }
    };
  }

  private CandidateSelectListener candidateSelectListener;

  // Finally, we only need vertical scrolling.
  // TODO(hidehiko): Remove horizontal scrolling related codes.
  private final EdgeEffectCompat topEdgeEffect = new EdgeEffectCompat(getContext());
  private final EdgeEffectCompat bottomEdgeEffect = new EdgeEffectCompat(getContext());

  // The Scroller which manages the status of scrolling the view.
  // Default behavior of ScrollView does not suffice our UX design
  // so we introduced this Scroller.
  // TODO(matsuzakit): The parameter is TBD (needs UX study?).
  protected final SnapScroller scroller = new SnapScroller();
  // The CandidateLayouter which calculates the layout of candidate words.
  private CandidateLayouter layouter;
  // The calculated layout, created by this.layouter.
  protected CandidateLayout calculatedLayout;
  // The CandidateList which is currently shown on the view.
  protected CandidateList currentCandidateList;
  // The Y position where the last touch event occurs.
  float lastEventPosition;

  // No padding by default.
  private int horizontalPadding = 0;

  protected final CandidateLayoutRenderer candidateLayoutRenderer =
      new CandidateLayoutRenderer(this);

  CandidateWordGestureDetector candidateWordGestureDetector =
      new CandidateWordGestureDetector(getContext());

  // Scroll orientation.
  private final OrientationTrait orientationTrait;

  protected final BackgroundDrawableFactory backgroundDrawableFactory =
      new BackgroundDrawableFactory(getResources().getDisplayMetrics().density);
  private DrawableType backgroundDrawableType = null;

  CandidateWordView(Context context, OrientationTrait orientationFeature) {
    super(context);
    this.orientationTrait = orientationFeature;
  }

  CandidateWordView(Context context, AttributeSet attributeSet,
                    OrientationTrait orientationTrait) {
    super(context, attributeSet);
    this.orientationTrait = orientationTrait;
  }

  CandidateWordView(Context context, AttributeSet attributeSet, int defaultStyle,
                    OrientationTrait orientationTrait) {
    super(context, attributeSet, defaultStyle);
    this.orientationTrait = orientationTrait;
  }

  void setCandidateSelectListener(CandidateSelectListener candidateSelectListener) {
    this.candidateSelectListener = candidateSelectListener;
  }

  CandidateLayouter getCandidateLayouter() {
    return layouter;
  }

  void setCandidateLayouter(CandidateLayouter layouter) {
    this.layouter = layouter;
  }

  protected void setHorizontalPadding(int horizontalPadding) {
    this.horizontalPadding = horizontalPadding;
    updateLayouter();
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    super.onLayout(changed, left, top, right, bottom);
    int width = Math.max(right - left - horizontalPadding * 2, 0);
    int height = bottom - top;
    if (layouter != null && layouter.setViewSize(width, height)) {
      updateCalculatedLayout();
    }
    updateScroller();
  }

  @Override
  public boolean onTouchEvent(MotionEvent event) {
    return candidateWordGestureDetector.onTouchEvent(event);
  }

  @Override
  protected void onAttachedToWindow() {
    super.onAttachedToWindow();
    candidateLayoutRenderer.onAttachedToWindow();
  }

  @Override
  protected void onDetachedFromWindow() {
    candidateLayoutRenderer.onDetachedFromWindow();
    super.onDetachedFromWindow();
  }

  public void setEmojiProviderType(EmojiProviderType providerType) {
    candidateLayoutRenderer.setEmojiProviderType(providerType);
  }

  @Override
  public void draw(Canvas canvas) {
    super.draw(canvas);

    // Render edge effect.
    boolean postInvalidateIsNeeded = false;
    if (!topEdgeEffect.isFinished()) {
      int saveCount = canvas.save();
      try {
        canvas.translate(0, Math.min(0, getScrollY()));
        topEdgeEffect.setSize(getWidth(), getHeight());
        if (topEdgeEffect.draw(canvas)) {
          postInvalidateIsNeeded = true;
        }
      } finally {
        canvas.restoreToCount(saveCount);
      }
    }

    if (!bottomEdgeEffect.isFinished()) {
      int saveCount = canvas.save();
      try {
        int width = getWidth();
        int height = getHeight();
        canvas.translate(-width, getScrollY() + height);
        canvas.rotate(180, width, 0);
        bottomEdgeEffect.setSize(width, height);
        if (bottomEdgeEffect.draw(canvas)) {
          postInvalidateIsNeeded = true;
        }
      } finally {
        canvas.restoreToCount(saveCount);
      }
    }

    if (postInvalidateIsNeeded) {
      ViewCompat.postInvalidateOnAnimation(this);
    }
  }

  @Override
  protected void onDraw(Canvas canvas) {
    super.onDraw(canvas);
    if (calculatedLayout == null || currentCandidateList == null) {
      // No layout is available.
      return;
    }

    // Paint the candidates.
    canvas.save();
    try {
      canvas.translate(horizontalPadding, 0);
      CandidateWord pressedCandidate = candidateWordGestureDetector.getPressedCandidate();
      int pressedCandidateIndex = (pressedCandidate != null && pressedCandidate.hasIndex())
          ? pressedCandidate.getIndex() : -1;
      candidateLayoutRenderer.drawCandidateLayout(canvas, calculatedLayout, pressedCandidateIndex);
    } finally {
      canvas.restore();
    }
  }

  @Override
  public final void computeScroll() {
    if (scroller.isScrolling()) {
      // If still scrolling, update the scroll position and invalidate the window.
      Float velocity = scroller.computeScrollOffset();
      orientationTrait.scrollTo(this, scroller.getScrollPosition());
      if (velocity != null) {
        // The end of scrolling. Check edge effect.
        if (velocity < 0) {
          topEdgeEffect.onAbsorb(velocity.intValue());
          if (!bottomEdgeEffect.isFinished()) {
            bottomEdgeEffect.onRelease();
          }
        } else if (velocity > 0) {
          bottomEdgeEffect.onAbsorb(velocity.intValue());
          if (!topEdgeEffect.isFinished()) {
            topEdgeEffect.onRelease();
          }
        }
      }

      // This invalidation makes next scrolling.
      ViewCompat.postInvalidateOnAnimation(this);
    }
    super.computeScroll();
  }

  private int getUpdatedScrollPosition(Row row, Span span) {
    int scrollPosition = orientationTrait.getScrollPosition(this);
    float candidatePosition = orientationTrait.getCandidatePosition(row, span);
    float candidateLength = orientationTrait.getCandidateLength(row, span);
    int viewLength = orientationTrait.getViewLength(this);
    if (candidatePosition < scrollPosition ||
        candidatePosition + candidateLength > scrollPosition + viewLength) {
      return (int) candidatePosition;
    } else {
      return scrollPosition;
    }
  }

  /**
   * If focused candidate is invisible (including partial invisible),
   * update scroll position to see the candidate.
   */
  protected void updateScrollPositionBasedOnFocusedIndex() {
    int scrollPosition = 0;
    if (calculatedLayout != null && currentCandidateList != null) {
      int focusedIndex = currentCandidateList.getFocusedIndex();
      row_loop: for (Row row : calculatedLayout.getRowList()) {
        for (Span span : row.getSpanList()) {
          CandidateWord candidateWord = span.getCandidateWord();
          if (candidateWord == null) {
            continue;
          }
          if (candidateWord.getIndex() == focusedIndex) {
            scrollPosition = getUpdatedScrollPosition(row, span);
            break row_loop;
          }
        }
      }
    }

    setScrollPosition(scrollPosition);
  }

  void setScrollPosition(int position) {
    scroller.scrollTo(position);
    orientationTrait.scrollTo(this, scroller.getScrollPosition());
    invalidate();
  }

  void update(CandidateList candidateList) {
    CandidateList previousCandidateList = currentCandidateList;
    currentCandidateList = candidateList;
    candidateLayoutRenderer.setCandidateList(candidateList);
    if (layouter != null && !equals(candidateList, previousCandidateList)) {
      updateCalculatedLayout();
    }
    updateScroller();
    invalidate();
  }

  private static boolean equals(CandidateList list1, CandidateList list2) {
    if (list1 == list2) {
      return true;
    }
    if (list1 == null || list2 == null) {
      return false;
    }

    return list1.getCandidatesList().equals(list2.getCandidatesList());
  }

  /**
   * Updates the layouter, and also updates the calculatedLayout based on the updated layouter.
   *
   * TODO(hidehiko): This method is remaining here to reduce a CL size smaller
   * in order to make refactoring step by step. This will be cleaned when CandidateWordView
   * is refactored.
   */
  protected final void updateLayouter() {
    updateCalculatedLayout();
    updateScroller();
  }

  /**
   * Updates the calculatedLayout if possible.
   */
  private void updateCalculatedLayout() {
    if (currentCandidateList == null || layouter == null) {
      calculatedLayout = null;
    } else {
      calculatedLayout = layouter.layout(currentCandidateList);
    }
  }

  private void updateScroller() {
    if (calculatedLayout == null || layouter == null) {
      scroller.setPageSize(0);
      scroller.setContentSize(0);
    } else {
      int pageSize = orientationTrait.getPageSize(layouter);
      int contentSize =
          (calculatedLayout == null) ? 0 : (int) orientationTrait.getContentSize(calculatedLayout);
      if (pageSize != 0) {
        // Ceil to align pages.
        contentSize = (contentSize + pageSize - 1) / pageSize * pageSize;
      }
      scroller.setPageSize(pageSize);
      scroller.setContentSize(contentSize);
    }
    scroller.setViewSize(orientationTrait.getViewLength(this));
  }

  public CandidateList getCandidateList() {
    return currentCandidateList;
  }

  /**
   * Utility method for creating paint instance.
   */
  protected static Paint createPaint(
      boolean antiAlias, int color, Align textAlign, float textSize) {
    Paint paint = new Paint();
    paint.setAntiAlias(antiAlias);
    paint.setColor(color);
    paint.setTextAlign(textAlign);
    paint.setTextSize(textSize);
    return paint;
  }

  protected void setBackgroundDrawableType(DrawableType drawableType) {
    backgroundDrawableType = drawableType;
    resetBackground();
  }

  private void resetBackground() {
    candidateLayoutRenderer.setSpanBackgroundDrawable(
        backgroundDrawableFactory.getDrawable(backgroundDrawableType));
  }

  void setSkinType(SkinType skinType) {
    backgroundDrawableFactory.setSkinType(skinType);
    resetBackground();
  }
}
