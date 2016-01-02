// Copyright 2010-2016, Google Inc.
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

import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.view.DummyDrawable;
import org.mozc.android.inputmethod.japanese.view.Skin;
import com.google.common.base.Preconditions;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.ToggleButton;

/**
 * View class for the button to expand/fold conversion candidate view.
 *
 */
public class InputFrameFoldButtonView extends ToggleButton {

  private static final int[] STATE_EMPTY = {};
  private static final int[] STATE_CHECKED = { android.R.attr.state_checked };

  private Drawable arrowDrawable = DummyDrawable.getInstance();
  private Drawable backgroundDefaultDrawable = DummyDrawable.getInstance();
  private Drawable backgroundScrolledDrawable = DummyDrawable.getInstance();
  private boolean showBackgroundForScrolled = false;

  public InputFrameFoldButtonView(Context context) {
    super(context);
  }

  public InputFrameFoldButtonView(Context context, AttributeSet attributeSet) {
    super(context, attributeSet);
  }

  public InputFrameFoldButtonView(Context context, AttributeSet attributeSet, int defStyle) {
    super(context, attributeSet, defStyle);
  }

  @Override
  protected void onFinishInflate() {
    super.onFinishInflate();
    setBackgroundColor(Color.TRANSPARENT);
  }

  @Override
  protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
    super.onSizeChanged(width, height, oldWidth, oldHeight);
    backgroundDefaultDrawable.setBounds(0, 0, width, height);
    backgroundScrolledDrawable.setBounds(0, 0, width, height);
    arrowDrawable.setBounds(0, 0, width, height);
  }

  @Override
  protected void drawableStateChanged() {
    super.drawableStateChanged();
    arrowDrawable.setState(isChecked() ? STATE_CHECKED : STATE_EMPTY);
    invalidate();
  }

  @Override
  protected void onDraw(Canvas canvas) {
    Drawable background =
        showBackgroundForScrolled ? backgroundScrolledDrawable : backgroundDefaultDrawable;
    background.draw(canvas);
    arrowDrawable.draw(canvas);
  }

  public void setSkin(Skin skin) {
    Preconditions.checkNotNull(skin);
    Resources resources = getResources();
    arrowDrawable = skin.getDrawable(resources, R.raw.keyboard__fold__tab)
        .getConstantState().newDrawable();
    arrowDrawable.setBounds(0, 0, getWidth(), getHeight());
    backgroundDefaultDrawable =
        skin.getDrawable(resources, R.raw.keyboard_fold_tab_background_default)
            .getConstantState().newDrawable();
    backgroundDefaultDrawable.setBounds(0, 0, getWidth(), getHeight());
    backgroundScrolledDrawable =
        skin.getDrawable(resources, R.raw.keyboard_fold_tab_background_scrolled)
            .getConstantState().newDrawable();
    backgroundScrolledDrawable.setBounds(0, 0, getWidth(), getHeight());
    invalidate();
  }

  public void showBackgroundForScrolled(boolean showBackgroundForScrolled) {
    if (this.showBackgroundForScrolled != showBackgroundForScrolled) {
      this.showBackgroundForScrolled = showBackgroundForScrolled;
      invalidate();
    }
  }
}
