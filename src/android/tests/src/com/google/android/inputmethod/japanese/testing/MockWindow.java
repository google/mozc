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

package org.mozc.android.inputmethod.japanese.testing;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.SurfaceHolder.Callback2;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;

/**
 * @see MockContext
 */
public class MockWindow extends Window {

  public MockWindow(Context context) {
    super(context);
  }

  @Override
  public void addContentView(View view, LayoutParams params) {
  }

  @Override
  public void closeAllPanels() {
  }

  @Override
  public void closePanel(int featureId) {
  }

  @Override
  public View getCurrentFocus() {
    return null;
  }

  @Override
  public View getDecorView() {
    return null;
  }

  @Override
  public LayoutInflater getLayoutInflater() {
    return null;
  }

  @Override
  public int getVolumeControlStream() {
    return 0;
  }

  @Override
  public void invalidatePanelMenu(int featureId) {
  }

  @Override
  public boolean isFloating() {
    return false;
  }

  @Override
  public boolean isShortcutKey(int keyCode, KeyEvent event) {
    return false;
  }

  @Override
  protected void onActive() {
  }

  @Override
  public void onConfigurationChanged(Configuration newConfig) {
  }

  @Override
  public void openPanel(int featureId, KeyEvent event) {
  }

  @Override
  public View peekDecorView() {
    return null;
  }

  @Override
  public boolean performContextMenuIdentifierAction(int id, int flags) {
    return false;
  }

  @Override
  public boolean performPanelIdentifierAction(int featureId, int id, int flags) {
    return false;
  }

  @Override
  public boolean performPanelShortcut(int featureId, int keyCode, KeyEvent event, int flags) {
    return false;
  }

  @Override
  public void restoreHierarchyState(Bundle savedInstanceState) {
  }

  @Override
  public Bundle saveHierarchyState() {
    return null;
  }

  @Override
  public void setBackgroundDrawable(Drawable drawable) {
  }

  @Override
  public void setChildDrawable(int featureId, Drawable drawable) {
  }

  @Override
  public void setChildInt(int featureId, int value) {
  }

  @Override
  public void setContentView(int layoutResID) {
  }

  @Override
  public void setContentView(View view) {
  }

  @Override
  public void setContentView(View view, LayoutParams params) {
  }

  @Override
  public void setFeatureDrawable(int featureId, Drawable drawable) {
  }

  @Override
  public void setFeatureDrawableAlpha(int featureId, int alpha) {
  }

  @Override
  public void setFeatureDrawableResource(int featureId, int resId) {
  }

  @Override
  public void setFeatureDrawableUri(int featureId, Uri uri) {
  }

  @Override
  public void setFeatureInt(int featureId, int value) {
  }

  @Override
  public void setTitle(CharSequence title) {
  }

  @Deprecated
  @Override
  public void setTitleColor(int textColor) {
  }

  @Override
  public void setVolumeControlStream(int streamType) {
  }

  @Override
  public boolean superDispatchGenericMotionEvent(MotionEvent event) {
    return false;
  }

  @Override
  public boolean superDispatchKeyEvent(KeyEvent event) {
    return false;
  }

  @Override
  public boolean superDispatchKeyShortcutEvent(KeyEvent event) {
    return false;
  }

  @Override
  public boolean superDispatchTouchEvent(MotionEvent event) {
    return false;
  }

  @Override
  public boolean superDispatchTrackballEvent(MotionEvent event) {
    return false;
  }

  @Override
  public void takeInputQueue(android.view.InputQueue.Callback callback) {
  }

  @Override
  public void takeKeyEvents(boolean get) {
  }

  @Override
  public void takeSurface(Callback2 callback) {
  }

  @Override
  public void togglePanel(int featureId, KeyEvent event) {
  }

  // hidden public abstract method.
  public void alwaysReadCloseOnTouchAttr() {
  }

  @Override
  public int getStatusBarColor() {
    return 0;
  }

  @Override
  public void setStatusBarColor(int color) {
  }

  @Override
  public int getNavigationBarColor() {
    return 0;
  }

  @Override
  public void setNavigationBarColor(int color) {
  }
}
