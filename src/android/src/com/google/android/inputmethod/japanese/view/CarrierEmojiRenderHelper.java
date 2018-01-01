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

package org.mozc.android.inputmethod.japanese.view;

import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.MozcUtil;
import org.mozc.android.inputmethod.japanese.emoji.EmojiData;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.emoji.EmojiRenderableChecker;
import org.mozc.android.inputmethod.japanese.emoji.EmojiUtil;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateList;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.annotation.SuppressLint;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint.Align;
import android.graphics.Rect;
import android.os.Handler;
import android.text.Layout;
import android.text.Layout.Alignment;
import android.text.StaticLayout;
import android.text.TextPaint;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.TextView;

import java.util.Arrays;
import java.util.BitSet;
import java.util.Collections;
import java.util.EnumSet;
import java.util.Set;

import javax.annotation.Nullable;

/**
 * Helper to render Japanese Emoji.
 *
 */
public class CarrierEmojiRenderHelper {

  /**
   * Background view to support rendering carrier emoji (including animated emoji
   * which is supported _somehow_ on TextView by manufacturers).
   *
   * Due to the API limitation, a unit of emoji-support by each carrier/manufacture is
   * {@link android.widget.TextView}, in other words, more simpler components, such as
   * {@code Canvas} or {@code Paint} don't, unfortunately. So, we implement this class
   * to support emoji on MechaMozc.
   */
  static class BackgroundTextView extends TextView {

    /**
     * Unfortunately some methods are called by a constructor of the super class, so we CANNOT
     * assume that this targetView field isn't null.
     */
    @Nullable private final View targetView;

    /**
     * {@code invalidate} is designed to be invoked on the UI thread.
     * So, just can use simple lock flag to prohibit unexpected {@code invalidate} invocation
     * during rendering process.
     */
    private boolean invalidateLocked = false;

    BackgroundTextView(View targetView) {
      super(Preconditions.checkNotNull(targetView).getContext());
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
      return targetView.postDelayed(Preconditions.checkNotNull(runnable), delayMilliseconds);
    }

    @Override
    public void postInvalidateDelayed(long delayMilliseconds) {
      if (targetView == null) {
        return;
      }
      targetView.postInvalidateDelayed(delayMilliseconds);
    }

    @Override
    @Nullable
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

    /**
     * Ignores accessibility events.
     * <p>
     * As {@link #isShown()} is overridden with fake, without this hack
     * the framework thinks this view is shown correctly and calls {@link #getParent()},
     * which throws NPE as this view has no parent.
     */
    @Override
    public void sendAccessibilityEventUnchecked(AccessibilityEvent event) {
    }
  }

  private static final Set<EmojiProviderType> RENDERABLE_EMOJI_PROVIDER_SET;
  static {
    EmojiRenderableChecker renderableChecker = new EmojiRenderableChecker();
    EnumSet<EmojiProviderType> renderableEmojiProviderSet = EnumSet.noneOf(EmojiProviderType.class);

    // Test DOCOMO i-mode mark.
    boolean isDocomoEmojiRenderable =
        renderableChecker.isRenderable(new String(Character.toChars(0xFEE10)));
    if (isDocomoEmojiRenderable) {
      renderableEmojiProviderSet.add(EmojiProviderType.DOCOMO);
    }

    // Test with EZ mark.
    boolean isKddiEmojiRenderable =
        renderableChecker.isRenderable(new String(Character.toChars(0xFEE40)));
    if (isKddiEmojiRenderable) {
      renderableEmojiProviderSet.add(EmojiProviderType.KDDI);
    }

    // Test Shibuya-tower for Softbank.
    boolean isSoftbankEmojiRenderable =
        renderableChecker.isRenderable(new String(Character.toChars(0xFE4C5)));

    if (isSoftbankEmojiRenderable) {
      renderableEmojiProviderSet.add(EmojiProviderType.SOFTBANK);
    }

    RENDERABLE_EMOJI_PROVIDER_SET = Collections.unmodifiableSet(renderableEmojiProviderSet);
  }

  private final Set<EmojiProviderType> renderableEmojiProviderSet;
  private final BackgroundTextView backgroundTextView;
  private final TextPaint candidateTextPaint;
  private float candidateTextSize;

  private EmojiProviderType emojiProviderType = EmojiProviderType.NONE;
  private boolean systemSupportedEmoji = false;
  private Optional<BitSet> emojiBitSet = Optional.absent();
  private Optional<int[]> emojiLineIndex = Optional.absent();

  // Rectangle memory cache to avoid GC.
  private final Rect rect = new Rect();

  public CarrierEmojiRenderHelper(View targetView) {
    this(RENDERABLE_EMOJI_PROVIDER_SET,
         new BackgroundTextView(Preconditions.checkNotNull(targetView)));
  }

  @VisibleForTesting
  CarrierEmojiRenderHelper(
      Set<EmojiProviderType> renderableEmojiProviderSet, BackgroundTextView backgroundTextView) {
    this.renderableEmojiProviderSet = Preconditions.checkNotNull(renderableEmojiProviderSet);
    this.backgroundTextView = Preconditions.checkNotNull(backgroundTextView);

    this.candidateTextPaint = new TextPaint();
    this.candidateTextPaint.setAntiAlias(true);
    this.candidateTextPaint.setColor(Color.BLACK);
    this.candidateTextPaint.setTextAlign(Align.LEFT);
  }

  public void setCandidateTextSize(float candidateTextSize) {
    this.candidateTextSize = candidateTextSize;
  }

  /**
   * Sets the provider type to this instance.
   * @param providerType the type of emoji provider.
   *     {@link EmojiProviderType#NONE} if the runtime environment doesn't support emoji.
   */
  public void setEmojiProviderType(EmojiProviderType providerType) {
    Preconditions.checkNotNull(providerType);

    if (emojiProviderType == providerType) {
      return;
    }

    emojiProviderType = providerType;

    // Reset the current state.
    emojiLineIndex = Optional.absent();
    backgroundTextView.setText("");
    emojiBitSet = Optional.absent();

    if (!EmojiUtil.isCarrierEmojiProviderType(providerType)) {
      systemSupportedEmoji = false;
      return;
    }

    systemSupportedEmoji = renderableEmojiProviderSet.contains(providerType);
    if (systemSupportedEmoji) {
      // CAUTION! EmojiData.CARRIER_CATEGORY_NAME may contain null elements.
      switch (providerType) {
        case DOCOMO:
          emojiBitSet = Optional.of(buildEmojiSet(new String[][] {
              EmojiData.DOCOMO_FACE_NAME,
              EmojiData.DOCOMO_FOOD_NAME,
              EmojiData.DOCOMO_ACTIVITY_NAME,
              EmojiData.DOCOMO_CITY_NAME,
              EmojiData.DOCOMO_NATURE_NAME,
          }));
          break;
        case SOFTBANK:
          emojiBitSet = Optional.of(buildEmojiSet(new String[][] {
              EmojiData.SOFTBANK_FACE_NAME,
              EmojiData.SOFTBANK_FOOD_NAME,
              EmojiData.SOFTBANK_ACTIVITY_NAME,
              EmojiData.SOFTBANK_CITY_NAME,
              EmojiData.SOFTBANK_NATURE_NAME,
          }));
          break;
        case KDDI:
          emojiBitSet = Optional.of(buildEmojiSet(new String[][] {
              EmojiData.KDDI_FACE_NAME,
              EmojiData.KDDI_FOOD_NAME,
              EmojiData.KDDI_ACTIVITY_NAME,
              EmojiData.KDDI_CITY_NAME,
              EmojiData.KDDI_NATURE_NAME,
          }));
          break;
        default:
          MozcLog.e("Unexpected Emoji provider type is given: " + providerType.name());
      }
    }
  }

  /**
   * Builds the bitset, whose range is [MIN_PUA_CODE_POINT, MAX_PUA_CODE_POINT] with
   * offset MIN_PUA_CODE_POINT.
   * If the bit is true, it means the code point is a part of the carrier's emoji characters.
   *
   * @param providerDescriptionList is an array of {FACE, FOOD, ACTIVITY, CITY, NATURE}
   *   descriptions, in {@link EmojiData}. It may contains null elements.
   */
  private static BitSet buildEmojiSet(String[][] providerDescriptionList) {
    String[][] valueList = {
        EmojiData.FACE_PUA_VALUES,
        EmojiData.FOOD_PUA_VALUES,
        EmojiData.ACTIVITY_PUA_VALUES,
        EmojiData.CITY_PUA_VALUES,
        EmojiData.NATURE_PUA_VALUES,
    };
    if (valueList.length != providerDescriptionList.length) {
      throw new IllegalArgumentException();
    }
    BitSet result = new BitSet(
        EmojiUtil.MAX_EMOJI_PUA_CODE_POINT - EmojiUtil.MIN_EMOJI_PUA_CODE_POINT + 1);
    for (int i = 0; i < valueList.length; ++i) {
      buildEmojiSetInternal(result, valueList[i], providerDescriptionList[i]);
    }
    return result;
  }

  /**
   * @param descriptionList may contain null elements.
   */
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
      if (!EmojiUtil.isCarrierEmoji(codePoint)) {
        // The code point is out of range.
        throw new IllegalArgumentException("Code Point: " + Integer.toHexString(codePoint));
      }
      bitset.set(codePoint - EmojiUtil.MIN_EMOJI_PUA_CODE_POINT);
    }
  }

  @VisibleForTesting
  boolean isSystemSupportedEmoji() {
    return systemSupportedEmoji;
  }

  /**
   * @return {@code true} if the given {@code value} is the renderable emoji supported
   *   by this class.
   */
  public boolean isRenderableEmoji(String value) {
    if (Preconditions.checkNotNull(value).length() == 0 ||
        !EmojiUtil.isCarrierEmojiProviderType(emojiProviderType)) {
      // The value is empty, or the provider is isn't a carrier.
      return false;
    }

    int codePoint = value.codePointAt(0);
    if (!EmojiUtil.isCarrierEmoji(codePoint)) {
      // Not in the target range.
      return false;
    }

    if (!emojiBitSet.isPresent()) {
      // This is NOT a system supported emoji provider.
      return false;
    }

    // This is a system supported emoji provider.
    // Check if the code point is supported.
    return emojiBitSet.get().get(codePoint - EmojiUtil.MIN_EMOJI_PUA_CODE_POINT);
  }

  /**
   * Sets the candidate list (of emoji characters) to be rendered.
   * We assume that the id of the each CandidateWord equals to the position (index) of the
   * instance in the given list.
   */
  public void setCandidateList(Optional<CandidateList> candidateList) {
    Preconditions.checkNotNull(candidateList);

    if (!systemSupportedEmoji) {
      // This is not the system supported emoji. No need to prepare
      // the TextView hack.
      return;
    }

    if (!candidateList.isPresent() || candidateList.get().getCandidatesCount() == 0) {
      // No candidates are available.
      emojiLineIndex = Optional.absent();
      backgroundTextView.setText("");
      return;
    }

    // Do everything in a try clause, and ignore any exceptions, because the failure of this
    // would just causes no-animation, which should be a smaller issue.
    try {
      emojiLineIndex = Optional.of(new int[candidateList.get().getCandidatesCount()]);
      String concatText = toString(candidateList.get(), emojiLineIndex.get());
      if (concatText.length() == 0) {
        // The candidateList doesn't contain any emoji candidates.
        emojiLineIndex = Optional.absent();
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
  @VisibleForTesting String toString(CandidateList candidateList, int[] emojiLineIndex) {
    Preconditions.checkNotNull(candidateList);
    Preconditions.checkNotNull(emojiLineIndex);
    Preconditions.checkArgument(emojiLineIndex.length == candidateList.getCandidatesCount());

    // Construct string, of which line has each Emoji character.
    int currentLine = 0;
    StringBuilder sb = new StringBuilder();
    for (int i = 0; i < candidateList.getCandidatesCount(); ++i) {
      // Here we assume the id equals to the position of the candidate word.
      // The CandidateList generated by toCandidateWordList satisfies it, and used by
      // getLineBounds in drawEmoji, below.
      CandidateWord candidateWord = candidateList.getCandidates(i);
      String value = candidateWord.getValue();
      if (value != null && isRenderableEmoji(value)) {
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
  @SuppressLint("MissingSuperCall")
  public void onDetachedFromWindow() {
    backgroundTextView.onDetachedFromWindow();
  }

  /**
   * Renders the given candidateWord, which should be set to this instance via setCandidateList
   * before the invocation of this method, to the (centerX, centerY) on canvas.
   */
  public void drawEmoji(Canvas canvas, CandidateWord candidateWord, float centerX, float centerY) {
    Preconditions.checkNotNull(canvas);
    Preconditions.checkNotNull(candidateWord);
    if (!systemSupportedEmoji) {
      return;
    }

    if (!emojiLineIndex.isPresent()) {
      // No emoji is available.
      return;
    }

    int line = emojiLineIndex.get()[candidateWord.getIndex()];
    if (line < 0) {
      // The word is not the emoji.
      return;
    }

    Layout layout = backgroundTextView.getLayout();
    int saveCount = canvas.save();
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
      canvas.restoreToCount(saveCount);
    }
  }
}
