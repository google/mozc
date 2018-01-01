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

package org.mozc.android.inputmethod.japanese.ui;

import org.mozc.android.inputmethod.japanese.ViewEventListener;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.Annotation;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.Candidates;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCandidates.Candidates.Candidate;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Command;
import org.mozc.android.inputmethod.japanese.protobuf.ProtoCommands.Output;
import org.mozc.android.inputmethod.japanese.testing.InstrumentationTestCaseWithMock;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;

import android.content.res.Resources;
import android.graphics.Rect;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.MotionEvent;

/**
 * Test for FloatingCandidateLayoutRenderer.
 */
public class FloatingCandidateLayoutRendererTest extends InstrumentationTestCaseWithMock {

  private Command generateCommandsProto(int candidatesNum) {
    Preconditions.checkArgument(0 <= candidatesNum && candidatesNum < 10);
    Candidates.Builder candidatesBuilder = Candidates.newBuilder();
    for (int i = 0; i < candidatesNum; ++i) {
      candidatesBuilder.addCandidate(Candidate.newBuilder()
          .setIndex(i)
          .setValue("cand_" + Integer.toString(i))
          .setId(i)
          .setAnnotation(Annotation.newBuilder().setShortcut(Integer.toString(i + 1))));
    }
    candidatesBuilder.setSize(candidatesNum);
    candidatesBuilder.setPosition(0);

    return Command.newBuilder()
        .setOutput(Output.newBuilder().setCandidates(candidatesBuilder))
        .buildPartial();
  }

  @SmallTest
  public void testSetCandidates() {
    Resources resources = getInstrumentation().getTargetContext().getResources();
    FloatingCandidateLayoutRenderer layoutRenderer = new FloatingCandidateLayoutRenderer(resources);
    layoutRenderer.setMaxWidth(Integer.MAX_VALUE);

    // No candidates
    layoutRenderer.setCandidates(generateCommandsProto(0));
    assertFalse(layoutRenderer.getWindowRect().isPresent());

    layoutRenderer.setCandidates(generateCommandsProto(1));
    Optional<Rect> candidateRect1 = layoutRenderer.getWindowRect();
    assertTrue(candidateRect1.isPresent());
    assertTrue(0 > candidateRect1.get().left);
    assertEquals(0, candidateRect1.get().top);
    assertTrue(candidateRect1.get().width() > 0);
    assertTrue(candidateRect1.get().height() > 0);

    layoutRenderer.setCandidates(generateCommandsProto(2));
    Optional<Rect> candidateRect2 = layoutRenderer.getWindowRect();
    assertTrue(candidateRect2.isPresent());
    assertTrue(0 > candidateRect2.get().left);
    assertEquals(0, candidateRect2.get().top);
    assertTrue(candidateRect2.get().width() > 0);
    assertTrue(candidateRect2.get().height() > 0);

    int candidateHeight = candidateRect2.get().height() - candidateRect1.get().height();

    layoutRenderer.setCandidates(generateCommandsProto(3));
    Optional<Rect> candidateRect3 = layoutRenderer.getWindowRect();
    assertTrue(candidateRect3.isPresent());
    assertTrue(0 > candidateRect3.get().left);
    assertEquals(0, candidateRect3.get().top);
    assertTrue(candidateRect3.get().width() > 0);
    assertEquals(candidateRect2.get().height() + candidateHeight, candidateRect3.get().height());
  }

  public void testSetMaxWidth() {
    Resources resources = getInstrumentation().getTargetContext().getResources();
    FloatingCandidateLayoutRenderer layoutRenderer = new FloatingCandidateLayoutRenderer(resources);
    layoutRenderer.setMaxWidth(Integer.MAX_VALUE);

    layoutRenderer.setCandidates(generateCommandsProto(1));
    Rect originalCandidateRect = layoutRenderer.getWindowRect().get();
    int maxWidth = originalCandidateRect.width() - 1;

    layoutRenderer.setMaxWidth(maxWidth);
    Optional<Rect> candidateRectWithLimit = layoutRenderer.getWindowRect();
    assertTrue(candidateRectWithLimit.isPresent());
    assertEquals(maxWidth, candidateRectWithLimit.get().width());
    assertEquals(originalCandidateRect.height(), candidateRectWithLimit.get().height());

    layoutRenderer.setMaxWidth(originalCandidateRect.width());
    assertEquals(originalCandidateRect, layoutRenderer.getWindowRect().get());

    layoutRenderer.setMaxWidth(0);
    assertFalse(layoutRenderer.getWindowRect().isPresent());
  }

  @SmallTest
  public void testTouchEvent() {
    Resources resources = getInstrumentation().getTargetContext().getResources();
    FloatingCandidateLayoutRenderer layoutRenderer = new FloatingCandidateLayoutRenderer(resources);
    layoutRenderer.setMaxWidth(Integer.MAX_VALUE);

    layoutRenderer.setCandidates(generateCommandsProto(1));
    int hight1 = layoutRenderer.getWindowRect().get().height();
    layoutRenderer.setCandidates(generateCommandsProto(2));
    int hight2 = layoutRenderer.getWindowRect().get().height();
    int candidateHeight = hight2 - hight1;

    ViewEventListener listener = createMock(ViewEventListener.class);
    layoutRenderer.setViewEventListener(listener);

    resetAll();
    listener.onConversionCandidateSelected(0, Optional.<Integer>absent());
    replayAll();
    MotionEvent downEvent = MotionEvent.obtain(0L, 0L, MotionEvent.ACTION_DOWN, 0, 0, 0);
    MotionEvent upEvent = MotionEvent.obtain(0L, 0L, MotionEvent.ACTION_UP, 0, 0, 0);
    try {
      layoutRenderer.onTouchEvent(downEvent);
      layoutRenderer.onTouchEvent(upEvent);
    } finally {
      downEvent.recycle();
      upEvent.recycle();
    }
    verifyAll();

    resetAll();
    listener.onConversionCandidateSelected(0, Optional.<Integer>absent());
    replayAll();
    downEvent = MotionEvent.obtain(0L, 0L, MotionEvent.ACTION_DOWN, 0, 0, 0);
    upEvent = MotionEvent.obtain(0L, 0L, MotionEvent.ACTION_UP, 0, candidateHeight - 1, 0);
    try {
      layoutRenderer.onTouchEvent(downEvent);
      layoutRenderer.onTouchEvent(upEvent);
    } finally {
      downEvent.recycle();
      upEvent.recycle();
    }
    verifyAll();

    resetAll();
    listener.onConversionCandidateSelected(1, Optional.<Integer>absent());
    replayAll();
    downEvent = MotionEvent.obtain(0L, 0L, MotionEvent.ACTION_DOWN, 0, candidateHeight, 0);
    upEvent = MotionEvent.obtain(0L, 0L, MotionEvent.ACTION_UP, 0, candidateHeight, 0);
    try {
      layoutRenderer.onTouchEvent(downEvent);
      layoutRenderer.onTouchEvent(upEvent);
    } finally {
      downEvent.recycle();
      upEvent.recycle();
    }
    verifyAll();
  }
}
