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

package org.mozc.android.inputmethod.japanese.view;

import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.emoji.EmojiData;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.Paint.Align;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.text.Layout;
import android.text.Layout.Alignment;
import android.text.StaticLayout;
import android.text.TextPaint;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import java.util.Arrays;
import java.util.BitSet;
import java.util.Collections;
import java.util.EnumSet;
import java.util.Set;

/**
 * Helper to render Japanese Emoji.
 *
 */
public class EmojiRenderHelper {

  /**
   * Background view to support rendering emoji (including animated emoji
   * which is supported _somehow_ on TextView by manufacturers).
   *
   * Due to the API limitation, a unit of emoji-support by each carrier/manufacture is
   * {@link android.widget.TextView}, in other words, more simpler components, such as
   * {@code Canvas} or {@code Paint} don't, unfortunately. So, we implement this class
   * to support emoji on MechaMozc.
   */
  static class BackgroundTextView extends TextView {
    private final View targetView;

    /**
     * {@code invalidate} is designed to be invoked on the UI thread.
     * So, just can use simple lock flag to prohibit unexpected {@code invalidate} invocation
     * during rendering process.
     */
    private boolean invalidateLocked = false;

    BackgroundTextView(View targetView) {
      super(targetView.getContext());
      this.targetView = targetView;
      setLayoutParams(new ViewGroup.LayoutParams(0, 0));
      setVisibility(View.VISIBLE);
    }

    @Override
    public boolean isShown() {
      if (targetView == null) {
        return false;
      }
      // Fake as this is actually shown to user, when the enclosing candidate view is shown.
      // On some devices, this is necessary to support animated emoji.
      return targetView.isShown();
    }

    public void lockInvalidate() {
      invalidateLocked = true;
    }

    public void unlockInvalidate() {
      invalidateLocked = false;
    }

    // On some devices, following methods are invoked as an entry point of animation callback.
    // So, we'll route this to the enclosing candidate view.

    @Override
    public void invalidate() {
      if (!invalidateLocked && targetView != null) {
        targetView.invalidate();
      }
    }

    @Override
    public void postInvalidate() {
      if (targetView != null) {
        targetView.postInvalidate();
      }
    }

    @Override
    public boolean postDelayed(Runnable runnable, long delayMilliseconds) {
      if (targetView == null) {
        return false;
      }
      // On some devices, View#postDelayed is used as infrastructure of the animation of emoji.
      // So, proxy the runnable event to the enclosing instance.
      return targetView.postDelayed(runnable, delayMilliseconds);
    }

    @Override
    public void postInvalidateDelayed(long delayMilliseconds) {
      if (targetView == null) {
        return;
      }
      targetView.postInvalidateDelayed(delayMilliseconds);
    }

    @Override
    public Handler getHandler() {
      if (targetView == null) {
        return null;
      }
      // The result of getHandler is used to create another Handler for animation of emoji
      // on some devices. Just proxy it.
      return targetView.getHandler();
    }

    /**
     * Expose as public to access this method from the enclosing candidate view.
     */
    @Override
    public void onAttachedToWindow() {
      super.onAttachedToWindow();
    }

    /**
     * Expose as public to access this method from the enclosing candidate view.
     */
    @Override
    public void onDetachedFromWindow() {
      super.onDetachedFromWindow();
    }

    /**
     * Unfortunately measure(int, int) is final so make a proxy method for testing purpose.
     */
    void measureInternal(int width, int height) {
      super.measure(width, height);
    }
  }

  /**
   * Checker of whether the given code point is renderable on the current device or not.
   *
   * When isRenderable is called, this class tries to render the given code point
   * by using {@link StaticLayout}, instead of Canvas#drawText directly, and compares
   * the pre-rendered pixel-data of fallback (garbled) character's.
   * If they are same, the code point cannot rendered on the current device. If not, it can.
   *
   * The reason why we use StaticLayout instead of Canvas#drawText is that some devices
   * support Emoji characters, which is the main target of this check, only on TextView
   * (and its related widget), but not on Canvas class, for some reason (maybe for animation
   * support). So, even if we can see the Emoji characters in TextView on the device,
   * Canvas#drawText will render a fallback glyph.
   *
   * Note: this class uses internal buffer, so is not thread-safe.
   */
  private static class RenderableChecker {
    private static final int TEST_FONT_SIZE = 10;
    private static final int FOREGROUND_COLOR = Color.WHITE;
    private static final int BACKGROUND_COLOR = Color.BLACK;
    private static final int FALLBACK_CHARACTER = '\uFFFF';

    private Bitmap bitmap;
    private Canvas canvas;
    private TextPaint paint;

    private final int[] fallbackGlyphPixels;
    private final int[] targetGlyphPixels;

    RenderableChecker() {
      paint = new TextPaint();
      paint.setTextSize(TEST_FONT_SIZE);
      paint.setColor(FOREGROUND_COLOR);

      bitmap = MozcUtil.createBitmap(TEST_FONT_SIZE, TEST_FONT_SIZE, Config.ARGB_8888);
      canvas = new Canvas(bitmap);

      fallbackGlyphPixels = new int[TEST_FONT_SIZE * TEST_FONT_SIZE];
      targetGlyphPixels = new int[TEST_FONT_SIZE * TEST_FONT_SIZE];

      renderGlyphInternal(FALLBACK_CHARACTER, fallbackGlyphPixels);
    }

    boolean isRenderable(int codePoint) {
      try {
        renderGlyphInternal(codePoint, targetGlyphPixels);
        return !Arrays.equals(targetGlyphPixels, fallbackGlyphPixels);
      } catch (NullPointerException e) {
        MozcLog.e("Unknown exception is happen: ", e);
      }
      return true;
    }

    private void renderGlyphInternal(int codePoint, int[] buffer) {
      canvas.drawColor(BACKGROUND_COLOR);

      StaticLayout layout = new StaticLayout(
          new String(new int[] {codePoint}, 0, 1),
          paint, TEST_FONT_SIZE, Alignment.ALIGN_NORMAL, 1, 0, false);
      layout.draw(canvas);

      bitmap.getPixels(buffer, 0, TEST_FONT_SIZE, 0, 0, TEST_FONT_SIZE, TEST_FONT_SIZE);
    }

    void release() {
      bitmap.recycle();
    }
  }

  private static final Set<EmojiProviderType> RENDERABLE_EMOJI_PROVIDER_SET;
  static {
    RenderableChecker renderableChecker = new RenderableChecker();
    try {
      EnumSet<EmojiProviderType> renderableEmojiProviderSet =
          EnumSet.noneOf(EmojiProviderType.class);
      // Test DOCOMO i-mode mark.
      boolean isDocomoEmojiRenderable = renderableChecker.isRenderable(0xFEE10);
      if (isDocomoEmojiRenderable) {
        renderableEmojiProviderSet.add(EmojiProviderType.DOCOMO);
      }

      // Test with EZ mark.
      boolean isKddiEmojiRenderable = renderableChecker.isRenderable(0xFEE40);
      if (isKddiEmojiRenderable) {
        renderableEmojiProviderSet.add(EmojiProviderType.KDDI);
      }

      // Test Shibuya-tower for Softbank.
      boolean isSoftbankEmojiRenderable = renderableChecker.isRenderable(0xFE4C5);
      if (isSoftbankEmojiRenderable) {
        renderableEmojiProviderSet.add(EmojiProviderType.SOFTBANK);
      }
      RENDERABLE_EMOJI_PROVIDER_SET = Collections.unmodifiableSet(renderableEmojiProviderSet);
    } finally {
      renderableChecker.release();
    }
  }

  private final BackgroundTextView backgroundTextView;
  private final EmojiDrawableFactory emojiDrawableFactory;
  private final TextPaint candidateTextPaint;
  private float candidateTextSize;

  private EmojiProviderType emojiProviderType = null;
  private boolean systemSupportedEmoji = false;
  private BitSet emojiBitSet = null;
  private int[] emojiLineIndex = null;

  // For emoji animation.
  private long baseTime = 0;
  private long drawTime = 0;
  private long nextInvalidateTime = Long.MAX_VALUE;

  // Rectangle memory cache to avoid GC.
  private final Rect rect = new Rect();

  public EmojiRenderHelper(View targetView) {
    backgroundTextView = new BackgroundTextView(targetView);
    emojiDrawableFactory = new EmojiDrawableFactory(targetView.getResources());

    candidateTextPaint = new TextPaint();
    candidateTextPaint.setAntiAlias(true);
    candidateTextPaint.setColor(Color.BLACK);
    candidateTextPaint.setTextAlign(Align.LEFT);
  }

  public void setCandidateTextSize(float candidateTextSize) {
    this.candidateTextSize = candidateTextSize;
  }

  /**
   * Sets the provider type to this instance.
   * Note that this method may cancel the current animation state.
   * @param providerType the type of emoji provider.
   *     {@code null} if the runtime environment doesn't support emoji.
   */
  public void setEmojiProviderType(EmojiProviderType providerType) {
    if (emojiProviderType == providerType) {
      return;
    }

    emojiProviderType = providerType;
    emojiDrawableFactory.setProviderType(providerType);

    // Reset the current state.
    emojiLineIndex = null;
    backgroundTextView.setText("");
    emojiBitSet = null;

    if (providerType == null) {
      systemSupportedEmoji = false;
      return;
    }

    systemSupportedEmoji = RENDERABLE_EMOJI_PROVIDER_SET.contains(providerType);
    if (systemSupportedEmoji) {
      switch (providerType) {
        case DOCOMO:
          emojiBitSet = buildEmojiSet(new String[][] {
              EmojiData.DOCOMO_FACE_NAME,
              EmojiData.DOCOMO_FOOD_NAME,
              EmojiData.DOCOMO_ACTIVITY_NAME,
              EmojiData.DOCOMO_CITY_NAME,
              EmojiData.DOCOMO_NATURE_NAME,
          });
          break;
        case SOFTBANK:
          emojiBitSet = buildEmojiSet(new String[][] {
              EmojiData.SOFTBANK_FACE_NAME,
              EmojiData.SOFTBANK_FOOD_NAME,
              EmojiData.SOFTBANK_ACTIVITY_NAME,
              EmojiData.SOFTBANK_CITY_NAME,
              EmojiData.SOFTBANK_NATURE_NAME,
          });
          break;
        case KDDI:
          emojiBitSet = buildEmojiSet(new String[][] {
              EmojiData.KDDI_FACE_NAME,
              EmojiData.KDDI_FOOD_NAME,
              EmojiData.KDDI_ACTIVITY_NAME,
              EmojiData.KDDI_CITY_NAME,
              EmojiData.KDDI_NATURE_NAME,
          });
          break;
      }
    }
  }

  /**
   * Builds the bitset, whose range is [MIN_PUA_CODE_POINT, MAX_PUA_CODE_POINT] with
   * offset MIN_PUA_CODE_POINT.
   * If the bit is true, it means the code point is a part of the carrier's emoji characters.
   *
   * @param providerDescriptionList is an array of {FACE, FOOD, ACTIVITY, CITY, NATURE}
   *   descriptions, in {@link EmojiData}.
   */
  private static BitSet buildEmojiSet(String[][] providerDescriptionList) {
    String[][] valueList = {
        EmojiData.FACE_VALUES,
        EmojiData.FOOD_VALUES,
        EmojiData.ACTIVITY_VALUES,
        EmojiData.CITY_VALUES,
        EmojiData.NATURE_VALUES,
    };
    if (valueList.length != providerDescriptionList.length) {
      throw new IllegalArgumentException();
    }
    BitSet result = new BitSet(
        EmojiDrawableFactory.MAX_EMOJI_PUA_CODE_POINT
            - EmojiDrawableFactory.MIN_EMOJI_PUA_CODE_POINT + 1);
    for (int i = 0; i < valueList.length; ++i) {
      buildEmojiSetInternal(result, valueList[i], providerDescriptionList[i]);
    }
    return result;
  }

  private static void buildEmojiSetInternal(
      BitSet bitset, String[] valueList, String[] descriptionList) {
    if (valueList.length != descriptionList.length) {
      // Should have the same number of elements.
      throw new IllegalArgumentException();
    }

    for (int i = 0; i < valueList.length; ++i) {
      if (descriptionList[i] == null) {
        continue;
      }
      int codePoint = valueList[i].codePointAt(0);
      if (!EmojiDrawableFactory.isInEmojiPuaRange(codePoint)) {
        // The code point is out of range.
        throw new IllegalArgumentException("Code Point: " + Integer.toHexString(codePoint));
      }
      bitset.set(codePoint - EmojiDrawableFactory.MIN_EMOJI_PUA_CODE_POINT);
    }
  }

  public boolean isSystemSupportedEmoji() {
    return systemSupportedEmoji;
  }

  /**
   * @return {@code true} if the given {@code value} is the renderable emoji supported
   *   by this class.
   */
  public boolean isRenderableEmoji(String value) {
    if (value == null || value.length() == 0 || emojiProviderType == null) {
      // The value is empty, or no provider is set.
      return false;
    }

    int codePoint = value.codePointAt(0);
    if (!EmojiDrawableFactory.isInEmojiPuaRange(codePoint)) {
      // Not in the target range.
      return false;
    }

    if (emojiBitSet != null) {
      // This is system supported emoji provider.
      // Check if the code point is supported.
      return emojiBitSet.get(codePoint - EmojiDrawableFactory.MIN_EMOJI_PUA_CODE_POINT);
    }

    // This is not system supported emoji provider.
    // Thus, check if the drawable data is available or not.
    return emojiDrawableFactory.hasDrawable(codePoint);
  }

  /**
   * Sets the candidate list (of emoji characters) to be rendered.
   * We assume that the id of the each CandidateWord equals to the position (index) of the
   * instance in the given list.
   */
  public void setCandidateList(CandidateList candidateList) {
    baseTime = System.currentTimeMillis();
    if (!systemSupportedEmoji) {
      // This is not the system supported emoji. No need to prepare
      // the TextView hack.
      return;
    }

    if (candidateList == null || candidateList.getCandidatesCount() == 0) {
      // No candidates are available.
      emojiLineIndex = null;
      backgroundTextView.setText("");
      return;
    }

    // Do everything in a try clause, and ignore any exceptions, because the failure of this
    // would just causes no-animation, which should be a smaller issue.
    try {
      emojiLineIndex = new int[candidateList.getCandidatesCount()];
      String concatText = toString(candidateList, emojiLineIndex);
      if (concatText.length() == 0) {
        // The candidateList doesn't contain any emoji candidates.
        emojiLineIndex = null;
        backgroundTextView.setText("");
        return;
      }

      // Calculate the bounding box.
      float candidateTextSize = this.candidateTextSize;
      candidateTextPaint.setTextSize(candidateTextSize);
      candidateTextPaint.getTextBounds(concatText, 0, concatText.length(), rect);
      int measureWidth = rect.width();
      int measureHeight = rect.height();

      // Actually set text and size to the background text view.
      backgroundTextView.setText(concatText);
      backgroundTextView.setTextSize(TypedValue.COMPLEX_UNIT_PX, candidateTextSize);
      ViewGroup.LayoutParams layoutParams = backgroundTextView.getLayoutParams();
      if (layoutParams != null &&
          (layoutParams.width != measureWidth || layoutParams.height != measureHeight)) {
        layoutParams.width = measureWidth;
        layoutParams.height = measureHeight;
        backgroundTextView.setLayoutParams(layoutParams);
      }
      backgroundTextView.measureInternal(measureWidth, measureHeight);
    } catch (Exception e) {
      // Ignore any exceptions here, so that it will fall back to StaticLayout.
      // It should be *better* (not best) than unexpected failure of Mozc service on production.
      MozcLog.w("Failed at backgroundTextView preparation: ", e);
    }
  }

  /**
   * Builds a String instance, which concat all emoji candidates in candateList.
   * The length of {@code emojiLineIndex} should be equal to candidateList.getCandidatesCount().
   * This method fills the line index for the emoji candidates, or -1 for non-emoji candidates.
   */
  String toString(CandidateList candidateList, int[] emojiLineIndex) {
    // Construct string, of which line has each Emoji character.
    int currentLine = 0;
    StringBuilder sb = new StringBuilder();
    for (int i = 0; i < candidateList.getCandidatesCount(); ++i) {
      // Here we assume the id equals to the position of the candidate word.
      // The CandidateList generated by toCandidateWordList satisfies it, and used by
      // getLineBounds in drawEmoji, below.
      CandidateWord candidateWord = candidateList.getCandidates(i);
      String value = candidateWord.getValue();
      if (isRenderableEmoji(value)) {
        emojiLineIndex[i] = currentLine;
        sb.append(value);
        sb.append("\n");
        ++currentLine;
      } else {
        emojiLineIndex[i] = -1;
      }
    }
    return sb.toString();
  }

  /**
   * To enable emoji animation on some devices, it is necessary to call this method
   * from the client view's onAttachedToWindow.
   */
  public void onAttachedToWindow() {
    backgroundTextView.onAttachedToWindow();
  }

  /**
   * To enable emoji animation on some devices, it is necessary to call this method
   * from the client view's onDetachedFromWindow.
   */
  public void onDetachedFromWindow() {
    backgroundTextView.onDetachedFromWindow();
  }

  /**
   * Renders the given candidateWord, which should be set to this instance via setCandidateList
   * before the invocation of this method, to the (centerX, centerY) on canvas.
   */
  public void drawEmoji(Canvas canvas, CandidateWord candidateWord, float centerX, float centerY) {
    if (systemSupportedEmoji) {
      drawEmojiByTextView(canvas, candidateWord, centerX, centerY);
    } else {
      drawEmojiDrawable(canvas, candidateWord, centerX, centerY);
    }
  }

  private void drawEmojiByTextView(
      Canvas canvas, CandidateWord candidateWord, float centerX, float centerY) {
    if (emojiLineIndex == null) {
      // No emoji is available.
      return;
    }

    int line = emojiLineIndex[candidateWord.getIndex()];
    if (line < 0) {
      // The word is not the emoji.
      return;
    }

    Layout layout = backgroundTextView.getLayout();
    canvas.save();
    backgroundTextView.lockInvalidate();
    try {
      // The text content of backgroundTextView has a candidate word per line,
      // so getLineBounds returns the bounding box of the candidate, here.
      layout.getLineBounds(line, rect);
      canvas.clipRect(centerX - rect.width() / 2, centerY - rect.height() / 2,
                      centerX + rect.width() / 2, centerY + rect.height() / 2);
      canvas.translate(centerX - rect.centerX(), centerY - rect.centerY());
      layout.draw(canvas);
    } finally {
      backgroundTextView.unlockInvalidate();
      canvas.restore();
    }
  }

  private void drawEmojiDrawable(
      Canvas canvas, CandidateWord candidateWord, float centerX, float centerY) {
    int codePoint = candidateWord.getValue().codePointAt(0);
    Drawable drawable = emojiDrawableFactory.getDrawable(codePoint);
    if (drawable == null) {
      return;
    }

    if (drawable instanceof EmojiAnimationDrawable) {
      EmojiAnimationDrawable animationDrawable = EmojiAnimationDrawable.class.cast(drawable);
      animationDrawable.selectDrawableByTime(drawTime);
      nextInvalidateTime =
          Math.min(nextInvalidateTime, animationDrawable.getNextFrameTiming(drawTime));
    }

    canvas.save();
    try {
      float size = this.candidateTextSize;
      float scale = size / drawable.getIntrinsicHeight();
      canvas.translate(centerX - size / 2, centerY - size / 2);
      canvas.scale(scale, scale);
      drawable.setBounds(0, 0, drawable.getIntrinsicWidth(), drawable.getIntrinsicHeight());
      drawable.draw(canvas);
    } finally {
      canvas.restore();
    }
  }

  public void onPreDraw() {
    nextInvalidateTime = Long.MAX_VALUE;
    drawTime = System.currentTimeMillis() - baseTime;
  }

  public void onPostDraw() {
    if (nextInvalidateTime < Long.MAX_VALUE) {
      backgroundTextView.postInvalidateDelayed(
          baseTime + nextInvalidateTime - System.currentTimeMillis());
    }
  }
}
