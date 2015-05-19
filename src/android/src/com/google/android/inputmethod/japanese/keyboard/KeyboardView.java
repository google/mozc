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

package org.mozc.android.inputmethod.japanese.keyboard;

import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchAction;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.view.DrawableCache;
import org.mozc.android.inputmethod.japanese.view.MozcDrawableFactory;
import org.mozc.android.inputmethod.japanese.view.SkinType;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Shader.TileMode;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Looper;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Basic implementation of a keyboard's view.
 * This class supports taps and flicks. The clients of this class can handle them via
 * {@code KeyboardActionListener}.
 *
 */
public class KeyboardView extends View {
  private final BackgroundDrawableFactory backgroundDrawableFactory =
      new BackgroundDrawableFactory(getResources().getDisplayMetrics().density);
  private final DrawableCache drawableCache =
      new DrawableCache(new MozcDrawableFactory(getResources()));
  private final PopUpPreview.Pool popupPreviewPool =
      new PopUpPreview.Pool(
          this, Looper.getMainLooper(), backgroundDrawableFactory, drawableCache);
  private final long popupDismissDelay;

  private Keyboard keyboard;
  private MetaState metaState = MetaState.UNMODIFIED;
  private final KeyboardViewBackgroundSurface backgroundSurface =
      new KeyboardViewBackgroundSurface(backgroundDrawableFactory, drawableCache);
  private boolean isKeyPressed;

  private final int keycodeSymbol;
  private final float scaledDensity;

  private int flickSensitivity;
  private boolean popupEnabled = true;
  private SkinType skinType = SkinType.ORANGE_LIGHTGRAY;

  // A map from pointerId to KeyEventContext.
  // Note: the pointerId should be small integers, e.g. 0, 1, 2... So if it turned out
  //   that the usage of Map is heavy, we probably can replace this map by a List or
  //   an array.
  // We use LinkedHashMap with accessOrder=false here, in order to ensure sending key events
  // in the pressing order in flushPendingKeyEvent.
  // Its initial capacity (16) and load factor (0.75) are just heuristics.
  private final Map<Integer, KeyEventContext> keyEventContextMap =
      new LinkedHashMap<Integer, KeyEventContext>(16, 0.75f, false);

  private KeyEventHandler keyEventHandler = null;

  // This constructor is package private for this unit test.
  public KeyboardView(Context context) {
    super(context);
  }

  public KeyboardView(Context context, AttributeSet attrSet) {
    super(context, attrSet);
  }

  public KeyboardView(Context context, AttributeSet attrs, int defStyle) {
    super(context, attrs, defStyle);
  }

  // Initializer shared by constructors.
  {
    Context context = getContext();
    Resources res = context.getResources();
    popupDismissDelay = res.getInteger(R.integer.config_popup_dismiss_delay);
    keycodeSymbol = res.getInteger(R.integer.key_symbol);
    scaledDensity = res.getDisplayMetrics().scaledDensity;
  }

  /**
   * At the moment, it is not limited to, but we expected the range of the flickSensitivity
   * is [-10,+10] inclusive. 0 is the keyboard default sensitivity.
   */
  public void setFlickSensitivity(int flickSensitivity) {
    this.flickSensitivity = flickSensitivity;
  }

  public int getFlickSensitivity() {
    return flickSensitivity;
  }

  private float getFlickSensitivityInDip() {
    // To adapt the flickSensitiy Level to actual length, we scale 1.5f heuristically.
    return - flickSensitivity * 1.5f * scaledDensity;
  }

  KeyEventContext getKeyEventContextByKey(Key key) {
    for (KeyEventContext keyEventContext : keyEventContextMap.values()) {
      if (keyEventContext != null && key == keyEventContext.key) {
        return keyEventContext;
      }
    }
    return null;
  }

  private void disposeKeyEventContext(KeyEventContext keyEventContext) {
    if (keyEventContext == null) {
      return;
    }
    if (keyEventHandler != null) {
      keyEventHandler.cancelDelayedKeyEvent(keyEventContext);
    }
    backgroundSurface.requestUpdateKey(keyEventContext.key, null);
    if (popupEnabled) {
      popupPreviewPool.releaseDelayed(keyEventContext.pointerId, popupDismissDelay);
    }
  }

  /**
   * Reset the internal state of this view.
   */
  public void resetState() {
    // To re-render the key in the normal state, notify the background surface about it.
    for (KeyEventContext keyEventContext : keyEventContextMap.values()) {
      disposeKeyEventContext(keyEventContext);
    }
    keyEventContextMap.clear();
  }

  private void flushPendingKeyEvent(TouchEvent relativeTouchEvent) {
    // Back up values and clear the map first to avoid stack overflow
    // in case this method is invoked recursively from the callback.
    // TODO(hidehiko): Refactor around keyEventHandler and keyEventContext. Also we should be
    //   able to refactor this method with resetState.
    KeyEventContext[] keyEventContextArray =
        keyEventContextMap.values().toArray(new KeyEventContext[keyEventContextMap.size()]);
    keyEventContextMap.clear();
    KeyEventHandler keyEventHandler = this.keyEventHandler;

    for (KeyEventContext keyEventContext : keyEventContextArray) {
      int keyCode = keyEventContext.getKeyCode();
      int pressedKeyCode = keyEventContext.getPressedKeyCode();
      disposeKeyEventContext(keyEventContext);
      if (keyEventHandler != null) {
        // Send relativeTouchEvent as well if exists.
        List<TouchEvent> touchEventList = relativeTouchEvent == null
            ? Collections.singletonList(keyEventContext.getTouchEvent())
            : Arrays.asList(relativeTouchEvent, keyEventContext.getTouchEvent());
        keyEventHandler.sendKey(keyCode, touchEventList);
        keyEventHandler.sendRelease(pressedKeyCode);
      }
    }
  }

  /** Set a given keyboard to this view, and send a request to update. */
  public void setKeyboard(Keyboard keyboard) {
    flushPendingKeyEvent(null);

    this.keyboard = keyboard;
    this.metaState = MetaState.UNMODIFIED;
    this.drawableCache.clear();
    backgroundSurface.requestUpdateKeyboard(keyboard, metaState);
    backgroundSurface.requestUpdateSize(keyboard.contentRight - keyboard.contentLeft,
                                        keyboard.contentBottom - keyboard.contentTop);
    invalidate();
  }

  public void setPopupEnabled(boolean popupEnabled) {
    this.popupEnabled = popupEnabled;
    if (!popupEnabled) {
      // When popup up is disabled, release all resources for popup immediately.
      popupPreviewPool.releaseAll();
    }
  }

  public boolean isPopupEnabled() {
    return popupEnabled;
  }

  @Override
  public void onDetachedFromWindow() {
    backgroundSurface.reset();
    super.onDetachedFromWindow();
  }

  /** @return the current keyboard instance */
  public Keyboard getKeyboard() {
    return keyboard;
  }

  public void setSkinType(SkinType skinType) {
    this.skinType = skinType;
    drawableCache.setSkinType(skinType);
    backgroundDrawableFactory.setSkinType(skinType);
    resetBackground();
    if (keyboard != null) {
      backgroundSurface.requestUpdateKeyboard(keyboard, metaState);
    }
  }

  private void resetBackground() {
    Drawable keyboardBackground =
        drawableCache.getDrawable(skinType.windowBackgroundResourceId);
    if (keyboardBackground == null) {
      setBackgroundColor(Color.BLACK);  // Set default background color.
    } else {
      if (keyboardBackground instanceof BitmapDrawable) {
        // If the background is bitmap resource, set repeat mode.
        BitmapDrawable.class.cast(keyboardBackground).setTileModeXY(
            TileMode.REPEAT, TileMode.REPEAT);
      }
      setBackgroundDrawable(keyboardBackground);
    }
  }

  public void setKeyEventHandler(KeyEventHandler keyEventHandler) {
    // This method needs to be invoked from a thread which the looper held by older keyEventHandler
    // points. Otherwise, there can be inconsistent state.
    KeyEventHandler oldKeyEventHandler = this.keyEventHandler;
    if (oldKeyEventHandler != null) {
      // Cancel pending key event messages sent by this view.
      for (KeyEventContext keyEventContext : keyEventContextMap.values()) {
        oldKeyEventHandler.cancelDelayedKeyEvent(keyEventContext);
      }
    }

    this.keyEventHandler = keyEventHandler;
  }

  @Override
  public void onDraw(Canvas canvas) {
    super.onDraw(canvas);

    if (keyboard == null) {
      // We have nothing to do.
      return;
    }

    backgroundSurface.update();
    backgroundSurface.draw(canvas);
  }

  private static int getPointerIndex(int action) {
    return
        (action & MotionEvent.ACTION_POINTER_INDEX_MASK) >> MotionEvent.ACTION_POINTER_INDEX_SHIFT;
  }

  private void onDown(MotionEvent event) {
    int pointerIndex = getPointerIndex(event.getAction());
    float x = event.getX(pointerIndex);
    float y = event.getY(pointerIndex);

    Key key = getKeyByCoord(x, y);
    if (key == null) {
      // Just ignore if a key isn't found.
      return;
    }

    if (getKeyEventContextByKey(key) != null) {
      // If the key is already pressed, we simply ignore event sequence related to this press.
      return;
    }

    // Create a new key event context.
    int pointerId = event.getPointerId(pointerIndex);
    float flickThreshold = Math.max(keyboard.getFlickThreshold() + getFlickSensitivityInDip(), 1);
    KeyEventContext keyEventContext = new KeyEventContext(
        key, pointerId, x, y, getWidth(), getHeight(),
        flickThreshold * flickThreshold, metaState);

    MetaState nextMetaState = keyEventContext.getNextMetaState();
    if (nextMetaState != null) {
      // This is a modifier key, so we toggle the keyboard UI at the press timing instead of
      // releasing timing. It is in order to support multi-touch with the modifier key.
      // For example, "SHIFT + a" multi-touch will produce 'A' keycode, not 'a' keycode.

      // At first, we flush all pending key event under the current (older) keyboard.
      flushPendingKeyEvent(keyEventContext.getTouchEvent());

      // And then set isKeyPressed flag to false, in order to reset keyboard to unmodified state
      // after multi-touch key events.
      // For example, we expect the metaState will be back to unmodified if a user types
      // "SHIFT + a".
      isKeyPressed = false;

      // Update the metaState and request to update the full keyboard image
      // to update all key icons.
      metaState = nextMetaState;
      backgroundSurface.requestUpdateKeyboard(keyboard, nextMetaState);
    } else {
      // Remember if a non-modifier key is pressed.
      isKeyPressed = true;

      // Request to update the image of only this key on the view.
      backgroundSurface.requestUpdateKey(key, keyEventContext.flickDirection);
    }

    if (keyEventContextMap.put(pointerId, keyEventContext) != null) {
      // keyEventContextMap contains older event.
      // TODO(hidehiko): Switch to ignoring new event, or overwriting the old event
      //   not to show unknown exceptions to users.
      throw new IllegalStateException("Conflicting keyEventContext is found: " + pointerId);
    }

    // Show popup.
    if (popupEnabled) {
      popupPreviewPool.getInstance(pointerId)
          .showIfNecessary(key, keyEventContext.getCurrentPopUp());
    }

    if (keyEventHandler != null) {
      // Clear pending key events and overwrite by this press key's one.
      for (KeyEventContext context : keyEventContextMap.values()) {
        keyEventHandler.cancelDelayedKeyEvent(context);
      }
      keyEventHandler.maybeStartDelayedKeyEvent(keyEventContext);

      // Finally we send a notification to listeners.
      keyEventHandler.sendPress(keyEventContext.getPressedKeyCode());
    }
  }

  private void onUp(MotionEvent event) {
    int pointerIndex = getPointerIndex(event.getAction());
    KeyEventContext keyEventContext = keyEventContextMap.remove(event.getPointerId(pointerIndex));
    if (keyEventContext == null) {
      // No corresponding event is found, so we have nothing to do.
      return;
    }

    float x = event.getX(pointerIndex);
    float y = event.getY(pointerIndex);
    keyEventContext.update(x, y, TouchAction.TOUCH_UP, event.getEventTime() - event.getDownTime());

    int keyCode = keyEventContext.getKeyCode();
    int pressedKeyCode = keyEventContext.getPressedKeyCode();
    disposeKeyEventContext(keyEventContext);

    if (keyEventHandler != null) {
      // In multi touch event, CursorView and SymbolInputView can't show by not primary touch event
      // because user may intend to input characters rapidly by multi touch, not change mode.
      // TODO(yoichio): Move this logic to ViewManager. "In theory" this should be done
      // in the class.
      if (keyCode != KeyEntity.INVALID_KEY_CODE &&
          (keyCode != keycodeSymbol || event.getAction() == MotionEvent.ACTION_UP)) {
        keyEventHandler.sendKey(keyCode,
            Collections.singletonList(keyEventContext.getTouchEvent()));
      }
      keyEventHandler.sendRelease(pressedKeyCode);
    }

    if (keyEventContext.getNextMetaState() != null) {
      if (isKeyPressed) {
        // A user pressed at least one key with pressing modifier key, and then the user
        // released this modifier key. So, we flush all pending events here, and
        // reset the keyboard's meta state to unmodified.
        flushPendingKeyEvent(keyEventContext.getTouchEvent());
        metaState = MetaState.UNMODIFIED;
        backgroundSurface.requestUpdateKeyboard(keyboard, MetaState.UNMODIFIED);
      }
    } else {
      if (metaState.isOneTimeMetaState && keyEventContextMap.isEmpty()) {
        // The current state is one time only, and we hit a release non-modifier key event here.
        // So, we reset the meta state to unmodified.
        metaState = MetaState.UNMODIFIED;
        backgroundSurface.requestUpdateKeyboard(keyboard, MetaState.UNMODIFIED);
      }
    }
  }

  private void onMove(MotionEvent event) {
    int pointerCount = event.getPointerCount();
    for (int i = 0; i < pointerCount; ++i) {
      KeyEventContext keyEventContext = keyEventContextMap.get(event.getPointerId(i));
      if (keyEventContext == null) {
        continue;
      }

      Key key = keyEventContext.key;
      if (keyEventContext.update(event.getX(i), event.getY(i), TouchAction.TOUCH_MOVE,
                                 event.getEventTime() - event.getDownTime())) {
        // The key's state is updated from, at least, initial state, so we'll cancel the
        // pending key events.
        if (keyEventHandler != null) {
          keyEventHandler.cancelDelayedKeyEvent(keyEventContext);
        }
        if (popupEnabled) {
          popupPreviewPool.getInstance(keyEventContext.pointerId).showIfNecessary(
              key, keyEventContext.getCurrentPopUp());
        }
      }
      backgroundSurface.requestUpdateKey(key, keyEventContext.flickDirection);
    }
  }

  // Note that event is not used but this function takes it to standardize the signature to
  // other onXXX methods defined above.
  private void onCancel(MotionEvent event) {
    resetState();
    if (keyEventHandler != null) {
      keyEventHandler.sendCancel();
    }
  }

  @Override
  public boolean onTouchEvent(MotionEvent event) {
    // Note: Once we get rid of supporting API Level 7 or lower, we can switch this
    // to event.getActionMasked().
    switch (event.getAction() & MotionEvent.ACTION_MASK) {
      case MotionEvent.ACTION_DOWN:
      case MotionEvent.ACTION_POINTER_DOWN:
        onDown(event);
        break;
      case MotionEvent.ACTION_UP:
      case MotionEvent.ACTION_POINTER_UP:
        onUp(event);
        break;
      case MotionEvent.ACTION_MOVE:
        onMove(event);
        break;
      case MotionEvent.ACTION_CANCEL:
        onCancel(event);
        break;
      default:
        // The event is not handled by this class.
        return false;
    }

    // The keyboard's state is somehow changed. Update the view.
    invalidate();
    return true;
  }

  /**
   * Finds a key containing the given coordinate.
   * @param x {@code x}-coordinate.
   * @param y {@code y}-coordinate.
   * @return A corresponding {@code Key} instance, or {@code null} if not found.
   */
  private Key getKeyByCoord(float x, float y) {
    if (y < 0 || keyboard == null || keyboard.getRowList().isEmpty()) {
      return null;
    }

    int rowBottom = 0;
    Row lastRow = keyboard.getRowList().get(keyboard.getRowList().size() - 1);
    for (Row row : keyboard.getRowList()) {
      rowBottom += row.getHeight() + row.getVerticalGap();
      Key prevKey = null;
      for (Key key : row.getKeyList()) {
        if ((// Stick vertical gaps to the keys above.
             y < key.getY() + key.getHeight() + row.getVerticalGap() ||
             // Or the key is at the bottom of the keyboard.
             // Note: Some devices sense touch events of out-side of screen.
             //   So, for better user experiences, we return the bottom row
             //   if a user touches below the screen bottom boundary.
             row == lastRow ||
             key.getY() + key.getHeight() >= keyboard.contentBottom) &&
            // Horizontal gap is included in the width,
            // so we don't need to calculate horizontal gap in addition to width.
            x < key.getX() + key.getWidth() &&
            // The following condition selects a key hit in A, C, or D
            // (C and D are on the same key), and excludes a key hit in B.
            //                +---+---+
            // current row -> | A | C |
            //                +---+   |
            // next row    -> | B | D |
            //                +---+---+
            // The condition y < rowBottom allows hits on A and C, and the other
            // condition key.getX() <= x allows hits on C and D but not B.
            // Hence, the hits on B are excluded.
            (y < rowBottom || key.getX() <= x)) {
          if (!key.isSpacer()) {
            return key;  // Found a key.
          }

          switch (key.getStick()) {
            case LEFT:
              if (prevKey != null) {
                return prevKey;
              }
              break;
            case EVEN:
              // Split the spacer evenly, assuming we don't have any consecutive spacers.
              if (x < key.getX() + key.getWidth() / 2 && prevKey != null) {
                return prevKey;
              }
              break;
            case RIGHT:
              // Do nothing to delegate the target to the next one.
          }
        }

        if (!key.isSpacer()) {
          prevKey = key;
        }
      }

      if ((y < rowBottom || row == lastRow) && prevKey != null) {
        return prevKey;
      }
    }

    return null;  // Not found.
  }
}
