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

package org.mozc.android.inputmethod.japanese.model;

import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.ui.FloatingModeIndicator;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.annotation.TargetApi;
import android.view.inputmethod.EditorInfo;


/**
 * State machine to manage the behavior of {@link FloatingModeIndicator} through
 * {FloatingModeIndicatorController#ControllerListener}.
 * <p>
 * This class doesn't have Looper to keep this class testable.
 */
@TargetApi(21)
public class FloatingModeIndicatorController {

  /** Listener interface to control floating mode indicator. */
  public interface ControllerListener {
    public void show(CompositionMode mode);
    public void showWithDelay(CompositionMode mode);
    public void hide();
  }

  /**
   * Shows a indicator by {@link #onStartInputView} if the method is not called in the last
   * time period.
   */
  @VisibleForTesting static final int MIN_INTERVAL_TO_SHOW_INDICATOR_ON_START_MILLIS = 3000;

  /**
   * Maximum time to wait until the cursor position is stabilized.
   * The position is not stabilized especially on start.
   */
  @VisibleForTesting static final int MAX_UNSTABLE_TIME_ON_START_MILLIS = 1000;

  /**
   * The initial value for time related variables.
   * abs(INVALID_TIME) should be greater than all other time related constants.
   */
  private static final long INVALID_TIME = -1000000;

  private final ControllerListener listener;

  /** Name of the application package that owns the editor. */
  private Optional<String> editorPackageName = Optional.absent();
  private boolean hasComposition;
  private CompositionMode compositionMode = CompositionMode.HIRAGANA;

  private long lastFocusChangedTime = INVALID_TIME;
  private long lastPositionUnstabilizedTime = INVALID_TIME;
  private boolean isCursorPositionStabilizedExplicitly;

  public FloatingModeIndicatorController(ControllerListener listener) {
    this.listener = Preconditions.checkNotNull(listener);
  }

  public void onStartInputView(long time, EditorInfo editorInfo) {
    Preconditions.checkNotNull(editorInfo);

    long interval = time - lastFocusChangedTime;
    lastFocusChangedTime = time;

    Optional<String> newEditorPackageName = Optional.fromNullable(editorInfo.packageName);
    if (!editorPackageName.equals(newEditorPackageName)
        || interval >= MIN_INTERVAL_TO_SHOW_INDICATOR_ON_START_MILLIS) {
      markCursorPositionUnstabilized(time);
      editorPackageName = newEditorPackageName;
      update(time, true);
    }
  }

  public void onCursorAnchorInfoChanged(long time) {
    if (!isCursorPositionStabilized(time)) {
      update(time, true);
    }
  }

  public void setCompositionMode(long time, CompositionMode mode) {
    Preconditions.checkNotNull(mode);
    if (compositionMode != mode) {
      // TODO(hsumita): Don't unstabilize the cursor position.
      // Note: CursorAnchorInfo may be updated soon if this method is called by focus change event.
      markCursorPositionUnstabilized(time);
      compositionMode = mode;
      update(time, true);
    }
  }

  public void setHasComposition(long time, boolean hasComposition) {
    if (this.hasComposition != hasComposition) {
      this.hasComposition = hasComposition;
      update(time, false);
    }
  }

  /**
   * Make the cursor position stabilized.
   * <p>
   * Caller class should call this method to notify that the last delayed event is invoked without
   * interruption.
   */
  public void markCursorPositionStabilized() {
    isCursorPositionStabilizedExplicitly = true;
  }

  private void markCursorPositionUnstabilized(long time) {
    isCursorPositionStabilizedExplicitly = false;
    lastPositionUnstabilizedTime = time;
  }

  /**
   * Updates the visibility of floating mode indicator.
   * <p>
   * <ul>
   * <li>Hide the indicator if there is a composition.
   * <li>Show indicator if {@code showIfNecessary} is true. If the cursor position is not
   *     stabilized, this method invokes delayed callback.
   * </ul>
   */
  private void update(long time, boolean showIfNecessary) {
    if (hasComposition) {
      listener.hide();
      return;
    }

    if (!showIfNecessary) {
      return;
    }

    if (isCursorPositionStabilized(time)) {
      listener.show(compositionMode);
    } else {
      listener.showWithDelay(compositionMode);
    }
  }

  @VisibleForTesting boolean isCursorPositionStabilized(long time) {
    return isCursorPositionStabilizedExplicitly
        || time - lastPositionUnstabilizedTime >= MAX_UNSTABLE_TIME_ON_START_MILLIS;
  }
}
