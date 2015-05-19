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

import org.mozc.android.inputmethod.japanese.FeedbackManager.FeedbackEvent;
import org.mozc.android.inputmethod.japanese.LayoutParamsAnimator.InterpolationListener;
import org.mozc.android.inputmethod.japanese.ViewManager.LayoutAdjustment;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.keyboard.BackgroundDrawableFactory;
import org.mozc.android.inputmethod.japanese.keyboard.KeyEventHandler;
import org.mozc.android.inputmethod.japanese.model.SymbolCandidateStorage;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.ui.SideFrameStubProxy;
import org.mozc.android.inputmethod.japanese.view.MozcDrawableFactory;
import org.mozc.android.inputmethod.japanese.view.RoundRectKeyDrawable;
import org.mozc.android.inputmethod.japanese.view.SkinType;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.inputmethodservice.InputMethodService.Insets;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.AccelerateDecelerateInterpolator;
import android.view.animation.AccelerateInterpolator;
import android.view.animation.AlphaAnimation;
import android.view.animation.Animation;
import android.view.animation.AnimationSet;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.Interpolator;
import android.view.animation.TranslateAnimation;
import android.widget.CompoundButton;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;

/**
 * Root {@code View} of the MechaMozc.
 * It is expected that instance methods are used after inflation is done.
 *
 */
public class MozcView extends LinearLayout {

  /**
   * Decides insets.
   * Insets.touchableRegion needs API Level 11. So we split it to RegionInsetsCalculator
   * which is used under reflection.
   */
  static interface InsetsCalculator {
    boolean isFloatingMode(MozcView mozcView);
    void setInsets(MozcView mozcView, int contentViewWidth, int contentViewHeight,
                   Insets outInsets);
  }

  static class DefaultInsetsCalculator implements InsetsCalculator {
    static void setInsetsDefault(MozcView mozcView, int contentViewWidth, int contentViewHeight,
                                 Insets outInsets) {
      outInsets.touchableInsets = Insets.TOUCHABLE_INSETS_CONTENT;
      outInsets.contentTopInsets = contentViewHeight - mozcView.getVisibleViewHeight();
      outInsets.visibleTopInsets = outInsets.contentTopInsets;
    }

    @Override
    public boolean isFloatingMode(MozcView mozcView) {
      return false;
    }

    @Override
    public void setInsets(MozcView mozcView, int contentViewWidth, int contentViewHeight,
                          Insets outInsets) {
      setInsetsDefault(mozcView, contentViewWidth, contentViewHeight, outInsets);
    }
  }

  /**
   * Sets regional Inset to transparent background.
   *
   * public accessibility for easier invocation via reflection.
   */
  @TargetApi(11)
  public static class RegionInsetsCalculator implements InsetsCalculator {
    @Override
    public boolean isFloatingMode(MozcView mozcView) {
      Resources resources = mozcView.getResources();
      return mozcView.layoutAdjustment != LayoutAdjustment.FILL
          && !mozcView.narrowMode
          && resources.getDisplayMetrics().widthPixels
              >= mozcView.dimensionPixelSize.imeWindowRegionInsetThreshold;
    }

    @Override
    public void setInsets(MozcView mozcView, int contentViewWidth, int contentViewHeight,
                          Insets outInsets) {
      if (!isFloatingMode(mozcView)) {
        DefaultInsetsCalculator.setInsetsDefault(mozcView, contentViewWidth, contentViewHeight,
                                                 outInsets);
        return;
      }
      mozcView.getResources();
      int height = mozcView.getVisibleViewHeight();
      int width = mozcView.getSideAdjustedWidth();
      int left =
          mozcView.layoutAdjustment == LayoutAdjustment.RIGHT ? (contentViewWidth - width) : 0;

      outInsets.touchableInsets = Insets.TOUCHABLE_INSETS_REGION;
      outInsets.touchableRegion.set(
          left, contentViewHeight - height, left + width, contentViewHeight);
      outInsets.contentTopInsets = contentViewHeight;
      outInsets.visibleTopInsets = contentViewHeight;
      return;
    }
  }

  static class DimensionPixelSize {
    final int imeWindowPartialWidth;
    final int imeWindowRegionInsetThreshold;
    final int narrowFrameHeight;
    final int narrowImeWindowHeight;
    final int sideFrameWidth;
    final int translucentBorderHeight;
    public DimensionPixelSize(Resources resources) {
      imeWindowPartialWidth = resources.getDimensionPixelSize(R.dimen.ime_window_partial_width);
      imeWindowRegionInsetThreshold = resources.getDimensionPixelSize(
          R.dimen.ime_window_region_inset_threshold);
      narrowFrameHeight = resources.getDimensionPixelSize(R.dimen.narrow_frame_height);
      narrowImeWindowHeight = resources.getDimensionPixelSize(R.dimen.narrow_ime_window_height);
      translucentBorderHeight = resources.getDimensionPixelSize(
          R.dimen.translucent_border_height);
      sideFrameWidth = resources.getDimensionPixelSize(R.dimen.side_frame_width);
    }
  }

  static class HeightLinearInterpolationListener implements InterpolationListener {
    private final int fromHeight;
    private final int toHeight;

    public HeightLinearInterpolationListener(int fromHeight, int toHeight) {
      this.fromHeight = fromHeight;
      this.toHeight = toHeight;
    }

    @Override
    public ViewGroup.LayoutParams calculateAnimatedParams(
        float interpolation, ViewGroup.LayoutParams currentLayoutParams) {
      currentLayoutParams.height = fromHeight + (int) ((toHeight - fromHeight) * interpolation);
      return currentLayoutParams;
    }
  }

  // TODO(hidehiko): Refactor CandidateViewListener along with View structure refactoring.
  class InputFrameFoldButtonClickListener implements OnClickListener {
    private final ViewEventListener eventListener;
    private final View keyboardView;
    private final long foldDuration;
    private final Interpolator foldKeyboardViewInterpolator;
    private final long expandDuration;
    private final Interpolator expandKeyboardViewInterpolator;
    private final LayoutParamsAnimator layoutParamsAnimator;
    InputFrameFoldButtonClickListener(
        ViewEventListener eventListener, View keyboardView,
        long foldDuration, Interpolator foldKeyboardViewInterpolator,
        long expandDuration, Interpolator expandKeyboardViewInterpolator,
        LayoutParamsAnimator layoutParamsAnimator,
        float snapVelocityThreshold) {
      this.eventListener = eventListener;
      this.keyboardView = keyboardView;
      this.foldDuration = foldDuration;
      this.foldKeyboardViewInterpolator = foldKeyboardViewInterpolator;
      this.expandDuration = expandDuration;
      this.expandKeyboardViewInterpolator = expandKeyboardViewInterpolator;
      this.layoutParamsAnimator = layoutParamsAnimator;
    }

    @Override
    public void onClick(View v) {
      if (keyboardView.getHeight() == getInputFrameHeight()) {
        eventListener.onFireFeedbackEvent(FeedbackEvent.INPUTVIEW_FOLD);
        layoutParamsAnimator.startAnimation(
            keyboardView,
            new HeightLinearInterpolationListener(keyboardView.getHeight(), 0),
            foldKeyboardViewInterpolator, foldDuration, 0);
        CompoundButton.class.cast(v).setChecked(true);
      } else {
        eventListener.onFireFeedbackEvent(FeedbackEvent.INPUTVIEW_EXPAND);
        layoutParamsAnimator.startAnimation(
            keyboardView,
            new HeightLinearInterpolationListener(keyboardView.getHeight(), getInputFrameHeight()),
            expandKeyboardViewInterpolator, expandDuration, 0);
        CompoundButton.class.cast(v).setChecked(false);
      }
    }
  }

  // TODO(hidehiko): Move hard coded parameters to dimens.xml or skin.
  private static final float NARROW_MODE_BUTTON_CORNOR_RADIUS = 3.5f;  // in dip.
  private static final float NARROW_MODE_BUTTON_LEFT_OFFSET = 2.0f;
  private static final float NARROW_MODE_BUTTON_TOP_OFFSET = 1.0f;
  private static final float NARROW_MODE_BUTTON_RIGHT_OFFSET = 2.0f;
  private static final float NARROW_MODE_BUTTON_BOTTOM_OFFSET = 3.0f;
  private static final InsetsCalculator insetsCalculator;

  private final InOutAnimatedFrameLayout.VisibilityChangeListener onVisibilityChangeListener =
      new InOutAnimatedFrameLayout.VisibilityChangeListener() {
        @Override
        public void onVisibilityChange(int oldvisibility, int newvisibility) {
          updateInputFrameHeight();
        }
      };

  private final DimensionPixelSize dimensionPixelSize = new DimensionPixelSize(getResources());
  private final SideFrameStubProxy leftFrameStubProxy = new SideFrameStubProxy();
  private final SideFrameStubProxy rightFrameStubProxy = new SideFrameStubProxy();

  private MozcDrawableFactory mozcDrawableFactory = new MozcDrawableFactory(getResources());

  private boolean fullscreenMode = false;
  private boolean narrowMode = false;
  private SkinType skinType = SkinType.ORANGE_LIGHTGRAY;
  private LayoutAdjustment layoutAdjustment = LayoutAdjustment.FILL;
  private int inputFrameHeight = 0;
  private int imeWindowHeight = 0;
  private Animation candidateViewInAnimation;
  private Animation candidateViewOutAnimation;
  private Animation symbolInputViewInAnimation;
  private Animation symbolInputViewOutAnimation;
  private Animation dropShadowCandidateViewInAnimation;
  private Animation dropShadowCandidateViewOutAnimation;
  private Animation dropShadowSymbolInputViewInAnimation;
  private Animation dropShadowSymbolInputViewOutAnimation;
  private boolean isDropShadowExpanded = false;

  static {
    // API Level 11 is Build.VERSION_CODES.HONEYCOMB.
    // When right/left adjustment mode, outInsets uses touchableRegion to cut out IME rectangle.
    // Because only after API.11(HONEYCOMB) supports touchableRegion, filter it.
    InsetsCalculator tmpCalculator = null;
    if (Build.VERSION.SDK_INT >= 11) {
      // Try to create RegsionInsetsCalculator if the API level is high enough.
      try {
        Class<?> clazz = Class.forName(new StringBuilder(MozcView.class.getCanonicalName())
            .append('$')
            .append("RegionInsetsCalculator")
            .toString());
        tmpCalculator = InsetsCalculator.class.cast(clazz.newInstance());
      } catch (ClassNotFoundException e) {
        MozcLog.e(e.getMessage(), e);
      } catch (IllegalArgumentException e) {
        MozcLog.e(e.getMessage(), e);
      } catch (IllegalAccessException e) {
        MozcLog.e(e.getMessage(), e);
      } catch (InstantiationException e) {
        MozcLog.e(e.getMessage(), e);
      }
    }

    if (tmpCalculator == null) {
      tmpCalculator = new DefaultInsetsCalculator();
    }
    insetsCalculator = tmpCalculator;
  }

  public MozcView(Context context) {
    super(context);
  }

  public MozcView(Context context, AttributeSet attrSet) {
    super(context, attrSet);
  }

  private static Drawable createButtonBackgroundDrawable(float density) {
    return BackgroundDrawableFactory.createPressableDrawable(
        new RoundRectKeyDrawable(
            (int) (NARROW_MODE_BUTTON_LEFT_OFFSET * density),
            (int) (NARROW_MODE_BUTTON_TOP_OFFSET * density),
            (int) (NARROW_MODE_BUTTON_RIGHT_OFFSET * density),
            (int) (NARROW_MODE_BUTTON_BOTTOM_OFFSET * density),
            (int) (NARROW_MODE_BUTTON_CORNOR_RADIUS * density),
            0xFFE9E4E4, 0xFFB2ADAD, 0, 0xFF1E1E1E),
        new RoundRectKeyDrawable(
            (int) (NARROW_MODE_BUTTON_LEFT_OFFSET * density),
            (int) (NARROW_MODE_BUTTON_TOP_OFFSET * density),
            (int) (NARROW_MODE_BUTTON_RIGHT_OFFSET * density),
            (int) (NARROW_MODE_BUTTON_BOTTOM_OFFSET * density),
            (int) (NARROW_MODE_BUTTON_CORNOR_RADIUS * density),
            0xFF858087, 0xFF67645F, 0, 0xFF1E1E1E));
  }

  private void setupImageButton(ImageView view, int resourceID) {
    float density = getResources().getDisplayMetrics().density;
    view.setImageDrawable(mozcDrawableFactory.getDrawable(resourceID));
    view.setBackgroundDrawable(createButtonBackgroundDrawable(density));
    view.setPadding(0, 0, 0, 0);
  }

  private static Animation createAlphaAnimation(float fromAlpha, float toAlpha, long duration) {
    AlphaAnimation animation = new AlphaAnimation(fromAlpha, toAlpha);
    animation.setDuration(duration);
    return animation;
  }

  private static Animation createCandidateViewTransitionAnimation(int fromY, int toY,
                                                                  float fromAlpha, float toAlpha,
                                                                  long duration) {
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

  @Override
  public void onFinishInflate() {
    setKeyboardHeightRatio(100);

    setupImageButton(getWidenButton(), R.raw.hardware__function__close);
    setupImageButton(getHardwareCompositionButton(), R.raw.qwerty__function__kana__icon);

    leftFrameStubProxy.initialize(this,
                                  R.id.stub_left_frame, R.id.left_frame,
                                  R.id.dropshadow_left_short_top, R.id.dropshadow_left_long_top,
                                  R.id.left_adjust_button, R.raw.adjust_arrow_left, 1.0f,
                                  R.id.left_dropshadow_short, R.id.left_dropshadow_long);
    rightFrameStubProxy.initialize(this,
                                   R.id.stub_right_frame, R.id.right_frame,
                                   R.id.dropshadow_right_short_top, R.id.dropshadow_right_long_top,
                                   R.id.right_adjust_button, R.raw.adjust_arrow_right, 0.0f,
                                   R.id.right_dropshadow_short, R.id.right_dropshadow_long);
  }

  public void setEventListener(final ViewEventListener viewEventListener,
                               OnClickListener widenButtonClickListener,
                               OnClickListener leftAdjustButtonClickListener,
                               OnClickListener rightAdjustButtonClickListener) {
    checkInflated();

    // Propagate the given listener into the child views.
    // Set CandidateViewListener as well here, because it uses viewEventListener.
    Resources resources = getResources();
    int foldOvershootDurationRate =
        resources.getInteger(R.integer.input_frame_fold_overshoot_duration_rate);
    int foldOvershootRate =
        resources.getInteger(R.integer.input_frame_fold_overshoot_rate);
    int expandOvershootDurationRate =
        resources.getInteger(R.integer.input_frame_expand_overshoot_duration_rate);
    int expandOvershootRate =
        resources.getInteger(R.integer.input_frame_expand_overshoot_rate);
    getCandidateView().setViewEventListener(
        viewEventListener,
        new InputFrameFoldButtonClickListener(
            viewEventListener, getKeyboardFrame(),
            resources.getInteger(R.integer.input_frame_fold_duration),
            SequentialInterpolator.newBuilder()
                .add(new DecelerateInterpolator(),
                    foldOvershootDurationRate, -foldOvershootRate / 1e6f)
                .add(new AccelerateInterpolator(), 1e6f - foldOvershootDurationRate, 1)
                .build(),
            resources.getInteger(R.integer.input_frame_expand_duration),
            SequentialInterpolator.newBuilder()
                .add(new DecelerateInterpolator(),
                    expandOvershootDurationRate, 1 + expandOvershootRate / 1e6f)
                .add(new AccelerateDecelerateInterpolator(), 1e6f - expandOvershootDurationRate, 1)
                .build(),
        new LayoutParamsAnimator(new Handler(Looper.myLooper())),
        resources.getInteger(R.integer.input_frame_snap_velocity_threshold) / 1e3f));

    getSymbolInputView().setViewEventListener(
        viewEventListener,
        /**
         * Click handler of the close button.
         */
        new OnClickListener() {
          @Override
          public void onClick(View v) {
            if (viewEventListener != null) {
              viewEventListener.onFireFeedbackEvent(FeedbackEvent.INPUTVIEW_FOLD);
            }
            startSymbolInputViewOutAnimation();
          }
        });

    getHardwareCompositionButton().setOnClickListener(new OnClickListener() {
      @Override
      public void onClick(View v) {
        viewEventListener.onClickHardwareKeyboardCompositionModeButton();
      }
    });

    getWidenButton().setOnClickListener(widenButtonClickListener);

    leftFrameStubProxy.setButtonOnClickListener(leftAdjustButtonClickListener);
    rightFrameStubProxy.setButtonOnClickListener(rightAdjustButtonClickListener);
  }

  public void setKeyEventHandler(KeyEventHandler keyEventHandler) {
    checkInflated();

    // Propagate the given keyEventHandler to the child views.
    getKeyboardView().setKeyEventHandler(keyEventHandler);
    getSymbolInputView().setKeyEventHandler(keyEventHandler);
  }

  // TODO(hidehiko): Probably we'd like to remove this method when we decide to move MVC model.
  public JapaneseKeyboard getJapaneseKeyboard() {
    checkInflated();
    return getKeyboardView().getJapaneseKeyboard();
  }

  public void setJapaneseKeyboard(JapaneseKeyboard keyboard) {
    checkInflated();
    getKeyboardView().setJapaneseKeyboard(keyboard);
  }

  public void setEmojiEnabled(boolean emojiEnabled) {
    checkInflated();
    getSymbolInputView().setEmojiEnabled(emojiEnabled);
  }

  public void setFlickSensitivity(int flickSensitivity) {
    checkInflated();
    getKeyboardView().setFlickSensitivity(flickSensitivity);
  }

  public void setEmojiProviderType(EmojiProviderType emojiProviderType) {
    checkInflated();
    getSymbolInputView().setEmojiProviderType(emojiProviderType);
  }

  public void setSymbolCandidateStorage(SymbolCandidateStorage symbolCandidateStorage) {
    checkInflated();
    getSymbolInputView().setSymbolCandidateStorage(symbolCandidateStorage);
  }

  public void setPopupEnabled(boolean popupEnabled) {
    checkInflated();
    getKeyboardView().setPopupEnabled(popupEnabled);
  }

  public boolean isPopupEnabled() {
    checkInflated();
    return getKeyboardView().isPopupEnabled();
  }

  public void setSkinType(SkinType skinType) {
    checkInflated();
    this.skinType = skinType;
    getKeyboardView().setSkinType(skinType);
    getSymbolInputView().setSkinType(skinType);
    getCandidateView().setSkinType(skinType);
  }

  public SkinType getSkinType() {
    return skinType;
  }

  /**
   * Checks whether the inflation is finished or not. If not, throws an IllegalStateException,
   * or do nothing otherwise.
   * Exposed as a package private method for testing purpose.
   */
  void checkInflated() {
    if (getChildCount() == 0) {
      throw new IllegalStateException("It is necessary to inflate mozc_view.xml");
    }
  }

  public void setCommand(Command outCommand) {
    checkInflated();

    CandidateView candidateView = getCandidateView();
    if (outCommand.getOutput().getAllCandidateWords().getCandidatesCount() > 0) {
      // Call CandidateView#update only if there are some candidates in the output.
      // In such case the candidate view will clear its canvas.
      candidateView.update(outCommand);
      startCandidateViewInAnimation();

    } else {
      // We don't call update method here, because it will clear the view's contents during the
      // animation.
      // TODO(hidehiko): Clear the candidates when the animation is finished.
      startCandidateViewOutAnimation();
    }
  }

  public void reset() {
    checkInflated();

    // Reset keyboard frame and view.
    resetKeyboardFrameVisibility();
    resetKeyboardViewState();

    // Reset candidate view.
    CandidateView candidateView = getCandidateView();
    candidateView.clearAnimation();
    candidateView.setVisibility(View.GONE);
    candidateView.reset();

    // Reset symbol input view visibility. Set Visibility directly (without animation).
    SymbolInputView symbolInputView = getSymbolInputView();
    symbolInputView.clearAnimation();
    symbolInputView.setVisibility(View.GONE);

    resetFullscreenMode();
    setNarrowMode(narrowMode);
    setLayoutAdjustment(layoutAdjustment);
    collapseDropShadowAndBackground();
    updateBackgroundColor();
  }

  public void resetKeyboardFrameVisibility() {
    checkInflated();

    if (narrowMode) {
      return;
    }

    View keyboardFrame = getKeyboardFrame();
    keyboardFrame.setVisibility(View.VISIBLE);

    // The height may be changed so reset it here.
    ViewGroup.LayoutParams layoutParams = keyboardFrame.getLayoutParams();
    int keyboardFrameHeight = getInputFrameHeight();
    if (layoutParams.height != keyboardFrameHeight) {
      layoutParams.height = keyboardFrameHeight;
      keyboardFrame.setLayoutParams(layoutParams);

      // Also reset the state of the folding button, which is "conceptually" a part of
      // the keyboard.
      getCandidateView().setInputFrameFoldButtonChecked(false);
    }
  }

  public void resetKeyboardViewState() {
    checkInflated();

    getKeyboardView().resetState();
  }

  public boolean showSymbolInputView() {
    checkInflated();

    SymbolInputView view = getSymbolInputView();
    if (view.getVisibility() == View.VISIBLE) {
      return false;
    }

    if (!view.isInflated()) {
      view.inflateSelf();
    }

    view.reset();
    startSymbolInputViewInAnimation();
    return true;
  }

  public boolean hideSymbolInputView() {
    checkInflated();

    SymbolInputView view = getSymbolInputView();
    if (view.getVisibility() != View.VISIBLE) {
      return false;
    }

    startSymbolInputViewOutAnimation();
    return true;
  }

  /**
   * Decides input frame height in not fullscreen mode.
   */
  public int getVisibleViewHeight() {
    checkInflated();

    // Means only software keyboard or narrow frame
    boolean isDefaultView = getCandidateView().getVisibility() != View.VISIBLE
        && getSymbolInputView().getVisibility() != View.VISIBLE;

    if (narrowMode) {
      if (isDefaultView) {
        return dimensionPixelSize.narrowFrameHeight;
      } else {
        return dimensionPixelSize.narrowImeWindowHeight
            - dimensionPixelSize.translucentBorderHeight;
      }
    } else {
      if (isDefaultView) {
        return getInputFrameHeight();
      } else {
        return imeWindowHeight - dimensionPixelSize.translucentBorderHeight;
      }
    }
  }

  void updateInputFrameHeight() {
    // input_frame's height depends on fullscreen mode, narrow mode and Candidate/Symbol views.
    if (fullscreenMode) {
      setLayoutHeight(getBottomFrame(), getVisibleViewHeight()
          + dimensionPixelSize.translucentBorderHeight);
      setLayoutHeight(getKeyboardFrame(), getInputFrameHeight());
    } else {
      if (narrowMode) {
        setLayoutHeight(getBottomFrame(), dimensionPixelSize.narrowImeWindowHeight);
      } else {
        setLayoutHeight(getBottomFrame(), imeWindowHeight);
        setLayoutHeight(getKeyboardFrame(), getInputFrameHeight());
      }
    }
  }

  int getSideAdjustedWidth() {
    return dimensionPixelSize.imeWindowPartialWidth + dimensionPixelSize.sideFrameWidth;
  }

  public void setFullscreenMode(boolean fullscreenMode) {
    this.fullscreenMode = fullscreenMode;
  }

  public boolean isFullscreenMode() {
    return fullscreenMode;
  }

  void resetFullscreenMode() {
    if (fullscreenMode) {
      // In fullscreen mode, InputMethodService shows extract view which height is 0 and
      // weight is 0. So our MozcView height should be fixed.
      // If CandidateView or SymbolInputView appears, MozcView height is enlarged to fix them.
      setLayoutHeight(getOverlayView(), 0);
      setLayoutHeight(getTextInputFrame(), LayoutParams.WRAP_CONTENT);
      getCandidateView().setOnVisibilityChangeListener(onVisibilityChangeListener);
      getSymbolInputView().setOnVisibilityChangeListener(onVisibilityChangeListener);
    } else {
      setLayoutHeight(getOverlayView(), LayoutParams.MATCH_PARENT);
      setLayoutHeight(getTextInputFrame(), LayoutParams.MATCH_PARENT);
      getCandidateView().setOnVisibilityChangeListener(null);
      getSymbolInputView().setOnVisibilityChangeListener(null);
    }

    updateInputFrameHeight();
    updateBackgroundColor();
  }

  static void setLayoutHeight(View view, int height) {
    ViewGroup.LayoutParams layoutParams = view.getLayoutParams();
    layoutParams.height = height;
    view.setLayoutParams(layoutParams);
  }

  public boolean isNarrowMode() {
    return narrowMode;
  }

  public void setNarrowMode(boolean narrowMode) {
    this.narrowMode = narrowMode;

    // In narrow mode, hide software keyboard and show narrow status bar.
    getCandidateView().setNarrowMode(narrowMode);
    if (narrowMode) {
      getKeyboardFrame().setVisibility(GONE);
      getNarrowFrame().setVisibility(VISIBLE);
    } else {
      getKeyboardFrame().setVisibility(VISIBLE);
      getNarrowFrame().setVisibility(GONE);
      resetKeyboardFrameVisibility();
    }

    setLayoutAdjustment(layoutAdjustment);
    updateInputFrameHeight();
    updateBackgroundColor();
  }

  public void setHardwareCompositionButtonImage(CompositionMode compositionMode) {
    switch (compositionMode) {
      case HIRAGANA:
        getHardwareCompositionButton().setImageDrawable(
            mozcDrawableFactory.getDrawable(R.raw.qwerty__function__kana__icon));
        break;
      default:
        getHardwareCompositionButton().setImageDrawable(
            mozcDrawableFactory.getDrawable(R.raw.qwerty__function__alphabet__icon));
        break;
    }
  }

  public Rect getKeyboardSize() {
    Resources resources = getResources();
    // TODO(yoichio): replace resources.getDisplayMetrics().widthPixels with targetWindow.width.
    return new Rect(0, 0,
                    layoutAdjustment == LayoutAdjustment.FILL
                        ? resources.getDisplayMetrics().widthPixels
                        : dimensionPixelSize.imeWindowPartialWidth,
                    getInputFrameHeight());
  }

  public void setLayoutAdjustment(LayoutAdjustment layoutAdjustment) {
    checkInflated();

    this.layoutAdjustment = layoutAdjustment;

    // If on narrowMode, the view is always shown with full-width regard less of given
    // layoutAdjustment.
    LayoutAdjustment temporaryAdjustment = narrowMode ? LayoutAdjustment.FILL : layoutAdjustment;

    View view = getForegroundFrame();
    FrameLayout.LayoutParams layoutParams =
        FrameLayout.LayoutParams.class.cast(view.getLayoutParams());
    Resources resources = getResources();
    layoutParams.width = temporaryAdjustment == LayoutAdjustment.FILL
        ? resources.getDisplayMetrics().widthPixels : getSideAdjustedWidth();
    layoutParams.gravity = Gravity.BOTTOM;
    if (temporaryAdjustment == LayoutAdjustment.LEFT) {
      layoutParams.gravity |= Gravity.LEFT;
    } else if (temporaryAdjustment == LayoutAdjustment.RIGHT) {
      layoutParams.gravity |= Gravity.RIGHT;
    }
    view.setLayoutParams(layoutParams);

    leftFrameStubProxy.setFrameVisibility(
        temporaryAdjustment == LayoutAdjustment.RIGHT ? VISIBLE : GONE);
    rightFrameStubProxy.setFrameVisibility(
        temporaryAdjustment == LayoutAdjustment.LEFT ? VISIBLE : GONE);

    updateBackgroundColor();

    // TODO(yoichio): Update SymbolInputView width scale.
    // getSymbolInputView().setWidthScale(layoutParams.width
    //     / (float)resources.getDisplayMetrics().widthPixels);
  }

  public void startLayoutAdjustmentAnimation() {
    Resources resources = getResources();
    int delta = resources.getDisplayMetrics().widthPixels
        - dimensionPixelSize.imeWindowPartialWidth;
    TranslateAnimation translateAnimation = new TranslateAnimation(
        layoutAdjustment == LayoutAdjustment.LEFT ? delta : -delta, 0, 0, 0);
    translateAnimation.setDuration(resources.getInteger(
        R.integer.layout_adjustment_transition_duration));
    translateAnimation.setInterpolator(new DecelerateInterpolator());
    getForegroundFrame().startAnimation(translateAnimation);
  }

  void updateBackgroundColor() {
    // If fullscreenMode, background should not show original window.
    // If narrowMode, it is always full-width.
    // If isFloatingMode, background should be transparent.
    int resourceId = (fullscreenMode || (!narrowMode && !insetsCalculator.isFloatingMode(this))) ?
        R.color.input_frame_background : 0;
    getBottomBackground().setBackgroundResource(resourceId);
  }

  /**
   * This function is called to compute insets.
   */
  public void setInsets(int contentViewWidth, int contentViewHeight, Insets outInsets) {
    insetsCalculator.setInsets(this, contentViewWidth, contentViewHeight, outInsets);
  }


  void expandDropShadowAndBackground() {
    leftFrameStubProxy.flipDropShadowVisibility(INVISIBLE);
    rightFrameStubProxy.flipDropShadowVisibility(INVISIBLE);
    getDropShadowTop().setVisibility(VISIBLE);
    getResources();
    setLayoutHeight(getBottomBackground(), imeWindowHeight
        - (fullscreenMode ? 0 : dimensionPixelSize.translucentBorderHeight));
    isDropShadowExpanded = true;
  }

  void collapseDropShadowAndBackground() {
    leftFrameStubProxy.flipDropShadowVisibility(VISIBLE);
    rightFrameStubProxy.flipDropShadowVisibility(VISIBLE);
    getDropShadowTop().setVisibility(fullscreenMode ? VISIBLE : INVISIBLE);
    getResources();
    setLayoutHeight(getBottomBackground(), getInputFrameHeight()
        + (fullscreenMode ? dimensionPixelSize.translucentBorderHeight : 0));
    isDropShadowExpanded = false;
  }

  void startDropShadowAnimation(Animation mainAnimation, Animation subAnimation) {
    leftFrameStubProxy.startDropShadowAnimation(subAnimation, mainAnimation);
    rightFrameStubProxy.startDropShadowAnimation(subAnimation, mainAnimation);
    getDropShadowTop().startAnimation(mainAnimation);
  }

  void startCandidateViewInAnimation() {
    getCandidateView().startInAnimation();
    if (!isDropShadowExpanded) {
      expandDropShadowAndBackground();
      startDropShadowAnimation(candidateViewInAnimation, dropShadowCandidateViewInAnimation);
    }
  }

  void startCandidateViewOutAnimation() {
    getCandidateView().startOutAnimation();
    if (getSymbolInputView().getVisibility() != VISIBLE && isDropShadowExpanded) {
      collapseDropShadowAndBackground();
      startDropShadowAnimation(candidateViewOutAnimation, dropShadowCandidateViewOutAnimation);
    }
  }

  void startSymbolInputViewInAnimation() {
    getSymbolInputView().startInAnimation();
    if (!isDropShadowExpanded) {
      expandDropShadowAndBackground();
      startDropShadowAnimation(symbolInputViewInAnimation, dropShadowSymbolInputViewInAnimation);
    }
  }

  void startSymbolInputViewOutAnimation() {
    getSymbolInputView().startOutAnimation();
    if (getCandidateView().getVisibility() != VISIBLE && isDropShadowExpanded) {
      collapseDropShadowAndBackground();
      startDropShadowAnimation(symbolInputViewOutAnimation,
                               dropShadowSymbolInputViewOutAnimation);
    }
  }

  /**
   * Reset components depending inputFrameHeight or imeWindowHeight.
   * This should be called when inputFrameHeight and/or imeWindowHeight are updated.
   */
  void resetHeightDependingComponents() {
    // Create In/Out animation which dropshadows share between CandidateView and SymbolInputView.
    {
      CandidateView candidateView = getCandidateView();
      int windowHeight = imeWindowHeight;
      int inputFrameHeight = getInputFrameHeight();
      int candidateViewHeight = windowHeight - inputFrameHeight;
      long duration = getResources().getInteger(R.integer.candidate_frame_transition_duration);
      float fromAlpha = 0.0f;
      float toAlpha = 1.0f;

      candidateViewInAnimation = createCandidateViewTransitionAnimation(
          candidateViewHeight, 0, fromAlpha, toAlpha, duration);
      candidateView.setInAnimation(candidateViewInAnimation);
      dropShadowCandidateViewInAnimation = createAlphaAnimation(
          1.0f - fromAlpha, 1.0f - toAlpha, duration);

      candidateViewOutAnimation = createCandidateViewTransitionAnimation(
          0, candidateViewHeight, toAlpha, fromAlpha, duration);
      candidateView.setOutAnimation(candidateViewOutAnimation);
      dropShadowCandidateViewOutAnimation = createAlphaAnimation(
          1.0f - toAlpha, 1.0f - fromAlpha, duration);
    }

    SymbolInputView symbolInputView = getSymbolInputView();
    {
      long duration = getResources().getInteger(R.integer.symbol_input_transition_duration_in);
      float fromAlpha = 0.3f;
      float toAlpha = 1.0f;

      symbolInputViewInAnimation = createAlphaAnimation(fromAlpha, toAlpha, duration);
      symbolInputView.setInAnimation(symbolInputViewInAnimation);
      dropShadowSymbolInputViewInAnimation = createAlphaAnimation(
          1.0f - fromAlpha, 1.0f - toAlpha, duration);

      symbolInputViewOutAnimation = createAlphaAnimation(toAlpha, fromAlpha, duration);
      symbolInputView.setOutAnimation(symbolInputViewOutAnimation);
      dropShadowSymbolInputViewOutAnimation = createAlphaAnimation(
          1.0f - toAlpha, 1.0f - fromAlpha, duration);
    }

    // Reset drop shadow height.
    int shortHeight = getInputFrameHeight() + dimensionPixelSize.translucentBorderHeight;
    int longHeight = imeWindowHeight;
    leftFrameStubProxy.setDropShadowHeight(shortHeight, longHeight);
    rightFrameStubProxy.setDropShadowHeight(shortHeight, longHeight);

    // Reset side adjust buttons height.
    leftFrameStubProxy.resetAdjustButtonBottomMargin(getInputFrameHeight());
    rightFrameStubProxy.resetAdjustButtonBottomMargin(getInputFrameHeight());
  }

  /**
   * Sets keyboard height rated to original height.
   * @param keyboardHeightRatio target ratio percentage. Default is 100.
   */
  public void setKeyboardHeightRatio(int keyboardHeightRatio) {
    checkInflated();

    Resources resources = getResources();
    float heightScale = keyboardHeightRatio * 0.01f;
    int originalImeWindowHeight = resources.getDimensionPixelSize(R.dimen.ime_window_height);
    int originalInputFrameHeight = resources.getDimensionPixelSize(R.dimen.input_frame_height);
    imeWindowHeight = Math.round(originalImeWindowHeight * heightScale);
    inputFrameHeight = Math.round(originalInputFrameHeight * heightScale);
    // TODO(yoichio): Update SymbolInputView height scale.
    // getSymbolInputView().setHeightScale(heightScale);

    updateInputFrameHeight();
    resetHeightDependingComponents();
  }

  public int getInputFrameHeight() {
    return inputFrameHeight;
  }

  // Getters of child views.
  // TODO(hidehiko): Remove (or hide) following methods, in order to split the dependencies to
  //   those child views from other components.
  public CandidateView getCandidateView() {
    return CandidateView.class.cast(findViewById(R.id.candidate_view));
  }

  public View getKeyboardFrame() {
    return findViewById(R.id.keyboard_frame);
  }

  public JapaneseKeyboardView getKeyboardView() {
    return JapaneseKeyboardView.class.cast(findViewById(R.id.keyboard_view));
  }

  public SymbolInputView getSymbolInputView() {
    return SymbolInputView.class.cast(findViewById(R.id.symbol_input_view));
  }

  View getOverlayView() {
    return findViewById(R.id.overlay_view);
  }

  LinearLayout getTextInputFrame() {
    return LinearLayout.class.cast(findViewById(R.id.textinput_frame));
  }

  FrameLayout getNarrowFrame() {
    return FrameLayout.class.cast(findViewById(R.id.narrow_frame));
  }

  ImageView getHardwareCompositionButton() {
    return ImageView.class.cast(findViewById(R.id.hardware_composition_button));
  }

  ImageView getWidenButton() {
    return ImageView.class.cast(findViewById(R.id.widen_button));
  }

  View getForegroundFrame() {
    return findViewById(R.id.foreground_frame);
  }

  View getDropShadowTop() {
    return findViewById(R.id.dropshadow_top);
  }

  View getBottomFrame() {
    return findViewById(R.id.bottom_frame);
  }

  View getBottomBackground() {
    return findViewById(R.id.bottom_background);
  }
}
