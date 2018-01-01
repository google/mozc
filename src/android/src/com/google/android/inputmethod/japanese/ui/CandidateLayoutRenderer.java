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

import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Row;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Span;
import org.mozc.android.inputmethod.japanese.view.CarrierEmojiRenderHelper;
import org.mozc.android.inputmethod.japanese.view.Skin;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.annotation.SuppressLint;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Paint.Align;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.text.Layout;
import android.text.Layout.Alignment;
import android.text.StaticLayout;
import android.text.TextPaint;

import java.util.List;
import java.util.Locale;

/**
 * Renders the {@link CandidateLayout} instance to the {@link Canvas}.
 * After set all the parameter, clients can render the CandidateLayout as follows.
 *
 * {@code
 * CandidateList candidateList = ...;
 * CandidateLayoutRenderer renderer = ...;
 * CandidateLayout candidateLayout = layouter.layout(candidateList);
 *   :
 *   :
 * // it is necessary to set the original CandidateList before the actual rendering.
 * renderer.setCandidateList(candidateList);
 *   :
 *   :
 * renderer.drawCandidateLayout(canvas, candidateLayout, pressedCandidateIndex);
 *   :
 *   :
 * // If the original candidateList is same, it's ok to invoke the rendering
 * // twice or more, without re-invoking the setCandidateList.
 * renderer.drawCandidateLayout(canvas, candidateLayout, pressedCandidateIndex);
 * }
 */
public class CandidateLayoutRenderer {

  /**
   * The layout value width may be shorter than the rendered value without any settings.
   * This policy sets how to compress (scale) the value.
   */
  public enum ValueScalingPolicy {

    /** Scales uniformly (in other words, the aspect ratio will be kept). */
    UNIFORM,

    /** Scales only horizontally, so the height of the text will be kept. */
    HORIZONTAL,
  }

  /** Specifies if the description can keep its own region or not. */
  public enum DescriptionLayoutPolicy {

    /** The description's region will be shared with the value's. */
    OVERLAY,

    /**
     * The description can keep its region exclusively (i.e., the value and description
     * won't be overlapped).
     */
    EXCLUSIVE,

    /**
     * Like View.GONE, the descriptor is not shown and doesn't occupy any are.
     */
    GONE,
  }

  private static final int[] STATE_EMPTY = {};

  // This is actually not the constant, but we should treat this as constant.
  // Should not edit its contents.
  private static final int[] STATE_FOCUSED = { android.R.attr.state_focused };

  /** Locale field for {@link Paint#setTextLocale(Locale)}. */
  private static final Optional<Locale> TEXT_LOCALE = (Build.VERSION.SDK_INT >= 17)
      ? Optional.of(Locale.JAPAN) : Optional.<Locale>absent();

  private final TextPaint valuePaint = createTextPaint(true, Color.BLACK, Align.LEFT);
  private final TextPaint focusedValuePaint = createTextPaint(true, Color.BLACK, Align.LEFT);
  private final TextPaint descriptionPaint = createTextPaint(true, Color.GRAY, Align.RIGHT);
  private final Paint separatorPaint = new Paint();

  /**
   * The cache of Rect instance for the clip used in drawCandidateList method to reduce the
   * number of resource allocation.
   */
  private final Rect clipBounds = new Rect();

  private float valueTextSize = 0;
  private float valueHorizontalPadding = 0;

  private float descriptionTextSize = 0;
  private float descriptionHorizontalPadding = 0;
  private float descriptionVerticalPadding = 0;

  private ValueScalingPolicy valueScalingPolicy = ValueScalingPolicy.UNIFORM;
  private DescriptionLayoutPolicy descriptionLayoutPolicy = DescriptionLayoutPolicy.OVERLAY;

  private Optional<Drawable> spanBackgroundDrawable = Optional.absent();
  @VisibleForTesting int focusedIndex = -1;

  public CandidateLayoutRenderer() {
  }

  @SuppressLint("NewApi")
  private static TextPaint createTextPaint(boolean antiAlias, int color, Align align) {
    TextPaint textPaint = new TextPaint();
    textPaint.setAntiAlias(antiAlias);
    textPaint.setColor(color);
    textPaint.setTextAlign(Preconditions.checkNotNull(align));
    if (TEXT_LOCALE.isPresent()) {
      textPaint.setTextLocale(TEXT_LOCALE.get());
    }
    return textPaint;
  }

  private static boolean isFocused(
      CandidateWord candidateWord, int focusedIndex, int pressedCandidateIndex) {
    int index = Preconditions.checkNotNull(candidateWord).getIndex();
    return (index == focusedIndex) || (index == pressedCandidateIndex);
  }

  public void setSkin(Skin skin) {
    Preconditions.checkNotNull(skin);
    valuePaint.setColor(skin.candidateValueTextColor);
    focusedValuePaint.setColor(skin.candidateValueFocusedTextColor);
    descriptionPaint.setColor(skin.candidateDescriptionTextColor);
  }

  public void setValueTextSize(float valueTextSize) {
    this.valueTextSize = valueTextSize;
    this.valuePaint.setTextSize(valueTextSize);
  }

  public void setValueHorizontalPadding(float valueHorizontalPadding) {
    this.valueHorizontalPadding = valueHorizontalPadding;
  }

  public void setDescriptionTextSize(float descriptionTextSize) {
    this.descriptionTextSize = descriptionTextSize;
    this.descriptionPaint.setTextSize(descriptionTextSize);
  }

  public void setDescriptionHorizontalPadding(float descriptionHorizontalPadding) {
    this.descriptionHorizontalPadding = descriptionHorizontalPadding;
  }

  public void setDescriptionVerticalPadding(float descriptionVerticalPadding) {
    this.descriptionVerticalPadding = descriptionVerticalPadding;
  }

  public void setValueScalingPolicy(ValueScalingPolicy valueScalingPolicy) {
    this.valueScalingPolicy = Preconditions.checkNotNull(valueScalingPolicy);
  }

  public void setDescriptionLayoutPolicy(DescriptionLayoutPolicy descriptionLayoutPolicy) {
    this.descriptionLayoutPolicy = Preconditions.checkNotNull(descriptionLayoutPolicy);
  }

  public void setSpanBackgroundDrawable(Optional<Drawable> drawable) {
    this.spanBackgroundDrawable = Preconditions.checkNotNull(drawable);
  }

  public void setCandidateList(Optional<CandidateList> candidateList) {
    Preconditions.checkNotNull(candidateList);
    focusedIndex = (candidateList.isPresent() && candidateList.get().hasFocusedIndex())
        ? candidateList.get().getFocusedIndex()
        : -1;
  }

  public void setSeparatorWidth(float separatorWidth) {
    separatorPaint.setStrokeWidth(separatorWidth);
  }

  public void setSeparatorColor(int color) {
    separatorPaint.setColor(color);
  }

  /**
   * Renders the {@code candidateLayout} to the given {@code canvas}.
   */
  public void drawCandidateLayout(
      Canvas canvas, CandidateLayout candidateLayout, int pressedCandidateIndex,
      CarrierEmojiRenderHelper carrierEmojiRenderHelper) {
    Preconditions.checkNotNull(canvas);
    Preconditions.checkNotNull(candidateLayout);

    Rect clipBounds = this.clipBounds;
    canvas.getClipBounds(clipBounds);

    boolean drawSeparators = Color.alpha(separatorPaint.getColor()) != 0;

    int focusedIndex = this.focusedIndex;
    for (Row row : candidateLayout.getRowList()) {
      float top = row.getTop();
      if (top >= clipBounds.bottom) {
        break;
      }
      if (top + row.getHeight() < clipBounds.top) {
        continue;
      }

      float separatorMargin = row.getHeight() * 0.2f;
      float separatorTop = row.getTop() + separatorMargin;
      float separatorBottom = row.getTop() + row.getHeight() - separatorMargin;

      for (Span span : row.getSpanList()) {
        if (span.getLeft() > clipBounds.right) {
          break;
        }
        if (span.getRight() < clipBounds.left) {
          continue;
        }
        // Even if span.getCandidateWord() is absent, draw the span in order to draw the background.
        drawSpan(canvas, row, span,
                 span.getCandidateWord().isPresent()
                     && isFocused(span.getCandidateWord().get(),
                                  focusedIndex, pressedCandidateIndex),
                 carrierEmojiRenderHelper);
        if (drawSeparators && span.getLeft() != 0f) {
          float separatorX = span.getLeft();
          canvas.drawLine(separatorX, separatorTop, separatorX, separatorBottom, separatorPaint);
        }
      }
    }
  }

  @VisibleForTesting void drawSpan(
      Canvas canvas, Row row, Span span, boolean isFocused,
      CarrierEmojiRenderHelper carrierEmojiRenderHelper) {
    drawSpanBackground(
        Preconditions.checkNotNull(canvas), Preconditions.checkNotNull(row), span, isFocused);
    if (!span.getCandidateWord().isPresent()) {
      return;
    }

    if (carrierEmojiRenderHelper.isRenderableEmoji(span.getCandidateWord().get().getValue())) {
      drawCarrierEmoji(canvas, row, span, carrierEmojiRenderHelper);
    } else {
      drawText(canvas, row, span, isFocused);
    }

    drawDescription(canvas, row, span);
  }

  private void drawSpanBackground(Canvas canvas, Row row, Span span, boolean isFocused) {
    if (!this.spanBackgroundDrawable.isPresent()) {
      // No background available.
      return;
    }

    Drawable spanBackgroundDrawable = this.spanBackgroundDrawable.get();
    spanBackgroundDrawable.setBounds(
        (int) span.getLeft(), (int) row.getTop(),
        (int) span.getRight(), (int) (row.getTop() + row.getHeight()));
    spanBackgroundDrawable.setState(isFocused ? STATE_FOCUSED : STATE_EMPTY);
    spanBackgroundDrawable.draw(canvas);
  }

  private void drawCarrierEmoji(
      Canvas canvas, Row row, Span span, CarrierEmojiRenderHelper carrierEmojiRenderHelper) {
    Preconditions.checkState(span.getCandidateWord().isPresent());

    float descriptionWidth = (descriptionLayoutPolicy == DescriptionLayoutPolicy.EXCLUSIVE)
        ? span.getDescriptionWidth() : 0;
    float centerX = span.getLeft() + (span.getWidth() - descriptionWidth) / 2;
    float centerY = row.getTop() + row.getHeight() / 2;

    carrierEmojiRenderHelper.drawEmoji(canvas, span.getCandidateWord().get(), centerX, centerY);
  }

  private void drawText(Canvas canvas, Row row, Span span, boolean isFocused) {
    Preconditions.checkState(span.getCandidateWord().isPresent());

    String valueText = span.getCandidateWord().get().getValue();
    if (valueText == null || valueText.length() == 0) {
      // No value is available.
      return;
    }
    // Calculate layout or get cached one.
    // If isFocused is true, special paint should be applied.
    // The resulting drawing is so special that it will not re reused.
    // Therefore if isFocused is true cache is not used and always calculate the layout.
    // In this case calculated layout is not cached.
    Layout layout;
    if (!isFocused && span.getCachedLayout().isPresent()) {
      layout = span.getCachedLayout().get();
    } else {
      // Set the scaling of the text.
      float descriptionWidth = (descriptionLayoutPolicy == DescriptionLayoutPolicy.EXCLUSIVE)
          ? span.getDescriptionWidth() : 0;
      // Ensure that StaticLayout instance has positive width.
      float displayValueWidth =
          Math.max(1f, span.getWidth() - valueHorizontalPadding * 2 - descriptionWidth);
      float textScale = Math.min(1f, displayValueWidth / span.getValueWidth());
      TextPaint textPaint = isFocused ? this.focusedValuePaint : this.valuePaint;
      if (valueScalingPolicy == ValueScalingPolicy.HORIZONTAL) {
        textPaint.setTextSize(valueTextSize);
        textPaint.setTextScaleX(textScale);
      } else {
        // Calculate the max limit of the "text size", in which we can render the candidate text
        // inside the given span.
        // Rendered text should be inside the givenWidth.
        // Adjustment by font size can keep aspect ratio,
        // which is important for Emoticon especially.
        // Calculate the width with the default text size.
        textPaint.setTextSize(valueTextSize * textScale);
      }
      // Layout's width is theoretically `span.getWidth() - descriptionWidth`.
      // However because of the spec of Paint#setTextScaleX() and Paint#setTextSize(),
      // Paint#measureText() might return larger width than what both above methods expect it to be.
      // As a workaround, if theoretical width is smaller than the result of Paint#measureText(),
      // employ the width returned by Paint#measureText().
      // This workaround is to avoid from unexpected line-break.
      // NOTE: Canvas#scale() cannot be used here because we have to use StaticLayout to draw
      //       Emoji and StaticLayout requires width in its constructor.
      layout = new StaticLayout(
          valueText, new TextPaint(textPaint),
          (int) Math.ceil(Math.max(span.getWidth() - descriptionWidth,
                                   textPaint.measureText(valueText))),
          Alignment.ALIGN_CENTER, 1, 0, false);
      if (!isFocused) {
        span.setCachedLayout(layout);
      }
    }

    // Actually render the image to the canvas.
    int saveCount = canvas.save();
    try {
      canvas.translate(span.getLeft(), row.getTop() + (row.getHeight() - layout.getHeight()) / 2);
      layout.draw(canvas);
    } finally {
      canvas.restoreToCount(saveCount);
    }
  }

  private void drawDescription(Canvas canvas, Row row, Span span) {
    List<String> descriptionList = span.getSplitDescriptionList();
    if (span.getDescriptionWidth() <= 0 || descriptionList.isEmpty()
        || descriptionLayoutPolicy == DescriptionLayoutPolicy.GONE) {
      // No description available or the layout policy is GONE.
      return;
    }

    // Set the x-orientation scale based on the description's width to fit the span's region.
    TextPaint descriptionPaint = this.descriptionPaint;
    descriptionPaint.setTextSize(descriptionTextSize);
    float centerOrRight;
    if (descriptionLayoutPolicy == DescriptionLayoutPolicy.OVERLAY) {
      float displayWidth = span.getWidth() - descriptionHorizontalPadding * 2;
      descriptionPaint.setTextScaleX(Math.min(1f, displayWidth / span.getDescriptionWidth()));
      descriptionPaint.setTextAlign(Align.CENTER);
      centerOrRight = (span.getLeft() + span.getRight()) / 2f;
    } else {
      descriptionPaint.setTextScaleX(1f);
      descriptionPaint.setTextAlign(Align.RIGHT);
      centerOrRight = span.getRight() - descriptionHorizontalPadding;
    }

    // Render first "N" description lines based on the layout height.
    float descriptionTextSize = this.descriptionTextSize;
    float descriptionHeight = row.getHeight() - descriptionVerticalPadding * 2;
    int numDescriptionLines =
        Math.min((int) (descriptionHeight / descriptionTextSize), descriptionList.size());

    float top = row.getTop() + row.getHeight()
        - descriptionVerticalPadding - descriptionTextSize * (numDescriptionLines - 1);
    for (String description : descriptionList.subList(0, numDescriptionLines)) {
      canvas.drawText(description, centerOrRight, top, descriptionPaint);
      top += descriptionTextSize;
    }
  }
}
