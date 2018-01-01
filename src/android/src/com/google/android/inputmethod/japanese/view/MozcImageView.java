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

import com.google.common.base.Preconditions;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.ImageView;

/**
 * A ImageView which accepts mozc drawable as src.
 * <p>
 * In addition to ImageView, following XML attributes are supported:
 * <ul>
 * <li>maxImageWidth(dimension): If set, additional padding (left and right) are automatically
 * set in order to make drawn image thinner than given width.
 * {@code setPadding} and {@code setPaddingRelative} are disabled
 *  (UnsupportedOperationException will be thrown}.
 * <li>maxImageHeight(dimension): Same as maxImageWidth.
 * </ul>
 * <p>
 * This class is not intended to be changed its padding.
 * There was a warning code for it but on old Android framework setPadding() is always called
 * so the warning code has been removed.
 */
public class MozcImageView extends ImageView implements MozcImageCapableView {

  private final MozcImageCapableViewDelegate delegate = new MozcImageCapableViewDelegate(this);

  public MozcImageView(Context context, AttributeSet attrs, int defStyle) {
    super(context, attrs, defStyle);
    Preconditions.checkArgument(
        MozcImageCapableViewDelegate.assertSrcAttribute(getResources(), attrs));
    delegate.loadMozcImageSource(attrs);
  }

  public MozcImageView(Context context, AttributeSet attrs) {
    super(context, attrs);
    Preconditions.checkArgument(
        MozcImageCapableViewDelegate.assertSrcAttribute(getResources(), attrs));
    delegate.loadMozcImageSource(attrs);
  }

  public MozcImageView(Context context) {
    super(context);
  }

  public void setRawId(int rawId) {
    delegate.setRawId(rawId);
  }

  public void setSkin(Skin skin) {
    delegate.setSkin(skin);
  }

  @Override
  protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
    super.onSizeChanged(width, height, oldWidth, oldHeight);
    delegate.updateAdditionalPadding();
  }

  @Override
  public void setMaxImageWidth(int maxImageWidth) {
    delegate.setMaxImageWidth(maxImageWidth);
  }

  @Override
  public void setMaxImageHeight(int maxImageHeight) {
    delegate.setMaxImageHeight(maxImageHeight);
  }

  @Override
  public ImageView asImageView() {
    return this;
  }
}
