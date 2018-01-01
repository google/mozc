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

package org.mozc.android.inputmethod.japanese.keyboard;

import org.mozc.android.inputmethod.japanese.MemoryManageable;
import org.mozc.android.inputmethod.japanese.accessibility.AccessibilityUtil;
import org.mozc.android.inputmethod.japanese.accessibility.KeyboardAccessibilityDelegate;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchAction;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Input.TouchEvent;
import org.mozc.android.inputmethod.japanese.resources.R;
import org.mozc.android.inputmethod.japanese.view.DrawableCache;
import org.mozc.android.inputmethod.japanese.view.Skin;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;
import com.google.common.collect.ForwardingMap;
import com.google.common.collect.Sets;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.os.Handler;
import android.os.Looper;
import android.support.v4.view.ViewCompat;
import android.text.InputType;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;

import java.util.Arrays;
import java.util.Collections;
import java.util.EnumSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Basic implementation of a keyboard's view.
 * This class supports taps and flicks. The clients of this class can handle them via
 * {@code KeyboardActionListener}.
 *
 */
public class KeyboardView extends View implements MemoryManageable {

  private final BackgroundDrawableFactory backgroundDrawableFactory =
      new BackgroundDrawableFactory(getResources());
  private final DrawableCache drawableCache = new DrawableCache(getResources());
  private final PopUpPreview.Pool popupPreviewPool =
      new PopUpPreview.Pool(
          this, Looper.getMainLooper(), backgroundDrawableFactory, getResources());
  private final long popupDismissDelay;

  private Optional<Keyboard> keyboard = Optional.absent();
  // Do not update directly. Use setMetaState instead.
  @VisibleForTesting Set<MetaState> metaState;
  @VisibleForTesting final KeyboardViewBackgroundSurface backgroundSurface =
      new KeyboardViewBackgroundSurface(backgroundDrawableFactory, drawableCache);
  @VisibleForTesting boolean isKeyPressed;

  private final float scaledDensity;

  private int flickSensitivity;
  private boolean popupEnabled = true;

  private final KeyboardAccessibilityDelegate accessibilityDelegate;

  /** Just for testing purpose. If true, HANDLING_TOUCH_EVENET metastate is removed directly. */
  @VisibleForTesting boolean enableDelayForHandlingTouchEvent = true;

  /**
   * Decorator class for {@code Map} for {@code KeyEventContextMap}.
   * <p>
   * When the number of the content is changed, meta state "HANDLING_TOUCH_EVENT"
   * is updated.
   */
  private final class KeyEventContextMap extends ForwardingMap<Integer, KeyEventContext>{

    // HANDLING_TOUCH_EVENT metastate must be kept turned on for DELAY millisecond
    // after registered KeyEventContext becomes empty.
    // Otherwise in the time between last TOUCH_UP event and the result from conversion engine
    // (though it is typically very short time) Globe key is shown, which triggers unexpected
    // IME switch.
    private static final long DELAY = 300;  // in millisecond.
    // Used for registering delayed update of HANDLING_TOUCH_EVENT.
    private final Handler delayedHandlingTouchEventHandler = new Handler(Looper.getMainLooper());
    // A Runnable to unset HANDLING_TOUCH_EVENT.
    private final Runnable metastateUnsetter = new Runnable() {
      @Override
      public void run() {
        updateMetaStates(Collections.<MetaState>emptySet(),
                         Collections.singleton(MetaState.HANDLING_TOUCH_EVENT));
      }
    };

    private final Map<Integer, KeyEventContext> delegate;

    private KeyEventContextMap(Map<Integer, KeyEventContext> delegate) {
      this.delegate = delegate;
    }

    @Override
    protected Map<Integer, KeyEventContext> delegate() {
      return delegate;
    }

    @Override
    public void clear() {
      super.clear();
      // Clearing the map corresponds to initialization so the metastate must be updated
      // immediately.
      updateHandlingTouchEventMetaState(false);
    }

    @Override
    public KeyEventContext put(Integer key, KeyEventContext value) {
      KeyEventContext result = super.put(key, value);
      updateHandlingTouchEventMetaState(true);
      return result;
    }

    @Override
    public void putAll(Map<? extends Integer, ? extends KeyEventContext> map) {
      super.putAll(map);
      updateHandlingTouchEventMetaState(true);
    }

    @Override
    public KeyEventContext remove(Object object) {
      KeyEventContext result = super.remove(object);
      updateHandlingTouchEventMetaState(true);
      return result;
    }

    /**
     * @param delayForUnset true if unsetting HANDLING_TOUCH_EVENT must be delayed.
     */
    private void updateHandlingTouchEventMetaState(boolean delayForUnset) {
      // Clear the handler to guarantee that the handler has zero or one message.
      delayedHandlingTouchEventHandler.removeCallbacks(metastateUnsetter);
      if (keyEventContextMap.isEmpty()) {
        if (delayForUnset && enableDelayForHandlingTouchEvent) {
          // After DELAY milliseconds, HANDLING_TOUCH_EVENT will be unset.
          delayedHandlingTouchEventHandler.postDelayed(metastateUnsetter, DELAY);
        } else {
          updateMetaStates(Collections.<MetaState>emptySet(),
                           Collections.singleton(MetaState.HANDLING_TOUCH_EVENT));
        }
      } else {
        // Setting HANDLING_TOUCH_EVENT must be processed immediately in order to inactivate
        // Globe key as soon as possible.
        updateMetaStates(Collections.singleton(MetaState.HANDLING_TOUCH_EVENT),
                         Collections.<MetaState>emptySet());
      }
    }
  }

  // A map from pointerId to KeyEventContext.
  // Note: the pointerId should be small integers, e.g. 0, 1, 2... So if it turned out
  //   that the usage of Map is heavy, we probably can replace this map by a List or
  //   an array.
  // We use LinkedHashMap with accessOrder=false here, in order to ensure sending key events
  // in the pressing order in flushPendingKeyEvent.
  // Its initial capacity (16) and load factor (0.75) are just heuristics.
  @VisibleForTesting public final Map<Integer, KeyEventContext> keyEventContextMap =
      new KeyEventContextMap(new LinkedHashMap<Integer, KeyEventContext>(16, 0.75f, false));

  private Optional<KeyEventHandler> keyEventHandler = Optional.absent();

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
    scaledDensity = res.getDisplayMetrics().scaledDensity;
    accessibilityDelegate = new KeyboardAccessibilityDelegate(
        this, new KeyboardAccessibilityDelegate.TouchEventEmulator() {
          @Override
          public void emulateLongPress(Key key) {
            Preconditions.checkNotNull(key);
            emulateImpl(key, true);
          }

          @Override
          public void emulateKeyInput(Key key) {
            Preconditions.checkNotNull(key);
            emulateImpl(key, false);
          }

          private void emulateImpl(Key key, boolean isLongPress) {
            KeyEventContext keyEventContext = new KeyEventContext(key, 0, 0, 0, 0, 0, 0, metaState);
            processKeyEventContextForOnDownEvent(keyEventContext);
            if (isLongPress && keyEventHandler.isPresent()) {
              keyEventHandler.get().handleMessageLongPress(keyEventContext);
            }
            processKeyEventContextForOnUpEvent(keyEventContext);
            // Without the invalidation this view cannot know that its content
            // has been updated.
            invalidateIfRequired();
          }
        });
    ViewCompat.setAccessibilityDelegate(this, accessibilityDelegate);
    // Not sure if globe is really activated.
    // However metastate requires GLOBE or NO_GLOBE state.
    setMetaStates(EnumSet.of(MetaState.NO_GLOBE));
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
    return -flickSensitivity * 1.5f * scaledDensity;
  }

  private Optional<KeyEventContext> getKeyEventContextByKey(Key key) {
    Preconditions.checkNotNull(key);
    for (KeyEventContext keyEventContext : keyEventContextMap.values()) {
      if (key == keyEventContext.key) {
        return Optional.of(keyEventContext);
      }
    }
    return Optional.absent();
  }

  private void disposeKeyEventContext(KeyEventContext keyEventContext) {
    Preconditions.checkNotNull(keyEventContext);
    if (keyEventHandler.isPresent()) {
      keyEventHandler.get().cancelDelayedKeyEvent(keyEventContext);
    }
    backgroundSurface.removePressedKey(keyEventContext.key);
    if (popupEnabled || keyEventContext.longPressCallback.isPresent()) {
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

  private void flushPendingKeyEvent(Optional<TouchEvent> relativeTouchEvent) {
    Preconditions.checkNotNull(relativeTouchEvent);

    // Back up values and clear the map first to avoid stack overflow
    // in case this method is invoked recursively from the callback.
    // TODO(hidehiko): Refactor around keyEventHandler and keyEventContext. Also we should be
    //   able to refactor this method with resetState.
    KeyEventContext[] keyEventContextArray =
        keyEventContextMap.values().toArray(new KeyEventContext[keyEventContextMap.size()]);
    keyEventContextMap.clear();

    for (KeyEventContext keyEventContext : keyEventContextArray) {
      int keyCode = keyEventContext.getKeyCode();
      int pressedKeyCode = keyEventContext.getPressedKeyCode();
      disposeKeyEventContext(keyEventContext);
      if (keyEventHandler.isPresent()) {
        // Send relativeTouchEvent as well if exists.
        // TODO(hsumita): Confirm that we can put null on touchEventList or not.
        List<TouchEvent> touchEventList = relativeTouchEvent.isPresent()
            ? Arrays.asList(relativeTouchEvent.get(), keyEventContext.getTouchEvent().orNull())
            : Collections.singletonList(keyEventContext.getTouchEvent().orNull());
        keyEventHandler.get().sendKey(keyCode, touchEventList);
        keyEventHandler.get().sendRelease(pressedKeyCode);
      }
    }
  }

  /** Set a given keyboard to this view, and send a request to update. */
  public void setKeyboard(Keyboard keyboard) {
    flushPendingKeyEvent(Optional.<TouchEvent>absent());

    this.keyboard = Optional.of(keyboard);
    updateMetaStates(Collections.<MetaState>emptySet(), MetaState.CHAR_TYPE_EXCLUSIVE_GROUP);
    accessibilityDelegate.setKeyboard(this.keyboard);
    this.drawableCache.clear();
    backgroundSurface.reset(this.keyboard, Collections.<MetaState>emptySet());
    invalidateIfRequired();
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

  /** @return the current keyboard instance */
  public Optional<Keyboard> getKeyboard() {
    return keyboard;
  }

  @SuppressWarnings("deprecation")
  public void setSkin(Skin skin) {
    Preconditions.checkNotNull(skin);
    drawableCache.setSkin(skin);
    popupPreviewPool.setSkin(skin);
    backgroundDrawableFactory.setSkin(skin);
    if (keyboard.isPresent()) {
      backgroundSurface.reset(this.keyboard, Collections.<MetaState>emptySet());
    }
    setBackgroundDrawable(skin.windowBackgroundDrawable.getConstantState().newDrawable());
  }

  public void setKeyEventHandler(KeyEventHandler keyEventHandler) {
    // This method needs to be invoked from a thread which the looper held by older keyEventHandler
    // points. Otherwise, there can be inconsistent state.
    Optional<KeyEventHandler> oldKeyEventHandler = this.keyEventHandler;
    if (oldKeyEventHandler.isPresent()) {
      // Cancel pending key event messages sent by this view.
      for (KeyEventContext keyEventContext : keyEventContextMap.values()) {
        oldKeyEventHandler.get().cancelDelayedKeyEvent(keyEventContext);
      }
    }
    this.keyEventHandler = Optional.of(keyEventHandler);
  }

  @Override
  public void onDraw(Canvas canvas) {
    super.onDraw(canvas);

    if (!keyboard.isPresent()) {
      // We have nothing to do.
      return;
    }
    // Draw keyboard.
    backgroundSurface.draw(canvas);
  }

  @SuppressLint("InlinedApi")
  private static int getPointerIndex(int action) {
    return
        (action & MotionEvent.ACTION_POINTER_INDEX_MASK) >> MotionEvent.ACTION_POINTER_INDEX_SHIFT;
  }

  private void onDown(MotionEvent event) {
    Preconditions.checkState(keyboard.isPresent());

    int pointerIndex = getPointerIndex(event.getAction());
    float x = event.getX(pointerIndex);
    float y = event.getY(pointerIndex);

    Optional<Key> optionalKey = getKeyByCoord(x, y);
    if (!optionalKey.isPresent()) {
      // Just ignore if a key isn't found.
      return;
    }

    if (getKeyEventContextByKey(optionalKey.get()).isPresent()) {
      // If the key is already pressed, we simply ignore event sequence related to this press.
      return;
    }

    // Create a new key event context.
    int pointerId = event.getPointerId(pointerIndex);
    float flickThreshold =
        Math.max(keyboard.get().getFlickThreshold() + getFlickSensitivityInDip(), 1);
    final KeyEventContext keyEventContext = new KeyEventContext(
        optionalKey.get(), pointerId, x, y, getWidth(), getHeight(),
        flickThreshold * flickThreshold, metaState);

    // Show popup.
    updatePopUp(keyEventContext, false);
    Optional<KeyEntity> keyEntity =
        KeyEventContext.getKeyEntity(keyEventContext.key, metaState,
                                     Optional.of(Flick.Direction.CENTER));
    if (keyEntity.isPresent() && keyEntity.get().getPopUp().isPresent()
        && !keyEntity.get().isLongPressTimeoutTrigger()) {
      keyEventContext.setLongPressCallback(new Runnable() {
          @Override
          public void run() {
            updatePopUp(keyEventContext, true);
          }
        });
    }

    // Process the KeyEventContext (e.g., sending messages to KeyEventHandler, updating the surface,
    // flushing pending key events and so on)
    processKeyEventContextForOnDownEvent(keyEventContext);
    // keyEventContextMap contains older event.
    // TODO(hidehiko): Switch to ignoring new event, or overwriting the old event
    //   not to show unknown exceptions to users.
    Preconditions.checkState(
        keyEventContextMap.put(pointerId, keyEventContext) == null,
        "Conflicting keyEventContext is found: " + pointerId);
  }

  private void processKeyEventContextForOnDownEvent(
      final KeyEventContext keyEventContext) {
    Set<MetaState> nextMetaStates = keyEventContext.getNextMetaStates(metaState);

    if (!nextMetaStates.equals(metaState)) {
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
      setMetaStates(nextMetaStates);
      backgroundSurface.reset(keyboard, nextMetaStates);
    } else {
      // Remember if a non-modifier key is pressed.
      isKeyPressed = true;

      // Add the key as pressed one into the renderer.
      backgroundSurface.addPressedKey(keyEventContext.key, keyEventContext.flickDirection);
    }
    if (keyEventHandler.isPresent()) {
      // Clear pending key events and overwrite by this press key's one.
      for (KeyEventContext context : keyEventContextMap.values()) {
        keyEventHandler.get().cancelDelayedKeyEvent(context);
      }
      keyEventHandler.get().maybeStartDelayedKeyEvent(keyEventContext);
      // Finally we send a notification to listeners.
      keyEventHandler.get().sendPress(keyEventContext.getPressedKeyCode());
    }
  }

  private void onUp(MotionEvent event) {
    Preconditions.checkState(keyboard.isPresent());

    int pointerIndex = getPointerIndex(event.getAction());
    KeyEventContext keyEventContext = keyEventContextMap.remove(event.getPointerId(pointerIndex));
    if (keyEventContext == null) {
      // No corresponding event is found, so we have nothing to do.
      return;
    }

    float x = event.getX(pointerIndex);
    float y = event.getY(pointerIndex);
    keyEventContext.update(x, y, TouchAction.TOUCH_UP, event.getEventTime() - event.getDownTime());

    processKeyEventContextForOnUpEvent(keyEventContext);
  }

  private void processKeyEventContextForOnUpEvent(KeyEventContext keyEventContext) {
    disposeKeyEventContext(keyEventContext);

    int keyCode = keyEventContext.getKeyCode();
    int pressedKeyCode = keyEventContext.getPressedKeyCode();

    if (keyEventHandler.isPresent()) {
      if (keyCode != KeyEntity.INVALID_KEY_CODE) {
        // TODO(hsumita): Confirm that we can put null as a touch event or not.
        keyEventHandler.get().sendKey(keyCode,
            Collections.singletonList(keyEventContext.getTouchEvent().orNull()));
      }
      keyEventHandler.get().sendRelease(pressedKeyCode);
    }

    if (keyEventContext.isMetaStateToggleEvent()) {
      if (isKeyPressed) {
        // A user pressed at least one key with pressing modifier key, and then the user
        // released this modifier key. So, we flush all pressed keys here, and
        // reset the keyboard's meta state to unmodified.
        flushPendingKeyEvent(keyEventContext.getTouchEvent());
        updateMetaStates(Collections.<MetaState>emptySet(), MetaState.CHAR_TYPE_EXCLUSIVE_GROUP);
        backgroundSurface.reset(keyboard, Collections.<MetaState>emptySet());
      }
    } else {
      if (!metaState.isEmpty() && keyEventContextMap.isEmpty()) {
        Set<MetaState> nextMetaState = Sets.newEnumSet(metaState, MetaState.class);
        for (MetaState state : metaState) {
          if (state.isOneTimeMetaState) {
            // The current state is one time only, and we hit a release non-modifier key event here.
            // So, we reset the meta state to unmodified.
            nextMetaState.remove(state);
          }
        }
        if (!nextMetaState.equals(metaState)) {
          setMetaStates(nextMetaState);
          backgroundSurface.reset(keyboard, Collections.<MetaState>emptySet());
        }
      }
    }
    backgroundSurface.removePressedKey(keyEventContext.key);
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
        if (keyEventHandler.isPresent()) {
          // The key's state is updated from, at least, initial state, so we'll cancel the
          // pending key events, and invoke new pending key events if necessary.
          keyEventHandler.get().cancelDelayedKeyEvent(keyEventContext);
          if (keyEventContext.flickDirection == Flick.Direction.CENTER) {
            keyEventHandler.get().maybeStartDelayedKeyEvent(keyEventContext);
          }
        }
        updatePopUp(keyEventContext, false);
      }
      backgroundSurface.addPressedKey(key, keyEventContext.flickDirection);
    }
  }

  // Note that event is not used but this function takes it to standardize the signature to
  // other onXXX methods defined above.
  private void onCancel(@SuppressWarnings("unused") MotionEvent event) {
    resetState();
    if (keyEventHandler.isPresent()) {
      keyEventHandler.get().sendCancel();
    }
  }

  @Override
  public boolean onTouchEvent(MotionEvent event) {
    switch (event.getActionMasked()) {
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

    // The keyboard's state might be changed. Update the view if required.
    invalidateIfRequired();
    return true;
  }

  /**
   * Finds a key containing the given coordinate.
   * @param x {@code x}-coordinate.
   * @param y {@code y}-coordinate.
   * @return A corresponding {@code Key} instance, or {@code Optional.<Key>absent()} if not found.
   */
  @VisibleForTesting Optional<Key> getKeyByCoord(float x, float y) {
    if (y < 0 || !keyboard.isPresent() || keyboard.get().getRowList().isEmpty()) {
      return Optional.absent();
    }

    List<Row> rowList = keyboard.get().getRowList();
    int rowBottom = 0;
    Row lastRow = rowList.get(rowList.size() - 1);
    for (Row row : rowList) {
      rowBottom += row.getHeight() + row.getVerticalGap();
      Key prevKey = null;
      for (Key key : row.getKeyList()) {
        if ((// Stick vertical gaps to the keys above.
             y < key.getY() + key.getHeight() + row.getVerticalGap()
             // Or the key is at the bottom of the keyboard.
             // Note: Some devices sense touch events of out-side of screen.
             //   So, for better user experiences, we return the bottom row
             //   if a user touches below the screen bottom boundary.
             || row == lastRow
             || key.getY() + key.getHeight() >= keyboard.get().contentBottom)
            // Horizontal gap is included in the width,
            // so we don't need to calculate horizontal gap in addition to width.
            && x < key.getX() + key.getWidth()
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
            && (y < rowBottom || key.getX() <= x)) {
          if (!key.isSpacer()) {
            return Optional.of(key);  // Found a key.
          }

          switch (key.getStick()) {
            case LEFT:
              if (prevKey != null) {
                return Optional.of(prevKey);
              }
              break;
            case EVEN:
              // Split the spacer evenly, assuming we don't have any consecutive spacers.
              if (x < key.getX() + key.getWidth() / 2 && prevKey != null) {
                return Optional.of(prevKey);
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
        return Optional.of(prevKey);
      }
    }

    return Optional.absent();  // Not found.
  }

  @Override
  public void trimMemory() {
    drawableCache.clear();
    popupPreviewPool.releaseAll();
  }

  @Override
  public boolean dispatchTouchEvent(MotionEvent event) {
    Preconditions.checkNotNull(event);
    if (AccessibilityUtil.isTouchExplorationEnabled(getContext())) {
      return accessibilityDelegate.dispatchTouchEvent(event);
    }
    return super.dispatchTouchEvent(event);
  }

  @Override
  public boolean dispatchHoverEvent(MotionEvent event) {
    Preconditions.checkNotNull(event);
    if (AccessibilityUtil.isTouchExplorationEnabled(getContext())) {
      return accessibilityDelegate.dispatchHoverEvent(event);
    }
    return false;
  }

  @VisibleForTesting
  public Set<MetaState> getMetaStates() {
    return this.metaState;
  }

  private void setMetaStates(Set<MetaState> metaState) {
    Preconditions.checkNotNull(metaState);
    Preconditions.checkArgument(MetaState.isValidSet(metaState));
    this.metaState = metaState;
    accessibilityDelegate.setMetaState(metaState);
    backgroundSurface.setMetaStates(metaState);
  }

  public void updateMetaStates(Set<MetaState> addedMetaStates, Set<MetaState> removedMetaStates) {
    Preconditions.checkNotNull(addedMetaStates);
    Preconditions.checkNotNull(removedMetaStates);

    setMetaStates(Sets.union(Sets.difference(metaState, removedMetaStates),
                             addedMetaStates).immutableCopy());
    invalidateIfRequired();
  }

  public void setPasswordField(boolean isPasswordField) {
    accessibilityDelegate.setPasswordField(isPasswordField);
  }

  public void setEditorInfo(EditorInfo editorInfo) {
    Preconditions.checkNotNull(editorInfo);

    Set<MetaState> metaStates = EnumSet.noneOf(MetaState.class);
    // If IME_FLAG_NO_ENTER_ACTION is set, normal action icon should be shown.
    if ((editorInfo.imeOptions & EditorInfo.IME_FLAG_NO_ENTER_ACTION) == 0) {
      switch (editorInfo.imeOptions & EditorInfo.IME_MASK_ACTION) {
        case EditorInfo.IME_ACTION_DONE:
          metaStates.add(MetaState.ACTION_DONE);
          break;
        case EditorInfo.IME_ACTION_GO:
          metaStates.add(MetaState.ACTION_GO);
          break;
        case EditorInfo.IME_ACTION_NEXT:
          metaStates.add(MetaState.ACTION_NEXT);
          break;
        case EditorInfo.IME_ACTION_NONE:
          metaStates.add(MetaState.ACTION_NONE);
          break;
        case EditorInfo.IME_ACTION_PREVIOUS:
          metaStates.add(MetaState.ACTION_PREVIOUS);
          break;
        case EditorInfo.IME_ACTION_SEARCH:
          metaStates.add(MetaState.ACTION_SEARCH);
          break;
        case EditorInfo.IME_ACTION_SEND:
          metaStates.add(MetaState.ACTION_SEND);
          break;
        default:
          // Do nothing
      }
    }

    // InputType variation is *NOT* bit-fields in fact.
    int clazz = editorInfo.inputType & InputType.TYPE_MASK_CLASS;
    int variation = editorInfo.inputType & InputType.TYPE_MASK_VARIATION;
    switch (clazz) {
      case InputType.TYPE_CLASS_TEXT:
        switch (variation) {
          case InputType.TYPE_TEXT_VARIATION_URI:
            metaStates.add(MetaState.VARIATION_URI);
            break;
          case InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS:
          case InputType.TYPE_TEXT_VARIATION_WEB_EMAIL_ADDRESS:
            metaStates.add(MetaState.VARIATION_EMAIL_ADDRESS);
            break;
          default:
            // Do nothing
        }
        break;
      default:
        // Do nothing
    }

    updateMetaStates(metaStates, Sets.union(MetaState.ACTION_EXCLUSIVE_GROUP,
                                            MetaState.VARIATION_EXCLUSIVE_GROUP));
  }

  public void setGlobeButtonEnabled(boolean isGlobeButtonEnabled) {
    if (isGlobeButtonEnabled) {
      updateMetaStates(EnumSet.of(MetaState.GLOBE), EnumSet.of(MetaState.NO_GLOBE));
    } else {
      updateMetaStates(EnumSet.of(MetaState.NO_GLOBE), EnumSet.of(MetaState.GLOBE));
    }
  }

  private void updatePopUp(KeyEventContext keyEventContext, boolean isDelayedPopUp) {
    PopUpPreview popUpPreview = popupPreviewPool.getInstance(keyEventContext.pointerId);
    // Even if popup is disabled by preference, delayed popup (== popup for long-press)
    // is shown otherwise a user cannot know how long (s)he has to press the key
    // to get a character corresponding to long-press.
    if (popupEnabled || isDelayedPopUp) {
      popUpPreview.showIfNecessary(
          keyEventContext.key, keyEventContext.getCurrentPopUp(), isDelayedPopUp);
    } else {
      popUpPreview.dismiss();
    }
  }

  private void invalidateIfRequired() {
    if (backgroundSurface.isDirty()) {
      invalidate();
    }
  }
}
