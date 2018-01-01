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

import org.mozc.android.inputmethod.japanese.InOutAnimatedFrameLayout.VisibilityChangeListener;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.util.CursorAnchorInfoWrapper;
import org.mozc.android.inputmethod.japanese.view.Skin;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.res.Resources;
import android.view.View;
import android.view.animation.AlphaAnimation;
import android.view.animation.Animation;
import android.view.animation.AnimationSet;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.TranslateAnimation;
import android.view.inputmethod.EditorInfo;

/**
 * Manages candidate views (floating, on-keyboard).
 */
class CandidateViewManager implements MemoryManageable {

  /** Listener interface to handle the height change of a keyboard candidate view. */
  public interface KeyboardCandidateViewHeightListener {
    public void onExpanded();
    public void onCollapse();
  }

  /** {@link CandidateMode#FLOATING} is only available on Lollipop or later. */
  private enum CandidateMode {
    KEYBOARD, NUMBER, FLOATING,
  }

  private static final Animation NO_ANIMATION = new Animation() {};
  @VisibleForTesting static final Command EMPTY_COMMAND = Command.getDefaultInstance();

  @VisibleForTesting final CandidateView keyboardCandidateView;
  @VisibleForTesting final FloatingCandidateView floatingCandidateView;
  /**
   * SymbolInputView which number candidate view belongs to is created lazily.
   * Therefore number candidate view is not accessible when CandidateViewManager is instantiated.
   */
  @VisibleForTesting Optional<CandidateView> numberCandidateView = Optional.absent();

  private Optional<KeyboardCandidateViewHeightListener> keyboardCandidateViewHeightListener =
      Optional.absent();
  /**
   * Current active candidate view.
   * {@link CandidateMode#FLOATING} is only available on Lollipop or later.
   */
  private CandidateMode candidateMode = CandidateMode.KEYBOARD;

  /** Cache of {@link EditorInfo} instance to switch candidate views. */
  private EditorInfo editorInfo = new EditorInfo();
  /** Cache of {@link Skin} instance to switch candidate views. */
  private Skin skin = Skin.getFallbackInstance();
  /** Cache of candidate text size. */
  private float candidateTextSize;
  /** Cache of description text size. */
  private float descriptionTextSize;
  /** Cache of {@link ViewEventListener}. */
  private Optional<ViewEventListener> viewEventListener = Optional.absent();
  /** Cache of {@link VisibilityChangeListener}. */
  private Optional<VisibilityChangeListener> onVisibilityChangeListener = Optional.absent();
  /**
   * True if extracted mode (== fullscreen mode) is activated.
   * <p>
   * On extracted mode, floating candidate should be disabled in order to show extracted view
   * in the screen.
   */
  private boolean isExtractedMode = false;
  private boolean allowFloatingMode = false;
  private boolean narrowMode = false;

  /** Cache of {@link CursorAnchorInfoWrapper} instance to switch candidate views. */
  private CursorAnchorInfoWrapper cursorAnchorInfo = new CursorAnchorInfoWrapper();

  private Animation numberCandidateViewInAnimation = NO_ANIMATION;
  private Animation numberCandidateViewOutAnimation = NO_ANIMATION;

  @SuppressLint("NewApi")
  public CandidateViewManager(
      CandidateView keyboardCandidateView, FloatingCandidateView floatingCandidateView) {
    this.keyboardCandidateView = Preconditions.checkNotNull(keyboardCandidateView);
    this.floatingCandidateView = Preconditions.checkNotNull(floatingCandidateView);
  }

  public void setNumberCandidateView(CandidateView numberCandidateView) {
    this.numberCandidateView = Optional.of(numberCandidateView);
    numberCandidateView.setSkin(skin);
    numberCandidateView.enableFoldButton(true);
    numberCandidateView.setInAnimation(numberCandidateViewInAnimation);
    numberCandidateView.setOutAnimation(numberCandidateViewOutAnimation);
    if (candidateTextSize > 0 && descriptionTextSize > 0) {
      numberCandidateView.setCandidateTextDimension(candidateTextSize, descriptionTextSize);
    }
    if (viewEventListener.isPresent()) {
      numberCandidateView.setViewEventListener(viewEventListener.get());
    }
    numberCandidateView.setOnVisibilityChangeListener(onVisibilityChangeListener.orNull());
  }

  /**
   * Updates the candidate views by {@code outCommand} and may invoke some animations.
   * <p>
   * On-keyboard candidate view may animate and the animation listener may be invoked.
   */
  public void update(Command outCommand) {
    // Disable the animation in some situation to avoid ugly UI.
    boolean withAnimation = !(isExtractedMode && candidateMode == CandidateMode.KEYBOARD);
    updateInternal(Preconditions.checkNotNull(outCommand), withAnimation);
  }

  private void updateWithoutAnimation(Command outCommand) {
    updateInternal(outCommand, false);
  }

  private void updateInternal(Command outCommand, boolean withAnimation) {
    if (candidateMode == CandidateMode.FLOATING) {
      floatingCandidateView.setCandidates(outCommand);
      return;
    }

    Preconditions.checkState(
        candidateMode == CandidateMode.KEYBOARD
        || (candidateMode == CandidateMode.NUMBER && numberCandidateView.isPresent()));
    CandidateView candidateView = (candidateMode == CandidateMode.KEYBOARD)
        ? keyboardCandidateView : numberCandidateView.get();

    if (withAnimation) {
      if (hasCandidates(outCommand)) {
        candidateView.update(outCommand);
        // Call CandidateView#update only if there are some candidates in the output.
        // In such case the candidate view will clear its canvas.
        startKeyboardCandidateViewInAnimation();
      } else {
        // We don't call update method here and clear candidates at the end of this animation,
        // because it will clear the view's contents during the animation.
        startKeyboardCandidateViewOutAnimation();
      }
    } else {
      candidateView.update(outCommand);
      if (hasCandidates(outCommand)) {
        candidateView.setVisibility(View.VISIBLE);
      } else {
        candidateView.setVisibility(View.GONE);
      }
    }
  }

  public void setOnVisibilityChangeListener(Optional<VisibilityChangeListener> listener) {
    this.onVisibilityChangeListener = Preconditions.checkNotNull(listener);
    keyboardCandidateView.setOnVisibilityChangeListener(listener.orNull());
    if (numberCandidateView.isPresent()) {
      numberCandidateView.get().setOnVisibilityChangeListener(listener.orNull());
    }
  }

  /**
   * Enables/Disables a floating candidate view.
   * <p>
   * This method turned floating mode on if it is preferred.
   * The floating candidate view is only available on Lollipop or later.
   */
  private void updateCandiadateWindowActivation() {
    boolean floatingMode = narrowMode && allowFloatingMode && !isExtractedMode;
    if (floatingMode == (candidateMode == CandidateMode.FLOATING)) {
      return;
    }

    // Clears candidates on the current candidate window.
    updateWithoutAnimation(EMPTY_COMMAND);

    // Updates the other candidate view.
    candidateMode = floatingMode ? CandidateMode.FLOATING : CandidateMode.KEYBOARD;
    updateWithoutAnimation(EMPTY_COMMAND);
    setEditorInfo(editorInfo);
    if (FloatingCandidateView.isAvailable()) {
      setCursorAnchorInfo(cursorAnchorInfo);
    }
    // In order to show extracted view correctly, make the visibility GONE when it is not activated.
    floatingCandidateView.setVisibility(floatingMode ? View.VISIBLE : View.GONE);
  }

  public void setAllowFloatingMode(boolean allowFloatingMode) {
    Preconditions.checkArgument(!allowFloatingMode || FloatingCandidateView.isAvailable());
    this.allowFloatingMode = allowFloatingMode;
    updateCandiadateWindowActivation();
  }

  public void setNarrowMode(boolean narrowMode) {
    this.narrowMode = narrowMode;
    keyboardCandidateView.enableFoldButton(!narrowMode);
    updateCandiadateWindowActivation();
  }

  public void setNumberMode(boolean numberMode) {
    CandidateMode nextMode = numberMode ? CandidateMode.NUMBER : CandidateMode.KEYBOARD;
    if (candidateMode == nextMode) {
      return;
    }
    if (nextMode == CandidateMode.NUMBER) {
      // Hide keyboard candidate view since it is higher than symbol view.
      updateWithoutAnimation(EMPTY_COMMAND);
    }
    candidateMode = nextMode;
    // Set empty command in order to clear the candidates which have been registered into the next
    // view. Otherwise such candidates (typically they are obsolete) are shown unexpectedly.
    // TODO(hsumita): Revisit when Mozc server returns candidates for SWITCH_INPUT_MODE command.
    updateWithoutAnimation(EMPTY_COMMAND);
  }

  public void setCandidateTextDimension(float candidateTextSize, float descriptionTextSize) {
    this.candidateTextSize = candidateTextSize;
    this.descriptionTextSize = descriptionTextSize;
    if (numberCandidateView.isPresent()) {
      numberCandidateView.get().setCandidateTextDimension(
          candidateTextSize, descriptionTextSize);
    } else {
      keyboardCandidateView.setCandidateTextDimension(candidateTextSize, descriptionTextSize);
    }
  }

  public void setInputFrameFoldButtonChecked(boolean isChecked) {
    switch (candidateMode) {
      case KEYBOARD:
        keyboardCandidateView.setInputFrameFoldButtonChecked(isChecked);
        break;
      case NUMBER:
        numberCandidateView.get().setInputFrameFoldButtonChecked(isChecked);
        break;
      case FLOATING:
        throw new IllegalStateException("Fold button is not available on floating mode.");
    }
  }

  public void onStartInputView(EditorInfo editorInfo) {
    floatingCandidateView.onStartInputView(editorInfo);
  }

  public void setEditorInfo(EditorInfo info) {
    this.editorInfo = Preconditions.checkNotNull(info);
    if (candidateMode == CandidateMode.FLOATING) {
      floatingCandidateView.setEditorInfo(info);
    }
  }

  @TargetApi(21)
  public void setCursorAnchorInfo(CursorAnchorInfoWrapper info) {
    this.cursorAnchorInfo = Preconditions.checkNotNull(info);
    if (candidateMode == CandidateMode.FLOATING) {
      floatingCandidateView.setCursorAnchorInfo(info);
    }
  }

  public void setSkin(Skin skin) {
    this.skin = Preconditions.checkNotNull(skin);
    keyboardCandidateView.setSkin(skin);
    if (numberCandidateView.isPresent()) {
      numberCandidateView.get().setSkin(skin);
    }
  }

  public void setEventListener(ViewEventListener viewEventListener,
                               KeyboardCandidateViewHeightListener hightListener) {
    this.viewEventListener = Optional.of(viewEventListener);
    this.keyboardCandidateViewHeightListener = Optional.of(hightListener);
    keyboardCandidateView.setViewEventListener(viewEventListener);
    floatingCandidateView.setViewEventListener(viewEventListener);
    if (numberCandidateView.isPresent()) {
      numberCandidateView.get().setViewEventListener(viewEventListener);
    }
  }

  /**
   * Set true if extracted mode (== fullscreen mode) is activated.
   */
  public void setExtractedMode(boolean isExtractedMode) {
    this.isExtractedMode = isExtractedMode;
    updateCandiadateWindowActivation();
  }

  public void setHardwareCompositionMode(CompositionMode mode) {
    if (isFloatingMode()) {
      floatingCandidateView.setCompositionMode(mode);
    }
  }

  public void reset() {
    keyboardCandidateView.clearAnimation();
    keyboardCandidateView.setVisibility(View.GONE);
    keyboardCandidateView.reset();
    if (numberCandidateView.isPresent()) {
      numberCandidateView.get().clearAnimation();
      numberCandidateView.get().setVisibility(View.GONE);
      numberCandidateView.get().reset();
    }

    candidateMode = CandidateMode.KEYBOARD;
    floatingCandidateView.setVisibility(View.GONE);
  }

  public void resetHeightDependingComponents(
      Resources resources, int windowHeight, int inputFrameHeight) {
    Preconditions.checkNotNull(resources);

    int keyboardCandidateViewHeight = windowHeight - inputFrameHeight;
    long duration = resources.getInteger(R.integer.candidate_frame_transition_duration);
    float fromAlpha = 0.0f;
    float toAlpha = 1.0f;

    keyboardCandidateView.setInAnimation(createKeyboardCandidateViewTransitionAnimation(
        keyboardCandidateViewHeight, 0, fromAlpha, toAlpha, duration));
    keyboardCandidateView.setOutAnimation(createKeyboardCandidateViewTransitionAnimation(
        0, keyboardCandidateViewHeight, toAlpha, fromAlpha, duration));

    int numberCandidateViewHeight = resources.getDimensionPixelSize(R.dimen.button_frame_height);
    numberCandidateViewInAnimation = createKeyboardCandidateViewTransitionAnimation(
        numberCandidateViewHeight, 0, fromAlpha, toAlpha, duration);
    numberCandidateViewOutAnimation = createKeyboardCandidateViewTransitionAnimation(
        0, numberCandidateViewHeight, toAlpha, fromAlpha, duration);
    if (numberCandidateView.isPresent()) {
      numberCandidateView.get().setInAnimation(numberCandidateViewInAnimation);
      numberCandidateView.get().setOutAnimation(numberCandidateViewOutAnimation);
    }
  }

  public boolean isKeyboardCandidateViewVisible() {
    return keyboardCandidateView.getVisibility() == View.VISIBLE;
  }

  private static Animation createKeyboardCandidateViewTransitionAnimation(
      int fromY, int toY, float fromAlpha, float toAlpha, long duration) {
    AnimationSet animation = new AnimationSet(false);
    animation.setDuration(duration);

    AlphaAnimation alphaAnimation = new AlphaAnimation(fromAlpha, toAlpha);
    alphaAnimation.setDuration(duration);
    animation.addAnimation(alphaAnimation);

    TranslateAnimation translateAnimation = new TranslateAnimation(0, 0, fromY, toY);
    translateAnimation.setInterpolator(new DecelerateInterpolator());
    translateAnimation.setDuration(duration);
    animation.addAnimation(translateAnimation);
    return animation;
  }

  private void startKeyboardCandidateViewInAnimation() {
    switch (candidateMode) {
      case KEYBOARD:
        keyboardCandidateView.startInAnimation();
        if (keyboardCandidateViewHeightListener.isPresent()) {
          keyboardCandidateViewHeightListener.get().onExpanded();
        }
        break;
      case NUMBER:
        numberCandidateView.get().startInAnimation();
        break;
      case FLOATING:
        throw new IllegalStateException("Floating mode doesn't support in-animation.");
    }
  }

  private void startKeyboardCandidateViewOutAnimation() {
    switch (candidateMode) {
      case KEYBOARD:
        if (keyboardCandidateViewHeightListener.isPresent()) {
          keyboardCandidateViewHeightListener.get().onCollapse();
        }
        keyboardCandidateView.startOutAnimation();
        break;
      case NUMBER:
        numberCandidateView.get().startOutAnimation();
        break;
      case FLOATING:
        throw new IllegalStateException("Floating mode doesn't support out-animation.");
    }
  }

  private static boolean hasCandidates(Command command) {
    return command.getOutput().getAllCandidateWords().getCandidatesCount() > 0;
  }

  @Override
  public void trimMemory() {
    keyboardCandidateView.trimMemory();
    if (numberCandidateView.isPresent()) {
      numberCandidateView.get().trimMemory();
    }
  }

  public boolean isFloatingMode() {
    return candidateMode == CandidateMode.FLOATING;
  }
}
