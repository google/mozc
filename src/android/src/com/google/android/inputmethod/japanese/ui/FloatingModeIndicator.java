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

import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.model.FloatingModeIndicatorController;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.SessionCommand;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.util.CursorAnchorInfoWrapper;
import org.mozc.android.inputmethod.japanese.view.MozcImageView;
import org.mozc.android.inputmethod.japanese.view.Skin;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Preconditions;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.inputmethodservice.InputMethodService;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.view.View;
import android.view.animation.AlphaAnimation;
import android.view.animation.Animation;
import android.view.animation.Animation.AnimationListener;
import android.view.animation.AnimationSet;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.ScaleAnimation;
import android.view.inputmethod.EditorInfo;

/**
 * Draws mode indicator for floating candidate window.
 */
@TargetApi(21)
public class FloatingModeIndicator {

  /** The message to hide the mode indicator. */
  @VisibleForTesting static final int HIDE_MODE_INDICATOR = 0;
  @VisibleForTesting static final int SHOW_MODE_INDICATOR = 1;

  /**
   * Delay to ensure that the cursor position is stabilized.
   * <p>
   * Unfortunately, the cursor position is unstable especially on start.
   * We don't have a concrete way to know that the cursor position is stabilized or not, since
   * {@link InputMethodService#onUpdateCursorAnchorInfo} is not called until it is updated.
   * As a workaround, we introduced this delay.
   * <p>
   * The delay should be greater than 33.3 msec for 30fps devices.
   */
  private static final int DELAY_TO_STABILIZE_MILLIS = 50;

  private class OutAnimationListener implements AnimationListener {
    @Override
    public void onAnimationEnd(Animation animation) {
      if (!isVisible) {
        popup.getContentView().setVisibility(View.GONE);
      }
    }

    @Override
    public void onAnimationRepeat(Animation animation) {
    }

    @Override
    public void onAnimationStart(Animation animation) {
    }
  }

  @VisibleForTesting class ModeIndicatorMessageCallback implements Handler.Callback {
    @Override
    public boolean handleMessage(Message msg) {
      return handleWhat(msg.what);
    }

    @VisibleForTesting boolean handleWhat(int what) {
      switch (what) {
        case HIDE_MODE_INDICATOR:
          hide();
          break;
        case SHOW_MODE_INDICATOR:
          show();
          break;
        default:
          MozcLog.e("Unknown message. what:" + what);
          break;
      }
      return true;
    }
  }

  private class ControllerListenerImpl
      implements FloatingModeIndicatorController.ControllerListener {
    public ControllerListenerImpl() {}

    @Override
    public void show(CompositionMode mode) {
      updateIcon(mode);
      FloatingModeIndicator.this.show();
    }

    @Override
    public void showWithDelay(CompositionMode mode) {
      updateIcon(mode);
      FloatingModeIndicator.this.showWithDelay();
    }

    @Override
    public void hide() {
      FloatingModeIndicator.this.hide();
    }
  }

  @VisibleForTesting final Handler handler;
  @VisibleForTesting final PopUpLayouter<MozcImageView> popup;
  @VisibleForTesting final ModeIndicatorMessageCallback messageCallback =
      new ModeIndicatorMessageCallback();
  private final FloatingModeIndicatorController controller =
      new FloatingModeIndicatorController(new ControllerListenerImpl());
  private final View parentView;
  @VisibleForTesting final Drawable kanaIndicatorDrawable;
  @VisibleForTesting final Drawable abcIndicatorDrawable;

  private final int indicatorSize;
  private final int verticalMargin;
  private final Rect drawRect;
  private final Animation inAnimation;
  private final Animation outAnimation;
  private final int displayTime;

  private CursorAnchorInfoWrapper cursorAnchorInfo = new CursorAnchorInfoWrapper();
  /** True if the mode indicator is shown and is not hiding. */
  private boolean isVisible = false;

  public FloatingModeIndicator(View parent) {
    parentView = Preconditions.checkNotNull(parent);
    handler = new Handler(Looper.getMainLooper(), messageCallback);

    Context context = parent.getContext();
    Resources resources = context.getResources();
    Skin skin = Skin.getFallbackInstance();
    kanaIndicatorDrawable =
        skin.getDrawable(resources, R.raw.floating_mode_indicator__kana_normal);
    abcIndicatorDrawable =
        skin.getDrawable(resources, R.raw.floating_mode_indicator__alphabet_normal);
    indicatorSize =
        resources.getDimensionPixelSize(R.dimen.floating_mode_indicator_size);
    verticalMargin =
        resources.getDimensionPixelSize(R.dimen.floating_mode_indicator_vertical_margin);
    displayTime = resources.getInteger(R.integer.floating_mode_indicator_display_time);

    MozcImageView contentView = new MozcImageView(context);
    contentView.setVisibility(View.GONE);
    popup = new PopUpLayouter<MozcImageView>(parentView, contentView);

    inAnimation =  createInAnimation(resources, indicatorSize / 2f, verticalMargin);
    outAnimation = createOutAnimation(resources, indicatorSize / 2f, verticalMargin);
    drawRect = new Rect(0, 0, indicatorSize, indicatorSize);
  }

  public void onStartInputView(EditorInfo editorInfo) {
    reset();
    controller.onStartInputView(System.currentTimeMillis(), editorInfo);
  }

  public void setCursorAnchorInfo(CursorAnchorInfoWrapper cursorAnchorInfo) {
    this.cursorAnchorInfo = Preconditions.checkNotNull(cursorAnchorInfo);
    updateDrawRect();
    controller.onCursorAnchorInfoChanged(System.currentTimeMillis());
  }

  private void updateDrawRect() {
    float[] cursorPosition = new float[] {
        cursorAnchorInfo.getInsertionMarkerHorizontal(),
        cursorAnchorInfo.getInsertionMarkerBottom()
    };
    cursorAnchorInfo.getMatrix().mapPoints(cursorPosition);
    int location[] = new int[2];
    parentView.getLocationOnScreen(location);
    int left = Math.round(cursorPosition[0] - indicatorSize / 2) - location[0];
    int top = Math.round(cursorPosition[1] + verticalMargin) - location[1];
    // TODO(hsumita): Put the indicator over the cursor if there is no enough space below.
    // Note: We always have enough space below thanks to the narrow frame at this time.
    drawRect.offsetTo(left, top);
    popup.setBounds(drawRect);
  }

  /**
   * Updates the state of the mode indicator according to the {@code command}.
   * <p>
   * This method hides the indicator if there is a composition text.
   */
  public void setCommand(Command command) {
    Preconditions.checkNotNull(command);
    Input input = command.getInput();
    if (input.getType() == Input.CommandType.SEND_COMMAND
        && input.getCommand().getType() == SessionCommand.CommandType.SWITCH_INPUT_MODE) {
      // Simply ignores SWITCH_INPUT_MODE since the command doesn't have a composition text.
      return;
    }
    boolean hasComposition = command.getOutput().getPreedit().getSegmentCount() > 0;
    controller.setHasComposition(System.currentTimeMillis(), hasComposition);
  }

  /** Shows the mode indicator according to the current composition mode. */
  public void setCompositionMode(CompositionMode mode) {
    controller.setCompositionMode(System.currentTimeMillis(), mode);
  }

  /**
   * Shows the mode indicator with animation.
   * <p>
   * This method issues hide command with delay.
   */
  private void show() {
    handler.removeMessages(SHOW_MODE_INDICATOR);
    resetAnimation();
    if (!isVisible) {
      isVisible = true;
      View contentView = popup.getContentView();
      contentView.setVisibility(View.VISIBLE);
      contentView.startAnimation(inAnimation);
      controller.markCursorPositionStabilized();
    }
    handler.sendMessageDelayed(handler.obtainMessage(HIDE_MODE_INDICATOR), displayTime);
  }

  private void showWithDelay() {
    if (isVisible) {
      // Show the indicator without delay if it is already shown.
      show();
    } else {
      handler.removeMessages(SHOW_MODE_INDICATOR);
      handler.sendMessageDelayed(
          handler.obtainMessage(SHOW_MODE_INDICATOR), DELAY_TO_STABILIZE_MILLIS);
      Preconditions.checkState(handler.hasMessages(SHOW_MODE_INDICATOR));
    }
  }

  /** Hides the mode indicator with animation. */
  public void hide() {
    if (!isVisible) {
      return;
    }
    isVisible = false;
    resetAnimation();
    popup.getContentView().startAnimation(outAnimation);
  }

  private Animation createInAnimation(Resources resources, float pivotX, float pivotY) {
    AnimationSet animationSet = new AnimationSet(true);
    animationSet.setDuration(resources.getInteger(R.integer.floating_mode_indicator_in_duration));
    animationSet.setInterpolator(new DecelerateInterpolator());
    animationSet.addAnimation(new ScaleAnimation(0f, 1f, 0f, 1f, pivotX, pivotY));
    animationSet.addAnimation(new AlphaAnimation(0f, 1f));
    return animationSet;
  }

  private Animation createOutAnimation(Resources resources, float pivotX, float pivotY) {
    AnimationSet animationSet = new AnimationSet(true);
    animationSet.setDuration(resources.getInteger(R.integer.floating_mode_indicator_out_duration));
    animationSet.setInterpolator(new DecelerateInterpolator());
    animationSet.setAnimationListener(new OutAnimationListener());
    animationSet.addAnimation(new ScaleAnimation(1f, 0f, 1f, 0f, pivotX, pivotY));
    animationSet.addAnimation(new AlphaAnimation(1f, 0f));
    return animationSet;
  }

  private void updateIcon(CompositionMode mode) {
    popup.getContentView().setImageDrawable(mode == CompositionMode.HIRAGANA
        ? kanaIndicatorDrawable : abcIndicatorDrawable);
  }

  private void reset() {
    resetAnimation();
    isVisible = false;
    popup.getContentView().setVisibility(View.GONE);
  }

  /** Resets all ongoing and scheduled animations. */
  private void resetAnimation() {
    popup.getContentView().clearAnimation();
    handler.removeMessages(HIDE_MODE_INDICATOR);
    handler.removeMessages(SHOW_MODE_INDICATOR);
  }

  @VisibleForTesting boolean isVisible() {
    return isVisible;
  }
}
