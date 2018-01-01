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
import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.support.v4.view.AccessibilityDelegateCompat;
import android.support.v4.view.accessibility.AccessibilityEventCompat;
import android.support.v4.view.accessibility.AccessibilityNodeInfoCompat;
import android.support.v4.view.accessibility.AccessibilityNodeProviderCompat;
import android.support.v4.view.accessibility.AccessibilityRecordCompat;
import android.view.MotionEvent;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;

/**
 * Delegate object for candidate view to support accessibility.
 * <p>
 * This class is similar to {@code KeyboardAccessibilityDelegate} but the behavior
 * is different. It is not good idea to extract common behavior as super class.
 */
// TODO(matsuzakit): It seems that TYPE_VIEW_SCROLLED event from IME cannot reach to accessibility
//                   service. Alternative solution might be required.
public class CandidateWindowAccessibilityDelegate extends AccessibilityDelegateCompat {

  private final CandidateWindowAccessibilityNodeProvider nodeProvider;
  private final View view;
  private Optional<CandidateWord> lastHoverCandidateWord = Optional.absent();
  // Size of whole content in pixel.
  private int contentSize;
  // Size of view in pixel.
  private int viewSize;

  public CandidateWindowAccessibilityDelegate(View view) {
    this(Preconditions.checkNotNull(view), new CandidateWindowAccessibilityNodeProvider(view));
  }

  @VisibleForTesting
  CandidateWindowAccessibilityDelegate(View view,
                                       CandidateWindowAccessibilityNodeProvider nodeProvider) {
    this.view = Preconditions.checkNotNull(view);
    this.nodeProvider = Preconditions.checkNotNull(nodeProvider);
  }

  @Override
  public AccessibilityNodeProviderCompat getAccessibilityNodeProvider(View view) {
    return nodeProvider;
  }

  /**
   * Sets (updated) candidate layout.
   * <p>
   * Should be called when the view's candidate layout is updated.
   */
  public void setCandidateLayout(Optional<CandidateLayout> layout, int contentSize, int viewSize) {
    Preconditions.checkNotNull(layout);
    Preconditions.checkArgument(contentSize >= 0);
    Preconditions.checkArgument(viewSize >= 0);

    nodeProvider.setCandidateLayout(layout);
    this.contentSize = contentSize;
    this.viewSize = viewSize;
    sendAccessibilityEvent(view, AccessibilityEventCompat.TYPE_WINDOW_CONTENT_CHANGED);
  }

  /**
   * Dispatched from {@code View#dispatchHoverEvent}.
   *
   * @return {@code true} if the event was handled by the view, false otherwise
   */
  public boolean dispatchHoverEvent(MotionEvent event) {
    Preconditions.checkNotNull(event);

    Optional<CandidateWord> optionalCandidateWord =
        nodeProvider.getCandidateWord((int) event.getX(), (int) event.getY() + view.getScrollY());
    switch (event.getAction()) {
      case MotionEvent.ACTION_HOVER_ENTER:
        Preconditions.checkState(!lastHoverCandidateWord.isPresent());
        if (optionalCandidateWord.isPresent()) {
          CandidateWord candidateWord = optionalCandidateWord.get();
          // Notify the user that we are entering new virtual view.
          nodeProvider.sendAccessibilityEventForCandidateWordIfAccessibilityEnabled(
              candidateWord, AccessibilityEventCompat.TYPE_VIEW_HOVER_ENTER);
          // Make virtual view focus on the candidate.
          nodeProvider.performActionForCandidateWord(
              candidateWord, AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS);
        }
        lastHoverCandidateWord = optionalCandidateWord;
        break;
      case MotionEvent.ACTION_HOVER_EXIT:
        if (optionalCandidateWord.isPresent()) {
          CandidateWord candidateWord = optionalCandidateWord.get();
          // Notify the user that we are exiting from the candidate.
          nodeProvider.sendAccessibilityEventForCandidateWordIfAccessibilityEnabled(
              candidateWord, AccessibilityEventCompat.TYPE_VIEW_HOVER_EXIT);
          // Make virtual view unfocused.
          nodeProvider.performActionForCandidateWord(
              candidateWord, AccessibilityNodeInfoCompat.ACTION_CLEAR_ACCESSIBILITY_FOCUS);
        }
        lastHoverCandidateWord = Optional.absent();
        break;
      case MotionEvent.ACTION_HOVER_MOVE:
        if (optionalCandidateWord.equals(lastHoverCandidateWord)) {
          // Hovering status is unchanged.
          break;
        }
        if (lastHoverCandidateWord.isPresent()) {
          // Notify the user that we are exiting from lastHoverCandidateWord.
          nodeProvider.sendAccessibilityEventForCandidateWordIfAccessibilityEnabled(
              lastHoverCandidateWord.get(), AccessibilityEventCompat.TYPE_VIEW_HOVER_EXIT);
          // Make virtual view unfocused.
          nodeProvider.performActionForCandidateWord(
              lastHoverCandidateWord.get(),
              AccessibilityNodeInfoCompat.ACTION_CLEAR_ACCESSIBILITY_FOCUS);
        }
        if (optionalCandidateWord.isPresent()) {
          CandidateWord candidateWord = optionalCandidateWord.get();
          // Notify the user that we are entering new virtual view.
          nodeProvider.sendAccessibilityEventForCandidateWordIfAccessibilityEnabled(
              candidateWord, AccessibilityEventCompat.TYPE_VIEW_HOVER_ENTER);
          // Make virtual view focus on the candidate.
          nodeProvider.performActionForCandidateWord(
              candidateWord, AccessibilityNodeInfoCompat.ACTION_ACCESSIBILITY_FOCUS);
        }
        lastHoverCandidateWord = optionalCandidateWord;
        break;
    }
    return optionalCandidateWord.isPresent();
  }

  @Override
  public void onInitializeAccessibilityEvent(View host, AccessibilityEvent event) {
    super.onInitializeAccessibilityEvent(host, event);

    AccessibilityRecordCompat record = AccessibilityEventCompat.asRecord(event);
    record.setScrollable(viewSize < contentSize);
    record.setScrollX(0);
    record.setScrollY(view.getScrollY());
    record.setMaxScrollX(0);
    record.setMaxScrollY(contentSize);
  }

  @Override
  public void onInitializeAccessibilityNodeInfo(View host, AccessibilityNodeInfoCompat info) {
    super.onInitializeAccessibilityNodeInfo(host, info);

    info.setClassName(getClass().getName());
    info.setScrollable(viewSize < contentSize);
    info.setFocusable(true);
    int scrollY = view.getScrollY();
    if (scrollY > 0) {
        info.addAction(AccessibilityNodeInfoCompat.ACTION_SCROLL_BACKWARD);
    }
    if (scrollY < contentSize - viewSize) {
        info.addAction(AccessibilityNodeInfoCompat.ACTION_SCROLL_FORWARD);
    }
  }
}
