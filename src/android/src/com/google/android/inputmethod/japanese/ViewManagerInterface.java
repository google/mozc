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

import org.mozc.android.inputmethod.japanese.JapaneseKeyboard.KeyboardSpecification;
import org.mozc.android.inputmethod.japanese.emoji.EmojiProviderType;
import org.mozc.android.inputmethod.japanese.model.JapaneseSoftwareKeyboardModel;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.InputStyle;
import org.mozc.android.inputmethod.japanese.preference.ClientSidePreference.KeyboardLayout;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.CompositionMode;
import org.mozc.android.inputmethod.japanese.view.SkinType;
import com.google.common.annotations.VisibleForTesting;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.inputmethodservice.InputMethodService;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;
import android.view.inputmethod.EditorInfo;

/**
 * Interface for ViewManager which manages Input, Candidate and Extracted views.
 *
 */
public interface ViewManagerInterface extends MemoryManageable {

  /**
   * Keyboard layout position.
   */
  public enum LayoutAdjustment {
    FILL,
    RIGHT,
    LEFT,
  }

  /**
   * Creates new input view.
   *
   * "Input view" is a software keyboard in almost all cases.
   *
   * Previously created input view is not accessed any more after calling this method.
   *
   * @param context
   * @return newly created view.
   */
  public View createMozcView(Context context);

  /**
   * Renders views which this instance own based on Command.Output.
   *
   * Note that showing/hiding views is Service's responsibility.
   */
  public void render(Command outCommand);

  /**
   * @return true if {@code event} should be consumed by Mozc client side and should be processed
   *         asynchronously.
   */
  public boolean isKeyConsumedOnViewAsynchronously(KeyEvent event);

  /**
   * Consumes and handles the given key event.
   *
   * @throws IllegalArgumentException If {@code KeyEvent} is not the key to consume.
   */
  public void consumeKeyOnViewSynchronously(KeyEvent event);

  /**
   * @return whether the view should consume the generic motion event or not.
   */
  public boolean isGenericMotionToConsume(MotionEvent event);

  /**
   * Consumes and handles the given generic motion event.
   *
   * @throws IllegalArgumentException If {@code MotionEvent} is not the key to consume.
   */
  public boolean consumeGenericMotion(MotionEvent event);

  /**
   * @return the current keyboard specification.
   */
  public KeyboardSpecification getJapaneseKeyboardSpecification();

  /**
   * Set {@code EditorInfo} instance to the current view.
   */
  public void setEditorInfo(EditorInfo attribute);

  /**
   * Set text for IME action button label.
   */
  public void setTextForActionButton(CharSequence text);

  public boolean hideSubInputView();

  /**
   * Set this keyboard layout to the specified one.
   *
   * @param keyboardLayout New keyboard layout.
   * @throws NullPointerException If {@code keyboardLayout} is {@code null}.
   */
  public void setKeyboardLayout(KeyboardLayout keyboardLayout);

  /**
   * Set the input style.
   *
   * @param inputStyle new input style.
   * @throws NullPointerException If {@code inputStyle} is {@code null}.
   * TODO(hidehiko): Refactor out following keyboard switching logic into another class.
   */
  public void setInputStyle(InputStyle inputStyle);

  public void setQwertyLayoutForAlphabet(boolean qwertyLayoutForAlphabet);

  public void setFullscreenMode(boolean fullscreenMode);

  public boolean isFullscreenMode();

  public void setFlickSensitivity(int flickSensitivity);

  public void setEmojiProviderType(EmojiProviderType emojiProviderType);

  /**
   * @param isNarrowMode Whether mozc view shows in narrow mode or normal.
   */
  public void setNarrowMode(boolean isNarrowMode);

  public boolean isNarrowMode();

  public void setPopupEnabled(boolean popupEnabled);

  public void setHardwareKeyboardCompositionMode(CompositionMode compositionMode);

  public void setSkinType(SkinType skinType);

  public void setLayoutAdjustment(Resources resources, LayoutAdjustment layoutAdjustment);

  public void setKeyboardHeightRatio(int keyboardHeightRatio);

  public void onConfigurationChanged(Configuration newConfig);

  /**
   * Reset the status of the current input view.
   */
  public void reset();

  public void computeInsets(
      Context context, InputMethodService.Insets outInsets, Window window);

  @VisibleForTesting
  public ViewEventListener getEventListener();

  @VisibleForTesting
  public JapaneseSoftwareKeyboardModel getJapaneseSoftwareKeyboardModel();

  @VisibleForTesting
  public boolean isPopupEnabled();

  @VisibleForTesting
  public int getFlickSensitivity();

  @VisibleForTesting
  public EmojiProviderType getEmojiProviderType();

  @VisibleForTesting
  public SkinType getSkinType();

  @VisibleForTesting
  public LayoutAdjustment getLayoutAdjustment();

  @VisibleForTesting
  public int getKeyboardHeightRatio();
}
