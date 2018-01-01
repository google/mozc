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
import org.mozc.android.inputmethod.japanese.ViewEventListener;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.Candidates;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.Candidates.Candidate;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.Category;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import org.mozc.android.inputmethod.japanese.resources.R;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Paint.Align;
import android.graphics.Paint.FontMetrics;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.view.MotionEvent;

import java.util.Locale;

/**
 * Layouts floating candidate window and draw it's contents on canvas.
 *
 * The point of origin of layout is NOT a left-top corner of candidate list BUT the left-top corner
 * of the candidate column and the right-top corner of the shortcut column.
 *
 * TODO(hsumita): Rewrite using LinearLayout or something.
 */
public class FloatingCandidateLayoutRenderer {

  private static class WindowRects {

    public final Rect window;
    public final Optional<Rect> focus;
    public final Optional<Rect> pageIndicator;
    public final Optional<RectF> scrollIndicator;

    WindowRects(Rect window, Optional<Rect> focus, Optional<Rect> pageIndicator,
                Optional<RectF> scrollIndicator) {
      this.window = Preconditions.checkNotNull(window);
      this.focus = Preconditions.checkNotNull(focus);
      this.pageIndicator = Preconditions.checkNotNull(pageIndicator);
      this.scrollIndicator = Preconditions.checkNotNull(scrollIndicator);
    }
  }

  /** Locale field for {@link Paint#setTextLocale(Locale)}. */
  private static final Optional<Locale> TEXT_LOCALE = (Build.VERSION.SDK_INT >= 17)
      ? Optional.of(Locale.JAPAN) : Optional.<Locale>absent();

  private static final String FOOTER_TEXT_FORMAT = "%d / %d";

  private final Paint candidatePaint;
  private final Paint focusedCandidatePaint;
  private final Paint descriptionPaint;
  private final Paint shortcutPaint;
  private final Paint footerPaint;
  private final Paint separatorPaint;
  private final Paint windowBackgroundPaint;
  private final Paint focuseBackgroundPaint;
  private final Paint scrollIndicatorPaint;

  private final int windowMinimumWidth;
  private final int windowHorizontalPadding;
  private final float windowRoundRectRadius;
  private final int candidateHeight;
  private final int candidateOffsetY;
  private final int candidateDescriptionMinimumPadding;
  private final int footerHeight;
  private final float footerTextCenterToBaseLineOffset;
  private final int horizontalSeparatorPadding;
  private final int shortcutWidth;
  private final float shortcutCenterX;
  private final int scrollIndicatorWidth;
  private final int scrollIndicatorRadius;

  private Optional<WindowRects> windowRects = Optional.absent();
  private Optional<ViewEventListener> viewEventListener = Optional.absent();
  private Optional<Candidates> candidates = Optional.absent();
  private Optional<Integer> maxWidth = Optional.absent();
  /** Focused candidate index, or tapped candidate index if exists. */
  private Optional<Integer> focusedOrTappedCandidateIndexOnPage = Optional.absent();
  /** TappedInfo for the current touch operation. Set on TOUCH_DOWN, reset on TOUCH_UP. */
  private Optional<Integer> tappingCandidateIndex = Optional.absent();
  private int totalCandidatesCount;
  private int maxCandidateWidth;
  private int maxDescriptionWidth;

  public FloatingCandidateLayoutRenderer(Resources res) {
    Preconditions.checkNotNull(res);

    candidatePaint = new Paint();
    candidatePaint.setColor(res.getColor(R.color.floating_candidate_text));
    candidatePaint.setTextSize(res.getDimension(R.dimen.floating_candidate_text_size));
    candidatePaint.setAntiAlias(true);
    if (TEXT_LOCALE.isPresent()) {
      candidatePaint.setTextLocale(TEXT_LOCALE.get());
    }

    focusedCandidatePaint = new Paint(candidatePaint);
    focusedCandidatePaint.setColor(res.getColor(R.color.floating_candidate_focused_text));

    descriptionPaint = new Paint(candidatePaint);
    descriptionPaint.setTextSize(
        res.getDimension(R.dimen.floating_candidate_description_text_size));
    descriptionPaint.setColor(res.getColor(R.color.floating_candidate_description_text));

    shortcutPaint = new Paint(candidatePaint);
    shortcutPaint.setTextSize(res.getDimension(R.dimen.floating_candidate_shortcut_text_size));
    shortcutPaint.setColor(res.getColor(R.color.floating_candidate_shortcut_text));

    scrollIndicatorPaint = new Paint();
    scrollIndicatorPaint.setColor(res.getColor(R.color.floating_candidate_scroll_indicator));

    footerPaint = new Paint(candidatePaint);
    footerPaint.setTextSize(res.getDimension(R.dimen.floating_candidate_footer_text_size));
    footerPaint.setColor(res.getColor(R.color.floating_candidate_footer_text));

    separatorPaint = new Paint();
    separatorPaint.setStrokeWidth(
        res.getDimension(R.dimen.floating_candidate_separator_width));
    separatorPaint.setColor(res.getColor(R.color.floating_candidate_footer_separator));

    windowBackgroundPaint = new Paint();
    windowBackgroundPaint.setColor(res.getColor(R.color.floating_candidate_window_background));
    windowBackgroundPaint.setShadowLayer(
        res.getDimension(R.dimen.floating_candidate_window_shadow_radius),
        0, res.getDimension(R.dimen.floating_candidate_window_shadow_offset_y),
        res.getColor(R.color.floating_candidate_shadow));

    focuseBackgroundPaint = new Paint();
    focuseBackgroundPaint.setColor(res.getColor(R.color.floating_candidate_focus_background));

    float candidateVerticalPadding =
        res.getDimension(R.dimen.floating_candidate_candidate_vertical_padding);
    FontMetrics candidateMetrics = candidatePaint.getFontMetrics();
    candidateHeight = (int) Math.ceil(
        candidateMetrics.descent - candidateMetrics.ascent + candidateVerticalPadding * 2);
    candidateOffsetY = (int) Math.ceil(-candidateMetrics.ascent + candidateVerticalPadding);

    windowMinimumWidth = res.getDimensionPixelSize(R.dimen.floating_candidate_window_minimum_width);
    windowHorizontalPadding =
        res.getDimensionPixelOffset(R.dimen.floating_candidate_window_horizontal_padding);
    windowRoundRectRadius = res.getDimension(R.dimen.floating_candidate_window_round_rect_radius);
    candidateDescriptionMinimumPadding =
        res.getDimensionPixelSize(R.dimen.floating_candidate_candidate_description_minimum_padding);
    horizontalSeparatorPadding =
        res.getDimensionPixelSize(R.dimen.floating_candidate_separator_horizontal_padding);

    scrollIndicatorWidth =
        res.getDimensionPixelSize(R.dimen.floating_candidate_scroll_indicator_width);
    scrollIndicatorRadius =
        res.getDimensionPixelSize(R.dimen.floating_candidate_scroll_indicator_radius);

    FontMetrics footerMetrics = footerPaint.getFontMetrics();
    float footerTextHeight = -footerMetrics.ascent + footerMetrics.descent;
    footerHeight = Math.round(footerTextHeight * 2f);
    footerTextCenterToBaseLineOffset = (-footerMetrics.ascent - footerMetrics.descent) / 2f;

    float shortcutCharacterWidth = shortcutPaint.measureText("m");
    float shortcutCandidatePadding =
        res.getDimensionPixelSize(R.dimen.floating_candidate_shortcut_candidate_padding);
    shortcutWidth = Math.round(shortcutCharacterWidth + shortcutCandidatePadding);
    shortcutCenterX = -shortcutCharacterWidth / 2f - shortcutCandidatePadding;

    updateLayout();
  }

  /** Handle touch event and invoke some actions. */
  public void onTouchEvent(MotionEvent event) {
    if (!candidates.isPresent() || !viewEventListener.isPresent()) {
      return;
    }
    ViewEventListener listener = viewEventListener.get();

    Optional<Integer> optionalCandidateIndex = getTappingCandidate(event);

    if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
      tappingCandidateIndex = optionalCandidateIndex;
      updateLayout();
      return;
    }
    if (event.getActionMasked() != MotionEvent.ACTION_UP) {
      return;
    }

    if (!optionalCandidateIndex.isPresent() || !tappingCandidateIndex.isPresent()
        || !optionalCandidateIndex.equals(tappingCandidateIndex)) {
      tappingCandidateIndex = Optional.absent();
      updateLayout();
      return;
    }
    int candidateIndex = optionalCandidateIndex.get();
    tappingCandidateIndex = Optional.absent();

    listener.onConversionCandidateSelected(
        candidates.get().getCandidate(candidateIndex).getId(),
        Optional.<Integer>absent());
  }

  /** Sets the max width of this window. */
  public void setMaxWidth(int maxWidth) {
    if (maxWidth > 0) {
      this.maxWidth = Optional.of(maxWidth);
    } else {
      this.maxWidth = Optional.absent();
    }
    updateLayout();
  }

  /** Sets candidates. */
  public void setCandidates(Command outCommand) {
    Preconditions.checkNotNull(outCommand);
    if (outCommand.getOutput().getCandidates().getCandidateCount() == 0) {
      candidates = Optional.<Candidates>absent();
      totalCandidatesCount = 0;
    } else {
      candidates = Optional.of(outCommand.getOutput().getCandidates());
      totalCandidatesCount = outCommand.getOutput().getAllCandidateWords().getCandidatesCount();
    }
    updateLayout();
  }

  /** Sets a view event listener to handle touch events. */
  public void setViewEventListener(ViewEventListener listener) {
    viewEventListener = Optional.of(listener);
  }

  /**
   * Gets the rectangle of this window.
   * Defensive-copied value is returned so caller-side can modify it.
   */
  public Optional<Rect> getWindowRect() {
    if (windowRects.isPresent()) {
      return Optional.of(new Rect(windowRects.get().window));
    } else {
      return Optional.absent();
    }
  }

  /** Draws this candidate window. */
  public void draw(Canvas canvas) {
    Preconditions.checkNotNull(canvas);
    Preconditions.checkState(candidates.isPresent());
    Preconditions.checkState(windowRects.isPresent());

    Candidates candidatesData = candidates.get();
    WindowRects rects = windowRects.get();

    canvas.drawRoundRect(
        new RectF(rects.window), windowRoundRectRadius, windowRoundRectRadius,
        windowBackgroundPaint);

    if (rects.focus.isPresent()) {
      canvas.drawRect(rects.focus.get(), focuseBackgroundPaint);
    }

    // Candidates, descriptions and shortcuts.
    int focusedIndex = focusedOrTappedCandidateIndexOnPage.or(-1);
    for (int i = 0; i < candidatesData.getCandidateCount(); ++i) {
      Candidate candidate = candidatesData.getCandidate(i);
      int offsetY = getCandidateRowOffsetY(i) + candidateOffsetY;
      Paint paint = (i == focusedIndex) ? focusedCandidatePaint : candidatePaint;
      drawTextWithLimit(canvas, candidate.getValue(), paint, 0, offsetY, maxCandidateWidth);
      if (candidate.getAnnotation().hasDescription()) {
        drawTextWithAlignAndLimit(
            canvas, candidate.getAnnotation().getDescription(), descriptionPaint,
            rects.window.right - windowHorizontalPadding, offsetY,
            Align.RIGHT, maxDescriptionWidth);
      }
      if (candidate.getAnnotation().hasShortcut()) {
        drawTextWithAlign(
            canvas, candidate.getAnnotation().getShortcut(), shortcutPaint,
            shortcutCenterX, offsetY, Align.CENTER);
      }
    }

    // Footer. Don't show if suggestion mode.
    if (rects.pageIndicator.isPresent()) {
      Rect indicatorRect = rects.pageIndicator.get();
      drawHorizontalSeparator(
          canvas, separatorPaint, rects.window.left, rects.window.right, indicatorRect.top);
      drawPageIndicator(canvas, footerPaint, indicatorRect);
    }

    // Scroll indicator
    if (rects.scrollIndicator.isPresent()) {
      canvas.drawRoundRect(
          rects.scrollIndicator.get(), scrollIndicatorRadius, scrollIndicatorRadius,
          scrollIndicatorPaint);
    }
  }

  private void drawPageIndicator(Canvas canvas, Paint paint, Rect rect) {
    drawTextWithAlign(
        canvas, String.format(FOOTER_TEXT_FORMAT,
            candidates.get().getFocusedIndex() + 1, totalCandidatesCount),
        paint, rect.exactCenterX(), rect.exactCenterY() + footerTextCenterToBaseLineOffset,
        Align.CENTER);
  }

  private void drawHorizontalSeparator(Canvas canvas, Paint paint, int startX, int endX, int y) {
    canvas.drawLine(
        Math.min(startX, endX) + horizontalSeparatorPadding, y,
        Math.max(startX, endX) - horizontalSeparatorPadding, y, paint);
  }

  /**
   * Draws {@code text} into {@code canvas} with the text align and the limitation of text width.
   * <p>
   * If measured width of {@code text} is wider than maxWidth, the {@code text} is drawn with
   * horizontal compression in order to fit {@code maxWidth}.
   */
  private void drawTextWithAlignAndLimit(
      Canvas canvas, String text, Paint paint, float x, float y, Align align, float maxWidth) {
    float textWidth = paint.measureText(text);

    int saveCount = canvas.save(Canvas.MATRIX_SAVE_FLAG);
    Align originalAlign = paint.getTextAlign();
    try {
      canvas.translate(x, y);
      if (textWidth > maxWidth) {
        // Use Canvas#scale() instead of Paint#setTextScaleX() for accurate scaling.
        canvas.scale(maxWidth / textWidth, 1.0f);
      }
      paint.setTextAlign(align);
      canvas.drawText(text, 0, 0, paint);
    } finally {
      canvas.restoreToCount(saveCount);
      paint.setTextAlign(originalAlign);
    }
  }

  /** See {@link #drawTextWithAlignAndLimit}. */
  private void drawTextWithAlign(
      Canvas canvas, String text, Paint paint, float x, float y, Align align) {
    drawTextWithAlignAndLimit(canvas, text, paint, x, y, align, Float.MAX_VALUE);
  }

  /** See {@link #drawTextWithAlignAndLimit}. */
  private void drawTextWithLimit(
      Canvas canvas, String text, Paint paint, float x, float y, float maxWidth) {
    drawTextWithAlignAndLimit(canvas, text, paint, x, y, paint.getTextAlign(), maxWidth);
  }

  private Optional<Integer> getTappingCandidate(MotionEvent event) {
    if (!windowRects.isPresent()) {
      return Optional.absent();
    }

    WindowRects rects = windowRects.get();
    int x = Math.round(event.getX());
    int y = Math.round(event.getY());

    if (!rects.window.contains(x, y)) {
      return Optional.absent();
    }

    int candidateIndex = y / candidateHeight;
    if (candidateIndex < candidates.get().getCandidateCount()) {
      return Optional.of(candidateIndex);
    } else {
      return Optional.absent();
    }
  }

  private void updateLayout() {
    if (!candidates.isPresent() || !maxWidth.isPresent()) {
      windowRects = Optional.absent();
      return;
    }

    Candidates candidatesData = candidates.get();
    int candidateNumberOnPage = candidatesData.getCandidateCount();
    boolean hasShortcut = candidatesData.getCandidateCount() > 0
        && !candidatesData.getCandidate(0).getAnnotation().getShortcut().isEmpty();
    int leftEdgePosition = hasShortcut
        ? -windowHorizontalPadding - shortcutWidth : -windowHorizontalPadding;

    // Candidates and descriptions
    maxCandidateWidth = 0;
    maxDescriptionWidth = 0;
    for (int i = 0; i < candidateNumberOnPage; ++i) {
      Candidate candidate = candidatesData.getCandidate(i);
      maxCandidateWidth = Math.max(
          maxCandidateWidth, Math.round(candidatePaint.measureText(candidate.getValue())));
      maxDescriptionWidth = Math.max(
          maxDescriptionWidth,
          Math.round(descriptionPaint.measureText(candidate.getAnnotation().getDescription())));
    }
    int fixedWidth =
        -leftEdgePosition + candidateDescriptionMinimumPadding + windowHorizontalPadding;
    int flexibleWidth = maxCandidateWidth + maxDescriptionWidth;
    if (fixedWidth + flexibleWidth > maxWidth.get()) {
      int availableWidth = maxWidth.get() - fixedWidth;
      float shrinkRate = MozcUtil.clamp((float) availableWidth / flexibleWidth, 0f, 1f);
      maxDescriptionWidth = Math.round(maxDescriptionWidth * shrinkRate);
      maxCandidateWidth = availableWidth - maxDescriptionWidth;
    }
    int rightEdgePosition = Math.max(
        Math.min(windowMinimumWidth, maxWidth.get()) + leftEdgePosition,
        maxCandidateWidth + candidateDescriptionMinimumPadding + maxDescriptionWidth
            + windowHorizontalPadding);

    // Footer
    int horizontalSeparatorY = candidateHeight * candidateNumberOnPage;
    int bottomEdgePosition;
    Optional<Rect> pageIndicatorRect;
    if (candidatesData.getCategory() != Category.SUGGESTION) {
      bottomEdgePosition = horizontalSeparatorY + footerHeight;
      pageIndicatorRect = Optional.of(
          new Rect(leftEdgePosition, horizontalSeparatorY, rightEdgePosition, bottomEdgePosition));
    } else {
      bottomEdgePosition = horizontalSeparatorY;
      pageIndicatorRect = Optional.absent();
    }

    // Focus
    Optional<Rect> focusRect = Optional.absent();
    focusedOrTappedCandidateIndexOnPage = getTappedOrFocusedIndexOnPage();
    if (focusedOrTappedCandidateIndexOnPage.isPresent()) {
      int offsetY = candidateHeight * focusedOrTappedCandidateIndexOnPage.get();
      focusRect = Optional.of(new Rect(
          leftEdgePosition, offsetY, rightEdgePosition, offsetY + candidateHeight));
    } else {
      focusRect = Optional.absent();
    }

    // Scroll indicator
    Optional<RectF> scrollIndicatorRect;
    if (totalCandidatesCount > candidatesData.getPageSize()) {
      int currentPageIndex = getCurrentPageNumber() - 1;
      float scrollIndicatorHeight =
          (float) bottomEdgePosition * candidatesData.getPageSize() / totalCandidatesCount;
      float scrollIndicatorOffset = scrollIndicatorHeight * currentPageIndex;
      scrollIndicatorRect = Optional.of(new RectF(
          rightEdgePosition - scrollIndicatorWidth, scrollIndicatorOffset, rightEdgePosition,
          Math.min(bottomEdgePosition, scrollIndicatorOffset + scrollIndicatorHeight)));
    } else {
      scrollIndicatorRect = Optional.absent();
    }

    // Window
    Rect windowRect = new Rect(leftEdgePosition, 0, rightEdgePosition, bottomEdgePosition);

    windowRects = Optional.of(
        new WindowRects(windowRect, focusRect, pageIndicatorRect, scrollIndicatorRect));
  }

  private Optional<Integer> getTappedOrFocusedIndexOnPage() {
    if (tappingCandidateIndex.isPresent()) {
      return tappingCandidateIndex;
    } else if (candidates.isPresent() && candidates.get().hasFocusedIndex()) {
      return Optional.of(candidates.get().getFocusedIndex() % candidates.get().getPageSize());
    }
    return Optional.absent();
  }

  private int getCandidateRowOffsetY(int index) {
    return index * candidateHeight;
  }

  private int getCurrentPageNumber() {
    return (int) Math.ceil(
        (float) (candidates.get().getFocusedIndex() + 1) / candidates.get().getPageSize());
  }
}
