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

import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Row;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Span;
import org.mozc.android.inputmethod.japanese.view.EmojiRenderHelper;

import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint.Align;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.text.Layout;
import android.text.StaticLayout;
import android.text.TextPaint;
import android.text.Layout.Alignment;
import android.util.FloatMath;
import android.view.View;

import java.util.List;

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
  }

  private static final int[] STATE_EMPTY = {};

  // This is actually not the constant, but we should treat this as constant.
  // Should not edit its contents.
  private static final int[] STATE_FOCUSED = { android.R.attr.state_focused };

  private final EmojiRenderHelper emojiRenderHelper;
  private final TextPaint valuePaint = createTextPaint(true, Color.BLACK, Align.LEFT);
  private final TextPaint descriptionPaint = createTextPaint(true, Color.GRAY, Align.RIGHT);

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

  private Drawable spanBackgroundDrawable = null;
  private int focusedIndex = -1;

  public CandidateLayoutRenderer(View targetView) {
    emojiRenderHelper = new EmojiRenderHelper(targetView);
  }

  private static TextPaint createTextPaint(boolean antiAlias, int color, Align align) {
    TextPaint textPaint = new TextPaint();
    textPaint.setAntiAlias(antiAlias);
    textPaint.setColor(color);
    textPaint.setTextAlign(align);
    return textPaint;
  }

  private static boolean isFocused(
      CandidateWord candidateWord, int focusedIndex, int pressedCandidateIndex) {
    if (candidateWord == null) {
      return false;
    }
    int index = candidateWord.getIndex();
    return (index == focusedIndex) || (index == pressedCandidateIndex);
  }

  public void setValueTextSize(float valueTextSize) {
    this.valueTextSize = valueTextSize;
    this.valuePaint.setTextSize(valueTextSize);
    this.emojiRenderHelper.setCandidateTextSize(valueTextSize);
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
    if (valueScalingPolicy == null) {
      throw new NullPointerException();
    }
    this.valueScalingPolicy = valueScalingPolicy;
  }

  public void setDescriptionLayoutPolicy(DescriptionLayoutPolicy descriptionLayoutPolicy) {
    if (descriptionLayoutPolicy == null) {
      throw new NullPointerException();
    }
    this.descriptionLayoutPolicy = descriptionLayoutPolicy;
  }

  public void setEmojiProviderType(EmojiProviderType emojiProviderType) {
    this.emojiRenderHelper.setEmojiProviderType(emojiProviderType);
  }

  public void setSpanBackgroundDrawable(Drawable drawable) {
    this.spanBackgroundDrawable = drawable;
  }

  public void setCandidateList(CandidateList candidateList) {
    focusedIndex = (candidateList != null && candidateList.hasFocusedIndex())
        ? candidateList.getFocusedIndex()
        : -1;
    emojiRenderHelper.setCandidateList(candidateList);
  }

  /**
   * In order to support emoji rendering, the client of this class must invoke this method
   * in its onAttachedToWindow method.
   */
  public void onAttachedToWindow() {
    emojiRenderHelper.onAttachedToWindow();
  }

  /**
   * In order to support emoji rendering, the client of this class must invoke this method
   * in its onDeachedFromWindow method.
   */
  public void onDetachedFromWindow() {
    emojiRenderHelper.onDetachedFromWindow();
  }

  /**
   * Renders the {@code candidateLayout} to the given {@code canvas}.
   */
  public void drawCandidateLayout(
      Canvas canvas, CandidateLayout candidateLayout, int pressedCandidateIndex) {
    if (candidateLayout == null) {
      return;
    }
    emojiRenderHelper.onPreDraw();
    Rect clipBounds = this.clipBounds;
    canvas.getClipBounds(clipBounds);

    int focusedIndex = this.focusedIndex;
    for (Row row : candidateLayout.getRowList()) {
      float top = row.getTop();
      if (top > clipBounds.bottom) {
        break;
      }
      if (top + row.getHeight() < clipBounds.top) {
        continue;
      }

      for (Span span : row.getSpanList()) {
        if (span.getLeft() > clipBounds.right) {
          break;
        }
        if (span.getRight() < clipBounds.left) {
          continue;
        }

        drawSpan(canvas, row, span,
                 isFocused(span.getCandidateWord(), focusedIndex, pressedCandidateIndex));
      }
    }

    emojiRenderHelper.onPostDraw();
  }

  // Exposed as package private for testing purpose.
  void drawSpan(Canvas canvas, Row row, Span span, boolean isFocused) {
    drawSpanBackground(canvas, row, span, isFocused);
    if (span.getCandidateWord() == null) {
      return;
    }

    if (emojiRenderHelper.isRenderableEmoji(span.getCandidateWord().getValue())) {
      drawEmoji(canvas, row, span);
      if (descriptionLayoutPolicy == DescriptionLayoutPolicy.OVERLAY) {
        // Hack: This is a quick hack to keep the current behavior (especially on SymbolInputView).
        // TODO(hidehiko): Remove this hack from here, instead, do not attach the description
        //   to the candidate.
        return;
      }
    } else {
      drawText(canvas, row, span);
    }

    drawDescription(canvas, row, span);
  }

  private void drawSpanBackground(Canvas canvas, Row row, Span span, boolean isFocused) {
    Drawable spanBackgroundDrawable = this.spanBackgroundDrawable;
    if (spanBackgroundDrawable == null) {
      // No background available.
      return;
    }

    spanBackgroundDrawable.setBounds(
        (int) span.getLeft(), (int) row.getTop(),
        (int) span.getRight(), (int) (row.getTop() + row.getHeight()));
    spanBackgroundDrawable.setState(isFocused ? STATE_FOCUSED : STATE_EMPTY);
    spanBackgroundDrawable.draw(canvas);
  }

  private void drawEmoji(Canvas canvas, Row row, Span span) {
    float descriptionWidth = (descriptionLayoutPolicy == DescriptionLayoutPolicy.EXCLUSIVE)
        ? span.getDescriptionWidth() : 0;
    float centerX = span.getLeft() + (span.getWidth() - descriptionWidth) / 2;
    float centerY = row.getTop() + row.getHeight() / 2;
    emojiRenderHelper.drawEmoji(canvas, span.getCandidateWord(), centerX, centerY);
  }

  private void drawText(Canvas canvas, Row row, Span span) {
    String valueText = span.getCandidateWord().getValue();
    if (valueText == null || valueText.length() == 0) {
      // No value is available.
      return;
    }

    Layout layout = span.getCachedLayout();
    if (layout == null) {
      // Set the scaling of the text.
      float descriptionWidth = (descriptionLayoutPolicy == DescriptionLayoutPolicy.EXCLUSIVE)
          ? span.getDescriptionWidth() : 0;
      float displayValueWidth =
          span.getWidth() - valueHorizontalPadding * 2 - descriptionWidth;
      float textScale = Math.min(1f, displayValueWidth / span.getValueWidth());
      TextPaint valuePaint = this.valuePaint;
      if (valueScalingPolicy == ValueScalingPolicy.HORIZONTAL) {
        valuePaint.setTextSize(valueTextSize);
        valuePaint.setTextScaleX(textScale);
      } else {
        // Calculate the max limit of the "text size", in which we can render the candidate text
        // inside the given span.
        // Rendered text should be inside the givenWidth.
        // Adjustment by font size can keep aspect ratio,
        // which is important for Emoticon especially.
        // Calculate the width with the default text size.
        valuePaint.setTextSize(valueTextSize * textScale);
      }

      // Layout the text. In order to avoid unexpected line breaking, we include the horizontal
      // padding (on both sides) into the width for the layout. It should be safe,
      // because the Alignment is ALIGN_CENTER, so that having the padding should have
      // no bad effect.
      layout = new StaticLayout(
        valueText, new TextPaint(valuePaint),
        (int) FloatMath.ceil(span.getWidth() - descriptionWidth),
        Alignment.ALIGN_CENTER, 1, 0, false);
      span.setCachedLayout(layout);
    }

    // Actually render the image to the canvas.
    canvas.save();
    try {
      canvas.translate(span.getLeft(), row.getTop() + (row.getHeight() - layout.getHeight()) / 2);
      layout.draw(canvas);
    } finally {
      canvas.restore();
    }
  }

  private void drawDescription(Canvas canvas, Row row, Span span) {
    List<String> descriptionList = span.getSplitDescriptionList();
    if (span.getDescriptionWidth() <= 0 || descriptionList.isEmpty()) {
      // No description available.
      return;
    }

    // Set the x-orientation scale based on the description's width to fit the span's region.
    TextPaint descriptionPaint = this.descriptionPaint;
    descriptionPaint.setTextSize(descriptionTextSize);
    if (descriptionLayoutPolicy == DescriptionLayoutPolicy.OVERLAY) {
      float displayWidth = span.getWidth() - descriptionHorizontalPadding * 2;
      descriptionPaint.setTextScaleX(Math.min(1f, displayWidth / span.getDescriptionWidth()));
    } else {
      descriptionPaint.setTextScaleX(1f);
    }

    // Render first "N" description lines based on the layout height.
    float descriptionTextSize = this.descriptionTextSize;
    float descriptionHeight = row.getHeight() - descriptionVerticalPadding * 2;
    int numDescriptionLines =
        Math.min((int) (descriptionHeight / descriptionTextSize), descriptionList.size());

    float top = row.getTop() + row.getHeight()
        - descriptionVerticalPadding - descriptionTextSize * (numDescriptionLines - 1);
    float right = span.getRight() - descriptionHorizontalPadding;
    for (String description : descriptionList.subList(0, numDescriptionLines)) {
      canvas.drawText(description, right, top, descriptionPaint);
      top += descriptionTextSize;
    }
  }
}
