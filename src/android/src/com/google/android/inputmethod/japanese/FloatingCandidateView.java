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

import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.Category;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Output;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Preedit.Segment;
import org.mozc.android.inputmethod.japanese.ui.FloatingCandidateLayoutRenderer;
import org.mozc.android.inputmethod.japanese.ui.FloatingModeIndicator;
import org.mozc.android.inputmethod.japanese.util.CursorAnchorInfoWrapper;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.text.InputType;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.PopupWindow;

/**
 * Floating candidate view for hardware keyboard.
 */
@TargetApi(21)
public class FloatingCandidateView extends View {

  private interface FloatingCandidateViewProxy {
    public void draw(Canvas canvas);
    public void viewSizeChanged(int width, int height);
    public void onStartInputView(EditorInfo editorInfo);
    public void setCursorAnchorInfo(CursorAnchorInfoWrapper info);
    public void setCandidates(Command outCommand);
    public void setEditorInfo(EditorInfo editorInfo);
    public void setCompositionMode(CompositionMode mode);
    public void setViewEventListener(ViewEventListener listener);
    public void setVisibility(int visibility);
    public Optional<Rect> getVisibleRect();
  }

  private static class FloatingCandidateViewStub implements FloatingCandidateViewProxy {
    @Override
    public void draw(Canvas canvas) {}

    @Override
    public void viewSizeChanged(int width, int height) {}

    @Override
    public void onStartInputView(EditorInfo editorInfo) {}

    @Override
    public void setCursorAnchorInfo(CursorAnchorInfoWrapper info) {}

    @Override
    public void setCandidates(Command outCommand) {}

    @Override
    public void setEditorInfo(EditorInfo editorInfo) {}

    @Override
    public void setCompositionMode(CompositionMode mode) {}

    @Override
    public void setViewEventListener(ViewEventListener listener) {}

    @Override
    public void setVisibility(int visibility) {}

    @Override
    public Optional<Rect> getVisibleRect() {
      return Optional.absent();
    }
  }

  @TargetApi(21)
  private static class FloatingCandidateViewImpl implements FloatingCandidateViewProxy {

    private final View parentView;

    /** Layouts the floating candidate window and draws it's contents. */
    private final FloatingCandidateLayoutRenderer layoutRenderer;

    private final FloatingModeIndicator modeIndicator;

    /**
     * Pop-up window to handle touch events.
     * <p>
     * A touch down event on outside a touchable region (set by {@link MozcService#onComputeInsets})
     * cannot be caught by view, and we cannot expand the touchable region since all touch down
     * events inside the region are not delegated to a background application.
     * To handle these touch events, we employ pop-up window.
     * <p>
     * This window is always invisible since we cannot control the transition behavior.
     * (e.g. Pop-up window always move with animation)
     */
    private final PopupWindow touchEventReceiverWindow;

    private final int windowVerticalMargin;
    private final int windowHorizontalMargin;

    /**
     * Base position of the floating candidate window.
     * <p>
     * It is same as the cursor rectangle on pre-composition state, and the left-edge of the focused
     * segment on other states.
     */
    private int basePositionTop;
    private int basePositionBottom;
    private int basePositionX;
    private Optional<CursorAnchorInfoWrapper> cursorAnchorInfo = Optional.absent();
    private Category candidatesCategory = Category.CONVERSION;
    private int highlightedCharacterStart;
    private int compositionCharacterEnd;
    /** True if EditorInfo says suggestion should be suppressed. */
    private boolean suppressSuggestion;
    /**
     * Horizontal offset of the candidate window. See also {@link FloatingCandidateLayoutRenderer}
     */
    private int offsetX;
    /** Vertical offset of the candidate window. See also {@link FloatingCandidateLayoutRenderer} */
    private int offsetY;
    private boolean isCandidateWindowShowing;

    public FloatingCandidateViewImpl(View parentView) {
      Context context = Preconditions.checkNotNull(parentView).getContext();
      this.parentView = parentView;
      this.layoutRenderer = new FloatingCandidateLayoutRenderer(context.getResources());
      this.modeIndicator = new FloatingModeIndicator(parentView);
      this.touchEventReceiverWindow = createPopupWindow(context);
      Resources resources = context.getResources();
      this.windowVerticalMargin =
          Math.round(resources.getDimension(R.dimen.floating_candidate_window_vertical_margin));
      this.windowHorizontalMargin =
          Math.round(resources.getDimension(R.dimen.floating_candidate_window_horizontal_margin));
    }

    public FloatingCandidateViewImpl(View parentView, PopupWindow popupWindowMock,
                                     FloatingCandidateLayoutRenderer layoutRenderer,
                                     FloatingModeIndicator modeIndicator) {
      this.parentView = Preconditions.checkNotNull(parentView);
      this.layoutRenderer = Preconditions.checkNotNull(layoutRenderer);
      this.modeIndicator = Preconditions.checkNotNull(modeIndicator);
      this.touchEventReceiverWindow = Preconditions.checkNotNull(popupWindowMock);
      Resources resources = parentView.getContext().getResources();
      this.windowVerticalMargin =
          resources.getDimensionPixelSize(R.dimen.floating_candidate_window_vertical_margin);
      this.windowHorizontalMargin =
          resources.getDimensionPixelSize(R.dimen.floating_candidate_window_horizontal_margin);
    }

    private PopupWindow createPopupWindow(Context context) {
      return new PopupWindow(new View(context) {
        @Override
        public boolean onTouchEvent(MotionEvent event) {
          Optional<Rect> rect = layoutRenderer.getWindowRect();
          if (!rect.isPresent()) {
            return false;
          }

          MotionEvent copiedEvent = MotionEvent.obtain(event);
          try {
            copiedEvent.offsetLocation(rect.get().left, rect.get().top);
            layoutRenderer.onTouchEvent(copiedEvent);
            // TODO(hsumita): Don't invalidate the view if not necessary.
            parentView.invalidate();
          } finally {
            copiedEvent.recycle();
          }
          return true;
        }
      });
    }

    @Override
    public void draw(Canvas canvas) {
      if (!isCandidateWindowShowing) {
        return;
      }

      int saveId = canvas.save(Canvas.MATRIX_SAVE_FLAG);
      try {
        canvas.translate(offsetX, offsetY);
        layoutRenderer.draw(canvas);
      } finally {
        canvas.restoreToCount(saveId);
      }
    }

    @Override
    public void viewSizeChanged(int width, int height) {
      layoutRenderer.setMaxWidth(width - windowHorizontalMargin * 2);
      updateCandidateWindowWithSize(width, height);
    }

    @Override
    public void onStartInputView(EditorInfo editorInfo) {
      modeIndicator.onStartInputView(editorInfo);
    }

    /** Sets {@link CursorAnchorInfoWrapper} to update the candidate window position. */
    @Override
    public void setCursorAnchorInfo(CursorAnchorInfoWrapper info) {
      cursorAnchorInfo = Optional.of(info);
      modeIndicator.setCursorAnchorInfo(info);
      updateCandidateWindow();
    }

    /** Sets {@link Command} to update the contents of the candidate window. */
    @Override
    public void setCandidates(Command outCommand) {
      Output output = Preconditions.checkNotNull(outCommand).getOutput();
      layoutRenderer.setCandidates(outCommand);
      modeIndicator.setCommand(outCommand);
      highlightedCharacterStart = output.getPreedit().getHighlightedPosition();
      int currentPreeditPosition = 0;
      for (Segment segment : output.getPreedit().getSegmentList()) {
        currentPreeditPosition += segment.getValueLength();
      }
      compositionCharacterEnd = currentPreeditPosition;
      candidatesCategory = output.getCandidates().getCategory();
      updateCandidateWindow();
    }

    /** Sets {@link EditorInfo} for context-aware behavior. */
    @Override
    public void setEditorInfo(EditorInfo editorInfo) {
      Preconditions.checkNotNull(editorInfo);
      boolean previusSuppressSuggestion = suppressSuggestion;
      suppressSuggestion = shouldSuppressSuggestion(editorInfo);
      if (previusSuppressSuggestion != suppressSuggestion) {
        updateCandidateWindow();
      }
    }

    @Override
    public void setCompositionMode(CompositionMode mode) {
      modeIndicator.setCompositionMode(mode);
    }

    /** Set view event listener to handle events invoked by the candidate window. */
    @Override
    public void setViewEventListener(ViewEventListener listener) {
      layoutRenderer.setViewEventListener(Preconditions.checkNotNull(listener));
    }

    @Override
    public void setVisibility(int visibility) {
      if (visibility != View.VISIBLE) {
        modeIndicator.hide();
      }
    }

    /**
     * Updates the candidate window.
     * <p>
     * All layout related states should be updated before call this method.
     */
    private void updateCandidateWindow() {
      updateCandidateWindowWithSize(parentView.getWidth(), parentView.getHeight());
    }

    private int calculateWindowLeftPosition(Rect rect, int basePositionX, int viewWidth) {
      return MozcUtil.clamp(
          basePositionX + rect.left,
          windowHorizontalMargin, viewWidth - rect.width() - windowHorizontalMargin);
    }

    /**
     * Updates the candidate window with width and height.
     * <p>
     * All layout related states should be updated before call this method.
     */
    private void updateCandidateWindowWithSize(int viewWidth, int viewHeight) {
      if (suppressSuggestion && candidatesCategory == Category.SUGGESTION) {
        dismissCandidateWindow();
        return;
      }

      Optional<Rect> optionalWindowRect = layoutRenderer.getWindowRect();
      if (!optionalWindowRect.isPresent()) {
        dismissCandidateWindow();
        return;
      }

      Rect rect = optionalWindowRect.get();
      updateBasePosition(rect, viewWidth);
      int lowerAreaHeight = viewHeight - basePositionBottom - windowVerticalMargin;
      int upperAreaHeight = basePositionTop - windowVerticalMargin;
      int top = (lowerAreaHeight < rect.height() && lowerAreaHeight < upperAreaHeight)
        ? MozcUtil.clamp(basePositionTop - rect.height() - windowVerticalMargin,
                         0, viewHeight - rect.height())
        : Math.max(0, basePositionBottom + windowVerticalMargin);
      int left = calculateWindowLeftPosition(rect, basePositionX, viewWidth);

      offsetX = left - rect.left;
      offsetY = top - rect.top;

      rect.offset(offsetX, offsetY);
      showCandidateWindow(rect);
    }

    /** Return true if floating candidate window should be suppressed. */
    private boolean shouldSuppressSuggestion(EditorInfo editorInfo) {
      if ((editorInfo.inputType & EditorInfo.TYPE_MASK_CLASS) != InputType.TYPE_CLASS_TEXT) {
        return true;
      }

      if ((editorInfo.inputType
           & (InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS | InputType.TYPE_TEXT_FLAG_AUTO_COMPLETE))
           != 0) {
        return true;
      }

      switch (editorInfo.inputType & EditorInfo.TYPE_MASK_VARIATION) {
        case InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS:
        case InputType.TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS:
        case InputType.TYPE_TEXT_VARIATION_PASSWORD:
        case InputType.TYPE_TEXT_VARIATION_WEB_PASSWORD:
        case InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD:
        case InputType.TYPE_TEXT_VARIATION_URI:
        case InputType.TYPE_TEXT_VARIATION_FILTER:
          return true;
        default:
          return false;
      }
    }

    private void resetBasePosition() {
      basePositionTop = 0;
      basePositionBottom = 0;
      basePositionX = 0;
      return;
    }

    /**
     * Update {@code basePositionTop}, {@code basePositionBottom} and {@code basePositionX} using
     * {@code cursorAnchorInfoWrapper}.
     */
    private void updateBasePosition(Rect windowRect, int viewWidth) {
      if (!cursorAnchorInfo.isPresent()) {
        resetBasePosition();
        return;
      }

      CursorAnchorInfoWrapper info = cursorAnchorInfo.get();
      int composingStartIndex = info.getComposingTextStart() + highlightedCharacterStart;
      int composingEndIndex = info.getComposingTextStart() + compositionCharacterEnd - 1;
      RectF firstCharacterBounds = info.getCharacterBounds(composingStartIndex);
      float[] points;
      if (firstCharacterBounds != null) {
        points = new float[] {firstCharacterBounds.left, firstCharacterBounds.top,
                              firstCharacterBounds.left, firstCharacterBounds.bottom};
      } else if (!Float.isNaN(info.getInsertionMarkerHorizontal())) {
        points = new float[] {info.getInsertionMarkerHorizontal(), info.getInsertionMarkerTop(),
                              info.getInsertionMarkerHorizontal(), info.getInsertionMarkerBottom()};
      } else {
        resetBasePosition();
        return;
      }

      // Adjust the bottom base position not to hide composition characters by the floating
      // candidate window.
      int windowLeft = calculateWindowLeftPosition(windowRect, (int) points[0], viewWidth);
      for (int i = composingEndIndex; i > composingStartIndex; --i) {
        RectF bounds = info.getCharacterBounds(i);
        if (bounds == null) {
          continue;
        }
        if (bounds.bottom <= points[3]) {
          break;
        }
        if (bounds.right > windowLeft) {
          points[3] = bounds.bottom;
          break;
        }
      }

      info.getMatrix().mapPoints(points);
      int[] screenOffset = new int[2];
      parentView.getLocationOnScreen(screenOffset);
      basePositionX = Math.round(points[0]) - screenOffset[0];
      basePositionTop = Math.round(points[1]) - screenOffset[1];
      basePositionBottom = Math.round(points[3]) - screenOffset[1];
    }

    /**
     * Shows the candidate window.
     * <p>
     * First {@code touchEventReceiverWindow} is shown (or is updated its position if it has been
     * already shown). Then this view is invalidated. As the result {@code draw} will be called back
     * and visible candidate window will be shown.
    */
    private void showCandidateWindow(Rect rect) {
      isCandidateWindowShowing = true;
      if (touchEventReceiverWindow.isShowing()) {
        touchEventReceiverWindow.update(rect.left, rect.top, rect.width(), rect.height());
      } else {
        touchEventReceiverWindow.setWidth(rect.width());
        touchEventReceiverWindow.setHeight(rect.height());
        touchEventReceiverWindow.showAtLocation(
            parentView, Gravity.NO_GRAVITY, rect.left, rect.top);
      }
      parentView.postInvalidate();
    }

    /**
     * Dismisses the candidate window.
     * <p>
     * Does the very similar things as {@showCandidateWindow}.
    */
    private void dismissCandidateWindow() {
      if (isCandidateWindowShowing) {
        isCandidateWindowShowing = false;
        touchEventReceiverWindow.dismiss();
        parentView.postInvalidate();
      }
    }

    @Override
    public Optional<Rect> getVisibleRect() {
      Optional<Rect> rect = layoutRenderer.getWindowRect();
      if (touchEventReceiverWindow.isShowing() && rect.isPresent()) {
        rect.get().offset(offsetX, offsetY);
        return rect;
      } else {
        return Optional.<Rect>absent();
      }
    }
  }

  private final FloatingCandidateViewProxy floatingCandidateViewProxy;

  public FloatingCandidateView(Context context) {
    super(context);
    floatingCandidateViewProxy = createFloatingCandidateViewInstance(this);
  }

  public FloatingCandidateView(Context context, AttributeSet attrs) {
    super(context, attrs);
    floatingCandidateViewProxy = createFloatingCandidateViewInstance(this);
  }

  @VisibleForTesting
  FloatingCandidateView(Context context, PopupWindow popupWindowMock) {
    super(context);
    floatingCandidateViewProxy = new FloatingCandidateViewImpl(
        this, popupWindowMock, new FloatingCandidateLayoutRenderer(context.getResources()),
        new FloatingModeIndicator(this));
  }

  @VisibleForTesting
  FloatingCandidateView(Context context, PopupWindow popupWindowMock,
                        FloatingCandidateLayoutRenderer layoutRenderer,
                        FloatingModeIndicator modeIndicator) {
    super(context);
    floatingCandidateViewProxy =
        new FloatingCandidateViewImpl(this, popupWindowMock, layoutRenderer, modeIndicator);
  }

  private static FloatingCandidateViewProxy createFloatingCandidateViewInstance(View view) {
    return isAvailable()
        ? new FloatingCandidateViewImpl(view)
        : new FloatingCandidateViewStub();
  }

  public static boolean isAvailable() {
    return Build.VERSION.SDK_INT >= 21;
  }

  @Override
  public void setVisibility(int visibility) {
    super.setVisibility(visibility);
    floatingCandidateViewProxy.setVisibility(visibility);
  }

  @Override
  public void onDraw(Canvas canvas) {
    super.onDraw(canvas);
    floatingCandidateViewProxy.draw(canvas);
  }

  @Override
  public void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
    super.onSizeChanged(width, height, oldWidth, oldHeight);
    floatingCandidateViewProxy.viewSizeChanged(width, height);
  }

  public void onStartInputView(EditorInfo editorInfo) {
    floatingCandidateViewProxy.onStartInputView(editorInfo);
  }

  /** Sets {@link CursorAnchorInfoWrapper} to update the candidate window position. */
  public void setCursorAnchorInfo(CursorAnchorInfoWrapper info) {
    floatingCandidateViewProxy.setCursorAnchorInfo(info);
  }

  /** Sets {@link Command} to update the contents of the candidate window. */
  public void setCandidates(Command outCommand) {
    floatingCandidateViewProxy.setCandidates(outCommand);
  }

  /** Sets {@link EditorInfo} for context-aware behavior. */
  public void setEditorInfo(EditorInfo editorInfo) {
    floatingCandidateViewProxy.setEditorInfo(editorInfo);
  }

  public void setCompositionMode(CompositionMode mode) {
    floatingCandidateViewProxy.setCompositionMode(mode);
  }

  /** Set view event listener to handle events invoked by the candidate window. */
  public void setViewEventListener(ViewEventListener listener) {
    floatingCandidateViewProxy.setViewEventListener(listener);
  }

  @VisibleForTesting Optional<Rect> getVisibleRect() {
    return floatingCandidateViewProxy.getVisibleRect();
  }
}
