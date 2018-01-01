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

package org.mozc.android.inputmethod.japanese.accessibility;

import org.mozc.android.inputmethod.japanese.MozcLog;
import org.mozc.android.inputmethod.japanese.keyboard.Flick.Direction;
import org.mozc.android.inputmethod.japanese.keyboard.Key;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState;
import org.mozc.android.inputmethod.japanese.keyboard.KeyState.MetaState;
import org.mozc.android.inputmethod.japanese.keyboard.Keyboard;
import org.mozc.android.inputmethod.japanese.keyboard.Row;
import org.mozc.android.inputmethod.japanese.resources.R;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.content.Context;
import android.graphics.Rect;
import android.os.Bundle;
import android.support.v4.view.ViewCompat;
import android.support.v4.view.accessibility.AccessibilityEventCompat;
import android.support.v4.view.accessibility.AccessibilityNodeInfoCompat;
import android.support.v4.view.accessibility.AccessibilityNodeProviderCompat;
import android.support.v4.view.accessibility.AccessibilityRecordCompat;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Set;

import javax.annotation.Nullable;

/**
 * Represents keyboard's virtual structure.
 *
 * <p>Note about virtual view ID: This class uses {@code Key}'s {@code sourceId}
 * as virtual view ID. It is changed by metastate of the keyboard.
 */
class KeyboardAccessibilityNodeProvider extends AccessibilityNodeProviderCompat {

  @VisibleForTesting static final int UNDEFINED = Integer.MIN_VALUE;
  @VisibleForTesting static final int PASSWORD_RESOURCE_ID =
      R.string.cd_key_uchar_katakana_middle_dot;

  // View for keyboard.
  private final View view;
  // Keyboard model.
  private Optional<Keyboard> keyboard = Optional.absent();
  // Keys in the keyboard.
  // Don't access directly. Use #getKeys() instead for lazy creation.
  private Optional<Collection<Key>> keys = Optional.absent();
  // Virtual ID of focused (in the light of accessibility) view.
  private int virtualFocusedViewId = UNDEFINED;
  private Set<MetaState> metaState = Collections.emptySet();
  // If true content description of printable characters are not spoken.
  // For password field.
  private boolean shouldObscureInput = false;

  KeyboardAccessibilityNodeProvider(View view) {
    this.view = Preconditions.checkNotNull(view);
  }

  private Context getContext() {
    return view.getContext();
  }

  /**
   * Returns all the keys in the {@code keyboard}.
   *
   * <p>Lazy creation is done inside.
   * <p>If {@code keyboard} is not set, empty collection is returned.
   */
  private Collection<Key> getKeys() {
    if (keys.isPresent()) {
      return keys.get();
    }
    if (!keyboard.isPresent()) {
      return Collections.emptyList();
    }
    // Initial size is estimated roughly.
    Collection<Key> tempKeys = new ArrayList<Key>(keyboard.get().getRowList().size() * 10);
    for (Row row : keyboard.get().getRowList()) {
      for (Key key : row.getKeyList()) {
        if (!key.isSpacer()) {
          tempKeys.add(key);
        }
      }
    }
    keys = Optional.of(tempKeys);
    return tempKeys;
  }

  /**
   * Returns a {@code Key} based on given position.
   *
   * @param x horizontal location in screen coordinate (pixel)
   * @param y vertical location in screen coordinate (pixel)
   */
  Optional<Key> getKey(int x, int y) {
    for (Key key : getKeys()) {
      int left = key.getX();
      if (left > x) {
        continue;
      }
      int right = left + key.getWidth();
      if (right <= x) {
        continue;
      }
      int top = key.getY();
      if (top > y) {
        continue;
      }
      int bottom = top + key.getHeight();
      if (bottom <= y) {
        continue;
      }
      return Optional.of(key);
    }
    return Optional.absent();
  }


  @Override
  @Nullable
  public AccessibilityNodeInfoCompat createAccessibilityNodeInfo(int virtualViewId) {
    if (virtualViewId == UNDEFINED) {
      return null;
    }
    if (virtualViewId == View.NO_ID) {
      // Required to return the information about keyboardView.
      AccessibilityNodeInfoCompat info =
          AccessibilityNodeInfoCompat.obtain(view);
      if (info == null) {
        // In old Android OS AccessibilityNodeInfoCompat.obtain() returns null.
        return null;
      }
      Preconditions.checkNotNull(info);
      ViewCompat.onInitializeAccessibilityNodeInfo(view, info);
      // Add the virtual children of the root View.
      for (Key key : getKeys()) {
        // NOTE: sourceID is always non-negative
        // so it can be added to AccessibilityNodeInfoCompat safely.
        info.addChild(view, getSourceId(key));
      }
      return info;
    }
    // Required to return the information about child view (== key).
    // Find the view that corresponds to the given id.
    Optional<Key> optionalKey = getKeyFromSouceId(virtualViewId);
    if (!optionalKey.isPresent()) {
      MozcLog.e("Virtual view id " + virtualViewId + " is not found");
      return null;
    }
    Key key = optionalKey.get();
    Rect boundsInParent =
        new Rect(key.getX(), key.getY(),
                 key.getX() + key.getWidth(), key.getY() + key.getHeight());
    int[] parentLocationOnScreen = new int[2];
    view.getLocationOnScreen(parentLocationOnScreen);
    Rect boundsInScreen = new Rect(boundsInParent);
    boundsInScreen.offset(parentLocationOnScreen[0], parentLocationOnScreen[1]);

    AccessibilityNodeInfoCompat info = AccessibilityNodeInfoCompat.obtain();
    if (info == null) {
      // In old Android OS AccessibilityNodeInfoCompat.obtain() returns null.
      return null;
    }
    info.setPackageName(getContext().getPackageName());
    info.setClassName(key.getClass().getName());
    info.setContentDescription(getContentDescription(key).orNull());
    info.setBoundsInParent(boundsInParent);
    info.setBoundsInScreen(boundsInScreen);
    info.setParent(view);
    info.setSource(view, virtualViewId);
    info.setEnabled(true);
    info.setVisibleToUser(true);

    if (virtualFocusedViewId == virtualViewId) {
      info.addAction(AccessibilityNodeInfoCompat.ACTION_CLEAR_ACCESSIBILITY_FOCUS);
    } else {
      info.addAction(AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS);
    }
    return info;
  }

  private static KeyState getKeyState(Key key, Set<MetaState> metaState) {
    Preconditions.checkArgument(!key.isSpacer());
    return key.getKeyState(metaState).get();
  }

  /**
   * Returns source id of the given {@code key} unmodified-center
   */
  private int getSourceId(Key key) {
    Preconditions.checkNotNull(key);
    if (key.isSpacer()) {
      return UNDEFINED;
    }
    return getKeyState(key, metaState).getFlick(
        Direction.CENTER).get().getKeyEntity().getSourceId();
  }

  private Optional<Integer> getKeyCode(Key key) {
    Preconditions.checkNotNull(key);
    if (key.isSpacer()) {
      return Optional.absent();
    }
    return Optional.of(getKeyState(key, metaState).getFlick(
        Direction.CENTER).get().getKeyEntity().getKeyCode());
  }

  /**
   * Returns {@code Key} from source Id.
   */
  private Optional<Key> getKeyFromSouceId(int sourceId) {
    if (!keyboard.isPresent()) {
      return Optional.absent();
    }
    for (Row row : keyboard.get().getRowList()) {
      for (Key key : row.getKeyList()) {
        if (sourceId == getSourceId(key)) {
          return Optional.of(key);
        }
      }
    }
    return Optional.absent();
  }

  /**
   * Creates a {@code AccessibilityEvent} from {@code Key} and {@code eventType}.
   */
  private AccessibilityEvent createAccessibilityEvent(Key key, int eventType) {
    Preconditions.checkNotNull(key);

    AccessibilityEvent event = AccessibilityEvent.obtain(eventType);
    event.setPackageName(getContext().getPackageName());
    event.setClassName(key.getClass().getName());
    event.setContentDescription(getContentDescription(key).orNull());
    event.setEnabled(true);
    AccessibilityRecordCompat record = AccessibilityEventCompat.asRecord(event);
    record.setSource(view, getSourceId(key));
    return event;
  }

  @Override
  public boolean performAction(int virtualViewId, int action, Bundle arguments) {
    Optional<Key> key = getKeyFromSouceId(virtualViewId);
    return key.isPresent()
        ? performActionForKeyInternal(key.get(), virtualViewId, action)
        : false;
  }

  boolean performActionForKey(Key key, int action) {
    return performActionForKeyInternal(key, getSourceId(Preconditions.checkNotNull(key)), action);
  }

  /**
   * Processes accessibility action for key on virtual view structure.
   */
  boolean performActionForKeyInternal(Key key, int virtualViewId, int action) {
    switch (action) {
      case AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS:
        if (virtualFocusedViewId == virtualViewId) {
          // If focused virtual view is unchanged, do nothing.
          return false;
        }
        // Framework requires the keyboard to have focus.
        // Return FOCUSED event to the framework as response.
        virtualFocusedViewId = virtualViewId;
        if (isAccessibilityEnabled()) {
          AccessibilityUtil.sendAccessibilityEvent(
              getContext(),
              createAccessibilityEvent(
                  key,
                  AccessibilityEventCompat.TYPE_VIEW_ACCESSIBILITY_FOCUSED));
        }
        return true;
      case AccessibilityNodeInfoCompat.ACTION_CLEAR_ACCESSIBILITY_FOCUS:
        // Framework requires the keyboard to clear focus.
        // Return FOCUSE_CLEARED event to the framework as response.
        if (virtualFocusedViewId != virtualViewId) {
          return false;
        }
        virtualFocusedViewId = UNDEFINED;
        if (isAccessibilityEnabled()) {
          AccessibilityUtil.sendAccessibilityEvent(
              getContext(),
              createAccessibilityEvent(
                  key,
                  AccessibilityEventCompat.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED));
        }
        return true;
      default:
        return false;
    }
  }

  /**
   * @return {@code true} if {@link android.view.accessibility.AccessibilityManager} is enabled
   */
  private boolean isAccessibilityEnabled() {
    return AccessibilityUtil.isAccessibilityEnabled(getContext());
  }

  void sendAccessibilityEventForKeyIfAccessibilityEnabled(Key key, int eventType) {
    if (isAccessibilityEnabled()) {
      AccessibilityEvent event = createAccessibilityEvent(key, eventType);
      AccessibilityUtil.sendAccessibilityEvent(getContext(), event);
    }
  }

  void setKeyboard(Optional<Keyboard> keyboard) {
    this.keyboard = Preconditions.checkNotNull(keyboard);
    resetVirtualStructure();
  }

  void setMetaState(Set<MetaState> metaState) {
    this.metaState = Preconditions.checkNotNull(metaState);
    resetVirtualStructure();
  }

  void setObscureInput(boolean shouldObscureInput) {
    this.shouldObscureInput = shouldObscureInput;
    resetVirtualStructure();
  }

  private void resetVirtualStructure() {
    keys = Optional.absent();
    if (isAccessibilityEnabled()) {
      AccessibilityEvent event = AccessibilityEvent.obtain();
      ViewCompat.onInitializeAccessibilityEvent(view, event);
      event.setEventType(AccessibilityEventCompat.TYPE_WINDOW_CONTENT_CHANGED);
      AccessibilityUtil.sendAccessibilityEvent(getContext(), event);
    }
  }

  private Optional<String> getContentDescription(Key key) {
    Preconditions.checkNotNull(key);

    if (!shouldObscureInput) {
      return Optional.fromNullable(getKeyState(key, metaState).getContentDescription());
    }
    Optional<Integer> optionalKeyCode = getKeyCode(key);
    if (!optionalKeyCode.isPresent()) {
      // Spacer
      return null;
    }
    int code = optionalKeyCode.get();
    boolean isDefinedNonCtrl = Character.isDefined(code) && !Character.isISOControl(code);

    return isDefinedNonCtrl
        ? Optional.of(getContext().getString(PASSWORD_RESOURCE_ID))
        : Optional.of(getKeyState(key, metaState).getContentDescription());
  }
}
