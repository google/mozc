// Copyright 2010-2014, Google Inc.
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

import org.mozc.android.inputmethod.japanese.CandidateView.ConversionCandidateWordView;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.ui.ConversionCandidateLayouter;
import com.google.common.base.Preconditions;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewGroup;
import android.widget.ToggleButton;

/**
 * A container (ViewGroup) of candidate word view and input frame fold button.
 *
 * The fold button should be laid out based on candidate word view's internal state
 * (candidate layouter), which is determined at layout flow.
 * The initial implementation sets the button's layout parameter from onLayout method
 * but setting layout parameter on layout flow (e.g. in onLayout() and onSizeChanged())
 * causes unexpected behavior (e.g. infinite loop).
 * This class is introduced to solve this issue.
 *
 * This class is a simple ViewGroup. The layout algorithm is fixed (layout .xml file doesn't affect
 * anything) so {@link #onMeasure(int, int)} is omitted.
 * First, candidate word view fills all the view.
 * Second, input frame fold button is laid out based on ConversionCandidateLayouter's state
 * (which cannot be accessed from standard view layouters. This is the reason why this class is
 * introduced).
 * The layout flow descibed above doesn't depend on {@link android.view.ViewGroup.LayoutParams}
 * we can avoid from unexpected behavior.
 */
public class ConversionCandidateWordContainerView extends ViewGroup {

  private float foldingIconSize;

  public ConversionCandidateWordContainerView(
      Context context, AttributeSet attrs, int defStyle) {
    super(context, attrs, defStyle);
  }

  public ConversionCandidateWordContainerView(Context context, AttributeSet attrs) {
    super(context, attrs);
  }

  public ConversionCandidateWordContainerView(Context context) {
    super(context);
  }

  void setCandidateTextDimension(float candidateTextSize) {
    Preconditions.checkArgument(candidateTextSize > 0);

    foldingIconSize = candidateTextSize
        + getResources().getDimension(R.dimen.candidate_vertical_padding_size) * 2;
  }

  @Override
  protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
    // Note: Don't use getMeasuredHeight/Width for this and children because #onMeasure
    //       is omitted so they return invalid value.
    ConversionCandidateWordView candidateWordView =
        ConversionCandidateWordView.class.cast(findViewById(R.id.candidate_word_view));
    candidateWordView.layout(left, top, right, bottom);
    ToggleButton inputFrameFoldButton =
        ToggleButton.class.cast(findViewById(R.id.input_frame_fold_button));
    if (inputFrameFoldButton.getVisibility() != VISIBLE) {
      return;
    }

    ConversionCandidateLayouter layouter = candidateWordView.getCandidateLayouter();
    float topMargin = (layouter.getRowHeight() - foldingIconSize) / 2;
    float rightMargin = (layouter.getChunkWidth() - foldingIconSize) / 2;
    inputFrameFoldButton.layout((int) (right - rightMargin - foldingIconSize),
                                (int) (top + topMargin),
                                (int) (right - rightMargin),
                                (int) (top + topMargin + foldingIconSize));
  }
}
