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

import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.CandidateWord;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Row;
import org.mozc.android.inputmethod.japanese.ui.CandidateLayout.Span;
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;
import com.google.common.base.Strings;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Rect;
import android.os.Bundle;
import android.support.v4.view.ViewCompat;
import android.support.v4.view.accessibility.AccessibilityEventCompat;
import android.support.v4.view.accessibility.AccessibilityNodeInfoCompat;
import android.support.v4.view.accessibility.AccessibilityNodeProviderCompat;
import android.support.v4.view.accessibility.AccessibilityRecordCompat;
import android.util.SparseArray;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;

import javax.annotation.Nullable;

/**
 * Represents candidate window's virtual structure.
 *
 * <p>Note about virtual view ID: This class uses {@code CandidateWord}'s {@code id}
 * as virtual view ID.
 */
class CandidateWindowAccessibilityNodeProvider extends AccessibilityNodeProviderCompat {

  @VisibleForTesting static final int UNDEFINED_VIRTUAL_VIEW_ID = Integer.MAX_VALUE;
  @VisibleForTesting static final int FOLD_BUTTON_VIRTUAL_VIEW_ID = Integer.MAX_VALUE - 1;
  private Optional<CandidateLayout> layout = Optional.absent();
  // The view backed by this class.
  private final View view;
  // Caches only Row. Caches Span might be slow on construction.
  private Optional<SparseArray<Row>> virtualViewIdToRow = Optional.absent();
  // Virtual ID of focused (in the light of accessibility) view.
  private int virtualFocusedViewId = UNDEFINED_VIRTUAL_VIEW_ID;
  // Negative virtual id makes Talkback confused (to be more exact, negative values
  // seems to be _reserved_).
  // We used to use candidate ID as virtual view id but it violates Talkback's
  // assumption.
  // By adding this offset we can avoid negative virtual view ID.
  private static final int VIRTUAL_VIEW_ID_OFFSET = 10000;

  CandidateWindowAccessibilityNodeProvider(View view) {
    this.view = Preconditions.checkNotNull(view);
  }

  private Context getContext() {
    return view.getContext();
  }

  /**
   * Sets updated layout and resets virtual view structure.
   */
  void setCandidateLayout(Optional<CandidateLayout> layout) {
    this.layout = Preconditions.checkNotNull(layout);
    resetVirtualStructure();
  }

  /**
   * Returns a {@code Row} which contains a {@code CandidateWord} of which the
   * {@code id} is given {@code virtualViewId}.
   */
  private Optional<Row> getRowByVirtualViewId(int virtualViewId) {
    if (!virtualViewIdToRow.isPresent()) {
      if (!layout.isPresent()) {
        return Optional.absent();
      }
      SparseArray<Row> virtualViewIdToRow = new SparseArray<Row>();
      for (Row row : layout.get().getRowList()) {
        for (Span span : row.getSpanList()) {
          if (span.getCandidateWord().isPresent()) {
            // - Skip reserved empty span, which is for folding button.
            // - Use append method expecting that the id is in ascending order.
            //   Even if not, it works well.
            virtualViewIdToRow.append(
                candidateIdToVirtualId(span.getCandidateWord().get().getId()), row);
          } else {
            virtualViewIdToRow.append(FOLD_BUTTON_VIRTUAL_VIEW_ID, row);
          }
        }
      }
      this.virtualViewIdToRow = Optional.of(virtualViewIdToRow);
    }
    return Optional.fromNullable(virtualViewIdToRow.get().get(virtualViewId));
  }

  @SuppressLint("InlinedApi")
  private Optional<AccessibilityNodeInfoCompat> createNodeInfoForVirtualViewId(int virtualViewId) {
    Preconditions.checkArgument(virtualViewId >= 0);
    Optional<Row> optionalRow = getRowByVirtualViewId(virtualViewId);
    if (!optionalRow.isPresent()) {
      return Optional.absent();
    }
    int candidateId = virtualViewIdToCandidateId(virtualViewId);
    Row row = optionalRow.get();
    for (Span span : row.getSpanList()) {
      if (span.getCandidateWord().isPresent()
          && span.getCandidateWord().get().getId() != candidateId) {
        continue;
      }
      AccessibilityNodeInfoCompat info = createNodeInfoForSpan(virtualViewId, row, span);
      info.setContentDescription(span.getCandidateWord().isPresent()
          ? getContentDescription(span.getCandidateWord().get()) : null);
      if (virtualFocusedViewId == virtualViewId) {
        info.addAction(AccessibilityNodeInfoCompat.ACTION_CLEAR_ACCESSIBILITY_FOCUS);
      } else {
        info.addAction(AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS);
      }
      return Optional.of(info);
    }
    return Optional.absent();
  }

  private AccessibilityNodeInfoCompat createNodeInfoForSpan(int virtualViewId, Row row, Span span) {
    AccessibilityNodeInfoCompat info = AccessibilityNodeInfoCompat.obtain();
    Rect boundsInParent =
        new Rect((int) (span.getLeft()), (int) (row.getTop()),
                 (int) (span.getRight()), (int) (row.getTop() + row.getHeight()));
    int[] parentLocationOnScreen = new int[2];
    view.getLocationOnScreen(parentLocationOnScreen);
    Rect boundsInScreen = new Rect(boundsInParent);
    boundsInScreen.offset(parentLocationOnScreen[0], parentLocationOnScreen[1]);

    info.setPackageName(getContext().getPackageName());
    info.setClassName(Span.class.getName());
    info.setBoundsInParent(boundsInParent);
    info.setBoundsInScreen(boundsInScreen);
    info.setParent(view);
    info.setSource(view, virtualViewId);
    info.setEnabled(true);
    info.setVisibleToUser(true);
    return info;
  }

  @Nullable
  @Override
  public AccessibilityNodeInfoCompat createAccessibilityNodeInfo(int virtualViewId) {
    if (virtualViewId == UNDEFINED_VIRTUAL_VIEW_ID) {
      return null;
    }
    if (virtualViewId == View.NO_ID) {
      // Required to return the information about entire view.
      AccessibilityNodeInfoCompat info =
          AccessibilityNodeInfoCompat.obtain(view);
      Preconditions.checkNotNull(info);
      ViewCompat.onInitializeAccessibilityNodeInfo(view, info);
      if (!layout.isPresent()) {
        return info;
      }
      for (Row row : layout.get().getRowList()) {
        for (Span span : row.getSpanList()) {
          if (span.getCandidateWord().isPresent()) {
            // Skip reserved empty span, which is for folding button.
            info.addChild(view, candidateIdToVirtualId(span.getCandidateWord().get().getId()));
          } else {
            info.addChild(view, FOLD_BUTTON_VIRTUAL_VIEW_ID);
          }
        }
      }
      return info;
    }
    return createNodeInfoForVirtualViewId(virtualViewId).orNull();
  }

  private boolean isAccessibilityEnabled() {
    return AccessibilityUtil.isAccessibilityEnabled(getContext());
  }

  private void resetVirtualStructure() {
    virtualViewIdToRow = Optional.absent();
    if (isAccessibilityEnabled()) {
      AccessibilityEvent event = AccessibilityEvent.obtain();
      ViewCompat.onInitializeAccessibilityEvent(view, event);
      event.setEventType(AccessibilityEventCompat.TYPE_WINDOW_CONTENT_CHANGED);
      AccessibilityUtil.sendAccessibilityEvent(getContext(), event);
    }
  }

  /**
   * Returns a {@code CandidateWord} based on given position if available.
   *
   * @param x horizontal location in screen coordinate (pixel)
   * @param y vertical location in screen coordinate (pixel)
   */
  Optional<CandidateWord> getCandidateWord(int x, int y) {
    if (!layout.isPresent()) {
      return Optional.absent();
    }
    for (Row row : layout.get().getRowList()) {
      if (y < row.getTop() || y >= row.getTop() + row.getHeight()) {
        continue;
      }
      for (Span span : row.getSpanList()) {
        if (x >= span.getLeft() && x < span.getRight()) {
          return span.getCandidateWord();
        }
      }
    }
    return Optional.absent();
  }

  void sendAccessibilityEventForCandidateWordIfAccessibilityEnabled(CandidateWord candidateWord,
                                                                    int eventType) {
    if (isAccessibilityEnabled()) {
      AccessibilityEvent event = createAccessibilityEvent(candidateWord, eventType);
      AccessibilityUtil.sendAccessibilityEvent(getContext(), event);
    }
  }

  private AccessibilityEvent createAccessibilityEvent(CandidateWord candidateWord,
                                                      int eventType) {
    Preconditions.checkNotNull(candidateWord);

    AccessibilityEvent event = AccessibilityEvent.obtain(eventType);
    event.setPackageName(getContext().getPackageName());
    event.setClassName(candidateWord.getClass().getName());
    event.setContentDescription(getContentDescription(candidateWord));
    event.setEnabled(true);
    AccessibilityRecordCompat record = AccessibilityEventCompat.asRecord(event);
    record.setSource(view, candidateIdToVirtualId(candidateWord.getId()));
    return event;
  }

  /**
   * Returns content description based on value and annotation.
   */
  private String getContentDescription(CandidateWord candidateWord) {
    Preconditions.checkNotNull(candidateWord);
    String contentDescription = Strings.nullToEmpty(candidateWord.getValue());
    if (candidateWord.hasAnnotation() && candidateWord.getAnnotation().hasDescription()) {
      contentDescription += " " + candidateWord.getAnnotation().getDescription();
    }
    return contentDescription;
  }

  private Optional<CandidateWord> getCandidateWordFromVirtualViewId(int virtualViewId) {
    Optional<Row> optionalRow = getRowByVirtualViewId(virtualViewId);
    if (!optionalRow.isPresent()) {
      return Optional.absent();
    }
    int candidateId = virtualViewIdToCandidateId(virtualViewId);
    Row row = optionalRow.get();
    for (Span span : row.getSpanList()) {
      Optional<CandidateWord> candidateWord = span.getCandidateWord();
      if (candidateWord.isPresent() && candidateWord.get().getId() == candidateId) {
        return candidateWord;
      }
    }
    return Optional.absent();
  }

  @Override
  public boolean performAction(int virtualViewId, int action, Bundle arguments) {
    Optional<CandidateWord> candidateWord = getCandidateWordFromVirtualViewId(virtualViewId);
    return candidateWord.isPresent()
        ? performActionForCandidateWordInternal(candidateWord.get(), virtualViewId, action)
        : false;
  }

  boolean performActionForCandidateWord(CandidateWord candidateWord,
                                        int actionAccessibilityFocus) {
    Preconditions.checkNotNull(candidateWord);
    return performActionForCandidateWordInternal(
        candidateWord, candidateIdToVirtualId(candidateWord.getId()), actionAccessibilityFocus);
  }

  private boolean performActionForCandidateWordInternal(CandidateWord candidateWord,
                                                        int virtualViewId, int action) {
    Preconditions.checkArgument(virtualViewId >= 0);
    Preconditions.checkNotNull(candidateWord);

    switch (action) {
      case AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS:
        if (virtualFocusedViewId == virtualViewId) {
          // If focused virtual view is unchanged, do nothing.
          return false;
        }
        // Framework requires the candidate window to have focus.
        // Return FOCUSED event to the framework as response.
        virtualFocusedViewId = virtualViewId;
        if (isAccessibilityEnabled()) {
          AccessibilityUtil.sendAccessibilityEvent(
              getContext(),
              createAccessibilityEvent(
                  candidateWord,
                  AccessibilityEventCompat.TYPE_VIEW_ACCESSIBILITY_FOCUSED));
        }
        return true;
      case AccessibilityNodeInfoCompat.ACTION_CLEAR_ACCESSIBILITY_FOCUS:
        // Framework requires the candidate window to clear focus.
        // Return FOCUSE_CLEARED event to the framework as response.
        if (virtualFocusedViewId != virtualViewId) {
          return false;
        }
        virtualFocusedViewId = UNDEFINED_VIRTUAL_VIEW_ID;
        if (isAccessibilityEnabled()) {
          AccessibilityUtil.sendAccessibilityEvent(
              getContext(),
              createAccessibilityEvent(
                  candidateWord,
                  AccessibilityEventCompat.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED));
        }
        return true;
      default:
        return false;
    }
  }

  private static int virtualViewIdToCandidateId(int virtualViewId) {
    if (virtualViewId == UNDEFINED_VIRTUAL_VIEW_ID
        || virtualViewId == FOLD_BUTTON_VIRTUAL_VIEW_ID) {
      return virtualViewId;
    }
    return virtualViewId - VIRTUAL_VIEW_ID_OFFSET;
  }

  private static int candidateIdToVirtualId(int candidateId) {
    Preconditions.checkArgument(candidateId != UNDEFINED_VIRTUAL_VIEW_ID
                                && candidateId != FOLD_BUTTON_VIRTUAL_VIEW_ID);
    return candidateId + VIRTUAL_VIEW_ID_OFFSET;
  }
}